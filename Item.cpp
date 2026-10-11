#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "Item.h"
#include "Player.h"
#include "Renderer.h"    // Texture
#include "AItexture.h"   // the pictures
#include "Config.h"

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