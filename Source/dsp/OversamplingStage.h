#pragma once
#include <juce_dsp/juce_dsp.h>

/**
 * Minimal oversampling wrapper — delegates entirely to JUCE's built-in
 * polyphase IIR cascade. No custom filtering, no manual stages.
 */
class OversamplingStage
{
public:
    OversamplingStage()
        : os2x (2, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false),
          os4x (2, 4, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false),
          os8x (2, 8, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false)
    {
        setMode(0); // default Off — ADAA handles anti-aliasing
    }

    void prepare(juce::dsp::ProcessSpec& spec) {
        os2x.initProcessing(spec.maximumBlockSize);
        os4x.initProcessing(spec.maximumBlockSize);
        os8x.initProcessing(spec.maximumBlockSize);
    }
    void reset() { os2x.reset(); os4x.reset(); os8x.reset(); }

    void setMode(int m) { currentMode = juce::jlimit(0, 3, m); }
    int  getMode() const noexcept { return currentMode; }

    int getLatencySamples() const noexcept {
        if (currentMode == 0) return 0;
        auto* os = (currentMode == 1) ? &os2x : (currentMode == 2) ? &os4x : &os8x;
        return os->getLatencyInSamples();
    }

    int getCurrentFactor() const noexcept {
        static constexpr int f[] = {1, 2, 4, 8};
        return f[currentMode];
    }

    juce::dsp::AudioBlock<float> processSamplesUp(juce::dsp::AudioBlock<float>& block) {
        if (currentMode == 0) return block;
        auto* os = (currentMode == 1) ? &os2x : (currentMode == 2) ? &os4x : &os8x;
        return os->processSamplesUp(block);
    }

    void processSamplesDown(juce::dsp::AudioBlock<float>& block) {
        if (currentMode == 0) return;
        auto* os = (currentMode == 1) ? &os2x : (currentMode == 2) ? &os4x : &os8x;
        os->processSamplesDown(block);
    }

private:
    juce::dsp::Oversampling<float> os2x, os4x, os8x;
    int currentMode = 2;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OversamplingStage)
};
