#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// =====================================================
// The core of the game: states, input, the Screen
// interface every screen implements, and the Game class
// that runs whichever screen is active.
// =====================================================


enum class GameState
{
    MainMenu,
    Run,
    Quit      // not a screen: tells the loop to exit
};


// What a screen draws into each frame.
// pixels: SCREEN_W x SCREEN_H, 0x00RRGGBB, top-down.
// dc:     GDI device context for the same pixels (for text).
struct Frame
{
    DWORD* pixels;
    HDC    dc;
    int    width;
    int    height;
};


// Keyboard + mouse, fed by window messages so nothing is read
// while the window doesn't have focus.
class Input
{
public:
    Input();

    void Attach(HWND hwnd);

    // Call from the window procedure for every message.
    void HandleMessage(UINT msg, WPARAM wp, LPARAM lp);

    // Call once at the start of every frame.
    void BeginFrame();

    // Call once at the end of every frame.
    void EndFrame();

    // ----- Keys (virtual-key codes; VK_LBUTTON / VK_RBUTTON for mouse) -----
    bool IsDown(int vk) const;      // held right now
    bool WasPressed(int vk) const;  // went down this frame (once per press)

    // ----- Mouse -----
    int MouseX() const { return m_mouseX; }    // game pixels
    int MouseY() const { return m_mouseY; }
    int MouseDX() const { return m_mouseDX; }  // only while locked
    int MouseDY() const { return m_mouseDY; }

    // Locked = cursor hidden and kept in the window, for mouse look.
    // The lock is dropped automatically while the window isn't focused
    // and comes back when it is.
    void SetMouseLockWanted(bool want);

private:
    void ApplyMouseLock();
    void ClearAll();
    POINT ClientCenterOnScreen() const;

    HWND m_hwnd;

    bool m_down[256];
    bool m_pressed[256];

    int  m_mouseX, m_mouseY;
    int  m_mouseDX, m_mouseDY;

    bool m_wantLock;
    bool m_locked;
    bool m_inSizeMove;   // user is dragging / resizing the window
};


// One screen of the game (main menu, a delivery run, ...).
class Screen
{
public:
    virtual ~Screen() {}

    // Called every time this screen becomes the active one.
    virtual void Enter() {}

    // Returns the state to switch to. Returning its own state means "stay".
    virtual GameState Update(float dt, const Input& input) = 0;

    virtual void Render(Frame& frame) = 0;

    virtual bool WantsMouseLock() const { return false; }
};


// Owns the input, the back buffer and one object per screen,
// and only talks to the active screen.
class Game
{
public:
    Game();
    ~Game();

    // Creates the back buffer and screens. False if something failed.
    bool Init(HWND hwnd);

    void Update(float dt);
    void Render();

    bool IsRunning() const { return m_state != GameState::Quit; }

    Input& GetInput() { return m_input; }

private:
    Game(const Game&);              // not copyable
    Game& operator=(const Game&);

    Screen* ScreenFor(GameState state) const;
    void    SwitchTo(GameState next);

    HWND      m_hwnd;
    Input     m_input;
    GameState m_state;

    // Back buffer: a DIB section the screens draw into, then
    // stretched to the window in one go (no flicker).
    HDC     m_backDC;
    HBITMAP m_backBitmap;
    HGDIOBJ m_oldBitmap;
    Frame   m_frame;

    // Screens (created in Init)
    Screen* m_mainMenu;
    Screen* m_run;
};
