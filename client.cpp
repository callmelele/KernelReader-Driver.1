#include <windows.h>
#include <iostream>
#include <TlHelp32.h>
#include <d3d11.h>
#include <dwmapi.h>
#include <string> 

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

void AimAt(float x, float y, int sw, int sh) {
    float CenterX = sw / 2.0f;
    float CenterY = sh / 2.0f;

    float TargetX = config::smoothing;
    float TargetY = config::smoothing;

    if (x != 0) {
        if (x > CenterX) {
            TargetX = -(CenterX - x);
            TargetX /= 0.5f;
            if (TargetX + CenterX > CenterX * 2) TargetX = 0;
        }
        else {
            TargetX = x - CenterX;
            TargetX /= 1.5f;
            if (TargetX + CenterX < 0) TargetX = 0;
        }
    }

    if (y != 0) {
        if (y > CenterY) {
            TargetY = -(CenterY - y);
            TargetY /= 1.5f;
            if (TargetY + CenterY > CenterY * 2) TargetY = 0;
        }
        else {
            TargetY = y - CenterY;
            TargetY /= 1.5f;
            if (TargetY + CenterY < 0) TargetY = 0;
        }
    }

    mouse_event(MOUSEEVENTF_MOVE, (DWORD)TargetX, (DWORD)TargetY, NULL, NULL);
}

int main() {
    HANDLE hDriver = CreateFileA("\\\\.\\FinalFix_01", GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    DWORD pid = 0; while (pid == 0) pid = GetPidByName(L"cs2.exe");
    uintptr_t clientDll = 0; while (clientDll == 0) clientDll = GetModuleBase(pid, L"client.dll");

    Overlay ov;
    if (!ov.Init()) return 1;
    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);

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

        Vector3 localPos;
        ReadMemory(hDriver, pid, localPawn + offsets::m_vOldOrigin, localPos);

        float closestDist = FLT_MAX;
        Vector2 bestTarget = { 0, 0 };

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
                    float textWidth = ImGui::CalcTextSize(distStr.c_str()).x;
                    drawList->AddText(ImVec2(screenOrigin.x - (textWidth / 2.0f), screenOrigin.y), IM_COL32(255, 255, 255, 255), distStr.c_str());
                }
            }

            /* bone structure
                Torso, Head(6) → Neck(5) → Spine(4) → Pelvis(0)
                Right Arm, Neck(5) → R_Shoulder(13) → R_Elbow(14) → R_Hand(16)
                Left Arm, Neck(5) → L_Shoulder(8) → L_Elbow(9) → L_Hand(11)
                Right Leg, Pelvis(0) → R_Hip(25) → R_Knee(26) → R_Foot(28)
                Left Leg, Pelvis(0) → L_Hip(22) → L_Knee(23) → L_Foot(25)*/

            if (boneArray && config::g_showSkeleton || config::g_showHeadDot) {
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
                    GetBoneScreenPos(hDriver, pid, boneArray, 27, vMatrix, sw, sh, rFoot)) {


                    drawList->AddLine(ImVec2(neck.x, neck.y), ImVec2(spine.x, spine.y), config::color, config::thickiness);
                    drawList->AddLine(ImVec2(spine.x, spine.y), ImVec2(pelvis.x, pelvis.y), config::color, config::thickiness);

                    //upper body
                    drawList->AddLine(ImVec2(neck.x, neck.y), ImVec2(lShoulder.x, lShoulder.y), config::color, config::thickiness);
                    drawList->AddLine(ImVec2(lShoulder.x, lShoulder.y), ImVec2(lElbow.x, lElbow.y), config::color, config::thickiness);
                    drawList->AddLine(ImVec2(lElbow.x, lElbow.y), ImVec2(lHand.x, lHand.y), config::color, config::thickiness);
                    drawList->AddLine(ImVec2(neck.x, neck.y), ImVec2(rShoulder.x, rShoulder.y), config::color, config::thickiness);
                    drawList->AddLine(ImVec2(rShoulder.x, rShoulder.y), ImVec2(rElbow.x, rElbow.y), config::color, config::thickiness);
                    drawList->AddLine(ImVec2(rElbow.x, rElbow.y), ImVec2(rHand.x, rHand.y), config::color, config::thickiness);

                    //lower body
                    drawList->AddLine(ImVec2(pelvis.x, pelvis.y), ImVec2(lHip.x, lHip.y), config::color, config::thickiness);
                    drawList->AddLine(ImVec2(lHip.x, lHip.y), ImVec2(lKnee.x, lKnee.y), config::color, config::thickiness);
                    drawList->AddLine(ImVec2(lKnee.x, lKnee.y), ImVec2(lFoot.x, lFoot.y), config::color, config::thickiness);
                    drawList->AddLine(ImVec2(pelvis.x, pelvis.y), ImVec2(rHip.x, rHip.y), config::color, config::thickiness);
                    drawList->AddLine(ImVec2(rHip.x, rHip.y), ImVec2(rKnee.x, rKnee.y), config::color, config::thickiness);
                    drawList->AddLine(ImVec2(rKnee.x, rKnee.y), ImVec2(rFoot.x, rFoot.y), config::color, config::thickiness);
                }

                if (config::g_showFOV) {
                    drawList->AddCircle(
                        ImVec2(sw / 2.0f, sh / 2.0f),
                        config::g_fov,
                        config::FOVCOLOR,
                        100,                          
                        1.0f                          
                    );
                }

                if (GetBoneScreenPos(hDriver, pid, boneArray, 6, vMatrix, sw, sh, head)) {
                    if (config::g_showHeadDot)
                        drawList->AddCircle(ImVec2(head.x, head.y), 6.0f, config::Hcolor, 0, 3.0f);
                    if (config::g_aimbotEnabled) {
                        float crossDist = sqrtf(powf(head.x - sw / 2, 2) + powf(head.y - sh / 2, 2));
                        if (crossDist < closestDist) {
                            closestDist = crossDist;
                            bestTarget = head;
                        }
                    }

                }
            }
        }

        if (GetAsyncKeyState(VK_RBUTTON) && closestDist < config::g_fov) {
            AimAt(bestTarget.x, bestTarget.y, sw, sh);
        }

        ImGui::End();

        ov.DrawMenu(); 
        ov.EndFrame();


        if (GetAsyncKeyState(VK_END)) break;
        Sleep(1);
    }
    return 0;
}