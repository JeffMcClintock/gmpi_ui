#pragma once
#include "Drawing.h"
#include <cstring>
#include <iterator>

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

struct RgbaHalfPixel
{
    uint16_t red;
    uint16_t green;
    uint16_t blue;
    uint16_t alpha;

    float r() const
    {
        return detail::halfToFloat(red);
    }

    void setR(float value)
    {
        red = detail::floatToHalf(value);
    }

    float g() const
    {
        return detail::halfToFloat(green);
    }

    void setG(float value)
    {
        green = detail::floatToHalf(value);
    }

    float b() const
    {
        return detail::halfToFloat(blue);
    }

    void setB(float value)
    {
        blue = detail::floatToHalf(value);
    }

    float a() const
    {
        return detail::halfToFloat(alpha);
    }

    void setA(float value)
    {
        alpha = detail::floatToHalf(value);
    }

    void getRGBA(float* values) const
    {
        values[0] = detail::halfToFloat(red);
        values[1] = detail::halfToFloat(green);
        values[2] = detail::halfToFloat(blue);
        values[3] = detail::halfToFloat(alpha);
    }

    void setRGBA(const float* values)
    {
        red   = detail::floatToHalf(values[0]);
        green = detail::floatToHalf(values[1]);
        blue  = detail::floatToHalf(values[2]);
        alpha = detail::floatToHalf(values[3]);
    }
};

template<typename TPixel>
struct PixelPosition
{
    TPixel* pixel = {};
    int32_t x = 0;
    int32_t y = 0;

    TPixel& value() const
    {
        return *pixel;
    }

    TPixel& operator*() const
    {
        return *pixel;
    }

    TPixel* operator->() const
    {
        return pixel;
    }
};

template<typename TPixel>
class PixelRange
{
    BitmapPixels& pixels;

public:
    explicit PixelRange(BitmapPixels& pixels) : pixels(pixels)
    {
    }

    class iterator
    {
        uint8_t* data = {};
        int32_t bytesPerRow = 0;
        uint32_t width = 0;
        size_t index = 0;
        size_t total = 0;
        PixelPosition<TPixel> current;

        void updateCurrent()
        {
            if(index >= total)
                return;

            current.x = static_cast<int32_t>(index % width);
            current.y = static_cast<int32_t>(index / width);
            auto row = data + static_cast<size_t>(current.y) * bytesPerRow;
            current.pixel = reinterpret_cast<TPixel*>(row) + current.x;
        }

    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = PixelPosition<TPixel>;
        using difference_type = std::ptrdiff_t;
        using reference = PixelPosition<TPixel>&;
        using pointer = PixelPosition<TPixel>*;

        iterator(BitmapPixels& pixels, size_t index) : data(pixels.getAddress()), bytesPerRow(pixels.getBytesPerRow()), index(index)
        {
            const auto size = pixels.getSize();
            width = size.width;
            total = static_cast<size_t>(size.width) * size.height;
            updateCurrent();
        }

        reference operator*()
        {
            return current;
        }

        pointer operator->()
        {
            return &current;
        }

        iterator& operator++()
        {
            ++index;
            updateCurrent();
            return *this;
        }

        bool operator==(const iterator& other) const
        {
            return data == other.data && index == other.index;
        }

        bool operator!=(const iterator& other) const
        {
            return !(*this == other);
        }
    };

    iterator begin()
    {
        return iterator{ pixels, 0 };
    }

    iterator end()
    {
        const auto size = pixels.getSize();
        return iterator{ pixels, static_cast<size_t>(size.width) * size.height };
    }
};

template<typename TPixel>
inline PixelRange<TPixel> pixelIterator(BitmapPixels& pixels)
{
    return PixelRange<TPixel>(pixels);
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
            if (imageBpp == 16)
            {
                // 128bpp float: premultiplied RGBA 32-bit float (macOS default RT).
                float ch[4]; std::memcpy(ch, px, 16);
                for (int c = 0; c < 4; ++c)
                    ch[c] *= maskVal;
                std::memcpy(px, ch, 16);
            }
            else if (imageBpp == 8)
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
