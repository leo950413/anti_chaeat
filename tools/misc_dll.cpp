#include <windows.h>
#include <iostream>

#pragma comment(lib, "user32.lib")

// TargetGame structure layout (from src/main.cpp)
struct Player {
    int health;       // offset 0x00
    int ammo;         // offset 0x04
    int maxAmmo;      // offset 0x08
    int reserveAmmo;  // offset 0x0C
    int score;        // offset 0x10
};

// Known offset to g_Player pointer in TargetGame.exe
const uintptr_t G_PLAYER_OFFSET = 0x283DE0;

// Helper to safely get the game's g_Player pointer
Player* GetTargetPlayer() {
    uintptr_t gameBase = (uintptr_t)GetModuleHandleA(NULL);
    if (!gameBase) return nullptr;

    Player** ppPlayer = reinterpret_cast<Player**>(gameBase + G_PLAYER_OFFSET);
    if (!ppPlayer || IsBadReadPtr(ppPlayer, sizeof(Player*))) return nullptr;

    return *ppPlayer;
}

// ----------------------------------------------------------------------------
// Exported Function: RemoteAction
// This function is invoked remotely by the injector via CreateRemoteThread!
// The signature matches LPTHREAD_START_ROUTINE: DWORD WINAPI Func(LPVOID lpParam)
// ----------------------------------------------------------------------------
extern "C" __declspec(dllexport) DWORD WINAPI RemoteAction(LPVOID lpParam) {
    uintptr_t command = reinterpret_cast<uintptr_t>(lpParam);
    Player* player = GetTargetPlayer();

    char buffer[256];

    switch (command) {
    case 1: // Add 500 Score
        if (player) {
            player->score += 500;
            wsprintfA(buffer, "[RemoteAction] Score boosted by +500! Current Score: %d", player->score);
        } else {
            wsprintfA(buffer, "[RemoteAction] Player object not ready yet.");
        }
        std::cout << buffer << std::endl;
        MessageBoxA(NULL, buffer, "TargetGame - Injected RemoteAction", MB_OK | MB_ICONINFORMATION);
        break;

    case 2: // Heal to Full Health
        if (player) {
            player->health = 100;
            wsprintfA(buffer, "[RemoteAction] Player restored to full health (100 HP)!");
        } else {
            wsprintfA(buffer, "[RemoteAction] Player object not ready yet.");
        }
        std::cout << buffer << std::endl;
        MessageBoxA(NULL, buffer, "TargetGame - Injected RemoteAction", MB_OK | MB_ICONINFORMATION);
        break;

    case 3: // Refill Ammo
        if (player) {
            player->ammo = player->maxAmmo;
            player->reserveAmmo = 90;
            wsprintfA(buffer, "[RemoteAction] Ammo refilled! Magazine: %d | Reserve: %d", player->ammo, player->reserveAmmo);
        } else {
            wsprintfA(buffer, "[RemoteAction] Player object not ready yet.");
        }
        std::cout << buffer << std::endl;
        MessageBoxA(NULL, buffer, "TargetGame - Injected RemoteAction", MB_OK | MB_ICONINFORMATION);
        break;

    case 4: // In-Game Status Alert
    default:
        if (player) {
            wsprintfA(buffer, "[RemoteAction Executed!]\nHP: %d | Ammo: %d/%d | Score: %d",
                      player->health, player->ammo, player->reserveAmmo, player->score);
        } else {
            wsprintfA(buffer, "[RemoteAction Executed!]\nDLL is active in TargetGame.exe, waiting for game start.");
        }
        MessageBoxA(NULL, buffer, "RemoteAction Executed via Injected DLL", MB_OK | MB_ICONINFORMATION);
        break;
    }

    return 0; // Thread exit code
}

// Background thread spawned upon DLL attachment
DWORD WINAPI MainThread(LPVOID lpParam) {
    AllocConsole();
    FILE* f;
    freopen_s(&f, "CONOUT$", "w", stdout);

    std::cout << "==================================================" << std::endl;
    std::cout << "[+] misc_dll.dll successfully injected into TargetGame!" << std::endl;
    std::cout << "[+] Export 'RemoteAction' is ready for remote calls." << std::endl;
    std::cout << "==================================================" << std::endl;

    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        CreateThread(nullptr, 0, MainThread, hModule, 0, nullptr);
        break;

    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}
