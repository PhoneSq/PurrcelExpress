#include <math.h>
#include "Enemy.h"
#include "World.h"
#include "Player.h"
#include "Config.h"

// =====================================================
// Tuning
// =====================================================

// Closer than this, yellow and purple stop being clever and just charge.
static const float CHARGE_RANGE = 1.5f;

// Yellow aims this many tiles ahead of where the player is looking.
static const float YELLOW_LOOKAHEAD = 5.0f;

// Purple builds its flank around a point this far ahead of the player.
static const float PURPLE_PIVOT_AHEAD = 2.5f;

// How far from a taken tile an enemy will look for a free one.
static const int   FREE_TILE_RADIUS = 4;

// Biggest map BFS has to handle.
static const int   BFS_MAX_W = 64;
static const int   BFS_MAX_H = 64;


// =====================================================
// Helpers
// =====================================================

static bool TileHasEnemy(const Enemy* self, const Enemy* all, int count, int tx, int ty)
{
    for (int i = 0; i < count; ++i)
    {
        if (&all[i] == self) continue;
        if ((int)all[i].X() == tx && (int)all[i].Y() == ty) return true;
    }
    return false;
}

static bool TileIsTargeted(const Enemy* self, const Enemy* all, int count, int tx, int ty)
{
    for (int i = 0; i < count; ++i)
    {
        if (&all[i] == self) continue;
        if (all[i].TargetTileX() == tx && all[i].TargetTileY() == ty) return true;
    }
    return false;
}

// Walks from the player in the direction they face, stopping at the first
// wall. Gives the last open tile within 'distance'.
static void TileAhead(const Map& map, const Player& p, float distance, int& outX, int& outY)
{
    float lastX = p.X(), lastY = p.Y();
    for (float d = 0.25f; d <= distance; d += 0.25f)
    {
        float tx = p.X() + p.DirX() * d;
        float ty = p.Y() + p.DirY() * d;
        if (map.IsWall((int)tx, (int)ty)) break;
        lastX = tx;
        lastY = ty;
    }
    outX = (int)lastX;
    outY = (int)lastY;
}

// If (wantX, wantY) is a wall or off the map, slide back along the line
// toward (fallbackX, fallbackY) until an open tile is found.
static void ClampToOpen(const Map& map, float wantX, float wantY,
    float fallbackX, float fallbackY, int& outX, int& outY)
{
    for (int i = 0; i <= 32; ++i)
    {
        float f = i / 32.0f;
        float tx = wantX + (fallbackX - wantX) * f;
        float ty = wantY + (fallbackY - wantY) * f;
        int   ix = (int)floorf(tx), iy = (int)floorf(ty);
        if (!map.IsWall(ix, iy)) { outX = ix; outY = iy; return; }
    }
    outX = (int)fallbackX;
    outY = (int)fallbackY;
}


// =====================================================
// Enemy
// =====================================================

Enemy::Enemy()
    : m_x(0), m_y(0), m_type(EnemyType::Blue),
    m_targetX(-1), m_targetY(-1), m_nextX(-1), m_nextY(-1),
    m_repathTimer(0)
{
}

void Enemy::Spawn(float x, float y, EnemyType type)
{
    m_x = x;
    m_y = y;
    m_type = type;
    m_targetX = m_targetY = -1;
    m_nextX = m_nextY = -1;
    m_repathTimer = 0.0f;
}

void Enemy::ChooseTarget(const Map& map, const Player& player,
    const Enemy* all, int count, int& outX, int& outY) const
{
    // Blue (and everyone when close): go straight for the player.
    int wantX = (int)player.X();
    int wantY = (int)player.Y();

    float dx = player.X() - m_x, dy = player.Y() - m_y;
    bool close = (dx * dx + dy * dy) < CHARGE_RANGE * CHARGE_RANGE;

    if (!close && m_type == EnemyType::Yellow)
    {
        TileAhead(map, player, YELLOW_LOOKAHEAD, wantX, wantY);
    }
    else if (!close && m_type == EnemyType::Purple)
    {
        // 1. A pivot point a little ahead of the player.
        // 2. A line from the nearest blue enemy through the pivot.
        // 3. Same distance again past the pivot = purple's target,
        //    so blue and purple end up on opposite sides of you.
        int pivotTX, pivotTY;
        TileAhead(map, player, PURPLE_PIVOT_AHEAD, pivotTX, pivotTY);
        float pivotX = pivotTX + 0.5f, pivotY = pivotTY + 0.5f;

        const Enemy* nearestBlue = nullptr;
        float best = 1e9f;
        for (int i = 0; i < count; ++i)
        {
            if (&all[i] == this || all[i].Type() != EnemyType::Blue) continue;
            float bx = all[i].X() - m_x, by = all[i].Y() - m_y;
            float d = bx * bx + by * by;
            if (d < best) { best = d; nearestBlue = &all[i]; }
        }

        float aimX = pivotX, aimY = pivotY;
        if (nearestBlue)
        {
            aimX = pivotX + (pivotX - nearestBlue->X());
            aimY = pivotY + (pivotY - nearestBlue->Y());
        }
        ClampToOpen(map, aimX, aimY, pivotX, pivotY, wantX, wantY);
    }

    // Never share a target: if someone is on it or heading for it,
    // take the nearest free tile instead.
    if (!TileHasEnemy(this, all, count, wantX, wantY) &&
        !TileIsTargeted(this, all, count, wantX, wantY))
    {
        outX = wantX;
        outY = wantY;
        return;
    }

    outX = wantX;
    outY = wantY;
    int bestDist = 1 << 30;
    for (int oy = -FREE_TILE_RADIUS; oy <= FREE_TILE_RADIUS; ++oy)
        for (int ox = -FREE_TILE_RADIUS; ox <= FREE_TILE_RADIUS; ++ox)
        {
            int tx = wantX + ox, ty = wantY + oy;
            if (map.IsWall(tx, ty)) continue;
            if (TileHasEnemy(this, all, count, tx, ty))   continue;
            if (TileIsTargeted(this, all, count, tx, ty)) continue;

            int d = ox * ox + oy * oy;
            if (d < bestDist) { bestDist = d; outX = tx; outY = ty; }
        }
}

bool Enemy::FindNextStep(const Map& map, const Enemy* all, int count, int goalX, int goalY)
{
    int startX = (int)m_x, startY = (int)m_y;
    int w = map.Width(), h = map.Height();
    if (w > BFS_MAX_W || h > BFS_MAX_H) return false;
    if (map.IsWall(goalX, goalY)) return false;

    if (startX == goalX && startY == goalY)
    {
        m_nextX = goalX;
        m_nextY = goalY;
        return true;
    }

    // BFS over tiles. parent[] remembers where we came from.
    static short parent[BFS_MAX_W * BFS_MAX_H];
    static short queue[BFS_MAX_W * BFS_MAX_H];
    for (int i = 0; i < w * h; ++i) parent[i] = -1;

    int head = 0, tail = 0;
    int startIndex = startY * w + startX;
    int goalIndex = goalY * w + goalX;
    parent[startIndex] = (short)startIndex;
    queue[tail++] = (short)startIndex;

    const int stepX[4] = { 1, -1, 0, 0 };
    const int stepY[4] = { 0, 0, 1, -1 };

    bool found = false;
    while (head < tail)
    {
        int cur = queue[head++];
        if (cur == goalIndex) { found = true; break; }

        int cx = cur % w, cy = cur / w;
        for (int i = 0; i < 4; ++i)
        {
            int nx = cx + stepX[i], ny = cy + stepY[i];
            if (map.IsWall(nx, ny)) continue;

            int ni = ny * w + nx;
            if (parent[ni] != -1) continue;

            // Route around other enemies (but the goal itself is fine).
            if (ni != goalIndex && TileHasEnemy(this, all, count, nx, ny)) continue;

            parent[ni] = (short)cur;
            queue[tail++] = (short)ni;
        }
    }

    if (!found) return false;

    // Walk back from the goal until the tile right after the start.
    int step = goalIndex;
    while (parent[step] != startIndex) step = parent[step];

    m_nextX = step % w;
    m_nextY = step / w;
    return true;
}

void Enemy::Repath(const Map& map, const Player& player, const Enemy* all, int count)
{
    int tx, ty;
    ChooseTarget(map, player, all, count, tx, ty);
    m_targetX = tx;
    m_targetY = ty;

    if (FindNextStep(map, all, count, tx, ty)) return;

    // Clever target unreachable: fall back to chasing the player.
    if (FindNextStep(map, all, count, (int)player.X(), (int)player.Y())) return;

    m_nextX = m_nextY = -1;   // boxed in: wait
}

void Enemy::Update(float dt, const Map& map, const Player& player,
    const Enemy* all, int count)
{
    m_repathTimer -= dt;
    if (m_repathTimer <= 0.0f || m_nextX < 0)
    {
        Repath(map, player, all, count);
        m_repathTimer = ENEMY_REPATH_TIME;
    }
    if (m_nextX < 0) return;

    // Walk toward the center of the next tile.
    float goalX = m_nextX + 0.5f, goalY = m_nextY + 0.5f;
    float dx = goalX - m_x, dy = goalY - m_y;
    float dist = sqrtf(dx * dx + dy * dy);

    // Standing on the player's tile: walk straight at them instead of the
    // tile center, so we can actually reach them.
    if (m_nextX == (int)player.X() && m_nextY == (int)player.Y() &&
        (int)m_x == m_nextX && (int)m_y == m_nextY)
    {
        dx = player.X() - m_x;
        dy = player.Y() - m_y;
        dist = sqrtf(dx * dx + dy * dy);
    }

    float move = ENEMY_SPEED * dt;
    if (dist < 0.01f)
    {
        m_nextX = m_nextY = -1;   // arrived: repath next frame
        return;
    }
    if (move > dist) move = dist;

    float nx = m_x + dx / dist * move;
    float ny = m_y + dy / dist * move;

    // Don't step into a tile another enemy is standing on; wait instead.
    bool changesTile = (int)nx != (int)m_x || (int)ny != (int)m_y;
    if (changesTile && TileHasEnemy(this, all, count, (int)nx, (int)ny)) return;

    if (!map.IsWall((int)nx, (int)m_y)) m_x = nx;
    if (!map.IsWall((int)m_x, (int)ny)) m_y = ny;
}