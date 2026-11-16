# AntiCheat Lab - 遊戲安全與反外掛攻防演示平台 (3D FPS Arena)

本專案為作業系統 (OS) 與系統安全課程設計的教育性演示平台，使用 C++ 與 Raylib 5.5 開發 3D FPS 競技場遊戲，並實作三種業界最經典的外掛攻擊途徑（Attack Vectors）與其對應的反制技術（Defenses）。

---

## 快速啟動（一鍵執行腳本）

在專案根目錄下，我們為您準備了編號快捷腳本：

| 腳本名稱 | 功能說明 | 說明 |
| :--- | :--- | :--- |
| **`1_Run_TargetGame.bat`** | 啟動目標遊戲主程式 | 啟動 3D FPS 競技場 (`TargetGame.exe`) |
| **`2_Attack_MemoryModifier.bat`** | 途徑 2：外部記憶體讀寫攻擊 | 模擬 Cheat Engine，透過 RPM/WPM 搜尋並篡改 HP/Ammo/Score |
| **`3_Attack_RemoteCaller.bat`** | 途徑 3：遠端 DLL 注入與函數呼叫 | 將 `misc_dll.dll` 注入遊戲並觸發 `RemoteAction` 指令 |
| **`4_Toggle_Wallhack.bat`** | 途徑 1：透視貼圖切換器 | 一鍵在「正常不透明磚牆」與「透明外掛偽造牆」之間切換 |
| **`5_Build_All.bat`** | 一鍵完整編譯專案 | 使用 MSVC x64 與 Ninja 自動重新編譯所有工具與遊戲 |

---

## 專案結構 (Directory Tour)

```
anti_chaeat/
├── 1_Run_TargetGame.bat          # 遊戲啟動器
├── 2_Attack_MemoryModifier.bat   # 記憶體修改器啟動器
├── 3_Attack_RemoteCaller.bat     # DLL 遠端呼叫器啟動器
├── 4_Toggle_Wallhack.bat         # 貼圖透視外掛切換器
├── 5_Build_All.bat               # 一鍵完整重新編譯腳本
├── CMakeLists.txt                # CMake 構建設定檔 (自動整合 Raylib 5.5)
├── README.md                     # 本操作指南
├── presentation.pptx             # 13 頁完整演示簡報 (含記憶體步驟圖與遊戲實機對比)
├── presentation_images/          # 簡報專用高畫質實機截圖與架構圖集
├── presentation_updated.pptx     # 簡報備份檔
├── process_injection_notes.md    # Process Injection 技術原理手記
│
├── src/                          # 遊戲主程式源碼
│   ├── main.cpp                  # 3D FPS 核心邏輯、Alpha Cutout 渲染管線、HUD
│   ├── anticheat.hpp             # 內建反外掛防禦（SHA-256 校驗、XOR 記憶體混淆）
│   └── sha256.hpp                # 獨立 SHA-256 密碼學雜湊演算法實作
│
├── assets/                       # 遊戲資源
│   ├── wall.png                  # 正式版不透明磚牆材質 (128x128)
│   └── fake_wall.png             # 攻擊者偽造之全透明材質 (用於 Wallhack)
│
├── tools/                        # 攻擊展示與分析工具
│   ├── build.bat                 # 編譯 tools/ 專用腳本
│   ├── memory_modifcication.cpp  # 外部記憶體修改器原始碼 (RPM/WPM)
│   ├── MemoryModifier.exe        # 已編譯之記憶體修改器
│   ├── misc_dll.cpp              # 注入專用動態連結庫 (匯出 RemoteAction)
│   ├── misc_dll.dll              # 已編譯之注入 Payload
│   ├── remote_caller.cpp         # 遠端執行注入器 (StackOverflow 19798912)
│   ├── remote_caller.exe         # 已編譯之遠端注入器
│   ├── generate_presentation.py  # 自動生成 PPT 簡報之 Python 腳本
│
└── out/build/x64-debug/          # 遊戲建置輸出目錄 (包含 TargetGame.exe 與 runtime assets)
```

---

## 三大攻擊途徑與防護演示說明

### 途徑一：資產篡改透視 (Asset Tampering & Wallhack)
* **攻擊手法**：
  1. 執行 `4_Toggle_Wallhack.bat`，選擇 `[1]` 切換為透明偽造貼圖。
  2. 執行 `1_Run_TargetGame.bat` 啟動遊戲。
  3. 原本正中央遮蔽敵方機器人的厚實不透明磚牆變成**完全透明**，玩家在牆後即可洞悉敵人位置與動作（Wallhack 成立）。
* **防禦機制 (F1 鍵)**：
  1. 在遊戲中按下 **`F1`** 觸發內建的 SHA-256 資產完整性檢查。
  2. 遊戲比對記憶體/硬碟中的 `wall.png` 雜湊值與官方 Release 簽名。
  3. 螢幕頂端立即閃爍紅色安全警報：`[SECURITY ALERT] Asset Tampering Detected: assets/wall.png hash mismatch!`。

---

### 途徑二：外部記憶體修改 (External Memory Tampering)
* **攻擊手法**：
  1. 執行 `1_Run_TargetGame.bat` 進入遊戲。
  2. 執行 `2_Attack_MemoryModifier.bat` 開啟記憶體修改器。
  3. 修改器透過 `OpenProcess` 獲取遊戲 Handle，並利用靜態基底偏移量 (`TargetGame.exe + 0x270A78` -> `Player*`) 解析出玩家血量、彈藥與分數。
  4. 選擇選單修改 HP (100)、補充備彈 (90) 或鎖定分數為 999999。
* **防禦機制 (F2 鍵)**：
  1. 在遊戲中按下 **`F2`** 切換至 **`Memory Mode: XOR-PROTECTED`**。
  2. 遊戲將數值在記憶體中拆解為 `Encrypted ^ Key`，且每次數值變動均重新生成隨機 XOR Key。
  3. 外部攻擊程式讀取到的皆為亂碼/死數值，直接修改明文記憶體會導致校驗失敗並被判定為異常。

---

### 途徑三：遠端 DLL 注入與函數執行 (DLL Injection & Remote Execution)
* **攻擊手法**：
  1. 執行 `1_Run_TargetGame.bat` 進入遊戲。
  2. 執行 `3_Attack_RemoteCaller.bat`。
  3. 注入器執行標準 Windows 系統安全穿透 4 步驟：
     - **步驟 1 (VirtualAllocEx)**：在遊戲進程虛擬記憶體中配置空間。
     - **步驟 2 (WriteProcessMemory)**：將 `misc_dll.dll` 完整路徑寫入目標進程空間。
     - **步驟 3 (CreateRemoteThread + LoadLibraryA)**：誘使遊戲進程主動載入該 DLL。
     - **步驟 4 (RVA Offset Calculation + RemoteAction)**：計算 DLL 匯出函數 `RemoteAction` 的相對虛擬位址（RVA），並建立遠端執行緒傳遞參數執行代碼（如強制增加分數、滿血復活、或彈出惡意對話框）。
* **防禦機制**：
  - Windows API 監控、程式碼簽章驗證（Code Signing）、執行緒來源檢查（Callstack Verification）、防止未經授權的跨進程記憶體配置。

---

## 遊戲控制鍵位

| 鍵位 | 功能 |
| :--- | :--- |
| **W, A, S, D** | 前後左右移動角色 |
| **滑鼠移動** | 第一人稱視角旋轉 (FPS Camera) |
| **滑鼠左鍵** | 射擊 (消耗彈藥，擊中標靶獲得分數) |
| **R 鍵** | 換彈匣 (Reload) |
| **空白鍵 (SPACE)** | 模擬受到傷害 (-15 HP) |
| **H 鍵** | 拾取補包治療 (+15 HP) |
| **F1 鍵** | 執行資產 SHA-256 完整性檢查（防護演示 1） |
| **F2 鍵** | 切換 XOR 記憶體混淆防護（防護演示 2） |
| **F12 鍵** | 遊戲內高畫質截圖 (儲存至目前目錄) |
| **ESC 鍵** | 釋放滑鼠游標 / 暫停選單 |
