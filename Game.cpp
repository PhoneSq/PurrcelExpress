#include <windowsx.h>   // GET_X_LPARAM / GET_Y_LPARAM
#include "Game.h"
#include "Screen.h"
#include "Config.h"

// =====================================================
// INPUT
// =====================================================

Input::Input()
    : m_hwnd(nullptr),
    m_mouseX(0), m_mouseY(0),
    m_mouseDX(0), m_mouseDY(0),
    m_wantLock(false), m_locked(false), m_inSizeMove(false)
{
    ClearAll();
}

void Input::Attach(HWND hwnd)
{
    m_hwnd = hwnd;
}

void Input::ClearAll()
{
    for (int i = 0; i < 256; ++i)
    {
        m_down[i] = false;
        m_pressed[i] = false;
    }
    m_mouseDX = m_mouseDY = 0;
}

void Input::HandleMessage(UINT msg, WPARAM wp, LPARAM lp)
{
    if (!m_hwnd) return;   // not attached to the window yet

    switch (msg)
    {
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        if (wp < 256)
        {
            bool isRepeat = (lp & (1 << 30)) != 0;   // key was already down
            if (!isRepeat) m_pressed[wp] = true;
            m_down[wp] = true;
        }
        break;

    case WM_KEYUP:
    case WM_SYSKEYUP:
        if (wp < 256) m_down[wp] = false;
        break;

    case WM_LBUTTONDOWN: m_down[VK_LBUTTON] = true;  m_pressed[VK_LBUTTON] = true; break;
    case WM_LBUTTONUP:   m_down[VK_LBUTTON] = false; break;
    case WM_RBUTTONDOWN: m_down[VK_RBUTTON] = true;  m_pressed[VK_RBUTTON] = true; break;
    case WM_RBUTTONUP:   m_down[VK_RBUTTON] = false; break;

    case WM_MOUSEMOVE:
    {
        // Window pixels -> game pixels (the window can be any size).
        RECT rc;
        GetClientRect(m_hwnd, &rc);
        if (rc.right > 0 && rc.bottom > 0)
        {
            m_mouseX = GET_X_LPARAM(lp) * SCREEN_W / rc.right;
            m_mouseY = GET_Y_LPARAM(lp) * SCREEN_H / rc.bottom;
        }
        break;
    }

    case WM_KILLFOCUS:
        ClearAll();
        ApplyMouseLock();
        break;

    case WM_ENTERSIZEMOVE:
        m_inSizeMove = true;
        ApplyMouseLock();
        break;

    case WM_EXITSIZEMOVE:
        m_inSizeMove = false;
        break;
    }
}

POINT Input::ClientCenterOnScreen() const
{
    RECT rc;
    GetClientRect(m_hwnd, &rc);
    POINT p = { rc.right / 2, rc.bottom / 2 };
    ClientToScreen(m_hwnd, &p);
    return p;
}

void Input::SetMouseLockWanted(bool want)
{
    m_wantLock = want;
    ApplyMouseLock();
}

void Input::ApplyMouseLock()
{
    bool shouldLock = m_wantLock && !m_inSizeMove && GetForegroundWindow() == m_hwnd;
    if (shouldLock == m_locked) return;
    m_locked = shouldLock;

    if (m_locked)
    {
        RECT rc;
        GetClientRect(m_hwnd, &rc);
        POINT tl = { rc.left, rc.top }, br = { rc.right, rc.bottom };
        ClientToScreen(m_hwnd, &tl);
        ClientToScreen(m_hwnd, &br);
        RECT clip = { tl.x, tl.y, br.x, br.y };
        ClipCursor(&clip);
        ShowCursor(FALSE);

        // Start centered so the first frame has zero mouse movement.
        POINT c = ClientCenterOnScreen();
        SetCursorPos(c.x, c.y);
    }
    else
    {
        ClipCursor(nullptr);
        ShowCursor(TRUE);
    }
}

void Input::BeginFrame()
{
    ApplyMouseLock();   // re-grabs the mouse after alt-tab

    m_mouseDX = m_mouseDY = 0;
    if (m_locked)
    {
        POINT p, c = ClientCenterOnScreen();
        GetCursorPos(&p);
        m_mouseDX = p.x - c.x;
        m_mouseDY = p.y - c.y;
        SetCursorPos(c.x, c.y);
    }
}

void Input::EndFrame()
{
    for (int i = 0; i < 256; ++i) m_pressed[i] = false;
}

bool Input::IsDown(int vk) const
{
    return vk >= 0 && vk < 256 && m_down[vk];
}

bool Input::WasPressed(int vk) const
{
    return vk >= 0 && vk < 256 && m_pressed[vk];
}


// =====================================================
// GAME
// =====================================================

Game::Game()
    : m_hwnd(nullptr),
    m_state(GameState::MainMenu),
    m_backDC(nullptr), m_backBitmap(nullptr), m_oldBitmap(nullptr),
    m_mainMenu(nullptr), m_run(nullptr)
{
    m_frame.pixels = nullptr;
    m_frame.dc = nullptr;
    m_frame.width = SCREEN_W;
    m_frame.height = SCREEN_H;
}

Game::~Game()
{
    m_input.SetMouseLockWanted(false);   // give the cursor back

    delete m_mainMenu;
    delete m_run;

    if (m_backDC)
    {
        SelectObject(m_backDC, m_oldBitmap);
        DeleteObject(m_backBitmap);
        DeleteDC(m_backDC);
    }
}

bool Game::Init(HWND hwnd)
{
    m_hwnd = hwnd;
    m_input.Attach(hwnd);

    // ----- back buffer -----
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = SCREEN_W;
    bmi.bmiHeader.biHeight = -SCREEN_H;   // top-down rows
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    HDC windowDC = GetDC(hwnd);
    m_backDC = CreateCompatibleDC(windowDC);
    ReleaseDC(hwnd, windowDC);
    if (!m_backDC) return false;

    void* bits = nullptr;
    m_backBitmap = CreateDIBSection(m_backDC, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!m_backBitmap) return false;
    m_oldBitmap = SelectObject(m_backDC, m_backBitmap);

    m_frame.pixels = (DWORD*)bits;
    m_frame.dc = m_backDC;

    // ----- screens -----
    m_mainMenu = new MainMenuScreen();
    m_run = new RunScreen();

    SwitchTo(GameState::MainMenu);
    return true;
}

Screen* Game::ScreenFor(GameState state) const
{
    switch (state)
    {
    case GameState::MainMenu: return m_mainMenu;
    case GameState::Run:      return m_run;
    default:                  return nullptr;
    }
}

void Game::SwitchTo(GameState next)
{
    m_state = next;

    Screen* screen = ScreenFor(next);
    if (!screen) return;   // Quit

    screen->Enter();
    m_input.SetMouseLockWanted(screen->WantsMouseLock());
}

void Game::Update(float dt)
{
    Screen* screen = ScreenFor(m_state);
    if (!screen) return;

    m_input.BeginFrame();

    GameState next = screen->Update(dt, m_input);
    if (next != m_state) SwitchTo(next);

    m_input.EndFrame();
}

void Game::Render()
{
    Screen* screen = ScreenFor(m_state);
    if (!screen) return;

    GdiFlush();   // GDI must be done with the pixels before we write them directly
    screen->Render(m_frame);

    RECT rc;
    GetClientRect(m_hwnd, &rc);
    HDC dc = GetDC(m_hwnd);
    SetStretchBltMode(dc, COLORONCOLOR);   // nearest-neighbour: crisp pixels
    StretchBlt(dc, 0, 0, rc.right, rc.bottom, m_backDC, 0, 0, SCREEN_W, SCREEN_H, SRCCOPY);
    ReleaseDC(m_hwnd, dc);
}