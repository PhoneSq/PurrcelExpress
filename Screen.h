#pragma once
#include "Game.h"
#include "World.h"
#include "Player.h"
#include "Renderer.h"
#include "Enemy.h"
#include "Config.h"

// =====================================================
// Every screen of the game. New screens go here too.
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

    int  HitTest(int x, int y) const;
    void FillBox(Frame& frame, const RECT& r, DWORD color);
    void OutlineBox(Frame& frame, const RECT& r, DWORD color);

    RECT           m_buttons[BUTTON_COUNT];
    const wchar_t* m_labels[BUTTON_COUNT];
    GameState      m_targets[BUTTON_COUNT];
    int            m_selected;
    int            m_lastMouseX, m_lastMouseY;

    HFONT m_titleFont;
    HFONT m_buttonFont;
};


// One delivery: walk to the drop-off, then back to the start.
class RunScreen : public Screen
{
public:
    RunScreen();
    ~RunScreen();

    void      Enter() override;
    GameState Update(float dt, const Input& input) override;
    void      Render(Frame& frame) override;
    bool      WantsMouseLock() const override { return true; }

private:
    void StartNewRun();
    void SpawnEnemies();
    bool PlayerCaught() const;
    void DrawEnemies(Frame& frame);
    void DrawHud(Frame& frame);

    Map      m_map;
    Player   m_player;
    Delivery m_delivery;
    Renderer m_renderer;

    Enemy m_enemies[MAX_ENEMIES];
    int   m_enemyCount;

    float m_time;        // seconds, drives the marker pulse
    bool  m_needsReset;  // start fresh next time we enter (after dying)

    HFONT m_hudFont;
    HFONT m_bigFont;
};


// "YOU GOT CAUGHT" - retry or back to the menu.
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