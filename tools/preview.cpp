// Renders an audio preview of RASPA (16-bit stereo WAV). Not part of the plugin.
// Stage 6 tour, 124 BPM: two bars each of eight of the new factory presets
// (they use per-step Chance and Repeats; all lanes latched).
#include "engine.h"
#include "defaults.h"
#include "generator.h"
#include "presets.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <sstream>
using namespace raspa;

struct LaneCfg { int voice, genre, dtime; bool hp; float a, b, pitch, decay, level, density, variation, swing, accent, humanize, pan, cutoff, res, send[4]; };

static void applyPreset (Engine& e, int index)
{
    LaneCfg c[4];
    for (int l = 0; l < 4; ++l)
    {
        const auto& d = laneDefaults[l]; const auto& m = laneMix[l];
        c[l] = { d.voice, d.genre, m.delayTime, false, d.a, d.b, 0.0f, d.decay, d.level, d.density, d.variation, d.swing, d.accent, d.humanize,
                 m.pan, 1.0f, 0.0f, { m.send[0], m.send[1], m.send[2], m.send[3] } };
    }
    e.fx = FxSettings();
    std::istringstream in (factoryPresets[index].settings);
    std::string item;
    std::vector<std::string> stepItems;
    for (auto& ln : e.lanes) for (int k = 0; k < 16; ++k) { ln.chance[k].store (0); ln.ratchet[k].store (1); }
    while (in >> item)
    {
        auto eq = item.find ('='); std::string key = item.substr (0, eq);
        if (key.rfind ("ch_", 0) == 0 || key.rfind ("rt_", 0) == 0) { stepItems.push_back (item); continue; }
        float v = std::stof (item.substr (eq + 1));
        auto us = key.rfind ('_');
        int lane = (us != std::string::npos && isdigit (key[us + 1])) ? key[us + 1] - '1' : -1;
        std::string k = lane >= 0 ? key.substr (0, us) : key;
        if (lane >= 0)
        {
            auto& x = c[lane];
            if (k == "voice") x.voice = (int) v; else if (k == "genre") x.genre = (int) v; else if (k == "dtime") x.dtime = (int) v;
            else if (k == "grain") x.a = v; else if (k == "seed") x.b = v; else if (k == "pitch") x.pitch = v; else if (k == "decay") x.decay = v;
            else if (k == "level") x.level = v; else if (k == "density") x.density = v; else if (k == "variation") x.variation = v;
            else if (k == "swing") x.swing = v; else if (k == "accent") x.accent = v; else if (k == "humanize") x.humanize = v;
            else if (k == "hipass") x.hp = v > 0.5f; else if (k == "pan") x.pan = v; else if (k == "cutoff") x.cutoff = v; else if (k == "res") x.res = v;
            else if (k == "sendA") x.send[0] = v; else if (k == "sendB") x.send[1] = v; else if (k == "sendC") x.send[2] = v; else if (k == "sendD") x.send[3] = v;
        }
        else
        {
            auto& f = e.fx;
            if (k == "verb_size") f.verbSize = v; else if (k == "verb_decay") f.verbDecay = v; else if (k == "verb_damp") f.verbDamp = v; else if (k == "verb_size") f.verbSize = v;
            else if (k == "dly_feedback") f.dlyFeedback = v; else if (k == "dly_cutoff") f.dlyCutoff = v; else if (k == "dly_res") f.dlyRes = v;
            else if (k == "cho_depth") f.choDepth = v; else if (k == "rev_length") f.revLength = (int) v; else if (k == "rev_slice") f.revSlice = v;
        }
    }
    for (int l = 0; l < 4; ++l)
    {
        auto& L = e.lanes[l]; auto& x = c[l];
        L.voice.setParams (x.voice, x.a, x.b, x.decay, x.pitch);
        L.level = x.level; L.swing = x.swing; L.accent = x.accent; L.humanize = x.humanize;
        L.pan = x.pan; L.cutoff = x.cutoff; L.highpass = x.hp; L.res = x.res; L.delayTime = x.dtime;
        for (int k = 0; k < 4; ++k) L.send[k] = x.send[k];
        int p[16]; generatePattern (x.voice, x.genre, x.density, x.variation, 1, p);
        for (int s = 0; s < 16; ++s) L.pattern[s].store (p[s]);
        L.latch = true;
    }
    for (auto& it : stepItems)
    {
        int lane = it[3] - '1'; std::string d = it.substr (5);
        for (int k = 0; k < 16 && k < (int) d.size(); ++k)
        {
            if (it[0] == 'c') e.lanes[lane].chance[k].store (d[(size_t) k] - '0');
            else e.lanes[lane].ratchet[k].store (d[(size_t) k] - '0');
        }
    }
}

int main (int argc, char** argv)
{
    const double sr = 48000, bpm = 124;
    Engine e; e.prepare (sr);
    const int tour[8] = { 20, 24, 26, 30, 32, 34, 37, 39 };
    const double bar = 4 * 60.0 / bpm * sr;
    const long long total = (long long) (16.5 * bar);
    std::vector<float> outL ((size_t) total), outR ((size_t) total);
    double ppq = 0; const int block = 256;
    std::vector<float> L (block), R (block);
    int shown = -1;
    for (long long t = 0; t < total; t += block)
    {
        int len = (int) std::min<long long> (block, total - t);
        int sec = std::min ((int) (t / bar) / 2, 7);
        if (sec != shown) { shown = sec; applyPreset (e, tour[sec]); std::printf ("bar %2d: %s\n", sec * 2 + 1, factoryPresets[tour[sec]].name); }
        e.process (L.data(), R.data(), len, { bpm, t < (long long) (16 * bar), ppq }, {});
        std::memcpy (&outL[(size_t) t], L.data(), (size_t) len * sizeof (float));
        std::memcpy (&outR[(size_t) t], R.data(), (size_t) len * sizeof (float));
        ppq += len * bpm / 60.0 / sr;
    }
    float peak = 0; for (size_t i = 0; i < outL.size(); ++i) peak = std::max ({ peak, std::fabs (outL[i]), std::fabs (outR[i]) });
    std::printf ("peak %.3f\n", peak);
    FILE* f = std::fopen (argc > 1 ? argv[1] : "preview.wav", "wb");
    auto w32 = [&] (uint32_t v) { std::fwrite (&v, 4, 1, f); };
    auto w16 = [&] (uint16_t v) { std::fwrite (&v, 2, 1, f); };
    uint32_t dataBytes = (uint32_t) total * 4;
    std::fwrite ("RIFF", 1, 4, f); w32 (36 + dataBytes); std::fwrite ("WAVEfmt ", 1, 8, f);
    w32 (16); w16 (1); w16 (2); w32 ((uint32_t) sr); w32 ((uint32_t) sr * 4); w16 (4); w16 (16);
    std::fwrite ("data", 1, 4, f); w32 (dataBytes);
    for (size_t i = 0; i < outL.size(); ++i)
        for (float v : { outL[i], outR[i] })
        { int16_t s = (int16_t) std::lround (std::clamp (v * 0.9f, -1.0f, 1.0f) * 32767); std::fwrite (&s, 2, 1, f); }
    std::fclose (f);
}
