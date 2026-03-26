#pragma once
#include "Drawing.h"
#include <cstring>

namespace gmpi { namespace drawing {

// IEEE 754 half-precision ↔ float conversion helpers.
namespace detail
{
    inline uint16_t floatToHalf(float f)
    {
        uint32_t bits; std::memcpy(&bits, &f, 4);
        uint16_t sign = static_cast<uint16_t>((bits >> 16) & 0x8000u);
        int32_t  exp  = static_cast<int32_t>((bits >> 23) & 0xffu) - 127 + 15;
        uint32_t mant = bits & 0x7fffffu;
        if ((bits & 0x7fffffffu) > 0x7f800000u) return sign | 0x7e00u;
        if (exp >= 31) return sign | 0x7c00u;
        if (exp <= 0) {
            if (exp < -10) return sign;
            mant |= 0x800000u;
            int shift = 14 - exp;
            return sign | static_cast<uint16_t>((mant + (1u << (shift - 1))) >> shift);
        }
        return sign | static_cast<uint16_t>((static_cast<uint32_t>(exp) << 10) | (mant >> 13));
    }

    inline float halfToFloat(uint16_t h)
    {
        const uint32_t sign = (h >> 15) & 0x1u;
        const uint32_t exp  = (h >> 10) & 0x1fu;
        const uint32_t mant = h & 0x3ffu;
        uint32_t bits;
        if (exp == 0)
        {
            if (mant == 0) { bits = sign << 31; }
            else
            {
                uint32_t e = 0, m = mant;
                while (!(m & 0x400u)) { m <<= 1; ++e; }
                bits = (sign << 31) | ((127 - 15 - e + 1) << 23) | ((m & 0x3ffu) << 13);
            }
        }
        else if (exp == 31)
        {
            bits = (sign << 31) | 0x7f800000u | (mant << 13);
        }
        else
        {
            bits = (sign << 31) | ((exp + (127 - 15)) << 23) | (mant << 13);
        }
        float f; std::memcpy(&f, &bits, 4); return f;
    }
}

// Apply an 8-bit monochrome mask to a colour bitmap, pixel-by-pixel.
// For each pixel the mask value (0–255) scales all premultiplied channels,
// so mask=0 → fully transparent, mask=255 → unchanged.
// Handles both 64bppPRGBAHalf and 32bppPBGRA colour formats.
//
// Both bitmaps must be the same dimensions and CPU-readable.
// The mask must be 8bpp (1 byte per pixel), e.g. created with
// BitmapRenderTargetFlags::Mask | CpuReadable.
inline void applyMask(Bitmap& image, Bitmap& mask)
{
    auto imagePx = image.lockPixels(BitmapLockFlags::ReadWrite);
    auto maskPx  = mask.lockPixels(BitmapLockFlags::Read);

    uint8_t*       imageData = imagePx.getAddress();
    const uint8_t* maskData  = maskPx.getAddress();
    const int32_t  imageBpr  = imagePx.getBytesPerRow();
    const int32_t  maskBpr   = maskPx.getBytesPerRow();
    const auto     size      = image.getSize();
    const int32_t  imageBpp  = imageBpr / static_cast<int32_t>(size.width);

    for (uint32_t y = 0; y < size.height; ++y)
    {
        for (uint32_t x = 0; x < size.width; ++x)
        {
            const float maskVal = maskData[y * maskBpr + x] / 255.0f;
            if (maskVal >= 1.0f) continue;

            uint8_t* px = imageData + y * imageBpr + x * imageBpp;
            if (imageBpp == 8)
            {
                // 64bppPRGBAHalf: premultiplied RGBA half-float.
                uint16_t ch[4]; std::memcpy(ch, px, 8);
                for (int c = 0; c < 4; ++c)
                    ch[c] = detail::floatToHalf(detail::halfToFloat(ch[c]) * maskVal);
                std::memcpy(px, ch, 8);
            }
            else
            {
                // 32bppPBGRA: premultiplied BGRA 8-bit.
                for (int c = 0; c < 4; ++c)
                    px[c] = static_cast<uint8_t>(px[c] * maskVal + 0.5f);
            }
        }
    }
}

}} // namespace gmpi::drawing
