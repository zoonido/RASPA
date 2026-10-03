// effects.h — ÁCIDO effects (phase 3b)
// Stereo effects after the voice and Drive, in signal order:
// Chorus -> Delay -> Reverb -> Crush -> Comp.
// Every effect is a true bypass at 0% (Dry/Wet, Crush, Comp).
#pragma once

#include "acid_voice.h"
#include <array>
#include <vector>

namespace acido
{

// ---------------------------------------------------------------------------
// Building blocks
// ---------------------------------------------------------------------------
class DelayLine
{
public:
    void prepare (int maxSamples)
    {
        buf.assign ((size_t) std::max (4, maxSamples + 4), 0.0f);
        w = 0;
    }
    void clear() { std::fill (buf.begin(), buf.end(), 0.0f); }
    void push (float x) { buf[(size_t) w] = x; if (++w >= (int) buf.size()) w = 0; }

    // Reads `d` samples back (fractional, linear interpolation). d >= 1.
    float read (float d) const
    {
        const int size = (int) buf.size();
        d = std::clamp (d, 1.0f, (float) (size - 3));
        const int   i = (int) d;
        const float f = d - (float) i;
        int a = w - i;     if (a < 0) a += size;
        int b = a - 1;     if (b < 0) b += size;
        return buf[(size_t) a] + (buf[(size_t) b] - buf[(size_t) a]) * f;
    }
    float readInt (int d) const
    {
        int a = w - d; if (a < 0) a += (int) buf.size();
        return buf[(size_t) a];
    }

private:
    std::vector<float> buf;
    int w = 0;
};

struct OnePole   // simple 6 dB/oct low-pass or high-pass
{
    float z = 0.0f;
    float lp (float x, float a) { z += (x - z) * a; return z; }
    float hp (float x, float a) { z += (x - z) * a; return x - z; }
    static float coef (float hz, double sr) { return 1.0f - std::exp ((float) (-kTwoPi * hz / sr)); }
};

struct Smoothed
{
    float v = -1.0f;
    float next (float target, float c) { if (v < 0.0f) v = target; v += (target - v) * c; return v; }
};

// Equal-power dry/wet: 0 = all dry (exact bypass), 1 = all wet.
inline float dryGain (float mix) { return (float) std::cos (mix * kPi * 0.5); }
inline float wetGain (float mix) { return (float) std::sin (mix * kPi * 0.5); }

// ---------------------------------------------------------------------------
// Chorus — two slowly moving taps, spread left/right. The low end stays mono
// so the bass remains solid; Tone sets the brightness of the chorused signal.
// ---------------------------------------------------------------------------
class Chorus
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        line.prepare ((int) (0.05 * sr));
        lfo = 0.0; hpL = hpR = hpL2 = hpR2 = hpL3 = hpR3 = lpL = lpR = {}; mix = tone = {};
    }

    void process (float* L, float* R, int n, float toneTarget, float mixTarget)
    {
        const float sc = smoothCoef (20.0f, sr);
        const float hpA = OnePole::coef (220.0f, sr);
        for (int i = 0; i < n; ++i)
        {
            const float m = mix.next (mixTarget, sc);
            const float t = tone.next (toneTarget, sc);
            const float in = 0.5f * (L[i] + R[i]);
            line.push (in);

            lfo += 0.35 / sr; if (lfo >= 1.0) lfo -= 1.0;
            const float base = (float) (0.012 * sr), depth = (float) (0.004 * sr);
            float wl = line.read (base + depth * (float) std::sin (kTwoPi * lfo));
            float wr = line.read (base + depth * (float) std::cos (kTwoPi * lfo));

            const float lpA = OnePole::coef (1500.0f * std::pow (16000.0f / 1500.0f, t), sr);
            // 18 dB/oct high-pass: the bass stays in the dry, centred signal.
            wl = lpL.lp (hpL3.hp (hpL2.hp (hpL.hp (wl, hpA), hpA), hpA), lpA);
            wr = lpR.lp (hpR3.hp (hpR2.hp (hpR.hp (wr, hpA), hpA), hpA), lpA);

            const float dg = dryGain (m), wg = wetGain (m);
            L[i] = L[i] * dg + wl * wg;
            R[i] = R[i] * dg + wr * wg;
        }
    }

private:
    double sr = 44100.0, lfo = 0.0;
    DelayLine line;
    OnePole hpL, hpR, hpL2, hpR2, hpL3, hpR3, lpL, lpR;
    Smoothed mix, tone;
};

// ---------------------------------------------------------------------------
// Delay — ping-pong: the first echo on the left, the next on the right.
// Repeats get slightly darker and thinner so they never muddy the bass.
// ---------------------------------------------------------------------------
class PingPongDelay
{
public:
    static constexpr float kMaxSeconds = 4.0f;

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        l.prepare ((int) (kMaxSeconds * sr) + 8);
        r.prepare ((int) (kMaxSeconds * sr) + 8);
        time = -1.0f; lpL = lpR = hpL = hpR = {}; mix = fb = {};
    }

    void process (float* L, float* R, int n, float seconds, float feedbackTarget, float mixTarget)
    {
        const float sc = smoothCoef (20.0f, sr);
        const float tc = smoothCoef (80.0f, sr);        // time changes glide like tape
        const float target = std::clamp (seconds, 0.001f, kMaxSeconds) * (float) sr;
        if (time < 0.0f) time = target;
        const float lpA = OnePole::coef (5000.0f, sr), hpA = OnePole::coef (120.0f, sr);

        for (int i = 0; i < n; ++i)
        {
            time += (target - time) * tc;
            const float m = mix.next (mixTarget, sc);
            const float f = fb.next (std::min (feedbackTarget, 0.95f), sc);

            const float outL = l.read (time), outR = r.read (time);
            const float in = 0.5f * (L[i] + R[i]);
            l.push (in + f * softClip (hpR.hp (lpR.lp (outR, lpA), hpA)));
            r.push (     f * softClip (hpL.hp (lpL.lp (outL, lpA), hpA)));

            const float dg = dryGain (m), wg = wetGain (m);
            L[i] = L[i] * dg + outL * wg;
            R[i] = R[i] * dg + outR * wg;
        }
    }

private:
    double sr = 44100.0;
    DelayLine l, r;
    float time = -1.0f;
    OnePole lpL, lpR, hpL, hpR;
    Smoothed mix, fb;
};

// ---------------------------------------------------------------------------
// Reverb — 8-line feedback delay network with 4 characters, used as a send:
// the dry sound is never turned down, Send only adds the reverb on top.
// ---------------------------------------------------------------------------
class Reverb
{
public:
    enum Type { Room = 0, Hall = 1, Plate = 2, Spring = 3 };

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        for (auto& d : lines)    d.prepare ((int) (0.2 * sr));
        for (auto& d : diffuse)  d.prepare ((int) (0.03 * sr));
        for (auto& d : disperse) d = 0.0f;
        pre.prepare ((int) (0.05 * sr));
        type = -1;
        mix = size = {};
        for (auto& f : damp) f = {};
        hpL = hpR = inHp1 = inHp2 = {};
    }

    void process (float* L, float* R, int n, int typeIndex, float sizeTarget, float mixTarget)
    {
        if (typeIndex != type) setType (typeIndex);
        const float sc = smoothCoef (20.0f, sr);
        const float hpA = OnePole::coef (150.0f, sr);

        for (int i = 0; i < n; ++i)
        {
            const float m = mix.next (mixTarget, sc);
            const float s = size.next (sizeTarget, sc);
            if ((i & 31) == 0) updateDecay (s);

            // Input: pre-delay, diffusion (and dispersion for Spring).
            // Keep deep bass out of the reverb: 12 dB/oct high-pass on the way in.
            const float in = inHp2.hp (inHp1.hp (0.5f * (L[i] + R[i]), hpA), hpA);
            pre.push (in);
            float x = preSamples > 0 ? pre.readInt (preSamples) : in;
            for (int k = 0; k < 4; ++k) x = allpass (diffuse[(size_t) k], x, diffLen[(size_t) k], 0.65f);
            if (type == Spring)
                for (auto& z : disperse)   // first-order allpass chain: the spring "chirp"
                {
                    const float y = -0.62f * x + z;
                    z = x + 0.62f * y;
                    x = y;
                }

            // Feedback network (Householder mixing keeps it dense and stable).
            std::array<float, 8> o {};
            float sum = 0.0f;
            for (size_t k = 0; k < 8; ++k)
            {
                o[k] = damp[k].lp (lines[k].readInt (len[k]), dampA) * gain[k];
                sum += o[k];
            }
            const float mixDown = sum * 0.25f;   // 2 / 8
            for (size_t k = 0; k < 8; ++k) lines[k].push (o[k] - mixDown + (k < 4 ? x : -x) * 0.35f);

            float wl = (o[0] - o[2] + o[4] - o[6]) * 0.6f;
            float wr = (o[1] - o[3] + o[5] - o[7]) * 0.6f;
            wl = hpL.hp (wl, hpA);                // keep the bass out of the reverb
            wr = hpR.hp (wr, hpA);

            // Send: the original sound always passes at full level; the knob only
            // sets how much reverb is added on top.
            const float wg = wetGain (m);
            L[i] += wl * wg;
            R[i] += wr * wg;
        }
    }

private:
    static float allpass (DelayLine& d, float x, int len, float g)
    {
        const float delayed = d.readInt (len);
        const float y = -g * x + delayed;
        d.push (x + g * y);
        return y;
    }

    void setType (int t)
    {
        type = std::clamp (t, 0, 3);
        // Line lengths (ms), RT60 range (s), pre-delay (ms), damping (Hz) per type.
        static const float base[8] = { 29.7f, 37.1f, 41.1f, 43.7f, 47.9f, 53.3f, 59.9f, 67.7f };
        const float scale[4]  = { 0.55f, 1.6f, 0.9f, 0.45f };
        const float preMs[4]  = { 4.0f, 22.0f, 0.0f, 0.0f };
        const float dampHz[4] = { 5000.0f, 6500.0f, 10000.0f, 3800.0f };
        rtMin = std::array<float, 4> { 0.3f, 1.5f, 0.8f, 0.8f }[(size_t) type];
        rtMax = std::array<float, 4> { 1.2f, 6.0f, 4.0f, 3.0f }[(size_t) type];

        for (size_t k = 0; k < 8; ++k) { len[k] = std::max (8, (int) (base[k] * scale[type] * 0.001 * sr)); lines[k].clear(); damp[k] = {}; }
        const float dms[4] = { 4.8f, 3.6f, 12.7f, 9.3f };
        for (size_t k = 0; k < 4; ++k) { diffLen[k] = std::max (2, (int) (dms[k] * 0.001 * sr)); diffuse[k].clear(); }
        for (auto& z : disperse) z = 0.0f;
        preSamples = (int) (preMs[type] * 0.001 * sr);
        pre.clear();
        dampA = OnePole::coef (dampHz[type], sr);
        lastSize = -1.0f;
    }

    void updateDecay (float s)
    {
        if (std::fabs (s - lastSize) < 1e-4f) return;
        lastSize = s;
        const float rt = rtMin * std::pow (rtMax / rtMin, s);
        for (size_t k = 0; k < 8; ++k)
            gain[k] = std::pow (10.0f, -3.0f * (float) len[k] / (rt * (float) sr));
    }

    double sr = 44100.0;
    std::array<DelayLine, 8> lines;
    std::array<DelayLine, 4> diffuse;
    std::array<float, 8> disperse {};
    std::array<int, 8> len {};
    std::array<int, 4> diffLen {};
    std::array<float, 8> gain {};
    std::array<OnePole, 8> damp;
    DelayLine pre;
    OnePole hpL, hpR, inHp1, inHp2;
    int type = -1, preSamples = 0;
    float rtMin = 0.5f, rtMax = 2.0f, dampA = 0.5f, lastSize = -1.0f;
    Smoothed mix, size;
};

// ---------------------------------------------------------------------------
// Crush — fewer bits and a lower sample rate together. 0% = bypass.
// ---------------------------------------------------------------------------
class Crusher
{
public:
    void prepare (double sampleRate) { sr = sampleRate; amt = {}; phase = 1.0f; heldL = heldR = 0.0f; }

    void process (float* L, float* R, int n, float amountTarget)
    {
        const float sc = smoothCoef (20.0f, sr);
        for (int i = 0; i < n; ++i)
        {
            const float a = amt.next (amountTarget, sc);
            if (a <= 0.0f) continue;

            const float hold = 1.0f + 23.0f * a * a;          // down to ~2 kHz sample rate
            phase += 1.0f / hold;
            if (phase >= 1.0f) { phase -= 1.0f; heldL = L[i]; heldR = R[i]; }

            const float levels = std::pow (2.0f, 16.0f - 12.0f * a) * 0.5f;   // 16 bits -> 4 bits
            const float cl = std::round (heldL * levels) / levels;
            const float cr = std::round (heldR * levels) / levels;

            const float m = std::min (1.0f, a * 20.0f);        // fades in over the first 5%
            L[i] += (cl - L[i]) * m;
            R[i] += (cr - R[i]) * m;
        }
    }

private:
    double sr = 44100.0;
    Smoothed amt;
    float phase = 1.0f, heldL = 0.0f, heldR = 0.0f;
};

// ---------------------------------------------------------------------------
// Comp — one knob for punch: lowers the threshold and raises the ratio
// together. A slow-ish attack lets each note's hit through before the
// compressor grabs it, and generous make-up gain lifts the body of the note,
// so more Comp means a louder, denser, punchier bassline. 0% = bypass.
// ---------------------------------------------------------------------------
class Compressor
{
public:
    void prepare (double sampleRate) { sr = sampleRate; env = 0.0f; amt = {}; }

    void process (float* L, float* R, int n, float amountTarget)
    {
        const float sc  = smoothCoef (20.0f, sr);
        const float att = smoothCoef (20.0f, sr), rel = smoothCoef (110.0f, sr);
        for (int i = 0; i < n; ++i)
        {
            const float a = amt.next (amountTarget, sc);
            const float level = std::max (std::fabs (L[i]), std::fabs (R[i]));
            env += (level - env) * (level > env ? att : rel);
            if (a <= 0.0f) continue;

            const float threshDb = -20.0f * a;
            const float ratio    = 1.0f + 3.0f * a;
            const float envDb    = 20.0f * std::log10 (env + 1e-9f);
            const float knee     = 6.0f;
            const float over     = envDb - threshDb;
            float reduction = 0.0f;
            if (over > knee * 0.5f)       reduction = over * (1.0f - 1.0f / ratio);
            else if (over > -knee * 0.5f) reduction = (1.0f - 1.0f / ratio) * (over + knee * 0.5f) * (over + knee * 0.5f) / (2.0f * knee);

            const float makeup = -threshDb * (1.0f - 1.0f / ratio);
            const float g = dbToGain (makeup - reduction);
            L[i] *= g;
            R[i] *= g;
        }
    }

private:
    double sr = 44100.0;
    float env = 0.0f;
    Smoothed amt;
};

} // namespace acido
