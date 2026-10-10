#include <math.h>
#include "Renderer.h"
#include "World.h"
#include "Player.h"
#include "Config.h"

void Renderer::DrawWorld(DWORD* pixels, const Map& map, const Player& p)
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
        float rayX = p.DirX() + p.PlaneX() * camX;
        float rayY = p.DirY() + p.PlaneY() * camX;

        int mapX = (int)p.X(), mapY = (int)p.Y();
        float deltaX = (rayX == 0) ? 1e30f : fabsf(1.0f / rayX);
        float deltaY = (rayY == 0) ? 1e30f : fabsf(1.0f / rayY);

        int stepX, stepY;
        float sideX, sideY;
        if (rayX < 0) { stepX = -1; sideX = (p.X() - mapX) * deltaX; }
        else { stepX = 1; sideX = (mapX + 1.0f - p.X()) * deltaX; }
        if (rayY < 0) { stepY = -1; sideY = (p.Y() - mapY) * deltaY; }
        else { stepY = 1; sideY = (mapY + 1.0f - p.Y()) * deltaY; }

        // DDA: step through grid cells until we hit a wall
        int side = 0;
        while (true)
        {
            if (sideX < sideY) { sideX += deltaX; mapX += stepX; side = 0; }
            else { sideY += deltaY; mapY += stepY; side = 1; }
            if (map.IsWall(mapX, mapY)) break;
        }

        float dist = (side == 0) ? sideX - deltaX : sideY - deltaY;
        if (dist < 0.0001f) dist = 0.0001f;
        m_depth[x] = dist;

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
}

// Mixes color into the pixel already there. alpha: 0..255
static void Blend(DWORD& dst, DWORD color, int alpha)
{
    int r = (int)((dst >> 16) & 255), g = (int)((dst >> 8) & 255), b = (int)(dst & 255);
    int cr = (int)((color >> 16) & 255), cg = (int)((color >> 8) & 255), cb = (int)(color & 255);
    r += (cr - r) * alpha / 255;
    g += (cg - g) * alpha / 255;
    b += (cb - b) * alpha / 255;
    dst = (DWORD)(r << 16 | g << 8 | b);
}

void Renderer::DrawMarker(DWORD* pixels, const Player& p,
    float worldX, float worldY, DWORD color, float pulse)
{
    // Into camera space: tx = sideways, ty = depth.
    float sx = worldX - p.X();
    float sy = worldY - p.Y();
    float invDet = 1.0f / (p.PlaneX() * p.DirY() - p.DirX() * p.PlaneY());
    float tx = invDet * (p.DirY() * sx - p.DirX() * sy);
    float ty = invDet * (-p.PlaneY() * sx + p.PlaneX() * sy);
    if (ty < 0.2f) return;   // behind or right on top of us

    const float PILLAR_WIDTH = 0.3f;   // tiles
    float planeLen = sqrtf(p.PlaneX() * p.PlaneX() + p.PlaneY() * p.PlaneY());

    int centerX = (int)((SCREEN_W / 2) * (1.0f + tx / ty));
    int halfWidth = (int)(PILLAR_WIDTH * 0.5f * (SCREEN_W / 2) / (planeLen * ty));
    if (halfWidth < 1) halfWidth = 1;

    // Same height as a wall at that distance: floor to ceiling.
    int height = (int)(SCREEN_H / ty);
    int bottom = SCREEN_H / 2 + height / 2;
    int top = SCREEN_H / 2 - height / 2;

    int x0 = centerX - halfWidth, x1 = centerX + halfWidth;
    int y0 = top < 0 ? 0 : top;
    int y1 = bottom > SCREEN_H ? SCREEN_H : bottom;
    if (x0 < 0) x0 = 0;
    if (x1 > SCREEN_W - 1) x1 = SCREEN_W - 1;

    for (int x = x0; x <= x1; ++x)
    {
        if (ty >= m_depth[x]) continue;   // a wall is in front

        // 0 in the middle of the pillar, 1 at its edges.
        float u = (float)(x - centerX) / halfWidth;
        if (u < 0) u = -u;
        float across = 1.0f - u * u;
        if (across <= 0.0f) continue;

        for (int y = y0; y < y1; ++y)
        {
            float up = (float)(bottom - y) / height;          // 0 floor, 1 top
            float strength = across * (1.0f - 0.7f * up) * (0.6f + 0.4f * pulse);
            Blend(pixels[y * SCREEN_W + x], color, (int)(230 * strength));
        }
    }
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