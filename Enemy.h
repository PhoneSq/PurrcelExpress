#pragma once

// =====================================================
// Enemies. Each type picks a different tile to chase,
// then they all walk there with BFS pathfinding.
// =====================================================

class Map;
class Player;


enum class EnemyType
{
    Blue,     // chases the player directly
    Yellow,   // heads for where the player is looking (cuts you off)
    Purple    // flanks: goes to the opposite side of you from the nearest blue
};


class Enemy
{
public:
    Enemy();

    void Spawn(float x, float y, EnemyType type);

    // 'all' / 'count' = every enemy in the level (this one included),
    // so enemies can avoid each other's tiles.
    void Update(float dt, const Map& map, const Player& player,
        const Enemy* all, int count);

    float     X() const { return m_x; }
    float     Y() const { return m_y; }
    EnemyType Type() const { return m_type; }

    // The tile this enemy is heading for. Other enemies read this
    // so two enemies never aim at the same tile.
    int TargetTileX() const { return m_targetX; }
    int TargetTileY() const { return m_targetY; }

private:
    // Which tile this enemy wants to reach, based on its type.
    void ChooseTarget(const Map& map, const Player& player,
        const Enemy* all, int count, int& outX, int& outY) const;

    // BFS from our tile to (goalX, goalY). Stores the first step in
    // m_nextX/m_nextY. False if the goal can't be reached.
    bool FindNextStep(const Map& map, const Enemy* all, int count, int goalX, int goalY);

    void Repath(const Map& map, const Player& player, const Enemy* all, int count);

    float     m_x, m_y;
    EnemyType m_type;

    int   m_targetX, m_targetY;   // where we're ultimately going
    int   m_nextX, m_nextY;       // the next tile on the way (-1 = none)
    float m_repathTimer;
};
