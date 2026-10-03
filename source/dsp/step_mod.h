// step_mod.h — ÁCIDO step modulator (phase 4a)
// One 16-step lane per knob. Lanes are relative: they move the knob up and
// down around where you set it. They follow Ableton's song position, so a
// pattern always lines up with the bar, and freeze while the transport is
// stopped.
//
// Lanes are written by the plugin window and read by the audio thread, so
// every setting is an atomic value.
#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <cmath>
#include <memory>
#include <vector>

namespace acido
{

constexpr int kSteps = 16;

// Lane rates, in beats (quarter notes) per step.
inline float laneRateBeats (int rateIndex)
{
    static const float beats[4] = { 0.125f, 0.25f, 0.5f, 1.0f };   // 1/32, 1/16, 1/8, 1/4
    return beats[std::clamp (rateIndex, 0, 3)];
}

struct Lane
{
    std::array<std::atomic<float>, kSteps> steps;   // -1..+1 each
    std::atomic<int>   length { kSteps };           // 1..16
    std::atomic<int>   rate   { 1 };                // 0 = 1/32, 1 = 1/16, 2 = 1/8, 3 = 1/4
    std::atomic<bool>  smooth { false };            // false = Step, true = Smooth
    std::atomic<float> depth  { 0.5f };             // 0..1
    std::atomic<int>   playStep { -1 };             // for the window's playhead only

    Lane() { clear(); }

    void clear() { for (auto& s : steps) s.store (0.0f); }

    bool isActive() const
    {
        if (depth.load() <= 0.0f) return false;
        const int len = length.load();
        for (int i = 0; i < len; ++i) if (std::fabs (steps[(size_t) i].load()) > 1e-6f) return true;
        return false;
    }

    // Draws a shape across the lane's current length (steps past it are cleared).
    enum Shape { Sine, Triangle, Saw, Random, Clear };
    void fill (Shape shape, unsigned seed = 1)
    {
        const int len = length.load();
        uint32_t r = seed * 2654435761u + 1u;
        for (int i = 0; i < kSteps; ++i)
        {
            float v = 0.0f;
            if (i < len)
            {
                const float t = (float) i / (float) len;
                switch (shape)
                {
                    case Sine:     v = std::sin (6.28318530718f * t); break;
                    case Triangle: v = 1.0f - 4.0f * std::fabs (t - 0.5f); break;
                    case Saw:      v = len > 1 ? 1.0f - 2.0f * (float) i / (float) (len - 1) : 1.0f; break;
                    case Random:   r ^= r << 13; r ^= r >> 17; r ^= r << 5;
                                   v = (float) ((double) r / 4294967295.0 * 2.0 - 1.0); break;
                    case Clear:    v = 0.0f; break;
                }
            }
            steps[(size_t) i].store (std::clamp (v, -1.0f, 1.0f));
        }
    }
};

// The raw lane value at a song position (in beats), before depth and smoothing.
inline float laneValueAt (const Lane& lane, double ppq, int* stepOut = nullptr)
{
    const int len = std::clamp (lane.length.load(), 1, kSteps);
    const double pos = std::max (0.0, ppq) / laneRateBeats (lane.rate.load());
    const double whole = std::floor (pos);
    const int idx = (int) std::fmod (whole, (double) len);
    if (stepOut) *stepOut = idx;

    const float a = lane.steps[(size_t) idx].load();
    if (! lane.smooth.load()) return a;

    const float b = lane.steps[(size_t) ((idx + 1) % len)].load();
    const float f = (float) (pos - whole);
    return a + (b - a) * f;                     // Smooth: glide toward the next step
}

// Turns lanes into per-knob offsets for the audio thread.
class StepModulator
{
public:
    void prepare (double sampleRate, size_t numLanes)
    {
        sr = sampleRate;
        offsets.assign (numLanes, 0.0f);
        targets.assign (numLanes, 0.0f);
    }

    // Call once per small chunk of audio. `ppq` is the song position (beats) at
    // the start of the chunk. While stopped the offsets stay where they were.
    void process (const std::vector<std::unique_ptr<Lane>>& lanes, bool playing, double ppq, int numSamples)
    {
        if (offsets.size() != lanes.size()) { offsets.assign (lanes.size(), 0.0f); targets.assign (lanes.size(), 0.0f); }

        // 5 ms smoothing so step jumps never click.
        const float c = 1.0f - std::exp (-(float) numSamples / (0.005f * (float) sr));

        for (size_t i = 0; i < lanes.size(); ++i)
        {
            const Lane& lane = *lanes[i];
            if (playing)
            {
                int step = -1;
                targets[i] = laneValueAt (lane, ppq, &step) * std::clamp (lane.depth.load(), 0.0f, 1.0f);
                lanes[i]->playStep.store (step);
            }
            offsets[i] += (targets[i] - offsets[i]) * c;
        }
    }

    // Offset for lane i, in the knob's 0..1 range (add it to the knob's position).
    float offset (size_t i) const { return i < offsets.size() ? offsets[i] : 0.0f; }

private:
    double sr = 44100.0;
    std::vector<float> offsets, targets;
};

// Knob position (0..1) plus a lane offset, kept inside the knob's range.
inline float applyOffset (float knob01, float offset) { return std::clamp (knob01 + offset, 0.0f, 1.0f); }

} // namespace acido
