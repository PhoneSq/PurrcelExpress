#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include "AISound.h"

// =====================================================
// Plays the sounds from AISound. A tiny mixer: up to
// MAX_VOICES sounds at once, fed to Windows (waveOut) by
// its own thread so the game never waits on it.
//
//   Audio::Get().Play(Sfx::Dash);
//   Audio::Get().Play(Sfx::Click, 0.5f);   // quieter
//
// If there's no sound device, Play just does nothing.
// =====================================================

class Audio
{
public:
    static Audio& Get();

    bool Init();        // makes every sound and opens the device (Game::Init)
    void Shutdown();    // stops and frees everything (Game's destructor)

    // volume: 0..1 (times SFX_VOLUME). pitch: 1 = normal, 1.1 = a bit higher
    // (and shorter). Starts right away, mixes with the rest.
    void Play(Sfx id, float volume = 1.0f, float pitch = 1.0f);

private:
    Audio();
    Audio(const Audio&);
    Audio& operator=(const Audio&);

    static const int MAX_VOICES = 16;
    static const int BUFFERS = 4;
    static const int BUFFER_SAMPLES = 512;    // ~23 ms each at 22050 Hz

    struct Voice
    {
        const short* data;
        int          length;
        unsigned     pos;      // 16.16 fixed point (so pitch can step by fractions)
        unsigned     step;     // 1.0 = 65536
        int          volume;   // 0..256
        bool         active;
    };

    static DWORD WINAPI ThreadMain(LPVOID self);
    void Run();
    void Mix(short* out, int count);

    SoundClip        m_clips[(int)Sfx::Count];
    Voice            m_voices[MAX_VOICES];

    HWAVEOUT         m_device;
    WAVEHDR          m_headers[BUFFERS];
    short            m_buffers[BUFFERS][BUFFER_SAMPLES];
    HANDLE           m_event;     // Windows signals it when a buffer finished playing
    HANDLE           m_thread;
    CRITICAL_SECTION m_lock;      // the voices are shared with the game thread
    volatile bool    m_quit;
    bool             m_ready;
};