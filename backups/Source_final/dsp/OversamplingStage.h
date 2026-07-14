#pragma once
#include <juce_dsp/juce_dsp.h>

class OversamplingStage
{
public:
    OversamplingStage()
        : off (2, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false),
          os2x(2, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false),
          os4x(2, 4, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false),
          os8x(2, 8, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false)
    {
        oversamplers[0] = &off;
        oversamplers[1] = &os2x;
        oversamplers[2] = &os4x;
        oversamplers[3] = &os8x;
        setMode(2); // default 4x
    }

    void prepare(juce::dsp::ProcessSpec& spec) {
        for (auto* os : oversamplers) { os->initProcessing(spec.maximumBlockSize); os->reset(); }
    }
    void reset() { for (auto* os : oversamplers) os->reset(); }
    void setMode(int m) { currentMode = juce::jlimit(0, 3, m); }
    int getMode() const noexcept { return currentMode; }
    int getLatencySamples() const noexcept {
        return currentMode == 0 ? 0 : oversamplers[currentMode]->getLatencyInSamples();
    }

    juce::dsp::AudioBlock<float> processSamplesUp(juce::dsp::AudioBlock<float>& block) {
        return (currentMode == 0) ? block : oversamplers[currentMode]->processSamplesUp(block);
    }
    void processSamplesDown(juce::dsp::AudioBlock<float>& block) {
        if (currentMode != 0) oversamplers[currentMode]->processSamplesDown(block);
    }

private:
    juce::dsp::Oversampling<float> off, os2x, os4x, os8x;
    juce::dsp::Oversampling<float>* oversamplers[4];
    int currentMode = 2;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OversamplingStage)
};
