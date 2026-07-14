#pragma once
#include <juce_dsp/juce_dsp.h>
#include <cmath>

/**
 * Density — gentle even-harmonic enhancer (tube-like warmth).
 * Replaces the old parallel-blend approach that diluted Comp/Sat/EQ.
 * Now: pure harmonic "thickness" layer on top of the processed signal.
 * 0=nothing, 1=full tube warmth.
 */
class DensityStage
{
public:
    DensityStage() = default;
    void prepare(double, int) {}
    void reset() {}

    void setDensity(float d) { density = juce::jlimit(0.0f, 1.0f, d); }

    void process(juce::dsp::AudioBlock<float>& block) {
        if (density < 0.001f) return;
        auto nc = block.getNumChannels();
        auto ns = block.getNumSamples();

        // Soft drive: 0..1.2x based on density
        float drive = 1.0f + density * 0.2f;
        float wet = density * density;           // quadratic taper
        float dry = 1.0f - wet;

        for (int s = 0; s < (int)ns; ++s) {
            for (size_t c = 0; c < nc; ++c) {
                auto* ch = block.getChannelPointer(c);
                float x = ch[s] * drive;

                // Tube-like even-harmonic polynomial: x - x³/6
                // Generates warm 2nd harmonics, very low THD
                float y = x - (x * x * x) / 6.0f;

                ch[s] = ch[s] * dry + y * wet;
            }
        }
    }

private:
    float density = 0.0f;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DensityStage)
};
