#pragma once
#include <array>
#include <string_view>

/**
 * ============================================================================
 *  MacroMorphFX — Parameter Registry
 * ============================================================================
 *
 *  RULE: Every plugin parameter ID, range, and default lives HERE.
 *        No parameter ID may be invented inline in a .cpp file.
 *        If you need a new parameter, add it to this file first.
 *
 *  RULE: Never rename an ID once shipped — it affects preset recall.
 *
 *  See docs/WORKFLOW.md — Rule 2 (Parameter Registry).
 *
 * ============================================================================
 */

namespace Params
{
    // ---------------------------------------------------------------------
    // Parameter IDs (never rename once shipped; this affects preset recall)
    // ---------------------------------------------------------------------
    namespace ID
    {
        // Global / performance
        static constexpr std::string_view bypass      = "bypass";
        static constexpr std::string_view inputGainDb = "inputGainDb";
        static constexpr std::string_view outputGainDb= "outputGainDb";
        static constexpr std::string_view mix         = "mix";

        static constexpr std::string_view sceneA      = "sceneA";   // 1..8 (discrete)
        static constexpr std::string_view sceneB      = "sceneB";   // 1..8 (discrete)
        static constexpr std::string_view morph       = "morph";    // 0..1
        static constexpr std::string_view macro1      = "macro1";   // 0..1
        static constexpr std::string_view macro2      = "macro2";
        static constexpr std::string_view macro3      = "macro3";
        static constexpr std::string_view macro4      = "macro4";

        // Filter
        static constexpr std::string_view filtMode    = "filtMode";     // 0..2 (LP,BP,HP)
        static constexpr std::string_view filtCutoff  = "filtCutoffHz"; // Hz
        static constexpr std::string_view filtReso    = "filtReso";     // 0..1 (mapped to Q)

        // Drive
        static constexpr std::string_view driveAmt    = "driveAmt";     // 0..1
        static constexpr std::string_view driveTone   = "driveTone";    // 0..1

        // Delay
        static constexpr std::string_view delaySync   = "delaySync";    // discrete
        static constexpr std::string_view delayFb     = "delayFeedback";// 0..0.95
        static constexpr std::string_view delayTone   = "delayTone";    // 0..1
        static constexpr std::string_view delayWidth  = "delayWidth";   // 0..1
        static constexpr std::string_view delayPingP  = "delayPingPong";// bool

        // Reverb
        static constexpr std::string_view revSize     = "revSize";      // 0..1
        static constexpr std::string_view revDamp     = "revDamp";      // 0..1
        static constexpr std::string_view revPreDelay = "revPreDelayMs";// 0..200
        static constexpr std::string_view revWidth    = "revWidth";     // 0..1
    }

    // ---------------------------------------------------------------------
    // Smoothing categories (keeps click-free changes consistent)
    // ---------------------------------------------------------------------
    enum class SmoothGroup
    {
        none,
        gain,      // input/output gain, mix
        cutoff,    // filter cutoff
        feedback,  // delay feedback
        timeish,   // reverb size / predelay style params
        tone       // tone controls
    };

    // Default smoothing times in milliseconds (initial targets)
    // Adjust later based on listening tests.
    constexpr float smoothingMs(SmoothGroup g)
    {
        switch (g)
        {
            case SmoothGroup::gain:     return 20.0f;
            case SmoothGroup::cutoff:   return 20.0f;
            case SmoothGroup::feedback: return 50.0f;
            case SmoothGroup::timeish:  return 100.0f;
            case SmoothGroup::tone:     return 30.0f;
            default:                    return 0.0f;
        }
    }

    // ---------------------------------------------------------------------
    // Parameter spec (for one canonical registry)
    // ---------------------------------------------------------------------
    enum class ParamType
    {
        float01,
        floatRange,
        choice,
        toggle
    };

    struct ParamSpec
    {
        std::string_view id;
        ParamType type;

        // For float parameters:
        float minValue;
        float maxValue;
        float defaultValue;

        // For choice parameters:
        int   numChoices;     // e.g. 8 for scenes
        int   defaultChoice;  // 0-based index

        SmoothGroup smooth;
    };

    // Note: choice/toggle use min/max/default as 0/1 placeholders; real handling is in processor.
    static constexpr std::array<ParamSpec, 25> all = {{

        // Global / performance
        { ID::bypass,      ParamType::toggle,    0.f,   1.f,   0.f,   2, 0, SmoothGroup::none },
        { ID::inputGainDb, ParamType::floatRange,-24.f, 24.f,  0.f,   0, 0, SmoothGroup::gain },
        { ID::outputGainDb,ParamType::floatRange,-24.f, 24.f,  0.f,   0, 0, SmoothGroup::gain },
        { ID::mix,         ParamType::float01,   0.f,   1.f,   1.f,   0, 0, SmoothGroup::gain },

        { ID::sceneA,      ParamType::choice,    0.f,   1.f,   0.f,   8, 0, SmoothGroup::none }, // default scene 1
        { ID::sceneB,      ParamType::choice,    0.f,   1.f,   0.f,   8, 1, SmoothGroup::none }, // default scene 2
        { ID::morph,       ParamType::float01,   0.f,   1.f,   0.f,   0, 0, SmoothGroup::gain }, // smooth like gain
        { ID::macro1,      ParamType::float01,   0.f,   1.f,   0.f,   0, 0, SmoothGroup::gain },
        { ID::macro2,      ParamType::float01,   0.f,   1.f,   0.f,   0, 0, SmoothGroup::gain },
        { ID::macro3,      ParamType::float01,   0.f,   1.f,   0.f,   0, 0, SmoothGroup::gain },
        { ID::macro4,      ParamType::float01,   0.f,   1.f,   0.f,   0, 0, SmoothGroup::gain },

        // Filter
        { ID::filtMode,    ParamType::choice,    0.f,   1.f,   0.f,   3, 0, SmoothGroup::none }, // 0=LP
        { ID::filtCutoff,  ParamType::floatRange,20.f,  20000.f, 8000.f,0,0, SmoothGroup::cutoff },
        { ID::filtReso,    ParamType::float01,   0.f,   1.f,   0.2f,  0, 0, SmoothGroup::tone },

        // Drive
        { ID::driveAmt,    ParamType::float01,   0.f,   1.f,   0.f,   0, 0, SmoothGroup::tone },
        { ID::driveTone,   ParamType::float01,   0.f,   1.f,   0.5f,  0, 0, SmoothGroup::tone },

        // Delay
        // Suggested sync choices later: 1/16, 1/8, 1/4, 1/2, 1 bar, dotted, triplet, etc.
        { ID::delaySync,   ParamType::choice,    0.f,   1.f,   0.f,   8, 2, SmoothGroup::none }, // default index 2
        { ID::delayFb,     ParamType::floatRange,0.f,   0.95f, 0.25f, 0, 0, SmoothGroup::feedback },
        { ID::delayTone,   ParamType::float01,   0.f,   1.f,   0.5f,  0, 0, SmoothGroup::tone },
        { ID::delayWidth,  ParamType::float01,   0.f,   1.f,   0.7f,  0, 0, SmoothGroup::tone },
        { ID::delayPingP,  ParamType::toggle,    0.f,   1.f,   0.f,   2, 0, SmoothGroup::none },

        // Reverb
        { ID::revSize,     ParamType::float01,   0.f,   1.f,   0.35f, 0, 0, SmoothGroup::timeish },
        { ID::revDamp,     ParamType::float01,   0.f,   1.f,   0.5f,  0, 0, SmoothGroup::tone },
        { ID::revPreDelay, ParamType::floatRange,0.f,   200.f,  10.f, 0, 0, SmoothGroup::timeish },
        { ID::revWidth,    ParamType::float01,   0.f,   1.f,   0.8f,  0, 0, SmoothGroup::tone },
    }};
} // namespace Params
