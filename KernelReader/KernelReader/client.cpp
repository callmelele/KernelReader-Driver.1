#include <windows.h>
#include <iostream>
#include <TlHelp32.h>
#include "offsets.h"

#define IOCTL_READ_MEMORY CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)

enum COMMAND_TYPE {
    COMMAND_NONE = 0,
    COMMAND_READ_MEMORY,
    COMMAND_WRITE_MEMORY,
    COMMAND_GET_BASE_ADDRESS
};

typedef struct _KERNEL_COMMAND_REQUEST {
    unsigned long ProcessId;
    unsigned __int64 Address;
    void* Buffer;
    unsigned __int64 Size;
    COMMAND_TYPE Command;
} KERNEL_COMMAND_REQUEST, * PKERNEL_COMMAND_REQUEST;

template <typename T>
void ReadMemory(HANDLE hDriver, DWORD pid, uintptr_t address, T& buffer) {
    KERNEL_COMMAND_REQUEST req = { 0 };
    req.ProcessId = pid;
    req.Address = address;
    req.Buffer = &buffer;
    req.Size = sizeof(T);
    req.Command = COMMAND_READ_MEMORY;
    DeviceIoControl(hDriver, IOCTL_READ_MEMORY, &req, sizeof(req), &req, sizeof(req), nullptr, nullptr);
}

DWORD GetPidByName(const wchar_t* processName) {
    DWORD pid = 0;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W entry = { sizeof(entry) };
        if (Process32FirstW(snapshot, &entry)) {
            do {
                if (_wcsicmp(entry.szExeFile, processName) == 0) {
                    pid = entry.th32ProcessID;
                    break;
                }
            } while (Process32NextW(snapshot, &entry));
        }
        CloseHandle(snapshot);
    }
    return pid;
}

uintptr_t GetModuleBase(DWORD pid, const wchar_t* modName) {
    uintptr_t base = 0;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (snapshot != INVALID_HANDLE_VALUE) {
        MODULEENTRY32W entry = { sizeof(entry) };
        if (Module32FirstW(snapshot, &entry)) {
            do {
                if (_wcsicmp(entry.szModule, modName) == 0) {
                    base = (uintptr_t)entry.modBaseAddr;
                    break;
                }
            } while (Module32NextW(snapshot, &entry));
        }
        CloseHandle(snapshot);
    }
    return base;
}

int main() {
    HANDLE hDriver = CreateFileA("\\\\.\\FinalFix_01", GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    if (hDriver == INVALID_HANDLE_VALUE) {
        std::cout << "[-] Driver handle failed. Check mapping." << std::endl;
        std::cout << "[-] Press any key to exit..." << std::endl;
        std::cin.get();
        return 1;
    }

    std::cout << "[+] Connected to Driver" << std::endl;

    DWORD pid = 0;
    uintptr_t clientDll = 0;

    // Find CS2
    while (pid == 0) {
        pid = GetPidByName(L"cs2.exe");
        if (pid == 0) {
            std::cout << "\r[!] Waiting for CS2...     " << std::flush;
            Sleep(1000);
        }
    }

    std::cout << "\n[+] Found CS2! PID: " << pid << std::endl;

    // Get client.dll
    while (clientDll == 0) {
        clientDll = GetModuleBase(pid, L"client.dll");
        if (clientDll == 0) {
            std::cout << "\r[!] Waiting for client.dll...     " << std::flush;
            Sleep(500);
        }
    }

    std::cout << "[+] Found client.dll at: 0x" << std::hex << clientDll << std::endl;
    std::cout << "[+] Monitoring started..." << std::endl;

    // Main loop
    while (true) {
        uintptr_t localPawn = 0;
        ReadMemory(hDriver, pid, clientDll + offsets::dwLocalPlayerPawn, localPawn);

        if (localPawn != 0) {
            // Read health
            int health = 0;

            ReadMemory(hDriver, pid, localPawn + offsets::m_iHealth, health);

            // Read weapon and ammo
            uintptr_t weaponPtr = 0;
            ReadMemory(hDriver, pid, localPawn + offsets::m_pClippingWeapon, weaponPtr);

            int ammo = 0;
            

            if (weaponPtr != 0) {
                ReadMemory(hDriver, pid, weaponPtr + offsets::m_iClip1, ammo);
            }
            if (ammo < 0) {
                ammo = 0;
            }

            if (health > 0 && health <= 100) {
                std::cout << "\r[+] Health: " << std::dec << health
                    << " | Ammo: " << ammo << "      " << std::flush;
            }
        }
        else {
            std::cout << "\r[!] Waiting for player spawn...      " << std::flush;
        }

        Sleep(50);
    }

    CloseHandle(hDriver);
    return 0;
}