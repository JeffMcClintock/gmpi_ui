#pragma once
/* Copyright (c) 2007-2025 SynthEdit Ltd
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*     * Redistributions of source code must retain the above copyright
*       notice, this list of conditions and the following disclaimer.
*     * Redistributions in binary form must reproduce the above copyright
*       notice, this list of conditions and the following disclaimer in the
*       documentation and/or other materials provided with the distribution.
*     * Neither the name SEM, nor SynthEdit, nor 'Music Plugin Interface' nor the
*       names of its contributors may be used to endorse or promote products
*       derived from this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY SynthEdit Ltd ``AS IS'' AND ANY
* EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL SynthEdit Ltd BE LIABLE FOR ANY
* DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
* LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
* ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
* SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/*
#include "helpers/PixelSnapper.h"

// snap a line crisply to the nearest pixel.
// Usage:

const float penWidth = 0.5f; // in dips (1/96 inch)
pixelSnapper snap(penWidth, drawingHost->getRasterizationScale());

float left  = snap(20.0f);
float right = snap(300.0f);
float y     = snap(100.0f);

g.drawLine(gmpi::drawing::Point(left, y), gmpi::drawing::Point(right, y), brush, snap.penWidth);
*/

#include <cmath>

struct pixelSnapper
{
	float penWidth{};
	float snapOffset{};
	float rasterizationScale{};
	float inv_rasterizationScale{};

	pixelSnapper(float penWidthTargetDips, float p_rasterizationScale) :
		rasterizationScale(p_rasterizationScale)
		, inv_rasterizationScale(1.0f / p_rasterizationScale)
	{
		const float penWidthPhysical = (std::min)(1.0f, penWidthTargetDips * rasterizationScale);
		const float penWidthPhysicalSnapped = std::round(penWidthPhysical);
		const bool penWidthIsOdd = (static_cast<int>(penWidthPhysicalSnapped) & 1) != 0;
		penWidth = penWidthPhysicalSnapped * inv_rasterizationScale;
		snapOffset = penWidthIsOdd ? 0.5f : 0.0f;  // 3 pixel lines center on middle line, 2 pixel line center on edges
	}

	float operator()(float v) const
	{
		return (snapOffset + std::round(rasterizationScale * v)) * inv_rasterizationScale;
	}
};

struct pixelSnapper2
{
	gmpi::drawing::Matrix3x2 transform;
	gmpi::drawing::Matrix3x2 inverted;

	pixelSnapper2(gmpi::drawing::Matrix3x2 t, float rasterisationScale)
	{
		transform = t * gmpi::drawing::makeScale({ rasterisationScale, rasterisationScale });
		inverted = invert(transform);
	}

	struct lineSnap
	{
		float width;
		float center_offset;
	};

	lineSnap thickness(float strokeWidth)
	{
		const auto& scale = transform._11;
		const auto& invscale = inverted._11;

		const auto physicalPixelWidthSnapped = std::max(1.0f, std::round(strokeWidth * scale));
		const auto widthIsOdd = (static_cast<int>(physicalPixelWidthSnapped) & 1) != 0;

		return
		{
			 invscale * physicalPixelWidthSnapped
			,widthIsOdd ? invscale * 0.5f : 0.0f
		};
	}

	// only snap to odd number of pixels (for lines)
	lineSnap thickness_odd(float strokeWidth)
	{
		const auto& scale = transform._11;
		const auto& invscale = inverted._11;

		const auto physicalPixelWidthSnapped = (std::max)(1.0f, 1.0f + 2.0f * std::round(0.5f * strokeWidth * scale - 0.5f));
		assert((static_cast<int>(physicalPixelWidthSnapped) & 1) != 0);

		return
		{
			 invscale * physicalPixelWidthSnapped
			,invscale * 0.5f
		};
	}
	// snap to pixel origin (top-left)
	float snapX(float x) const
	{
		x = transform._11 * x + transform._31;
		x = std::round(x);
		return inverted._11 * x + inverted._31;
	}

	float snapY(float y) const
	{
		y = transform._22 * y + transform._32;
		y = std::round(y);
		return inverted._22 * y + inverted._32;
	}

	gmpi::drawing::Point snapPixelOrigin(gmpi::drawing::Point p) // snap a point to a pixel left-top
	{
		p = transformPoint(transform, p);
		p.x = std::round(p.x);
		p.y = std::round(p.y);
		return transformPoint(inverted, p);
	}
};


#if 0

#pragma once
#include <JuceHeader.h>

/*
#include "PixelSnapper.h"
*/

class PixelSnapper2
{
	inline static std::unordered_map<juce::Component*, PixelSnapper2> instances;

	juce::AffineTransform transform{};
	juce::AffineTransform inverted{};

public:
	static void setDirty()
	{
		instances.clear();
	}

	inline static PixelSnapper2& getFor(juce::Component* component)
	{
		auto it = instances.find(component);
		if(it != instances.end())
			return it->second;

		auto& newSnapper = instances[component];
		newSnapper.initTransform(component);

		return newSnapper;
	}

	void initTransform(juce::Component* component)
	{
		constexpr bool disablePixelSnapper = false;

		if constexpr(disablePixelSnapper)
		{
			transform = juce::AffineTransform::identity;
			inverted = juce::AffineTransform::identity;
			return;
		}

		const bool isCached = nullptr != component->getCachedComponentImage();

		std::vector<juce::Component*> hierarchy;
		for(auto c = component; c; c = c->getParentComponent())
		{
			hierarchy.push_back(c);
		}

		// top component is handled as a special case.
		auto& topComponent = *hierarchy.back();
		auto topPeer = topComponent.getPeer();
		const auto scale = static_cast<float>(topPeer->getPlatformScaleFactor()); // DPI of window

#if 0 // debug scaling calculation
		_RPT0(0, "\n=============================\n");
		_RPTN(0, "top peer scaleFactor %f\n", scale);

		for(auto it = hierarchy.rbegin(); it != hierarchy.rend(); it++)
		{
			auto& child = *(*it);
			auto pos = child.getPosition();
			auto trn = child.getTransform();

			_RPTN(0, "component pos %3d,%3d transform [%f,%f,%f,%f,%f,%f]\n",
				pos.getX(), pos.getY(),
				trn.mat00, trn.mat01, trn.mat02,
				trn.mat10, trn.mat11, trn.mat12);
		}
		_RPT0(0, "=============================\n");
#endif

		// don't double-process the top component
		hierarchy.pop_back();

		// copy the scaling logic from the printing call stack.

		transform = {};

		// top component is handled as a special case.
		{
			auto peerBounds = topPeer->getBounds();
			auto componentBounds = topComponent.getLocalBounds();

			if(topComponent.isTransformed())
			{
				transform = transform.followedBy(topComponent.getTransform());
				componentBounds = componentBounds.transformedBy(topComponent.getTransform());
			}

			if(peerBounds.getWidth() != componentBounds.getWidth() || peerBounds.getHeight() != componentBounds.getHeight())
				// Tweak the scaling so that the component's integer size exactly aligns with the peer's scaled size
				transform = transform.followedBy(juce::AffineTransform::scale((float)peerBounds.getWidth() / (float)componentBounds.getWidth(),
					(float)peerBounds.getHeight() / (float)componentBounds.getHeight()));
		}

		// remaining components
#if 0
		juce::Point<int> totalOffset{};
		for(auto it = hierarchy.rbegin(); it != hierarchy.rend(); it++)
		{
			auto& child = *(*it);

			if(child.isTransformed())
			{
				transform = transform.followedBy(child.getTransform());
			}

			const auto offset = child.getPosition();
			transform = transform.followedBy(juce::AffineTransform::translation((float)offset.getX(), (float)offset.getY()));

			// offset is handled as a final step, ignoring component scaling. (but affected by DPI scaling)
			totalOffset += child.getPosition();
		}
		//			transform = transform.followedBy(juce::AffineTransform::translation((float)totalOffset.getX(), (float)totalOffset.getY()));
#else
		for(auto it = hierarchy.begin(); it != hierarchy.end(); it++)
		{
			auto& child = *(*it);

			if(!isCached) // cached components bitmap suffers no offset from parents. only scaling.
			{
				const auto offset = child.getPosition();
				transform = transform.followedBy(juce::AffineTransform::translation((float)offset.getX(), (float)offset.getY()));
			}

			if(child.isTransformed())
			{
				transform = transform.followedBy(child.getTransform());
			}
		}
#endif

		transform = transform.followedBy(juce::AffineTransform::scale(scale));
		inverted = transform.inverted();
	}

	float snapWidth(float x) // snap e.g. a rectangle size or line-width to nearest pixel
	{
		const auto& scale = transform.mat00;
		const auto& invscale = inverted.mat00;

		return  std::max(1.0f, std::round(x * scale)) * invscale;
	}

	gmpi::drawing::Point snapPixelOrigin(gmpi::drawing::Point p) // snap a point to a pixel left-top
	{
		transformPoint(transform, p.x, p.y);
		p.x = std::round(p.x);
		p.y = std::round(p.y);
		inverted.transformPoint(p.x, p.y);
		return p;
	}

	gmpi::drawing::Point snapPixelCenter(gmpi::drawing::Point p) // snap a point to a pixel center (for lines)
	{
		transformPoint(transform, p.x, p.y);
		p.x = std::floorf(p.x) + 0.5f;
		p.y = std::floorf(p.y) + 0.5f;
		inverted.transformPoint(p.x, p.y);
		return p;
	}

	juce::Rectangle<float> snapFilledRect(juce::Rectangle<float> r) // snap a filled rectangle
	{
		const auto topLeft = snapPixelOrigin(r.getTopLeft());
		const auto width = snapWidth(r.getWidth());
		const auto height = snapWidth(r.getHeight());

		return juce::Rectangle<float>(topLeft.x, topLeft.y, width, height);
	}

	// snap an outline path.
	// if width is even, snap to pixel edges. if width is odd, snap to pixel centers.
	std::pair<float, juce::Rectangle<float>> snapOutline(float lineWidth, juce::Rectangle<float> r)
	{
		float lineWidthSnapped{};
		bool isOdd{};
		{
			const auto& scale = transform.mat00;
			const auto& invscale = inverted.mat00;

			const auto nativeWidth = std::max(1.0f, std::round(lineWidth * scale));
			isOdd = static_cast<int>(nativeWidth) % 2 == 1;

			lineWidthSnapped = nativeWidth * invscale;
		}

		if(isOdd)
		{
			// odd line width - snap to pixel centers
			const auto topLeft = snapPixelCenter(r.getTopLeft());
			const auto width = snapWidth(r.getWidth());
			const auto height = snapWidth(r.getHeight());
			return { lineWidthSnapped, juce::Rectangle<float>(topLeft.x, topLeft.y, width, height) };
		}

		// even line width - snap to pixel edges
		const auto topLeft = snapPixelOrigin(r.getTopLeft());
		const auto width = snapWidth(r.getWidth());
		const auto height = snapWidth(r.getHeight());
		return { lineWidthSnapped, juce::Rectangle<float>(topLeft.x, topLeft.y, width, height) };
	}

	// snap a rounded outline path.
	// if width is even, snap to pixel edges. if width is odd, snap to pixel centers.
	std::pair<float, juce::Rectangle<float>> snapRoundedRectangle(float lineWidth, juce::Rectangle<float> r)
	{
		auto unsnappedDrawRect = r;
		auto strokeWidthSnapped = snapWidth(lineWidth);

		// JUCE draws *rounded* rectangles fully *outside* the center line. need to reduce it to get it centered.
		const float reduction = strokeWidthSnapped * 0.5f;
		unsnappedDrawRect.reduce(reduction, reduction);

		return snapOutline(lineWidth, unsnappedDrawRect);
	}
};



#endif