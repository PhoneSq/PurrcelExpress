#include <math.h>
#include "World.h"

// =====================================================
// MAP
// =====================================================

// Legend:
//   '#' wall      '.' empty
//   'S' start     'D' drop-off
//   'B' blue enemy   'Y' yellow enemy   'P' purple enemy
static const char* const LEVEL[16] =
{
    "################",
    "#S...........B.#",
    "#..##......##..#",
    "#..#........#..#",
    "#..............#",
    "#.....####.....#",
    "#.....#P.#.....#",
    "#.....#..#.....#",
    "#..............#",
    "#..#........#..#",
    "#..##......##..#",
    "#..............#",
    "#......##......#",
    "#.............D#",
    "#.Y............#",
    "################",
};

Map::Map()
    : m_rows(LEVEL),
    m_startX(1.5f), m_startY(1.5f),
    m_spotX(1.5f), m_spotY(1.5f),
    m_spawnCount(0)
{
    for (int y = 0; y < HEIGHT; ++y)
        for (int x = 0; x < WIDTH; ++x)
        {
            char c = m_rows[y][x];
            float cx = x + 0.5f, cy = y + 0.5f;

            if (c == 'S') { m_startX = cx; m_startY = cy; }
            if (c == 'D') { m_spotX = cx; m_spotY = cy; }

            if ((c == 'B' || c == 'Y' || c == 'P') && m_spawnCount < MAX_SPAWNS)
            {
                m_spawns[m_spawnCount].x = cx;
                m_spawns[m_spawnCount].y = cy;
                m_spawns[m_spawnCount].kind = c;
                ++m_spawnCount;
            }
        }
}

bool Map::IsWall(int x, int y) const
{
    if (x < 0 || y < 0 || x >= WIDTH || y >= HEIGHT) return true;
    return m_rows[y][x] == '#';
}


// =====================================================
// DELIVERY
// =====================================================

// How close (in tiles) counts as "reached".
static const float REACH_DISTANCE = 0.6f;

void Delivery::Start(float startX, float startY, float spotX, float spotY)
{
    m_startX = startX; m_startY = startY;
    m_spotX = spotX;  m_spotY = spotY;
    m_phase = Phase::ToSpot;
}

float Delivery::TargetX() const
{
    return (m_phase == Phase::ToSpot) ? m_spotX : m_startX;
}

float Delivery::TargetY() const
{
    return (m_phase == Phase::ToSpot) ? m_spotY : m_startY;
}

float Delivery::DistanceFrom(float x, float y) const
{
    float dx = TargetX() - x;
    float dy = TargetY() - y;
    return sqrtf(dx * dx + dy * dy);
}

void Delivery::Update(float playerX, float playerY)
{
    if (m_phase == Phase::Done) return;
    if (DistanceFrom(playerX, playerY) > REACH_DISTANCE) return;

    m_phase = (m_phase == Phase::ToSpot) ? Phase::ReturnHome : Phase::Done;
}