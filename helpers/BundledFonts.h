#pragma once

/*
#include "helpers/BundledFonts.h"
*/

// Font files a product ships with itself, consulted by findFont() BEFORE the
// platform font database.
//
// Why bother: asking the OS for a family by name gives a different face on every
// platform - "Verdana" resolves to Verdana on Windows and (via fontconfig) to Noto
// Sans on a typical Linux box. Different advance widths mean different text
// extents, and anything sized from text - a module box auto-fitted to its pin
// labels - then comes out a different size per platform. Shipping the face removes
// the substitution entirely, so layout is identical everywhere and golden images
// mean something across machines.
//
//     registerBundledFont("Selawik", assetDir / "selawk.ttf");
//     registerBundledFont("Selawik", assetDir / "selawkb.ttf", FontWeight::Bold);
//
// Registration is global and additive; do it once during startup, before any text
// format is created.

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

#include "FontFile.h"

namespace gmpi
{
namespace drawing
{

namespace detail
{

struct BundledFontEntry
{
    std::string family; // lower-cased for case-insensitive matching
    FontWeight weight{ FontWeight::Regular };
    FontStyle style{ FontStyle::Normal };
    std::filesystem::path file;
};

inline std::vector<BundledFontEntry>& bundledFonts()
{
    static std::vector<BundledFontEntry> entries;
    return entries;
}

inline std::string lowerAscii(std::string_view s)
{
    std::string r(s);
    std::transform(r.begin(), r.end(), r.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return r;
}

inline bool readWholeFileBytes(const std::filesystem::path& p, std::vector<uint8_t>& out)
{
    std::ifstream f(p, std::ios::binary | std::ios::ate);
    if (!f)
        return false;

    const auto size = f.tellg();
    if (size <= 0)
        return false;

    out.resize(static_cast<size_t>(size));
    f.seekg(0);
    f.read(reinterpret_cast<char*>(out.data()), size);
    return static_cast<bool>(f);
}

} // namespace detail

// Make a font file available under `family`. Call once per weight/style shipped.
inline void registerBundledFont(std::string_view family,
                                std::filesystem::path file,
                                FontWeight weight = FontWeight::Regular,
                                FontStyle style = FontStyle::Normal)
{
    if (family.empty() || file.empty())
        return;

    auto& entries = detail::bundledFonts();
    const auto lowered = detail::lowerAscii(family);

    // re-registering the same slot replaces it, so a host can override an asset
    for (auto& e : entries)
    {
        if (e.family == lowered && e.weight == weight && e.style == style)
        {
            e.file = std::move(file);
            return;
        }
    }

    entries.push_back({ lowered, weight, style, std::move(file) });
}

inline void clearBundledFonts()
{
    detail::bundledFonts().clear();
}

// Serve `request` from a registered file, or return false to let the caller fall
// through to the platform font database.
inline bool findBundledFont(const FontRequest& request, FontData& returnFont)
{
    const auto& entries = detail::bundledFonts();
    if (entries.empty())
        return false;

    // A request carrying mustCoverCodepoint is a FALLBACK probe: the text engine
    // has hit a character the primary face lacks and is asking who can draw it.
    // Bundled fonts must never answer those. Selawik covers Latin only, so
    // claiming a Cyrillic codepoint here would return a face that draws .notdef
    // instead of letting the system database find one that has the glyph.
    if (request.mustCoverCodepoint != 0)
        return false;

    const auto wanted = detail::lowerAscii(request.familyName);

    const detail::BundledFontEntry* best{};
    int bestScore = -1;

    for (const auto& e : entries)
    {
        if (e.family != wanted)
            continue;

        // Prefer the requested style, then the closest weight. Shipping Regular
        // and Bold and asking for SemiBold should give Bold, not nothing.
        const int styleScore  = (e.style == request.style) ? 1000 : 0;
        const int weightDelta = std::abs(static_cast<int>(e.weight) - static_cast<int>(request.weight));
        const int score = styleScore - weightDelta;

        if (score > bestScore)
        {
            bestScore = score;
            best = &e;
        }
    }

    if (!best)
        return false;

    returnFont = {};
    if (!detail::readWholeFileBytes(best->file, returnFont.bytes))
        return false;

    returnFont.faceIndex = 0;
    // the path identifies what was resolved, so two requests landing on the same
    // file share one loaded copy (same contract as the platform providers)
    returnFont.resolvedName = best->file.string();
    return true;
}

} // namespace drawing
} // namespace gmpi
