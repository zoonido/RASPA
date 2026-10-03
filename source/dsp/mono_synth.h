// mono_synth.h — ÁCIDO mono note handling
// Keeps track of held keys and turns them into 303-style behaviour:
//  - a key pressed while another is held  -> slide (no envelope retrigger)
//  - a key pressed with nothing held       -> new note (envelopes restart)
//  - releasing a key while another is held -> slide back to the held key
#pragma once

#include "acid_voice.h"
#include <vector>

namespace acido
{

class MonoSynth
{
public:
    void prepare (double sampleRate)
    {
        voice.prepare (sampleRate);
        held.clear();
        held.reserve (128);
    }

    void noteOn (int note, int velocity)
    {
        removeHeld (note);
        const bool somethingHeld = ! held.empty();
        held.push_back ({ note, velocity });

        if (somethingHeld) voice.slideTo (note, velocity);
        else               voice.noteOn  (note, velocity);
    }

    void noteOff (int note)
    {
        const bool wasCurrent = ! held.empty() && held.back().note == note;
        removeHeld (note);

        if (held.empty())    voice.noteOff();
        else if (wasCurrent) voice.slideTo (held.back().note, held.back().velocity);
    }

    void allNotesOff()
    {
        held.clear();
        voice.noteOff();
    }

    void render (float* out, int numSamples, const VoiceParams& p)
    {
        voice.render (out, numSamples, p);
    }

    void renderStereo (float* L, float* R, int numSamples, const VoiceParams& p, float* noiseOut = nullptr)
    {
        voice.renderStereo (L, R, numSamples, p, noiseOut);
    }

    const AcidVoice& getVoice() const { return voice; }

private:
    struct Held { int note; int velocity; };

    void removeHeld (int note)
    {
        held.erase (std::remove_if (held.begin(), held.end(),
                                    [note] (const Held& h) { return h.note == note; }),
                    held.end());
    }

    AcidVoice voice;
    std::vector<Held> held;
};

} // namespace acido
