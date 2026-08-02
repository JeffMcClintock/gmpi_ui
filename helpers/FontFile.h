#pragma once

/*
#include "helpers/FontFile.h"
*/

// The interchange format between font discovery and a text engine, kept in its
// own header so neither side has to include the other: the software text
// engine needs the type but must stay free of platform code, and the providers
// in helpers/FontProvider.h must not depend on any particular text engine.
//
// Discovery is the only genuinely platform-specific part of text. Turning a
// family name plus weight/style into actual font bytes needs the system font
// database (DirectWrite, CoreText, fontconfig); everything after that —
// shaping, outlines, layout, rasterization — is portable.

#include <cstdint>
#include <string>
#include <vector>

#include "../GmpiApiDrawing.h"

namespace gmpi { namespace drawing {

struct FontRequest
{
    std::string familyName;                        // "Arial"; "system-ui" means the OS UI font
    FontWeight weight{ FontWeight::Regular };
    FontStyle style{ FontStyle::Normal };
    FontStretch stretch{ FontStretch::Normal };

    // Stage B+: when set, the provider should return a font that can render
    // this codepoint, for fallback across scripts. No single font covers Latin,
    // CJK and emoji, so text has to be split into runs by covering font.
    uint32_t mustCoverCodepoint{};
};

struct FontData
{
    std::vector<uint8_t> bytes;   // the whole font file
    uint32_t faceIndex{};         // index within a collection (.ttc)
    std::string resolvedName;     // what the system actually matched, for diagnostics

    explicit operator bool() const { return !bytes.empty(); }
};

}} // namespace gmpi::drawing
