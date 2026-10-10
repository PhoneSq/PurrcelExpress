#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "Config.h"
#include "Game.h"

#ifdef _MSC_VER
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(linker, "/SUBSYSTEM:WINDOWS")
#endif

// Just the window and the loop. Everything else lives in Game.

static Game* g_game = nullptr;

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (g_game) g_game->GetInput().HandleMessage(msg, wp, lp);

    if (msg == WM_CLOSE)
    {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

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
    if (!hwnd) return 1;

    Game game;
    g_game = &game;
    if (!game.Init(hwnd)) return 1;

    LARGE_INTEGER freq, last, now;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&last);

    bool running = true;
    while (running)
    {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT) running = false;
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (!running) break;

        QueryPerformanceCounter(&now);
        float dt = (float)(now.QuadPart - last.QuadPart) / freq.QuadPart;
        last = now;
        if (dt > 0.05f) dt = 0.05f;   // don't jump forward after a hitch

        game.Update(dt);
        if (!game.IsRunning()) break;

        game.Render();
    }

    g_game = nullptr;
    DestroyWindow(hwnd);
    return 0;
}