#pragma once
#include "Game.h"
#include "World.h"
#include "Player.h"
#include "Item.h"
#include "Enemy.h"
#include "Renderer.h"
#include "Config.h"

// =====================================================
// Every screen of the game. New screens go here too.
// Flow: MainMenu -> DeliverySelect -> Run -> (delivered) DeliverySelect
//                       |    ^            -> (caught)    GameOver
//                       v    |
//                        Shop
// =====================================================


// PLAY / EXIT
class MainMenuScreen : public Screen
{
public:
    MainMenuScreen();
    ~MainMenuScreen();

    void      Enter() override;
    GameState Update(float dt, const Input& input) override;
    void      Render(Frame& frame) override;

private:
    static const int BUTTON_COUNT = 2;

    int HitTest(int x, int y) const;

    RECT           m_buttons[BUTTON_COUNT];
    const wchar_t* m_labels[BUTTON_COUNT];
    GameState      m_targets[BUTTON_COUNT];
    int            m_selected;
    int            m_lastMouseX, m_lastMouseY;

    HFONT m_titleFont;
    HFONT m_buttonFont;
};


// Pick one of DELIVERY_CHOICES random deliveries.
class DeliverySelectScreen : public Screen
{
public:
    explicit DeliverySelectScreen(GameData& data);
    ~DeliverySelectScreen();

    void      Enter() override;   // rolls a fresh list
    GameState Update(float dt, const Input& input) override;
    void      Render(Frame& frame) override;

private:
    RECT RowRect(int index) const;
    int  HitTest(int x, int y) const;   // row index, SHOP_BUTTON or -1

    static const int SHOP_BUTTON = 100;
    RECT m_shopButton;

    GameData& m_data;
    Contract  m_contracts[DELIVERY_CHOICES];
    int       m_selected;
    int       m_lastMouseX, m_lastMouseY;

    HFONT m_titleFont;
    HFONT m_nameFont;
    HFONT m_textFont;
};


// Buy one item to carry into deliveries.
class ShopScreen : public Screen
{
public:
    explicit ShopScreen(GameData& data);
    ~ShopScreen();

    void      Enter() override;
    GameState Update(float dt, const Input& input) override;
    void      Render(Frame& frame) override;

private:
    // Every item except ItemType::None gets a card.
    static const int ITEM_COUNT = (int)ItemType::Count - 1;
    static const int BACK_BUTTON = 100;

    RECT     CardRect(int index) const;
    ItemType CardItem(int index) const { return (ItemType)(index + 1); }
    int      HitTest(int x, int y) const;   // card index, BACK_BUTTON or -1
    void     TryBuy(int index);

    GameData& m_data;
    int       m_selected;
    int       m_lastMouseX, m_lastMouseY;
    RECT      m_backButton;
    Texture   m_icons[(int)ItemType::Count];

    // "BOUGHT!" / "NOT ENOUGH MONEY" under the cards
    const wchar_t* m_feedback;
    COLORREF       m_feedbackColor;
    float          m_feedbackTimer;

    HFONT m_titleFont;
    HFONT m_nameFont;
    HFONT m_textFont;
};


// One delivery: visit every stop, then get back to the start.
class RunScreen : public Screen, public ItemWorld
{
public:
    explicit RunScreen(GameData& data);
    ~RunScreen();

    void      Enter() override;   // builds a fresh level for data.chosen
    GameState Update(float dt, const Input& input) override;
    void      Render(Frame& frame) override;
    bool      WantsMouseLock() const override { return true; }

    // ItemWorld: what items can do here.
    void DropBone(float x, float y) override;

private:
    void SpawnEnemies();
    void CheckBites();          // dogs touching you take hit points
    void UseHeldItem();         // Q: drink the potion you're carrying
    void ShowMessage(const wchar_t* text, COLORREF color);
    void DrawActors(Frame& frame, const Camera& cam, float cameraDistance);
    void DrawSpottedAlert(Frame& frame, float cameraDistance);
    void DrawHud(Frame& frame, const RECT& minimap);
    void DrawFinished(Frame& frame);

    GameData& m_data;

    Map         m_map;
    Player      m_player;
    DeliveryRun m_run;
    Renderer    m_renderer;

    Enemy m_enemies[MAX_ENEMIES];
    int   m_enemyCount;

    // Third person: the cat seen from behind (V toggles).
    bool    m_thirdPerson;
    Texture m_catBack;       // walking straight / standing
    Texture m_catBackSide;   // walking diagonally (faces right; flipped for left)
    Texture m_catSide;       // strafing (faces right; flipped for left)
    Texture m_package;       // left on the doorstep once delivered (Pack.png)

    // Dog pictures, one per type (Blue, Yellow, Purple). A type without
    // a picture is drawn as the placeholder blob.
    Texture m_dogs[3];

    Texture m_itemIcons[(int)ItemType::Count];   // for the item slot
    Texture m_alertIcon;                         // red "!" when a dog spots you
    float   m_alertTimer;                        // seconds the "!" stays up
    Texture m_heart;                             // over dogs going for a bone

    // A dropped bone (one at a time)
    bool    m_boneActive;
    float   m_boneX, m_boneY;
    float   m_boneTimer;

    float m_time;       // seconds, drives pulsing and blinking
    float m_hitFlash;   // red screen flash after a bite (seconds left)

    // Short message in the middle-top ("EXPRESS DELIVERED!" ...)
    wchar_t  m_message[64];
    COLORREF m_messageColor;
    float    m_messageTimer;

    HFONT m_hudFont;
    HFONT m_timerFont;
    HFONT m_bigFont;
};


// "YOU GOT CAUGHT" - back to the deliveries or the menu.
class GameOverScreen : public Screen
{
public:
    GameOverScreen();
    ~GameOverScreen();

    void      Enter() override;
    GameState Update(float dt, const Input& input) override;
    void      Render(Frame& frame) override;

private:
    float m_timer;   // ignores input for a moment so mashed keys don't skip it

    HFONT m_titleFont;
    HFONT m_textFont;
};