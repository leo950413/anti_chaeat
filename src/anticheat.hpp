#pragma once
#include "sha256.hpp"
#include <iostream>
#include <cstdlib>

#ifdef _WIN32
#pragma comment(lib, "user32.lib")
extern "C" __declspec(dllimport) int __stdcall MessageBoxA(
    void* hWnd,
    const char* lpText,
    const char* lpCaption,
    unsigned int uType
);
#endif

namespace SecurityLab {

// Defense 1: Cryptographic Asset Verification (SHA-256)
inline bool VerifyAsset(const std::string& path, const std::string& expectedHash, std::string& actualHashOut) {
    actualHashOut = SHA256::HashFile(path);
    if (actualHashOut.empty()) {
        return false;
    }
    return (actualHashOut == expectedHash);
}

// Native Windows Security Alert Prompt on Hash Mismatch
inline void PromptAssetTamperAlert(const std::string& path, const std::string& expectedHash, const std::string& actualHash) {
    std::string msg =
        "[SECURITY ALERT] Asset Integrity Violation Detected!\n\n"
        "Target File : " + path + "\n\n"
        "Expected SHA-256 :\n" + expectedHash + "\n\n"
        "Actual SHA-256 :\n" + (actualHash.empty() ? "<FILE MISSING>" : actualHash) + "\n\n"
        "Result : HASH MISMATCH!\n"
        "Click OK to terminate the game process immediately.";

#ifdef _WIN32
    // 0x00000010L = MB_ICONERROR | MB_OK, 0x00040000L = MB_TOPMOST
    MessageBoxA(nullptr, msg.c_str(), "AntiCheat Lab - Asset Tampering Detected", 0x00000010L | 0x00040000L);
#endif
}

// Defense 2: Memory Obfuscation (XOR-protected integer)
class ProtectedInt {
public:
    ProtectedInt(int initialValue = 100) {
        set(initialValue);
    }

    int get() const {
        return m_encrypted ^ m_key;
    }

    void set(int value) {
        m_key = (rand() << 16) | (rand() & 0xFFFF);
        m_encrypted = value ^ m_key;
    }

    int getRawEncrypted() const { return m_encrypted; }
    int getRawKey() const { return m_key; }

private:
    int m_encrypted;
    int m_key;
};

} // namespace SecurityLab
