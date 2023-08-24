#pragma once
#include <assert.h>

/* USEAGE.

#include "../shared/xp_simd.h"

#ifndef GMPI_SSE_AVAILABLE
	// No SSE. Use C++ instead.
#else
	// Use SSE instructions.
#endif

*/

// #define GMPI_TEST_SSE_DISABLED // use for testing non-SSE codepath.

#if defined(GMPI_TEST_SSE_DISABLED)
	#define GMPI_USE_SSE 0
#else
	#if defined(_M_IX86) || defined(_M_X64) || defined(__i386__) || defined(__x86_64__)
		#include <emmintrin.h>
		#define GMPI_USE_SSE 1
	#else
		#if defined(__arm__)
/* dosn't compile on M1
			#include "./sse2neon.h"
			#define GMPI_USE_SSE 1
*/
		#endif

		#define GMPI_USE_SSE 0
	#endif
#endif

// Include SSE Intrinsics if available and set macro GMPI_SSE_AVAILABLE.
#if GMPI_USE_SSE

#define GMPI_SSE_AVAILABLE

inline int FastRealToIntTruncateTowardZero(const float& f)
{
	return _mm_cvtt_ss2si(_mm_load_ss(&f)); // fast float-to-int using SSE. truncation toward zero.
}
inline int FastRealToIntTruncateTowardZero(const double& f)
{
//	return _mm_cvtsd_si32(_mm_set_sd(f)); // uses current rounding mode.
	return _mm_cvttsd_si32(_mm_set_sd(f)); // truncates.
}

inline int  FastRealToIntFloor( double value )
{
    __m128d t = _mm_set_sd( value );
    int i = _mm_cvtsd_si32(t); // fast float-to-int using SSE. truncation toward zero.
    return i - _mm_movemask_pd(_mm_cmplt_sd(t, _mm_cvtsi32_sd(t,i)));
}

#else

inline int FastRealToIntTruncateTowardZero(const float& f)
{
	return static_cast<int>(f);
}

inline int FastRealToIntTruncateTowardZero(const double& f)
{
	assert(f >= 0.0f); // else rounding wrong.
    return static_cast<int>(f);
}

inline int FastRealToIntFloor(double f)
{
    return static_cast<int>(f);
}
#endif


