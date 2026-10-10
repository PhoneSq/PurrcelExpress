#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "Player.h"

class Renderer
{
public:
    // Draws the 3D view into a SCREEN_W x SCREEN_H pixel buffer (0x00RRGGBB).
    void DrawWorld(DWORD* pixels, const Player& p);

private:
    void DrawCrosshair(DWORD* pixels);
};
