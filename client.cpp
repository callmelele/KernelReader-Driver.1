#include "serial.h" 
#include <windows.h>
#include <winioctl.h>
#include <iostream>
#include <TlHelp32.h>
#include <d3d11.h>
#include <dwmapi.h>
#include <string> 
#include <chrono>
#include <algorithm>
#include <cstdint>

#include "imgui/imgui.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_dx11.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dwmapi.lib")

#include "offsets.h"
#include "overlay.h"
#include "config.h"

struct Vector2 { float x, y; };
struct Vector3 {
    float x, y, z;
    float DistTo(Vector3 other) {
        return sqrtf(powf(other.x - x, 2) + powf(other.y - y, 2) + powf(other.z - z, 2));
    }
};
struct ViewMatrix { float matrix[4][4]; };

struct BoneData {
    Vector3 pos;
    char pad[20];
};

#define IOCTL_READ_MEMORY CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)

typedef struct _KERNEL_COMMAND_REQUEST {
    unsigned long ProcessId;
    unsigned __int64 Address;
    void* Buffer;
    unsigned __int64 Size;
    int Command;
} KERNEL_COMMAND_REQUEST;

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

template <typename T>
bool ReadMemory(HANDLE hDriver, DWORD pid, uintptr_t address, T& buffer) {
    if (address < 0x10000 || address > 0x7FFFFFFEFFFF) return false;
    KERNEL_COMMAND_REQUEST req = { pid, (unsigned __int64)address, &buffer, sizeof(T), 1 };
    return DeviceIoControl(hDriver, IOCTL_READ_MEMORY, &req, sizeof(req), &req, sizeof(req), nullptr, nullptr);
}

bool ReadString(HANDLE hDriver, DWORD pid, uintptr_t address, char* buffer, size_t size) {
    KERNEL_COMMAND_REQUEST req = { pid, (unsigned __int64)address, buffer, (unsigned __int64)size, 1 };
    return DeviceIoControl(hDriver, IOCTL_READ_MEMORY, &req, sizeof(req), &req, sizeof(req), nullptr, nullptr);
}

bool WorldToScreen(Vector3 pos, Vector2& screen, ViewMatrix vMatrix, int width, int height) {
    float w = vMatrix.matrix[3][0] * pos.x + vMatrix.matrix[3][1] * pos.y + vMatrix.matrix[3][2] * pos.z + vMatrix.matrix[3][3];
    if (w < 0.01f) return false;
    float x = (vMatrix.matrix[0][0] * pos.x + vMatrix.matrix[0][1] * pos.y + vMatrix.matrix[0][2] * pos.z + vMatrix.matrix[0][3]) / w;
    float y = (vMatrix.matrix[1][0] * pos.x + vMatrix.matrix[1][1] * pos.y + vMatrix.matrix[1][2] * pos.z + vMatrix.matrix[1][3]) / w;
    screen.x = (width / 2) + (0.5f * x * width + 0.5f);
    screen.y = (height / 2) - (0.5f * y * height + 0.5f);
    return true;
}

bool GetBoneScreenPos(HANDLE hDriver, DWORD pid, uintptr_t boneArray, int index, ViewMatrix vMatrix, int sw, int sh, Vector2& screenPos) {
    BoneData bone;
    if (ReadMemory(hDriver, pid, boneArray + (index * 32), bone)) {
        return WorldToScreen(bone.pos, screenPos, vMatrix, sw, sh);
    }
    return false;
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


void PerformMove(float x, float y, int sw, int sh) {
    float moveX = (x - (float)sw / 2.0f) * config::g_speed;
    float moveY = (y - (float)sh / 2.0f) * config::g_speed;

    static auto lastSend = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(now - lastSend).count();

    if (config::g_hardware && esp32.IsAnyConnected()) {
        if (elapsed < 1000 && abs(moveX) < 1.0f && abs(moveY) < 1.0f) return;
        lastSend = now;

        int8_t finalX = (int8_t)std::clamp((int)moveX, -127, 127);
        int8_t finalY = (int8_t)std::clamp((int)moveY, -127, 127);

        esp32.SendData(finalX, finalY, 0, config::g_useUDP);
    }
    else {
        mouse_event(MOUSEEVENTF_MOVE, (DWORD)moveX, (DWORD)moveY, NULL, NULL);
    }
}

void PerformClick() {
    if (config::g_hardware && esp32.IsAnyConnected()) {
        esp32.SendData(0, 0, 1, config::g_useUDP);
    }
    else {
        mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
        mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
    }
}

int main() {
    HANDLE hDriver = CreateFileA("\\\\.\\FinalFix_01", GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    DWORD pid = 0; while (pid == 0) pid = GetPidByName(L"cs2.exe");
    uintptr_t clientDll = 0; while (clientDll == 0) clientDll = GetModuleBase(pid, L"client.dll");

    Overlay ov;
    if (!ov.Init()) return 1;
    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);

    auto lastShotTime = std::chrono::steady_clock::now();
    auto targetFirstSeen = std::chrono::steady_clock::now();
    bool isTargetLocked = false;

    while (true) {
        ov.StartFrame();
        SetWindowPos(ov.hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
        MSG msg;
        while (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessage(&msg); }

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2((float)sw, (float)sh));
        ImGui::Begin("##ESP", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoInputs);
        ImDrawList* drawList = ImGui::GetWindowDrawList();

        ViewMatrix vMatrix;
        ReadMemory(hDriver, pid, clientDll + offsets::dwViewMatrix, vMatrix);
        uintptr_t entityList = 0, localPawn = 0;
        ReadMemory(hDriver, pid, clientDll + offsets::dwEntityList, entityList);
        ReadMemory(hDriver, pid, clientDll + offsets::dwLocalPlayerPawn, localPawn);

        int localTeam = 0;
        ReadMemory(hDriver, pid, localPawn + offsets::m_iTeamNum, localTeam);

        int localIndex = 1;
        ReadMemory(hDriver, pid, localPawn + offsets::m_iIDEntIndex, localIndex);

        Vector3 localPos;
        ReadMemory(hDriver, pid, localPawn + offsets::m_vOldOrigin, localPos);


        if (!config::g_hardware) {
            config::g_useUDP = false;
        }

        if (config::g_triggerbotEnabled) {
            auto now = std::chrono::steady_clock::now();
            int crosshairId = 0;
            bool enemyInCrosshair = false;

            if (ReadMemory(hDriver, pid, localPawn + offsets::m_iIDEntIndex, crosshairId) && crosshairId > 0) {
                uintptr_t entEntry = 0, entPawn = 0;
                ReadMemory(hDriver, pid, entityList + (8LL * (crosshairId >> 9) + 16), entEntry);
                ReadMemory(hDriver, pid, entEntry + (112LL * (crosshairId & 0x1FF)), entPawn);

                if (entPawn != 0 && entPawn != localPawn) {
                    int enemyTeam = 0;
                    ReadMemory(hDriver, pid, entPawn + offsets::m_iTeamNum, enemyTeam);

                    if (!config::g_enemiesOnly || (enemyTeam != localTeam)) {
                        enemyInCrosshair = true;
                    }
                }
            }

            if (enemyInCrosshair) {
                if (!isTargetLocked) { targetFirstSeen = now; isTargetLocked = true; }
                auto timeSinceSeen = std::chrono::duration_cast<std::chrono::milliseconds>(now - targetFirstSeen).count();
                auto timeSinceLastShot = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastShotTime).count();
                if (timeSinceSeen >= config::g_reaction && timeSinceLastShot >= config::g_shotInterval) {
                    PerformClick();
                    lastShotTime = now;
                }
            }
            else { isTargetLocked = false; }
        }

        float closestDist = FLT_MAX;
        Vector2 bestTarget = { 0, 0 };

        const int BONE_HEAD = 6;
        const int BONE_SPINE = 4;
        const int BONE_PELVIS = 0;
        std::vector<int> allBones = { 6, 4, 0, 8, 13, 22, 25 };


        for (int i = 1; i < 64; i++) {
            uintptr_t listEntry = 0, controller = 0, listEntry2 = 0, pawn = 0;
            ReadMemory(hDriver, pid, entityList + (8LL * (i >> 9) + 16), listEntry);
            ReadMemory(hDriver, pid, listEntry + (112LL * (i & 0x1FF)), controller);

            

            uint32_t pawnHandle = 0;
            ReadMemory(hDriver, pid, controller + 0x6C4, pawnHandle);
            if (pawnHandle == 0 || pawnHandle == 0xFFFFFFFF) continue;

            ReadMemory(hDriver, pid, entityList + (8LL * ((pawnHandle & 0x7FFF) >> 9) + 16), listEntry2);
            ReadMemory(hDriver, pid, listEntry2 + (112LL * (pawnHandle & 0x1FF)), pawn);
            if (!pawn || pawn == localPawn) continue;

            int enemyTeam = 0;
            ReadMemory(hDriver, pid, pawn + offsets::m_iTeamNum, enemyTeam);

            bool isEnemy = (enemyTeam != localTeam);
            bool shouldProcess = (!config::g_enemiesOnly || isEnemy);

            int health = 0;
            ReadMemory(hDriver, pid, pawn + offsets::m_iHealth, health);

            char playerName[128] = { 0 };
            ReadString(hDriver, pid, controller + offsets::m_iszPlayerName, playerName, sizeof(playerName));

            if (config::g_checkAlive) {
                if (health <= 0 || health > 100) continue;
            }

            if (!shouldProcess) continue;

            Vector3 enemyPos;
            ReadMemory(hDriver, pid, pawn + offsets::m_vOldOrigin, enemyPos);
            Vector2 screenPos;


            uintptr_t sceneNode = 0;
            ReadMemory(hDriver, pid, pawn + offsets::m_pGameSceneNode, sceneNode);
            uintptr_t boneArray = 0;
            ReadMemory(hDriver, pid, sceneNode + 0x160 + 0x80, boneArray);

            if (config::g_showDistance) {
                Vector3 enemyPos;
                ReadMemory(hDriver, pid, pawn + offsets::m_vOldOrigin, enemyPos);
                Vector2 screenOrigin;
                if (WorldToScreen(enemyPos, screenOrigin, vMatrix, sw, sh)) {
                    float distance = localPos.DistTo(enemyPos) / 39.37f;
                    std::string distStr = std::to_string((int)distance) + "m";
                    drawList->AddText(ImVec2(screenOrigin.x, screenOrigin.y), IM_COL32(255, 255, 255, 255), distStr.c_str());
                }
            }

            float currentThick = config::thickiness;
            float currentSize = 6.0f;
            float barHeight = 60.0f;
            float barX1 = 22;
            float barX2 = 18;
            float textScale = 1.0f;
            float nameY = 25;


            if (config::g_dynamicThickness) {
                float distance = localPos.DistTo(enemyPos) / 39.37f;
                float scalingFactor = (15.0f / (distance + 1.0f));

                currentThick = scalingFactor * config::thickiness;
                if (currentThick < 1.0f) currentThick = 1.0f;
                if (currentThick > config::thickiness * 2.0f) currentThick = config::thickiness * 2.0f;

                barHeight = scalingFactor * 90.0f;
                currentSize = scalingFactor * 7.0f;
                barX1 = scalingFactor * 25.0f;
                barX2 = scalingFactor * 21.0f;

                textScale = std::clamp(scalingFactor * 1.5f, 0.6f, 1.2f);
                nameY = scalingFactor * 30.0f;

            }

            if (WorldToScreen(enemyPos, screenPos, vMatrix, sw, sh)) {
                if (config::g_showSnaplines) {
                    ImVec2 startPoint = config::g_snapLinesBottom ? ImVec2(sw / 2.0f, sh) : ImVec2(sw / 2.0f, sh / 2.0f);
                    drawList->AddLine(startPoint, ImVec2(screenPos.x, screenPos.y), config::SnapLineColor, 1.0f);
                }


                if (config::g_showHealthBar) {
                    float currentHealthHeight = (health / 100.0f) * barHeight;
                    ImColor healthColor = ImColor::HSV(health * 0.01f * 0.35f, 1.0f, 1.0f);

                    drawList->AddRectFilled(ImVec2(screenPos.x - barX1, screenPos.y - barHeight), ImVec2(screenPos.x - barX2, screenPos.y), IM_COL32(0, 0, 0, 200));
                    drawList->AddRectFilled(ImVec2(screenPos.x - barX1, screenPos.y - currentHealthHeight), ImVec2(screenPos.x - barX2, screenPos.y), healthColor);
                }

            }

            if (boneArray) {
                Vector2 head, neck, spine, pelvis, lShoulder, lElbow, lHand, rShoulder, rElbow, rHand, lHip, lKnee, lFoot, rHip, rKnee, rFoot;

                if (config::g_showSkeleton &&
                    GetBoneScreenPos(hDriver, pid, boneArray, 5, vMatrix, sw, sh, neck) &&
                    GetBoneScreenPos(hDriver, pid, boneArray, 4, vMatrix, sw, sh, spine) &&
                    GetBoneScreenPos(hDriver, pid, boneArray, 0, vMatrix, sw, sh, pelvis) &&
                    GetBoneScreenPos(hDriver, pid, boneArray, 8, vMatrix, sw, sh, lShoulder) &&
                    GetBoneScreenPos(hDriver, pid, boneArray, 9, vMatrix, sw, sh, lElbow) &&
                    GetBoneScreenPos(hDriver, pid, boneArray, 11, vMatrix, sw, sh, lHand) &&
                    GetBoneScreenPos(hDriver, pid, boneArray, 13, vMatrix, sw, sh, rShoulder) &&
                    GetBoneScreenPos(hDriver, pid, boneArray, 14, vMatrix, sw, sh, rElbow) &&
                    GetBoneScreenPos(hDriver, pid, boneArray, 16, vMatrix, sw, sh, rHand) &&
                    GetBoneScreenPos(hDriver, pid, boneArray, 22, vMatrix, sw, sh, lHip) &&
                    GetBoneScreenPos(hDriver, pid, boneArray, 23, vMatrix, sw, sh, lKnee) &&
                    GetBoneScreenPos(hDriver, pid, boneArray, 24, vMatrix, sw, sh, lFoot) &&
                    GetBoneScreenPos(hDriver, pid, boneArray, 25, vMatrix, sw, sh, rHip) &&
                    GetBoneScreenPos(hDriver, pid, boneArray, 26, vMatrix, sw, sh, rKnee) &&
                    GetBoneScreenPos(hDriver, pid, boneArray, 27, vMatrix, sw, sh, rFoot))
                {
                    drawList->AddLine(ImVec2(neck.x, neck.y), ImVec2(spine.x, spine.y), config::color, currentThick);
                    drawList->AddLine(ImVec2(spine.x, spine.y), ImVec2(pelvis.x, pelvis.y), config::color, currentThick);
                    drawList->AddLine(ImVec2(neck.x, neck.y), ImVec2(lShoulder.x, lShoulder.y), config::color, currentThick);
                    drawList->AddLine(ImVec2(lShoulder.x, lShoulder.y), ImVec2(lElbow.x, lElbow.y), config::color, currentThick);
                    drawList->AddLine(ImVec2(lElbow.x, lElbow.y), ImVec2(lHand.x, lHand.y), config::color, currentThick);
                    drawList->AddLine(ImVec2(neck.x, neck.y), ImVec2(rShoulder.x, rShoulder.y), config::color, currentThick);
                    drawList->AddLine(ImVec2(rShoulder.x, rShoulder.y), ImVec2(rElbow.x, rElbow.y), config::color, currentThick);
                    drawList->AddLine(ImVec2(rElbow.x, rElbow.y), ImVec2(rHand.x, rHand.y), config::color, currentThick);
                    drawList->AddLine(ImVec2(pelvis.x, pelvis.y), ImVec2(lHip.x, lHip.y), config::color, currentThick);
                    drawList->AddLine(ImVec2(lHip.x, lHip.y), ImVec2(lKnee.x, lKnee.y), config::color, currentThick);
                    drawList->AddLine(ImVec2(lKnee.x, lKnee.y), ImVec2(lFoot.x, lFoot.y), config::color, currentThick);
                    drawList->AddLine(ImVec2(pelvis.x, pelvis.y), ImVec2(rHip.x, rHip.y), config::color, currentThick);
                    drawList->AddLine(ImVec2(rHip.x, rHip.y), ImVec2(rKnee.x, rKnee.y), config::color, currentThick);
                    drawList->AddLine(ImVec2(rKnee.x, rKnee.y), ImVec2(rFoot.x, rFoot.y), config::color, currentThick);
                }

                Vector2 currentTargetBonePos = { 0, 0 };
                bool foundBone = false;

                if (config::Selecteditem == 0) foundBone = GetBoneScreenPos(hDriver, pid, boneArray, BONE_HEAD, vMatrix, sw, sh, currentTargetBonePos);
                else if (config::Selecteditem == 1) foundBone = GetBoneScreenPos(hDriver, pid, boneArray, BONE_SPINE, vMatrix, sw, sh, currentTargetBonePos);
                else if (config::Selecteditem == 2) foundBone = GetBoneScreenPos(hDriver, pid, boneArray, BONE_PELVIS, vMatrix, sw, sh, currentTargetBonePos);
                else if (config::Selecteditem == 3) {
                    float minBoneDist = FLT_MAX;
                    for (int boneID : allBones) {
                        Vector2 tempPos;
                        if (GetBoneScreenPos(hDriver, pid, boneArray, boneID, vMatrix, sw, sh, tempPos)) {
                            float d = sqrtf(powf(tempPos.x - sw / 2, 2) + powf(tempPos.y - sh / 2, 2));
                            if (d < minBoneDist) { minBoneDist = d; currentTargetBonePos = tempPos; foundBone = true; }
                        }
                    }
                }

                if (foundBone && config::g_aimbotEnabled) {
                    float screenDist = sqrtf(powf(currentTargetBonePos.x - sw / 2, 2) + powf(currentTargetBonePos.y - sh / 2, 2));
                    if (screenDist < closestDist) {
                        closestDist = screenDist;
                        bestTarget = currentTargetBonePos;
                    }
                }

                if (GetBoneScreenPos(hDriver, pid, boneArray, 6, vMatrix, sw, sh, head)) {
                    if (config::g_showHeadDot) drawList->AddCircle(ImVec2(head.x, head.y), currentSize, config::Hcolor, 0, currentThick);
                    if (playerName[0] != '\0' && config::g_showNames) {
                        float textWidth = ImGui::CalcTextSize(playerName).x * textScale;
                        ImGui::SetWindowFontScale(textScale);
                        drawList->AddText(ImVec2(head.x - (textWidth / 2.0f), head.y - nameY), config::nameColor, playerName);
                        ImGui::SetWindowFontScale(1.0f);
                    }
                }
            }
        }

        if (config::g_aimbotEnabled && closestDist <= config::g_fov && GetAsyncKeyState(VK_RBUTTON)) {
            PerformMove(bestTarget.x, bestTarget.y, sw, sh);
        }

        if (config::g_showFOV) drawList->AddCircle(ImVec2(sw / 2.0f, sh / 2.0f), config::g_fov, config::FOVCOLOR, 100, 1.0f);

        ImGui::End();
        ov.DrawMenu();
        ov.EndFrame();

        if (GetAsyncKeyState(VK_END)) break;
        Sleep(1);
    }
    return 0;
}
