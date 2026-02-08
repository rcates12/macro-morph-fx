#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "Params.h"
#include "SceneData.h"
#include "MacroEngine.h"
#include "DSP/FilterModule.h"
#include "DSP/DriveModule.h"
#include "DSP/DelayModule.h"
#include "DSP/ReverbModule.h"
#include "PresetData.h"

//==============================================================================
/**
 *  MacroMorphFX Audio Processor
 *
 *  Signal chain: Input Gain → Filter → Drive → Delay → Reverb → Mix → Output Gain
 */
class MacroMorphFXProcessor final : public juce::AudioProcessor
{
public:
    //==============================================================================
    MacroMorphFXProcessor();
    ~MacroMorphFXProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    /** The value tree state that holds all automatable parameters. */
    juce::AudioProcessorValueTreeState apvts;

    //==============================================================================
    /** Store current APVTS module param values into a scene slot (0–7). */
    void storeScene (int sceneIndex);

    /** Capture the current morphed+macro'd sound and store it into a scene slot (0–7).
        This flattens the current morph position + macro offsets into a clean scene. */
    void storeCurrentToScene (int sceneIndex);

    /** Load a factory preset (scenes + macros + reset performance params). */
    void loadFactoryPreset (int index);

    /** Set a single scene parameter value (used by editable module panel). */
    void setSceneParam (int sceneIndex, int paramIndex, float value);

    /** Save the current state to an XML file (user preset). Returns true on success. */
    bool saveUserPreset (const juce::File& file) const;

    /** Load state from an XML file (user preset). Returns true on success. */
    bool loadUserPreset (const juce::File& file);

    /** Read-only access to scene data (for UI display). */
    const SceneParams& getScene (int index) const    { return scenes_[static_cast<size_t> (std::clamp (index, 0, kNumScenes - 1))]; }

    /** Read-only access to macro engine (for UI display). */
    const MacroEngine& getMacroEngine() const        { return macroEngine_; }

    /** Mutable access to macro engine (for UI configuration). */
    MacroEngine& getMacroEngine()                    { return macroEngine_; }

    /** Last computed DSP values (morph + macro + smoothing).
        Safe to read from GUI thread for display purposes. */
    const SceneParams& getLastComputedParams() const { return lastComputedParams_; }

private:
    //==============================================================================
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    /** Load preset scene + macro data (no APVTS reset). */
    void loadFactoryPresetData (int index);

    // ── Preset tracking ────────────────────────────────────────────────
    int currentProgram_ = 0;

    // ── Scene + Morph + Macro (Lane D) ─────────────────────────────────
    std::array<SceneParams, kNumScenes> scenes_;
    MacroEngine macroEngine_;

    // DSP modules (Lane A) — in signal chain order
    FilterModule filterModule;
    DriveModule  driveModule;
    DelayModule  delayModule;
    ReverbModule reverbModule;

    // Gain helpers
    juce::dsp::Gain<float> inputGain;
    juce::dsp::Gain<float> outputGain;

    // Dry buffer for dry/wet mix
    juce::AudioBuffer<float> dryBuffer;

    // ── Parameter smoothing (per SPEC: cutoff 20ms, fb 50ms, etc.) ───
    std::array<juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>,
               SceneParam::kCount> smoothScene_;

    // ── Bypass crossfade (10ms per SPEC) ──────────────────────────────
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> bypassSmooth_;

    // ── Last computed params (for UI display, written on audio thread) ─
    SceneParams lastComputedParams_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MacroMorphFXProcessor)
};
