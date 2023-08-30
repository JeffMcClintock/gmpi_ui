#pragma once

/*
#include "GmpiUI.h"
*/

#include <JuceHeader.h>
#include "../../../Drawing.h"

class GmpiViewComponent :
	#ifdef _WIN32
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
	GmpiViewComponent();
	~GmpiViewComponent();

	// override this in your derived class to draw on your component.
	virtual void OnRender(GmpiDrawing::Graphics& g) {}

	// call this to repaint the component.
	void invalidateRect();
};


// handy conversions between JUCE and GMPI types.
inline GmpiDrawing::RectL toGmpi(juce::Rectangle<int> r)
{
	return { r.getX(), r.getY(), r.getRight(), r.getBottom() };
}

inline GmpiDrawing::Rect toGmpi(juce::Rectangle<float> r)
{
	return { r.getX(), r.getY(), r.getRight(), r.getBottom() };
}

inline GmpiDrawing::Color toGmpi(juce::Colour r)
{
	GmpiDrawing::Color ret;
	ret.InitFromSrgba(r.getRed(), r.getGreen(), r.getBlue(), r.getFloatAlpha());
	return ret;
}
