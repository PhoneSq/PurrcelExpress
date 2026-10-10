#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

enum MenuAction
{
    MENU_NONE,
    MENU_PLAY,
    MENU_EXIT
};

class Menu
{
public:
    Menu();
    ~Menu();

    // Coordinates are in game pixels (0..SCREEN_W, 0..SCREEN_H).
    void       OnMouseMove(int x, int y);
    MenuAction OnClick(int x, int y);
    MenuAction OnKey(WPARAM vk);   // Up/Down/W/S to pick, Enter/Space to confirm

    // Fills the pixel buffer, then draws text on top with GDI.
    void Draw(HDC dc, DWORD* pixels);

private:
    static const int BUTTON_COUNT = 2;

    int  HitTest(int x, int y) const;
    void FillRect(DWORD* pixels, const RECT& r, DWORD color);
    void Outline(DWORD* pixels, const RECT& r, DWORD color);

    RECT         m_buttons[BUTTON_COUNT];
    const wchar_t* m_labels[BUTTON_COUNT];
    MenuAction   m_actions[BUTTON_COUNT];
    int          m_selected;

    HFONT m_titleFont;
    HFONT m_buttonFont;
};
