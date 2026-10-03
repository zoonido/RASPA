#include "plugin_processor.h"
#include "plugin_editor.h"
#include "defaults.h"
#include "generator.h"
#include "presets.h"

using namespace raspa;

RaspaProcessor::RaspaProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "RASPA", createLayout())
{
    for (int i = 0; i < numLanes; ++i)
    {
        auto raw = [&] (const char* n) { return apvts.getRawParameterValue (pid (n, i)); };
        lp[i] = { raw ("voice"), raw ("latch"), raw ("grain"), raw ("seed"), raw ("decay"), raw ("level"), raw ("pitch"),
                  raw ("genre"), raw ("density"), raw ("variation"), raw ("swing"), raw ("accent"), raw ("humanize"), raw ("lock"),
                  raw ("pan"), raw ("hipass"), raw ("cutoff"), raw ("res"), { raw ("sendA"), raw ("sendB"), raw ("sendC"), raw ("sendD") }, raw ("dtime") };
        seed[i].store (1);
        regenerate (i);
        for (auto* n : { "voice", "genre", "density", "variation" })
            apvts.addParameterListener (pid (n, i), this);
    }
    static const char* globals[18] = { "verb_size", "verb_decay", "verb_damp", "verb_pre", "verb_return",
                                       "dly_feedback", "dly_cutoff", "dly_res", "dly_return",
                                       "cho_rate", "cho_depth", "cho_width", "cho_return",
                                       "rev_length", "rev_slice", "rev_fade", "rev_return", "master" };
    for (int k = 0; k < 18; ++k) g[k] = apvts.getRawParameterValue (globals[k]);
    for (int i = 0; i < numLanes; ++i) outNote[i] = apvts.getRawParameterValue (pid ("outnote", i));
    for (int i = 0; i < numLanes; ++i) inNote[i] = apvts.getRawParameterValue (pid ("innote", i));
    midiOutOn = apvts.getRawParameterValue ("midiout_on");
    midiOutCh = apvts.getRawParameterValue ("midiout_ch");
    events.reserve (1024);
}

RaspaProcessor::~RaspaProcessor()
{
    for (int i = 0; i < numLanes; ++i)
        for (auto* n : { "voice", "genre", "density", "variation" })
            apvts.removeParameterListener (pid (n, i), this);
}

void RaspaProcessor::parameterChanged (const juce::String& id, float)
{
    if (loadingState.load()) return;          // a loaded set keeps its saved patterns
    int lane = id.getTrailingIntValue() - 1;
    if (lane >= 0 && lane < numLanes) regenerate (lane);
}

void RaspaProcessor::regenerate (int lane)
{
    if (isLocked (lane)) return;
    int p[numSteps];
    raspa::generatePattern ((int) lp[lane].voice->load(), (int) lp[lane].genre->load(),
                            lp[lane].density->load(), lp[lane].variation->load(), seed[lane].load(), p);
    for (int s = 0; s < numSteps; ++s) engine.lanes[lane].pattern[s].store (p[s]);
}

void RaspaProcessor::roll (int lane)
{
    for (int i = 0; i < numLanes; ++i)
        if ((lane < 0 || lane == i) && ! isLocked (i))
        {
            seed[i].store ((uint32_t) juce::Random::getSystemRandom().nextInt (1 << 30) + 2u);
            regenerate (i);
        }
}

juce::AudioProcessorValueTreeState::ParameterLayout RaspaProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (int i = 0; i < numLanes; ++i)
    {
        auto lane = " " + juce::String (i + 1);
        const auto& d = laneDefaults[i];
        juce::StringArray voices;
        for (int t = 0; t < numVoiceTypes; ++t) voices.add (juce::String::fromUTF8 (voiceName (t)));
        layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { pid ("voice", i), 2 }, "Voice" + lane, voices, d.voice));
        layout.add (std::make_unique<juce::AudioParameterBool>  (juce::ParameterID { pid ("latch", i), 1 }, "Latch" + lane, false));
        // ids stay "grain"/"seed" so stage-1 sets still load; they are the two voice knobs
        layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { pid ("grain", i), 1 }, "Voice A" + lane, 0.0f, 1.0f, d.a));
        layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { pid ("seed",  i), 1 }, "Voice B" + lane, 0.0f, 1.0f, d.b));
        layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { pid ("decay", i), 1 }, "Decay" + lane, 0.0f, 1.0f, d.decay));
        layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { pid ("level", i), 1 }, "Level" + lane, 0.0f, 1.0f, d.level));
        layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { pid ("pitch", i), 4 }, "Pitch" + lane,
                        juce::NormalisableRange<float> (-12.0f, 12.0f), 0.0f,
                        juce::AudioParameterFloatAttributes().withLabel ("st")
                            .withStringFromValueFunction ([] (float v, int) { return (v > 0.004f ? "+" : "") + juce::String (v, 2) + " st"; })));
        juce::StringArray genres;
        for (int g = 0; g < numGenres; ++g) genres.add (genreName (g));
        layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { pid ("genre", i), 3 }, "Genre" + lane, genres, d.genre));
        layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { pid ("density", i), 3 },   "Density" + lane,   0.0f, 1.0f, d.density));
        layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { pid ("variation", i), 3 }, "Variation" + lane, 0.0f, 1.0f, d.variation));
        layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { pid ("swing", i), 3 },     "Swing" + lane,     0.0f, 1.0f, d.swing));
        layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { pid ("accent", i), 3 },    "Accent" + lane,    0.0f, 1.0f, d.accent));
        layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { pid ("humanize", i), 3 },  "Humanize" + lane,  0.0f, 1.0f, d.humanize));
        layout.add (std::make_unique<juce::AudioParameterBool>  (juce::ParameterID { pid ("lock", i), 3 },      "Lock" + lane, false));

        const auto& m = laneMix[i];
        layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { pid ("pan", i), 5 },    "Pan" + lane,    -1.0f, 1.0f, m.pan));
        layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { pid ("cutoff", i), 5 }, "Cutoff" + lane, 0.0f, 1.0f, 1.0f));
        layout.add (std::make_unique<juce::AudioParameterBool>  (juce::ParameterID { pid ("hipass", i), 8 }, "Filter high-pass" + lane, false));
        layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { pid ("res", i), 5 },    "Resonance" + lane, 0.0f, 1.0f, 0.0f));
        const char* sendNames[4] = { "Reverb send", "Delay send", "Chorus send", "Reverse send" };
        const char* sendIds[4] = { "sendA", "sendB", "sendC", "sendD" };
        for (int k = 0; k < 4; ++k)
            layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { pid (sendIds[k], i), 5 }, sendNames[k] + lane, 0.0f, 1.0f, m.send[k]));
        juce::StringArray times;
        for (int d = 0; d < numDelayTimes; ++d) times.add (delayTimeName (d));
        layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { pid ("dtime", i), 5 }, "Delay time" + lane, times, m.delayTime));
        layout.add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { pid ("innote", i), 7 }, "MIDI in note" + lane, 0, 127, firstNote + i,
                        juce::AudioParameterIntAttributes().withStringFromValueFunction ([] (int v, int) { return noteName (v); })));
        layout.add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { pid ("outnote", i), 6 }, "MIDI out note" + lane, 0, 127, firstNote + i,
                        juce::AudioParameterIntAttributes().withStringFromValueFunction ([] (int v, int) { return noteName (v); })));
    }
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "midiout_on", 6 }, "MIDI out", true));
    layout.add (std::make_unique<juce::AudioParameterInt>  (juce::ParameterID { "midiout_ch", 6 }, "MIDI out channel", 1, 16, 10));

    auto f = [&] (const char* id, const char* name, float def)
    { layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 5 }, name, 0.0f, 1.0f, def)); };
    const raspa::FxSettings d;
    f ("verb_size", "Reverb size", d.verbSize);       f ("verb_decay", "Reverb decay", d.verbDecay);
    f ("verb_damp", "Reverb damp", d.verbDamp);       f ("verb_pre", "Reverb pre-delay", d.verbPre);
    f ("verb_return", "Reverb return", d.verbReturn);
    f ("dly_feedback", "Delay feedback", d.dlyFeedback); f ("dly_cutoff", "Delay cutoff", d.dlyCutoff);
    f ("dly_res", "Delay resonance", d.dlyRes);       f ("dly_return", "Delay return", d.dlyReturn);
    f ("cho_rate", "Chorus rate", d.choRate);         f ("cho_depth", "Chorus depth", d.choDepth);
    f ("cho_width", "Chorus width", d.choWidth);      f ("cho_return", "Chorus return", d.choReturn);
    juce::StringArray lens;
    for (int r = 0; r < numReverseLengths; ++r) lens.add (reverseLengthName (r));
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "rev_length", 5 }, "Reverse length", lens, d.revLength));
    f ("rev_slice", "Reverse slice", d.revSlice);     f ("rev_fade", "Reverse fade", d.revFade);
    f ("rev_return", "Reverse return", d.revReturn);
    f ("master", "Master volume", d.master);
    return layout;
}

void RaspaProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate);
    scratch.setSize (2, juce::jmax (samplesPerBlock, 32));
}

bool RaspaProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void RaspaProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();

    Transport t;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto bpm = pos->getBpm()) t.bpm = *bpm;
            t.playing = pos->getIsPlaying();
            if (auto ppq = pos->getPpqPosition()) t.ppq = *ppq; else t.playing = false;
        }
    hostBpm.store (t.bpm);
    hostPlaying.store (t.playing);

    for (int i = 0; i < numLanes; ++i)
    {
        auto& l = engine.lanes[i];
        l.latch = lp[i].latch->load() > 0.5f;
        l.level = lp[i].level->load();
        l.swing = lp[i].swing->load();
        l.accent = lp[i].accent->load();
        l.humanize = lp[i].humanize->load();
        l.pan = lp[i].pan->load();
        l.cutoff = lp[i].cutoff->load();
        l.highpass = lp[i].hipass->load() > 0.5f;
        l.res = lp[i].res->load();
        for (int k = 0; k < 4; ++k) l.send[k] = lp[i].send[k]->load();
        l.delayTime = (int) lp[i].dtime->load();
        l.inNote = (int) inNote[i]->load();
        l.voice.setParams ((int) lp[i].voice->load(), lp[i].grain->load(), lp[i].seed->load(), lp[i].decay->load(), lp[i].pitch->load());
    }

    auto& fx = engine.fx;
    fx.verbSize = g[0]->load(); fx.verbDecay = g[1]->load(); fx.verbDamp = g[2]->load(); fx.verbPre = g[3]->load(); fx.verbReturn = g[4]->load();
    fx.dlyFeedback = g[5]->load(); fx.dlyCutoff = g[6]->load(); fx.dlyRes = g[7]->load(); fx.dlyReturn = g[8]->load();
    fx.choRate = g[9]->load(); fx.choDepth = g[10]->load(); fx.choWidth = g[11]->load(); fx.choReturn = g[12]->load();
    fx.revLength = (int) g[13]->load(); fx.revSlice = g[14]->load(); fx.revFade = g[15]->load(); fx.revReturn = g[16]->load();
    fx.master = g[17]->load();

    events.clear();
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (m.isNoteOn() && learnLane.load() >= 0)
        {
            const int lane = learnLane.exchange (-1);      // MIDI learn: this note becomes the lane's launch note
            if (lane >= 0 && lane < numLanes) learned[lane].store (m.getNoteNumber());
        }
        else if (m.isNoteOn())
            events.push_back ({ meta.samplePosition, m.getNoteNumber(), true });
        else if (m.isNoteOff())
            events.push_back ({ meta.samplePosition, m.getNoteNumber(), false });
        else if (m.isAllNotesOff() || m.isAllSoundOff())
            for (int i = 0; i < numLanes; ++i)
                for (int k = 0; k < 8; ++k)
                    events.push_back ({ meta.samplePosition, engine.lanes[i].inNote, false });
        if (events.size() >= events.capacity() - 40) break;
    }
    midi.clear();   // incoming notes are used up; what goes out is RASPA's own hits

    if (scratch.getNumSamples() < n) scratch.setSize (2, n, false, false, true);
    const bool outOn = midiOutOn->load() > 0.5f;
    const int ch = juce::jlimit (1, 16, (int) midiOutCh->load());
    if (! outOn || ch != soundingCh)
        for (int i = 0; i < numLanes; ++i)                       // switched off / channel changed: close open notes
            if (sounding[i] >= 0) { midi.addEvent (juce::MidiMessage::noteOff (soundingCh, sounding[i]), 0); sounding[i] = -1; }
    soundingCh = ch;
    engine.midiOutEnabled = outOn;
    engine.process (scratch.getWritePointer (0), scratch.getWritePointer (1), n, t, events);

    for (const auto& m : engine.midiOut)
    {
        if (m.on)
        {
            const int note = juce::jlimit (0, 127, (int) outNote[m.lane]->load());
            const auto vel = (juce::uint8) juce::jlimit (1, 127, (int) std::lround (m.velocity * 127.0f));
            midi.addEvent (juce::MidiMessage::noteOn (ch, note, vel), m.sample);
            sounding[m.lane] = note;
        }
        else if (sounding[m.lane] >= 0)
        {
            midi.addEvent (juce::MidiMessage::noteOff (ch, sounding[m.lane]), m.sample);   // the note that was actually sent
            sounding[m.lane] = -1;
        }
    }

    const int chans = buffer.getNumChannels();
    if (chans == 1)
    {
        buffer.copyFrom (0, 0, scratch, 0, 0, n);
        buffer.addFrom (0, 0, scratch, 1, 0, n);
        buffer.applyGain (0.5f);
    }
    else
        for (int c = 0; c < chans; ++c)
            buffer.copyFrom (c, 0, scratch, juce::jmin (c, 1), 0, n);
}

void RaspaProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    for (int i = 0; i < numLanes; ++i)
    {
        juce::String p;
        for (int s = 0; s < numSteps; ++s) p << engine.lanes[i].pattern[s].load();
        state.setProperty ("pattern_" + juce::String (i + 1), p, nullptr);
        juce::String c, r;
        for (int k = 0; k < numSteps; ++k) { c << engine.lanes[i].chance[k].load(); r << engine.lanes[i].ratchet[k].load(); }
        state.setProperty ("chance_" + juce::String (i + 1), c, nullptr);
        state.setProperty ("ratchet_" + juce::String (i + 1), r, nullptr);
        state.setProperty ("seed_" + juce::String (i + 1), (juce::int64) seed[i].load(), nullptr);
    }
    state.setProperty ("preset", currentPresetName, nullptr);
    state.setProperty ("preset_index", currentPreset, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void RaspaProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        auto state = juce::ValueTree::fromXml (*xml);
        if (! state.isValid()) return;
        loadingState.store (true);
        apvts.replaceState (state);
        loadingState.store (false);
        currentPresetName = state.getProperty ("preset", "Init").toString();
        currentPreset = (int) state.getProperty ("preset_index", -1);
        for (int i = 0; i < numLanes; ++i)
        {
            const auto p = state.getProperty ("pattern_" + juce::String (i + 1)).toString();
            if (p.length() == numSteps)
                for (int s = 0; s < numSteps; ++s)
                    engine.lanes[i].pattern[s].store (juce::jlimit (0, 3, p[s] - '0'));
            const auto c = state.getProperty ("chance_" + juce::String (i + 1)).toString();
            const auto r = state.getProperty ("ratchet_" + juce::String (i + 1)).toString();
            for (int k = 0; k < numSteps; ++k)
            {
                engine.lanes[i].chance[k].store (c.length() == numSteps ? juce::jlimit (0, 3, c[k] - '0') : 0);
                engine.lanes[i].ratchet[k].store (r.length() == numSteps ? juce::jlimit (1, 4, r[k] - '0') : 1);
            }
            if (state.hasProperty ("seed_" + juce::String (i + 1)))
                seed[i].store ((uint32_t) (juce::int64) state.getProperty ("seed_" + juce::String (i + 1)));
        }
    }
}

// ------------------------------------------------------------ presets
juce::String RaspaProcessor::noteName (int note)
{
    static const char* names[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    note = juce::jlimit (0, 127, note);
    return juce::String (names[note % 12]) + juce::String (note / 12 - 2);   // Ableton naming: 36 = C1
}

juce::File RaspaProcessor::userPresetFolder()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("raspa").getChildFile ("presets");
}

int RaspaProcessor::numFactory() const { return raspa::numFactoryPresets; }

juce::StringArray RaspaProcessor::getPresetNames()
{
    juce::StringArray names;
    for (int i = 0; i < raspa::numFactoryPresets; ++i) names.add (raspa::factoryPresets[i].name);
    userFiles = userPresetFolder().findChildFiles (juce::File::findFiles, false, "*.raspa");
    userFiles.sort();
    for (auto& f : userFiles) names.add (f.getFileNameWithoutExtension());
    return names;
}

void RaspaProcessor::loadPreset (int index)
{
    auto names = getPresetNames();
    if (index < 0 || index >= names.size()) return;

    if (index >= raspa::numFactoryPresets)
    {
        auto file = userFiles[index - raspa::numFactoryPresets];
        juce::MemoryBlock mb;
        if (! file.loadFileAsData (mb)) return;
        setStateInformation (mb.getData(), (int) mb.getSize());
    }
    else
    {
        loadingState.store (true);
        // everything back to default (Latch, MIDI out routing and Master stay as they are)
        for (auto* p : getParameters())
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            {
                const auto id = rp->getParameterID();
                if (id.startsWith ("latch") || id.startsWith ("innote") || id.startsWith ("outnote") || id.startsWith ("midiout") || id == "master") continue;
                rp->setValueNotifyingHost (rp->getDefaultValue());
            }
        for (int i = 0; i < numLanes; ++i)
        {
            seed[i].store (1);
            for (int k = 0; k < numSteps; ++k) { engine.lanes[i].chance[k].store (0); engine.lanes[i].ratchet[k].store (1); }
        }
        juce::StringArray items, stepStrings;
        items.addTokens (raspa::factoryPresets[index].settings, " ", "");
        for (auto& item : items)
        {
            auto key = item.upToFirstOccurrenceOf ("=", false, false);
            auto value = item.fromFirstOccurrenceOf ("=", false, false).getFloatValue();
            if (key.startsWith ("ch_") || key.startsWith ("rt_")) { stepStrings.add (item); continue; }
            if (key.startsWith ("rng_"))
            {
                int lane = key.getTrailingIntValue() - 1;
                if (lane >= 0 && lane < numLanes) seed[lane].store ((uint32_t) value);
            }
            else if (auto* rp = apvts.getParameter (key))
                rp->setValueNotifyingHost (rp->convertTo0to1 (value));
            else
                jassertfalse;   // unknown key in a factory preset
        }
        loadingState.store (false);
        for (int i = 0; i < numLanes; ++i) regenerate (i);
        for (auto& item : stepStrings)
            applyStepString (item.upToFirstOccurrenceOf ("=", false, false), item.fromFirstOccurrenceOf ("=", false, false));
    }
    currentPreset = index;
    currentPresetName = names[index];
}

// "ch_2=0000200002000020" sets lane 2's chance per step, "rt_2=1111211111112111" its repeats
void RaspaProcessor::applyStepString (const juce::String& key, const juce::String& digits)
{
    const int lane = key.getTrailingIntValue() - 1;
    if (lane < 0 || lane >= numLanes || digits.length() != numSteps) return;
    for (int k = 0; k < numSteps; ++k)
    {
        if (key.startsWith ("ch_")) engine.lanes[lane].chance[k].store (juce::jlimit (0, 3, digits[k] - '0'));
        else engine.lanes[lane].ratchet[k].store (juce::jlimit (1, 4, digits[k] - '0'));
    }
}

void RaspaProcessor::stepPreset (int delta)
{
    const int count = getPresetNames().size();
    if (count == 0) return;
    loadPreset (((currentPreset < 0 ? (delta > 0 ? -1 : 0) : currentPreset) + delta + count) % count);
}

bool RaspaProcessor::saveUserPreset (const juce::String& rawName)
{
    auto name = juce::File::createLegalFileName (rawName.trim());
    if (name.isEmpty()) return false;
    auto folder = userPresetFolder();
    if (! folder.createDirectory()) return false;
    juce::MemoryBlock mb;
    getStateInformation (mb);
    auto file = folder.getChildFile (name + ".raspa");
    if (! file.replaceWithData (mb.getData(), mb.getSize())) return false;
    auto names = getPresetNames();
    currentPreset = names.indexOf (name);
    currentPresetName = name;
    return true;
}

juce::AudioProcessorEditor* RaspaProcessor::createEditor() { return new RaspaEditor (*this); }

#if ! RASPA_NO_PLUGIN_ENTRY
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new RaspaProcessor(); }
#endif
