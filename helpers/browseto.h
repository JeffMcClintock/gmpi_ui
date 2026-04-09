#pragma once
/*
#include "helpers/browseto.h"
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
#include <shlobj.h>
#include <shellapi.h>
#elif defined(__APPLE__)
// forward declaration of the Objective-C++ implementation in browseto.mm
void browse_to_impl(const char* utf8_path);
#endif

namespace gmpi
{

// Open the system file browser and reveal the given path (file or folder).
// On Windows: opens Explorer with the item selected.
// On macOS:   opens Finder with the item selected ("Show in Finder").
inline void browse_to(const std::string& utf8_path)
{
#if defined(_WIN32)
    const auto wide = gmpi::unicode::to_wide(utf8_path);
    // SHParseDisplayName + SHOpenFolderAndSelectItems gives proper "select item" behaviour
    PIDLIST_ABSOLUTE pidl = nullptr;
    if (SUCCEEDED(SHParseDisplayName(wide.c_str(), nullptr, &pidl, 0, nullptr)))
    {
        SHOpenFolderAndSelectItems(pidl, 0, nullptr, 0);
        ILFree(pidl);
    }
    else
    {
        // fallback: just open the path
        ShellExecuteW(nullptr, L"open", wide.c_str(), nullptr, nullptr, SW_SHOWDEFAULT);
    }
#elif defined(__APPLE__)
    // implemented in browseto.mm (requires AppKit / Objective-C++)
    browse_to_impl(utf8_path.c_str());
#endif
}

// Convenience overload for wide strings (common in SynthEdit internals).
inline void browse_to(const std::wstring& path)
{
    browse_to(gmpi::unicode::to_utf8(path));
}

} // namespace gmpi
