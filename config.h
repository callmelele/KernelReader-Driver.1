#pragma once
#include <windows.h>
#include "imgui/imgui.h"

namespace config {
    inline bool g_showDistance = true;
    inline bool g_showSkeleton = true;
    inline bool g_showHeadDot = true;
    inline bool g_showFOV = false;
    inline bool g_showSnaplines = true;
    inline bool g_showHealthBar = true;
    inline bool g_showNames = true;


    inline bool g_aimbotEnabled = true;
    inline bool g_bezier = true;
    inline bool g_triggerbotAimbotOnly = true;
    inline bool g_triggerbotEnabled = false;
    inline bool g_menu = true;

    static int Selecteditem = 0;

    inline float thickiness = 2.5f;
    inline float fov = 150.0f;
    inline float g_fov = 100.0f;
    inline int opacity = 255;
    inline float g_reaction = 100;
    inline float g_shotInterval = 200;
    inline float g_speed = 1.0f;

    inline ImU32 color = IM_COL32(255, 255, 255, 255);
    inline ImU32 Hcolor = IM_COL32(255, 255, 255, 255);
    inline ImU32 FOVCOLOR = IM_COL32(255, 0, 0, 255);
    inline ImU32 SnapLineColor = IM_COL32(255, 255, 255, 155);
    inline ImU32 nameColor = IM_COL32(255, 255, 255, 255);

    //misc
    inline bool g_enemiesOnly = true;
    inline bool g_visibleOnly = true;
    inline bool g_checkAlive = true;
    inline bool g_snapLinesBottom = true;
    inline bool g_showBoxes = true;
    inline bool g_dynamicThickness = true;
    inline bool g_hardware = true;
    inline bool g_useUDP = true;


}
