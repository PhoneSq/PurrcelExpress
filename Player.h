#pragma once

class Player
{
public:
    Player();

    void Reset();
    void Update(float dt, const bool keys[256], int mouseDX);

    float x, y;            // position in tiles
    float dirX, dirY;      // facing direction
    float planeX, planeY;  // camera plane (sets the FOV)

private:
    void Rotate(float angle);
    void Move(float dx, float dy);
};
