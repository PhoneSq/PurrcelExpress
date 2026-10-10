#include <math.h>
#include "World.h"

// =====================================================
// MAP
// =====================================================

static const char* const LEVEL[16] =
{
    "################",
    "#S.............#",
    "#..##......##..#",
    "#..#........#..#",
    "#..............#",
    "#.....####.....#",
    "#.....#..#.....#",
    "#.....#..#.....#",
    "#..............#",
    "#..#........#..#",
    "#..##......##..#",
    "#..............#",
    "#......##......#",
    "#.............D#",
    "#..............#",
    "################",
};

Map::Map()
    : m_rows(LEVEL),
    m_startX(1.5f), m_startY(1.5f),
    m_spotX(1.5f), m_spotY(1.5f)
{
    for (int y = 0; y < HEIGHT; ++y)
        for (int x = 0; x < WIDTH; ++x)
        {
            if (m_rows[y][x] == 'S') { m_startX = x + 0.5f; m_startY = y + 0.5f; }
            if (m_rows[y][x] == 'D') { m_spotX = x + 0.5f; m_spotY = y + 0.5f; }
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