#pragma once

/*
#include "JuceGmpiComponent.h"
*/

#include <JuceHeader.h>
#include "./Drawing.h"

class GmpiComponent :
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
