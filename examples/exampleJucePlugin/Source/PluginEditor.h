/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
/**
*/

#define USE_JUCE_RENDERER 1

#ifdef _WIN32
// Add the path to the gmpi_ui library in the Projucer setting 'Header Search Paths'.
#include "backends/DrawingFrame_win32.h"

struct BouncingRectangles
{
    struct Sprite
	{
        juce::Rectangle<int> pos;
		juce::Point<int> vel;
		juce::Colour colour;
	};

    std::vector< Sprite > rects;

    BouncingRectangles()
    {
        for (int i = 0; i < 100000; ++i)
        {
            Sprite s;
            s.pos = { rand() % 500, rand() % 500, 1 + rand() % 50, 1 + rand() % 50 };
            s.vel = { rand() % 10 - 5, rand() % 10 - 5 };
            s.colour = juce::Colour((juce::uint8) (rand() % 255), (juce::uint8) (rand() % 255), (juce::uint8) (rand() % 255), (float) (rand() % 100) * 0.01f);

            rects.push_back(s);
        }
    }

    void step()
    {
        for (auto& s : rects)
		{
			s.pos += s.vel;

			if (s.pos.getX() < 0)
			{
				s.pos.setX(0);
				s.vel.setX(-s.vel.getX());
			}
			if (s.pos.getY() < 0)
			{
				s.pos.setY(0);
				s.vel.setY(-s.vel.getY());
			}
			if (s.pos.getRight() > 400)
			{
				s.pos.setX(400 - s.pos.getWidth());
				s.vel.setX(-s.vel.getX());
			}
			if (s.pos.getBottom() > 300)
			{
				s.pos.setY(300 - s.pos.getHeight());
				s.vel.setY(-s.vel.getY());
			}
		}
	}
};


class JuceDrawingFrame : public GmpiGuiHosting::DrawingFrameBase, public juce::HWNDComponent
{
    GmpiDrawing::Point cubaseBugPreviousMouseMove = { -1,-1 };
public:
    HWND getWindowHandle() override
    {
        return (HWND)getHWND();
    }

    LRESULT WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
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

// TODO?
/*
struct Drawable // IUnknown etc
{
    virtual int32_t MP_STDCALL OnRender(GmpiDrawing_API::IMpDeviceContext* drawingContext) = 0;
};
*/

class GmpiCanvas : public gmpi_gui_api::IMpGraphics3
{
    BouncingRectangles& model;

public:

    GmpiCanvas(BouncingRectangles& m) : model(m)
    {
	}

    // IMpGraphics
    int32_t MP_STDCALL measure(GmpiDrawing_API::MP1_SIZE availableSize, GmpiDrawing_API::MP1_SIZE* returnDesiredSize) {return gmpi::MP_OK;}
    int32_t MP_STDCALL arrange(GmpiDrawing_API::MP1_RECT finalRect) {return gmpi::MP_OK;} // TODO const, and reference maybe?
    int32_t MP_STDCALL OnRender(GmpiDrawing_API::IMpDeviceContext* drawingContext);
    int32_t MP_STDCALL onPointerDown(int32_t flags, GmpiDrawing_API::MP1_POINT point) {return gmpi::MP_OK;}
    int32_t MP_STDCALL onPointerMove(int32_t flags, GmpiDrawing_API::MP1_POINT point) {return gmpi::MP_OK;}
    int32_t MP_STDCALL onPointerUp(int32_t flags, GmpiDrawing_API::MP1_POINT point) {return gmpi::MP_OK;}    // IMpGraphics2
    int32_t MP_STDCALL hitTest(GmpiDrawing_API::MP1_POINT point) {return gmpi::MP_OK;} // TODO!!! include mouse flags (for Patch Cables)
    int32_t MP_STDCALL getToolTip(GmpiDrawing_API::MP1_POINT point, gmpi::IString* returnString) {return gmpi::MP_OK;}

    // IMpGraphics3
    int32_t MP_STDCALL hitTest2(int32_t flags, GmpiDrawing_API::MP1_POINT point) {return gmpi::MP_OK;}
    int32_t MP_STDCALL onMouseWheel(int32_t flags, int32_t delta, GmpiDrawing_API::MP1_POINT point) {return gmpi::MP_OK;}
    int32_t MP_STDCALL setHover(bool isMouseOverMe) {return gmpi::MP_OK;}

    // IMpUnknown
    int32_t MP_STDCALL queryInterface(const gmpi::MpGuid& iid, void** returnInterface) override
    {
        *returnInterface = nullptr;

        if (iid == gmpi_gui_api::SE_IID_GRAPHICS_MPGUI3)
        {
            *returnInterface = static_cast<IMpGraphics3*>(this);
            addRef();
            return gmpi::MP_OK;
        }

        if (iid == gmpi_gui_api::SE_IID_GRAPHICS_MPGUI2)
        {
            *returnInterface = static_cast<IMpGraphics2*>(this);
            addRef();
            return gmpi::MP_OK;
        }

        if (iid == gmpi_gui_api::SE_IID_GRAPHICS_MPGUI)
        {
            *returnInterface = static_cast<IMpGraphics*>(this);
            addRef();
            return gmpi::MP_OK;
        }

        return gmpi::MP_NOSUPPORT;
    }
    GMPI_REFCOUNT;
};

class NewProjectAudioProcessorEditor : public juce::AudioProcessorEditor, juce::Timer
{
public:
    NewProjectAudioProcessorEditor (NewProjectAudioProcessor&);
    ~NewProjectAudioProcessorEditor() override;

    //==============================================================================
#if USE_JUCE_RENDERER
    void paint (juce::Graphics&) override;
#endif

    void parentHierarchyChanged() override;
    void resized() override;
    

    // timer
    void timerCallback() override;

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    NewProjectAudioProcessor& audioProcessor;
    JuceDrawingFrame drawingframe;

    BouncingRectangles model;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NewProjectAudioProcessorEditor)
};
