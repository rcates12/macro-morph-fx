# Decisions Log

> Every notable technical choice is recorded here with date and rationale.
> New entries go at the **top** of the log.

---

## 2026-02-08 — User Presets + Macro Curves + UI Polish

### Macro curve types: exp(x²), log(√x), s-curve(smoothstep) per target
**Rationale:** The SPEC defines "curve (linear / exp / log / s-curve)" as a per-target property on macro mappings. Exponential (`x²`) makes the macro respond more at high values, logarithmic (`√x`) more at low values, and s-curve (`3x² - 2x³`) provides a smooth ease-in/out. The curve is applied to the raw 0..1 macro knob value before multiplying by the target's amount. This is per-target (not per-macro), so one macro can have linear cutoff with exponential drive.

### User presets as .mmfx binary files
**Rationale:** The user preset format reuses `getStateInformation` / `setStateInformation` — the same binary-wrapped XML format that JUCE uses for DAW state. This means user presets contain everything: APVTS params, 8 scenes, macro mappings with curves. The `.mmfx` extension is unique to MacroMorphFX, making file association straightforward. `FileChooser::launchAsync` is used for non-blocking OS dialogs.

### Curve selector as third element per macro config row
**Rationale:** Each macro target row already had [Param ▼][Amount ═]. Adding [Curve ▼] as a third element keeps the layout consistent and discoverable. The macro config panel height increased from 155px to 165px to accommodate slightly wider rows. Factory presets default to linear curves for backward compatibility.

### Tooltips on all major controls
**Rationale:** The user is new to VST3 development and benefits from in-context help. Tooltips provide non-intrusive guidance: hovering over the morph slider explains "Blend between Scene A and Scene B", hovering over a macro knob shows its mapping description. This is cheap to implement (one `setTooltip()` call per component) and improves discoverability without cluttering the UI.

---

## 2026-02-08 — Scene Editing + Bypass Crossfade + Delay Smoothing

### Click-free bypass via SmoothedValue crossfade (10ms)
**Rationale:** The SPEC requires "Bypass: crossfade 10 ms". Previously, bypass just returned early from `processBlock` — a hard cut that causes audible clicks. Now, `bypassSmooth_` ramps between 0 (unbypassed) and 1 (bypassed) over 10ms. When transitioning, both dry and processed paths run, and the output is crossfaded: `wet + bv * (dry - wet)`. When fully bypassed and settled, all DSP is skipped (CPU savings). The existing `dryBuffer` (saved before input gain) is reused for the bypass dry signal — no extra buffer needed.

### Interpolated delay time with 50ms ramp
**Rationale:** Changing delay sync (e.g. 1/8 → 1/4) or BPM previously caused an instant jump in the delay read position, producing a click. Now `smoothDelay_` ramps between old and new delay times over 50ms, and the read position uses linear interpolation between adjacent samples (fractional delay). This eliminates clicks during morph transitions between scenes with different delay sync values.

### Output safety clamp at ±4.0
**Rationale:** The SPEC states "MVP can clamp output to avoid runaway". Feedback loops (especially delay with high feedback + drive) can accumulate energy and clip DAW outputs. A hard clamp at ±4.0 (~12 dBFS) after output gain catches runaway signals. This is more conservative than ±1.0 (allows headroom for transients) but prevents damage. A soft clipper (tanh) could replace this in v2.

### Editable module panel (sliders write to active scene)
**Rationale:** Previously the module panel was read-only (showed computed DSP values). This made sound design difficult — the only way to edit scenes was to set up the morph/macros and use "Store". Now each of the 14 scene parameters has a draggable slider that writes directly to the active scene via `setSceneParam()`. An "EDIT: A/B" toggle lets the user choose which scene to modify. The 15Hz timer refreshes slider values from the scene data unless the slider is being dragged (`isMouseButtonDown()` check prevents fighting the user).

### setSceneParam atomic write without mutex
**Rationale:** `setSceneParam()` writes a single `float` in `scenes_[idx].values[paramIdx]`. On x86, aligned float writes are atomic. The audio thread reads `scenes_[]` in `processBlock`. Since it's a single float (not a struct), there's no tearing risk. This is the same safety model used for `lastComputedParams_` (audio→GUI) — now applied in reverse (GUI→audio). No mutex, no priority inversion.

---

## 2026-02-08 — Macro Config UI + Factory Presets

### 8 factory presets via JUCE program API
**Rationale:** JUCE's `getNumPrograms()` / `setCurrentProgram()` API is the standard way DAWs discover and switch presets. We define 8 factory presets in `PresetData.h`, each containing 8 scenes + 4 macro configurations. Presets are generated programmatically: the "Init" preset uses hand-crafted base scenes, while other presets transform the base scenes (e.g. "Dark Ambience" scales cutoff by 0.35×, adds reverb). This avoids 64 hand-crafted scene definitions while keeping each preset musically distinct.

### Preset data extracted from initDefaultScenes
**Rationale:** The original `initDefaultScenes()` duplicated scene data that now lives in `PresetData.h::makeBaseScenes()`. To eliminate duplication, the constructor now calls `loadFactoryPresetData(0)` (Init preset) instead. `loadFactoryPreset()` additionally resets all APVTS performance params to defaults, while `loadFactoryPresetData()` only loads scenes + macros (safe for constructor use before the editor exists).

### Macro config panel uses fixed 4-slot layout per macro
**Rationale:** Each macro has 4 target slots (ComboBox + amount Slider). Rather than dynamically adding/removing rows, we use fixed slots where unused slots show "None". This simplifies the UI code (no dynamic component creation) and caps complexity. 4 slots per macro is sufficient — the defaults use at most 3. The ComboBox only offers the 11 continuous scene params (discrete params are excluded since MacroEngine skips them).

### Store button flash feedback via timer countdown
**Rationale:** When the user clicks "STORE → A", the button text changes to "STORED!" with accent color for ~1 second (15 timer ticks at 15 Hz), then reverts. This provides clear visual confirmation without modal dialogs or toast notifications. The existing 15 Hz timer handles the countdown — no extra timer needed.

### Panels stack vertically with dynamic height
**Rationale:** Module panel and Macro Config panel can both be open simultaneously. The editor height adjusts dynamically: `500 + (modules ? 140 : 0) + (macroConfig ? 155 : 0)`. This avoids mutual exclusion and lets the user see DSP values while editing macro mappings. Maximum height (both open) is 795px — tall but acceptable for a configuration mode.

---

## 2026-02-08 — Module Panel + Parameter Smoothing

### Block-level SmoothedValue for morph transitions
**Rationale:** The SPEC requires click-free parameter changes (cutoff ~20ms, feedback ~50ms, reverb ~100ms). We use `juce::SmoothedValue<float>` per scene parameter, initialized in `prepareToPlay` with per-SPEC smoothing times. In `processBlock`, targets are set from the morph+macro output, then the smoother is advanced by `numSamples`. Discrete params (filtMode, delaySync, delayPingP) snap instantly via `setCurrentAndTargetValue()`. This eliminates clicks when morphing between scenes or changing macros.

### Module panel as read-only display (not editable)
**Rationale:** The DSP reads from the scene/morph/macro pipeline, not from APVTS module params. Making the module panel knobs editable would require deciding which scene to write to (A or B?) and handling the circular dependency with morph. For MVP, the panel shows the final computed DSP values (read-only), updated at 15 Hz from `lastComputedParams_`. The user shapes sound via morph + macros + Store buttons. Editable module knobs are deferred to a future milestone.

### lastComputedParams_ shared between audio and GUI threads without locks
**Rationale:** The struct `SceneParams` (14 floats) is written by the audio thread in `processBlock` and read by the GUI thread's timer at 15 Hz. On x86, aligned float writes are atomic at the hardware level, so occasional tearing across the struct is the worst case — and for display purposes (not audio), this is completely acceptable. No mutex needed, no priority inversion risk.

---

## 2026-02-08 — Custom Performance UI

### Custom editor replaces GenericAudioProcessorEditor
**Rationale:** The GenericEditor showed all 25 APVTS params as a flat list, which was confusing — module knobs didn't drive DSP (scene pipeline does). The custom UI groups controls by function: scene selectors, morph slider, macro knobs, performance knobs, store buttons. This matches the SPEC's "Performance page" concept.

### Scene buttons use timer-based highlight (not APVTS attachment)
**Rationale:** Scene A/B are choice params in APVTS, but the UI uses 8 radio-style TextButtons (not a ComboBox). A 15 Hz timer polls the APVTS parameter value and highlights the active button. This avoids complex attachment logic for a simple visual state. The button click uses `setValueNotifyingHost()` with gesture wrapping for proper DAW automation support.

### "Store" captures morph+macro snapshot
**Rationale:** `storeCurrentToScene()` computes the same morph+macro pipeline as `processBlock` and saves the result into a scene slot. This "flattens" a morph position into a clean scene. The user hears a sound they like, hits Store, and the scene now contains that sound. This is more intuitive than storing raw APVTS knob values (which don't drive DSP).

### JUCE FontOptions API (not deprecated Font(float))
**Rationale:** JUCE 8 deprecates `Font(float)` in favor of `Font(FontOptions)`. Using `FontOptions` avoids deprecation warnings and is forward-compatible. All label fonts use `juce::FontOptions(fontSize)`.

---

## 2026-02-08 — Scene + Morph + Macro architecture

### DSP reads from morph pipeline, not APVTS module params
**Rationale:** When morph is active, the DSP module parameters are derived from scene interpolation + macro offsets, not from the APVTS module parameter knobs directly. This keeps the morph system authoritative. A custom UI will replace the GenericEditor to provide proper scene editing (store/recall). The module knobs in APVTS still exist for automation compatibility and scene editing via `storeScene()`.

### Scene data stored as flat float array (14 params)
**Rationale:** Using `float values[14]` indexed by `SceneParam::Index` enum makes morph interpolation trivial (loop over indices). The `SceneParam::info` table stores metadata (ranges, defaults, discrete flags) alongside the Params::ID references, keeping everything in sync.

### Macros use linear offset as fraction of param range
**Rationale:** `offset = macroValue * mapping.amount * (paramMax - paramMin)` is simple and predictable. amount = 1.0 means the macro can sweep the full range. Non-linear curves (exp/log/s-curve) are deferred to a later milestone — linear works well for MVP.

### Default macro mappings provided (Filter Sweep, Dirt, Space, Width)
**Rationale:** Provides immediate musical value for testing and first-run experience. Mappings are configurable (per-preset data, stored in state), but having sensible defaults means the plugin is useful out of the box.

### State format: wrapped XML with backward compat
**Rationale:** The new `MacroMorphFXPreset` root element wraps APVTS + Scenes + MacroMappings. Old saves (with just the APVTS ValueTree root) are loaded via fallback path, preserving backward compatibility with Milestone 0–2 states.

### 8 distinct factory scenes
**Rationale:** Each scene has a unique character (Clean, Dark Drive, Bright Echo, Wide Space, Crushed, Dub, Shimmer, Telephone) so morph transitions are audible and interesting during testing. These serve as the default scene bank for every new preset.

---

## 2026-02-07 — Initial project decisions

### Target DAW: Ableton Live
**Rationale:** Primary user workflow is Ableton-based. VST3 is Ableton's preferred plugin format on Windows. Other DAWs can be supported later without architectural changes.

### Plugin format: VST3 only (AU later if needed)
**Rationale:** Ableton on Windows uses VST3. Targeting one format keeps the initial build simple. Adding AU for macOS is a one-line CMake change when needed.

### Platforms: macOS + Windows
**Rationale:** Covers the vast majority of Ableton users. Linux is not an Ableton-supported platform.

### UI style: Hybrid (Performance + collapsible Module panel)
**Rationale:** Performance page (Morph, Macros, Scene selectors) is what you use live. Module panel (individual effect knobs) is for sound design. Collapsible keeps the default view clean.

### Signal chain: Input → Filter → Drive → Delay → Reverb → Mix → Output
**Rationale:** Fixed order simplifies the MVP. Filter before drive gives more predictable distortion. Delay before reverb is the conventional send-style order. Re-orderable chain can be a v2 feature.

### Scenes: 8 scenes stored in plugin state
**Rationale:** 8 is enough for variety without overwhelming the UI. Scenes store only module parameters (not macros/morph), keeping the snapshot concept clean.

### Discrete morph rule: if morph < 0.5 choose A, else B
**Rationale:** Discrete parameters (filter mode, delay sync, ping-pong toggle) can't be interpolated continuously. Threshold at 0.5 gives equal weight to both scenes and is predictable for the user.

### Macro order: apply macros AFTER morphing base scene params
**Rationale:** Morph sets the base sound (Scene A/B blend), then macros add musical offsets on top. This keeps the mental model simple: morph = foundation, macros = expression.

### MVP: 0 latency, stable + click-free, minimal DSP complexity
**Rationale:** Latency-free operation is critical for live use in Ableton. Click-free transitions are a baseline quality bar. Keep DSP simple initially to get the architecture right before optimizing.

---

## 2026-02-07 — Build system decisions

### CMake over Projucer
**Rationale:** CMake is the officially recommended approach for new JUCE projects. It integrates with any IDE/editor, works from the command line, and avoids the Projucer GUI dependency. Better for CI/CD in the future.

### JUCE added as a Git submodule
**Rationale:** Submodule pins an exact commit, making builds reproducible. Anyone cloning the repo gets the right version automatically with `git submodule update --init`. Simpler than FetchContent for a single large dependency.

### Pass-through processor as first milestone
**Rationale:** Proves the full toolchain works end-to-end (CMake, JUCE, MSVC, VST3 output, Ableton scan) before adding any real DSP. Reduces variables when debugging build issues.
