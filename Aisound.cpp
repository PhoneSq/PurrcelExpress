#include <math.h>
#include "AISound.h"

// =====================================================
// Helpers
// =====================================================

namespace
{
    const float TWO_PI = 6.2831853f;

    // White noise (xorshift), -1..1
    struct Noise
    {
        unsigned s = 0x9E3779B9u;
        float Next()
        {
            s ^= s << 13; s ^= s >> 17; s ^= s << 5;
            return (s & 0xFFFF) / 32767.5f - 1.0f;
        }
    };

    // One-pole low-pass filter: lets through what's below 'cutoff' Hz.
    struct LowPass
    {
        float y = 0.0f;
        float Run(float x, float cutoff)
        {
            float a = 1.0f - expf(-TWO_PI * cutoff / SOUND_RATE);
            y += a * (x - y);
            return y;
        }
    };

    // A buzzier sine (between a sine and a square wave). drive 1 = soft, 5 = square-ish.
    float Buzz(float phase, float drive) { return tanhf(drive * sinf(phase)); }

    // Bell-like note (a sine plus a couple of quieter overtones).
    float Bell(float freq, float t) { return sinf(TWO_PI * freq * t) + 0.3f * sinf(TWO_PI * 2.0f * freq * t) + 0.1f * sinf(TWO_PI * 3.0f * freq * t); }

    float Clamp01(float x) { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }

    // Resonant band-pass filter (lets a narrow band ring): used for wet "squelch".
    struct BandPass
    {
        float low = 0.0f, band = 0.0f;
        float Run(float x, float center, float q)
        {
            float f = 2.0f * sinf(3.14159265f * center / SOUND_RATE);
            low += f * band;
            float high = x - low - q * band;
            band += f * high;
            return band;
        }
    };

    // Quick fade-in so nothing starts with a click.
    float Attack(float t, float time) { return Clamp01(t / time); }
}


// =====================================================
// Recipes. Each fills 'f' (n samples, -1..1-ish).
// =====================================================

static float Length(Sfx id)
{
    switch (id)
    {
    case Sfx::Click:       return 0.035f;
    case Sfx::Bought:      return 0.50f;
    case Sfx::Denied:      return 0.22f;
    case Sfx::UseItem:     return 0.34f;
    case Sfx::Delivered:   return 0.55f;
    case Sfx::Spotted:     return 0.18f;
    case Sfx::Bite:        return 0.22f;
    case Sfx::Dash:        return 0.40f;
    case Sfx::GroundPound: return 0.55f;
    case Sfx::FurShot:     return 0.16f;
    case Sfx::Step:        return 0.07f;
    default:               return 0.1f;
    }
}

static void Recipe(Sfx id, float* f, int n)
{
    Noise noise;
    LowPass lp, lp2;
    float phase = 0.0f;
    const float T = (float)n / SOUND_RATE;

    for (int i = 0; i < n; ++i)
    {
        float t = (float)i / SOUND_RATE;
        float s = 0.0f;

        switch (id)
        {
        case Sfx::Click:
        {
            // A tiny tick: short high blip with a bit of noise.
            float env = expf(-t * 160.0f) * Attack(t, 0.001f);
            s = (Buzz(TWO_PI * 1800.0f * t, 2.0f) * 0.6f + noise.Next() * 0.25f) * env;
            break;
        }
        case Sfx::Bought:
        {
            // Coin: two quick bright notes (B5 then E6), then a little sparkle.
            float a = Buzz(TWO_PI * 988.0f * t, 2.5f) * expf(-t * 20.0f) * Attack(t, 0.002f);
            float t2 = t - 0.07f;
            float b = t2 > 0.0f ? Buzz(TWO_PI * 1319.0f * t2, 2.5f) * expf(-t2 * 6.0f) * Attack(t2, 0.002f) : 0.0f;
            float sparkle = t2 > 0.0f ? sinf(TWO_PI * 2637.0f * t) * 0.25f * expf(-t2 * 9.0f) * (0.5f + 0.5f * sinf(TWO_PI * 30.0f * t)) : 0.0f;
            s = a * 0.8f + b + sparkle;
            break;
        }
        case Sfx::Denied:
        {
            // Low buzz, two short pulses: "nuh-uh".
            bool on = (t < 0.08f) || (t > 0.11f && t < 0.20f);
            float env = on ? 1.0f : 0.0f;
            s = Buzz(TWO_PI * 150.0f * t, 5.0f) * env * 0.7f;
            s = lp.Run(s, 1800.0f);
            break;
        }
        case Sfx::UseItem:
        {
            // Bubbly rising "gulp": a wobbling pitch climbing up, then a sparkle.
            float freq = 330.0f + 520.0f * (t / T) + 70.0f * sinf(TWO_PI * 16.0f * t);
            phase += TWO_PI * freq / SOUND_RATE;
            float env = Attack(t, 0.01f) * Clamp01((T - t) / 0.08f);
            s = sinf(phase) * env * 0.8f;
            if (t > T * 0.6f) s += sinf(TWO_PI * 2093.0f * t) * 0.2f * Clamp01((T - t) / 0.1f);
            break;
        }
        case Sfx::Delivered:
        {
            // Happy chime: G5 then C6, bell-like.
            float a = Bell(784.0f, t) * expf(-t * 6.0f) * Attack(t, 0.003f);
            float t2 = t - 0.13f;
            float b = t2 > 0.0f ? Bell(1047.0f, t2) * expf(-t2 * 5.0f) * Attack(t2, 0.003f) : 0.0f;
            s = (a + b) * 0.5f;
            break;
        }
        case Sfx::Spotted:
        {
            // Alert: two quick blips going up.
            float t2 = t - 0.09f;
            float a = t < 0.07f ? Buzz(TWO_PI * 880.0f * t, 3.0f) * Attack(t, 0.002f) * Clamp01((0.07f - t) / 0.01f) : 0.0f;
            float b = t2 > 0.0f ? Buzz(TWO_PI * 1320.0f * t2, 3.0f) * Attack(t2, 0.002f) * Clamp01((T - t) / 0.02f) : 0.0f;
            s = (a + b) * 0.6f;
            break;
        }
        case Sfx::Bite:
        {
            // Crunch: three gritty noise bursts and a low thud underneath.
            float burst = 0.0f;
            for (int k = 0; k < 3; ++k)
            {
                float tb = t - k * 0.05f;
                if (tb > 0.0f && tb < 0.04f) burst += expf(-tb * 70.0f) * (1.0f - k * 0.25f);
            }
            float crunch = lp.Run(noise.Next(), 2600.0f) * burst * 2.2f;
            float thud = sinf(TWO_PI * 150.0f * t) * expf(-t * 22.0f) * 0.6f;
            s = crunch + thud;
            break;
        }
        case Sfx::Dash:
        {
            // Whoosh: noise through a filter that sweeps from high to low.
            float cutoff = 6000.0f * powf(250.0f / 6000.0f, t / T);
            float env = Attack(t, 0.03f) * powf(Clamp01(1.0f - t / T), 1.5f);
            s = lp2.Run(lp.Run(noise.Next(), cutoff), cutoff * 1.5f) * env * 3.0f;
            break;
        }
        case Sfx::GroundPound:
        {
            // Thump: a low sine falling in pitch, plus a short dull impact.
            float freq = 38.0f + 110.0f * expf(-t * 14.0f);
            phase += TWO_PI * freq / SOUND_RATE;
            float body = sinf(phase) * expf(-t * 6.5f) * Attack(t, 0.002f);
            float impact = lp.Run(noise.Next(), 700.0f) * expf(-t * 35.0f) * 2.0f;
            s = body + impact;
            break;
        }
        case Sfx::FurShot:
        {
            // "Pew": a quick pitch drop.
            float freq = 420.0f + 1150.0f * expf(-t * 26.0f);
            phase += TWO_PI * freq / SOUND_RATE;
            s = Buzz(phase, 2.5f) * expf(-t * 13.0f) * Attack(t, 0.002f) * 0.7f;
            break;
        }
        case Sfx::Step:
        {
            // Chibi "pi": a tiny clean blip that flicks up in pitch right at
            // the start and then just rings out. Pure tone, no noise.
            float freq = 880.0f * (1.0f + 0.30f * (1.0f - expf(-t * 90.0f)));
            phase += TWO_PI * freq / SOUND_RATE;
            float env = Attack(t, 0.003f) * expf(-t * 55.0f);
            s = (sinf(phase) + 0.12f * sinf(3.0f * phase)) * env;   // soft, round
            break;
        }
        default: break;
        }
        f[i] = s;
    }
}


// =====================================================
// Public
// =====================================================

void MakeSound(Sfx id, SoundClip& out)
{
    int n = (int)(Length(id) * SOUND_RATE);
    float* f = new float[n];
    Recipe(id, f, n);

    // Same loudness for every sound (the game picks the volume when it plays).
    float peak = 0.0001f;
    for (int i = 0; i < n; ++i) { float a = fabsf(f[i]); if (a > peak) peak = a; }
    float gain = 0.8f / peak;

    out.samples = new short[n];
    out.count = n;
    for (int i = 0; i < n; ++i)
    {
        float v = f[i] * gain;
        if (i > n - 64) v *= (n - i) / 64.0f;   // fade the very end: no click
        out.samples[i] = (short)(v * 32767.0f);
    }
    delete[] f;
}

void FreeSound(SoundClip& clip)
{
    delete[] clip.samples;
    clip.samples = nullptr;
    clip.count = 0;
}