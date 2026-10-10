#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <math.h>
#include "Player.h"
#include "World.h"
#include "Game.h"
#include "Config.h"

Player::Player()
{
    Reset();
}

void Player::Reset()
{
    m_x = 1.5f;      m_y = 1.5f;
    m_dirX = 1.0f;   m_dirY = 0.0f;
    m_planeX = 0.0f; m_planeY = 0.66f;
}

void Player::Rotate(float a)
{
    float c = cosf(a), s = sinf(a);
    float old = m_dirX;
    m_dirX = m_dirX * c - m_dirY * s;
    m_dirY = old * s + m_dirY * c;
    old = m_planeX;
    m_planeX = m_planeX * c - m_planeY * s;
    m_planeY = old * s + m_planeY * c;
}

void Player::Move(float dx, float dy, const Map& map)
{
    // X and Y checked separately so you slide along walls
    if (!map.IsWall((int)(m_x + dx), (int)m_y)) m_x += dx;
    if (!map.IsWall((int)m_x, (int)(m_y + dy))) m_y += dy;
}

void Player::Update(float dt, const Input& input, const Map& map)
{
    // look
    float turn = input.MouseDX() * MOUSE_SENS;
    if (input.IsDown(VK_LEFT))  turn -= TURN_SPEED * dt;
    if (input.IsDown(VK_RIGHT)) turn += TURN_SPEED * dt;
    if (turn != 0.0f) Rotate(turn);

    // move
    float speed = MOVE_SPEED * dt;
    if (input.IsDown('W') || input.IsDown(VK_UP))   Move(m_dirX * speed, m_dirY * speed, map);
    if (input.IsDown('S') || input.IsDown(VK_DOWN)) Move(-m_dirX * speed, -m_dirY * speed, map);
    if (input.IsDown('D'))                          Move(-m_dirY * speed, m_dirX * speed, map);
    if (input.IsDown('A'))                          Move(m_dirY * speed, -m_dirX * speed, map);
}