#pragma once

// RASPA stage 2: the seven voices. Every lane can load any of them.
// Each voice has two "voice knobs" (A and B) plus the shared Decay.
//
//   Shaker        A Grain   B Seed     particle collisions through a shell resonance
//   Guiro         A Stroke  B Tick     a scrape = fast run of wooden ticks; accents get the long stroke
//   Hi-hat        A Metal   B Open     six square oscillators + noise; Open lengthens accented hits
//   Crack/Clank   A Morph   B Tone     dry crack (snap, rim) morphing to a metal clank
//   Triangle      A Ring    B Mute     bright metal partials; Mute damps the non-accented hits
//   Broken Glass  A Shards  B Spread   a scatter of tiny FM shards (how many, how spread out)
//   Lo-Fi         A Dust    B Body     dusty tick or boom-bap snap, with crackle and grit
//   Metal Guiro   A Stroke  B Tick     the guira: same scrape, but on a ringing metal body
//
// Pitch (every voice): -12..+12 semitones, continuous, for fine tuning.
//
// Plain C++ (no JUCE) so the test program can check it directly.

#include <cmath>
#include <cstdint>
#include <algorithm>

namespace raspa
{
    enum VoiceType { shaker = 0, guiro, hihat, clank, triangle, glass, lofi, metalGuiro, numVoiceTypes };

    inline const char* voiceName (int t)
    {
        static const char* n[numVoiceTypes] = { "SHAKER", "G\xc3\x9cIRO", "HI-HAT", "CRACK/CLANK", "TRIANGLE", "BROKEN GLASS", "LO-FI", "METAL G\xc3\x9cIRO" };
        return n[std::clamp (t, 0, numVoiceTypes - 1)];
    }
    inline const char* knobAName (int t)
    {
        static const char* n[numVoiceTypes] = { "GRAIN", "STROKE", "METAL", "MORPH", "RING", "SHARDS", "DUST", "STROKE" };
        return n[std::clamp (t, 0, numVoiceTypes - 1)];
    }
    inline const char* knobBName (int t)
    {
        static const char* n[numVoiceTypes] = { "SEED", "TICK", "OPEN", "TONE", "MUTE", "SPREAD", "BODY", "TICK" };
        return n[std::clamp (t, 0, numVoiceTypes - 1)];
    }

    constexpr float twoPi = 6.28318530718f;

    struct Rng
    {
        uint32_t s;
        explicit Rng (uint32_t seed = 0x12345678u) : s (seed ? seed : 1u) {}
        inline uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
        inline float uni() { return (float) (next() >> 8) * (1.0f / 16777216.0f); }   // 0..1
        inline float bi()  { return uni() * 2.0f - 1.0f; }                              // -1..1
    };

    // State-variable band-pass (TPT form, stable at any setting), unity gain at centre
    struct BandPass
    {
        float g = 0, k = 1, a1 = 0, a2 = 0, a3 = 0, ic1 = 0, ic2 = 0;
        void set (float freq, float q, float sr)
        {
            freq = std::clamp (freq, 20.0f, sr * 0.45f);
            g = std::tan (3.14159265f * freq / sr);
            k = 1.0f / std::max (q, 0.1f);
            a1 = 1.0f / (1.0f + g * (g + k));
            a2 = g * a1;
            a3 = g * a2;
        }
        inline float process (float x)
        {
            float v3 = x - ic2;
            float v1 = a1 * ic1 + a2 * v3;
            float v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2 * v1 - ic1;
            ic2 = 2 * v2 - ic2;
            return v1 * k;
        }
        void reset() { ic1 = ic2 = 0; }
    };

    struct OnePole   // simple low-pass; hp() gives the matching high-pass
    {
        float z = 0, c = 0;
        void set (float freq, float sr) { c = std::exp (-twoPi * std::clamp (freq, 10.0f, sr * 0.45f) / sr); }
        inline float lp (float x) { z = x + c * (z - x); return z; }
        inline float hp (float x) { return x - lp (x); }
        void reset() { z = 0; }
    };

    inline float coefFor (float seconds, float sr) { return std::exp (-6.9f / (std::max (seconds, 1.0e-4f) * sr)); } // -60 dB in `seconds`

    // ------------------------------------------------------------ Voice
    struct Voice
    {
        float sr = 48000;
        int type = shaker;
        float A = 0.5f, B = 0.5f, D = 0.35f;
        float P = 0.0f, pr = 1.0f;   // pitch in semitones (-12..+12) and as a frequency ratio
        Rng rng;

        explicit Voice (uint32_t seed = 1234) : rng (seed) {}

        void prepare (float sampleRate) { sr = sampleRate; update(); reset(); }

        // pitch: semitones, -12..+12 (moves every frequency of the voice; the "air" filters stay put)
        void setParams (int t, float a, float b, float d, float pitch = 0.0f)
        {
            t = std::clamp (t, 0, numVoiceTypes - 1);
            pitch = std::clamp (pitch, -12.0f, 12.0f);
            if (t != type) { type = t; reset(); A = a; B = b; D = d; setPitch (pitch); update(); return; }
            if (std::fabs (a - A) + std::fabs (b - B) + std::fabs (d - D) + std::fabs (pitch - P) < 1.0e-6f) return;
            A = a; B = b; D = d; setPitch (pitch);
            update();
        }
        void setPitch (float pitch) { P = pitch; pr = std::pow (2.0f, P / 12.0f); }

        void reset()
        {
            shake = burst = swell = 0;
            for (auto* f : { &bp1, &bp2, &bp3, &bp4 }) f->reset();
            hpf.reset(); lpf.reset(); lpf2.reset();
            env = env2 = 0; strokeLeft = 0; tickPhase = 0;
            for (auto& p : part) p.amp = 0;
            for (auto& s : shards) s.on = false;
            crackle = 0; held = 0; holdCount = 0;
        }

        bool active() const
        {
            if (shake > 0 || std::fabs (burst) > 1.0e-5f || env > 1.0e-5f || env2 > 1.0e-5f || strokeLeft > 0 || crackle > 1.0e-5f) return true;
            for (auto& p : part) if (p.amp > 1.0e-5f) return true;
            for (auto& s : shards) if (s.on) return true;
            return false;
        }

        // stepValue: 1 ghost, 2 medium, 3 accent
        void trigger (float velocity, bool accent)
        {
            switch (type)
            {
                case shaker:   trigShaker (velocity); break;
                case guiro:    trigGuiro (velocity, accent); break;
                case hihat:    trigHat (velocity, accent); break;
                case clank:    trigClank (velocity); break;
                case triangle: trigTriangle (velocity, accent); break;
                case glass:    trigGlass (velocity); break;
                case lofi:     trigLofi (velocity); break;
                case metalGuiro: trigGuiro (velocity, accent); break;
                default: break;
            }
        }

        inline float process()
        {
            float y;
            switch (type)
            {
                case shaker:   y = procShaker(); break;
                case guiro:    y = procGuiro(); break;
                case hihat:    y = procHat(); break;
                case clank:    y = procClank(); break;
                case triangle: y = procTriangle(); break;
                case glass:    y = procGlass(); break;
                case lofi:     y = procLofi(); break;
                case metalGuiro: y = procGuiro(); break;
                default:       y = 0; break;
            }
            return y * outGain;
        }

    private:
        float outGain = 1;

        // shared state
        BandPass bp1, bp2, bp3, bp4;
        OnePole hpf, lpf, lpf2;
        float env = 0, env2 = 0, envCoef = 0.999f, env2Coef = 0.99f;

        // shaker
        float shake = 0, shakeCoef = 0.999f, swell = 1, swellCoef = 0.99f;
        float burst = 0, burstCoef = 0.9f, collideProb = 0.1f, loudComp = 1;

        // guiro
        int strokeLeft = 0, strokeLen = 1;
        float tickPhase = 0, tickInc = 0, strokeVel = 0;

        // modal partials (triangle, clank)
        struct Partial { float phase = 0, inc = 0, amp = 0, coef = 0.999f, gain = 1; };
        Partial part[6];

        // hi-hat
        float sqPhase[6] = {}, sqInc[6] = {};
        float hatOpenCoef = 0.999f, metalMix = 0.5f;

        // glass
        struct Shard { bool on = false; int wait = 0; float pc = 0, pm = 0, ic = 0, im = 0, index = 0, idxCoef = 0.99f, amp = 0, ampCoef = 0.99f; };
        Shard shards[24];
        int maxShards = 8; float spreadSec = 0.05f;

        // lofi
        float crackle = 0, crackleCoef = 0.999f, held = 0; int holdCount = 0, holdN = 1; float bodyPhase = 0, bodyInc = 0, bitScale = 1024;

        // --------------------------------------------------------- per-voice settings
        void update()
        {
            switch (type)
            {
                case shaker:
                {
                    float lenSec = 0.025f * std::pow (24.0f, D);
                    shakeCoef = coefFor (lenSec, sr);
                    float rate = 300.0f * std::pow (30.0f, A);
                    collideProb = std::min (rate / sr, 0.9f);
                    float burstSec = 0.0006f + 0.0019f * B;
                    burstCoef = coefFor (burstSec, sr);
                    float f = 9500.0f * std::pow (3000.0f / 9500.0f, B) * pr;
                    bp1.set (f, 1.6f, sr);
                    bp2.set (std::min (f * 1.62f, sr * 0.45f), 2.2f, sr);
                    swellCoef = std::exp (-1.0f / ((0.0015f + 0.0045f * B) * sr));
                    hpf.set (1800.0f, sr);
                    loudComp = 1.0f / std::sqrt (std::max (rate * burstSec, 0.05f));
                    outGain = 6.9f * loudComp;
                    break;
                }
                case guiro:
                {
                    tickInc = (70.0f + 330.0f * B) / sr;                       // ticks per sample
                    bp1.set (1500.0f * pr, 7.0f, sr);                               // wooden body
                    bp2.set (3300.0f * pr, 5.0f, sr);
                    bp3.set (5600.0f * pr, 3.0f, sr);
                    envCoef = coefFor (0.0012f + 0.004f * D, sr);              // each tick's ring
                    hpf.set (500.0f, sr);
                    outGain = 3.6f;
                    break;
                }
                case metalGuiro:
                {
                    // guira: a thin metal sheet scraped with a wire fork; bright, ringing ticks
                    tickInc = (110.0f + 420.0f * B) / sr;
                    bp1.set (3100.0f * pr, 18.0f, sr);
                    bp2.set (4730.0f * pr, 20.0f, sr);
                    bp3.set (6870.0f * pr, 16.0f, sr);
                    bp4.set (9300.0f * pr, 10.0f, sr);
                    envCoef = coefFor (0.0008f + 0.003f * D, sr);
                    hpf.set (1800.0f, sr);
                    outGain = 3.0f;
                    break;
                }
                case hihat:
                {
                    static const float ratios[6] = { 205.3f, 304.4f, 369.6f, 522.7f, 540.0f, 800.0f };
                    for (int i = 0; i < 6; ++i) sqInc[i] = ratios[i] * 1.6f * pr / sr;
                    metalMix = A;
                    envCoef = coefFor (0.03f + 0.22f * D, sr);
                    hatOpenCoef = coefFor ((0.03f + 0.22f * D) * (1.0f + 7.0f * B), sr);
                    bp1.set (9500.0f * pr, 1.2f, sr);
                    hpf.set (6500.0f, sr);
                    outGain = 2.0f;
                    break;
                }
                case clank:
                {
                    // crack: noise snap; clank: inharmonic metal partials
                    float base = 380.0f * std::pow (4.0f, B) * pr;                 // 380 Hz .. 1.5 kHz
                    static const float r[6] = { 1.0f, 1.47f, 2.09f, 2.56f, 3.83f, 5.12f };
                    static const float gn[6] = { 1.0f, 0.8f, 0.6f, 0.5f, 0.35f, 0.25f };
                    float len = 0.06f + 0.5f * D;
                    for (int i = 0; i < 6; ++i)
                    {
                        part[i].inc = std::min (base * r[i], sr * 0.45f) / sr;
                        part[i].coef = coefFor (len / (1.0f + 0.35f * (float) i), sr);
                        part[i].gain = gn[i];
                    }
                    bp1.set (1200.0f * std::pow (3.5f, B) * pr, 1.4f, sr);          // crack colour
                    envCoef = coefFor (0.006f + 0.06f * D, sr);
                    hpf.set (300.0f, sr);
                    outGain = 1.0f;
                    break;
                }
                case triangle:
                {
                    float base = 900.0f * std::pow (2.6f, A) * pr;                  // 900 Hz .. 2.3 kHz
                    static const float r[6] = { 1.0f, 2.76f, 3.4f, 5.40f, 6.83f, 8.93f };
                    static const float gn[6] = { 0.55f, 1.0f, 0.5f, 0.8f, 0.4f, 0.5f };
                    for (int i = 0; i < 6; ++i)
                    {
                        part[i].inc = std::min (base * r[i], sr * 0.45f) / sr;
                        part[i].gain = gn[i];
                    }
                    hpf.set (700.0f, sr);
                    envCoef = coefFor (0.004f, sr);                            // strike click
                    outGain = 0.19f;
                    break;
                }
                case glass:
                {
                    maxShards = 2 + (int) std::lround (A * 22.0f);
                    spreadSec = 0.004f + 0.25f * B * B;
                    envCoef = coefFor (0.02f, sr);                             // the break crack
                    hpf.set (2500.0f, sr);
                    outGain = 0.62f;
                    break;
                }
                case lofi:
                {
                    // Body 0: thin vinyl tick; 1: thick boom-bap snap/shaker
                    bp1.set (3200.0f * std::pow (0.45f, B) * pr, 1.2f, sr);
                    bp2.set (5200.0f * pr, 0.8f, sr);                               // thin tick colour
                    envCoef = coefFor (0.012f + (0.05f + 0.25f * D) * B + 0.03f * D, sr);
                    env2Coef = coefFor (0.04f + 0.08f * B, sr);
                    bodyInc = 175.0f * pr / sr;
                    lpf.set (5200.0f, sr);                                     // "tape": dull top
                    lpf2.set (7000.0f, sr);
                    hpf.set (90.0f, sr);
                    crackleCoef = coefFor (0.15f + 0.5f * A, sr);
                    holdN = 1 + (int) std::lround (A * 3.0f);                   // sample-rate grit
                    bitScale = std::pow (2.0f, 11.0f - 5.0f * A);              // 11 .. 6 bits
                    outGain = 1.0f;
                    break;
                }
                default: break;
            }
        }

        // --------------------------------------------------------- shaker
        void trigShaker (float v)
        {
            if (shake < 0.05f) swell = 0; else swell = std::min (swell, 0.5f);
            shake = std::max (shake, v);
        }
        float procShaker()
        {
            if (shake > 1.0e-4f)
            {
                swell = 1.0f - (1.0f - swell) * swellCoef;
                float energy = shake * swell;
                if (rng.uni() < collideProb) burst += energy * (0.6f + 0.4f * rng.uni());
                shake *= shakeCoef;
            }
            else shake = 0;
            burst *= burstCoef;
            float n = rng.bi() * burst;
            float y = 0.7f * bp1.process (n) + 0.45f * bp2.process (n);
            return hpf.hp (y);
        }

        // --------------------------------------------------------- guiro
        void trigGuiro (float v, bool accent)
        {
            // accents: the long stroke ("largo"); other steps: short strokes ("corto")
            float sec = (0.03f + 0.25f * A) * (accent ? 1.0f : 0.38f);
            strokeLen = std::max (1, (int) (sec * sr));
            strokeLeft = strokeLen;
            strokeVel = v;
            tickPhase = 1.0f; // tick right away
        }
        float procGuiro()
        {
            if (strokeLeft > 0)
            {
                float t = 1.0f - (float) strokeLeft / (float) strokeLen;      // 0..1 through the stroke
                float speed = 0.75f + 0.5f * t;                                 // the scrape speeds up
                tickPhase += tickInc * speed * (0.85f + 0.3f * rng.uni());
                if (tickPhase >= 1.0f)
                {
                    tickPhase -= 1.0f;
                    float shape = std::sin (3.14159265f * std::min (t * 1.15f + 0.08f, 1.0f));
                    env = std::max (env, strokeVel * (0.35f + 0.65f * shape) * (0.8f + 0.2f * rng.uni()));
                }
                --strokeLeft;
            }
            float n = rng.bi() * env;
            env *= envCoef;
            float y;
            if (type == metalGuiro)   // metal sheet: ringing partials plus the bright wire rasp
                y = 0.9f * bp1.process (n) + 0.8f * bp2.process (n) + 0.6f * bp3.process (n) + 0.5f * bp4.process (n) + 0.25f * n;
            else                      // wooden gourd
                y = 1.0f * bp1.process (n) + 0.7f * bp2.process (n) + 0.35f * bp3.process (n);
            return hpf.hp (y);
        }

        // --------------------------------------------------------- hi-hat
        float hatCoef = 0.999f;
        void trigHat (float v, bool accent)
        {
            env = std::max (env * 0.3f, v);       // a new hit chokes the last one
            hatCoef = accent ? hatOpenCoef : envCoef;
        }
        float procHat()
        {
            float sq = 0;
            for (int i = 0; i < 6; ++i)
            {
                sqPhase[i] += sqInc[i]; if (sqPhase[i] >= 1.0f) sqPhase[i] -= 1.0f;
                sq += sqPhase[i] < 0.5f ? 1.0f : -1.0f;
            }
            sq *= (1.0f / 6.0f);
            float src = metalMix * sq + (1.0f - metalMix) * rng.bi() * 0.8f;
            float y = hpf.hp (bp1.process (src));
            float out = y * env;
            env *= hatCoef;
            return out;
        }

        // --------------------------------------------------------- crack / clank
        void trigClank (float v)
        {
            env = v;                                                    // crack burst
            for (int i = 0; i < 6; ++i) { part[i].amp = v * part[i].gain; part[i].phase = 0.25f; }
        }
        float procClank()
        {
            float crack = bp1.process (rng.bi() * env) * 1.6f;
            env *= envCoef;
            float metal = 0;
            for (auto& p : part)
            {
                if (p.amp < 1.0e-6f) { p.amp = 0; continue; }
                metal += std::sin (twoPi * p.phase) * p.amp;
                p.phase += p.inc; if (p.phase >= 1.0f) p.phase -= 1.0f;
                p.amp *= p.coef;
            }
            metal *= 0.3f;
            float y = (1.0f - A) * crack + A * (metal + 0.35f * crack * (1.0f - A * 0.5f));
            return hpf.hp (y);
        }

        // --------------------------------------------------------- triangle
        void trigTriangle (float v, bool accent)
        {
            // open ring on accents; Mute damps everything else (the hand on the triangle)
            float open = 0.35f + 2.6f * D;                                    // seconds
            float len = accent ? open : open * (1.0f - 0.94f * B);
            for (int i = 0; i < 6; ++i)
            {
                part[i].amp = v * part[i].gain;                               // new strike replaces the ring
                part[i].coef = coefFor (len / (1.0f + 0.25f * (float) i), sr);
            }
            env = v;
        }
        float procTriangle()
        {
            float y = 0;
            for (auto& p : part)
            {
                if (p.amp < 1.0e-6f) { p.amp = 0; continue; }
                y += std::sin (twoPi * p.phase) * p.amp;
                p.phase += p.inc; if (p.phase >= 1.0f) p.phase -= 1.0f;
                p.amp *= p.coef;
            }
            y += rng.bi() * env * 0.5f;                                       // beater click
            env *= envCoef;
            return hpf.hp (y);
        }

        // --------------------------------------------------------- broken glass
        void trigGlass (float v)
        {
            env = v;
            int spreadSamples = (int) (spreadSec * sr);
            int made = 0;
            for (auto& s : shards)
            {
                if (made >= maxShards) break;
                if (s.on && s.amp > 0.05f) continue;    // keep shards that are still loud
                s.on = true;
                // first shards land at once, the rest scatter (more of them early)
                float u = rng.uni();
                s.wait = made < 2 ? 0 : (int) (u * u * (float) spreadSamples);
                float fc = 2200.0f * std::pow (4.2f, rng.uni()) * pr;              // 2.2 .. 9 kHz
                float ratio = 1.41f + 2.3f * rng.uni();
                s.ic = std::min (fc, sr * 0.45f) / sr;
                s.im = std::min (fc * ratio, sr * 0.45f) / sr;
                s.pc = rng.uni(); s.pm = rng.uni();
                s.index = 2.0f + 4.0f * rng.uni();
                s.idxCoef = coefFor (0.004f + 0.01f * rng.uni(), sr);
                s.amp = v * (0.5f + 0.5f * rng.uni()) * (made < 2 ? 1.0f : 0.75f);
                // tinkle: how long each piece rings (Decay)
                s.ampCoef = coefFor ((0.02f + 0.38f * D) * (0.4f + 0.6f * rng.uni()), sr);
                ++made;
            }
        }
        float procGlass()
        {
            float y = rng.bi() * env * 0.6f;              // the break itself
            env *= envCoef;
            for (auto& s : shards)
            {
                if (! s.on) continue;
                if (s.wait > 0) { --s.wait; continue; }
                float m = std::sin (twoPi * s.pm) * s.index;
                y += std::sin (twoPi * s.pc + m) * s.amp * 0.35f;
                s.pc += s.ic; if (s.pc >= 1.0f) s.pc -= 1.0f;
                s.pm += s.im; if (s.pm >= 1.0f) s.pm -= 1.0f;
                s.index *= s.idxCoef;
                s.amp *= s.ampCoef;
                if (s.amp < 1.0e-5f) s.on = false;
            }
            return hpf.hp (y);
        }

        // --------------------------------------------------------- lo-fi dusty perc
        void trigLofi (float v)
        {
            env = v; env2 = v * B;   // body thump only when Body is up
            bodyPhase = 0;
            crackle = std::max (crackle, v * (0.15f + 0.85f * A));
        }
        float procLofi()
        {
            float n = rng.bi();
            float hit = (1.0f - B) * bp2.process (n) * 0.9f + B * bp1.process (n) * 1.3f;
            hit *= env;
            env *= envCoef;
            float body = std::sin (twoPi * bodyPhase) * env2 * 0.8f;
            bodyPhase += bodyInc * (1.0f + 0.6f * env2); if (bodyPhase >= 1.0f) bodyPhase -= 1.0f;
            env2 *= env2Coef;
            // dust: sparse crackle pops that fade with the hit
            float dust = 0;
            if (crackle > 1.0e-5f)
            {
                if (rng.uni() < 0.0025f) dust = rng.bi() * crackle * 0.9f;
                crackle *= crackleCoef;
            }
            float y = hpf.hp (lpf.lp (lpf2.lp (hit + body + dust)));
            // grit: sample hold + bit reduction
            if (++holdCount >= holdN) { holdCount = 0; held = y; }
            float q = std::round (held * bitScale) / bitScale;
            return q;
        }
    };
}
