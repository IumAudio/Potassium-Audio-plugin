#pragma once
#include <juce_dsp/juce_dsp.h>
#include <cmath>

/**
 * Clean output trim stage.
 *
 * Simple precision gain with smooth parameter chasing
 * (PurestGain-inspired) to avoid zipper noise on automation.
 */
class OutputGainStage
{
public:
    OutputGainStage() = default;

    void prepare (double sampleRate, int /*blockSize*/)
    {
        fs = (float) sampleRate;
        gainChase = 1.0f;
    }

    void reset() { gainChase = 1.0f; }

    /** @param dB  -24 .. +24 dB */
    void setGainDB (float dB)
    {
        targetGain = std::pow (10.0f, dB / 20.0f);
    }

    void process (juce::dsp::AudioBlock<float>& block)
    {
        auto numChannels = block.getNumChannels();
        auto numSamples  = block.getNumSamples();

        float chaseSpeed = 0.002f; // fast enough to track automation, slow enough to avoid zipper

        for (int s = 0; s < (int) numSamples; ++s)
        {
            gainChase += chaseSpeed * (targetGain - gainChase);
            if (std::abs (gainChase - targetGain) < 0.00001f)
                gainChase = targetGain;

            for (size_t c = 0; c < numChannels; ++c)
            {
                auto* ch = block.getChannelPointer (c);
                ch[s] *= gainChase;
            }
        }
    }

private:
    float fs = 44100.0f;
    float targetGain = 1.0f;
    float gainChase  = 1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OutputGainStage)
};
