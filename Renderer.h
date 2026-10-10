#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "Config.h"

class Map;
class Player;

// Draws the 3D view. Call DrawWorld first each frame (it fills the
// depth buffer), then markers, then the crosshair on top.
// pixels: SCREEN_W x SCREEN_H, 0x00RRGGBB
class Renderer
{
public:
    void DrawWorld(DWORD* pixels, const Map& map, const Player& player);

    // A glowing pillar standing on the floor at (worldX, worldY).
    // color: 0x00RRGGBB. pulse: 0..1, makes it breathe.
    void DrawMarker(DWORD* pixels, const Player& player,
        float worldX, float worldY, DWORD color, float pulse);

    void DrawCrosshair(DWORD* pixels);

private:
    // Distance to the wall in each screen column, so things
    // behind walls can be hidden.
    float m_depth[SCREEN_W];
};