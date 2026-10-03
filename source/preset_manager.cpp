// preset_manager.cpp — ÁCIDO presets (phase 5)
#include "preset_manager.h"
#include "plugin_processor.h"

namespace
{
    struct LaneSpec
    {
        const char* id;
        std::vector<float> steps;   // up to 16 values, -1..1 (or empty with a fill shape)
        int length = 16;
        int rate = 1;               // 0 = 1/32, 1 = 1/16, 2 = 1/8, 3 = 1/4
        bool smooth = false;
        float depth = 0.5f;
        int fill = -1;              // acido::Lane::Shape, or -1 to use `steps`
    };

    struct Factory
    {
        const char* name;                                   // UTF-8
        std::vector<std::pair<const char*, float>> values;  // real units; choices by index
        std::vector<LaneSpec> lanes;
    };

    // Values not listed keep their defaults (the Squelch Init sound).
    const std::vector<Factory>& factory()
    {
        static const std::vector<Factory> presets {
            { "Squelch Init", {}, {} },

            { "Rubber Acid",
              { { "wave", 1 }, { "shape", 50 }, { "cutoff", 260 }, { "reso", 90 }, { "envmod", 75 }, { "decay", 220 },
                { "accent", 90 }, { "slide", 80 }, { "drive", 45 }, { "warmth", 30 }, { "dmix", 15 }, { "vol", -4 } }, {} },

            { "Night Bus Line",
              { { "cutoff", 320 }, { "reso", 72 }, { "envmod", 55 }, { "decay", 420 }, { "drift", 35 },
                { "dmix", 35 }, { "ddiv", 6 }, { "dfb", 55 }, { "rtype", 1 }, { "rmix", 28 }, { "rsize", 70 } }, {} },

            { "Round Sub Pluck",
              { { "wave", 2 }, { "shape", 12 }, { "sub", 80 }, { "suboct", 1 }, { "cutoff", 900 }, { "reso", 25 },
                { "envmod", 35 }, { "decay", 160 }, { "drive", 20 }, { "warmth", 70 }, { "dmix", 0 }, { "rmix", 8 } }, {} },

            { "Chirrido",
              { { "wave", 3 }, { "shape", 45 }, { "cutoff", 500 }, { "reso", 85 }, { "envmod", 70 }, { "decay", 250 },
                { "ffm", 30 }, { "accent", 85 }, { "crush", 12 }, { "dmix", 20 }, { "ddiv", 3 }, { "vol", -5 } }, {} },

            { "Neblina Pad",
              { { "mode", 1 }, { "voices", 6 }, { "shape", 60 }, { "unison", 5 }, { "neblina", 60 }, { "cutoff", 900 },
                { "reso", 30 }, { "envmod", 25 }, { "decay", 1500 }, { "accent", 20 }, { "slide", 40 }, { "velo", 70 },
                { "sub", 20 }, { "drive", 10 }, { "cmix", 30 }, { "dmix", 20 }, { "rtype", 1 }, { "rmix", 45 }, { "rsize", 80 } }, {} },

            { "Acid Stabs",
              { { "mode", 1 }, { "voices", 4 }, { "wave", 1 }, { "shape", 45 }, { "unison", 2 }, { "cutoff", 650 },
                { "reso", 60 }, { "envmod", 55 }, { "decay", 280 }, { "slide", 15 }, { "dmix", 30 }, { "rtype", 1 }, { "rmix", 25 },
                { "vol", -6 } }, {} },

            { "Squelch Motion",
              { { "cutoff", 300 }, { "reso", 88 } },
              { { "cutoff", {}, 16, 1, false, 0.45f, (int) acido::Lane::Saw },
                { "envmod", { 1, 0, 0.5f, 0, 1, 0, 0, 0.5f, 1, 0, 0.5f, 0 }, 12, 1, false, 0.3f } } },

            { "Tape Burn",
              { { "drive", 80 }, { "warmth", 90 }, { "cutoff", 450 }, { "reso", 70 }, { "noise", 25 }, { "comp", 50 },
                { "sub", 50 }, { "vol", -4 } }, {} },

            { "Lo-Fi Crush",
              { { "wave", 1 }, { "shape", 35 }, { "cutoff", 600 }, { "reso", 65 }, { "crush", 45 }, { "rtype", 3 },
                { "rmix", 30 }, { "dmix", 25 }, { "vol", -5 } }, {} },

            { "Wobble Bass",
              { { "sub", 65 }, { "cutoff", 220 }, { "reso", 70 }, { "envmod", 20 }, { "decay", 900 }, { "drive", 40 } },
              { { "cutoff", {}, 8, 2, true, 0.55f, (int) acido::Lane::Sine } } },

            { "Cumbia \xc3\x81" "cida",
              { { "wave", 1 }, { "shape", 50 }, { "cutoff", 380 }, { "reso", 78 }, { "envmod", 60 }, { "decay", 300 },
                { "slide", 120 }, { "drift", 40 }, { "dmix", 25 }, { "ddiv", 5 }, { "dfb", 35 }, { "rtype", 3 }, { "rmix", 18 },
                { "vol", -4 } },
              { { "envmod", { 1, 0, 0, 1, 0, 0, 1, 0, 1, 0, 0, 1, 0, 0, 1, 0 }, 16, 1, false, 0.35f } } },
        };
        return presets;
    }
}

int PresetManager::numFactoryPresets()          { return (int) factory().size(); }
juce::String PresetManager::factoryName (int i) { return juce::String::fromUTF8 (factory()[(size_t) i].name); }

juce::File PresetManager::userFolder()
{
    return juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("ACIDO").getChildFile ("Presets");
}

void PresetManager::refresh()
{
    entries.clear();
    for (int i = 0; i < numFactoryPresets(); ++i)
        entries.push_back ({ factoryName (i), true, i, {} });

    auto files = userFolder().findChildFiles (juce::File::findFiles, false, "*.acidopreset");
    std::sort (files.begin(), files.end(), [] (const juce::File& a, const juce::File& b)
               { return a.getFileNameWithoutExtension().compareIgnoreCase (b.getFileNameWithoutExtension()) < 0; });
    for (auto& f : files)
        entries.push_back ({ f.getFileNameWithoutExtension(), false, -1, f });

    // Keep pointing at the same preset if it still exists.
    current = 0;
    for (int i = 0; i < (int) entries.size(); ++i)
        if (entries[(size_t) i].name == currentName && entries[(size_t) i].factory == currentFactory) current = i;
}

bool PresetManager::load (int index)
{
    if (index < 0 || index >= (int) entries.size()) return false;
    const auto& e = entries[(size_t) index];

    if (e.factory)
        applyFactory (e.factoryIndex);
    else
    {
        auto xml = juce::XmlDocument::parse (e.file);
        if (xml == nullptr || ! proc.applyState (juce::ValueTree::fromXml (*xml))) return false;
    }

    current = index;
    currentName = e.name;
    currentFactory = e.factory;
    markClean();
    return true;
}

void PresetManager::applyFactory (int i)
{
    const auto& f = factory()[(size_t) juce::jlimit (0, numFactoryPresets() - 1, i)];

    for (auto* p : proc.getParameters())                      // start from the defaults
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (p))
            r->setValueNotifyingHost (r->getDefaultValue());

    for (const auto& [id, value] : f.values)
        if (auto* r = proc.apvts.getParameter (id))
            r->setValueNotifyingHost (r->convertTo0to1 (value));

    proc.resetLanes();
    for (const auto& spec : f.lanes)
    {
        const int li = proc.laneIndexFor (spec.id);
        if (li < 0) continue;
        auto& lane = *proc.lanes[(size_t) li];
        lane.length = juce::jlimit (1, acido::kSteps, spec.length);
        lane.rate   = spec.rate;
        lane.smooth = spec.smooth;
        lane.depth  = spec.depth;
        if (spec.fill >= 0) lane.fill ((acido::Lane::Shape) spec.fill);
        else for (size_t s = 0; s < spec.steps.size() && s < (size_t) acido::kSteps; ++s) lane.steps[s] = spec.steps[s];
    }
}

juce::String PresetManager::safeFileName (const juce::String& name)
{
    auto s = name.trim().removeCharacters ("/\\:*?\"<>|");
    return s.isEmpty() ? juce::String ("Untitled") : s.substring (0, 60);
}

bool PresetManager::saveAsNew (const juce::String& wanted)
{
    auto folder = userFolder();
    if (! folder.createDirectory()) return false;

    // Never replace an existing preset by accident: add " 2", " 3" ...
    const auto base = safeFileName (wanted);
    auto name = base;
    for (int n = 2; folder.getChildFile (name + ".acidopreset").exists(); ++n) name = base + " " + juce::String (n);

    currentName = name;
    currentFactory = false;
    auto state = proc.makeState();
    auto xml = state.createXml();
    if (xml == nullptr || ! xml->writeTo (folder.getChildFile (name + ".acidopreset"))) return false;

    refresh();
    markClean();
    return true;
}

bool PresetManager::overwrite()
{
    if (currentFactory || current < 0 || current >= (int) entries.size()) return false;
    const auto& e = entries[(size_t) current];
    auto xml = proc.makeState().createXml();
    if (xml == nullptr || ! xml->writeTo (e.file)) return false;
    markClean();
    return true;
}

void PresetManager::restoreName (const juce::String& name, bool factory)
{
    if (name.isEmpty()) return;
    currentName = name;
    currentFactory = factory;
    refresh();
    markClean();
}

// A number that changes whenever any knob or lane changes.
juce::int64 PresetManager::fingerprint() const
{
    juce::int64 h = 1469598103934665603LL;
    auto mix = [&h] (float v) { h = (h ^ (juce::int64) std::lround (v * 10000.0f)) * 1099511628211LL; };
    for (auto* p : proc.getParameters()) mix (p->getValue());
    for (const auto& lane : proc.lanes)
    {
        for (const auto& s : lane->steps) mix (s.load());
        mix ((float) lane->length.load()); mix ((float) lane->rate.load());
        mix (lane->smooth.load() ? 1.0f : 0.0f); mix (lane->depth.load());
    }
    return h;
}

void PresetManager::markClean()     { cleanPrint = fingerprint(); }
bool PresetManager::isModified() const { return fingerprint() != cleanPrint; }
