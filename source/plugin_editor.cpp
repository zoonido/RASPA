// plugin_editor.cpp — ÁCIDO interface (phase 6)
#include "plugin_editor.h"
#include "BinaryData.h"

// ===========================================================================
// Fonts and helpers
// ===========================================================================
namespace
{
    juce::Typeface::Ptr typeface (const char* data, int size) { return juce::Typeface::createSystemTypefaceFor (data, (size_t) size); }

    juce::Typeface::Ptr plexRegular()  { static auto t = typeface (BinaryData::IBMPlexMonoRegular_ttf, BinaryData::IBMPlexMonoRegular_ttfSize); return t; }
    juce::Typeface::Ptr plexMedium()   { static auto t = typeface (BinaryData::IBMPlexMonoMedium_ttf, BinaryData::IBMPlexMonoMedium_ttfSize); return t; }
    juce::Typeface::Ptr chakraBold()   { static auto t = typeface (BinaryData::ChakraPetchBold_ttf, BinaryData::ChakraPetchBold_ttfSize); return t; }
    juce::Typeface::Ptr chakraSemi()   { static auto t = typeface (BinaryData::ChakraPetchSemiBold_ttf, BinaryData::ChakraPetchSemiBold_ttfSize); return t; }

    // Text with letter spacing like the mockup's small caps labels.
    void spaced (juce::Graphics& g, const juce::String& t, juce::Rectangle<float> r, float size, float spacing,
                 juce::Colour c, juce::Justification j = juce::Justification::centredLeft, bool medium = false)
    {
        g.setColour (c);
        g.setFont (ui::mono (size, medium).withExtraKerningFactor (spacing));
        g.drawText (t, r, j, false);
    }

    void panelBox (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour fill = ui::panel)
    {
        g.setColour (fill);
        g.fillRoundedRectangle (r, 10.0f);
        g.setColour (ui::border);
        g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.0f);
    }

    juce::Path chevron (bool right)
    {
        juce::Path p;
        if (right) { p.startNewSubPath (6, 3); p.lineTo (11, 8); p.lineTo (6, 13); }
        else       { p.startNewSubPath (10, 3); p.lineTo (5, 8); p.lineTo (10, 13); }
        return p;
    }

    // Plain-language help for the Info panel (from the mockup).
    struct InfoText { const char* section; const char* text; };
    InfoText infoFor (const juce::String& id, int wave)
    {
        static const char* shapeText[4] = {
            "Morphs the saw toward a softer triangle. Low is bright and buzzy; high is rounder and gentler.",
            "Pulse width. 50% is a hollow square; moving away thins it into a nasal, reedy tone.",
            "Wavefolding. Folds the sine back on itself to add harmonics, from a pure tone to a bright growl.",
            "FM index: how hard the modulator bends the sine. Low adds warmth; high gets metallic and clangy." };

        static const std::map<juce::String, InfoText> texts {
            { "tune",    { "OSC", "Shifts the pitch of the whole voice in semitones, so you can match your track without transposing the MIDI." } },
            { "sub",     { "SUB", "A deep sine one or two octaves below the note. It skips the filter, so it keeps its full weight however you set Cutoff and Resonance." } },
            { "suboct",  { "SUB", "Plays the sub one or two octaves below the note." } },
            { "wave",    { "OSC", "Saw and Square are the classic acid waves. Sine is pure (fold it with Shape); FM adds metallic, rubbery tones." } },
            { "neblina", { "OSC \xc2\xb7 MACRO", "Detunes the unison voices with a slow wobble, adds drifting random resonances and a light phaser. 0 is dry and focused; higher becomes a moving haze." } },
            { "cutoff",  { "FILTER", "Where the low-pass filter starts cutting the highs. The envelope and accents open it upward from here: the core of the acid sound." } },
            { "reso",    { "FILTER", "Boosts the frequencies right at the cutoff. High values give the classic squelch and whistle." } },
            { "envmod",  { "FILTER", "How far the filter envelope opens the cutoff on each note. More means a bigger \xe2\x80\x9cwow\xe2\x80\x9d per note." } },
            { "decay",   { "FILTER", "How fast the filter closes after each note. Short is plucky and tight; long is a slow wah." } },
            { "ffm",     { "FILTER", "Lets the oscillator modulate the cutoff at audio rate. Grit at low amounts, a screaming metallic edge at high amounts." } },
            { "accent",  { "VOICE", "How much harder accented notes (velocity 100 or more) hit: louder, with a brighter and faster filter sweep." } },
            { "slide",   { "VOICE", "Glide time. In Mono, overlapping notes glide into each other; in Poly, each new note glides from the last one played." } },
            { "drift",   { "VOICE", "Tiny random changes to pitch and filter on every note, like an analog circuit that never sits perfectly still." } },
            { "velo",    { "VOICE", "How much velocity affects normal, non-accented notes. At 0 every note hits the same, like the original 303." } },
            { "mode",    { "VOICE", "Mono plays one note at a time, with 303 slides. Poly plays chords; each new note glides from the last one." } },
            { "voices",  { "VOICE", "How many notes Poly can play at once (2\xe2\x80\x93" "8). When all are busy, the oldest note is taken over." } },
            { "noise",   { "NOISE \xc2\xb7 DRIVE", "Lays an airy hiss on top of each note. It follows the note's volume and never pushes the note itself down." } },
            { "drive",   { "NOISE \xc2\xb7 DRIVE", "Blends a distorted copy on top of the clean sound, like a pedal on a send: more Drive means more grit, never a duller or quieter note." } },
            { "warmth",  { "NOISE \xc2\xb7 DRIVE", "Character of the distortion: low is bright and fuzzy, high is round and tape-like." } },
            { "unison",  { "CHORUS \xc2\xb7 UNISON", "Stacks 1\xe2\x80\x93" "7 copies of the oscillator, spread in pitch and across the stereo field. The deep bass stays centred." } },
            { "ctone",   { "CHORUS \xc2\xb7 UNISON", "Brightness of the chorus signal. Turn it down to keep the effect soft." } },
            { "cmix",    { "CHORUS \xc2\xb7 UNISON", "Balance between the dry sound and the chorus." } },
            { "dsync",   { "DELAY", "SYNC steps through note values locked to Ableton's tempo; MS sets the time freely in milliseconds." } },
            { "dtime",   { "DELAY", "Echo time. In SYNC it steps through note values locked to Ableton; in MS it moves freely in milliseconds." } },
            { "dfb",     { "DELAY", "How much of each echo feeds back into the delay. Higher means more repeats." } },
            { "dmix",    { "DELAY", "Balance between the dry sound and the echoes." } },
            { "rtype",   { "REVERB", "The kind of space: a tight Room, a long Hall, a bright Plate or a boingy Spring." } },
            { "rsize",   { "REVERB", "Size of the virtual space, from a tight room to a long tail." } },
            { "rmix",    { "REVERB", "How much reverb is added on top. The original sound always stays at full level, so turning it up never pushes the bassline back." } },
            { "crush",   { "OUTPUT", "Lowers bit depth and sample rate together. Light settings add grit; high settings go lo-fi and crunchy." } },
            { "comp",    { "OUTPUT", "One-knob punch: lets each note's hit through, then lifts the body of the note. More means a louder, denser, punchier bassline." } },
            { "vol",     { "OUTPUT", "Final output level of the plugin." } },
        };
        if (id == "shape") return { "OSC", shapeText[juce::jlimit (0, 3, wave)] };
        auto it = texts.find (id);
        return it != texts.end() ? it->second : InfoText { "", "" };
    }

    // Layout (in the 1130 x 1160 design space, from the mockup).
    namespace L
    {
        const juce::Rectangle<float> header  { 24, 24, 1082, 56 };
        const juce::Rectangle<float> osc     { 24, 96, 200, 512 };
        const juce::Rectangle<float> filter  { 240, 96, 610, 512 };
        const juce::Rectangle<float> info    { 866, 96, 240, 288 };
        const juce::Rectangle<float> voice   { 866, 400, 240, 208 };
        const juce::Rectangle<float> drive   { 24, 624, 152, 220 };
        const juce::Rectangle<float> chorus  { 194, 624, 200, 220 };
        const juce::Rectangle<float> delay   { 414, 624, 250, 220 };
        const juce::Rectangle<float> reverb  { 684, 624, 250, 220 };
        const juce::Rectangle<float> output  { 954, 624, 152, 220 };
        const juce::Rectangle<float> step    { 24, 860, 1082, 240 };
        const juce::Rectangle<float> display { 260, 138, 570, 296 };
        const juce::Rectangle<float> sync    { 522, 30, 150, 44 };
        const float arrowsX[4] = { 179, 400, 670, 940 };
    }
}

// Sizes are CSS-style point sizes, matching the mockup.
juce::Font ui::mono (float size, bool medium) { return juce::Font (juce::FontOptions (medium ? plexMedium() : plexRegular()).withPointHeight (size)); }
juce::Font ui::title (float size, bool bold) { return juce::Font (juce::FontOptions (bold ? chakraBold() : chakraSemi()).withPointHeight (size)); }

// ===========================================================================
// Knob
// ===========================================================================
Knob::Knob (AcidoEditor& ed, const juce::String& paramId, const juce::String& l, int s, juce::Colour c, bool rowStyle)
    : editor (ed), label (l), size (s), colour (c), row (rowStyle)
{
    setParameter (paramId);
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
}

void Knob::setParameter (const juce::String& paramId)
{
    param = editor.proc.apvts.getParameter (paramId);
    laneId = editor.proc.laneIndexFor (paramId.toStdString()) >= 0 ? paramId : juce::String();
    attachment = std::make_unique<juce::ParameterAttachment> (*param, [this] (float) { repaint(); });
    attachment->sendInitialUpdate();
    repaint();
}

void Knob::paint (juce::Graphics& g)
{
    const bool selected = laneId.isNotEmpty() && editor.selectedLane() == laneId;
    auto b = getLocalBounds().toFloat();
    if (selected)
    {
        g.setColour (ui::body);
        g.fillRoundedRectangle (b.reduced (0.5f), 8.0f);
        g.setColour (ui::accent);
        g.drawRoundedRectangle (b.reduced (0.5f), 8.0f, 1.0f);
    }

    const float sz = (float) size;
    const auto knobArea = row ? juce::Rectangle<float> (4.0f, (b.getHeight() - sz) * 0.5f, sz, sz)
                              : juce::Rectangle<float> ((b.getWidth() - sz) * 0.5f, 5.0f, sz, sz);
    const auto c = knobArea.getCentre();
    const float scale = sz / 100.0f;
    const float v = param->getValue();
    const float a0 = juce::degreesToRadians (-135.0f), a1 = a0 + juce::degreesToRadians (270.0f) * v;

    juce::Path trackPath, valuePath;
    trackPath.addCentredArc (c.x, c.y, 40 * scale, 40 * scale, 0.0f, a0, juce::degreesToRadians (135.0f), true);
    valuePath.addCentredArc (c.x, c.y, 40 * scale, 40 * scale, 0.0f, a0, a1, true);
    const juce::PathStrokeType stroke (7.0f * scale * (sz < 56 ? 1.15f : 1.0f), juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    g.setColour (ui::track);  g.strokePath (trackPath, stroke);
    if (v > 0.002f) { g.setColour (colour); g.strokePath (valuePath, stroke); }

    g.setColour (ui::body);
    g.fillEllipse (juce::Rectangle<float> (58 * scale, 58 * scale).withCentre (c));
    g.setColour (ui::bodyLine);
    g.drawEllipse (juce::Rectangle<float> (58 * scale, 58 * scale).withCentre (c), 2.0f * scale);

    const juce::Point<float> tip (c.x + 22 * scale * std::sin (a1), c.y - 22 * scale * std::cos (a1));
    g.setColour (ui::pointer);
    g.drawLine ({ c, tip }, 4.0f * scale * (sz < 56 ? 1.2f : 1.0f));

    // Name (with a dot when its step lane is active) and value.
    const bool active = laneId.isNotEmpty() && editor.laneActive (laneId);
    const auto valueText = param->getCurrentValueAsText() + (param->getLabel().isNotEmpty() && ! param->getLabel().startsWith ("%") ? " " + param->getLabel() : param->getLabel());
    const auto nameFont = ui::mono (row ? 11.5f : (sz < 56 ? 11.0f : 12.0f)).withExtraKerningFactor (row ? 0.04f : 0.08f);
    const float nameW = juce::GlyphArrangement::getStringWidth (nameFont, label);

    if (row)
    {
        float x = knobArea.getRight() + 8.0f;
        if (active) { g.setColour (ui::accent); g.fillEllipse (x, b.getCentreY() - 3.0f, 6.0f, 6.0f); x += 11.0f; }
        g.setColour (ui::text); g.setFont (nameFont);
        g.drawText (label, juce::Rectangle<float> (x, 0, nameW + 4, b.getHeight()), juce::Justification::centredLeft);
        spaced (g, valueText, juce::Rectangle<float> (x + nameW + 5, 0, b.getRight() - x - nameW - 5, b.getHeight()), 10.5f, 0.0f, ui::muted);
    }
    else
    {
        const float y = knobArea.getBottom() + 6.0f;
        const float total = nameW + (active ? 11.0f : 0.0f);
        float x = (b.getWidth() - total) * 0.5f;
        if (active) { g.setColour (ui::accent); g.fillEllipse (x, y + 5.0f, 6.0f, 6.0f); x += 11.0f; }
        g.setColour (ui::text); g.setFont (nameFont);
        g.drawText (label, juce::Rectangle<float> (x, y, nameW + 4, 16), juce::Justification::centredLeft);
        spaced (g, valueText, juce::Rectangle<float> (0, y + 18, b.getWidth(), 14), 11.0f, 0.0f, ui::muted, juce::Justification::centred);
    }
}

void Knob::mouseDown (const juce::MouseEvent&) { dragStart = param->getValue(); dragging = false; }

void Knob::mouseDrag (const juce::MouseEvent& e)
{
    if (! dragging && e.getDistanceFromDragStart() < 3) return;
    if (! dragging) { dragging = true; attachment->beginGesture(); }
    const float sens = e.mods.isShiftDown() ? 800.0f : 200.0f;
    const float v = juce::jlimit (0.0f, 1.0f, dragStart - (float) e.getDistanceFromDragStartY() / sens);
    attachment->setValueAsPartOfGesture (param->convertFrom0to1 (v));
}

void Knob::mouseUp (const juce::MouseEvent&)
{
    if (dragging) attachment->endGesture();
    else if (laneId.isNotEmpty()) editor.selectLane (laneId);     // a click (no drag) opens the knob's lane
    dragging = false;
}

void Knob::mouseDoubleClick (const juce::MouseEvent&)
{
    attachment->setValueAsCompleteGesture (param->convertFrom0to1 (param->getDefaultValue()));
}

void Knob::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    const float v = juce::jlimit (0.0f, 1.0f, param->getValue() + w.deltaY * 0.05f);
    attachment->setValueAsCompleteGesture (param->convertFrom0to1 (v));
}

void Knob::mouseEnter (const juce::MouseEvent&) { editor.showInfo (param->getParameterID() == "ddiv" || param->getParameterID() == "dms" ? "dtime" : param->getParameterID()); }
void Knob::mouseExit (const juce::MouseEvent&)  { editor.showInfo ({}); }

// ===========================================================================
// SegButtons
// ===========================================================================
SegButtons::SegButtons (AcidoEditor& ed, juce::StringArray l, std::function<int()> g, std::function<void (int)> s, float f, const juce::String& i)
    : editor (ed), labels (std::move (l)), get (std::move (g)), set (std::move (s)), font (f), info (i) {}

std::unique_ptr<SegButtons> SegButtons::forParameter (AcidoEditor& ed, const juce::String& paramId, juce::StringArray labels, float fontSize)
{
    auto* prm = ed.proc.apvts.getParameter (paramId);
    auto seg = std::make_unique<SegButtons> (ed, labels,
        [prm] { return (int) std::lround (prm->convertFrom0to1 (prm->getValue())); },
        nullptr, fontSize, paramId);
    auto* raw = seg.get();
    seg->attachment = std::make_unique<juce::ParameterAttachment> (*prm, [raw] (float) { raw->repaint(); });
    seg->set = [raw] (int i) { raw->attachment->setValueAsCompleteGesture ((float) i); };
    return seg;
}

juce::Rectangle<float> SegButtons::cell (int i) const
{
    const int n = labels.size();
    const int cols = columns > 0 ? columns : n;
    const int rows = (n + cols - 1) / cols;
    const float gap = 6.0f;
    const float w = ((float) getWidth() - gap * (float) (cols - 1)) / (float) cols;
    const float h = ((float) getHeight() - gap * (float) (rows - 1)) / (float) rows;
    return { (float) (i % cols) * (w + gap), (float) (i / cols) * (h + gap), w, h };
}

void SegButtons::paint (juce::Graphics& g)
{
    const int current = get ? get() : -1;
    for (int i = 0; i < labels.size(); ++i)
    {
        auto r = cell (i).reduced (0.5f);
        const bool on = i == current;
        g.setColour (on ? ui::accent : ui::button);
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (on ? ui::accent : ui::line);
        g.drawRoundedRectangle (r, 6.0f, 1.0f);
        const auto tc = on ? ui::bg : ui::soft;

        if (i < (int) icons.size() && ! icons[(size_t) i].isEmpty())
        {
            const float textW = juce::GlyphArrangement::getStringWidth (ui::mono (font, on).withExtraKerningFactor (0.08f), labels[i]);
            const float total = 24.0f + 8.0f + textW;
            const float x = r.getCentreX() - total * 0.5f;
            auto p = icons[(size_t) i];
            p.applyTransform (juce::AffineTransform::scale (24.0f / 28.0f).translated (x, r.getCentreY() - 7.0f));
            g.setColour (tc);
            g.strokePath (p, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            spaced (g, labels[i], { x + 32.0f, r.getY(), textW + 4.0f, r.getHeight() }, font, 0.08f, tc, juce::Justification::centredLeft, on);
        }
        else
            spaced (g, labels[i], r, font, 0.06f, tc, juce::Justification::centred, on);
    }
}

void SegButtons::mouseDown (const juce::MouseEvent& e)
{
    for (int i = 0; i < labels.size(); ++i)
        if (cell (i).contains (e.position)) { if (set) set (i); repaint(); return; }
}
void SegButtons::mouseEnter (const juce::MouseEvent&) { if (info.isNotEmpty()) editor.showInfo (info); }
void SegButtons::mouseExit (const juce::MouseEvent&)  { if (info.isNotEmpty()) editor.showInfo ({}); }

// ===========================================================================
// Stepper
// ===========================================================================
Stepper::Stepper (AcidoEditor& ed, std::function<juce::String()> t, std::function<void (int)> s, std::function<bool()> en, const juce::String& i)
    : editor (ed), text (std::move (t)), step (std::move (s)), enabled (std::move (en)), info (i) {}

void Stepper::paint (juce::Graphics& g)
{
    const bool on = ! enabled || enabled();
    const float h = (float) getHeight(), w = (float) getWidth();
    for (int side = 0; side < 2; ++side)
    {
        auto r = juce::Rectangle<float> (side == 0 ? 0.0f : w - h, 0.0f, h, h).reduced (0.5f);
        g.setColour (ui::button.withAlpha (on ? 1.0f : 0.35f));
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (ui::line.withAlpha (on ? 1.0f : 0.35f));
        g.drawRoundedRectangle (r, 6.0f, 1.0f);
        spaced (g, side == 0 ? juce::String (juce::CharPointer_UTF8 ("\xe2\x88\x92")) : "+", r, 14.0f, 0.0f, ui::soft.withAlpha (on ? 1.0f : 0.35f), juce::Justification::centred);
    }
    spaced (g, text(), { h, 0.0f, w - 2 * h, h }, 12.0f, 0.06f, on ? textColour : ui::faint, juce::Justification::centred);
}

void Stepper::mouseDown (const juce::MouseEvent& e)
{
    if (enabled && ! enabled()) return;
    if (e.position.x < (float) getHeight()) step (-1);
    else if (e.position.x > (float) (getWidth() - getHeight())) step (1);
    repaint();
}
void Stepper::mouseEnter (const juce::MouseEvent&) { if (info.isNotEmpty()) editor.showInfo (info); }
void Stepper::mouseExit (const juce::MouseEvent&)  { if (info.isNotEmpty()) editor.showInfo ({}); }

// ===========================================================================
// MacroSlider (Neblina)
// ===========================================================================
MacroSlider::MacroSlider (AcidoEditor& ed) : editor (ed), param (ed.proc.apvts.getParameter ("neblina"))
{
    attachment = std::make_unique<juce::ParameterAttachment> (*param, [this] (float) { repaint(); });
    attachment->sendInitialUpdate();
}

juce::Rectangle<float> MacroSlider::trackArea() const { return getLocalBounds().toFloat().withTrimmedTop (22).withHeight (32); }

void MacroSlider::paint (juce::Graphics& g)
{
    const bool selected = editor.selectedLane() == "neblina";
    const bool active = editor.laneActive ("neblina");
    const float v = param->getValue();

    float x = 0.0f;
    if (active) { g.setColour (ui::accent); g.fillEllipse (0.0f, 5.0f, 6.0f, 6.0f); x = 11.0f; }
    spaced (g, "NEBLINA", { x, 0, 120, 16 }, 12.0f, 0.12f, ui::text);
    spaced (g, juce::String (juce::roundToInt (v * 100.0f)) + "%", { 0, 0, (float) getWidth(), 16 }, 11.0f, 0.0f, ui::muted, juce::Justification::centredRight);

    auto t = trackArea();
    if (selected)
    {
        g.setColour (ui::body); g.fillRoundedRectangle (t, 8.0f);
        g.setColour (ui::accent); g.drawRoundedRectangle (t.reduced (0.5f), 8.0f, 1.0f);
    }
    auto bar = t.reduced (10.0f, 0.0f).withSizeKeepingCentre (t.getWidth() - 20.0f, 8.0f);
    g.setColour (ui::track); g.fillRoundedRectangle (bar, 4.0f);
    g.setColour (ui::accent); g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * v), 4.0f);
    auto thumb = juce::Rectangle<float> (20, 20).withCentre ({ bar.getX() + bar.getWidth() * v, bar.getCentreY() });
    g.setColour (ui::pointer); g.fillEllipse (thumb);
    g.setColour (ui::accent); g.drawEllipse (thumb.reduced (1.5f), 3.0f);

    g.setColour (ui::faint);
    g.setFont (ui::mono (10.0f).withExtraKerningFactor (0.04f));
    g.drawFittedText (juce::String (juce::CharPointer_UTF8 ("unison drift \xc2\xb7 random resonances \xc2\xb7 phaser")),
                      juce::Rectangle<float> (0, t.getBottom() + 6.0f, (float) getWidth(), 30).toNearestInt(), juce::Justification::topLeft, 2, 1.0f);
}

void MacroSlider::setFromX (float x)
{
    auto bar = trackArea().reduced (10.0f, 0.0f);
    attachment->setValueAsPartOfGesture (param->convertFrom0to1 (juce::jlimit (0.0f, 1.0f, (x - bar.getX()) / bar.getWidth())));
}
void MacroSlider::mouseDown (const juce::MouseEvent& e) { downX = e.position.x; dragging = false; }
void MacroSlider::mouseDrag (const juce::MouseEvent& e)
{
    if (! dragging && e.getDistanceFromDragStart() < 3) return;
    if (! dragging) { dragging = true; attachment->beginGesture(); }
    setFromX (e.position.x);
}
void MacroSlider::mouseUp (const juce::MouseEvent& e)
{
    if (dragging) attachment->endGesture();
    else
    {
        editor.selectLane ("neblina");
        if (trackArea().contains (e.position)) { attachment->beginGesture(); setFromX (e.position.x); attachment->endGesture(); }
    }
    dragging = false;
}
void MacroSlider::mouseDoubleClick (const juce::MouseEvent&) { attachment->setValueAsCompleteGesture (param->convertFrom0to1 (param->getDefaultValue())); }
void MacroSlider::mouseEnter (const juce::MouseEvent&) { editor.showInfo ("neblina"); }
void MacroSlider::mouseExit (const juce::MouseEvent&)  { editor.showInfo ({}); }

// ===========================================================================
// EnvelopeDisplay — same curves as the mockup
// ===========================================================================
void EnvelopeDisplay::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (ui::display); g.fillRoundedRectangle (b, 6.0f);
    g.setColour (ui::border);  g.drawRoundedRectangle (b.reduced (0.5f), 6.0f, 1.0f);

    auto val = [this] (const char* id) { return proc.apvts.getParameter (id)->getValue(); };
    const float cutoff = val ("cutoff"), envmod = val ("envmod"), decay = val ("decay"), accent = val ("accent");
    const float W = b.getWidth(), H = b.getHeight();
    auto mapY = [H] (float y96) { return y96 / 96.0f * H; };

    g.setColour (juce::Colour (0xff22201a));
    for (float y : { 24.0f, 48.0f, 72.0f }) g.drawHorizontalLine ((int) mapY (y), b.getX() + 1, b.getRight() - 1);

    const float base = 84.0f - cutoff * 40.0f;
    auto curve = [&] (float peak, float k)
    {
        juce::Path p;
        p.startNewSubPath (0, mapY (base));
        p.lineTo (W * 14.0f / 360.0f, mapY (base));
        for (int i = 0; i <= 60; ++i)
        {
            const float t = (float) i / 60.0f;
            p.lineTo (W * (16.0f + t * 344.0f) / 360.0f, mapY (base - (base - peak) * std::exp (-t * k)));
        }
        return p;
    };
    const float peak = base - envmod * 56.0f;
    const float d = std::max (decay, 0.05f);
    auto normal = curve (peak, 1.8f / d);
    auto acc = curve (std::max (peak - 18.0f * (0.4f + accent), 6.0f), 3.4f / d);

    float dash[] = { 4.0f, 4.0f };
    juce::Path baseLine; baseLine.startNewSubPath (0, mapY (base)); baseLine.lineTo (W, mapY (base));
    juce::Path dashed; juce::PathStrokeType (1.0f).createDashedStroke (dashed, baseLine, dash, 2);
    g.setColour (ui::dim); g.fillPath (dashed);

    juce::Path fill (normal); fill.lineTo (W, H); fill.lineTo (0, H); fill.closeSubPath();
    g.setColour (ui::accent.withAlpha (0.08f)); g.fillPath (fill);
    g.setColour (ui::accent2); g.strokePath (acc, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved));
    g.setColour (ui::accent);  g.strokePath (normal, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved));
}

// ===========================================================================
// StepGrid
// ===========================================================================
void StepGrid::paint (juce::Graphics& g)
{
    auto* lane = editor.currentLane();
    const float W = (float) getWidth(), gridH = 124.0f, gap = 4.0f;
    const float colW = (W - gap * 15.0f) / 16.0f;
    const float mid = 62.0f;
    const int len = lane ? juce::jlimit (1, acido::kSteps, lane->length.load()) : 16;
    const int play = lane && editor.proc.hostPlaying.load() ? lane->playStep.load() : -1;
    const bool smooth = lane && lane->smooth.load();

    for (int i = 0; i < acido::kSteps; ++i)
    {
        const bool in = i < len;
        auto col = juce::Rectangle<float> ((colW + gap) * (float) i, 0.0f, colW, gridH);
        g.setColour (in ? (i == play ? juce::Colour (0xff2e2b22) : juce::Colour (0xff1a1913)) : juce::Colour (0xff141309));
        g.fillRoundedRectangle (col, 4.0f);
        if (i == play) { g.setColour (ui::dim); g.drawRoundedRectangle (col.reduced (0.5f), 4.0f, 1.0f); }
        g.setColour (ui::line); g.fillRect (col.getX(), mid, colW, 1.0f);

        const float v = lane ? lane->steps[(size_t) i].load() : 0.0f;
        const float h = std::abs (v) * 56.0f;
        auto bar = juce::Rectangle<float> (col.getX() + 4.0f, v >= 0 ? mid - h : mid, colW - 8.0f, h);
        g.setColour ((in ? ui::accent : ui::line).withAlpha (smooth && in ? 0.35f : 1.0f));
        g.fillRoundedRectangle (bar, 2.0f);

        const auto numColour = ! in ? juce::Colour (0xff4a4638) : (i % 4 == 0 ? ui::soft : ui::faint);
        spaced (g, juce::String (i + 1), { col.getX(), gridH + 6.0f, colW, 14.0f }, 11.0f, 0.0f, numColour, juce::Justification::centred);
    }

    if (lane && smooth && len > 0)
    {
        juce::Path p;
        auto pt = [&] (int i) { return juce::Point<float> ((colW + gap) * (float) i + colW * 0.5f, mid - lane->steps[(size_t) (i % len)].load() * 56.0f); };
        p.startNewSubPath (pt (0));
        for (int i = 0; i < len - 1; ++i)
        {
            const auto a = pt (i), c = pt (i + 1);
            p.quadraticTo (a, (a + c) * 0.5f);
        }
        p.lineTo (pt (len - 1));
        g.setColour (ui::accent);
        g.strokePath (p, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
}

void StepGrid::edit (const juce::MouseEvent& e)
{
    auto* lane = editor.currentLane();
    if (lane == nullptr) return;
    const float W = (float) getWidth(), colW = (W - 60.0f) / 16.0f;
    const int i = juce::jlimit (0, acido::kSteps - 1, (int) (e.position.x / (colW + 4.0f)));
    float v = juce::jlimit (-1.0f, 1.0f, (62.0f - e.position.y) / 56.0f);
    if (std::abs (v) < 0.06f || e.mods.isRightButtonDown() || e.mods.isAltDown()) v = 0.0f;   // right-click / Option-click clears
    lane->steps[(size_t) i] = v;
    repaint();
}

// ===========================================================================
// FlatButton
// ===========================================================================
void FlatButton::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    const float a = enabled ? 1.0f : 0.35f;
    const auto fill = style == Filled ? ui::accent : (style == Outline ? ui::panel : ui::button);
    g.setColour ((hover && enabled && style != Filled ? fill.brighter (0.08f) : fill).withMultipliedAlpha (a));
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour ((style == Normal ? ui::line : ui::accent).withMultipliedAlpha (a));
    g.drawRoundedRectangle (r, 6.0f, 1.0f);
    const auto tc = style == Filled ? ui::bg : (style == Outline ? ui::accent : ui::soft);

    if (customPaint) { customPaint (g, r); return; }
    if (! icon.isEmpty())
    {
        auto p = icon;
        const float textW = text.isEmpty() ? 0.0f : juce::GlyphArrangement::getStringWidth (ui::mono (fontSize).withExtraKerningFactor (0.1f), text) + 8.0f;
        const float x0 = r.getCentreX() - (16.0f + textW) * 0.5f;
        p.applyTransform (juce::AffineTransform::translation (x0, r.getCentreY() - 8.0f));
        g.setColour (tc.withMultipliedAlpha (a));
        g.strokePath (p, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        if (text.isNotEmpty()) spaced (g, text, { x0 + 24.0f, r.getY(), textW, r.getHeight() }, fontSize, 0.1f, tc.withMultipliedAlpha (a));
    }
    else
        spaced (g, text, r, fontSize, 0.08f, tc.withMultipliedAlpha (a), juce::Justification::centred, style == Filled);
}

// ===========================================================================
// Root — panels, headings and everything painted directly
// ===========================================================================
void Root::paint (juce::Graphics& g) { editor.paintRoot (g); }

void AcidoEditor::paintRoot (juce::Graphics& g)
{
    g.fillAll (ui::bg);

    // Header
    g.setColour (ui::accent);
    g.setFont (ui::title (38.0f).withExtraKerningFactor (0.04f));
    const auto logo = juce::String (juce::CharPointer_UTF8 ("\xc3\x81" "CIDO"));
    g.drawText (logo, juce::Rectangle<float> (24, 20, 200, 56), juce::Justification::centredLeft);
    const float logoW = juce::GlyphArrangement::getStringWidth (ui::title (38.0f).withExtraKerningFactor (0.04f), logo);
    spaced (g, "ACID BASS SYNTH", { 24 + logoW + 14, 41, 220, 20 }, 12.0f, 0.18f, ui::muted);

    {   // Ableton sync badge
        auto r = L::sync.reduced (0.5f);
        g.setColour (ui::border); g.drawRoundedRectangle (r, 8.0f, 1.0f);
        const bool playing = proc.hostPlaying.load();
        g.setColour (playing ? ui::accent : ui::dim);
        g.fillEllipse (r.getX() + 12, r.getCentreY() - 4, 8, 8);
        spaced (g, "SYNC", { r.getX() + 28, r.getY(), 44, r.getHeight() }, 12.0f, 0.1f, ui::soft);
        spaced (g, juce::String (proc.hostBpm.load(), 2) + " BPM", { r.getX() + 68, r.getY(), 80, r.getHeight() }, 12.0f, 0.0f, ui::muted);
    }

    // Panels
    for (auto r : { L::osc, L::filter, L::voice, L::drive, L::chorus, L::delay, L::reverb, L::output, L::step }) panelBox (g, r);
    panelBox (g, L::info, ui::display);

    auto heading = [&g] (const juce::String& t, juce::Rectangle<float> panel, float padX = 20.0f)
    { spaced (g, t, { panel.getX() + padX, panel.getY() + 14, panel.getWidth() - 2 * padX, 16 }, 11.0f, 0.16f, ui::muted); };
    const auto dot = juce::String (juce::CharPointer_UTF8 (" \xc2\xb7 "));
    heading ("OSC" + dot + "SUB", L::osc);
    heading ("FILTER" + dot + "24 dB LOW-PASS", L::filter);
    spaced (g, juce::String (juce::CharPointer_UTF8 ("\xe2\x80\x94 ENV")), { 700, 110, 60, 16 }, 11.0f, 0.0f, ui::accent);
    spaced (g, juce::String (juce::CharPointer_UTF8 ("\xe2\x80\x94 ACCENT")), { 758, 110, 80, 16 }, 11.0f, 0.0f, ui::accent2);
    heading ("VOICE" + dot + "PLAY", L::voice, 16.0f);
    heading ("NOISE" + dot + "DRIVE", L::drive, 14.0f);
    heading ("CHORUS" + dot + "UNISON", L::chorus, 14.0f);
    heading ("DELAY", L::delay, 14.0f);
    heading ("REVERB", L::reverb, 14.0f);
    heading ("OUTPUT", L::output, 14.0f);

    // Signal-flow arrows between effect panels
    for (float x : L::arrowsX)
    {
        juce::Path p; p.startNewSubPath (x + 3, 728); p.lineTo (x + 7, 734); p.lineTo (x + 3, 740);
        g.setColour (ui::dim); g.strokePath (p, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Info panel
    {
        auto r = L::info.reduced (20.0f);
        spaced (g, "INFO", r.removeFromTop (16), 11.0f, 0.16f, ui::muted);
        const auto t = infoFor (infoId, (int) std::lround (proc.apvts.getRawParameterValue ("wave")->load()));
        spaced (g, juce::String (juce::CharPointer_UTF8 (t.section)), L::info.reduced (20.0f).withHeight (16), 10.0f, 0.12f, ui::faint, juce::Justification::centredRight);
        r.removeFromTop (8);
        const bool empty = infoId.isEmpty();
        juce::String title = empty ? juce::String ("Hover a control") : juce::String();
        if (! empty)
        {
            title = infoId == "neblina" ? "NEBLINA" : infoId == "dtime" ? "TIME" : infoId.toUpperCase();
            static const std::map<juce::String, juce::String> names {
                { "tune", "TUNE" }, { "shape", "SHAPE" }, { "sub", "SUB" }, { "suboct", "SUB OCTAVE" }, { "wave", "WAVEFORM" },
                { "cutoff", "CUTOFF" }, { "reso", "RESO" }, { "envmod", "ENV MOD" }, { "decay", "DECAY" }, { "ffm", "FILTER FM" },
                { "accent", "ACCENT" }, { "slide", "SLIDE" }, { "drift", "DRIFT" }, { "velo", "VELO" }, { "mode", "MONO / POLY" },
                { "voices", "VOICES" }, { "noise", "NOISE" }, { "drive", "DRIVE" }, { "warmth", "WARMTH" }, { "unison", "UNISON" },
                { "ctone", "CHORUS TONE" }, { "cmix", "CHORUS DRY/WET" }, { "dsync", "SYNC / MS" }, { "dfb", "FEEDBACK" },
                { "dmix", "DELAY DRY/WET" }, { "rtype", "REVERB TYPE" }, { "rsize", "SIZE" }, { "rmix", "REVERB SEND" },
                { "crush", "CRUSH" }, { "comp", "COMP" }, { "vol", "VOLUME" } };
            if (auto it = names.find (infoId); it != names.end()) title = it->second;
        }
        g.setColour (infoId == "accent" ? ui::accent2 : (empty ? ui::text : ui::accent));
        g.setFont (ui::title (22.0f, false).withExtraKerningFactor (0.04f));
        g.drawText (title, r.removeFromTop (30), juce::Justification::centredLeft);
        r.removeFromTop (6);

        juce::AttributedString body;
        body.append (empty ? juce::String ("Point at any knob, switch or the Neblina slider to see what it does to the sound.")
                           : juce::String (juce::CharPointer_UTF8 (t.text)), ui::mono (12.0f), ui::soft);
        body.setLineSpacing (4.0f);
        body.setWordWrap (juce::AttributedString::byWord);
        body.draw (g, r.removeFromTop (160));

        if (! empty)
        {
            const bool mod = proc.laneIndexFor (infoId.toStdString()) >= 0;
            const bool isSwitch = juce::StringArray { "wave", "suboct", "mode", "voices", "unison", "dsync", "rtype" }.contains (infoId);
            auto foot = L::info.reduced (20.0f).removeFromBottom (28);
            if (! isSwitch)
            {
                g.setColour (mod ? ui::accent : ui::dim);
                g.fillEllipse (foot.getX(), foot.getY() + 4, 6, 6);
                g.setColour (ui::muted);
                g.setFont (ui::mono (10.5f));
                g.drawFittedText (mod ? "Click to edit its step mod lane" : "Not a step mod target (it would bend the pitch)",
                                  foot.withTrimmedLeft (12).toNearestInt(), juce::Justification::topLeft, 2, 1.0f);
            }
        }
    }

    // Step modulator header and labels
    {
        auto r = L::step.reduced (20.0f);
        spaced (g, "STEP MOD", { r.getX(), r.getY(), 80, 32 }, 11.0f, 0.16f, ui::muted);
        g.setColour (ui::accent);
        g.setFont (ui::mono (16.0f, true).withExtraKerningFactor (0.08f));
        const auto target = juce::String (juce::CharPointer_UTF8 ("\xe2\x86\x92 ")) + selectedLabel();
        g.drawText (target, juce::Rectangle<float> (r.getX() + 88, r.getY(), 220, 32), juce::Justification::centredLeft);
        const float tw = juce::GlyphArrangement::getStringWidth (ui::mono (16.0f, true).withExtraKerningFactor (0.08f), target);
        spaced (g, "click any knob to edit its lane", { r.getX() + 100 + tw, r.getY(), 260, 32 }, 11.0f, 0.0f, ui::muted);
        spaced (g, "FILL", { 764, r.getY(), 40, 32 }, 11.0f, 0.1f, ui::muted);

        const char* names[4] = { "RATE", "LENGTH", "MODE", "DEPTH" };
        for (int i = 0; i < 4; ++i) spaced (g, names[i], { 44, 926.0f + 40.0f * (float) i, 60, 32 }, 11.0f, 0.1f, ui::muted);
    }

    // Footer legend
    {
        float x = 24.0f;
        auto item = [&] (juce::Colour c, const juce::String& t)
        {
            g.setColour (c); g.fillEllipse (x, 1122, 7, 7);
            spaced (g, t, { x + 12, 1116, 400, 20 }, 11.0f, 0.0f, ui::muted);
            x += 24.0f + juce::GlyphArrangement::getStringWidth (ui::mono (11.0f), t);
        };
        item (ui::accent2, juce::String (juce::CharPointer_UTF8 ("ACCENT \xe2\x80\x94 note velocity \xe2\x89\xa5 100")));
        item (ui::accent, proc.apvts.getRawParameterValue ("mode")->load() > 0.5f
                             ? juce::String (juce::CharPointer_UTF8 ("SLIDE (poly) \xe2\x80\x94 each new note glides from the last one"))
                             : juce::String (juce::CharPointer_UTF8 ("SLIDE \xe2\x80\x94 overlapping notes glide")));
        item (ui::accent, juce::String (juce::CharPointer_UTF8 ("dot on a knob \xe2\x80\x94 its step lane is active")));
    }
}

void AcidoEditor::SavePanel::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    juce::DropShadow (juce::Colours::black.withAlpha (0.55f), 24, { 0, 10 }).drawForRectangle (g, getLocalBounds());
    g.setColour (ui::popup); g.fillRoundedRectangle (r, 10.0f);
    g.setColour (ui::line);  g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.0f);
    spaced (g, "SAVE PRESET", { 18, 14, 200, 16 }, 11.0f, 0.16f, ui::muted);
    spaced (g, "NAME", { 18, 38, 100, 16 }, 11.0f, 0.1f, ui::soft);
    spaced (g, "Stores every knob, switch, voice mode and step lane.", { 18, 104, 310, 20 }, 10.5f, 0.0f, ui::muted);
}

// ===========================================================================
// AcidoEditor
// ===========================================================================
namespace
{
    juce::Path wavePath (int w)
    {
        juce::Path p;
        switch (w)
        {
            case 0: p.startNewSubPath (2, 13); p.lineTo (12, 3); p.lineTo (12, 13); p.lineTo (24, 3); p.lineTo (24, 13); break;
            case 1: p.startNewSubPath (2, 13); p.lineTo (2, 3); p.lineTo (9, 3); p.lineTo (9, 13); p.lineTo (16, 13);
                    p.lineTo (16, 3); p.lineTo (23, 3); p.lineTo (23, 13); p.lineTo (26, 13); break;
            case 2: p.startNewSubPath (2, 8); p.quadraticTo (8, -3, 14, 8); p.quadraticTo (20, 19, 26, 8); break;
            default: p.startNewSubPath (2, 8); p.quadraticTo (4, 1, 6, 8); p.quadraticTo (8, 15, 10, 8);
                     p.quadraticTo (13, -1, 16, 8); p.quadraticTo (19, 17, 22, 8); p.quadraticTo (24, 3, 26, 8); break;
        }
        return p;
    }
}

AcidoEditor::AcidoEditor (AcidoProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    laf.setDefaultSansSerifTypeface (plexRegular());
    laf.setColour (juce::PopupMenu::backgroundColourId, ui::popup);
    laf.setColour (juce::PopupMenu::textColourId, ui::text);
    laf.setColour (juce::PopupMenu::headerTextColourId, ui::muted);
    laf.setColour (juce::PopupMenu::highlightedBackgroundColourId, ui::track);
    laf.setColour (juce::PopupMenu::highlightedTextColourId, ui::accent);
    laf.setColour (juce::TextEditor::backgroundColourId, ui::bg);
    laf.setColour (juce::TextEditor::outlineColourId, ui::dim);
    laf.setColour (juce::TextEditor::focusedOutlineColourId, ui::accent);
    laf.setColour (juce::TextEditor::textColourId, ui::text);
    laf.setColour (juce::CaretComponent::caretColourId, ui::accent);
    setLookAndFeel (&laf);

    addAndMakeVisible (root);
    auto add = [this] (juce::Component& c) { root.addAndMakeVisible (c); };
    auto knob = [this, &add] (std::unique_ptr<Knob>& k, const char* id, const char* label, int size, juce::Colour c = ui::accent, bool rowStyle = false)
    {
        k = std::make_unique<Knob> (*this, id, label, size, c, rowStyle);
        add (*k); allKnobs.push_back (k.get());
    };

    // ---- Header ----
    prevButton.icon = chevron (false); nextButton.icon = chevron (true);
    prevButton.onClick = [this] { proc.presets.previous(); refreshAfterPreset(); };
    nextButton.onClick = [this] { proc.presets.next();     refreshAfterPreset(); };
    presetButton.onClick = [this] { showPresetMenu(); };
    presetButton.customPaint = [this] (juce::Graphics& g, juce::Rectangle<float> r)
    {
        const int number = proc.presets.getCurrentIndex() + 1;
        spaced (g, (number < 10 ? "0" : "") + juce::String (number), r.withTrimmedLeft (14).withWidth (26), 14.0f, 0.0f, ui::muted);
        const bool dirty = proc.presets.isModified();
        spaced (g, proc.presets.getCurrentName(), r.withTrimmedLeft (44).withTrimmedRight (dirty ? 44 : 30), 14.0f, 0.0f, ui::text);
        if (dirty) { g.setColour (ui::accent2); g.fillEllipse (r.getRight() - 38, r.getCentreY() - 3.5f, 7, 7); }
        juce::Path v; v.startNewSubPath (r.getRight() - 22, r.getCentreY() - 2); v.lineTo (r.getRight() - 18, r.getCentreY() + 2); v.lineTo (r.getRight() - 14, r.getCentreY() - 2);
        g.setColour (ui::text); g.strokePath (v, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    };
    {
        juce::Path disk;
        disk.startNewSubPath (2.5f, 2.5f); disk.lineTo (11, 2.5f); disk.lineTo (13.5f, 5); disk.lineTo (13.5f, 13.5f); disk.lineTo (2.5f, 13.5f); disk.closeSubPath();
        disk.startNewSubPath (5, 2.5f); disk.lineTo (5, 6); disk.lineTo (10, 6); disk.lineTo (10, 2.5f);
        disk.startNewSubPath (5, 13.5f); disk.lineTo (5, 9.5f); disk.lineTo (11, 9.5f); disk.lineTo (11, 13.5f);
        saveButton.icon = disk;
        saveButton.fontSize = 13.0f;
    }
    saveButton.onClick = [this] { openSave (! savePanel.isVisible()); };
    for (auto* c : { &prevButton, &nextButton, &presetButton, &saveButton }) add (*c);

    // ---- OSC ----
    waves = SegButtons::forParameter (*this, "wave", { "SAW", "SQR", "SINE", "FM" }, 12.0f);
    waves->columns = 2;
    for (int i = 0; i < 4; ++i) waves->icons.push_back (wavePath (i));
    add (*waves);
    knob (tune, "tune", "TUNE", 64);
    knob (shape, "shape", "SHAPE", 64);
    knob (sub, "sub", "SUB", 64);
    subOct = SegButtons::forParameter (*this, "suboct", { juce::String (juce::CharPointer_UTF8 ("\xe2\x88\x92" "1 OCT")), juce::String (juce::CharPointer_UTF8 ("\xe2\x88\x92" "2 OCT")) }, 11.0f);
    subOct->columns = 1;
    add (*subOct);
    neblina = std::make_unique<MacroSlider> (*this); add (*neblina);

    // ---- Filter ----
    add (envelope);
    knob (cutoff, "cutoff", "CUTOFF", 96);
    knob (reso, "reso", "RESO", 64);
    knob (envmod, "envmod", "ENV MOD", 64);
    knob (decay, "decay", "DECAY", 64);
    knob (ffm, "ffm", "FM", 64);

    // ---- Voice ----
    voiceMode = SegButtons::forParameter (*this, "mode", { "MONO", "POLY" }, 12.0f); add (*voiceMode);
    {
        auto* vp = proc.apvts.getParameter ("voices");
        voices = std::make_unique<Stepper> (*this,
            [this] { return proc.apvts.getRawParameterValue ("mode")->load() > 0.5f ? juce::String ((int) proc.apvts.getRawParameterValue ("voices")->load()) : juce::String ("1"); },
            [this] (int d) { voices->attachment->setValueAsCompleteGesture ((float) juce::jlimit (2, 8, (int) proc.apvts.getRawParameterValue ("voices")->load() + d)); },
            [this] { return proc.apvts.getRawParameterValue ("mode")->load() > 0.5f; }, "voices");
        voices->attachment = std::make_unique<juce::ParameterAttachment> (*vp, [this] (float) { voices->repaint(); });
        add (*voices);
    }
    knob (accent, "accent", "ACCENT", 44, ui::accent2);
    knob (slide, "slide", "SLIDE", 44);
    knob (drift, "drift", "DRIFT", 44);
    knob (velo, "velo", "VELO", 44);

    // ---- Effects ----
    knob (noise, "noise", "NOISE", 38, ui::accent, true);
    knob (drive, "drive", "DRIVE", 38, ui::accent, true);
    knob (warmth, "warmth", "WARMTH", 38, ui::accent, true);
    {
        auto* up = proc.apvts.getParameter ("unison");
        unison = std::make_unique<Stepper> (*this,
            [this] { const int u = (int) proc.apvts.getRawParameterValue ("unison")->load(); return u <= 1 ? juce::String ("UNISON OFF") : juce::String (u) + " VOICES"; },
            [this] (int d) { unison->attachment->setValueAsCompleteGesture ((float) juce::jlimit (1, 7, (int) proc.apvts.getRawParameterValue ("unison")->load() + d)); },
            nullptr, "unison");
        unison->attachment = std::make_unique<juce::ParameterAttachment> (*up, [this] (float) { unison->repaint(); });
        add (*unison);
    }
    knob (ctone, "ctone", "TONE", 50);
    knob (cmix, "cmix", "DRY/WET", 50);
    delaySync = SegButtons::forParameter (*this, "dsync", { "SYNC", "MS" }, 11.0f); add (*delaySync);
    knob (dtime, "ddiv", "TIME", 50);
    knob (dfb, "dfb", "FEEDBK", 50);
    knob (dmix, "dmix", "DRY/WET", 50);
    reverbType = SegButtons::forParameter (*this, "rtype", { "ROOM", "HALL", "PLATE", "SPRNG" }, 11.0f); add (*reverbType);
    knob (rsize, "rsize", "SIZE", 50);
    knob (rmix, "rmix", "SEND", 50);
    knob (crush, "crush", "CRUSH", 38, ui::accent, true);
    knob (comp, "comp", "COMP", 38, ui::accent, true);
    knob (vol, "vol", "VOLUME", 38, ui::accent, true);

    // ---- Step modulator ----
    laneRate = std::make_unique<SegButtons> (*this, juce::StringArray { "1/32", "1/16", "1/8", "1/4" },
        [this] { auto* l = currentLane(); return l ? l->rate.load() : 1; },
        [this] (int i) { if (auto* l = currentLane()) l->rate = i; }, 11.0f);
    laneMode = std::make_unique<SegButtons> (*this, juce::StringArray { "STEP", "SMOOTH" },
        [this] { auto* l = currentLane(); return l && l->smooth.load() ? 1 : 0; },
        [this] (int i) { if (auto* l = currentLane()) { l->smooth = i == 1; grid.repaint(); } }, 11.0f);
    laneLength = std::make_unique<Stepper> (*this,
        [this] { auto* l = currentLane(); return juce::String (l ? l->length.load() : 16) + " steps"; },
        [this] (int d) { if (auto* l = currentLane()) { l->length = juce::jlimit (1, acido::kSteps, l->length.load() + d); grid.repaint(); } });
    laneDepth = std::make_unique<Stepper> (*this,
        [this] { auto* l = currentLane(); return juce::String (juce::CharPointer_UTF8 ("\xc2\xb1")) + juce::String (juce::roundToInt ((l ? l->depth.load() : 0.5f) * 100.0f)) + "%"; },
        [this] (int d) { if (auto* l = currentLane()) l->depth = juce::jlimit (0.0f, 1.0f, std::round (l->depth.load() * 10.0f + (float) d) / 10.0f); });
    for (auto* c : std::initializer_list<juce::Component*> { laneRate.get(), laneMode.get(), laneLength.get(), laneDepth.get(), &grid }) add (*c);

    const acido::Lane::Shape shapes[5] = { acido::Lane::Sine, acido::Lane::Triangle, acido::Lane::Saw, acido::Lane::Random, acido::Lane::Clear };
    for (int i = 0; i < 5; ++i)
    {
        fills[i].onClick = [this, sh = shapes[i]]
        {
            if (auto* l = currentLane()) { l->fill (sh, (unsigned) juce::Random::getSystemRandom().nextInt()); grid.repaint(); }
        };
        add (fills[i]);
    }

    // ---- Save panel ----
    savePanel.setVisible (false);
    savePanel.setInterceptsMouseClicks (false, true);
    nameBox.setFont (ui::mono (14.0f));
    nameBox.setIndents (12, 11);
    nameBox.onReturnKey = [this] { saveNewButton.onClick(); };
    nameBox.onEscapeKey = [this] { openSave (false); };
    cancelButton.onClick = [this] { openSave (false); };
    overwriteButton.onClick = [this] { if (proc.presets.overwrite()) { openSave (false); refreshAfterPreset(); } };
    saveNewButton.onClick = [this] { if (proc.presets.saveAsNew (nameBox.getText())) { openSave (false); refreshAfterPreset(); } };
    for (auto* c : std::initializer_list<juce::Component*> { &nameBox, &cancelButton, &overwriteButton, &saveNewButton }) savePanel.addAndMakeVisible (*c);
    root.addChildComponent (savePanel);

    layout();
    refreshAfterPreset();

    // Window size: 60–125% of the design, keeping its shape.
    setResizable (true, true);
    setResizeLimits ((int) (kWidth * 0.6f), (int) (kHeight * 0.6f), (int) (kWidth * 1.25f), (int) (kHeight * 1.25f));
    getConstrainer()->setFixedAspectRatio ((double) kWidth / (double) kHeight);
    setSize ((int) (kWidth * proc.uiScale), (int) (kHeight * proc.uiScale));
    startTimerHz (30);
}

AcidoEditor::~AcidoEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void AcidoEditor::resized()
{
    const float s = (float) getWidth() / (float) kWidth;
    proc.uiScale = s;
    root.setBounds (0, 0, kWidth, kHeight);
    root.setTransform (juce::AffineTransform::scale (s));
}

void AcidoEditor::layout()
{
    auto kb = [] (Knob& k, float x, float y, float w, float h) { k.setBounds (juce::Rectangle<float> (x, y, w, h).toNearestInt()); };

    // Header (right to left)
    saveButton.setBounds (1010, 30, 96, 44);
    nextButton.setBounds (958, 30, 44, 44);
    presetButton.setBounds (730, 30, 220, 44);
    prevButton.setBounds (678, 30, 44, 44);
    savePanel.setBounds (766, 84, 340, 190);
    nameBox.setBounds (18, 58, 304, 40);
    cancelButton.setBounds (18, 132, 92, 40);
    overwriteButton.setBounds (116, 132, 92, 40);
    saveNewButton.setBounds (214, 132, 108, 40);

    // OSC
    waves->setBounds (44, 138, 160, 82);
    kb (*tune, 48, 232, 72, 108);  kb (*shape, 128, 232, 72, 108);
    kb (*sub, 44, 356, 72, 108);
    subOct->setBounds (128, 368, 76, 70);
    neblina->setBounds (44, 490, 160, 90);

    // Filter
    envelope.setBounds (L::display.toNearestInt());
    kb (*cutoff, 260, 448, 96, 140);
    kb (*reso, 402, 480, 72, 108); kb (*envmod, 521, 480, 72, 108); kb (*decay, 640, 480, 72, 108); kb (*ffm, 758, 480, 72, 108);

    // Voice
    voiceMode->setBounds (882, 440, 116, 36);
    voices->setBounds (1006, 442, 84, 32);
    kb (*accent, 882, 490, 52, 96); kb (*slide, 934, 490, 52, 96); kb (*drift, 986, 490, 52, 96); kb (*velo, 1038, 490, 52, 96);

    // Effects
    kb (*noise, 32, 662, 142, 50); kb (*drive, 32, 718, 142, 50); kb (*warmth, 32, 774, 142, 50);
    unison->setBounds (208, 662, 172, 32);
    kb (*ctone, 222, 730, 60, 100); kb (*cmix, 306, 730, 60, 100);
    delaySync->setBounds (428, 662, 222, 32);
    kb (*dtime, 434, 730, 60, 100); kb (*dfb, 509, 730, 60, 100); kb (*dmix, 584, 730, 60, 100);
    reverbType->setBounds (698, 662, 222, 32);
    kb (*rsize, 722, 730, 60, 100); kb (*rmix, 836, 730, 60, 100);
    kb (*crush, 962, 662, 142, 50); kb (*comp, 962, 718, 142, 50); kb (*vol, 962, 774, 142, 50);

    // Step modulator
    const int bx[5] = { 808, 866, 920, 974, 1028 }, bw[5] = { 54, 50, 50, 50, 58 };
    for (int i = 0; i < 5; ++i) fills[i].setBounds (bx[i], 880, bw[i], 32);
    laneRate->setBounds (108, 926, 196, 32);
    laneLength->setBounds (108, 966, 196, 32);
    laneMode->setBounds (108, 1006, 196, 32);
    laneDepth->setBounds (108, 1046, 196, 32);
    grid.setBounds (324, 926, 762, 146);
}

void AcidoEditor::showInfo (const juce::String& id)
{
    if (id == infoId) return;
    infoId = id;
    root.repaint (L::info.toNearestInt());
}

void AcidoEditor::selectLane (const juce::String& id)
{
    if (proc.laneIndexFor (id.toStdString()) < 0) return;
    laneId = id;
    for (auto* k : allKnobs) k->repaint();
    neblina->repaint();
    laneRate->repaint(); laneMode->repaint(); laneLength->repaint(); laneDepth->repaint(); grid.repaint();
    root.repaint (L::step.toNearestInt().removeFromTop (60));
}

acido::Lane* AcidoEditor::currentLane()
{
    const int i = proc.laneIndexFor (laneId.toStdString());
    return i >= 0 ? proc.lanes[(size_t) i].get() : nullptr;
}

bool AcidoEditor::laneActive (const juce::String& id) const
{
    const int i = proc.laneIndexFor (id.toStdString());
    return i >= 0 && proc.lanes[(size_t) i]->isActive();
}

juce::String AcidoEditor::selectedLabel() const
{
    if (laneId == "neblina") return "NEBLINA";
    for (auto* k : allKnobs)
        if (k->getLaneId() == laneId)
        {
            static const std::map<juce::String, juce::String> full { { "cmix", "CHORUS DRY/WET" }, { "dmix", "DELAY DRY/WET" }, { "rmix", "REVERB SEND" },
                                                                     { "ctone", "CHORUS TONE" }, { "dfb", "DELAY FEEDBACK" }, { "rsize", "REVERB SIZE" }, { "ffm", "FILTER FM" } };
            if (auto it = full.find (laneId); it != full.end()) return it->second;
        }
    auto* prm = proc.apvts.getParameter (laneId);
    return prm ? prm->getName (40).toUpperCase() : laneId.toUpperCase();
}

void AcidoEditor::refreshAfterPreset()
{
    presetButton.repaint();
    for (auto* k : allKnobs) k->repaint();
    grid.repaint(); laneRate->repaint(); laneMode->repaint(); laneLength->repaint(); laneDepth->repaint();
    root.repaint();
}

void AcidoEditor::showPresetMenu()
{
    juce::PopupMenu menu;
    const auto& entries = proc.presets.getEntries();
    menu.addSectionHeader ("FACTORY");
    bool userHeader = false;
    for (size_t i = 0; i < entries.size(); ++i)
    {
        if (! entries[i].factory && ! userHeader) { menu.addSectionHeader ("USER"); userHeader = true; }
        const int n = (int) i + 1;
        menu.addItem (n, (n < 10 ? "0" : "") + juce::String (n) + "   " + entries[i].name, true, (int) i == proc.presets.getCurrentIndex());
    }
    if (! userHeader) { menu.addSectionHeader ("USER"); menu.addItem (-1, "(save a preset to see it here)", false); }
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetButton).withMinimumWidth (280),
                        [safe = juce::Component::SafePointer<AcidoEditor> (this)] (int r)
                        { if (safe != nullptr && r > 0) { safe->proc.presets.load (r - 1); safe->refreshAfterPreset(); } });
}

void AcidoEditor::openSave (bool open)
{
    if (open)
    {
        const bool factory = proc.presets.currentIsFactory();
        nameBox.setText (proc.presets.getCurrentName() + (factory ? " copy" : ""), false);
        overwriteButton.enabled = ! factory;
        overwriteButton.repaint();
        savePanel.setVisible (true);
        savePanel.toFront (true);
        nameBox.grabKeyboardFocus();
        nameBox.selectAll();
    }
    else
        savePanel.setVisible (false);
    root.repaint();
}

void AcidoEditor::timerCallback()
{
    ++tick;
    grid.repaint();

    // Delay Time follows Sync / MS.
    const bool sync = proc.apvts.getRawParameterValue ("dsync")->load() < 0.5f;
    if (sync != lastDelaySync) { lastDelaySync = sync; dtime->setParameter (sync ? "ddiv" : "dms"); }

    // Shape's name follows the waveform.
    const int wave = (int) std::lround (proc.apvts.getRawParameterValue ("wave")->load());
    if (wave != lastWave)
    {
        lastWave = wave;
        const char* names[4] = { "SHAPE", "PW", "FOLD", "INDEX" };
        shape->setLabel (names[juce::jlimit (0, 3, wave)]);
        if (infoId == "shape") root.repaint (L::info.toNearestInt());
    }

    if (tick % 3 == 0) envelope.repaint();
    if (tick % 15 == 0)
    {
        for (auto* k : allKnobs) k->repaint();                   // lane dots
        neblina->repaint(); voices->repaint();
        root.repaint (L::sync.toNearestInt());
        root.repaint (juce::Rectangle<int> (24, 1110, 1082, 30)); // footer (Mono/Poly text)

        presetButton.repaint();   // name and unsaved-changes dot
    }
}
