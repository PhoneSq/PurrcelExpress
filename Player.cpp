#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <math.h>
#include "Player.h"
#include "World.h"
#include "Game.h"
#include "Config.h"

// =====================================================
// STAMINA
// =====================================================

void Stamina::Reset(float maxSeconds)
{
    m_max = maxSeconds;
    m_recover = STAMINA_RECOVER_RATE;
    m_current = m_max;
    m_regenDelay = 0.0f;
    m_exhausted = false;
}

bool Stamina::Update(float dt, bool wantsSprint)
{
    bool sprinting = wantsSprint && !m_exhausted && m_current > 0.0f;

    if (sprinting)
    {
        m_current -= dt;
        m_regenDelay = STAMINA_RECOVER_DELAY;
        if (m_current <= 0.0f)
        {
            m_current = 0.0f;
            m_exhausted = true;
        }
    }
    else
    {
        if (m_regenDelay > 0.0f) m_regenDelay -= dt;
        else                     m_current += m_recover * dt;

        if (m_current > m_max) m_current = m_max;

        // Out of breath: sprint stays locked until the bar is partly back.
        if (m_exhausted && m_current >= m_max * STAMINA_RESTART_AT)
            m_exhausted = false;
    }
    return sprinting;
}

void Stamina::Refill()
{
    m_current = m_max;
    m_regenDelay = 0.0f;
    m_exhausted = false;
}

float Stamina::Fraction() const
{
    if (m_max <= 0.0f) return 0.0f;
    float f = m_current / m_max;
    return f < 0.0f ? 0.0f : (f > 1.0f ? 1.0f : f);
}


// =====================================================
// PLAYER
// =====================================================

Player::Player()
    : m_maxHp(PLAYER_MAX_HP), m_speed(MOVE_SPEED), m_staminaSeconds(STAMINA_MAX_SECONDS)
{
    Reset(1.5f, 1.5f);
}

void Player::SetStats(const RunStats& stats)
{
    m_maxHp = stats.maxHp < 1 ? 1 : stats.maxHp;
    m_speed = stats.moveSpeed;
    m_staminaSeconds = stats.staminaSeconds;
    m_ability.SetCooldownScale(stats.cooldownScale);
}

void Player::Reset(float x, float y)
{
    m_x = x;         m_y = y;
    m_dirX = 1.0f;   m_dirY = 0.0f;
    m_planeX = 0.0f; m_planeY = 0.66f;
    m_stamina.Reset(m_staminaSeconds);
    m_sprinting = false;
    m_forwardInput = m_strafeInput = 0.0f;
    m_hp = m_maxHp;
    m_invuln = 0.0f;
    m_ability.Reset(m_ability.Type());   // same ability, fresh cooldown
    m_dashDirX = 1.0f; m_dashDirY = 0.0f;
}

bool Player::UseAbility()
{
    if (!m_ability.TryStart()) return false;
    if (m_ability.Type() == AbilityType::Dash) { m_dashDirX = m_dirX; m_dashDirY = m_dirY; }
    return true;
}

void Player::SetCharacter(CharacterType type)
{
    m_ability.Reset(GetCharacterInfo(type).ability);
}

bool Player::TakeHit()
{
    if (m_invuln > 0.0f || m_hp <= 0 || m_ability.IsInvulnerable()) return false;
    --m_hp;
    m_invuln = HIT_INVULN_TIME;
    return true;
}

bool Player::Heal()
{
    if (m_hp >= m_maxHp) return false;
    ++m_hp;
    return true;
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
    if (m_invuln > 0.0f) m_invuln -= dt;
    float dashLeft = m_ability.DashTimeLeft();   // before this frame's tick
    m_ability.Update(dt);

    // look
    float turn = input.MouseDX() * MOUSE_SENS;
    if (input.IsDown(VK_LEFT))  turn -= TURN_SPEED * dt;
    if (input.IsDown(VK_RIGHT)) turn += TURN_SPEED * dt;
    if (turn != 0.0f) Rotate(turn);

    // Dashing: zip along the locked direction, ignore the movement keys.
    if (dashLeft > 0.0f)
    {
        float step = (dt < dashLeft ? dt : dashLeft) * (DASH_DISTANCE / DASH_TIME);
        // Small pieces, so a slow frame can't carry you through a wall.
        int pieces = (int)(step / 0.15f) + 1;
        for (int i = 0; i < pieces; ++i)
            Move(m_dashDirX * step / pieces, m_dashDirY * step / pieces, map);
        m_forwardInput = m_strafeInput = 0.0f;
        m_sprinting = m_stamina.Update(dt, false);
        return;
    }

    // which way we want to go
    float forward = 0.0f, strafe = 0.0f;
    if (input.IsDown('W') || input.IsDown(VK_UP))   forward += 1.0f;
    if (input.IsDown('S') || input.IsDown(VK_DOWN)) forward -= 1.0f;
    if (input.IsDown('D'))                          strafe += 1.0f;
    if (input.IsDown('A'))                          strafe -= 1.0f;
    m_forwardInput = forward;
    m_strafeInput = strafe;

    float mx = m_dirX * forward - m_dirY * strafe;
    float my = m_dirY * forward + m_dirX * strafe;
    float len = sqrtf(mx * mx + my * my);
    bool moving = len > 0.0f;

    // sprint: hold SHIFT while moving
    m_sprinting = m_stamina.Update(dt, moving && input.IsDown(VK_SHIFT));

    if (moving)
    {
        // Normalized, so diagonal movement isn't faster than straight.
        float speed = m_speed * (m_sprinting ? SPRINT_MULTIPLIER : 1.0f) * dt;
        Move(mx / len * speed, my / len * speed, map);
    }
}