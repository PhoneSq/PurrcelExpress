#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

class Map;
class Player;

// Draws the 3D view (raycast walls, floor, ceiling).
class Renderer
{
public:
    // pixels: SCREEN_W x SCREEN_H, 0x00RRGGBB
    void DrawWorld(DWORD* pixels, const Map& map, const Player& player);

private:
    void DrawCrosshair(DWORD* pixels);
};