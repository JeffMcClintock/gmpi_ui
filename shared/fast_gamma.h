#pragma once
#include <math.h>
#include "./xp_simd.h"

/*
#include "../shared/fast_gamma.h"
using namespace se_sdk;
*/

namespace se_sdk
{
	class gammaLookupTables
	{
	public:
		float SrgbToFloat[256];
		float RgbToFloat[256];
		float LinearToSRGB[256];
		float RGBtoSRGB[256];

		gammaLookupTables();
	};

	// trick to allow static array in header-only implementation.
	template <typename T>
	struct static_holder
	{
		static T static_resource_;
	};

	template <typename T>
	T static_holder<T>::static_resource_;

	class FastGamma : private static_holder<gammaLookupTables>
	{

	public:
		// Fast version.
		inline static float sRGB_to_float(unsigned char pixel)
		{
			return static_resource_.SrgbToFloat[pixel];
		}

		inline static float RGB_to_float(unsigned char pixel)
		{
			return static_resource_.RgbToFloat[pixel];
		}

		// Fast version.
		inline static float RGB_to_SRGBf(unsigned char pixel)
		{
			return static_resource_.RGBtoSRGB[pixel];
		}

		// Fast version.
		inline static unsigned char float_to_sRGB(float intensity)
		{
			auto& toSRGB = static_resource_.LinearToSRGB;

			unsigned char i;

			if (intensity > toSRGB[128])
				i = 128;
			else
				i = 0;

			if (intensity > toSRGB[i + 64])
				i += 64;

			if (intensity > toSRGB[i + 32])
				i += 32;

			if (intensity > toSRGB[i + 16])
				i += 16;

			if (intensity > toSRGB[i + 8])
				i += 8;

			if (intensity > toSRGB[i + 4])
				i += 4;

			if (intensity > toSRGB[i + 2])
				i += 2;

			if (intensity > toSRGB[i + 1])
				i += 1;

			return i;
		}

		// Convert normalized float intensity to normalised srbg value.
		inline static double linearToSrgb(double linear)
		{
			if (linear <= 0.00313066844250063)
			{
				return linear * 12.92;
			}
			else
			{
				return 1.055 * pow(linear, 1.0 / 2.4) - 0.055;
			}
		}

		inline static double srgbToLinear(double srgb)
		{
			if (srgb <= 0.0404482362771082)
			{
				return srgb / 12.92;
			}
			else
			{
				return pow((srgb + 0.055) / 1.055, 2.4);
			}
		}

		inline static double gamma22ToLinear(double srgb)
		{
			return pow(srgb, 2.2);
		}

		inline static double LinearTogamma22(double srgb)
		{
			return pow(srgb, 1.0/2.2);
		}

		// straight divide by 255.
		inline static float pixelToNormalised(unsigned char pixel)
		{
			constexpr float c = 1.0f / 255.0f;
			return c * static_cast<float>(pixel);
		}

		// straight multiply by 255.
		inline static unsigned char fastNormalisedToPixel(float normalised)
		{
			return static_cast<unsigned char>(FastRealToIntTruncateTowardZero(0.5f + normalised * 255.0f));
		}
		inline static unsigned char normalisedToPixel(double normalised)
		{
			// return (int) (0.5 + normalised * 255.0);
			return fastNormalisedToPixel(static_cast<float>(normalised));
		}	
	};

	inline gammaLookupTables::gammaLookupTables()
	{
		constexpr double oneOver255 = 1.0f / 255.0f;

		for (int i = 1; i < 256; ++i)
		{
			auto srgb = i * oneOver255;

			RgbToFloat[i] = static_cast<float>(FastGamma::gamma22ToLinear(srgb));
			SrgbToFloat[i] = static_cast<float>(FastGamma::srgbToLinear(srgb));
			LinearToSRGB[i] = static_cast<float>(FastGamma::srgbToLinear((i - 0.5f) * oneOver255));
		}
		RgbToFloat[0] = LinearToSRGB[0] = SrgbToFloat[0] = 0.0f;

		for (int i = 1; i < 256; ++i)
		{
			auto rgb = i * oneOver255;

			auto linear = FastGamma::gamma22ToLinear(rgb);
			RGBtoSRGB[i] = static_cast<float>(FastGamma::linearToSrgb(linear));
		}
	}
} // namespace.
