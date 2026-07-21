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
        // Run empty oversampling chain to preserve latency
        auto block = juce::dsp::AudioBlock<float>(buffer);
        auto osBlock = oversampling.processSamplesUp(block);
        oversampling.processSamplesDown(block);
        updateMeters(0.0f, 0.0f, -60.0f);
        return;
    }

    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    auto block = juce::dsp::AudioBlock<float>(buffer);

    // ── Base-rate processing (always at the DAW sample rate) ──
    // EQ crossover & compressor timing are independent of oversampling.
    inputGain.process(block);
    if (!*eqBypassParam)     eqStage.process(block);
    if (!*compBypassParam)   compressor.process(block);

    // ── Oversampled processing ──
    auto osBlock = oversampling.processSamplesUp(block);

    if (!*driveBypassParam)  saturator.process(osBlock);
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

    // Phase correlation for particle display
    if (buffer.getNumChannels() >= 2) {
        double midSum = 0.0, sideSum = 0.0;
        for (int s = 0; s < buffer.getNumSamples(); ++s) {
            float mid  = buffer.getSample(0, s) + buffer.getSample(1, s);
            float side = buffer.getSample(0, s) - buffer.getSample(1, s);
            midSum  += (double)mid * mid;
            sideSum += (double)side * side;
        }
        float corr = (float)((midSum - sideSum) / (midSum + sideSum + 1e-10));
        phaseCorrelation += 0.15f * (corr - phaseCorrelation); // smooth
    }

    updateMeters(inputGain.getSweetSpotMeter(), limiter.getGainReductionDB(), inputGain.getRMSdB());
}
