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