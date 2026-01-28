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