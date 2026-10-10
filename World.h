#pragma once

// =====================================================
// The game world: the map (later also delivery spots,
// enemy spawns and level generation).
// =====================================================

class Map
{
public:
    Map();

    int Width()  const { return WIDTH; }
    int Height() const { return HEIGHT; }

    // True for walls and anything outside the map.
    bool IsWall(int x, int y) const;

private:
    static const int WIDTH = 16;
    static const int HEIGHT = 16;

    const char* const* m_rows;   // '#' = wall, '.' = empty
};