#pragma once

/**
 * ============================================================================
 *  MacroMorphFX — Scene Data
 * ============================================================================
 *
 *  Each scene stores a snapshot of the 14 module parameters (NOT macros,
 *  NOT morph, NOT performance params like input/output gain).
 *
 *  The plugin stores 8 scenes per preset.  The morph engine interpolates
 *  between two selected scenes (A and B) based on the morph knob (0..1).
 *
 *  See docs/SPEC.md — "Scenes" section.
 * ============================================================================
 */

#include "Params.h"
#include <array>
#include <algorithm>
#include <cmath>

// ─── Scene parameter index ─────────────────────────────────────────────────
namespace SceneParam
{
    enum Index
    {
        filtMode = 0,
        filtCutoff,
        filtReso,
        driveAmt,
        driveTone,
        delaySync,
        delayFb,
        delayTone,
        delayWidth,
        delayPingP,
        revSize,
        revDamp,
        revPreDelay,
        revWidth,
        kCount   // = 14
    };

    /** Metadata for each scene parameter (range, default, discrete flag). */
    struct Info
    {
        std::string_view id;     // must match Params::ID::*
        float minVal;
        float maxVal;
        float defaultVal;
        bool  isDiscrete;        // filtMode, delaySync, delayPingP
    };

    /** Canonical info table — order matches Index enum above. */
    static constexpr std::array<Info, kCount> info = {{
        { Params::ID::filtMode,    0.f,    2.f,     0.f,    true  },
        { Params::ID::filtCutoff,  20.f,   20000.f, 8000.f, false },
        { Params::ID::filtReso,    0.f,    1.f,     0.2f,   false },
        { Params::ID::driveAmt,    0.f,    1.f,     0.f,    false },
        { Params::ID::driveTone,   0.f,    1.f,     0.5f,   false },
        { Params::ID::delaySync,   0.f,    7.f,     2.f,    true  },
        { Params::ID::delayFb,     0.f,    0.95f,   0.25f,  false },
        { Params::ID::delayTone,   0.f,    1.f,     0.5f,   false },
        { Params::ID::delayWidth,  0.f,    1.f,     0.7f,   false },
        { Params::ID::delayPingP,  0.f,    1.f,     0.f,    true  },
        { Params::ID::revSize,     0.f,    1.f,     0.35f,  false },
        { Params::ID::revDamp,     0.f,    1.f,     0.5f,   false },
        { Params::ID::revPreDelay, 0.f,    200.f,   10.f,   false },
        { Params::ID::revWidth,    0.f,    1.f,     0.8f,   false },
    }};

} // namespace SceneParam

// ─── Scene parameter snapshot ──────────────────────────────────────────────

struct SceneParams
{
    float values[SceneParam::kCount] {};

    /** Factory: fill every value from the canonical defaults. */
    static SceneParams createDefault()
    {
        SceneParams s;
        for (int i = 0; i < SceneParam::kCount; ++i)
            s.values[i] = SceneParam::info[static_cast<size_t> (i)].defaultVal;
        return s;
    }

    /**
     *  Morph between two scenes.
     *
     *  SPEC rules:
     *    - Continuous params: linear interpolation
     *    - Discrete params (mode/sync/pingpong): A if morph < 0.5, else B
     */
    static SceneParams morph (const SceneParams& a, const SceneParams& b, float t)
    {
        SceneParams result;

        for (int i = 0; i < SceneParam::kCount; ++i)
        {
            const auto& inf = SceneParam::info[static_cast<size_t> (i)];

            if (inf.isDiscrete)
            {
                result.values[i] = (t < 0.5f) ? a.values[i] : b.values[i];
            }
            else
            {
                result.values[i] = a.values[i] + t * (b.values[i] - a.values[i]);
            }
        }

        return result;
    }

    /** Clamp every value to its valid range. */
    void clampToRanges()
    {
        for (int i = 0; i < SceneParam::kCount; ++i)
        {
            const auto& inf = SceneParam::info[static_cast<size_t> (i)];
            values[i] = std::clamp (values[i], inf.minVal, inf.maxVal);
        }
    }
};

// ─── Constants ─────────────────────────────────────────────────────────────

static constexpr int kNumScenes = 8;

