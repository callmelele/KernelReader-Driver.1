#pragma once
#include <d3d11.h>
#include <dwmapi.h>
#include <string> 
#include <algorithm>

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
    ImFont* mainFont = nullptr;

    static LRESULT CALLBACK WindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) return true;
        switch (msg) {
        case WM_DESTROY: PostQuitMessage(0); return 0;
        case WM_SYSCOMMAND: if ((wParam & 0xfff0) == SC_KEYMENU) return 0; break;
        }
        return DefWindowProc(hWnd, msg, wParam, lParam);
    }

    void ApplyCyberStyle() {
        auto& style = ImGui::GetStyle();
        auto& colors = style.Colors;

        style.WindowRounding = 10.0f;
        style.ChildRounding = 8.0f;   // This gives the tabs/content that rounded border
        style.FrameRounding = 5.0f;   // Rounded checkboxes and sliders
        style.GrabRounding = 5.0f;
        style.PopupRounding = 5.0f;
        style.WindowBorderSize = 1.0f;
        style.FrameBorderSize = 1.0f;
        style.ChildBorderSize = 1.0f;
        style.ItemSpacing = ImVec2(12, 12);

        colors[ImGuiCol_WindowBg] = ImVec4(0.07f, 0.07f, 0.09f, 0.98f);
        colors[ImGuiCol_Border] = ImVec4(0.15f, 0.15f, 0.20f, 1.00f);
        colors[ImGuiCol_ChildBg] = ImVec4(0.09f, 0.09f, 0.12f, 1.00f);

        colors[ImGuiCol_Header] = ImVec4(0.12f, 0.50f, 0.80f, 0.40f);
        colors[ImGuiCol_HeaderHovered] = ImVec4(0.12f, 0.50f, 0.80f, 0.60f);
        colors[ImGuiCol_HeaderActive] = ImVec4(0.12f, 0.50f, 0.80f, 1.00f);

        colors[ImGuiCol_FrameBg] = ImVec4(0.12f, 0.12f, 0.16f, 1.00f);
        colors[ImGuiCol_FrameBgHovered] = ImVec4(0.16f, 0.16f, 0.22f, 1.00f);
        colors[ImGuiCol_CheckMark] = ImVec4(0.00f, 0.80f, 1.00f, 1.00f);
        colors[ImGuiCol_SliderGrab] = ImVec4(0.00f, 0.80f, 1.00f, 0.80f);
        colors[ImGuiCol_Button] = ImVec4(0.15f, 0.15f, 0.20f, 1.00f);
        colors[ImGuiCol_Text] = ImVec4(0.95f, 0.95f, 1.00f, 1.00f);
    }

    bool Init() {
        WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WindowProc, 0, 0, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, L"CS2_OV", NULL };
        RegisterClassEx(&wc);

        hwnd = CreateWindowEx(WS_EX_TOPMOST | WS_EX_LAYERED, wc.lpszClassName, L"Leon CS2", WS_POPUP,
            0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), NULL, NULL, wc.hInstance, NULL);

        if (!hwnd) return false;

        SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 255, LWA_ALPHA);
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

        HRESULT hr = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, NULL, 0, D3D11_SDK_VERSION, &sd, &swapChain, &device, NULL, &deviceContext);
        if (FAILED(hr)) return false;

        ID3D11Texture2D* backBuffer;
        swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
        device->CreateRenderTargetView(backBuffer, NULL, &renderTargetView);
        backBuffer->Release();

        ShowWindow(hwnd, SW_SHOW);
        ImGui::CreateContext();
        ImGui_ImplWin32_Init(hwnd);
        ImGui_ImplDX11_Init(device, deviceContext);

        ImGuiIO& io = ImGui::GetIO();
        mainFont = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", 18.0f); // Cleaner than default
        if (!mainFont) mainFont = io.Fonts->AddFontDefault();

        ApplyCyberStyle();
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
                    SetForegroundWindow(hwnd);
                }
                else {
                    style |= WS_EX_TRANSPARENT;
                    SetWindowLong(hwnd, GWL_EXSTYLE, style);
                }
            }
            insertPressed = true;
        }
        else { insertPressed = false; }

        ImGui::GetIO().MouseDrawCursor = showMenu;
        if (showMenu) ImGui::GetIO().ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
        else ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoMouse;

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
    }

    void DrawMenu() {
        if (!showMenu) return;

        ImGui::PushFont(mainFont);
        ImGui::SetNextWindowSize(ImVec2(600, 450), ImGuiCond_Once);

        ImGui::Begin("LEON CS2 // INTERNAL ENGINE", &showMenu, ImGuiWindowFlags_NoCollapse);

        ImGui::BeginChild("Sidebar", ImVec2(140, 0), true);
        static int activeTab = 0;
        const char* tabNames[] = { "VISUALS", "AIMBOT", "COLORS", "MISC" };
        for (int i = 0; i < 4; i++) {
            if (ImGui::Selectable(tabNames[i], activeTab == i, 0, ImVec2(0, 35))) {
                activeTab = i;
            }
        }
        ImGui::EndChild();

        ImGui::SameLine();

        ImGui::BeginChild("ContentArea", ImVec2(0, 0), true);

        if (activeTab == 0) { // Visuals
            ImGui::TextColored(ImVec4(0, 0.8f, 1, 1), "ESP CONFIGURATION");
            ImGui::Separator();
            ImGui::Checkbox("Skeleton ESP", &config::g_showSkeleton);
            ImGui::Checkbox("Head Dot", &config::g_showHeadDot);
            ImGui::Checkbox("Show Distance", &config::g_showDistance);
            ImGui::Checkbox("SnapLines", &config::g_showSnaplines);
            ImGui::Checkbox("HealthBar", &config::g_showHealthBar);
            ImGui::Checkbox("Show Names", &config::g_showNames);
            ImGui::Checkbox("Dynamic Visuals", &config::g_dynamicThickness);
            ImGui::SliderFloat("Line Thickness", &config::thickiness, 1.0f, 5.0f);
        }
        else if (activeTab == 1) { // Aimbot
            ImGui::TextColored(ImVec4(0, 0.8f, 1, 1), "MOUSE CONTROL");
            ImGui::Separator();
            ImGui::Checkbox("Aimbot", &config::g_aimbotEnabled);
            ImGui::Checkbox("Quadratic Bezier Curve", &config::g_bezier);
            ImGui::Checkbox("Draw FOV Circle", &config::g_showFOV);
            ImGui::SliderFloat("Speed Multiplier", &config::g_speed, 0.1f, 10.0f);
            ImGui::SliderFloat("FOV Radius", &config::g_fov, 10.0f, 500.0f);

            static const char* bones[]{ "Head","Spine","Pelvis", "Closest Bone" };
            ImGui::Combo("Target Priority", &config::Selecteditem, bones, IM_ARRAYSIZE(bones));

            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0, 0.8f, 1, 1), "TRIGGERBOT");
            ImGui::Separator();
            ImGui::Checkbox("Enable Triggerbot", &config::g_triggerbotEnabled);
            ImGui::SliderFloat("Reaction (ms)", &config::g_reaction, 0, 1000);
            ImGui::SliderFloat("Repeat (ms)", &config::g_shotInterval, 0, 1000);
        }
        else if (activeTab == 2) { // Colors
            ImGui::TextColored(ImVec4(0, 0.8f, 1, 1), "THEME & OVERLAY COLORS");
            ImGui::Separator();

            auto ColorPicker = [](const char* label, ImU32& configColor) {
                ImVec4 tempCol = ImGui::ColorConvertU32ToFloat4(configColor);
                if (ImGui::ColorEdit4(label, &tempCol.x)) {
                    configColor = ImGui::ColorConvertFloat4ToU32(tempCol);
                }
                };

            ColorPicker("Skeleton Color", config::color);
            ColorPicker("Head Dot Color", config::Hcolor);
            ColorPicker("FOV Circle Color", config::FOVCOLOR);
            ColorPicker("Snapline Color", config::SnapLineColor);
            ColorPicker("Name Text Color", config::nameColor);
        }
        else if (activeTab == 3) { // Misc
            ImGui::TextColored(ImVec4(0, 0.8f, 1, 1), "HARDWARE & FILTERS");
            ImGui::Separator();
            ImGui::Checkbox("Team Check", &config::g_enemiesOnly);
            ImGui::Checkbox("Alive Check", &config::g_checkAlive);
            ImGui::Checkbox("SnapLines at bottom", &config::g_snapLinesBottom);

            ImGui::Spacing();
            ImGui::TextDisabled("DATATEKNIK / HARDWARE SETTINGS");
            ImGui::Checkbox("Hardware Mouse (HID)", &config::g_hardware);
            ImGui::Checkbox("Hardware via UDP (Bridge)", &config::g_useUDP);
        }

        ImGui::EndChild();
        ImGui::End();
        ImGui::PopFont();
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
