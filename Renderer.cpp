#include <math.h>
#include "Renderer.h"
#include "Config.h"
#include "Map.h"

void Renderer::DrawWorld(DWORD* pixels, const Player& p)
{
    const DWORD CEILING = 0x101014;
    const DWORD FLOOR = 0x1C1C1C;

    for (int y = 0; y < SCREEN_H; ++y)
    {
        DWORD c = (y < SCREEN_H / 2) ? CEILING : FLOOR;
        for (int x = 0; x < SCREEN_W; ++x)
            pixels[y * SCREEN_W + x] = c;
    }

    for (int x = 0; x < SCREEN_W; ++x)
    {
        // ray direction for this column
        float camX = 2.0f * x / SCREEN_W - 1.0f;
        float rayX = p.dirX + p.planeX * camX;
        float rayY = p.dirY + p.planeY * camX;

        int mapX = (int)p.x, mapY = (int)p.y;
        float deltaX = (rayX == 0) ? 1e30f : fabsf(1.0f / rayX);
        float deltaY = (rayY == 0) ? 1e30f : fabsf(1.0f / rayY);

        int stepX, stepY;
        float sideX, sideY;
        if (rayX < 0) { stepX = -1; sideX = (p.x - mapX) * deltaX; }
        else { stepX = 1; sideX = (mapX + 1.0f - p.x) * deltaX; }
        if (rayY < 0) { stepY = -1; sideY = (p.y - mapY) * deltaY; }
        else { stepY = 1; sideY = (mapY + 1.0f - p.y) * deltaY; }

        // DDA: step through grid cells until we hit a wall
        int side = 0;
        while (true)
        {
            if (sideX < sideY) { sideX += deltaX; mapX += stepX; side = 0; }
            else { sideY += deltaY; mapY += stepY; side = 1; }
            if (IsWall(mapX, mapY)) break;
        }

        float dist = (side == 0) ? sideX - deltaX : sideY - deltaY;
        if (dist < 0.0001f) dist = 0.0001f;

        int lineH = (int)(SCREEN_H / dist);
        int top = SCREEN_H / 2 - lineH / 2;
        int bottom = SCREEN_H / 2 + lineH / 2;
        if (top < 0)           top = 0;
        if (bottom > SCREEN_H) bottom = SCREEN_H;

        // green, darker with distance, darker on Y-facing sides
        float light = 1.0f - dist / 12.0f;
        if (light < 0.15f) light = 0.15f;
        if (side == 1) light *= 0.7f;
        DWORD green = (DWORD)(220 * light);
        DWORD color = (green / 5) << 16 | green << 8 | (green / 5);

        for (int y = top; y < bottom; ++y)
            pixels[y * SCREEN_W + x] = color;
    }

    DrawCrosshair(pixels);
}

void Renderer::DrawCrosshair(DWORD* pixels)
{
    int cx = SCREEN_W / 2, cy = SCREEN_H / 2;
    for (int i = 2; i <= 5; ++i)
    {
        pixels[cy * SCREEN_W + cx + i] = 0xFFFFFF;
        pixels[cy * SCREEN_W + cx - i] = 0xFFFFFF;
        pixels[(cy + i) * SCREEN_W + cx] = 0xFFFFFF;
        pixels[(cy - i) * SCREEN_W + cx] = 0xFFFFFF;
    }
}