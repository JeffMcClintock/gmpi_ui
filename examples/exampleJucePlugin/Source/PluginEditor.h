#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "BouncingRectangles.h"
#include "../../../GmpiUI.h"

#define USE_GMPI_RENDERER 1

#if USE_GMPI_RENDERER
#define BOXES_BASE_CLASS GmpiViewComponent
#else
#define BOXES_BASE_CLASS  juce::Component
#endif


class BouncingBoxesComponent :
    public BOXES_BASE_CLASS,
    public juce::Timer
{
    BouncingRectangles model;

public:
    BouncingBoxesComponent()
    {
        startTimerHz(30);
    }

    void timerCallback() override
    {
        model.step();
        
#if USE_GMPI_RENDERER
        invalidateRect();
#else
        repaint();
#endif
    }

#if USE_GMPI_RENDERER

    void OnRender(GmpiDrawing::Graphics& g) override
    {
        g.Clear(GmpiDrawing::Color::Green);

        auto brush = g.CreateSolidColorBrush(GmpiDrawing::Color::Red);

        for (auto& r : model.rects)
        {
            brush.SetColor( toGmpi(r.colour) );
            g.FillRectangle( toGmpi(r.pos), brush );
        }
    }
#else

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colours::green);

        for (auto& r : model.rects)
        {
            g.setColour(r.colour);
            g.fillRect(r.pos);
        }
    }
#endif
};

class PluginEditor : public juce::AudioProcessorEditor
{
public:
    PluginEditor (NewProjectAudioProcessor&);
    void resized() override;
    
private:
    NewProjectAudioProcessor& audioProcessor;

    BouncingBoxesComponent boxesComponent;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
