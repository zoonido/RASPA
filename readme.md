# raspa

RASPA, a 4-lane high percussion designer for Ableton (VST3, Mac).

**Current stage: 6 – chance, repeats, MIDI learn, 40 presets**

- **Lanes and MIDI:** C1, C#1, D1, D#1 launch lanes 1–4. Note-on starts the lane
  from step 1 instantly (or restarts it), note-off stops it. Latch makes a lane
  play with Ableton's transport, locked to the grid.
- **Eight synthesized voices**, any voice on any lane: Shaker, Güiro, Hi-hat,
  Crack/Clank, Triangle, Broken Glass (FM), Lo-Fi, Metal Güiro (güira).
  Each has two voice knobs plus Pitch (-12..+12 semitones, fine), Decay and Level.
- **Genre generator** per lane: Techno, House, Cumbia, Breaks, Afro-house,
  Reggaeton, or Euclidean (evenly spread hits with rotation). Density thins or
  fills, Variation reshapes, Dice rolls a new variation, Lock keeps the pattern.
  Swing, Accent and Humanize shape how it plays.

- **Shaping and sends:** per lane Pan, a resonant filter (LP or HP: Cutoff,
  Resonance) and four sends:
  A Reverb, B Filter Delay (each lane its own tempo-synced time), C Chorus,
  D Reverse (plays each beat-slice backwards into the next). Master volume.

- **MIDI out:** every hit is also sent as MIDI (note per lane, channel, on/off),
  to drive a Drum Rack or hardware.
- **Per step:** Chance (100/75/50/25%) and Repeats (x1–x4 rolls), edited with
  the VEL / CHANCE / REPEAT tabs above the pattern.
- **MIDI learn:** click a lane's IN button and play a note to set its launch note.
- **Presets:** 42 factory presets, plus your own saved to Documents/raspa/presets.
  The bottom bar explains whatever is under the mouse.

Every push builds the plugin for Mac automatically.
Download it from the **Actions** tab → latest run → **Artifacts**.
