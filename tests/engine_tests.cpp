// RASPA stage 1 engine checks. Build: g++ -std=c++17 -O2 -Isource tests/engine_tests.cpp
#include "engine.h"
#include "generator.h"
#include <cstdio>
#include <string>

using namespace raspa;

static int failures = 0, passes = 0;
static void check (bool ok, const std::string& what)
{
    if (ok) { ++passes; std::printf ("  ok    %s\n", what.c_str()); }
    else    { ++failures; std::printf ("  FAIL  %s\n", what.c_str()); }
}

// A tiny pretend host: plays blocks, keeps the transport moving, delivers notes.
struct Host
{
    Engine e;
    double sr = 48000, bpm = 120;
    bool playing = false;
    double ppq = 0;
    int block = 256;
    long long now = 0;
    std::vector<std::pair<long long, NoteEvent>> pending;  // absolute sample
    float peak = 0; bool bad = false; double sumSq = 0; long long n = 0;
    bool keep = false; std::vector<float> outL, outR;
    std::vector<std::pair<long long, MidiOutEvent>> midi;   // absolute sample

    Host() { e.prepare (sr); e.logging = true; }
    void fill (int lane, int value = 3) { for (auto& p : e.lanes[lane].pattern) p.store (value); }
    void note (long long at, int lane, bool on) { pending.push_back ({ at, { 0, firstNote + lane, on } }); }

    void run (long long samples)
    {
        std::vector<float> L (block), R (block);
        long long end = now + samples;
        while (now < end)
        {
            int len = (int) std::min<long long> (block, end - now);
            std::vector<NoteEvent> evs;
            for (auto& p : pending)
                if (p.first >= now && p.first < now + len)
                    evs.push_back ({ (int) (p.first - now), p.second.note, p.second.on });
            std::sort (evs.begin(), evs.end(), [] (auto& a, auto& b) { return a.sample < b.sample; });
            Transport t { bpm, playing, ppq };
            e.process (L.data(), R.data(), len, t, evs);
            for (auto& m : e.midiOut) midi.push_back ({ now + m.sample, m });
            if (keep) { outL.insert (outL.end(), L.begin(), L.begin() + len); outR.insert (outR.end(), R.begin(), R.begin() + len); }
            for (int i = 0; i < len; ++i)
            {
                if (! std::isfinite (L[i]) || ! std::isfinite (R[i])) bad = true;
                peak = std::max (peak, std::fabs (R[i]));
                peak = std::max (peak, std::fabs (L[i]));
                sumSq += (double) L[i] * L[i]; ++n;
            }
            if (playing) ppq += len * bpm / 60.0 / sr;
            now += len;
        }
    }
    const std::vector<std::pair<long long, int>>& hits (int lane) { return e.hitLog[lane]; }
};

// One hit of a voice: returns how long it stays above `floor` (samples) and its RMS.
struct HitInfo { long long len; double rms; float peak; bool bad; };
static HitInfo oneHit (int type, float a, float b, float d, int stepValue, float floor = 0.002f, long long maxLen = 48000 * 6)
{
    Voice v (777); v.prepare (48000); v.setParams (type, a, b, d);
    v.trigger (stepVelocity (stepValue), stepValue == 3);
    HitInfo h { 0, 0, 0, false }; double ss = 0;
    for (long long i = 0; i < maxLen; ++i)
    {
        float y = v.process();
        if (! std::isfinite (y)) h.bad = true;
        if (std::fabs (y) > floor) h.len = i;
        h.peak = std::max (h.peak, std::fabs (y));
        ss += (double) y * y;
    }
    h.rms = std::sqrt (ss / (double) maxLen);
    return h;
}

int main()
{
    std::printf ("RASPA engine checks (stage 6 + filter types)\n");
    const long long step = 6000; // 120 BPM at 48 kHz: one 16th = 6000 samples

    {   // 1. hold to play
        Host h; h.fill (0);
        h.note (1000, 0, true); h.note (1000 + 4 * step - 1, 0, false);
        h.run (200000);
        auto& x = h.hits (0);
        bool ok = x.size() == 4;
        for (size_t i = 0; ok && i < x.size(); ++i) ok = x[i].first == 1000 + (long long) i * step && x[i].second == (int) i;
        check (ok, "held note: step 1 fires on the note, one step every 16th, stops on release");
    }
    {   // 2. instant restart while playing
        Host h; h.fill (0);
        h.note (0, 0, true); h.note (15000, 0, true); h.note (40000, 0, false); h.note (40000, 0, false);
        h.run (60000);
        auto& x = h.hits (0);
        bool ok = x.size() >= 6 && x[0].first == 0 && x[1].first == step && x[2].first == 2 * step
               && x[3].first == 15000 && x[3].second == 0 && x[4].first == 15000 + step && x[4].second == 1;
        check (ok, "new note while playing restarts from step 1 instantly");
        check (x.back().first < 40000, "lane stops when every held note is released");
    }
    {   // 3. latch locks to the song grid
        Host h; h.fill (1); h.e.lanes[1].latch = true; h.playing = true;
        h.run (step * 20);
        auto& x = h.hits (1);
        bool ok = x.size() == 20;
        for (size_t i = 0; ok && i < x.size(); ++i) ok = x[i].first == (long long) i * step && x[i].second == (int) (i % 16);
        check (ok, "latch: plays with the transport, on the grid, steps wrap 16 -> 1");
    }
    {   // 4. latch joining mid-step waits for the next 16th
        Host h; h.fill (1); h.e.lanes[1].latch = true; h.playing = true; h.ppq = 1.1; // 4.4 steps in
        h.run (step * 3);
        auto& x = h.hits (1);
        long long first = (long long) std::llround (0.6 * step);
        check (! x.empty() && std::llabs (x[0].first - first) <= 1 && x[0].second == 5, "latch: joining mid-step waits for the next grid step");
    }
    {   // 5. latch + note restarts from step 1 at that instant
        Host h; h.fill (1); h.e.lanes[1].latch = true; h.playing = true;
        h.note (20000, 1, true); h.note (20500, 1, false);
        h.run (40000);
        auto& x = h.hits (1);
        bool found = false;
        for (size_t i = 0; i + 1 < x.size(); ++i)
            if (x[i].first == 20000 && x[i].second == 0 && x[i + 1].first == 26000 && x[i + 1].second == 1) found = true;
        check (found, "latch: a note restarts from step 1, note-off doesn't stop it");
    }
    {   // 6. latch with transport stopped = silent until a note, then hold-to-play
        Host h; h.fill (2); h.e.lanes[2].latch = true;
        h.run (50000);
        bool silent = h.hits (2).empty();
        h.note (60000, 2, true); h.note (60000 + 2 * step, 2, false);
        h.run (60000);
        check (silent && h.hits (2).size() == 2, "latch with Ableton stopped: silent, notes still play while held");
    }
    {   // 7. tempo follows the host
        Host h; h.bpm = 140; h.fill (3);
        h.note (0, 3, true);
        h.run (48000);
        auto& x = h.hits (3);
        double expect = 48000.0 * 60.0 / 140.0 / 4.0;
        check (x.size() > 3 && std::llabs (x[3].first - (long long) std::ceil (3 * expect - 1e-6)) <= 1, "step length follows the host tempo (140 BPM)");
    }
    {   // 8. pattern respected
        Host h;
        int pat[16] = { 3,0,0,1, 0,0,2,0, 3,0,0,0, 0,0,1,0 };
        for (int i = 0; i < 16; ++i) h.e.lanes[0].pattern[i].store (pat[i]);
        h.note (0, 0, true);
        h.run (16 * step);
        auto& x = h.hits (0);
        std::vector<int> got; for (auto& p : x) got.push_back (p.second);
        check (got == std::vector<int> { 0, 3, 6, 8, 14 }, "empty steps stay silent, filled steps play");
    }
    {   // 9. block size doesn't change timing
        auto scenario = [] (int block)
        {
            Host h; h.block = block; h.fill (0); h.fill (1); h.e.lanes[1].latch = true; h.playing = true;
            h.note (12345, 0, true); h.note (55555, 0, false); h.note (31000, 1, true); h.note (31001, 1, false);
            h.run (100000);
            auto a = h.hits (0); auto b = h.hits (1); a.insert (a.end(), b.begin(), b.end());
            return a;
        };
        check (scenario (32) == scenario (1024) && scenario (1024) == scenario (441), "same timing at any buffer size");
    }
    {   // 10. sound: safe at every extreme
        bool allSafe = true; float worst = 0;
        for (float g : { 0.0f, 0.5f, 1.0f })
            for (float s : { 0.0f, 1.0f })
                for (float d : { 0.0f, 1.0f })
                {
                    Host h;
                    for (int l = 0; l < 4; ++l) { h.fill (l); h.e.lanes[l].level = 1.0f; h.e.lanes[l].voice.setParams (shaker, g, s, d); h.note (0, l, true); }
                    h.run (96000);
                    worst = std::max (worst, h.peak);
                    if (h.bad || h.peak > 1.2f) allSafe = false;
                }
        char b[96]; std::snprintf (b, sizeof b, "no clicks/NaN at any setting, peak stays under 1.2 (worst %.2f)", worst);
        check (allSafe, b);
    }
    {   // 11. grain changes density, not loudness
        double rms[2]; int k = 0;
        for (float g : { 0.1f, 0.9f })
        {
            Host h; h.fill (0); h.e.lanes[0].voice.setParams (shaker, g, 0.5f, 0.6f); h.note (0, 0, true);
            h.run (96000);
            rms[k++] = std::sqrt (h.sumSq / h.n);
        }
        double ratio = rms[1] / rms[0];
        char b[96]; std::snprintf (b, sizeof b, "Grain low vs high stay within 2x in loudness (ratio %.2f)", ratio);
        check (ratio > 0.5 && ratio < 2.0, b);
    }
    {   // 12. audible, then silent after release
        Host h; h.fill (0); h.note (0, 0, true); h.note (step * 8, 0, false);
        h.run (step * 8);
        float playingPeak = h.peak;
        h.run (48000);                 // tail
        h.peak = 0; h.run (4800);
        check (playingPeak > 0.1f && h.peak == 0.0f, "audible while held, fully silent after the tail");
    }
    {   // 13. decay knob sets shake length
        auto tailLen = [] (float d)
        {
            Host h; h.e.lanes[0].pattern[0].store (3); h.e.lanes[0].voice.setParams (shaker, 0.7f, 0.4f, d);
            h.note (0, 0, true); h.note (100, 0, false);
            long long last = 0; std::vector<float> L (64), R (64);
            for (long long t = 0; t < 48000; t += 64)
            {
                std::vector<NoteEvent> ev;
                if (t == 0) { ev.push_back ({ 0, firstNote, true }); ev.push_back ({ 63, firstNote, false }); }
                h.e.process (L.data(), R.data(), 64, { 120, false, 0 }, ev);
                for (float v : L) if (std::fabs (v) > 0.001f) last = t;
            }
            return last;
        };
        long long a = tailLen (0.0f), b = tailLen (1.0f);
        check (b > a * 5, "Decay: short shake vs long shake");
    }

    std::printf ("\n  -- stage 2: the eight voices --\n");
    {   // every voice: safe at every extreme, audible, then silent
        for (int t = 0; t < numVoiceTypes; ++t)
        {
            bool safe = true, audible = true, quiet = true; float worst = 0;
            for (float a : { 0.0f, 1.0f }) for (float b : { 0.0f, 1.0f }) for (float d : { 0.0f, 1.0f })
            {
                Host h; h.fill (0); h.e.lanes[0].level = 1.0f; h.e.lanes[0].voice.setParams (t, a, b, d);
                h.note (0, 0, true); h.note (6000 * 16, 0, false);
                h.run (6000 * 16);
                if (h.bad || h.peak > 1.2f) safe = false;
                if (h.peak < 0.05f) audible = false;
                worst = std::max (worst, h.peak);
                h.run (48000 * 5); h.peak = 0; h.run (4800);
                if (h.peak > 1.0e-4f || h.e.lanes[0].voice.active()) quiet = false;
            }
            char b[120]; std::snprintf (b, sizeof b, "%s: safe at all extremes (peak %.2f), audible, silent after its tail", voiceName (t), worst);
            check (safe && audible && quiet, b);
        }
    }
    {   // levels balanced at default settings
        double lo = 1e9, hi = -1e9;
        for (int t = 0; t < numVoiceTypes; ++t)
        {
            Host h; int pat[16] = { 3,0,2,2, 3,0,2,1, 3,0,2,2, 3,0,1,2 };
            for (int i = 0; i < 16; ++i) h.e.lanes[0].pattern[i].store (pat[i]);
            h.e.lanes[0].level = 1; h.e.lanes[0].voice.setParams (t, 0.5f, 0.5f, 0.4f); h.note (0, 0, true);
            h.run (48000 * 4);
            double db = 20 * std::log10 (std::sqrt (h.sumSq / (double) h.n) + 1e-9);
            lo = std::min (lo, db); hi = std::max (hi, db);
        }
        char b[96]; std::snprintf (b, sizeof b, "all voices within 10 dB of each other (%.1f .. %.1f dB RMS)", lo, hi);
        check (hi - lo < 10.0, b);
    }
    {
        auto acc = oneHit (guiro, 0.6f, 0.5f, 0.3f, 3), gh = oneHit (guiro, 0.6f, 0.5f, 0.3f, 1);
        check (acc.len > gh.len * 2, "G\xc3\xbciro: accents get the long stroke, other steps a short one");
        auto slow = oneHit (guiro, 1.0f, 0.5f, 0.3f, 3), fast = oneHit (guiro, 0.0f, 0.5f, 0.3f, 3);
        check (slow.len > fast.len * 3, "G\xc3\xbciro: Stroke sets the scrape length");
    }
    {
        auto openAcc = oneHit (hihat, 0.5f, 1.0f, 0.3f, 3), closedAcc = oneHit (hihat, 0.5f, 0.0f, 0.3f, 3);
        auto openGhost = oneHit (hihat, 0.5f, 1.0f, 0.3f, 2), closedGhost = oneHit (hihat, 0.5f, 0.0f, 0.3f, 2);
        check (openAcc.len > closedAcc.len * 4 && std::llabs (openGhost.len - closedGhost.len) < closedGhost.len / 4 + 50,
               "Hi-hat: Open lengthens accented hits only");
    }
    {
        auto crack = oneHit (clank, 0.0f, 0.5f, 0.4f, 3), metal = oneHit (clank, 1.0f, 0.5f, 0.4f, 3);
        check (metal.len > crack.len * 3, "Crack/Clank: Morph goes from a short crack to a ringing clank");
    }
    {
        auto openHit = oneHit (triangle, 0.5f, 1.0f, 0.4f, 3), muted = oneHit (triangle, 0.5f, 1.0f, 0.4f, 2);
        auto noMute = oneHit (triangle, 0.5f, 0.0f, 0.4f, 2);
        check (openHit.len > muted.len * 5 && noMute.len > muted.len * 5, "Triangle: Mute damps the non-accented hits, accents ring open");
    }
    {
        auto tight = oneHit (glass, 0.8f, 0.0f, 0.0f, 3, 0.001f), wide = oneHit (glass, 0.8f, 1.0f, 0.0f, 3, 0.001f);
        check (wide.len > tight.len * 2, "Broken Glass: Spread scatters the shards over a longer time");
        auto few = oneHit (glass, 0.0f, 0.5f, 0.3f, 3), many = oneHit (glass, 1.0f, 0.5f, 0.3f, 3);
        check (many.rms > few.rms * 1.3, "Broken Glass: more Shards, more pieces");
    }
    {
        auto clean = oneHit (lofi, 0.0f, 0.5f, 0.4f, 3, 0.0005f), dusty = oneHit (lofi, 1.0f, 0.5f, 0.4f, 3, 0.0005f);
        check (dusty.len > clean.len, "Lo-Fi: Dust adds a crackle tail");
        auto thin = oneHit (lofi, 0.3f, 0.0f, 0.4f, 3), thick = oneHit (lofi, 0.3f, 1.0f, 0.4f, 3);
        check (thick.len > thin.len, "Lo-Fi: Body goes from a thin tick to a thicker hit");
    }
    {   // metal guiro: brighter than the wooden one, same long/short strokes
        auto zcr = [] (int type)
        {
            Voice v (99); v.prepare (48000); v.setParams (type, 0.6f, 0.5f, 0.3f); v.trigger (1.0f, true);
            int crossings = 0; float last = 0; double energy = 0;
            for (int i = 0; i < 12000; ++i) { float y = v.process(); if ((y > 0) != (last > 0) && std::fabs (y) > 1e-4f) ++crossings; last = y; energy += y * y; }
            return crossings;
        };
        check (zcr (metalGuiro) > zcr (guiro) * 1.5, "Metal G\xc3\xbciro: brighter, metallic scrape compared to the wooden g\xc3\xbciro");
        auto acc = oneHit (metalGuiro, 0.6f, 0.5f, 0.3f, 3), gh = oneHit (metalGuiro, 0.6f, 0.5f, 0.3f, 1);
        check (acc.len > gh.len * 2, "Metal G\xc3\xbciro: accents get the long stroke too");
    }
    {   // switching voice while playing
        Host h; h.fill (0); h.e.lanes[0].level = 1; h.note (0, 0, true);
        bool ok = true;
        for (int t = 0; t < numVoiceTypes * 2; ++t)
        {
            h.e.lanes[0].voice.setParams (t % numVoiceTypes, 0.5f, 0.5f, 0.5f);
            h.peak = 0; h.run (12000);
            if (h.bad || h.peak < 0.01f || h.peak > 1.2f) ok = false;
        }
        check (ok, "switching voices mid-pattern: no glitches, the new voice plays right away");
    }

    std::printf ("\n  -- stage 3: genre generator, swing, accent, humanize --\n");
    {
        bool exact = true;
        for (int v = 0; v < numVoiceTypes; ++v)
            for (int g = 0; g < numGenres - 1; ++g)   // the six genre templates (Euclidean is checked below)
            {
                int p[16]; generatePattern (v, g, 0.5f, 0.0f, 12345, p);
                const char* t = templates[roleFor (v)][g];
                for (int i = 0; i < 16; ++i)
                {
                    int want = t[i] - '0';
                    if ((want > 0) != (p[i] > 0) || (want > 0 && p[i] != want)) exact = false;
                }
            }
        check (exact, "Variation 0, Density 50%: every voice plays its genre template exactly");
    }
    {
        int p[16]; generatePattern (guiro, cumbia, 0.5f, 0.0f, 1, p);
        int want[16] = { 3,0,2,2, 3,0,2,2, 3,0,2,2, 3,0,2,2 };
        check (std::equal (p, p + 16, want), "Cumbia g\xc3\xbciro: largo-corto-corto (long stroke on the beat, two short after)");
    }
    {
        bool same = true, differ = false;
        for (uint32_t seed = 1; seed < 40; ++seed)
        {
            int a[16], b[16], c[16];
            generatePattern (shaker, afroHouse, 0.6f, 0.5f, seed, a);
            generatePattern (shaker, afroHouse, 0.6f, 0.5f, seed, b);
            generatePattern (shaker, afroHouse, 0.6f, 0.5f, seed + 1000, c);
            if (! std::equal (a, a + 16, b)) same = false;
            if (! std::equal (a, a + 16, c)) differ = true;
        }
        check (same && differ, "same dice = same pattern; a new roll gives a new variation");
    }
    {
        bool mono = true, ends = true;
        for (int v = 0; v < numVoiceTypes; ++v)
            for (int g = 0; g < numGenres - 1; ++g)
                for (float var : { 0.0f, 0.3f, 0.8f })
                {
                    int prev[16] = {};
                    for (int k = 0; k <= 20; ++k)
                    {
                        int p[16]; generatePattern (v, g, (float) k / 20.0f, var, 77, p);
                        for (int i = 0; i < 16; ++i) if (prev[i] > 0 && p[i] == 0) mono = false;
                        std::copy (p, p + 16, prev);
                        int count = 0; for (int x : p) count += x > 0;
                        if (k == 0 && count != 0) ends = false;
                        if (k == 20 && count != 16) ends = false;
                    }
                }
        check (mono, "raising Density only adds steps, never moves the ones already playing");
        check (ends, "Density 0 = silence, Density 100% = every step");
    }
    {
        int total = 0, changed = 0;
        for (uint32_t seed = 1; seed < 50; ++seed)
        {
            int a[16], b[16];
            generatePattern (hihat, house, 0.5f, 0.0f, seed, a);
            generatePattern (hihat, house, 0.5f, 0.7f, seed, b);
            for (int i = 0; i < 16; ++i) { ++total; changed += a[i] != b[i]; }
        }
        check (changed > total / 8 && changed < total * 3 / 4, "Variation reshapes part of the pattern, keeps the genre's backbone");
    }
    {   // Euclidean
        bool even = true, counts = true;
        for (int k = 0; k <= 20; ++k)
        {
            float d = (float) k / 20.0f;
            int p[16]; generatePattern (shaker, euclidean, d, 0.0f, 5, p);
            int hits = 0; std::vector<int> pos;
            for (int i = 0; i < 16; ++i) if (p[i]) { ++hits; pos.push_back (i); }
            if (hits != euclidHits (d)) counts = false;
            if (hits >= 2)
            {
                int mn = 99, mx = 0;
                for (size_t i = 0; i < pos.size(); ++i)
                {
                    int gap = (i + 1 < pos.size() ? pos[i + 1] : pos[0] + 16) - pos[i];
                    mn = std::min (mn, gap); mx = std::max (mx, gap);
                }
                if (mx - mn > 1) even = false;
            }
            if (hits > 0 && (p[0] != 3)) even = false;
        }
        int p5[16]; generatePattern (hihat, euclidean, 0.5f, 0.0f, 1, p5);
        int on5[16]; for (int i = 0; i < 16; ++i) on5[i] = p5[i] > 0;
        int want5[16] = { 1,0,0,0, 1,0,0,1, 0,0,1,0, 0,1,0,0 };
        check (counts && even, "Euclidean: Density sets the number of hits, always spread evenly, first hit is an accent");
        check (std::equal (on5, on5 + 16, want5), "Euclidean: Density 50% = the classic 5-in-16");
        int r0[16], r3[16]; generatePattern (shaker, euclidean, 0.5f, 0.0f, 9, r0); generatePattern (shaker, euclidean, 0.5f, 3.0f / 15.0f, 9, r3);
        bool rotated = true; for (int i = 0; i < 16; ++i) if (r3[(i + 3) % 16] != r0[i]) rotated = false;
        check (rotated, "Euclidean: Variation rotates the pattern step by step");
        int a[16], b[16]; generatePattern (guiro, euclidean, 0.7f, 0.0f, 1, a); generatePattern (guiro, euclidean, 0.7f, 0.0f, 999, b);
        bool samePlaces = true, diffLevels = false;
        for (int i = 0; i < 16; ++i) { if ((a[i] > 0) != (b[i] > 0)) samePlaces = false; if (a[i] != b[i]) diffLevels = true; }
        check (samePlaces && diffLevels, "Euclidean: Dice changes the accents, not where the hits fall");
    }
    {   // swing
        Host h; h.fill (0); h.e.lanes[0].swing = 1.0f; h.note (0, 0, true);
        h.run (8 * step);
        auto& x = h.hits (0);
        bool ok = x.size() >= 6 && x[0].first == 0 && x[1].first == step + step / 2 && x[2].first == 2 * step && x[3].first == 3 * step + step / 2;
        check (ok, "Swing 100%: every second 16th lands half a step late (75% swing), beats stay put");
        Host h2; h2.fill (0); h2.e.lanes[0].swing = 0.5f; h2.note (0, 0, true); h2.run (4 * step);
        check (h2.hits (0).size() >= 2 && h2.hits (0)[1].first == step + step / 4, "Swing 50%: a quarter step late (62.5% swing)");
    }
    {   // restart drops a waiting swung hit
        Host h; h.fill (0); h.e.lanes[0].swing = 1.0f;
        h.note (0, 0, true); h.note (step + 100, 0, true);
        h.run (3 * step);
        auto& x = h.hits (0);
        bool ok = x.size() >= 2 && x[1].first == step + 100 && x[1].second == 0;
        for (auto& p : x) if (p.first == step + step / 2) ok = false;
        check (ok, "a restart during a swung step cancels the late hit");
    }
    {   // accent contrast
        auto ratio = [] (float acc)
        {
            Host h; int pat[16] = { 3,1,3,1, 3,1,3,1, 3,1,3,1, 3,1,3,1 };
            for (int i = 0; i < 16; ++i) h.e.lanes[0].pattern[i].store (pat[i]);
            h.e.lanes[0].accent = acc; h.note (0, 0, true); h.run (16 * step);
            auto& v = h.e.velLog[0];
            return v.size() >= 2 ? v[1] / v[0] : 0.0f;
        };
        float flat = ratio (0.0f), mid = ratio (0.5f), strong = ratio (1.0f);
        char b[120]; std::snprintf (b, sizeof b, "Accent: ghost/accent ratio %.2f (0%%) > %.2f (50%%) > %.2f (100%%)", flat, mid, strong);
        check (flat > mid && mid > strong && std::fabs (mid - stepVelocity (1)) < 0.01f && flat > 0.6f && strong < 0.1f, b);
    }
    {   // humanize
        Host h; h.fill (0); h.e.lanes[0].humanize = 1.0f; h.note (0, 0, true); h.run (64 * step);
        auto& x = h.hits (0); auto& v = h.e.velLog[0];
        long long minD = 1 << 30, maxD = -1; float vmin = 9, vmax = 0;
        for (size_t i = 0; i < x.size(); ++i)
        {
            long long d = x[i].first - (long long) i * step;
            minD = std::min (minD, d); maxD = std::max (maxD, d);
            vmin = std::min (vmin, v[i]); vmax = std::max (vmax, v[i]);
        }
        check (x.size() == 64 && minD >= 0 && maxD <= 576 && maxD - minD > 200, "Humanize: hits drift up to 12 ms late, never early, none lost");
        check (vmin >= 0.84f && vmax <= 1.0f && vmax - vmin > 0.05f, "Humanize: velocity wobbles by up to 15%");
        Host h0; h0.fill (0); h0.note (0, 0, true); h0.run (16 * step);
        bool tight = true; for (size_t i = 0; i < h0.hits (0).size(); ++i) if (h0.hits (0)[i].first != (long long) i * step) tight = false;
        check (tight, "Humanize 0: perfectly on the grid");
    }

    std::printf ("\n  -- pitch --\n");
    {
        // strength of one frequency in a triangle hit (Goertzel), triangle's loudest partial = 2.76 x 900 Hz at Ring 0
        auto power = [] (float pitch, double freq)
        {
            Voice v (5); v.prepare (48000); v.setParams (triangle, 0.0f, 0.0f, 0.6f, pitch); v.trigger (1.0f, true);
            double w = 2.0 * 3.14159265358979 * freq / 48000.0, c = 2 * std::cos (w), s1 = 0, s2 = 0;
            for (int i = 0; i < 24000; ++i) { double s0 = v.process() + c * s1 - s2; s2 = s1; s1 = s0; }
            return s1 * s1 + s2 * s2 - c * s1 * s2;
        };
        const double f = 900.0 * 2.76;
        bool octaveUp = power (12, f * 2) > 50 * power (12, f) && power (0, f) > 50 * power (0, f * 2);
        bool octaveDown = power (-12, f / 2) > 50 * power (-12, f);
        bool semitone = power (1, f * std::pow (2.0, 1.0 / 12.0)) > 20 * power (1, f);
        check (octaveUp && octaveDown && semitone, "Pitch: +12 = exactly an octave up, -12 an octave down, +1 one semitone");
        bool safe = true;
        for (int t = 0; t < numVoiceTypes; ++t)
            for (float pt : { -12.0f, 12.0f })
                for (float a : { 0.0f, 1.0f }) for (float bb : { 0.0f, 1.0f })
                {
                    Host h; h.fill (0); h.e.lanes[0].level = 1; h.e.lanes[0].voice.setParams (t, a, bb, 0.5f, pt); h.note (0, 0, true);
                    h.run (48000);
                    if (h.bad || h.peak > 1.2f || h.peak < 0.02f) safe = false;
                }
        check (safe, "Pitch: every voice stays safe and audible from -12 to +12");
        Voice a (3), b2 (3); a.prepare (48000); b2.prepare (48000);
        a.setParams (hihat, 0.5f, 0.5f, 0.4f); b2.setParams (hihat, 0.5f, 0.5f, 0.4f, 0.0f);
        a.trigger (1, true); b2.trigger (1, true);
        bool same = true; for (int i = 0; i < 4800; ++i) if (a.process() != b2.process()) same = false;
        check (same, "Pitch 0 sounds exactly as before");
    }

    std::printf ("\n  -- stage 4: pan, filter, sends --\n");
    auto energy = [] (const std::vector<float>& v, size_t a, size_t b) { double e = 0; for (size_t i = a; i < std::min (b, v.size()); ++i) e += (double) v[i] * v[i]; return e; };
    auto zcRate = [] (const std::vector<float>& v) { int c = 0; for (size_t i = 1; i < v.size(); ++i) if ((v[i] > 0) != (v[i - 1] > 0)) ++c; return (double) c / (double) v.size(); };
    {   // dry path untouched with everything at default
        Host a; a.fill (0); a.keep = true; a.note (0, 0, true); a.run (48000);
        bool same = a.outL == a.outR;
        check (same, "pan centre, filter open, sends at 0: left = right = the same sound as before");
    }
    {
        Host h; h.fill (0); h.keep = true; h.e.lanes[0].pan = -1.0f; h.note (0, 0, true); h.run (24000);
        Host r; r.fill (0); r.keep = true; r.e.lanes[0].pan = 0.5f; r.note (0, 0, true); r.run (24000);
        double rl = energy (r.outL, 0, 24000), rr = energy (r.outR, 0, 24000);
        check (energy (h.outR, 0, 24000) == 0.0 && energy (h.outL, 0, 24000) > 0.0 && rl < rr * 0.5 && rl > 0, "Pan: hard left = nothing on the right; half right = quieter left");
    }
    {
        auto bright = [&] (float cut, float res)
        {
            Host h; h.fill (0); h.keep = true; h.e.lanes[0].cutoff = cut; h.e.lanes[0].res = res; h.note (0, 0, true); h.run (48000);
            return std::make_pair (zcRate (h.outL), h.peak);
        };
        auto open = bright (1.0f, 0.0f), mid = bright (0.55f, 0.0f), low = bright (0.2f, 0.0f), ring = bright (0.3f, 1.0f);
        check (open.first > mid.first && mid.first > low.first, "Cutoff: lower = darker");
        check (ring.second < 1.2f, "Resonance at max with a low cutoff stays safe");
    }
    {   // delay: per-lane times, echoes, feedback, filter
        Host h; h.keep = true; h.e.lanes[0].pattern[0].store (3); h.e.lanes[1].pattern[0].store (3);
        h.e.lanes[0].voice.setParams (clank, 1.0f, 0.5f, 0.0f);
        h.e.lanes[1].voice.setParams (clank, 1.0f, 0.5f, 0.0f);
        h.e.lanes[0].pan = -1; h.e.lanes[1].pan = 1;
        h.e.lanes[0].send[1] = 1; h.e.lanes[1].send[1] = 1;
        h.e.lanes[0].delayTime = 5;   // 1/8 = 12000 samples at 120 BPM
        h.e.lanes[1].delayTime = 8;   // 1/4 = 24000
        h.e.fx.dlyFeedback = 0.5f; h.e.fx.dlyCutoff = 1.0f; h.e.fx.dlyRes = 0;
        h.note (0, 0, true); h.note (0, 1, true); h.note (100, 0, false); h.note (100, 1, false);
        h.run (96000);
        auto onset = [&] (const std::vector<float>& v, size_t from) { for (size_t i = from; i < v.size(); ++i) if (std::fabs (v[i]) > 0.003f) return (long long) i; return -1LL; };
        long long l1 = onset (h.outL, 9000), r1 = onset (h.outR, 18000);
        double e1 = energy (h.outL, 12000, 14000), e2 = energy (h.outL, 24000, 26000), e3 = energy (h.outL, 36000, 38000);
        char b[150]; std::snprintf (b, sizeof b, "Delay: lane 1 echoes after 1/8 (%lld), lane 2 after 1/4 (%lld), each with its own time", l1, r1);
        check (std::llabs (l1 - 12000) < 40 && std::llabs (r1 - 24000) < 40, b);
        check (e1 > e2 && e2 > e3 && e3 > 0, "Delay: Feedback repeats, each one quieter");
        auto echoBright = [&] (float cut)
        {
            Host d; d.keep = true; d.e.lanes[0].pattern[0].store (3); d.e.lanes[0].voice.setParams (hihat, 0.5f, 0.0f, 0.0f);
            d.e.lanes[0].send[1] = 1; d.e.fx.dlyCutoff = cut; d.e.fx.dlyFeedback = 0.0f; d.e.lanes[0].level = 1;
            d.note (0, 0, true); d.note (50, 0, false); d.run (30000);
            std::vector<float> echoPart (d.outL.begin() + 11000, d.outL.begin() + 18000);
            return zcRate (echoPart);
        };
        check (echoBright (1.0f) > echoBright (0.4f) * 1.5, "Delay: the delay filter darkens the echoes");
        Host m; m.fill (0); m.e.lanes[0].send[1] = 1; m.e.fx.dlyFeedback = 1.0f; m.e.fx.dlyRes = 1.0f; m.e.lanes[0].delayTime = 0;
        m.note (0, 0, true); m.run (48000 * 6);
        check (! m.bad && m.peak < 1.2f, "Delay: max feedback + max resonance never runs away");
    }
    {   // reverb
        auto tail = [&] (float decay)
        {
            Host h; h.keep = true; h.e.lanes[0].pattern[0].store (3); h.e.lanes[0].voice.setParams (clank, 0.0f, 0.5f, 0.0f);
            h.e.lanes[0].send[0] = 1; h.e.fx.verbDecay = decay; h.e.fx.verbReturn = 1;
            h.note (0, 0, true); h.note (50, 0, false); h.run (48000 * 4);
            return std::make_pair (energy (h.outL, 48000, 96000), h.outL != h.outR);
        };
        auto shortT = tail (0.1f), longT = tail (0.9f);
        check (longT.first > shortT.first * 10 && longT.second, "Reverb: Decay sets the tail length; the tail is stereo");
        Host q; q.fill (0); q.e.lanes[0].send[0] = 1; q.e.fx.verbDecay = 1; q.e.fx.verbSize = 1; q.e.fx.verbDamp = 0;
        q.note (0, 0, true); q.run (48000 * 4); bool stable = ! q.bad && q.peak < 1.2f;
        q.note (q.now, 0, false); q.run (48000 * 12); q.peak = 0; q.run (48000);
        check (stable && q.peak < 0.002f, "Reverb: stable at the biggest, longest setting and dies away");
    }
    {   // chorus
        Host h; h.fill (0); h.keep = true; h.e.lanes[0].send[2] = 1; h.e.fx.choReturn = 1; h.e.fx.choWidth = 1; h.e.fx.choDepth = 1;
        h.note (0, 0, true); h.run (48000);
        Host z; z.fill (0); z.keep = true; z.e.lanes[0].send[2] = 1; z.e.fx.choReturn = 0; z.note (0, 0, true); z.run (48000);
        check (h.outL != h.outR && z.outL == z.outR, "Chorus: widens to stereo; Return 0 = no chorus at all");
    }
    {   // reverse: a hit at the start of a slice comes back as a swell into the next slice
        Host h; h.keep = true; h.playing = true; h.e.lanes[0].latch = true;
        h.e.lanes[0].pattern[0].store (3); h.e.lanes[0].voice.setParams (clank, 0.3f, 0.5f, 0.1f);
        h.e.lanes[0].level = 1; h.e.lanes[0].send[3] = 1; h.e.fx.revLength = 2; h.e.fx.revSlice = 1.0f; h.e.fx.revFade = 0.0f; h.e.fx.revReturn = 1;
        Host dry; dry.keep = true; dry.playing = true; dry.e.lanes[0].latch = true;
        dry.e.lanes[0].pattern[0].store (3); dry.e.lanes[0].voice.setParams (clank, 0.3f, 0.5f, 0.1f); dry.e.lanes[0].level = 1;
        h.run (24000 * 2); dry.run (24000 * 2);
        std::vector<float> wet (h.outL.size()); for (size_t i = 0; i < wet.size(); ++i) wet[i] = h.outL[i] - dry.outL[i];
        double first = energy (wet, 24000, 30000), last = energy (wet, 42000, 48000), early = energy (wet, 0, 24000);
        check (early < 1e-9 && last > first * 4, "Reverse: the slice plays backwards in the next one, swelling into the beat");
    }
    {   // everything on: safe, silent afterwards, same at any buffer size
        auto scene = [] (Host& h, int block)
        {
            h.block = block; h.keep = true; h.playing = true;
            for (int l = 0; l < 4; ++l)
            {
                h.fill (l, 2); h.e.lanes[l].latch = true; h.e.lanes[l].voice.setParams (l * 2, 0.6f, 0.4f, 0.5f);
                for (int k = 0; k < 4; ++k) h.e.lanes[l].send[k] = 1.0f;
                h.e.lanes[l].pan = -1.0f + 0.66f * (float) l; h.e.lanes[l].cutoff = 0.7f; h.e.lanes[l].res = 0.6f; h.e.lanes[l].delayTime = l * 3;
            }
            h.e.fx.dlyFeedback = 0.9f; h.e.fx.verbDecay = 0.8f;
            h.run (48000 * 3);
        };
        Host a, b; scene (a, 64); scene (b, 1000);
        check (! a.bad && a.peak < 1.2f && a.outL == b.outL && a.outR == b.outR, "all lanes, all sends at full: safe, and identical at any buffer size");
        Host c; scene (c, 256);
        for (int l = 0; l < 4; ++l) c.e.lanes[l].latch = false;
        c.e.fx.dlyFeedback = 0.5f;   // (at 90% a 1/4-dotted echo honestly rings for ~30 s)
        c.playing = false; c.run (48000 * 20); c.peak = 0; c.run (48000);
        check (c.peak < 1.0e-4f, "after stopping, every effect tail dies to silence");
    }

    std::printf ("\n  -- stage 5: MIDI out --\n");
    {
        Host h; h.fill (0); h.e.lanes[1].pattern[0].store (3); h.e.lanes[1].pattern[8].store (1);
        h.note (0, 0, true); h.note (16 * step, 0, false); h.note (0, 1, true); h.note (16 * step, 1, false);
        h.run (20 * step);
        int ons[2] = { 0, 0 }, offs[2] = { 0, 0 }; bool timing = true, order = true; int open[2] = { 0, 0 };
        for (auto& [at, m] : h.midi)
        {
            if (m.on) { ++ons[m.lane]; ++open[m.lane]; } else { ++offs[m.lane]; --open[m.lane]; }
            if (open[m.lane] < 0 || open[m.lane] > 1) order = false;
        }
        size_t k = 0;
        for (auto& [at, m] : h.midi) if (m.on && m.lane == 0) { if (at != (long long) k * step) timing = false; ++k; }
        check (ons[0] == 16 && ons[1] == 2 && (long long) h.e.hitCount[0] == ons[0], "MIDI out: one note per hit, per lane");
        check (timing, "MIDI out: notes sit exactly where the hits play");
        check (order && offs[0] == ons[0] && offs[1] == ons[1], "MIDI out: every note gets its note-off, never overlapping on a lane");
        float vg = 0, va = 0;
        for (auto& [at, m] : h.midi) if (m.on && m.lane == 1) { if (at == 0) va = m.velocity; else vg = m.velocity; }
        check (va > 0.95f && vg > 0.3f && vg < 0.45f, "MIDI out: velocity follows accents and ghosts");
    }
    {
        Host h; h.fill (0); h.e.lanes[0].swing = 1.0f; h.note (0, 0, true); h.run (4 * step);
        bool swung = false; for (auto& [at, m] : h.midi) if (m.on && at == step + step / 2) swung = true;
        Host x; x.fill (0); x.e.midiOutEnabled = false; x.note (0, 0, true); x.run (4 * step);
        check (swung && x.midi.empty() && x.e.hitCount[0] > 0, "MIDI out: swing included; switched off = no MIDI, sound still plays");
    }

    std::printf ("\n  -- stage 6: chance, repeats, MIDI learn --\n");
    {
        Host h; h.fill (0); h.note (0, 0, true); h.run (64 * step);
        check (h.e.hitCount[0] == 64, "Chance 100% (default): every step plays");
        auto rate = [&] (int c)
        {
            Host x; x.fill (0); for (auto& v : x.e.lanes[0].chance) v.store (c);
            x.note (0, 0, true); x.run (1600 * step);
            return (double) x.e.hitCount[0] / 1600.0;
        };
        double r75 = rate (1), r50 = rate (2), r25 = rate (3);
        char b[120]; std::snprintf (b, sizeof b, "Chance 75/50/25%%: played %.0f%% / %.0f%% / %.0f%% of the time", r75 * 100, r50 * 100, r25 * 100);
        check (std::fabs (r75 - 0.75) < 0.05 && std::fabs (r50 - 0.5) < 0.05 && std::fabs (r25 - 0.25) < 0.05, b);
        Host m; m.fill (0); for (auto& v : m.e.lanes[0].chance) v.store (2); m.note (0, 0, true); m.run (64 * step);
        int ons = 0; for (auto& [at, ev] : m.midi) if (ev.on) ++ons;
        check (ons == (int) m.e.hitCount[0], "Chance: a skipped step sends no MIDI either");
    }
    {
        Host h; h.e.lanes[0].pattern[0].store (3); h.e.lanes[0].ratchet[0].store (3);
        h.note (0, 0, true); h.run (2 * step);
        auto& x = h.hits (0); auto& v = h.e.velLog[0];
        bool ok = x.size() == 3 && x[0].first == 0 && x[1].first == step / 3 && x[2].first == 2 * step / 3;
        check (ok, "Repeats x3: three hits evenly inside the step");
        check (v.size() == 3 && v[1] < v[0] && v[2] < v[1], "Repeats: each repeat a little softer, like a roll");
        Host four; four.fill (0); for (auto& r : four.e.lanes[0].ratchet) r.store (4); four.note (0, 0, true); four.run (16 * step);
        bool tidy = true; int open = 0;
        for (auto& [at, ev] : four.midi) { open += ev.on ? 1 : -1; if (open < 0 || open > 1) tidy = false; }
        check (four.e.hitCount[0] == 64 && tidy, "Repeats x4 on every step: 64 hits in a bar, MIDI notes never overlap");
    }
    {
        Host h; h.e.lanes[0].pattern[1].store (3); h.e.lanes[0].ratchet[1].store (2); h.e.lanes[0].swing = 1.0f;
        h.note (0, 0, true); h.run (3 * step);
        auto& x = h.hits (0);
        check (x.size() == 2 && x[0].first == step + step / 2 && x[1].first == step + step / 2 + step / 2, "Repeats follow swing: the roll starts where the swung hit lands");
        Host r; r.e.lanes[0].pattern[0].store (3); r.e.lanes[0].ratchet[0].store (4);
        r.note (0, 0, true); r.note (2000, 0, true); r.run (step);
        bool cut = true; for (auto& p : r.hits (0)) if (p.first == 3000 || p.first == 4500) cut = false;
        check (cut, "a restart cuts a roll short");
    }
    {
        Host h; h.fill (0); h.fill (1);
        h.e.lanes[0].inNote = 60; h.e.lanes[1].inNote = 60;
        h.note (0, 0, true);                       // C1 no longer launches lane 1
        h.pending.push_back ({ step * 4, { 0, 60, true } });
        h.run (8 * step);
        check (h.hits (0).size() == 4 && h.hits (0)[0].first == step * 4 && h.hits (1).size() == 4,
               "MIDI learn: lanes follow their learned note; two lanes can share one note");
    }

    std::printf ("\n  -- filter types --\n");
    {
        // energy of the lo-fi thump (low) vs its top: high-pass should remove the low part
        auto bands = [] (bool hp, float cut)
        {
            Host h; h.keep = true; h.e.lanes[0].pattern[0].store (3); h.e.lanes[0].voice.setParams (lofi, 0.0f, 1.0f, 0.6f, -12.0f);
            h.e.lanes[0].highpass = hp; h.e.lanes[0].cutoff = cut; h.note (0, 0, true); h.run (12000);
            auto goertzel = [&] (double f) { double w = 2 * 3.14159265358979 * f / 48000, c = 2 * std::cos (w), s1 = 0, s2 = 0;
                                             for (float v : h.outL) { double s0 = v + c * s1 - s2; s2 = s1; s1 = s0; } return s1 * s1 + s2 * s2 - c * s1 * s2; };
            return std::make_pair (goertzel (90), goertzel (3000));
        };
        auto open = bands (true, 0.0f), cut = bands (true, 0.45f), lp = bands (false, 0.35f);
        check (cut.first < open.first * 0.05 && cut.second > open.second * 0.3, "High-pass: removes the low thump, keeps the top");
        check (lp.second < open.second * 0.2 && lp.first > open.first * 0.3, "Low-pass: removes the top, keeps the low thump");
        Host a; a.fill (0); a.keep = true; a.note (0, 0, true); a.run (24000);
        Host b; b.fill (0); b.keep = true; b.e.lanes[0].highpass = true; b.e.lanes[0].cutoff = 0.0f; b.note (0, 0, true); b.run (24000);
        check (a.outL == b.outL, "High-pass fully left = filter off, sound untouched");
        Host r; r.fill (0); r.e.lanes[0].highpass = true; r.e.lanes[0].cutoff = 0.6f; r.e.lanes[0].res = 1.0f; r.note (0, 0, true); r.run (48000);
        r.e.lanes[0].highpass = false; r.run (48000);
        check (! r.bad && r.peak < 1.2f, "High-pass at max resonance, and switching type while playing: safe");
    }

    std::printf ("\n%d passed, %d failed\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
