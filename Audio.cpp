#include "Audio.h"
#include "Config.h"

#pragma comment(lib, "winmm.lib")   // waveOut lives in winmm (built into Windows)

Audio& Audio::Get()
{
    static Audio instance;
    return instance;
}

Audio::Audio()
    : m_device(nullptr), m_event(nullptr), m_thread(nullptr), m_quit(false), m_ready(false)
{
    for (int i = 0; i < MAX_VOICES; ++i) m_voices[i].active = false;
}

bool Audio::Init()
{
    if (m_ready) return true;

    for (int i = 0; i < (int)Sfx::Count; ++i) MakeSound((Sfx)i, m_clips[i]);
    InitializeCriticalSection(&m_lock);

    WAVEFORMATEX fmt = {};
    fmt.wFormatTag = WAVE_FORMAT_PCM;
    fmt.nChannels = 1;
    fmt.nSamplesPerSec = SOUND_RATE;
    fmt.wBitsPerSample = 16;
    fmt.nBlockAlign = 2;
    fmt.nAvgBytesPerSec = SOUND_RATE * 2;

    m_event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (waveOutOpen(&m_device, WAVE_MAPPER, &fmt, (DWORD_PTR)m_event, 0, CALLBACK_EVENT) != MMSYSERR_NOERROR)
    {
        m_device = nullptr;
        return false;   // no sound device: the game runs silent
    }

    // Prime every buffer with silence and queue it.
    for (int b = 0; b < BUFFERS; ++b)
    {
        ZeroMemory(&m_headers[b], sizeof(WAVEHDR));
        ZeroMemory(m_buffers[b], sizeof(m_buffers[b]));
        m_headers[b].lpData = (LPSTR)m_buffers[b];
        m_headers[b].dwBufferLength = sizeof(m_buffers[b]);
        waveOutPrepareHeader(m_device, &m_headers[b], sizeof(WAVEHDR));
        waveOutWrite(m_device, &m_headers[b], sizeof(WAVEHDR));
    }

    m_quit = false;
    m_thread = CreateThread(nullptr, 0, ThreadMain, this, 0, nullptr);
    m_ready = (m_thread != nullptr);
    return m_ready;
}

void Audio::Shutdown()
{
    if (m_thread)
    {
        m_quit = true;
        SetEvent(m_event);
        WaitForSingleObject(m_thread, 1000);
        CloseHandle(m_thread);
        m_thread = nullptr;
    }
    if (m_device)
    {
        waveOutReset(m_device);
        for (int b = 0; b < BUFFERS; ++b) waveOutUnprepareHeader(m_device, &m_headers[b], sizeof(WAVEHDR));
        waveOutClose(m_device);
        m_device = nullptr;
    }
    if (m_event) { CloseHandle(m_event); m_event = nullptr; }
    if (m_ready || m_clips[0].samples) DeleteCriticalSection(&m_lock);
    for (int i = 0; i < (int)Sfx::Count; ++i) FreeSound(m_clips[i]);
    m_ready = false;
}

void Audio::Play(Sfx id, float volume, float pitch)
{
    if (!m_ready) return;
    const SoundClip& clip = m_clips[(int)id];
    if (!clip.samples) return;

    int vol = (int)(volume * SFX_VOLUME * 256.0f);
    if (vol <= 0) return;
    if (vol > 256) vol = 256;

    EnterCriticalSection(&m_lock);
    // A free voice, or else steal the one closest to finishing.
    int pick = -1, mostDone = -1;
    for (int i = 0; i < MAX_VOICES; ++i)
    {
        if (!m_voices[i].active) { pick = i; break; }
        int done = (int)((m_voices[i].pos >> 16) * 100 / m_voices[i].length);
        if (done > mostDone) { mostDone = done; pick = i; }
    }
    Voice& v = m_voices[pick];
    v.data = clip.samples;
    v.length = clip.count;
    v.pos = 0;
    if (pitch < 0.25f) pitch = 0.25f;
    if (pitch > 4.0f)  pitch = 4.0f;
    v.step = (unsigned)(pitch * 65536.0f);
    v.volume = vol;
    v.active = true;
    LeaveCriticalSection(&m_lock);
}

DWORD WINAPI Audio::ThreadMain(LPVOID self)
{
    ((Audio*)self)->Run();
    return 0;
}

void Audio::Run()
{
    while (!m_quit)
    {
        WaitForSingleObject(m_event, 50);
        if (m_quit) break;

        // Refill whichever buffers Windows has finished playing.
        for (int b = 0; b < BUFFERS; ++b)
        {
            if (!(m_headers[b].dwFlags & WHDR_DONE)) continue;
            Mix(m_buffers[b], BUFFER_SAMPLES);
            m_headers[b].dwFlags &= ~WHDR_DONE;
            waveOutWrite(m_device, &m_headers[b], sizeof(WAVEHDR));
        }
    }
}

void Audio::Mix(short* out, int count)
{
    static int acc[BUFFER_SAMPLES];
    for (int i = 0; i < count; ++i) acc[i] = 0;

    EnterCriticalSection(&m_lock);
    for (int v = 0; v < MAX_VOICES; ++v)
    {
        Voice& voice = m_voices[v];
        if (!voice.active) continue;
        for (int i = 0; i < count; ++i)
        {
            int idx = (int)(voice.pos >> 16);
            if (idx >= voice.length) { voice.active = false; break; }
            acc[i] += (voice.data[idx] * voice.volume) >> 8;
            voice.pos += voice.step;
        }
        if ((int)(voice.pos >> 16) >= voice.length) voice.active = false;
    }
    LeaveCriticalSection(&m_lock);

    for (int i = 0; i < count; ++i)
    {
        int s = acc[i];
        if (s > 32767) s = 32767;
        if (s < -32768) s = -32768;
        out[i] = (short)s;
    }
}