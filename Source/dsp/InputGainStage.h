#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>

class InputGainStage
{
public:
    InputGainStage() = default;

    void prepare(double sampleRate, int samplesPerBlock)
    {
        fs = sampleRate;
        double Tb = (double)samplesPerBlock / fs;

        // Short-term RMS: 50ms window — reads close to peak but still averaged
        stRMSCoeff = 1.0 - std::exp(-Tb / 0.050);

        // Bar + sweet-spot: 15ms smoothing — more peak-responsive
        meterSmoothCoeff = 1.0 - std::exp(-Tb / 0.015);

        // Slow display (top): asymmetric on short-term RMS — slower attack, slow release
        slowAttackCoeff  = 1.0 - std::exp(-Tb / 0.350);   // 350ms rise
        slowReleaseCoeff = 1.0 - std::exp(-Tb / 1.500);   // 1.5s fall

        // Fast peak hold (bottom): 40 dB/s after 500ms hold
        fastDecayPS = std::exp(-2.302585 / (0.5 * fs));

        gainChase = 1.0f;
        fastPeakHold = 0.0f;
        fastHoldTimer = 0;
        stRMS = 0.0f;
        stRMSdB = slowDisplaydB = currentRMSdB = smoothedMeter = -60.0f;
        slowInit = true;
    }

    void reset() {
        gainChase = 1.0f;
        fastPeakHold = 0.0f;
        fastHoldTimer = 0;
        stRMS = 0.0f;
        stRMSdB = slowDisplaydB = currentRMSdB = smoothedMeter = -60.0f;
        slowInit = true;
    }

    void setGainDB(float gainDB) {
        targetGainLin = std::pow(10.0f, gainDB / 20.0f);
    }

    // ── Meter getters ──────────────────────────────────────────────────────
    float getRMSdB()    const noexcept { return currentRMSdB; }                       // bar + sweet-spot
    float getSlowPeak() const noexcept { return slowDisplaydB; }                       // top — slow smoothing of short-term RMS
    float getFastPeak() const noexcept { return 20.0f * std::log10(fastPeakHold + 1e-20f); } // bottom — peak + 150ms hold
    float getPeakdB()   const noexcept { return getFastPeak(); }

    float getSweetSpotMeter() const noexcept {
        constexpr float centreDB = -10.5f, widthDB = 5.5f;
        auto dev = (smoothedMeter - centreDB) / widthDB;
        auto norm = std::abs(dev);
        if (norm < 0.5f) return 1.0f;
        if (norm < 1.5f) return 1.0f - (norm - 0.5f) * 0.5f;
        return 0.0f;
    }

    void process(juce::dsp::AudioBlock<float>& block)
    {
        auto nc = block.getNumChannels();
        auto ns = block.getNumSamples();

        float blockRmsSum = 0.0f;
        int   count       = 0;

        for (int s = 0; s < (int)ns; ++s)
        {
            gainChase += 0.0005f * (targetGainLin - gainChase);
            if (std::abs(gainChase - targetGainLin) < 0.00001f)
                gainChase = targetGainLin;

            float mx = 0.0f;
            for (size_t c = 0; c < nc; ++c) {
                float* ch = block.getChannelPointer(c);
                ch[s] *= gainChase;
                float a = std::abs(ch[s]);
                if (a > mx) mx = a;
                blockRmsSum += a * a;
                ++count;
            }

            // ── Fast peak hold: instant attack, 150ms hold ──────────────
            if (mx > fastPeakHold) {
                fastPeakHold  = mx;
                fastHoldTimer = (int)(fs * 0.50);   // 500ms hold
            }
        }

        if (count > 0) {
            // Short-term RMS: 50ms exponential moving average of squared signal
            float blockRMS = std::sqrt(blockRmsSum / (float)count);
            stRMS += (float)stRMSCoeff * (blockRMS - stRMS);
            stRMSdB = 20.0f * std::log10(stRMS + 1e-20f);

            // Bar: short-term RMS +1.0dB boost
            currentRMSdB = stRMSdB + 1.0f;
            smoothedMeter += (float)meterSmoothCoeff * (currentRMSdB - smoothedMeter);

            // Slow display (top): asymmetric smooth of short-term RMS +9.0dB boost
            float boosted = stRMSdB + 9.0f;
            if (slowInit) {
                slowDisplaydB = boosted;
                slowInit = false;
            } else if (boosted > slowDisplaydB) {
                slowDisplaydB += (float)slowAttackCoeff * (boosted - slowDisplaydB);
            } else {
                slowDisplaydB += (float)slowReleaseCoeff * (boosted - slowDisplaydB);
            }
        }

        // ── Fast peak hold decay (per block) ────────────────────────────
        fastHoldTimer -= (int)ns;
        if (fastHoldTimer <= 0) {
            fastPeakHold *= (float)std::pow(fastDecayPS, (double)ns);
            if (fastPeakHold < 1e-10f) fastPeakHold = 1e-10f;
        }
    }

private:
    double fs = 44100.0;
    double stRMSCoeff = 0.999, meterSmoothCoeff = 0.999, slowAttackCoeff = 0.999, slowReleaseCoeff = 0.999, fastDecayPS = 0.9999;
    float  targetGainLin = 1.0f, gainChase = 1.0f;
    float  stRMS = 0.0f;
    float  stRMSdB = -60.0f, currentRMSdB = -60.0f, smoothedMeter = -60.0f;
    float  slowDisplaydB = -60.0f;
    float  fastPeakHold = 0.0f;
    int    fastHoldTimer = 0;
    bool   slowInit = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InputGainStage)
};
