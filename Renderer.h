#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "Config.h"

class Map;
class Player;
enum class TimeOfDay;
class Enemy;
class DeliveryRun;


// =====================================================
// An image loaded from a PNG in the assets folder.
// Pixels are 0x00RRGGBB (same as the framebuffer), with a
// separate alpha byte per pixel for transparency.
// =====================================================
class Texture
{
public:
    Texture();
    ~Texture();

    // Looks in "assets\" next to the working folder or the .exe
    // (and up to two folders above the .exe). False if not found.
    bool Load(const wchar_t* fileName);

    bool IsLoaded() const { return m_pixels != nullptr; }
    void Clear() { Free(); }   // back to "no picture"

    // For pictures made in code: make an empty (transparent) image,
    // set pixels, then call FinishEdits().
    void Create(int width, int height);
    void SetPixel(int x, int y, DWORD color, int alpha = 255);
    void FinishEdits();

    // Size of the visible part (fully transparent edges are trimmed,
    // so empty space in the PNG doesn't shrink the sprite or lift it
    // off the floor).
    int VisibleWidth()  const { return m_x1 - m_x0; }
    int VisibleHeight() const { return m_y1 - m_y0; }

    // u, v: 0..1 across the visible part. Returns color, puts alpha (0..255) in outAlpha.
    DWORD Sample(float u, float v, int& outAlpha) const;

private:
    Texture(const Texture&);              // not copyable
    Texture& operator=(const Texture&);

    bool LoadFile(const wchar_t* path);
    void Free();
    void ComputeBounds();

    int            m_width, m_height;
    DWORD* m_pixels;
    unsigned char* m_alpha;
    int            m_x0, m_y0, m_x1, m_y1;   // visible box
};


// =====================================================
// How a sprite is bent for the walk animation (fake frames
// made from one picture).
// =====================================================
struct SpritePose
{
    float stretchX = 1.0f;   // wider (>1) / thinner (<1)
    float stretchY = 1.0f;   // taller (>1) / shorter (<1)
    float lean = 0.0f;   // top slides sideways by this much of the width (+ right)
};


// Fakes a walk cycle from one picture: hops, squashes on landing,
// stretches on the way up and leans side to side every step.
// Steps follow distance moved, so running = quicker steps.
class WalkCycle
{
public:
    // moved: distance this frame (tiles). canWalk: false while flying,
    // stunned... (the animation settles down smoothly).
    void Update(float dt, float moved, float stepsPerTile, bool canWalk);

    // size: hop height / squash multiplier (1 = normal walk, 3 = big bouncy hops)
    float      Lift(float size = 1.0f) const;   // hop height, in wall heights
    SpritePose Pose(float size = 1.0f) const;
    bool       OddStep() const;    // flips every step (mirror the back view = waddle)
    int        StepCount() const;  // goes up by one each time a foot lands
    bool       IsWalking() const { return m_amount > 0.5f; }

private:
    float m_phase = 0.0f;   // one step = pi
    float m_amount = 0.0f;   // 0 standing .. 1 walking
};


// =====================================================
// Where the 3D view is drawn from. First person: the cat's eyes.
// Third person: pulled back behind the cat.
// (Same getters as Player so drawing code reads the same.)
// =====================================================
struct Camera
{
    float x, y;
    float dirX, dirY;
    float planeX, planeY;

    float X() const { return x; }
    float Y() const { return y; }
    float DirX() const { return dirX; }
    float DirY() const { return dirY; }
    float PlaneX() const { return planeX; }
    float PlaneY() const { return planeY; }
};

// Builds the camera for this frame. In third person it backs away from the
// player, stopping before walls; outDistance = how far it got (0 in first person).
Camera MakeCamera(const Player& player, const Map& map, bool thirdPerson, float& outDistance);


// Draws the 3D view. Call DrawWorld first each frame (it fills the
// depth buffer), then markers / sprites, then the crosshair on top.
// pixels: SCREEN_W x SCREEN_H, 0x00RRGGBB
class Renderer
{
public:
    Renderer();   // draws the prop pictures (trees, bushes, ...)
    ~Renderer();

    // Time of day + a seed: paints a new sky (with skyline) and sets the
    // haze and light. Night also means lit windows and street lights on.
    void SetSky(TimeOfDay time, unsigned seed);
    bool IsNight() const { return m_night; }

    // Moves the clouds along. Call once per frame.
    void AdvanceSky(float dt);

    void DrawWorld(DWORD* pixels, const Map& map, const Camera& cam);

    // A glowing pillar standing on the floor at (worldX, worldY).
    // color: 0x00RRGGBB. pulse: 0..1, makes it breathe.
    void DrawMarker(DWORD* pixels, const Camera& cam,
        float worldX, float worldY, DWORD color, float pulse);

    // A placeholder enemy figure standing on the floor at (worldX, worldY).
    void DrawEnemy(DWORD* pixels, const Camera& cam,
        float worldX, float worldY, DWORD color);

    // A picture standing on the floor at (worldX, worldY).
    // scale: height compared to a wall (0.5 = half a wall).
    // mirror: flip left-right. fade: 0 invisible .. 1 solid.
    // light: street-light brightness there (0..1), tints it warm.
    // lift: float it this far above the floor, in wall heights (hearts over dogs).
    // pose: squash/stretch and lean for the walk animation (nullptr = none).
    void DrawSprite(DWORD* pixels, const Camera& cam, float worldX, float worldY,
        const Texture& texture, float scale, bool mirror, float fade,
        float light = 0.0f, float lift = 0.0f, const SpritePose* pose = nullptr);

    // A soft glowing ball (the head of a street lamp).
    // height: how high above the floor, in wall heights. radius: same units.
    void DrawGlow(DWORD* pixels, const Camera& cam, float worldX, float worldY,
        float height, float radius, DWORD color);

    // Pictures and sizes for map props (bush, flowers, tree, lamp).
    const Texture& PropTexture(int tileType) const;
    float          PropScale(int tileType) const;   // height in wall heights

    void DrawCrosshair(DWORD* pixels);

    // Top-right map overview: walls, start, stops, enemies, you.
    // Returns where it was drawn (so a legend can go under it).
    RECT DrawMinimap(DWORD* pixels, const Map& map, const Player& player,
        const DeliveryRun& run, const Enemy* enemies, int enemyCount,
        float time);

private:
    Renderer(const Renderer&);              // not copyable (owns the sky)
    Renderer& operator=(const Renderer&);

    // Roofs in one screen column: marches the ray on from 'startDist' (where it
    // hit a house wall whose top is at row 'wallTop') and paints every bit of
    // roof that rises above what's already drawn, near to far.
    void DrawRoofColumn(DWORD* pixels, int x, const Map& map, const Camera& cam,
        float rayX, float rayY, float startDist, float wallTop);

    Texture m_tree, m_bush, m_flowers, m_lampOn, m_lampOff;

    // Sky panorama layers (SKY_PANO_W x SCREEN_H/2 each), see PaintSky.
    DWORD* m_skyBase;
    DWORD* m_skyClouds;
    DWORD* m_skyline;
    float  m_cloudOffset;   // pixels the clouds have drifted

    bool  m_night;
    DWORD m_hazeColor;   // what far things fade into
    float m_hazeDist;    // how far you can see

    // Distance to the wall in each screen column, so things
    // behind walls can be hidden.
    float m_depth[SCREEN_W];

    // Nearest low wall (hedge) in each column: things farther away than it
    // are hidden below its top edge.
    float m_lowDepth[SCREEN_W];
    int   m_lowTop[SCREEN_W];

    bool HiddenByHedge(int x, int y, float depth) const
    {
        return depth > m_lowDepth[x] && y >= m_lowTop[x];
    }
};