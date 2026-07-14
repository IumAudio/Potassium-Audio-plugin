#pragma once
#include <juce_dsp/juce_dsp.h>
#include <cmath>

/**
 * Stereo Width — PurestDrive saturation on Side channel.
 * M/S → Side gets sin() waveshaper → reconstruct.
 * One Wide control. Very subtle bus-level processing.
 */
class StereoWidthStage
{
public:
    StereoWidthStage() = default;
    void prepare(double, int) {}
    void reset() { prevSide = 0.0; }

    /** 0=none, 1=full width */
    void setWidth(float w) { wide = juce::jlimit(0.0f, 1.0f, w); }

    void process(juce::dsp::AudioBlock<float>& block) {
        auto ns = block.getNumSamples();
        if (block.getNumChannels() < 2 || wide < 0.001f) return;

        // Tiny side gain + saturation for natural width
        float sideGain = 1.0f + wide * 0.33f; // 1.0~1.2x
        float satAmt = wide * 0.25f;

        for (int s = 0; s < (int)ns; ++s) {
            float L = block.getChannelPointer(0)[s];
            float R = block.getChannelPointer(1)[s];

            float mid  = (L + R) * 0.5f;
            float side = (L - R) * 0.5f * sideGain;

            // PurestDrive-style sat on Side — same sin() waveshaper as Drive module
            float drySide = side;
            float wetSide = std::sin(side);
            float apply = std::fabs(prevSide + wetSide) * 0.5f * satAmt;
            side = drySide * (1.0f - apply) + wetSide * apply;
            prevSide = std::sin(drySide);

            block.getChannelPointer(0)[s] = mid + side;
            block.getChannelPointer(1)[s] = mid - side;
        }
    }

private:
    float wide = 0.0f;
    double prevSide = 0.0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StereoWidthStage)
};
