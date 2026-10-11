#pragma once

// =====================================================
// The cats you can play, and their abilities (E).
//
// Adding a cat:
//   1. add it to CharacterType (before Count)
//   2. give it a row in GetCharacterInfo (Character.cpp)
// Adding an ability:
//   1. add it to AbilityType, with a row in GetAbilityInfo
//   2. give it its timing in Ability::Start / Ability::Update,
//      and its effect where RunScreen handles the ability
// =====================================================

enum class AbilityType
{
    None,
    GroundPound,   // jump up, slam down, knock nearby dogs back
    FurBarrage,    // a few seconds of shooting fur balls that stun dogs
    Dash,          // float up and zip forward the way you're looking
    Count
};

struct AbilityInfo
{
    const wchar_t* name;
    const wchar_t* description;   // shown on the character select screen
    float          cooldown;      // seconds from using it until it's ready again
    float          duration;      // seconds it stays active (0 = instant)
};

const AbilityInfo& GetAbilityInfo(AbilityType type);

class Texture;
// The picture for the ability slot and the character select screen.
void MakeAbilityIcon(AbilityType type, Texture& out);


enum class CharacterType
{
    CatA,
    CatB,
    CatC,
    Count
};

struct CharacterInfo
{
    const wchar_t* name;
    AbilityType    ability;
    // Pictures in the assets folder.
    const wchar_t* spriteBack;       // walking straight / standing
    const wchar_t* spriteBackSide;   // walking diagonally (faces right)
    const wchar_t* spriteSide;       // strafing, and the select screen (faces right)
    const wchar_t* spriteSideLeft;   // facing left, if it differs (nullptr = mirror spriteSide)
    const wchar_t* spriteAbility;    // pose while the ability is going (nullptr = none)
};

const CharacterInfo& GetCharacterInfo(CharacterType type);


// One cat's ability during a run: cooldown and (for the ground pound) the jump.
class Ability
{
public:
    void Reset(AbilityType type);

    // E pressed. False if it's still cooling down (or there's no ability).
    bool TryStart();

    void Update(float dt);

    AbilityType Type() const { return m_type; }
    bool  IsReady() const {
        return m_type != AbilityType::None && m_cooldown <= 0.0f &&
            !IsAirborne() && !IsActive();
    }

    // Abilities that last a while (fur barrage)
    bool  IsActive() const { return m_activeTime > 0.0f; }
    float ActiveFraction() const;      // 1 = just started .. 0 = over
    float CooldownFraction() const;   // 1 = just used .. 0 = ready

    // Off the ground (ground pound jump or mid-dash): can't be bitten.
    bool  IsAirborne() const { return m_airTime > 0.0f || m_dashTime > 0.0f; }
    float Height() const;              // how high off the ground, in wall heights

    // Ground pound
    float JumpHeight() const;          // in wall heights, 0 on the ground
    bool  JustLanded() const { return m_justLanded; }   // true for one frame: SLAM

    // Dash
    bool  IsDashing() const { return m_dashTime > 0.0f; }

    // Can't be bitten: in the air, mid-dash, or just after a dash.
    bool  IsInvulnerable() const { return IsAirborne() || m_dashGrace > 0.0f; }
    float DashTimeLeft() const { return m_dashTime; }

private:
    AbilityType m_type = AbilityType::None;
    float       m_cooldown = 0.0f;   // seconds left
    float       m_airTime = 0.0f;   // seconds left in the air
    float       m_activeTime = 0.0f;   // seconds left of an ability that lasts
    float       m_dashTime = 0.0f;   // seconds left of a dash
    float       m_dashGrace = 0.0f;   // seconds of safety left after a dash
    bool        m_justLanded = false;
};