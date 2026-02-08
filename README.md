# Macro Morph FX

A creative multi-effect **VST3 plugin** built with [JUCE](https://juce.com/). Morph between 8 scene snapshots, shape your sound with 4 configurable macro knobs, and perform expressive transitions — all in real time.

![Format](https://img.shields.io/badge/format-VST3-blue)
![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20macOS-lightgrey)
![DAW](https://img.shields.io/badge/DAW-Ableton%20Live-green)
![License](https://img.shields.io/badge/license-MIT-orange)

---

## What It Does

Macro Morph FX is a **stereo audio effect** with a fixed signal chain:

```
Input Gain → Filter → Drive → Delay → Reverb → Mix → Output Gain
```

What makes it unique is the **scene + morph + macro** performance system:

- **8 Scenes** — Each scene is a snapshot of all 14 module parameters (filter cutoff, drive amount, delay feedback, reverb size, etc.)
- **Morph** — A single slider smoothly interpolates between Scene A and Scene B, blending all parameters in real time
- **4 Macros** — Each macro knob offsets multiple parameters at once (e.g. "Filter Sweep" opens the cutoff and adds resonance). Macros support per-target curve types: linear, exponential, logarithmic, and S-curve
- **8 Factory Presets** — Init, Dark Ambience, Rhythmic Delay, Lo-Fi, Shimmer Pad, Dub Station, Distortion Box, Wide Stereo
- **User Presets** — Save and load your own `.mmfx` preset files

### DSP Modules

| Module   | Features                                              |
|----------|-------------------------------------------------------|
| **Filter** | State-variable (LP / BP / HP), 20 Hz–20 kHz, resonance |
| **Drive**  | Tanh waveshaper with tone control                     |
| **Delay**  | Tempo-synced (1/32 to 1 bar + dotted), feedback, tone, width, ping-pong |
| **Reverb** | Freeverb algorithm with size, damping, pre-delay, width |

All parameters are **click-free** with per-parameter smoothing (20–100 ms depending on parameter type). Bypass uses a 10 ms crossfade. Output is safety-clamped at ±4.0 to prevent runaway feedback.

---

## Getting Started

### Prerequisites

- **Git** (with submodule support)
- **CMake** 3.22 or later
- **C++ compiler:**
  - Windows: Visual Studio 2022 or later (with "Desktop development with C++" workload)
  - macOS: Xcode 14 or later

### Clone & Build

```bash
# Clone with JUCE submodule
git clone --recurse-submodules https://github.com/rcates12/macro-morph-fx.git
cd macro-morph-fx

# Configure
cmake -B build

# Build (Release)
cmake --build build --config Release
```

### Build Outputs

| Target        | Location                                                               |
|---------------|------------------------------------------------------------------------|
| **VST3**      | `build/MacroMorphFX_artefacts/Release/VST3/MacroMorphFX.vst3`        |
| **Standalone** | `build/MacroMorphFX_artefacts/Release/Standalone/MacroMorphFX.exe`   |

### Install the VST3

Copy the `.vst3` file to your system's VST3 folder:

- **Windows:** `C:\Program Files\Common Files\VST3\`
- **macOS:** `~/Library/Audio/Plug-Ins/VST3/`

Then rescan plugins in your DAW.

### Quick Test (No DAW Required)

Run the **Standalone** build directly — it works as a self-contained app with your system audio. Great for testing without a DAW.

---

## How to Use

### Performance Page (Main View)

1. **Select Scene A and Scene B** using the numbered buttons (1–8)
2. **Drag the Morph slider** to blend between the two scenes
3. **Turn the 4 Macro knobs** to apply expressive offsets (Filter Sweep, Dirt, Space, Width)
4. **Adjust Mix** (dry/wet), **Input Gain**, and **Output Gain**
5. **Store** your current sound into a scene slot with the Store → A / Store → B buttons

### Module Panel (Collapsible)

Click **▸ MODULES** to expand the module panel. Here you can directly edit the 14 DSP parameters for the active scene. Use the **EDIT: A / EDIT: B** toggle to choose which scene you're editing.

### Macro Config Panel (Collapsible)

Click **▸ MACRO CONFIG** to expand the macro configuration panel. For each of the 4 macros, you can assign up to 4 parameter targets, set the amount (-1 to +1), and choose a response curve (Linear, Exp, Log, S-Curve).

### Presets

- Use the **preset dropdown** in the header to switch between 8 factory presets
- Click **Save** to export your current state as a `.mmfx` file
- Click **Load** to import a previously saved `.mmfx` preset

---

## Project Structure

```
Source/
  Params.h              — Parameter ID/range/default registry
  SceneData.h           — Scene snapshot struct + morph interpolation
  MacroEngine.h         — Macro mapping engine (4 macros × N targets × curves)
  PresetData.h          — 8 factory presets (scenes + macro configs)
  PluginProcessor.h/cpp — Audio processing, state I/O, morph+macro pipeline
  PluginEditor.h/cpp    — Custom UI (performance + module panel + macro config)
  DSP/
    FilterModule.h      — SVF LP/BP/HP filter
    DriveModule.h       — Tanh waveshaper + tone
    DelayModule.h       — Tempo-synced delay with smoothed time + fractional read
    ReverbModule.h      — Freeverb + pre-delay

docs/
  SPEC.md               — Canonical design specification
  DECISIONS.md          — Technical decision log
  WORKFLOW.md           — Development workflow (Ralph Loop)
  STATE.md              — Current milestone status
```

---

## Contributing

Contributions are welcome! Whether it's bug fixes, new factory presets, DSP improvements, or UI enhancements — feel free to open an issue or submit a pull request.

### How to Contribute

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/my-feature`)
3. Follow the [Ralph Loop workflow](docs/WORKFLOW.md) — read SPEC.md first, keep changes minimal, include a handoff summary
4. Ensure the build succeeds with 0 errors and 0 warnings
5. Submit a pull request

### Ideas for Contributions

- Additional factory presets (target: 20)
- Custom knob / slider painting
- Level meters or waveform display
- Keyboard shortcuts (Ctrl+S / Ctrl+O for save/load)
- macOS / Linux testing
- Ableton automation testing

---

## License

This project is open source. See the repository for license details.
