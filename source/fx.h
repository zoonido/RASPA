#pragma once

// RASPA stage 4: lane filter and the four send buses.
//
//   lane: voice -> level -> filter (low-pass or high-pass: Cutoff, Res) -> pan -> dry mix
//                                       \-> sends A..D (post filter, post level)
//   A  Reverb        Size, Decay, Damp, Pre-delay, Return       (8-line feedback delay network)
//   B  Filter Delay  per-lane time (tempo-synced), shared Feedback, Cutoff, Res, Return
//   C  Chorus        Rate, Depth, Width, Return                 (two modulated delays, stereo)
//   D  Reverse       Length (tempo-synced), Slice, Fade, Return (plays the last bar-slice backwards,
//                                                                 swelling into the next one)
//
// Every send at 0 leaves the dry sound untouched. All memory is allocated in prepare().
// Plain C++ (no JUCE) so the test program can check it directly.

#include <cmath>
#include <vector>
#include <algorithm>
#include "voices.h"

namespace raspa
{
    // ------------------------------------------------------------ resonant low/band SVF (stable at any setting)
    struct Svf
    {
        float g = 1, k = 2, a1 = 0, a2 = 0, a3 = 0, ic1 = 0, ic2 = 0;
        float lastF = -1, lastQ = -1;
        void set (float freq, float res01, float sr)   // res01: 0 (flat) .. 1 (almost self-oscillating)
        {
            if (std::fabs (freq - lastF) + std::fabs (res01 - lastQ) < 1.0e-6f) return;
            lastF = freq; lastQ = res01;
            freq = std::clamp (freq, 20.0f, sr * 0.45f);
            g = std::tan (3.14159265f * freq / sr);
            k = 2.0f - 1.94f * std::clamp (res01, 0.0f, 1.0f);   // damping: 2 (none) .. 0.06 (very resonant)
            a1 = 1.0f / (1.0f + g * (g + k)); a2 = g * a1; a3 = g * a2;
        }
        inline float lp (float x)
        {
            float v3 = x - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2 * v1 - ic1; ic2 = 2 * v2 - ic2;
            return v2;
        }
        inline float hp (float x)
        {
            float v3 = x - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2 * v1 - ic1; ic2 = 2 * v2 - ic2;
            return x - k * v1 - v2;
        }
        void reset() { ic1 = ic2 = 0; }
    };

    // high-pass mode of the lane filter: knob 0..1 -> 20 Hz (open, filter off) .. 20 kHz
    inline float highpassHz (float c) { return 20.0f * std::pow (1000.0f, std::clamp (c, 0.0f, 1.0f)); }

    // lane cutoff knob 0..1 -> 150 Hz .. 20 kHz (1 = fully open, filter bypassed)
    inline float cutoffHz (float c) { return 150.0f * std::pow (133.3f, std::clamp (c, 0.0f, 1.0f)); }

    // tempo-synced note lengths, in quarter notes
    constexpr int numDelayTimes = 11;
    inline const char* delayTimeName (int i)
    {
        static const char* n[numDelayTimes] = { "1/32", "1/16T", "1/16", "1/16D", "1/8T", "1/8", "1/8D", "1/4T", "1/4", "1/4D", "1/2" };
        return n[std::clamp (i, 0, numDelayTimes - 1)];
    }
    inline double delayTimeBeats (int i)
    {
        static const double b[numDelayTimes] = { 0.125, 1.0 / 6.0, 0.25, 0.375, 1.0 / 3.0, 0.5, 0.75, 2.0 / 3.0, 1.0, 1.5, 2.0 };
        return b[std::clamp (i, 0, numDelayTimes - 1)];
    }
    constexpr int numReverseLengths = 5;
    inline const char* reverseLengthName (int i)
    {
        static const char* n[numReverseLengths] = { "1/16", "1/8", "1/4", "1/2", "1 BAR" };
        return n[std::clamp (i, 0, numReverseLengths - 1)];
    }
    inline double reverseLengthBeats (int i)
    {
        static const double b[numReverseLengths] = { 0.25, 0.5, 1.0, 2.0, 4.0 };
        return b[std::clamp (i, 0, numReverseLengths - 1)];
    }

    // ------------------------------------------------------------ one lane's echo (bus B)
    struct LaneDelay
    {
        std::vector<float> buf;
        int w = 0;
        float smoothLen = -1;
        Svf filt;

        void prepare (double sr) { buf.assign ((size_t) (sr * 3.2) + 8, 0.0f); w = 0; smoothLen = -1; filt.reset(); }
        void reset() { std::fill (buf.begin(), buf.end(), 0.0f); filt.reset(); }

        // returns the echo (already through the delay filter)
        inline float process (float in, float lenSamples, float feedback)
        {
            const int n = (int) buf.size();
            if (smoothLen < 0) smoothLen = lenSamples;
            smoothLen += (lenSamples - smoothLen) * 0.0007f;          // glide on tempo/time changes, no clicks
            float rp = (float) w - std::clamp (smoothLen, 1.0f, (float) n - 4.0f);
            if (rp < 0) rp += (float) n;
            int i0 = (int) rp; float fr = rp - (float) i0;
            int i1 = i0 + 1; if (i1 >= n) i1 -= n;
            float d = buf[(size_t) i0] + (buf[(size_t) i1] - buf[(size_t) i0]) * fr;
            float out = filt.lp (d);
            float wr = in + std::tanh (out * feedback * 1.05f) / 1.05f;  // soft limit in the loop
            buf[(size_t) w] = wr;
            if (++w >= n) w = 0;
            return out;
        }
    };

    // ------------------------------------------------------------ bus A: reverb (8-line FDN)
    struct Reverb
    {
        static constexpr int N = 8;
        std::vector<float> line[N];
        int w[N] = {}, len[N] = {};
        float gain[N] = {}, damp[N] = {};
        float dampCoef = 0.3f;
        std::vector<float> pre[2];
        int pw = 0, preLen = 1;
        double sr = 48000;
        float lastSize = -1, lastDecay = -1, lastDamp = -1, lastPre = -1;

        void prepare (double sampleRate)
        {
            sr = sampleRate;
            static const int base[N] = { 1031, 1327, 1523, 1871, 2053, 2381, 2713, 3089 };
            for (int i = 0; i < N; ++i) { line[i].assign ((size_t) (base[i] * 1.8 * sr / 48000.0) + 4, 0.0f); w[i] = 0; damp[i] = 0; }
            for (auto& p : pre) p.assign ((size_t) (0.13 * sr) + 4, 0.0f);
            pw = 0; lastSize = -1;
        }
        void reset() { for (auto& l : line) std::fill (l.begin(), l.end(), 0.0f); for (auto& p : pre) std::fill (p.begin(), p.end(), 0.0f); for (auto& d : damp) d = 0; }

        void set (float size, float decay, float dampAmt, float preDelay)
        {
            if (std::fabs (size - lastSize) + std::fabs (decay - lastDecay) + std::fabs (dampAmt - lastDamp) + std::fabs (preDelay - lastPre) < 1.0e-6f) return;
            lastSize = size; lastDecay = decay; lastDamp = dampAmt; lastPre = preDelay;
            static const int base[N] = { 1031, 1327, 1523, 1871, 2053, 2381, 2713, 3089 };
            float scale = 0.45f + 1.3f * std::clamp (size, 0.0f, 1.0f);
            float rt60 = 0.3f * std::pow (25.0f, std::clamp (decay, 0.0f, 1.0f));      // 0.3 .. 7.5 s
            for (int i = 0; i < N; ++i)
            {
                len[i] = std::min ((int) (base[i] * scale * sr / 48000.0), (int) line[i].size() - 2);
                gain[i] = std::pow (10.0f, -3.0f * (float) len[i] / (rt60 * (float) sr));
            }
            dampCoef = 0.05f + 0.8f * std::clamp (dampAmt, 0.0f, 1.0f);
            preLen = std::max (1, std::min ((int) (preDelay * 0.12 * sr), (int) pre[0].size() - 2));
        }

        inline void process (float inL, float inR, float& outL, float& outR)
        {
            const int pn = (int) pre[0].size();
            int pr = pw - preLen; if (pr < 0) pr += pn;
            float xl = pre[0][(size_t) pr], xr = pre[1][(size_t) pr];
            pre[0][(size_t) pw] = inL; pre[1][(size_t) pw] = inR;
            if (++pw >= pn) pw = 0;

            float o[N]; float sum = 0;
            for (int i = 0; i < N; ++i)
            {
                int n = (int) line[i].size();
                int r = w[i] - len[i]; if (r < 0) r += n;
                float v = line[i][(size_t) r];
                damp[i] = v + dampCoef * (damp[i] - v);         // high frequencies die faster
                o[i] = damp[i] * gain[i];
                sum += o[i];
            }
            const float h = 2.0f / N;                           // Householder mix: energy-preserving
            for (int i = 0; i < N; ++i)
            {
                float in = (i < N / 2 ? xl : xr) * 1.1f;
                int n = (int) line[i].size();
                line[i][(size_t) w[i]] = o[i] - h * sum + in;
                if (++w[i] >= n) w[i] = 0;
            }
            outL = (o[0] + o[2] + o[4] + o[6]) * 0.5f;
            outR = (o[1] + o[3] + o[5] + o[7]) * 0.5f;
        }
    };

    // ------------------------------------------------------------ bus C: chorus
    struct Chorus
    {
        std::vector<float> buf[2];
        int w = 0;
        double phase = 0;
        double sr = 48000;

        void prepare (double sampleRate) { sr = sampleRate; for (auto& b : buf) b.assign ((size_t) (0.05 * sr) + 4, 0.0f); w = 0; phase = 0; }
        void reset() { for (auto& b : buf) std::fill (b.begin(), b.end(), 0.0f); }

        inline float read (const std::vector<float>& b, float delaySamples) const
        {
            int n = (int) b.size();
            float rp = (float) w - delaySamples; if (rp < 0) rp += (float) n;
            int i0 = (int) rp; float fr = rp - (float) i0; int i1 = i0 + 1; if (i1 >= n) i1 -= n;
            return b[(size_t) i0] + (b[(size_t) i1] - b[(size_t) i0]) * fr;
        }

        inline void process (float inL, float inR, float rate, float depth, float width, float& outL, float& outR)
        {
            const int n = (int) buf[0].size();
            buf[0][(size_t) w] = inL; buf[1][(size_t) w] = inR;
            float hz = 0.08f * std::pow (60.0f, std::clamp (rate, 0.0f, 1.0f));          // 0.08 .. 4.8 Hz
            phase += hz / sr; if (phase >= 1.0) phase -= 1.0;
            float base = 0.009f * (float) sr, sweep = 0.0065f * (float) sr * std::clamp (depth, 0.0f, 1.0f);
            float off = 3.14159265f * std::clamp (width, 0.0f, 1.0f);                     // 0 mono .. 180 degrees wide
            float pl = (float) (phase * 6.28318530718);
            float dl = base + sweep * 0.5f * (1.0f + std::sin (pl));
            float dr = base + sweep * 0.5f * (1.0f + std::sin (pl + off));
            float dl2 = base * 1.6f + sweep * 0.5f * (1.0f + std::sin (pl + 2.1f));
            float dr2 = base * 1.6f + sweep * 0.5f * (1.0f + std::sin (pl + 2.1f + off));
            outL = 0.6f * (read (buf[0], dl) + read (buf[0], dl2));
            outR = 0.6f * (read (buf[1], dr) + read (buf[1], dr2));
            if (++w >= n) w = 0;
        }
    };

    // ------------------------------------------------------------ bus D: reverse
    // Records each slice of time (Length, locked to the beat) and plays it backwards
    // during the next slice, so a hit at the start of a slice becomes a swell that
    // rises into the start of the next one. Slice = how much of the end of the
    // reversed slice you hear; Fade = how gently it fades in.
    struct Reverse
    {
        std::vector<float> rec[2][2];   // [buffer][channel]
        int cur = 0;
        long long lastSeg = -1;
        int filled[2] = { 0, 0 };

        void prepare (double sr) { for (auto& b : rec) for (auto& c : b) c.assign ((size_t) (sr * 6.1) + 4, 0.0f); cur = 0; lastSeg = -1; filled[0] = filled[1] = 0; }
        void reset() { for (auto& b : rec) for (auto& c : b) std::fill (c.begin(), c.end(), 0.0f); lastSeg = -1; filled[0] = filled[1] = 0; }

        // pos: position in samples (from the song when playing), segLen: slice length in samples
        inline void process (float inL, float inR, long long pos, int segLen, float slice, float fade, float& outL, float& outR)
        {
            segLen = std::clamp (segLen, 64, (int) rec[0][0].size() - 2);
            long long seg = pos >= 0 ? pos / segLen : -1;
            int o = (int) (pos - seg * (long long) segLen);
            if (seg != lastSeg)
            {
                if (lastSeg >= 0 && seg == lastSeg + 1) cur ^= 1;     // finished slice becomes the one we play
                else { filled[cur ^ 1] = 0; }                            // jump (loop, locate): nothing to play yet
                filled[cur] = 0;
                lastSeg = seg;
            }
            rec[cur][0][(size_t) o] = inL; rec[cur][1][(size_t) o] = inR;
            filled[cur] = std::max (filled[cur], o + 1);

            outL = outR = 0;
            const int prev = cur ^ 1;
            if (filled[prev] < segLen) return;
            const int start = (int) ((1.0f - std::clamp (slice, 0.05f, 1.0f)) * (float) segLen);
            if (o < start) return;
            const int active = segLen - start;
            const float fadeIn = std::max (1.0f, std::clamp (fade, 0.0f, 1.0f) * 0.9f * (float) active);
            const float fadeOut = std::min (96.0f, (float) active * 0.1f);     // tiny, avoids a click at the slice edge
            float env = std::min (1.0f, (float) (o - start + 1) / fadeIn) * std::min (1.0f, (float) (segLen - o) / std::max (1.0f, fadeOut));
            const int r = segLen - 1 - o;
            outL = rec[prev][0][(size_t) r] * env;
            outR = rec[prev][1][(size_t) r] * env;
        }
    };

    // ------------------------------------------------------------ settings for the buses
    struct FxSettings
    {
        float verbSize = 0.5f, verbDecay = 0.45f, verbDamp = 0.4f, verbPre = 0.1f, verbReturn = 0.8f;
        float dlyFeedback = 0.4f, dlyCutoff = 0.82f, dlyRes = 0.2f, dlyReturn = 0.8f;
        float choRate = 0.3f, choDepth = 0.5f, choWidth = 0.8f, choReturn = 0.8f;
        int revLength = 2; float revSlice = 0.7f, revFade = 0.6f, revReturn = 0.8f;
        float master = 1.0f;     // 1 = unity (gain is master squared)
    };
}
