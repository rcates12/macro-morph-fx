#pragma once

/**
 * ============================================================================
 *  MacroMorphFX — Macro Mapping Engine
 * ============================================================================
 *
 *  Each of the 4 macros can map to multiple scene parameters with a
 *  configurable amount (-1..+1 as fraction of the parameter's full range).
 *
 *  Application order (per SPEC):
 *    finalParams = applyMacros(baseParams, macroValues)
 *
 *  Macros are applied AFTER morph interpolation.
 *  Discrete parameters are not affected by macros.
 *
 *  Curve types (linear only for MVP; exp/log/s-curve later).
 *
 *  See docs/SPEC.md — "Macros" section.
 * ============================================================================
 */

#include "SceneData.h"
#include <vector>
#include <array>
#include <algorithm>
#include <cmath>

// ─── Macro curve types (per SPEC) ──────────────────────────────────────────

enum class MacroCurve
{
    linear = 0,   // f(x) = x
    exponential,  // f(x) = x^2
    logarithmic,  // f(x) = sqrt(x)
    sCurve,       // f(x) = smoothstep (3x^2 - 2x^3)
    kCount
};

static constexpr const char* macroCurveNames[] = {
    "Linear", "Exp", "Log", "S-Curve"
};

/** Apply curve to a 0..1 macro value. */
inline float applyMacroCurve (float x, MacroCurve curve)
{
    x = std::clamp (x, 0.0f, 1.0f);

    switch (curve)
    {
        case MacroCurve::exponential:  return x * x;
        case MacroCurve::logarithmic:  return std::sqrt (x);
        case MacroCurve::sCurve:       return x * x * (3.0f - 2.0f * x);
        case MacroCurve::linear:
        default:                       return x;
    }
}

// ─── Macro target ──────────────────────────────────────────────────────────

struct MacroTarget
{
    int        sceneParamIndex;   // SceneParam::Index
    float      amount;            // -1..+1 (fraction of parameter range)
    MacroCurve curve = MacroCurve::linear;  // response curve (per SPEC)
};

// ─── Macro engine ──────────────────────────────────────────────────────────

class MacroEngine
{
public:
    static constexpr int kNumMacros = 4;

    MacroEngine()
    {
        initDefaultMappings();
    }

    // ── Mapping access ─────────────────────────────────────────────────

    void setMappings (int macroIndex, std::vector<MacroTarget> targets)
    {
        if (macroIndex >= 0 && macroIndex < kNumMacros)
            mappings_[static_cast<size_t> (macroIndex)] = std::move (targets);
    }

    const std::vector<MacroTarget>& getMappings (int macroIndex) const
    {
        return mappings_[static_cast<size_t> (macroIndex)];
    }

    void clearAllMappings()
    {
        for (auto& m : mappings_)
            m.clear();
    }

    // ── Apply macros to morphed scene params ───────────────────────────

    /**
     *  Modifies `params` in-place by adding macro offsets.
     *
     *  For each macro with a non-zero value, walk its targets and add:
     *      offset = macroValue * mapping.amount * (paramMax - paramMin)
     *
     *  The result is clamped to the parameter's valid range.
     *  Discrete parameters (filtMode, delaySync, delayPingPong) are skipped.
     */
    void apply (SceneParams& params, const float macroValues[kNumMacros]) const
    {
        for (int m = 0; m < kNumMacros; ++m)
        {
            const float rawMacroVal = macroValues[m];

            if (rawMacroVal < 0.001f)
                continue;   // macro is at zero — no contribution

            for (const auto& target : mappings_[static_cast<size_t> (m)])
            {
                const int idx = target.sceneParamIndex;

                if (idx < 0 || idx >= SceneParam::kCount)
                    continue;

                const auto& inf = SceneParam::info[static_cast<size_t> (idx)];

                if (inf.isDiscrete)
                    continue;   // macros don't affect discrete params

                // Apply curve to the raw 0..1 macro value
                const float curvedVal = applyMacroCurve (rawMacroVal, target.curve);

                const float range  = inf.maxVal - inf.minVal;
                const float offset = curvedVal * target.amount * range;

                params.values[idx] = std::clamp (
                    params.values[idx] + offset,
                    inf.minVal,
                    inf.maxVal);
            }
        }
    }

    // ── Serialisation helpers ──────────────────────────────────────────

    /** Number of targets in a given macro. */
    int getTargetCount (int macroIndex) const
    {
        if (macroIndex < 0 || macroIndex >= kNumMacros)
            return 0;
        return static_cast<int> (mappings_[static_cast<size_t> (macroIndex)].size());
    }

private:
    std::array<std::vector<MacroTarget>, kNumMacros> mappings_;

    /**
     *  Musically useful factory defaults.
     *
     *  Macro 1 — "Filter Sweep":  opens cutoff, adds resonance
     *  Macro 2 — "Dirt":          pushes drive, darkens tone
     *  Macro 3 — "Space":         more delay feedback + reverb size
     *  Macro 4 — "Width":         widens delay + reverb stereo
     */
    void initDefaultMappings()
    {
        mappings_[0] = {
            { SceneParam::filtCutoff,  0.5f },
            { SceneParam::filtReso,    0.3f },
        };

        mappings_[1] = {
            { SceneParam::driveAmt,    0.7f },
            { SceneParam::driveTone,  -0.3f },
        };

        mappings_[2] = {
            { SceneParam::delayFb,     0.4f },
            { SceneParam::revSize,     0.5f },
            { SceneParam::revPreDelay, 0.2f },
        };

        mappings_[3] = {
            { SceneParam::delayWidth,  0.3f },
            { SceneParam::revWidth,    0.2f },
        };
    }
};

