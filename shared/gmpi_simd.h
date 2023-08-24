#pragma once
#ifndef GMPI_SIMD_H_INCLUDED
#define GMPI_SIMD_H_INCLUDED

////////////////////////////////////////////////////////////////////////////////////
// SIMD Support.
#ifndef NOMINMAX
	#define NOMINMAX
#endif

#include <assert.h>
#ifdef _WIN32
#include <intrin.h>
#endif
#include <emmintrin.h>
#include <immintrin.h>
#include <math.h>
#include <algorithm>

namespace simd_gmpi
{
	/* TODO
	
	"Value" confusing
	SimdSupportLevel::Basic vs simd_med confusing
	
	*/

	struct SimdSupportLevel
    {
        enum {None, Basic, Latest};
    };

#ifdef _WIN32

	//  Windows
#define cpuid(info, x)    __cpuidex(info, x, 0)

#else

	//  GCC Intrinsics
#include <cpuid.h>
	void cpuid(int info[4], int InfoType) {
		__cpuid_count(InfoType, 0, info[0], info[1], info[2], info[3]);
	}

#endif

	/*SimdSupportLevel*/ int getCpuSupportLevel()
	{
		//  Misc.
		bool HW_MMX;
		bool HW_x64;
		bool HW_ABM;      // Advanced Bit Manipulation
		bool HW_RDRAND;
		bool HW_BMI1;
		bool HW_BMI2;
		bool HW_ADX;
		bool HW_PREFETCHWT1;

		//  SIMD: 128-bit
		bool HW_SSE;
		bool HW_SSE2;
		bool HW_SSE3;
		bool HW_SSSE3;
		bool HW_SSE41;
		bool HW_SSE42;
		bool HW_SSE4a;
		bool HW_AES;
		bool HW_SHA;

		//  SIMD: 256-bit
		bool HW_AVX;
		bool HW_XOP;
		bool HW_FMA3;
		bool HW_FMA4;
		bool HW_AVX2;

		//  SIMD: 512-bit
		bool HW_AVX512F;    //  AVX512 Foundation
		bool HW_AVX512CD;   //  AVX512 Conflict Detection
		bool HW_AVX512PF;   //  AVX512 Prefetch
		bool HW_AVX512ER;   //  AVX512 Exponential + Reciprocal
		bool HW_AVX512VL;   //  AVX512 Vector Length Extensions
		bool HW_AVX512BW;   //  AVX512 Byte + Word
		bool HW_AVX512DQ;   //  AVX512 Doubleword + Quadword
		bool HW_AVX512IFMA; //  AVX512 Integer 52-bit Fused Multiply-Add
		bool HW_AVX512VBMI; //  AVX512 Vector Byte Manipulation Instructions

		int info[4];
		cpuid(info, 0);
		int nIds = info[0];

		cpuid(info, 0x80000000);
		unsigned nExIds = info[0];

		//  Detect Features
		if (nIds >= 0x00000001) {
			cpuid(info, 0x00000001);
			HW_MMX = (info[3] & ((int)1 << 23)) != 0;
			HW_SSE = (info[3] & ((int)1 << 25)) != 0;
			HW_SSE2 = (info[3] & ((int)1 << 26)) != 0;
			HW_SSE3 = (info[2] & ((int)1 << 0)) != 0;

			HW_SSSE3 = (info[2] & ((int)1 << 9)) != 0;
			HW_SSE41 = (info[2] & ((int)1 << 19)) != 0;
			HW_SSE42 = (info[2] & ((int)1 << 20)) != 0;
			HW_AES = (info[2] & ((int)1 << 25)) != 0;

			HW_AVX = (info[2] & ((int)1 << 28)) != 0;
			HW_FMA3 = (info[2] & ((int)1 << 12)) != 0;

			HW_RDRAND = (info[2] & ((int)1 << 30)) != 0;
		}
		if (nIds >= 0x00000007) {
			cpuid(info, 0x00000007);
			HW_AVX2 = (info[1] & ((int)1 << 5)) != 0;

			HW_BMI1 = (info[1] & ((int)1 << 3)) != 0;
			HW_BMI2 = (info[1] & ((int)1 << 8)) != 0;
			HW_ADX = (info[1] & ((int)1 << 19)) != 0;
			HW_SHA = (info[1] & ((int)1 << 29)) != 0;
			HW_PREFETCHWT1 = (info[2] & ((int)1 << 0)) != 0;

			HW_AVX512F = (info[1] & ((int)1 << 16)) != 0;
			HW_AVX512CD = (info[1] & ((int)1 << 28)) != 0;
			HW_AVX512PF = (info[1] & ((int)1 << 26)) != 0;
			HW_AVX512ER = (info[1] & ((int)1 << 27)) != 0;
			HW_AVX512VL = (info[1] & ((int)1 << 31)) != 0;
			HW_AVX512BW = (info[1] & ((int)1 << 30)) != 0;
			HW_AVX512DQ = (info[1] & ((int)1 << 17)) != 0;
			HW_AVX512IFMA = (info[1] & ((int)1 << 21)) != 0;
			HW_AVX512VBMI = (info[2] & ((int)1 << 1)) != 0;
		}
		if (nExIds >= 0x80000001) {
			cpuid(info, 0x80000001);
			HW_x64 = (info[3] & ((int)1 << 29)) != 0;
			HW_ABM = (info[2] & ((int)1 << 5)) != 0;
			HW_SSE4a = (info[2] & ((int)1 << 6)) != 0;
			HW_FMA4 = (info[2] & ((int)1 << 16)) != 0;
			HW_XOP = (info[2] & ((int)1 << 11)) != 0;
		}
#ifdef _WIN64 // Only 64-bit OS supports AVX
		if (HW_AVX2)
		{
			return (int) SimdSupportLevel::Latest;
		}
		else
#endif
		{
			if (HW_SSE2)
			{
				return (int)SimdSupportLevel::Basic;
			}
			else
			{
				return (int) SimdSupportLevel::None;
			}
		}
	}

	// CPU has no SIMD
	namespace simd_level0
	{
		enum { supportLevel = SimdSupportLevel::None};
		typedef float Value;
	}

	// CPU has older but common type of SIMD, e.g. SSE2
	namespace simd_level1
	{
		enum { supportLevel = SimdSupportLevel::Basic };
		typedef __m128 Value;
	}

	// CPU has latest SIMD support, e.g. AVX2
	namespace simd_level2
	{
		enum { supportLevel = SimdSupportLevel::Latest};
		typedef __m256 Value;
	}

	template<typename VECTOR_T>
	inline bool isAlignedTo(const float* p)
	{
		return 0 == (reinterpret_cast<intptr_t>(p) & (sizeof(VECTOR_T) - 1));
	}

	template<>
	inline bool isAlignedTo<simd_level2::Value>(const float* p)
	{
		return true;
	}


	// Traits

	template< typename VECTOR_T >
	constexpr int vectorSize()
	{
		return sizeof(VECTOR_T) / sizeof(float);
	}

	// contructors.
	// All members initialised to same value.
	template< typename VECTOR_T >
	inline VECTOR_T makeVector(float v);

	// float
	template<>
	inline float makeVector<float>(float v)
	{
		return v;
	}

	// SSE2
	template<>
	inline __m128 makeVector<__m128>(float v)
	{
		return _mm_set1_ps(v);
	}

	// AVX2
	template<>
	inline __m256 makeVector<__m256>(float v)
	{
		return _mm256_set1_ps(v);
	}

	// load from memory.
	template< typename VECTOR_T >
	inline VECTOR_T load(const float *v);

	// float
	template<>
	inline float load<float>(const float *v)
	{
		return *v;
	}

	// SSE2
	template<>
	inline __m128 load<__m128>(const float *v)
	{
		return _mm_loadu_ps(v);
	}

	// AVX2
	template<>
	inline __m256 load<__m256>(const float *v)
	{
		return _mm256_loadu_ps(v);
	}


	// store to memory.
	template< typename VECTOR_T >
	inline void store(float *dest, VECTOR_T source);

	// float
	template<>
	inline void store<float>(float *dest, float source)
	{
		*dest = source;
	}

	// SSE2
	template<>
	inline void store<__m128>(float *dest, __m128 source)
	{
		_mm_storeu_ps(dest, source);
	}

	// AVX2
	template<>
	inline void store<__m256>(float *dest, __m256 source)
	{
		_mm256_storeu_ps(dest, source);
	}

	// extract one value.
	template< typename VECTOR_T >
	inline float extract(int i, VECTOR_T source)
	{
		assert(i >= 0 && i < vectorSize<VECTOR_T>());

		float o[vectorSize<VECTOR_T>()];
		store(o, source);
		return o[i];
	}

	template<>
	inline float extract<float>(int i, float source)
	{
		assert(i >= 0 && i < vectorSize<float>());
		return source;
	}

	// insert one value.
	template< typename VECTOR_T >
	inline VECTOR_T insert(int i, float source, VECTOR_T& dest)
	{
		assert(i >= 0 && i < vectorSize<VECTOR_T>());

		float o[vectorSize<VECTOR_T>()];
		store(o, dest);
		o[i] = source;
		return load<VECTOR_T>(o);
	}

	template<>
	inline float insert<float>(int i, float source, float& dest)
	{
		assert(i >= 0 && i < vectorSize<float>());
		dest = source;
		return dest;
	}


	// insert one value.
	template< typename VECTOR_T >
	inline void set(VECTOR_T& dest, int i, float a)
	{
		assert(i >= 0 && i < vectorSize<VECTOR_T>());

		float o[vectorSize<VECTOR_T>()];
		store(o, dest);
		o[i] = a;
		dest = load<VECTOR_T>(o);
	}

	template<>
	inline void set<float>(float& dest, int i, float a)
	{
		assert(i >= 0 && i < vectorSize<float>());
		dest = a;
	}

    // Mac compilers seems to already define these operators.
#ifdef _MSC_VER
    
	// ====================== OPERATIONS ================================
	// SUBTRACT
	// SSE2
	inline __m128 operator-( __m128 a, __m128 b)
	{
		return _mm_sub_ps(a, b);
	}

	// AVX2
	inline __m256 operator-(__m256 a, __m256 b)
	{
		return _mm256_sub_ps(a, b);
	}


	// UNARY MINUS
	// SSE2
	__m128 operator-(__m128 a) {
		// return _mm_sub_ps(_mm_setzero_ps(), a); // SLOWER.
		return _mm_xor_ps(a, _mm_set1_ps(-0.0f));
	}

	// AVX2
	__m256 operator-(__m256 a) {
		return _mm256_xor_ps(a, _mm256_set1_ps(-0.0f));
	}

	// MULTIPLY
	// SSE2
	inline __m128 operator*(__m128 a, __m128 b)
	{
		return _mm_mul_ps(a, b);
	}

	// AVX2
	inline __m256 operator*(__m256 a, __m256 b)
	{
		return _mm256_mul_ps(a, b);
	}


	// DIVIDE
	// SSE2
	inline __m128 operator/(__m128 a, __m128 b)
	{
		return _mm_div_ps(a, b);
	}

	// AVX2
	inline __m256 operator/(__m256 a, __m256 b)
	{
		return _mm256_div_ps(a, b);
	}


	// ADD
	// SSE2
	inline __m128 operator+(__m128 a, __m128 b)
	{
		return _mm_add_ps(a, b);
	}

	// AVX2
	inline __m256 operator+(__m256 a, __m256 b)
	{
		return _mm256_add_ps(a, b);
	}
#endif
    
	// *=
	template<typename T>
	inline const T& operator*=(T& lhs, const T& rhs)
	{
		lhs = lhs * rhs;
		return lhs;
	}

	// vector fabs()
	template<typename T>
	inline T simd_fabs(T a);

	template<>
	inline float simd_fabs<float>(float a)
	{
		return fabsf(a);
	}

	template<>
	inline __m128 simd_fabs<__m128>(__m128 a)
	{
		return _mm_max_ps(_mm_sub_ps(_mm_setzero_ps(), a), a);
	}

	template<>
	inline __m256 simd_fabs<__m256>(__m256 a)
	{
		return _mm256_max_ps(_mm256_sub_ps(_mm256_setzero_ps(), a), a);
	}

	// MADD
	template<typename T>
	inline T MADD(T a, T b, T c)
	{
		return a * b + c;
	}

	// AVX2
	template<>
	inline __m256 MADD<__m256>(__m256 a, __m256 b, __m256 c)
	{
		return _mm256_fmadd_ps(a, b, c);
	}

	template< typename VECTOR_T>
	VECTOR_T MoveGE(VECTOR_T xA, VECTOR_T xB, VECTOR_T xSource, const VECTOR_T& xTarget);

	template<>
	inline float MoveGE<float>(float xA, float xB, float xSource, const float& xTarget)
	{
		return xA > xB ? xSource : xTarget;
	}

	template<>
	inline __m128 MoveGE<__m128>(__m128 xA, __m128 xB, __m128 xSource, const __m128& xTarget)
	{
		__m128 vMask = _mm_cmpge_ps(xA, xB);
		return _mm_or_ps(_mm_and_ps(vMask, xSource), _mm_andnot_ps(vMask, xTarget));
	}

	template<>
	inline __m256 MoveGE<__m256>(__m256 xA, __m256 xB, __m256 xSource, const __m256& xTarget)
	{
		__m256 vMask = _mm256_cmp_ps(xA, xB, _CMP_GE_OQ);
		return _mm256_or_ps(_mm256_and_ps(vMask, xSource), _mm256_andnot_ps(vMask, xTarget));
	}

	// vector min()
	template<typename T>
	inline T simd_min(T a, T b);

	template<>
	inline float simd_min<float>(float a, float b)
	{
		return (std::min)(a, b);
	}

	template<>
	inline __m128 simd_min<__m128>(__m128 a, __m128 b)
	{
		return _mm_min_ps(a, b);
	}

	template<>
	inline __m256 simd_min<__m256>(__m256 a, __m256 b)
	{
		return _mm256_min_ps(a, b);
	}

	// vector max()
	template<typename T>
	inline T simd_max(T a, T b);

	template<>
	inline float simd_max<float>(float a, float b)
	{
		return (std::max)(a, b);
	}

	template<>
	inline __m128 simd_max<__m128>(__m128 a, __m128 b)
	{
		return _mm_max_ps(a, b);
	}

	template<>
	inline __m256 simd_max<__m256>(__m256 a, __m256 b)
	{
		return _mm256_max_ps(a, b);
	}
	// FLOOR
	template< typename VECTOR_T >
	inline VECTOR_T floor(VECTOR_T a);

	template<>
	inline __m128 floor<__m128>(__m128 a)
	{
		static const __m128 one = _mm_set1_ps(1.0f);
		__m128 fval = _mm_cvtepi32_ps(_mm_cvttps_epi32(a));
		return _mm_sub_ps(fval, _mm_and_ps(_mm_cmplt_ps(a, fval), one));
	}

	template<>
	inline __m256 floor<__m256>(__m256 a)
	{
		return _mm256_floor_ps(a);
	}

	// Create Mask from Testing equality
	template<typename VECTOR_T>
	inline VECTOR_T TestEQ(VECTOR_T a, VECTOR_T b)
	{
		float r[vectorSize<VECTOR_T>()];
		float af[vectorSize<VECTOR_T>()];
		float bf[vectorSize<VECTOR_T>()];
		store(af, a);
		store(bf, b);

		for (int i = 0; i < vectorSize<VECTOR_T>(); ++i)
			*((uint32_t *)(&r[i])) = af[i] == bf[i] ? (uint32_t)(-1) : 0;

		return load<VECTOR_T>(r);
	}

	// Create Mask from Testing equality
	template<typename VECTOR_T>
	inline VECTOR_T MaskedMove(VECTOR_T a, VECTOR_T b, VECTOR_T mask)
	{
		float maskf[vectorSize<VECTOR_T>()];
		float af[vectorSize<VECTOR_T>()];
		float bf[vectorSize<VECTOR_T>()];
		store(maskf, mask);
		store(af, a);
		store(bf, b);

		float r[vectorSize<VECTOR_T>()];

		for (int i = 0; i < vectorSize<VECTOR_T>(); ++i)
			*((uint32_t *)(&r[i])) = ((*((uint32_t *)(&af[i])) & *((uint32_t *)(&maskf[i]))) | (*((uint32_t *)(&bf[i])) & ~(*((uint32_t *)(&maskf[i])))));

		return load<VECTOR_T>(r);
	}


	//////////////////////////////////////////////

//	template<typename VECTOR_T, template< typename > class FUNCTOR>
	template<typename VECTOR_T, class FUNCTOR>
	void processVectors(FUNCTOR& f, int sampleFrames, float* dest, const float* source )
	{
		if (vectorSize<VECTOR_T>() > 1)
		{
			// process fiddly non-sse-aligned prequel.
			while (!isAlignedTo<VECTOR_T>(source) && sampleFrames > 0)
			{
				*dest++ = f(*source++);
				--sampleFrames;
			}

			// Process aligned samples in chunks.
			while (sampleFrames >= vectorSize<VECTOR_T>())
			{
				store<VECTOR_T>(dest, f( load<VECTOR_T>(source) ));
				sampleFrames -= vectorSize<VECTOR_T>();
				source += vectorSize<VECTOR_T>();
				dest += vectorSize<VECTOR_T>();
			}
		}

		// Process odd number at end individually.
		while (sampleFrames > 0)
		{
			*dest++ = f(*source++);
			--sampleFrames;
		}
	}
/*
	template< class VECTOR_TYPEA, template< typename > class FUNCTOR >
	void processVectors2(int sampleFrames, float* dest, const float* source)
	{
		//FUNCTOR f;
		auto a = FUNCTOR<float>::compute(5.5f);

		*((VECTOR_TYPEA*)dest) = FUNCTOR<VECTOR_TYPEA>::compute(*((const VECTOR_TYPEA*)source));
		// *dest++ = FUNCTOR(*source++);
 
		FUNCTOR<float> f1;
		auto c = f1(5.6f);

/ /		auto e = FUNCTOR<float>(5.6f);


		FUNCTOR<VECTOR_TYPEA> f2;
		auto d = f2(*((const VECTOR_TYPEA*)source));
	}
	*/
}

#endif

/*

template<typename VECTOR_T>
inline VECTOR_T Negate2(VECTOR_T a)
{
return -a;
}


template<typename VECTOR_T>
struct Negate
{
static inline VECTOR_T compute(VECTOR_T a) { return -a; }
inline VECTOR_T operator()(VECTOR_T a) { return -a; }
};

*/
