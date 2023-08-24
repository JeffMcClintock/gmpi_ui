#ifndef UNICODE_CONVERSION2_H_INCLUDED
#define UNICODE_CONVERSION2_H_INCLUDED

/*
#include "../shared/unicode_conversion2.h"

using namespace FastUnicode;
*/

#include <string>

namespace FastUnicode
{
	inline std::wstring Utf8ToWstring(char const* p_string)
	{
		/* FYI: The C++ standard method. Hideously slow. UPDATE: Slowness is caused by constructor only. Make it static to minimise this.
		std::wstring_convert<std::codecvt_utf8<wchar_t>> convert;
		return convert.from_bytes(p_string);
		*/

		assert(p_string != nullptr);

		const char* in = p_string;
		std::wstring out;
		unsigned int codepoint = 0;
		int following = 0;
		for (; *in != 0; ++in)
		{
			unsigned char ch = *in;
			if (ch <= 0x7f)
			{
				codepoint = ch;
				following = 0;
			}
			else if (ch <= 0xbf)
			{
				if (following > 0)
				{
					codepoint = (codepoint << 6) | (ch & 0x3f);
					--following;
				}
			}
			else if (ch <= 0xdf)
			{
				codepoint = ch & 0x1f;
				following = 1;
			}
			else if (ch <= 0xef)
			{
				codepoint = ch & 0x0f;
				following = 2;
			}
			else
			{
				codepoint = ch & 0x07;
				following = 3;
			}
			if (following == 0)
			{
				if (codepoint > 0xffff)
				{
					out.append(1, static_cast<wchar_t>(0xd800 + (codepoint >> 10)));
					out.append(1, static_cast<wchar_t>(0xdc00 + (codepoint & 0x03ff)));
				}
				else
					out.append(1, static_cast<wchar_t>(codepoint));
				codepoint = 0;
			}
		}
		return out;
	}

	inline std::string WStringToUtf8(const std::wstring& p_cstring)
	{
/* FYI: The C++ standard method. Hideously slow.
		std::wstring_convert<std::codecvt_utf8<wchar_t>> convert;
		return convert.to_bytes(p_cstring);
*/

		std::string out;
		unsigned int codepoint = 0;
		for (auto in = p_cstring.begin(); in != p_cstring.end(); ++in)
		{
			if (*in >= 0xd800 && *in <= 0xdbff)
				codepoint = ((*in - 0xd800) << 10) + 0x10000;
			else
			{
				if (*in >= 0xdc00 && *in <= 0xdfff)
					codepoint |= *in - 0xdc00;
				else
					codepoint = *in;

				if (codepoint <= 0x7f)
					out.append(1, static_cast<char>(codepoint));
				else if (codepoint <= 0x7ff)
				{
					out.append(1, static_cast<char>(0xc0 | ((codepoint >> 6) & 0x1f)));
					out.append(1, static_cast<char>(0x80 | (codepoint & 0x3f)));
				}
				else if (codepoint <= 0xffff)
				{
					out.append(1, static_cast<char>(0xe0 | ((codepoint >> 12) & 0x0f)));
					out.append(1, static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
					out.append(1, static_cast<char>(0x80 | (codepoint & 0x3f)));
				}
				else
				{
					out.append(1, static_cast<char>(0xf0 | ((codepoint >> 18) & 0x07)));
					out.append(1, static_cast<char>(0x80 | ((codepoint >> 12) & 0x3f)));
					out.append(1, static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
					out.append(1, static_cast<char>(0x80 | (codepoint & 0x3f)));
				}
				codepoint = 0;
			}
		}
		return out;
	}
} // namespace
#endif
