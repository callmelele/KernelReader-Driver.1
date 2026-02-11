#include <windows.h>
#include <iostream>
#include <TlHelp32.h>
#include <string>
#include "offsets.h"

struct Vector3 { float x, y, z; };

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
bool ReadMemory(HANDLE hDriver, DWORD pid, uintptr_t address, T& buffer) {
    if (address < 0x10000 || address > 0x7FFFFFFEFFFF) return false;
    KERNEL_COMMAND_REQUEST req = { pid, (unsigned __int64)address, &buffer, sizeof(T), COMMAND_READ_MEMORY };
    return DeviceIoControl(hDriver, IOCTL_READ_MEMORY, &req, sizeof(req), &req, sizeof(req), nullptr, nullptr);
}

DWORD GetPidByName(const wchar_t* processName) {
    DWORD pid = 0;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W entry = { sizeof(entry) };
        if (Process32FirstW(snapshot, &entry)) {
            do { if (_wcsicmp(entry.szExeFile, processName) == 0) { pid = entry.th32ProcessID; break; } } while (Process32NextW(snapshot, &entry));
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
            do { if (_wcsicmp(entry.szModule, modName) == 0) { base = (uintptr_t)entry.modBaseAddr; break; } } while (Module32NextW(snapshot, &entry));
        }
        CloseHandle(snapshot);
    }
    return base;
}


int main() {
    HANDLE hDriver = CreateFileA("\\\\.\\FinalFix_01", GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    if (hDriver == INVALID_HANDLE_VALUE) return 1;

    DWORD pid = 0;
    while (pid == 0) pid = GetPidByName(L"cs2.exe");
    uintptr_t clientDll = 0;
    while (clientDll == 0) clientDll = GetModuleBase(pid, L"client.dll");

    std::cout << "[+] System Fully Calibrated. Tracking Entities..." << std::endl;

    while (true) {
        uintptr_t entityList = 0;
        ReadMemory(hDriver, pid, clientDll + offsets::dwEntityList, entityList);

        uintptr_t localPawn = 0;
        ReadMemory(hDriver, pid, clientDll + offsets::dwLocalPlayerPawn, localPawn);

		uintptr_t weaponptr = 0;
		ReadMemory(hDriver, pid, localPawn + offsets::m_pClippingWeapon, weaponptr);

		int ammo = 0;
		ReadMemory(hDriver, pid, weaponptr + offsets::m_iClip1, ammo);
        if (ammo < 0)
			ammo = 0;
		int health = 0;
		ReadMemory(hDriver, pid, localPawn + offsets::m_iHealth, health);

        int localTeam = 0;
        if (localPawn) ReadMemory(hDriver, pid, localPawn + offsets::m_iTeamNum, localTeam);

        system("cls");
        std::cout << "--- CS2 Kernel Radar ---" << std::endl;
        std::cout << "Local Team: " << (localTeam == 2 ? "T" : "CT") << "\n" << std::endl;
		std::cout << "Ammo: " << ammo << " / " << "Health: " << health << "\n" << std::endl;

        for (int i = 1; i < 64; i++) {

            uintptr_t listEntry = 0;
            if (!ReadMemory(hDriver, pid, entityList + (8LL * (i >> 9) + 16), listEntry)) continue;

            uintptr_t controller = 0;
            if (!ReadMemory(hDriver, pid, listEntry + (112LL * (i & 0x1FF)), controller)) continue;

            uint32_t pawnHandle = 0;
            ReadMemory(hDriver, pid, controller + 0x6C4, pawnHandle); 
            if (pawnHandle == 0 || pawnHandle == 0xFFFFFFFF) continue;

            uintptr_t listEntry2 = 0;
            ReadMemory(hDriver, pid, entityList + (8LL * ((pawnHandle & 0x7FFF) >> 9) + 16), listEntry2);

            uintptr_t pawn = 0;
            ReadMemory(hDriver, pid, listEntry2 + (112LL * (pawnHandle & 0x1FF)), pawn);
            if (!pawn || pawn == localPawn) continue;

            int health = 0;
            int team = 0;
            ReadMemory(hDriver, pid, pawn + offsets::m_iHealth, health);
            ReadMemory(hDriver, pid, pawn + offsets::m_iTeamNum, team);

            if (health > 0 && health <= 100) {
                // Get Position for ESP/Radar
                uintptr_t sceneNode = 0;
                Vector3 pos = { 0, 0, 0 };
                if (ReadMemory(hDriver, pid, pawn + offsets::m_pGameSceneNode, sceneNode)) {
                    ReadMemory(hDriver, pid, sceneNode + offsets::m_vecAbsOrigin, pos);
                }

                const char* relation = (team == localTeam) ? "[TEAM]" : "[ENEMY]";
                printf("%-7s | HP: %-3d | Pos: %d, %d, %d\n", relation, health, (int)pos.x, (int)pos.y, (int)pos.z);
            }
        }
        Sleep(50);
    }
    return 0;
}
