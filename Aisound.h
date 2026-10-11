#pragma once

// =====================================================================
//  AI-GENERATED SOUND
//
//  Every sound effect in the game is made here, in code, by an AI
//  (Claude): sine waves, noise and pitch sweeps written straight into
//  sample buffers. No sound files are loaded.
//
//  Audio.h / Audio.cpp only PLAY these.
// =====================================================================

const int SOUND_RATE = 22050;   // samples per second (mono, 16-bit)

// Every sound in the game. Add new ones before Count and give them a
// recipe in MakeSound (AISound.cpp).
enum class Sfx
{
    Click,          // menu buttons
    Bought,         // shop purchase: coin "cha-ching"
    Denied,         // can't do that (can't afford, HP already full...)
    UseItem,        // drinking a potion / dropping a bone: bubbly rise
    Delivered,      // package delivered: happy two-note chime
    Spotted,        // a dog saw you: alert blip
    Bite,           // a dog got you: crunchy hit
    Dash,           // whoosh
    GroundPound,    // deep thump
    FurShot,        // "pew"
    Step,           // soft padded paw step (played quietly, pitch varied)
    Count
};

// A finished sound: 16-bit samples at SOUND_RATE.
struct SoundClip
{
    short* samples = nullptr;
    int    count = 0;
};

// Builds the sound into 'out' (allocates with new[]; free with FreeSound).
void MakeSound(Sfx id, SoundClip& out);
void FreeSound(SoundClip& clip);