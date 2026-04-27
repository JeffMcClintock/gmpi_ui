#pragma once
/*
#include "helpers/openurl.h"
*/

#include <string>
#include "helpers/unicode_conversion.h"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#elif defined(__APPLE__)
// forward declaration of the Objective-C++ implementation in openurl.mm
void open_url_impl(const char* utf8_url);
#endif

namespace gmpi
{

// Open a URL in the system default browser.
// On Windows: uses ShellExecuteW with the "open" verb.
// On macOS:   uses NSWorkspace openURL:.
inline void open_url(const std::string& utf8_url)
{
#if defined(_WIN32)
    const auto wide = gmpi::unicode::to_wide(utf8_url);
    ShellExecuteW(nullptr, L"open", wide.c_str(), nullptr, nullptr, SW_SHOWDEFAULT);
#elif defined(__APPLE__)
    // implemented in openurl.mm (requires AppKit / Objective-C++)
    open_url_impl(utf8_url.c_str());
#endif
}

// Convenience overload for wide strings (common in SynthEdit internals).
inline void open_url(const std::wstring& url)
{
    open_url(gmpi::unicode::to_utf8(url));
}

} // namespace gmpi
