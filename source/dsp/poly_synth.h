// poly_synth.h — ÁCIDO poly mode (phase 4b)
// Up to 8 voices. Every new note glides from the last note played (over the
// Slide time) and restarts its envelopes. When all voices are busy, the
// oldest note is taken over. Unison is capped so Poly never runs more than
// 24 oscillators at once.
#pragma once

#include "acid_voice.h"

namespace acido
{

constexpr int kMaxVoices = 8;
constexpr int kMaxOscillators = 24;

// Unison copies allowed for a given voice count (Poly keeps the total at 24 or less).
inline int unisonLimit (int unison, int voices)
{
    return std::clamp (std::min (unison, kMaxOscillators / std::max (1, voices)), 1, kMaxUnison);
}

class PolySynth
{
public:
    void prepare (double sampleRate)
    {
        for (int v = 0; v < kMaxVoices; ++v)
        {
            voices[(size_t) v].prepare (sampleRate, 0x9E3779B9u * (uint32_t) (v + 1));   // each voice drifts its own way
            notes[(size_t) v] = -1;
            ages[(size_t) v] = 0;
        }
        lastNote = -1;
        counter = 0;
    }

    void setVoiceCount (int n)
    {
        n = std::clamp (n, 2, kMaxVoices);
        if (n < voiceCount)
            for (int v = n; v < kMaxVoices; ++v) { voices[(size_t) v].noteOff(); notes[(size_t) v] = -1; }
        voiceCount = n;
    }

    void noteOn (int note, int velocity)
    {
        // Same key again while still held: restart that voice.
        int v = findVoice (note);
        if (v < 0) v = freeVoice();
        if (v < 0) v = oldestVoice();

        voices[(size_t) v].noteOnFrom (note, velocity, lastNote >= 0 ? lastNote : note);
        notes[(size_t) v] = note;
        ages[(size_t) v] = ++counter;
        lastNote = note;
    }

    void noteOff (int note)
    {
        for (int v = 0; v < kMaxVoices; ++v)
            if (notes[(size_t) v] == note) { voices[(size_t) v].noteOff(); notes[(size_t) v] = -1; }
    }

    void allNotesOff()
    {
        for (int v = 0; v < kMaxVoices; ++v) { voices[(size_t) v].noteOff(); notes[(size_t) v] = -1; }
    }

    void renderStereo (float* L, float* R, int n, VoiceParams p, float* noiseOut = nullptr)
    {
        p.unison = unisonLimit (p.unison, voiceCount);
        for (auto& v : voices) v.renderStereo (L, R, n, p, noiseOut);   // idle voices return straight away
    }

    int activeVoices() const
    {
        int count = 0;
        for (const auto& v : voices) if (v.isGateOn()) ++count;
        return count;
    }
    const AcidVoice& getVoice (int v) const { return voices[(size_t) v]; }

private:
    int findVoice (int note) const
    {
        for (int v = 0; v < voiceCount; ++v) if (notes[(size_t) v] == note) return v;
        return -1;
    }
    int freeVoice() const
    {
        for (int v = 0; v < voiceCount; ++v) if (notes[(size_t) v] < 0 && ! voices[(size_t) v].isActive()) return v;
        for (int v = 0; v < voiceCount; ++v) if (notes[(size_t) v] < 0) return v;   // releasing voice
        return -1;
    }
    int oldestVoice() const
    {
        int best = 0;
        for (int v = 1; v < voiceCount; ++v) if (ages[(size_t) v] < ages[(size_t) best]) best = v;
        return best;
    }

    std::array<AcidVoice, kMaxVoices> voices;
    std::array<int, kMaxVoices> notes {};
    std::array<uint64_t, kMaxVoices> ages {};
    int voiceCount = 6, lastNote = -1;
    uint64_t counter = 0;
};

} // namespace acido
