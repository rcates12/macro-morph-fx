#pragma once

#include <juce_dsp/juce_dsp.h>

/**
 *  ReverbModule — Simple algorithmic reverb
 *
 *  Wraps juce::dsp::Reverb (Freeverb algorithm) with the parameter interface
 *  defined in Params.h.
 *
 *  Params from Params.h:
 *    revSize      (0..1)     — room size
 *    revDamp      (0..1)     — damping / tone
 *    revPreDelay  (0..200)   — pre-delay in ms
 *    revWidth     (0..1)     — stereo width
 *
 *  Implementation:
 *    - Pre-delay via a short delay line
 *    - Reverb via JUCE's built-in Reverb (Freeverb)
 *
 *  Lane A — DSP modules (Source/DSP/*)
 */
class ReverbModule
{
public:
    ReverbModule() = default;

    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        numChannels = static_cast<int> (spec.numChannels);

        reverb.prepare (spec);

        // Pre-delay buffer: max 200ms
        int maxPreDelaySamples = static_cast<int> (sampleRate * 0.2) + 1;
        for (int ch = 0; ch < 2; ++ch)
        {
            preDelayBuffer[ch].resize (static_cast<size_t> (maxPreDelaySamples), 0.0f);
            preDelayWritePos[ch] = 0;
        }

        preDelaySamples = 0;
    }

    void reset()
    {
        reverb.reset();
        for (int ch = 0; ch < 2; ++ch)
        {
            std::fill (preDelayBuffer[ch].begin(), preDelayBuffer[ch].end(), 0.0f);
            preDelayWritePos[ch] = 0;
        }
    }

    /**
     *  @param size01       0..1 room size (from Params::ID::revSize)
     *  @param damping01    0..1 damping (from Params::ID::revDamp)
     *  @param preDelayMs   0..200 pre-delay in ms (from Params::ID::revPreDelay)
     *  @param width01      0..1 stereo width (from Params::ID::revWidth)
     */
    void setParameters (float size01, float damping01, float preDelayMs, float width01)
    {
        juce::dsp::Reverb::Parameters params;
        params.roomSize   = size01;
        params.damping    = damping01;
        params.width      = width01;
        params.wetLevel   = 1.0f;   // We handle dry/wet mix externally
        params.dryLevel   = 0.0f;   // Pure wet signal from reverb
        params.freezeMode = 0.0f;
        reverb.setParameters (params);

        // Pre-delay in samples
        preDelaySamples = static_cast<int> (preDelayMs * 0.001 * sampleRate);
        int maxSamples = static_cast<int> (preDelayBuffer[0].size()) - 1;
        preDelaySamples = std::clamp (preDelaySamples, 0, maxSamples);
    }

    void process (juce::dsp::AudioBlock<float>& block)
    {
        // Apply pre-delay if needed
        if (preDelaySamples > 0)
        {
            for (size_t ch = 0; ch < block.getNumChannels() && ch < 2; ++ch)
            {
                auto* data = block.getChannelPointer (ch);
                for (size_t s = 0; s < block.getNumSamples(); ++s)
                {
                    // Write current sample
                    preDelayBuffer[ch][static_cast<size_t> (preDelayWritePos[ch])] = data[s];

                    // Read delayed sample
                    int readPos = preDelayWritePos[ch] - preDelaySamples;
                    if (readPos < 0)
                        readPos += static_cast<int> (preDelayBuffer[ch].size());
                    data[s] = preDelayBuffer[ch][static_cast<size_t> (readPos)];

                    // Advance
                    preDelayWritePos[ch]++;
                    if (preDelayWritePos[ch] >= static_cast<int> (preDelayBuffer[ch].size()))
                        preDelayWritePos[ch] = 0;
                }
            }
        }

        // Process through reverb
        juce::dsp::ProcessContextReplacing<float> context (block);
        reverb.process (context);
    }

private:
    double sampleRate = 44100.0;
    int numChannels = 2;

    juce::dsp::Reverb reverb;

    // Pre-delay
    std::vector<float> preDelayBuffer[2];
    int preDelayWritePos[2] = { 0, 0 };
    int preDelaySamples = 0;
};

