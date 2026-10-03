// plugin_processor.cpp — ÁCIDO plugin shell (phase 6)
#include "plugin_processor.h"
#include "plugin_editor.h"


namespace
{
    // A range that spends more of the knob's travel on the low end (for Hz / ms).
    juce::NormalisableRange<float> logRange (float lo, float hi, float centre)
    {
        juce::NormalisableRange<float> r (lo, hi);
        r.setSkewForCentre (centre);
        return r;
    }

    // Readable values: "420 Hz", "82%", "60 ms", "-3 dB", "0.5 st".
    juce::AudioParameterFloatAttributes unit (const juce::String& label)
    {
        return juce::AudioParameterFloatAttributes()
            .withLabel (label)
            .withStringFromValueFunction ([label] (float v, int)
            {
                if (label == "%" || std::abs (v) >= 100.0f) return juce::String (juce::roundToInt (v));
                const auto oneDecimal = juce::String (v, 1);
                return oneDecimal.endsWith (".0") ? oneDecimal.dropLastCharacters (2) : oneDecimal;
            });
    }
}

// Parameter order matters: Ableton and Push 1 show them in this order, so the
// first eight are the core acid controls (spec: Behaviour rules > General).
juce::AudioProcessorValueTreeState::ParameterLayout AcidoProcessor::createLayout()
{
    using P = juce::AudioParameterFloat;
    using C = juce::AudioParameterChoice;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> ps;

    auto pct = [&ps] (const char* id, const char* name, float def)
    {
        ps.push_back (std::make_unique<P> (juce::ParameterID { id, 1 }, name,
                                           juce::NormalisableRange<float> (0.0f, 100.0f), def, unit ("%")));
    };

    // Bank 1 — core acid controls
    ps.push_back (std::make_unique<P> (juce::ParameterID { "cutoff", 1 }, "Cutoff",
                                       logRange (20.0f, 18000.0f, 800.0f), 420.0f, unit ("Hz")));
    pct ("reso",   "Resonance", 82.0f);
    pct ("envmod", "Env Mod",   60.0f);
    ps.push_back (std::make_unique<P> (juce::ParameterID { "decay", 1 }, "Decay",
                                       logRange (30.0f, 3000.0f, 300.0f), 340.0f, unit ("ms")));
    pct ("accent", "Accent",    70.0f);
    ps.push_back (std::make_unique<P> (juce::ParameterID { "slide", 1 }, "Slide",
                                       logRange (10.0f, 1000.0f, 100.0f), 60.0f, unit ("ms")));
    pct ("drive",  "Drive",     30.0f);
    ps.push_back (std::make_unique<P> (juce::ParameterID { "vol", 1 }, "Volume",
                                       juce::NormalisableRange<float> (-60.0f, 6.0f), -3.0f, unit ("dB")));

    // Bank 2 — oscillator
    ps.push_back (std::make_unique<C> (juce::ParameterID { "wave", 1 }, "Waveform",
                                       juce::StringArray { "Saw", "Square", "Sine", "FM" }, 0));
    ps.push_back (std::make_unique<P> (juce::ParameterID { "tune", 1 }, "Tune",
                                       juce::NormalisableRange<float> (-24.0f, 24.0f), 0.0f, unit ("st")));
    pct ("shape",  "Shape",     20.0f);
    pct ("sub",    "Sub",       40.0f);
    ps.push_back (std::make_unique<C> (juce::ParameterID { "suboct", 1 }, "Sub Octave",
                                       juce::StringArray { "-1 oct", "-2 oct" }, 0));
    pct ("ffm",    "Filter FM",  0.0f);
    pct ("noise",  "Noise",      0.0f);
    pct ("warmth", "Warmth",    45.0f);

    // Bank 3 — feel, chorus and delay
    pct ("drift",  "Drift",     20.0f);
    pct ("velo",   "Velo",      50.0f);
    pct ("cmix",   "Chorus Dry/Wet", 0.0f);
    pct ("ctone",  "Chorus Tone",   60.0f);
    pct ("dmix",   "Delay Dry/Wet", 20.0f);
    ps.push_back (std::make_unique<P> (juce::ParameterID { "dfb", 1 }, "Delay Feedback",
                                       juce::NormalisableRange<float> (0.0f, 95.0f), 45.0f, unit ("%")));
    ps.push_back (std::make_unique<C> (juce::ParameterID { "ddiv", 1 }, "Delay Time (Sync)",
                                       juce::StringArray { "1/32", "1/16 T", "1/16", "1/16 D", "1/8 T", "1/8",
                                                           "1/8 D", "1/4 T", "1/4", "1/4 D", "1/2", "1 bar" }, 6));
    ps.push_back (std::make_unique<C> (juce::ParameterID { "dsync", 1 }, "Delay Sync / MS",
                                       juce::StringArray { "Sync", "MS" }, 0));

    // Bank 4 — reverb and output
    pct ("rmix",   "Reverb Send",    15.0f);
    pct ("rsize",  "Reverb Size",    62.0f);
    ps.push_back (std::make_unique<C> (juce::ParameterID { "rtype", 1 }, "Reverb Type",
                                       juce::StringArray { "Room", "Hall", "Plate", "Spring" }, 2));
    pct ("crush",  "Crush",           0.0f);
    pct ("comp",   "Comp",           35.0f);
    ps.push_back (std::make_unique<P> (juce::ParameterID { "dms", 1 }, "Delay Time (MS)",
                                       logRange (1.0f, 2000.0f, 250.0f), 375.0f, unit ("ms")));
    pct ("neblina", "Neblina",        0.0f);
    ps.push_back (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { "unison", 1 }, "Unison", 1, 7, 1));

    // Bank 5 — voices
    ps.push_back (std::make_unique<C> (juce::ParameterID { "mode", 1 }, "Mono / Poly",
                                       juce::StringArray { "Mono", "Poly" }, 0));
    ps.push_back (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { "voices", 1 }, "Poly Voices", 2, 8, 6));

    return { ps.begin(), ps.end() };
}

AcidoProcessor::AcidoProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "ACIDO", createLayout())
{
    for (auto* id : { "cutoff", "reso", "envmod", "decay", "accent", "slide", "drive", "vol",
                      "wave", "tune", "shape", "sub", "suboct", "ffm", "noise", "warmth", "drift", "velo",
                      "cmix", "ctone", "dmix", "dfb", "ddiv", "dsync", "rmix", "rsize", "rtype", "crush", "comp", "dms",
                      "neblina", "unison", "mode", "voices" })
        params[id] = apvts.getRawParameterValue (id);

    for (auto* p : getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (p))
            ranged[r->getParameterID().toStdString()] = r;

    for (const auto& id : modTargets())
    {
        laneOf[id] = (int) lanes.size();
        lanes.push_back (std::make_unique<acido::Lane>());
    }

    presets.refresh();
    presets.load (0);   // Squelch Init
}

// Knobs that can take a step lane (spec: Parameters table, "Step mod: Yes").
const std::vector<std::string>& AcidoProcessor::modTargets()
{
    static const std::vector<std::string> ids {
        "cutoff", "reso", "envmod", "decay", "accent", "slide", "drive", "vol",
        "tune", "shape", "sub", "ffm", "noise", "warmth", "drift", "velo",
        "cmix", "ctone", "dmix", "dfb", "rmix", "rsize", "crush", "comp", "neblina" };
    return ids;
}

int AcidoProcessor::laneIndexFor (const std::string& id) const
{
    const auto it = laneOf.find (id);
    return it == laneOf.end() ? -1 : it->second;
}

float AcidoProcessor::value (const char* id)
{
    const float raw = params.at (id)->load();
    const auto lane = laneOf.find (id);
    if (lane == laneOf.end()) return raw;

    const float offset = modulator.offset ((size_t) lane->second);
    if (std::abs (offset) < 1e-7f) return raw;

    auto* p = ranged.at (id);
    return p->convertFrom0to1 (acido::applyOffset (p->convertTo0to1 (raw), offset));
}

juce::AudioProcessorEditor* AcidoProcessor::createEditor() { return new AcidoEditor (*this); }

bool AcidoProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void AcidoProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    engine.prepare (sampleRate);
    modulator.prepare (sampleRate, lanes.size());
    left.assign  ((size_t) juce::jmax (1, samplesPerBlock), 0.0f);
    right.assign ((size_t) juce::jmax (1, samplesPerBlock), 0.0f);
}

acido::SoundParams AcidoProcessor::readParams()
{
    acido::SoundParams p;
    auto& v = p.voice;
    v.cutoffHz = value ("cutoff");
    v.reso     = value ("reso")   * 0.01f;
    v.envMod   = value ("envmod") * 0.01f;
    v.decayMs  = value ("decay");
    v.accent   = value ("accent") * 0.01f;
    v.slideMs  = value ("slide");
    v.wave     = (int) value ("wave");
    v.tune     = value ("tune");
    v.shape    = value ("shape")  * 0.01f;
    v.subLevel = value ("sub")    * 0.01f;
    v.subOct   = (int) value ("suboct") + 1;
    v.filterFm = value ("ffm")    * 0.01f;
    v.noise    = value ("noise")  * 0.01f;
    v.drift    = value ("drift")  * 0.01f;
    v.velo     = value ("velo")   * 0.01f;
    p.drive    = value ("drive")  * 0.01f;
    p.warmth   = value ("warmth") * 0.01f;
    p.volumeDb = value ("vol");
    p.chorusMix  = value ("cmix")  * 0.01f;
    p.chorusTone = value ("ctone") * 0.01f;
    p.delayMix   = value ("dmix")  * 0.01f;
    p.delayFb    = value ("dfb")   * 0.01f;
    p.delayDiv   = (int) value ("ddiv");
    p.delaySync  = (int) value ("dsync") == 0;
    p.delayMs    = value ("dms");
    p.reverbMix  = value ("rmix")  * 0.01f;
    p.reverbSize = value ("rsize") * 0.01f;
    p.reverbType = (int) value ("rtype");
    p.crush      = value ("crush") * 0.01f;
    p.comp       = value ("comp")  * 0.01f;
    v.neblina    = value ("neblina") * 0.01f;
    v.unison     = (int) value ("unison");
    p.poly       = (int) value ("mode") == 1;
    p.voices     = (int) value ("voices");

    return p;
}

void AcidoProcessor::handleMidi (const juce::MidiMessage& m)
{
    if (m.isNoteOn())                                   engine.noteOn (m.getNoteNumber(), m.getVelocity());
    else if (m.isNoteOff())                             engine.noteOff (m.getNoteNumber());
    else if (m.isAllNotesOff() || m.isAllSoundOff())    engine.allNotesOff();
}

void AcidoProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    if ((int) left.size() < numSamples)   // safety for hosts that send bigger blocks
    {
        left.resize ((size_t) numSamples);
        right.resize ((size_t) numSamples);
    }

    // Ableton's tempo, play state and song position.
    double bpm = 120.0, ppqStart = 0.0;
    bool playing = false;
    if (auto* host = getPlayHead())
        if (auto position = host->getPosition())
        {
            if (auto b = position->getBpm()) if (*b > 1.0) bpm = *b;
            if (auto q = position->getPpqPosition()) ppqStart = *q;
            playing = position->getIsPlaying();
        }
    const double beatsPerSample = bpm / 60.0 / currentSampleRate;
    hostBpm = bpm;
    hostPlaying = playing;

    // Apply Mono/Poly before this block's notes arrive, so no note goes to the wrong mode.
    engine.setMode ((int) params.at ("mode")->load() == 1, (int) params.at ("voices")->load());

    // Render in small chunks (at most 32 samples) so step lanes stay tight,
    // splitting at MIDI events so every note starts on its exact sample.
    constexpr int kChunk = 32;
    auto renderSpan = [&] (int from, int to)
    {
        for (int pos = from; pos < to;)
        {
            const int len = juce::jmin (kChunk, to - pos);
            modulator.process (lanes, playing, ppqStart + pos * beatsPerSample, len);
            auto p = readParams();
            p.bpm = bpm;
            engine.render (left.data() + pos, right.data() + pos, len, p);
            pos += len;
        }
    };

    int pos = 0;
    for (const auto meta : midi)
    {
        const int eventPos = juce::jlimit (0, numSamples, meta.samplePosition);
        if (eventPos > pos) { renderSpan (pos, eventPos); pos = eventPos; }
        handleMidi (meta.getMessage());
    }
    if (pos < numSamples) renderSpan (pos, numSamples);

    if (buffer.getNumChannels() >= 2)
    {
        buffer.copyFrom (0, 0, left.data(), numSamples);
        buffer.copyFrom (1, 0, right.data(), numSamples);
        for (int ch = 2; ch < buffer.getNumChannels(); ++ch) buffer.clear (ch, 0, numSamples);
    }
    else if (buffer.getNumChannels() == 1)
    {
        buffer.copyFrom (0, 0, left.data(), numSamples, 0.5f);
        buffer.addFrom  (0, 0, right.data(), numSamples, 0.5f);
    }
}

// Lanes are saved next to the knob values, inside the Live Set.
void AcidoProcessor::saveLanes (juce::ValueTree& state) const
{
    juce::ValueTree all ("LANES");
    const auto& ids = modTargets();
    for (size_t i = 0; i < lanes.size(); ++i)
    {
        const auto& lane = *lanes[i];
        juce::StringArray values;
        for (auto& v : lane.steps) values.add (juce::String (v.load(), 4));
        juce::ValueTree t ("LANE");
        t.setProperty ("id", juce::String (ids[i]), nullptr);
        t.setProperty ("steps", values.joinIntoString (","), nullptr);
        t.setProperty ("length", lane.length.load(), nullptr);
        t.setProperty ("rate", lane.rate.load(), nullptr);
        t.setProperty ("smooth", lane.smooth.load(), nullptr);
        t.setProperty ("depth", lane.depth.load(), nullptr);
        all.appendChild (t, nullptr);
    }
    state.appendChild (all, nullptr);
}

void AcidoProcessor::loadLanes (const juce::ValueTree& state)
{
    for (auto& lane : lanes) { lane->clear(); lane->length = acido::kSteps; lane->rate = 1; lane->smooth = false; lane->depth = 0.5f; }

    const auto all = state.getChildWithName ("LANES");
    for (const auto& t : all)
    {
        const int i = laneIndexFor (t.getProperty ("id").toString().toStdString());
        if (i < 0) continue;
        auto& lane = *lanes[(size_t) i];
        const auto values = juce::StringArray::fromTokens (t.getProperty ("steps").toString(), ",", "");
        for (int s = 0; s < acido::kSteps && s < values.size(); ++s)
            lane.steps[(size_t) s] = juce::jlimit (-1.0f, 1.0f, values[s].getFloatValue());
        lane.length = juce::jlimit (1, acido::kSteps, (int) t.getProperty ("length", acido::kSteps));
        lane.rate   = juce::jlimit (0, 3, (int) t.getProperty ("rate", 1));
        lane.smooth = (bool) t.getProperty ("smooth", false);
        lane.depth  = juce::jlimit (0.0f, 1.0f, (float) t.getProperty ("depth", 0.5f));
    }
}

// The full sound as one tree: every knob, the lanes and the preset name.
// Used for the Live Set and for preset files alike.
juce::ValueTree AcidoProcessor::makeState()
{
    auto state = apvts.copyState();
    state.removeChild (state.getChildWithName ("LANES"), nullptr);
    saveLanes (state);
    state.setProperty ("presetName", presets.getCurrentName(), nullptr);
    state.setProperty ("presetIsFactory", presets.currentIsFactory(), nullptr);
    return state;
}

bool AcidoProcessor::applyState (juce::ValueTree state)
{
    if (! state.hasType (apvts.state.getType())) return false;
    loadLanes (state);
    state.removeChild (state.getChildWithName ("LANES"), nullptr);
    apvts.replaceState (state);
    return true;
}

void AcidoProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = makeState();
    state.setProperty ("uiScale", uiScale, nullptr);    // window size lives in the Live Set, not in presets
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void AcidoProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        auto state = juce::ValueTree::fromXml (*xml);
        const auto name = state.getProperty ("presetName").toString();
        const bool factory = (bool) state.getProperty ("presetIsFactory", false);
        uiScale = juce::jlimit (0.6f, 1.25f, (float) state.getProperty ("uiScale", 0.75f));
        if (applyState (state))
            presets.restoreName (name, factory);   // shows the preset name again, unmodified
    }
}

// This creates the plugin when Ableton loads it.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AcidoProcessor();
}
