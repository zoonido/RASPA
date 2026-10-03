// voice_tests.cpp — automated checks for ÁCIDO's sound engine (phase 4b).
// GitHub runs this on every upload. It prints a pass/fail list and writes
// acido_preview.wav, an 8-bar acid line you can download and listen to.
#include "../source/dsp/acid_engine.h"
#include "../source/dsp/step_mod.h"
#include <cmath>
#include <chrono>
#include <complex>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

using namespace acido;

static int failures = 0;
static void check (bool ok, const std::string& name, const std::string& detail = "")
{
    std::printf ("%s  %s%s%s\n", ok ? "PASS" : "FAIL", name.c_str(), detail.empty() ? "" : "  — ", detail.c_str());
    if (! ok) ++failures;
}
static std::string db (float v) { char b[32]; std::snprintf (b, 32, "%.1f dB", v); return b; }

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static const double SR = 48000.0;

// Voice settings with every extra switched off, so each test changes one thing.
static VoiceParams clean()
{
    VoiceParams p;
    p.shape = 0; p.subLevel = 0; p.filterFm = 0; p.noise = 0; p.drift = 0; p.velo = 0;
    return p;
}
// Filter wide open, no envelope: hear the oscillator itself.
static VoiceParams open()
{
    VoiceParams p = clean();
    p.cutoffHz = 18000; p.reso = 0; p.envMod = 0; p.accent = 0;
    return p;
}

static std::vector<float> render (MonoSynth& s, int n, const VoiceParams& p)
{
    std::vector<float> b ((size_t) n, 0.0f);
    s.render (b.data(), n, p);
    return b;
}

// Renders the engine and keeps only the left channel (for level/spectrum checks).
static void renderL (AcidEngine& e, float* L, int n, const SoundParams& p)
{
    std::vector<float> R ((size_t) n);
    e.render (L, R.data(), n, p);
}

// Settings with every effect switched off.
static SoundParams dryParams()
{
    SoundParams p;
    p.chorusMix = 0; p.delayMix = 0; p.reverbMix = 0; p.crush = 0; p.comp = 0;
    return p;
}

// Plays one held note and returns a steady-state chunk.
static std::vector<float> steadyNote (const VoiceParams& p, int note = 57, int velocity = 80, int len = 8192)
{
    MonoSynth s; s.prepare (SR);
    s.noteOn (note, velocity);
    render (s, 9600, p);
    return render (s, len, p);
}

static float rms (const std::vector<float>& b, size_t from = 0, size_t to = 0)
{
    if (to == 0) to = b.size();
    double sum = 0.0;
    for (size_t i = from; i < to; ++i) sum += (double) b[i] * b[i];
    return (float) std::sqrt (sum / (double) (to - from));
}

// Level of one frequency, in dB (windowed single-bin DFT).
static float levelAt (const std::vector<float>& b, double f)
{
    const size_t n = b.size();
    std::complex<double> acc (0.0, 0.0);
    for (size_t i = 0; i < n; ++i)
    {
        const double w = 0.5 - 0.5 * std::cos (kTwoPi * (double) i / (double) (n - 1));
        acc += std::polar ((double) b[i] * w, -kTwoPi * f * (double) i / SR);
    }
    return (float) (20.0 * std::log10 (std::abs (acc) + 1e-12));
}

// Strongest level inside a band, in dB.
static float bandDb (const std::vector<float>& b, double lo, double hi, double step = 5.0)
{
    float best = -300.0f;
    for (double f = lo; f <= hi; f += step) best = std::max (best, levelAt (b, f));
    return best;
}

// Pitch from upward zero crossings (for clean sine tones).
static double measureHz (const std::vector<float>& b)
{
    double first = -1, last = -1; int count = 0;
    for (size_t i = 1; i < b.size(); ++i)
        if (b[i - 1] < 0.0f && b[i] >= 0.0f)
        {
            const double t = (double) (i - 1) + (double) (-b[i - 1] / (b[i] - b[i - 1]));
            if (first < 0) first = t; else ++count;
            last = t;
        }
    return count > 0 ? SR * count / (last - first) : 0.0;
}

// ---------------------------------------------------------------------------
// Core voice (phase 2)
// ---------------------------------------------------------------------------
static void testStability()
{
    int problems = 0, combos = 0;
    for (double sr : { 44100.0, 48000.0, 96000.0 })
        for (int wave = 0; wave < 4; ++wave)
            for (float amount : { 0.0f, 1.0f })
                for (float cut : { 20.0f, 18000.0f })
                {
                    ++combos;
                    AcidEngine e; e.prepare (sr);
                    SoundParams p;
                    auto& v = p.voice;
                    v.wave = wave; v.shape = amount; v.reso = amount; v.cutoffHz = cut; v.envMod = 1; v.accent = 1;
                    v.filterFm = amount; v.subLevel = amount; v.noise = amount; v.drift = amount; v.decayMs = 3000;
                    p.drive = amount; p.warmth = amount; p.volumeDb = 6;
                    p.chorusMix = amount; p.delayMix = amount; p.delayFb = 0.95f * amount; p.reverbMix = amount;
                    p.reverbSize = amount; p.reverbType = wave; p.crush = amount; p.comp = amount;
                    std::vector<float> b (30000);
                    e.noteOn (24, 127); renderL (e, b.data(), 30000, p);
                    bool bad = false;
                    for (float x : b) if (! std::isfinite (x) || std::fabs (x) > 6.0f) bad = true;
                    e.noteOn (96, 127); renderL (e, b.data(), 30000, p);
                    for (float x : b) if (! std::isfinite (x) || std::fabs (x) > 6.0f) bad = true;
                    if (bad) ++problems;
                }
    check (problems == 0, "Stable at extreme settings (" + std::to_string (combos) + " combinations, all 4 waves, 3 sample rates)",
           problems ? std::to_string (problems) + " blew up" : "");
}

static void testSilenceAfterRelease()
{
    AcidEngine e; e.prepare (SR);
    SoundParams p = dryParams(); p.voice.reso = 1.0f; p.voice.noise = 1.0f; p.drive = 1.0f;
    std::vector<float> b (12000);
    e.noteOn (36, 100); renderL (e, b.data(), 12000, p);
    e.noteOff (36);     renderL (e, b.data(), 4800, p);
    renderL (e, b.data(), 4800, p);
    float peak = 0.0f; for (int i = 0; i < 4800; ++i) peak = std::max (peak, std::fabs (b[(size_t) i]));
    check (peak < 1e-3f, "Silent after note-off (effects off; noise and drive on)", "peak " + std::to_string (peak));
}

static void testResonancePeak()
{
    auto measure = [] (float reso)
    {
        VoiceParams p = clean(); p.reso = reso; p.cutoffHz = 1000; p.envMod = 0; p.accent = 0;
        auto b = steadyNote (p, 33, 80, 4096);
        return bandDb (b, 850, 1150) - bandDb (b, 100, 300);
    };
    const float change = measure (0.85f) - measure (0.0f);
    check (change > 20.0f, "Resonance creates a peak at the cutoff", "peak vs bass rises " + db (change));
}

static void testResonanceRange()
{
    // The filter must not ring on its own until the very top of the knob.
    auto ringsOn = [] (float reso)
    {
        LadderFilter f; float tail = 0.0f;
        for (int i = 0; i < 48000; ++i)
        {
            const float y = f.process (i < 480 ? 0.5f : 0.0f, 420.0f, 4.2f * reso, SR);
            if (i > 38000) tail = std::max (tail, std::fabs (y));
        }
        return tail > 1e-3f;
    };
    check (! ringsOn (0.5f) && ! ringsOn (0.7f) && ! ringsOn (0.9f), "Filter never whistles on its own below 90% Resonance");
    check (ringsOn (1.0f), "At 100% Resonance the filter can whistle on its own (classic self-oscillation)");

    // The resonant peak builds gradually and stays under the note itself.
    auto peak = [] (float reso)
    {
        VoiceParams p = clean(); p.reso = reso; p.cutoffHz = 420; p.envMod = 0; p.accent = 0;
        auto b = steadyNote (p, 33, 80, 16384);
        return bandDb (b, 380, 470, 2) - levelAt (b, 55.0);
    };
    const float p50 = peak (0.5f), p90 = peak (0.9f);
    check (p50 < p90 - 8.0f && p90 < 0.0f, "Resonance builds evenly and never drowns out the note",
           "peak vs note: " + db (p50) + " at 50%, " + db (p90) + " at 90%");
}

static void testFilterSlope()
{
    VoiceParams p = clean(); p.reso = 0; p.cutoffHz = 500; p.envMod = 0; p.accent = 0;
    auto b = steadyNote (p, 45, 80, 4096);
    const float drop = bandDb (b, 400, 600) - bandDb (b, 3800, 4200);
    check (drop > 45.0f, "24 dB filter cuts steeply above the cutoff", db (drop) + " less 3 octaves up");
}

static void testSlide()
{
    MonoSynth s; s.prepare (SR);
    VoiceParams p = clean(); p.slideMs = 60.0f;
    s.noteOn (36, 80);  render (s, 4800, p);
    const float envBefore = s.getVoice().getFilterEnv();
    s.noteOn (48, 80);  render (s, 480, p);
    const float midNote = s.getVoice().getCurrentNote(), envAfter = s.getVoice().getFilterEnv();
    render (s, 14400, p);
    const float endNote = s.getVoice().getCurrentNote();
    check (midNote > 36.5f && midNote < 47.5f, "Overlapping note glides (not a jump)", "pitch after 10 ms " + std::to_string (midNote));
    check (std::fabs (endNote - 48.0f) < 0.05f, "Glide arrives at the new note");
    check (envAfter < envBefore, "Slide does not retrigger the filter envelope");
}

static void testNewNoteJumps()
{
    MonoSynth s; s.prepare (SR);
    VoiceParams p = clean(); p.slideMs = 500.0f;
    s.noteOn (36, 80); render (s, 4800, p); s.noteOff (36); render (s, 4800, p);
    s.noteOn (48, 80); render (s, 48, p);
    check (std::fabs (s.getVoice().getCurrentNote() - 48.0f) < 0.01f, "Separate notes start on pitch (no glide)");
    check (s.getVoice().getFilterEnv() > 0.9f, "Separate notes retrigger the filter envelope");
}

static void testReturnToHeld()
{
    MonoSynth s; s.prepare (SR);
    VoiceParams p = clean(); p.slideMs = 20.0f;
    s.noteOn (36, 80); render (s, 2400, p);
    s.noteOn (43, 80); render (s, 4800, p);
    s.noteOff (43);    render (s, 9600, p);
    check (std::fabs (s.getVoice().getCurrentNote() - 36.0f) < 0.05f, "Mono returns to a still-held key");
}

static void testAccent()
{
    auto level = [] (int velocity)
    {
        MonoSynth s; s.prepare (SR);
        VoiceParams p = clean(); p.accent = 0.7f;
        s.noteOn (36, velocity);
        return rms (render (s, 9600, p), 480, 9600);
    };
    const float gain = 20.0f * std::log10 (level (100) / level (99));
    check (gain > 2.0f, "Velocity 100+ is accented (louder)", "+" + db (gain));
}

static void testBlockSizeIndependence()
{
    SoundParams p; p.chorusMix = 0.4f; p.delayMix = 0.4f; p.reverbMix = 0.4f; p.crush = 0.3f;
    AcidEngine a; a.prepare (SR); a.noteOn (36, 110);
    AcidEngine b; b.prepare (SR); b.noteOn (36, 110);
    std::vector<float> bl (4096), br (4096), sl (4096), sr (4096);
    a.render (bl.data(), br.data(), 4096, p);
    for (int i = 0; i < 64; ++i) b.render (sl.data() + i * 64, sr.data() + i * 64, 64, p);
    float diff = 0.0f;
    for (size_t i = 0; i < bl.size(); ++i) diff = std::max ({ diff, std::fabs (bl[i] - sl[i]), std::fabs (br[i] - sr[i]) });
    check (diff < 1e-5f, "Same sound whatever Ableton's buffer size (whole engine and effects)");
}

// ---------------------------------------------------------------------------
// Oscillator (phase 3a)
// ---------------------------------------------------------------------------
static const double F = 220.0;   // MIDI note 57

static void testSine()
{
    VoiceParams p = open(); p.wave = 2; p.shape = 0;
    auto pure = steadyNote (p);
    const float h1 = levelAt (pure, F);
    const float worst = std::max (levelAt (pure, 2 * F), levelAt (pure, 3 * F)) - h1;
    check (worst < -25.0f, "Sine at Shape 0 is a clean tone (only the filter's light saturation)", "strongest overtone " + db (worst));

    p.shape = 0.6f;
    auto folded = steadyNote (p);
    const float h3 = levelAt (folded, 3 * F) - levelAt (folded, F);
    check (h3 > -25.0f, "Sine Fold adds harmonics", "3rd harmonic at " + db (h3));
}

static void testSawMorph()
{
    VoiceParams p = open(); p.wave = 0; p.shape = 0;
    auto saw = steadyNote (p);
    const float sawH2 = levelAt (saw, 2 * F) - levelAt (saw, F);
    p.shape = 1;
    auto tri = steadyNote (p);
    const float triH2 = levelAt (tri, 2 * F) - levelAt (tri, F);
    const float triH3 = levelAt (tri, 3 * F) - levelAt (tri, F);
    check (sawH2 > -9.0f, "Saw has every harmonic", "2nd harmonic at " + db (sawH2));
    check (triH2 < -30.0f && triH3 < -15.0f, "Saw Shape 100% becomes a soft triangle", "2nd " + db (triH2) + ", 3rd " + db (triH3));
}

static void testPulseWidth()
{
    VoiceParams p = open(); p.wave = 1; p.shape = 0.5f;
    auto square = steadyNote (p);
    const float sqH2 = levelAt (square, 2 * F) - levelAt (square, F);
    p.shape = 0.1f;
    auto narrow = steadyNote (p);
    const float nH2 = levelAt (narrow, 2 * F) - levelAt (narrow, F);
    check (sqH2 < -30.0f, "Square PW 50% has only odd harmonics", "2nd harmonic at " + db (sqH2));
    check (nH2 > -15.0f, "Narrow pulse width changes the tone", "2nd harmonic at " + db (nH2));
}

static void testFm()
{
    VoiceParams p = open(); p.wave = 3; p.shape = 0;
    auto plain = steadyNote (p);
    const float worst = std::max (levelAt (plain, 2 * F), levelAt (plain, 3 * F)) - levelAt (plain, F);
    p.shape = 0.6f;
    auto rich = steadyNote (p);
    const float side = levelAt (rich, 3 * F) - levelAt (rich, F);
    check (worst < -25.0f, "FM at Index 0 is a clean tone (only the filter's light saturation)", "strongest overtone " + db (worst));
    check (side > -20.0f, "FM Index adds sidebands", "3rd harmonic at " + db (side));
}

static void testSub()
{
    VoiceParams p = open(); p.wave = 0; p.subLevel = 0;
    auto none = steadyNote (p);
    p.subLevel = 1; p.subOct = 1;
    auto one = steadyNote (p);
    p.subOct = 2;
    auto two = steadyNote (p);
    const float off = levelAt (none, F / 2) - levelAt (none, F);
    const float oct1 = levelAt (one, F / 2) - levelAt (one, F);
    const float oct2 = levelAt (two, F / 4) - levelAt (two, F);
    check (off < -40.0f && oct1 > -10.0f, "Sub adds a tone one octave down", db (oct1) + " vs the main pitch");
    check (oct2 > -10.0f, "Sub -2 oct plays two octaves down", db (oct2) + " vs the main pitch");
}

// Brightness: level of the sample-to-sample change (weights the highs).
static float brightness (const std::vector<float>& b)
{
    double sum = 0.0;
    for (size_t i = 1; i < b.size(); ++i) { const double d = b[i] - b[i - 1]; sum += d * d; }
    return (float) std::sqrt (sum / (double) (b.size() - 1));
}

static void testFilterFm()
{
    VoiceParams p = clean(); p.cutoffHz = 800; p.reso = 0.3f; p.envMod = 0; p.accent = 0;
    auto dry = steadyNote (p, 45, 80, 24000);
    p.filterFm = 1.0f;
    auto fm = steadyNote (p, 45, 80, 24000);
    const float gain = 20.0f * std::log10 (brightness (fm) / brightness (dry));
    check (gain > 6.0f, "Filter FM adds brightness and grit", "+" + db (gain) + " in the highs");
}

static void testDrift()
{
    auto pitches = [] (float drift)
    {
        MonoSynth s; s.prepare (SR);
        VoiceParams p = open(); p.wave = 2; p.drift = drift;
        std::vector<double> hz;
        for (int i = 0; i < 4; ++i)
        {
            s.noteOn (57, 80); render (s, 2400, p);
            hz.push_back (measureHz (render (s, 9600, p)));
            s.noteOff (57); render (s, 2400, p);
        }
        return hz;
    };
    double still = 0, moving = 0;
    for (double h : pitches (0.0f)) still  = std::max (still,  std::fabs (h - F));
    for (double h : pitches (1.0f)) moving = std::max (moving, std::fabs (h - F));
    check (still < 0.05, "Drift 0% keeps every note exactly in tune");
    char d[64]; std::snprintf (d, 64, "up to %.2f Hz off 220 Hz", moving);
    check (moving > 0.3 && moving < 4.0, "Drift 100% makes notes land slightly differently", d);
}

static void testVelo()
{
    auto level = [] (float velo, int vel)
    {
        VoiceParams p = clean(); p.velo = velo;
        return rms (steadyNote (p, 45, vel, 4096));
    };
    const float full = 20.0f * std::log10 (level (1.0f, 50) / level (1.0f, 99));
    const float none = 20.0f * std::log10 (level (0.0f, 50) / level (0.0f, 99));
    check (full < -4.0f, "Velo 100%: softer notes play quieter", db (full) + " at velocity 50");
    check (std::fabs (none) < 0.1f, "Velo 0%: every note plays the same, like a 303");
}

static void testNoise()
{
    VoiceParams p = clean(); p.cutoffHz = 400; p.envMod = 0; p.accent = 0;
    auto quiet = steadyNote (p, 45, 80, 4096);
    p.noise = 1.0f;
    auto hiss = steadyNote (p, 45, 80, 4096);
    const float gain = 20.0f * std::log10 (rms (hiss) / rms (quiet));
    check (gain > 1.0f, "Noise 100% adds clear hiss to the note", "+" + db (gain) + " overall");

    // The default sound must have no hiss: level above 8 kHz compared with the note.
    AcidEngine e; e.prepare (SR);
    SoundParams d;                         // Squelch Init defaults (effects on)
    std::vector<float> b (9600), tail (4096);
    e.noteOn (45, 80); renderL (e, b.data(), 9600, d); renderL (e, tail.data(), 4096, d);
    const float hissDb = bandDb (tail, 10000, 14000, 100) - bandDb (tail, 100, 1000, 5);
    check (hissDb < -60.0f, "Default sound is free of background hiss", "10–14 kHz sits " + db (-hissDb) + " below the note");
}

// ---------------------------------------------------------------------------
// Drive + Warmth (phase 3a)
// ---------------------------------------------------------------------------
static std::vector<float> sine (float amp, int n = 8192)
{
    std::vector<float> b ((size_t) n);
    for (int i = 0; i < n; ++i) b[(size_t) i] = amp * (float) std::sin (kTwoPi * F * i / SR);
    return b;
}

static std::vector<float> driven (float drive, float warmth, float amp = 0.5f)
{
    Drive d; d.prepare (SR);
    auto warm = sine (amp, 9600), warmR = warm;
    d.process (warm.data(), warmR.data(), 9600, drive, warmth);
    auto b = sine (amp), r = b;
    d.process (b.data(), r.data(), (int) b.size(), drive, warmth);
    return b;
}

static void testDriveBypass()
{
    auto in = sine (0.5f);
    auto out = driven (0.0f, 1.0f);
    float diff = 0.0f; for (size_t i = 0; i < in.size(); ++i) diff = std::max (diff, std::fabs (in[i] - out[i]));
    check (diff < 1e-6f, "Drive 0% leaves the sound untouched");
}

static void testDriveHarmonics()
{
    auto clean0 = driven (0.0f, 0.0f);
    auto hard = driven (1.0f, 0.0f);
    const float h3 = levelAt (hard, 3 * F) - levelAt (hard, F);
    const float h3clean = levelAt (clean0, 3 * F) - levelAt (clean0, F);
    check (h3 > h3clean + 40.0f && h3 > -25.0f, "Drive adds distortion harmonics", "3rd harmonic at " + db (h3));

    auto warm = driven (1.0f, 1.0f);
    const float brightH2 = levelAt (hard, 2 * F) - levelAt (hard, F);
    const float warmH2   = levelAt (warm, 2 * F) - levelAt (warm, F);
    check (warmH2 > brightH2 + 10.0f, "Warmth adds even (tape-like) harmonics", "2nd harmonic " + db (brightH2) + " -> " + db (warmH2));

    const float darker = bandDb (hard, 5000, 8000, 50) - bandDb (warm, 5000, 8000, 50);
    check (darker > 6.0f, "Warmth rounds off the highs", db (darker) + " less at 5–8 kHz");

    double mean = 0; for (float x : warm) mean += x; mean /= (double) warm.size();
    check (std::fabs (mean) < 0.01, "No DC offset from the warm curve");
}

// Drive works as a send: the dry note stays, grit is added on top.
static void testDriveIsASend()
{
    auto measure = [] (float drive)
    {
        AcidEngine e; e.prepare (SR);
        SoundParams p = dryParams(); p.drive = drive; p.voice.noise = 0;
        std::vector<float> l (24000), r (24000), bl (16384), br (16384);
        e.noteOn (33, 80); e.render (l.data(), r.data(), 24000, p);
        e.render (bl.data(), br.data(), 16384, p);
        return bl;
    };
    auto dry = measure (0.0f), full = measure (1.0f);
    const float fund = levelAt (full, 55.0) - levelAt (dry, 55.0);
    const float level = 20.0f * std::log10 (rms (full) / rms (dry));
    const float grit = 20.0f * std::log10 (brightness (full) / brightness (dry));
    check (std::fabs (fund) < 0.5f, "Drive keeps the note's body: low end untouched", "fundamental " + db (fund));
    check (level >= 0.0f && level < 6.0f, "Drive never makes the sound quieter", db (level) + " at 100%");
    check (grit > 5.0f, "Drive clearly adds grit on top", "+" + db (grit) + " brightness at 100%");
}

// Noise is a send too: hiss on top, the note stays where it is.
static void testNoiseSend()
{
    auto measure = [] (float noise)
    {
        AcidEngine e; e.prepare (SR);
        SoundParams p; p.delayMix = 0; p.reverbMix = 0; p.voice.noise = noise;   // Drive and Comp at their defaults
        std::vector<float> l (24000), r (24000), bl (16384), br (16384);
        e.noteOn (33, 80); e.render (l.data(), r.data(), 24000, p);
        e.render (bl.data(), br.data(), 16384, p);
        return bl;
    };
    auto quiet = measure (0.0f), hiss = measure (1.0f), half = measure (0.5f);
    const float fund = levelAt (hiss, 55.0) - levelAt (quiet, 55.0);
    check (std::fabs (fund) < 0.5f, "Noise doesn't push the note down", "fundamental " + db (fund) + " at 100%");
    const float air = bandDb (hiss, 6000, 9000, 50) - bandDb (quiet, 6000, 9000, 50);
    const float airHalf = bandDb (half, 6000, 9000, 50) - bandDb (quiet, 6000, 9000, 50);
    check (airHalf > 30.0f && air > airHalf + 6.0f, "Noise adds hiss that grows steadily with the knob",
           "+" + db (airHalf) + " at 50%, +" + db (air) + " at 100% (6–9 kHz)");
}

// ---------------------------------------------------------------------------
// Effects (phase 3b)
// ---------------------------------------------------------------------------
static std::vector<float> impulse (int n) { std::vector<float> b ((size_t) n, 0.0f); b[0] = 1.0f; return b; }

static float maxDiff (const std::vector<float>& a, const std::vector<float>& b)
{
    float d = 0.0f; for (size_t i = 0; i < a.size(); ++i) d = std::max (d, std::fabs (a[i] - b[i])); return d;
}

// Index of the loudest sample inside [from, to).
static size_t peakIndex (const std::vector<float>& b, size_t from, size_t to)
{
    size_t best = from; for (size_t i = from; i < to && i < b.size(); ++i) if (std::fabs (b[i]) > std::fabs (b[best])) best = i; return best;
}

static float windowRmsDb (const std::vector<float>& b, size_t from, size_t len)
{
    return 20.0f * std::log10 (rms (b, from, std::min (b.size(), from + len)) + 1e-12f);
}

static void testChorus()
{
    auto l = sine (0.5f, 48000), r = l, inL = l;
    Chorus c; c.prepare (SR);
    c.process (l.data(), r.data(), 48000, 0.6f, 0.0f);
    check (maxDiff (l, inL) < 1e-7f && maxDiff (r, inL) < 1e-7f, "Chorus Dry/Wet 0% leaves the sound untouched");

    auto wl = sine (0.5f, 48000), wr = wl;
    Chorus c2; c2.prepare (SR);
    c2.process (wl.data(), wr.data(), 48000, 0.6f, 0.5f);
    check (maxDiff (wl, wr) > 0.05f, "Chorus spreads the sound left/right");

    std::vector<float> bl (48000), br;
    for (int i = 0; i < 48000; ++i) bl[(size_t) i] = 0.5f * (float) std::sin (kTwoPi * 50.0 * i / SR);
    br = bl;
    Chorus c3; c3.prepare (SR);
    c3.process (bl.data(), br.data(), 48000, 0.6f, 0.5f);
    std::vector<float> side (48000);
    for (size_t i = 0; i < side.size(); ++i) side[i] = bl[i] - br[i];
    const float sideDb = 20.0f * std::log10 (rms (side, 9600) / rms (bl, 9600));
    check (sideDb < -30.0f, "Chorus keeps the bass centred (mono)", "left/right difference at 50 Hz: " + db (sideDb));
}

static void testDelay()
{
    const int n = (int) (3.0 * SR);
    auto l = impulse (n), r = std::vector<float> ((size_t) n, 0.0f);
    PingPongDelay d; d.prepare (SR);
    SoundParams sp; sp.delaySync = true; sp.delayDiv = 6; sp.bpm = 120.0;    // 1/8 dotted at 120 BPM = 375 ms
    const float secs = AcidEngine::delaySeconds (sp);
    d.process (l.data(), r.data(), n, secs, 0.5f, 1.0f);

    const size_t expect = (size_t) std::lround (0.375 * SR);
    const size_t first = peakIndex (l, 100, (size_t) (0.6 * SR));
    check (std::labs ((long) first - (long) expect) < 5, "Delay 1/8 dotted lands at 375 ms at 120 BPM",
           std::to_string ((int) std::lround (first * 1000.0 / SR)) + " ms");
    const size_t second = peakIndex (r, (size_t) (0.6 * SR), (size_t) (0.9 * SR));
    check (std::labs ((long) second - (long) (2 * expect)) < 5, "Ping-pong: the next echo comes from the right");
    const size_t third = peakIndex (l, (size_t) (1.0 * SR), (size_t) (1.3 * SR));
    check (std::fabs (l[third]) < std::fabs (l[first]), "Echoes fade with each repeat");

    sp.bpm = 90.0; sp.delayDiv = 8;   // 1/4 at 90 BPM
    check (std::fabs (AcidEngine::delaySeconds (sp) - 0.6667f) < 0.001f, "Sync follows Ableton's tempo (1/4 at 90 BPM = 667 ms)");
    sp.delaySync = false; sp.delayMs = 250.0f;
    check (std::fabs (AcidEngine::delaySeconds (sp) - 0.25f) < 1e-6f, "MS mode uses milliseconds directly");

    // Feedback at maximum still dies away.
    const int longN = (int) (40.0 * SR);
    auto ll = impulse (longN), rr = std::vector<float> ((size_t) longN, 0.0f);
    PingPongDelay d2; d2.prepare (SR);
    d2.process (ll.data(), rr.data(), longN, 0.3f, 0.95f, 1.0f);
    const float tail = windowRmsDb (ll, (size_t) (38.0 * SR), (size_t) SR) - windowRmsDb (ll, (size_t) (0.25 * SR), (size_t) (0.1 * SR));
    check (tail < -60.0f, "Feedback 95% still fades out (no runaway)", "after 38 s: " + db (tail));

    auto bl = sine (0.5f), br = bl, in = bl;
    PingPongDelay d3; d3.prepare (SR);
    d3.process (bl.data(), br.data(), (int) bl.size(), 0.3f, 0.5f, 0.0f);
    check (maxDiff (bl, in) < 1e-7f, "Delay Dry/Wet 0% leaves the sound untouched");
}

static void testReverb()
{
    const char* names[4] = { "Room", "Hall", "Plate", "Spring" };
    float decay[4] {};
    for (int t = 0; t < 4; ++t)
    {
        const int n = (int) (4.0 * SR);
        auto l = impulse (n), r = std::vector<float> ((size_t) n, 0.0f);
        Reverb rv; rv.prepare (SR);
        rv.process (l.data(), r.data(), n, t, 0.6f, 1.0f);
        const float early = windowRmsDb (l, (size_t) (0.05 * SR), (size_t) (0.1 * SR));
        const float late  = windowRmsDb (l, (size_t) (0.8 * SR),  (size_t) (0.1 * SR));
        decay[t] = early - late;
        bool finite = true; for (float x : l) if (! std::isfinite (x)) finite = false;
        check (finite && early > -60.0f, std::string ("Reverb ") + names[t] + " produces a tail", "drops " + db (decay[t]) + " by 0.8 s");
    }
    check (decay[1] < decay[0], "Hall rings longer than Room");

    auto sizeDrop = [] (float size)
    {
        const int n = (int) (3.0 * SR);
        auto l = impulse (n), r = std::vector<float> ((size_t) n, 0.0f);
        Reverb rv; rv.prepare (SR);
        rv.process (l.data(), r.data(), n, Reverb::Plate, size, 1.0f);
        return windowRmsDb (l, (size_t) (0.05 * SR), (size_t) (0.1 * SR)) - windowRmsDb (l, (size_t) (1.0 * SR), (size_t) (0.1 * SR));
    };
    check (sizeDrop (1.0f) < sizeDrop (0.0f), "Bigger Size gives a longer tail");

    const int longN = (int) (30.0 * SR);
    auto ll = impulse (longN), rr = std::vector<float> ((size_t) longN, 0.0f);
    Reverb big; big.prepare (SR);
    big.process (ll.data(), rr.data(), longN, Reverb::Hall, 1.0f, 1.0f);
    float peak = 0.0f; for (size_t i = (size_t) (29.0 * SR); i < ll.size(); ++i) peak = std::max (peak, std::fabs (ll[i]));
    check (peak < 1e-4f, "Biggest Hall still fades to silence");

    auto bl = sine (0.5f), br = bl, in = bl;
    Reverb rv0; rv0.prepare (SR);
    rv0.process (bl.data(), br.data(), (int) bl.size(), Reverb::Hall, 0.6f, 0.0f);
    check (maxDiff (bl, in) < 1e-7f, "Reverb Send 0% leaves the sound untouched");

    // Send: the original sound stays at full level however much reverb is added.
    auto keep = [] (float send)
    {
        auto l = sine (0.5f, (int) SR), r = l, in = l;
        Reverb rv; rv.prepare (SR);
        rv.process (l.data(), r.data(), (int) l.size(), Reverb::Hall, 0.6f, send);
        return levelAt (std::vector<float> (l.begin(), l.begin() + 2048), F) - levelAt (std::vector<float> (in.begin(), in.begin() + 2048), F);
    };
    check (std::fabs (keep (1.0f)) < 0.5f, "Reverb Send 100% keeps the original sound at full level (first 40 ms, before the tail)",
           db (keep (1.0f)));

    auto wetLevel = [] (double hz)
    {
        std::vector<float> l ((size_t) (2 * SR)), r, in;
        for (size_t i = 0; i < l.size(); ++i) l[i] = 0.5f * (float) std::sin (kTwoPi * hz * (double) i / SR);
        r = l; in = l;
        Reverb rv; rv.prepare (SR);
        rv.process (l.data(), r.data(), (int) l.size(), Reverb::Hall, 0.6f, 1.0f);
        for (size_t i = 0; i < l.size(); ++i) l[i] -= in[i];          // the reverb alone
        return windowRmsDb (l, (size_t) SR, (size_t) SR);
    };
    const float bassGap = wetLevel (1000.0) - wetLevel (40.0);
    check (bassGap > 10.0f, "Reverb keeps deep bass out of the tail", db (bassGap) + " less at 40 Hz");
}

static void testCrush()
{
    auto l = sine (0.5f), r = l, in = l;
    Crusher c; c.prepare (SR);
    c.process (l.data(), r.data(), (int) l.size(), 0.0f);
    check (maxDiff (l, in) < 1e-7f, "Crush 0% leaves the sound untouched");

    auto hl = sine (0.5f), hr = hl;
    Crusher c2; c2.prepare (SR);
    c2.process (hl.data(), hr.data(), (int) hl.size(), 1.0f);
    const float grit = bandDb (hl, 4000, 9000, 5) - levelAt (hl, F);
    check (grit > -40.0f, "Crush 100% adds lo-fi grit", "5–9 kHz content at " + db (grit));
    const float lvl = 20.0f * std::log10 (rms (hl) / rms (in));
    check (std::fabs (lvl) < 3.0f, "Crush keeps the level steady", db (lvl));
}

static void testComp()
{
    // Alternating loud and quiet bursts.
    const int n = (int) (2.0 * SR), burst = (int) (0.25 * SR);
    std::vector<float> src ((size_t) n);
    for (int i = 0; i < n; ++i) src[(size_t) i] = ((i / burst) % 2 ? 0.1f : 0.9f) * (float) std::sin (kTwoPi * F * i / SR);

    auto l = src, r = src;
    Compressor c; c.prepare (SR);
    c.process (l.data(), r.data(), n, 0.0f);
    check (maxDiff (l, src) < 1e-7f, "Comp 0% leaves the sound untouched");

    auto cl = src, cr = src;
    Compressor c2; c2.prepare (SR);
    c2.process (cl.data(), cr.data(), n, 1.0f);
    auto gap = [&] (const std::vector<float>& b)
    {
        return windowRmsDb (b, (size_t) (burst * 4 + burst / 2), (size_t) (burst / 2))
             - windowRmsDb (b, (size_t) (burst * 5 + burst / 2), (size_t) (burst / 2));
    };
    const float before = gap (src), after = gap (cl);
    check (before - after > 6.0f, "Comp evens out loud and quiet notes", "gap " + db (before) + " -> " + db (after));
}

// Comp should add punch: the body of each note comes up, the hit still leads.
static void testCompPunch()
{
    auto note = [] (float comp)
    {
        AcidEngine e; e.prepare (SR);
        SoundParams p = dryParams(); p.comp = comp;
        std::vector<float> l (24000), r (24000);
        e.noteOn (33, 110); e.render (l.data(), r.data(), 24000, p);
        return l;
    };
    auto body = [] (const std::vector<float>& b) { return windowRmsDb (b, 2880, 6720); };
    auto hit  = [] (const std::vector<float>& b) { float pk = 0; for (int i = 0; i < 720; ++i) pk = std::max (pk, std::fabs (b[(size_t) i])); return 20.0f * std::log10 (pk); };
    auto off = note (0.0f), some = note (0.35f), full = note (1.0f);
    const float lift35 = body (some) - body (off), lift100 = body (full) - body (off);
    check (lift35 > 1.5f && lift100 > 3.0f, "Comp lifts the body of each note (punch, not dampening)",
           "+" + db (lift35) + " at 35%, +" + db (lift100) + " at 100%");
    check (hit (full) - body (full) > hit (off) - body (off), "Comp lets each note's hit through first");
}

// The sub skips the filter, so Resonance and Cutoff never thin it out.
static void testSubPower()
{
    auto subLevel = [] (float reso, float cutoff)
    {
        VoiceParams p = clean(); p.subLevel = 1.0f; p.reso = reso; p.cutoffHz = cutoff; p.envMod = 0; p.accent = 0;
        auto b = steadyNote (p, 33, 80, 32768);
        return levelAt (b, 27.5);
    };
    const float base = subLevel (0.0f, 420.0f);
    const float squelch = subLevel (0.9f, 420.0f), dark = subLevel (0.0f, 60.0f);
    check (std::fabs (squelch - base) < 0.5f && std::fabs (dark - base) < 0.5f,
           "Sub keeps its full weight at any Resonance or Cutoff",
           "change: " + db (squelch - base) + " at 90% Reso, " + db (dark - base) + " at 60 Hz Cutoff");

    VoiceParams p = clean(); p.subLevel = 1.0f; p.cutoffHz = 420; p.envMod = 0; p.accent = 0;
    auto b = steadyNote (p, 33, 80, 32768);
    const float over = levelAt (b, 27.5 * 3) - levelAt (b, 27.5);
    check (over > -40.0f && over < -15.0f, "Sub has a touch of overtone so it's heard on small speakers", "3rd harmonic " + db (over));
}

// Loudest hits never go over 0 dBFS, whatever the settings.
static void testSafety()
{
    AcidEngine e; e.prepare (SR);
    SoundParams p; p.comp = 1.0f; p.drive = 1.0f; p.volumeDb = 6.0f; p.voice.subLevel = 1.0f; p.voice.accent = 1.0f;
    std::vector<float> l (48000), r (48000);
    e.noteOn (33, 127); e.render (l.data(), r.data(), 48000, p);
    float pk = 0; for (size_t i = 0; i < l.size(); ++i) pk = std::max ({ pk, std::fabs (l[i]), std::fabs (r[i]) });
    check (pk < 1.0f, "Output never goes over 0 dBFS (everything at maximum)", "peak " + db (20.0f * std::log10 (pk)));
}

static void testDefaultsStereo()
{
    AcidEngine e; e.prepare (SR);
    SoundParams p;
    std::vector<float> l (48000), r (48000);
    e.noteOn (45, 100); e.render (l.data(), r.data(), 12000, p);
    e.noteOff (45);     e.render (l.data(), r.data(), 48000, p);
    check (maxDiff (l, r) > 1e-3f, "Default sound is stereo (delay and reverb tails)");
}

// ---------------------------------------------------------------------------
// Step modulator (phase 4a)
// ---------------------------------------------------------------------------
static void testLaneTiming()
{
    acido::Lane lane;
    for (int i = 0; i < kSteps; ++i) lane.steps[(size_t) i] = (float) i / 16.0f;
    auto stepAt = [&lane] (int rate, double ppq) { lane.rate = rate; int s = -1; laneValueAt (lane, ppq, &s); return s; };

    check (stepAt (1, 1.0) == 4 && stepAt (0, 1.0) == 8 && stepAt (2, 1.0) == 2 && stepAt (3, 1.0) == 1,
           "Lane rates: beat 2 is step 5 at 1/16, 9 at 1/32, 3 at 1/8, 2 at 1/4");
    check (stepAt (1, 4.0) == 0 && stepAt (1, 7.75) == 15, "Lanes lock to the bar (bar 2 starts on step 1)");

    lane.length = 7;
    check (stepAt (1, 2.25) == 2, "Shorter lanes wrap around (step 10 of a 7-step lane is step 3)");
    lane.length = 16;

    acido::Lane glide;
    glide.steps[1] = 1.0f;
    glide.smooth = true;
    const float half = laneValueAt (glide, 0.125);   // halfway through step 1 at 1/16
    check (std::fabs (half - 0.5f) < 1e-4f, "Smooth mode glides between steps");
    glide.smooth = false;
    check (std::fabs (laneValueAt (glide, 0.125)) < 1e-6f, "Step mode holds each value for the whole step");
}

static void testModulator()
{
    std::vector<std::unique_ptr<acido::Lane>> lanes;
    lanes.push_back (std::make_unique<acido::Lane>());
    auto& lane = *lanes[0];
    for (auto& s : lane.steps) s = 1.0f;
    lane.depth = 0.5f;

    acido::StepModulator m; m.prepare (SR, 1);
    double ppq = 0.0;
    auto run = [&] (bool playing, double seconds)
    {
        const int chunks = (int) (seconds * SR / 32.0);
        for (int i = 0; i < chunks; ++i) { m.process (lanes, playing, ppq, 32); if (playing) ppq += 32.0 * 2.0 / SR; }
    };

    run (true, 0.005);
    const float after5ms = m.offset (0);
    run (true, 0.05);
    check (after5ms > 0.2f && after5ms < 0.45f, "Step changes glide over ~5 ms (no clicks)", "63% point: " + std::to_string (after5ms / 0.5f));
    check (std::fabs (m.offset (0) - 0.5f) < 1e-3f, "Depth 50% moves the knob by half its range");

    for (auto& s : lane.steps) s = -1.0f;
    run (false, 0.2);
    check (std::fabs (m.offset (0) - 0.5f) < 1e-3f, "Transport stopped: lanes freeze where they are");
    run (true, 0.2);
    check (std::fabs (m.offset (0) + 0.5f) < 1e-3f, "Transport playing: lanes follow the new steps");

    lane.depth = 0.0f;
    run (true, 0.2);
    check (std::fabs (m.offset (0)) < 1e-3f && ! lane.isActive(), "Depth 0% switches a lane off");

    check (applyOffset (0.9f, 0.5f) == 1.0f && applyOffset (0.1f, -0.5f) == 0.0f, "Knob plus lane stays inside the knob's range");
}

static void testFills()
{
    acido::Lane lane;
    lane.fill (acido::Lane::Sine);
    check (std::fabs (lane.steps[4].load() - 1.0f) < 1e-4f && std::fabs (lane.steps[12].load() + 1.0f) < 1e-4f, "Sine fill peaks on steps 5 and 13");
    lane.fill (acido::Lane::Saw);
    check (std::fabs (lane.steps[0].load() - 1.0f) < 1e-4f && std::fabs (lane.steps[15].load() + 1.0f) < 1e-4f, "Saw fill ramps from top to bottom");

    lane.length = 7;
    lane.fill (acido::Lane::Random, 42);
    bool inRange = true, beyondClear = true, varied = false;
    for (int i = 0; i < kSteps; ++i)
    {
        const float v = lane.steps[(size_t) i].load();
        if (v < -1.0f || v > 1.0f) inRange = false;
        if (i >= 7 && std::fabs (v) > 0.0f) beyondClear = false;
        if (i > 0 && i < 7 && std::fabs (v - lane.steps[0].load()) > 0.05f) varied = true;
    }
    check (inRange && beyondClear && varied, "Random fill stays in range and only fills the lane's length");

    lane.fill (acido::Lane::Clear);
    check (! lane.isActive(), "Clear empties the lane");

    acido::Lane hidden; hidden.length = 4; hidden.steps[10] = 1.0f;
    check (! hidden.isActive(), "Steps past the lane's length don't count");
}

// ---------------------------------------------------------------------------
// Poly, Unison and Neblina (phase 4b)
// ---------------------------------------------------------------------------
static SoundParams polyParams (int voices)
{
    SoundParams p = dryParams();
    p.poly = true; p.voices = voices; p.drive = 0.0f;
    p.voice = open(); p.voice.wave = 2;   // clean sines: easy to see which notes sound
    return p;
}

// Plays notes into an engine and returns the left channel of a steady chunk.
static std::vector<float> playNotes (AcidEngine& e, const SoundParams& p, std::initializer_list<int> notes)
{
    std::vector<float> l (9600), r (9600);
    e.render (l.data(), r.data(), 64, p);          // lets the engine switch mode first
    for (int n : notes) { e.noteOn (n, 80); e.render (l.data(), r.data(), 480, p); }
    e.render (l.data(), r.data(), 9600, p);
    std::vector<float> ol (8192), orr (8192);
    e.render (ol.data(), orr.data(), 8192, p);
    return ol;
}

static float noteLevel (const std::vector<float>& b, int note) { return levelAt (b, noteToHz ((float) note)); }

static void testPoly()
{
    AcidEngine mono; mono.prepare (SR);
    SoundParams mp = polyParams (6); mp.poly = false;
    auto m = playNotes (mono, mp, { 57, 61, 64 });
    check (noteLevel (m, 64) - noteLevel (m, 57) > 30.0f, "Mono plays one note at a time");

    AcidEngine poly; poly.prepare (SR);
    auto c = playNotes (poly, polyParams (6), { 57, 61, 64 });
    const float spread = std::max ({ noteLevel (c, 57), noteLevel (c, 61), noteLevel (c, 64) })
                       - std::min ({ noteLevel (c, 57), noteLevel (c, 61), noteLevel (c, 64) });
    check (spread < 3.0f && poly.getPoly().activeVoices() == 3, "Poly plays a 3-note chord", "3 voices, levels within " + db (spread));

    AcidEngine two; two.prepare (SR);
    auto s = playNotes (two, polyParams (2), { 57, 61, 64 });
    check (noteLevel (s, 57) < noteLevel (s, 64) - 30.0f && noteLevel (s, 61) > noteLevel (s, 64) - 3.0f,
           "With 2 voices, a 3rd note takes over the oldest");

    // Glide from the last note played.
    AcidEngine g; g.prepare (SR);
    SoundParams gp = polyParams (6); gp.voice.slideMs = 200.0f;
    std::vector<float> l (4800), r (4800);
    g.render (l.data(), r.data(), 64, gp);
    g.noteOn (48, 80); g.render (l.data(), r.data(), 4800, gp);
    g.noteOn (60, 80); g.render (l.data(), r.data(), 480, gp);
    float newest = 0.0f;
    for (int v = 0; v < kMaxVoices; ++v)
        if (g.getPoly().getVoice (v).isGateOn() && g.getPoly().getVoice (v).getFilterEnv() > 0.5f) newest = g.getPoly().getVoice (v).getCurrentNote();
    check (newest > 48.5f && newest < 59.5f, "Poly: each new note glides from the last note played", "pitch after 10 ms " + std::to_string (newest));

    // Release everything -> silence.
    AcidEngine q; q.prepare (SR);
    SoundParams qp = polyParams (8); qp.voice.noise = 0.5f;
    playNotes (q, qp, { 48, 52, 55, 60 });
    for (int n : { 48, 52, 55, 60 }) q.noteOff (n);
    std::vector<float> ql (9600), qr (9600);
    q.render (ql.data(), qr.data(), 9600, qp);
    q.render (ql.data(), qr.data(), 4800, qp);
    float peak = 0.0f; for (int i = 0; i < 4800; ++i) peak = std::max (peak, std::fabs (ql[(size_t) i]));
    check (peak < 1e-3f && q.getPoly().activeVoices() == 0, "Poly: releasing every key goes silent");

    // The plugin sets the mode before each block's notes: a note right after switching still sounds.
    AcidEngine first; first.prepare (SR);
    SoundParams fp = polyParams (6);
    first.setMode (true, 6);
    first.noteOn (57, 90);
    std::vector<float> fl (4800), fr (4800);
    first.render (fl.data(), fr.data(), 4800, fp);
    check (rms (fl) > 0.01f, "A note played right after switching to Poly isn't lost");

    // Switching modes mid-note doesn't leave notes stuck.
    AcidEngine sw; sw.prepare (SR);
    SoundParams sp = polyParams (6); sp.poly = false;
    playNotes (sw, sp, { 45 });
    sp.poly = true;
    std::vector<float> a (4800), b (4800);
    sw.render (a.data(), b.data(), 4800, sp);
    sw.noteOff (45);
    sw.render (a.data(), b.data(), 4800, sp); sw.render (a.data(), b.data(), 4800, sp);
    float pk = 0.0f; for (float x : a) pk = std::max (pk, std::fabs (x));
    check (pk < 1e-3f, "Switching Mono/Poly never leaves a note stuck");
}

static void testUnison()
{
    auto render = [] (int unison, float neblina, int voices, bool isPoly)
    {
        AcidEngine e; e.prepare (SR);
        SoundParams p = dryParams(); p.voice = clean(); p.voice.unison = unison; p.voice.neblina = 0.0f;
        p.poly = isPoly; p.voices = voices;
        (void) neblina;
        std::vector<float> l (24000), r (24000);
        e.render (l.data(), r.data(), 64, p);
        e.noteOn (45, 80);
        e.render (l.data(), r.data(), 24000, p);
        return std::make_pair (l, r);
    };
    auto one = render (1, 0.0f, 6, false);
    check (maxDiff (one.first, one.second) < 1e-7f, "Unison 1: the voice stays centred (mono)");

    auto three = render (3, 0.0f, 6, false);
    std::vector<float> side (three.first.size());
    for (size_t i = 0; i < side.size(); ++i) side[i] = three.first[i] - three.second[i];
    const float width = 20.0f * std::log10 (rms (side, 4800) / rms (three.first, 4800));
    check (width > -20.0f, "Unison 3 spreads the sound across the stereo field", "side level " + db (width));

    const float lvl = 20.0f * std::log10 (rms (three.first, 4800) / rms (one.first, 4800));

    // Deep bass stays centred even with wide unison (note 33 = 55 Hz).
    AcidEngine lo; lo.prepare (SR);
    SoundParams lp = dryParams(); lp.voice = open(); lp.voice.unison = 7;
    std::vector<float> ll (24000), lr (24000);
    lo.render (ll.data(), lr.data(), 64, lp);
    lo.noteOn (33, 80);
    lo.render (ll.data(), lr.data(), 24000, lp);
    std::vector<float> ls (8192), lm (8192);
    for (size_t i = 0; i < 8192; ++i) { ls[i] = ll[i + 12000] - lr[i + 12000]; lm[i] = ll[i + 12000] + lr[i + 12000]; }
    const float bassSide = levelAt (ls, 55.0) - levelAt (lm, 55.0);
    check (bassSide < -20.0f, "Unison keeps the deep bass centred", "left/right difference at 55 Hz: " + db (bassSide));
    check (std::fabs (lvl) < 4.0f, "Unison keeps the level steady", db (lvl));

    check (unisonLimit (7, 8) == 3 && unisonLimit (7, 2) == 7 && unisonLimit (3, 6) == 3 && unisonLimit (7, 6) == 4,
           "Poly caps Unison at 24 oscillators in total (8 voices x 3)");
}

static void testNeblina()
{
    // Texture stage: bypass at 0 %.
    auto l = sine (0.5f), r = l, in = l;
    NeblinaTexture t; t.prepare (SR);
    t.process (l.data(), r.data(), (int) l.size(), 0.0f);
    check (maxDiff (l, in) < 1e-7f && maxDiff (r, in) < 1e-7f, "Neblina 0% leaves the sound untouched");

    // At 100 % the sound keeps changing over time (drifting resonances + phaser).
    AcidEngine e; e.prepare (SR);
    SoundParams p = dryParams(); p.voice = clean(); p.voice.cutoffHz = 3000; p.voice.neblina = 1.0f; p.voice.unison = 3;
    const int n = (int) (6.0 * SR);
    std::vector<float> L ((size_t) n), R ((size_t) n);
    e.render (L.data(), R.data(), 64, p);
    e.noteOn (45, 80);
    e.render (L.data(), R.data(), n, p);
    std::vector<float> w1 (L.begin() + (long) (1.0 * SR), L.begin() + (long) (1.0 * SR) + 8192);
    std::vector<float> w2 (L.begin() + (long) (4.5 * SR), L.begin() + (long) (4.5 * SR) + 8192);
    float change = 0.0f;
    for (int h = 2; h <= 20; ++h) change = std::max (change, std::fabs (levelAt (w1, 110.0 * h) - levelAt (w2, 110.0 * h)));
    check (change > 3.0f, "Neblina keeps the tone moving over time", "harmonics shift up to " + db (change));
    bool finite = true; float peak = 0.0f;
    for (float x : L) { if (! std::isfinite (x)) finite = false; peak = std::max (peak, std::fabs (x)); }
    check (finite && peak < 2.0f, "Neblina 100% stays stable");

    check (std::fabs (unisonSpreadCents (0.0f) - 8.0f) < 1e-4f && std::fabs (unisonSpreadCents (1.0f) - 33.0f) < 1e-4f,
           "Neblina widens the unison detune (\u00b18 to \u00b133 cents)");
}

static void testCpu()
{
    AcidEngine e; e.prepare (SR);
    SoundParams p;
    p.poly = true; p.voices = 8; p.voice.unison = 7; p.voice.neblina = 1.0f;
    p.chorusMix = 0.5f; p.delayMix = 0.5f; p.reverbMix = 0.5f; p.crush = 0.3f;
    std::vector<float> l (512), r (512);
    e.render (l.data(), r.data(), 64, p);
    for (int n = 0; n < 8; ++n) e.noteOn (40 + n * 3, 90);
    const auto start = std::chrono::steady_clock::now();
    for (int b = 0; b < (int) (SR / 512); ++b) e.render (l.data(), r.data(), 512, p);
    const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - start).count();
    char d[64]; std::snprintf (d, 64, "1 s of audio in %.0f ms", secs * 1000.0);
    check (secs < 0.5, "Worst case (8 voices x 3 unison, all effects) runs well under real time", d);
}

// ---------------------------------------------------------------------------
static void writeStereoWav (const char* path, const std::vector<float>& L, const std::vector<float>& R, int sr)
{
    FILE* f = std::fopen (path, "wb");
    if (! f) return;
    const uint32_t n = (uint32_t) L.size(), bytes = n * 4;
    auto u32 = [&] (uint32_t v) { std::fwrite (&v, 4, 1, f); };
    auto u16 = [&] (uint16_t v) { std::fwrite (&v, 2, 1, f); };
    std::fwrite ("RIFF", 1, 4, f); u32 (36 + bytes); std::fwrite ("WAVEfmt ", 1, 8, f);
    u32 (16); u16 (1); u16 (2); u32 ((uint32_t) sr); u32 ((uint32_t) sr * 4); u16 (4); u16 (16);
    std::fwrite ("data", 1, 4, f); u32 (bytes);
    for (size_t i = 0; i < L.size(); ++i)
        for (float s : { L[i], R[i] })
        {
            const int16_t v = (int16_t) std::lround (std::clamp (s, -1.0f, 1.0f) * 32767.0f);
            std::fwrite (&v, 2, 1, f);
        }
    std::fclose (f);
}

static void renderPreview()
{
    struct Step { int note; bool accent, slide, rest; };
    const Step pat[16] = {
        {33,true,false,false},{33,false,false,false},{45,false,true,false},{33,false,false,false},
        {36,true,false,false},{0,false,false,true},  {33,false,true,false},{40,false,false,false},
        {33,true,false,false},{45,false,true,false}, {43,false,false,false},{33,false,false,false},
        {38,true,true,false}, {40,false,false,false},{0,false,false,true},  {31,true,false,false}};

    const int sr = 48000, stepLen = (int) std::lround (60.0 / 128.0 / 4.0 * sr);
    AcidEngine e; e.prepare (sr);
    std::vector<float> outL, outR;
    int last = -1;

    auto play = [&] (const SoundParams& p, int steps)
    {
        std::vector<float> bl ((size_t) stepLen), br ((size_t) stepLen);
        for (int s = 0; s < steps; ++s)
        {
            const Step& st = pat[s % 16];
            const Step& nx = pat[(s + 1) % 16];
            if (! st.rest)
            {
                e.noteOn (st.note, st.accent ? 110 : 80);
                if (last >= 0 && last != st.note) e.noteOff (last);
                last = st.note;
            }
            const bool hold = ! st.rest && nx.slide && ! nx.rest;
            const int gate = hold ? stepLen : stepLen / 2;
            e.render (bl.data(), br.data(), gate, p);
            if (! hold && last >= 0) { e.noteOff (last); last = -1; }
            e.render (bl.data() + gate, br.data() + gate, stepLen - gate, p);
            outL.insert (outL.end(), bl.begin(), bl.end());
            outR.insert (outR.end(), br.begin(), br.end());
        }
    };

    for (int bar = 0; bar < 8; ++bar)
    {
        SoundParams p;   // Squelch Init defaults: delay 1/8 dotted, plate reverb, comp
        p.bpm = 128.0;
        p.voice.reso = 0.85f; p.voice.decayMs = 300.0f; p.voice.cutoffHz = 300.0f + 120.0f * (float) (bar % 2);
        switch (bar / 2)
        {
            case 0: break;                                                                              // defaults
            case 1: p.voice.wave = 1; p.voice.shape = 0.5f; p.chorusMix = 0.45f; p.reverbType = Reverb::Room; p.reverbMix = 0.25f; break;
            case 2: p.voice.wave = 3; p.voice.shape = 0.4f; p.delayDiv = 3; p.delayFb = 0.6f; p.delayMix = 0.35f;
                    p.reverbType = Reverb::Spring; p.reverbMix = 0.3f; break;                          // FM, 1/16 dotted, spring
            default: p.crush = 0.35f; p.reverbType = Reverb::Hall; p.reverbMix = 0.3f; p.drive = 0.6f; break;
        }
        play (p, 16);
    }
    // 2 bars: Unison 5 with Neblina 60%.
    {
        SoundParams p; p.bpm = 128.0;
        p.voice.reso = 0.8f; p.voice.decayMs = 350.0f; p.voice.cutoffHz = 380.0f;
        p.voice.unison = 5; p.voice.neblina = 0.6f; p.reverbMix = 0.2f;
        play (p, 32);
    }
    // 2 bars: Poly chord stabs (Square, 4 voices, unison 2, Neblina 40%).
    {
        SoundParams p; p.bpm = 128.0; p.poly = true; p.voices = 4;
        p.voice.wave = 1; p.voice.shape = 0.5f; p.voice.reso = 0.6f; p.voice.cutoffHz = 700.0f; p.voice.envMod = 0.5f;
        p.voice.unison = 2; p.voice.neblina = 0.4f; p.voice.slideMs = 15.0f;
        p.delayMix = 0.3f; p.reverbType = Reverb::Hall; p.reverbMix = 0.3f;
        if (last >= 0) { e.noteOff (last); last = -1; }
        const int chords[4][3] = { { 57, 60, 64 }, { 57, 60, 64 }, { 55, 59, 62 }, { 53, 57, 60 } };
        std::vector<float> bl ((size_t) stepLen), br ((size_t) stepLen);
        for (int s = 0; s < 32; ++s)
        {
            const bool hit = (s % 4 == 0) || (s % 8 == 3) || (s % 8 == 6);
            const auto& c = chords[(s / 8) % 4];
            if (hit) for (int k = 0; k < 3; ++k) e.noteOn (c[k], s % 8 == 0 ? 110 : 85);
            e.render (bl.data(), br.data(), stepLen / 2, p);
            if (hit) for (int k = 0; k < 3; ++k) e.noteOff (c[k]);
            e.render (bl.data() + stepLen / 2, br.data() + stepLen / 2, stepLen - stepLen / 2, p);
            outL.insert (outL.end(), bl.begin(), bl.end());
            outR.insert (outR.end(), br.begin(), br.end());
        }
    }

    // Let the last tails ring out for one bar.
    SoundParams tailP; tailP.bpm = 128.0; tailP.poly = true; tailP.voices = 4; tailP.delayMix = 0.3f;
    tailP.reverbType = Reverb::Hall; tailP.reverbMix = 0.3f;
    std::vector<float> tl ((size_t) (stepLen * 16)), tr ((size_t) (stepLen * 16));
    if (last >= 0) e.noteOff (last);
    e.render (tl.data(), tr.data(), stepLen * 16, tailP);
    outL.insert (outL.end(), tl.begin(), tl.end());
    outR.insert (outR.end(), tr.begin(), tr.end());

    float peak = 1e-6f;
    for (size_t i = 0; i < outL.size(); ++i) peak = std::max ({ peak, std::fabs (outL[i]), std::fabs (outR[i]) });
    for (size_t i = 0; i < outL.size(); ++i) { outL[i] *= 0.9f / peak; outR[i] *= 0.9f / peak; }
    writeStereoWav ("acido_preview.wav", outL, outR, sr);
    std::printf ("Wrote acido_preview.wav (stereo) — 12 bars at 128 BPM, 2 bars each:\n"
                 "  Defaults · Square with chorus and room · FM with delay and spring · Crush with hall ·\n"
                 "  Unison 5 with Neblina · Poly chord stabs — then the tail rings out\n");
}

int main()
{
    std::printf ("ÁCIDO sound checks\n\nCore voice\n");
    testStability();
    testSilenceAfterRelease();
    testResonancePeak();
    testResonanceRange();
    testFilterSlope();
    testSlide();
    testNewNoteJumps();
    testReturnToHeld();
    testAccent();
    testBlockSizeIndependence();
    std::printf ("\nOscillator and voice\n");
    testSine();
    testSawMorph();
    testPulseWidth();
    testFm();
    testSub();
    testSubPower();
    testFilterFm();
    testDrift();
    testVelo();
    testNoise();
    std::printf ("\nDrive and Noise (sends)\n");
    testDriveBypass();
    testDriveHarmonics();
    testDriveIsASend();
    testNoiseSend();
    testSafety();
    std::printf ("\nEffects\n");
    testChorus();
    testDelay();
    testReverb();
    testCrush();
    testComp();
    testCompPunch();
    testDefaultsStereo();
    std::printf ("\nStep modulator\n");
    testLaneTiming();
    testModulator();
    testFills();
    std::printf ("\nPoly, Unison and Neblina\n");
    testPoly();
    testUnison();
    testNeblina();
    testCpu();
    std::printf ("\n");
    renderPreview();
    std::printf ("\n%s: %d check(s) failed\n", failures ? "FAILED" : "ALL PASSED", failures);
    return failures ? 1 : 0;
}
