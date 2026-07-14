#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

/**
 * Minimal prototype editor.
 *
 * JUCE's GenericAudioProcessorEditor auto-generates a list of all parameters
 * with sliders/switches. Perfect for the prototype phase — no custom UI needed.
 *
 * The user tweaks parameters in the DAW (or via this generic panel) to find
 * the sweet spots, then we build the real WebView UI in a later phase.
 */
class PotassiumAudioEditor final : public juce::AudioProcessorEditor
{
public:
    explicit PotassiumAudioEditor (PotassiumAudioProcessor& p)
        : AudioProcessorEditor (&p), processorRef (p)
    {
        // Use JUCE's built-in generic editor for the prototype
        genericEditor.reset (new juce::GenericAudioProcessorEditor (p));
        addAndMakeVisible (genericEditor.get());

        // Size auto-fits to the parameter count
        setSize (genericEditor->getWidth(), genericEditor->getHeight());
    }

    void resized() override
    {
        if (genericEditor != nullptr)
            genericEditor->setBounds (getLocalBounds());
    }

private:
    PotassiumAudioProcessor& processorRef;
    std::unique_ptr<juce::GenericAudioProcessorEditor> genericEditor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PotassiumAudioEditor)
};
