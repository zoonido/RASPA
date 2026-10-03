#pragma once
#include "plugin_processor.h"

namespace raspa::ui
{
    namespace col
    {
        const juce::Colour ground  { 0xff14110e }, panel { 0xff1d1914 }, line { 0xff2e2820 },
                           text    { 0xffece4d6 }, muted { 0xffb3a590 }, dim  { 0xff8f8270 },
                           brass   { 0xffe0a84a }, teal  { 0xff7cc4c6 },
                           knob    { 0xff262019 }, ring  { 0xff40362b }, cell { 0xff191510 };
    }

    juce::Font mono (float size, bool bold = false);
    juce::Font sans (float size, bool bold = false);

    struct LookAndFeel : juce::LookAndFeel_V4
    {
        LookAndFeel();
        void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                               float start, float end, juce::Slider&) override;
        void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
        void drawButtonText (juce::Graphics&, juce::TextButton&, bool over, bool down) override;
        void drawComboBox (juce::Graphics&, int w, int h, bool down, int, int, int, int, juce::ComboBox&) override;
        juce::Font getComboBoxFont (juce::ComboBox&) override;
        void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
        juce::Font getPopupMenuFont() override;
    };

    // Small square button with a drawn icon (dice or lock)
    struct IconButton : juce::Button
    {
        enum Kind { dice, lock };
        IconButton (Kind k) : juce::Button (k == dice ? "Dice" : "Lock"), kind (k) {}
        void paintButton (juce::Graphics&, bool over, bool down) override;
        Kind kind;
    };

    // The 16-step pattern of one lane. Click: off -> accent -> medium -> ghost -> off.
    // Right-click (or Cmd-click) clears a step. The outlined step is the one playing.
    struct StepGrid : juce::Component, juce::SettableTooltipClient
    {
        enum Mode { velocity = 0, chance, repeats };
        StepGrid (raspa::Lane& l, const int& m) : lane (l), mode (m) {}
        const int& mode;   // shared by all lanes: what a click edits
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        int cellAt (juce::Point<int> p) const;
        juce::Rectangle<float> cellBounds (int i) const;
        raspa::Lane& lane;
        int shownStep = -2;
    };
}

class RaspaEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit RaspaEditor (RaspaProcessor&);
    static constexpr int numBusKnobs = 17;
    ~RaspaEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    struct LaneUI
    {
        std::unique_ptr<raspa::ui::StepGrid> grid;
        juce::TextButton latch { "LATCH" };
        juce::ComboBox voice;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> voiceAtt;
        int shownVoice = -1;
        juce::Slider knobs[8];
        juce::Slider sends[4];
        juce::ComboBox dtime, outNote;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> outNoteAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sendAtt[4];
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> dtimeAtt;
        juce::ComboBox genre;
        raspa::ui::IconButton dice { raspa::ui::IconButton::dice }, lock { raspa::ui::IconButton::lock };
        juce::Slider gen[5];
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> genreAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> lockAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> genAtt[5];
        int patternHash = -1;
        juce::TextButton learn;
        juce::TextButton ftype { "LP" };
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> ftypeAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> latchAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> knobAtt[8];
        juce::Rectangle<int> bounds;
        bool shownPlaying = false;
    };

    RaspaProcessor& proc;
    raspa::ui::LookAndFeel lnf;
    LaneUI lanes[raspa::numLanes];
    double shownBpm = -1;
    juce::TextButton diceAll { "DICE ALL" };
    int gridMode = 0;
    juce::TextButton modeVel { "VEL" }, modeChance { "CHANCE" }, modeRepeat { "REPEAT" };
    int blink = 0;

    // bottom strip: the four send buses and master
    juce::Slider bus[numBusKnobs];
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> busAtt[numBusKnobs];
    juce::ComboBox revLen;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> revLenAtt;
    juce::Rectangle<int> busBounds[5];

    // header: presets
    juce::TextButton presetPrev { "<" }, presetNext { ">" }, presetName, presetSave { "SAVE" };
    void showPresetMenu();
    void savePresetDialog();
    void refreshPresetName();

    // master: MIDI out
    juce::TextButton midiOut { "MIDI OUT" };
    juce::ComboBox midiCh;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> midiOutAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> midiChAtt;

    // help bar: shows what's under the mouse
    juce::String helpText;
    std::unique_ptr<juce::AlertWindow> saveWindow;
    void setupKnob (juce::Slider&, const juce::String& paramId);

    static constexpr int W = 1280, H = 724;
    static constexpr int headW = 140, genW = 230, stepW = 344, voiceW = 290, sendW = 150, gap = 12;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RaspaEditor)
};
