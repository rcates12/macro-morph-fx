#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>

/**
 *  DelayModule — Tempo-synced stereo delay
 *
 *  Params from Params.h:
 *    delaySync      (choice 0..7)  — note value for tempo sync
 *    delayFeedback  (0..0.95)      — feedback amount (hard clamped per SPEC)
 *    delayTone      (0..1)         — feedback tone (0 = dark, 1 = bright)
 *    delayWidth     (0..1)         — stereo width (0 = mono, 1 = full stereo)
 *    delayPingPong  (bool)         — ping-pong mode
 *
 *  Implementation:
 *    - Circular buffer delay line per channel
 *    - Tempo sync via BPM from host playhead
 *    - Smoothed delay time (50ms ramp) with fractional read (linear interpolation)
 *    - Feedback with one-pole tone filter in the loop
 *    - Ping-pong: alternates feedback between L and R
 *    - Width: crossfade between mono (L=R) and stereo delay
 *
 *  Lane A — DSP modules (Source/DSP/*)
 */
class DelayModule
{
public:
    DelayModule() = default;

    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        numChannels = static_cast<int> (spec.numChannels);

        // Max delay: 2 seconds (covers 1 bar at 30 BPM)
        bufSize_ = static_cast<int> (sampleRate * 2.0);

        for (int ch = 0; ch < 2; ++ch)
        {
            delayLine[ch].resize (static_cast<size_t> (bufSize_), 0.0f);
            writePos[ch] = 0;
        }

        // Feedback tone filter
        for (int ch = 0; ch < 2; ++ch)
        {
            toneLPF[ch].prepare (spec);
            toneLPF[ch].setType (juce::dsp::StateVariableTPTFilterType::lowpass);
            toneLPF[ch].setCutoffFrequency (20000.0f);
            toneLPF[ch].setResonance (0.707f);
        }

        // Smooth delay time changes over 50 ms to avoid clicks
        smoothDelay_.reset (sampleRate, 0.05);
        smoothDelay_.setCurrentAndTargetValue (static_cast<float> (bufSize_ / 4));
    }

    void reset()
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            std::fill (delayLine[ch].begin(), delayLine[ch].end(), 0.0f);
            writePos[ch] = 0;
            toneLPF[ch].reset();
        }
    }

    /**
     *  @param syncIndex   0..7 note value index (from Params::ID::delaySync)
     *  @param feedback    0..0.95 (from Params::ID::delayFb)
     *  @param tone01      0..1 (from Params::ID::delayTone)
     *  @param width01     0..1 (from Params::ID::delayWidth)
     *  @param pingPong    true/false (from Params::ID::delayPingP)
     *  @param bpm         current host BPM (from playhead)
     */
    void setParameters (int syncIndex, float feedback, float tone01,
                        float width01, bool pingPong, double bpm)
    {
        // Clamp feedback per SPEC safety cap
        fb = std::min (feedback, 0.95f);
        width = width01;
        isPingPong = pingPong;

        // Sync index to note duration in beats:
        //   0=1/32, 1=1/16, 2=1/8, 3=1/4, 4=1/2, 5=1bar, 6=1/8dot, 7=1/4dot
        static constexpr float noteBeats[] = {
            0.125f,   // 1/32
            0.25f,    // 1/16
            0.5f,     // 1/8
            1.0f,     // 1/4
            2.0f,     // 1/2
            4.0f,     // 1 bar
            0.75f,    // 1/8 dotted
            1.5f      // 1/4 dotted
        };

        int idx = std::clamp (syncIndex, 0, 7);
        float beats = noteBeats[idx];

        // Convert beats to samples: samples = beats * (60 / bpm) * sampleRate
        double safeBpm = (bpm > 20.0) ? bpm : 120.0;  // fallback if host doesn't report BPM
        float newDelay = static_cast<float> (beats * (60.0 / safeBpm) * sampleRate);

        // Clamp to buffer size
        newDelay = std::clamp (newDelay, 1.0f, static_cast<float> (bufSize_ - 1));
        smoothDelay_.setTargetValue (newDelay);

        // Tone filter: 0 = dark (500 Hz), 1 = bright (20000 Hz)
        float toneCutoff = 500.0f * std::pow (40.0f, tone01);
        for (int ch = 0; ch < 2; ++ch)
            toneLPF[ch].setCutoffFrequency (toneCutoff);
    }

    void process (juce::AudioBuffer<float>& buffer)
    {
        const int numSamples = buffer.getNumSamples();
        const int channels = std::min (buffer.getNumChannels(), 2);

        for (int s = 0; s < numSamples; ++s)
        {
            // Advance smoothed delay time once per sample (shared by both channels)
            const float currentDelay = smoothDelay_.getNextValue();

            // ── Read delayed samples for all channels FIRST (before any writes) ──
            float delayed[2] = { 0.0f, 0.0f };
            for (int ch = 0; ch < channels; ++ch)
            {
                float readPosF = static_cast<float> (writePos[ch]) - currentDelay;
                if (readPosF < 0.0f)
                    readPosF += static_cast<float> (bufSize_);

                // Fractional delay read (linear interpolation)
                int idx0 = static_cast<int> (std::floor (readPosF));
                float frac = readPosF - std::floor (readPosF);

                // Wrap indices safely
                if (idx0 < 0) idx0 += bufSize_;
                if (idx0 >= bufSize_) idx0 -= bufSize_;
                int idx1 = (idx0 + 1 >= bufSize_) ? 0 : idx0 + 1;

                delayed[ch] = delayLine[ch][static_cast<size_t> (idx0)] * (1.0f - frac)
                            + delayLine[ch][static_cast<size_t> (idx1)] * frac;
            }

            // ── Process feedback, write, width, and output per channel ──────────
            for (int ch = 0; ch < channels; ++ch)
            {
                auto* data = buffer.getWritePointer (ch);

                // Apply tone filter to feedback signal
                float filteredFeedback = toneLPF[ch].processSample (ch, delayed[ch]);

                // Determine feedback input
                float feedbackSample;
                if (isPingPong && channels == 2)
                {
                    // Ping-pong: feed the OTHER channel's delayed signal
                    int otherCh = 1 - ch;
                    feedbackSample = toneLPF[ch].processSample (ch, delayed[otherCh]) * fb;
                }
                else
                {
                    feedbackSample = filteredFeedback * fb;
                }

                // Write to delay line: input + feedback
                delayLine[ch][static_cast<size_t> (writePos[ch])] = data[s] + feedbackSample;

                // Width: blend between mono delay (L=R average) and stereo
                float wetSample = delayed[ch];
                if (width < 1.0f && channels == 2)
                {
                    int otherCh = 1 - ch;
                    float monoDelay = (delayed[ch] + delayed[otherCh]) * 0.5f;
                    wetSample = monoDelay + width * (delayed[ch] - monoDelay);
                }

                // Mix delayed signal with dry
                data[s] = data[s] + wetSample;

                // Advance write position
                writePos[ch]++;
                if (writePos[ch] >= bufSize_)
                    writePos[ch] = 0;
            }
        }
    }

private:
    double sampleRate = 44100.0;
    int numChannels = 2;
    int bufSize_ = 0;

    std::vector<float> delayLine[2];
    int writePos[2] = { 0, 0 };

    float fb = 0.25f;
    float width = 0.7f;
    bool isPingPong = false;

    juce::dsp::StateVariableTPTFilter<float> toneLPF[2];

    // Smoothed delay time in samples (avoids clicks on tempo/sync changes)
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothDelay_;
};
