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

// Sprint (hold SHIFT)
const float SPRINT_MULTIPLIER = 1.6f;   // speed while sprinting
const float STAMINA_MAX_SECONDS = 4.0f;   // seconds of sprint on a full bar
const float STAMINA_RECOVER_RATE = 0.8f;   // seconds refilled per second of rest
const float STAMINA_RECOVER_DELAY = 0.6f;   // pause after sprinting before refilling
const float STAMINA_RESTART_AT = 0.25f;  // after running out, locked until this full

// Third person (V toggles)
const float THIRD_PERSON_DISTANCE = 1.3f;   // tiles behind the cat
const float CAT_SPRITE_SCALE = 0.32f;  // cat height compared to a wall
const float HEDGE_HEIGHT = 0.42f;  // hedges are low walls: this much of a wall's height
const float PACKAGE_SCALE = 0.18f;  // delivered package left on the doorstep
// Dog height compared to a wall, per type: { Blue (Chasey), Yellow, Purple (Blocky) }
const float DOG_SPRITE_SCALES[3] = { 0.48f, 0.40f, 0.40f };
const bool  DOG_SPRITE_FACES_RIGHT = true;  // which way the dog PNGs face (flipped for the other way)

// Enemies
const int   MAX_ENEMIES = 12;
const int   MIN_ENEMIES = 3;      // always one of each type
const float ENEMY_SPEED = 1.6f;   // tiles per second
const float ENEMY_REPATH_TIME = 0.2f;   // seconds between path updates
const float ENEMY_CATCH_RANGE = 0.45f;  // tiles: closer than this = bitten
const float DOG_BITE_COOLDOWN = 2.0f;   // seconds a dog sits still after biting
const float ENEMY_SPAWN_DIST = 6.0f;   // tiles: never spawn closer than this to the start

// Dogs roam around where they spawned until they spot you, then chase for good.
const float DOG_SPOT_RANGE = 4.5f;  // tiles: closer than this (and in sight) = spotted
const bool  DOG_NEEDS_SIGHT = true;  // houses and fences block their view (hedges don't)
const int   DOG_ROAM_RADIUS = 3;     // tiles around the spawn point they wander in
const float DOG_ROAM_SPEED = 0.9f;  // tiles per second while wandering
const float DOG_ROAM_PAUSE_MIN = 0.5f;  // seconds they sniff around between walks
const float DOG_ROAM_PAUSE_MAX = 2.0f;
const float DOG_RETURN_SPEED = 1.2f;  // tiles per second walking back home (after a bone)
const float SPOTTED_ALERT_TIME = 1.2f;  // seconds the red "!" stays over the cat

// Health (hit points, not percent)
const int   PLAYER_MAX_HP = 3;
const float HIT_INVULN_TIME = 1.2f;   // seconds you can't be hit again after a bite

// Deliveries
const int   DELIVERY_CHOICES = 5;      // deliveries offered on the select screen
const int   MAX_STOPS = 4;      // stops = difficulty, capped here
const float REACH_DISTANCE = 0.6f;   // tiles: closer than this = reached

// Express (one timed stop with a bonus)
const int   EXPRESS_CHANCE = 60;     // percent, only for deliveries with 2+ stops
const int   EXPRESS_MIN_TIME = 40;     // seconds
const int   EXPRESS_MAX_TIME = 50;
const float EXPRESS_URGENT = 10.0f;  // seconds left when the timer turns red

// Shop (one item at a time; it stays with you until used on a delivery)
const int   HEALTH_POTION_PRICE = 150;   // +1 HP
const int   STAMINA_POTION_PRICE = 100;   // stamina back to full
const int   BONE_PRICE = 120;   // drop it: nearby dogs go for it instead of you
const float BONE_LURE_TIME = 3.0f;  // seconds the bone keeps dogs busy (then it's gone)
const float BONE_LURE_RANGE = 7.0f;  // tiles: dogs closer than this smell it
const float BONE_SCALE = 0.10f; // bone height compared to a wall
const float HEART_SCALE = 0.12f; // hearts over a dog that's after the bone
const int   ITEM_USE_KEY = 'Q';

// Time of day (rolled per delivery)
const int   NIGHT_CHANCE = 50;     // percent of deliveries at night

// Maps: more enemies = bigger map
const int   MAP_BASE_SIZE = 16;     // tiles per side with MIN_ENEMIES
const int   MAP_GROWTH = 2;      // extra tiles per side per extra enemy
const int   MAP_MAX_SIZE = 40;