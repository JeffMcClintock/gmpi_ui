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
#elif defined(__linux__)
#include <unistd.h>
#include <sys/wait.h>
#endif

namespace gmpi
{

// Open a URL in the system default browser.
// On Windows: uses ShellExecuteW with the "open" verb.
// On macOS:   uses NSWorkspace openURL:.
// On Linux:   execs xdg-open.
inline void open_url(const std::string& utf8_url)
{
#if defined(_WIN32)
    const auto wide = gmpi::unicode::to_wide(utf8_url);
    ShellExecuteW(nullptr, L"open", wide.c_str(), nullptr, nullptr, SW_SHOWDEFAULT);
#elif defined(__APPLE__)
    // implemented in openurl.mm (requires AppKit / Objective-C++)
    open_url_impl(utf8_url.c_str());
#elif defined(__linux__)
    // fork/exec rather than system(): the URL then reaches xdg-open as one
    // argument, with no shell to reinterpret quotes or metacharacters in it.
    // Double-fork so the intermediate child exits immediately and the browser
    // is reparented to init - nobody has to reap it, and the app never blocks
    // on a browser that outlives us.
    const pid_t pid = fork();
    if (pid == 0)
    {
        if (fork() == 0)
        {
            execlp("xdg-open", "xdg-open", utf8_url.c_str(), (char*)nullptr);
            _exit(127); // exec failed (no xdg-open installed)
        }
        _exit(0);
    }
    else if (pid > 0)
    {
        int status = 0;
        waitpid(pid, &status, 0); // the intermediate child, which exits at once
    }
#endif
}

// Convenience overload for wide strings (common in SynthEdit internals).
inline void open_url(const std::wstring& url)
{
    open_url(gmpi::unicode::to_utf8(url));
}

} // namespace gmpi
