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

struct Vector2 { float x, y; };
struct Vector3 { float x, y, z; };
struct ViewMatrix { float matrix[4][4]; };

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

class Overlay {
public:
    HWND hwnd;
    ID3D11Device* device;
    ID3D11DeviceContext* deviceContext;
    IDXGISwapChain* swapChain;
    ID3D11RenderTargetView* renderTargetView;

    static LRESULT CALLBACK WindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) return true;
        switch (msg) {
        case WM_SIZE: return 0;
        case WM_SYSCOMMAND: if ((wParam & 0xFFF0) == SC_KEYMENU) return 0; break;
        case WM_DESTROY: PostQuitMessage(0); return 0;
        }
        return DefWindowProc(hWnd, msg, wParam, lParam);
    }

    bool Init() {
        WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WindowProc, 0, 0, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, L"CS2_OV", NULL };
        RegisterClassEx(&wc);
        hwnd = CreateWindowEx(WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED, wc.lpszClassName, L"CS2 Overlay", WS_POPUP, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), NULL, NULL, wc.hInstance, NULL);

        SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA);
        MARGINS margins = { -1 };
        DwmExtendFrameIntoClientArea(hwnd, &margins);

        DXGI_SWAP_CHAIN_DESC sd = {};
        sd.BufferCount = 2;
        sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.OutputWindow = hwnd;
        sd.SampleDesc.Count = 1;
        sd.Windowed = TRUE;
        sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

        D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, NULL, 0, D3D11_SDK_VERSION, &sd, &swapChain, &device, NULL, &deviceContext);
        ID3D11Texture2D* backBuffer;
        swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
        device->CreateRenderTargetView(backBuffer, NULL, &renderTargetView);
        backBuffer->Release();

        ShowWindow(hwnd, SW_SHOW);
        UpdateWindow(hwnd);
        ImGui::CreateContext();
        ImGui_ImplWin32_Init(hwnd);
        ImGui_ImplDX11_Init(device, deviceContext);
        return true;
    }
};

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
        while (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2((float)sw, (float)sh));
        ImGui::Begin("##ESP", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoInputs);
        ImDrawList* drawList = ImGui::GetWindowDrawList();

        ViewMatrix vMatrix;
        ReadMemory(hDriver, pid, clientDll + offsets::dwViewMatrix, vMatrix);
        uintptr_t entityList = 0;
        ReadMemory(hDriver, pid, clientDll + offsets::dwEntityList, entityList);
        uintptr_t localPawn = 0;
        ReadMemory(hDriver, pid, clientDll + offsets::dwLocalPlayerPawn, localPawn);

        //will use to draw for text on  screen later.
        /*int localHealth = 0;
        ReadMemory(hDriver, pid, localPawn + offsets::m_iHealth, localHealth);
        std::string healthDisplay = "HP: " + std::to_string(localHealth);
        ImGui::SetWindowFontScale(2.0f);
        drawList->AddText(ImVec2(100, 100), IM_COL32(255, 0, 0, 255), healthDisplay.c_str());*/

        for (int i = 1; i < 64; i++) {
            uintptr_t listEntry = 0, controller = 0, listEntry2 = 0, pawn = 0;
            if (!ReadMemory(hDriver, pid, entityList + (8LL * (i >> 9) + 16), listEntry)) continue;
            if (!ReadMemory(hDriver, pid, listEntry + (112LL * (i & 0x1FF)), controller)) continue;

            uint32_t pawnHandle = 0;
            ReadMemory(hDriver, pid, controller + 0x6C4, pawnHandle);
            if (pawnHandle == 0 || pawnHandle == 0xFFFFFFFF) continue;

            ReadMemory(hDriver, pid, entityList + (8LL * ((pawnHandle & 0x7FFF) >> 9) + 16), listEntry2);
            ReadMemory(hDriver, pid, listEntry2 + (112LL * (pawnHandle & 0x1FF)), pawn);
            if (!pawn || pawn == localPawn) continue;

            Vector3 worldPos;
            uintptr_t sceneNode;
            if (ReadMemory(hDriver, pid, pawn + offsets::m_pGameSceneNode, sceneNode)) {
                ReadMemory(hDriver, pid, sceneNode + offsets::m_vecAbsOrigin, worldPos);
            }

            Vector2 screenPos;
            if (WorldToScreen(worldPos, screenPos, vMatrix, sw, sh)) {
                drawList->AddCircle(ImVec2(screenPos.x, screenPos.y), 5.0f, IM_COL32(255, 0, 0, 255), 12, 1.5f);
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
        Sleep(5); 
    }
    return 0;
}
