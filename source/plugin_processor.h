#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "engine.h"

class RaspaProcessor : public juce::AudioProcessor,
                       private juce::AudioProcessorValueTreeState::Listener
{
public:
    RaspaProcessor();
    ~RaspaProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "raspa"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    static juce::String pid (const char* name, int lane) { return juce::String (name) + "_" + juce::String (lane + 1); }

    juce::AudioProcessorValueTreeState apvts;
    raspa::Engine engine;
    // generator: roll a new variation for one lane (or all with lane = -1); ignored when locked
    void roll (int lane);
    bool isLocked (int lane) const { return lp[lane].lock->load() > 0.5f; }
    std::atomic<uint32_t> seed[raspa::numLanes];

    // presets: factory first, then the user's own (Documents/raspa/presets)
    juce::StringArray getPresetNames();
    int numFactory() const;
    void loadPreset (int index);
    void stepPreset (int delta);
    bool saveUserPreset (const juce::String& name);
    juce::String currentPresetName { "Init" };
    int currentPreset = -1;
    static juce::File userPresetFolder();

    static juce::String noteName (int note);

    // MIDI learn for the launch notes: set learnLane, the next note-on is captured for it
    std::atomic<int> learnLane { -1 };
    std::atomic<int> learned[raspa::numLanes] { { -1 }, { -1 }, { -1 }, { -1 } };

    std::atomic<double> hostBpm { 120.0 };
    std::atomic<bool> hostPlaying { false };

private:
    struct LaneParams { std::atomic<float>* voice; std::atomic<float>* latch; std::atomic<float>* grain; std::atomic<float>* seed; std::atomic<float>* decay; std::atomic<float>* level; std::atomic<float>* pitch;
                        std::atomic<float>* genre; std::atomic<float>* density; std::atomic<float>* variation; std::atomic<float>* swing;
                        std::atomic<float>* accent; std::atomic<float>* humanize; std::atomic<float>* lock;
                        std::atomic<float>* pan; std::atomic<float>* hipass; std::atomic<float>* cutoff; std::atomic<float>* res; std::atomic<float>* send[4]; std::atomic<float>* dtime; };
    std::atomic<float>* g[18] = {};
    std::atomic<float>* outNote[raspa::numLanes] = {};
    std::atomic<float>* inNote[raspa::numLanes] = {};
    void applyStepString (const juce::String& key, const juce::String& digits);
    std::atomic<float>* midiOutOn = nullptr;
    std::atomic<float>* midiOutCh = nullptr;
    int sounding[raspa::numLanes] = { -1, -1, -1, -1 };   // note each lane has on (MIDI out), -1 none
    int soundingCh = 10;
    juce::Array<juce::File> userFiles;
    void parameterChanged (const juce::String& id, float newValue) override;
    void regenerate (int lane);
    std::atomic<bool> loadingState { false };
    LaneParams lp[raspa::numLanes];
    std::vector<raspa::NoteEvent> events;
    juce::AudioBuffer<float> scratch;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RaspaProcessor)
};
