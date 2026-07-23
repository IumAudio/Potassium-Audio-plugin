#pragma once
#include <juce_dsp/juce_dsp.h>
#include <cmath>

/**
 * Drive — PurestDrive saturation with ADAA antialiasing.
 * sin() waveshaper. ADAA (Antiderivative Anti-Aliasing) replaces the
 * naive sin() with its antiderivative -cos(), giving ~1st-order alias suppression
 * without extra oversampling. Zero extra latency.
 *
 * Ref: Parker et al., "Antiderivative Antialiasing for Nonlinear Audio Systems"
 */
class SaturatorStage
{
public:
    SaturatorStage() = default;
    void prepare(double, int) {}
    void reset() { prevL = prevR = 0.0; prevInL = prevInR = 0.0; }
    int getLatencySamples() const noexcept { return (useADAA && intensity > 0.001f) ? 1 : 0; }

    /** 0=off, 1=max drive */
    void setDrive(float d)     { intensity = d; }
    void setADAA(bool on)      { useADAA = on; }
    void setCharacter(float)   {}  // unused
    void setTrim(float dB)     { trimLin = std::pow(10.0f, dB / 20.0f); }

    void process(juce::dsp::AudioBlock<float>& block) {
        auto nc = block.getNumChannels();
        auto ns = block.getNumSamples();
        if (intensity < 0.001f) return;

        float wet = 0.35f + intensity * 0.35f;  // 0.35–0.70
        float dry = 1.0f - wet;

        for (int s = 0; s < (int)ns; ++s) {
            for (size_t c = 0; c < nc && c < 2; ++c) {
                float* ch = block.getChannelPointer(c);
                double in = ch[s];
                double clean = in;

                // ADAA sin(): F(x)=-cos(x), ΔF/Δx
                double& prevIn = (c == 0) ? prevInL : prevInR;
                double dx = in - prevIn;
                double sinOut;
                if (std::abs(dx) < 1e-12) {
                    sinOut = std::sin((in + prevIn) * 0.5);  // fallback
                } else {
                    // (F(in) - F(prev)) / (in - prev) = (cos(prev) - cos(in)) / dx
                    sinOut = (std::cos(prevIn) - std::cos(in)) / dx;
                }
                prevIn = in;

                // PurestDrive-style adaptive intensity
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
    bool useADAA = true;
    double prevL = 0.0, prevR = 0.0;
    double prevInL = 0.0, prevInR = 0.0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SaturatorStage)
};
