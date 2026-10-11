#include <math.h>
#include "Renderer.h"
#include <objidl.h>    // GDI+ needs this (WIN32_LEAN_AND_MEAN leaves it out)
#include <gdiplus.h>   // PNG loading, built into Windows
#include "World.h"
#include "Player.h"
#include "Enemy.h"
#include "Config.h"
#include "AItexture.h"   // all the art made in code lives there

#ifdef _MSC_VER
#pragma comment(lib, "gdiplus.lib")
#endif

// =====================================================
// WORLD (sky, floor, walls)
// =====================================================

// Light color of the time of day (sunset = warm), applied to the whole world.
// Set by SetSky. 0xFFFFFF = no tint.
static int s_tintR = 255, s_tintG = 255, s_tintB = 255;

static inline DWORD Fog(DWORD c, float dist, DWORD haze, float hazeDist)
{
    if ((s_tintR & s_tintG & s_tintB) != 255)
        c = (DWORD)((((c >> 16) & 255) * s_tintR / 255) << 16 |
            (((c >> 8) & 255) * s_tintG / 255) << 8 |
            ((c & 255) * s_tintB / 255));

    float t = dist / hazeDist;
    if (t > 1.0f) t = 1.0f;
    t = t * t * (3.0f - 2.0f * t);   // smooth
    int r = (int)((c >> 16) & 255), g = (int)((c >> 8) & 255), b = (int)(c & 255);
    int fr = (haze >> 16) & 255, fg = (haze >> 8) & 255, fb = haze & 255;
    return Rgb(r + (int)((fr - r) * t), g + (int)((fg - g) * t), b + (int)((fb - b) * t));
}

// Warm street light on top of a color. light: 0..1
static inline DWORD Lit(DWORD c, float light)
{
    if (light <= 0.0f) return c;
    int r = (int)((c >> 16) & 255), g = (int)((c >> 8) & 255), b = (int)(c & 255);
    r = (int)(r * (1.0f + 0.9f * light) + 45 * light);
    g = (int)(g * (1.0f + 0.6f * light) + 28 * light);
    b = (int)(b * (1.0f + 0.15f * light));
    return Rgb(r, g, b);
}

void Renderer::DrawWorld(DWORD* pixels, const Map& map, const Camera& p)
{
    const int horizon = SCREEN_H / 2;

    // ----- Sky: the panorama, looked up by which way each column faces -----
    // (Fixed to the world, so it slides past like it's very far away.)
    // Front to back: skyline, clouds (drifting), sky.
    {
        const int W = SKY_PANO_W;
        int cloudShift = (int)m_cloudOffset;
        for (int x = 0; x < SCREEN_W; ++x)
        {
            float camX = 2.0f * x / SCREEN_W - 1.0f;
            float rx = p.DirX() + p.PlaneX() * camX, ry = p.DirY() + p.PlaneY() * camX;
            float a = atan2f(ry, rx);                         // -pi .. pi
            int u = (int)((a + 3.14159265f) * (W / 6.2831853f));
            if (u >= W) u -= W;
            int uc = u + cloudShift; uc %= W;

            for (int y = 0; y < horizon; ++y)
            {
                int row = y * W;
                DWORD s = m_skyline[row + u];
                if (s) { pixels[y * SCREEN_W + x] = s & 0xFFFFFF; continue; }
                DWORD c = m_skyClouds[row + uc];
                pixels[y * SCREEN_W + x] = c ? (c & 0xFFFFFF) : m_skyBase[row + u];
            }
        }
    }

    // ----- Floor: cast row by row -----
    float rd0x = p.DirX() - p.PlaneX(), rd0y = p.DirY() - p.PlaneY();
    float rd1x = p.DirX() + p.PlaneX(), rd1y = p.DirY() + p.PlaneY();
    for (int y = horizon; y < SCREEN_H; ++y)
    {
        float rowDist = (0.5f * SCREEN_H) / (y - horizon + 0.5f);
        float stepX = rowDist * (rd1x - rd0x) / SCREEN_W;
        float stepY = rowDist * (rd1y - rd0y) / SCREEN_W;
        float fx = p.X() + rowDist * rd0x;
        float fy = p.Y() + rowDist * rd0y;

        for (int x = 0; x < SCREEN_W; ++x)
        {
            float light = m_night ? map.LightAt(fx, fy) : 0.0f;
            pixels[y * SCREEN_W + x] = Fog(Lit(FloorColor(map, fx, fy), light),
                rowDist * (1.0f - 0.6f * light), m_hazeColor, m_hazeDist);
            fx += stepX;
            fy += stepY;
        }
    }

    // ----- Walls: DDA, one ray per column -----
    for (int x = 0; x < SCREEN_W; ++x)
    {
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

        // Low walls (hedges) the ray passes over on its way to a real wall.
        struct LowHit { float d1, d2; int side, mx, my; };
        const int MAX_LOW = 8;
        LowHit lows[MAX_LOW];
        int lowCount = 0;

        int side = 0;
        while (true)
        {
            if (sideX < sideY) { sideX += deltaX; mapX += stepX; side = 0; }
            else { sideY += deltaY; mapY += stepY; side = 1; }

            TileType t = map.TileAt(mapX, mapY);
            if (Map::IsLowWall(t))
            {
                // Remember where the ray enters and leaves this tile, keep going.
                if (lowCount < MAX_LOW)
                {
                    LowHit& h = lows[lowCount++];
                    h.d1 = (side == 0) ? sideX - deltaX : sideY - deltaY;
                    h.d2 = (sideX < sideY) ? sideX : sideY;
                    h.side = side; h.mx = mapX; h.my = mapY;
                }
                continue;
            }
            if (map.IsSolid(mapX, mapY)) break;   // props are sprites, rays pass them
        }

        float dist = (side == 0) ? sideX - deltaX : sideY - deltaY;
        if (dist < 0.0001f) dist = 0.0001f;
        m_depth[x] = dist;

        // Where along the wall face the ray hit (0..1).
        float wallX = (side == 0) ? p.Y() + dist * rayY : p.X() + dist * rayX;
        wallX -= floorf(wallX);
        if ((side == 0 && rayX > 0) || (side == 1 && rayY < 0)) wallX = 1.0f - wallX;

        TileType tile = map.TileAt(mapX, mapY);

        // The tile on the side the ray came from: a path there means a front door.
        int outX = (side == 0) ? mapX - stepX : mapX;
        int outY = (side == 1) ? mapY - stepY : mapY;
        bool door = map.TileAt(outX, outY) == TILE_PATH && map.IsHouse(mapX, mapY);

        // Is the left / right side of this wall section a real corner?
        // (The neighbor along the face isn't the same building.)
        int alongX = (side == 0) ? 0 : 1, alongY = (side == 0) ? 1 : 0;
        bool flipped = (side == 0 && rayX > 0) || (side == 1 && rayY < 0);
        TileType lowSide = map.TileAt(mapX - alongX, mapY - alongY);   // where u = 0 before flipping
        TileType highSide = map.TileAt(mapX + alongX, mapY + alongY);
        bool lowEdge = lowSide != tile || map.IsSolid(outX - alongX, outY - alongY);
        bool highEdge = highSide != tile || map.IsSolid(outX + alongX, outY + alongY);
        bool edgeL = flipped ? highEdge : lowEdge;
        bool edgeR = flipped ? lowEdge : highEdge;

        // Street light on this wall face (sampled just outside it).
        float hitX = p.X() + rayX * dist - rayX * 0.05f;
        float hitY = p.Y() + rayY * dist - rayY * 0.05f;
        float light = m_night ? map.LightAt(hitX, hitY) : 0.0f;
        float fogDist = dist * (1.0f - 0.6f * light);

        float lineH = SCREEN_H / dist;
        float top = horizon - lineH * 0.5f;
        int y0 = (int)top;           if (y0 < 0) y0 = 0;
        int y1 = (int)(top + lineH); if (y1 > SCREEN_H) y1 = SCREEN_H;

        float shade = (side == 1) ? 0.78f : 1.0f;   // corners read clearly

        for (int y = y0; y < y1; ++y)
        {
            float v = (y - top) / lineH;
            DWORD c = WallColor(tile, mapX, mapY, wallX, v, door, m_night, edgeL, edgeR);
            pixels[y * SCREEN_W + x] = Fog(Lit(Scale(c, shade), light), fogDist, m_hazeColor, m_hazeDist);
        }

        // ----- Roofs above the walls -----
        if (map.IsHouse(mapX, mapY))
            DrawRoofColumn(pixels, x, map, p, rayX, rayY, dist, top);

        // ----- Low walls (hedges), far to near, over whatever is behind them -----
        m_lowDepth[x] = 1e9f;
        m_lowTop[x] = SCREEN_H;
        const float eyeAbove = 0.5f - HEDGE_HEIGHT;   // the eye is a bit above the hedge tops

        for (int i = lowCount - 1; i >= 0; --i)
        {
            const LowHit& h = lows[i];
            float d1 = h.d1 < 0.05f ? 0.05f : h.d1;
            float d2 = h.d2 < d1 ? d1 : h.d2;

            float bottom = horizon + 0.5f * SCREEN_H / d1;           // where it meets the ground
            float top1 = horizon + eyeAbove * SCREEN_H / d1;       // top edge of the near face
            float top2 = horizon + eyeAbove * SCREEN_H / d2;       // far edge of the top surface

            // Position along the face in world units (mirrored on the far
            // sides so the pattern reads left-to-right from every angle).
            float along = (h.side == 0) ? p.Y() + d1 * rayY : p.X() + d1 * rayX;
            if ((h.side == 0 && rayX > 0) || (h.side == 1 && rayY < 0)) along = -along;

            float hx = p.X() + rayX * d1 - rayX * 0.05f, hy = p.Y() + rayY * d1 - rayY * 0.05f;
            float hl = m_night ? map.LightAt(hx, hy) : 0.0f;
            float hShade = (h.side == 1) ? 0.85f : 1.0f;

            // Side face
            int fy0 = (int)top1;   if (fy0 < 0) fy0 = 0;
            int fy1 = (int)bottom; if (fy1 > SCREEN_H) fy1 = SCREEN_H;
            for (int y = fy0; y < fy1; ++y)
            {
                float v = (y - top1) / (bottom - top1);
                DWORD c = HedgeSideColor(along, v);
                pixels[y * SCREEN_W + x] = Fog(Lit(Scale(c, hShade), hl), d1 * (1.0f - 0.6f * hl),
                    m_hazeColor, m_hazeDist);
            }

            // Top surface, seen from slightly above
            // Up close the top edge can be far below the screen: clamp both ends.
            int ty0 = (int)top2; if (ty0 < 0) ty0 = 0; if (ty0 > SCREEN_H) ty0 = SCREEN_H;
            int ty1 = fy0;       if (ty1 > SCREEN_H) ty1 = SCREEN_H;
            for (int y = ty0; y < ty1; ++y)
            {
                float rowDist = eyeAbove * SCREEN_H / (y + 0.5f - horizon);
                float wx = p.X() + rayX * rowDist, wy = p.Y() + rayY * rowDist;
                DWORD c = HedgeTopColor(wx, wy);
                pixels[y * SCREEN_W + x] = Fog(Lit(c, hl), rowDist * (1.0f - 0.6f * hl),
                    m_hazeColor, m_hazeDist);
            }

            if (i == 0)   // nearest one hides sprites behind it
            {
                m_lowDepth[x] = d1;
                m_lowTop[x] = ty0;
            }
        }
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

void Renderer::DrawMarker(DWORD* pixels, const Camera& p,
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
            if (HiddenByHedge(x, y, ty)) continue;
            float up = (float)(bottom - y) / height;          // 0 floor, 1 top
            float strength = across * (1.0f - 0.7f * up) * (0.6f + 0.4f * pulse);
            Blend(pixels[y * SCREEN_W + x], color, (int)(230 * strength));
        }
    }
}

void Renderer::DrawEnemy(DWORD* pixels, const Camera& p,
    float worldX, float worldY, DWORD color)
{
    float sx = worldX - p.X();
    float sy = worldY - p.Y();
    float invDet = 1.0f / (p.PlaneX() * p.DirY() - p.DirX() * p.PlaneY());
    float tx = invDet * (p.DirY() * sx - p.DirX() * sy);
    float ty = invDet * (-p.PlaneY() * sx + p.PlaneX() * sy);
    if (ty < 0.15f) return;

    // 75% of a wall's height, standing on the floor.
    int wallH = (int)(SCREEN_H / ty);
    int height = wallH * 3 / 4;
    int width = height * 3 / 5;
    if (height < 2 || width < 2) return;

    int centerX = (int)((SCREEN_W / 2) * (1.0f + tx / ty));
    int bottom = SCREEN_H / 2 + wallH / 2;
    int top = bottom - height;
    int left = centerX - width / 2;

    // Darker with distance.
    float light = 1.0f - ty / 14.0f;
    if (light < 0.3f) light = 0.3f;
    int r = (int)(((color >> 16) & 255) * light);
    int g = (int)(((color >> 8) & 255) * light);
    int b = (int)((color & 255) * light);
    DWORD body = (DWORD)(r << 16 | g << 8 | b);

    for (int x = left; x < left + width; ++x)
    {
        if (x < 0 || x >= SCREEN_W) continue;
        if (ty >= m_depth[x]) continue;   // a wall is in front

        float u = (float)(x - left) / width * 2.0f - 1.0f;   // -1..1

        for (int y = top; y < bottom; ++y)
        {
            if (y < 0 || y >= SCREEN_H) continue;
            if (HiddenByHedge(x, y, ty)) continue;
            float v = (float)(y - top) / height;              // 0 top, 1 bottom

            DWORD c;
            if (!DogPlaceholderPixel(u, v, body, c)) continue;
            pixels[y * SCREEN_W + x] = c;
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


// =====================================================
// MINIMAP
// =====================================================

// Express markers blink, faster when time is almost up.
static bool ExpressBlinkOn(float timeLeft)
{
    float rate = (timeLeft < EXPRESS_URGENT) ? 6.0f : 2.0f;
    return ((int)(timeLeft * rate) % 2) == 0;
}

static void FillSquare(DWORD* pixels, int cx, int cy, int size, DWORD color)
{
    int half = size / 2;
    for (int y = cy - half; y <= cy + half; ++y)
        for (int x = cx - half; x <= cx + half; ++x)
            if (x >= 0 && y >= 0 && x < SCREEN_W && y < SCREEN_H)
                pixels[y * SCREEN_W + x] = color;
}

RECT Renderer::DrawMinimap(DWORD* pixels, const Map& map, const Player& player,
    const DeliveryRun& run, const Enemy* enemies, int enemyCount,
    float /*time*/)
{
    // Bigger maps get smaller cells so the minimap stays about the same size.
    int cell = 112 / map.Height();
    if (cell < 2) cell = 2;
    if (cell > 7) cell = 7;

    const int margin = 8;
    int w = cell * map.Width(), h = cell * map.Height();
    int left = SCREEN_W - margin - w;
    int top = margin;

    // Frame + background (75% opaque so the view shows through a little).
    for (int y = top - 1; y <= top + h; ++y)
        for (int x = left - 1; x <= left + w; ++x)
        {
            DWORD& d = pixels[y * SCREEN_W + x];
            bool border = (x == left - 1 || y == top - 1 || x == left + w || y == top + h);
            if (border) { d = 0xB4B4B4; continue; }

            int tx = (x - left) / cell, ty = (y - top) / cell;
            DWORD c = MinimapTileColor(map.TileAt(tx, ty));
            d = ((d & 0xFCFCFC) >> 2) + ((c & 0xFCFCFC) >> 2) * 3;
        }

    auto toX = [&](float wx) { return left + (int)(wx * cell); };
    auto toY = [&](float wy) { return top + (int)(wy * cell); };
    int dot = cell + 1;

    // Start (green)
    FillSquare(pixels, toX(run.StartX()), toY(run.StartY()), dot, 0x50FF50);

    // Stops: orange, grey once delivered, blinking cyan if express.
    for (int i = 0; i < run.StopCount(); ++i)
    {
        const Stop& s = run.GetStop(i);
        DWORD c = s.delivered ? 0x6E6E6E : 0xFF8C00;
        int size = dot;
        if (s.express && !s.delivered)
        {
            c = ExpressBlinkOn(s.timeLeft) ? 0x00E6FF : 0x005064;
            size = dot + 2;
        }
        FillSquare(pixels, toX(s.x), toY(s.y), size, c);
    }

    // Enemies (red)
    for (int i = 0; i < enemyCount; ++i)
        FillSquare(pixels, toX(enemies[i].X()), toY(enemies[i].Y()), dot,
            enemies[i].IsChasing() ? 0xFF2020 : 0x903030);   // dim = still wandering

    // You: facing line + white dot on top.
    int px = toX(player.X()), py = toY(player.Y());
    int len = cell * 2;
    for (int i = 0; i <= len; ++i)
    {
        int x = px + (int)(player.DirX() * i), y = py + (int)(player.DirY() * i);
        if (x >= 0 && y >= 0 && x < SCREEN_W && y < SCREEN_H) pixels[y * SCREEN_W + x] = 0xFFFFFF;
    }
    FillSquare(pixels, px, py, dot, 0xFFFFFF);

    RECT r = { left, top, left + w, top + h };
    return r;
}


// =====================================================
// TEXTURE (PNG loading with GDI+)
// =====================================================

Texture::Texture()
    : m_width(0), m_height(0), m_pixels(nullptr), m_alpha(nullptr),
    m_x0(0), m_y0(0), m_x1(0), m_y1(0)
{
}

Texture::~Texture()
{
    Free();
}

void Texture::Free()
{
    delete[] m_pixels;
    delete[] m_alpha;
    m_pixels = nullptr;
    m_alpha = nullptr;
    m_width = m_height = 0;
}

// GDI+ is started once, the first time a texture loads.
static bool StartGdiPlus()
{
    static bool started = false;
    static ULONG_PTR token = 0;
    if (!started)
    {
        Gdiplus::GdiplusStartupInput input;
        started = Gdiplus::GdiplusStartup(&token, &input, nullptr) == Gdiplus::Ok;
    }
    return started;
}

bool Texture::Load(const wchar_t* fileName)
{
    Free();

    // Folder of the .exe (with trailing backslash).
    wchar_t exeDir[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exeDir, MAX_PATH);
    for (int i = lstrlenW(exeDir) - 1; i >= 0; --i)
        if (exeDir[i] == L'\\' || exeDir[i] == L'/') { exeDir[i + 1] = 0; break; }

    // Places to look, in order.
    const wchar_t* prefixes[4] = { L"", exeDir, exeDir, exeDir };
    const wchar_t* middles[4] = { L"assets\\", L"assets\\", L"..\\assets\\", L"..\\..\\assets\\" };

    for (int i = 0; i < 4; ++i)
    {
        wchar_t path[MAX_PATH * 2];
        lstrcpynW(path, prefixes[i], MAX_PATH);
        lstrcatW(path, middles[i]);
        lstrcatW(path, fileName);

        if (GetFileAttributesW(path) == INVALID_FILE_ATTRIBUTES) continue;
        if (LoadFile(path)) return true;
    }

    OutputDebugStringW(L"Texture NOT found: assets\\");
    OutputDebugStringW(fileName);
    OutputDebugStringW(L"\n");
    return false;
}

bool Texture::LoadFile(const wchar_t* path)
{
    if (!StartGdiPlus()) return false;

    Gdiplus::Bitmap bitmap(path);
    if (bitmap.GetLastStatus() != Gdiplus::Ok) return false;

    int w = (int)bitmap.GetWidth(), h = (int)bitmap.GetHeight();
    if (w <= 0 || h <= 0) return false;

    Gdiplus::Rect area(0, 0, w, h);
    Gdiplus::BitmapData data;
    if (bitmap.LockBits(&area, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &data) != Gdiplus::Ok)
        return false;

    m_width = w;
    m_height = h;
    m_pixels = new DWORD[w * h];
    m_alpha = new unsigned char[w * h];

    for (int y = 0; y < h; ++y)
    {
        const DWORD* row = (const DWORD*)((const BYTE*)data.Scan0 + y * data.Stride);
        for (int x = 0; x < w; ++x)
        {
            m_pixels[y * w + x] = row[x] & 0x00FFFFFF;          // 0xAARRGGBB -> 0x00RRGGBB
            m_alpha[y * w + x] = (unsigned char)(row[x] >> 24);
        }
    }
    bitmap.UnlockBits(&data);

    ComputeBounds();
    return true;
}

void Texture::ComputeBounds()
{
    // Box around everything that isn't (nearly) transparent.
    int w = m_width, h = m_height;
    m_x0 = w; m_y0 = h; m_x1 = 0; m_y1 = 0;
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            if (m_alpha[y * w + x] > 16)
            {
                if (x < m_x0) m_x0 = x;
                if (y < m_y0) m_y0 = y;
                if (x + 1 > m_x1) m_x1 = x + 1;
                if (y + 1 > m_y1) m_y1 = y + 1;
            }
    if (m_x1 <= m_x0 || m_y1 <= m_y0) { m_x0 = 0; m_y0 = 0; m_x1 = w; m_y1 = h; }
}

void Texture::Create(int width, int height)
{
    Free();
    m_width = width;
    m_height = height;
    m_pixels = new DWORD[width * height];
    m_alpha = new unsigned char[width * height];
    for (int i = 0; i < width * height; ++i) { m_pixels[i] = 0; m_alpha[i] = 0; }
}

void Texture::SetPixel(int x, int y, DWORD color, int alpha)
{
    if (!m_pixels || x < 0 || y < 0 || x >= m_width || y >= m_height) return;
    m_pixels[y * m_width + x] = color & 0x00FFFFFF;
    m_alpha[y * m_width + x] = (unsigned char)(alpha < 0 ? 0 : (alpha > 255 ? 255 : alpha));
}

void Texture::FinishEdits()
{
    if (m_pixels) ComputeBounds();
}

DWORD Texture::Sample(float u, float v, int& outAlpha) const
{
    outAlpha = 0;
    if (!m_pixels) return 0;

    int x = m_x0 + (int)(u * (m_x1 - m_x0));
    int y = m_y0 + (int)(v * (m_y1 - m_y0));
    if (x < m_x0) x = m_x0;
    if (x >= m_x1) x = m_x1 - 1;
    if (y < m_y0) y = m_y0;
    if (y >= m_y1) y = m_y1 - 1;

    outAlpha = m_alpha[y * m_width + x];
    return m_pixels[y * m_width + x];
}


// =====================================================
// CAMERA
// =====================================================

// True if a camera here would be clear of walls, with a small gap
// so a wall never fills the whole screen.
static bool CameraSpotClear(const Map& map, float x, float y)
{
    const float GAP = 0.18f;
    return !map.IsSolid((int)x, (int)y) &&
        !map.IsSolid((int)(x + GAP), (int)y) && !map.IsSolid((int)(x - GAP), (int)y) &&
        !map.IsSolid((int)x, (int)(y + GAP)) && !map.IsSolid((int)x, (int)(y - GAP));
}

Camera MakeCamera(const Player& player, const Map& map, bool thirdPerson, float& outDistance)
{
    Camera cam;
    cam.x = player.X();        cam.y = player.Y();
    cam.dirX = player.DirX();  cam.dirY = player.DirY();
    cam.planeX = player.PlaneX(); cam.planeY = player.PlaneY();
    outDistance = 0.0f;

    if (!thirdPerson) return cam;

    // Walk straight back from the player until the full distance
    // or a wall gets in the way.
    const float STEP = 0.05f;
    for (float d = STEP; d <= THIRD_PERSON_DISTANCE; d += STEP)
    {
        float tx = player.X() - player.DirX() * d;
        float ty = player.Y() - player.DirY() * d;
        if (!CameraSpotClear(map, tx, ty)) break;
        cam.x = tx;
        cam.y = ty;
        outDistance = d;
    }
    return cam;
}


// =====================================================
// SPRITES
// =====================================================

void Renderer::DrawSprite(DWORD* pixels, const Camera& p, float worldX, float worldY,
    const Texture& tex, float scale, bool mirror, float fade, float light,
    float lift, const SpritePose* pose)
{
    if (!tex.IsLoaded() || fade <= 0.0f) return;

    float sx = worldX - p.X();
    float sy = worldY - p.Y();
    float invDet = 1.0f / (p.PlaneX() * p.DirY() - p.DirX() * p.PlaneY());
    float tx = invDet * (p.DirY() * sx - p.DirX() * sy);
    float ty = invDet * (-p.PlaneY() * sx + p.PlaneX() * sy);
    if (ty < 0.1f) return;

    float stretchX = pose ? pose->stretchX : 1.0f;
    float stretchY = pose ? pose->stretchY : 1.0f;
    float lean = pose ? pose->lean : 0.0f;

    // Height: a fraction of a wall at that distance. Width keeps the picture's shape.
    int wallH = (int)(SCREEN_H / ty);
    int height = (int)(wallH * scale * stretchY);
    int width = (int)(wallH * scale * stretchX * tex.VisibleWidth() / tex.VisibleHeight());
    if (height < 2 || width < 2) return;

    int centerX = (int)((SCREEN_W / 2) * (1.0f + tx / ty));
    int bottom = SCREEN_H / 2 + wallH / 2 - (int)(lift * wallH);   // feet on the floor (or lifted)
    int top = bottom - height;
    int left = centerX - width / 2;

    float fogDist = ty * (1.0f - 0.6f * light);

    for (int y = top; y < bottom; ++y)
    {
        if (y < 0 || y >= SCREEN_H) continue;
        float v = (y - top + 0.5f) / height;

        // Lean: the top slides sideways, the feet stay put.
        int shift = (int)(lean * width * (1.0f - v));

        for (int x = left + shift; x < left + shift + width; ++x)
        {
            if (x < 0 || x >= SCREEN_W) continue;
            if (ty >= m_depth[x]) continue;   // a wall is in front
            if (HiddenByHedge(x, y, ty)) continue;

            float u = (x - left - shift + 0.5f) / width;
            if (mirror) u = 1.0f - u;

            int alpha;
            DWORD c = tex.Sample(u, v, alpha);
            alpha = (int)(alpha * fade);
            if (alpha <= 8) continue;

            // Same street light and haze as the walls around it.
            DWORD shaded = Fog(Lit(c, light), fogDist, m_hazeColor, m_hazeDist);

            if (alpha >= 250) pixels[y * SCREEN_W + x] = shaded;
            else              Blend(pixels[y * SCREEN_W + x], shaded, alpha);
        }
    }
}


// =====================================================
// GLOW + PROPS (pictures made in code)
// =====================================================

void Renderer::DrawGlow(DWORD* pixels, const Camera& p, float worldX, float worldY,
    float height, float radius, DWORD color)
{
    float sx = worldX - p.X(), sy = worldY - p.Y();
    float invDet = 1.0f / (p.PlaneX() * p.DirY() - p.DirX() * p.PlaneY());
    float tx = invDet * (p.DirY() * sx - p.DirX() * sy);
    float ty = invDet * (-p.PlaneY() * sx + p.PlaneX() * sy);
    if (ty < 0.2f) return;

    float wallH = SCREEN_H / ty;
    int cx = (int)((SCREEN_W / 2) * (1.0f + tx / ty));
    int cy = (int)(SCREEN_H / 2 + wallH / 2 - height * wallH);
    int r = (int)(radius * wallH);
    if (r < 2) r = 2;

    // Dimmer from far away (but never gone: that's the point of a lamp).
    float strength = 0.75f - ty / 40.0f;
    if (strength < 0.35f) strength = 0.35f;

    int cr = (int)((color >> 16) & 255), cg = (int)((color >> 8) & 255), cb = (int)(color & 255);

    for (int y = cy - r; y <= cy + r; ++y)
    {
        if (y < 0 || y >= SCREEN_H) continue;
        for (int x = cx - r; x <= cx + r; ++x)
        {
            if (x < 0 || x >= SCREEN_W || ty >= m_depth[x]) continue;
            float dx = (float)(x - cx) / r, dy = (float)(y - cy) / r;
            float d2 = dx * dx + dy * dy;
            if (d2 >= 1.0f) continue;
            float k = (1.0f - d2) * (1.0f - d2) * strength;   // additive, soft edge

            DWORD& d = pixels[y * SCREEN_W + x];
            int nr = (int)((d >> 16) & 255) + (int)(cr * k);
            int ng = (int)((d >> 8) & 255) + (int)(cg * k);
            int nb = (int)(d & 255) + (int)(cb * k);
            d = Rgb(nr, ng, nb);
        }
    }
}

// ----- prop pictures -----

void Renderer::DrawRoofColumn(DWORD* pixels, int x, const Map& map, const Camera& p,
    float rayX, float rayY, float startDist, float wallTop)
{
    const int   horizon = SCREEN_H / 2;
    const float H = (float)SCREEN_H;

    // Everything at or below 'yTop' in this column is already drawn (closer
    // stuff); only roof rising above it can show. Starts at the wall's top.
    float yTop = wallTop;
    if (yTop <= 0.0f) return;

    float t = startDist + 0.001f;
    float prevT = t, prevY = yTop;      // last sample (for filling rows smoothly)
    bool  inHouse = false;
    bool  drewThisHouse = false;

    const float maxDist = m_hazeDist * 1.3f;   // past this it's all haze anyway
    for (int guard = 0; guard < 2000 && t < maxDist; ++guard)
    {
        // Nothing farther than this can poke above yTop, even the tallest roof.
        float above = horizon - yTop;
        if (above <= 0.5f) break;
        if (t > (0.5f + ROOF_MAX_RISE) * H / above) break;

        float wx = p.X() + rayX * t, wy = p.Y() + rayY * t;
        RoofSample r;
        if (map.RoofAt(wx, wy, r))
        {
            float y = horizon - (0.5f + r.rise) * H / t;   // wall top is 0.5 above the eye

            if (!inHouse) { inHouse = true; drewThisHouse = false; prevT = t; prevY = horizon - 0.5f * H / t; }

            if (y < yTop)
            {
                // Fill the new rows. Each row gets its own spot on the roof
                // (interpolated between the last two samples) so it's smooth.
                int r0 = (int)y;          if (r0 < 0) r0 = 0;
                int r1 = (int)yTop;
                for (int row = r1 - 1; row >= r0; --row)
                {
                    float f = (prevY - (row + 0.5f)) / (prevY - y);
                    if (f < 0.0f) f = 0.0f;
                    if (f > 1.0f) f = 1.0f;
                    float tr = prevT + (t - prevT) * f;
                    RoofSample rs;
                    if (!map.RoofAt(p.X() + rayX * tr, p.Y() + rayY * tr, rs)) rs = r;

                    float light = m_night ? map.LightAt(p.X() + rayX * tr, p.Y() + rayY * tr) * 0.6f : 0.0f;
                    pixels[row * SCREEN_W + x] = Fog(Lit(RoofColor(rs), light), tr * (1.0f - 0.6f * light),
                        m_hazeColor, m_hazeDist);
                }
                yTop = y;
                drewThisHouse = true;
            }
            prevT = t; prevY = y;
            t += 0.02f + t * 0.006f;   // fine steps over a house (coarser far away)
        }
        else
        {
            // Left a house: outline its top edge against whatever is behind.
            if (inHouse && drewThisHouse)
            {
                int row = (int)yTop;
                if (row >= 0 && row < SCREEN_H) pixels[row * SCREEN_W + x] = Fog(0x2A1E1A, t, m_hazeColor, m_hazeDist);
            }
            inHouse = false;

            int tx = (int)wx, ty = (int)wy;
            if (map.IsSolid(tx, ty) && !Map::IsLowWall(map.TileAt(tx, ty))) break;   // the fence: end of the world
            t += 0.2f;   // open ground: hop along
        }
    }
    if (inHouse && drewThisHouse)
    {
        int row = (int)yTop;
        if (row >= 0 && row < SCREEN_H) pixels[row * SCREEN_W + x] = Fog(0x2A1E1A, t, m_hazeColor, m_hazeDist);
    }
}

void Renderer::SetSky(TimeOfDay time, unsigned seed)
{
    m_night = (time == TimeOfDay::Night);

    SkyLook look = GetSkyLook(time);
    m_hazeColor = look.haze;
    m_hazeDist = look.hazeDist;
    s_tintR = (look.tint >> 16) & 255;
    s_tintG = (look.tint >> 8) & 255;
    s_tintB = look.tint & 255;

    PaintSky(time, seed, m_skyBase, m_skyClouds, m_skyline, SKY_PANO_W, SCREEN_H / 2);
    m_cloudOffset = 0.0f;
}

void Renderer::AdvanceSky(float dt)
{
    m_cloudOffset += CLOUD_DRIFT * dt;
    if (m_cloudOffset >= SKY_PANO_W) m_cloudOffset -= SKY_PANO_W;
}

Renderer::~Renderer()
{
    delete[] m_skyBase;
    delete[] m_skyClouds;
    delete[] m_skyline;
}

Renderer::Renderer()
{
    MakeTree(m_tree);
    MakeBush(m_bush);
    MakeFlowers(m_flowers);
    MakeLamp(m_lampOn, true);
    MakeLamp(m_lampOff, false);

    const int skySize = SKY_PANO_W * (SCREEN_H / 2);
    m_skyBase = new DWORD[skySize];
    m_skyClouds = new DWORD[skySize];
    m_skyline = new DWORD[skySize];
    SetSky(TimeOfDay::Noon, 1);
    for (int i = 0; i < SCREEN_W; ++i) { m_depth[i] = 1e9f; m_lowDepth[i] = 1e9f; m_lowTop[i] = SCREEN_H; }
}

const Texture& Renderer::PropTexture(int tile) const
{
    switch (tile)
    {
    case TILE_TREE:    return m_tree;
    case TILE_FLOWERS: return m_flowers;
    case TILE_LAMP:    return m_night ? m_lampOn : m_lampOff;
    default:           return m_bush;
    }
}

float Renderer::PropScale(int tile) const
{
    switch (tile)
    {
    case TILE_TREE:    return 1.45f;   // taller than the houses
    case TILE_FLOWERS: return 0.18f;
    case TILE_LAMP:    return 1.15f;
    default:           return 0.42f;   // bush
    }
}


// =====================================================
// WALK CYCLE
// =====================================================

void WalkCycle::Update(float dt, float moved, float stepsPerTile, bool canWalk)
{
    bool walking = canWalk && moved > 0.0005f;
    if (walking) m_phase += moved * stepsPerTile * 3.14159265f;

    // Ease in and out instead of snapping.
    float target = walking ? 1.0f : 0.0f;
    float rate = 8.0f * dt;
    if (m_amount < target) { m_amount += rate; if (m_amount > target) m_amount = target; }
    else { m_amount -= rate; if (m_amount < target) m_amount = target; }

    if (m_phase > 6283.0f) m_phase -= 6283.1853f;   // keep it small (multiple of 2 pi)
}

float WalkCycle::Lift() const
{
    return fabsf(sinf(m_phase)) * WALK_BOUNCE * m_amount;
}

SpritePose WalkCycle::Pose() const
{
    // s: 0 when a foot hits the ground, 1 at the top of the hop.
    float s = fabsf(sinf(m_phase));
    float k = (2.0f * s - 1.0f) * WALK_SQUASH * m_amount;   // -squash .. +stretch
    SpritePose pose;
    pose.stretchY = 1.0f + k;
    pose.stretchX = 1.0f - k * 0.8f;
    pose.lean = sinf(m_phase) * WALK_LEAN * m_amount;
    return pose;
}

int WalkCycle::StepCount() const
{
    return (int)(m_phase / 3.14159265f);
}

bool WalkCycle::OddStep() const
{
    return m_amount > 0.3f && ((int)(m_phase / 3.14159265f) & 1);
}