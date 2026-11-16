#include "raylib.h"
#include "raymath.h"
#include "anticheat.hpp"
#include <string>
#include <vector>
#include <cmath>

// --- Player State ---
struct Player {
    int health = 100;
    int ammo = 30;
    int maxAmmo = 30;
    int reserveAmmo = 90;
    int score = 0;

    void takeDamage(int amount) {
        health -= amount;
        if (health < 0) health = 0;
    }

    void heal(int amount) {
        health += amount;
        if (health > 100) health = 100;
    }
};

// --- Wall Obstacle ---
struct Wall3D {
    Vector3 position;
    Vector3 size;
    BoundingBox getBox() const {
        return {
            { position.x - size.x / 2, position.y - size.y / 2, position.z - size.z / 2 },
            { position.x + size.x / 2, position.y + size.y / 2, position.z + size.z / 2 }
        };
    }
};

// --- Enemy Bot ---
struct Bot {
    Vector3 position = { 0.0f, 1.0f, -14.0f };
    int health = 100;
    bool isAlive = true;
    bool isDying = false;
    float respawnTimer = 0.0f;
    float deathProgress = 0.0f;
    float shootCooldown = 0.0f;
    float muzzleFlashTimer = 0.0f;
    float hitFlashTimer = 0.0f;
    float moveDirection = 1.0f;
    float walkCycle = 0.0f;
    float facingAngle = 0.0f;

    BoundingBox getBoundingBox() const {
        return {
            { position.x - 0.7f, position.y - 1.0f, position.z - 0.7f },
            { position.x + 0.7f, position.y + 1.2f, position.z + 0.7f }
        };
    }

    Vector3 getHeadPosition() const {
        return Vector3{ position.x, position.y + 1.15f, position.z };
    }

    Vector3 getRifleMuzzlePosition() const {
        // Rifle tip position extending outward towards facing angle
        float forwardX = sinf(facingAngle);
        float forwardZ = cosf(facingAngle);
        float rightX = cosf(facingAngle);
        float rightZ = -sinf(facingAngle);
        return Vector3{
            position.x + forwardX * 1.1f + rightX * 0.3f,
            position.y + 0.45f,
            position.z + forwardZ * 1.1f + rightZ * 0.3f
        };
    }

    void update(float dt, Vector3 playerPos, Player& player, std::vector<Vector3>& tracerBullets, const std::vector<Wall3D>& walls) {
        if (!isAlive) {
            respawnTimer -= dt;
            if (respawnTimer <= 0.0f) {
                isAlive = true;
                isDying = false;
                deathProgress = 0.0f;
                health = 100;
                position = Vector3{ (float)GetRandomValue(-8, 8), 1.0f, -14.0f };
            }
            return;
        }

        if (isDying) {
            deathProgress += dt * 3.0f;
            if (deathProgress >= 1.0f) deathProgress = 1.0f;
            return;
        }

        if (hitFlashTimer > 0.0f) hitFlashTimer -= dt;
        if (muzzleFlashTimer > 0.0f) muzzleFlashTimer -= dt;

        // Face towards the player dynamically
        float dx = playerPos.x - position.x;
        float dz = playerPos.z - position.z;
        facingAngle = atan2f(dx, dz);

        // Walking patrol AI & animation
        float speed = 2.4f;
        position.x += moveDirection * speed * dt;
        walkCycle += dt * 8.0f; // Walk animation phase

        if (position.x > 8.0f) { position.x = 8.0f; moveDirection = -1.0f; }
        if (position.x < -8.0f) { position.x = -8.0f; moveDirection = 1.0f; }

        // Bot Shooting AI: Only shoot when line of sight is NOT blocked by a wall
        shootCooldown -= dt;
        Vector3 muzzlePos = getRifleMuzzlePosition();
        float distToPlayer = Vector3Distance(muzzlePos, playerPos);

        if (shootCooldown <= 0.0f && distToPlayer < 30.0f) {
            // Check if any wall blocks line of sight
            Vector3 dirToPlayer = Vector3Normalize(Vector3Subtract(playerPos, muzzlePos));
            Ray sightRay = { muzzlePos, dirToPlayer };
            bool wallBlocks = false;

            for (const auto& w : walls) {
                RayCollision wc = GetRayCollisionBox(sightRay, w.getBox());
                if (wc.hit && wc.distance < distToPlayer) {
                    wallBlocks = true;
                    break;
                }
            }

            if (!wallBlocks) {
                // Shoot cooldown: 1.7s to 2.1s with natural delay
                shootCooldown = 1.7f + (float)GetRandomValue(0, 40) / 100.0f;
                muzzleFlashTimer = 0.12f;

                // Human-like Inaccuracy & Aim Spread (Spread scales with distance)
                float spreadAmount = 0.75f + (distToPlayer * 0.045f);
                float spreadX = ((float)GetRandomValue(-100, 100) / 100.0f) * spreadAmount;
                float spreadY = ((float)GetRandomValue(-100, 100) / 100.0f) * (spreadAmount * 0.6f);
                float spreadZ = ((float)GetRandomValue(-100, 100) / 100.0f) * spreadAmount;

                Vector3 targetPoint = Vector3{
                    playerPos.x + spreadX,
                    playerPos.y + spreadY,
                    playerPos.z + spreadZ
                };

                // Cast ray from bot rifle towards spread target
                Vector3 shotDir = Vector3Normalize(Vector3Subtract(targetPoint, muzzlePos));
                Ray botShotRay = { muzzlePos, shotDir };

                // Player Bounding Box for hit detection (Height: 1.8m, Radius: 0.5m)
                BoundingBox playerBox = {
                    { playerPos.x - 0.45f, playerPos.y - 1.8f, playerPos.z - 0.45f },
                    { playerPos.x + 0.45f, playerPos.y + 0.2f, playerPos.z + 0.45f }
                };

                RayCollision playerHit = GetRayCollisionBox(botShotRay, playerBox);

                tracerBullets.push_back(muzzlePos);
                if (playerHit.hit) {
                    // Bot scored a hit!
                    player.takeDamage(8);
                    tracerBullets.push_back(playerHit.point);
                } else {
                    // Bot missed! Bullet whizzes past player
                    tracerBullets.push_back(targetPoint);
                }
            }
        }
    }

    void takeDamage(int amount) {
        if (isDying || !isAlive) return;

        health -= amount;
        hitFlashTimer = 0.15f;
        if (health <= 0) {
            health = 0;
            isDying = true;
            deathProgress = 0.0f;
            respawnTimer = 3.5f;
            isAlive = false; 
        }
    }
};

// --- Global Game State (Stored in static data section of TargetGame.exe) ---
Player* g_Player = nullptr;

int main(int argc, char* argv[]) {
    const char* screenshotFile = nullptr;
    int screenshotDelayFrames = 40;
    int currentFrameCount = 0;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--screenshot") == 0 && i + 1 < argc) {
            screenshotFile = argv[++i];
        }
    }

    const int screenWidth = 1280;
    const int screenHeight = 720;

    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(screenWidth, screenHeight, "AntiCheat Lab - 3D FPS Arena");
    SetTargetFPS(60);

    // --- 3D Camera Setup ---
    Camera3D camera = { 0 };
    camera.position = Vector3{ 0.0f, 1.8f, 10.0f };
    camera.target = Vector3{ 0.0f, 1.8f, 0.0f };
    camera.up = Vector3{ 0.0f, 1.0f, 0.0f };
    camera.fovy = 65.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    // Mouse starts free; click to lock, ESC/TAB to unlock
    EnableCursor();
    bool cursorLocked = (screenshotFile != nullptr); // Auto-lock if taking screenshot

    // --- Load Assets & Perform SHA-256 Integrity Check ---
    const std::string EXPECTED_WALL_HASH = "f5d747c0e0d482375eed57a147ac8426cfa3eab367436edd1fe4770b7135c5b9";
    std::string actualWallHash = "";
    bool assetVerified = SecurityLab::VerifyAsset("assets/wall.png", EXPECTED_WALL_HASH, actualWallHash);

    Texture2D wallTexture = LoadTexture("assets/wall.png");
    Model wallModel = LoadModelFromMesh(GenMeshCube(1.0f, 1.0f, 1.0f));
    SetMaterialTexture(&wallModel.materials[0], MATERIAL_MAP_DIFFUSE, wallTexture);

    // Alpha cutout / discard shader: if pixel alpha < 0.5, GPU discards the pixel & depth
    // This allows transparent textures to act as a true wallhack!
    const char* alphaDiscardShaderCode =
        "#version 330\n"
        "in vec2 fragTexCoord;\n"
        "in vec4 fragColor;\n"
        "out vec4 finalColor;\n"
        "uniform sampler2D texture0;\n"
        "uniform vec4 colDiffuse;\n"
        "void main() {\n"
        "    vec4 texel = texture(texture0, fragTexCoord);\n"
        "    if (texel.a < 0.5) discard;\n"
        "    finalColor = texel * colDiffuse * fragColor;\n"
        "}\n";
    Shader alphaDiscardShader = LoadShaderFromMemory(0, alphaDiscardShaderCode);
    wallModel.materials[0].shader = alphaDiscardShader;

    // Game Entities (Player dynamically allocated on the Heap, pointer stored globally)
    g_Player = new Player();
    Bot bot;

    // Walls positioned between player spawn and enemy bot
    std::vector<Wall3D> walls = {
        { { 0.0f, 2.5f, -4.0f }, { 6.0f, 5.0f, 1.0f } },   // Center Wall
        { { -9.0f, 2.5f, -4.0f }, { 4.0f, 5.0f, 1.0f } },  // Left Wall
        { { 9.0f, 2.5f, -4.0f }, { 4.0f, 5.0f, 1.0f } }   // Right Wall
    };

    // Visual effect states
    std::vector<Vector3> activeTracers;
    float hitmarkerTimer = 0.0f;
    float shootAnimTimer = 0.0f;

    struct WallSpark {
        Vector3 point;
        float timer;
    };
    std::vector<WallSpark> wallSparks;

    // --- Ammo & Reload Animation State ---
    bool isReloading = false;
    float reloadTimer = 0.0f;
    const float RELOAD_DURATION = 1.3f;

    struct ShellCasing {
        Vector2 pos;
        Vector2 vel;
        float rot;
        float rotSpeed;
        float life;
    };
    std::vector<ShellCasing> casings;

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        // Toggle Cursor Lock with ESC or TAB
        if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_TAB)) {
            cursorLocked = !cursorLocked;
            if (cursorLocked) DisableCursor();
            else EnableCursor();
        }

        // Left-click to capture and lock mouse for FPS controls
        bool justLocked = false;
        if (!cursorLocked && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            cursorLocked = true;
            DisableCursor();
            justLocked = true;
        }

        // --- Camera & Movement Update with Wall Collision & Sliding ---
        if (cursorLocked) {
            Vector3 oldCamPos = camera.position;

            UpdateCamera(&camera, CAMERA_FIRST_PERSON);
            camera.position.y = 1.8f; // Lock player height

            // Calculate look direction vector to keep camera.target synchronized
            Vector3 lookOffset = Vector3Subtract(camera.target, oldCamPos);

            // 1. Collision against 3D Obstacle Walls (Player collision radius = 0.65m)
            const float pRadius = 0.65f;
            for (const auto& w : walls) {
                float minX = w.position.x - (w.size.x / 2.0f) - pRadius;
                float maxX = w.position.x + (w.size.x / 2.0f) + pRadius;
                float minZ = w.position.z - (w.size.z / 2.0f) - pRadius;
                float maxZ = w.position.z + (w.size.z / 2.0f) + pRadius;

                if (camera.position.x > minX && camera.position.x < maxX &&
                    camera.position.z > minZ && camera.position.z < maxZ) {
                    
                    // Test if X-movement alone caused the collision
                    bool xBlocked = (camera.position.x > minX && camera.position.x < maxX &&
                                     oldCamPos.z > minZ && oldCamPos.z < maxZ);
                    // Test if Z-movement alone caused the collision
                    bool zBlocked = (oldCamPos.x > minX && oldCamPos.x < maxX &&
                                     camera.position.z > minZ && camera.position.z < maxZ);

                    if (xBlocked) {
                        camera.position.x = oldCamPos.x;
                    }
                    if (zBlocked) {
                        camera.position.z = oldCamPos.z;
                    }
                    if (!xBlocked && !zBlocked) {
                        // Corner collision: push both back
                        camera.position.x = oldCamPos.x;
                        camera.position.z = oldCamPos.z;
                    }
                }
            }

            // 2. Outer Perimeter Boundaries (Arena: 40x50 -> X: [-18.5, 18.5], Z: [-23.5, 23.5])
            if (camera.position.x < -18.5f) camera.position.x = -18.5f;
            if (camera.position.x >  18.5f) camera.position.x =  18.5f;
            if (camera.position.z < -23.5f) camera.position.z = -23.5f;
            if (camera.position.z >  23.5f) camera.position.z =  23.5f;

            // Re-anchor camera target to maintain camera orientation
            camera.target = Vector3Add(camera.position, lookOffset);
        }

        // --- Reload Weapon Animation & Logic ---
        if (isReloading) {
            reloadTimer += dt;
            if (reloadTimer >= RELOAD_DURATION) {
                isReloading = false;
                reloadTimer = 0.0f;
                int needed = g_Player->maxAmmo - g_Player->ammo;
                int toLoad = (g_Player->reserveAmmo >= needed) ? needed : g_Player->reserveAmmo;
                g_Player->ammo += toLoad;
                g_Player->reserveAmmo -= toLoad;
            }
        } else if (IsKeyPressed(KEY_R) && g_Player->ammo < g_Player->maxAmmo && g_Player->reserveAmmo > 0) {
            isReloading = true;
            reloadTimer = 0.0f;
        }

        // --- Player Shooting Logic ---
        if (cursorLocked && !justLocked && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            if (g_Player->ammo > 0 && !isReloading) {
                g_Player->ammo--;
                shootAnimTimer = 0.09f;

                // Spawn flying brass shell casing animation
                ShellCasing casing;
                casing.pos = Vector2{ (float)screenWidth - 230.0f, (float)screenHeight - 140.0f };
                casing.vel = Vector2{ (float)GetRandomValue(180, 280), (float)GetRandomValue(-380, -260) };
                casing.rot = 0.0f;
                casing.rotSpeed = (float)GetRandomValue(300, 700);
                casing.life = 0.8f;
                casings.push_back(casing);

                // Cast Ray from Center of Screen (FPS Crosshair)
                Ray shootRay = GetScreenToWorldRay(Vector2{ (float)screenWidth / 2.0f, (float)screenHeight / 2.0f }, camera);

                // 1. Test collision against all 3D walls
                float closestWallDist = 9999.0f;
                Vector3 wallHitPoint = Vector3{ 0.0f, 0.0f, 0.0f };
                bool hitWall = false;

                for (const auto& w : walls) {
                    RayCollision wc = GetRayCollisionBox(shootRay, w.getBox());
                    if (wc.hit && wc.distance < closestWallDist) {
                        closestWallDist = wc.distance;
                        wallHitPoint = wc.point;
                        hitWall = true;
                    }
                }

                // 2. Test collision against Enemy Bot
                bool hitEnemy = false;
                Vector3 enemyHitPoint = Vector3{ 0.0f, 0.0f, 0.0f };
                if (bot.isAlive) {
                    RayCollision botHit = GetRayCollisionBox(shootRay, bot.getBoundingBox());
                    // Bullet only damages the bot if NO wall was in between!
                    if (botHit.hit && (!hitWall || botHit.distance < closestWallDist)) {
                        hitEnemy = true;
                        enemyHitPoint = botHit.point;
                        bot.takeDamage(35);
                        hitmarkerTimer = 0.15f;
                        if (!bot.isAlive) {
                            g_Player->score += 100;
                        }
                    }
                }

                // 3. Bullet Tracer & Impact FX
                activeTracers.push_back(camera.position);
                if (hitEnemy) {
                    // Tracer terminates on enemy body
                    activeTracers.push_back(enemyHitPoint);
                } else if (hitWall) {
                    // Tracer terminates on wall surface + creates impact sparks
                    activeTracers.push_back(wallHitPoint);
                    wallSparks.push_back({ wallHitPoint, 0.30f });
                } else {
                    // Tracer travels into the distance
                    activeTracers.push_back(Vector3Add(camera.position, Vector3Scale(shootRay.direction, 60.0f)));
                }
            } else if (g_Player->ammo == 0 && !isReloading && g_Player->reserveAmmo > 0) {
                // Auto trigger reload on empty click
                isReloading = true;
                reloadTimer = 0.0f;
            }
        }

        // Update Shell Casings
        for (auto it = casings.begin(); it != casings.end(); ) {
            it->pos.x += it->vel.x * dt;
            it->pos.y += it->vel.y * dt;
            it->vel.y += 980.0f * dt; // Gravity
            it->rot += it->rotSpeed * dt;
            it->life -= dt;
            if (it->life <= 0.0f || it->pos.y > screenHeight + 20) {
                it = casings.erase(it);
            } else {
                ++it;
            }
        }

        // Update Timers
        if (hitmarkerTimer > 0.0f) hitmarkerTimer -= dt;
        if (shootAnimTimer > 0.0f) shootAnimTimer -= dt;

        // Update Wall Impact Sparks
        for (auto it = wallSparks.begin(); it != wallSparks.end(); ) {
            it->timer -= dt;
            if (it->timer <= 0.0f) it = wallSparks.erase(it);
            else ++it;
        }

        // Update Bot AI (Passing walls for line-of-sight check)
        bot.update(dt, camera.position, *g_Player, activeTracers, walls);

        // --- Rendering ---
        BeginDrawing();
        ClearBackground(GetColor(0x1a1a24FF));

        // --- 3D Scene ---
        BeginMode3D(camera);

        // 1. Arena Floor
        DrawPlane(Vector3{ 0.0f, 0.0f, 0.0f }, Vector2{ 40.0f, 50.0f }, DARKGRAY);
        DrawGrid(40, 1.0f);

        // 2. Arena Outer Boundaries
        DrawCube(Vector3{ 0.0f, 3.0f, 25.0f }, 40.0f, 6.0f, 1.0f, GRAY);
        DrawCube(Vector3{ 0.0f, 3.0f, -25.0f }, 40.0f, 6.0f, 1.0f, GRAY);
        DrawCube(Vector3{ -20.0f, 3.0f, 0.0f }, 1.0f, 6.0f, 50.0f, GRAY);
        DrawCube(Vector3{ 20.0f, 3.0f, 0.0f }, 1.0f, 6.0f, 50.0f, GRAY);

        // 3. Draw Textured Walls
        for (const auto& w : walls) {
            if (wallTexture.id > 0) {
                // If wall.png is opaque, it covers everything behind it.
                // If wall.png was edited to be transparent, bot is visible through it!
                DrawModelEx(wallModel, w.position, Vector3{ 0.0f, 1.0f, 0.0f }, 0.0f, w.size, WHITE);
            } else {
                DrawCube(w.position, w.size.x, w.size.y, w.size.z, BROWN);
                DrawCubeWires(w.position, w.size.x, w.size.y, w.size.z, DARKGRAY);
            }
        }

        // 4. Draw Advanced Enemy Combat Bot (Humanoid Model & Animations)
        if (bot.isAlive) {
            float bob = fabsf(cosf(bot.walkCycle)) * 0.05f;
            Vector3 bPos = Vector3{ bot.position.x, bot.position.y + bob, bot.position.z };

            // Unit direction vectors for dynamic facing angle
            float fwdX = sinf(bot.facingAngle);
            float fwdZ = cosf(bot.facingAngle);
            float rgtX = cosf(bot.facingAngle);
            float rgtZ = -sinf(bot.facingAngle);

            Color armorColor = (bot.hitFlashTimer > 0.0f) ? WHITE : Color{ 48, 56, 68, 255 };      // Gunmetal tactical armor
            Color plateColor = (bot.hitFlashTimer > 0.0f) ? YELLOW : Color{ 190, 40, 40, 255 };     // Red armor plating
            Color coreColor  = (bot.hitFlashTimer > 0.0f) ? WHITE : Color{ 0, 230, 255, 255 };      // Cyan cyber-core glow

            // --- A. Animated Legs & Combat Boots (Walking cycle) ---
            float legSwing = sinf(bot.walkCycle) * 0.30f;
            // Left Leg
            Vector3 lLeg = Vector3{ bPos.x - rgtX * 0.22f + fwdX * legSwing, bPos.y - 0.45f, bPos.z - rgtZ * 0.22f + fwdZ * legSwing };
            DrawCube(lLeg, 0.20f, 0.70f, 0.22f, Color{ 32, 36, 44, 255 });
            DrawCube(Vector3{ lLeg.x + fwdX * 0.06f, lLeg.y - 0.35f, lLeg.z + fwdZ * 0.06f }, 0.22f, 0.16f, 0.30f, BLACK); // Boot

            // Right Leg
            Vector3 rLeg = Vector3{ bPos.x + rgtX * 0.22f - fwdX * legSwing, bPos.y - 0.45f, bPos.z + rgtZ * 0.22f - fwdZ * legSwing };
            DrawCube(rLeg, 0.20f, 0.70f, 0.22f, Color{ 32, 36, 44, 255 });
            DrawCube(Vector3{ rLeg.x - fwdX * 0.06f, rLeg.y - 0.35f, rLeg.z - fwdZ * 0.06f }, 0.22f, 0.16f, 0.30f, BLACK); // Boot

            // --- B. Torso, Armor Vest & Glowing Power Core ---
            Vector3 torsoPos = Vector3{ bPos.x, bPos.y + 0.35f, bPos.z };
            DrawCube(torsoPos, 0.65f, 0.80f, 0.42f, armorColor); // Base Torso
            // Front Chest Armor Plate
            DrawCube(Vector3{ torsoPos.x + fwdX * 0.12f, torsoPos.y + 0.08f, torsoPos.z + fwdZ * 0.12f }, 0.52f, 0.48f, 0.32f, plateColor);
            // Glowing Cyber-Core Reactor
            DrawSphere(Vector3{ torsoPos.x + fwdX * 0.24f, torsoPos.y + 0.08f, torsoPos.z + fwdZ * 0.24f }, 0.10f, coreColor);
            // Shoulder Pauldrons
            DrawCube(Vector3{ bPos.x - rgtX * 0.44f, bPos.y + 0.62f, bPos.z - rgtZ * 0.44f }, 0.24f, 0.24f, 0.30f, plateColor);
            DrawCube(Vector3{ bPos.x + rgtX * 0.44f, bPos.y + 0.62f, bPos.z + rgtZ * 0.44f }, 0.24f, 0.24f, 0.30f, plateColor);

            // --- C. Tactical Helmet & Glowing Cyber Visor ---
            Vector3 headPos = bot.getHeadPosition();
            DrawSphere(headPos, 0.30f, armorColor);
            // Helmet Cap & Side Guards
            DrawCube(Vector3{ headPos.x, headPos.y + 0.10f, headPos.z }, 0.48f, 0.18f, 0.48f, Color{ 26, 30, 38, 255 });
            // Glowing Red Cyber Visor Slit
            Vector3 visorPos = Vector3{ headPos.x + fwdX * 0.26f, headPos.y, headPos.z + fwdZ * 0.26f };
            DrawCube(visorPos, 0.34f, 0.10f, 0.12f, (bot.hitFlashTimer > 0.0f) ? YELLOW : RED);

            // --- D. 3D Combat Assault Rifle & Hands ---
            Vector3 rifleCenter = Vector3{ bPos.x + fwdX * 0.60f + rgtX * 0.25f, bPos.y + 0.38f, bPos.z + fwdZ * 0.60f + rgtZ * 0.25f };
            // Rifle Body
            DrawCube(rifleCenter, 0.14f, 0.18f, 0.65f, Color{ 22, 22, 26, 255 });
            // Rifle Scope
            DrawCube(Vector3{ rifleCenter.x, rifleCenter.y + 0.12f, rifleCenter.z }, 0.08f, 0.08f, 0.24f, BLACK);
            // Magazine
            DrawCube(Vector3{ rifleCenter.x, rifleCenter.y - 0.14f, rifleCenter.z - fwdZ * 0.05f }, 0.08f, 0.18f, 0.14f, DARKGRAY);

            // Bot Muzzle Flash Animation when firing
            if (bot.muzzleFlashTimer > 0.0f) {
                Vector3 muzzlePos = bot.getRifleMuzzlePosition();
                DrawSphere(muzzlePos, 0.26f, Fade(ORANGE, 0.9f));
                DrawSphere(muzzlePos, 0.14f, YELLOW);
                DrawSphere(muzzlePos, 0.06f, WHITE);
            }

            // --- E. In-World Floating 3D Health Bar ---
            Vector3 barPos = Vector3{ bot.position.x, bot.position.y + 1.85f, bot.position.z };
            DrawCube(barPos, 1.5f, 0.22f, 0.06f, BLACK);
            float hpPct = (float)bot.health / 100.0f;
            Color barCol = (hpPct > 0.5f) ? GREEN : ((hpPct > 0.25f) ? ORANGE : RED);
            DrawCube(Vector3{ barPos.x - 0.75f + (0.75f * hpPct), barPos.y, barPos.z + 0.01f }, 
                     1.46f * hpPct, 0.18f, 0.07f, barCol);
        } else if (bot.isDying) {
            // Death Ragdoll / Fall Animation: Bot tilts back onto floor
            float tilt = bot.deathProgress * 90.0f;
            float fallY = bot.position.y - (bot.deathProgress * 0.85f);
            Vector3 fallPos = Vector3{ bot.position.x, fallY, bot.position.z - (bot.deathProgress * 0.6f) };

            // Fallen robot silhouette on the floor
            DrawCube(fallPos, 0.7f, 0.25f, 1.4f, Color{ 60, 65, 75, 255 });
            DrawSphere(Vector3{ fallPos.x, fallPos.y + 0.15f, fallPos.z - 0.7f }, 0.28f, DARKGRAY);
            // Damaged smoking core
            DrawSphere(Vector3{ fallPos.x, fallPos.y + 0.18f, fallPos.z }, 0.12f, ORANGE);
        }

        // 5. Draw Bullet Tracers
        for (size_t i = 0; i + 1 < activeTracers.size(); i += 2) {
            DrawLine3D(activeTracers[i], activeTracers[i + 1], YELLOW);
        }
        activeTracers.clear();

        // 6. Draw Wall Impact Sparks
        for (const auto& s : wallSparks) {
            float fade = s.timer / 0.30f;
            DrawSphere(s.point, 0.16f * fade, YELLOW);
            DrawSphere(s.point, 0.08f * fade, ORANGE);
        }

        EndMode3D();

        // --- 2D HUD & First-Person Weapon Rendering ---

        // 1. Draw Flying Brass Shell Casings
        for (const auto& c : casings) {
            Rectangle rect = { c.pos.x, c.pos.y, 14.0f, 6.0f };
            Vector2 origin = { 7.0f, 3.0f };
            DrawRectanglePro(rect, origin, c.rot, Color{ 240, 195, 60, (unsigned char)(255 * (c.life / 0.8f)) });
            DrawRectangleLinesEx(rect, 1.0f, Fade(BLACK, c.life / 0.8f));
        }

        // 2. Draw Weapon Viewmodel with Recoil & Reload Animations
        Vector2 gunBase = Vector2{ (float)screenWidth - 280.0f, (float)screenHeight - 160.0f };
        float gunAngle = 0.0f;

        if (isReloading) {
            float progress = reloadTimer / RELOAD_DURATION; // 0.0 to 1.0
            // Gun dips downward and tilts during reload
            float reloadCurve = sinf(progress * PI);
            gunBase.y += reloadCurve * 150.0f;
            gunBase.x += reloadCurve * 40.0f;
            gunAngle = reloadCurve * 25.0f;

            // Ejected falling empty magazine (at ~25% reload progress)
            if (progress > 0.15f && progress < 0.65f) {
                float magFall = (progress - 0.15f) * 600.0f;
                DrawRectangle((int)gunBase.x + 25, (int)gunBase.y + 40 + (int)magFall, 26, 60, Color{ 40, 40, 45, 255 });
            }

            // Fresh magazine sliding up into magwell (at ~50% - 85% progress)
            if (progress >= 0.45f && progress < 0.90f) {
                float magInsert = (1.0f - ((progress - 0.45f) / 0.45f)) * 80.0f;
                DrawRectangle((int)gunBase.x + 25, (int)gunBase.y + 40 + (int)magInsert, 26, 60, Color{ 50, 50, 55, 255 });
                DrawRectangle((int)gunBase.x + 27, (int)gunBase.y + 42 + (int)magInsert, 22, 10, GOLD); // Brass visible in mag
            }
        } else if (shootAnimTimer > 0.0f) {
            gunBase.y += 15.0f; // Recoil animation kick
            gunBase.x -= 5.0f;
            // Muzzle flash
            DrawCircle((int)gunBase.x + 30, (int)gunBase.y - 40, 24.0f, Fade(ORANGE, 0.85f));
            DrawCircle((int)gunBase.x + 30, (int)gunBase.y - 40, 14.0f, Fade(YELLOW, 0.95f));
            DrawCircle((int)gunBase.x + 30, (int)gunBase.y - 40, 6.0f, WHITE);
        }

        // Draw Gun Body
        Rectangle gunLower = { gunBase.x, gunBase.y - 20, 70, 180 };
        Rectangle gunUpper = { gunBase.x + 15, gunBase.y - 80, 35, 70 };
        DrawRectangleRec(gunLower, DARKGRAY);
        DrawRectangleRec(gunUpper, BLACK);
        DrawRectangleLinesEx(gunLower, 2.0f, BLACK);
        DrawRectangleLinesEx(gunUpper, 2.0f, BLACK);

        // Slide ejection port
        DrawRectangle((int)gunBase.x + 18, (int)gunBase.y - 65, 20, 18, Color{ 25, 25, 28, 255 });

        // Center Crosshair
        int cx = screenWidth / 2;
        int cy = screenHeight / 2;
        DrawLine(cx - 10, cy, cx - 3, cy, GREEN);
        DrawLine(cx + 3, cy, cx + 10, cy, GREEN);
        DrawLine(cx, cy - 10, cx, cy - 3, GREEN);
        DrawLine(cx, cy + 3, cx, cy + 10, GREEN);

        // Hitmarker
        if (hitmarkerTimer > 0.0f) {
            DrawLine(cx - 8, cy - 8, cx + 8, cy + 8, RED);
            DrawLine(cx + 8, cy - 8, cx - 8, cy + 8, RED);
        }

        // Center Reload Progress Indicator
        if (isReloading) {
            int barWidth = 200;
            int barHeight = 12;
            int bx = screenWidth / 2 - barWidth / 2;
            int by = screenHeight / 2 + 50;
            float progress = reloadTimer / RELOAD_DURATION;

            DrawRectangle(bx - 10, by - 26, barWidth + 20, 48, Fade(BLACK, 0.75f));
            DrawText("RELOADING...", screenWidth / 2 - 45, by - 22, 14, GOLD);
            DrawRectangle(bx, by, barWidth, barHeight, DARKGRAY);
            DrawRectangle(bx, by, (int)(barWidth * progress), barHeight, GOLD);
            DrawRectangleLines(bx, by, barWidth, barHeight, WHITE);
        }

        // --- Bottom Left: Player Health ---
        DrawRectangle(20, screenHeight - 90, 280, 70, Fade(BLACK, 0.7f));
        DrawRectangleLines(20, screenHeight - 90, 280, 70, DARKGRAY);

        int curHealth = g_Player->health;
        Color hpColor = (curHealth > 50) ? GREEN : ((curHealth > 25) ? ORANGE : RED);
        DrawText(TextFormat("HEALTH: %d", curHealth), 35, screenHeight - 80, 22, hpColor);
        DrawRectangle(35, screenHeight - 52, 240, 14, DARKGRAY);
        DrawRectangle(35, screenHeight - 52, (int)(2.4f * curHealth), 14, hpColor);

        // --- Bottom Right: Ammo HUD with Visual Bullet Cartridge Rack ---
        int ammoBoxWidth = 320;
        int ammoBoxHeight = 110;
        int ammoBoxX = screenWidth - ammoBoxWidth - 20;
        int ammoBoxY = screenHeight - ammoBoxHeight - 20;

        DrawRectangle(ammoBoxX, ammoBoxY, ammoBoxWidth, ammoBoxHeight, Fade(BLACK, 0.8f));
        DrawRectangleLines(ammoBoxX, ammoBoxY, ammoBoxWidth, ammoBoxHeight, (g_Player->ammo <= 5) ? RED : DARKGRAY);

        // Numbers & Score
        Color ammoNumColor = (g_Player->ammo <= 5) ? RED : (g_Player->ammo <= 10 ? ORANGE : WHITE);
        DrawText(TextFormat("AMMO: %d / %d", g_Player->ammo, g_Player->reserveAmmo), ammoBoxX + 15, ammoBoxY + 12, 20, ammoNumColor);
        DrawText(TextFormat("SCORE: %d", g_Player->score), ammoBoxX + 195, ammoBoxY + 14, 16, GOLD);

        // Visual Bullet Icons Array (30 cartridges: 2 rows of 15 bullets)
        for (int i = 0; i < g_Player->maxAmmo; ++i) {
            int col = i % 15;
            int row = i / 15;
            int bx = ammoBoxX + 16 + (col * 19);
            int by = ammoBoxY + 45 + (row * 24);

            if (i < g_Player->ammo) {
                // Loaded brass cartridge
                DrawRectangle(bx, by + 4, 12, 14, Color{ 220, 175, 45, 255 }); // Brass casing
                DrawRectangle(bx + 2, by, 8, 4, Color{ 190, 95, 40, 255 });    // Copper bullet tip
                DrawRectangle(bx, by + 16, 12, 2, Color{ 160, 120, 30, 255 }); // Primer base
            } else {
                // Empty spent cartridge slot
                DrawRectangleLines(bx, by + 4, 12, 14, Color{ 60, 60, 65, 255 });
            }
        }

        // Low Ammo / Reload Alert
        if (g_Player->ammo == 0) {
            DrawText("[EMPTY - PRESS 'R' TO RELOAD]", ammoBoxX + 35, ammoBoxY + 92, 12, RED);
        } else if (g_Player->ammo <= 5) {
            DrawText("[LOW AMMO - 'R' TO RELOAD]", ammoBoxX + 50, ammoBoxY + 92, 12, ORANGE);
        }

        // Overlay Banner when Mouse is Unlocked
        if (!cursorLocked) {
            DrawRectangle(0, 0, screenWidth, screenHeight, Fade(BLACK, 0.45f));
            int bannerW = 460;
            int bannerH = 90;
            int bx = screenWidth / 2 - bannerW / 2;
            int by = screenHeight / 2 - bannerH / 2;

            DrawRectangle(bx, by, bannerW, bannerH, Fade(BLACK, 0.85f));
            DrawRectangleLines(bx, by, bannerW, bannerH, GOLD);
            DrawText("PAUSED / MOUSE UNLOCKED", screenWidth / 2 - 135, by + 16, 18, GOLD);
            DrawText("LEFT-CLICK TO CAPTURE MOUSE & PLAY", screenWidth / 2 - 160, by + 44, 14, WHITE);
            DrawText("Press [ESC] or [TAB] to release mouse anytime", screenWidth / 2 - 145, by + 66, 12, LIGHTGRAY);
        }

        EndDrawing();

        if (screenshotFile != nullptr) {
            currentFrameCount++;
            if (currentFrameCount >= screenshotDelayFrames) {
                TakeScreenshot(screenshotFile);
                break;
            }
        } else if (!assetVerified) {
            // Pop up native Windows prompt and exit game after Confirm (OK) button is clicked
            SecurityLab::PromptAssetTamperAlert("assets/wall.png", EXPECTED_WALL_HASH, actualWallHash);
            break;
        }
    }

    delete g_Player;
    g_Player = nullptr;
    UnloadModel(wallModel);
    UnloadTexture(wallTexture);
    CloseWindow();
    return 0;
}
