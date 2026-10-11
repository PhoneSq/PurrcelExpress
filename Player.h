#pragma once

// =====================================================
// The cat you control. Which cat it is (and its ability)
// comes from Character.h.
// =====================================================

#include "Character.h"

class Input;
class Map;



// Sprint stamina. Sprinting drains it; after a short pause it refills.
// Running out locks sprinting until the bar is partly full again.
class Stamina
{
public:
    // Full bar. multiplier 1.0 = normal (an upgrade can raise it later:
    // bigger bar and faster refill).
    void Reset(float multiplier = 1.0f);

    // wantsSprint = SHIFT held while moving.
    // Returns true if the player really sprints this frame.
    bool Update(float dt, bool wantsSprint);

    // Back to a full bar right now (stamina potion).
    void Refill();

    float Fraction() const;               // 0 empty .. 1 full
    bool  IsExhausted() const { return m_exhausted; }

private:
    float m_current = 0.0f;   // seconds of sprint left
    float m_max = 0.0f;
    float m_recover = 0.0f;   // seconds refilled per second of rest
    float m_regenDelay = 0.0f;   // pause before refilling starts
    bool  m_exhausted = false;
};


class Player
{
public:
    Player();

    // Puts the player at (x, y), facing +X, with full stamina.
    void Reset(float x, float y);

    // Which cat this is (sets the ability). Call before Reset.
    void SetCharacter(CharacterType type);

    // ----- Ability (E) -----
    bool           UseAbility();      // E. False if it's not ready.
    const Ability& GetAbility() const { return m_ability; }
    float          DashDirX() const { return m_dashDirX; }
    float          DashDirY() const { return m_dashDirY; }
    void Update(float dt, const Input& input, const Map& map);

    float X() const { return m_x; }
    float Y() const { return m_y; }
    float DirX() const { return m_dirX; }
    float DirY() const { return m_dirY; }
    float PlaneX() const { return m_planeX; }   // camera plane (sets the FOV)
    float PlaneY() const { return m_planeY; }

    const Stamina& GetStamina() const { return m_stamina; }
    bool           IsSprinting() const { return m_sprinting; }

    // ----- Health (whole hit points) -----
    int  Hp() const { return m_hp; }
    bool IsInvulnerable() const { return m_invuln > 0.0f; }

    // Takes 1 HP unless still invulnerable from the last hit
    // (or in the air from a ground pound). Returns true if the hit landed.
    bool TakeHit();

    // +1 HP up to the max. False if already full (nothing happens).
    bool Heal();

    // Stamina back to full right now.
    void RefillStamina() { m_stamina.Refill(); }

    // What the movement keys asked for this frame: -1, 0 or +1.
    // (Used to pick which cat picture to show in third person.)
    float ForwardInput() const { return m_forwardInput; }
    float StrafeInput() const { return m_strafeInput; }

private:
    void Rotate(float angle);
    void Move(float dx, float dy, const Map& map);

    float m_x, m_y;
    float m_dirX, m_dirY;
    float m_planeX, m_planeY;

    Stamina m_stamina;
    bool    m_sprinting;
    float   m_forwardInput, m_strafeInput;

    int     m_hp;
    float   m_invuln;   // seconds of invincibility left

    Ability m_ability;
    float   m_dashDirX, m_dashDirY;   // locked in when the dash starts (where you were looking)
};