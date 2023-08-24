#pragma once

#include <JuceHeader.h>

#ifdef _WIN32
#include "./DrawingFrame_win32.h"

class JuceDrawingFrame : public GmpiGuiHosting::DrawingFrameBase, public juce::HWNDComponent
{
    GmpiDrawing::Point cubaseBugPreviousMouseMove = { -1,-1 };
public:
    HWND getWindowHandle() override
    {
        return (HWND) getHWND();
    }

    void open(void* pParentWnd, int width, int height);
};
#else

class JuceDrawingFrame : public juce::NSViewComponent
{
public:
    ~JuceDrawingFrame();
    void open(class IGuiHost2* controller, int width, int height);
};

#endif

//==============================================================================

class SynthEditEditor :
    public juce::AudioProcessorEditor
{
public:
    SynthEditEditor (class SE2JUCE_Processor&, class SeJuceController&);
    
    //==============================================================================
    void parentHierarchyChanged() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    SeJuceController& controller;
    JuceDrawingFrame drawingframe;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SynthEditEditor)
};
