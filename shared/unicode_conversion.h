#pragma once
/*
#include "../shared/unicode_conversion.h"

using namespace JmUnicodeConversions;
*/

#include <string>
#include <assert.h>
#include <stdlib.h>	 // wcstombs() on Linux.
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

#include "./simdutf/simdutf.h"

/*
For fastest conversion, set USE_SIMD_UTF_CONVERSION to 1 and include simdutf.cpp in the build.
*/

namespace JmUnicodeConversions
{

inline std::string WStringToUtf8(const std::wstring& p_cstring )
{
    std::string res;

#ifdef USE_SIMD_UTF_CONVERSION
    if constexpr(sizeof(wchar_t) == 2)
    {
        assert(sizeof(wchar_t) == 2);

        const size_t expected_utf8words = simdutf::utf8_length_from_utf16le((const char16_t*) p_cstring.data(), p_cstring.size());

        res.resize(expected_utf8words);

        simdutf::convert_utf16le_to_utf8((const char16_t*) p_cstring.data(), p_cstring.size(), (char*) res.data());
    }
    else
    {
        assert(sizeof(wchar_t) == 4);

        const size_t expected_utf8words = simdutf::utf8_length_from_utf32((const char32_t*) p_cstring.data(), p_cstring.size());

        res.resize(expected_utf8words);

        simdutf::convert_utf32_to_utf8((const char32_t*) p_cstring.data(), p_cstring.size(), (char*) res.data());
    }
#else
#if defined(_WIN32)
    const size_t size = WideCharToMultiByte(
		CP_UTF8,
		0,
		p_cstring.data(),
		static_cast<int>(p_cstring.size()),
		0,
		0,
		NULL,
		NULL
	);
    
	res.resize(size);

	WideCharToMultiByte(
		CP_UTF8,
		0,
		p_cstring.data(),
		static_cast<int>(p_cstring.size()),
		const_cast<LPSTR>(res.data()),
		static_cast<int>(size),
		NULL,
		NULL
	);
#else
	const auto size = wcstombs(0, p_cstring.c_str(), 0);
	res.resize(size);
	wcstombs((char*)res.data(), p_cstring.c_str(), size);
#endif
#endif
	return res;
}

inline std::wstring Utf8ToWstring(const char* pstr, size_t psize)
{
	std::wstring res;

#ifdef USE_SIMD_UTF_CONVERSION
    if constexpr(sizeof(wchar_t) == 2)
    {
        const size_t expected_utf16words = simdutf::utf16_length_from_utf8(pstr, psize);

        res.resize(expected_utf16words);

        simdutf::convert_utf8_to_utf16le(pstr, psize, (char16_t*) res.data());
    }
    else
    {
        const size_t expected_utfwords = simdutf::utf32_length_from_utf8(pstr, psize);

        res.resize(expected_utfwords);

        simdutf::convert_utf8_to_utf32(pstr, psize, (char32_t*) res.data());
    }
#else

#if defined(_WIN32)
	const size_t size = MultiByteToWideChar(
		CP_UTF8,
		0,
		pstr,
		static_cast<int>(psize),
		0,
		0
	);

	res.resize(size);

	MultiByteToWideChar(
		CP_UTF8,
		0,
		pstr,
		static_cast<int>(psize),
		const_cast<LPWSTR>(res.data()),
		static_cast<int>(size)
	);
#else
	const auto size = mbstowcs(0, pstr, 0);
	res.resize(size);
	mbstowcs(const_cast<wchar_t*>(res.data()), pstr, size);
#endif
#endif
	return res;
}

inline std::wstring Utf8ToWstring(const std::string& p_string)
{
	return Utf8ToWstring(p_string.data(), p_string.size());
}

inline std::wstring Utf8ToWstring(const char* p_string)
{
	return Utf8ToWstring(p_string, strlen(p_string));
}

#ifdef _WIN32
    
    inline std::wstring ToUtf16( const std::wstring& s )
    {
        assert( sizeof(wchar_t) == 2 );
        return s;
    }
    
#else
/*
#ifdef __INTEL__
    typedef char16_t TChar;
#else
    typedef unsigned short TChar;
#endif
 */
#if __cplusplus <= 199711L // condition for new mac
    typedef unsigned short TChar;
#else
    typedef char16_t TChar;
#endif
    
    typedef std::basic_string<TChar, std::char_traits<TChar>, std::allocator<TChar> > utf16_string;
    
    inline utf16_string ToUtf16( const std::wstring& s )
    {
        // On Mac. Wide-string is 32-bit.Lame conversion.
        utf16_string r;
        r.resize( s.size() );
        
        TChar* dest = (TChar*) r.data();
        for( size_t i = 0 ; i < s.size() ; ++i )
        {
            *dest++ = (TChar) s[i];
        }
        return r;
    }
    
#endif
}
