# MacroMorphFX — Project State

> Updated at every handoff. Gives a snapshot of where the project stands.

---

## Current Milestone

**Milestone 8 — User Presets + Macro Curves + UI Polish** (COMPLETE)

| Task                                                | Status |
|-----------------------------------------------------|--------|
| Macro curve types (exp/log/s-curve) in MacroEngine  | Done   |
| Macro curve serialization (state save/load)          | Done   |
| Macro curve selector in macro config UI panel        | Done   |
| User preset save (FileChooser + XML → .mmfx file)   | Done   |
| User preset load (FileChooser → setStateInformation) | Done   |
| Save/Load buttons in header bar                      | Done   |
| Tooltips on all major controls                       | Done   |
| Build succeeds (0 errors, 0 warnings)                | Done   |

## Previous Milestones

- Milestone 0 — Skeleton Build (COMPLETE)
- Milestone 1 — Parameter Wiring + Filter (COMPLETE)
- Milestone 2 — Full DSP Signal Chain (COMPLETE)
- Milestone 3 — Scene Storage + Morph Engine + Macros (COMPLETE)
- Milestone 4 — Custom Performance UI + Scene Editing (COMPLETE)
- Milestone 5 — Module Panel + Parameter Smoothing (COMPLETE)
- Milestone 6 — Macro Config UI + Factory Presets + Store Feedback (COMPLETE)
- Milestone 7 — Scene Editing + Bypass Crossfade + Delay Smoothing (COMPLETE)

## Build Outputs

| Target       | Location                                                          |
|--------------|-------------------------------------------------------------------|
| VST3         | `build/MacroMorphFX_artefacts/Release/VST3/MacroMorphFX.vst3`   |
| Standalone   | `build/MacroMorphFX_artefacts/Release/Standalone/MacroMorphFX.exe`|
| PluginHost   | `build/JUCE/extras/AudioPluginHost/AudioPluginHost_artefacts/Release/AudioPluginHost.exe` |

## Build Commands

```
cmake -B build -G "Visual Studio 18 2026"
cmake --build build --config Release
```

## Signal Chain

```
Input Gain → Filter (SVF LP/BP/HP) → Drive (tanh + tone) → Delay (sync/fb/pp) → Reverb (Freeverb) → Mix → Output Gain → Bypass Crossfade → Safety Clamp (±4.0)
```

## Morph + Macro + Smoothing Pipeline

```
Scene A ─┐
         ├─ morph(A, B, t) ─→ applyMacros() ─→ SmoothedValue[] ─→ DSP modules
Scene B ─┘                                           │
                                              lastComputedParams_ → UI display
                                                     │
                          Module panel sliders ─→ setSceneParam() ─→ scenes_[]
```

- 8 factory presets: Init, Dark Ambience, Rhythmic Delay, Lo-Fi, Shimmer Pad, Dub Station, Distortion Box, Wide Stereo
- Each preset defines 8 scenes + 4 macro configs
- Preset selector in header loads scenes + macros + resets performance params
- Morph: linear lerp for continuous, threshold at 0.5 for discrete
- 4 macros with configurable mappings (editable in Macro Config panel)
- Per-target curve types: Linear, Exp (x²), Log (√x), S-Curve (smoothstep)
- SmoothedValue per param: cutoff 20ms, feedback 50ms, reverb 100ms, tone 30ms
- Bypass: 10ms SmoothedValue crossfade between dry and processed
- Delay: SmoothedValue (50ms) + fractional read (linear interpolation) for click-free tempo changes
- Output: hard clamp at ±4.0 to prevent runaway

## UI Layout

```
┌──────────────────────────────────────────────────┐
│  MACRO MORPH FX  [Preset▼][Save][Load] [BYPASS]  │  Header
├──────────────────────────────────────────────────┤
│  SCENE A  [1][2][3][4][5][6][7][8]               │  Scene A row
│           ═══════════ MORPH ═══════════           │  Morph slider
│  SCENE B  [1][2][3][4][5][6][7][8]               │  Scene B row
├──────────────────────────────────────────────────┤
│  MACRO 1     MACRO 2     MACRO 3     MACRO 4    │  Macro knobs
│    ◐           ◐           ◐           ◐        │
│  Flt Sweep    Dirt        Space       Width      │
├──────────────────────────────────────────────────┤
│  MIX    IN GAIN   OUT GAIN   [STORE→A][STORE→B] │  Bottom row
│   ◐       ◐         ◐                           │
│  [▸ MODULES] [▸ MACRO CONFIG]                    │  Collapse toggles
├ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ┤
│  FILTER        DRIVE         DELAY      REVERB   │  Module panel
│  Mode  [LP══] Amt   [═══]  Sync [1/8═] Size [══]│  (EDITABLE)
│  Cut [═1kHz═] Tone  [═══]  FB   [═══]  Damp [══]│
│  Reso  [═══]                Tone [═══]  PDly [══]│
│                              Width[═══]  Width[══]│  [EDIT: A]
│                              PP  [Off═]           │
├ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ┤
│  MACRO 1         MACRO 2         MACRO 3     MACRO 4  │  Macro config
│  [Cutoff▼][=][Lin▼] [DrvAmt▼][=][Exp▼] ...          │  (expanded)
│  [Reso  ▼][=][Lin▼] [DrvTn ▼][=][Lin▼] ...          │  param+amt+curve
│  [None  ▼][=][Lin▼] [None  ▼][=][Lin▼] ...          │
│  [None  ▼][=][Lin▼] [None  ▼][=][Lin▼] ...          │
└──────────────────────────────────────────────────────┘
```

## File Map

```
Source/
  Params.h              — 25 parameter IDs/ranges/defaults + smoothing groups
  SceneData.h           — SceneParams struct, 14-param scene snapshot, morph()
  MacroEngine.h         — MacroTarget, MacroEngine (4 macros × N targets)
  PresetData.h          — 8 factory presets (scenes + macro configs)
  PluginProcessor.h/cpp — APVTS, morph+macro+smoothing pipeline, bypass crossfade, state I/O
  PluginEditor.h/cpp    — Custom performance UI + editable module panel + macro config
  DSP/
    FilterModule.h      — SVF LP/BP/HP filter
    DriveModule.h       — Tanh waveshaper + tone filter
    DelayModule.h       — Tempo-synced delay with smoothed time + fractional read
    ReverbModule.h      — Freeverb + pre-delay
```

## Known Issues

- No Ableton installed yet — test via Standalone and AudioPluginHost.
- `COPY_PLUGIN_AFTER_BUILD` disabled (needs admin).
- Macro config UI edits MacroEngine from GUI thread (no lock) — safe for MVP but not production-grade.
- Preset selector doesn't indicate unsaved changes (e.g. "Init*").
- Module panel edits Scene A/B directly — no undo support.
- User presets saved as binary-wrapped XML (.mmfx) — not human-readable.

## Next Up

1. Ableton testing — install and verify VST3 scanning + automation
2. Additional factory presets (target: 20 per SPEC, currently 8)
3. Keyboard shortcuts (e.g. Ctrl+S to save, Ctrl+O to load)
4. Visual refinements — custom knob painting, level meters, waveform display
5. Production hardening — thread-safe macro config, undo support
