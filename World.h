#pragma once

// =====================================================
// The game world: the map and the delivery objective
// (later also enemy spawns and level generation).
// =====================================================

class Map
{
public:
    Map();

    int Width()  const { return WIDTH; }
    int Height() const { return HEIGHT; }

    // True for walls and anything outside the map.
    bool IsWall(int x, int y) const;

    // Centers of the 'S' (start) and 'D' (drop-off) tiles in the level.
    float StartX() const { return m_startX; }
    float StartY() const { return m_startY; }
    float SpotX()  const { return m_spotX; }
    float SpotY()  const { return m_spotY; }

    // Enemy spawn tiles: 'B' blue, 'Y' yellow, 'P' purple.
    struct Spawn
    {
        float x, y;
        char  kind;   // 'B', 'Y' or 'P'
    };
    int          SpawnCount()     const { return m_spawnCount; }
    const Spawn& GetSpawn(int i)  const { return m_spawns[i]; }

private:
    static const int WIDTH = 16;
    static const int HEIGHT = 16;
    static const int MAX_SPAWNS = 8;

    const char* const* m_rows;   // see the legend in World.cpp

    float m_startX, m_startY;
    float m_spotX, m_spotY;

    Spawn m_spawns[MAX_SPAWNS];
    int   m_spawnCount;
};


// One delivery: walk to the drop-off, then back to the start.
class Delivery
{
public:
    enum class Phase
    {
        ToSpot,      // carrying the package
        ReturnHome,  // package dropped, head back
        Done
    };

    void Start(float startX, float startY, float spotX, float spotY);

    // Moves to the next phase when the player reaches the target.
    void Update(float playerX, float playerY);

    Phase GetPhase() const { return m_phase; }

    // Where the player should go right now.
    float TargetX() const;
    float TargetY() const;

    float DistanceFrom(float x, float y) const;

private:
    Phase m_phase = Phase::ToSpot;
    float m_startX = 0, m_startY = 0;
    float m_spotX = 0, m_spotY = 0;
};