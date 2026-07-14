#pragma once
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <vector>

/**
 * Transparent brickwall limiter. Input Gain pushes into fixed ceiling.
 * 2ms lookahead, instant attack, smooth release. Clean — no coloration.
 */
class LimiterStage
{
public:
    LimiterStage() = default;

    void prepare(double sampleRate, int blockSize) {
        fs = (float)sampleRate;
        lookahead = juce::jmax(4, (int)(0.002f * fs));
        bufSize = (lookahead + blockSize + 4) * 2;
        delayBuf.assign(bufSize, 0.0f);
        writePos = 0;
        peakEnv = 0.0f;
        grSmooth = 1.0f;
    }

    void reset() {
        std::fill(delayBuf.begin(), delayBuf.end(), 0.0f);
        writePos = 0;
        peakEnv = 0.0f;
        grSmooth = 1.0f;
    }

    /** Input gain (dB) to push into the brickwall. 0=off, +12=max loudness. */
    void setInputGain(float dB) { inputGainLin = std::pow(10.0f, dB / 20.0f); }

    /** Release 0=fast, 1=slow */
    void setRelease(float r) {
        float ms = 10.0f + r * r * 490.0f; // 10–500ms
        releaseCoeff = std::exp(-1.0f / (ms * fs / 1000.0f));
    }

    /** Fixed ceiling in dB (typically -0.3) */
    void setCeiling(float dB) { ceilLin = std::pow(10.0f, dB / 20.0f); }

    float getGainReductionDB() const noexcept {
        auto current = 20.0f * std::log10(grSmooth + 1e-20f);
        if (current < grPeak) grPeak = current; // instant attack
        else grPeak += 0.1f * (current - grPeak); // fast release
        if (grPeak > 0) grPeak = 0;
        return grPeak;
    }

    void process(juce::dsp::AudioBlock<float>& block) {
        auto nc = (int)block.getNumChannels();
        auto ns = (int)block.getNumSamples();
        int ch = (nc > 2) ? 2 : nc;

        for (int s = 0; s < ns; ++s) {
            // Write input (with gain) to delay line
            for (int c = 0; c < ch; ++c)
                delayBuf[writePos + c] = block.getSample(c, s) * inputGainLin;

            // Peak detection on gained input
            float peak = 0.0f;
            for (int c = 0; c < ch; ++c)
                peak = std::max(peak, std::fabs(delayBuf[writePos + c]));

            // Envelope: instant attack, smooth release
            if (peak > peakEnv) peakEnv = peak;
            else peakEnv += releaseCoeff * (peak - peakEnv);
            peakEnv = juce::jmax(1e-15f, peakEnv);

            // Brickwall gain = ceil / peak (clamped to 1.0)
            float targetGain = (peakEnv > ceilLin) ? (ceilLin / peakEnv) : 1.0f;
            targetGain = juce::jlimit(0.01f, 1.0f, targetGain);

            // Instant attack, smooth release on gain reduction
            if (targetGain < grSmooth) grSmooth = targetGain;
            else grSmooth += releaseCoeff * (targetGain - grSmooth);

            // Read delayed (non-gained!) signal + apply GR
            int rp = writePos - lookahead * ch;
            if (rp < 0) rp += bufSize;
            rp = (rp / ch) * ch;

            for (int c = 0; c < ch; ++c) {
                float delayed = delayBuf[rp + c]; // already has inputGain applied
                float out = delayed * grSmooth;
                // Hard ceiling clamp (clean, no saturation)
                if (out > ceilLin)  out = ceilLin;
                if (out < -ceilLin) out = -ceilLin;
                block.setSample(c, s, out);
            }

            writePos += ch;
            if (writePos >= bufSize) writePos = 0;
        }
    }

private:
    float fs = 44100.0f;
    float inputGainLin = 1.0f, ceilLin = 0.966f; // -0.3dB
    float releaseCoeff = 0.999f;
    float peakEnv = 0.0f, grSmooth = 1.0f;
    mutable float grPeak = 0.0f;
    int lookahead = 0, bufSize = 0, writePos = 0;
    std::vector<float> delayBuf;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LimiterStage)
};
