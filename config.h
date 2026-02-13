#pragma once
#include <windows.h>
#include "imgui/imgui.h"

namespace config {
    inline bool g_showDistance = true;
    inline bool g_showSkeleton = true;
    inline bool g_showHeadDot = true;
    inline bool g_showFOV = false;

    inline bool g_aimbotEnabled = true;
    inline bool g_menu = true;

    inline float thickiness = 2.5f;
    inline float fov = 150.0f;
    inline float smoothing = 1.5f;
    inline float g_fov = 100.0f;
    inline int opacity = 255;

    inline ImU32 color = IM_COL32(255, 255, 255, 255);
    inline ImU32 Hcolor = IM_COL32(255, 255, 255, 255);
    inline ImU32 FOVCOLOR = IM_COL32(255, 0, 0, 255);
}