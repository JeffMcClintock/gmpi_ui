
#pragma once
/* Copyright (c) 2007-2021 SynthEdit Ltd
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
#include "../shared/GraphHelpers.h"
*/

#include <vector>
#include "Drawing.h"

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

