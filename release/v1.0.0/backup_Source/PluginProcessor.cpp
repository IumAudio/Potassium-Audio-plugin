#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <juce_dsp/juce_dsp.h>

juce::AudioProcessorEditor* PotassiumAudioProcessor::createEditor()
{
    return new PotassiumAudioEditor(*this);
}

void PotassiumAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    updateParameters();

    if (*bypassParam) {
        updateMeters(0.0f, 0.0f, -60.0f);
        return;
    }

    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    auto block   = juce::dsp::AudioBlock<float>(buffer);
    auto osBlock = oversampling.processSamplesUp(block);

    // ── Signal chain ──
    // InputGain → ButterComp2 → Drive → SmoothEQ3 → StereoFX → Limiter → OutputGain

    inputGain.process(osBlock);
    if (!*compBypassParam)   compressor.process(osBlock);
    if (!*driveBypassParam)  saturator.process(osBlock);
    if (!*eqBypassParam)     eqStage.process(osBlock);
    if (!*stereoBypassParam) stereoWidth.process(osBlock);
    if (!*limitBypassParam)  limiter.process(osBlock);
    outputGain.process(osBlock);

    oversampling.processSamplesDown(block);

    // Track output peak
    float outPeak = 0.0f;
    for (int s = 0; s < buffer.getNumSamples(); ++s) {
        outPeak = std::max(outPeak, std::fabs(buffer.getSample(0, s)));
        if (buffer.getNumChannels() > 1) outPeak = std::max(outPeak, std::fabs(buffer.getSample(1, s)));
    }
    outputPeakDB = 20.0f * std::log10(outPeak + 1e-20f);

    updateMeters(inputGain.getSweetSpotMeter(), limiter.getGainReductionDB(), inputGain.getRMSdB());
}
