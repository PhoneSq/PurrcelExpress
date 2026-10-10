#include "Menu.h"
#include "Config.h"

static HFONT MakeFont(int height)
{
    return CreateFontW(height, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        NONANTIALIASED_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");
}

Menu::Menu() : m_selected(0)
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

    m_labels[0] = L"PLAY";  m_actions[0] = MENU_PLAY;
    m_labels[1] = L"EXIT";  m_actions[1] = MENU_EXIT;

    m_titleFont = MakeFont(56);
    m_buttonFont = MakeFont(26);
}

Menu::~Menu()
{
    DeleteObject(m_titleFont);
    DeleteObject(m_buttonFont);
}

int Menu::HitTest(int x, int y) const
{
    for (int i = 0; i < BUTTON_COUNT; ++i)
    {
        const RECT& r = m_buttons[i];
        if (x >= r.left && x < r.right && y >= r.top && y < r.bottom) return i;
    }
    return -1;
}

void Menu::OnMouseMove(int x, int y)
{
    int hit = HitTest(x, y);
    if (hit >= 0) m_selected = hit;
}

MenuAction Menu::OnClick(int x, int y)
{
    int hit = HitTest(x, y);
    return (hit >= 0) ? m_actions[hit] : MENU_NONE;
}

MenuAction Menu::OnKey(WPARAM vk)
{
    if (vk == VK_UP || vk == 'W') m_selected = (m_selected + BUTTON_COUNT - 1) % BUTTON_COUNT;
    if (vk == VK_DOWN || vk == 'S') m_selected = (m_selected + 1) % BUTTON_COUNT;
    if (vk == VK_RETURN || vk == VK_SPACE) return m_actions[m_selected];
    return MENU_NONE;
}

void Menu::FillRect(DWORD* pixels, const RECT& r, DWORD color)
{
    for (int y = r.top; y < r.bottom; ++y)
        for (int x = r.left; x < r.right; ++x)
            pixels[y * SCREEN_W + x] = color;
}

void Menu::Outline(DWORD* pixels, const RECT& r, DWORD color)
{
    for (int x = r.left; x < r.right; ++x)
    {
        pixels[r.top * SCREEN_W + x] = color;
        pixels[(r.bottom - 1) * SCREEN_W + x] = color;
    }
    for (int y = r.top; y < r.bottom; ++y)
    {
        pixels[y * SCREEN_W + r.left] = color;
        pixels[y * SCREEN_W + r.right - 1] = color;
    }
}

void Menu::Draw(HDC dc, DWORD* pixels)
{
    const DWORD BG = 0x0A0A0C;
    const DWORD GREEN = 0x1E8C1E;
    const DWORD DARK_BTN = 0x161816;

    // background
    for (int i = 0; i < SCREEN_W * SCREEN_H; ++i) pixels[i] = BG;

    // buttons
    for (int i = 0; i < BUTTON_COUNT; ++i)
    {
        bool sel = (i == m_selected);
        FillRect(pixels, m_buttons[i], sel ? GREEN : DARK_BTN);
        Outline(pixels, m_buttons[i], GREEN);
    }

    // text (GDI draws straight into the same buffer)
    SetBkMode(dc, TRANSPARENT);

    HFONT old = (HFONT)SelectObject(dc, m_titleFont);
    SetTextColor(dc, RGB(50, 220, 50));
    RECT title = { 0, 70, SCREEN_W, 140 };
    DrawTextW(dc, L"PURRCEL EXPRESS", -1, &title, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    SelectObject(dc, m_buttonFont);
    for (int i = 0; i < BUTTON_COUNT; ++i)
    {
        bool sel = (i == m_selected);
        SetTextColor(dc, sel ? RGB(10, 10, 12) : RGB(50, 220, 50));
        RECT r = m_buttons[i];
        DrawTextW(dc, m_labels[i], -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    SelectObject(dc, old);
}