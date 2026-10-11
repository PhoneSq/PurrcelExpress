// =====================================================================
//  AI-GENERATED ART - see the note at the top of AItexture.h.
// =====================================================================

#include <math.h>
#include "AItexture.h"
#include "Renderer.h"   // Texture


// =====================================================
// WALLS AND FLOORS
// =====================================================

// House palette, picked to sit next to the hand-drawn characters:
// flat warm colors, dark brown outlines, very little shading.
//                                     cream      soft blue  peach      sage
static const DWORD HOUSE_COLORS[4] = { 0xF3E2C4, 0xA9C3DE, 0xF2BE8E, 0xBFD6A8 };
//                                     terracotta navy       brown      teal
static const DWORD ROOF_COLORS[4] = { 0xC4683F, 0x34426E, 0x7A4A30, 0x2F5D58 };

static const DWORD INK = 0x2A1E1A;   // outline color (dark brown, like the character art)
static const DWORD TRIM = 0xFFF6E6;   // window frames, corner boards
static const DWORD STONE = 0x9A8F86;   // foundation

// A slightly uneven line width, so outlines look drawn by hand.
static float Wobble(int tileX, int tileY, float v)
{
    return (Hash(tileX * 31 + (int)(v * 40.0f), tileY * 17) % 3) * 0.004f;
}

// One house wall section. u: 0..1 across, v: 0 top .. 1 bottom.
// edgeL / edgeR: this side is a real corner of the house (draw an outline there).
static DWORD HouseWall(int type, int tileX, int tileY, float u, float v,
    bool door, bool night, bool edgeL, bool edgeR)
{
    DWORD wall = HOUSE_COLORS[type], roof = ROOF_COLORS[type];
    float wob = Wobble(tileX, tileY, v);

    // ----- roof overhang: two rows of scalloped shingles -----
    // (the real sloped roof continues above the wall, see RoofColor)
    if (v < 0.20f)
    {
        int   row = (int)((v - 0.035f) / 0.0825f);         // 0 or 1
        float rv = (v - 0.035f) / 0.0825f - row;          // 0..1 inside the row
        float off = (row % 2) ? 0.0833f : 0.0f;            // stagger rows
        float su = (u + off) * 6.0f;
        float cu = su - floorf(su) - 0.5f;                 // -0.5..0.5 inside a shingle
        // Bottom edge of each shingle is a little arc.
        float arc = 0.78f + 0.9f * cu * cu;
        if (rv > arc - 0.12f && rv < arc + 0.02f) return Scale(roof, 0.62f);
        if (fabsf(cu) > 0.47f && rv < arc)        return Scale(roof, 0.75f);
        return roof;
    }
    if (v < 0.215f + wob) return INK;                        // eave line
    if (v < 0.25f) return Scale(wall, 0.74f);                // shadow under the roof

    // ----- foundation -----
    if (v > 0.915f - wob && v < 0.93f) return INK;
    if (v >= 0.93f)
    {
        int stone = (int)(u * 7.0f + ((int)(v * 30.0f) % 2) * 0.5f);
        return (Hash(stone, tileX * 7 + tileY) % 2) ? STONE : Scale(STONE, 0.9f);
    }

    // ----- corners: outline + cream corner board, only at real corners -----
    if (edgeL && u < 0.025f + wob) return INK;
    if (edgeR && u > 0.975f - wob) return INK;
    if ((edgeL && u < 0.075f) || (edgeR && u > 0.925f)) return TRIM;

    // ----- front door: arched, roof-colored, little round window -----
    if (door)
    {
        float dx = (u - 0.5f) / 0.15f;                       // -1..1 across the door
        float archY = 0.50f;                                 // where the arch ends
        bool inside = fabsf(dx) <= 1.0f &&
            (v >= archY || dx * dx + ((archY - v) / 0.09f) * ((archY - v) / 0.09f) <= 1.0f);
        bool inOutline = fabsf(dx) <= 1.22f &&
            (v >= archY || dx * dx * 0.67f + ((archY - v) / 0.11f) * ((archY - v) / 0.11f) <= 1.0f);
        if (v < 0.915f && inOutline && !inside) return INK;
        if (v < 0.915f && inside)
        {
            float wx = dx / 0.45f, wy = (v - 0.53f) / 0.045f;    // round window in the door
            float wr = wx * wx + wy * wy;
            if (wr < 1.0f)  return night ? 0xFFD27A : 0xB8DAF0;
            if (wr < 1.6f)  return INK;
            if (fabsf(u - 0.585f) < 0.012f && fabsf(v - 0.72f) < 0.015f) return 0xE8C050;   // knob
            return ((int)((u - 0.35f) * 20.0f) % 3 == 0) ? Scale(roof, 0.85f) : roof;      // planks
        }
        // (the rest of the door wall is plain siding below)
    }
    else
    {
        // ----- one window per section; style varies per house section -----
        int style = Hash(tileX, tileY) % 3;   // 0 shutters, 1 flower box, 2 round
        DWORD glass = night ? 0xFFD27A : ((v < 0.47f && u < 0.52f) ? 0xC8E4F6 : 0x9CC4E2);

        if (style == 2)
        {
            float dx = (u - 0.5f) / 0.17f, dy = (v - 0.50f) / 0.13f;
            float r = dx * dx + dy * dy;
            if (r < 1.0f)
            {
                if (fabsf(u - 0.5f) < 0.012f || fabsf(v - 0.50f) < 0.010f) return TRIM;
                return glass;
            }
            if (r < 1.35f) return TRIM;
            if (r < 1.62f) return INK;
        }
        else
        {
            const float x0 = 0.32f, x1 = 0.68f, y0 = 0.36f, y1 = 0.64f;
            bool inFrameBox = u > x0 - 0.035f && u < x1 + 0.035f && v > y0 - 0.035f && v < y1 + 0.035f;
            bool inGlass = u > x0 && u < x1 && v > y0 && v < y1;

            if (inGlass)
            {
                if (fabsf(u - 0.5f) < 0.014f || fabsf(v - 0.50f) < 0.012f) return TRIM;
                return glass;
            }
            if (inFrameBox)
            {
                bool outer = u < x0 - 0.02f || u > x1 + 0.02f || v < y0 - 0.02f || v > y1 + 0.02f;
                return outer ? INK : TRIM;
            }

            if (style == 0)
            {
                // Shutters on both sides, roof-colored, with slats.
                bool left = u > 0.19f && u < 0.27f;
                bool right = u > 0.73f && u < 0.81f;
                if ((left || right) && v > y0 - 0.02f && v < y1 + 0.02f)
                {
                    float su = left ? (u - 0.19f) / 0.08f : (u - 0.73f) / 0.08f;
                    if (su < 0.12f || su > 0.88f || v < y0 || v > y1) return INK;
                    return (((int)((v - y0) * 45.0f)) % 2) ? Scale(roof, 0.8f) : roof;
                }
            }
            else
            {
                // Flower box under the window.
                if (u > 0.29f && u < 0.71f && v > y1 + 0.035f && v < y1 + 0.10f)
                {
                    if (v < y1 + 0.06f)
                    {
                        unsigned f = Hash((int)(u * 30.0f), tileX * 13 + tileY);
                        if (f % 3 == 0) return (f % 2) ? 0xF070A8 : 0xF8E060;   // blossoms
                        return 0x5E9A4A;                                       // leaves
                    }
                    if (u < 0.30f || u > 0.70f || v > y1 + 0.093f) return INK;
                    return Scale(roof, 0.9f);
                }
            }
        }
    }

    // ----- siding: soft lines, flat color -----
    float board = v * 11.0f;
    if (board - floorf(board) < 0.07f) return Scale(wall, 0.9f);
    return wall;
}

// Color of a wall at (u, v): u = across the face 0..1, v = top 0 .. bottom 1.
DWORD WallColor(TileType tile, int tileX, int tileY, float u, float v, bool door, bool night,
    bool edgeL, bool edgeR)
{
    int tu = (int)(u * 16), tv = (int)(v * 16);
    unsigned n = Hash(tileX * 16 + tu, tileY * 16 + tv);

    switch (tile)
    {
    case TILE_HOUSE1: case TILE_HOUSE2: case TILE_HOUSE3: case TILE_HOUSE4:
        return HouseWall(tile - TILE_HOUSE1, tileX, tileY, u, v, door, night, edgeL, edgeR);

    case TILE_HEDGE:
    {
        int k = 70 + (int)(n % 50);
        return Rgb(30 * k / 100, 125 * k / 100, 40 * k / 100);
    }

    case TILE_FENCE:
    default:
    {
        // Wooden planks with gaps and a top rail.
        if (v < 0.06f) return 0x6E4524;
        float plank = u * 6.0f - (int)(u * 6.0f);
        if (plank < 0.08f) return 0x3A2414;
        int k = 85 + (int)(n % 20);
        return Rgb(140 * k / 100, 96 * k / 100, 56 * k / 100);
    }
    }
}

// Floor color at world point (fx, fy).
DWORD FloorColor(const Map& map, float fx, float fy)
{
    int tx = (int)fx, ty = (int)fy;
    TileType t = map.TileAt(tx, ty);
    unsigned n = Hash((int)(fx * 8), (int)(fy * 8));

    if (t == TILE_PATH)
    {
        // Concrete slabs with seams.
        float sx = fx * 2.0f - floorf(fx * 2.0f), sy = fy * 2.0f - floorf(fy * 2.0f);
        if (sx < 0.05f || sy < 0.05f) return 0x8E8A84;
        int k = 94 + (int)(n % 10);
        return Rgb(170 * k / 100, 166 * k / 100, 158 * k / 100);
    }

    if (t != TILE_ROAD)   // grass, and the grass under props
    {
        int k = 85 + (int)(n % 25);
        return Rgb(58 * k / 100, 120 * k / 100, 52 * k / 100);
    }

    // Road: asphalt with dashed yellow center lines.
    int k = 92 + (int)(n % 12);
    DWORD asphalt = Rgb(58 * k / 100, 59 * k / 100, 64 * k / 100);

    // Roads take tiles 1-2, 8-9, 15-16... so a center line sits
    // at coordinate 2, 9, 16... (between the two lanes).
    auto inRoadBand = [](int c) { return c > 0 && (c - 1) % Map::STEP < Map::ROAD_WIDTH; };
    bool rowRoad = inRoadBand(ty), colRoad = inRoadBand(tx);

    if (rowRoad && !colRoad)   // horizontal street
    {
        float line = (float)((ty - 1) / Map::STEP * Map::STEP + 2);
        if (fabsf(fy - line) < 0.04f && ((int)(fx * 2.0f) % 2 == 0)) return 0xD8B840;
    }
    if (colRoad && !rowRoad)   // vertical street
    {
        float line = (float)((tx - 1) / Map::STEP * Map::STEP + 2);
        if (fabsf(fx - line) < 0.04f && ((int)(fy * 2.0f) % 2 == 0)) return 0xD8B840;
    }
    return asphalt;
}


// ----- Hedges: puffy leaf clumps like the bush props, outlined on top -----

static const DWORD HEDGE_HIGHLIGHT = 0x9BD172;
static const DWORD HEDGE_LIGHT = 0x76B85A;
static const DWORD HEDGE_MID = 0x5A9C42;
static const DWORD HEDGE_DARK = 0x3F7834;
static const DWORD HEDGE_GAP = 0x2C5A26;
static const DWORD HEDGE_INK = 0x1E3A18;

enum ClumpPart { CLUMP_NONE, CLUMP_INK, CLUMP_SHADOW, CLUMP_BODY, CLUMP_LIGHT };

// Round clumps on a staggered grid, in world units (a: along, b: down).
// Neighbors overlap; the lower row sits in front, so the surface looks puffy.
static ClumpPart Clump(float a, float b, int seed)
{
    int row = (int)floorf(b);
    for (int r = row + 1; r >= row - 1; --r)            // front (lower) rows first
    {
        float shift = (r & 1) ? 0.5f : 0.0f;
        int col = (int)floorf(a - shift);
        float bestD = 9.0f, bestX = 0, bestY = 0;
        for (int c = col - 1; c <= col + 1; ++c)
        {
            unsigned n = Hash(c * 7 + seed, r * 13 + seed * 3);
            float cx = a - (c + shift + 0.5f + ((int)(n % 5) - 2) * 0.04f);
            float cy = b - (r + 0.5f + ((int)((n >> 4) % 5) - 2) * 0.04f);
            float rad = 0.60f + (n >> 8) % 4 * 0.03f;
            float d = sqrtf(cx * cx + cy * cy) / rad;
            if (d < bestD) { bestD = d; bestX = cx / rad; bestY = cy / rad; }
        }
        if (bestD > 1.0f) continue;

        if (bestD > 0.84f)                              // rim: ink on top, shadow underneath
            return bestY < 0.25f ? CLUMP_INK : CLUMP_SHADOW;
        if (bestX < -0.05f && bestY < -0.10f && bestD < 0.55f) return CLUMP_LIGHT;
        if (bestY > 0.35f) return CLUMP_SHADOW;          // underside of the puff
        return CLUMP_BODY;
    }
    return CLUMP_NONE;
}

static const float CLUMPS_PER_TILE = 4.5f;

DWORD HedgeSideColor(float along, float v)
{
    if (v > 0.94f) return HEDGE_GAP;                        // shadow where it meets the ground
    float b = v * HEDGE_HEIGHT * CLUMPS_PER_TILE - 0.25f;   // top row half above the edge
    switch (Clump(along * CLUMPS_PER_TILE, b, 1))
    {
    case CLUMP_INK:    return HEDGE_INK;
    case CLUMP_SHADOW: return HEDGE_DARK;
    case CLUMP_LIGHT:  return HEDGE_LIGHT;
    case CLUMP_BODY:   return HEDGE_MID;
    default:           return v < 0.2f ? HEDGE_LIGHT : HEDGE_GAP;  // gaps along the top show the sunny top
    }
}

DWORD HedgeTopColor(float fx, float fy)
{
    // Seen from above: the sunny side of the same clumps.
    switch (Clump(fx * CLUMPS_PER_TILE, fy * CLUMPS_PER_TILE, 5))
    {
    case CLUMP_INK:    return HEDGE_DARK;
    case CLUMP_SHADOW: return HEDGE_MID;
    case CLUMP_LIGHT:  return HEDGE_HIGHLIGHT;
    case CLUMP_BODY:   return HEDGE_LIGHT;
    default:           return HEDGE_DARK;
    }
}

// =====================================================
// SKY
// =====================================================

namespace
{
    // Everything that changes between times of day.
    struct SkyPalette
    {
        DWORD top, mid, horizon;      // gradient, top of the screen -> horizon
        DWORD haze;  float hazeDist;  DWORD tint;
        DWORD sun, sunGlow;           // disc + the glow around it
        float sunRadius;              // pixels
        float sunHeight;              // 0 horizon .. 1 top of the screen
        bool  moon;                   // moon + stars instead of the sun
        int   cloudCount;
        DWORD cloud, cloudShade;
        DWORD distant, nearer;            // skyline: tall city buildings / rooftops and trees
        bool  litWindows;
    };

    const SkyPalette& Palette(TimeOfDay t)
    {
        static const SkyPalette P[(int)TimeOfDay::Count] =
        {
            // Morning: peach at the bottom, soft blue above, pale sun low on the side
            { 0x7FB0E6, 0xC4D6EE, 0xF6D8C4,  0xEBD9CE, 22.0f, 0xFFF2EA,
              0xFFF4C8, 0xFFE2B8, 17.0f, 0.24f, false, 9,  0xFFF6F2, 0xE6C6C8,
              0xC5BED2, 0x8C8AA6, false },
              // Noon: clean blue, small high sun, white clouds
              { 0x3C82D6, 0x78AEE8, 0xC2DCF2,  0xBCD4EA, 24.0f, 0xFFFFFF,
                0xFFFBE0, 0xFFF6D0, 13.0f, 0.80f, false, 11, 0xFFFFFF, 0xD4E0EE,
                0xA6BACE, 0x6C879E, false },
                // Sunset: indigo -> pink -> orange, huge sun sitting on the skyline
                { 0x34366E, 0xD6648A, 0xFFB25C,  0xF0A070, 20.0f, 0xFFD8B6,
                  0xFFE680, 0xFFB060, 30.0f, 0.07f, false, 8,  0xF8A6A0, 0xA85A7C,
                  0xB2667E, 0x51304A, false },
                  // Night: deep blue-purple, moon, stars, lit windows
                  { 0x0E1228, 0x222650, 0x463862,  0x2A2438, 15.0f, 0xFFFFFF,
                    0xF4F0DA, 0x8A86B8, 15.0f, 0.55f, true,  4,  0x3A3E68, 0x282A4C,
                    0x2C2946, 0x141328, true },
        };
        int i = (int)t;
        if (i < 0 || i >= (int)TimeOfDay::Count) i = 3;
        return P[i];
    }

    DWORD Mix(DWORD a, DWORD b, float t)
    {
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;
        int ar = (a >> 16) & 255, ag = (a >> 8) & 255, ab = a & 255;
        int br = (b >> 16) & 255, bg = (b >> 8) & 255, bb = b & 255;
        return Rgb(ar + (int)((br - ar) * t), ag + (int)((bg - ag) * t), ab + (int)((bb - ab) * t));
    }

    DWORD AddLight(DWORD c, DWORD light, float k)
    {
        return Rgb(((c >> 16) & 255) + (int)(((light >> 16) & 255) * k),
            ((c >> 8) & 255) + (int)(((light >> 8) & 255) * k),
            (c & 255) + (int)((light & 255) * k));
    }

    // Tiny random generator for painting (xorshift).
    struct Dice
    {
        unsigned s;
        unsigned Next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
        int   Range(int lo, int hi) { return lo + (int)(Next() % (unsigned)(hi - lo + 1)); }
        float Unit() { return (Next() % 10000) / 10000.0f; }
    };

    inline int Wrap(int x, int w) { x %= w; return x < 0 ? x + w : x; }

    const DWORD SKY_SOLID = 0xFF000000;

    // A filled circle into a layer (with a mask of which layer owns each pixel).
    void SkyBlob(DWORD* layer, unsigned char* mask, int w, int h, float cx, float cy, float r,
        DWORD color, unsigned char id)
    {
        for (int y = (int)(cy - r) - 1; y <= (int)(cy + r) + 1; ++y)
        {
            if (y < 0 || y >= h) continue;
            for (int x = (int)(cx - r) - 1; x <= (int)(cx + r) + 1; ++x)
            {
                float dx = x + 0.5f - cx, dy = y + 0.5f - cy;
                if (dx * dx + dy * dy > r * r) continue;
                int i = y * w + Wrap(x, w);
                layer[i] = SKY_SOLID | color;
                if (mask) mask[i] = id;
            }
        }
    }

    void Rect(DWORD* layer, unsigned char* mask, int w, int h, int x0, int y0, int x1, int y1,
        DWORD color, unsigned char id)
    {
        for (int y = y0; y < y1; ++y)
        {
            if (y < 0 || y >= h) continue;
            for (int x = x0; x < x1; ++x)
            {
                int i = y * w + Wrap(x, w);
                layer[i] = SKY_SOLID | color;
                if (mask) mask[i] = id;
            }
        }
    }

    // Ink line along the top edge of everything painted with this id.
    void TopEdges(DWORD* layer, const unsigned char* mask, int w, int h, unsigned char id, DWORD ink)
    {
        for (int y = 1; y < h; ++y)
            for (int x = 0; x < w; ++x)
            {
                int i = y * w + x;
                if (mask[i] == id && mask[i - w] < id) layer[i] = SKY_SOLID | ink;
            }
    }
}

SkyLook GetSkyLook(TimeOfDay time)
{
    const SkyPalette& p = Palette(time);
    SkyLook look = { p.haze, p.hazeDist, p.tint };
    return look;
}

void PaintSky(TimeOfDay time, unsigned seed, DWORD* base, DWORD* clouds, DWORD* skyline, int w, int h)
{
    const SkyPalette& p = Palette(time);
    Dice dice = { seed ? seed : 1u };
    for (int i = 0; i < 8; ++i) dice.Next();

    // ----- Base: gradient + sun glow -----
    float sunX = dice.Unit() * w;
    float sunY = h - 1 - p.sunHeight * h;
    float glowR = p.sunRadius * (p.moon ? 4.0f : 6.0f);

    for (int y = 0; y < h; ++y)
    {
        float e = (float)(h - y) / h;   // 0 at the horizon .. 1 at the top
        DWORD g = (e < 0.35f) ? Mix(p.horizon, p.mid, e / 0.35f) : Mix(p.mid, p.top, (e - 0.35f) / 0.65f);
        for (int x = 0; x < w; ++x)
        {
            float dx = (float)Wrap(x - (int)sunX + w / 2, w) - w / 2, dy = y - sunY;
            float d = sqrtf(dx * dx + dy * dy);
            DWORD c = g;
            if (d < glowR)
            {
                float k = 1.0f - d / glowR;
                c = AddLight(c, p.sunGlow, k * k * (p.moon ? 0.25f : 0.55f));
            }
            base[y * w + x] = c;
        }
    }

    // Stars (night): little dots, a few bigger crosses, fewer near the glowing horizon.
    if (p.moon)
    {
        for (int i = 0; i < 700; ++i)
        {
            int x = dice.Range(0, w - 1), y = dice.Range(0, (int)(h * 0.8f));
            float bright = 0.35f + 0.65f * dice.Unit();
            DWORD c = Mix(base[y * w + x], 0xFFFFFF, bright * (1.0f - (float)y / h));
            base[y * w + x] = c;
            if (i % 25 == 0)
            {
                if (y > 0)     base[(y - 1) * w + x] = Mix(base[(y - 1) * w + x], c, 0.6f);
                if (y < h - 1) base[(y + 1) * w + x] = Mix(base[(y + 1) * w + x], c, 0.6f);
                base[y * w + Wrap(x - 1, w)] = Mix(base[y * w + Wrap(x - 1, w)], c, 0.6f);
                base[y * w + Wrap(x + 1, w)] = Mix(base[y * w + Wrap(x + 1, w)], c, 0.6f);
            }
        }
    }

    // Sun / moon disc with an outline (like the rest of the art).
    {
        float r = p.sunRadius;
        DWORD rim = p.moon ? 0xB8B29A : Mix(p.sun, 0xE07020, 0.45f);
        for (int y = (int)(sunY - r) - 1; y <= (int)(sunY + r) + 1; ++y)
        {
            if (y < 0 || y >= h) continue;
            for (int x = (int)(sunX - r) - 1; x <= (int)(sunX + r) + 1; ++x)
            {
                float dx = x + 0.5f - sunX, dy = y + 0.5f - sunY;
                float d = sqrtf(dx * dx + dy * dy);
                if (d > r) continue;
                DWORD c = (d > r - 1.5f) ? rim : p.sun;
                if (!p.moon && d < r * 0.55f && dx < 0 && dy < 0) c = Mix(p.sun, 0xFFFFFF, 0.5f);   // shine
                base[y * w + Wrap(x, w)] = c;
            }
        }
        if (p.moon)   // a few craters
        {
            const float cr[3][3] = { { -0.35f, -0.2f, 0.22f }, { 0.25f, 0.3f, 0.16f }, { 0.3f, -0.35f, 0.12f } };
            for (int k = 0; k < 3; ++k)
            {
                float ccx = sunX + cr[k][0] * r, ccy = sunY + cr[k][1] * r, rr = cr[k][2] * r;
                for (int y = (int)(ccy - rr); y <= (int)(ccy + rr); ++y)
                    for (int x = (int)(ccx - rr); x <= (int)(ccx + rr); ++x)
                    {
                        float dx = x + 0.5f - ccx, dy = y + 0.5f - ccy;
                        if (y >= 0 && y < h && dx * dx + dy * dy <= rr * rr)
                            base[y * w + Wrap(x, w)] = 0xD8D2B8;
                    }
            }
        }
    }

    // ----- Clouds: puffs in a row, flat-ish bottoms, shaded underside, outlined -----
    unsigned char* mask = new unsigned char[w * h];
    for (int i = 0; i < w * h; ++i) { clouds[i] = 0; skyline[i] = 0; mask[i] = 0; }

    for (int c = 0; c < p.cloudCount; ++c)
    {
        // One cloud = a flat-bottomed body with round puffs bulging on top.
        unsigned char id = (unsigned char)(c + 1);
        float cx = dice.Unit() * w;
        float cy = h * (0.18f + 0.42f * dice.Unit());   // bottom line
        float width = 50.0f + 100.0f * dice.Unit();
        float body = 9.0f + 5.0f * dice.Unit();          // body thickness

        // Body: a pill from left to right.
        for (int y = (int)(cy - body); y <= (int)cy; ++y)
            for (int x = (int)(cx - width * 0.5f); x <= (int)(cx + width * 0.5f); ++x)
            {
                float edge = fabsf(x - cx) - (width * 0.5f - body);   // rounded ends
                float dy = y - (cy - body * 0.5f);
                if (edge > 0.0f && edge * edge + dy * dy > body * body * 0.25f) continue;
                if (y < 0 || y >= h) continue;
                int idx = y * w + Wrap(x, w);
                clouds[idx] = SKY_SOLID | p.cloud;
                mask[idx] = id;
            }

        // Puffs along the top, biggest in the middle.
        int puffs = 2 + (int)(width / 26.0f);
        for (int k = 0; k < puffs; ++k)
        {
            float t = (k + 0.5f) / puffs;
            float r = body * (0.8f + 0.9f * (1.0f - fabsf(t - 0.5f) * 1.6f)) + dice.Unit() * 3.0f;
            float px = cx - width * 0.5f + body + (width - 2.0f * body) * t;
            SkyBlob(clouds, mask, w, h, px, cy - body * 0.6f, r, p.cloud, id);
        }

        // Flat bottom, shaded underside (only this cloud's pixels).
        for (int y = (int)(cy - 3); y < (int)(cy + 30); ++y)
        {
            if (y < 0 || y >= h) continue;
            for (int x = (int)(cx - width); x <= (int)(cx + width); ++x)
            {
                int idx = y * w + Wrap(x, w);
                if (mask[idx] != id) continue;
                if (y > (int)cy) { clouds[idx] = 0; mask[idx] = 0; }
                else             clouds[idx] = SKY_SOLID | p.cloudShade;
            }
        }
    }
    // Outline every cloud edge.
    DWORD cloudInk = Scale(p.cloudShade, 0.72f);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
        {
            int i = y * w + x;
            if (!mask[i]) continue;
            bool edge = y == 0 || y == h - 1 || !mask[i - w] || !mask[i + w] ||
                !mask[y * w + Wrap(x - 1, w)] || !mask[y * w + Wrap(x + 1, w)];
            if (edge) clouds[i] = SKY_SOLID | cloudInk;
        }

    // ----- Skyline: a downtown of tall buildings, rooftops and trees in front -----
    for (int i = 0; i < w * h; ++i) mask[i] = 0;
    float downtown = dice.Unit() * w;
    float downtown2 = downtown + w * (0.35f + 0.3f * dice.Unit());   // a smaller second cluster

    // Far layer: buildings (taller near downtown), with windows and antennas.
    for (int x = 0; x < w; )
    {
        int bw = dice.Range(18, 56);
        float d1 = fabsf((float)Wrap((int)(x - downtown + w / 2), w) - w / 2) / (w * 0.5f);
        float d2 = fabsf((float)Wrap((int)(x - downtown2 + w / 2), w) - w / 2) / (w * 0.5f);
        float tall = 1.0f - d1 * 3.0f;
        float tall2 = (1.0f - d2 * 4.5f) * 0.6f;
        if (tall2 > tall) tall = tall2;
        if (tall < 0.0f) tall = 0.0f;
        int bh = dice.Range(14, 26) + (int)(tall * dice.Range(25, 95));

        Rect(skyline, mask, w, h, x, h - bh, x + bw, h, p.distant, 1);

        // Setback top on some tall ones.
        if (bh > 50 && dice.Range(0, 2) == 0)
        {
            int sw = bw / 2, sh = dice.Range(8, 18);
            Rect(skyline, mask, w, h, x + (bw - sw) / 2, h - bh - sh, x + (bw + sw) / 2, h - bh, p.distant, 1);
        }
        // Antenna.
        if (bh > 60 && dice.Range(0, 1) == 0)
            Rect(skyline, mask, w, h, x + bw / 2, h - bh - dice.Range(8, 16), x + bw / 2 + 1, h - bh, p.distant, 1);
        // Water tower on a few low ones.
        if (bh < 30 && dice.Range(0, 7) == 0)
        {
            Rect(skyline, mask, w, h, x + bw / 2 - 5, h - bh - 9, x + bw / 2 + 5, h - bh - 3, p.distant, 1);
            Rect(skyline, mask, w, h, x + bw / 2 - 4, h - bh - 3, x + bw / 2 - 3, h - bh, p.distant, 1);
            Rect(skyline, mask, w, h, x + bw / 2 + 3, h - bh - 3, x + bw / 2 + 4, h - bh, p.distant, 1);
        }
        // Windows: a grid, lit at night.
        for (int wy = h - bh + 4; wy < h - 4; wy += 6)
            for (int wx = x + 3; wx < x + bw - 3; wx += 5)
            {
                DWORD wc;
                if (p.litWindows) wc = (dice.Range(0, 99) < 35) ? 0xFFD27A : Scale(p.distant, 0.8f);
                else              wc = Mix(p.distant, p.horizon, 0.35f);
                Rect(skyline, nullptr, w, h, wx, wy, wx + 2, wy + 3, wc, 1);
            }
        x += bw + dice.Range(0, 6);
    }
    TopEdges(skyline, mask, w, h, 1, Scale(p.distant, 0.82f));

    // Near layer: house roofs and tree tops, darker, with a strong ink top line.
    for (int x = 0; x < w; )
    {
        int kind = dice.Range(0, 9);
        if (kind < 5)   // house with a pointed roof
        {
            int bw = dice.Range(22, 40), wallH = dice.Range(7, 13), roofH = dice.Range(6, 11);
            Rect(skyline, mask, w, h, x, h - wallH, x + bw, h, p.nearer, 2);
            for (int ry = 0; ry < roofH; ++ry)
            {
                int inset = (bw / 2 + 2) * ry / roofH;
                Rect(skyline, mask, w, h, x - 2 + inset, h - wallH - ry - 1, x + bw + 2 - inset, h - wallH - ry, p.nearer, 2);
            }
            if (p.litWindows && dice.Range(0, 1) == 0)
                Rect(skyline, nullptr, w, h, x + bw / 3, h - wallH + 3, x + bw / 3 + 3, h - wallH + 6, 0xFFC864, 2);
            x += bw + dice.Range(2, 8);
        }
        else if (kind < 8)   // tree clump
        {
            float r = (float)dice.Range(6, 12);
            SkyBlob(skyline, mask, w, h, x + r, h - r * 0.9f, r, p.nearer, 2);
            SkyBlob(skyline, mask, w, h, x + r * 1.8f, h - r * 0.6f, r * 0.75f, p.nearer, 2);
            x += (int)(r * 2.6f) + dice.Range(0, 6);
        }
        else
            x += dice.Range(10, 40);   // gap: lets the city show through
    }
    TopEdges(skyline, mask, w, h, 2, Mix(p.nearer, 0x000000, 0.45f));

    // Haze over the bottom few rows so the skyline melts into the fog.
    for (int y = h - 10; y < h; ++y)
    {
        float k = (y - (h - 10)) / 10.0f * 0.6f;
        for (int x = 0; x < w; ++x)
        {
            int i = y * w + x;
            if (skyline[i]) skyline[i] = SKY_SOLID | Mix(skyline[i] & 0xFFFFFF, p.haze, k);
        }
    }

    delete[] mask;
}

// Sloped roof: rows of scalloped shingles following the slope, with ink on
// the eave, the ridge and the hips, and each side lit a little differently.
DWORD RoofColor(const RoofSample& r)
{
    DWORD roof = ROOF_COLORS[r.type & 3];
    static const float FACET_LIGHT[4] = { 0.90f, 1.0f, 0.78f, 1.08f };   // west, east, north, south

    if (r.edge < 0.03f) return INK;                                     // eave
    if (r.edge2 - r.edge < 0.035f && r.edge > 0.06f) return INK;         // ridge / hip line

    // Shingle rows: row 0 at the eave, going up the slope.
    float rowPos = r.edge * 6.0f;
    int   row = (int)rowPos;
    float rv = 1.0f - (rowPos - row);                  // 0 top of the row .. 1 its bottom
    float su = r.along * 6.0f + ((row & 1) ? 0.5f : 0.0f);
    float cu = su - floorf(su) - 0.5f;                 // -0.5..0.5 inside a shingle
    float arc = 0.78f + 0.9f * cu * cu;                 // the round bottom of each shingle

    DWORD c = roof;
    if (rv > arc - 0.14f && rv < arc + 0.02f) c = Scale(roof, 0.62f);
    else if (fabsf(cu) > 0.46f && rv < arc)   c = Scale(roof, 0.76f);
    return Scale(c, FACET_LIGHT[r.facet & 3]);
}

DWORD MinimapTileColor(TileType tile)
{
    switch (tile)
    {
    case TILE_ROAD:    return 0x2C2C32;
    case TILE_GRASS:   return 0x1E4A1A;
    case TILE_HEDGE:   return 0x146014;
    case TILE_TREE:    return 0x0E4A12;
    case TILE_PATH:    return 0x8A8680;
    case TILE_BUSH:    return 0x2E7A2A;
    case TILE_FLOWERS: return 0xC86A9A;
    case TILE_LAMP:    return 0xFFD27A;
    case TILE_FENCE:   return 0x7A5230;
    default:           return Scale(HOUSE_COLORS[tile - TILE_HOUSE1], 0.75f);
    }
}


// =====================================================
// PROP SPRITES
// =====================================================

struct Blob { float x, y, r; };

// Fills round leafy clusters: lit from the top-left, dark outline, leaf speckle.
// Is (x, y) inside any of the blobs? (pixel centers)
static bool InFoliage(const Blob* blobs, int count, int x, int y)
{
    for (int b = 0; b < count; ++b)
    {
        float dx = (x + 0.5f - blobs[b].x) / blobs[b].r;
        float dy = (y + 0.5f - blobs[b].y) / blobs[b].r;
        if (dx * dx + dy * dy < 1.0f) return true;
    }
    return false;
}

// A leafy shape made of overlapping round blobs: shaded domes lit from the
// top-left, a few lighter / darker speckles, and a dark outline.
// Every pixel is worked out on its own in one pass (no shared scratch
// buffer), so the result can't depend on how the compiler orders things.
static void PaintFoliage(Texture& t, int w, int h, const Blob* blobs, int count,
    int baseR, int baseG, int baseB, unsigned seed)
{
    const DWORD outline = Rgb(baseR / 3, baseG / 3, baseB / 3);

    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
        {
            if (!InFoliage(blobs, count, x, y)) continue;

            // Edge of the shape (a neighbor is outside, or the picture's border): outline.
            bool edge = x == 0 || y == 0 || x == w - 1 || y == h - 1 ||
                !InFoliage(blobs, count, x - 1, y) || !InFoliage(blobs, count, x + 1, y) ||
                !InFoliage(blobs, count, x, y - 1) || !InFoliage(blobs, count, x, y + 1);
            if (edge) { t.SetPixel(x, y, outline, 255); continue; }

            // Inside: shade by the top-most blob here (a little dome).
            float best = -1.0f, lightAmt = 0.75f;
            for (int b = 0; b < count; ++b)
            {
                float dx = (x + 0.5f - blobs[b].x) / blobs[b].r;
                float dy = (y + 0.5f - blobs[b].y) / blobs[b].r;
                float d2 = dx * dx + dy * dy;
                if (d2 >= 1.0f) continue;
                float rest = 1.0f - d2;
                float nz = rest > 0.0f ? sqrtf(rest) : 0.0f;     // dome height
                float top = nz + (blobs[b].y < h * 0.5f ? 0.15f : 0.0f);
                if (top > best)
                {
                    best = top;
                    lightAmt = 0.5f + 0.5f * (0.55f * nz - 0.30f * dx - 0.40f * dy);
                }
            }
            if (!(lightAmt > 0.2f)) lightAmt = 0.2f;    // (also catches NaN)
            if (lightAmt > 1.3f)    lightAmt = 1.3f;

            unsigned n = Hash(x + (int)seed * 131, y);
            float speckle = ((n % 100) < 12) ? 1.25f : (((n >> 8) % 100) < 10 ? 0.75f : 1.0f);
            float k = lightAmt * speckle;
            t.SetPixel(x, y, Rgb((int)(baseR * k), (int)(baseG * k), (int)(baseB * k)), 255);
        }
}

void MakeTree(Texture& t)
{
    const int W = 64, H = 96;
    t.Create(W, H);

    // Trunk with a darker right side and a little root flare.
    for (int y = 50; y < H; ++y)
    {
        int half = (y > H - 6) ? 6 + (y - (H - 6)) / 2 : 4;
        for (int x = 32 - half; x < 32 + half; ++x)
        {
            bool shadow = x >= 33;
            t.SetPixel(x, y, shadow ? 0x5A3A22 : 0x7E5432);
        }
    }

    const Blob canopy[] =
    {
        { 32, 30, 21 }, { 17, 40, 14 }, { 47, 40, 14 }, { 23, 20, 13 },
        { 42, 19, 13 }, { 32, 50, 14 }, { 12, 29, 10 }, { 52, 29, 10 },
    };
    PaintFoliage(t, W, 64, canopy, 8, 70, 150, 62, 1);
    t.FinishEdits();
}

void MakeBush(Texture& t)
{
    const int W = 48, H = 30;
    t.Create(W, H);
    const Blob b[] = { { 13, 19, 12 }, { 25, 14, 13 }, { 36, 19, 11 }, { 24, 23, 12 } };
    PaintFoliage(t, W, H, b, 4, 62, 138, 58, 2);
    t.FinishEdits();
}

void MakeFlowers(Texture& t)
{
    const int W = 40, H = 18;
    t.Create(W, H);
    const Blob b[] = { { 9, 12, 8 }, { 20, 10, 9 }, { 31, 12, 8 } };
    PaintFoliage(t, W, H, b, 3, 70, 150, 60, 3);

    // Blossoms: little crosses in a few colors.
    const DWORD colors[3] = { 0xF070A8, 0xF8E060, 0xF4F4F4 };
    for (int i = 0; i < 9; ++i)
    {
        unsigned n = Hash(i * 7 + 3, i * 13 + 1);
        int x = 4 + (int)(n % (W - 8)), y = 3 + (int)((n >> 8) % 8);
        DWORD c = colors[i % 3];
        t.SetPixel(x, y, c); t.SetPixel(x - 1, y, c); t.SetPixel(x + 1, y, c);
        t.SetPixel(x, y - 1, c); t.SetPixel(x, y + 1, c);
        t.SetPixel(x, y, 0xFFE08A);
    }
    t.FinishEdits();
}

void MakeLamp(Texture& t, bool lit)
{
    const int W = 20, H = 100;
    t.Create(W, H);

    for (int y = 92; y < H; ++y)          // base
        for (int x = 5; x < 15; ++x) t.SetPixel(x, y, 0x2E2E36);
    for (int y = 18; y < 92; ++y)         // pole with a highlight
        for (int x = 8; x < 12; ++x) t.SetPixel(x, y, x == 8 ? 0x6A6A78 : 0x3C3C46);
    for (int y = 3; y < 7; ++y)           // cap
        for (int x = 3 + (6 - y); x < 17 - (6 - y); ++x) t.SetPixel(x, y, 0x26262E);
    for (int y = 7; y < 18; ++y)          // glass with frame
        for (int x = 4; x < 16; ++x)
        {
            bool frame = x == 4 || x == 15 || y == 17 || x == 9 || x == 10;
            t.SetPixel(x, y, frame ? 0x26262E : (lit ? 0xFFE6A0 : 0xC8D0D4));
        }
    t.FinishEdits();
}


// =====================================================
// DOGS (placeholder)
// =====================================================

bool DogPlaceholderPixel(float u, float v, DWORD body, DWORD& out)
{
    // Squat blob: rounded top corners and rounded bottom corners.
    float cornerV = (v < 0.5f) ? (0.3f - v) / 0.3f : (v - 0.85f) / 0.15f;
    if (cornerV > 0.0f && fabsf(u) > 0.6f)
    {
        float cu = (fabsf(u) - 0.6f) / 0.4f;
        if (cu * cu + cornerV * cornerV > 1.0f) return false;
    }

    const DWORD eye = 0xF0F0F0, pupil = 0x101018;
    out = body;

    // Angry eyes: a slanted brow cuts off the inner top of each eye.
    for (int side = -1; side <= 1; side += 2)
    {
        float ex = u - side * 0.35f, ey = (v - 0.35f) * 2.0f;
        if (ex * ex + ey * ey < 0.05f)
        {
            bool underBrow = ey < -0.05f - side * ex * 1.2f;
            if (underBrow) continue;   // brow: stays body color
            out = (ex * ex + (ey - 0.05f) * (ey - 0.05f) < 0.012f) ? pupil : eye;
        }
    }
    return true;
}


// =====================================================
// UI
// =====================================================

// Potion bottle for the shop and the item slot. liquid = 0xRRGGBB.
void MakePotion(Texture& t, DWORD liquid)
{
    static const char* const ART[13] =
    {
        "   KKKK   ",
        "   KCCK   ",
        "   KKKK   ",
        "   KGGK   ",
        "  KKGGKK  ",
        " KGGGGGGK ",
        "KGWLLLLLLK",
        "KWLLLLLLLK",
        "KWLLLLLLDK",
        "KLLLLLLLDK",
        "KLLLLLLDDK",
        " KDDDDDDK ",
        "  KKKKKK  ",
    };
    t.Create(10, 13);
    DWORD dark = Scale(liquid, 0.7f);
    for (int y = 0; y < 13; ++y)
        for (int x = 0; x < 10; ++x)
        {
            switch (ART[y][x])
            {
            case 'K': t.SetPixel(x, y, INK);      break;
            case 'C': t.SetPixel(x, y, 0xB07A48); break;   // cork
            case 'G': t.SetPixel(x, y, 0xD8EEF0); break;   // glass
            case 'W': t.SetPixel(x, y, 0xFFFFFF); break;   // shine
            case 'L': t.SetPixel(x, y, liquid);   break;
            case 'D': t.SetPixel(x, y, dark);     break;
            }
        }
    t.FinishEdits();
}

void MakeAlert(Texture& t)
{
    static const char* const ART[14] =
    {
        " KKKKK ",
        "KRRWRRK",
        "KRRWRRK",
        "KRRRRRK",
        " KRRRK ",
        " KRRRK ",
        " KRRRK ",
        "  KRK  ",
        "  KRK  ",
        "   K   ",
        "  KKK  ",
        " KRRRK ",
        " KRRRK ",
        "  KKK  ",
    };
    t.Create(7, 14);
    for (int y = 0; y < 14; ++y)
        for (int x = 0; x < 7; ++x)
        {
            switch (ART[y][x])
            {
            case 'K': t.SetPixel(x, y, INK);      break;
            case 'R': t.SetPixel(x, y, 0xE83838); break;
            case 'W': t.SetPixel(x, y, 0xFF9A8A); break;   // shine
            }
        }
    t.FinishEdits();
}

// Draws a small piece of pixel art: K ink, plus up to three letters -> colors.
static void PaintArt(Texture& t, const char* const* art, int w, int h,
    char c1, DWORD v1, char c2, DWORD v2, char c3, DWORD v3)
{
    t.Create(w, h);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
        {
            char ch = art[y][x];
            if (ch == 'K') t.SetPixel(x, y, INK);
            else if (ch == c1)  t.SetPixel(x, y, v1);
            else if (ch == c2)  t.SetPixel(x, y, v2);
            else if (ch == c3)  t.SetPixel(x, y, v3);
        }
    t.FinishEdits();
}

void MakeBone(Texture& t)
{
    static const char* const ART[7] =
    {
        " KK        KK ",
        "KHWK      KWWK",
        "KWWWKKKKKKWWWK",
        " KWWWWWWWWWWK ",
        "KWWDKKKKKKWDDK",
        "KWDK      KDDK",
        " KK        KK ",
    };
    PaintArt(t, ART, 14, 7, 'W', 0xF4EAD5, 'D', 0xD2C3A3, 'H', 0xFFFFFF);
}

void MakeHeart(Texture& t)
{
    static const char* const ART[8] =
    {
        " KK   KK ",
        "KRWK KRRK",
        "KRWRKRRRK",
        "KRRRRRRRK",
        " KRRRRRK ",
        "  KRRRK  ",
        "   KRK   ",
        "    K    ",
    };
    PaintArt(t, ART, 9, 8, 'R', 0xF0507A, 'W', 0xFFB8CC, 0, 0);
}

void MakeDumbbell(Texture& t)
{
    static const char* const ART[7] =
    {
        " KKK      KKK ",
        "KGWGK    KGWGK",
        "KGWGKKKKKKGWGK",
        "KGGGHHHHHHGGGK",
        "KGGGKKKKKKGGGK",
        "KGGGK    KGGGK",
        " KKK      KKK ",
    };
    PaintArt(t, ART, 14, 7, 'G', 0x5A6270, 'W', 0x9AA3B0, 'H', 0xC8CCD2);
}

void MakeShadow(Texture& t)
{
    const int W = 24, H = 8;
    t.Create(W, H);
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
        {
            float dx = (x + 0.5f - W * 0.5f) / (W * 0.5f), dy = (y + 0.5f - H * 0.5f) / (H * 0.5f);
            float d = dx * dx + dy * dy;
            if (d < 1.0f) t.SetPixel(x, y, 0x101010, (int)(140 * (1.0f - d)));
        }
    t.FinishEdits();
}

void MakeFurBall(Texture& t)
{
    const int S = 12;
    t.Create(S, S);
    for (int y = 0; y < S; ++y)
        for (int x = 0; x < S; ++x)
        {
            float dx = x + 0.5f - S * 0.5f, dy = y + 0.5f - S * 0.5f;
            float d = sqrtf(dx * dx + dy * dy) + (Hash(x, y) % 3) * 0.3f;   // fluffy edge
            if (d > S * 0.5f) continue;
            DWORD c = (d > S * 0.5f - 1.2f) ? INK : ((dx < -1 && dy < -1) ? 0xA8A8A8 : 0x7A7A7A);
            t.SetPixel(x, y, c);
        }
    t.FinishEdits();
}

void MakeDizzy(Texture& t, int phase)
{
    const int W = 30, H = 12;
    t.Create(W, H);

    // The ring: a thin oval.
    const float rx = 12.5f, ry = 3.6f, cx = W * 0.5f, cy = H * 0.5f + 0.5f;
    for (int i = 0; i < 160; ++i)
    {
        float a = i / 160.0f * 6.2832f;
        t.SetPixel((int)(cx + cosf(a) * rx), (int)(cy + sinf(a) * ry), 0xFFE9A0, 220);
    }

    // Three little stars on it, a third of a turn apart.
    for (int s = 0; s < 3; ++s)
    {
        float a = (s / 3.0f + phase / 9.0f) * 6.2832f;
        int sx = (int)(cx + cosf(a) * rx), sy = (int)(cy + sinf(a) * ry);
        static const int PX[13] = { 0, -1, 1, 0, 0, -2, 2, 0, 0, -1, 1, -1, 1 };
        static const int PY[13] = { 0, 0, 0, -1, 1, 0, 0, -2, 2, -1, -1, 1, 1 };
        for (int k = 0; k < 13; ++k)
        {
            bool tip = (k >= 5 && k <= 8);
            bool corner = (k >= 9);
            DWORD c = corner ? INK : (tip ? 0xF0B020 : 0xFFE040);
            int px = sx + PX[k], py = sy + PY[k];
            if (px >= 0 && py >= 0 && px < W && py < H) t.SetPixel(px, py, c);
        }
    }
    t.FinishEdits();
}