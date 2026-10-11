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
    m_repathTimer(0), m_biteCooldown(0), m_moveX(0), m_moveY(0),
    m_homeX(0), m_homeY(0), m_avoidX(-1000.0f), m_avoidY(-1000.0f),
    m_mode(Mode::Roam), m_lureX(0), m_lureY(0), m_lureTimer(0),
    m_knockVX(0), m_knockVY(0), m_knockTime(0), m_stun(0), m_justSpotted(false),
    m_roamPause(0), m_rng(1)
{
}

void Enemy::Spawn(float x, float y, EnemyType type, float avoidX, float avoidY)
{
    m_x = x;
    m_y = y;
    m_type = type;
    m_targetX = m_targetY = -1;
    m_nextX = m_nextY = -1;
    m_repathTimer = 0.0f;
    m_biteCooldown = 0.0f;
    m_moveX = m_moveY = 0.0f;

    m_homeX = (int)x;
    m_homeY = (int)y;
    m_avoidX = avoidX;
    m_avoidY = avoidY;
    m_mode = Mode::Roam;
    m_lureTimer = 0.0f;
    m_knockTime = m_stun = 0.0f;
    m_justSpotted = false;
    m_rng = (unsigned)m_homeX * 73856093u ^ (unsigned)m_homeY * 19349663u ^ ((unsigned)type + 1) * 83492791u;
    if (m_rng == 0) m_rng = 1;
    m_roamPause = (NextRandom() % 100) / 100.0f * DOG_ROAM_PAUSE_MAX;   // don't all start at once
}

void Enemy::Knockback(float fromX, float fromY)
{
    float dx = m_x - fromX, dy = m_y - fromY;
    float len = sqrtf(dx * dx + dy * dy);
    if (len < 0.01f)   // right on top of the cat: any direction
    {
        float a = (NextRandom() % 628) / 100.0f;
        dx = cosf(a); dy = sinf(a); len = 1.0f;
    }
    m_knockVX = dx / len * KNOCKBACK_SPEED;
    m_knockVY = dy / len * KNOCKBACK_SPEED;
    m_knockTime = KNOCKBACK_DISTANCE / KNOCKBACK_SPEED;
    m_stun = 0.0f;
    m_nextX = m_nextY = -1;   // repath once it's back on its feet
}

void Enemy::Spot()
{
    m_mode = Mode::Chase;
    m_justSpotted = true;
    m_nextX = m_nextY = -1;   // repath right away, for the chase
}

void Enemy::Lure(float x, float y, float seconds)
{
    m_mode = Mode::Lured;
    m_lureX = (int)x;
    m_lureY = (int)y;
    m_lureTimer = seconds;
    m_targetX = m_lureX;
    m_targetY = m_lureY;
    m_nextX = m_nextY = -1;
}

void Enemy::GoToLure(float dt, const Map& map, const Enemy* all, int count)
{
    m_repathTimer -= dt;
    if (m_repathTimer <= 0.0f || m_nextX < 0)
    {
        m_repathTimer = ENEMY_REPATH_TIME;
        if (!FindNextStep(map, all, count, m_lureX, m_lureY)) m_nextX = m_nextY = -1;
    }
    // Other dogs got there first: it waits next to them (StepToward won't
    // walk into a dog's tile), which looks like a crowd around the bone.
    StepToward(dt, ENEMY_SPEED, nullptr, all, count, map);
}

void Enemy::ReturnHome(float dt, const Map& map, const Enemy* all, int count)
{
    int tx = (int)m_x, ty = (int)m_y;
    bool inArea = tx >= m_homeX - DOG_ROAM_RADIUS && tx <= m_homeX + DOG_ROAM_RADIUS &&
        ty >= m_homeY - DOG_ROAM_RADIUS && ty <= m_homeY + DOG_ROAM_RADIUS;
    if (inArea)
    {
        m_mode = Mode::Roam;
        m_targetX = m_targetY = m_nextX = m_nextY = -1;
        m_roamPause = DOG_ROAM_PAUSE_MIN;
        return;
    }

    m_targetX = m_homeX;
    m_targetY = m_homeY;
    m_repathTimer -= dt;
    if (m_repathTimer <= 0.0f || m_nextX < 0)
    {
        m_repathTimer = ENEMY_REPATH_TIME;
        if (!FindNextStep(map, all, count, m_homeX, m_homeY))
        {
            // Can't get home right now (dogs in the way): wait, try again soon.
            m_nextX = m_nextY = -1;
            return;
        }
    }
    StepToward(dt, DOG_RETURN_SPEED, nullptr, all, count, map);
}

unsigned Enemy::NextRandom()
{
    // xorshift32
    m_rng ^= m_rng << 13;
    m_rng ^= m_rng >> 17;
    m_rng ^= m_rng << 5;
    return m_rng;
}

bool Enemy::CanSee(const Map& map, const Player& player) const
{
    float dx = player.X() - m_x, dy = player.Y() - m_y;
    float dist = sqrtf(dx * dx + dy * dy);
    if (dist > DOG_SPOT_RANGE) return false;
    if (!DOG_NEEDS_SIGHT) return true;

    // Walk the line in small steps; houses and fences block, hedges are low.
    int steps = (int)(dist / 0.1f) + 1;
    for (int i = 1; i < steps; ++i)
    {
        float f = (float)i / steps;
        int tx = (int)(m_x + dx * f), ty = (int)(m_y + dy * f);
        if (map.IsSolid(tx, ty) && !Map::IsLowWall(map.TileAt(tx, ty))) return false;
    }
    return true;
}

bool Enemy::PickRoamTarget(const Map& map, const Enemy* all, int count)
{
    const int span = DOG_ROAM_RADIUS * 2 + 1;
    for (int attempt = 0; attempt < 12; ++attempt)
    {
        int tx = m_homeX + (int)(NextRandom() % span) - DOG_ROAM_RADIUS;
        int ty = m_homeY + (int)(NextRandom() % span) - DOG_ROAM_RADIUS;
        if (map.IsWall(tx, ty)) continue;
        if (tx == (int)m_x && ty == (int)m_y) continue;

        float ax = tx + 0.5f - m_avoidX, ay = ty + 0.5f - m_avoidY;
        float keepAway = DOG_SPOT_RANGE + 1.5f;
        if (ax * ax + ay * ay < keepAway * keepAway) continue;
        if (TileHasEnemy(this, all, count, tx, ty) || TileIsTargeted(this, all, count, tx, ty)) continue;

        // Only somewhere it can actually walk to (not across a house).
        if (!FindNextStep(map, all, count, tx, ty, true)) continue;
        m_targetX = tx;
        m_targetY = ty;
        return true;
    }
    return false;
}

void Enemy::Roam(float dt, const Map& map, const Enemy* all, int count)
{
    if (m_roamPause > 0.0f)
    {
        m_roamPause -= dt;
        return;
    }

    // No walk going: pick a new spot.
    if (m_targetX < 0)
    {
        if (!PickRoamTarget(map, all, count))
        {
            m_roamPause = DOG_ROAM_PAUSE_MIN;   // boxed in, try again in a bit
            return;
        }
        m_repathTimer = ENEMY_REPATH_TIME;
    }

    // Repath now and then (other dogs move around too).
    m_repathTimer -= dt;
    if (m_repathTimer <= 0.0f || m_nextX < 0)
    {
        m_repathTimer = ENEMY_REPATH_TIME;
        if (!FindNextStep(map, all, count, m_targetX, m_targetY, true))
        {
            m_targetX = m_targetY = -1;
            m_nextX = m_nextY = -1;
            m_roamPause = DOG_ROAM_PAUSE_MIN;
            return;
        }
    }

    StepToward(dt, DOG_ROAM_SPEED, nullptr, all, count, map);

    // Reached the spot: sniff around for a moment.
    if ((int)m_x == m_targetX && (int)m_y == m_targetY && m_nextX < 0)
    {
        m_targetX = m_targetY = -1;
        float t = (NextRandom() % 1000) / 1000.0f;
        m_roamPause = DOG_ROAM_PAUSE_MIN + (DOG_ROAM_PAUSE_MAX - DOG_ROAM_PAUSE_MIN) * t;
    }
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

bool Enemy::FindNextStep(const Map& map, const Enemy* all, int count, int goalX, int goalY,
    bool stayHome)
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
            if (stayHome && (nx < m_homeX - DOG_ROAM_RADIUS || nx > m_homeX + DOG_ROAM_RADIUS ||
                ny < m_homeY - DOG_ROAM_RADIUS || ny > m_homeY + DOG_ROAM_RADIUS))
                continue;

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
    m_justSpotted = false;

    // Flying from a ground pound (stops at walls), then dazed.
    if (m_knockTime > 0.0f)
    {
        float step = dt < m_knockTime ? dt : m_knockTime;
        float nx = m_x + m_knockVX * step, ny = m_y + m_knockVY * step;
        float oldX = m_x, oldY = m_y;
        if (!map.IsWall((int)nx, (int)m_y)) m_x = nx;
        if (!map.IsWall((int)m_x, (int)ny)) m_y = ny;
        if (m_x != oldX || m_y != oldY) { m_moveX = oldX - m_x; m_moveY = oldY - m_y; }   // keeps facing the cat
        m_knockTime -= dt;
        if (m_knockTime <= 0.0f) { m_knockTime = 0.0f; m_stun = KNOCKBACK_STUN; }
        return;
    }
    if (m_stun > 0.0f)
    {
        m_stun -= dt;
        return;
    }

    // Just bit someone: sit still for a moment.
    if (m_biteCooldown > 0.0f)
    {
        m_biteCooldown -= dt;
        return;
    }

    switch (m_mode)
    {
    case Mode::Lured:
        m_lureTimer -= dt;
        if (m_lureTimer > 0.0f)
        {
            GoToLure(dt, map, all, count);
            return;
        }
        // Bone's gone: you again if you're close, otherwise home.
        if (CanSee(map, player)) { Spot(); break; }
        m_mode = Mode::ReturnHome;
        m_targetX = m_targetY = m_nextX = m_nextY = -1;
        return;

    case Mode::ReturnHome:
        if (CanSee(map, player)) { Spot(); break; }
        ReturnHome(dt, map, all, count);
        return;

    case Mode::Roam:
        if (CanSee(map, player)) { Spot(); break; }
        Roam(dt, map, all, count);
        return;

    case Mode::Chase:
        break;
    }

    m_repathTimer -= dt;
    if (m_repathTimer <= 0.0f || m_nextX < 0)
    {
        Repath(map, player, all, count);
        m_repathTimer = ENEMY_REPATH_TIME;
    }
    if (m_nextX < 0) return;

    StepToward(dt, ENEMY_SPEED, &player, all, count, map);
}

void Enemy::StepToward(float dt, float speed, const Player* chaseTarget,
    const Enemy* all, int count, const Map& map)
{
    if (m_nextX < 0) return;

    // Walk toward the center of the next tile.
    float goalX = m_nextX + 0.5f, goalY = m_nextY + 0.5f;
    float dx = goalX - m_x, dy = goalY - m_y;
    float dist = sqrtf(dx * dx + dy * dy);

    // Standing on the player's tile: walk straight at them instead of the
    // tile center, so we can actually reach them.
    if (chaseTarget &&
        m_nextX == (int)chaseTarget->X() && m_nextY == (int)chaseTarget->Y() &&
        (int)m_x == m_nextX && (int)m_y == m_nextY)
    {
        dx = chaseTarget->X() - m_x;
        dy = chaseTarget->Y() - m_y;
        dist = sqrtf(dx * dx + dy * dy);
    }

    float move = speed * dt;
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

    float oldX = m_x, oldY = m_y;
    if (!map.IsWall((int)nx, (int)m_y)) m_x = nx;
    if (!map.IsWall((int)m_x, (int)ny)) m_y = ny;
    if (m_x != oldX || m_y != oldY) { m_moveX = m_x - oldX; m_moveY = m_y - oldY; }
}