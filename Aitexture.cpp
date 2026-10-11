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

    // ----- roof band: two rows of scalloped shingles -----
    if (v < 0.035f + wob) return INK;                       // roof top line
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

DWORD SkyColor(float t, bool night)
{
    if (night) return Rgb(26 + (int)(80 * t * t), 30 + (int)(40 * t * t), 58 + (int)(52 * t * t));
    return Rgb(70 + (int)(120 * t), 132 + (int)(90 * t), 210 + (int)(35 * t));   // blue to pale
}

DWORD HazeColor(bool night) { return night ? 0x2A2438 : 0xBCD4EA; }
float HazeDistance(bool night) { return night ? 15.0f : 24.0f; }

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
static void PaintFoliage(Texture& t, int w, int h, const Blob* blobs, int count,
    int baseR, int baseG, int baseB, unsigned seed)
{
    static unsigned char inside[128 * 128];
    for (int i = 0; i < w * h; ++i) inside[i] = 0;

    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
        {
            float best = -1.0f, lightAmt = 0.0f;
            for (int b = 0; b < count; ++b)
            {
                float dx = (x + 0.5f - blobs[b].x) / blobs[b].r;
                float dy = (y + 0.5f - blobs[b].y) / blobs[b].r;
                float d2 = dx * dx + dy * dy;
                if (d2 >= 1.0f) continue;
                float nz = sqrtf(1.0f - d2);                 // dome height
                float top = nz + (blobs[b].y < h * 0.5f ? 0.15f : 0.0f);
                if (top > best)
                {
                    best = top;
                    lightAmt = 0.5f + 0.5f * (0.55f * nz - 0.30f * dx - 0.40f * dy);
                }
            }
            if (best < 0.0f) continue;
            inside[y * w + x] = 1;

            unsigned n = Hash(x + (int)seed * 131, y);
            float speckle = ((n % 100) < 12) ? 1.25f : (((n >> 8) % 100) < 10 ? 0.75f : 1.0f);
            float k = lightAmt * speckle;
            t.SetPixel(x, y, Rgb((int)(baseR * k), (int)(baseG * k), (int)(baseB * k)));
        }

    // Dark outline on the edge pixels.
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
        {
            if (!inside[y * w + x]) continue;
            bool edge = x == 0 || y == 0 || x == w - 1 || y == h - 1 ||
                !inside[y * w + x - 1] || !inside[y * w + x + 1] ||
                !inside[(y - 1) * w + x] || !inside[(y + 1) * w + x];
            if (edge) t.SetPixel(x, y, Rgb(baseR / 3, baseG / 3, baseB / 3));
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