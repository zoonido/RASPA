// plugin_processor.h — ÁCIDO plugin shell (phase 6)
#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "dsp/acid_engine.h"
#include "dsp/step_mod.h"
#include "preset_manager.h"
#include <map>
#include <string>

class AcidoProcessor : public juce::AudioProcessor
{
public:
    AcidoProcessor();
    ~AcidoProcessor() override = default;

    // Audio
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    // Editor (a working test window for now; the designed interface comes in phase 6)
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    // Info
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 12.0; }   // reverb and delay tails

    // Programs (presets come in phase 5)
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Squelch Init"; }
    void changeProgramName (int, const juce::String&) override {}

    // State saved inside the Live Set
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    // Step modulator: one lane per knob listed in modTargets(), same order.
    static const std::vector<std::string>& modTargets();
    std::vector<std::unique_ptr<acido::Lane>> lanes;
    int laneIndexFor (const std::string& id) const;
    void resetLanes() { loadLanes ({}); }

    // The whole sound (knobs + lanes + preset name), for the Live Set and preset files.
    juce::ValueTree makeState();
    bool applyState (juce::ValueTree state);

    PresetManager presets { *this };

    // For the window: Ableton's tempo and play state, and the window size.
    std::atomic<double> hostBpm { 120.0 };
    std::atomic<bool> hostPlaying { false };
    float uiScale = 0.75f;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    acido::SoundParams readParams();
    float value (const char* id);   // knob value plus its step lane, in real units
    void handleMidi (const juce::MidiMessage& m);

    acido::AcidEngine engine;
    std::vector<float> left, right;   // stereo render buffers

    // Fast pointers to the current parameter values, by ID.
    std::map<std::string, std::atomic<float>*> params;
    std::map<std::string, juce::RangedAudioParameter*> ranged;
    std::map<std::string, int> laneOf;
    acido::StepModulator modulator;
    double currentSampleRate = 44100.0;

    void saveLanes (juce::ValueTree& state) const;
    void loadLanes (const juce::ValueTree& state);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AcidoProcessor)
};
