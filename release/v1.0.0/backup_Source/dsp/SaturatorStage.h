#pragma once
#include <juce_dsp/juce_dsp.h>
#include <cmath>

/**
 * Drive — PurestDrive saturation. Single knob.
 * sin() waveshaper with adaptive intensity (previous-sample polarity).
 * 0=clean, 1=full drive. Very gentle at low settings.
 */
class SaturatorStage
{
public:
    SaturatorStage() = default;
    void prepare(double, int) {}
    void reset() { prevL = prevR = 0.0; }

    /** 0=off, 1=max drive */
    void setDrive(float d)     { intensity = d; }
    void setCharacter(float)   {}  // unused — merged into single Drive
    void setTrim(float dB)     { trimLin = std::pow(10.0f, dB / 20.0f); }

    void process(juce::dsp::AudioBlock<float>& block) {
        auto nc = block.getNumChannels();
        auto ns = block.getNumSamples();
        if (intensity < 0.001f) return;

        // Drive amount with internal character baked in at sweet spot
        float wet = 0.35f + intensity * 0.35f;  // 0.35–0.70 range
        float dry = 1.0f - wet;

        for (int s = 0; s < (int)ns; ++s) {
            for (size_t c = 0; c < nc && c < 2; ++c) {
                float* ch = block.getChannelPointer(c);
                double in = ch[s];
                double clean = in;

                // PurestDrive sin waveshaper
                double sinOut = std::sin(in);
                double& prev = (c == 0) ? prevL : prevR;
                double apply = std::fabs(prev + sinOut) * 0.5 * intensity;
                double sat = clean * (1.0 - apply) + sinOut * apply;
                prev = std::sin(clean);

                ch[s] = (float)(clean * dry + sat * wet) * trimLin;
            }
        }
    }

private:
    double intensity = 0.0, trimLin = 1.0;
    double prevL = 0.0, prevR = 0.0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SaturatorStage)
};
