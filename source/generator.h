#pragma once

// RASPA stage 3: the genre generator.
// Writes a 16-step pattern for a lane from (voice, genre, density, variation, seed).
//
// Each voice belongs to a role (shaker, guiro, hat, triangle, sparse hits, backbeat),
// and each role has a template per genre: one character per 16th step,
// '3' accent, '2' medium, '1' ghost, '0' rarely played.
//  - Density 0.5 plays the template as written; lower thins it, higher fills it in.
//    Sweeping Density only adds or removes steps, the rest stay put.
//  - Variation blends the template with the seed's own random shape and nudges
//    some step levels up or down. Variation 0 = the pure genre template.
//  - The seed (the dice) picks which variation you get. Same seed, same pattern.
//
// EUCLIDEAN works differently (the same for every voice):
//  - Density sets how many hits, spread as evenly as possible over 16 steps:
//    0% none, 50% five hits (the classic 5-in-16), 100% all sixteen.
//  - Variation rotates the pattern, 0..15 steps (0 = first hit on step 1).
//  - Dice picks which hits are accents, mediums or ghosts (the first hit stays an accent).
//
// Plain C++ (no JUCE) so the test program can check it directly.

#include <cstdint>
#include <algorithm>
#include "voices.h"

namespace raspa
{
    enum Genre { techno = 0, house, cumbia, breaks, afroHouse, reggaeton, euclidean, numGenres };

    inline const char* genreName (int g)
    {
        static const char* n[numGenres] = { "TECHNO", "HOUSE", "CUMBIA", "BREAKS", "AFRO-HOUSE", "REGGAETON", "EUCLIDEAN" };
        return n[std::clamp (g, 0, numGenres - 1)];
    }

    enum Role { roleShaker = 0, roleGuiro, roleHat, roleTriangle, roleSparse, roleBackbeat, numRoles };

    inline int roleFor (int voice)
    {
        switch (voice)
        {
            case shaker:     return roleShaker;
            case guiro:      return roleGuiro;
            case metalGuiro: return roleGuiro;
            case hihat:      return roleHat;
            case triangle:   return roleTriangle;
            case clank:      return roleSparse;
            case glass:      return roleSparse;
            case lofi:       return roleBackbeat;
            default:         return roleShaker;
        }
    }

    //                                   TECHNO              HOUSE               CUMBIA              BREAKS              AFRO-HOUSE          REGGAETON
    inline const char* const templates[numRoles][numGenres - 1] =
    {
        /* shaker   */ { "1131113111311131", "2131213121312131", "3022302230223022", "2120212021202121", "2131213121322131", "2012302220123022" },
        /* guiro    */ { "0030003000300030", "1030103010301030", "3022302230223022", "3002003030020030", "3021302130213021", "3121312131213121" },
        /* hat      */ { "1030103010301030", "1131113111311131", "2020202020202020", "2120202120212020", "1131113111211131", "2023202220232022" },
        /* triangle */ { "0030003000300030", "1030103010301030", "3010301030103011", "3000003000300000", "1031103110311031", "3001001030010010" },
        /* sparse   */ { "0003000100030000", "0000300000003000", "3000001030000010", "0000300100003010", "0010300000103000", "0003002000030020" },
        /* backbeat */ { "0000300000003000", "0000300000003010", "0020300200203002", "0000300100003000", "0010300100103000", "0003003000030030" },
    };

    struct GenRng
    {
        uint32_t s;
        explicit GenRng (uint32_t seed) : s (seed * 2654435761u + 0x9e3779b9u) { if (s == 0) s = 1; next(); next(); }
        uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
        float uni() { return (float) (next() >> 8) * (1.0f / 16777216.0f); }
    };

    inline int euclidHits (float density)
    {
        density = std::clamp (density, 0.0f, 1.0f);
        return density <= 0.5f ? (int) std::lround (density * 10.0f)                // 0 .. 5
                               : 5 + (int) std::lround ((density - 0.5f) * 22.0f);  // 5 .. 16
    }

    inline void generateEuclid (float density, float variation, uint32_t seed, int out[16])
    {
        const int k = euclidHits (density);
        const int rot = (int) std::lround (std::clamp (variation, 0.0f, 1.0f) * 15.0f);
        GenRng rng (seed * 31u + 7u);
        int base[16]; int hit = 0;
        for (int i = 0; i < 16; ++i)
        {
            float u = rng.uni();
            bool on = ((i * k) % 16) < k;           // evenly spread hits (Bjorklund pattern)
            base[i] = ! on ? 0 : (hit++ == 0 ? 3 : (u < 0.3f ? 3 : u < 0.7f ? 2 : 1));
        }
        for (int i = 0; i < 16; ++i) out[(i + rot) % 16] = base[i];
    }

    inline void generatePattern (int voice, int genre, float density, float variation, uint32_t seed, int out[16])
    {
        if (genre == euclidean) { generateEuclid (density, variation, seed, out); return; }
        const char* t = templates[roleFor (voice)][std::clamp (genre, 0, numGenres - 2)];
        density = std::clamp (density, 0.0f, 1.0f);
        variation = std::clamp (variation, 0.0f, 1.0f);
        GenRng rng (seed + (uint32_t) genre * 977u + (uint32_t) roleFor (voice) * 131u);

        for (int i = 0; i < 16; ++i)
        {
            // draw every number for every step, always in the same order, so a step's
            // fate depends only on (seed, step) and not on the knob positions
            float jitter = rng.uni(), shape = rng.uni(), nudge = rng.uni(), dir = rng.uni();

            int base = t[i] - '0';
            float w = base == 3 ? 1.0f : base == 2 ? 0.8f : base == 1 ? 0.6f : 0.12f;
            // the seed's own shape: favours beats and offbeats a little, like a player would
            float own = 0.15f + 0.85f * shape * (i % 2 == 0 ? 1.0f : 0.8f);
            float score = (1.0f - variation) * w + variation * own + 0.12f * (jitter - 0.5f);

            bool on = score > 0.5f + (0.5f - density) * 1.2f;   // 0 = none, 0.5 = template, 1 = all
            int v = 0;
            if (on)
            {
                v = std::max (base, 1);
                if (nudge < variation * 0.5f)
                    v = std::clamp (v + (dir < 0.5f ? -1 : 1), 1, 3);
            }
            out[i] = v;
        }
    }
}
