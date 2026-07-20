
#pragma once

// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2007-2021 SynthEdit Ltd

/*
#include "../shared/GraphHelpers.h"
*/

#include <vector>
#include "GmpiUiDrawing.h"

inline void SimplifyGraph(const std::vector<gmpi::drawing::Point>& in, std::vector<gmpi::drawing::Point>& out)
{
	if (in.size() < 2)
	{
		out = in;
		return;
	}

	out.clear();

    constexpr float tollerance{ 0.3f }; // points this close in pixels to the projected line are skipped.
    constexpr float tolleranceX{ 0.00001f }; // points this close horizontally are skipped.

	auto prev = in[0];
    auto lastOut = prev;
    lastOut.y -= 1000.0f; // force prediction failure on first point.
    float slope{ 0.0f };

    for(auto& p : in)
    {
        if (fabsf(p.x - lastOut.x) < tolleranceX) // skip points with same x value (infinite slope)
            continue;

        const float predictedY = lastOut.y + slope * (p.x - lastOut.x);
        const float err = p.y - predictedY;

        if (err > tollerance || err < -tollerance)
        {
            out.push_back(prev);
            lastOut = prev;
            slope = (p.y - lastOut.y) / (p.x - lastOut.x);
        }
         
        prev = p;
    }

    assert(!out.empty()); // should always contain at least the start point.
    assert(out.back() != in.back());
    
    if (fabsf(in.back().x - out.back().x) > tolleranceX) // avoid points with same x value (infinite slope))
        out.push_back(in.back());
}

