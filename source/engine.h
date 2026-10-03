#pragma once

// RASPA engine.
// Four lanes. Each lane has a 16-step pattern and one of seven voices (voices.h).
//
// How a lane plays:
//   - Note-on for the lane starts its pattern from step 1, instantly.
//     If it is already playing, it restarts from step 1.
//   - Note-off stops it (hold to play). The last hit rings out naturally.
//   - Latch on + Ableton playing: the lane runs with the transport, locked to
//     the song grid. A note-on still restarts it from step 1 at that instant.
//     With Latch on but Ableton stopped, notes work as hold-to-play.
//
// Plain C++ (no JUCE) so the test program can check it directly.

#include <cmath>
#include <cstdint>
#include <atomic>
#include <vector>
#include <algorithm>
#include "voices.h"
#include "fx.h"

namespace raspa
{
    constexpr int numLanes = 4;
    constexpr int numSteps = 16;
    constexpr int firstNote = 36; // C1 in Ableton

    // step value 0 = off, 1 = ghost, 2 = medium, 3 = accent
    inline float chanceValue (int c)
    {
        static const float table[4] = { 1.0f, 0.75f, 0.5f, 0.25f };
        return table[std::clamp (c, 0, 3)];
    }

    inline float stepVelocity (int v)
    {
        static const float table[4] = { 0.0f, 0.38f, 0.68f, 1.0f };
        return table[std::clamp (v, 0, 3)];
    }

    // ------------------------------------------------------------ Lane
    struct Lane
    {
        std::atomic<int> pattern[numSteps];
        std::atomic<int> displayStep { -1 };       // for the UI playhead
        std::atomic<bool> displayPlaying { false };

        // settings (written by the processor before each block)
        bool latch = false;
        float level = 0.8f;
        float swing = 0.0f;      // 0..1 -> even 16ths pushed late by 0..50% of a step (50%..75% swing)
        float accent = 0.5f;     // 0 flat, 0.5 as written, 1 strong contrast
        float humanize = 0.0f;   // 0..1 -> up to 12 ms late and +-15% velocity, at random
        float pan = 0.0f;        // -1 left .. +1 right (balance: centre leaves both sides at full level)
        float cutoff = 1.0f;     // 0..1 (low-pass: 1 = open, filter off; high-pass: 0 = open)
        bool highpass = false;   // filter type: false = low-pass, true = high-pass
        float res = 0.0f;        // 0..1
        float send[4] = { 0, 0, 0, 0 };   // A reverb, B delay, C chorus, D reverse
        int delayTime = 5;       // index into delayTimeName (5 = 1/8)

        Svf filter;
        LaneDelay echo;
        bool filterOn = false, filterWasHigh = false;

        Voice voice;

        // play state
        int heldCount = 0;
        bool freeRunning = false;
        double freePos = 0;          // steps since note-on
        bool latchRunning = false;
        double restartPpq = 0;       // grid origin while latched
        long long lastIdx = -1;

        // a hit waiting for its swing / humanize delay
        int pendingWait = -1;
        float pendingVel = 0;
        bool pendingAccent = false;
        Rng feel { 0x51f15eedu };

        // stage 6: per-step chance (0 = 100%, 1 = 75%, 2 = 50%, 3 = 25%) and repeats (1..4 hits per step)
        std::atomic<int> chance[numSteps];
        std::atomic<int> ratchet[numSteps];
        int inNote = firstNote;    // MIDI note that launches this lane (learnable)

        // a running ratchet: the extra hits inside one step
        int ratchetLeft = 0, ratchetEvery = 0, ratchetCount = 0, pendingRatchet = 1;
        float ratchetVel = 0; bool ratchetAcc = false; int ratchetStep = 0;

        explicit Lane (uint32_t seed) : voice (seed)
        {
            for (auto& p : pattern) p.store (0);
            for (auto& c : chance) c.store (0);
            for (auto& r : ratchet) r.store (1);
        }

        void resetPlay()
        {
            heldCount = 0; freeRunning = false; freePos = 0;
            latchRunning = false; restartPpq = 0; lastIdx = -1; pendingWait = -1; ratchetLeft = 0;
            displayStep.store (-1); displayPlaying.store (false);
        }
    };

    struct Transport
    {
        double bpm = 120.0;
        bool playing = false;
        double ppq = 0.0;           // quarter notes at block start
    };

    // stage 5: every hit the lanes play is also sent out as MIDI (note on, then a short note off)
    struct MidiOutEvent
    {
        int sample;    // position in this block
        int lane;
        bool on;
        float velocity;
    };

    struct NoteEvent
    {
        int sample;
        int note;
        bool on;
    };

    class Engine
    {
    public:
        Lane lanes[numLanes] { Lane (11), Lane (2222), Lane (33333), Lane (444444) };
        Engine() { for (int i = 0; i < numLanes; ++i) lanes[i].inNote = firstNote + i; }

        // test helpers
        long long hitCount[numLanes] = { 0, 0, 0, 0 };
        std::vector<std::pair<long long, int>> hitLog[numLanes]; // (absolute sample, step)
        bool logging = false;
        int pendingStep[numLanes] = { -2, -2, -2, -2 };

        // MIDI out: filled by process(), read by the plugin after each block
        std::vector<MidiOutEvent> midiOut;
        bool midiOutEnabled = true;
        std::vector<float> velLog[numLanes];
        long long sampleClock = 0;

        FxSettings fx;

        void prepare (double sampleRate)
        {
            sr = sampleRate;
            for (auto& l : lanes) { l.voice.prepare ((float) sr); l.resetPlay(); l.echo.prepare (sr); l.filter.reset(); l.filterOn = false; }
            reverb.prepare (sr); chorus.prepare (sr); reverse.prepare (sr);
            midiOut.clear(); midiOut.reserve (512);
            for (auto& c : offCountdown) c = -1;
            wasPlaying = false; freeClock = 0;
        }

        // Render numSamples into left/right (overwrites). Events must be sorted by sample.
        void process (float* left, float* right, int numSamples,
                      const Transport& t, const std::vector<NoteEvent>& events)
        {
            const double bpm = (t.bpm > 1.0 && std::isfinite (t.bpm)) ? t.bpm : 120.0;
            const double stepsPerSample = bpm / 60.0 * 4.0 / sr;
            const double ppqPerSample = bpm / 60.0 / sr;

            if (! t.playing && wasPlaying)
                for (auto& l : lanes) { l.latchRunning = false; l.restartPpq = 0; }
            wasPlaying = t.playing;

            // per-block settings
            float gl[numLanes], gr[numLanes], dlyLen[numLanes];
            for (int i = 0; i < numLanes; ++i)
            {
                Lane& l = lanes[i];
                float p = std::clamp (l.pan, -1.0f, 1.0f);
                gl[i] = p > 0 ? 1.0f - p : 1.0f;
                gr[i] = p < 0 ? 1.0f + p : 1.0f;
                bool on = (l.highpass ? l.cutoff > 0.001f : l.cutoff < 0.999f) || l.res > 0.001f;
                if (on && ! l.filterOn) l.filter.reset();
                if (on && l.filterOn && l.highpass != l.filterWasHigh) l.filter.reset();   // type switched: start clean
                l.filterOn = on; l.filterWasHigh = l.highpass;
                if (on) l.filter.set (l.highpass ? highpassHz (l.cutoff) : cutoffHz (l.cutoff), l.res, (float) sr);
                l.echo.filt.set (cutoffHz (fx.dlyCutoff), fx.dlyRes * 0.85f, (float) sr);
                dlyLen[i] = (float) (delayTimeBeats (l.delayTime) * 60.0 / bpm * sr);
            }
            reverb.set (fx.verbSize, fx.verbDecay, fx.verbDamp, fx.verbPre);
            const float fb = std::clamp (fx.dlyFeedback, 0.0f, 1.0f) * 0.92f;
            const int revLen = (int) (reverseLengthBeats (fx.revLength) * 60.0 / bpm * sr);
            const float master = fx.master * fx.master;

            midiOut.clear();
            noteLen = (int) std::min (0.03 * sr, 0.9 / stepsPerSample);   // 30 ms, or shorter at very fast tempos

            size_t ev = 0;
            for (int s = 0; s < numSamples; ++s)
            {
                const double ppqNow = t.ppq + s * ppqPerSample;
                curSample = s;
                for (int i = 0; i < numLanes; ++i)
                    if (offCountdown[i] >= 0 && offCountdown[i]-- == 0) pushOut (s, i, false, 0.0f);

                while (ev < events.size() && events[ev].sample <= s)
                    handleNote (events[ev++], t.playing, ppqNow);

                float dryL = 0, dryR = 0, aL = 0, aR = 0, bL = 0, bR = 0, cL = 0, cR = 0, dL = 0, dR = 0;
                for (int i = 0; i < numLanes; ++i)
                {
                    Lane& l = lanes[i];
                    advance (l, i, t.playing, ppqNow, stepsPerSample);
                    float y = l.voice.active() ? l.voice.process() * l.level : 0.0f;
                    if (l.filterOn) y = l.highpass ? l.filter.hp (y) : l.filter.lp (y);
                    float yl = y * gl[i], yr = y * gr[i];
                    dryL += yl; dryR += yr;
                    aL += yl * l.send[0]; aR += yr * l.send[0];
                    float e = l.echo.process (y * l.send[1], dlyLen[i], fb);
                    bL += e * gl[i]; bR += e * gr[i];
                    cL += yl * l.send[2]; cR += yr * l.send[2];
                    dL += yl * l.send[3]; dR += yr * l.send[3];
                }

                float rvL, rvR, chL, chR, reL, reR;
                reverb.process (aL, aR, rvL, rvR);
                chorus.process (cL, cR, fx.choRate, fx.choDepth, fx.choWidth, chL, chR);
                long long pos = t.playing ? (long long) std::floor (ppqNow * 60.0 / bpm * sr + 0.5) : freeClock;
                reverse.process (dL, dR, pos, revLen, fx.revSlice, fx.revFade, reL, reR);

                float outL = dryL + fx.verbReturn * rvL + fx.dlyReturn * bL + fx.choReturn * chL + fx.revReturn * reL;
                float outR = dryR + fx.verbReturn * rvR + fx.dlyReturn * bR + fx.choReturn * chR + fx.revReturn * reR;
                left[s]  = std::tanh (outL * master * 0.9f) / 0.9f;   // soft safety clip
                right[s] = std::tanh (outR * master * 0.9f) / 0.9f;
                ++sampleClock; ++freeClock;
            }

            for (auto& l : lanes)
            {
                bool running = (l.latch && t.playing) ? l.latchRunning : l.freeRunning;
                l.displayPlaying.store (running);
                l.displayStep.store (running && l.lastIdx >= 0 ? (int) (l.lastIdx % numSteps) : -1);
            }
        }

    private:
        double sr = 48000;
        bool wasPlaying = false;
        long long freeClock = 0;
        Reverb reverb;
        Chorus chorus;
        Reverse reverse;

        void handleNote (const NoteEvent& e, bool playing, double ppqNow)
        {
            for (auto& l : lanes)
                if (l.inNote == e.note) handleLaneNote (l, e.on, playing, ppqNow);
        }

        void handleLaneNote (Lane& l, bool on, bool playing, double ppqNow)
        {
            if (on)
            {
                ++l.heldCount;
                if (l.latch && playing)
                {
                    l.restartPpq = ppqNow;
                    l.latchRunning = true;
                }
                else
                {
                    l.freeRunning = true;
                    l.freePos = 0;
                }
                l.lastIdx = -1;      // step 1 fires on this exact sample
                l.pendingWait = -1;  // forget a swung hit from before the restart
                l.ratchetLeft = 0;   // and any repeats still running
            }
            else
            {
                l.heldCount = std::max (0, l.heldCount - 1);
                if (l.heldCount == 0)
                    l.freeRunning = false;
            }
        }

        int curSample = 0, noteLen = 1440;
        int offCountdown[numLanes] = { -1, -1, -1, -1 };

        void pushOut (int sample, int lane, bool on, float vel)
        {
            if (midiOut.size() < midiOut.capacity()) midiOut.push_back ({ sample, lane, on, vel });
        }

        void fire (Lane& l, int laneIndex, float vel, bool acc, int step)
        {
            if (midiOutEnabled)
            {
                if (offCountdown[laneIndex] >= 0) pushOut (curSample, laneIndex, false, 0.0f);   // close the previous note first
                pushOut (curSample, laneIndex, true, vel);
                offCountdown[laneIndex] = noteLen - 1;
            }
            l.voice.trigger (vel, acc);
            ++hitCount[laneIndex];
            if (logging) { hitLog[laneIndex].push_back ({ sampleClock, step }); velLog[laneIndex].push_back (vel); }
        }

        // the first hit of a step; with Repeats > 1 the rest follow evenly inside the step
        void fireFirst (Lane& l, int laneIndex, float vel, bool acc, int step, int reps, double stepsPerSample)
        {
            fire (l, laneIndex, vel, acc, step);
            l.ratchetLeft = reps - 1;
            if (reps > 1)
            {
                l.ratchetEvery = std::max (1, (int) std::lround (1.0 / stepsPerSample / reps));
                l.ratchetCount = l.ratchetEvery;
                l.ratchetVel = vel; l.ratchetAcc = false; l.ratchetStep = step;
            }
        }

        void advance (Lane& l, int laneIndex, bool playing, double ppqNow, double stepsPerSample)
        {
            if (l.ratchetLeft > 0 && --l.ratchetCount <= 0)
            {
                l.ratchetVel *= 0.82f;   // each repeat a little softer, like a real roll
                fire (l, laneIndex, l.ratchetVel, l.ratchetAcc, l.ratchetStep);
                --l.ratchetLeft;
                l.ratchetCount = l.ratchetEvery;
            }
            if (l.pendingWait >= 0)
            {
                if (l.pendingWait == 0) { fireFirst (l, laneIndex, l.pendingVel, l.pendingAccent, pendingStep[laneIndex], l.pendingRatchet, stepsPerSample); }
                --l.pendingWait;
            }

            long long idx;

            if (l.latch && playing)
            {
                double pos = (ppqNow - l.restartPpq) * 4.0;
                if (! l.latchRunning)
                {
                    // Latch just engaged or transport just started: join the grid.
                    l.latchRunning = true;
                    long long fl = (long long) std::floor (pos + 1e-9);
                    double frac = pos - (double) fl;
                    l.lastIdx = (frac < stepsPerSample) ? fl - 1 : fl;
                }
                idx = (long long) std::floor (pos + 1e-9);
            }
            else
            {
                l.latchRunning = false;
                if (! l.freeRunning) return;
                idx = (long long) std::floor (l.freePos + 1e-9);
                l.freePos += stepsPerSample;
            }

            if (idx > l.lastIdx)
            {
                if (idx >= 0)
                {
                    int step = (int) (idx % numSteps);
                    int v = l.pattern[step].load (std::memory_order_relaxed);
                    if (v > 0 && l.chance[step].load (std::memory_order_relaxed) > 0
                        && l.feel.uni() >= chanceValue (l.chance[step].load (std::memory_order_relaxed)))
                        v = 0;   // Chance: this time the step stays silent
                    if (v > 0)
                    {
                        const int reps = std::clamp (l.ratchet[step].load (std::memory_order_relaxed), 1, 4);
                        // Accent: shapes the contrast between ghost, medium and accent
                        float gamma = std::pow (2.0f, (l.accent - 0.5f) * 3.0f);
                        float vel = std::pow (stepVelocity (v), gamma);
                        // Swing: every second 16th lands late; Humanize: small random lateness
                        double delay = 0;
                        if (step % 2 == 1) delay += l.swing * 0.5 / stepsPerSample;
                        if (l.humanize > 0.0f)
                        {
                            delay += l.feel.uni() * l.humanize * 0.012 * sr;
                            vel *= 1.0f + l.humanize * 0.15f * l.feel.bi();
                        }
                        vel = std::clamp (vel, 0.02f, 1.0f);
                        int wait = (int) std::lround (delay);
                        if (wait <= 0) fireFirst (l, laneIndex, vel, v == 3, step, reps, stepsPerSample);
                        else
                        {
                            if (l.pendingWait >= 0) fire (l, laneIndex, l.pendingVel, l.pendingAccent, -2); // never drop a hit
                            l.pendingWait = wait - 1; l.pendingVel = vel; l.pendingAccent = (v == 3); l.pendingRatchet = reps;
                            if (logging) pendingStep[laneIndex] = step;
                        }
                    }
                }
                l.lastIdx = idx;
            }
        }
    };
}
