// preset_manager.h — ÁCIDO presets (phase 5)
// Factory presets are built into the plugin and can't be overwritten.
// User presets are files in ~/Music/ACIDO/Presets (one .acidopreset per sound).
// A preset stores every knob, the voice mode and all step lanes.
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

class AcidoProcessor;

class PresetManager
{
public:
    explicit PresetManager (AcidoProcessor& p) : proc (p) {}

    struct Entry
    {
        juce::String name;
        bool factory = false;
        int factoryIndex = -1;
        juce::File file;
    };

    static juce::File userFolder();

    void refresh();                                   // re-reads the User folder
    const std::vector<Entry>& getEntries() const { return entries; }
    int getCurrentIndex() const { return current; }

    bool load (int index);
    void next()     { if (! entries.empty()) load ((current + 1) % (int) entries.size()); }
    void previous() { if (! entries.empty()) load ((current - 1 + (int) entries.size()) % (int) entries.size()); }

    // Saves the current sound as a new User preset. Returns false if it couldn't be written.
    bool saveAsNew (const juce::String& name);
    // Replaces the current User preset with the current sound (not allowed for factory presets).
    bool overwrite();

    juce::String getCurrentName() const  { return currentName; }
    bool currentIsFactory() const        { return currentFactory; }
    bool isModified() const;              // changed since the preset was loaded or saved

    // Called when a Live Set is reopened: shows the saved name, counted as unmodified.
    void restoreName (const juce::String& name, bool factory);

    static int numFactoryPresets();
    static juce::String factoryName (int i);

private:
    void applyFactory (int i);
    void markClean();
    juce::int64 fingerprint() const;
    static juce::String safeFileName (const juce::String& name);

    AcidoProcessor& proc;
    std::vector<Entry> entries;
    int current = 0;
    juce::String currentName { "Squelch Init" };
    bool currentFactory = true;
    juce::int64 cleanPrint = 0;
};
