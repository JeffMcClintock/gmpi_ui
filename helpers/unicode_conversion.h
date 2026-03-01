#pragma once
/*
#include "helpers/unicode_conversion.h"

using namespace JmUnicodeConversions;
*/

#include <string>
#include <string_view>
#include <assert.h>
#include <stdlib.h>	 // wcstombs() on Linux.
#if defined(_WIN32)
#undef  WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#undef  NOMINMAX
#define NOMINMAX
#include "windows.h"
#endif

namespace gmpi::unicode
{
inline std::string to_utf8(std::wstring_view text)
{
    std::string res;

#if defined(_WIN32)
    const size_t size = WideCharToMultiByte(
		CP_UTF8,
		0,
		text.data(),
		static_cast<int>(text.size()),
		0,
		0,
		NULL,
		NULL
	);
    
	res.resize(size);

	WideCharToMultiByte(
		CP_UTF8,
		0,
		text.data(),
		static_cast<int>(text.size()),
		const_cast<LPSTR>(res.data()),
		static_cast<int>(size),
		NULL,
		NULL
	);
#else
	const std::wstring temp(text);
	const auto size = wcstombs(0, temp.c_str(), 0);
	res.resize(size);
	wcstombs((char*)res.data(), temp.c_str(), size);
#endif

    return res;
}

inline std::wstring to_wide(std::string_view utf8)
{
	std::wstring res;

#if defined(_WIN32)
	const size_t size = MultiByteToWideChar(
		CP_UTF8,
		0,
		utf8.data(),
		static_cast<int>(utf8.size()),
		0,
		0
	);

	res.resize(size);

	MultiByteToWideChar(
		CP_UTF8,
		0,
		utf8.data(),
		static_cast<int>(utf8.size()),
		const_cast<LPWSTR>(res.data()),
		static_cast<int>(size)
	);
#else
	const std::string temp(utf8);
	const auto size = mbstowcs(0, temp.c_str(), 0);
	res.resize(size);
	mbstowcs(const_cast<wchar_t*>(res.data()), temp.c_str(), size);
#endif

    return res;
}
}
