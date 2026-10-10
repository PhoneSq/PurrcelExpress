#pragma once

// Every tuning knob in one place.

// Internal resolution (stretched up to the window)
const int   SCREEN_W = 640;
const int   SCREEN_H = 400;
const int   SCALE = 2;

const wchar_t* const GAME_TITLE = L"PURRCEL EXPRESS";

// Player
const float MOVE_SPEED = 3.0f;    // tiles per second
const float TURN_SPEED = 2.5f;    // radians per second (arrow keys)
const float MOUSE_SENS = 0.003f;  // radians per mouse pixel

// Enemies
const int   MAX_ENEMIES = 8;
const float ENEMY_SPEED = 1.6f;   // tiles per second
const float ENEMY_REPATH_TIME = 0.2f;   // seconds between path updates
const float ENEMY_CATCH_RANGE = 0.45f;  // tiles: closer than this = caught