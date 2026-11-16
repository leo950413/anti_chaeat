import os
import sys
import json
import shutil
import argparse
from PIL import Image, ImageDraw, ImageFont
from pygments import lex
from pygments.lexers import CppLexer
from pygments.token import Token
from pptx import Presentation
from pptx.util import Inches, Pt
from pptx.enum.text import PP_ALIGN
from pptx.dml.color import RGBColor
from pptx.enum.shapes import MSO_SHAPE, MSO_SHAPE_TYPE

DEFAULT_JSON_PATHS = [
    r"C:\Users\leo95\OneDrive\桌面\anti_chaeat\presentation_data.json",
    r"C:\Users\leo95\OneDrive\桌面\OS\anti_chaeat\presentation_data.json"
]

DEFAULT_PPTX_PATHS = [
    r"C:\Users\leo95\OneDrive\桌面\anti_chaeat\presentation.pptx",
    r"C:\Users\leo95\OneDrive\桌面\OS\anti_chaeat\presentation.pptx",
    r"C:\Users\leo95\OneDrive\桌面\anti_chaeat\presentation_updated.pptx",
    r"C:\Users\leo95\OneDrive\桌面\OS\anti_chaeat\presentation_updated.pptx"
]

IMG_DIRS = [
    r"C:\Users\leo95\OneDrive\桌面\anti_chaeat\presentation_images",
    r"C:\Users\leo95\OneDrive\桌面\OS\anti_chaeat\presentation_images"
]

def rgb_to_hex(rgb_color):
    if rgb_color is None:
        return None
    return f"#{rgb_color[0]:02X}{rgb_color[1]:02X}{rgb_color[2]:02X}"

def hex_to_rgb(hex_str, default=(15, 23, 42)):
    if not hex_str or not isinstance(hex_str, str):
        return RGBColor(*default)
    h = hex_str.lstrip("#")
    if len(h) == 6:
        return RGBColor(int(h[0:2], 16), int(h[2:4], 16), int(h[4:6], 16))
    return RGBColor(*default)

def find_img(filename):
    if not filename:
        return ""
    if os.path.isabs(filename) and os.path.exists(filename):
        return filename
    candidates = [
        os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "presentation_images", filename),
        os.path.join(r"C:\Users\leo95\OneDrive\桌面\anti_chaeat\presentation_images", filename),
        os.path.join(r"C:\Users\leo95\OneDrive\桌面\OS\anti_chaeat\presentation_images", filename),
        os.path.join(r"C:\Users\leo95\OneDrive\桌面\anti_chaeat\assets", filename),
        os.path.join(r"C:\Users\leo95\OneDrive\桌面\anti_chaeat\tools", filename),
        os.path.join(r"C:\Users\leo95\OneDrive\桌面\OS\anti_chaeat\tools", filename)
    ]
    for p in candidates:
        if os.path.exists(p):
            return p
    return os.path.join(r"C:\Users\leo95\OneDrive\桌面\anti_chaeat\presentation_images", filename)


def render_code_to_image(code_lines, slide_num, window_title="C++ / x86-64 Disassembly", custom_fname=None):
    """
    Renders a list of code lines into a Carbon / VS Code Dark+ style
    syntax-highlighted PNG image using Pygments + Pillow and saves it
    to presentation_images/code_slide_XX.png.
    """
    # Strip leading/trailing blank lines
    lines = list(code_lines)
    while lines and not lines[0].strip():
        lines.pop(0)
    while lines and not lines[-1].strip():
        lines.pop()
    if not lines:
        return None

    num_lines = len(lines)
    font_px = 38 if num_lines <= 6 else (34 if num_lines <= 10 else 30)
    line_h = int(font_px * 1.42)
    pad_x = 28
    pad_y = 24
    gutter_w = 64

    try:
        font_reg = ImageFont.truetype(r"C:\Windows\Fonts\consola.ttf", font_px)
        font_bold = ImageFont.truetype(r"C:\Windows\Fonts\consolab.ttf", font_px)
    except Exception:
        font_reg = ImageFont.load_default()
        font_bold = font_reg

    # Measure max width tightly around actual text so PowerPoint doesn't shrink short snippets
    dummy_img = Image.new("RGBA", (10, 10))
    dummy_draw = ImageDraw.Draw(dummy_img)
    max_text_w = 400
    for ln in lines:
        bbox = dummy_draw.textbbox((0, 0), ln, font=font_bold)
        w = bbox[2] - bbox[0]
        if w > max_text_w:
            max_text_w = w

    img_w = pad_x * 2 + gutter_w + max_text_w + 24
    img_h = pad_y * 2 + num_lines * line_h + 6

    img = Image.new("RGBA", (img_w, img_h), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)

    # Outer rounded card only (no top window bar or dots)
    draw.rounded_rectangle([0, 0, img_w - 1, img_h - 1], radius=18, fill=(15, 23, 42, 255), outline=(51, 65, 85, 255), width=2)

    # Token color mapping (VS Code Dark+ palette)
    def token_color(ttype, val):
        if ttype in Token.Comment or val.strip().startswith("//") or val.strip().startswith(";"):
            return (106, 153, 85, 255), False      # Green comment
        if ttype in Token.Literal.String or ttype in Token.Literal.String.Char:
            return (206, 145, 120, 255), False     # Coral string
        if ttype in Token.Keyword or val in ("DWORD", "HANDLE", "HWND", "uintptr_t", "LPCVOID", "LPVOID", "BOOL", "MODULEENTRY32W", "Texture2D", "Model", "Color", "mov", "cmp", "jle", "dword", "ptr", "rax", "eax", "rsp"):
            return (86, 156, 214, 255), True       # Sky blue keyword
        if ttype in Token.Name.Function or val in ("LoadTexture", "LoadModelFromMesh", "SetMaterialTexture", "DrawModelEx", "VerifyAsset", "PromptAssetTamperAlert", "FindWindowA", "GetWindowThreadProcessId", "OpenProcess", "CreateToolhelp32Snapshot", "Module32FirstW", "Module32NextW", "CloseHandle", "ReadProcessMemory", "WriteProcessMemory", "CreateRemoteThread", "GetLastError"):
            return (220, 220, 170, 255), False     # Warm yellow function
        if ttype in Token.Literal.Number or val.startswith("0x"):
            return (181, 206, 168, 255), False     # Mint number
        if ttype in Token.Operator or ttype in Token.Punctuation:
            return (212, 212, 212, 255), False
        return (248, 250, 252, 255), False         # Crisp white

    lexer = CppLexer(stripnl=False)

    for idx, line in enumerate(lines):
        y = pad_y + idx * line_h
        # Draw line number
        ln_str = f"{idx + 1:2d}"
        draw.text((pad_x - 6, y), ln_str, fill=(100, 116, 139, 255), font=font_reg)

        x = pad_x + gutter_w
        stripped = line.strip()
        if stripped.startswith("//"):
            draw.text((x, y), line, fill=(110, 195, 115, 255), font=font_bold)
            continue

        # Handle inline '-> ...' explanation at the end of assembly lines
        code_part = line
        arrow_part = None
        if " -> " in line and not stripped.startswith("//"):
            idx_arrow = line.index(" -> ")
            code_part = line[:idx_arrow]
            arrow_part = line[idx_arrow:]

        # Lex single line
        for ttype, val in lex(code_part, lexer):
            val = val.rstrip("\r\n")
            if not val:
                continue
            color, is_b = token_color(ttype, val)
            use_f = font_bold if is_b else font_reg
            draw.text((x, y), val, fill=color, font=use_f)
            tb = draw.textbbox((0, 0), val, font=use_f)
            x += (tb[2] - tb[0])

        if arrow_part:
            draw.text((x, y), arrow_part, fill=(110, 195, 115, 255), font=font_reg)

    fname = custom_fname or f"code_slide_{slide_num:02d}.png"
    out_rgb = img.convert("RGB")
    saved_path = None
    for d in IMG_DIRS:
        os.makedirs(d, exist_ok=True)
        p = os.path.join(d, fname)
        out_rgb.save(p)
        if saved_path is None:
            saved_path = p
    return fname


def export_pptx_to_json(pptx_path, json_out_paths=None):
    """
    Reads an existing .pptx file and exports all slides, text, styling,
    layout metadata, and image references into a structured JSON file.
    """
    if not os.path.exists(pptx_path):
        raise FileNotFoundError(f"PPTX file not found: {pptx_path}")

    # Preserve high-level semantic slide definitions if presentation_data.json already exists
    existing_slides_by_num = {}
    if os.path.exists(DEFAULT_JSON_PATHS[0]):
        try:
            with open(DEFAULT_JSON_PATHS[0], "r", encoding="utf-8") as ef:
                edata = json.load(ef)
                for es in edata.get("slides", []):
                    existing_slides_by_num[es.get("slide_number")] = es
        except Exception:
            pass

    prs = Presentation(pptx_path)
    img_out_dir = IMG_DIRS[0]
    os.makedirs(img_out_dir, exist_ok=True)

    known_sizes = {}
    for root_dir in IMG_DIRS:
        if os.path.exists(root_dir):
            for fname in os.listdir(root_dir):
                fpath = os.path.join(root_dir, fname)
                if os.path.isfile(fpath):
                    known_sizes[os.path.getsize(fpath)] = fname

    deck_data = {
        "metadata": {
            "source_pptx": pptx_path,
            "slide_width_inches": round(prs.slide_width.inches, 3),
            "slide_height_inches": round(prs.slide_height.inches, 3),
            "total_slides": len(prs.slides)
        },
        "theme": {
            "BG_COLOR": "#F1F5F9",
            "CARD_BG": "#FFFFFF",
            "CARD_BORDER": "#BAC8D8",
            "CYAN_ACCENT": "#0284C7",
            "GREEN_ACCENT": "#059669",
            "ORANGE_ACCENT": "#D97706",
            "RED_ACCENT": "#DC2626",
            "PURPLE_ACCENT": "#7C3AED",
            "TEXT_LIGHT": "#0F172A",
            "TEXT_MUTED": "#475569"
        },
        "slides": []
    }

    for slide_idx, slide in enumerate(prs.slides):
        slide_num = slide_idx + 1
        shapes_data = []
        text_boxes = []
        picture_file = None

        for sh_idx, shape in enumerate(slide.shapes):
            sh_info = {
                "name": shape.name,
                "shape_type": int(shape.shape_type),
                "left_in": round(shape.left.inches, 3),
                "top_in": round(shape.top.inches, 3),
                "width_in": round(shape.width.inches, 3),
                "height_in": round(shape.height.inches, 3)
            }

            if shape.shape_type == MSO_SHAPE_TYPE.AUTO_SHAPE:
                try:
                    sh_info["auto_shape_type"] = int(shape.auto_shape_type)
                except Exception:
                    pass
                try:
                    if shape.fill.type == 1:
                        sh_info["fill_color"] = rgb_to_hex(shape.fill.fore_color.rgb)
                except Exception:
                    pass
                try:
                    sh_info["line_color"] = rgb_to_hex(shape.line.color.rgb)
                    if shape.line.width:
                        sh_info["line_width_pt"] = round(shape.line.width.pt, 1)
                except Exception:
                    pass

            if shape.shape_type == MSO_SHAPE_TYPE.PICTURE:
                blob = shape.image.blob
                ext = shape.image.ext or "png"
                matched_name = known_sizes.get(len(blob))
                if not matched_name:
                    matched_name = f"slide{slide_num:02d}_img{sh_idx + 1}.{ext}"
                    out_img_path = os.path.join(img_out_dir, matched_name)
                    with open(out_img_path, "wb") as img_f:
                        img_f.write(blob)
                sh_info["image_filename"] = matched_name
                picture_file = matched_name

            if shape.has_text_frame:
                paragraphs = []
                for p in shape.text_frame.paragraphs:
                    p_color = None
                    try:
                        if p.font.color and p.font.color.type == 1:
                            p_color = rgb_to_hex(p.font.color.rgb)
                    except Exception:
                        pass
                    p_info = {
                        "text": p.text,
                        "font_name": p.font.name or "Segoe UI",
                        "font_size_pt": round(p.font.size.pt, 1) if p.font.size else None,
                        "bold": bool(p.font.bold),
                        "italic": bool(p.font.italic),
                        "color": p_color,
                        "space_after_pt": round(p.space_after.pt, 1) if p.space_after else 0
                    }
                    paragraphs.append(p_info)
                sh_info["paragraphs"] = paragraphs
                if any(p["text"].strip() for p in paragraphs):
                    text_boxes.append(sh_info)

            shapes_data.append(sh_info)

        prev = existing_slides_by_num.get(slide_num, {})
        slide_entry = {
            "slide_number": slide_num,
            "type": prev.get("type", "custom"),
            "category": prev.get("category", ""),
            "title": prev.get("title", ""),
            "heading": prev.get("heading", ""),
            "heading_color": prev.get("heading_color", "#0284C7"),
            "image": prev.get("image", picture_file),
            "video": prev.get("video"),
            "items": prev.get("items", []),
            "shapes": shapes_data
        }
        deck_data["slides"].append(slide_entry)

    out_paths = json_out_paths or DEFAULT_JSON_PATHS
    for jp in out_paths:
        try:
            with open(jp, "w", encoding="utf-8") as f:
                json.dump(deck_data, f, indent=2, ensure_ascii=False)
            print(f"[+] Exported {len(deck_data['slides'])} slides to JSON: {jp}")
        except Exception as e:
            print(f"[!] Could not write JSON to {jp}: {e}")

    return deck_data


def build_pptx_from_json(json_path, pptx_out_paths=None, use_exact_shapes=False, only_slides=None):
    with open(json_path, "r", encoding="utf-8") as f:
        data = json.load(f)

    target_set = set(only_slides) if only_slides else None
    existing_pptx = next((p for p in (pptx_out_paths or DEFAULT_PPTX_PATHS) if os.path.exists(p)), None)

    if target_set and existing_pptx:
        prs = Presentation(existing_pptx)
    else:
        prs = Presentation()
        meta = data.get("metadata", {})
        prs.slide_width = Inches(meta.get("slide_width_inches", 13.333))
        prs.slide_height = Inches(meta.get("slide_height_inches", 7.5))
    blank_layout = prs.slide_layouts[6]

    theme = data.get("theme", {})
    BG_COLOR = hex_to_rgb(theme.get("BG_COLOR", "#F1F5F9"))
    CARD_BG = hex_to_rgb(theme.get("CARD_BG", "#FFFFFF"))
    CARD_BORDER = hex_to_rgb(theme.get("CARD_BORDER", "#BAC8D8"))
    CYAN_ACCENT = hex_to_rgb(theme.get("CYAN_ACCENT", "#0284C7"))
    GREEN_ACCENT = hex_to_rgb(theme.get("GREEN_ACCENT", "#059669"))
    RED_ACCENT = hex_to_rgb(theme.get("RED_ACCENT", "#DC2626"))
    TEXT_LIGHT = hex_to_rgb(theme.get("TEXT_LIGHT", "#0F172A"))
    TEXT_MUTED = hex_to_rgb(theme.get("TEXT_MUTED", "#475569"))

    def set_bg(slide):
        bg = slide.shapes.add_shape(MSO_SHAPE.RECTANGLE, Inches(0), Inches(0), prs.slide_width, prs.slide_height)
        bg.fill.solid()
        bg.fill.fore_color.rgb = BG_COLOR
        bg.line.color.rgb = BG_COLOR
        return bg

    def add_header(slide, title, category):
        tb_cat = slide.shapes.add_textbox(Inches(0.6), Inches(0.28), Inches(12.1), Inches(0.4))
        p_cat = tb_cat.text_frame.paragraphs[0]
        p_cat.text = (category or "").upper()
        p_cat.font.name = "Segoe UI"
        p_cat.font.size = Pt(13)
        p_cat.font.bold = True
        p_cat.font.color.rgb = CYAN_ACCENT

        tb_title = slide.shapes.add_textbox(Inches(0.6), Inches(0.62), Inches(12.1), Inches(0.7))
        p_title = tb_title.text_frame.paragraphs[0]
        p_title.text = title or ""
        p_title.font.name = "Segoe UI"
        p_title.font.size = Pt(26)
        p_title.font.bold = True
        p_title.font.color.rgb = TEXT_LIGHT

    def add_card(slide, left=Inches(0.6), top=Inches(1.38), width=Inches(12.133), height=Inches(5.72)):
        card = slide.shapes.add_shape(MSO_SHAPE.ROUNDED_RECTANGLE, left, top, width, height)
        card.fill.solid()
        card.fill.fore_color.rgb = CARD_BG
        card.line.color.rgb = CARD_BORDER
        card.line.width = Pt(1.5)
        return card

    for s_data in data.get("slides", []):
        s_type = s_data.get("type", "custom")
        slide_num = s_data.get("slide_number", 0)

        if target_set is not None and slide_num not in target_set:
            continue

        if target_set is not None and 1 <= slide_num <= len(prs.slides):
            slide = prs.slides[slide_num - 1]
            for old_sh in list(slide.shapes):
                slide.shapes._spTree.remove(old_sh._element)
            in_place = True
        else:
            in_place = False

        if use_exact_shapes or s_type == "custom":
            if not in_place:
                slide = prs.slides.add_slide(blank_layout)
            for sh in s_data.get("shapes", []):
                left = Inches(sh["left_in"])
                top = Inches(sh["top_in"])
                width = Inches(sh["width_in"])
                height = Inches(sh["height_in"])
                sh_type = sh.get("shape_type")

                if sh_type == int(MSO_SHAPE_TYPE.AUTO_SHAPE):
                    auto_t = sh.get("auto_shape_type", int(MSO_SHAPE.RECTANGLE))
                    new_sh = slide.shapes.add_shape(auto_t, left, top, width, height)
                    if sh.get("fill_color"):
                        new_sh.fill.solid()
                        new_sh.fill.fore_color.rgb = hex_to_rgb(sh["fill_color"])
                    if sh.get("line_color"):
                        new_sh.line.color.rgb = hex_to_rgb(sh["line_color"])
                    if sh.get("line_width_pt"):
                        new_sh.line.width = Pt(sh["line_width_pt"])
                elif sh_type == int(MSO_SHAPE_TYPE.PICTURE):
                    img_p = find_img(sh.get("image_filename", ""))
                    if os.path.exists(img_p):
                        slide.shapes.add_picture(img_p, left, top, width=width)
                elif sh_type == int(MSO_SHAPE_TYPE.TEXT_BOX) or "paragraphs" in sh:
                    tb = slide.shapes.add_textbox(left, top, width, height)
                    tf = tb.text_frame
                    tf.word_wrap = True
                    for idx, p_info in enumerate(sh.get("paragraphs", [])):
                        p = tf.paragraphs[0] if idx == 0 else tf.add_paragraph()
                        p.text = p_info.get("text", "")
                        if p_info.get("font_name"):
                            p.font.name = p_info["font_name"]
                        if p_info.get("font_size_pt"):
                            p.font.size = Pt(p_info["font_size_pt"])
                        p.font.bold = bool(p_info.get("bold", False))
                        p.font.italic = bool(p_info.get("italic", False))
                        if p_info.get("color"):
                            p.font.color.rgb = hex_to_rgb(p_info["color"])
                        if p_info.get("space_after_pt"):
                            p.space_after = Pt(p_info["space_after_pt"])
            continue

        if not in_place:
            slide = prs.slides.add_slide(blank_layout)
        set_bg(slide)

        if s_type == "title_slide":
            add_card(slide, Inches(1.0), Inches(1.1), Inches(11.333), Inches(5.3))
            tb = slide.shapes.add_textbox(Inches(1.5), Inches(1.65), Inches(10.333), Inches(4.2))
            tf = tb.text_frame
            tf.word_wrap = True

            p0 = tf.paragraphs[0]
            p0.text = s_data.get("category", "GAME SECURITY & REVERSE ENGINEERING LAB")
            p0.font.name = "Segoe UI"
            p0.font.size = Pt(18)
            p0.font.bold = True
            p0.font.color.rgb = CYAN_ACCENT
            p0.space_after = Pt(20)

            p1 = tf.add_paragraph()
            p1.text = s_data.get("title", "")
            p1.font.name = "Segoe UI"
            p1.font.size = Pt(36)
            p1.font.bold = True
            p1.font.color.rgb = TEXT_LIGHT
            p1.space_after = Pt(28)

            if s_data.get("heading", "").strip():
                p2 = tf.add_paragraph()
                p2.text = s_data.get("heading", "")
                p2.font.name = "Segoe UI"
                p2.font.size = Pt(18)
                p2.font.color.rgb = TEXT_MUTED
                p2.space_after = Pt(28)

            p3 = tf.add_paragraph()
            p3.text = s_data.get("footer", "Target Platform: 3D FPS Arena (C++ / Raylib 5.5)  |  OS: Windows x64")
            p3.font.name = "Segoe UI"
            p3.font.size = Pt(17)
            p3.font.bold = True
            p3.font.color.rgb = GREEN_ACCENT

        elif s_type in ("blank_title_slide", "video_slide"):
            add_header(slide, s_data.get("title", ""), s_data.get("category", ""))
            vid_name = s_data.get("video") or "ce.mp4"
            vid_path = find_img(vid_name)
            poster_path = find_img("ce_poster.png")
            if os.path.exists(vid_path):
                add_card(slide)
                vid_w = 9.6
                vid_h = 5.4
                left_pos = (13.333 - vid_w) / 2.0
                top_pos = 1.54
                slide.shapes.add_movie(
                    vid_path,
                    Inches(left_pos),
                    Inches(top_pos),
                    Inches(vid_w),
                    Inches(vid_h),
                    poster_frame_image=poster_path if os.path.exists(poster_path) else None,
                    mime_type="video/mp4"
                )

        elif s_type == "image_slide":
            add_header(slide, s_data.get("title", ""), s_data.get("category", ""))
            add_card(slide)

            tb_sub = slide.shapes.add_textbox(Inches(0.9), Inches(1.5), Inches(11.5), Inches(0.55))
            p_sub = tb_sub.text_frame.paragraphs[0]
            p_sub.text = s_data.get("heading", "")
            p_sub.font.name = "Segoe UI"
            p_sub.font.size = Pt(21)
            p_sub.font.bold = True
            p_sub.font.color.rgb = hex_to_rgb(s_data.get("heading_color", "#0284C7"))

            items = [b for b in s_data.get("items", []) if b.strip()]
            has_items = len(items) > 0

            img_path = find_img(s_data.get("image", ""))
            if os.path.exists(img_path):
                try:
                    with Image.open(img_path) as im:
                        w_px, h_px = im.size
                    aspect = w_px / float(h_px)
                except Exception:
                    aspect = 16.0 / 9.0
                max_h = 3.55 if has_items else 4.75
                max_w = 10.5 if has_items else 11.2
                disp_h = max_h
                disp_w = disp_h * aspect
                if disp_w > max_w:
                    disp_w = max_w
                left_pos = (13.333 - disp_w) / 2.0
                top_pos = 2.08 if has_items else 2.12
                slide.shapes.add_picture(img_path, Inches(left_pos), Inches(top_pos), width=Inches(disp_w))

            if has_items:
                tb_desc = slide.shapes.add_textbox(Inches(0.9), Inches(5.72), Inches(11.5), Inches(1.3))
                tf = tb_desc.text_frame
                tf.word_wrap = True
                for idx, b in enumerate(items):
                    p = tf.paragraphs[0] if idx == 0 else tf.add_paragraph()
                    clean_b = b.lstrip("• ").strip()
                    p.text = "• " + clean_b
                    p.font.name = "Segoe UI"
                    p.font.size = Pt(16.5)
                    p.font.color.rgb = TEXT_LIGHT
                    p.space_after = Pt(4)

        elif s_type == "code_slide":
            add_header(slide, s_data.get("title", ""), s_data.get("category", ""))
            add_card(slide)

            tb_sub = slide.shapes.add_textbox(Inches(0.9), Inches(1.48), Inches(11.5), Inches(0.5))
            p_sub = tb_sub.text_frame.paragraphs[0]
            p_sub.text = s_data.get("heading", "")
            p_sub.font.name = "Segoe UI"
            p_sub.font.size = Pt(21)
            p_sub.font.bold = True
            p_sub.font.color.rgb = hex_to_rgb(s_data.get("heading_color", "#0284C7"))

            raw_items = s_data.get("items", [])
            bullet_lines = [ln for ln in raw_items if ln.strip().startswith("• ") or ln.strip().startswith("▶ ")]
            code_lines = [ln for ln in raw_items if not (ln.strip().startswith("• ") or ln.strip().startswith("▶ "))]

            has_bullets = len(bullet_lines) > 0
            bullet_h = 0.0
            if has_bullets:
                bullet_h = 1.65 if len(bullet_lines) >= 3 else (1.15 if len(bullet_lines) == 2 else 0.9)

            if "---SPLIT---" in code_lines:
                sp_idx = code_lines.index("---SPLIT---")
                block1 = code_lines[:sp_idx]
                block2 = code_lines[sp_idx + 1:]
                img1_name = render_code_to_image(block1, slide_num, custom_fname=f"code_slide_{slide_num:02d}_cpp.png")
                img2_name = render_code_to_image(block2, slide_num, custom_fname=f"code_slide_{slide_num:02d}_asm.png")
                s_data["image"] = img1_name
                s_data["image_2"] = img2_name
                p1 = find_img(img1_name) if img1_name else ""
                p2 = find_img(img2_name) if img2_name else ""

                cur_top = 2.02
                for img_p in (p1, p2):
                    if img_p and os.path.exists(img_p):
                        with Image.open(img_p) as cim:
                            cw, ch = cim.size
                        aspect = cw / float(ch)
                        disp_w = min(11.3, 2.25 * aspect)
                        disp_h = disp_w / aspect
                        left_pos = (13.333 - disp_w) / 2.0
                        slide.shapes.add_picture(img_p, Inches(left_pos), Inches(cur_top), width=Inches(disp_w))
                        cur_top += disp_h + 0.18
            else:
                # Render code lines into a syntax-highlighted PNG image
                code_img_name = render_code_to_image(code_lines, slide_num, window_title=s_data.get("title", "C++ Source Code"))
                s_data["image"] = code_img_name
                code_img_path = find_img(code_img_name) if code_img_name else ""

                if code_img_path and os.path.exists(code_img_path):
                    with Image.open(code_img_path) as cim:
                        cw, ch = cim.size
                    aspect = cw / float(ch)

                    max_img_h = 4.75 - bullet_h
                    max_img_w = 11.3
                    disp_h = max_img_h
                    disp_w = disp_h * aspect
                    if disp_w > max_img_w:
                        disp_w = max_img_w
                        disp_h = disp_w / aspect

                    left_pos = (13.333 - disp_w) / 2.0
                    top_pos = 2.02
                    slide.shapes.add_picture(code_img_path, Inches(left_pos), Inches(top_pos), width=Inches(disp_w))

                    if has_bullets:
                        b_top = max(2.08 + disp_h + 0.08, 6.85 - bullet_h)
                        tb_b = slide.shapes.add_textbox(Inches(0.9), Inches(b_top), Inches(11.5), Inches(bullet_h + 0.15))
                        tf_b = tb_b.text_frame
                        tf_b.word_wrap = True
                        b_font_pt = 14.5 if len(bullet_lines) >= 3 else 16.0
                        for idx, b in enumerate(bullet_lines):
                            p = tf_b.paragraphs[0] if idx == 0 else tf_b.add_paragraph()
                            p.text = b.strip()
                            p.font.name = "Segoe UI"
                            p.font.size = Pt(b_font_pt)
                            p.font.color.rgb = TEXT_LIGHT
                            p.space_after = Pt(3)

        elif s_type in ("text_slide", "matrix_slide"):
            is_matrix = (s_type == "matrix_slide")
            add_header(slide, s_data.get("title", ""), s_data.get("category", ""))
            add_card(slide)

            tb = slide.shapes.add_textbox(Inches(1.0), Inches(1.62), Inches(11.3), Inches(5.25))
            tf = tb.text_frame
            tf.word_wrap = True

            h_color = hex_to_rgb(s_data.get("heading_color", "#0284C7"))
            p0 = tf.paragraphs[0]
            p0.text = s_data.get("heading", "")
            p0.font.name = "Segoe UI"
            p0.font.size = Pt(22 if is_matrix else 23)
            p0.font.bold = True
            p0.font.color.rgb = h_color
            p0.space_after = Pt(12 if is_matrix else 14)

            items = s_data.get("items", [])
            for idx, line in enumerate(items):
                p = tf.add_paragraph()
                p.text = line
                if is_matrix:
                    if line.strip().startswith("Conclusion:"):
                        p.font.name = "Segoe UI"
                        p.font.size = Pt(15.5)
                        p.font.bold = True
                        p.font.color.rgb = GREEN_ACCENT
                    elif idx == 0:
                        p.font.name = "Consolas"
                        p.font.size = Pt(13.5)
                        p.font.bold = True
                        p.font.color.rgb = CYAN_ACCENT
                        p.space_after = Pt(10)
                    else:
                        p.font.name = "Consolas"
                        p.font.size = Pt(13)
                        p.font.color.rgb = TEXT_LIGHT
                        p.space_after = Pt(9)
                else:
                    p.font.name = "Segoe UI"
                    if line.startswith("▶ "):
                        p.font.size = Pt(19)
                        p.font.bold = True
                        p.font.color.rgb = h_color
                        p.space_after = Pt(4)
                    elif line.startswith("    "):
                        p.font.size = Pt(17)
                        p.font.color.rgb = TEXT_LIGHT
                        p.space_after = Pt(12)
                    else:
                        p.font.size = Pt(19)
                        p.font.color.rgb = TEXT_LIGHT
                        p.space_after = Pt(14)

    out_paths = pptx_out_paths or DEFAULT_PPTX_PATHS
    for path in out_paths:
        try:
            prs.save(path)
            print(f"[+] Built presentation ({len(prs.slides)} slides) from JSON -> {path}")
        except PermissionError:
            print(f"[!] Warning: {path} is locked by PowerPoint (please close it to update).")
        except Exception as e:
            print(f"[!] Could not save to {path}: {e}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Two-way PPTX <-> JSON Presentation Generator & Exporter")
    parser.add_argument("--export", nargs="?", const=DEFAULT_PPTX_PATHS[0], metavar="PPTX_PATH",
                        help="Export an existing .pptx file into presentation_data.json")
    parser.add_argument("--json", default=DEFAULT_JSON_PATHS[0],
                        help="Path to presentation_data.json for building or exporting")
    parser.add_argument("--exact", action="store_true",
                        help="Rebuild PPTX using exact shape coordinates exported from PowerPoint")
    parser.add_argument("--slide", type=str, default=None,
                        help="Comma-separated slide numbers to update in-place without affecting other slides (e.g. --slide 10 or --slide 10,13)")
    args = parser.parse_args()

    if args.export:
        export_pptx_to_json(args.export)
    else:
        if not os.path.exists(args.json):
            export_pptx_to_json(DEFAULT_PPTX_PATHS[0])
        only_list = [int(x.strip()) for x in args.slide.split(",") if x.strip().isdigit()] if args.slide else None
        build_pptx_from_json(args.json, use_exact_shapes=args.exact, only_slides=only_list)
