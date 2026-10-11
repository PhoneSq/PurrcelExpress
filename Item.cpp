#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "Item.h"
#include "Player.h"
#include "Renderer.h"    // Texture
#include "AItexture.h"   // the pictures
#include "Config.h"
#include "World.h"      // Random

const ItemInfo& GetItemInfo(ItemType type)
{
    static const ItemInfo INFO[(int)ItemType::Count] =
    {
        // name              description                          price                 color     used                  fail
        { L"Nothing",        L"",                                 0,                    0x000000, L"",                  L"" },
        { L"Health Potion",  L"Heals 1 HP.",                      HEALTH_POTION_PRICE,  0xE04848, L"+1 HP",             L"HP ALREADY FULL" },
        { L"Stamina Potion", L"Instantly refills your stamina.",  STAMINA_POTION_PRICE, 0x6CC24A, L"STAMINA REFILLED!", L"" },
        { L"Bone",           L"Drop it to lure nearby dogs away for 3 seconds.",
                                                                  BONE_PRICE,           0xF4EAD5, L"BONE DROPPED!",     L"" },
    };
    int i = (int)type;
    if (i < 0 || i >= (int)ItemType::Count) i = 0;
    return INFO[i];
}

bool UseItem(ItemType type, Player& player, ItemWorld& world)
{
    switch (type)
    {
    case ItemType::HealthPotion:  return player.Heal();
    case ItemType::StaminaPotion: player.RefillStamina(); return true;
    case ItemType::Bone:          world.DropBone(player.X(), player.Y()); return true;
    default:                      return false;
    }
}

void MakeItemIcon(ItemType type, Texture& out)
{
    switch (type)
    {
    case ItemType::Bone:
        if (!out.Load(L"Bone.png")) MakeBone(out);   // your art; the drawn one if it's missing
        break;
    case ItemType::None: break;
    default:             MakePotion(out, GetItemInfo(type).color); break;
    }
}


// =====================================================
// PERKS
// =====================================================

const PerkInfo& GetPerkInfo(PerkType type)
{
    static const PerkInfo INFO[(int)PerkType::Count] =
    {
        // name            good                       bad                        bubble color
        { L"Zoomies",      L"+0.1 move speed",        L"",                       0x5AA8E8 },
        { L"Extra Life",   L"+1 max HP",              L"",                       0xE05A5A },
        { L"Big Lungs",    L"+1s of sprint",          L"",                       0x6CC24A },
        { L"Chonky",       L"Max HP x2",              L"-0.2 move speed",        0xF0A040 },
        { L"Quick Paws",   L"Ability cooldown -20%",  L"-1 max HP",              0x9A82D6 },
    };
    int i = (int)type;
    if (i < 0 || i >= (int)PerkType::Count) i = 0;
    return INFO[i];
}

bool CanTakePerk(PerkType type, const RunStats& s)
{
    switch (type)
    {
    case PerkType::MaxHp:        return s.maxHp + PERK_HP_UP <= PERK_MAX_HP;
    case PerkType::GainWeight:   return s.maxHp * 2 <= PERK_MAX_HP && s.moveSpeed - PERK_WEIGHT_SPEED >= PERK_MIN_SPEED - 0.001f;
    case PerkType::QuickAbility: return s.maxHp - PERK_COOLDOWN_HP_COST >= 1;
    default:                     return true;
    }
}

void ApplyPerk(PerkType type, RunStats& s)
{
    switch (type)
    {
    case PerkType::Speed:        s.moveSpeed += PERK_SPEED_UP; break;
    case PerkType::MaxHp:        s.maxHp += PERK_HP_UP; break;
    case PerkType::MaxStamina:   s.staminaSeconds += PERK_STAMINA_UP; break;
    case PerkType::GainWeight:   s.maxHp *= 2; s.moveSpeed -= PERK_WEIGHT_SPEED; break;
    case PerkType::QuickAbility: s.cooldownScale *= PERK_COOLDOWN_SCALE; s.maxHp -= PERK_COOLDOWN_HP_COST; break;
    default: return;
    }
    ++s.perksTaken;
}

int RollPerks(PerkType* out, int count, Random& rng, const RunStats& stats)
{
    // Shuffle the takeable ones and deal the first few.
    PerkType pool[(int)PerkType::Count];
    int n = 0;
    for (int i = 0; i < (int)PerkType::Count; ++i)
        if (CanTakePerk((PerkType)i, stats)) pool[n++] = (PerkType)i;
    for (int i = n - 1; i > 0; --i)
    {
        int j = rng.Range(0, i);
        PerkType t = pool[i]; pool[i] = pool[j]; pool[j] = t;
    }
    if (count > n) count = n;
    for (int i = 0; i < count; ++i) out[i] = pool[i];
    return count;
}

void MakePerkIcon(PerkType type, Texture& out)
{
    switch (type)
    {
    case PerkType::Speed:        MakeSpeedIcon(out);   break;
    case PerkType::MaxHp:        MakeHeart(out);       break;
    case PerkType::MaxStamina:   MakeStaminaIcon(out); break;
    case PerkType::GainWeight:   MakeFishIcon(out);    break;
    case PerkType::QuickAbility: MakeClockIcon(out);   break;
    default: break;
    }
}