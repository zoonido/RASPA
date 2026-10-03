// Draws the RASPA window into a PNG (for screenshots only).
#include "plugin_processor.h"
#include "plugin_editor.h"
#include "presets.h"
#include <cstdio>

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    RaspaProcessor proc;
    proc.hostBpm.store (124.0);
    proc.hostPlaying.store (true);
    // show the stage-1 example state: lanes 1, 2, 4 playing, lane 2 latched, lane 4 just restarted
    *proc.apvts.getRawParameterValue ("latch_2") = 1.0f;
    proc.apvts.getParameter ("latch_2")->setValueNotifyingHost (1.0f);
    proc.engine.lanes[1].latch = true;
    int steps[4] = { 5, 5, -1, 2 };
    for (int i = 0; i < 4; ++i)
    {
        proc.engine.lanes[i].displayStep.store (steps[i]);
        proc.engine.lanes[i].displayPlaying.store (steps[i] >= 0);
    }
    proc.apvts.getParameter ("voice_2")->setValueNotifyingHost (proc.apvts.getParameter ("voice_2")->convertTo0to1 (7.0f));
    proc.apvts.getParameter ("voice_4")->setValueNotifyingHost (proc.apvts.getParameter ("voice_4")->convertTo0to1 (4.0f));
    proc.apvts.getParameter ("genre_4")->setValueNotifyingHost (proc.apvts.getParameter ("genre_4")->convertTo0to1 (6.0f));
    // check every factory preset: all keys must be real parameters
    int bad = 0;
    for (int i = 0; i < raspa::numFactoryPresets; ++i)
    {
        juce::StringArray items; items.addTokens (raspa::factoryPresets[i].settings, " ", "");
        for (auto& it : items)
        {
            auto key = it.upToFirstOccurrenceOf ("=", false, false);
            if (! key.startsWith ("rng_") && ! key.startsWith ("ch_") && ! key.startsWith ("rt_") && proc.apvts.getParameter (key) == nullptr) { std::printf ("preset %s: unknown key %s\n", raspa::factoryPresets[i].name, key.toRawUTF8()); ++bad; }
        }
        proc.loadPreset (i);
        if (proc.currentPresetName != raspa::factoryPresets[i].name) ++bad;
    }
    std::printf ("factory presets checked: %d, problems: %d\n", raspa::numFactoryPresets, bad);
    proc.loadPreset (40);
    proc.apvts.getParameter ("latch_2")->setValueNotifyingHost (1.0f);
    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    ed->setVisible (true);
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
    juce::File out (argc > 1 ? argv[1] : "raspa.png");
    out.deleteFile();
    juce::FileOutputStream os (out);
    juce::PNGImageFormat().writeImageToStream (img, os);
    ed.reset();
    return 0;
}
