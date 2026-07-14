#pragma once
#include <juce_dsp/juce_dsp.h>
#include <cmath>

/**
 * SmoothEQ3 — AirWindows 3-band EQ (MIT license).
 * Crossovers at 200Hz (bass/mid) and 4000Hz (mid/treble).
 * 3rd-order Butterworth via biquad + exponential IIR cascade.
 * A=treble, B=mid, C=bass. Range 0..1, center=0.5=flat.
 */
class EQStage
{
    // biquad array indices (matches original SmoothEQ3)
    enum { biq_freq=0, biq_a0=1, biq_a1=2, biq_a2=3, biq_b1=4, biq_b2=5,
           biq_sL1=6, biq_sL2=7, biq_sR1=8, biq_sR2=9, biq_total=10 };

public:
    EQStage() = default;

    void prepare(double sampleRate, int) {
        fs = sampleRate;
        setupCoefs();
        reset();
    }

    void reset() {
        for (int i = 0; i < biq_total; i++) highFast[i] = lowFast[i] = 0.0;
        highFastIIRL = lowFastIIRL = highFastIIRR = lowFastIIRR = 0.0;
    }

    /** 0..1, 0.5=flat. Cubic taper: g=(v-0.5)*2, gain=1+g*|g|*g */
    void setLowShelfDB(float v)  { bassGain   = deCubic(v); }
    void setHighShelfDB(float v) { trebleGain = deCubic(v); }
    void setMidBand(float v)     { midGain    = deCubic(v); }

    void process(juce::dsp::AudioBlock<float>& block) {
        auto nc = (int)block.getNumChannels();
        auto ns = (int)block.getNumSamples();
        if (isFlat() && nc <= 2) return;

        auto* hf = highFast; auto* lf = lowFast;
        (void)lf; // used below

        for (int s = 0; s < ns; ++s) {
            // ── LEFT CHANNEL ──
            {
                double in = block.getChannelPointer(0)[s];

                // Stage 1: biquad crossovers
                double treble = in;
                double out = (treble * hf[biq_a0]) + hf[biq_sL1];
                hf[biq_sL1] = (treble * hf[biq_a1]) - (out * hf[biq_b1]) + hf[biq_sL2];
                hf[biq_sL2] = (treble * hf[biq_a2]) - (out * hf[biq_b2]);
                double mid = out; treble = in - mid;

                out = (mid * lf[biq_a0]) + lf[biq_sL1];
                lf[biq_sL1] = (mid * lf[biq_a1]) - (out * lf[biq_b1]) + lf[biq_sL2];
                lf[biq_sL2] = (mid * lf[biq_a2]) - (out * lf[biq_b2]);
                double bass = out; mid -= bass;

                // Apply gains
                treble = bass * bassGain + mid * midGain + treble * trebleGain;

                // Stage 2: exponential IIR (steeper slope)
                highFastIIRL = highFastIIRL * highCoef + treble * (1.0 - highCoef);
                mid = highFastIIRL; treble -= mid;

                lowFastIIRL = lowFastIIRL * lowCoef + mid * (1.0 - lowCoef);
                bass = lowFastIIRL; mid -= bass;

                block.getChannelPointer(0)[s] = (float)(bass * bassGain + mid * midGain + treble * trebleGain);
            }

            // ── RIGHT CHANNEL ──
            if (nc > 1) {
                double in = block.getChannelPointer(1)[s];

                double treble = in;
                double out = (treble * hf[biq_a0]) + hf[biq_sR1];
                hf[biq_sR1] = (treble * hf[biq_a1]) - (out * hf[biq_b1]) + hf[biq_sR2];
                hf[biq_sR2] = (treble * hf[biq_a2]) - (out * hf[biq_b2]);
                double mid = out; treble = in - mid;

                out = (mid * lf[biq_a0]) + lf[biq_sR1];
                lf[biq_sR1] = (mid * lf[biq_a1]) - (out * lf[biq_b1]) + lf[biq_sR2];
                lf[biq_sR2] = (mid * lf[biq_a2]) - (out * lf[biq_b2]);
                double bass = out; mid -= bass;

                treble = bass * bassGain + mid * midGain + treble * trebleGain;

                highFastIIRR = highFastIIRR * highCoef + treble * (1.0 - highCoef);
                mid = highFastIIRR; treble -= mid;

                lowFastIIRR = lowFastIIRR * lowCoef + mid * (1.0 - lowCoef);
                bass = lowFastIIRR; mid -= bass;

                block.getChannelPointer(1)[s] = (float)(bass * bassGain + mid * midGain + treble * trebleGain);
            }
        }
    }

private:
    static double deCubic(double v) {
        double g = (v - 0.5) * 2.0;
        return 1.0 + g * std::fabs(g) * std::fabs(g); // g³ preserving sign
    }

    bool isFlat() const {
        return std::fabs(bassGain-1.0)<0.0001 && std::fabs(midGain-1.0)<0.0001 && std::fabs(trebleGain-1.0)<0.0001;
    }

    void setupCoefs() {
        // High crossover (4000Hz, Q=1.0 biquad lowpass)
        highFast[biq_freq] = 4000.0 / fs;
        double omega = 2.0 * 3.14159265358979 * (4000.0 / fs);
        double K = 2.0 - std::cos(omega);
        highCoef = -std::sqrt(K*K - 1.0) + K;
        K = std::tan(3.14159265358979 * highFast[biq_freq]);
        double norm = 1.0 / (1.0 + K + K*K);
        highFast[biq_a0] = K * K * norm;
        highFast[biq_a1] = 2.0 * highFast[biq_a0];
        highFast[biq_a2] = highFast[biq_a0];
        highFast[biq_b1] = 2.0 * (K*K - 1.0) * norm;
        highFast[biq_b2] = (1.0 - K + K*K) * norm;

        // Low crossover (200Hz, Q=1.0 biquad lowpass)
        lowFast[biq_freq] = 200.0 / fs;
        omega = 2.0 * 3.14159265358979 * (200.0 / fs);
        K = 2.0 - std::cos(omega);
        lowCoef = -std::sqrt(K*K - 1.0) + K;
        K = std::tan(3.14159265358979 * lowFast[biq_freq]);
        norm = 1.0 / (1.0 + K + K*K);
        lowFast[biq_a0] = K * K * norm;
        lowFast[biq_a1] = 2.0 * lowFast[biq_a0];
        lowFast[biq_a2] = lowFast[biq_a0];
        lowFast[biq_b1] = 2.0 * (K*K - 1.0) * norm;
        lowFast[biq_b2] = (1.0 - K + K*K) * norm;
    }

    double fs = 44100.0;
    double highFast[biq_total] = {};
    double lowFast[biq_total] = {};
    double highCoef = 0.0, lowCoef = 0.0;
    double highFastIIRL = 0.0, lowFastIIRL = 0.0;
    double highFastIIRR = 0.0, lowFastIIRR = 0.0;
    double bassGain = 1.0, midGain = 1.0, trebleGain = 1.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EQStage)
};
