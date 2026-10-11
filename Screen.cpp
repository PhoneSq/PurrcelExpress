#include <math.h>
#include "Screen.h"
#include "Config.h"
#include "AItexture.h"

// =====================================================
// Shared drawing helpers
// =====================================================

static HFONT MakeFont(int height)
{
    return CreateFontW(height, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        NONANTIALIASED_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");
}

static void FillBox(Frame& frame, const RECT& r, DWORD color)
{
    for (int y = r.top; y < r.bottom; ++y)
        for (int x = r.left; x < r.right; ++x)
            frame.pixels[y * frame.width + x] = color;
}

static void OutlineBox(Frame& frame, const RECT& r, DWORD color)
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

// Darkens the whole frame (for overlays).
static void DimFrame(Frame& frame)
{
    for (int i = 0; i < frame.width * frame.height; ++i)
        frame.pixels[i] = (frame.pixels[i] >> 2) & 0x3F3F3F;
}

// Text with a 1px dark shadow so it stays readable over the 3D view.
static void ShadowText(HDC dc, const wchar_t* text, RECT r, UINT flags, COLORREF color)
{
    RECT shadow = r;
    OffsetRect(&shadow, 1, 1);
    SetTextColor(dc, RGB(0, 0, 0));
    DrawTextW(dc, text, -1, &shadow, flags);
    SetTextColor(dc, color);
    DrawTextW(dc, text, -1, &r, flags);
}

static void PlainText(HDC dc, const wchar_t* text, RECT r, UINT flags, COLORREF color)
{
    SetTextColor(dc, color);
    DrawTextW(dc, text, -1, &r, flags);
}

// Express markers blink, faster when time is almost up.
static bool ExpressBlinkOn(float timeLeft)
{
    float rate = (timeLeft < EXPRESS_URGENT) ? 6.0f : 2.0f;
    return ((int)(timeLeft * rate) % 2) == 0;
}

// Draws a texture as big as fits in the box (x, y, w, h), keeping its shape,
// centered. Nearest pixel, alpha cut at half.
static void DrawIcon(Frame& frame, const Texture& tex, int x, int y, int w, int h)
{
    if (!tex.IsLoaded()) return;
    int tw = tex.VisibleWidth(), th = tex.VisibleHeight();
    if (tw * h > th * w) { int nh = w * th / tw; y += (h - nh) / 2; h = nh; }   // wide: fit width
    else { int nw = h * tw / th; x += (w - nw) / 2; w = nw; }   // tall: fit height
    if (w < 1 || h < 1) return;
    for (int py = 0; py < h; ++py)
    {
        int sy = y + py;
        if (sy < 0 || sy >= frame.height) continue;
        for (int px = 0; px < w; ++px)
        {
            int sx = x + px;
            if (sx < 0 || sx >= frame.width) continue;
            int alpha;
            DWORD c = tex.Sample((px + 0.5f) / w, (py + 0.5f) / h, alpha);
            if (alpha >= 128) frame.pixels[sy * frame.width + sx] = c;
        }
    }
}

// Moves the whole picture down (dy > 0) or up (dy < 0); the gap is filled
// with the edge row (sky at the top, ground at the bottom).
static void ShiftFrameY(Frame& frame, int dy)
{
    if (dy == 0) return;
    int W = frame.width, H = frame.height;
    if (dy >= H || -dy >= H) return;
    if (dy > 0)
    {
        for (int y = H - 1; y >= dy; --y)
            CopyMemory(&frame.pixels[y * W], &frame.pixels[(y - dy) * W], W * sizeof(DWORD));
        for (int y = 0; y < dy; ++y)
            CopyMemory(&frame.pixels[y * W], &frame.pixels[dy * W], W * sizeof(DWORD));
    }
    else
    {
        int up = -dy;
        for (int y = 0; y < H - up; ++y)
            CopyMemory(&frame.pixels[y * W], &frame.pixels[(y + up) * W], W * sizeof(DWORD));
        for (int y = H - up; y < H; ++y)
            CopyMemory(&frame.pixels[y * W], &frame.pixels[(H - up - 1) * W], W * sizeof(DWORD));
    }
}

// ----- Hand-drawn HUD pieces (same ink + cream as the character art) -----

static const DWORD HUD_INK = 0x2A1E1A;
static const DWORD HUD_CREAM = 0xFFF6E6;
static const DWORD HUD_EMPTY = 0x4A3A34;

// Distance from (px, py) to the edge of a pill (rounded-end bar) filling r.
// Negative = inside. A square r gives a circle.
static float PillDist(const RECT& r, float px, float py)
{
    float h = (float)(r.bottom - r.top);
    float w = (float)(r.right - r.left);
    float rad = (h < w ? h : w) * 0.5f;
    float cy = (r.top + r.bottom) * 0.5f;
    float x0 = r.left + rad, x1 = r.right - rad;
    float cx = px < x0 ? x0 : (px > x1 ? x1 : px);
    float dx = px - cx, dy = py - cy;
    return sqrtf(dx * dx + dy * dy) - rad;
}

static DWORD Brighten(DWORD c, int add)
{
    int r = (int)((c >> 16) & 255) + add, g = (int)((c >> 8) & 255) + add, b = (int)(c & 255) + add;
    if (r > 255) r = 255;
    if (g > 255) g = 255;
    if (b > 255) b = 255;
    return (DWORD)(r << 16 | g << 8 | b);
}

// A pill with an ink outline. Pixels left of fillRight use 'fill' (with a
// little shine along the top), the rest 'empty'.
static void DrawPill(Frame& frame, const RECT& r, DWORD empty, DWORD fill, int fillRight)
{
    for (int y = r.top; y < r.bottom; ++y)
    {
        if (y < 0 || y >= frame.height) continue;
        for (int x = r.left; x < r.right; ++x)
        {
            if (x < 0 || x >= frame.width) continue;
            float d = PillDist(r, x + 0.5f, y + 0.5f);
            if (d > 0.0f) continue;

            DWORD c;
            if (d > -2.0f)          c = HUD_INK;
            else if (x < fillRight) c = (y < r.top + (r.bottom - r.top) * 0.4f) ? Brighten(fill, 45) : fill;
            else                    c = empty;
            frame.pixels[y * frame.width + x] = c;
        }
    }
}

// Ink lines splitting a pill into 'parts' equal segments.
static void PillDividers(Frame& frame, const RECT& r, int parts)
{
    for (int i = 1; i < parts; ++i)
    {
        int x = r.left + (r.right - r.left) * i / parts;
        for (int y = r.top; y < r.bottom; ++y)
            for (int dx = -1; dx <= 0; ++dx)
                if (PillDist(r, x + dx + 0.5f, y + 0.5f) < 0.0f)
                    frame.pixels[y * frame.width + x + dx] = HUD_INK;
    }
}

static const DWORD UI_BG = 0x0A0A0C;
static const DWORD UI_GREEN = 0x1E8C1E;
static const DWORD UI_DARK = 0x161816;
static const COLORREF TEXT_GREEN = RGB(50, 220, 50);


// =====================================================
// MAIN MENU
// =====================================================

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

    m_labels[0] = L"PLAY";  m_targets[0] = GameState::CharacterSelect;
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

void MainMenuScreen::Render(Frame& frame)
{
    for (int i = 0; i < frame.width * frame.height; ++i) frame.pixels[i] = UI_BG;

    for (int i = 0; i < BUTTON_COUNT; ++i)
    {
        FillBox(frame, m_buttons[i], i == m_selected ? UI_GREEN : UI_DARK);
        OutlineBox(frame, m_buttons[i], UI_GREEN);
    }

    // Text is drawn with GDI into the same pixels.
    HDC dc = frame.dc;
    SetBkMode(dc, TRANSPARENT);

    HGDIOBJ oldFont = SelectObject(dc, m_titleFont);
    RECT title = { 0, 70, frame.width, 140 };
    PlainText(dc, GAME_TITLE, title, DT_CENTER | DT_VCENTER | DT_SINGLELINE, TEXT_GREEN);

    SelectObject(dc, m_buttonFont);
    for (int i = 0; i < BUTTON_COUNT; ++i)
    {
        RECT r = m_buttons[i];
        PlainText(dc, m_labels[i], r, DT_CENTER | DT_VCENTER | DT_SINGLELINE,
            i == m_selected ? RGB(10, 10, 12) : TEXT_GREEN);
    }

    SelectObject(dc, oldFont);
}


// =====================================================
// CHARACTER SELECT
// =====================================================

CharacterSelectScreen::CharacterSelectScreen(GameData& data)
    : m_data(data), m_selected(0), m_lastMouseX(-1), m_lastMouseY(-1)
{
    m_titleFont = MakeFont(26);
    m_nameFont = MakeFont(17);
    m_textFont = MakeFont(13);

    for (int i = 0; i < COUNT; ++i)
        m_portraits[i].Load(GetCharacterInfo((CharacterType)i).spriteSide);
    for (int i = 1; i < (int)AbilityType::Count; ++i)
        MakeAbilityIcon((AbilityType)i, m_abilityIcons[i]);
}

CharacterSelectScreen::~CharacterSelectScreen()
{
    DeleteObject(m_titleFont);
    DeleteObject(m_nameFont);
    DeleteObject(m_textFont);
}

void CharacterSelectScreen::Enter()
{
    m_selected = (int)m_data.character;
    m_lastMouseX = m_lastMouseY = -1;
}

RECT CharacterSelectScreen::CardRect(int index) const
{
    const int gap = 24, top = 62, h = 290;
    int w = (SCREEN_W - 60 - (COUNT - 1) * gap) / COUNT;
    int left = (SCREEN_W - (COUNT * w + (COUNT - 1) * gap)) / 2 + index * (w + gap);
    RECT r = { left, top, left + w, top + h };
    return r;
}

int CharacterSelectScreen::HitTest(int x, int y) const
{
    for (int i = 0; i < COUNT; ++i)
    {
        RECT r = CardRect(i);
        if (x >= r.left && x < r.right && y >= r.top && y < r.bottom) return i;
    }
    return -1;
}

GameState CharacterSelectScreen::Update(float, const Input& input)
{
    int mx = input.MouseX(), my = input.MouseY();
    int hover = HitTest(mx, my);
    if ((mx != m_lastMouseX || my != m_lastMouseY) && m_lastMouseX != -1 && hover >= 0)
        m_selected = hover;
    m_lastMouseX = mx;
    m_lastMouseY = my;

    if (input.WasPressed(VK_LEFT) || input.WasPressed('A'))
        m_selected = (m_selected + COUNT - 1) % COUNT;
    if (input.WasPressed(VK_RIGHT) || input.WasPressed('D'))
        m_selected = (m_selected + 1) % COUNT;

    bool click = input.WasPressed(VK_LBUTTON) && hover >= 0;
    if (click) m_selected = hover;
    if (click || input.WasPressed(VK_RETURN) || input.WasPressed(VK_SPACE))
    {
        m_data.character = (CharacterType)m_selected;
        return GameState::DeliverySelect;
    }

    if (input.WasPressed(VK_ESCAPE)) return GameState::MainMenu;
    return GameState::CharacterSelect;
}

void CharacterSelectScreen::Render(Frame& frame)
{
    for (int i = 0; i < frame.width * frame.height; ++i) frame.pixels[i] = UI_BG;

    for (int i = 0; i < COUNT; ++i)
    {
        RECT r = CardRect(i);
        bool sel = (i == m_selected);
        const CharacterInfo& info = GetCharacterInfo((CharacterType)i);

        FillBox(frame, r, sel ? 0x0A3C0A : UI_DARK);
        OutlineBox(frame, r, sel ? 0x32DC32 : 0x2A4A2A);

        // The cat, side view, on a cream backdrop.
        RECT pic = { r.left + 10, r.top + 10, r.right - 10, r.top + 130 };
        FillBox(frame, pic, sel ? 0xFFF6E6 : 0xC9BCA8);
        DrawIcon(frame, m_portraits[i], pic.left + 6, pic.top + 6,
            pic.right - pic.left - 12, pic.bottom - pic.top - 12);

        // Ability icon next to the ability name.
        DrawIcon(frame, m_abilityIcons[(int)info.ability], r.left + 12, r.top + 166, 28, 14);
    }

    HDC dc = frame.dc;
    SetBkMode(dc, TRANSPARENT);
    HGDIOBJ oldFont = SelectObject(dc, m_titleFont);
    wchar_t buf[96];

    RECT title = { 0, 16, frame.width, 50 };
    PlainText(dc, L"CHOOSE YOUR CAT", title, DT_CENTER | DT_SINGLELINE, TEXT_GREEN);

    for (int i = 0; i < COUNT; ++i)
    {
        RECT r = CardRect(i);
        bool sel = (i == m_selected);
        const CharacterInfo& info = GetCharacterInfo((CharacterType)i);
        const AbilityInfo& ab = GetAbilityInfo(info.ability);

        SelectObject(dc, m_nameFont);
        RECT name = { r.left, r.top + 138, r.right, r.top + 158 };
        PlainText(dc, info.name, name, DT_CENTER | DT_SINGLELINE, sel ? RGB(255, 255, 255) : RGB(180, 180, 180));

        SelectObject(dc, m_textFont);
        RECT abName = { r.left + 46, r.top + 165, r.right - 6, r.top + 181 };
        wsprintfW(buf, L"%s (E)", ab.name);
        PlainText(dc, buf, abName, DT_LEFT | DT_SINGLELINE, RGB(255, 220, 0));

        RECT desc = { r.left + 10, r.top + 190, r.right - 10, r.bottom - 26 };
        PlainText(dc, ab.description, desc, DT_LEFT | DT_WORDBREAK, RGB(170, 200, 170));

        RECT cd = { r.left + 10, r.bottom - 22, r.right - 10, r.bottom - 6 };
        wsprintfW(buf, L"Cooldown: %ds", (int)(ab.cooldown + 0.5f));
        PlainText(dc, buf, cd, DT_LEFT | DT_SINGLELINE, RGB(120, 140, 120));
    }

    RECT hint = { 0, 372, frame.width, 390 };
    PlainText(dc, L"LEFT/RIGHT: select     ENTER / CLICK: choose     ESC: back", hint,
        DT_CENTER | DT_SINGLELINE, RGB(110, 110, 110));

    SelectObject(dc, oldFont);
}


// =====================================================
// DELIVERY SELECT
// =====================================================

DeliverySelectScreen::DeliverySelectScreen(GameData& data)
    : m_data(data), m_selected(0), m_lastMouseX(-1), m_lastMouseY(-1)
{
    m_titleFont = MakeFont(26);
    m_nameFont = MakeFont(17);
    m_textFont = MakeFont(13);
    RollContracts(m_contracts, m_data.rng);
    m_data.newContracts = false;

    m_shopButton = { 40, 18, 140, 46 };
}

DeliverySelectScreen::~DeliverySelectScreen()
{
    DeleteObject(m_titleFont);
    DeleteObject(m_nameFont);
    DeleteObject(m_textFont);
}

void DeliverySelectScreen::Enter()
{
    // Only a new list after a delivery: popping into the shop or the
    // menu and back doesn't give free rerolls.
    if (m_data.newContracts)
    {
        RollContracts(m_contracts, m_data.rng);
        m_data.newContracts = false;
        m_selected = 0;
    }
    m_lastMouseX = m_lastMouseY = -1;
}

RECT DeliverySelectScreen::RowRect(int index) const
{
    const int left = 40, width = SCREEN_W - 80;
    const int top = 62, height = 52, gap = 6;
    RECT r = { left, top + index * (height + gap), left + width, top + index * (height + gap) + height };
    return r;
}

int DeliverySelectScreen::HitTest(int x, int y) const
{
    for (int i = 0; i < DELIVERY_CHOICES; ++i)
    {
        RECT r = RowRect(i);
        if (x >= r.left && x < r.right && y >= r.top && y < r.bottom) return i;
    }
    const RECT& s = m_shopButton;
    if (x >= s.left && x < s.right && y >= s.top && y < s.bottom) return SHOP_BUTTON;
    return -1;
}

GameState DeliverySelectScreen::Update(float, const Input& input)
{
    int mx = input.MouseX(), my = input.MouseY();
    int hit = HitTest(mx, my);

    if (input.WasPressed('B') || (input.WasPressed(VK_LBUTTON) && hit == SHOP_BUTTON))
        return GameState::Shop;

    int hover = (hit == SHOP_BUTTON) ? -1 : hit;
    if ((mx != m_lastMouseX || my != m_lastMouseY) && m_lastMouseX != -1 && hover >= 0)
        m_selected = hover;
    m_lastMouseX = mx;
    m_lastMouseY = my;

    bool accept = (input.WasPressed(VK_LBUTTON) && hover >= 0) ||
        input.WasPressed(VK_RETURN) || input.WasPressed(VK_SPACE);
    if (accept)
    {
        if (input.WasPressed(VK_LBUTTON) && hover >= 0) m_selected = hover;
        m_data.chosen = m_contracts[m_selected];
        return GameState::Run;
    }

    if (input.WasPressed(VK_UP) || input.WasPressed('W'))
        m_selected = (m_selected + DELIVERY_CHOICES - 1) % DELIVERY_CHOICES;
    if (input.WasPressed(VK_DOWN) || input.WasPressed('S'))
        m_selected = (m_selected + 1) % DELIVERY_CHOICES;

    if (input.WasPressed(VK_ESCAPE)) return GameState::MainMenu;

    return GameState::DeliverySelect;
}

void DeliverySelectScreen::Render(Frame& frame)
{
    for (int i = 0; i < frame.width * frame.height; ++i) frame.pixels[i] = UI_BG;

    // Rows (pixels first, text after)
    for (int i = 0; i < DELIVERY_CHOICES; ++i)
    {
        RECT r = RowRect(i);
        bool sel = (i == m_selected);
        FillBox(frame, r, sel ? 0x0A3C0A : UI_DARK);
        if (sel) OutlineBox(frame, r, 0x32DC32);

        // Difficulty pips on the right of the second line: green -> red.
        static const DWORD PIP_COLORS[5] = { 0x32C832, 0x96D232, 0xE6C828, 0xE67828, 0xE63232 };
        const Contract& c = m_contracts[i];
        for (int p = 0; p < 5; ++p)
        {
            RECT pip = { r.right - 12 - (5 - p) * 12, r.top + 32, r.right - 12 - (5 - p) * 12 + 8, r.top + 40 };
            FillBox(frame, pip, p < c.difficulty ? PIP_COLORS[c.difficulty - 1] : 0x3C3C3C);
        }
    }

    bool shopHover = HitTest(m_lastMouseX, m_lastMouseY) == SHOP_BUTTON;
    FillBox(frame, m_shopButton, shopHover ? 0x0A3C0A : UI_DARK);
    OutlineBox(frame, m_shopButton, UI_GREEN);

    HDC dc = frame.dc;
    SetBkMode(dc, TRANSPARENT);
    HGDIOBJ oldFont = SelectObject(dc, m_titleFont);

    RECT title = { 0, 16, frame.width, 50 };
    PlainText(dc, L"SELECT DELIVERY", title, DT_CENTER | DT_SINGLELINE, TEXT_GREEN);

    SelectObject(dc, m_nameFont);
    wchar_t buf[128];
    wsprintfW(buf, L"$%d", m_data.money);
    RECT money = { 0, 22, frame.width - 40, 50 };
    PlainText(dc, buf, money, DT_RIGHT | DT_SINGLELINE, RGB(255, 220, 0));

    for (int i = 0; i < DELIVERY_CHOICES; ++i)
    {
        const Contract& c = m_contracts[i];
        RECT r = RowRect(i);
        bool sel = (i == m_selected);

        SelectObject(dc, m_nameFont);
        RECT name = { r.left + 12, r.top + 6, r.right - 12, r.top + 26 };
        wsprintfW(buf, L"%s%s", sel ? L"> " : L"  ", c.client);
        PlainText(dc, buf, name, DT_LEFT | DT_SINGLELINE, sel ? RGB(255, 255, 255) : RGB(170, 170, 170));

        // Pay on the right of the first line, express bonus before it.
        wsprintfW(buf, L"$%d", c.reward);
        PlainText(dc, buf, name, DT_RIGHT | DT_SINGLELINE, RGB(255, 220, 0));
        if (c.express)
        {
            RECT ex = name;
            ex.right -= 70;
            wsprintfW(buf, L"EXPRESS +$%d", c.expressBonus);
            PlainText(dc, buf, ex, DT_RIGHT | DT_SINGLELINE, RGB(0, 230, 255));
        }

        SelectObject(dc, m_textFont);
        RECT detail = { r.left + 28, r.top + 30, r.right - 80, r.bottom };
        wsprintfW(buf, L"Stops: %d     Dogs: %d", c.stops, c.enemies);
        PlainText(dc, buf, detail, DT_LEFT | DT_SINGLELINE, sel ? RGB(200, 255, 200) : RGB(120, 120, 120));

        RECT when = detail;
        when.left += 200;
        PlainText(dc, c.night ? L"NIGHT" : L"DAY", when, DT_LEFT | DT_SINGLELINE,
            c.night ? RGB(170, 150, 255) : RGB(255, 210, 110));
    }

    SelectObject(dc, m_nameFont);
    PlainText(dc, L"SHOP (B)", m_shopButton, DT_CENTER | DT_VCENTER | DT_SINGLELINE, TEXT_GREEN);

    SelectObject(dc, m_textFont);
    RECT carry = { 0, 352, frame.width, 368 };
    if (m_data.heldItem != ItemType::None)
        wsprintfW(buf, L"Carrying: %s  (press Q during a delivery to use it)", GetItemInfo(m_data.heldItem).name);
    else
        lstrcpyW(buf, L"Not carrying an item");
    PlainText(dc, buf, carry, DT_CENTER | DT_SINGLELINE,
        m_data.heldItem != ItemType::None ? RGB(255, 220, 0) : RGB(90, 90, 90));

    RECT hint = { 0, 372, frame.width, 390 };
    PlainText(dc, L"UP/DOWN: select     ENTER / CLICK: accept     B: shop     ESC: back", hint,
        DT_CENTER | DT_SINGLELINE, RGB(110, 110, 110));

    SelectObject(dc, oldFont);
}


// =====================================================
// SHOP
// =====================================================

static const float FEEDBACK_TIME = 2.0f;

ShopScreen::ShopScreen(GameData& data)
    : m_data(data), m_selected(0), m_lastMouseX(-1), m_lastMouseY(-1),
    m_feedback(L""), m_feedbackColor(RGB(255, 255, 255)), m_feedbackTimer(0.0f)
{
    m_titleFont = MakeFont(26);
    m_nameFont = MakeFont(17);
    m_textFont = MakeFont(13);
    m_backButton = { 40, 18, 140, 46 };

    for (int i = 1; i < (int)ItemType::Count; ++i)
        MakeItemIcon((ItemType)i, m_icons[i]);
}

ShopScreen::~ShopScreen()
{
    DeleteObject(m_titleFont);
    DeleteObject(m_nameFont);
    DeleteObject(m_textFont);
}

void ShopScreen::Enter()
{
    m_lastMouseX = m_lastMouseY = -1;
    m_feedbackTimer = 0.0f;
}

RECT ShopScreen::CardRect(int index) const
{
    // Cards shrink to fit as items get added.
    const int h = 210, gap = 30;
    int w = (SCREEN_W - 80 - (ITEM_COUNT - 1) * gap) / ITEM_COUNT;
    if (w > 220) w = 220;
    int total = ITEM_COUNT * w + (ITEM_COUNT - 1) * gap;
    int left = (SCREEN_W - total) / 2 + index * (w + gap);
    RECT r = { left, 70, left + w, 70 + h };
    return r;
}

int ShopScreen::HitTest(int x, int y) const
{
    for (int i = 0; i < ITEM_COUNT; ++i)
    {
        RECT r = CardRect(i);
        if (x >= r.left && x < r.right && y >= r.top && y < r.bottom) return i;
    }
    const RECT& b = m_backButton;
    if (x >= b.left && x < b.right && y >= b.top && y < b.bottom) return BACK_BUTTON;
    return -1;
}

void ShopScreen::TryBuy(int index)
{
    const ItemInfo& info = GetItemInfo(CardItem(index));

    if (m_data.heldItem != ItemType::None)
    {
        m_feedback = L"YOU ALREADY HAVE AN ITEM - USE IT FIRST";
        m_feedbackColor = RGB(255, 160, 60);
    }
    else if (m_data.money < info.price)
    {
        m_feedback = L"NOT ENOUGH MONEY";
        m_feedbackColor = RGB(255, 80, 80);
    }
    else
    {
        m_data.money -= info.price;
        m_data.heldItem = CardItem(index);
        m_feedback = L"BOUGHT!";
        m_feedbackColor = RGB(80, 255, 80);
    }
    m_feedbackTimer = FEEDBACK_TIME;
}

GameState ShopScreen::Update(float dt, const Input& input)
{
    if (m_feedbackTimer > 0.0f) m_feedbackTimer -= dt;

    int mx = input.MouseX(), my = input.MouseY();
    int hit = HitTest(mx, my);

    if (input.WasPressed(VK_ESCAPE) || input.WasPressed('B') ||
        (input.WasPressed(VK_LBUTTON) && hit == BACK_BUTTON))
        return GameState::DeliverySelect;

    int hover = (hit == BACK_BUTTON) ? -1 : hit;
    if ((mx != m_lastMouseX || my != m_lastMouseY) && m_lastMouseX != -1 && hover >= 0)
        m_selected = hover;
    m_lastMouseX = mx;
    m_lastMouseY = my;

    if (input.WasPressed(VK_LEFT) || input.WasPressed('A'))
        m_selected = (m_selected + ITEM_COUNT - 1) % ITEM_COUNT;
    if (input.WasPressed(VK_RIGHT) || input.WasPressed('D'))
        m_selected = (m_selected + 1) % ITEM_COUNT;

    if (input.WasPressed(VK_LBUTTON) && hover >= 0)
    {
        m_selected = hover;
        TryBuy(m_selected);
    }
    else if (input.WasPressed(VK_RETURN) || input.WasPressed(VK_SPACE))
        TryBuy(m_selected);

    return GameState::Shop;
}

void ShopScreen::Render(Frame& frame)
{
    for (int i = 0; i < frame.width * frame.height; ++i) frame.pixels[i] = UI_BG;

    bool holding = m_data.heldItem != ItemType::None;

    for (int i = 0; i < ITEM_COUNT; ++i)
    {
        RECT r = CardRect(i);
        bool sel = (i == m_selected);
        FillBox(frame, r, sel ? 0x0A3C0A : UI_DARK);
        OutlineBox(frame, r, sel ? 0x32DC32 : 0x2A4A2A);

        // Potion at 4x, centered near the top.
        const int iw = 64, ih = 52;   // potions fill the height, the bone the width
        DrawIcon(frame, m_icons[(int)CardItem(i)], (r.left + r.right - iw) / 2, r.top + 16, iw, ih);
    }

    bool backHover = HitTest(m_lastMouseX, m_lastMouseY) == BACK_BUTTON;
    FillBox(frame, m_backButton, backHover ? 0x0A3C0A : UI_DARK);
    OutlineBox(frame, m_backButton, UI_GREEN);

    HDC dc = frame.dc;
    SetBkMode(dc, TRANSPARENT);
    HGDIOBJ oldFont = SelectObject(dc, m_titleFont);
    wchar_t buf[128];

    RECT title = { 0, 16, frame.width, 50 };
    PlainText(dc, L"SHOP", title, DT_CENTER | DT_SINGLELINE, TEXT_GREEN);

    SelectObject(dc, m_nameFont);
    PlainText(dc, L"< BACK", m_backButton, DT_CENTER | DT_VCENTER | DT_SINGLELINE, TEXT_GREEN);

    wsprintfW(buf, L"$%d", m_data.money);
    RECT money = { 0, 22, frame.width - 40, 50 };
    PlainText(dc, buf, money, DT_RIGHT | DT_SINGLELINE, RGB(255, 220, 0));

    for (int i = 0; i < ITEM_COUNT; ++i)
    {
        RECT r = CardRect(i);
        bool sel = (i == m_selected);
        const ItemInfo& info = GetItemInfo(CardItem(i));
        bool owned = m_data.heldItem == CardItem(i);
        bool affordable = m_data.money >= info.price;

        SelectObject(dc, m_nameFont);
        RECT name = { r.left, r.top + 80, r.right, r.top + 100 };
        PlainText(dc, info.name, name, DT_CENTER | DT_SINGLELINE, sel ? RGB(255, 255, 255) : RGB(180, 180, 180));

        SelectObject(dc, m_textFont);
        RECT desc = { r.left + 10, r.top + 104, r.right - 10, r.top + 156 };
        PlainText(dc, info.description, desc, DT_CENTER | DT_WORDBREAK, RGB(170, 200, 170));

        SelectObject(dc, m_nameFont);
        RECT price = { r.left, r.bottom - 52, r.right, r.bottom - 32 };
        wsprintfW(buf, L"$%d", info.price);
        PlainText(dc, buf, price, DT_CENTER | DT_SINGLELINE,
            affordable ? RGB(255, 220, 0) : RGB(150, 70, 70));

        SelectObject(dc, m_textFont);
        RECT state = { r.left, r.bottom - 26, r.right, r.bottom - 10 };
        const wchar_t* label = owned ? L"CARRYING"
            : holding ? L"ONE ITEM AT A TIME"
            : affordable ? (sel ? L"ENTER / CLICK TO BUY" : L"BUY")
            : L"CAN'T AFFORD";
        COLORREF color = owned ? RGB(80, 255, 80)
            : (holding || !affordable) ? RGB(110, 110, 110)
            : (sel ? RGB(255, 255, 255) : RGB(170, 170, 170));
        PlainText(dc, label, state, DT_CENTER | DT_SINGLELINE, color);
    }

    RECT carry = { 0, 296, frame.width, 314 };
    if (holding)
        wsprintfW(buf, L"Carrying: %s - use it on a delivery (Q) to buy another",
            GetItemInfo(m_data.heldItem).name);
    else
        lstrcpyW(buf, L"You can carry one item. Press Q during a delivery to use it.");
    PlainText(dc, buf, carry, DT_CENTER | DT_SINGLELINE, holding ? RGB(255, 220, 0) : RGB(150, 150, 150));

    if (m_feedbackTimer > 0.0f)
    {
        SelectObject(dc, m_nameFont);
        RECT fb = { 0, 324, frame.width, 346 };
        PlainText(dc, m_feedback, fb, DT_CENTER | DT_SINGLELINE, m_feedbackColor);
        SelectObject(dc, m_textFont);
    }

    RECT hint = { 0, 372, frame.width, 390 };
    PlainText(dc, L"LEFT/RIGHT: select     ENTER / CLICK: buy     ESC / B: back", hint,
        DT_CENTER | DT_SINGLELINE, RGB(110, 110, 110));

    SelectObject(dc, oldFont);
}


// =====================================================
// RUN
// =====================================================

static const float MESSAGE_TIME = 3.0f;   // seconds a message stays up

RunScreen::RunScreen(GameData& data)
    : m_data(data), m_enemyCount(0), m_thirdPerson(true), m_loadedCharacter(-1), m_shake(0.0f), m_fireTimer(0.0f), m_alertTimer(0.0f),
    m_boneActive(false), m_boneX(0), m_boneY(0), m_boneTimer(0), m_time(0.0f), m_hitFlash(0.0f),
    m_messageColor(RGB(255, 255, 255)), m_messageTimer(0.0f)
{
    m_message[0] = 0;

    // The cat pictures are loaded in Enter (they depend on which cat you picked).
    m_package.Load(L"Pack.png");
    MakeShadow(m_shadow);
    if (!m_furBall.Load(L"Fur.png")) MakeFurBall(m_furBall);
    for (int i = 0; i < 3; ++i) MakeDizzy(m_dizzy[i], i);
    for (int i = 0; i < MAX_FUR_BALLS; ++i) m_furBalls[i].alive = false;
    for (int i = 1; i < (int)AbilityType::Count; ++i)
        MakeAbilityIcon((AbilityType)i, m_abilityIcons[i]);

    // Dog art per type. Add the blue and yellow file names when they exist.
    m_dogs[(int)EnemyType::Blue].Load(L"Chasey.png");     // the chaser
    m_dogs[(int)EnemyType::Purple].Load(L"Blocky.png");   // the blocker

    for (int i = 1; i < (int)ItemType::Count; ++i)
        MakeItemIcon((ItemType)i, m_itemIcons[i]);
    MakeAlert(m_alertIcon);
    MakeHeart(m_heart);
    m_hudFont = MakeFont(14);
    m_timerFont = MakeFont(26);
    m_bigFont = MakeFont(48);
}

RunScreen::~RunScreen()
{
    DeleteObject(m_hudFont);
    DeleteObject(m_timerFont);
    DeleteObject(m_bigFont);
}

void RunScreen::Enter()
{
    const Contract& c = m_data.chosen;

    m_map.Generate(MapSizeFor(c.enemies), m_data.rng);
    m_player.SetCharacter(m_data.character);
    m_player.Reset(m_map.StartX(), m_map.StartY());
    LoadCatPictures();
    m_run.Start(c, m_map, m_data.rng);
    m_renderer.SetNight(c.night);
    SpawnEnemies();

    m_messageTimer = 0.0f;
    m_hitFlash = 0.0f;
    m_alertTimer = 0.0f;
    m_shake = 0.0f;
    m_fireTimer = 0.0f;
    for (int i = 0; i < MAX_FUR_BALLS; ++i) m_furBalls[i].alive = false;
    m_boneActive = false;
    m_data.newContracts = true;   // whatever happens, the next list is a fresh one
}

void RunScreen::LoadCatPictures()
{
    if (m_loadedCharacter == (int)m_data.character) return;
    m_loadedCharacter = (int)m_data.character;

    // Any missing one is just skipped (the cat falls back to its back view).
    const CharacterInfo& info = GetCharacterInfo(m_data.character);
    m_catBack.Load(info.spriteBack);
    m_catBackSide.Load(info.spriteBackSide);
    m_catSide.Load(info.spriteSide);
}

void RunScreen::OnAbilityLanded()
{
    switch (m_player.GetAbility().Type())
    {
    case AbilityType::GroundPound:
    {
        // SLAM: every dog close by goes flying.
        for (int i = 0; i < m_enemyCount; ++i)
        {
            float dx = m_enemies[i].X() - m_player.X(), dy = m_enemies[i].Y() - m_player.Y();
            if (dx * dx + dy * dy <= GROUND_POUND_RADIUS * GROUND_POUND_RADIUS)
                m_enemies[i].Knockback(m_player.X(), m_player.Y());
        }
        m_shake = SLAM_SHAKE_TIME;
        break;
    }
    default:
        break;
    }
}

void RunScreen::ShootFurBall()
{
    for (int i = 0; i < MAX_FUR_BALLS; ++i)
    {
        FurBall& b = m_furBalls[i];
        if (b.alive) continue;
        // Straight ahead (the crosshair is dead center in both views).
        b.dx = m_player.DirX();
        b.dy = m_player.DirY();
        b.x = m_player.X() + b.dx * 0.3f;
        b.y = m_player.Y() + b.dy * 0.3f;
        b.travelled = 0.0f;
        b.alive = true;
        m_fireTimer = FUR_FIRE_INTERVAL;
        return;
    }
}

void RunScreen::UpdateFurBalls(float dt)
{
    // Small steps, so a fast ball can't skip through a dog or a wall.
    const float STEP = 0.15f;
    for (int i = 0; i < MAX_FUR_BALLS; ++i)
    {
        FurBall& b = m_furBalls[i];
        float remaining = FUR_SPEED * dt;
        while (b.alive && remaining > 0.0f)
        {
            float s = remaining < STEP ? remaining : STEP;
            remaining -= s;
            b.x += b.dx * s;
            b.y += b.dy * s;
            b.travelled += s;

            if (m_map.IsWall((int)b.x, (int)b.y) || b.travelled > FUR_RANGE) { b.alive = false; break; }

            for (int e = 0; e < m_enemyCount; ++e)
            {
                float dx = m_enemies[e].X() - b.x, dy = m_enemies[e].Y() - b.y;
                if (dx * dx + dy * dy < FUR_HIT_RADIUS * FUR_HIT_RADIUS)
                {
                    m_enemies[e].Stun(FUR_STUN_TIME);
                    b.alive = false;
                    break;
                }
            }
        }
    }
}

void RunScreen::SpawnEnemies()
{
    // Rotating mix; blue first so purple always has a blue to flank with.
    static const EnemyType CYCLE[3] = { EnemyType::Blue, EnemyType::Yellow, EnemyType::Purple };

    m_enemyCount = 0;
    int want = m_data.chosen.enemies;
    if (want > MAX_ENEMIES) want = MAX_ENEMIES;

    for (int i = 0; i < want; ++i)
    {
        float x = 0, y = 0;
        bool placed = false;

        for (int attempt = 0; attempt < 50 && !placed; ++attempt)
        {
            if (!m_map.RandomOpenTile(m_data.rng, m_map.StartX(), m_map.StartY(),
                ENEMY_SPAWN_DIST, x, y))
                break;

            placed = true;
            for (int j = 0; j < m_enemyCount; ++j)   // one enemy per tile
                if ((int)m_enemies[j].X() == (int)x && (int)m_enemies[j].Y() == (int)y)
                    placed = false;
        }
        if (!placed) continue;

        m_enemies[m_enemyCount++].Spawn(x, y, CYCLE[i % 3],
            m_map.StartX(), m_map.StartY());   // don't wander up to the start
    }
}

void RunScreen::CheckBites()
{
    for (int i = 0; i < m_enemyCount; ++i)
    {
        Enemy& e = m_enemies[i];
        float dx = e.X() - m_player.X();
        float dy = e.Y() - m_player.Y();
        if (dx * dx + dy * dy >= ENEMY_CATCH_RANGE * ENEMY_CATCH_RANGE) continue;
        if (!e.CanBite()) continue;

        // TakeHit fails while you're still invincible, so a pack of dogs
        // only costs one hit point per invincibility window.
        if (m_player.TakeHit())
        {
            e.OnBite();
            m_hitFlash = 0.35f;
            ShowMessage(m_player.Hp() > 0 ? L"OUCH! A DOG GOT YOU!" : L"", RGB(255, 80, 80));
        }
    }
}

void RunScreen::UseHeldItem()
{
    ItemType item = m_data.heldItem;
    if (item == ItemType::None) return;

    const ItemInfo& info = GetItemInfo(item);
    if (!UseItem(item, m_player, *this))
    {
        ShowMessage(info.failMessage, RGB(200, 200, 200));   // kept for later
        return;
    }

    m_data.heldItem = ItemType::None;
    DWORD c = info.color;   // 0xRRGGBB -> COLORREF
    ShowMessage(info.usedMessage, RGB((c >> 16) & 255, (c >> 8) & 255, c & 255));
}

void RunScreen::DropBone(float x, float y)
{
    m_boneActive = true;
    m_boneX = x;
    m_boneY = y;
    m_boneTimer = BONE_LURE_TIME;

    // Every dog close enough smells it - even the ones chasing you.
    for (int i = 0; i < m_enemyCount; ++i)
    {
        float dx = m_enemies[i].X() - x, dy = m_enemies[i].Y() - y;
        if (dx * dx + dy * dy <= BONE_LURE_RANGE * BONE_LURE_RANGE)
            m_enemies[i].Lure(x, y, BONE_LURE_TIME);
    }
}

void RunScreen::ShowMessage(const wchar_t* text, COLORREF color)
{
    lstrcpynW(m_message, text, 64);
    m_messageColor = color;
    m_messageTimer = MESSAGE_TIME;
}

GameState RunScreen::Update(float dt, const Input& input)
{
    m_time += dt;
    if (m_messageTimer > 0.0f) m_messageTimer -= dt;
    if (m_hitFlash > 0.0f)     m_hitFlash -= dt;

    // Finished: frozen until ENTER.
    if (m_run.Finished())
    {
        if (input.WasPressed(VK_RETURN) || input.WasPressed(VK_SPACE))
            return GameState::DeliverySelect;
        return GameState::Run;
    }

    // ESC abandons the delivery (no pay).
    if (input.WasPressed(VK_ESCAPE))
        return GameState::DeliverySelect;

    if (input.WasPressed('V'))
        m_thirdPerson = !m_thirdPerson;

    if (input.WasPressed(ITEM_USE_KEY))
        UseHeldItem();
    if (input.WasPressed(ABILITY_KEY) && m_player.UseAbility() &&
        m_player.GetAbility().Type() == AbilityType::FurBarrage)
        ShowMessage(L"FUR BARRAGE!  CLICK / SPACE TO SHOOT", RGB(220, 220, 220));

    // Fur barrage: one fur ball per click (or SPACE) while it lasts.
    if (m_fireTimer > 0.0f) m_fireTimer -= dt;
    if (m_player.GetAbility().Type() == AbilityType::FurBarrage && m_player.GetAbility().IsActive() &&
        m_fireTimer <= 0.0f && (input.WasPressed(VK_LBUTTON) || input.WasPressed(VK_SPACE)))
        ShootFurBall();
    UpdateFurBalls(dt);

    m_player.Update(dt, input, m_map);
    if (m_player.GetAbility().JustLanded())
        OnAbilityLanded();
    if (m_shake > 0.0f) m_shake -= dt;

    if (m_alertTimer > 0.0f) m_alertTimer -= dt;
    if (m_boneActive && (m_boneTimer -= dt) <= 0.0f) m_boneActive = false;
    for (int i = 0; i < m_enemyCount; ++i)
    {
        m_enemies[i].Update(dt, m_map, m_player, m_enemies, m_enemyCount);
        if (m_enemies[i].JustSpotted()) m_alertTimer = SPOTTED_ALERT_TIME;
    }

    CheckBites();
    if (m_player.Hp() <= 0)
        return GameState::GameOver;

    int events = m_run.Update(dt, m_player.X(), m_player.Y());

    if (events & DeliveryRun::EVENT_EXPRESS_EXPIRED)
        ShowMessage(L"EXPRESS EXPIRED - BONUS LOST", RGB(255, 70, 70));

    if (events & DeliveryRun::EVENT_EXPRESS_DELIVERED)
    {
        wchar_t buf[64];
        wsprintfW(buf, L"EXPRESS DELIVERED!  +$%d", m_data.chosen.expressBonus);
        ShowMessage(buf, RGB(0, 230, 255));
    }
    else if (events & DeliveryRun::EVENT_ALL_DELIVERED)
        ShowMessage(L"ALL DELIVERED! HEAD BACK TO START", RGB(80, 255, 80));
    else if (events & DeliveryRun::EVENT_STOP_DELIVERED)
        ShowMessage(L"PACKAGE DELIVERED!", RGB(255, 200, 80));

    if (events & DeliveryRun::EVENT_FINISHED)
        m_data.money += m_run.Payout();

    return GameState::Run;
}

void RunScreen::Render(Frame& frame)
{
    float cameraDistance;
    Camera cam = MakeCamera(m_player, m_map, m_thirdPerson, cameraDistance);

    m_renderer.DrawWorld(frame.pixels, m_map, cam);

    float pulse = 0.5f + 0.5f * sinf(m_time * 3.0f);

    // Markers: orange stops, blinking cyan express stop, green start once done.
    if (!m_run.AllDelivered())
    {
        for (int i = 0; i < m_run.StopCount(); ++i)
        {
            const Stop& s = m_run.GetStop(i);
            if (s.delivered) continue;
            DWORD color = 0xFF8C00;
            if (s.express) color = ExpressBlinkOn(s.timeLeft) ? 0x00E6FF : 0x005064;
            m_renderer.DrawMarker(frame.pixels, cam, s.x, s.y, color, pulse);
        }
    }
    else if (!m_run.Finished())
    {
        m_renderer.DrawMarker(frame.pixels, cam, m_run.StartX(), m_run.StartY(), 0x50FF50, pulse);
    }

    DrawActors(frame, cam, cameraDistance);

    // First person ground pound: the world drops away as you jump.
    // Landing: a short, sharp shake.
    int shift = 0;
    if (!m_thirdPerson) shift += (int)(m_player.GetAbility().JumpHeight() * frame.height * 0.6f);
    if (m_shake > 0.0f) shift += (int)(sinf(m_time * 90.0f) * 6.0f * (m_shake / SLAM_SHAKE_TIME));
    ShiftFrameY(frame, shift);

    m_renderer.DrawCrosshair(frame.pixels);
    if (m_alertTimer > 0.0f && !m_run.Finished()) DrawSpottedAlert(frame, cameraDistance);

    // Red flash right after a bite.
    if (m_hitFlash > 0.0f)
    {
        int a = (int)(m_hitFlash / 0.35f * 110);
        for (int i = 0; i < frame.width * frame.height; ++i)
        {
            DWORD c = frame.pixels[i];
            int r = (int)((c >> 16) & 255), g = (int)((c >> 8) & 255), b = (int)(c & 255);
            r += (230 - r) * a / 255;
            g -= g * a / 255;
            b -= b * a / 255;
            frame.pixels[i] = (DWORD)(r << 16 | g << 8 | b);
        }
    }

    RECT minimap = m_renderer.DrawMinimap(frame.pixels, m_map, m_player, m_run,
        m_enemies, m_enemyCount, m_time);

    if (m_run.Finished()) DrawFinished(frame);
    else                  DrawHud(frame, minimap);
}

// Everything that stands in the world as a sprite - props (trees, bushes,
// flowers, lamps), dogs and (in third person) the cat - drawn far to near
// so whatever is closer covers whatever is behind it.
void RunScreen::DrawActors(Frame& frame, const Camera& cam, float cameraDistance)
{
    struct Item
    {
        float dist;    // squared distance from the camera
        int   kind;    // KIND_*
        int   index;   // enemy index
        int   tile;    // prop tile type
        float x, y;
    };
    enum { KIND_PROP, KIND_DOG, KIND_CAT, KIND_PACKAGE, KIND_BONE, KIND_FUR };

    static const int MAX_ITEMS = 640;
    static Item items[MAX_ITEMS];
    int count = 0;

    // Props near the camera (farther ones are lost in the haze anyway).
    const int RANGE = 16;
    int cx = (int)cam.x, cy = (int)cam.y;
    for (int ty = cy - RANGE; ty <= cy + RANGE; ++ty)
        for (int tx = cx - RANGE; tx <= cx + RANGE && count < MAX_ITEMS; ++tx)
        {
            TileType t = m_map.TileAt(tx, ty);
            if (!Map::IsProp(t)) continue;
            Item& it = items[count++];
            it.x = tx + 0.5f;  it.y = ty + 0.5f;
            it.kind = KIND_PROP; it.tile = t; it.index = 0;
            float dx = it.x - cam.x, dy = it.y - cam.y;
            it.dist = dx * dx + dy * dy;
        }

    // Packages left on the doorsteps you've delivered to.
    if (m_package.IsLoaded())
        for (int i = 0; i < m_run.StopCount() && count < MAX_ITEMS; ++i)
        {
            const Stop& s = m_run.GetStop(i);
            if (!s.delivered) continue;
            Item& it = items[count++];
            it.x = s.x; it.y = s.y;
            it.kind = KIND_PACKAGE; it.index = i; it.tile = 0;
            float dx = it.x - cam.x, dy = it.y - cam.y;
            it.dist = dx * dx + dy * dy;
        }

    for (int i = 0; i < MAX_FUR_BALLS && count < MAX_ITEMS; ++i)
    {
        if (!m_furBalls[i].alive) continue;
        Item& it = items[count++];
        it.x = m_furBalls[i].x; it.y = m_furBalls[i].y;
        it.kind = KIND_FUR; it.index = i; it.tile = 0;
        float dx = it.x - cam.x, dy = it.y - cam.y;
        it.dist = dx * dx + dy * dy;
    }

    if (m_boneActive && count < MAX_ITEMS)
    {
        Item& it = items[count++];
        it.x = m_boneX; it.y = m_boneY;
        it.kind = KIND_BONE; it.index = 0; it.tile = 0;
        float dx = it.x - cam.x, dy = it.y - cam.y;
        it.dist = dx * dx + dy * dy;
    }

    for (int i = 0; i < m_enemyCount && count < MAX_ITEMS; ++i)
    {
        Item& it = items[count++];
        it.x = m_enemies[i].X(); it.y = m_enemies[i].Y();
        it.kind = KIND_DOG; it.index = i; it.tile = 0;
        float dx = it.x - cam.x, dy = it.y - cam.y;
        it.dist = dx * dx + dy * dy;
    }

    // The cat fades out when a wall pushes the camera in close,
    // so it never fills the screen.
    float catFade = 0.0f;
    if (m_thirdPerson && count < MAX_ITEMS)
    {
        catFade = (cameraDistance - 0.4f) / 0.6f;
        if (catFade > 1.0f) catFade = 1.0f;

        // Blink while invincible after a bite.
        if (m_player.IsInvulnerable() && ((int)(m_time * 12.0f) % 2)) catFade *= 0.25f;

        if (catFade > 0.0f)
        {
            Item& it = items[count++];
            it.x = m_player.X(); it.y = m_player.Y();
            it.kind = KIND_CAT; it.index = 0; it.tile = 0;
            float dx = it.x - cam.x, dy = it.y - cam.y;
            it.dist = dx * dx + dy * dy;
        }
    }

    // Insertion sort, farthest first.
    for (int i = 1; i < count; ++i)
    {
        Item k = items[i];
        int j = i - 1;
        while (j >= 0 && items[j].dist < k.dist) { items[j + 1] = items[j]; --j; }
        items[j + 1] = k;
    }

    for (int n = 0; n < count; ++n)
    {
        const Item& it = items[n];
        float light = m_renderer.IsNight() ? m_map.LightAt(it.x, it.y) : 0.0f;

        if (it.kind == KIND_PROP)
        {
            // Walk-through props fade when you're right inside them.
            float fade = 1.0f;
            if (it.tile == TILE_BUSH || it.tile == TILE_FLOWERS || it.tile == TILE_LAMP || it.tile == TILE_TREE)
            {
                float d = sqrtf(it.dist);
                fade = (d - 0.25f) / 0.45f;
                if (fade > 1.0f) fade = 1.0f;
            }
            m_renderer.DrawSprite(frame.pixels, cam, it.x, it.y, m_renderer.PropTexture(it.tile),
                m_renderer.PropScale(it.tile), false, fade, light);

            if (it.tile == TILE_LAMP && m_renderer.IsNight())
                m_renderer.DrawGlow(frame.pixels, cam, it.x, it.y, 1.07f, 0.32f, 0xFFD890);
        }
        else if (it.kind == KIND_FUR)
        {
            m_renderer.DrawSprite(frame.pixels, cam, it.x, it.y, m_furBall, FUR_SCALE, false, 1.0f, light, 0.18f);
        }
        else if (it.kind == KIND_BONE)
        {
            // Blinks for its last second, then it's gone.
            if (m_boneTimer < 1.0f && ((int)(m_boneTimer * 10.0f) % 2)) continue;
            m_renderer.DrawSprite(frame.pixels, cam, it.x, it.y, m_itemIcons[(int)ItemType::Bone],
                BONE_SCALE, false, 1.0f, light);
        }
        else if (it.kind == KIND_PACKAGE)
        {
            m_renderer.DrawSprite(frame.pixels, cam, it.x, it.y, m_package,
                PACKAGE_SCALE, false, 1.0f, light);
        }
        else if (it.kind == KIND_DOG)
        {
            const Enemy& e = m_enemies[it.index];
            const Texture& pic = m_dogs[(int)e.Type()];

            float headTop;   // dog height, in wall heights
            if (pic.IsLoaded())
            {
                // Flip so the dog faces the way it's running across the screen
                // (the camera plane points to screen-right).
                float across = e.MoveX() * cam.planeX + e.MoveY() * cam.planeY;
                bool mirror = DOG_SPRITE_FACES_RIGHT ? (across < 0.0f) : (across > 0.0f);
                headTop = DOG_SPRITE_SCALES[(int)e.Type()];
                m_renderer.DrawSprite(frame.pixels, cam, e.X(), e.Y(), pic, headTop, mirror, 1.0f, light);
            }
            else
            {
                DWORD color = (e.Type() == EnemyType::Yellow) ? 0xF0D020
                    : (e.Type() == EnemyType::Purple) ? 0xA040E0
                    : 0x3070F0;
                m_renderer.DrawEnemy(frame.pixels, cam, e.X(), e.Y(), color);
                headTop = 0.75f;
            }

            // Seeing stars: a spinning ring of them around its head.
            if (e.IsStunned())
            {
                int phase = (int)(m_time * 9.0f) % 3;
                m_renderer.DrawSprite(frame.pixels, cam, e.X(), e.Y(), m_dizzy[phase],
                    0.16f, false, 1.0f, light, headTop - 0.09f);
            }

            // In love with the bone: a heart bobbing over its head.
            if (e.IsLured())
            {
                float bob = 0.04f * sinf(m_time * 6.0f + it.index);
                m_renderer.DrawSprite(frame.pixels, cam, e.X(), e.Y(), m_heart,
                    HEART_SCALE, false, 1.0f, light, headTop + 0.04f + bob);
            }
        }
        else
        {
            // Pick the picture from the movement keys:
            //   strafe only         -> side
            //   forward/back + side -> back-side (diagonal)
            //   otherwise           -> back
            // The side pictures face right, so flip them when going left.
            bool strafing = m_player.StrafeInput() != 0.0f;
            bool forward = m_player.ForwardInput() != 0.0f;
            bool mirror = m_player.StrafeInput() < 0.0f;

            const Texture* pic = &m_catBack;
            if (strafing && forward && m_catBackSide.IsLoaded()) pic = &m_catBackSide;
            else if (strafing && m_catSide.IsLoaded())          pic = &m_catSide;
            if (pic == &m_catBack) mirror = false;

            // Ground pound: up in the air, with a shadow left on the ground.
            float jump = m_player.GetAbility().JumpHeight();
            if (jump > 0.0f)
                m_renderer.DrawSprite(frame.pixels, cam, m_player.X(), m_player.Y(), m_shadow,
                    0.05f, false, catFade * (1.0f - jump / GROUND_POUND_HEIGHT * 0.5f), light);

            m_renderer.DrawSprite(frame.pixels, cam, m_player.X(), m_player.Y(),
                *pic, CAT_SPRITE_SCALE, mirror, catFade, light, jump);
        }
    }
}

// Layout (see the sketch):
//   top center   mission bar (packages delivered), express timer under it
//   top right    minimap, legend under it
//   bottom left  HP bar (one segment per hit point) + stamina bar
//   bottom right item slot + ability slot (ability comes later)
//   bottom mid   messages (bites, out of breath, deliveries)
// The red "!" over the cat's head (or above the crosshair in first person).
// Pops in big, settles, and blinks out at the end.
void RunScreen::DrawSpottedAlert(Frame& frame, float cameraDistance)
{
    float age = SPOTTED_ALERT_TIME - m_alertTimer;
    if (m_alertTimer < 0.35f && ((int)(m_alertTimer * 16.0f) % 2)) return;

    float pop = (age < 0.15f) ? 1.6f - 0.6f * (age / 0.15f) : 1.0f;
    int w = (int)(7 * 3 * pop), h = (int)(14 * 3 * pop);

    // Third person: the cat stands straight ahead, its head at this height.
    int bottom = frame.height / 2 - 30;
    if (m_thirdPerson && cameraDistance > 0.4f)
        bottom = (int)(frame.height / 2 + (0.5f - CAT_SPRITE_SCALE) * frame.height / cameraDistance) - 6;

    DrawIcon(frame, m_alertIcon, (frame.width - w) / 2, bottom - h, w, h);
}

void RunScreen::DrawHud(Frame& frame, const RECT& minimap)
{
    const int W = frame.width, H = frame.height;

    // ===== Pixels first =====

    // Mission bar
    RECT mission = { (W - 260) / 2, 10, (W + 260) / 2, 28 };
    int stops = m_run.StopCount();
    bool backToStart = m_run.AllDelivered();
    int missionFill = backToStart ? mission.right
        : mission.left + (mission.right - mission.left) * m_run.DeliveredCount() / (stops > 0 ? stops : 1);
    DrawPill(frame, mission, HUD_EMPTY, backToStart ? 0x5CC85C : 0xF08C3C, missionFill);
    if (!backToStart && stops > 1) PillDividers(frame, mission, stops);

    // HP: one segment per hit point
    RECT hp = { 40, H - 46, 190, H - 30 };
    int hpFill = hp.left + (hp.right - hp.left) * m_player.Hp() / PLAYER_MAX_HP;
    bool blink = m_player.IsInvulnerable() && ((int)(m_time * 12.0f) % 2);
    DrawPill(frame, hp, HUD_EMPTY, blink ? 0xFFB0B0 : 0xE04848, hpFill);
    PillDividers(frame, hp, PLAYER_MAX_HP);

    // Stamina
    const Stamina& stamina = m_player.GetStamina();
    RECT run = { 40, H - 24, 170, H - 12 };
    DWORD runColor = stamina.IsExhausted() ? 0xE05A3C
        : m_player.IsSprinting() ? 0xA8F088
        : 0x6CC24A;
    DrawPill(frame, run, HUD_EMPTY, runColor, run.left + (int)((run.right - run.left) * stamina.Fraction()));

    // Item + ability slots
    const int R = 20;
    RECT ability = { W - 14 - 2 * R, H - 14 - 2 * R, W - 14, H - 14 };
    RECT item = ability;
    OffsetRect(&item, -(2 * R + 10), 0);
    DrawPill(frame, item, HUD_CREAM, HUD_CREAM, item.left);
    // Ability: icon, with a dark shade that drains from the top as it recharges.
    const Ability& ab = m_player.GetAbility();
    DrawPill(frame, ability, HUD_CREAM, HUD_CREAM, ability.left);
    DrawIcon(frame, m_abilityIcons[(int)ab.Type()], ability.left + R - 14, ability.top + R - 7, 28, 14);
    float cd = ab.CooldownFraction();
    if (ab.IsActive())
    {
        // Lasting ability: a green glow that drains as time runs out.
        int glowFrom = ability.bottom - (int)((ability.bottom - ability.top) * ab.ActiveFraction());
        for (int y = glowFrom; y < ability.bottom; ++y)
            for (int x = ability.left; x < ability.right; ++x)
                if (PillDist(ability, x + 0.5f, y + 0.5f) < -2.0f)
                {
                    DWORD& p = frame.pixels[y * W + x];
                    p = ((p >> 1) & 0x7F7F7F) + 0x205A20;   // half color + green
                }
    }
    else if (cd > 0.0f || ab.IsAirborne())
    {
        int shadeTo = ab.IsAirborne() ? ability.bottom : ability.bottom - (int)((ability.bottom - ability.top) * (1.0f - cd));
        for (int y = ability.top; y < shadeTo; ++y)
            for (int x = ability.left; x < ability.right; ++x)
                if (PillDist(ability, x + 0.5f, y + 0.5f) < -2.0f)
                {
                    DWORD& p = frame.pixels[y * W + x];
                    p = (p >> 1) & 0x7F7F7F;   // half as bright
                }
    }

    if (m_data.heldItem != ItemType::None)
        DrawIcon(frame, m_itemIcons[(int)m_data.heldItem], item.left + R - 14, item.top + R - 13, 28, 26);

    // ===== Text =====
    HDC dc = frame.dc;
    SetBkMode(dc, TRANSPARENT);
    HGDIOBJ oldFont = SelectObject(dc, m_hudFont);
    const Contract& c = m_data.chosen;
    wchar_t buf[128];

    if (backToStart) lstrcpyW(buf, L"BACK TO START");
    else             wsprintfW(buf, L"DELIVERED %d / %d", m_run.DeliveredCount(), stops);
    ShadowText(dc, buf, mission, DT_CENTER | DT_VCENTER | DT_SINGLELINE, RGB(255, 255, 255));

    // Express countdown under the mission bar
    if (const Stop* ex = m_run.ActiveExpress())
    {
        bool urgent = ex->timeLeft < EXPRESS_URGENT;
        wsprintfW(buf, L"EXPRESS  %ds", (int)ceilf(ex->timeLeft));
        SelectObject(dc, m_timerFont);
        RECT timer = { 0, mission.bottom + 4, W, mission.bottom + 32 };
        ShadowText(dc, buf, timer, DT_CENTER | DT_SINGLELINE,
            (urgent && ExpressBlinkOn(ex->timeLeft)) ? RGB(255, 60, 60) : RGB(0, 230, 255));
        SelectObject(dc, m_hudFont);
    }

    // Legend under the minimap
    RECT leg = { minimap.left, minimap.bottom + 4, W, minimap.bottom + 18 };
    ShadowText(dc, L"YOU", leg, DT_LEFT | DT_SINGLELINE, RGB(255, 255, 255)); OffsetRect(&leg, 0, 13);
    ShadowText(dc, L"START", leg, DT_LEFT | DT_SINGLELINE, RGB(80, 255, 80)); OffsetRect(&leg, 0, 13);
    ShadowText(dc, L"DROP-OFF", leg, DT_LEFT | DT_SINGLELINE, RGB(255, 140, 0)); OffsetRect(&leg, 0, 13);
    ShadowText(dc, L"DOGS", leg, DT_LEFT | DT_SINGLELINE, RGB(255, 60, 60));
    if (c.express)
    {
        OffsetRect(&leg, 0, 13);
        ShadowText(dc, L"EXPRESS", leg, DT_LEFT | DT_SINGLELINE, RGB(0, 230, 255));
    }

    // Bar labels
    RECT hpLabel = { 10, hp.top,  hp.left - 4,  hp.bottom };
    RECT runLabel = { 6,  run.top - 2, run.left - 4, run.bottom + 2 };
    ShadowText(dc, L"HP", hpLabel, DT_RIGHT | DT_VCENTER | DT_SINGLELINE, RGB(255, 255, 255));
    ShadowText(dc, L"RUN", runLabel, DT_RIGHT | DT_VCENTER | DT_SINGLELINE, RGB(255, 255, 255));

    // Key hint on the item slot
    if (m_data.heldItem != ItemType::None)
    {
        RECT key = { item.right - 10, item.bottom - 12, item.right + 4, item.bottom + 2 };
        ShadowText(dc, L"Q", key, DT_CENTER | DT_SINGLELINE, RGB(255, 255, 255));
    }

    // Key hint on the ability slot (dim while recharging)
    {
        RECT key = { ability.right - 10, ability.bottom - 12, ability.right + 4, ability.bottom + 2 };
        ShadowText(dc, L"E", key, DT_CENTER | DT_SINGLELINE, ab.IsReady() ? RGB(255, 255, 255) : RGB(120, 120, 120));
    }

    // Messages, bottom middle: the latest message, and "out of breath" under it
    int msgY = H - 58;
    if (m_messageTimer > 0.0f && m_message[0])
    {
        RECT msg = { 0, msgY, W, msgY + 18 };
        ShadowText(dc, m_message, msg, DT_CENTER | DT_SINGLELINE, m_messageColor);
    }
    if (stamina.IsExhausted())
    {
        RECT breath = { 0, msgY + 18, W, msgY + 36 };
        ShadowText(dc, L"OUT OF BREATH", breath, DT_CENTER | DT_SINGLELINE, RGB(255, 90, 60));
    }

    SelectObject(dc, oldFont);
}

void RunScreen::DrawFinished(Frame& frame)
{
    DimFrame(frame);

    HDC dc = frame.dc;
    SetBkMode(dc, TRANSPARENT);
    HGDIOBJ oldFont = SelectObject(dc, m_bigFont);
    const Contract& c = m_data.chosen;
    wchar_t buf[96];

    RECT title = { 0, 90, frame.width, 145 };
    ShadowText(dc, L"DELIVERED!", title, DT_CENTER | DT_SINGLELINE, TEXT_GREEN);

    SelectObject(dc, m_timerFont);
    RECT line = { 0, 160, frame.width, 190 };
    wsprintfW(buf, L"Reward  $%d", c.reward);
    ShadowText(dc, buf, line, DT_CENTER | DT_SINGLELINE, RGB(255, 220, 0));

    if (m_run.ExpressEarned())
    {
        OffsetRect(&line, 0, 30);
        wsprintfW(buf, L"Express bonus  +$%d", c.expressBonus);
        ShadowText(dc, buf, line, DT_CENTER | DT_SINGLELINE, RGB(0, 230, 255));
    }

    OffsetRect(&line, 0, 40);
    wsprintfW(buf, L"Money: $%d", m_data.money);
    ShadowText(dc, buf, line, DT_CENTER | DT_SINGLELINE, RGB(255, 255, 255));

    SelectObject(dc, m_hudFont);
    RECT hint = { 0, 330, frame.width, 350 };
    ShadowText(dc, L"ENTER: next delivery", hint, DT_CENTER | DT_SINGLELINE, RGB(200, 200, 200));

    SelectObject(dc, oldFont);
}


// =====================================================
// GAME OVER
// =====================================================

// Seconds the screen ignores input, so keys mashed while
// running from the dogs don't skip it by accident.
static const float GAME_OVER_INPUT_DELAY = 0.8f;

GameOverScreen::GameOverScreen() : m_timer(0.0f)
{
    m_titleFont = MakeFont(52);
    m_textFont = MakeFont(18);
}

GameOverScreen::~GameOverScreen()
{
    DeleteObject(m_titleFont);
    DeleteObject(m_textFont);
}

void GameOverScreen::Enter()
{
    m_timer = 0.0f;
}

GameState GameOverScreen::Update(float dt, const Input& input)
{
    m_timer += dt;
    if (m_timer < GAME_OVER_INPUT_DELAY) return GameState::GameOver;

    if (input.WasPressed(VK_RETURN) || input.WasPressed(VK_SPACE)) return GameState::DeliverySelect;
    if (input.WasPressed(VK_ESCAPE)) return GameState::MainMenu;
    return GameState::GameOver;
}

void GameOverScreen::Render(Frame& frame)
{
    for (int i = 0; i < frame.width * frame.height; ++i) frame.pixels[i] = 0x280000;

    HDC dc = frame.dc;
    SetBkMode(dc, TRANSPARENT);

    HGDIOBJ oldFont = SelectObject(dc, m_titleFont);
    RECT title = { 0, 110, frame.width, 175 };
    ShadowText(dc, L"YOU GOT CAUGHT", title, DT_CENTER | DT_SINGLELINE, RGB(255, 60, 60));

    SelectObject(dc, m_textFont);
    RECT sub = { 0, 190, frame.width, 215 };
    ShadowText(dc, L"The package never made it.", sub, DT_CENTER | DT_SINGLELINE, RGB(220, 180, 180));

    if (m_timer >= GAME_OVER_INPUT_DELAY)
    {
        RECT hint = { 0, 260, frame.width, 285 };
        ShadowText(dc, L"ENTER: back to deliveries     ESC: main menu", hint,
            DT_CENTER | DT_SINGLELINE, RGB(170, 130, 130));
    }

    SelectObject(dc, oldFont);
}