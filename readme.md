# acido

ÁCIDO, a 303-style acid bass synth VST3 for Ableton Live. GitHub builds it in
the cloud every time files are uploaded.

Every file and folder name is lowercase. The one exception is `CMakeLists.txt`:
the build tool only accepts that exact spelling.

**Current stage: phase 6 – the designed interface.** ÁCIDO is feature-complete
against the spec: the window now matches mockup v7.

Using the window:
- **Knobs:** drag up/down to turn (hold Shift for fine moves), scroll to nudge,
  double-click to reset. A single click (no drag) opens that knob's step lane.
- **Info panel:** hover any knob, switch or the Neblina slider to read what it does.
- **Step mod** (bottom): drag across the bars to draw; right-click or
  Option-click clears a step. A ● next to a knob's name means its lane is active.
- **Presets** (top right): click the name for the list, `<` `>` to step
  through, **SAVE** to name and save. An amber ● means unsaved changes.
- **Size:** drag the bottom-right corner (60–125%). Ableton remembers the
  size with the Live Set.

**Presets** (top right of the window: `<` name `>` ● Save):
- 12 factory presets, read-only: Squelch Init, Rubber Acid, Night Bus Line,
  Round Sub Pluck, Chirrido, Neblina Pad, Acid Stabs, Squelch Motion,
  Tape Burn, Lo-Fi Crush, Wobble Bass, Cumbia Ácida.
- Save opens a name box with **Save as new** and, for your own presets,
  **Overwrite**. A preset stores every knob, the voice mode and all lanes.
- Your presets live in `~/Music/ACIDO/Presets` as `.acidopreset` files: back
  them up, share them, or delete them there.
- An amber ● next to the name means there are unsaved changes.

- **Mono / Poly:** Mono is the classic 303 behaviour. Poly plays 2–8 voices;
  each new note glides from the last note played, and the oldest note is
  taken over when all voices are busy.
- **Unison:** 1–7 copies of the oscillator per voice, spread in pitch
  (±8 cents) and across the stereo field. The deep bass always stays centred.
  In Poly the total is capped at 24 oscillators (8 voices × 3).
- **Neblina:** widens the unison detune up to ±33 cents with a slow wobble,
  and adds drifting resonances and a light phaser. 0% is off. It can take a
  step lane like the other knobs.

Using the step modulator:
- Click a knob's name (or pick it in the STEP MOD list) to edit its lane.
- Drag across the 16 bars to draw; right-click or Option-click clears a step.
- Rate, Length, Step/Smooth and Depth are set per lane. Fill draws a shape.
- A ● before a knob's name means its lane is active.
- Lanes only move while Ableton is playing; they're saved in the Live Set.

Push 1 banks:
1. Cutoff, Resonance, Env Mod, Decay, Accent, Slide, Drive, Volume
2. Waveform, Tune, Shape, Sub, Sub Octave, Filter FM, Noise, Warmth
3. Drift, Velo, Chorus Dry/Wet, Chorus Tone, Delay Dry/Wet, Delay Feedback,
   Delay Time (Sync), Delay Sync/MS
4. Reverb Dry/Wet, Reverb Size, Reverb Type, Crush, Comp, Delay Time (MS),
   Neblina, Unison
5. Mono / Poly, Poly Voices

## What the repository should look like

```
.github/workflows/build.yml
source/plugin_processor.cpp
source/plugin_processor.h
source/dsp/acid_engine.h
source/dsp/acid_voice.h
source/dsp/drive.h
source/dsp/effects.h
source/dsp/mono_synth.h
source/dsp/neblina.h
source/dsp/poly_synth.h
source/dsp/step_mod.h
source/plugin_editor.cpp
source/plugin_editor.h
source/preset_manager.cpp
source/preset_manager.h
tests/voice_tests.cpp
CMakeLists.txt
readme.md
assets/fonts/   (Chakra Petch and IBM Plex Mono, SIL Open Font License)
```

## Where things live

- `source/dsp/acid_engine.h` – the whole sound in signal order: voices →
  Neblina → Drive (send) → Chorus → Delay → Reverb (send) → Crush → Comp →
  Noise (send) → Volume.
- `source/dsp/acid_voice.h` – the voice: 4 waveforms with Shape, sub, 24 dB
  ladder filter with Filter FM, envelopes, accent, slide, drift, velocity, noise.
- `source/dsp/drive.h` – Drive and Warmth, as a send: a distorted copy is
  blended on top of the clean sound, which always stays at full level.
- `source/dsp/effects.h` – Chorus, ping-pong Delay, 4-type Reverb, Crush, Comp.
- `source/dsp/mono_synth.h` – mono key handling: overlapping notes slide,
  releasing a key returns to one still held.
- `source/dsp/poly_synth.h` – Poly mode: up to 8 voices, glide, voice stealing.
- `source/dsp/neblina.h` – Neblina's drifting resonances and phaser.
- `source/dsp/step_mod.h` – the step lanes and how they follow the song position.
- `source/plugin_processor.*` – the plugin shell Ableton talks to: parameters,
  tempo and song position, step lanes, saving.
- `source/preset_manager.*` – factory presets and saving/loading your own.
- `source/plugin_editor.*` – the interface: knobs, Info panel, step lane
  editor and preset bar, drawn to match the mockup.
- `assets/fonts/` – the mockup's two fonts, built into the plugin. Both are
  free under the SIL Open Font License (license texts included).
- `tests/voice_tests.cpp` – automated sound checks, run on every upload;
  also renders `acido_preview.wav`.
- `.github/workflows/build.yml` – the cloud build recipe.

## What each GitHub build gives you

In the **Actions** tab, open the newest run (never use Re-run for new files):

- **acido-vst3-mac** – the plugin (Apple Silicon + Intel).
- **acido-preview** – a WAV preview of this build's sound.
- The summary page lists every sound check as PASS or FAIL.

## Installing a new build

1. Quit Ableton.
2. Unzip `acido-vst3-mac.zip` until `acido.vst3` appears.
3. Drag `acido.vst3` into `/Library/Audio/Plug-Ins/VST3` (replace the old one).
4. In Terminal:
   ```
   xattr -dr com.apple.quarantine /Library/Audio/Plug-Ins/VST3/acido.vst3
   ```
   No message means it worked. "Permission denied" → put `sudo ` in front.
5. Open Live, Option+Rescan, and load a fresh **ACIDO** (under ZOONIDO).
