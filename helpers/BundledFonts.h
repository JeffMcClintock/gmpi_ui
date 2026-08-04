#pragma once

/*
#include "helpers/BundledFonts.h"
*/

// A tiny process-global registry for fonts a caller wants guaranteed
// available regardless of what happens to be installed on the machine —
// e.g. a test fixture that wants pixel-identical text across every dev
// machine and CI runner, or a host bundling its own UI font with a plugin.
//
// helpers/FontProvider.h's findFont() checks this registry first, before
// touching the system font database, so a registered family always wins for
// the CPU backend. backends/DirectXGfx.h checks it too — Direct2D resolves
// fonts through its own DirectWrite system-collection path rather than
// through findFont, so it builds a private DirectWrite font collection from
// the same entries, keeping both backends resolving to the same bytes.

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <limits>
#include <string>
#include <vector>

#include "../GmpiApiDrawing.h"

namespace gmpi { namespace drawing {

struct BundledFontEntry
{
    std::string familyNameLower;
    FontWeight  weight{ FontWeight::Regular };
    FontStyle   style{ FontStyle::Normal };
    std::string filePath;
};

inline std::vector<BundledFontEntry>& bundledFontRegistry()
{
    static std::vector<BundledFontEntry> registry;
    return registry;
}

inline std::string toLowerAscii(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

// Registers a font file under a family name. Safe to call more than once for
// the same (family, weight, style, path) — e.g. from a header-only fixture
// included by several translation units, each running its own static
// initializer — duplicates are silently ignored rather than piling up.
inline void registerBundledFont(const std::string& familyName, FontWeight weight,
                                FontStyle style, const std::string& filePath)
{
    const std::string lower = toLowerAscii(familyName);
    auto& registry = bundledFontRegistry();
    for (const auto& entry : registry)
    {
        if (entry.familyNameLower == lower && entry.weight == weight &&
            entry.style == style && entry.filePath == filePath)
            return;
    }
    registry.push_back({ lower, weight, style, filePath });
}

// Finds the closest weight match for a family/style, or nullptr if the
// family was never registered. Style must match exactly — no bundled
// italics yet.
inline const BundledFontEntry* findBundledFont(const std::string& familyName,
                                                FontWeight weight, FontStyle style)
{
    const std::string lower = toLowerAscii(familyName);
    const BundledFontEntry* best{};
    int bestDist = (std::numeric_limits<int>::max)();
    for (const auto& entry : bundledFontRegistry())
    {
        if (entry.familyNameLower != lower || entry.style != style)
            continue;
        const int dist = std::abs(static_cast<int>(entry.weight) - static_cast<int>(weight));
        if (dist < bestDist)
        {
            bestDist = dist;
            best = &entry;
        }
    }
    return best;
}

}} // namespace gmpi::drawing
