#pragma once

// RASPA stage 5: factory presets.
// Each preset starts from the default settings and changes only what it lists.
// Latch is never touched, so loading a preset doesn't change how you play the lanes.
//
// Keys are parameter ids (lane number after the underscore). "rng_N" is lane N's dice seed.
// voice: 0 shaker 1 guiro 2 hi-hat 3 crack/clank 4 triangle 5 broken glass 6 lo-fi 7 metal guiro
// genre: 0 techno 1 house 2 cumbia 3 breaks 4 afro-house 5 reggaeton 6 euclidean
// dtime: 0 1/32 1 1/16T 2 1/16 3 1/16D 4 1/8T 5 1/8 6 1/8D 7 1/4T 8 1/4 9 1/4D 10 1/2
// rev_length: 0 1/16 1 1/8 2 1/4 3 1/2 4 1 bar
// ch_N: 16 digits, chance per step (0 = 100%, 1 = 75%, 2 = 50%, 3 = 25%)
// rt_N: 16 digits, repeats per step (1..4)

namespace raspa
{
    struct FactoryPreset { const char* name; const char* settings; };

    inline const FactoryPreset factoryPresets[] =
    {
        { "Cumbia Calle",
          "voice_1=0 genre_1=2 voice_2=1 genre_2=2 voice_3=2 genre_3=2 density_3=0.45 voice_4=4 genre_4=2 seed_4=0.8 "
          "swing_1=0.1 swing_2=0.1 swing_3=0.1 swing_4=0.1 sendA_4=0.3 sendB_2=0.2 dtime_2=6" },
        { "Cumbia Rebajada",
          "voice_1=0 genre_1=2 voice_2=1 genre_2=2 grain_2=0.7 voice_3=2 genre_3=2 voice_4=4 genre_4=2 seed_4=0.85 "
          "pitch_1=-5 pitch_2=-5 pitch_3=-5 pitch_4=-5 swing_1=0.18 swing_2=0.18 swing_3=0.18 swing_4=0.18 "
          "sendA_1=0.3 sendA_2=0.3 sendA_4=0.45 verb_decay=0.6 verb_size=0.7 cutoff_3=0.75" },
        { "Guira Merengue",
          "voice_1=0 genre_1=1 voice_2=7 genre_2=5 seed_2=0.75 voice_3=2 genre_3=1 voice_4=3 genre_4=5 grain_4=0.3 "
          "density_2=0.6 humanize_2=0.15 sendB_4=0.3 dtime_4=4" },
        { "Afro Terra",
          "voice_1=0 genre_1=4 voice_2=7 genre_2=4 voice_3=2 genre_3=4 voice_4=3 genre_4=4 grain_4=0.7 "
          "swing_1=0.15 swing_2=0.15 swing_3=0.15 swing_4=0.15 sendB_2=0.3 sendB_4=0.25 dtime_2=6 dtime_4=7 "
          "dly_feedback=0.45 sendA_4=0.3" },
        { "Afro Noche",
          "voice_1=0 genre_1=4 voice_2=4 genre_2=4 grain_2=0.4 seed_2=0.7 voice_3=5 genre_3=4 density_3=0.4 voice_4=6 genre_4=4 "
          "sendD_2=0.6 sendD_3=0.5 rev_length=2 sendA_2=0.4 sendA_3=0.4 verb_decay=0.7 cutoff_1=0.8" },
        { "Afro Shuffle",
          "voice_1=0 genre_1=4 voice_2=1 genre_2=4 voice_3=2 genre_3=4 voice_4=6 genre_4=4 grain_4=0.4 "
          "swing_1=0.4 swing_2=0.4 swing_3=0.4 swing_4=0.4 humanize_1=0.25 humanize_2=0.25 humanize_3=0.25 humanize_4=0.25" },
        { "Techno Grid",
          "voice_1=0 genre_1=0 voice_2=2 genre_2=0 grain_2=0.8 seed_2=0.5 voice_3=2 genre_3=1 grain_3=0.3 voice_4=3 genre_4=0 grain_4=0.1 "
          "humanize_1=0 humanize_2=0 humanize_3=0 humanize_4=0 swing_1=0 swing_4=0 sendC_3=0.3" },
        { "Techno Hypno",
          "voice_1=0 genre_1=0 density_1=0.6 variation_1=0.3 voice_2=7 genre_2=0 voice_3=2 genre_3=0 voice_4=3 genre_4=0 "
          "sendB_1=0.4 sendB_2=0.4 sendB_4=0.4 dtime_1=3 dtime_2=6 dtime_4=9 dly_feedback=0.6 dly_res=0.5 dly_cutoff=0.6 "
          "cutoff_1=0.7 res_1=0.4" },
        { "Techno Metal",
          "voice_1=7 genre_1=0 voice_2=4 genre_2=0 grain_2=0.9 voice_3=5 genre_3=0 density_3=0.4 voice_4=3 genre_4=0 grain_4=1 "
          "sendA_2=0.3 sendA_3=0.4 sendD_4=0.4 rev_length=1" },
        { "House Garage",
          "voice_1=0 genre_1=1 voice_2=1 genre_2=1 voice_3=2 genre_3=1 voice_4=6 genre_4=1 "
          "swing_1=0.45 swing_2=0.45 swing_3=0.45 swing_4=0.45 humanize_3=0.15" },
        { "House Deep",
          "voice_1=0 genre_1=1 voice_2=4 genre_2=1 grain_2=0.2 voice_3=2 genre_3=1 voice_4=3 genre_4=1 grain_4=0.6 "
          "cutoff_1=0.62 cutoff_3=0.68 sendA_1=0.4 sendA_2=0.5 sendA_4=0.5 verb_size=0.85 verb_decay=0.7 verb_damp=0.6" },
        { "House Disco",
          "voice_1=0 genre_1=1 density_1=0.6 voice_2=2 genre_2=1 seed_2=0.8 voice_3=4 genre_3=1 voice_4=1 genre_4=1 "
          "sendC_1=0.4 sendC_2=0.3 cho_depth=0.7 swing_1=0.2 swing_2=0.2" },
        { "Breaks Polvo",
          "voice_1=6 genre_1=3 grain_1=0.7 voice_2=0 genre_2=3 voice_3=2 genre_3=3 voice_4=6 genre_4=3 seed_4=0.8 grain_4=0.6 "
          "swing_1=0.35 swing_2=0.35 swing_3=0.35 swing_4=0.35 cutoff_2=0.8" },
        { "Breaks Jungle",
          "voice_1=0 genre_1=3 density_1=0.65 variation_1=0.4 voice_2=2 genre_2=3 variation_2=0.4 voice_3=3 genre_3=3 voice_4=7 genre_4=3 "
          "pitch_1=3 pitch_2=3 pitch_3=3 pitch_4=3 sendB_2=0.3 sendB_3=0.3 dtime_2=1 dtime_3=4" },
        { "Dembow",
          "voice_1=0 genre_1=5 voice_2=1 genre_2=5 voice_3=2 genre_3=5 voice_4=3 genre_4=5 grain_4=0.2" },
        { "Dembow Oscuro",
          "voice_1=0 genre_1=5 voice_2=7 genre_2=5 voice_3=6 genre_3=5 grain_3=0.6 voice_4=3 genre_4=5 grain_4=0.6 "
          "pitch_1=-4 pitch_2=-4 pitch_3=-4 pitch_4=-4 sendD_2=0.5 sendD_4=0.5 rev_length=2 cutoff_1=0.7" },
        { "Euclid 5-7-4-9",
          "voice_1=0 genre_1=6 density_1=0.5 voice_2=1 genre_2=6 density_2=0.59 variation_2=0.134 voice_3=2 genre_3=6 density_3=0.4 variation_3=0.334 "
          "voice_4=3 genre_4=6 density_4=0.68 variation_4=0.2 swing_1=0 swing_4=0 humanize_1=0 humanize_2=0 humanize_4=0" },
        { "Euclid Cristal",
          "voice_1=4 genre_1=6 density_1=0.45 grain_1=0.6 voice_2=7 genre_2=6 density_2=0.62 variation_2=0.2 voice_3=5 genre_3=6 density_3=0.3 variation_3=0.47 "
          "voice_4=4 genre_4=6 density_4=0.55 variation_4=0.6 pitch_4=7 sendA_1=0.5 sendA_3=0.5 sendA_4=0.5 verb_decay=0.8 verb_size=0.9" },
        { "Vidrio Roto",
          "voice_1=5 genre_1=4 grain_1=0.7 voice_2=5 genre_2=3 density_2=0.4 voice_3=7 genre_3=0 voice_4=5 genre_4=6 density_4=0.35 grain_4=1 seed_4=1 "
          "sendD_1=0.7 sendD_4=0.7 rev_length=4 rev_slice=0.9 sendA_2=0.5 pan_1=-0.6 pan_2=0.6" },
        { "Polvo Lo-Fi",
          "voice_1=6 genre_1=1 grain_1=0.8 seed_1=0.2 voice_2=6 genre_2=3 seed_2=0.9 voice_3=0 genre_3=1 voice_4=6 genre_4=2 "
          "cutoff_1=0.65 cutoff_3=0.6 sendC_1=0.4 sendC_3=0.4 cho_depth=0.8 swing_1=0.3 swing_2=0.3 swing_3=0.3 swing_4=0.3" },

        // ---- stage 6 additions: these use per-step Chance (ch_) and Repeats (rt_)
        { "Cumbia Viva",
          "voice_1=0 genre_1=2 voice_2=1 genre_2=2 voice_3=4 genre_3=2 seed_3=0.85 voice_4=3 genre_4=2 grain_4=0.5 "
          "ch_1=0000100001000010 rt_2=1112111111121111 ch_4=0002000200020002 humanize_1=0.2 humanize_2=0.2 sendA_3=0.3" },
        { "Cumbia Sonidera",
          "voice_1=7 genre_1=2 voice_2=1 genre_2=2 grain_2=0.65 voice_3=2 genre_3=2 voice_4=5 genre_4=2 density_4=0.35 "
          "sendB_1=0.35 dtime_1=6 dly_feedback=0.55 sendD_4=0.5 rev_length=1 rt_1=1111111111111121 pitch_2=-2" },
        { "Guacharaca",
          "voice_1=1 genre_1=2 grain_1=0.35 seed_1=0.8 voice_2=0 genre_2=2 voice_3=1 genre_3=4 pitch_3=4 voice_4=4 genre_4=2 seed_4=0.9 "
          "rt_1=1121112111211121 ch_3=1010101010101010 pan_1=-0.4 pan_3=0.4" },
        { "Afro Lluvia",
          "voice_1=0 genre_1=4 density_1=0.6 voice_2=5 genre_2=4 density_2=0.35 grain_2=0.4 voice_3=4 genre_3=4 voice_4=0 genre_4=6 density_4=0.59 seed_4=0.1 "
          "ch_1=1111111111111111 ch_2=2222222222222222 sendA_2=0.5 sendA_3=0.4 verb_decay=0.7 rt_4=1111211111112111" },
        { "Afro Rolling",
          "voice_1=0 genre_1=4 voice_2=7 genre_2=4 voice_3=2 genre_3=4 voice_4=3 genre_4=4 "
          "rt_1=1111111211111121 rt_3=1111111111111141 ch_3=0000000000000020 swing_1=0.2 swing_2=0.2 swing_3=0.2 swing_4=0.2" },
        { "Techno Ruido",
          "voice_1=5 genre_1=0 grain_1=0.9 voice_2=2 genre_2=0 voice_3=6 genre_3=0 grain_3=0.9 voice_4=3 genre_4=0 grain_4=0.2 "
          "ch_1=2222222222222222 sendD_1=0.6 rev_length=0 cutoff_3=0.7 res_3=0.5 rt_4=1111111111111131" },
        { "Techno Rolls",
          "voice_1=2 genre_1=0 grain_1=0.7 voice_2=0 genre_2=0 density_2=0.6 voice_3=3 genre_3=0 voice_4=7 genre_4=0 "
          "rt_1=1111111111111144 rt_2=1112111211121112 ch_2=0001000100010001 sendB_3=0.3 dtime_3=3" },
        { "Techno Tribal",
          "voice_1=0 genre_1=6 density_1=0.68 voice_2=1 genre_2=6 density_2=0.5 variation_2=0.2 voice_3=2 genre_3=0 voice_4=3 genre_4=6 density_4=0.4 variation_4=0.4 "
          "pitch_2=-3 pitch_4=-7 grain_4=0.9 sendA_4=0.3 humanize_1=0 humanize_2=0" },
        { "House Bounce",
          "voice_1=0 genre_1=1 voice_2=2 genre_2=1 seed_2=0.7 voice_3=6 genre_3=1 voice_4=4 genre_4=1 "
          "swing_1=0.3 swing_2=0.3 swing_3=0.3 swing_4=0.3 rt_2=1111111111112111 ch_4=0010001000100010" },
        { "House Soleado",
          "voice_1=0 genre_1=1 voice_2=4 genre_2=2 seed_2=0.6 voice_3=2 genre_3=1 voice_4=7 genre_4=1 "
          "sendC_1=0.3 sendA_2=0.4 sendB_4=0.3 dtime_4=5 pan_2=0.5 pan_4=-0.5" },
        { "Breaks Ratchet",
          "voice_1=0 genre_1=3 voice_2=2 genre_2=3 voice_3=3 genre_3=3 voice_4=6 genre_4=3 "
          "rt_1=1111111121111131 rt_2=1111112111111141 ch_1=0000000100000001 swing_1=0.25 swing_2=0.25" },
        { "Breaks Cristal",
          "voice_1=4 genre_1=3 grain_1=0.8 voice_2=5 genre_2=3 density_2=0.4 voice_3=0 genre_3=3 voice_4=7 genre_4=3 "
          "sendA_1=0.4 sendA_2=0.4 sendD_2=0.4 rev_length=2 ch_2=1111111111111111" },
        { "Dembow Fiesta",
          "voice_1=0 genre_1=5 density_1=0.6 voice_2=7 genre_2=5 voice_3=2 genre_3=5 voice_4=4 genre_4=5 seed_4=0.7 "
          "rt_2=1111111111111121 ch_1=0001000100010001 sendA_4=0.3" },
        { "Dembow Pluma",
          "voice_1=6 genre_1=5 grain_1=0.5 seed_1=0.3 voice_2=0 genre_2=5 voice_3=6 genre_3=5 grain_3=0.7 seed_3=0.9 voice_4=5 genre_4=5 density_4=0.35 "
          "cutoff_2=0.75 sendC_2=0.4 sendD_4=0.5 rev_length=1" },
        { "Euclid Rueda",
          "voice_1=0 genre_1=6 density_1=0.55 voice_2=1 genre_2=6 density_2=0.64 variation_2=0.27 voice_3=4 genre_3=6 density_3=0.36 variation_3=0.4 "
          "voice_4=3 genre_4=6 density_4=0.45 variation_4=0.6 rt_1=2111111111111111 ch_2=0101010101010101" },
        { "Euclid Metal",
          "voice_1=7 genre_1=6 density_1=0.73 voice_2=2 genre_2=6 density_2=0.5 variation_2=0.13 voice_3=4 genre_3=6 density_3=0.4 variation_3=0.53 "
          "voice_4=5 genre_4=6 density_4=0.3 variation_4=0.8 grain_2=0.9 seed_2=0.7 sendB_3=0.3 dtime_3=6" },
        { "Lluvia de Vidrio",
          "voice_1=5 genre_1=1 grain_1=0.3 seed_1=0.3 voice_2=5 genre_2=4 grain_2=0.9 seed_2=0.9 voice_3=4 genre_3=6 density_3=0.45 voice_4=0 genre_4=4 "
          "ch_1=2222222222222222 ch_2=3333333333333333 sendA_1=0.6 sendA_2=0.6 verb_size=1 verb_decay=0.85 pitch_3=12" },
        { "Maquinita",
          "voice_1=3 genre_1=6 density_1=0.59 grain_1=0.9 voice_2=3 genre_2=6 density_2=0.4 variation_2=0.33 grain_2=0.1 voice_3=2 genre_3=0 voice_4=7 genre_4=6 density_4=0.68 "
          "rt_1=1111111111111121 rt_4=1121112111211121 pitch_1=-5 pitch_2=5 sendB_2=0.35 dtime_2=1" },
        { "Polvo Ritual",
          "voice_1=6 genre_1=4 grain_1=0.7 voice_2=1 genre_2=4 voice_3=6 genre_3=2 seed_3=0.9 voice_4=4 genre_4=4 seed_4=0.95 "
          "cutoff_1=0.6 cutoff_2=0.7 sendD_2=0.5 rev_length=3 sendA_4=0.4 ch_4=0011001100110011 swing_1=0.25 swing_2=0.25" },
        { "Rumba Callejera",
          "voice_1=0 genre_1=2 density_1=0.6 voice_2=7 genre_2=2 seed_2=0.8 voice_3=1 genre_3=5 voice_4=3 genre_4=2 grain_4=0.4 "
          "humanize_1=0.3 humanize_2=0.3 humanize_3=0.3 humanize_4=0.3 rt_3=1111111111111121 ch_1=0010001000100010 sendA_1=0.2 sendA_4=0.2" },

        // ---- filter types (hipass_N=1 makes lane N's filter a high-pass)
        { "Polvo Grave",      // low-pass + down-pitched, gritty
          "voice_1=6 genre_1=3 grain_1=0.85 seed_1=0.8 pitch_1=-9 decay_1=0.5 cutoff_1=0.45 res_1=0.35 level_1=0.9 "
          "voice_2=0 genre_2=4 grain_2=0.5 seed_2=0.9 pitch_2=-10 cutoff_2=0.5 res_2=0.25 rt_2=1111111111111121 "
          "voice_3=2 genre_3=0 grain_3=0.75 seed_3=0.4 pitch_3=-12 cutoff_3=0.55 res_3=0.45 "
          "voice_4=6 genre_4=3 grain_4=1 seed_4=0.3 pitch_4=-5 cutoff_4=0.5 res_4=0.3 density_4=0.4 "
          "swing_1=0.3 swing_2=0.3 swing_3=0.3 swing_4=0.3 humanize_1=0.2 humanize_2=0.2 "
          "sendB_2=0.25 dtime_2=6 dly_cutoff=0.45 dly_feedback=0.45 sendD_4=0.4 rev_length=2 sendA_1=0.15 sendA_3=0.2 verb_damp=0.8" },
        { "Aire Alto",        // high-pass: thin, airy tops that sit above a full beat
          "voice_1=0 genre_1=4 hipass_1=1 cutoff_1=0.62 res_1=0.3 voice_2=7 genre_2=4 hipass_2=1 cutoff_2=0.55 "
          "voice_3=2 genre_3=1 hipass_3=1 cutoff_3=0.7 res_3=0.5 voice_4=4 genre_4=4 hipass_4=1 cutoff_4=0.5 pitch_4=5 "
          "sendC_1=0.3 sendA_4=0.4 swing_1=0.15 swing_2=0.15" },
    };

    constexpr int numFactoryPresets = (int) (sizeof (factoryPresets) / sizeof (factoryPresets[0]));
}
