#pragma once
// Starting settings for the four lanes. Patterns come from the generator.
#include "voices.h"
#include "generator.h"

namespace raspa
{
    struct LaneDefaults { int voice; float a, b, decay, level; int genre; float density, variation, swing, accent, humanize; };

    inline const LaneDefaults laneDefaults[4] =
    {
        //  voice   A      B      dec    lvl    genre      dens   var    swing  accent human
        { shaker, 0.60f, 0.35f, 0.30f, 0.80f, afroHouse, 0.50f, 0.00f, 0.15f, 0.50f, 0.10f },
        { guiro,  0.45f, 0.55f, 0.30f, 0.70f, cumbia,    0.50f, 0.00f, 0.00f, 0.50f, 0.10f },
        { hihat,  0.55f, 0.35f, 0.25f, 0.60f, techno,    0.50f, 0.00f, 0.00f, 0.50f, 0.00f },
        { clank,  0.45f, 0.50f, 0.35f, 0.60f, afroHouse, 0.50f, 0.00f, 0.15f, 0.50f, 0.10f },
    };

    // stage 4: mix settings per lane: pan, sends A-D (reverb, delay, chorus, reverse), delay time
    struct LaneMix { float pan; float send[4]; int delayTime; };
    inline const LaneMix laneMix[4] =
    {
        { -0.25f, { 0.15f, 0.00f, 0.00f, 0.00f }, 3 },   // 1/16 dotted
        {  0.20f, { 0.10f, 0.25f, 0.00f, 0.00f }, 6 },   // 1/8 dotted
        {  0.35f, { 0.00f, 0.00f, 0.20f, 0.00f }, 2 },   // 1/16
        { -0.35f, { 0.30f, 0.15f, 0.00f, 0.00f }, 7 },   // 1/4 triplet
    };
}
