#pragma once

/*
#include "../shared/string_utilities.h"
*/

#include <string>
#include <algorithm>
#include "../shared/xplatform.h"

inline std::wstring Left(const std::wstring& s, size_t count)
{
	return s.substr(0, count);
}

inline std::string Left(const std::string& s, size_t count)
{
	return s.substr(0, count);
}

inline std::wstring Right(const std::wstring& s, size_t count)
{
	count = (std::min)(count, s.size());
	return s.substr(s.size() - count);
}

inline std::string Right(const std::string& s, size_t count)
{
	count = (std::min)(count, s.size());
	return s.substr(s.size() - count);
}

// C:\temp\file.txt => file.txt
inline std::wstring StripPath(const std::wstring& p_filename)
{
	std::string::size_type p = p_filename.find_last_of(L"\\/");

	if (p != std::string::npos)
	{
		return Right(p_filename, p_filename.size() - p - 1);
	}

	return p_filename;
}

inline std::string StripPath(const std::string& p_filename)
{
	std::string::size_type p = p_filename.find_last_of("\\/");

	if (p != std::string::npos)
	{
		return Right(p_filename, p_filename.size() - p - 1);
	}

	return p_filename;
}

template<typename T1>
T1 StripFilename_implementation(const T1& p_filename) // Leaving path.
{
	const auto p1 = p_filename.find_last_of('\\');
	const auto p2 = p_filename.find_last_of('/');
	
	size_t p = std::string::npos;
	if (p1 != std::string::npos && p2 != std::string::npos)
	{
		p = (std::max)(p1, p2);
	}
	else
	{
		if (p1 != std::string::npos)
		{
			p = p1;
		}
		else
		{
			p = p2;
		}
	}

	if( p != std::string::npos )
	{
		return Left(p_filename, p);
	}

	return p_filename;
}

inline std::string StripFilename(const std::string& p_filename) // Leaving path.
{
	return StripFilename_implementation(p_filename);
}

inline std::wstring StripFilename(const std::wstring& p_filename) // Leaving path.
{
	return StripFilename_implementation(p_filename);
}

inline std::string StripExtension(const std::string& p_filename)
{
	size_t p = p_filename.size(); // Not using unsigned size_t else fails <0 test below. Could be re-written to fix this.

	while (p > 0)
	{
		p--;
		char c = p_filename[p];

		// path separator? Must be no extension (after the slash).
		if (c == '\\' || c == '/')
			break;

		if (c == '.')
			return p_filename.substr(0, p);
	}

	return p_filename;
}

template<typename T1>
T1 GetExtension(const T1& p_filename)
{
	size_t p = p_filename.size(); // Not using unsigned size_t else fails <0 test below. Could be re-written to fix this.

	while (p > 0)
	{
		p--;
		char c = p_filename[p];

		// path separator? Must be no extension (after the slash).
		if (c == '\\' || c == '/')
			return T1();

		if (c == '.')
			return p_filename.substr(p+1);
	}

	return T1();
}


inline std::wstring StripExtension( const std::wstring& p_filename)
{
	size_t p = p_filename.size(); // Not using unsigned size_t else fails <0 test below. Could be re-written to fix this.

	while( p > 0 )
	{
		p--;
		wchar_t c = p_filename[p];

		// path seperator? Must be no extension (after the slash).
		if( c == L'\\' || c == L'/' )
			break;

		if( c == L'.' )
			return p_filename.substr(0,p);
	}

	return p_filename;
}

// combines path and file,
// handles tricky situations like both having slashes, or not.
template<typename T1>
T1 combinePathAndFile(const T1 p_path, const T1 p_file) // Leaving path.
{
	// ensure path ends in slash
	auto first_bit = p_path;

	//	if( Right(first_bit,1) != (L"\\") && !first_bit.empty() )
	if (!first_bit.empty() )
	{
        auto last = first_bit[first_bit.size() -1];
        if (last != '\\' && last != '/')
		{
			first_bit += PLATFORM_PATH_SLASH;
        }
	}

	// ensure file does not begin with slash
	auto last_bit = p_file;

	if ( !last_bit.empty() && (last_bit[0] == '\\' || last_bit[0] == '/'))
	{
		last_bit = Right(p_file, p_file.size() - 1);
	}

	return first_bit + last_bit;
}

inline std::string combinePathAndFile(const char* p_path, const char* p_file) // Leaving path.
{
	std::string path(p_path);
	std::string file(p_file);
	return combinePathAndFile(path, file);
}

inline std::wstring combinePathAndFile(const wchar_t* p_path, const wchar_t* p_file) // Leaving path.
{
	std::wstring path(p_path);
	std::wstring file(p_file);
	return combinePathAndFile(path, file);
}

template<typename S>
inline S ToNativeSlashes(S path)
{
	for (auto& c : path)
	{
		if (c == '/' || c == '\\')
			c = PLATFORM_PATH_SLASH;
	}

	return path;
}

