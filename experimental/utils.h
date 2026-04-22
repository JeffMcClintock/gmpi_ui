#pragma once

#define _USE_MATH_DEFINES
#include <math.h>

namespace gmpi_sdk
{
	inline float tsin(float turn)
	{
		constexpr float conv = static_cast<float>(M_PI * 2.0);
		return sinf(turn * conv);
	}

	inline float tcos(float turn)
	{
		constexpr float conv = static_cast<float>(M_PI * 2.0);
		return cosf(turn * conv);
	}
}
