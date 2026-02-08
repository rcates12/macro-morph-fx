#pragma once

#include <juce_dsp/juce_dsp.h>
#include <cmath>

/**
 *  DriveModule — Waveshaper + Tone filter
 *
 *  Params from Params.h:
 *    driveAmt   (0..1)  — drive intensity (0 = clean, 1 = heavy distortion)
 *    driveTone  (0..1)  — post-drive tone (0 = dark, 1 = bright)
 *
 *  Implementation:
 *    - Soft-clip waveshaper: tanh(gain * x) where gain is derived from driveAmt
 *    - Post-drive tone filter: simple one-pole lowpass controlled by driveTone
 *
 *  Lane A — DSP modules (Source/DSP/*)
 */
class DriveModule
{
public:
    DriveModule() = default;

    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;

        // Tone filter: simple one-pole LPF (post-drive)
        toneFilter.prepare (spec);
        toneFilter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
        toneFilter.setCutoffFrequency (20000.0f);
        toneFilter.setResonance (0.707f);
    }

    void reset()
    {
        toneFilter.reset();
    }

    /**
     *  @param amount01  0..1 drive amount (from Params::ID::driveAmt)
     *  @param tone01    0..1 tone control (from Params::ID::driveTone)
     */
    void setParameters (float amount01, float tone01)
    {
        driveAmount = amount01;

        // Map tone 0..1 to cutoff frequency:
        //   0.0 → 800 Hz (dark)
        //   1.0 → 20000 Hz (bright / no filtering)
        float toneCutoff = 800.0f * std::pow (25.0f, tone01);  // 800 → 20000 Hz
        toneFilter.setCutoffFrequency (toneCutoff);
    }

    void process (juce::dsp::AudioBlock<float>& block)
    {
        if (driveAmount < 0.001f)
            return;  // No drive — skip processing entirely

        // Drive gain: map 0..1 to 1..50 (soft clip range)
        const float driveGain = 1.0f + driveAmount * 49.0f;

        // Waveshaper: tanh soft clip
        for (size_t ch = 0; ch < block.getNumChannels(); ++ch)
        {
            auto* data = block.getChannelPointer (ch);
            for (size_t s = 0; s < block.getNumSamples(); ++s)
            {
                data[s] = std::tanh (driveGain * data[s]);
            }
        }

        // Post-drive tone filter
        juce::dsp::ProcessContextReplacing<float> context (block);
        toneFilter.process (context);
    }

private:
    double sampleRate = 44100.0;
    float driveAmount = 0.0f;
    juce::dsp::StateVariableTPTFilter<float> toneFilter;
};

