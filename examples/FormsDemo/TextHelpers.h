#pragma once

#include <cstddef>
#include <string>

// 's' without its leading and trailing whitespace.
inline std::string trimmed(const std::string& s)
{
    const auto isBlank = [](char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; };

    std::size_t b = 0, e = s.size();
    while (b < e && isBlank(s[b])) ++b;
    while (e > b && isBlank(s[e - 1])) --e;
    return s.substr(b, e - b);
}
