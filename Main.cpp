#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>   // GET_X_LPARAM / GET_Y_LPARAM
#include "Config.h"
#include "Player.h"
#include "Renderer.h"
#include "Menu.h"

#ifdef _MSC_VER
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(linker, "/SUBSYSTEM:WINDOWS")
#endif

enum GameState
{
    STATE_MENU,
    STATE_PLAYING
};

// ---------------- globals ----------------
static GameState g_state = STATE_MENU;
static bool      g_running = true;
static bool      g_keys[256] = {};
static bool      g_mouseLocked = false;

static HDC    g_backDC = nullptr;   // memory DC we draw into
static DWORD* g_pixels = nullptr;   // points at the DIB section's pixels

static Player* g_player = nullptr;
static Menu* g_menu = nullptr;
static Renderer* g_renderer = nullptr;

// ---------------- mouse lock ----------------
static POINT ClientCenter(HWND hwnd)
{
    RECT rc;
    GetClientRect(hwnd, &rc);
    POINT p = { rc.right / 2, rc.bottom / 2 };
    ClientToScreen(hwnd, &p);
    return p;
}

static void LockMouse(HWND hwnd, bool lock)
{
    if (lock == g_mouseLocked) return;
    g_mouseLocked = lock;

    if (lock)
    {
        RECT rc;
        GetClientRect(hwnd, &rc);
        POINT tl = { rc.left, rc.top }, br = { rc.right, rc.bottom };
        ClientToScreen(hwnd, &tl);
        ClientToScreen(hwnd, &br);
        RECT clip = { tl.x, tl.y, br.x, br.y };
        ClipCursor(&clip);
        ShowCursor(FALSE);

        POINT c = ClientCenter(hwnd);
        SetCursorPos(c.x, c.y);
    }
    else
    {
        ClipCursor(nullptr);
        ShowCursor(TRUE);
    }
}

// ---------------- state changes ----------------
static void ClearKeys()
{
    for (int i = 0; i < 256; ++i) g_keys[i] = false;
}

static void EnterMenu(HWND hwnd)
{
    g_state = STATE_MENU;
    LockMouse(hwnd, false);
    ClearKeys();
}

static void EnterGame(HWND hwnd)
{
    g_state = STATE_PLAYING;
    ClearKeys();
    LockMouse(hwnd, true);
}

static void HandleMenuAction(HWND hwnd, MenuAction action)
{
    if (action == MENU_PLAY) EnterGame(hwnd);
    if (action == MENU_EXIT) g_running = false;
}

// Window pixels -> game pixels (the window may be any size)
static POINT ToGameCoords(HWND hwnd, LPARAM lp)
{
    RECT rc;
    GetClientRect(hwnd, &rc);
    POINT p = { 0, 0 };
    if (rc.right > 0 && rc.bottom > 0)
    {
        p.x = GET_X_LPARAM(lp) * SCREEN_W / rc.right;
        p.y = GET_Y_LPARAM(lp) * SCREEN_H / rc.bottom;
    }
    return p;
}

// ---------------- window proc ----------------
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
    case WM_CLOSE:
        g_running = false;
        return 0;

    case WM_KEYDOWN:
        if (g_state == STATE_MENU)
        {
            if (wp == VK_ESCAPE) g_running = false;
            else HandleMenuAction(hwnd, g_menu->OnKey(wp));
        }
        else
        {
            if (wp == VK_ESCAPE) EnterMenu(hwnd);
            else if (wp < 256)   g_keys[wp] = true;
        }
        return 0;

    case WM_KEYUP:
        if (wp < 256) g_keys[wp] = false;
        return 0;

    case WM_MOUSEMOVE:
        if (g_state == STATE_MENU)
        {
            POINT p = ToGameCoords(hwnd, lp);
            g_menu->OnMouseMove(p.x, p.y);
        }
        return 0;

    case WM_LBUTTONDOWN:
        if (g_state == STATE_MENU)
        {
            POINT p = ToGameCoords(hwnd, lp);
            HandleMenuAction(hwnd, g_menu->OnClick(p.x, p.y));
        }
        else
        {
            LockMouse(hwnd, true);   // re-grab after alt-tab
        }
        return 0;

    case WM_KILLFOCUS:
    case WM_ENTERSIZEMOVE:
        LockMouse(hwnd, false);
        ClearKeys();
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// ---------------- back buffer ----------------
static bool CreateBackBuffer(HWND hwnd)
{
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = SCREEN_W;
    bmi.bmiHeader.biHeight = -SCREEN_H;   // top-down rows
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    HDC windowDC = GetDC(hwnd);
    g_backDC = CreateCompatibleDC(windowDC);
    ReleaseDC(hwnd, windowDC);

    HBITMAP bmp = CreateDIBSection(g_backDC, &bmi, DIB_RGB_COLORS, (void**)&g_pixels, nullptr, 0);
    if (!bmp) return false;
    SelectObject(g_backDC, bmp);
    return true;
}

static void Present(HWND hwnd)
{
    RECT rc;
    GetClientRect(hwnd, &rc);
    HDC dc = GetDC(hwnd);
    SetStretchBltMode(dc, COLORONCOLOR);
    StretchBlt(dc, 0, 0, rc.right, rc.bottom, g_backDC, 0, 0, SCREEN_W, SCREEN_H, SRCCOPY);
    ReleaseDC(hwnd, dc);
}

// ---------------- entry point ----------------
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int)
{
    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = L"PurrcelExpress";
    RegisterClassW(&wc);

    RECT wr = { 0, 0, SCREEN_W * SCALE, SCREEN_H * SCALE };
    AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);
    HWND hwnd = CreateWindowW(wc.lpszClassName, L"Purrcel Express", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT,
        wr.right - wr.left, wr.bottom - wr.top,
        nullptr, nullptr, hInst, nullptr);
    if (!hwnd || !CreateBackBuffer(hwnd)) return 1;

    Player   player;
    Menu     menu;
    Renderer renderer;
    g_player = &player;
    g_menu = &menu;
    g_renderer = &renderer;

    LARGE_INTEGER freq, last, now;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&last);

    while (g_running)
    {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (!g_running) break;

        QueryPerformanceCounter(&now);
        float dt = (float)(now.QuadPart - last.QuadPart) / freq.QuadPart;
        last = now;
        if (dt > 0.05f) dt = 0.05f;

        GdiFlush();   // let GDI finish before we touch the pixels directly

        if (g_state == STATE_PLAYING)
        {
            int mouseDX = 0;
            if (g_mouseLocked)
            {
                POINT p, c = ClientCenter(hwnd);
                GetCursorPos(&p);
                mouseDX = p.x - c.x;
                SetCursorPos(c.x, c.y);
            }

            player.Update(dt, g_keys, mouseDX);
            renderer.DrawWorld(g_pixels, player);
        }
        else
        {
            menu.Draw(g_backDC, g_pixels);
            Sleep(1);   // menu doesn't need to spin the CPU
        }

        Present(hwnd);
    }

    LockMouse(hwnd, false);
    DestroyWindow(hwnd);
    return 0;
}