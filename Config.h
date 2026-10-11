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
const float ROOF_SLOPE = 0.6f;   // roof rise per tile in from the edge (wall heights)
const float ROOF_MAX_RISE = 0.9f;   // tallest a roof gets above the walls
const float PACKAGE_SCALE = 0.18f;  // delivered package left on the doorstep
// Dog height compared to a wall, per type: { Blue (Chasey), Yellow, Purple (Blocky) }
const float DOG_SPRITE_SCALES[3] = { 0.48f, 0.40f, 0.40f };
const bool  DOG_SPRITE_FACES_RIGHT = true;  // which way the dog PNGs face (flipped for the other way)

// Walk animation (made from one picture: bounce, squash, waddle)
const float WALK_BOUNCE = 0.022f; // hop height per step, in wall heights
const float WALK_SQUASH = 0.07f;  // how much it squashes / stretches (0.07 = 7%)
const float WALK_LEAN = 0.06f;  // side-to-side lean, as a share of its width
const float CAT_STEPS_PER_TILE = 2.2f;   // short fat legs: lots of little steps
const float DOG_STEPS_PER_TILE = 2.6f;

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

// Abilities (E)
const int   ABILITY_KEY = 'E';
const float GROUND_POUND_COOLDOWN = 6.0f;   // seconds
const float GROUND_POUND_AIR_TIME = 0.55f;  // seconds from jump to slam
const float GROUND_POUND_HEIGHT = 0.35f;  // how high, in wall heights
const float GROUND_POUND_RADIUS = 2.5f;   // tiles: dogs closer than this get knocked back
const float KNOCKBACK_DISTANCE = 2.0f;   // tiles they fly
const float KNOCKBACK_SPEED = 9.0f;   // tiles per second while flying
const float KNOCKBACK_STUN = 0.6f;   // seconds dazed after landing
const float SLAM_SHAKE_TIME = 0.25f;  // screen shake on landing

const float DASH_COOLDOWN = 7.0f;   // seconds
const float DASH_DISTANCE = 2.0f;   // tiles, the way you're looking
const float DASH_TIME = 0.3f;   // seconds it takes (you can't be bitten the whole time)
const float DASH_LIFT = 0.12f;  // floats up this much (wall heights) mid-dash
const float DASH_GRACE = 0.15f;  // still can't be bitten this long after landing

const float FUR_BARRAGE_TIME = 5.0f;   // seconds you can shoot fur balls
const float FUR_BARRAGE_COOLDOWN = 12.0f;  // seconds from activating until it's ready again
const float FUR_FIRE_INTERVAL = 0.25f;  // seconds between shots (click / SPACE)
const float FUR_SPEED = 12.0f;  // tiles per second
const float FUR_RANGE = 10.0f;  // tiles before it drops
const float FUR_HIT_RADIUS = 0.45f;  // tiles
const float FUR_STUN_TIME = 2.0f;   // seconds a hit dog sees stars
const float FUR_SCALE = 0.12f;  // fur ball size compared to a wall

// Sound
const float SFX_VOLUME = 0.7f;  // master volume for every sound effect (0..1)
const float FOOTSTEP_VOLUME = 0.10f; // the cat's squishy paw steps (0 = off)

// Time of day (rolled per delivery)
// How often each comes up: { Morning, Noon, Sunset, Night }
const int   TIME_OF_DAY_WEIGHTS[4] = { 20, 20, 25, 35 };
const float CLOUD_DRIFT = 6.0f;   // sky pixels per second the clouds slide

// Maps: more enemies = bigger map
const int   MAP_BASE_SIZE = 16;     // tiles per side with MIN_ENEMIES
const int   MAP_GROWTH = 2;      // extra tiles per side per extra enemy
const int   MAP_MAX_SIZE = 40;