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

GameState RunScreen::Update(float dt, const Input& input)
{
    if (input.WasPressed(VK_ESCAPE))
        return GameState::MainMenu;   // PLAY again resumes where you left off

    m_player.Update(dt, input, m_map);
    return GameState::Run;
}

void RunScreen::Render(Frame& frame)
{
    m_renderer.DrawWorld(frame.pixels, m_map, m_player);
}