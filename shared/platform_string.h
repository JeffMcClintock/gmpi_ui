#pragma once

/*
#include "../shared/platform_string.h"
*/

#include <string>
#include "./unicode_conversion.h"

#if defined(_WIN32) && (defined(UNICODE) || defined(_UNICODE))
    #define SE_PLATFORM_STRING_IS_WIDE 1
#else
    #define SE_PLATFORM_STRING_IS_WIDE 0
#endif


#ifdef _WIN32
	#include "tchar.h"
#else

#ifndef __TCHAR_DEFINED
	#if SE_PLATFORM_STRING_IS_WIDE
		typedef wchar_t _TCHAR;
	#ifndef _T
			#define _T(x) L##x
		#endif
		#else
		typedef char _TCHAR;
		#ifndef _T
			#define _T(x) x
		#endif
	#endif
//?	typedef _TCHAR* LPTSTR;
#endif

#endif

using platform_string = std::basic_string<_TCHAR, std::char_traits<_TCHAR>, std::allocator<_TCHAR>>;

#if SE_PLATFORM_STRING_IS_WIDE

	inline platform_string ToPlatformString(const std::wstring& s)
	{
		return s;
	}

	inline platform_string ToPlatformString(const std::string& s)
	{
		return JmUnicodeConversions::Utf8ToWstring(s);
	}

	inline platform_string ToPlatformString(const char* s)
	{
		return JmUnicodeConversions::Utf8ToWstring(s);
	}

	inline std::string ToUtf8String(platform_string s)
	{
		return JmUnicodeConversions::WStringToUtf8(s);
	}
	inline std::wstring toWstring(const platform_string& s)
	{
		return s;
	}
	inline std::string toString(const platform_string& s)
	{
		return JmUnicodeConversions::WStringToUtf8(s);
	}
	inline platform_string toPlatformString(const std::wstring& s)
	{
		return s;
	}
	inline platform_string toPlatformString(const std::string& s)
	{
		return JmUnicodeConversions::Utf8ToWstring(s);
	}
#else

	inline platform_string ToPlatformString(const std::wstring& s)
	{
		return JmUnicodeConversions::WStringToUtf8(s);
	}

	inline platform_string ToPlatformString(const std::string& s)
	{
		return s;
	}

	inline platform_string ToPlatformString(const char* s)
	{
		return platform_string(s);
	}

	inline std::string ToUtf8String(platform_string s)
	{
		return s;
	}
	inline std::string toString(const platform_string& s)
	{
		return s;
	}
	inline std::wstring toWstring(const platform_string& s)
	{
		return JmUnicodeConversions::Utf8ToWstring(s);
	}
	inline std::wstring toWstring(const _TCHAR* s)
	{
		return toWstring(platform_string(s));
	}
	inline platform_string toPlatformString(const std::string& s)
	{
		return s;
	}
	inline platform_string toPlatformString(const std::wstring& s)
	{
		return JmUnicodeConversions::WStringToUtf8(s);
	}
#endif


