#pragma once

// =====================================================================
//  AI-GENERATED ART
//
//  Everything in AItexture.h / AItexture.cpp was written by an AI (Claude).
//  These are the textures, colors and sprites the game draws in code
//  instead of loading from image files:
//
//    - walls:   houses (siding, roof trim, windows, front doors),
//               hedges, wooden fences
//    - floors:  asphalt with dashed center lines, concrete paths, grass
//    - sky:     time-of-day gradients, sun, moon, stars, clouds,
//               and the city skyline on the horizon
//    - props:   tree, bush, flower patch, street lamp
//    - Asia theme: timber + plaster houses, curved tile roofs, stone
//               streets, gravel paths, mountains and mist, cherry trees,
//               azaleas, hydrangeas, paper lanterns
//    - dogs:    the placeholder "angry blob" until real dog art exists
//    - UI:      the heart icon, the minimap colors
//
//  Hand-drawn art by the developer (the cat: PlayerBack.png,
//  PlayerBackSide.png, PlayerSide.png) is loaded from the assets
//  folder and is NOT part of this file.
//
//  To replace any of this with your own art, swap the function for one
//  that samples your image instead; nothing else has to change.
// =====================================================================

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "World.h"

class Texture;


// ----- Small color helpers used by the art (and the renderer) -----
inline unsigned Hash(int x, int y)
{
    unsigned h = (unsigned)x * 374761393u + (unsigned)y * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

inline DWORD Rgb(int r, int g, int b)
{
    r = r < 0 ? 0 : (r > 255 ? 255 : r);
    g = g < 0 ? 0 : (g > 255 ? 255 : g);
    b = b < 0 ? 0 : (b > 255 ? 255 : b);
    return (DWORD)(r << 16 | g << 8 | b);
}

inline DWORD Scale(DWORD c, float k)
{
    return Rgb((int)(((c >> 16) & 255) * k), (int)(((c >> 8) & 255) * k), (int)((c & 255) * k));
}


// ----- Map theme -----

// Which art set the world functions below draw with (suburb / asian old
// town). Set once per delivery, before the sky or props are made.
void  SetArtTheme(Theme t);
Theme GetArtTheme();


// ----- Walls, floors, sky -----

// Color of a wall at (u, v): u = across the face 0..1, v = top 0 .. bottom 1.
// door: this face has a front door (a path leads up to it).
// night: windows glow warm at night and reflect the sky by day.
// edgeL / edgeR: the left / right side of this section is a real corner of the
// building (houses draw an outline there; long walls don't look like panels).
DWORD WallColor(TileType tile, int tileX, int tileY, float u, float v, bool door, bool night,
    bool edgeL, bool edgeR);

// Hedges (low walls). Side: along = world position along the face (continuous\n// across tiles, so clumps never get cut), v top 0 .. bottom 1 of the hedge.
// Top: the flat trimmed top seen from above, at world point (fx, fy).
DWORD HedgeSideColor(float along, float v);
DWORD HedgeTopColor(float fx, float fy);

// Color of the ground at world point (fx, fy).
DWORD FloorColor(const Map& map, float fx, float fy);

// ----- Sky (one look per time of day) -----

struct SkyLook
{
    DWORD haze;       // what far things fade into (matches the sky at the horizon)
    float hazeDist;   // how far you can see
    DWORD tint;       // light color on the whole world (0xFFFFFF = none)
};
SkyLook GetSkyLook(TimeOfDay time);

// The sky is a 360 degree panorama, painted once per delivery.
// Width covers a full turn (about 1 pixel per screen pixel); height = rows
// from the top of the screen down to the horizon.
const int SKY_PANO_W = 3456;

// Paints the panorama in three layers (each w * h pixels, row 0 = top):
//   base:    gradient, sun or moon, stars
//   clouds:  drifting clouds       (0 = see-through, else 0xFF000000 | color)
//   skyline: buildings + rooftops  (0 = see-through, else 0xFF000000 | color)
// Same seed = same sky; each delivery gets a new one.
void PaintSky(TimeOfDay time, unsigned seed, DWORD* base, DWORD* clouds, DWORD* skyline, int w, int h);

// The sloped roof surface at one point (see RoofSample in World.h).
DWORD RoofColor(const RoofSample& r);

// Minimap color for a tile.
DWORD MinimapTileColor(TileType tile);


// ----- Prop sprites (drawn once at startup) -----
void MakeTree(Texture& t);
void MakeBush(Texture& t);
void MakeFlowers(Texture& t);
void MakeLamp(Texture& t, bool lit);   // lit = glass glowing (night)
// (all four follow the current art theme)


// ----- Dogs (placeholder look) -----

// Pixel of the placeholder dog at u (-1 left .. 1 right), v (0 top .. 1 bottom).
// Returns false where it's transparent.
bool DogPlaceholderPixel(float u, float v, DWORD body, DWORD& out);


// ----- UI -----

// Little potion bottle (shop + item slot), liquid = 0xRRGGBB.
void MakePotion(Texture& t, DWORD liquid);

// Red "!" shown over the cat when a dog spots you.
void MakeAlert(Texture& t);

// Dog bone (shop item, and lying on the ground once dropped).
void MakeBone(Texture& t);

// Little heart floating over a dog that's after a bone.
void MakeHeart(Texture& t);

// Spiky white shout bubble with a red "!!" over a barking watchdog.
void MakeBarkBubble(Texture& t);

// Perk icons (the heart is reused for Extra Life):
void MakeSpeedIcon(Texture& t);     // Zoomies: blue ">>" with speed lines
void MakeStaminaIcon(Texture& t);   // Big Lungs: green lightning bolt
void MakeFishIcon(Texture& t);      // Chonky: a fat orange fish
void MakeClockIcon(Texture& t);     // Quick Paws: purple stopwatch

// Dumbbell: the ground pound ability icon.
void MakeDumbbell(Texture& t);

// Soft dark oval: the shadow under the cat while it's in the air.
void MakeShadow(Texture& t);

// Grey fluffy ball (only used if Fur.png is missing).
void MakeFurBall(Texture& t);

// Dizzy ring with stars over a stunned dog. phase 0..2 turns the stars
// around the ring (swap frames to make them spin).
void MakeDizzy(Texture& t, int phase);