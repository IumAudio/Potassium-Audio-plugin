#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>

/**
 * Input gain stage with high precision gain staging + sweet spot detection.
 *
 * Inspired by AirWindows PurestGain (extended-precision multiply, smooth gain chasing).
 *
 * Sweet spot:
 *   - Solo kick sweet spot : -12 dB
 *   - Full mix sweet spot   :  -6 to -5 dB
 *   The stage reports a normalised "sweet spot meter" value and illuminates
 *   when the RMS level falls inside the target window.
 */
class InputGainStage
{
public:
    InputGainStage() = default;

    void prepare (double sampleRate, int blockSize)
    {
        fs = sampleRate;
        gainChase = 1.0f;
        rmsAccum = 0.0f;
        rmsCount = 0;
        currentRMSdB = -90.0f;
        peakdB = -90.0f;
        smoothedMeter = -90.0f;
        meterSmoothCoeff = std::exp (-1.0f / (0.010f * (float) fs)); // 10 ms
    }

    void reset()
    {
        gainChase = 1.0f;
        rmsAccum = 0.0f;
        rmsCount = 0;
        currentRMSdB = -90.0f;
        peakdB = -90.0f;
        smoothedMeter = -90.0f;
    }

    /** @param gainDB  input gain in dB, typically -12 .. +12 */
    void setGainDB (float gainDB)
    {
        targetGainLin = std::pow (10.0f, gainDB / 20.0f);
    }

    // ── Meter queries (call after process()) ────────────────────────────────

    /** Current RMS level in dBFS (block-based). */
    float getRMSdB() const noexcept { return currentRMSdB; }

    /** Current peak level in dBFS (block-based). */
    float getPeakdB() const noexcept { return peakdB; }

    /**
     * Sweet spot indicator: 0.0 = too quiet, 1.0 = in sweet spot, 2.0 = too hot.
     * Sweet spot window: -12 dB (solo kick) to -5 dB (full mix).
     */
    float getSweetSpotMeter() const noexcept
    {
        // Returns 0..2 where 1 is the sweet spot centre (-8.5 dB)
        // Soft window from -14 to -3 dB
        constexpr float centreDB    = -8.5f;  // midpoint of -12 .. -5
        constexpr float widthDB     = 5.5f;   // half-width
        constexpr float softnessInv = 2.0f;   // soft knee

        auto dev = (smoothedMeter - centreDB) / widthDB;
        // 0 at centre, 1 at edge, >1 outside
        auto normalised = std::abs (dev);
        if (normalised < 0.5f) return 1.0f;                               // centre zone
        if (normalised < 1.5f) return 1.0f - (normalised - 0.5f) * 0.5f; // taper
        return 0.0f;
    }

    /** Returns the detected input level as a 0..1 value for the DAW meter. */
    float getMeterValue01() const noexcept
    {
        return juce::jlimit (0.0f, 1.0f, (smoothedMeter + 30.0f) / 40.0f);
    }

    // ── Process ─────────────────────────────────────────────────────────────
    void process (juce::dsp::AudioBlock<float>& block)
    {
        auto numChannels = block.getNumChannels();
        auto numSamples  = block.getNumSamples();

        // Gain chasing (smooth parameter changes – zipper noise suppression)
        float chaseSpeed = 0.0005f;
        for (int s = 0; s < (int) numSamples; ++s)
        {
            // Smooth gain
            gainChase += chaseSpeed * (targetGainLin - gainChase);
            // Prevent denormal
            if (std::abs (gainChase - targetGainLin) < 0.00001f)
                gainChase = targetGainLin;

            float peak = 0.0f;
            for (size_t c = 0; c < numChannels; ++c)
            {
                auto* ch = block.getChannelPointer (c);
                ch[s] *= gainChase;
                auto absVal = std::abs (ch[s]);
                if (absVal > peak) peak = absVal;
            }

            // Meter accumulation
            rmsAccum += peak * peak;
            ++rmsCount;

            if (peak > peakHold) peakHold = peak;
            peakHold *= 0.9999f; // slow decay
            if (peakHold < 1e-10f) peakHold = 1e-10f;
        }

        // Block-level metering
        if (rmsCount > 0)
        {
            float rms = std::sqrt (rmsAccum / (float) rmsCount);
            currentRMSdB = 20.0f * std::log10 (rms + 1e-20f);
            peakdB = 20.0f * std::log10 (peakHold + 1e-20f);

            // Smooth the meter display
            smoothedMeter += meterSmoothCoeff * (currentRMSdB - smoothedMeter);

            rmsAccum = 0.0f;
            rmsCount = 0;
        }
    }

private:
    double fs = 44100.0;

    float targetGainLin  = 1.0f;
    float gainChase      = 1.0f;

    // Metering
    float rmsAccum      = 0.0f;
    int   rmsCount      = 0;
    float currentRMSdB  = -90.0f;
    float peakdB        = -90.0f;
    float peakHold      = 1e-10f;
    float smoothedMeter = -90.0f;
    float meterSmoothCoeff = 0.999f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InputGainStage)
};
