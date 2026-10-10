#include <math.h>
#include "Screen.h"
#include "Config.h"

// =====================================================
// MAIN MENU
// =====================================================

static HFONT MakeFont(int height)
{
    return CreateFontW(height, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        NONANTIALIASED_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");
}

MainMenuScreen::MainMenuScreen()
    : m_selected(0), m_lastMouseX(-1), m_lastMouseY(-1)
{
    const int w = 200, h = 44;
    const int left = (SCREEN_W - w) / 2;
    const int top = 190;

    for (int i = 0; i < BUTTON_COUNT; ++i)
    {
        m_buttons[i].left = left;
        m_buttons[i].right = left + w;
        m_buttons[i].top = top + i * (h + 20);
        m_buttons[i].bottom = m_buttons[i].top + h;
    }

    m_labels[0] = L"PLAY";  m_targets[0] = GameState::Run;
    m_labels[1] = L"EXIT";  m_targets[1] = GameState::Quit;

    m_titleFont = MakeFont(56);
    m_buttonFont = MakeFont(26);
}

MainMenuScreen::~MainMenuScreen()
{
    DeleteObject(m_titleFont);
    DeleteObject(m_buttonFont);
}

void MainMenuScreen::Enter()
{
    // Ignore where the mouse already is, so a resting cursor
    // doesn't steal the keyboard selection.
    m_lastMouseX = m_lastMouseY = -1;
}

int MainMenuScreen::HitTest(int x, int y) const
{
    for (int i = 0; i < BUTTON_COUNT; ++i)
    {
        const RECT& r = m_buttons[i];
        if (x >= r.left && x < r.right && y >= r.top && y < r.bottom) return i;
    }
    return -1;
}

GameState MainMenuScreen::Update(float, const Input& input)
{
    // Mouse: hover selects (only when it actually moves), click activates.
    int mx = input.MouseX(), my = input.MouseY();
    int hover = HitTest(mx, my);

    if ((mx != m_lastMouseX || my != m_lastMouseY) && m_lastMouseX != -1 && hover >= 0)
        m_selected = hover;
    m_lastMouseX = mx;
    m_lastMouseY = my;

    if (input.WasPressed(VK_LBUTTON) && hover >= 0)
        return m_targets[hover];

    // Keyboard
    if (input.WasPressed(VK_UP) || input.WasPressed('W'))
        m_selected = (m_selected + BUTTON_COUNT - 1) % BUTTON_COUNT;
    if (input.WasPressed(VK_DOWN) || input.WasPressed('S'))
        m_selected = (m_selected + 1) % BUTTON_COUNT;

    if (input.WasPressed(VK_RETURN) || input.WasPressed(VK_SPACE))
        return m_targets[m_selected];

    if (input.WasPressed(VK_ESCAPE))
        return GameState::Quit;

    return GameState::MainMenu;
}

void MainMenuScreen::FillBox(Frame& frame, const RECT& r, DWORD color)
{
    for (int y = r.top; y < r.bottom; ++y)
        for (int x = r.left; x < r.right; ++x)
            frame.pixels[y * frame.width + x] = color;
}

void MainMenuScreen::OutlineBox(Frame& frame, const RECT& r, DWORD color)
{
    for (int x = r.left; x < r.right; ++x)
    {
        frame.pixels[r.top * frame.width + x] = color;
        frame.pixels[(r.bottom - 1) * frame.width + x] = color;
    }
    for (int y = r.top; y < r.bottom; ++y)
    {
        frame.pixels[y * frame.width + r.left] = color;
        frame.pixels[y * frame.width + r.right - 1] = color;
    }
}

void MainMenuScreen::Render(Frame& frame)
{
    const DWORD BG = 0x0A0A0C;
    const DWORD GREEN = 0x1E8C1E;
    const DWORD DARK_BTN = 0x161816;

    for (int i = 0; i < frame.width * frame.height; ++i) frame.pixels[i] = BG;

    for (int i = 0; i < BUTTON_COUNT; ++i)
    {
        FillBox(frame, m_buttons[i], i == m_selected ? GREEN : DARK_BTN);
        OutlineBox(frame, m_buttons[i], GREEN);
    }

    // Text is drawn with GDI into the same pixels.
    HDC dc = frame.dc;
    SetBkMode(dc, TRANSPARENT);

    HGDIOBJ oldFont = SelectObject(dc, m_titleFont);
    SetTextColor(dc, RGB(50, 220, 50));
    RECT title = { 0, 70, frame.width, 140 };
    DrawTextW(dc, GAME_TITLE, -1, &title, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    SelectObject(dc, m_buttonFont);
    for (int i = 0; i < BUTTON_COUNT; ++i)
    {
        SetTextColor(dc, i == m_selected ? RGB(10, 10, 12) : RGB(50, 220, 50));
        RECT r = m_buttons[i];
        DrawTextW(dc, m_labels[i], -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    SelectObject(dc, oldFont);
}


// =====================================================
// RUN
// =====================================================

// Draws text with a 1px dark shadow so it stays readable over the 3D view.
static void ShadowText(HDC dc, const wchar_t* text, RECT r, UINT flags, COLORREF color)
{
    RECT shadow = r;
    OffsetRect(&shadow, 1, 1);
    SetTextColor(dc, RGB(0, 0, 0));
    DrawTextW(dc, text, -1, &shadow, flags);
    SetTextColor(dc, color);
    DrawTextW(dc, text, -1, &r, flags);
}

RunScreen::RunScreen() : m_time(0.0f)
{
    m_hudFont = MakeFont(16);
    m_bigFont = MakeFont(48);
    StartNewRun();
}

RunScreen::~RunScreen()
{
    DeleteObject(m_hudFont);
    DeleteObject(m_bigFont);
}

void RunScreen::StartNewRun()
{
    m_player.Reset(m_map.StartX(), m_map.StartY());
    m_delivery.Start(m_map.StartX(), m_map.StartY(), m_map.SpotX(), m_map.SpotY());
}

GameState RunScreen::Update(float dt, const Input& input)
{
    m_time += dt;

    if (input.WasPressed(VK_ESCAPE))
        return GameState::MainMenu;   // PLAY again resumes where you left off

    if (m_delivery.GetPhase() == Delivery::Phase::Done)
    {
        if (input.WasPressed(VK_RETURN)) StartNewRun();
        return GameState::Run;   // frozen until ENTER
    }

    m_player.Update(dt, input, m_map);
    m_delivery.Update(m_player.X(), m_player.Y());
    return GameState::Run;
}

void RunScreen::Render(Frame& frame)
{
    m_renderer.DrawWorld(frame.pixels, m_map, m_player);

    // Marker on the current target: orange = drop-off, green = way home.
    if (m_delivery.GetPhase() != Delivery::Phase::Done)
    {
        bool toSpot = m_delivery.GetPhase() == Delivery::Phase::ToSpot;
        DWORD color = toSpot ? 0xFF8C00 : 0x50FF50;
        float pulse = 0.5f + 0.5f * sinf(m_time * 3.0f);
        m_renderer.DrawMarker(frame.pixels, m_player,
            m_delivery.TargetX(), m_delivery.TargetY(), color, pulse);
    }

    m_renderer.DrawCrosshair(frame.pixels);
    DrawHud(frame);
}

void RunScreen::DrawHud(Frame& frame)
{
    HDC dc = frame.dc;
    SetBkMode(dc, TRANSPARENT);
    HGDIOBJ oldFont = SelectObject(dc, m_hudFont);

    Delivery::Phase phase = m_delivery.GetPhase();

    if (phase != Delivery::Phase::Done)
    {
        const wchar_t* objective = (phase == Delivery::Phase::ToSpot)
            ? L"Deliver the package to the ORANGE marker"
            : L"Package delivered! Head back to the GREEN marker";

        RECT line1 = { 10, 8,  frame.width, 28 };
        RECT line2 = { 10, 26, frame.width, 46 };
        ShadowText(dc, objective, line1, DT_LEFT | DT_SINGLELINE, RGB(255, 220, 0));

        // One decimal place with plain Win32 (wsprintf has no floats).
        int tenths = (int)(m_delivery.DistanceFrom(m_player.X(), m_player.Y()) * 10.0f);
        wchar_t distText[64];
        wsprintfW(distText, L"Distance: %d.%d tiles", tenths / 10, tenths % 10);
        ShadowText(dc, distText, line2, DT_LEFT | DT_SINGLELINE, RGB(230, 230, 230));
    }
    else
    {
        // Darken the view, then the message on top.
        for (int i = 0; i < frame.width * frame.height; ++i)
            frame.pixels[i] = (frame.pixels[i] >> 2) & 0x3F3F3F;

        SelectObject(dc, m_bigFont);
        RECT title = { 0, 130, frame.width, 190 };
        ShadowText(dc, L"DELIVERED!", title, DT_CENTER | DT_SINGLELINE, RGB(50, 220, 50));

        SelectObject(dc, m_hudFont);
        RECT hint = { 0, 210, frame.width, 230 };
        ShadowText(dc, L"ENTER: new delivery     ESC: main menu", hint,
            DT_CENTER | DT_SINGLELINE, RGB(200, 200, 200));
    }

    SelectObject(dc, oldFont);
}