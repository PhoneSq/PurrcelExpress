#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <math.h>
#include "Player.h"
#include "Config.h"
#include "Map.h"

Player::Player()
{
    Reset();
}

void Player::Reset()
{
    x = 1.5f;   y = 1.5f;
    dirX = 1.0f; dirY = 0.0f;
    planeX = 0.0f; planeY = 0.66f;
}

void Player::Rotate(float a)
{
    float c = cosf(a), s = sinf(a);
    float old = dirX;
    dirX = dirX * c - dirY * s;
    dirY = old * s + dirY * c;
    old = planeX;
    planeX = planeX * c - planeY * s;
    planeY = old * s + planeY * c;
}

void Player::Move(float dx, float dy)
{
    // X and Y checked separately so you slide along walls
    if (!IsWall((int)(x + dx), (int)y)) x += dx;
    if (!IsWall((int)x, (int)(y + dy))) y += dy;
}

void Player::Update(float dt, const bool keys[256], int mouseDX)
{
    // look
    float turn = mouseDX * MOUSE_SENS;
    if (keys[VK_LEFT])  turn -= TURN_SPEED * dt;
    if (keys[VK_RIGHT]) turn += TURN_SPEED * dt;
    if (turn != 0.0f) Rotate(turn);

    // move
    float speed = MOVE_SPEED * dt;
    if (keys['W'] || keys[VK_UP])   Move(dirX * speed, dirY * speed);
    if (keys['S'] || keys[VK_DOWN]) Move(-dirX * speed, -dirY * speed);
    if (keys['D'])                  Move(-dirY * speed, dirX * speed);
    if (keys['A'])                  Move(dirY * speed, -dirX * speed);
}