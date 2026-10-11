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

class Player;
class Texture;


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