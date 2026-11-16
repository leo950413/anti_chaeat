#include <windows.h>
#include <tlhelp32.h>
#include <iostream>

#pragma comment(lib, "user32.lib")

// Target offsets found via reverse engineering
const uintptr_t G_PLAYER_OFFSET = 0x283DE0; // TargetGame.exe + 0x283DE0 (pointer to Player)
const uintptr_t SCORE_OFFSET = 0x10;        // Player.score inside the Player object

// Find Process ID by window title
DWORD GetPidByWindowTitle(const char* windowTitle) {
    HWND hwnd = FindWindowA(NULL, windowTitle);
    if (!hwnd) {
        std::cerr << "[-] Game window not found! Please run TargetGame.exe first.\n";
        return 0;
    }
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    return pid;
}

// Get base address of TargetGame.exe in the target process
uintptr_t GetModuleBaseAddress(DWORD pid, const wchar_t* moduleName) {
    uintptr_t baseAddress = 0;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (snapshot != INVALID_HANDLE_VALUE) {
        MODULEENTRY32W me = { sizeof(me) };
        if (Module32FirstW(snapshot, &me)) {
            do {
                if (_wcsicmp(me.szModule, moduleName) == 0) {
                    baseAddress = (uintptr_t)me.modBaseAddr;
                    break;
                }
            } while (Module32NextW(snapshot, &me));
        }
        CloseHandle(snapshot);
    }
    return baseAddress;
}

int main() {
    std::cout << "==========================================\n";
    std::cout << "  TargetGame - External Memory Modifier   \n";
    std::cout << "==========================================\n\n";

    // 1. Locate game process
    DWORD pid = GetPidByWindowTitle("AntiCheat Lab - 3D FPS Arena");
    if (pid == 0) {
        system("pause");
        return 1;
    }
    std::cout << "[+] Found TargetGame! PID: " << pid << "\n";

    // 2. Open process with read/write permissions
    HANDLE hProc = OpenProcess(PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION, FALSE, pid);
    if (!hProc) {
        std::cerr << "[-] OpenProcess failed! Error: " << GetLastError() << "\n";
        system("pause");
        return 1;
    }

    // 3. Resolve Module Base Address
    uintptr_t baseAddr = GetModuleBaseAddress(pid, L"TargetGame.exe");
    if (baseAddr == 0) {
        std::cerr << "[-] Failed to find module base address.\n";
        CloseHandle(hProc);
        system("pause");
        return 1;
    }
    std::cout << "[+] TargetGame.exe Base Address: 0x" << std::hex << baseAddr << std::dec << "\n";

    // 4. Resolve pointer chain: [Base + 0x270A78] -> +0x10 (score)
    uintptr_t playerHeapAddr = 0;
    ReadProcessMemory(hProc, (LPCVOID)(baseAddr + G_PLAYER_OFFSET), &playerHeapAddr, sizeof(playerHeapAddr), NULL);
    if (playerHeapAddr == 0) {
        std::cerr << "[-] Player object not initialized in game yet. Please enter the arena.\n";
        CloseHandle(hProc);
        system("pause");
        return 1;
    }
    std::cout << "[+] Player Heap Address: 0x" << std::hex << playerHeapAddr << std::dec << "\n";

    uintptr_t scoreAddr = playerHeapAddr + SCORE_OFFSET;

    // 5. Read current score
    int currentScore = 0;
    ReadProcessMemory(hProc, (LPCVOID)scoreAddr, &currentScore, sizeof(currentScore), NULL);
    std::cout << "[+] Current Player Score: " << currentScore << "\n\n";

    // 6. Prompt and write new score
    std::cout << "Enter new score to write into memory: ";
    int newScore = 0;
    if (std::cin >> newScore) {
        if (WriteProcessMemory(hProc, (LPVOID)scoreAddr, &newScore, sizeof(newScore), NULL)) {
            std::cout << "[+] Successfully updated score to: " << newScore << "!\n";
        } else {
            std::cerr << "[-] Write failed! Error: " << GetLastError() << "\n";
        }
    }

    CloseHandle(hProc);
    std::cout << "\nFinished. Press any key to exit...\n";
    system("pause");
    return 0;
}
