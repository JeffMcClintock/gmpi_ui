#pragma once

/*
#include "helpers/TextSegmentation.h"
*/

// Where text may be split: grapheme cluster boundaries (what counts as one
// user-perceived character) and line break opportunities. Pure std, no
// dependencies, no platform code.
//
// This is a deliberate SUBSET of UAX #29 and UAX #14. The full algorithms need
// large Unicode property tables; what is here covers the scripts this renderer
// targets — Latin, CJK and emoji — and is documented range by range so the
// gaps are visible rather than surprising.
//
// Covered:
//   * combining marks, so 'e' + U+0301 stays one cluster
//   * ZWJ sequences, so a family emoji is not split into people
//   * variation selectors and skin-tone modifiers
//   * regional indicator pairs, so a flag is one cluster
//   * CJK: a break is allowed between ideographs, with kinsoku rules keeping
//     closing punctuation off the start of a line and opening punctuation off
//     the end
//   * spaces and hyphens for Latin
//
// Not covered: Indic and other complex scripts (their clusters need script
// specific tables), and the full UAX #14 pair table. Right-to-left is out of
// scope for this renderer entirely.

#include <cstdint>
#include <string_view>
#include <vector>

namespace gmpi { namespace drawing { namespace text {

struct Codepoint
{
    uint32_t value{};
    uint32_t byteOffset{}; // offset of this codepoint in the source UTF-8
    uint32_t byteLength{};
};

// Decode UTF-8, substituting U+FFFD for malformed sequences so that bad input
// degrades to visible replacement characters rather than being dropped.
inline std::vector<Codepoint> decodeUtf8(std::string_view utf8)
{
    std::vector<Codepoint> out;
    out.reserve(utf8.size());

    size_t i = 0;
    while (i < utf8.size())
    {
        const auto start = i;
        const auto byte = static_cast<uint8_t>(utf8[i]);
        uint32_t value{};
        int extra{};

        if (byte < 0x80)          { value = byte;        extra = 0; }
        else if ((byte & 0xE0) == 0xC0) { value = byte & 0x1F; extra = 1; }
        else if ((byte & 0xF0) == 0xE0) { value = byte & 0x0F; extra = 2; }
        else if ((byte & 0xF8) == 0xF0) { value = byte & 0x07; extra = 3; }
        else { value = 0xFFFD; extra = 0; }

        ++i;
        for (int k = 0; k < extra; ++k)
        {
            if (i >= utf8.size() || (static_cast<uint8_t>(utf8[i]) & 0xC0) != 0x80)
            {
                value = 0xFFFD;
                break;
            }
            value = (value << 6) | (static_cast<uint8_t>(utf8[i]) & 0x3F);
            ++i;
        }

        out.push_back({ value, uint32_t(start), uint32_t(i - start) });
    }
    return out;
}

// --- character classes -----------------------------------------------------

inline bool isCombiningMark(uint32_t c)
{
    return (c >= 0x0300 && c <= 0x036F)   // combining diacriticals
        || (c >= 0x0483 && c <= 0x0489)
        || (c >= 0x1AB0 && c <= 0x1AFF)
        || (c >= 0x1DC0 && c <= 0x1DFF)
        || (c >= 0x20D0 && c <= 0x20FF)   // combining marks for symbols
        || (c >= 0xFE20 && c <= 0xFE2F);  // half marks
}

inline bool isVariationSelector(uint32_t c)
{
    return (c >= 0xFE00 && c <= 0xFE0F) || (c >= 0xE0100 && c <= 0xE01EF);
}

inline bool isSkinToneModifier(uint32_t c) { return c >= 0x1F3FB && c <= 0x1F3FF; }
inline bool isZeroWidthJoiner(uint32_t c)  { return c == 0x200D; }
inline bool isRegionalIndicator(uint32_t c) { return c >= 0x1F1E6 && c <= 0x1F1FF; }

// Anything that attaches to the preceding character rather than standing alone.
inline bool isGraphemeExtender(uint32_t c)
{
    return isCombiningMark(c) || isVariationSelector(c) || isSkinToneModifier(c)
        || isZeroWidthJoiner(c) || c == 0x200C /* ZWNJ */
        || (c >= 0x20E0 && c <= 0x20E4)  // enclosing marks used by keycaps
        || c == 0xFE0F;
}

// Scripts written without spaces, where a line may break between characters.
inline bool isIdeographic(uint32_t c)
{
    return (c >= 0x3040 && c <= 0x30FF)   // hiragana, katakana
        || (c >= 0x3400 && c <= 0x4DBF)   // CJK extension A
        || (c >= 0x4E00 && c <= 0x9FFF)   // CJK unified
        || (c >= 0xF900 && c <= 0xFAFF)   // compatibility ideographs
        || (c >= 0xFF00 && c <= 0xFF60)   // fullwidth forms
        || (c >= 0xAC00 && c <= 0xD7A3)   // hangul syllables
        || (c >= 0x20000 && c <= 0x2FA1F);// CJK extensions B-F
}

// Kinsoku: characters that must not start a line.
inline bool isNoBreakBefore(uint32_t c)
{
    switch (c)
    {
    case 0x3001: case 0x3002:                       // 、 。
    case 0xFF0C: case 0xFF0E: case 0xFF1A: case 0xFF1B: // ，．：；
    case 0xFF01: case 0xFF1F:                       // ！ ？
    case 0x300D: case 0x300F: case 0x3011: case 0x3015: // 」 』 】 〕
    case 0x3009: case 0x300B: case 0xFF09: case 0xFF3D: case 0xFF5D: // 〉 》 ） ］ ｝
    case 0x30FC: case 0x3005:                       // ー 々
    case 0x2019: case 0x201D:                       // ' "
        return true;
    default:
        break;
    }
    // Small kana cannot begin a line either.
    return (c >= 0x3041 && c <= 0x3049 && (c & 1) == 1)
        || c == 0x3063 || c == 0x3083 || c == 0x3085 || c == 0x3087 || c == 0x308E
        || c == 0x30C3 || c == 0x30E3 || c == 0x30E5 || c == 0x30E7 || c == 0x30EE;
}

// Characters that must not end a line.
inline bool isNoBreakAfter(uint32_t c)
{
    switch (c)
    {
    case 0x300C: case 0x300E: case 0x3010: case 0x3014: // 「 『 【 〔
    case 0x3008: case 0x300A: case 0xFF08: case 0xFF3B: case 0xFF5B: // 〈 《 （ ［ ｛
    case 0x2018: case 0x201C:                       // ' "
        return true;
    default:
        return false;
    }
}

// --- boundaries ------------------------------------------------------------

// True at indices that begin a new grapheme cluster. Index 0 is always a
// boundary; the result has one entry per codepoint.
inline std::vector<uint8_t> graphemeBoundaries(const std::vector<Codepoint>& cps)
{
    std::vector<uint8_t> boundary(cps.size(), 1);
    if (cps.empty())
        return boundary;

    int regionalRun = 0;
    for (size_t i = 0; i < cps.size(); ++i)
    {
        const uint32_t c = cps[i].value;

        if (i == 0)
        {
            regionalRun = isRegionalIndicator(c) ? 1 : 0;
            continue;
        }

        const uint32_t previous = cps[i - 1].value;

        if (isGraphemeExtender(c))
        {
            boundary[i] = 0; // attaches to whatever came before
        }
        else if (isZeroWidthJoiner(previous))
        {
            boundary[i] = 0; // the glyph after a ZWJ joins the sequence
        }
        else if (isRegionalIndicator(c) && regionalRun == 1)
        {
            boundary[i] = 0; // second half of a flag
        }

        if (isRegionalIndicator(c))
            regionalRun = (regionalRun == 1 && boundary[i] == 0) ? 0 : 1;
        else
            regionalRun = 0;
    }
    return boundary;
}

// True at indices where a line MAY break before that codepoint. Breaks are
// only ever offered at grapheme cluster boundaries.
inline std::vector<uint8_t> lineBreakOpportunities(const std::vector<Codepoint>& cps)
{
    std::vector<uint8_t> canBreak(cps.size(), 0);
    if (cps.empty())
        return canBreak;

    const auto clusters = graphemeBoundaries(cps);

    for (size_t i = 1; i < cps.size(); ++i)
    {
        if (!clusters[i])
            continue; // never split a grapheme cluster

        const uint32_t previous = cps[i - 1].value;
        const uint32_t current = cps[i].value;

        if (isNoBreakBefore(current) || isNoBreakAfter(previous))
            continue;

        // After a space: the space stays on the finished line.
        if (previous == ' ' || previous == '\t')
        {
            canBreak[i] = 1;
            continue;
        }
        if (current == ' ' || current == '\t')
            continue;

        // After a hyphen, and between ideographs.
        if (previous == '-' || previous == 0x2010 || previous == 0x2013 || previous == 0x2014)
            canBreak[i] = 1;
        else if (isIdeographic(previous) || isIdeographic(current))
            canBreak[i] = 1;
    }
    return canBreak;
}

}}} // namespace gmpi::drawing::text
