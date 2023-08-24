#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "JuceGmpiComponent.h"
#include "BouncingRectangles.h"

#define USE_GMPI_RENDERER 1

#if USE_GMPI_RENDERER
#define BOXES_BASE_CLASS GmpiComponent
#else
#define BOXES_BASE_CLASS juce::Component
#endif

// this class tests the renderer by drawing thousands of colored rectangles.
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
        invalidateRect(); // GMPI-UI equivalent of 'repaint()'
#else
        repaint();
#endif
    }

#if USE_GMPI_RENDERER

    // GMPI-UI rendering
    void onRender(gmpi::drawing::Graphics& g) override;
#if 0
    {
        g.clear(gmpi::drawing::Colors::Green);

        auto brush = g.createSolidColorBrush(gmpi::drawing::Colors::Red);

        for (auto& r : model.rects)
        {
            brush.setColor( toGmpi(r.colour) );
            g.fillRectangle( toGmpi(r.pos), brush );
        }
    }
#endif
#else

    // JUCE rendering
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

class GmpiDrawingDemoComponent :
    public GmpiComponent
{
    int demo_idx = 1;

public:
    GmpiDrawingDemoComponent()
    {
    }
    // override hit test
bool hitTest(int x, int y) override
	{
    _RPT0(0, "hitTest\n");
		return true;
	}
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;

    gmpi::ReturnCode onPointerUp(gmpi::drawing::Point point, int32_t pflags) override
    {
        nextDemo();
        return gmpi::ReturnCode::Handled;
    }

    void onRender(gmpi::drawing::Graphics& g) override;
    void nextDemo();
};

class PluginEditor : public juce::AudioProcessorEditor
{
public:
    PluginEditor (NewProjectAudioProcessor&);
    void resized() override;

    void mouseDown(const juce::MouseEvent& e) override;
    bool hitTest(int x, int y) override
    {
        _RPT0(0, "hitTest top\n");
        return true;
    }

private:
    NewProjectAudioProcessor& audioProcessor;

//    BouncingBoxesComponent boxesComponent;

    GmpiDrawingDemoComponent clientComponent;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
