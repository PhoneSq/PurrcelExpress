#pragma once

// =====================================================
// Shop items. Everything about an item lives here:
// what it's called, what it costs, and what it does.
//
// Adding an item:
//   1. add it to ItemType (before Count)
//   2. give it a row in GetItemInfo (Item.cpp)
//   3. say what it does in UseItem (Item.cpp) and give it a
//      picture in MakeItemIcon
// The shop and the item slot pick it up automatically.
// =====================================================

#include "Config.h"

class Player;
class Texture;
class Random;


// What items can do to the world around the player. The run screen
// implements this; Item.cpp only calls it.
class ItemWorld
{
public:
    virtual ~ItemWorld() {}
    virtual void DropBone(float x, float y) = 0;
};


enum class ItemType
{
    None,
    HealthPotion,
    StaminaPotion,
    Bone,
    Count
};

struct ItemInfo
{
    const wchar_t* name;
    const wchar_t* description;   // shown on the shop card
    int            price;
    unsigned long  color;         // message color (and potion liquid) (0xRRGGBB)
    const wchar_t* usedMessage;   // shown when it's used
    const wchar_t* failMessage;   // shown when using it would do nothing
};

const ItemInfo& GetItemInfo(ItemType type);

// Uses the item. False if it would do nothing (health potion at
// full HP), so the item isn't wasted.
bool UseItem(ItemType type, Player& player, ItemWorld& world);

// The picture for the shop card and the item slot.
void MakeItemIcon(ItemType type, Texture& out);


// =====================================================
// Perks. After every finished delivery you get PERK_CHOICES
// random ones to pick from (or $PERK_SKIP_MONEY instead).
// They stack and last until the run ends.
//
// Adding a perk: add it to PerkType, give it a row in
// GetPerkInfo, say what it does in ApplyPerk (and when it
// can't be offered in CanTakePerk), and a picture in MakePerkIcon.
// =====================================================

enum class PerkType
{
    Speed,          // + move speed
    MaxHp,          // + max HP
    MaxStamina,     // + sprint time
    GainWeight,     // max HP x2, slower
    QuickAbility,   // shorter ability cooldown, - max HP
    Count
};

struct PerkInfo
{
    const wchar_t* name;
    const wchar_t* good;     // the upside (green)
    const wchar_t* bad;      // the downside (red), "" if none
    unsigned long  color;    // bubble color (0xRRGGBB)
};

const PerkInfo& GetPerkInfo(PerkType type);

// What the cat is like this run: the base numbers plus every perk taken.
struct RunStats
{
    float moveSpeed = MOVE_SPEED;
    int   maxHp = PLAYER_MAX_HP;
    float staminaSeconds = STAMINA_MAX_SECONDS;
    float cooldownScale = 1.0f;     // ability cooldown multiplier
    int   perksTaken = 0;
};

// False if taking it now would break something (no HP left to give, too slow...).
bool CanTakePerk(PerkType type, const RunStats& stats);
void ApplyPerk(PerkType type, RunStats& stats);

// Fills 'out' with up to 'count' different perks that can be taken.
// Returns how many it found.
int RollPerks(PerkType* out, int count, Random& rng, const RunStats& stats);

// The picture inside the perk bubble.
void MakePerkIcon(PerkType type, Texture& out);