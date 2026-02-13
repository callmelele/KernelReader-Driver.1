#pragma once
#include <d3d11.h>
#include <dwmapi.h>
#include <string> 

#include "imgui/imgui.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_dx11.h"
#include "config.h" 

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dwmapi.lib")

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

class Overlay {
public:
    HWND hwnd;
    ID3D11Device* device;
    ID3D11DeviceContext* deviceContext;
    IDXGISwapChain* swapChain;
    ID3D11RenderTargetView* renderTargetView;
    bool showMenu = true;

    static LRESULT CALLBACK WindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) return true;
        if (msg == WM_DESTROY) { PostQuitMessage(0); return 0; }
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
        ImGui::CreateContext();
        ImGui_ImplWin32_Init(hwnd);
        ImGui_ImplDX11_Init(device, deviceContext);

        ImGui::StyleColorsDark();
        return true;
    }

    void StartFrame() {
        static bool insertPressed = false;
        if (GetAsyncKeyState(VK_INSERT) & 0x8000) {
            if (!insertPressed) {
                showMenu = !showMenu;

                LONG style = GetWindowLong(hwnd, GWL_EXSTYLE);
                if (showMenu) {
                    style &= ~WS_EX_TRANSPARENT; 
                    SetWindowLong(hwnd, GWL_EXSTYLE, style);
                }
                else {
                    style |= WS_EX_TRANSPARENT;
                    SetWindowLong(hwnd, GWL_EXSTYLE, style);
                }
            }
            insertPressed = true;
        }
        else {
            insertPressed = false;
        }

        MSG msg;
        while (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
    }

    void DrawMenu() {
        if (!showMenu) return;

        ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Leon CS2 - Settings", &showMenu)) {
            if (ImGui::CollapsingHeader("Visuals", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Checkbox("Skeleton ESP", &config::g_showSkeleton);
                ImGui::Checkbox("Head Dot", &config::g_showHeadDot);
                ImGui::Checkbox("Show Distance", &config::g_showDistance);
                ImGui::SliderFloat("Thickness", &config::thickiness, 1.0f, 5.0f);
            }

            ImGui::Separator();
            if (ImGui::CollapsingHeader("Colors")) {
                static float skeletonCol[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
                static float HeadCol[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
                static float FovCol[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
                if (ImGui::ColorEdit4("Skeleton Color", skeletonCol)) {
                    config::color = IM_COL32(skeletonCol[0] * 255, skeletonCol[1] * 255, skeletonCol[2] * 255, skeletonCol[3] * 255);
                }
                if (ImGui::ColorEdit4("Head Color", HeadCol)) {
                    config::Hcolor = IM_COL32(HeadCol[0] * 255, HeadCol[1] * 255, HeadCol[2] * 255, HeadCol[3] * 255);
                }
                if (ImGui::ColorEdit4("FOV color", FovCol)) {
                    config::FOVCOLOR = IM_COL32(FovCol[0] * 255, FovCol[1] * 255, FovCol[2] * 255, FovCol[3] * 255);
                }
            }

            ImGui::Separator();
            if (ImGui::CollapsingHeader("AimBot")) {
                ImGui::Checkbox("Aimbot", &config::g_aimbotEnabled);
                ImGui::Checkbox("Draw FOV", &config::g_showFOV);
                ImGui::SliderFloat("Smoothness", & config::smoothing, 0.0f, 10.0f);
                ImGui::SliderFloat("FOV", & config::g_fov, 10.0f, 500.0f);


            }
            

            ImGui::Separator();
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Press INSERT to hide/show menu");
        }
        ImGui::End();
    }

    void EndFrame() {
        ImGui::Render();
        float clear_color[4] = { 0.f, 0.f, 0.f, 0.f };
        deviceContext->OMSetRenderTargets(1, &renderTargetView, NULL);
        deviceContext->ClearRenderTargetView(renderTargetView, clear_color);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        swapChain->Present(1, 0);
    }
};