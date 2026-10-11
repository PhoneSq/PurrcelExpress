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
// Flow: MainMenu -> CharacterSelect -> DeliverySelect -> Run -> (delivered) DeliverySelect
//                                         |    ^            -> (caught)    GameOver
//                                         v    |
//                                          Shop
// =====================================================


// A full-size picture (drawn for the 1280x800 window) shrunk to half size
// once when it loads, smoothly (2x2 pixels averaged), for the 640x400 screen.
// Empty pictures are ignored, so screens can fall back to plain drawing.
class HalfPicture
{
public:
    HalfPicture() : m_pixels(nullptr), m_alpha(nullptr), m_w(0), m_h(0) {}
    ~HalfPicture() { Free(); }

    bool Load(const wchar_t* fileName);   // false if the file isn't there
    bool IsLoaded() const { return m_pixels != nullptr; }
    int  Width() const { return m_w; }
    int  Height() const { return m_h; }

    // Top-left at (x, y), with soft (alpha-blended) edges.
    void Draw(Frame& frame, int x, int y) const;

private:
    HalfPicture(const HalfPicture&);
    HalfPicture& operator=(const HalfPicture&);
    void Free();

    DWORD* m_pixels;
    unsigned char* m_alpha;
    int            m_w, m_h;
};


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

    // Hand-made menu art (assets): background + a normal / hover picture per
    // button. Without it the menu falls back to the plain green boxes.
    HalfPicture    m_background;
    HalfPicture    m_buttonArt[BUTTON_COUNT][2];   // [button][0 normal, 1 hover]
    bool           m_hasArt;

    HFONT m_titleFont;
    HFONT m_buttonFont;
};


// Pick your cat (each has its own ability).
class CharacterSelectScreen : public Screen
{
public:
    explicit CharacterSelectScreen(GameData& data);
    ~CharacterSelectScreen();

    void      Enter() override;
    GameState Update(float dt, const Input& input) override;
    void      Render(Frame& frame) override;

private:
    static const int COUNT = (int)CharacterType::Count;

    RECT CardRect(int index) const;
    int  HitTest(int x, int y) const;

    GameData& m_data;
    int       m_selected;
    int       m_lastMouseX, m_lastMouseY;
    Texture   m_portraits[COUNT];                     // side view of each cat
    Texture   m_abilityIcons[(int)AbilityType::Count];

    HFONT m_titleFont;
    HFONT m_nameFont;
    HFONT m_textFont;
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
    void DrawSpeedLines(Frame& frame);   // dash
    void DrawHud(Frame& frame, const RECT& minimap);
    void DrawFinished(Frame& frame);

    GameData& m_data;

    Map         m_map;
    Player      m_player;
    DeliveryRun m_run;
    Renderer    m_renderer;

    Enemy m_enemies[MAX_ENEMIES];
    int   m_enemyCount;

    void LoadCatPictures();     // for m_data.character (only when it changed)
    void OnAbilityLanded();     // the slam of a ground pound
    void ShootFurBall();
    void UpdateFurBalls(float dt);

    // Third person: the cat seen from behind (V toggles).
    bool    m_thirdPerson;
    int     m_loadedCharacter;  // which cat's pictures are loaded (-1 none)
    Texture m_shadow;           // under the cat while it's in the air
    Texture m_abilityIcons[(int)AbilityType::Count];
    float   m_shake;            // screen shake seconds left

    // Walk animation (cat + one per dog), driven by how far each moved.
    WalkCycle m_catWalk;
    WalkCycle m_dogWalk[MAX_ENEMIES];
    float     m_lastCatX, m_lastCatY;
    int       m_lastStep;       // walk cycle step count last frame (paw sounds)
    float     m_lastDogX[MAX_ENEMIES], m_lastDogY[MAX_ENEMIES];

    // Fur barrage: fur balls in flight
    struct FurBall { float x, y, dx, dy, travelled; bool alive; };
    static const int MAX_FUR_BALLS = 24;
    FurBall m_furBalls[MAX_FUR_BALLS];
    float   m_fireTimer;        // seconds until the next shot is allowed
    Texture m_furBall;
    Texture m_dizzy[3];         // spinning stars over stunned dogs (3 frames)
    Texture m_catBack;       // walking straight / standing
    Texture m_catBackSide;   // walking diagonally (faces right; flipped for left)
    Texture m_catSide;       // strafing (faces right; flipped for left)
    Texture m_catSideLeft;   // strafing left, for cats whose sides differ (else empty)
    Texture m_catAbility;    // pose while the ability is going (else empty)
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