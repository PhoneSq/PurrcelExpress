#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <math.h>   // sinf/cosf/fabsf (Win32 has no math functions)

#ifdef _MSC_VER
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(linker, "/SUBSYSTEM:WINDOWS")   // use WinMain, not main()
#endif

// ---------------- settings ----------------
const int   SCREEN_W = 640;
const int   SCREEN_H = 400;
const int   SCALE = 2;
const float MOVE_SPEED = 3.0f;   // tiles per second
const float TURN_SPEED = 2.5f;   // radians per second

// ---------------- map ----------------
// '#' = wall, '.' = empty
const int MAP_W = 16;
const int MAP_H = 16;
const char* MAP[MAP_H] =
{
    "################",
    "#..............#",
    "#..##......##..#",
    "#..#........#..#",
    "#..............#",
    "#.....####.....#",
    "#.....#..#.....#",
    "#.....#..#.....#",
    "#..............#",
    "#..#........#..#",
    "#..##......##..#",
    "#..............#",
    "#......##......#",
    "#..............#",
    "#..............#",
    "################",
};

bool IsWall(int x, int y)
{
    if (x < 0 || y < 0 || x >= MAP_W || y >= MAP_H) return true;
    return MAP[y][x] == '#';
}

// ---------------- player ----------------
struct Player
{
    float x = 1.5f, y = 1.5f;      // position
    float dirX = 1.0f, dirY = 0.0f; // facing direction
    float planeX = 0.0f, planeY = 0.66f; // camera plane (FOV)

    void Rotate(float a)
    {
        float c = cosf(a), s = sinf(a);
        float old = dirX;
        dirX = dirX * c - dirY * s;
        dirY = old * s + dirY * c;
        old = planeX;
        planeX = planeX * c - planeY * s;
        planeY = old * s + planeY * c;
    }

    void Move(float dx, float dy)
    {
        // check X and Y separately so you slide along walls
        if (!IsWall((int)(x + dx), (int)y)) x += dx;
        if (!IsWall((int)x, (int)(y + dy))) y += dy;
    }
};

// ---------------- globals ----------------
DWORD g_pixels[SCREEN_W * SCREEN_H];
bool g_keys[256] = {};
bool g_running = true;

// ---------------- rendering ----------------
void Render(const Player& p)
{
    const DWORD CEILING = 0x101014;
    const DWORD FLOOR = 0x1C1C1C;

    for (int y = 0; y < SCREEN_H; ++y)
    {
        DWORD c = (y < SCREEN_H / 2) ? CEILING : FLOOR;
        for (int x = 0; x < SCREEN_W; ++x)
            g_pixels[y * SCREEN_W + x] = c;
    }

    for (int x = 0; x < SCREEN_W; ++x)
    {
        // ray direction for this column
        float camX = 2.0f * x / SCREEN_W - 1.0f;
        float rayX = p.dirX + p.planeX * camX;
        float rayY = p.dirY + p.planeY * camX;

        int mapX = (int)p.x, mapY = (int)p.y;
        float deltaX = (rayX == 0) ? 1e30f : fabsf(1.0f / rayX);
        float deltaY = (rayY == 0) ? 1e30f : fabsf(1.0f / rayY);

        int stepX, stepY;
        float sideX, sideY;
        if (rayX < 0) { stepX = -1; sideX = (p.x - mapX) * deltaX; }
        else { stepX = 1; sideX = (mapX + 1.0f - p.x) * deltaX; }
        if (rayY < 0) { stepY = -1; sideY = (p.y - mapY) * deltaY; }
        else { stepY = 1; sideY = (mapY + 1.0f - p.y) * deltaY; }

        // DDA: step through grid cells until we hit a wall
        int side = 0;
        while (true)
        {
            if (sideX < sideY) { sideX += deltaX; mapX += stepX; side = 0; }
            else { sideY += deltaY; mapY += stepY; side = 1; }
            if (IsWall(mapX, mapY)) break;
        }

        float dist = (side == 0) ? sideX - deltaX : sideY - deltaY;
        if (dist < 0.0001f) dist = 0.0001f;

        int lineH = (int)(SCREEN_H / dist);
        int top = SCREEN_H / 2 - lineH / 2;
        int bottom = SCREEN_H / 2 + lineH / 2;
        if (top < 0)           top = 0;
        if (bottom > SCREEN_H) bottom = SCREEN_H;

        // green, darker with distance, darker on Y-facing sides
        float light = 1.0f - dist / 12.0f;
        if (light < 0.15f) light = 0.15f;
        if (side == 1) light *= 0.7f;
        DWORD green = (DWORD)(220 * light);
        DWORD color = (green / 5) << 16 | green << 8 | (green / 5);

        for (int y = top; y < bottom; ++y)
            g_pixels[y * SCREEN_W + x] = color;
    }
}

// ---------------- win32 ----------------
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
    case WM_CLOSE:   g_running = false; return 0;
    case WM_KEYDOWN: if (wp == VK_ESCAPE) g_running = false;
        if (wp < 256) g_keys[wp] = true;  return 0;
    case WM_KEYUP:   if (wp < 256) g_keys[wp] = false; return 0;
    case WM_KILLFOCUS: for (int i = 0; i < 256; ++i) g_keys[i] = false; return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void Present(HWND hwnd)
{
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = SCREEN_W;
    bmi.bmiHeader.biHeight = -SCREEN_H; // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    RECT rc;
    GetClientRect(hwnd, &rc);
    HDC dc = GetDC(hwnd);
    StretchDIBits(dc, 0, 0, rc.right, rc.bottom, 0, 0, SCREEN_W, SCREEN_H,
        g_pixels, &bmi, DIB_RGB_COLORS, SRCCOPY);
    ReleaseDC(hwnd, dc);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int)
{
    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = L"Raycaster";
    RegisterClassW(&wc);

    RECT wr = { 0, 0, SCREEN_W * SCALE, SCREEN_H * SCALE };
    AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);
    HWND hwnd = CreateWindowW(L"Raycaster", L"Raycaster", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT,
        wr.right - wr.left, wr.bottom - wr.top,
        nullptr, nullptr, hInst, nullptr);

    Player player;

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

        QueryPerformanceCounter(&now);
        float dt = (float)(now.QuadPart - last.QuadPart) / freq.QuadPart;
        last = now;
        if (dt > 0.05f) dt = 0.05f;

        // turning
        if (g_keys[VK_LEFT] || g_keys['Q']) player.Rotate(-TURN_SPEED * dt);
        if (g_keys[VK_RIGHT] || g_keys['E']) player.Rotate(TURN_SPEED * dt);

        // moving
        float speed = MOVE_SPEED * dt;
        if (g_keys['W'] || g_keys[VK_UP])   player.Move(player.dirX * speed, player.dirY * speed);
        if (g_keys['S'] || g_keys[VK_DOWN]) player.Move(-player.dirX * speed, -player.dirY * speed);
        if (g_keys['D']) player.Move(-player.dirY * speed, player.dirX * speed);
        if (g_keys['A']) player.Move(player.dirY * speed, -player.dirX * speed);

        Render(player);
        Present(hwnd);
    }

    DestroyWindow(hwnd);
    return 0;
}