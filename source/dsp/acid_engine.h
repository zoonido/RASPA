// acid_engine.h — ÁCIDO sound engine (phase 4b)
// Everything the plugin renders, in signal order:
// voices (Mono or Poly, with Unison) -> Neblina texture -> Drive send ->
// Chorus -> Delay -> Reverb send -> Crush -> Comp -> Noise send -> Volume.
// Drive and Noise are sends: they add on top of the dry sound, never replace it.
#pragma once

#include "mono_synth.h"
#include "poly_synth.h"
#include "neblina.h"
#include "drive.h"
#include "effects.h"

namespace acido
{

// Delay note values for Sync mode, in beats (quarter notes).
inline const std::array<float, 12>& syncDivisionBeats()
{
    static const std::array<float, 12> beats {
        0.125f,          // 1/32
        1.0f / 6.0f,     // 1/16 triplet
        0.25f,           // 1/16
        0.375f,          // 1/16 dotted
        1.0f / 3.0f,     // 1/8 triplet
        0.5f,            // 1/8
        0.75f,           // 1/8 dotted
        2.0f / 3.0f,     // 1/4 triplet
        1.0f,            // 1/4
        1.5f,            // 1/4 dotted
        2.0f,            // 1/2
        4.0f };          // 1 bar
    return beats;
}

struct SoundParams
{
    VoiceParams voice;         // includes Unison and Neblina's detune
    bool  poly      = false;   // Mono / Poly
    int   voices    = 6;       // 2..8 in Poly
    float drive     = 0.30f;   // 0..1
    float warmth    = 0.45f;   // 0..1
    float chorusTone = 0.60f;  // 0..1
    float chorusMix = 0.0f;    // 0..1
    bool  delaySync = true;
    int   delayDiv  = 6;       // index into syncDivisionBeats(), 6 = 1/8 dotted
    float delayMs   = 375.0f;  // 1..2000 (MS mode)
    float delayFb   = 0.45f;   // 0..0.95
    float delayMix  = 0.20f;   // 0..1
    int   reverbType = Reverb::Plate;
    float reverbSize = 0.62f;  // 0..1
    float reverbMix = 0.15f;   // 0..1 — reverb Send (added on top of the dry sound)
    float crush     = 0.0f;    // 0..1
    float comp      = 0.35f;   // 0..1
    float volumeDb  = -3.0f;   // -60..+6
    double bpm      = 120.0;   // from Ableton; 120 if unknown
};

class AcidEngine
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        synth.prepare (sampleRate);
        poly.prepare (sampleRate);
        neblina.prepare (sampleRate);
        drive.prepare (sampleRate);
        noiseHp1 = noiseHp2 = noiseLp = {};
        noiseLevel = {};
        noiseBuf.assign (4096, 0.0f);
        polyMode = false;
        sideHp1 = sideHp2 = {};
        chorus.prepare (sampleRate);
        delay.prepare (sampleRate);
        reverb.prepare (sampleRate);
        crusher.prepare (sampleRate);
        comp.prepare (sampleRate);
        volume = -1.0f;
    }

    // Switching Mono/Poly lets the old mode's notes ring out and sends new
    // notes to the new mode.
    void setMode (bool usePoly, int voiceCount)
    {
        poly.setVoiceCount (voiceCount);
        if (usePoly == polyMode) return;
        if (polyMode) poly.allNotesOff(); else synth.allNotesOff();
        polyMode = usePoly;
    }

    void noteOn (int note, int velocity) { if (polyMode) poly.noteOn (note, velocity); else synth.noteOn (note, velocity); }
    void noteOff (int note)              { poly.noteOff (note); synth.noteOff (note); }
    void allNotesOff()                   { poly.allNotesOff(); synth.allNotesOff(); }

    static float delaySeconds (const SoundParams& p)
    {
        if (! p.delaySync) return p.delayMs * 0.001f;
        const auto& beats = syncDivisionBeats();
        const int i = std::clamp (p.delayDiv, 0, (int) beats.size() - 1);
        const double bpm = p.bpm > 1.0 ? p.bpm : 120.0;
        return (float) (beats[(size_t) i] * 60.0 / bpm);
    }

    // Renders `n` stereo samples into L and R (overwrites).
    void render (float* L, float* R, int n, const SoundParams& p)
    {
        setMode (p.poly, p.voices);
        std::fill (L, L + n, 0.0f);
        std::fill (R, R + n, 0.0f);
        if ((int) noiseBuf.size() < n) noiseBuf.resize ((size_t) n);
        float* noise = noiseBuf.data();
        std::fill (noise, noise + n, 0.0f);
        synth.renderStereo (L, R, n, p.voice, noise);   // both modes render so released notes can ring out
        poly.renderStereo  (L, R, n, p.voice, noise);

        // Keep the deep bass centred: unison width only above ~150 Hz.
        const float hpA = OnePole::coef (150.0f, sr);
        for (int i = 0; i < n; ++i)
        {
            const float mid = 0.5f * (L[i] + R[i]), side = 0.5f * (L[i] - R[i]);
            const float s2 = sideHp2.hp (sideHp1.hp (side, hpA), hpA);
            L[i] = mid + s2;
            R[i] = mid - s2;
        }

        neblina.process (L, R, n, p.voice.neblina);
        drive.process (L, R, n, p.drive, p.warmth);

        chorus.process  (L, R, n, p.chorusTone, p.chorusMix);
        delay.process   (L, R, n, delaySeconds (p), p.delayFb, p.delayMix);
        reverb.process  (L, R, n, p.reverbType, p.reverbSize, p.reverbMix);
        crusher.process (L, R, n, p.crush);
        comp.process    (L, R, n, p.comp);

        // Noise send: an airy hiss (high-passed) laid on top of the finished note,
        // after Drive and Comp, so nothing in the chain reacts to it or pushes the
        // note down when you turn it up.
        const float nhA = OnePole::coef (1500.0f, sr), nlA = OnePole::coef (12000.0f, sr);
        const float nsc = smoothCoef (20.0f, sr);
        for (int i = 0; i < n; ++i)
        {
            const float level = noiseLevel.next (0.35f * std::pow (p.voice.noise, 1.5f), nsc);
            const float s = noiseLp.lp (noiseHp2.hp (noiseHp1.hp (noise[i], nhA), nhA), nlA) * level;
            L[i] += s;
            R[i] += s;
        }


        // -7 dB of fixed headroom: Drive and Comp now add level instead of squashing
        // it, so this keeps peaks around -3 dBFS at the default Volume.
        const float target = dbToGain (p.volumeDb - 7.0f);
        const float smooth = smoothCoef (20.0f, sr);
        if (volume < 0.0f) volume = target;
        for (int i = 0; i < n; ++i)
        {
            volume += (target - volume) * smooth;
            L[i] = safetyClip (L[i] * volume);
            R[i] = safetyClip (R[i] * volume);
        }
    }

    // Output safety: untouched below about -2 dBFS, then rounds peaks off smoothly
    // so the loudest hits never go over 0 dBFS.
    static float safetyClip (float x)
    {
        const float a = std::fabs (x);
        if (a <= 0.8f) return x;
        return std::copysign (0.8f + 0.19f * std::tanh ((a - 0.8f) / 0.19f), x);
    }

    const MonoSynth& getSynth() const { return synth; }
    const PolySynth& getPoly() const  { return poly; }

private:
    double sr = 44100.0;
    MonoSynth synth;
    PolySynth poly;
    bool polyMode = false;
    NeblinaTexture neblina;
    OnePole sideHp1, sideHp2;
    Drive drive;
    std::vector<float> noiseBuf;
    OnePole noiseHp1, noiseHp2, noiseLp;
    Smoothed noiseLevel;
    Chorus chorus;
    PingPongDelay delay;
    Reverb reverb;
    Crusher crusher;
    Compressor comp;
    float volume = -1.0f;
};

} // namespace acido
