/**
 * VST3 plugin entry point.
 * JUCE requires createPluginFilter() to instantiate the plugin.
 */
#include <juce_dsp/juce_dsp.h>
#include "PluginProcessor.h"

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PotassiumAudioProcessor();
}
