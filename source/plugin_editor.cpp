#include "plugin_editor.h"
#include "generator.h"

using namespace raspa::ui;

juce::Font raspa::ui::mono (float size, bool bold)
{
    return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), size,
                                          bold ? juce::Font::bold : juce::Font::plain));
}

juce::Font raspa::ui::sans (float size, bool bold)
{
    return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
}

// ------------------------------------------------------------ look and feel
LookAndFeel::LookAndFeel()
{
    setColour (juce::Slider::rotarySliderFillColourId, col::brass);
    setColour (juce::TextButton::textColourOffId, col::muted);
    setColour (juce::TextButton::textColourOnId, col::ground);
    setColour (juce::ComboBox::textColourId, col::text);
    setColour (juce::ComboBox::backgroundColourId, col::knob);
    setColour (juce::PopupMenu::backgroundColourId, col::panel);
    setColour (juce::PopupMenu::textColourId, col::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, col::brass);
    setColour (juce::PopupMenu::highlightedTextColourId, col::ground);
}

void LookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (box.isMouseOver() ? juce::Colour (0xff2e2620) : col::knob); g.fillRoundedRectangle (r, 4.0f);
    g.setColour (col::ring); g.drawRoundedRectangle (r, 4.0f, 1.0f);
    juce::Path tri;
    float cx = (float) w - 12.0f, cy = (float) h * 0.5f;
    tri.addTriangle (cx - 4, cy - 2, cx + 4, cy - 2, cx, cy + 3);
    g.setColour (col::muted); g.fillPath (tri);
}

juce::Font LookAndFeel::getComboBoxFont (juce::ComboBox&) { return mono (11.0f, true); }
juce::Font LookAndFeel::getPopupMenuFont() { return mono (12.0f, true); }

void LookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (2, 1, box.getWidth() - 22, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}

void LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                    float start, float end, juce::Slider& slider)
{
    auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (2.0f);
    float d = juce::jmin (r.getWidth(), r.getHeight());
    auto c = r.getCentre();
    auto body = juce::Rectangle<float> (d, d).withCentre (c);
    g.setColour (col::knob);  g.fillEllipse (body);
    g.setColour (col::ring);  g.drawEllipse (body.reduced (1.0f), 2.0f);

    float a = start + pos * (end - start);
    float rad = d * 0.5f - 4.0f;
    juce::Point<float> tip (c.x + rad * std::sin (a), c.y - rad * std::cos (a));
    g.setColour (slider.findColour (juce::Slider::rotarySliderFillColourId));
    g.drawLine ({ c, tip }, 2.5f);
}

void LookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    if (b.getToggleState())
    {
        g.setColour (col::brass); g.fillRoundedRectangle (r, 4.0f);
    }
    else
    {
        g.setColour (over ? col::knob : col::panel); g.fillRoundedRectangle (r, 4.0f);
        g.setColour (col::ring); g.drawRoundedRectangle (r, 4.0f, 1.0f);
    }
}

void LookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    g.setFont (mono (11.0f, true));
    g.setColour (b.getToggleState() ? col::ground : col::muted);
    g.drawText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred);
}

// ------------------------------------------------------------ icon buttons
void IconButton::paintButton (juce::Graphics& g, bool over, bool)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    const bool on = getToggleState();
    const bool enabled = isEnabled();
    g.setColour (on ? col::brass : over && enabled ? juce::Colour (0xff2e2620) : col::knob);
    g.fillRoundedRectangle (r, 4.0f);
    if (! on) { g.setColour (col::ring); g.drawRoundedRectangle (r, 4.0f, 1.0f); }
    juce::Colour ink = on ? col::ground : enabled ? col::text : col::dim;
    g.setColour (ink);
    auto c = r.getCentre();
    if (kind == dice)
    {
        auto box = juce::Rectangle<float> (13, 13).withCentre (c);
        g.drawRoundedRectangle (box, 2.5f, 1.4f);
        for (auto p : { juce::Point<float> (-3.2f, -3.2f), juce::Point<float> (0, 0), juce::Point<float> (3.2f, 3.2f) })
            g.fillEllipse (juce::Rectangle<float> (2.4f, 2.4f).withCentre (c + p));
    }
    else
    {
        auto body = juce::Rectangle<float> (11, 8).withCentre (c + juce::Point<float> (0, 2.5f));
        g.fillRoundedRectangle (body, 1.5f);
        juce::Path shackle;
        float top = body.getY() - 5.0f;
        shackle.startNewSubPath (body.getX() + 2.5f, body.getY());
        shackle.lineTo (body.getX() + 2.5f, top + 2.5f);
        shackle.quadraticTo (c.x, top - 2.0f, body.getRight() - 2.5f, top + 2.5f);
        if (on) shackle.lineTo (body.getRight() - 2.5f, body.getY());
        g.strokePath (shackle, juce::PathStrokeType (1.5f));
    }
}

// ------------------------------------------------------------ step grid
juce::Rectangle<float> StepGrid::cellBounds (int i) const
{
    const float cw = 17.0f, inner = 4.0f, group = 8.0f;
    float x = i * (cw + inner) + (i / 4) * (group - inner);
    return { x, 0.0f, cw, (float) getHeight() };
}

int StepGrid::cellAt (juce::Point<int> p) const
{
    for (int i = 0; i < raspa::numSteps; ++i)
        if (cellBounds (i).expanded (2.0f, 0.0f).contains (p.toFloat())) return i;
    return -1;
}

void StepGrid::paint (juce::Graphics& g)
{
    const int playing = lane.displayStep.load();
    for (int i = 0; i < raspa::numSteps; ++i)
    {
        auto r = cellBounds (i).reduced (0.5f);
        const int v = lane.pattern[i].load();
        const int ch = lane.chance[i].load();
        const int rt = lane.ratchet[i].load();
        juce::Colour fill = v == 3 ? col::brass : v == 2 ? juce::Colour (0xff9c7535)
                          : v == 1 ? juce::Colour (0xff5c4626) : col::cell;

        if (mode == velocity)
        {
            g.setColour (fill); g.fillRoundedRectangle (r, 3.0f);
            if (v == 0) { g.setColour (juce::Colour (0xff3a3128)); g.drawRoundedRectangle (r, 3.0f, 1.0f); }
            if (v > 0 && ch > 0)   // chance below 100%: a teal dot on top
            { g.setColour (col::teal); g.fillEllipse (juce::Rectangle<float> (5, 5).withCentre ({ r.getCentreX(), r.getY() + 5 })); }
            if (v > 0 && rt > 1)   // repeats: small bars at the bottom
            {
                g.setColour (col::ground.withAlpha (0.75f));
                for (int k = 0; k < rt; ++k) g.fillRect (r.getX() + 3, r.getBottom() - 5.0f - (float) k * 4.0f, r.getWidth() - 6, 2.0f);
            }
        }
        else
        {
            g.setColour (col::cell); g.fillRoundedRectangle (r, 3.0f);
            g.setColour (juce::Colour (0xff3a3128)); g.drawRoundedRectangle (r, 3.0f, 1.0f);
            const float amount = mode == chance ? raspa::chanceValue (ch) : (float) rt / 4.0f;
            auto bar = r.reduced (2.0f);
            bar = bar.withTop (bar.getBottom() - bar.getHeight() * amount);
            g.setColour (v > 0 ? col::teal : col::teal.withAlpha (0.25f));
            g.fillRoundedRectangle (bar, 2.0f);
            g.setColour (v > 0 ? col::ground : col::dim); g.setFont (mono (8.5f, true));
            juce::String t = mode == chance ? juce::String ((int) std::lround (raspa::chanceValue (ch) * 100.0f)) : "x" + juce::String (rt);
            g.drawText (t, r.withHeight (14).translated (0, r.getHeight() - 15), juce::Justification::centred);
        }
        if (i == playing) { g.setColour (col::text); g.drawRoundedRectangle (r.expanded (1.0f), 3.0f, 2.0f); }
    }
}

void StepGrid::mouseDown (const juce::MouseEvent& e)
{
    int i = cellAt (e.getPosition());
    if (i < 0) return;
    const bool clear = e.mods.isPopupMenu() || e.mods.isCommandDown();
    if (mode == velocity)
    {
        int v = lane.pattern[i].load();
        lane.pattern[i].store (clear ? 0 : (v == 0 ? 3 : v - 1));
    }
    else if (mode == chance)
    {
        int c = lane.chance[i].load();       // 100 -> 75 -> 50 -> 25 -> 100
        lane.chance[i].store (clear ? 0 : (c + 1) % 4);
    }
    else
    {
        int r = lane.ratchet[i].load();      // x1 -> x2 -> x3 -> x4 -> x1
        lane.ratchet[i].store (clear ? 1 : r % 4 + 1);
    }
    repaint();
}

// ------------------------------------------------------------ editor
static const char* knobIds[8]   = { "grain", "seed", "pitch", "cutoff", "res", "pan", "decay", "level" };
static const char* sendIds[4]   = { "sendA", "sendB", "sendC", "sendD" };
static const char* sendNames[4] = { "VERB", "DLY", "CHOR", "RVRS" };
static const char* busIds[RaspaEditor::numBusKnobs] = { "verb_size", "verb_decay", "verb_damp", "verb_pre", "verb_return",
                                                        "dly_feedback", "dly_cutoff", "dly_res", "dly_return",
                                                        "cho_rate", "cho_depth", "cho_width", "cho_return",
                                                        "rev_slice", "rev_fade", "rev_return", "master" };
static const char* busNames[RaspaEditor::numBusKnobs] = { "SIZE", "DECAY", "DAMP", "PRE", "RTN",
                                                          "FDBK", "CUT", "RES", "RTN",
                                                          "RATE", "DEPTH", "WIDTH", "RTN",
                                                          "SLICE", "FADE", "RTN", "VOL" };
static const int busOf[RaspaEditor::numBusKnobs] = { 0,0,0,0,0, 1,1,1,1, 2,2,2,2, 3,3,3, 4 };
static const char* genIds[5]    = { "density", "variation", "swing", "accent", "humanize" };
static const char* genNames[5]  = { "DENS", "VAR", "SWING", "ACCNT", "HUMAN" };

static juce::String knobHelp (const juce::String& id)
{
    static const std::pair<const char*, const char*> help[] = {
        { "grain", "Voice knob A: Grain / Stroke / Metal / Morph / Ring / Shards / Dust, depending on the voice" },
        { "seed", "Voice knob B: Seed / Tick / Open / Tone / Mute / Spread / Body, depending on the voice" },
        { "pitch", "Pitch: -12 to +12 semitones, fine. Double-click to reset to 0." },
        { "cutoff", "Cutoff: low-pass filter on this lane. Fully right = open." },
        { "res", "Resonance of the lane filter" },
        { "pan", "Pan. Double-click to centre." },
        { "decay", "Decay: how long each hit rings" },
        { "level", "Level of this lane" },
        { "density", "Density: 50% = the genre as written, lower thins, higher fills. Euclidean: number of hits." },
        { "variation", "Variation: reshapes the pattern (0 = pure genre). Euclidean: rotation." },
        { "swing", "Swing: pushes every second 16th late, up to 75%" },
        { "accent", "Accent: contrast between ghosts and accents (50% = as written)" },
        { "humanize", "Humanize: hits land up to 12 ms late at random, with a little velocity wobble" },
        { "sendA", "Send to A: Reverb" }, { "sendB", "Send to B: Filter Delay (time set per lane below)" },
        { "sendC", "Send to C: Chorus" }, { "sendD", "Send to D: Reverse" },
        { "verb_size", "Reverb size" }, { "verb_decay", "Reverb decay: 0.3 to 7 seconds" }, { "verb_damp", "Reverb damping: darker tail" },
        { "verb_pre", "Reverb pre-delay: up to 120 ms" }, { "verb_return", "Reverb return level" },
        { "dly_feedback", "Delay feedback: how many repeats" }, { "dly_cutoff", "Delay filter cutoff, inside the echo loop" },
        { "dly_res", "Delay filter resonance" }, { "dly_return", "Delay return level" },
        { "cho_rate", "Chorus rate" }, { "cho_depth", "Chorus depth" }, { "cho_width", "Chorus stereo width" }, { "cho_return", "Chorus return level" },
        { "rev_slice", "Reverse slice: how much of each backwards slice you hear" }, { "rev_fade", "Reverse fade: how gently it swells in" },
        { "rev_return", "Reverse return level" }, { "master", "Master volume" } };
    for (auto& [key, text] : help)
        if (id == key || id.startsWith (juce::String (key) + "_")) return text;
    return {};
}

void RaspaEditor::setupKnob (juce::Slider& s, const juce::String& id)
{
    s.setTooltip (knobHelp (id));
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    s.setRotaryParameters (juce::degreesToRadians (-135.0f), juce::degreesToRadians (135.0f), true);
    addAndMakeVisible (s);
    if (auto* param = proc.apvts.getParameter (id))
        s.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
}

RaspaEditor::RaspaEditor (RaspaProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);
    for (int i = 0; i < raspa::numLanes; ++i)
    {
        auto& L = lanes[i];
        L.grid = std::make_unique<StepGrid> (proc.engine.lanes[i], gridMode);
        L.grid->setTooltip ("Pattern: click to edit what the VEL / CHANCE / REPEAT tabs show. Right-click resets a step. Lock keeps your edits.");
        L.learn.setTooltip ("Launch note for this lane. Click, then play a note to learn it. Click again to cancel.");
        L.learn.setButtonText ("IN " + RaspaProcessor::noteName ((int) proc.apvts.getRawParameterValue (RaspaProcessor::pid ("innote", i))->load()));
        L.learn.onClick = [this, i]
        {
            proc.learnLane.store (proc.learnLane.load() == i ? -1 : i);
            repaint();
        };
        addAndMakeVisible (L.learn);

        // filter type: LP / HP
        L.ftype.setClickingTogglesState (true);
        L.ftype.setTooltip ("Filter type: LP = low-pass (Cutoff right = open), HP = high-pass (Cutoff left = open)");
        addAndMakeVisible (L.ftype);
        L.ftypeAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, RaspaProcessor::pid ("hipass", i), L.ftype);
        L.ftype.onStateChange = [this, i] { auto& b = lanes[i].ftype; auto t = b.getToggleState() ? "HP" : "LP"; if (b.getButtonText() != t) b.setButtonText (t); };
        L.ftype.onClick = [this, i]
        {
            // keep the filter open when switching type: move Cutoff to the new type's "open" end
            auto* c = proc.apvts.getParameter (RaspaProcessor::pid ("cutoff", i));
            const bool hp = lanes[i].ftype.getToggleState();
            if (hp && c->getValue() > 0.99f) c->setValueNotifyingHost (0.0f);
            if (! hp && c->getValue() < 0.01f) c->setValueNotifyingHost (1.0f);
        };
        L.ftype.setButtonText (L.ftype.getToggleState() ? "HP" : "LP");
        addAndMakeVisible (*L.grid);

        for (int t = 0; t < raspa::numVoiceTypes; ++t)
            L.voice.addItem (juce::String::fromUTF8 (raspa::voiceName (t)), t + 1);
        L.voice.setTooltip ("Choose this lane's voice");
        addAndMakeVisible (L.voice);
        L.voiceAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, RaspaProcessor::pid ("voice", i), L.voice);
        L.voice.onChange = [this] { repaint(); };

        L.latch.setClickingTogglesState (true);
        L.latch.setTooltip ("Latch: the lane plays with Ableton's transport. Off: it plays while its note is held.");
        addAndMakeVisible (L.latch);
        L.latchAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, RaspaProcessor::pid ("latch", i), L.latch);

        for (int k = 0; k < 8; ++k)
        {
            setupKnob (L.knobs[k], RaspaProcessor::pid (knobIds[k], i));
            L.knobAtt[k] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, RaspaProcessor::pid (knobIds[k], i), L.knobs[k]);
        }
        L.knobs[2].setPopupDisplayEnabled (true, true, this);    // shows e.g. "+3.50 st" while turning
        L.knobs[2].setMouseDragSensitivity (600);               // slower drag for fine tuning

        // sends and per-lane delay time
        for (int k = 0; k < 4; ++k)
        {
            setupKnob (L.sends[k], RaspaProcessor::pid (sendIds[k], i));
            L.sends[k].setColour (juce::Slider::rotarySliderFillColourId, col::teal);
            L.sendAtt[k] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, RaspaProcessor::pid (sendIds[k], i), L.sends[k]);
        }
        for (int d = 0; d < raspa::numDelayTimes; ++d) L.dtime.addItem (juce::String ("DLY ") + raspa::delayTimeName (d), d + 1);
        L.dtime.setColour (juce::ComboBox::textColourId, col::teal);
        L.dtime.setTooltip ("This lane's echo time on the Filter Delay bus");
        addAndMakeVisible (L.dtime);
        L.dtimeAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, RaspaProcessor::pid ("dtime", i), L.dtime);
        for (int nn = 0; nn < 128; ++nn) L.outNote.addItem ("OUT " + RaspaProcessor::noteName (nn), nn + 1);
        L.outNote.setTooltip ("MIDI out: the note this lane's hits are sent on (to a Drum Rack or hardware)");
        addAndMakeVisible (L.outNote);
        L.outNoteAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, RaspaProcessor::pid ("outnote", i), L.outNote);

        // generator
        for (int g = 0; g < raspa::numGenres; ++g) L.genre.addItem (raspa::genreName (g), g + 1);
        L.genre.setTooltip ("Genre: the style the generator writes for this lane");
        addAndMakeVisible (L.genre);
        L.genreAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, RaspaProcessor::pid ("genre", i), L.genre);
        L.dice.setTooltip ("Dice: roll a new variation of this genre (off while locked)");
        L.dice.onClick = [this, i] { proc.roll (i); lanes[i].grid->repaint(); };
        addAndMakeVisible (L.dice);
        L.lock.setClickingTogglesState (true);
        L.lock.setTooltip ("Lock: keep this pattern. The generator and dice leave it alone.");
        addAndMakeVisible (L.lock);
        L.lockAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, RaspaProcessor::pid ("lock", i), L.lock);
        for (int k = 0; k < 5; ++k)
        {
            setupKnob (L.gen[k], RaspaProcessor::pid (genIds[k], i));
            L.genAtt[k] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, RaspaProcessor::pid (genIds[k], i), L.gen[k]);
        }
    }
    for (int k = 0; k < numBusKnobs; ++k)
    {
        setupKnob (bus[k], busIds[k]);
        bus[k].setColour (juce::Slider::rotarySliderFillColourId, k == numBusKnobs - 1 ? col::brass : col::teal);
        busAtt[k] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, busIds[k], bus[k]);
    }
    for (int r = 0; r < raspa::numReverseLengths; ++r) revLen.addItem (raspa::reverseLengthName (r), r + 1);
    revLen.setColour (juce::ComboBox::textColourId, col::teal);
    revLen.setTooltip ("Reverse: slice length, locked to the beat");
    addAndMakeVisible (revLen);
    revLenAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "rev_length", revLen);

    presetPrev.setTooltip ("Previous preset");
    presetNext.setTooltip ("Next preset");
    presetName.setTooltip ("Presets: click to choose. Factory presets first, then your own.");
    presetSave.setTooltip ("Save everything (patterns included) as your own preset, in Documents/raspa/presets");
    presetPrev.onClick = [this] { proc.stepPreset (-1); refreshPresetName(); };
    presetNext.onClick = [this] { proc.stepPreset (+1); refreshPresetName(); };
    presetName.onClick = [this] { showPresetMenu(); };
    presetSave.onClick = [this] { savePresetDialog(); };
    for (auto* b : { &presetPrev, &presetNext, &presetName, &presetSave }) addAndMakeVisible (*b);
    refreshPresetName();

    midiOut.setClickingTogglesState (true);
    midiOut.setTooltip ("MIDI OUT: send every hit as MIDI. Route it in Ableton with another track's MIDI From set to this track.");
    addAndMakeVisible (midiOut);
    midiOutAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "midiout_on", midiOut);
    for (int c = 1; c <= 16; ++c) midiCh.addItem ("CH " + juce::String (c), c);
    midiCh.setTooltip ("MIDI out channel");
    addAndMakeVisible (midiCh);
    midiChAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "midiout_ch", midiCh);

    const char* modeTips[3] = { "Edit velocity: accent, medium, ghost, off",
                                "Edit chance: how often each step plays (100, 75, 50, 25%)",
                                "Edit repeats: 1 to 4 hits inside the step (rolls and ratchets)" };
    int mi = 0;
    for (auto* b : { &modeVel, &modeChance, &modeRepeat })
    {
        b->setClickingTogglesState (true);
        b->setRadioGroupId (77);
        b->setTooltip (modeTips[mi]);
        const int m = mi++;
        b->onClick = [this, m] { gridMode = m; for (auto& L : lanes) L.grid->repaint(); };
        addAndMakeVisible (*b);
    }
    modeVel.setToggleState (true, juce::dontSendNotification);

    diceAll.setTooltip ("Roll every unlocked lane");
    diceAll.onClick = [this] { proc.roll (-1); for (auto& L : lanes) L.grid->repaint(); };
    addAndMakeVisible (diceAll);

    setSize (W, H);
    startTimerHz (30);
}

RaspaEditor::~RaspaEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void RaspaEditor::resized()
{
    {
        const int by = 100 + 4 * 104 + 3 * 8 + 12, bh = 112, bgap = 10;
        const int widths[5] = { 262, 220, 220, 300, 0 };
        int bx = 20;
        for (int b = 0; b < 5; ++b)
        {
            int w = b < 4 ? widths[b] : W - 20 - bx;
            busBounds[b] = { bx, by, w, bh };
            bx += w + bgap;
        }
        int idx[5] = { 0, 0, 0, 0, 0 };
        for (int k = 0; k < numBusKnobs; ++k)
        {
            int b = busOf[k];
            int kx = busBounds[b].getX() + 14 + (b == 3 ? 100 : 0) + idx[b]++ * 46;
            bus[k].setBounds (kx + 6, busBounds[b].getY() + 38, 34, 34);
        }
        revLen.setBounds (busBounds[3].getX() + 14, busBounds[3].getY() + 42, 90, 26);
        midiOut.setBounds (busBounds[4].getX() + 70, busBounds[4].getY() + 36, busBounds[4].getWidth() - 84, 26);
        midiCh.setBounds (busBounds[4].getX() + 70, busBounds[4].getY() + 68, busBounds[4].getWidth() - 84, 26);
    }
    diceAll.setBounds (W - 20 - 90 - 10 - 150 - 10 - 92, 18, 92, 28);
    {
        const int px = 20 + 13 + headW + gap + genW + gap + 120;
        modeVel.setBounds (px, 76, 44, 18);
        modeChance.setBounds (px + 48, 76, 62, 18);
        modeRepeat.setBounds (px + 114, 76, 62, 18);
    }
    presetPrev.setBounds (420, 18, 28, 28);
    presetName.setBounds (452, 18, 230, 28);
    presetNext.setBounds (686, 18, 28, 28);
    presetSave.setBounds (720, 18, 60, 28);
    const int left = 20, top = 64 + 12 + 24;
    const int laneH = 104, laneGap = 8;
    for (int i = 0; i < raspa::numLanes; ++i)
    {
        auto& L = lanes[i];
        L.bounds = { left, top + i * (laneH + laneGap), W - 2 * left, laneH };
        int x = L.bounds.getX() + 13;
        int cy = L.bounds.getCentreY();

        // lane column
        L.latch.setBounds (x, L.bounds.getY() + 68, 62, 24);
        L.learn.setBounds (x + 70, L.bounds.getY() + 68, 62, 24);
        L.voice.setBounds (x + 30, L.bounds.getY() + 12, headW - 30, 26);
        x += headW + gap;
        // generator column
        L.genre.setBounds (x, L.bounds.getY() + 12, genW - 66, 26);
        L.dice.setBounds (x + genW - 62, L.bounds.getY() + 12, 28, 26);
        L.lock.setBounds (x + genW - 30, L.bounds.getY() + 12, 28, 26);
        for (int k = 0; k < 5; ++k)
            L.gen[k].setBounds (x + k * 46 + 6, L.bounds.getY() + 44, 32, 32);
        x += genW + gap;
        // steps
        L.grid->setBounds (x, cy - 25, stepW, 50);
        x += stepW + gap;
        // voice knobs
        for (int k = 0; k < 8; ++k)
            L.knobs[k].setBounds (x + k * 36 + 2, cy - 26, 32, 32);
        L.ftype.setBounds (x + 3 * 36 + 2, L.bounds.getY() + 8, 32, 15);
        x += voiceW + gap;
        for (int k = 0; k < 4; ++k)
            L.sends[k].setBounds (x + k * 37 + 3, L.bounds.getY() + 10, 30, 30);
        L.dtime.setBounds (x, L.bounds.getY() + 62, 82, 26);
        L.outNote.setBounds (x + 86, L.bounds.getY() + 62, sendW - 86, 26);
    }
}

void RaspaEditor::paint (juce::Graphics& g)
{
    g.fillAll (col::ground);

    // header
    g.setColour (col::brass);
    g.setFont (sans (34.0f, true).withExtraKerningFactor (0.14f));
    g.drawText ("RASPA", 20, 14, 170, 40, juce::Justification::centredLeft);
    g.setFont (mono (11.0f));
    g.setColour (col::muted);
    g.drawText ("HIGH PERCUSSION DESIGNER", 196, 26, 260, 20, juce::Justification::centredLeft);

    auto pill = [&] (juce::Rectangle<int> r, const juce::String& t, juce::Colour c)
    {
        g.setColour (juce::Colour (0xff120f0c)); g.fillRoundedRectangle (r.toFloat(), 4.0f);
        g.setColour (col::line); g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 4.0f, 1.0f);
        g.setColour (c); g.setFont (mono (12.0f, true));
        g.drawText (t, r, juce::Justification::centred);
    };
    double bpm = proc.hostBpm.load();
    pill ({ W - 20 - 150 - 10 - 90, 18, 150, 28 }, "HOST " + juce::String (bpm, 2) + " BPM", col::text);
    pill ({ W - 20 - 90, 18, 90, 28 }, "STAGE 6", col::brass);
    g.setColour (col::line);
    g.fillRect (20, 63, W - 40, 1);

    // column titles
    const int colY = 64 + 12;
    int cx = 20 + 13;
    g.setFont (mono (10.0f));
    g.setColour (col::dim);
    const char* titles[5] = { "LANE \xc2\xb7 VOICE", "GENERATOR", "PATTERN \xc2\xb7 16 STEPS", "VOICE SHAPING", "SENDS A\xe2\x80\x93" "D" };
    const int widths[5] = { headW, genW, stepW, voiceW, sendW };
    for (int c = 0; c < 5; ++c)
    {
        g.drawText (juce::String::fromUTF8 (titles[c]), cx, colY, widths[c], 16, juce::Justification::centredLeft);
        cx += widths[c] + gap;
    }

    for (int i = 0; i < raspa::numLanes; ++i)
    {
        auto& L = lanes[i];
        auto& lane = proc.engine.lanes[i];
        const bool playing = lane.displayPlaying.load();
        auto b = L.bounds.toFloat();
        g.setColour (col::panel); g.fillRoundedRectangle (b, 8.0f);
        g.setColour (col::line);  g.drawRoundedRectangle (b.reduced (0.5f), 8.0f, 1.0f);

        int x = L.bounds.getX() + 13, y = L.bounds.getY();

        // lane column
        g.setColour (col::brass); g.setFont (mono (15.0f, true));
        g.drawText (juce::String (i + 1).paddedLeft ('0', 2), x, y + 12, 26, 26, juce::Justification::centredLeft);

        auto led = juce::Rectangle<float> (8, 8).withPosition ((float) x + 1, (float) y + 47);
        if (playing) { g.setColour (col::brass.withAlpha (0.35f)); g.fillEllipse (led.expanded (3)); g.setColour (col::brass); }
        else g.setColour (juce::Colour (0xff3a3128));
        g.fillEllipse (led);
        g.setColour (col::muted); g.setFont (mono (10.0f));
        juce::String status = ! playing ? "STOPPED"
                            : (lane.latch && proc.hostPlaying.load()) ? juce::String::fromUTF8 ("PLAYING \xc2\xb7 LATCHED")
                            : juce::String::fromUTF8 ("PLAYING \xc2\xb7 HELD");
        g.drawText (status, x + 15, y + 42, headW - 15, 18, juce::Justification::centredLeft);

        x += headW + gap;

        // generator knob labels
        g.setColour (col::muted); g.setFont (mono (10.0f));
        for (int k = 0; k < 5; ++k)
            g.drawText (genNames[k], x + k * 46, y + 78, 44, 14, juce::Justification::centred);
        x += genW + gap + stepW + gap;

        // knob labels
        g.setColour (col::muted); g.setFont (mono (10.0f));
        const int vt = L.voice.getSelectedId() - 1;
        const char* names[8] = { raspa::knobAName (vt), raspa::knobBName (vt), "PITCH", "CUT", "RES", "PAN", "DEC", "LVL" };
        g.setFont (mono (9.5f));
        for (int k = 0; k < 8; ++k)
            g.drawText (names[k], x + k * 36 - 2, L.bounds.getCentreY() + 8, 40, 14, juce::Justification::centred);
        x += voiceW + gap;

        // send labels
        g.setColour (col::muted); g.setFont (mono (9.5f));
        for (int k = 0; k < 4; ++k)
            g.drawText (sendNames[k], x + k * 37 - 2, y + 42, 40, 13, juce::Justification::centred);
    }

    // send buses + master
    const char* busTitles[5] = { "A \xc2\xb7 REVERB", "B \xc2\xb7 FILTER DELAY", "C \xc2\xb7 CHORUS", "D \xc2\xb7 REVERSE", "MASTER" };
    for (int b = 0; b < 5; ++b)
    {
        auto r = busBounds[b].toFloat();
        g.setColour (col::panel); g.fillRoundedRectangle (r, 8.0f);
        g.setColour (col::line);  g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);
        g.setColour (b == 4 ? col::brass : col::teal); g.setFont (mono (11.0f, true));
        g.drawText (juce::String::fromUTF8 (busTitles[b]), busBounds[b].getX() + 14, busBounds[b].getY() + 10, busBounds[b].getWidth() - 20, 16, juce::Justification::centredLeft);
        if (b == 1)
        {
            g.setColour (col::dim); g.setFont (mono (9.5f));
            g.drawText ("TIME PER LANE", busBounds[b].getX(), busBounds[b].getY() + 10, busBounds[b].getWidth() - 14, 16, juce::Justification::centredRight);
        }
    }
    g.setColour (col::muted); g.setFont (mono (9.5f));
    for (int k = 0; k < numBusKnobs; ++k)
        g.drawText (busNames[k], bus[k].getX() - 8, bus[k].getBottom() + 3, bus[k].getWidth() + 16, 13, juce::Justification::centred);
    g.drawText ("LEN", revLen.getX(), revLen.getBottom() + 7, revLen.getWidth(), 13, juce::Justification::centred);

    // help line
    g.setColour (col::dim); g.setFont (mono (10.5f));
    if (helpText.isNotEmpty()) g.setColour (col::text);
    g.drawText (helpText.isNotEmpty() ? helpText
                : juce::String::fromUTF8 ("MIDI C1 \xe2\x80\x93 D#1 launch lanes 1\xe2\x80\x93" "4 (hold to play, new note restarts).  "
                                          "LATCH = play with Ableton.  Click IN to learn a launch note.  VEL / CHANCE / REPEAT pick what a step click edits.  Hover anything for help."),
                20, H - 28, W - 40, 18, juce::Justification::centredLeft);
}

void RaspaEditor::refreshPresetName()
{
    presetName.setButtonText (proc.currentPresetName.toUpperCase());
    for (auto& L : lanes) L.grid->repaint();
    repaint();
}

void RaspaEditor::showPresetMenu()
{
    auto names = proc.getPresetNames();
    juce::PopupMenu factory, user, menu;
    for (int i = 0; i < names.size(); ++i)
    {
        if (i < proc.numFactory()) factory.addItem (i + 1, names[i], true, i == proc.currentPreset);
        else user.addItem (i + 1, names[i], true, i == proc.currentPreset);
    }
    menu.addSectionHeader ("FACTORY");
    menu.addSubMenu ("Factory presets", factory);
    if (user.getNumItems() > 0) menu.addSubMenu ("My presets", user);
    else menu.addItem (-1, "My presets (none saved yet)", false);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetName),
                        [this] (int r) { if (r > 0) { proc.loadPreset (r - 1); refreshPresetName(); } });
}

void RaspaEditor::savePresetDialog()
{
    saveWindow = std::make_unique<juce::AlertWindow> ("Save preset", "Name for your preset:", juce::MessageBoxIconType::NoIcon, this);
    saveWindow->addTextEditor ("name", proc.currentPreset >= proc.numFactory() ? proc.currentPresetName : juce::String());
    saveWindow->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    saveWindow->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    saveWindow->enterModalState (true, juce::ModalCallbackFunction::create ([this] (int r)
    {
        if (r == 1 && saveWindow != nullptr)
        {
            auto name = saveWindow->getTextEditorContents ("name");
            if (proc.saveUserPreset (name)) refreshPresetName();
            else juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Save preset", "Couldn't save that preset. Try another name.");
        }
        saveWindow.reset();
    }), false);
}

void RaspaEditor::timerCallback()
{
    bool needFull = false;
    ++blink;
    for (int i = 0; i < raspa::numLanes; ++i)
    {
        int n = proc.learned[i].exchange (-1);
        if (n >= 0)
            if (auto* p = proc.apvts.getParameter (RaspaProcessor::pid ("innote", i)))
                p->setValueNotifyingHost (p->convertTo0to1 ((float) n));
        auto& L = lanes[i];
        const bool learning = proc.learnLane.load() == i;
        juce::String t = learning ? ((blink / 8) % 2 ? "LEARN" : "PLAY...")
                                  : "IN " + RaspaProcessor::noteName ((int) proc.apvts.getRawParameterValue (RaspaProcessor::pid ("innote", i))->load());
        if (L.learn.getButtonText() != t) L.learn.setButtonText (t);
        L.learn.setToggleState (learning, juce::dontSendNotification);
    }
    {
        juce::String h;
        if (auto* c = juce::Desktop::getInstance().getMainMouseSource().getComponentUnderMouse())
            if (isParentOf (c))
                for (auto* p = c; p != nullptr && p != this; p = p->getParentComponent())
                    if (auto* tc = dynamic_cast<juce::TooltipClient*> (p))
                        if ((h = tc->getTooltip()).isNotEmpty()) break;
        if (h != helpText) { helpText = h; repaint (0, H - 34, W, 34); }
    }
    for (int i = 0; i < raspa::numLanes; ++i)
    {
        auto& L = lanes[i];
        auto& lane = proc.engine.lanes[i];
        int step = lane.displayStep.load();
        int hash = 0;
        for (int s = 0; s < raspa::numSteps; ++s) hash = hash * 4 + lane.pattern[s].load();
        for (int s2 = 0; s2 < raspa::numSteps; ++s2) hash = hash * 31 + lane.chance[s2].load() * 5 + lane.ratchet[s2].load();
        if (step != L.grid->shownStep || hash != L.patternHash) { L.grid->shownStep = step; L.patternHash = hash; L.grid->repaint(); }
        L.dice.setEnabled (! proc.isLocked (i));
        bool pl = lane.displayPlaying.load();
        if (pl != L.shownPlaying) { L.shownPlaying = pl; needFull = true; }
    }
    double bpm = proc.hostBpm.load();
    if (std::abs (bpm - shownBpm) > 0.001) { shownBpm = bpm; needFull = true; }
    if (needFull) repaint();
}
