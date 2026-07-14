#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "dsp/OversamplingStage.h"
#include "dsp/InputGainStage.h"
#include "dsp/CompressorStage.h"
#include "dsp/SaturatorStage.h"
#include "dsp/EQStage.h"
#include "dsp/DensityStage.h"
#include "dsp/LimiterStage.h"
#include "dsp/StereoWidthStage.h"
#include "dsp/OutputGainStage.h"

// ─── Parameter IDs ──────────────────────────────────────────────────────────
namespace ParamIDs
{
    inline constexpr const char* inputGain      = "inputGain";
    inline constexpr const char* compThreshold  = "compThreshold";
    inline constexpr const char* compRelease    = "compRelease";
    inline constexpr const char* compCharacter = "compCharacter";
    inline constexpr const char* satDrive       = "satDrive";
    inline constexpr const char* satCharacter   = "satCharacter";
    inline constexpr const char* eqLowShelf     = "eqLowShelf";
    inline constexpr const char* eqHighShelf    = "eqHighShelf";
    inline constexpr const char* density        = "density";
    inline constexpr const char* limiterThresh  = "limiterThresh";
    inline constexpr const char* limiterRelease = "limiterRelease";
    inline constexpr const char* stereoWide     = "stereoWide";
    inline constexpr const char* outputGain     = "outputGain";
    inline constexpr const char* overMode       = "overMode";
    inline constexpr const char* bypass         = "bypass";
    inline constexpr const char* compBypass     = "compBypass";
    inline constexpr const char* driveBypass    = "driveBypass";
    inline constexpr const char* eqBypass       = "eqBypass";
    inline constexpr const char* stereoBypass   = "stereoBypass";
    inline constexpr const char* limitBypass    = "limitBypass";

    // Meter parameters (read-only, for DAW mapping)
    inline constexpr const char* sweetSpot      = "sweetSpot";
    inline constexpr const char* limiterGR      = "limiterGR";
    inline constexpr const char* inputLevel     = "inputLevel";
}

// ─── Plugin Processor ───────────────────────────────────────────────────────
class PotassiumAudioProcessor final : public juce::AudioProcessor
{
public:
    PotassiumAudioProcessor()
        : AudioProcessor (BusesProperties().withInput  ("Stereo In",  juce::AudioChannelSet::stereo())
                                             .withOutput ("Stereo Out", juce::AudioChannelSet::stereo())),
          apvts (*this, nullptr, "Parameters", createParameterLayout())
    {
        // Init 3 preset slots
        auto defaultXml = apvts.copyState().createXml();
        programNames.add ("Default");
        programStates.add (defaultXml->toString());
        programNames.add ("100%");
        programStates.add (defaultXml->toString());
        programNames.add ("200%");
        programStates.add (defaultXml->toString());
        currentProgram = 0;
    }

    // ── Parameters ──────────────────────────────────────────────────────────

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
    {
        juce::AudioProcessorValueTreeState::ParameterLayout layout;

        using namespace juce;

        // ── Input ──────────────────────────────────────────────────────────
        layout.add (std::make_unique<AudioParameterFloat> (ParamIDs::inputGain, "Input Gain",
                                                           NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f,
                                                           AudioParameterFloatAttributes{}
                                                               .withLabel ("dB")
                                                               .withCategory (AudioProcessorParameter::inputGain)));

        // ── Compressor ─────────────────────────────────────────────────────
        layout.add (std::make_unique<AudioParameterFloat> (ParamIDs::compThreshold, "Comp",
                                                           NormalisableRange<float> (0.0f, 0.8f, 0.01f), 0.5f,
                                                           AudioParameterFloatAttributes{}
                                                               .withCategory (AudioProcessorParameter::genericParameter)));

        // ── Drive ─────────────────────────────────────────────────────────
        layout.add (std::make_unique<AudioParameterFloat> (ParamIDs::satDrive, "Drive",
                                                           NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.2f,
                                                           AudioParameterFloatAttributes{}
                                                               .withCategory (AudioProcessorParameter::genericParameter)));

        // ── EQ ─────────────────────────────────────────────────────────────
        layout.add (std::make_unique<AudioParameterFloat> (ParamIDs::eqLowShelf, "EQ Low",
                                                           NormalisableRange<float> (-6.0f, 6.0f, 0.1f), 0.0f,
                                                           AudioParameterFloatAttributes{}
                                                               .withLabel ("dB")
                                                               .withCategory (AudioProcessorParameter::genericParameter)));
        layout.add (std::make_unique<AudioParameterFloat> (ParamIDs::density, "EQ Mid",
                                                           NormalisableRange<float> (-6.0f, 6.0f, 0.1f), 0.0f,
                                                           AudioParameterFloatAttributes{}
                                                               .withLabel ("dB")
                                                               .withCategory (AudioProcessorParameter::genericParameter)));
        layout.add (std::make_unique<AudioParameterFloat> (ParamIDs::eqHighShelf, "EQ High",
                                                           NormalisableRange<float> (-6.0f, 6.0f, 0.1f), 0.0f,
                                                           AudioParameterFloatAttributes{}
                                                               .withLabel ("dB")
                                                               .withCategory (AudioProcessorParameter::genericParameter)));

        // ── Limiter ────────────────────────────────────────────────────────
        layout.add (std::make_unique<AudioParameterFloat> (ParamIDs::limiterThresh, "Limit Gain",
                                                           NormalisableRange<float> (0.0f, 12.0f, 0.1f), 0.0f,
                                                           AudioParameterFloatAttributes{}
                                                               .withLabel ("dB")
                                                               .withCategory (AudioProcessorParameter::genericParameter)));
        // ── Stereo Width ───────────────────────────────────────────────────
        layout.add (std::make_unique<AudioParameterFloat> (ParamIDs::stereoWide, "Wide",
                                                           NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.0f,
                                                           AudioParameterFloatAttributes{}
                                                               .withCategory (AudioProcessorParameter::genericParameter)));

        // ── Output ─────────────────────────────────────────────────────────
        layout.add (std::make_unique<AudioParameterFloat> (ParamIDs::outputGain, "Output Gain",
                                                           NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f,
                                                           AudioParameterFloatAttributes{}
                                                               .withLabel ("dB")
                                                               .withCategory (AudioProcessorParameter::outputGain)));

        // ── Oversampling ───────────────────────────────────────────────────
        layout.add (std::make_unique<AudioParameterChoice> (ParamIDs::overMode, "Oversampling",
                                                             juce::StringArray { "Off", "2x", "4x", "8x" }, 2,
                                                             AudioParameterChoiceAttributes{}
                                                                 .withCategory (AudioProcessorParameter::genericParameter)));

        // ── Bypasses ──────────────────────────────────────────────────────
        layout.add (std::make_unique<AudioParameterBool> (ParamIDs::compBypass, "Byp Comp", false,
                                                           AudioParameterBoolAttributes{}
                                                               .withCategory (AudioProcessorParameter::genericParameter)));
        layout.add (std::make_unique<AudioParameterBool> (ParamIDs::driveBypass, "Byp Drive", false,
                                                           AudioParameterBoolAttributes{}
                                                               .withCategory (AudioProcessorParameter::genericParameter)));
        layout.add (std::make_unique<AudioParameterBool> (ParamIDs::eqBypass, "Byp EQ", false,
                                                           AudioParameterBoolAttributes{}
                                                               .withCategory (AudioProcessorParameter::genericParameter)));
        layout.add (std::make_unique<AudioParameterBool> (ParamIDs::stereoBypass, "Byp Stereo", false,
                                                           AudioParameterBoolAttributes{}
                                                               .withCategory (AudioProcessorParameter::genericParameter)));
        layout.add (std::make_unique<AudioParameterBool> (ParamIDs::limitBypass, "Byp Limit", false,
                                                           AudioParameterBoolAttributes{}
                                                               .withCategory (AudioProcessorParameter::genericParameter)));

        // ── Bypass ─────────────────────────────────────────────────────────
        layout.add (std::make_unique<AudioParameterBool> (ParamIDs::bypass, "Bypass", false,
                                                           AudioParameterBoolAttributes{}
                                                               .withCategory (AudioProcessorParameter::genericParameter)));

        // ── Meters (read-only, for DAW mapping) ────────────────────────────
        layout.add (std::make_unique<AudioParameterFloat> (ParamIDs::sweetSpot, "Sweet Spot Meter",
                                                           NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.0f,
                                                           AudioParameterFloatAttributes{}
                                                               .withCategory (AudioProcessorParameter::inputMeter)
                                                               .withAutomatable (false)));
        layout.add (std::make_unique<AudioParameterFloat> (ParamIDs::limiterGR, "Limiter GR",
                                                           NormalisableRange<float> (0.0f, 24.0f, 0.1f), 0.0f,
                                                           AudioParameterFloatAttributes{}
                                                               .withLabel ("dB")
                                                               .withCategory (AudioProcessorParameter::inputMeter)
                                                               .withAutomatable (false)));
        layout.add (std::make_unique<AudioParameterFloat> (ParamIDs::inputLevel, "Input Level",
                                                           NormalisableRange<float> (-60.0f, 0.0f, 0.1f), -60.0f,
                                                           AudioParameterFloatAttributes{}
                                                               .withLabel ("dB")
                                                               .withCategory (AudioProcessorParameter::inputMeter)
                                                               .withAutomatable (false)));
        return layout;
    }

    // ── State ───────────────────────────────────────────────────────────────

    void getStateInformation (juce::MemoryBlock& destData) override
    {
        auto state = apvts.copyState();
        std::unique_ptr<juce::XmlElement> xml (state.createXml());
        auto* root = new juce::XmlElement ("PotassiumState");
        root->setAttribute ("program", currentProgram);
        root->addChildElement (xml.release());
        // Save program names
        auto* progs = new juce::XmlElement ("Programs");
        for (int i = 0; i < programNames.size(); ++i) {
            auto* p = new juce::XmlElement ("Prog");
            p->setAttribute ("name", programNames[i]);
            p->setAttribute ("state", programStates[i]);
            progs->addChildElement (p);
        }
        root->addChildElement (progs);
        copyXmlToBinary (*root, destData);
    }

    void setStateInformation (const void* data, int sizeInBytes) override
    {
        std::unique_ptr<juce::XmlElement> root (getXmlFromBinary (data, sizeInBytes));
        if (root == nullptr) return;

        // Load programs
        programNames.clear(); programStates.clear();
        if (auto* progs = root->getChildByName ("Programs")) {
            for (auto* p = progs->getFirstChildElement(); p != nullptr; p = p->getNextElement()) {
                programNames.add (p->getStringAttribute ("name", "Preset"));
                programStates.add (p->getStringAttribute ("state", ""));
            }
        }
        currentProgram = root->getIntAttribute ("program", 0);
        currentProgram = juce::jlimit (0, juce::jmax (0, programNames.size() - 1), currentProgram);

        // Restore APVTS
        if (auto* apvtsXml = root->getChildByName ("Parameters"))
            apvts.replaceState (juce::ValueTree::fromXml (*apvtsXml));
    }

    // ── Programs ────────────────────────────────────────────────────────────

    int getNumPrograms() override { return juce::jmax (1, programNames.size()); }
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override {
        if (index >= 0 && index < programNames.size()) {
            // Save current state to current slot first
            saveCurrentToProgram (currentProgram);
            // Load new
            currentProgram = index;
            auto xml = juce::parseXML (programStates[index]);
            if (xml != nullptr)
                apvts.replaceState (juce::ValueTree::fromXml (*xml));
        }
    }
    const juce::String getProgramName (int index) override {
        return (index >= 0 && index < programNames.size()) ? programNames[index] : juce::String ("Program ") + juce::String (index + 1);
    }
    void changeProgramName (int index, const juce::String& newName) override {
        if (index >= 0 && index < programNames.size()) programNames.set (index, newName);
    }

    /** Save current settings as a named preset */
    void saveAsPreset (const juce::String& name) {
        saveCurrentToProgram (currentProgram);
        auto state = apvts.copyState();
        auto xml = state.createXml();
        programNames.add (name);
        programStates.add (xml->toString());
        currentProgram = programNames.size() - 1;
    }

    // ── Audio processing ────────────────────────────────────────────────────

    void prepareToPlay (double sampleRate, int samplesPerBlock) override
    {
        juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, 2 };

        oversampling.prepare (spec);
        inputGain.prepare   (sampleRate, samplesPerBlock);
        compressor.prepare  (sampleRate, samplesPerBlock);
        saturator.prepare   (sampleRate, samplesPerBlock);
        eqStage.prepare     (sampleRate, samplesPerBlock);
        density.prepare     (sampleRate, samplesPerBlock);
        limiter.prepare     (sampleRate, samplesPerBlock);
        stereoWidth.prepare (sampleRate, samplesPerBlock);
        outputGain.prepare  (sampleRate, samplesPerBlock);

        setLatencySamples (oversampling.getLatencySamples());

        prevOversamplingMode = -1; // force update
    }

    void releaseResources() override {}

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override;

    // ── Editor ──────────────────────────────────────────────────────────────
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    // ── Identity ────────────────────────────────────────────────────────────
    const juce::String getName() const override { return "Potassium"; }
    bool acceptsMidi() const override    { return false; }
    bool producesMidi() const override   { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    // ── Program ─────────────────────────────────────────────────────────────
    // ── APVTS access ────────────────────────────────────────────────────────
    juce::AudioProcessorValueTreeState apvts;

private:
    // ── DSP modules ─────────────────────────────────────────────────────────
    OversamplingStage  oversampling;
    InputGainStage     inputGain;
    CompressorStage    compressor;
    SaturatorStage     saturator;
    EQStage            eqStage;
    DensityStage       density;
    LimiterStage       limiter;
    StereoWidthStage   stereoWidth;
    OutputGainStage    outputGain;

    int prevOversamplingMode = -1;

    // ── Parameter attachments ──────────────────────────────────────────────
    juce::AudioParameterFloat*   inputGainParam      = nullptr;
    juce::AudioParameterFloat*   compThresholdParam  = nullptr;
    juce::AudioParameterFloat*   compReleaseParam    = nullptr;
    juce::AudioParameterFloat*   compCharacterParam  = nullptr;
    juce::AudioParameterFloat*   satDriveParam       = nullptr;
    juce::AudioParameterFloat*   satCharacterParam   = nullptr;
    juce::AudioParameterFloat*   eqLowShelfParam     = nullptr;
    juce::AudioParameterFloat*   eqHighShelfParam    = nullptr;
    juce::AudioParameterFloat*   densityParam        = nullptr;
    juce::AudioParameterFloat*   limiterThreshParam  = nullptr;
    juce::AudioParameterFloat*   limiterReleaseParam = nullptr;
    juce::AudioParameterFloat*   stereoWideParam     = nullptr;
    juce::AudioParameterFloat*   outputGainParam     = nullptr;
    juce::AudioParameterChoice*  overModeParam       = nullptr;
    juce::AudioParameterBool*    bypassParam         = nullptr;
    juce::AudioParameterBool*    compBypassParam     = nullptr;
    juce::AudioParameterBool*    driveBypassParam    = nullptr;
    juce::AudioParameterBool*    eqBypassParam       = nullptr;
    juce::AudioParameterBool*    stereoBypassParam   = nullptr;
    juce::AudioParameterBool*    limitBypassParam    = nullptr;

    // Meter parameters
    juce::AudioParameterFloat*   sweetSpotParam      = nullptr;
    juce::AudioParameterFloat*   limiterGRParam      = nullptr;
    juce::AudioParameterFloat*   inputLevelParam     = nullptr;

    // ── Internal helpers ────────────────────────────────────────────────────

    void initParams()
    {
        if (inputGainParam != nullptr) return; // already initialized

        inputGainParam      = (juce::AudioParameterFloat*)   apvts.getParameter (ParamIDs::inputGain);
        compThresholdParam  = (juce::AudioParameterFloat*)   apvts.getParameter (ParamIDs::compThreshold);
        compReleaseParam    = (juce::AudioParameterFloat*)   apvts.getParameter (ParamIDs::compRelease);
        compCharacterParam  = (juce::AudioParameterFloat*)   apvts.getParameter (ParamIDs::compCharacter);
        satDriveParam       = (juce::AudioParameterFloat*)   apvts.getParameter (ParamIDs::satDrive);
        satCharacterParam   = (juce::AudioParameterFloat*)   apvts.getParameter (ParamIDs::satCharacter); // may be null (removed from UI)
        eqLowShelfParam     = (juce::AudioParameterFloat*)   apvts.getParameter (ParamIDs::eqLowShelf);
        eqHighShelfParam    = (juce::AudioParameterFloat*)   apvts.getParameter (ParamIDs::eqHighShelf);
        densityParam        = (juce::AudioParameterFloat*)   apvts.getParameter (ParamIDs::density);
        limiterThreshParam  = (juce::AudioParameterFloat*)   apvts.getParameter (ParamIDs::limiterThresh);
        limiterReleaseParam = (juce::AudioParameterFloat*)   apvts.getParameter (ParamIDs::limiterRelease);
        stereoWideParam     = (juce::AudioParameterFloat*)   apvts.getParameter (ParamIDs::stereoWide);
        outputGainParam     = (juce::AudioParameterFloat*)   apvts.getParameter (ParamIDs::outputGain);
        overModeParam       = (juce::AudioParameterChoice*)  apvts.getParameter (ParamIDs::overMode);
        bypassParam         = (juce::AudioParameterBool*)    apvts.getParameter (ParamIDs::bypass);
        compBypassParam     = (juce::AudioParameterBool*)    apvts.getParameter (ParamIDs::compBypass);
        driveBypassParam    = (juce::AudioParameterBool*)    apvts.getParameter (ParamIDs::driveBypass);
        eqBypassParam       = (juce::AudioParameterBool*)    apvts.getParameter (ParamIDs::eqBypass);
        stereoBypassParam   = (juce::AudioParameterBool*)    apvts.getParameter (ParamIDs::stereoBypass);
        limitBypassParam    = (juce::AudioParameterBool*)    apvts.getParameter (ParamIDs::limitBypass);
        sweetSpotParam      = (juce::AudioParameterFloat*)   apvts.getParameter (ParamIDs::sweetSpot);
        limiterGRParam      = (juce::AudioParameterFloat*)   apvts.getParameter (ParamIDs::limiterGR);
        inputLevelParam     = (juce::AudioParameterFloat*)   apvts.getParameter (ParamIDs::inputLevel);
    }

    void updateParameters()
    {
        initParams();

        if (inputGainParam->get() != lastInputGain)
        {
            lastInputGain = inputGainParam->get();
            inputGain.setGainDB (lastInputGain);
        }

        compressor.setThreshold  (compThresholdParam->get());
        compressor.setRelease    (0.5f);
        compressor.setCharacter  (0.5f);
        compressor.setMakeup     (0.0f);

        // Drive: PurestDrive standalone
        saturator.setDrive      (satDriveParam->get());
        saturator.setCharacter  (satCharacterParam ? satCharacterParam->get() : 0.5f);
        saturator.setTrim       (0.0f);

        // SmoothEQ3 3-band: map -6..+6 dB → 0..1 (0.5 = flat)
        eqStage.setLowShelfDB   (eqLowShelfParam->get() / 12.0f + 0.5f);
        eqStage.setHighShelfDB  (eqHighShelfParam->get() / 12.0f + 0.5f);
        eqStage.setMidBand      (densityParam->get() / 12.0f + 0.5f);

        limiter.setInputGain    (limiterThreshParam->get());
        limiter.setRelease      (0.3f);
        limiter.setCeiling      (-0.3f);

        stereoWidth.setWidth     (stereoWideParam->get());

        if (outputGainParam->get() != lastOutputGain)
        {
            lastOutputGain = outputGainParam->get();
            outputGain.setGainDB (lastOutputGain);
        }

        // Oversampling mode
        int osMode = overModeParam->getIndex();
        if (osMode != prevOversamplingMode)
        {
            prevOversamplingMode = osMode;
            oversampling.setMode (osMode);
            setLatencySamples (oversampling.getLatencySamples());
        }
    }

    void updateMeters (float sweetSpot, float grDB, float inDB)
    {
        sweetSpotParam->setValueNotifyingHost (sweetSpot);
        limiterGRParam->setValueNotifyingHost (juce::jlimit (0.0f, 24.0f, -grDB));
        inputLevelParam->setValueNotifyingHost (juce::jlimit (-60.0f, 0.0f, inDB));
    }

    void saveCurrentToProgram (int idx) {
        if (idx < 0 || idx >= programNames.size()) return;
        auto state = apvts.copyState();
        auto xml = state.createXml();
        programStates.set (idx, xml->toString());
    }

    float lastInputGain  = 0.0f;
    float lastOutputGain = 0.0f;

    juce::StringArray programNames;
    juce::StringArray programStates;
    int currentProgram = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PotassiumAudioProcessor)
};
