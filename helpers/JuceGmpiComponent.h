#pragma once

/*
#include "JuceGmpiComponent.h"
*/

#include <JuceHeader.h>
#include "./Drawing.h"

/*
GMPI_UI_USE_JUCE_GRAPHICS selects how GmpiComponent renders:

  0 - native backend: a native window (HWND + Direct2D on Windows, NSView + CoreGraphics
      on macOS) is overlaid on the JUCE window, bypassing juce::Graphics entirely.
  1 - JUCE backend: a plain juce::Component rendered through juce::Graphics
      (see backends/JuceGfx.h). Works on any platform JUCE supports.

By default the native backend is used where one exists (Windows, macOS), and the
JUCE backend everywhere else (e.g. Linux). Define GMPI_UI_USE_JUCE_GRAPHICS=1 in your
project to force the JUCE backend on all platforms.
*/
#ifndef GMPI_UI_USE_JUCE_GRAPHICS
	#if defined(_WIN32) || defined(__APPLE__)
		#define GMPI_UI_USE_JUCE_GRAPHICS 0
	#else
		#define GMPI_UI_USE_JUCE_GRAPHICS 1
	#endif
#endif

class GmpiComponent :
#if GMPI_UI_USE_JUCE_GRAPHICS
		public juce::Component
#elif defined(_WIN32)
		public juce::HWNDComponent
#else
		public juce::NSViewComponent
#endif
{
	struct Pimpl;
	std::unique_ptr<Pimpl> internal;

protected:
    void parentHierarchyChanged() override;

public:
	GmpiComponent();
	~GmpiComponent();

	// override this in your derived class to draw on your component.
	virtual void onRender(gmpi::drawing::Graphics& g) {}

	// call this to repaint the component.
	void invalidateRect();

	// override these to handle mouse events.
	virtual gmpi::ReturnCode onPointerDown(gmpi::drawing::Point point, int32_t pflags) { return gmpi::ReturnCode::Unhandled; }
	virtual gmpi::ReturnCode onPointerMove(gmpi::drawing::Point point, int32_t pflags) { return gmpi::ReturnCode::Unhandled; }
	virtual gmpi::ReturnCode onPointerUp(gmpi::drawing::Point point, int32_t pflags) { return gmpi::ReturnCode::Unhandled; }

#if GMPI_UI_USE_JUCE_GRAPHICS
	void paint(juce::Graphics& g) override;
	void mouseDown(const juce::MouseEvent& e) override;
	void mouseMove(const juce::MouseEvent& e) override;
	void mouseDrag(const juce::MouseEvent& e) override;
	void mouseUp(const juce::MouseEvent& e) override;
#endif
};


// handy conversions between JUCE and GMPI types.
inline gmpi::drawing::RectL toGmpi(juce::Rectangle<int> r)
{
	return { r.getX(), r.getY(), r.getRight(), r.getBottom() };
}

inline gmpi::drawing::Rect toGmpi(juce::Rectangle<float> r)
{
	return { r.getX(), r.getY(), r.getRight(), r.getBottom() };
}

inline gmpi::drawing::Color toGmpi(juce::Colour r)
{
	return gmpi::drawing::colorFromSrgba(r.getRed(), r.getGreen(), r.getBlue(), r.getFloatAlpha());
}
