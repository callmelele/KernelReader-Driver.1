#pragma once
#include <cstdint>

namespace offsets {
    constexpr uintptr_t dwLocalPlayerController = 0x22EF0B8;
    constexpr uintptr_t dwEntityList = 0x24AA0D8;
    constexpr uintptr_t dwLocalPlayerPawn = 0x2064AE0;
    constexpr uintptr_t dwViewMatrix = 0x230ADE0;
    constexpr uintptr_t m_hPlayerPawn = 0x90C;
    constexpr uintptr_t m_iszPlayerName = 0x6F8;

    constexpr uintptr_t m_hPawn = 0x6C4;
    constexpr uintptr_t m_iHealth = 0x354;            
    constexpr uintptr_t m_pClippingWeapon = 0x3DC0;   
    constexpr uintptr_t m_iClip1 = 0x18D0;  

    constexpr uintptr_t m_vecOrigin = 0x608;
    constexpr uintptr_t m_iTeamNum = 0x3F3;

    constexpr uintptr_t m_vecAbsOrigin = 0xD0;
    constexpr uintptr_t m_pGameSceneNode = 0x338;
    constexpr uintptr_t m_modelState = 0x160;
    constexpr uintptr_t m_vecViewOffset = 0xD58;

}
