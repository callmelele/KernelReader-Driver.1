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

struct Vector2 { float x, y; };
struct Vector3 { float x, y, z; };
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

int main() {
    HANDLE hDriver = CreateFileA("\\\\.\\FinalFix_01", GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    DWORD pid = 0; while (pid == 0) pid = GetPidByName(L"cs2.exe");
    uintptr_t clientDll = 0; while (clientDll == 0) clientDll = GetModuleBase(pid, L"client.dll");

    Overlay ov;
    if (!ov.Init()) return 1;
    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);

    while (true) {
        SetWindowPos(ov.hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
        MSG msg;
        while (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessage(&msg); }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2((float)sw, (float)sh));
        ImGui::Begin("##ESP", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoInputs);
        ImDrawList* drawList = ImGui::GetWindowDrawList();

        ViewMatrix vMatrix;
        ReadMemory(hDriver, pid, clientDll + offsets::dwViewMatrix, vMatrix);
        uintptr_t entityList = 0, localPawn = 0;
        ReadMemory(hDriver, pid, clientDll + offsets::dwEntityList, entityList);
        ReadMemory(hDriver, pid, clientDll + offsets::dwLocalPlayerPawn, localPawn);

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

            if (boneArray) {
                Vector2 head, neck;
                if (GetBoneScreenPos(hDriver, pid, boneArray, 6, vMatrix, sw, sh, head) &&
                    GetBoneScreenPos(hDriver, pid, boneArray, 5, vMatrix, sw, sh, neck)) {

                    drawList->AddCircleFilled(ImVec2(head.x, head.y), 4.0f, IM_COL32(255, 255, 0, 255));
                    drawList->AddLine(ImVec2(head.x, head.y), ImVec2(neck.x, neck.y), IM_COL32(255, 255, 255, 255), 2.0f);
                }
            }
        }

        ImGui::End();
        ImGui::Render();
        float clear_color[4] = { 0.f, 0.f, 0.f, 0.f };
        ov.deviceContext->OMSetRenderTargets(1, &ov.renderTargetView, NULL);
        ov.deviceContext->ClearRenderTargetView(ov.renderTargetView, clear_color);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        ov.swapChain->Present(1, 0);

        if (GetAsyncKeyState(VK_END)) break;
        Sleep(1);
    }
    return 0;
}
