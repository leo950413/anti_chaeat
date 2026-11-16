#include <windows.h>
#include <tlhelp32.h>
#include <iostream>
#include <string>

#pragma comment(lib, "user32.lib")

// Helper: Find target process ID by window title, fallback to process name
DWORD GetTargetProcessId() {
    // 1. Try finding by window title
    HWND hwnd = FindWindowA(NULL, "AntiCheat Lab - 3D FPS Arena");
    if (hwnd) {
        DWORD pid = 0;
        GetWindowThreadProcessId(hwnd, &pid);
        if (pid != 0) return pid;
    }

    // 2. Fallback: Search by executable name "TargetGame.exe"
    DWORD pid = 0;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W pe = { sizeof(pe) };
        if (Process32FirstW(snapshot, &pe)) {
            do {
                if (_wcsicmp(pe.szExeFile, L"TargetGame.exe") == 0) {
                    pid = pe.th32ProcessID;
                    break;
                }
            } while (Process32NextW(snapshot, &pe));
        }
        CloseHandle(snapshot);
    }
    return pid;
}

// Helper: Find module base address inside the remote target process
uintptr_t GetRemoteModuleBase(DWORD pid, const char* moduleName) {
    uintptr_t baseAddr = 0;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (snapshot != INVALID_HANDLE_VALUE) {
        MODULEENTRY32 me = { sizeof(me) };
        if (Module32First(snapshot, &me)) {
            do {
                if (_stricmp(me.szModule, moduleName) == 0) {
                    baseAddr = reinterpret_cast<uintptr_t>(me.modBaseAddr);
                    break;
                }
            } while (Module32Next(snapshot, &me));
        }
        CloseHandle(snapshot);
    }
    return baseAddr;
}

// Inject a DLL into the remote process and return its base address in the target
uintptr_t InjectDll(HANDLE hProcess, DWORD pid, const std::string& dllPath) {
    // Check if already injected
    uintptr_t existingBase = GetRemoteModuleBase(pid, "misc_dll.dll");
    if (existingBase) {
        std::cout << "[+] misc_dll.dll is already injected at remote address: 0x" << std::hex << existingBase << std::dec << "\n";
        return existingBase;
    }

    std::cout << "[*] Allocating memory for DLL path in target process...\n";
    size_t pathSize = dllPath.length() + 1;
    LPVOID pRemotePath = VirtualAllocEx(hProcess, NULL, pathSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!pRemotePath) {
        std::cerr << "[-] VirtualAllocEx failed! Error: " << GetLastError() << "\n";
        return 0;
    }

    std::cout << "[*] Writing DLL path to target memory...\n";
    if (!WriteProcessMemory(hProcess, pRemotePath, dllPath.c_str(), pathSize, NULL)) {
        std::cerr << "[-] WriteProcessMemory failed! Error: " << GetLastError() << "\n";
        VirtualFreeEx(hProcess, pRemotePath, 0, MEM_RELEASE);
        return 0;
    }

    FARPROC pLoadLibrary = GetProcAddress(GetModuleHandleA("Kernel32.dll"), "LoadLibraryA");
    std::cout << "[*] Creating remote thread calling LoadLibraryA...\n";
    HANDLE hThread = CreateRemoteThread(hProcess, NULL, 0,
                                        reinterpret_cast<LPTHREAD_START_ROUTINE>(pLoadLibrary),
                                        pRemotePath, 0, NULL);
    if (!hThread) {
        std::cerr << "[-] CreateRemoteThread failed! Error: " << GetLastError() << "\n";
        VirtualFreeEx(hProcess, pRemotePath, 0, MEM_RELEASE);
        return 0;
    }

    // Wait for LoadLibraryA to finish inside target
    WaitForSingleObject(hThread, INFINITE);
    CloseHandle(hThread);
    VirtualFreeEx(hProcess, pRemotePath, 0, MEM_RELEASE);

    // Sleep briefly to ensure the module is registered in PEB
    Sleep(150);

    uintptr_t remoteBase = GetRemoteModuleBase(pid, "misc_dll.dll");
    if (remoteBase) {
        std::cout << "[+] DLL successfully loaded at remote base: 0x" << std::hex << remoteBase << std::dec << "\n";
    } else {
        std::cerr << "[-] Failed to detect misc_dll.dll in target module list after injection.\n";
    }
    return remoteBase;
}

int main() {
    std::cout << "========================================================\n";
    std::cout << "  TargetGame - Remote Function Caller (StackOverflow 19798912)\n";
    std::cout << "========================================================\n\n";

    // 1. Locate the target process
    std::cout << "[*] Searching for 'AntiCheat Lab - 3D FPS Arena'...\n";
    DWORD pid = GetTargetProcessId();
    if (pid == 0) {
        std::cerr << "[-] Game not found! Please run TargetGame.exe first.\n";
        system("pause");
        return 1;
    }
    std::cout << "[+] Found TargetGame.exe! Process ID: " << pid << "\n";

    // 2. Open target process
    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!hProcess) {
        std::cerr << "[-] OpenProcess failed! Error: " << GetLastError() << "\n";
        system("pause");
        return 1;
    }

    // 3. Resolve absolute path of misc_dll.dll
    char fullDllPath[MAX_PATH];
    if (GetFullPathNameA("misc_dll.dll", MAX_PATH, fullDllPath, NULL) == 0 || GetFileAttributesA(fullDllPath) == INVALID_FILE_ATTRIBUTES) {
        // Try tools\\misc_dll.dll
        if (GetFullPathNameA("tools\\misc_dll.dll", MAX_PATH, fullDllPath, NULL) == 0 || GetFileAttributesA(fullDllPath) == INVALID_FILE_ATTRIBUTES) {
            std::cerr << "[-] Could not find misc_dll.dll! Please compile it first.\n";
            CloseHandle(hProcess);
            system("pause");
            return 1;
        }
    }
    std::cout << "[+] Target DLL Path: " << fullDllPath << "\n";

    // 4. Inject DLL into TargetGame
    uintptr_t remoteDllBase = InjectDll(hProcess, pid, fullDllPath);
    if (!remoteDllBase) {
        std::cerr << "[-] Failed to inject or locate misc_dll.dll in remote process.\n";
        CloseHandle(hProcess);
        system("pause");
        return 1;
    }

    // ------------------------------------------------------------------------
    // 5. Calculate Remote Function Address (Stack Overflow 19798912 Technique)
    // ------------------------------------------------------------------------
    // Step A: Load the same DLL locally in our own process
    HMODULE hLocalDll = LoadLibraryA(fullDllPath);
    if (!hLocalDll) {
        std::cerr << "[-] Failed to load DLL locally! Error: " << GetLastError() << "\n";
        CloseHandle(hProcess);
        system("pause");
        return 1;
    }

    // Step B: Get the local address of the exported function
    FARPROC pLocalFunc = GetProcAddress(hLocalDll, "RemoteAction");
    if (!pLocalFunc) {
        std::cerr << "[-] Could not find exported function 'RemoteAction' in DLL!\n";
        FreeLibrary(hLocalDll);
        CloseHandle(hProcess);
        system("pause");
        return 1;
    }

    // Step C: Compute the Relative Virtual Address (RVA / offset)
    uintptr_t offset = reinterpret_cast<uintptr_t>(pLocalFunc) - reinterpret_cast<uintptr_t>(hLocalDll);
    std::cout << "[+] Local DLL Base:     0x" << std::hex << (uintptr_t)hLocalDll << "\n";
    std::cout << "[+] Local RemoteAction: 0x" << (uintptr_t)pLocalFunc << "\n";
    std::cout << "[+] Function Offset:    0x" << offset << "\n";

    // Step D: Calculate the exact function address in the remote process
    uintptr_t pRemoteFunc = remoteDllBase + offset;
    std::cout << "[+] Remote Function Address: 0x" << pRemoteFunc << std::dec << "\n\n";

    // 6. Interactive Command Loop
    while (true) {
        std::cout << "\n--------------------------------------------\n";
        std::cout << "Choose an action to invoke remotely in the game:\n";
        std::cout << " [1] Add +500 Score\n";
        std::cout << " [2] Restore Full Health (100 HP)\n";
        std::cout << " [3] Refill Ammo (Magazine: 30, Reserve: 90)\n";
        std::cout << " [4] Prompt In-Game Status Alert\n";
        std::cout << " [0] Exit\n";
        std::cout << "Enter choice: ";

        int choice = -1;
        if (!(std::cin >> choice) || choice == 0) {
            break;
        }

        if (choice < 1 || choice > 4) {
            std::cout << "[-] Invalid choice, try again.\n";
            continue;
        }

        std::cout << "[*] Calling RemoteAction(" << choice << ") in TargetGame via CreateRemoteThread...\n";

        // Step E: Trigger the remote function execution with parameter
        HANDLE hExecThread = CreateRemoteThread(
            hProcess,
            NULL,
            0,
            reinterpret_cast<LPTHREAD_START_ROUTINE>(pRemoteFunc),
            reinterpret_cast<LPVOID>(static_cast<uintptr_t>(choice)),
            0,
            NULL
        );

        if (!hExecThread) {
            std::cerr << "[-] CreateRemoteThread for function call failed! Error: " << GetLastError() << "\n";
            continue;
        }

        // Wait for function to finish executing inside the game
        WaitForSingleObject(hExecThread, INFINITE);

        DWORD exitCode = 0;
        GetExitCodeThread(hExecThread, &exitCode);
        CloseHandle(hExecThread);

        std::cout << "[+] Remote function finished executing! Thread Exit Code: " << exitCode << "\n";
    }

    // Cleanup
    FreeLibrary(hLocalDll);
    CloseHandle(hProcess);
    std::cout << "[*] Exiting Remote Caller.\n";
    return 0;
}
