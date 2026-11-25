#pragma once

// Text <-> value conversions for the form's editors.

#include <cstddef>
#include <cstdlib>
#include <format>
#include <optional>
#include <string>

inline bool isBlank(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

// 's' without its leading and trailing whitespace.
inline std::string trimmed(const std::string& s)
{
    std::size_t b = 0, e = s.size();
    while (b < e && isBlank(s[b])) ++b;
    while (e > b && isBlank(s[e - 1])) --e;
    return s.substr(b, e - b);
}

// The shortest text that reads back as the same double: "1.5", "100", "0.001".
inline std::string niceDoubleToString(double v)
{
    return std::format("{}", v);
}

// The whole string as a number, whitespace around it allowed; "12abc" is rejected, not read as 12.
inline std::optional<double> parseDouble(const std::string& text)
{
    const char* begin = text.c_str();
    char* end{};
    const double v = std::strtod(begin, &end);

    if (end == begin)
        return {};

    while (isBlank(*end))
        ++end;

    if (*end != '\0')
        return {};

    return v;
}
