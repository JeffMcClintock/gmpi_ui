#pragma once
#define _USE_MATH_DEFINES
#include <math.h>

inline float interpolate_linear(const float* src, float frac)
{
	return src[0] + frac * (src[1] - src[0]);
}

template<int numCoefs = 8, int subsamples = 32>
inline float interpolate_sinc(const float* y, float fraction, const std::vector<float>& coefs)
{
	constexpr int tableSize = numCoefs * (1 + subsamples);
	assert(coefs.size() == tableSize); // don't forget to calcSincInterpolatorCoefs() first

	// Calculate which sub-filter to use.
	auto subFilterf = fraction * float(subsamples);
	int subFilter = FastRealToIntTruncateTowardZero(subFilterf);
	auto sincFraction = subFilterf - subFilter;
	auto interpolation_table_ptr = coefs.data() + subFilter * numCoefs;

#if !GMPI_USE_SSE
	// C++ version.
	auto ptr = y - 3;
	// now look up interpolation filter table
	// perform 2 filters at once, one 1 sub sample ahead
	float a = 0.f;
	for (int x = 4; x > 0; x--)
	{
		a += *ptr++ * *interpolation_table_ptr++;
		a += *ptr++ * *interpolation_table_ptr++; //unrolled
	}

	float a2 = 0.f;
	ptr = y - 3;
	for (int x = 4; x > 0; x--)
	{
		a2 += *ptr++ * *interpolation_table_ptr++;
		a2 += *ptr++ * *interpolation_table_ptr++;
	}

	// Linear interpolation between sinc tables.
	return a + fraction * (a2 - a);
#else
	__m128 t1 = _mm_mul_ps(_mm_loadu_ps(y - 3), _mm_load_ps(interpolation_table_ptr));
	__m128 t2 = _mm_mul_ps(_mm_loadu_ps(y + 1), _mm_load_ps(interpolation_table_ptr + 4));
	__m128 sum0 = _mm_add_ps(t1, t2);

	__m128 t3 = _mm_mul_ps(_mm_loadu_ps(y - 3), _mm_load_ps(interpolation_table_ptr + 8));
	__m128 t4 = _mm_mul_ps(_mm_loadu_ps(y + 1), _mm_load_ps(interpolation_table_ptr + 12));
	__m128 sum1 = _mm_add_ps(t3, t4);

	// Linear interpolation between sinc tables.
	auto r = _mm_add_ss(sum0, _mm_mul_ps(_mm_sub_ss(sum1, sum0), _mm_set1_ps(sincFraction)));

	// Horizontal Add.
	__m128 t = _mm_add_ps(r, _mm_movehl_ps(r, r));
	sum1 = _mm_add_ss(t, _mm_shuffle_ps(t, t, 1));
	float returnValue;
	_mm_store_ss(&returnValue, sum1);
	return returnValue;
#endif
};

template<int numCoefs = 8, int subsamples = 32>
std::vector<float> calcSincInterpolatorCoefs()
{
	assert((numCoefs & 1) == 0); // even number of coefs please.
	constexpr int table_width = numCoefs / 2;
	constexpr int tableSize = numCoefs * (1 + subsamples);
	constexpr float PI = M_PI;

	std::vector<float> coefs;
	coefs.resize(tableSize);
	coefs.assign(tableSize, -8888.f);
	// initialise interpolation table
	// as per page 523 MAMP


	for (int sub_table = 0; sub_table < subsamples; sub_table++)
	{
		int table_index = sub_table * numCoefs + numCoefs / 2 - 1;
		for (int x = -table_width; x < table_width; x++)
		{
			int i = sub_table + x * subsamples;
			// position on x axis
			double o = (double)i / subsamples;
			// filter impulse response
			double sinc = sin(PI * o) / (PI * o);

			// apply tailing function
			double hanning = cos(0.5 * PI * i / (subsamples * table_width));
			float windowed_sinc = (float)(sinc * hanning * hanning);

			assert((table_index - x) >= 0 && (table_index - x) < tableSize);
			coefs[table_index - x] = windowed_sinc;
		}
	}
	assert((table_width - 1) >= 0 && (table_width - 1) < tableSize);
	coefs[table_width - 1] = 1.f; // fix div by 0 anomaly

	// first table copied to last, shifted 1 place
	int idx = subsamples * numCoefs;
	assert((idx) >= 0 && (idx) < tableSize);
	coefs[idx] = 0.f;
	for (int table_entry = 1; table_entry < numCoefs; table_entry++)
	{
		assert((idx + table_entry) >= 0 && (idx + table_entry) < tableSize);
		coefs[idx + table_entry] = coefs[table_entry - 1];
	}

	// additional fine tuning.  Normalise all 32 posible filters so total gain is always 1.0
	// This fixes 'overtones' problems, due to different sub-filters having slightly different overall gains
	for (int sub_table = 0; sub_table <= subsamples; sub_table++)
	{
		int table_index = sub_table * numCoefs;
		assert(table_index >= 0 && table_index < tableSize);
		// calc sub table sum
		double fir_sum = 0.f;
		for (int table_entry = 0; table_entry < numCoefs; table_entry++)
		{
			fir_sum += coefs[table_index + table_entry];
		}

		// use it to normalise sub table
		for (int table_entry = 0; table_entry < numCoefs; table_entry++)
		{
			float adjusted = coefs[table_index + table_entry] / (float)fir_sum;
			if (fpclassify(adjusted) == FP_SUBNORMAL)
				adjusted = 0.f;
			assert((table_index + table_entry) >= 0 && (table_index + table_entry) < tableSize);
			coefs[table_index + table_entry] = adjusted;
		}
	}

#if 0
	// print out interpolation table
	for (int sub_table = 0; sub_table <= subsamples; sub_table++)
	{
		for (int x = 0; x < numCoefs; x++)
		{
			int table_index = sub_table * numCoefs + x;
			int linear_index = sub_table + subsamples * x;
			_RPT2(_CRT_WARN, "%d %+.5f\n", linear_index, interpolation_table2[table_index]);
		}
	}
#endif

	return coefs;
}