#pragma once

// =====================================================
// The cat you control. Characters and abilities will
// live here too when we add them.
// =====================================================

class Input;
class Map;


class Player
{
public:
    Player();

    void Reset();
    void Update(float dt, const Input& input, const Map& map);

    float X() const { return m_x; }
    float Y() const { return m_y; }
    float DirX() const { return m_dirX; }
    float DirY() const { return m_dirY; }
    float PlaneX() const { return m_planeX; }   // camera plane (sets the FOV)
    float PlaneY() const { return m_planeY; }

private:
    void Rotate(float angle);
    void Move(float dx, float dy, const Map& map);

    float m_x, m_y;
    float m_dirX, m_dirY;
    float m_planeX, m_planeY;
};