#include <math.h>
#include "World.h"

// =====================================================
// RANDOM
// =====================================================

unsigned Random::Next()
{
    // xorshift32
    unsigned x = m_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    m_state = x;
    return x;
}

int Random::Range(int lo, int hi)
{
    if (hi <= lo) return lo;
    return lo + (int)(Next() % (unsigned)(hi - lo + 1));
}

bool Random::Chance(int percent)
{
    return Range(1, 100) <= percent;
}


// =====================================================
// MAP (neighborhood generator)
// =====================================================

// What a block becomes, in percent (cumulative).
static const int LOT_HOUSE = 46;   // house with a yard and a front path
static const int LOT_BIG_HOUSE = 58;   // big house filling most of the block
static const int LOT_DUPLEX = 72;   // two small houses facing a shared path
// the rest: a park
static const int HEDGE_CHANCE = 30;   // house lot gets a hedge on one side
static const int YARD_TREE_CHANCE = 35;   // house lot gets a tree in the yard
static const int MERGE_CHANCE = 22;   // two neighboring blocks become one big park
static const int ROADWORK_CHANCE = 10;   // a street segment is fenced off

static const float LAMP_RADIUS = 3.2f;     // tiles a street light reaches

Map::Map() : m_width(0), m_height(0)
{
    Random rng(12345);
    Generate(MAP_BASE_SIZE, rng);
}

TileType Map::TileAt(int x, int y) const
{
    if (x < 0 || y < 0 || x >= m_width || y >= m_height) return TILE_FENCE;
    return (TileType)m_tiles[y][x];
}

bool Map::IsHouse(int x, int y) const
{
    TileType t = TileAt(x, y);
    return t >= TILE_HOUSE1 && t <= TILE_HOUSE4;
}

bool Map::IsDoorstep(int x, int y) const
{
    if (IsWall(x, y) || TileAt(x, y) == TILE_TREE) return false;   // no drop-offs inside a tree
    return IsHouse(x + 1, y) || IsHouse(x - 1, y) || IsHouse(x, y + 1) || IsHouse(x, y - 1);
}

bool Map::IsFrontDoor(int x, int y) const
{
    return TileAt(x, y) == TILE_PATH && IsDoorstep(x, y);
}

float Map::LightAt(float x, float y) const
{
    int ix = (int)(x * LIGHT_RES), iy = (int)(y * LIGHT_RES);
    if (ix < 0 || iy < 0 || ix >= m_width * LIGHT_RES || iy >= m_height * LIGHT_RES) return 0.0f;
    return m_light[iy][ix] / 255.0f;
}

void Map::Set(int x, int y, TileType t)
{
    if (x > 0 && y > 0 && x < m_width - 1 && y < m_height - 1)
        m_tiles[y][x] = (unsigned char)t;
}

void Map::Fill(int x0, int y0, int w, int h, TileType t)
{
    for (int y = y0; y < y0 + h; ++y)
        for (int x = x0; x < x0 + w; ++x)
            Set(x, y, t);
}

void Map::Generate(int size, Random& rng)
{
    if (size < 16)           size = 16;
    if (size > MAP_MAX_SIZE) size = MAP_MAX_SIZE;
    m_width = m_height = size;

    // 1. Everything is road, with a fence around the edge.
    for (int y = 0; y < m_height; ++y)
        for (int x = 0; x < m_width; ++x)
        {
            bool edge = (x == 0 || y == 0 || x == m_width - 1 || y == m_height - 1);
            m_tiles[y][x] = (unsigned char)(edge ? TILE_FENCE : TILE_ROAD);
        }

    // 2. Blocks between the roads. Roads take the first ROAD_WIDTH
    //    tiles of every STEP, starting right inside the fence.
    for (int by = 1 + ROAD_WIDTH; by + BLOCK_SIZE <= m_height - 1; by += STEP)
        for (int bx = 1 + ROAD_WIDTH; bx + BLOCK_SIZE <= m_width - 1; bx += STEP)
            BuildBlock(bx, by, rng);

    // 3. Variety: big parks, closed streets, street lights.
    MergeParks(rng);
    AddRoadwork(rng);
    AddStreetLamps();

    // 4. Nothing walkable is ever sealed off.
    RemovePockets();

    BuildLightMap();
}

void Map::BuildBlock(int bx, int by, Random& rng)
{
    Fill(bx, by, BLOCK_SIZE, BLOCK_SIZE, TILE_GRASS);

    int roll = rng.Range(1, 100);
    if (roll <= LOT_HOUSE)     BuildHouseLot(bx, by, rng);
    else if (roll <= LOT_BIG_HOUSE) BuildBigHouse(bx, by, rng);
    else if (roll <= LOT_DUPLEX)    BuildDuplex(bx, by, rng);
    else                            BuildPark(bx, by, BLOCK_SIZE, BLOCK_SIZE, rng);
}

// Puts 'count' of tile 't' on random grass tiles inside the rectangle.
void Map::Scatter(int x0, int y0, int w, int h, TileType t, int count, Random& rng)
{
    for (int tries = 0; count > 0 && tries < count * 12; ++tries)
    {
        int x = x0 + rng.Range(0, w - 1), y = y0 + rng.Range(0, h - 1);
        if (!IsGrass(x, y)) continue;
        Set(x, y, t);
        --count;
    }
}

void Map::BuildHouseLot(int bx, int by, Random& rng)
{
    // House inside the block, with a yard all the way around.
    int w = rng.Range(2, 3), h = rng.Range(2, 3);
    int hx = bx + 1 + rng.Range(0, 3 - w);
    int hy = by + 1 + rng.Range(0, 3 - h);
    TileType color = (TileType)(TILE_HOUSE1 + rng.Range(0, 3));
    Fill(hx, hy, w, h, color);

    int front = rng.Range(0, 3);   // 0 top, 1 bottom, 2 left, 3 right

    // Big square houses are sometimes L-shaped (a back corner missing).
    if (w == 3 && h == 3 && rng.Chance(50))
    {
        int cx = rng.Range(0, 1) ? hx : hx + 2;
        int cy = rng.Range(0, 1) ? hy : hy + 2;
        if (front == 0) cy = hy + 2;
        if (front == 1) cy = hy;
        if (front == 2) cx = hx + 2;
        if (front == 3) cx = hx;
        Set(cx, cy, TILE_GRASS);
    }

    // Front path: from the street side of the block to the front door.
    if (front <= 1)
    {
        int col = rng.Range(hx, hx + w - 1);
        if (front == 0) Fill(col, by, 1, hy - by, TILE_PATH);
        else            Fill(col, hy + h, 1, by + BLOCK_SIZE - (hy + h), TILE_PATH);
    }
    else
    {
        int row = rng.Range(hy, hy + h - 1);
        if (front == 2) Fill(bx, row, hx - bx, 1, TILE_PATH);
        else            Fill(hx + w, row, bx + BLOCK_SIZE - (hx + w), 1, TILE_PATH);
    }

    // Sometimes a hedge along a side of the lot that isn't the front.
    if (rng.Chance(HEDGE_CHANCE))
    {
        int side = rng.Range(0, 3);
        if (side == front) side = (side + 1) % 4;
        int gap = rng.Range(1, 3);
        for (int i = 0; i < BLOCK_SIZE; ++i)
        {
            if (i == gap) continue;
            int x = bx, y = by;
            if (side == 0) x = bx + i;
            if (side == 1) { x = bx + i; y = by + BLOCK_SIZE - 1; }
            if (side == 2) y = by + i;
            if (side == 3) { y = by + i; x = bx + BLOCK_SIZE - 1; }
            if (IsGrass(x, y)) Set(x, y, TILE_HEDGE);
        }
    }

    // Yard decoration.
    if (rng.Chance(YARD_TREE_CHANCE)) Scatter(bx, by, BLOCK_SIZE, BLOCK_SIZE, TILE_TREE, 1, rng);
    Scatter(bx, by, BLOCK_SIZE, BLOCK_SIZE, TILE_BUSH, rng.Range(1, 3), rng);
    Scatter(bx, by, BLOCK_SIZE, BLOCK_SIZE, TILE_FLOWERS, rng.Range(0, 2), rng);
}

void Map::BuildBigHouse(int bx, int by, Random& rng)
{
    TileType color = (TileType)(TILE_HOUSE1 + rng.Range(0, 3));

    if (rng.Range(0, 1))
    {
        // Wide house; path to the top or bottom street.
        int hx = bx + rng.Range(0, 1), hy = by + 1;
        Fill(hx, hy, 4, 3, color);
        int col = rng.Range(hx + 1, hx + 2);
        Set(col, rng.Range(0, 1) ? by : by + BLOCK_SIZE - 1, TILE_PATH);
    }
    else
    {
        // Tall house; path to the left or right street.
        int hx = bx + 1, hy = by + rng.Range(0, 1);
        Fill(hx, hy, 3, 4, color);
        int row = rng.Range(hy + 1, hy + 2);
        Set(rng.Range(0, 1) ? bx : bx + BLOCK_SIZE - 1, row, TILE_PATH);
    }

    Scatter(bx, by, BLOCK_SIZE, BLOCK_SIZE, TILE_BUSH, rng.Range(1, 2), rng);
}

void Map::BuildDuplex(int bx, int by, Random& rng)
{
    // Two small houses facing each other across a shared path
    // that runs straight through the block.
    TileType a = (TileType)(TILE_HOUSE1 + rng.Range(0, 3));
    TileType b = (TileType)(TILE_HOUSE1 + (a - TILE_HOUSE1 + rng.Range(1, 3)) % 4);

    if (rng.Range(0, 1))
    {
        Fill(bx + 2, by, 1, BLOCK_SIZE, TILE_PATH);   // path top to bottom
        Fill(bx, by + 1, 2, 2, a);
        Fill(bx + 3, by + 2, 2, 2, b);
    }
    else
    {
        Fill(bx, by + 2, BLOCK_SIZE, 1, TILE_PATH);   // path left to right
        Fill(bx + 1, by, 2, 2, a);
        Fill(bx + 2, by + 3, 2, 2, b);
    }

    Scatter(bx, by, BLOCK_SIZE, BLOCK_SIZE, TILE_FLOWERS, rng.Range(1, 2), rng);
    Scatter(bx, by, BLOCK_SIZE, BLOCK_SIZE, TILE_BUSH, rng.Range(0, 1), rng);
}

void Map::BuildPark(int x0, int y0, int w, int h, Random& rng)
{
    Fill(x0, y0, w, h, TILE_GRASS);

    // Paths through the middle (at least one).
    bool across = rng.Chance(65), down = rng.Chance(65);
    if (!across && !down) { if (w >= h) across = true; else down = true; }
    if (across) Fill(x0, y0 + h / 2, w, 1, TILE_PATH);
    if (down)   Fill(x0 + w / 2, y0, 1, h, TILE_PATH);

    int area = w * h;
    Scatter(x0, y0, w, h, TILE_TREE, 1 + area / 12 + rng.Range(0, 1), rng);
    Scatter(x0, y0, w, h, TILE_BUSH, 1 + area / 8, rng);
    Scatter(x0, y0, w, h, TILE_FLOWERS, 1 + area / 10, rng);
}

void Map::MergeParks(Random& rng)
{
    // Block grid size, and which blocks are already part of a big park.
    int cols = (m_width - 2) / STEP, rows = (m_height - 2) / STEP;
    if (cols < 3 && rows < 3) return;   // small maps stay simple

    bool used[8][8] = {};
    int maxMerges = 1 + (cols * rows) / 6;

    for (int r = 0; r < rows && maxMerges > 0; ++r)
        for (int c = 0; c < cols && maxMerges > 0; ++c)
        {
            if (used[r][c] || (r == 0 && c == 0)) continue;   // keep the start corner normal
            if (!rng.Chance(MERGE_CHANCE)) continue;

            int bx = 1 + ROAD_WIDTH + c * STEP, by = 1 + ROAD_WIDTH + r * STEP;
            bool wide = rng.Range(0, 1) == 0;

            if (wide && c + 1 < cols && !used[r][c + 1])
            {
                BuildPark(bx, by, BLOCK_SIZE * 2 + ROAD_WIDTH, BLOCK_SIZE, rng);
                used[r][c] = used[r][c + 1] = true;
                --maxMerges;
            }
            else if (!wide && r + 1 < rows && !used[r + 1][c])
            {
                BuildPark(bx, by, BLOCK_SIZE, BLOCK_SIZE * 2 + ROAD_WIDTH, rng);
                used[r][c] = used[r + 1][c] = true;
                --maxMerges;
            }
        }
}

void Map::AddRoadwork(Random& rng)
{
    // A fence across the full road width, in the middle of a segment
    // between two crossings. Never right next to the start.
    for (int by = 1 + ROAD_WIDTH; by + BLOCK_SIZE <= m_height - 1; by += STEP)
        for (int bx = 1 + ROAD_WIDTH; bx + BLOCK_SIZE <= m_width - 1; bx += STEP)
        {
            int midX = bx + BLOCK_SIZE / 2, midY = by + BLOCK_SIZE / 2;
            if (midX < 8 && midY < 8) continue;

            if (rng.Chance(ROADWORK_CHANCE))   // street above this block
                for (int i = 0; i < ROAD_WIDTH; ++i)
                    if (TileAt(midX, by - ROAD_WIDTH + i) == TILE_ROAD) Set(midX, by - ROAD_WIDTH + i, TILE_FENCE);

            if (rng.Chance(ROADWORK_CHANCE))   // street left of this block
                for (int i = 0; i < ROAD_WIDTH; ++i)
                    if (TileAt(bx - ROAD_WIDTH + i, midY) == TILE_ROAD) Set(bx - ROAD_WIDTH + i, midY, TILE_FENCE);
        }
}

void Map::AddStreetLamps()
{
    // A street light on two opposite corners of every block,
    // so they line the streets at a steady spacing.
    for (int by = 1 + ROAD_WIDTH; by + BLOCK_SIZE <= m_height - 1; by += STEP)
        for (int bx = 1 + ROAD_WIDTH; bx + BLOCK_SIZE <= m_width - 1; bx += STEP)
        {
            if (IsGrass(bx, by)) Set(bx, by, TILE_LAMP);
            int ex = bx + BLOCK_SIZE - 1, ey = by + BLOCK_SIZE - 1;
            if (IsGrass(ex, ey)) Set(ex, ey, TILE_LAMP);
        }
}

void Map::RemovePockets()
{
    static unsigned char reached[MAP_MAX_SIZE][MAP_MAX_SIZE];
    static short queue[MAP_MAX_SIZE * MAP_MAX_SIZE];
    for (int y = 0; y < m_height; ++y)
        for (int x = 0; x < m_width; ++x)
            reached[y][x] = 0;

    int head = 0, tail = 0;
    queue[tail++] = (short)(START_TILE * MAP_MAX_SIZE + START_TILE);
    reached[START_TILE][START_TILE] = 1;

    const int stepX[4] = { 1, -1, 0, 0 };
    const int stepY[4] = { 0, 0, 1, -1 };

    while (head < tail)
    {
        int cur = queue[head++];
        int cx = cur % MAP_MAX_SIZE, cy = cur / MAP_MAX_SIZE;
        for (int i = 0; i < 4; ++i)
        {
            int nx = cx + stepX[i], ny = cy + stepY[i];
            if (IsWall(nx, ny) || reached[ny][nx]) continue;
            reached[ny][nx] = 1;
            queue[tail++] = (short)(ny * MAP_MAX_SIZE + nx);
        }
    }

    for (int y = 0; y < m_height; ++y)
        for (int x = 0; x < m_width; ++x)
            if (!IsWall(x, y) && !reached[y][x])
                m_tiles[y][x] = TILE_HEDGE;
}

void Map::BuildLightMap()
{
    int w = m_width * LIGHT_RES, h = m_height * LIGHT_RES;
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            m_light[y][x] = 0;

    int reach = (int)(LAMP_RADIUS * LIGHT_RES) + 1;

    for (int ty = 0; ty < m_height; ++ty)
        for (int tx = 0; tx < m_width; ++tx)
        {
            if (TileAt(tx, ty) != TILE_LAMP) continue;
            float lx = tx + 0.5f, ly = ty + 0.5f;
            int cx = (int)(lx * LIGHT_RES), cy = (int)(ly * LIGHT_RES);

            for (int y = cy - reach; y <= cy + reach; ++y)
                for (int x = cx - reach; x <= cx + reach; ++x)
                {
                    if (x < 0 || y < 0 || x >= w || y >= h) continue;
                    float px = (x + 0.5f) / LIGHT_RES - lx, py = (y + 0.5f) / LIGHT_RES - ly;
                    float d = sqrtf(px * px + py * py) / LAMP_RADIUS;
                    if (d >= 1.0f) continue;
                    int add = (int)((1.0f - d) * (1.0f - d) * 255.0f);
                    int sum = m_light[y][x] + add;
                    m_light[y][x] = (unsigned char)(sum > 255 ? 255 : sum);
                }
        }
}

bool Map::RandomOpenTile(Random& rng, float fromX, float fromY, float minDist,
    float& outX, float& outY) const
{
    for (int attempt = 0; attempt < 1000; ++attempt)
    {
        int x = rng.Range(1, m_width - 2);
        int y = rng.Range(1, m_height - 2);
        if (IsWall(x, y) || TileAt(x, y) == TILE_TREE) continue;   // nobody spawns inside a tree

        float cx = x + 0.5f, cy = y + 0.5f;
        float dx = cx - fromX, dy = cy - fromY;
        if (dx * dx + dy * dy < minDist * minDist) continue;

        outX = cx;
        outY = cy;
        return true;
    }
    return false;
}


// =====================================================
// CONTRACTS
// =====================================================

static const wchar_t* const CLIENTS[] =
{
    L"Mrs. Whiskers",   L"Tuna Tavern",      L"Yarn Barn",
    L"Catnip Cafe",     L"Old Tom",          L"Mittens & Co.",
    L"Fishbone Deli",   L"Sir Fluffington",  L"Purrfect Bakery",
    L"Lady Paws",       L"Scratch Post Inc", L"The Sunny Window",
};
static const int CLIENT_COUNT = sizeof(CLIENTS) / sizeof(CLIENTS[0]);

void RollContracts(Contract* out, Random& rng)
{
    // Pick distinct clients for the list.
    int used[CLIENT_COUNT] = {};

    for (int i = 0; i < DELIVERY_CHOICES; ++i)
    {
        Contract& c = out[i];

        int pick = rng.Range(0, CLIENT_COUNT - 1);
        while (used[pick]) pick = (pick + 1) % CLIENT_COUNT;
        used[pick] = 1;
        c.client = CLIENTS[pick];

        c.difficulty = rng.Range(1, 5);
        c.stops = c.difficulty < MAX_STOPS ? c.difficulty : MAX_STOPS;

        c.enemies = c.difficulty * 2 + rng.Range(0, 2);
        if (c.enemies < MIN_ENEMIES) c.enemies = MIN_ENEMIES;
        if (c.enemies > MAX_ENEMIES) c.enemies = MAX_ENEMIES;

        // Roughly $100 per difficulty level, in steps of $10.
        c.reward = c.difficulty * 100 + rng.Range(0, 10) * 10;

        // Express: only with 2+ stops. Pays half the reward extra (rounded to $10).
        c.night = rng.Chance(NIGHT_CHANCE);
        c.express = c.stops >= 2 && rng.Chance(EXPRESS_CHANCE);
        c.expressBonus = c.express ? ((c.reward / 2 + 5) / 10) * 10 : 0;
    }
}

int MapSizeFor(int enemyCount)
{
    if (enemyCount < MIN_ENEMIES) enemyCount = MIN_ENEMIES;
    int size = MAP_BASE_SIZE + (enemyCount - MIN_ENEMIES) * MAP_GROWTH;

    // Snap to a whole number of blocks: 2 + STEP * blocks (16, 23, 30, 37).
    int blocks = (size - 2 + Map::STEP / 2) / Map::STEP;
    if (blocks < 2) blocks = 2;
    while (2 + blocks * Map::STEP > MAP_MAX_SIZE) --blocks;
    return 2 + blocks * Map::STEP;
}


// =====================================================
// DELIVERY RUN
// =====================================================

// Random spots tried per stop. The one farthest from the start
// and from the other stops wins.
static const int STOP_CANDIDATES = 600;

static float Dist(float ax, float ay, float bx, float by)
{
    float dx = bx - ax, dy = by - ay;
    return sqrtf(dx * dx + dy * dy);
}

void DeliveryRun::Start(const Contract& contract, const Map& map, Random& rng)
{
    m_contract = contract;
    m_startX = map.StartX();
    m_startY = map.StartY();
    m_allDelivered = false;
    m_finished = false;
    m_expressEarned = false;

    m_stopCount = contract.stops < MAX_STOPS ? contract.stops : MAX_STOPS;

    for (int i = 0; i < m_stopCount; ++i)
    {
        float bestScore = -1.0f, bestX = m_startX, bestY = m_startY;

        // Pass 0: front doors (path next to a house). Pass 1: any doorstep.
        for (int pass = 0; pass < 2 && bestScore < 0.0f; ++pass)
            for (int a = 0; a < STOP_CANDIDATES; ++a)
            {
                int tx = rng.Range(1, map.Width() - 2);
                int ty = rng.Range(1, map.Height() - 2);
                bool ok = (pass == 0) ? map.IsFrontDoor(tx, ty) : map.IsDoorstep(tx, ty);
                if (!ok) continue;

                float cx = tx + 0.5f, cy = ty + 0.5f;

                // Score = distance to the nearest thing it should avoid.
                float score = Dist(m_startX, m_startY, cx, cy);
                for (int j = 0; j < i; ++j)
                {
                    float d = Dist(m_stops[j].x, m_stops[j].y, cx, cy);
                    if (d < score) score = d;
                }
                // Don't put two packages on the same door.
                for (int j = 0; j < i; ++j)
                    if ((int)m_stops[j].x == tx && (int)m_stops[j].y == ty) score = -2.0f;

                if (score > bestScore) { bestScore = score; bestX = cx; bestY = cy; }
            }

        // No doorstep found (very unlikely): any open tile far away.
        if (bestScore < 0.0f)
            map.RandomOpenTile(rng, m_startX, m_startY, 6.0f, bestX, bestY);

        Stop& s = m_stops[i];
        s.x = bestX;
        s.y = bestY;
        s.delivered = false;
        s.express = false;
        s.timeLeft = 0.0f;
    }

    if (contract.express && m_stopCount > 0)
    {
        Stop& s = m_stops[rng.Range(0, m_stopCount - 1)];
        s.express = true;
        s.timeLeft = rng.Range(EXPRESS_MIN_TIME * 10, EXPRESS_MAX_TIME * 10) / 10.0f;
    }
}

int DeliveryRun::Update(float dt, float px, float py)
{
    if (m_finished) return 0;
    int events = 0;

    // Express countdown
    for (int i = 0; i < m_stopCount; ++i)
    {
        Stop& s = m_stops[i];
        if (!s.express || s.delivered) continue;

        s.timeLeft -= dt;
        if (s.timeLeft <= 0.0f)
        {
            // Too slow: bonus gone, but it's still a normal stop.
            s.timeLeft = 0.0f;
            s.express = false;
            events |= EVENT_EXPRESS_EXPIRED;
        }
    }

    if (!m_allDelivered)
    {
        bool all = true;
        for (int i = 0; i < m_stopCount; ++i)
        {
            Stop& s = m_stops[i];
            if (!s.delivered && Dist(px, py, s.x, s.y) < REACH_DISTANCE)
            {
                s.delivered = true;
                events |= EVENT_STOP_DELIVERED;
                if (s.express)
                {
                    s.express = false;
                    m_expressEarned = true;
                    events |= EVENT_EXPRESS_DELIVERED;
                }
            }
            if (!s.delivered) all = false;
        }
        if (all)
        {
            m_allDelivered = true;
            events |= EVENT_ALL_DELIVERED;
        }
    }
    else if (Dist(px, py, m_startX, m_startY) < REACH_DISTANCE)
    {
        m_finished = true;
        events |= EVENT_FINISHED;
    }

    return events;
}

int DeliveryRun::DeliveredCount() const
{
    int n = 0;
    for (int i = 0; i < m_stopCount; ++i)
        if (m_stops[i].delivered) ++n;
    return n;
}

const Stop* DeliveryRun::ActiveExpress() const
{
    for (int i = 0; i < m_stopCount; ++i)
        if (m_stops[i].express && !m_stops[i].delivered) return &m_stops[i];
    return nullptr;
}

void DeliveryRun::NearestTarget(float px, float py, float& outX, float& outY) const
{
    outX = m_startX;
    outY = m_startY;
    if (m_allDelivered) return;

    float best = 1e9f;
    for (int i = 0; i < m_stopCount; ++i)
    {
        const Stop& s = m_stops[i];
        if (s.delivered) continue;
        float d = Dist(px, py, s.x, s.y);
        if (d < best) { best = d; outX = s.x; outY = s.y; }
    }
}

int DeliveryRun::Payout() const
{
    return m_contract.reward + (m_expressEarned ? m_contract.expressBonus : 0);
}