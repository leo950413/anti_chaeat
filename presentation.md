# Game Security & Anti-Cheat Demonstration: Part 1
## The 3D FPS Security Testbed & Core Attack Vectors

---

# Section 1: Overview of the Demo Game

To demonstrate security attacks and defensive mechanisms realistically, we developed a playable **3D First-Person Shooter (FPS)** using **C++** and **Raylib**. 

Rather than discussing abstract theory, this game serves as the **shared target platform** for all attacks demonstrated in this project.

```text
┌────────────────────────────────────────────────────────────────────────┐
│                        3D FPS TARGET ENVIRONMENT                       │
│                                                                        │
│   [ Player Client ]                     [ Obstacle Wall ]   [ Enemy ]  │
│   - First-Person Camera                   (wall.png)        (AI Bot)   │
│   - Weapon Raycast (L-Click)                 │                 │       │
│   - RAM Health Address                       ▼                 ▼       │
│   - Security HUD                  Blocks direct sight      Patrols &   │
│                                   under normal state      shoots back  │
└────────────────────────────────────────────────────────────────────────┘
```

### Core Components of the Target Game

1. **Player & First-Person Controller:**
   - 3D camera with mouse-look capture (`DisableCursor()`) and WASD spatial movement.
   - Animated viewmodel weapon with recoil kick, muzzle flashes, and screen-center raycast shooting (`GetScreenToWorldRay`).
   - Health system exposed in virtual memory for external inspection.

2. **Autonomous Enemy Bot:**
   - 3D humanoid target positioned behind obstacle geometry.
   - Patrols laterally and shoots back at the player with tracer bullets when in range (<25m), actively depleting player health.
   - Has hit detection (hitmarkers), an in-world 3D health bar, and automatic respawning.

3. **3D Obstacle Geometry (`wall.png`):**
   - Solid textured walls separating the player and the enemy bot.
   - In normal gameplay, the opaque texture completely conceals the bot from player line-of-sight.

4. **Security HUD & Telemetry Dashboard:**
   - Displays real-time health, active memory mode, and the raw hexadecimal pointer address of player health in RAM.
   - Provides hotkeys (`F1`–`F4`) to toggle defenses and simulated exploits during live demonstrations.

---

# Section 2: Attack Vectors & Defensive Solutions

Each attack vector below demonstrates a specific exploitation technique against the target game, followed immediately by its architectural defense and **critical code implementation**.

---

## Attack Vector 1: Asset / Texture Tampering (The Transparent Wallhack)

### 1. The Attack Concept
Games load graphics assets (textures, models, audio) from local storage. If an asset is stored as a loose file on disk without verification, an attacker can modify the asset outside the game.

### 2. How It Exploits the Game
A common question is: *"If the texture is loaded only once at startup, how does changing the file work?"*
* **The Mechanism:** The attacker alters the file **prior to launching the game**.
* They open `assets/wall.png` in an image editor and erase the pixels or set the Alpha (transparency) channel to `0%`.
* When the game launches, it reads the modified file from disk, decompresses it, and uploads the transparent pixels to **GPU VRAM**.
* For the entire game session, the 3D walls render completely invisible, exposing the hidden enemy bot behind them.

### 3. Vulnerable Code
The game blindly trusts the file system and loads the texture directly into memory:

```cpp
// VULNERABLE: Direct asset loading without integrity verification
Texture2D wallTexture = LoadTexture("assets/wall.png");
Model wallModel = LoadModelFromMesh(GenMeshCube(1.0f, 1.0f, 1.0f));
SetMaterialTexture(&wallModel.materials[0], MATERIAL_MAP_DIFFUSE, wallTexture);
```

---

### 4. The Defense: Cryptographic Asset Hashing
Before uploading any file to memory or the GPU, the client calculates its **SHA-256 cryptographic hash** and compares it against an authentic manifest hash. Due to the **Avalanche Effect**, altering even a single transparent pixel completely changes the hash.

### 5. Defensive Implementation Code

```cpp
// DEFENSE: Verify file signature before loading into GPU memory
#include "sha256.hpp"

const std::string EXPECTED_WALL_HASH = 
    "f5d747c0e0d482375eed57a147ac8426cfa3eab367436edd1fe4770b7135c5b9";

bool VerifyAssetIntegrity(const std::string& filePath, const std::string& expectedHash) {
    std::string actualHash = SecurityLab::SHA256::HashFile(filePath);
    
    if (actualHash.empty() || actualHash != expectedHash) {
        // Log security event and refuse to use corrupted asset
        std::cerr << "[ANTI-CHEAT ALERT] Tampering detected in: " << filePath << "\n";
        return false;
    }
    return true; // File is authentic
}

// In main initialization:
if (!VerifyAssetIntegrity("assets/wall.png", EXPECTED_WALL_HASH)) {
    // Block game launch or fall back to a default solid color
    TriggerAntiCheatAlert("Texture tampering detected!");
} else {
    wallTexture = LoadTexture("assets/wall.png");
}
```

---

## Attack Vector 2: Memory Modification (Cheat Engine & Value Freezing)

### 1. The Attack Concept & Introducing Cheat Engine
In Windows, every process executes within its own virtual address space (Ring 3). However, Windows provides administrative and debugging APIs that permit external software to read and write memory across process boundaries.

**Cheat Engine** is the industry-standard memory scanner and debugger used to demonstrate this vector:
* **Step 1 (First Scan):** Cheat Engine opens a handle to the game process via `OpenProcess`. It scans the entire allocated virtual memory space for the player's initial health value (e.g., `100`). This initial search typically returns thousands of matching memory addresses.
* **Step 2 (State Mutation):** The player takes damage from the enemy bot, reducing health to `70`.
* **Step 3 (Next Scan / Filtering):** Cheat Engine scans *only* the addresses found in Step 1 for the new value `70`. This filtering eliminates static variables and narrows the search down to the exact address of the player's health variable.
* **Step 4 (Modification & Value Freezing):** The attacker modifies the value to `9999`, or clicks the "Freeze" checkbox. When frozen, Cheat Engine's background thread executes `WriteProcessMemory` in a tight loop, instantly overwriting any damage the bot inflicts (**God Mode**).

### 2. How It Exploits the Game
In our testbed, the companion console tool (`demos/memory_scanner.cpp`) implements the exact same mechanism as Cheat Engine:
* It attaches to `TargetGame.exe`, binds to the health RAM address displayed on the security HUD, and continuously forces health back to `100`, making the player completely immune to gunfire.

### 3. Vulnerable Code
Storing critical gameplay state as a naked, static primitive variable:

```cpp
// VULNERABLE: Plain integer stored at a predictable, unencrypted memory offset
struct Player {
    int rawHealth = 100; // Easily located via Cheat Engine First/Next scans
    
    void takeDamage(int amount) {
        rawHealth -= amount;
    }
};
```

---

### 4. Defensive Solutions

To defeat memory scanners like Cheat Engine, games utilize two complementary defense strategies: **Value Obfuscation** and **Multi-Space Storage (Redundant State Validation)**.

---

### Defense Method A: Dynamic Value Obfuscation (XOR Encryption)

#### Principle:
Instead of storing the actual integer in RAM, the value is scrambled using a mathematical operator (such as XOR) combined with a dynamically generated key.
* When Cheat Engine searches for `100` or `70`, **zero results are returned** because the byte sequence for `100` never exists in memory.
* Every time the value is updated (taking damage or healing), a **new random key** is generated, ensuring even static address scanners cannot lock onto a pattern.

#### Critical Implementation Code:

```cpp
// DEFENSE A: XOR Obfuscated Integer with Dynamic Ephemeral Keys
class ProtectedInt {
public:
    ProtectedInt(int initialValue = 100) {
        set(initialValue);
    }

    // Decrypt on-the-fly only inside CPU registers during evaluation
    int get() const {
        return m_encrypted ^ m_key;
    }

    // Re-encrypt with a freshly generated random key on every change
    void set(int value) {
        m_key = (rand() << 16) | (rand() & 0xFFFF);
        m_encrypted = value ^ m_key;
    }

private:
    int m_encrypted; // In RAM, Cheat Engine sees only random scrambled bytes
    int m_key;       // Dynamic key changes on every hit
};
```

---

### Defense Method B: Multi-Space Storage & Redundant State Validation (Shadow Memory)

#### Principle:
A fundamental security principle is: **Never trust a single memory location for critical state.**
* The game stores the value in **multiple, independent memory locations** (a primary storage location and a distant "shadow" or canary location, often stored in an inverted or hashed form).
* During the game tick, a security routine cross-validates both memory locations:
  $$\text{PrimaryValue} \stackrel{?}{=} \text{ShadowValue}$$
* If an attacker uses Cheat Engine or an external trainer to modify the visible health address, they modify only the primary variable.
* On the next frame, the game detects the state mismatch without a legitimate damage event and immediately triggers an **anti-cheat alert / rollback**.

#### Critical Implementation Code:

```cpp
// DEFENSE B: Multi-Space Dual-State Storage with Canary Verification
class MultiSpaceHealth {
public:
    MultiSpaceHealth(int initialHealth = 100) {
        setValue(initialHealth);
    }

    void setValue(int val) {
        m_primaryHealth = val;
        // Shadow copy stored in a completely different representation (Bitwise NOT + Salt)
        m_shadowHealth = ~val ^ CANARY_SALT;
    }

    // Validates that external tools haven't modified one without the other
    bool validateIntegrity() const {
        int expectedShadow = ~m_primaryHealth ^ CANARY_SALT;
        return (m_shadowHealth == expectedShadow);
    }

    int getHealth() const {
        if (!validateIntegrity()) {
            // Tampering detected: Primary was modified, but shadow wasn't!
            TriggerAntiCheatAlert("Memory Tampering: Shadow memory mismatch!");
            return 0; // Penalize or crash
        }
        return m_primaryHealth;
    }

private:
    int m_primaryHealth; // Location A: The target an attacker finds
    int m_shadowHealth;  // Location B: Hidden shadow copy in separate space
    static const int CANARY_SALT = 0x5A5A5A5A;
};
```

```
// In Player struct:
struct HardenedPlayer {
    ProtectedInt health{100};

    void takeDamage(int amount) {
        int current = health.get() - amount;
        if (current < 0) current = 0;
        health.set(current); // Re-encrypts with a brand new key
    }
};
```

---

## Attack Vector 3: Information Exposure (ESP / Wallhack)

### 1. The Attack Concept
**ESP (Extra-Sensory Perception)** reads the 3D world coordinates of all active entities from game memory and projects them onto the player's 2D screen using a **World-to-Screen matrix**, rendering bounding boxes and snaplines over hidden enemies.

### 2. How It Exploits the Game
* **The Root Cause:** The game engine holds the complete 3D transform of the enemy bot at all times, relying exclusively on GPU depth-testing ($Z$-buffering) to occlude it behind the wall.
* In the demo game, pressing **`F3`** activates simulated ESP:
  * Draws a glowing 3D wireframe bounding box around the bot that ignores wall occlusion.
  * Draws a 3D snapline connecting the player to the bot.
  * Projects a 2D HUD tag over the bot showing exact distance and remaining health.

### 3. Vulnerable Principle Code
The client updates and retains position coordinates for all entities, regardless of whether they are visible to the local player:

```cpp
// VULNERABLE: Client maintains world state for occluded entities
void RenderScene(Camera3D camera, const std::vector<Enemy>& enemies) {
    for (const auto& enemy : enemies) {
        // The game always submits the enemy to the render pipeline,
        // relying solely on the GPU Z-buffer to hide it behind walls.
        DrawEnemy(enemy); 
    }
}
```

---

### kernal

## How is the kernal in windows work

## BYOVD 
