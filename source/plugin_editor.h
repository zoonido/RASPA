// plugin_editor.h — ÁCIDO interface (phase 6)
// The designed window from mockup v7: every control drawn to match the
// mockup, the Info panel, the step modulator and the preset bar.
// Everything is laid out at 1130 x 1160 and scaled (60–125%) as a whole.
#pragma once

#include "plugin_processor.h"

class AcidoEditor;

namespace ui
{
    // Colours from the mockup.
    const juce::Colour bg       { 0xff16150f }, panel  { 0xff1f1d17 }, border { 0xff2e2b22 },
                       display  { 0xff121109 }, track  { 0xff35322a }, body   { 0xff2a2820 },
                       bodyLine { 0xff403c2f }, button { 0xff26241c }, line   { 0xff3a3629 },
                       text     { 0xffe9e4d4 }, soft   { 0xffcfc9b6 }, muted  { 0xff9d967f },
                       faint    { 0xff8a846f }, dim    { 0xff5c5646 }, pointer{ 0xfff3efe2 },
                       accent   { 0xffc8f03c }, accent2{ 0xfff0a03c }, popup  { 0xff24221b };

    juce::Font mono (float size, bool medium = false);
    juce::Font title (float size, bool bold = true);
}

// ---------------------------------------------------------------------------
// A knob bound to one parameter. Drag up/down to turn (Shift = fine),
// double-click to reset, single click to edit its step lane.
// ---------------------------------------------------------------------------
class Knob : public juce::Component
{
public:
    Knob (AcidoEditor& ed, const juce::String& paramId, const juce::String& label, int size,
          juce::Colour colour, bool rowStyle = false);

    void setParameter (const juce::String& paramId);   // Delay Time switches between Sync and MS
    void setLabel (const juce::String& l) { if (label != l) { label = l; repaint(); } }
    const juce::String& getLaneId() const { return laneId; }

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseEnter (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

private:
    AcidoEditor& editor;
    juce::String laneId, label;
    juce::RangedAudioParameter* param = nullptr;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    int size;
    juce::Colour colour;
    bool row;
    float dragStart = 0.0f;
    bool dragging = false;
};

// A row of buttons (waveforms, octaves, modes...) — bound to a parameter or to a lane setting.
class SegButtons : public juce::Component
{
public:
    SegButtons (AcidoEditor& ed, juce::StringArray labels, std::function<int()> getter, std::function<void (int)> setter,
                float fontSize, const juce::String& infoId = {});
    static std::unique_ptr<SegButtons> forParameter (AcidoEditor& ed, const juce::String& paramId, juce::StringArray labels, float fontSize);

    std::vector<juce::Path> icons;   // optional, one per button
    int columns = 0;                 // 0 = one row

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

    std::unique_ptr<juce::ParameterAttachment> attachment;

private:
    juce::Rectangle<float> cell (int i) const;
    AcidoEditor& editor;
    juce::StringArray labels;
    std::function<int()> get;
    std::function<void (int)> set;
    float font;
    juce::String info;
};

// "−  3 VOICES  +"
class Stepper : public juce::Component
{
public:
    Stepper (AcidoEditor& ed, std::function<juce::String()> text, std::function<void (int)> step,
             std::function<bool()> enabled = {}, const juce::String& infoId = {});
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    juce::Colour textColour = ui::text;

private:
    AcidoEditor& editor;
    std::function<juce::String()> text;
    std::function<void (int)> step;
    std::function<bool()> enabled;
    juce::String info;
};

// The Neblina macro slider.
class MacroSlider : public juce::Component
{
public:
    explicit MacroSlider (AcidoEditor& ed);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
private:
    juce::Rectangle<float> trackArea() const;
    void setFromX (float x);
    AcidoEditor& editor;
    juce::RangedAudioParameter* param;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    bool dragging = false;
    float downX = 0.0f;
};

// The filter envelope drawing (normal note and accented note).
class EnvelopeDisplay : public juce::Component
{
public:
    explicit EnvelopeDisplay (AcidoProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
private:
    AcidoProcessor& proc;
};

// The 16 step bars.
class StepGrid : public juce::Component
{
public:
    explicit StepGrid (AcidoEditor& ed) : editor (ed) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent& e) override { edit (e); }
    void mouseDrag (const juce::MouseEvent& e) override { edit (e); }
private:
    void edit (const juce::MouseEvent&);
    AcidoEditor& editor;
};

// Simple flat button drawn like the mockup.
class FlatButton : public juce::Component
{
public:
    enum Style { Normal, Outline, Filled };
    FlatButton (juce::String t, Style s = Normal) : text (std::move (t)), style (s) {}
    std::function<void()> onClick;
    juce::Path icon;
    bool enabled = true;
    float fontSize = 11.0f;
    std::function<void (juce::Graphics&, juce::Rectangle<float>)> customPaint;
    void setText (const juce::String& t) { text = t; repaint(); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { if (enabled && onClick) onClick(); }
    void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override { hover = false; repaint(); }
private:
    juce::String text;
    Style style;
    bool hover = false;
};

// Everything at 1130 x 1160; the editor scales this as a whole.
class Root : public juce::Component
{
public:
    explicit Root (AcidoEditor& ed) : editor (ed) {}
    void paint (juce::Graphics&) override;
private:
    AcidoEditor& editor;
};

class AcidoEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit AcidoEditor (AcidoProcessor&);
    ~AcidoEditor() override;

    void resized() override;
    void paint (juce::Graphics& g) override { g.fillAll (ui::bg); }

    // Shared with the controls
    AcidoProcessor& proc;
    void showInfo (const juce::String& id);
    void selectLane (const juce::String& id);
    const juce::String& selectedLane() const { return laneId; }
    acido::Lane* currentLane();
    bool laneActive (const juce::String& id) const;
    juce::String selectedLabel() const;
    void paintRoot (juce::Graphics&);

    static constexpr int kWidth = 1130, kHeight = 1160;

private:
    void timerCallback() override;
    void layout();
    void refreshAfterPreset();
    void showPresetMenu();
    void openSave (bool open);

    Root root { *this };
    juce::LookAndFeel_V4 laf;

    // Header
    FlatButton prevButton { "" }, nextButton { "" }, presetButton { "", FlatButton::Normal }, saveButton { "SAVE", FlatButton::Outline };

    // Top row
    std::unique_ptr<SegButtons> waves, subOct, voiceMode, delaySync, reverbType;
    std::unique_ptr<Knob> tune, shape, sub, cutoff, reso, envmod, decay, ffm, accent, slide, drift, velo;
    std::unique_ptr<MacroSlider> neblina;
    std::unique_ptr<Stepper> voices, unison;
    EnvelopeDisplay envelope { proc };

    // Effects row
    std::unique_ptr<Knob> noise, drive, warmth, ctone, cmix, dtime, dfb, dmix, rsize, rmix, crush, comp, vol;

    // Step modulator
    std::unique_ptr<SegButtons> laneRate, laneMode;
    std::unique_ptr<Stepper> laneLength, laneDepth;
    FlatButton fills[5] { FlatButton ("SINE"), FlatButton ("TRI"), FlatButton ("SAW"), FlatButton ("RND"), FlatButton ("CLEAR") };
    StepGrid grid { *this };

    // Save panel
    struct SavePanel : juce::Component
    {
        void paint (juce::Graphics&) override;
    } savePanel;
    juce::TextEditor nameBox;
    FlatButton cancelButton { "CANCEL" }, overwriteButton { "OVERWRITE" }, saveNewButton { "SAVE AS NEW", FlatButton::Filled };

    std::vector<Knob*> allKnobs;
    juce::String laneId { "cutoff" }, infoId;
    bool lastDelaySync = true;
    int lastWave = -1, tick = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AcidoEditor)
};
