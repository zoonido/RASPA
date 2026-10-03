// drive.h — ÁCIDO Drive + Warmth (send-style)
// The dry sound always passes at full level. Drive feeds a copy of it into a
// distortion and blends that back in on top, like a send to a pedal:
// turning Drive up adds grit and harmonics, and never makes the sound duller
// or quieter. The distorted copy leaves out the deep bass, so the low end
// stays tight. Warmth moves the distortion from bright and fuzzy (low) to
// round and tape-like (high). Drive 0% is a true bypass.
#pragma once

#include "acid_voice.h"
#include "effects.h"

namespace acido
{

class Drive
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        for (auto& c : ch) c = {};
        drive = {}; warmth = {};
    }

    void process (float* L, float* R, int n, float driveTarget, float warmthTarget)
    {
        const float sc = smoothCoef (20.0f, sr);
        const float hpA = OnePole::coef (110.0f, sr);                 // the send skips the deep bass
        const float envAtt = smoothCoef (1.0f, sr), envRel = smoothCoef (60.0f, sr), matchA = smoothCoef (30.0f, sr);
        for (int i = 0; i < n; ++i)
        {
            const float d = drive.next (driveTarget, sc);
            const float w = warmth.next (warmthTarget, sc);
            if (d <= 0.0f) continue;

            const float gain  = 2.0f + 60.0f * d * d;                   // how hard the pedal is hit
            const float send  = 0.9f * std::pow (d, 0.7f);              // how much of it is blended in
            const float toneA = OnePole::coef (12000.0f * std::pow (3500.0f / 12000.0f, w), sr);

            L[i] = processChannel (ch[0], L[i], gain, send, w, hpA, toneA, envAtt, envRel, matchA);
            R[i] = processChannel (ch[1], R[i], gain, send, w, hpA, toneA, envAtt, envRel, matchA);
        }
    }

private:
    struct Channel { OnePole hp1, hp2, hp3, tone, post; float dcX = 0, dcY = 0, envDry = 0, envWet = 0, match = 0; };

    static float follow (float env, float x, float att, float rel) { const float a = std::fabs (x); return env + (a - env) * (a > env ? att : rel); }

    static float processChannel (Channel& c, float dry, float gain, float send, float w, float hpA, float toneA,
                                 float envAtt, float envRel, float matchA)
    {
        // 18 dB/oct high-pass so the deep bass (and the note's fundamental on low notes)
        // stays in the clean dry signal and is never smeared or cancelled.
        const float x = c.hp3.hp (c.hp2.hp (c.hp1.hp (dry, hpA), hpA), hpA) * gain;

        const float fuzz = softClip (x);                                         // bright: odd harmonics, edgy
        const float tape = std::tanh (0.8f * x + 0.3f) - std::tanh (0.3f);       // warm: adds even harmonics
        float wet = fuzz + (tape - fuzz) * w;

        const float dcOut = wet - c.dcX + 0.995f * c.dcY;                          // remove the offset of the warm curve
        c.dcX = wet; c.dcY = dcOut;
        wet = c.post.hp (c.tone.lp (dcOut, toneA), hpA);

        // Keep the distorted copy at about the dry sound's peak level, whatever the gain.
        // The level follows gently (never jumps), so there are no spikes on new notes.
        c.envDry = follow (c.envDry, dry, envAtt, envRel);
        c.envWet = follow (c.envWet, wet, envAtt, envRel);
        const float target = std::min (1.5f, c.envDry / (c.envWet + 1e-3f));
        c.match += (target - c.match) * matchA;

        return dry + wet * c.match * send;
    }

    double sr = 44100.0;
    Channel ch[2];
    Smoothed drive, warmth;
};

} // namespace acido
