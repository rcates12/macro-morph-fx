#pragma once

#include <juce_dsp/juce_dsp.h>

/**
 *  FilterModule — SVF (State Variable TPT) Filter
 *
 *  Wraps juce::dsp::StateVariableTPTFilter with the parameter interface
 *  defined in Params.h (filtMode, filtCutoffHz, filtReso).
 *
 *  Lane A — DSP modules (Source/DSP/*)
 */
class FilterModule
{
public:
    FilterModule() = default;

    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        filter.prepare (spec);
        filter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
        filter.setCutoffFrequency (8000.0f);
        filter.setResonance (0.707f);   // flat (no resonance boost)
    }

    void reset()
    {
        filter.reset();
    }

    /**
     *  Update filter parameters (call once per processBlock, before processing).
     *
     *  @param mode      0 = LP, 1 = BP, 2 = HP  (from Params::ID::filtMode)
     *  @param cutoffHz  20–20000 Hz               (from Params::ID::filtCutoff)
     *  @param reso01    0.0–1.0 normalised reso   (from Params::ID::filtReso)
     */
    void setParameters (int mode, float cutoffHz, float reso01)
    {
        // Map mode index to JUCE filter type
        switch (mode)
        {
            case 0:  filter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);   break;
            case 1:  filter.setType (juce::dsp::StateVariableTPTFilterType::bandpass);  break;
            case 2:  filter.setType (juce::dsp::StateVariableTPTFilterType::highpass);  break;
            default: filter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);   break;
        }

        filter.setCutoffFrequency (cutoffHz);

        // Map normalised resonance (0..1) to JUCE SVF resonance.
        // JUCE SVF resonance: values < 1/sqrt(2) ≈ 0.707 = more resonance (higher Q)
        //   reso01 = 0.0  → resonance = 0.707 (flat, no boost)
        //   reso01 = 1.0  → resonance = 0.05  (aggressive Q, near self-oscillation)
        // We linearly interpolate from 0.707 down to 0.05.
        constexpr float resoFlat = 0.707f;
        constexpr float resoMax  = 0.05f;
        float mappedReso = resoFlat + reso01 * (resoMax - resoFlat);
        filter.setResonance (mappedReso);
    }

    /**
     *  Process an audio block in-place.
     */
    template <typename ProcessContext>
    void process (const ProcessContext& context)
    {
        filter.process (context);
    }

private:
    juce::dsp::StateVariableTPTFilter<float> filter;
};

