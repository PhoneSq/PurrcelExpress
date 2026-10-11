#pragma once

// =====================================================
// Enemies. Each dog wanders around its spawn point until
// it spots the player, then chases. Each type chases a
// different tile, and they all walk with BFS pathfinding.
//
// A dropped bone pulls nearby dogs off you for a while;
// afterwards they chase again if you're in sight, or walk
// home and go back to wandering.
// =====================================================

#include "Config.h"

class Map;
class Player;


enum class EnemyType
{
    Blue,     // chases the player directly
    Yellow,   // heads for where the player is looking (cuts you off)
    Purple,   // flanks: goes to the opposite side of you from the nearest blue
    Alert     // watchdog: stays put looking around, barks to call the others
};


class Enemy
{
public:
    Enemy();

    // (avoidX, avoidY): a spot it never wanders close to (the start),
    // so you aren't spotted the moment a delivery begins.
    void Spawn(float x, float y, EnemyType type, float avoidX = -1000.0f, float avoidY = -1000.0f);

    // 'all' / 'count' = every enemy in the level (this one included),
    // so enemies can avoid each other's tiles.
    void Update(float dt, const Map& map, const Player& player,
        const Enemy* all, int count);

    // Has it spotted the player? (Then it keeps chasing until a bone distracts it.)
    bool IsChasing() const { return m_mode == Mode::Chase; }

    // ----- Watchdog -----
    bool  IsWatchdog() const { return m_type == EnemyType::Alert; }
    // Where it's looking (radians, world angle) and whether it can see you right now.
    float LookAngle() const { return m_lookAngle; }
    bool  SeesPlayer() const { return m_seesPlayer; }
    // At its post and awake (its view cone counts).
    bool  IsWatching() const { return IsWatchdog() && m_mode == Mode::Roam && !IsKnockedBack(); }
    // True for the one frame it barked.
    bool  JustBarked() const { return m_justBarked; }
    // A watchdog barked nearby: come running (unless busy with a bone).
    void  HearBark();

    // Busy with a bone at (x, y) for 'seconds': ignores the player, doesn't bite.
    void Lure(float x, float y, float seconds);
    bool IsLured() const { return m_mode == Mode::Lured; }

    // True for the one frame this dog spotted the player.
    bool JustSpotted() const { return m_justSpotted; }

    // Sent flying away from (fromX, fromY), then dazed for a moment.
    void Knockback(float fromX, float fromY);
    bool IsKnockedBack() const { return m_knockTime > 0.0f || m_stun > 0.0f; }

    // Seeing stars: stands still, can't bite.
    void Stun(float seconds) { if (seconds > m_stun) m_stun = seconds; }
    bool IsStunned() const { return m_stun > 0.0f && m_knockTime <= 0.0f; }

    // Biting: a dog that just bit sits still for DOG_BITE_COOLDOWN.
    bool CanBite() const { return !IsWatchdog() && m_biteCooldown <= 0.0f && m_mode != Mode::Lured && !IsKnockedBack(); }
    void OnBite() { m_biteCooldown = DOG_BITE_COOLDOWN; }

    // Direction of the last step (for flipping the sprite). Zero if it hasn't moved.
    float MoveX() const { return m_moveX; }
    float MoveY() const { return m_moveY; }

    float     X() const { return m_x; }
    float     Y() const { return m_y; }
    EnemyType Type() const { return m_type; }

    // The tile this enemy is heading for. Other enemies read this
    // so two enemies never aim at the same tile.
    int TargetTileX() const { return m_targetX; }
    int TargetTileY() const { return m_targetY; }

private:
    enum class Mode
    {
        Roam,         // wandering near home, watching for the player
        Chase,        // after the player
        Lured,        // going for a bone
        ReturnHome    // bone's gone and the player isn't around: walk back
    };

    void Spot();                    // start chasing (and show the "!")
    void ReturnHome(float dt, const Map& map, const Enemy* all, int count);
    void GoToLure(float dt, const Map& map, const Enemy* all, int count);

    // Which tile this enemy wants to reach, based on its type.
    void ChooseTarget(const Map& map, const Player& player,
        const Enemy* all, int count, int& outX, int& outY) const;

    // BFS from our tile to (goalX, goalY). Stores the first step in
    // m_nextX/m_nextY. False if the goal can't be reached.
    // stayHome: only walk inside the wandering area around the spawn tile.
    bool FindNextStep(const Map& map, const Enemy* all, int count, int goalX, int goalY,
        bool stayHome = false);

    void Repath(const Map& map, const Player& player, const Enemy* all, int count);

    // Close enough and (if DOG_NEEDS_SIGHT) nothing solid in between?
    bool CanSee(const Map& map, const Player& player) const;
    // Nothing solid (houses, fences - not hedges) between us and (x, y)?
    bool ClearLine(const Map& map, float x, float y) const;
    // Watchdog sight: far, but only inside the view cone (or very close).
    bool CanSeeInCone(const Map& map, const Player& player) const;
    // Watchdog at its post: look around, follow the player and bark.
    void Watch(float dt, const Map& map, const Player& player);

    // Wandering: pick a random open tile near home and path to it.
    void Roam(float dt, const Map& map, const Enemy* all, int count);
    bool PickRoamTarget(const Map& map, const Enemy* all, int count);
    unsigned NextRandom();

    // Steps toward m_nextX/m_nextY (clears them on arrival). chaseTarget:
    // the player when chasing, so it closes the last bit inside their tile.
    void StepToward(float dt, float speed, const Player* chaseTarget,
        const Enemy* all, int count, const Map& map);

    float     m_x, m_y;
    EnemyType m_type;

    int   m_targetX, m_targetY;   // where we're ultimately going
    int   m_nextX, m_nextY;       // the next tile on the way (-1 = none)
    float m_repathTimer;
    float m_biteCooldown;
    float m_moveX, m_moveY;

    int      m_homeX, m_homeY;   // spawn tile: the middle of its wandering area
    float    m_avoidX, m_avoidY; // never wander near here
    Mode     m_mode;
    int      m_lureX, m_lureY;   // bone tile
    float    m_lureTimer;        // seconds left on the bone

    float    m_knockVX, m_knockVY; // flying velocity (tiles per second)
    float    m_knockTime;          // seconds of flying left
    float    m_stun;               // seconds dazed after landing
    bool     m_justSpotted;
    float    m_lookAngle;        // watchdog: where it's looking (radians)
    float    m_lookGoal;         // ...and where it's turning to
    float    m_glanceTimer;      // seconds until it picks a new direction
    float    m_barkTimer;        // seconds until it can bark again
    bool     m_seesPlayer;
    bool     m_justBarked;
    float    m_roamPause;        // seconds left sniffing around before the next walk
    unsigned m_rng;              // own little random generator for wandering
};