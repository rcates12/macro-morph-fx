#pragma once

/**
 * ============================================================================
 *  MacroMorphFX — Factory Preset Definitions
 * ============================================================================
 *
 *  Defines 8 factory presets, each containing:
 *    - 8 scene snapshots (SceneParams)
 *    - 4 macro configurations (targets + amounts)
 *
 *  The "Init" preset uses the same scenes as the original initDefaultScenes().
 *  Other presets transform the base scenes to create different characters.
 *
 *  See docs/SPEC.md — "Presets" section.
 * ============================================================================
 */

#include "SceneData.h"
#include "MacroEngine.h"
#include <array>
#include <algorithm>
#include <cmath>

// ─── Constants ─────────────────────────────────────────────────────────────

static constexpr int kNumFactoryPresets = 8;

static constexpr const char* kFactoryPresetNames[kNumFactoryPresets] = {
    "Init",
    "Dark Ambience",
    "Rhythmic Delay",
    "Lo-Fi",
    "Shimmer Pad",
    "Dub Station",
    "Distortion Box",
    "Wide Stereo"
};

// ─── Factory macro config struct ───────────────────────────────────────────

struct FactoryMacroConfig
{
    static constexpr int kMaxTargets = 4;
    int numTargets = 0;
    MacroTarget targets[kMaxTargets] = {};
};

struct FactoryPreset
{
    std::array<SceneParams, kNumScenes> scenes;
    std::array<FactoryMacroConfig, MacroEngine::kNumMacros> macros;
};

// ─── Helper: transform all scenes for a given parameter ────────────────────

inline void transformScenes (std::array<SceneParams, kNumScenes>& scenes,
                             int paramIdx, float addAmount, float mulAmount = 1.0f)
{
    const auto& inf = SceneParam::info[static_cast<size_t> (paramIdx)];

    for (auto& s : scenes)
    {
        s.values[paramIdx] = std::clamp (
            s.values[paramIdx] * mulAmount + addAmount,
            inf.minVal, inf.maxVal);
    }
}

// ─── Build the 8 base scenes (identical to original initDefaultScenes) ─────

inline std::array<SceneParams, kNumScenes> makeBaseScenes()
{
    std::array<SceneParams, kNumScenes> s;

    // 1: Clean default
    s[0] = SceneParams::createDefault();

    // 2: Dark Drive
    s[1] = SceneParams::createDefault();
    s[1].values[SceneParam::filtCutoff]  = 2000.f;
    s[1].values[SceneParam::filtReso]    = 0.5f;
    s[1].values[SceneParam::driveAmt]    = 0.4f;
    s[1].values[SceneParam::driveTone]   = 0.3f;
    s[1].values[SceneParam::revSize]     = 0.7f;

    // 3: Bright Echo (HP)
    s[2] = SceneParams::createDefault();
    s[2].values[SceneParam::filtMode]    = 2.f;
    s[2].values[SceneParam::filtCutoff]  = 500.f;
    s[2].values[SceneParam::delaySync]   = 4.f;
    s[2].values[SceneParam::delayFb]     = 0.6f;
    s[2].values[SceneParam::delayWidth]  = 1.0f;

    // 4: Wide Space
    s[3] = SceneParams::createDefault();
    s[3].values[SceneParam::revSize]     = 0.85f;
    s[3].values[SceneParam::revWidth]    = 1.0f;
    s[3].values[SceneParam::revPreDelay] = 50.f;
    s[3].values[SceneParam::delayWidth]  = 1.0f;
    s[3].values[SceneParam::delayPingP]  = 1.f;

    // 5: Crushed (BP, heavy drive)
    s[4] = SceneParams::createDefault();
    s[4].values[SceneParam::filtMode]    = 1.f;
    s[4].values[SceneParam::filtCutoff]  = 1200.f;
    s[4].values[SceneParam::filtReso]    = 0.7f;
    s[4].values[SceneParam::driveAmt]    = 0.8f;
    s[4].values[SceneParam::driveTone]   = 0.7f;

    // 6: Dub
    s[5] = SceneParams::createDefault();
    s[5].values[SceneParam::delaySync]   = 3.f;
    s[5].values[SceneParam::delayFb]     = 0.7f;
    s[5].values[SceneParam::delayTone]   = 0.25f;
    s[5].values[SceneParam::delayPingP]  = 1.f;
    s[5].values[SceneParam::revSize]     = 0.5f;

    // 7: Shimmer
    s[6] = SceneParams::createDefault();
    s[6].values[SceneParam::filtCutoff]  = 12000.f;
    s[6].values[SceneParam::revSize]     = 0.9f;
    s[6].values[SceneParam::revDamp]     = 0.2f;
    s[6].values[SceneParam::revWidth]    = 1.0f;
    s[6].values[SceneParam::revPreDelay] = 30.f;

    // 8: Telephone (narrow BP, dry)
    s[7] = SceneParams::createDefault();
    s[7].values[SceneParam::filtMode]    = 1.f;
    s[7].values[SceneParam::filtCutoff]  = 1500.f;
    s[7].values[SceneParam::filtReso]    = 0.6f;
    s[7].values[SceneParam::driveAmt]    = 0.2f;
    s[7].values[SceneParam::delayFb]     = 0.0f;
    s[7].values[SceneParam::revSize]     = 0.1f;

    return s;
}

// ─── Default macro configuration (matches MacroEngine::initDefaultMappings) ─

inline std::array<FactoryMacroConfig, MacroEngine::kNumMacros> makeDefaultMacros()
{
    std::array<FactoryMacroConfig, MacroEngine::kNumMacros> m {};

    // Macro 1: Filter Sweep
    m[0].numTargets = 2;
    m[0].targets[0] = { SceneParam::filtCutoff, 0.5f };
    m[0].targets[1] = { SceneParam::filtReso,   0.3f };

    // Macro 2: Dirt
    m[1].numTargets = 2;
    m[1].targets[0] = { SceneParam::driveAmt,   0.7f };
    m[1].targets[1] = { SceneParam::driveTone, -0.3f };

    // Macro 3: Space
    m[2].numTargets = 3;
    m[2].targets[0] = { SceneParam::delayFb,     0.4f };
    m[2].targets[1] = { SceneParam::revSize,     0.5f };
    m[2].targets[2] = { SceneParam::revPreDelay, 0.2f };

    // Macro 4: Width
    m[3].numTargets = 2;
    m[3].targets[0] = { SceneParam::delayWidth, 0.3f };
    m[3].targets[1] = { SceneParam::revWidth,   0.2f };

    return m;
}

// ─── Build all 8 factory presets ────────────────────────────────────────────

inline std::array<FactoryPreset, kNumFactoryPresets> createFactoryPresets()
{
    std::array<FactoryPreset, kNumFactoryPresets> p {};
    auto base         = makeBaseScenes();
    auto defaultMacros = makeDefaultMacros();

    // ── 0: Init ─────────────────────────────────────────────────────────
    p[0].scenes = base;
    p[0].macros = defaultMacros;

    // ── 1: Dark Ambience ────────────────────────────────────────────────
    p[1].scenes = base;
    transformScenes (p[1].scenes, SceneParam::filtCutoff, 0.f,  0.35f);
    transformScenes (p[1].scenes, SceneParam::revSize,    0.3f);
    transformScenes (p[1].scenes, SceneParam::revDamp,    0.15f);
    transformScenes (p[1].scenes, SceneParam::driveTone,  0.f,  0.5f);
    transformScenes (p[1].scenes, SceneParam::delayTone,  0.f,  0.5f);
    p[1].macros = defaultMacros;
    p[1].macros[0].numTargets = 2;
    p[1].macros[0].targets[0] = { SceneParam::filtCutoff, 0.8f };
    p[1].macros[0].targets[1] = { SceneParam::revDamp,  -0.3f };
    p[1].macros[2].numTargets = 2;
    p[1].macros[2].targets[0] = { SceneParam::revSize,     0.6f };
    p[1].macros[2].targets[1] = { SceneParam::revPreDelay, 0.4f };

    // ── 2: Rhythmic Delay ───────────────────────────────────────────────
    p[2].scenes = base;
    transformScenes (p[2].scenes, SceneParam::delayFb,    0.2f);
    transformScenes (p[2].scenes, SceneParam::delayWidth, 0.15f);
    transformScenes (p[2].scenes, SceneParam::revSize,    0.f, 0.5f);
    p[2].macros = defaultMacros;
    p[2].macros[2].numTargets = 3;
    p[2].macros[2].targets[0] = { SceneParam::delayFb,    0.5f };
    p[2].macros[2].targets[1] = { SceneParam::delayWidth, 0.3f };
    p[2].macros[2].targets[2] = { SceneParam::delayTone, -0.4f };

    // ── 3: Lo-Fi ────────────────────────────────────────────────────────
    p[3].scenes = base;
    transformScenes (p[3].scenes, SceneParam::filtCutoff, 0.f, 0.5f);
    transformScenes (p[3].scenes, SceneParam::driveAmt,   0.25f);
    transformScenes (p[3].scenes, SceneParam::filtReso,   0.1f);
    p[3].macros = defaultMacros;
    p[3].macros[1].numTargets = 3;
    p[3].macros[1].targets[0] = { SceneParam::driveAmt,    0.5f };
    p[3].macros[1].targets[1] = { SceneParam::driveTone,  -0.4f };
    p[3].macros[1].targets[2] = { SceneParam::filtCutoff, -0.3f };

    // ── 4: Shimmer Pad ──────────────────────────────────────────────────
    p[4].scenes = base;
    transformScenes (p[4].scenes, SceneParam::filtCutoff, 0.f, 1.5f);
    transformScenes (p[4].scenes, SceneParam::revSize,    0.4f);
    transformScenes (p[4].scenes, SceneParam::revDamp,    0.f, 0.3f);
    transformScenes (p[4].scenes, SceneParam::revWidth,   0.2f);
    transformScenes (p[4].scenes, SceneParam::driveAmt,   0.f, 0.3f);
    p[4].macros = defaultMacros;
    p[4].macros[0].numTargets = 2;
    p[4].macros[0].targets[0] = { SceneParam::filtCutoff, 0.4f };
    p[4].macros[0].targets[1] = { SceneParam::revSize,    0.3f };

    // ── 5: Dub Station ──────────────────────────────────────────────────
    p[5].scenes = base;
    transformScenes (p[5].scenes, SceneParam::delayFb,   0.25f);
    transformScenes (p[5].scenes, SceneParam::delayTone, 0.f, 0.4f);
    transformScenes (p[5].scenes, SceneParam::revSize,   0.15f);
    p[5].macros = defaultMacros;
    p[5].macros[2].numTargets = 3;
    p[5].macros[2].targets[0] = { SceneParam::delayFb,    0.3f };
    p[5].macros[2].targets[1] = { SceneParam::revSize,    0.4f };
    p[5].macros[2].targets[2] = { SceneParam::delayTone, -0.3f };

    // ── 6: Distortion Box ───────────────────────────────────────────────
    p[6].scenes = base;
    transformScenes (p[6].scenes, SceneParam::driveAmt,   0.4f);
    transformScenes (p[6].scenes, SceneParam::filtCutoff, 0.f, 0.6f);
    transformScenes (p[6].scenes, SceneParam::revSize,    0.f, 0.3f);
    transformScenes (p[6].scenes, SceneParam::delayFb,    0.f, 0.5f);
    p[6].macros = defaultMacros;
    p[6].macros[1].numTargets = 2;
    p[6].macros[1].targets[0] = { SceneParam::driveAmt,  0.4f };
    p[6].macros[1].targets[1] = { SceneParam::driveTone, 0.5f };

    // ── 7: Wide Stereo ──────────────────────────────────────────────────
    p[7].scenes = base;
    transformScenes (p[7].scenes, SceneParam::delayWidth,  0.2f);
    transformScenes (p[7].scenes, SceneParam::revWidth,    0.2f);
    transformScenes (p[7].scenes, SceneParam::revPreDelay, 15.f);
    for (auto& s : p[7].scenes)
    {
        if (s.values[SceneParam::delayFb] > 0.1f)
            s.values[SceneParam::delayPingP] = 1.f;
    }
    p[7].macros = defaultMacros;
    p[7].macros[3].numTargets = 3;
    p[7].macros[3].targets[0] = { SceneParam::delayWidth,  0.4f };
    p[7].macros[3].targets[1] = { SceneParam::revWidth,    0.3f };
    p[7].macros[3].targets[2] = { SceneParam::revPreDelay, 0.3f };

    return p;
}

