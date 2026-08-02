#pragma once

/*
#include "helpers/DecodedImage.h"
*/

// The interchange format between an image decoder and a renderer, kept in its
// own header so that neither side has to include the other: the CPU backend
// needs the type but must stay free of platform code, and the decoders in
// helpers/DecodeImage.h must not depend on any particular backend.
//
// Exactly one format, deliberately: 8-bit sRGB RGBA with straight
// (non-premultiplied) alpha, tightly packed. Every decoder normalises to it,
// and the renderer converts once, on load, into its own working format.

#include <cstdint>
#include <vector>

namespace gmpi { namespace drawing {

struct DecodedImage
{
    uint32_t width{};
    uint32_t height{};
    std::vector<uint8_t> pixels; // RGBA8, sRGB, straight alpha, tightly packed

    explicit operator bool() const
    {
        return width > 0 && height > 0 && pixels.size() >= size_t(width) * height * 4;
    }
};

}} // namespace gmpi::drawing
