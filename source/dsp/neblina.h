// neblina.h — ÁCIDO Neblina texture (phase 4b)
// The part of Neblina that works on the mixed sound: three resonant peaks
// that drift slowly to new frequencies, plus a light 4-stage phaser.
// (Neblina's unison detune lives in the voice.) 0% = bypass.
#pragma once

#include "effects.h"

namespace acido
{

class NeblinaTexture
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        rng = Random(); rng.state = 0xA5A5F00Du;
        for (size_t k = 0; k < 3; ++k)
        {
            freq[k] = target[k] = 400.0f + 700.0f * (float) k;
            timer[k] = (int) (sr * (0.5 + 0.7 * (double) k));
            for (auto& s : bpState[k]) s = {};
        }
        for (auto& ch : ap) for (auto& z : ch) z = 0.0f;
        fbL = fbR = 0.0f;
        lfo = 0.0;
        amount = {};
    }

    void process (float* L, float* R, int n, float amountTarget)
    {
        const float sc = smoothCoef (30.0f, sr);
        const float glide = smoothCoef (900.0f, sr);    // peaks drift, never jump
        for (int i = 0; i < n; ++i)
        {
            const float a = amount.next (amountTarget, sc);

            // Move each resonance toward a new random spot every 1.5-3 s.
            for (size_t k = 0; k < 3; ++k)
            {
                if (--timer[k] <= 0)
                {
                    timer[k] = (int) (sr * (1.5 + 0.75 * (1.0 + (double) rng.next())));
                    target[k] = 300.0f * std::pow (10.0f, 0.5f + 0.5f * rng.next());   // ~300 Hz .. 3 kHz
                }
                freq[k] += (target[k] - freq[k]) * glide;
            }
            if (a <= 0.0f) continue;

            // Resonances (band-pass, Q ~ 8), added in parallel.
            float resL = 0.0f, resR = 0.0f;
            if ((i & 15) == 0) for (size_t k = 0; k < 3; ++k) coefs[k] = svfCoefs (freq[k]);
            for (size_t k = 0; k < 3; ++k)
            {
                resL += bandpass (L[i], bpState[k][0], coefs[k]);
                resR += bandpass (R[i], bpState[k][1], coefs[k]);
            }

            // Phaser: 4 all-passes swept 300 Hz..2.4 kHz, left and right a quarter-turn apart.
            lfo += 0.18 / sr; if (lfo >= 1.0) lfo -= 1.0;
            const float phL = phaser (L[i] + 0.3f * fbL, ap[0], (float) std::sin (kTwoPi * lfo));
            const float phR = phaser (R[i] + 0.3f * fbR, ap[1], (float) std::cos (kTwoPi * lfo));
            fbL = phL; fbR = phR;

            const float phaseMix = 0.25f * a, resMix = 0.12f * a;
            L[i] = L[i] * (1.0f - phaseMix) + phL * phaseMix + resL * resMix;
            R[i] = R[i] * (1.0f - phaseMix) + phR * phaseMix + resR * resMix;
        }
    }

private:
    struct Svf { float g, k, a1, a2, a3; };
    struct SvfState { float ic1 = 0.0f, ic2 = 0.0f; };

    Svf svfCoefs (float hz) const
    {
        Svf c;
        c.g = (float) std::tan (kPi * std::min (hz, (float) (0.45 * sr)) / sr);
        c.k = 1.0f / 8.0f;
        c.a1 = 1.0f / (1.0f + c.g * (c.g + c.k));
        c.a2 = c.g * c.a1;
        c.a3 = c.g * c.a2;
        return c;
    }
    static float bandpass (float x, SvfState& s, const Svf& c)
    {
        const float v3 = x - s.ic2;
        const float v1 = c.a1 * s.ic1 + c.a2 * v3;
        const float v2 = s.ic2 + c.a2 * s.ic1 + c.a3 * v3;
        s.ic1 = 2.0f * v1 - s.ic1;
        s.ic2 = 2.0f * v2 - s.ic2;
        return v1;
    }
    float phaser (float x, std::array<float, 4>& z, float lfoValue) const
    {
        const float hz = 300.0f * std::pow (8.0f, 0.5f + 0.5f * lfoValue);
        const float t = (float) std::tan (kPi * hz / sr);
        const float c = (t - 1.0f) / (t + 1.0f);
        for (auto& s : z)
        {
            const float y = c * x + s;
            s = x - c * y;
            x = y;
        }
        return x;
    }

    double sr = 44100.0, lfo = 0.0;
    Random rng;
    std::array<float, 3> freq {}, target {};
    std::array<int, 3> timer {};
    std::array<std::array<SvfState, 2>, 3> bpState {};
    std::array<Svf, 3> coefs {};
    std::array<std::array<float, 4>, 2> ap {};
    float fbL = 0.0f, fbR = 0.0f;
    Smoothed amount;
};

} // namespace acido
