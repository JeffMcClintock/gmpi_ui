#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "JuceGmpiComponent.h"

class GmpiDrawingDemoComponent :
    public GmpiComponent
{
    int demo_idx = 1;
public:
    gmpi::ReturnCode onPointerUp(gmpi::drawing::Point point, int32_t pflags) override;
    void onRender(gmpi::drawing::Graphics& g) override;
};

class PluginEditor : public juce::AudioProcessorEditor
{
public:
    PluginEditor (NewProjectAudioProcessor&);
    void resized() override;

private:
    NewProjectAudioProcessor& audioProcessor;

    GmpiDrawingDemoComponent clientComponent;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
