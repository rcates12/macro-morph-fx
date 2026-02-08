# Macro Morph Multi-FX — SPEC

> This file is the **single source of truth** for the plugin's design.
> Every task begins by reading this document.

## Goal
A creative multi-effect VST3 (Ableton-first) with:
- Fixed signal chain: Input → Filter → Drive → Delay → Reverb → Output
- 8 Scenes (snapshots of module parameters)
- Scene A/B selection + Morph interpolation
- 4 Macros that apply musical offsets AFTER morphing
- Hybrid UI: Performance page (Morph + Macros + Scenes) + collapsible Module panel

## Target platforms / format
- Plugin format: VST3
- Platforms: macOS + Windows
- DAW target: Ableton Live
- Latency: 0 samples (MVP)

## Audio IO
- Stereo in/out (2-in, 2-out)
- Process precision: float (MVP), optional double later

## Signal Flow
Input Gain
→ Filter (SVF LP/BP/HP)
→ Drive (waveshaper + tone)
→ Delay (tempo sync, feedback, tone, width, ping-pong)
→ Reverb (simple algorithmic)
→ Mix (dry/wet)
→ Output Gain

Notes:
- Mix should be click-free (smoothed).
- Bypass should be click-free (smoothed crossfade).

## Controls

### Performance (front panel)
- Scene A (1–8)
- Scene B (1–8)
- Morph (0..1)
- Macro1..Macro4 (0..1)
- Mix (0..1)
- Input Gain (dB)
- Output Gain (dB)
- Bypass

### Module panel (simple knobs)
Filter:
- Mode (LP/BP/HP) (discrete)
- Cutoff (Hz)
- Resonance (Q-ish)

Drive:
- Amount
- Tone

Delay:
- Sync (discrete note value)
- Feedback
- Tone
- Width
- PingPong (bool)

Reverb:
- Size
- Damping/Tone
- PreDelay (ms)
- Width

## Scenes

### Scene storage
- 8 scenes stored per preset/state.
- Each scene stores *module parameter values* (NOT macros, NOT morph).

### Scene parameter set (stored per scene)
Filter: mode, cutoff, resonance
Drive: amount, tone
Delay: sync, feedback, tone, width, pingpong
Reverb: size, damping, predelay, width

### Morph rules
- baseParams = lerp(sceneAParams, sceneBParams, morph)
- Discrete params:
  - Mode / Sync / PingPong:
    - if morph < 0.5 use A else use B
- dB params:
  - Interpolate in linear gain (convert dB → gain → lerp → dB if needed)

## Macros

### Macro application order
finalParams = applyMacros(baseParams, macroValues)

### Macro mapping model
Each macro affects multiple targets:
- target parameter id
- amount (-1..+1)
- curve (linear / exp / log / s-curve)
- clamp min/max

Macro values are smoothed.

## Smoothing / Click-free requirements
- Smooth at least: gains, mix, filter cutoff, delay feedback, delay tone, reverb size.
- Smoothing targets (initial):
  - gains/mix: 10–30 ms
  - cutoff: ~20 ms
  - feedback: ~50 ms
  - reverb size: ~100 ms
- Bypass: crossfade 10 ms

## Safety caps
- Delay feedback hard clamp <= 0.95
- Output soft clip or limiter is optional (v1.1); MVP can clamp output to avoid runaway

## Presets (later milestone)
- Store:
  - all automatable params
  - 8 scenes
  - macro mapping config (if not fixed)
- Factory presets: 20

## MVP milestones (high level)
1. JUCE VST3 skeleton + parameter registry + state save/load
2. DSP modules (Filter, Drive, Delay, Reverb)
3. Scene store + Morph engine
4. Macro mapping engine
5. UI polish + presets + edge-case testing
