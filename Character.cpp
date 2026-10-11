#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <math.h>
#include "Character.h"
#include "Renderer.h"    // Texture
#include "AItexture.h"   // the pictures
#include "Config.h"

const AbilityInfo& GetAbilityInfo(AbilityType type)
{
    static const AbilityInfo INFO[(int)AbilityType::Count] =
    {
        // name             description                                                   cooldown               duration          badge color
        { L"None",          L"",                                                          0.0f,                  0.0f,             0xC9BCA8 },
        { L"Ground Pound",  L"Jump up and slam down, knocking nearby dogs back.",         GROUND_POUND_COOLDOWN, 0.0f,             0xF08A4C },   // orange
        { L"Fur Barrage",   L"For 5 seconds, click or SPACE to shoot fur balls. "
                            L"Hit dogs see stars for 2 seconds.",                         FUR_BARRAGE_COOLDOWN,  FUR_BARRAGE_TIME, 0x9A82D6 },   // lavender
        { L"Dash",          L"Float up and zip forward the way you're looking. "
                            L"Dogs can't bite you mid-dash.",                             DASH_COOLDOWN,         0.0f,             0x3DB8C6 },   // teal
    };
    int i = (int)type;
    if (i < 0 || i >= (int)AbilityType::Count) i = 0;
    return INFO[i];
}

void MakeAbilityIcon(AbilityType type, Texture& out)
{
    switch (type)
    {
    case AbilityType::GroundPound: MakeDumbbell(out); break;
    case AbilityType::FurBarrage:  if (!out.Load(L"Fur.png")) MakeFurBall(out); break;   // reuses the fur ball for now
    case AbilityType::Dash:        if (!out.Load(L"Dash.png")) MakeDumbbell(out); break;  // the dash pose for now
    default: break;
    }
}

const CharacterInfo& GetCharacterInfo(CharacterType type)
{
    static const CharacterInfo INFO[(int)CharacterType::Count] =
    {
        // name          ability                   back               back-side              side (right)        side (left)          ability pose
        { L"Fat Cat",    AbilityType::GroundPound, L"PlayerBack.png",  L"PlayerBackSide.png",  L"PlayerSide.png",   nullptr,             nullptr     },
        { L"Furry Cat",  AbilityType::FurBarrage,  L"PlayerBack1.png", L"PlayerBackSide1.png", L"PlayerSide1.png",  nullptr,             nullptr     },
        // Blind in the left eye: separate pictures for each side (g = good eye, b = blind eye).
        { L"Blind Cat",  AbilityType::Dash,        L"PlayerBack2.png", L"PlayerBackSide2.png", L"PlayerSideg2.png", L"PlayerSideb2.png", L"Dash.png" },
    };
    int i = (int)type;
    if (i < 0 || i >= (int)CharacterType::Count) i = 0;
    return INFO[i];
}


// =====================================================
// Ability
// =====================================================

void Ability::Reset(AbilityType type)
{
    m_type = type;
    m_cooldown = 0.0f;
    m_airTime = 0.0f;
    m_activeTime = 0.0f;
    m_dashTime = 0.0f;
    m_dashGrace = 0.0f;
    m_justLanded = false;
}

bool Ability::TryStart()
{
    if (!IsReady()) return false;

    switch (m_type)
    {
    case AbilityType::GroundPound:
        m_airTime = GROUND_POUND_AIR_TIME;
        break;
    case AbilityType::FurBarrage:
        m_activeTime = GetAbilityInfo(m_type).duration;
        break;
    case AbilityType::Dash:
        m_dashTime = DASH_TIME;
        break;
    default:
        return false;
    }
    m_cooldown = GetAbilityInfo(m_type).cooldown;
    return true;
}

void Ability::Update(float dt)
{
    m_justLanded = false;
    if (m_cooldown > 0.0f) m_cooldown -= dt;
    if (m_activeTime > 0.0f) m_activeTime -= dt;
    if (m_dashGrace > 0.0f) m_dashGrace -= dt;
    if (m_dashTime > 0.0f)
    {
        m_dashTime -= dt;
        if (m_dashTime <= 0.0f) { m_dashTime = 0.0f; m_dashGrace = DASH_GRACE; }
    }

    if (m_airTime > 0.0f)
    {
        m_airTime -= dt;
        if (m_airTime <= 0.0f)
        {
            m_airTime = 0.0f;
            m_justLanded = true;
        }
    }
}

float Ability::CooldownFraction() const
{
    float total = GetAbilityInfo(m_type).cooldown;
    if (total <= 0.0f || m_cooldown <= 0.0f) return 0.0f;
    return m_cooldown / total;
}

float Ability::ActiveFraction() const
{
    float total = GetAbilityInfo(m_type).duration;
    if (total <= 0.0f || m_activeTime <= 0.0f) return 0.0f;
    return m_activeTime / total;
}

float Ability::Height() const
{
    float h = JumpHeight();
    if (m_dashTime > 0.0f)
    {
        float t = 1.0f - m_dashTime / DASH_TIME;      // 0 .. 1 through the dash
        h += sinf(t * 3.14159265f) * DASH_LIFT;       // a little float up and back down
    }
    return h;
}

float Ability::JumpHeight() const
{
    if (m_airTime <= 0.0f) return 0.0f;

    // Slow float up, fast slam down: the first 65% of the jump goes up
    // (easing out), the rest drops (speeding up).
    float t = 1.0f - m_airTime / GROUND_POUND_AIR_TIME;   // 0 .. 1
    const float apex = 0.65f;
    float h;
    if (t < apex) { float a = t / apex;                 h = 1.0f - (1.0f - a) * (1.0f - a); }
    else { float a = (t - apex) / (1.0f - apex); h = 1.0f - a * a; }
    return h * GROUND_POUND_HEIGHT;
}