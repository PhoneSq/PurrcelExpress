#pragma once
#include "Game.h"
#include "World.h"
#include "Player.h"
#include "Renderer.h"

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


// Walking around the 3D world.
class RunScreen : public Screen
{
public:
    GameState Update(float dt, const Input& input) override;
    void      Render(Frame& frame) override;
    bool      WantsMouseLock() const override { return true; }

private:
    Map      m_map;
    Player   m_player;
    Renderer m_renderer;
};