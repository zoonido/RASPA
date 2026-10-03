// acid_voice.h — ÁCIDO voice (phase 4b)
// Plain C++ with no JUCE dependency, so it can be tested on its own.
// One voice: Saw / Square / Sine / FM oscillator with a Shape control, up to
// 7 unison copies spread in pitch and stereo, sub oscillator, 24 dB ladder
// low-pass with filter FM, filter envelope, accent, slide, drift, velocity
// and a note-following noise layer.
#pragma once

#include <algorithm>
#include <cmath>
#include <array>
#include <cstdint>
#include <vector>

namespace acido
{

// ---------------------------------------------------------------------------
// Settings the voice reads every block (already in real units)
// ---------------------------------------------------------------------------
struct VoiceParams
{
    int    wave      = 0;      // 0 = Saw, 1 = Square, 2 = Sine, 3 = FM
    float  tune      = 0.0f;   // semitones, -24..+24
    float  shape     = 0.20f;  // 0..1  (Saw: morph to triangle, Square: pulse width, Sine: fold, FM: index)
    float  subLevel  = 0.40f;  // 0..1
    int    subOct    = 1;      // 1 or 2 octaves below
    float  cutoffHz  = 420.0f; // 20..18000
    float  reso      = 0.82f;  // 0..1
    float  envMod    = 0.60f;  // 0..1
    float  decayMs   = 340.0f; // 30..3000
    float  filterFm  = 0.0f;   // 0..1
    float  accent    = 0.70f;  // 0..1
    float  slideMs   = 60.0f;  // 10..1000
    float  drift     = 0.20f;  // 0..1
    float  velo      = 0.50f;  // 0..1
    float  noise     = 0.0f;   // 0..1
    int    unison    = 1;      // 1..7 oscillator copies
    float  neblina   = 0.0f;   // 0..1 (here: extra, wobbling unison detune)
};

constexpr int kMaxUnison = 7;

// Unison detune: ±8 cents on its own, widened up to ±33 cents by Neblina.
inline float unisonSpreadCents (float neblina) { return 8.0f + 25.0f * neblina; }

// ---------------------------------------------------------------------------
// Small helpers
// ---------------------------------------------------------------------------
constexpr double kPi    = 3.14159265358979323846;
constexpr double kTwoPi = 2.0 * kPi;

inline float dbToGain (float db)   { return std::pow (10.0f, db / 20.0f); }
inline float noteToHz (float note) { return 440.0f * std::pow (2.0f, (note - 69.0f) / 12.0f); }

// Coefficient for a one-pole smoother that covers ~63% of the way in `ms`.
inline float smoothCoef (float ms, double sr)
{
    const double samples = std::max (1.0, (double) ms * 0.001 * sr);
    return (float) (1.0 - std::exp (-1.0 / samples));
}

// PolyBLEP / PolyBLAMP: remove most of the aliasing from sharp edges and corners.
inline float polyBlep (double t, double dt)
{
    if (t < dt)       { t /= dt;            return (float) (t + t - t * t - 1.0); }
    if (t > 1.0 - dt) { t = (t - 1.0) / dt; return (float) (t * t + t + t + 1.0); }
    return 0.0f;
}

inline float polyBlamp (double t, double dt)
{
    if (t < dt)       { t = t / dt - 1.0;         return (float) (-t * t * t / 3.0); }
    if (t > 1.0 - dt) { t = (t - 1.0) / dt + 1.0; return (float) ( t * t * t / 3.0); }
    return 0.0f;
}

// Fast, smooth saturation.
inline float softClip (float x)
{
    if (x >  3.0f) return  1.0f;
    if (x < -3.0f) return -1.0f;
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

// Small, fast random numbers; seeded so tests always hear the same thing.
struct Random
{
    uint32_t state = 0x1234567u;
    float next()   // -1..1
    {
        state ^= state << 13; state ^= state >> 17; state ^= state << 5;
        return (float) ((double) state / 4294967295.0 * 2.0 - 1.0);
    }
};

// ---------------------------------------------------------------------------
// 4-pole ladder low-pass (zero-delay-feedback form with a soft-clipped input)
// ---------------------------------------------------------------------------
class LadderFilter
{
public:
    void reset() { s1 = s2 = s3 = s4 = 0.0f; }

    float process (float x, float cutoffHz, float k, double sr)
    {
        const float fc = std::min (cutoffHz, (float) (0.45 * sr));
        const float g  = (float) std::tan (kPi * fc / sr);
        const float G  = g / (1.0f + g);
        const float b  = 1.0f / (1.0f + g);

        const float S  = b * (G * G * G * s1 + G * G * s2 + G * s3 + s4);
        const float G4 = G * G * G * G;
        const float y4Estimate = (G4 * x + S) / (1.0f + k * G4);

        // Partial bass compensation: resonance thins the low end, as on a 303.
        // The boost applies to the input only; boosting the feedback too would
        // push the filter into self-oscillation far too early.
        const float u = softClip (x * (1.0f + 0.25f * k) - k * y4Estimate);

        const float y1 = stage (u,  s1, G);
        const float y2 = stage (y1, s2, G);
        const float y3 = stage (y2, s3, G);
        return          stage (y3, s4, G);
    }

private:
    static float stage (float in, float& s, float G)
    {
        const float v = (in - s) * G;
        const float y = v + s;
        s = y + v;
        return y;
    }

    float s1 = 0.0f, s2 = 0.0f, s3 = 0.0f, s4 = 0.0f;
};

// ---------------------------------------------------------------------------
// The voice
// ---------------------------------------------------------------------------
class AcidVoice
{
public:
    void prepare (double sampleRate, uint32_t seed = 0x1234567u)
    {
        sr = sampleRate;
        filterL.reset(); filterR.reset();
        subPhase = 0.0;
        filterEnv = accentEnv = ampEnv = 0.0f;
        gate = false;
        smoothedCutoff = -1.0f;
        rng = Random(); rng.state = seed ? seed : 1u;
        wander = wanderTarget = 0.0f;
        wanderCounter = 0;
        notePitchRnd = noteCutRnd = 0.0f;

        // Unison copies start at different points of the wave and wobble at
        // their own slow rates, so they never lock together.
        for (int k = 0; k < kMaxUnison; ++k)
        {
            phases[(size_t) k] = k == 0 ? 0.0 : 0.5 + 0.5 * (double) rng.next();
            wobblePhase[(size_t) k] = 0.5 + 0.5 * (double) rng.next();
            wobbleRate[(size_t) k] = 0.1 + 0.2 * (1.0 + (double) rng.next());   // 0.1..0.5 Hz
        }
    }

    // A note with nothing held: start on pitch and restart the envelopes.
    void noteOn (int midiNote, int velocity) { noteOnFrom (midiNote, velocity, midiNote); }

    // Poly: start at `fromNote` (the last note played) and glide to the new one,
    // restarting the envelopes.
    void noteOnFrom (int midiNote, int velocity, int fromNote)
    {
        targetNote  = (float) midiNote;
        currentNote = (float) fromNote;
        setVelocity (velocity);
        filterEnv = 1.0f;
        if (accented) accentEnv = 1.0f;
        notePitchRnd = rng.next();                // Drift: each note lands slightly differently
        noteCutRnd   = rng.next();
        gate = true;
    }

    // A note that overlaps the previous one: glide, keep the envelopes running.
    void slideTo (int midiNote, int velocity)
    {
        targetNote = (float) midiNote;
        setVelocity (velocity);
        gate = true;
    }

    void noteOff() { gate = false; }

    bool isActive() const { return gate || ampEnv > 0.0001f; }
    bool isGateOn() const { return gate; }

    // Read-only views used by the automated checks.
    float getCurrentNote() const { return currentNote; }
    float getFilterEnv() const   { return filterEnv; }

    // Mono convenience (used by the checks): renders and ADDS the left channel.
    void render (float* out, int n, const VoiceParams& p)
    {
        std::vector<float> l ((size_t) n, 0.0f), r ((size_t) n, 0.0f);
        renderStereo (l.data(), r.data(), n, p);
        for (int i = 0; i < n; ++i) out[i] += l[(size_t) i];
    }

    // Renders `n` samples and ADDS them into L and R. If `noiseOut` is given, the
    // noise layer goes there instead (the engine mixes it in as a send).
    void renderStereo (float* L, float* R, int n, const VoiceParams& p, float* noiseOut = nullptr)
    {
        if (! isActive()) return;

        const float decayCoef   = std::exp (-1.0f / (0.001f * p.decayMs * (float) sr / 4.6f));
        const float accDecay    = std::exp (-1.0f / (0.001f * 200.0f * (float) sr / 4.6f));
        const float glideCoef   = smoothCoef (p.slideMs / 3.0f, sr);
        const float ampAttack   = smoothCoef (1.0f, sr);
        const float ampRelease  = smoothCoef (6.0f, sr);
        const float paramSmooth = smoothCoef (20.0f, sr);
        const float wanderCoef  = smoothCoef (300.0f, sr);
        const float k           = 4.2f * p.reso;
        const float envOctaves  = 1.0f + 5.0f * p.envMod;
        const float accOctaves  = 2.5f * p.accent;
        const float velGain     = accented ? 1.0f + p.accent
                                           : (1.0f - p.velo) + p.velo * ((float) noteVelocity / 99.0f);
        const float fmIndex     = 6.0f * p.shape;
        const float foldGain    = 1.0f + 4.0f * p.shape;
        const float pulseWidth  = 0.5f + (p.shape - 0.5f) * 0.9f;   // 5%..95%
        const double subDiv     = p.subOct >= 2 ? 4.0 : 2.0;
        const int   wanderEvery = std::max (1, (int) (sr * 0.05));

        // Unison layout: copies spread evenly in pitch and across the stereo field.
        const int copies = std::clamp (p.unison, 1, kMaxUnison);
        const bool stereo = copies > 1;
        const float spreadCents = unisonSpreadCents (p.neblina);
        const float wobbleCents = 6.0f * p.neblina;              // Neblina's slow wobble
        const float copyGain = 1.0f / std::sqrt ((float) copies);
        std::array<float, kMaxUnison> pos {}, panL {}, panR {};
        for (int c = 0; c < copies; ++c)
        {
            pos[(size_t) c] = copies == 1 ? 0.0f : -1.0f + 2.0f * (float) c / (float) (copies - 1);
            const float pan = 0.8f * pos[(size_t) c];            // -0.8..0.8
            const float angle = (float) ((pan + 1.0f) * kPi * 0.25);
            panL[(size_t) c] = (copies == 1 ? 1.0f : std::cos (angle) * 1.41421356f) * copyGain;
            panR[(size_t) c] = (copies == 1 ? 1.0f : std::sin (angle) * 1.41421356f) * copyGain;
        }

        if (smoothedCutoff < 0.0f) smoothedCutoff = p.cutoffHz;

        for (int i = 0; i < n; ++i)
        {
            // Drift: a slow random wander on top of each note's own offset.
            if (++wanderCounter >= wanderEvery) { wanderCounter = 0; wanderTarget = rng.next(); }
            wander += (wanderTarget - wander) * wanderCoef;
            const float driftSemis   = p.drift * (0.15f * notePitchRnd + 0.08f * wander);
            const float driftOctaves = p.drift * (0.25f * noteCutRnd + 0.10f * wander);

            // Pitch (with slide) ---------------------------------------------
            currentNote += (targetNote - currentNote) * glideCoef;
            const double freq = noteToHz (currentNote + p.tune + driftSemis);

            // Oscillator copies ---------------------------------------------
            float oscL = 0.0f, oscR = 0.0f, oscMono = 0.0f;
            for (int c = 0; c < copies; ++c)
            {
                const size_t ci = (size_t) c;
                double f = freq;
                if (stereo)
                {
                    wobblePhase[ci] += wobbleRate[ci] / sr;
                    if (wobblePhase[ci] >= 1.0) wobblePhase[ci] -= 1.0;
                    const float cents = pos[ci] * spreadCents + wobbleCents * (float) std::sin (kTwoPi * wobblePhase[ci]);
                    f *= std::exp2 ((double) cents / 1200.0);
                }
                const double dt = std::min (f / sr, 0.49);
                const float o = oscillator (p.wave, phases[ci], dt, p.shape, pulseWidth, foldGain, fmIndex);
                phases[ci] += dt; if (phases[ci] >= 1.0) phases[ci] -= 1.0;

                oscL += o * panL[ci];
                oscR += o * panR[ci];
                oscMono += o;
            }
            oscMono /= (float) copies;

            const float sub = (float) std::sin (kTwoPi * subPhase);
            subPhase += std::min (freq / sr, 0.49) / subDiv; if (subPhase >= 1.0) subPhase -= 1.0;

            // Envelopes -------------------------------------------------------
            filterEnv *= decayCoef;
            accentEnv *= accDecay;
            const float ampTarget = gate ? velGain : 0.0f;
            ampEnv += (ampTarget - ampEnv) * (ampTarget > ampEnv ? ampAttack : ampRelease);

            // Filter ----------------------------------------------------------
            smoothedCutoff += (p.cutoffHz - smoothedCutoff) * paramSmooth;
            const float octaves = p.envMod * filterEnv * envOctaves
                                + (accented ? accOctaves * accentEnv : 0.0f)
                                + driftOctaves
                                - p.filterFm * 3.0f * oscMono;      // Filter FM: opens the filter on each wave edge
            const float fc = std::clamp (smoothedCutoff * std::exp2 (octaves), 20.0f, 18000.0f);

            // The sub skips the filter: Cutoff and Resonance never thin it out. A touch
            // of saturation adds overtones so it's felt on small speakers too.
            const float subOut = p.subLevel > 0.0f ? 0.75f * p.subLevel * std::tanh (1.6f * sub) * 1.08f : 0.0f;
            const float outL = filterL.process (0.8f * oscL, fc, k, sr) + subOut;
            const float outR = (stereo ? filterR.process (0.8f * oscR, fc, k, sr) : outL - subOut) + subOut;

            // Noise follows the note's volume, so it never hisses between notes.
            const float noise = p.noise > 0.0f ? p.noise * rng.next() * ampEnv : 0.0f;

            if (noiseOut != nullptr) { noiseOut[i] += noise; L[i] += outL * ampEnv; R[i] += outR * ampEnv; }
            else                     { L[i] += (outL + 0.3f * noise) * ampEnv; R[i] += (outR + 0.3f * noise) * ampEnv; }
        }
    }

private:
    static float oscillator (int wave, double phase, double dt, float shape, float pw, float foldGain, float fmIndex)
    {
        switch (wave)
        {
            case 1:  // Square with pulse width
            {
                float s = phase < pw ? 1.0f : -1.0f;
                s += polyBlep (phase, dt);
                s -= polyBlep (std::fmod (phase + 1.0 - pw, 1.0), dt);
                return s - (2.0f * pw - 1.0f);                        // remove the DC offset
            }
            case 2:  // Sine with wavefolding
            {
                const float x = foldGain * (float) std::sin (kTwoPi * phase);
                return (float) (std::asin (std::sin (x * kPi / 2.0)) * 2.0 / kPi);
            }
            case 3:  // 2-operator FM, modulator at 2x the pitch
                return (float) std::sin (kTwoPi * phase + fmIndex * std::sin (2.0 * kTwoPi * phase));

            default: // Saw morphing to triangle
            {
                float saw = (float) (2.0 * phase - 1.0) - polyBlep (phase, dt);
                float tri = phase < 0.5 ? (float) (4.0 * phase - 1.0) : (float) (3.0 - 4.0 * phase);
                tri += 4.0f * (float) dt * polyBlamp (phase, dt);
                tri -= 4.0f * (float) dt * polyBlamp (std::fmod (phase + 0.5, 1.0), dt);
                return saw + (tri - saw) * shape;
            }
        }
    }

    void setVelocity (int v)
    {
        accented = v >= 100;
        noteVelocity = std::clamp (v, 1, 127);
    }

    double sr = 44100.0;
    std::array<double, kMaxUnison> phases {}, wobblePhase {}, wobbleRate {};
    double subPhase = 0.0;
    float  currentNote = 36.0f, targetNote = 36.0f;
    float  filterEnv = 0.0f, accentEnv = 0.0f, ampEnv = 0.0f;
    float  smoothedCutoff = -1.0f;
    float  notePitchRnd = 0.0f, noteCutRnd = 0.0f, wander = 0.0f, wanderTarget = 0.0f;
    int    wanderCounter = 0, noteVelocity = 100;
    bool   gate = false, accented = false;
    Random rng;
    LadderFilter filterL, filterR;
};

} // namespace acido
