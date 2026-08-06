#pragma once

/*
#include "backends/CpuGfx.h"
*/

// Pure-software GMPI-UI backend. No GPU, no platform graphics API. See PLAN.md.
//
// Everything drawable funnels through one path:
//     path -> flatten (Gfx_base sink) -> coverage (signed-area rasterizer) -> blend
//
// Surfaces are premultiplied linear scRGB stored as RGBA half-float
// (PixelFormat::RGBA_16f, the same 64bppPRGBAHalf that createCpuRenderTarget
// produces on Windows). All arithmetic is fp32; fp16 appears only in the
// load/store span codec below.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <functional>
#include <vector>

#include "../Drawing.h"
#include "../helpers/BitmapMask.h"    // detail::floatToHalf / halfToFloat
#include "../helpers/DecodedImage.h"  // decoder interchange format (no platform code)
#include "Gfx_base.h"

namespace gmpi
{
namespace cpugfx
{

inline float srgbToLinear01(float c)
{
    c = c < 0.0f ? 0.0f : (c > 1.0f ? 1.0f : c);
    return (c <= 0.04045f) ? c / 12.92f
                           : std::pow((c + 0.055f) / 1.055f, 2.4f);
}

// The sRGB transfer function, DECODE direction - the inverse of
// drawing::linearToSRGB01, which is the encode half. Needed because glyph
// coverage is composited in gamma space to match Direct2D (see
// blendCoverageMask) and has to come back to the linear surface afterwards.
// Blend one premultiplied pixel.
//
// `gammaSpace` exists for TEXT ONLY. Everything else in this renderer is
// linear and stays that way, but Direct2D renders text into a plain BGRA8
// target and therefore mixes glyph coverage using the ENCODED values. Blending
// the same coverage linearly gives visibly lighter glyphs - measured against
// the D2D references, ours came out lighter on 1460 of 1497 differing pixels,
// mean +42.5/255, peaking near half coverage exactly as the arithmetic
// predicts. Layout was never wrong; only the weight was.
inline void blendPixel(float* d, const float* sc, float c, bool gammaSpace)
{
    const float sa = sc[3];
    const float k  = 1.0f - sa * c;

    // Fully covered and fully opaque is a straight copy in either space, and
    // it is the common case inside a glyph - worth taking before the transfer
    // function, which is not cheap.
    if (!gammaSpace || (c >= 1.0f && sa >= 1.0f))
    {
        d[0] = sc[0] * c + d[0] * k;
        d[1] = sc[1] * c + d[1] * k;
        d[2] = sc[2] * c + d[2] * k;
        d[3] = sc[3] * c + d[3] * k;
        return;
    }

    const float dstA = d[3];
    const float outA = sa * c + dstA * k;

    for (int ch = 0; ch < 3; ++ch)
    {
        // un-premultiply: a gamma curve is meaningless on premultiplied values
        const float srcLin = sa   > 0.0f ? sc[ch] / sa   : 0.0f;
        const float dstLin = dstA > 0.0f ? d[ch]  / dstA : 0.0f;

        const float mixed = drawing::linearToSRGB01(srcLin) * (sa * c)
                          + drawing::linearToSRGB01(dstLin) * (dstA * k);

        d[ch] = outA > 0.0f ? srgbToLinear01(mixed / outA) * outA : 0.0f;
    }

    d[3] = outA;
}



// ---------------------------------------------------------------------------
// Span codec: the ONLY place fp16 appears. Plain loops so compilers can
// auto-vectorize; F16C/NEON intrinsic versions can slot in behind the same
// signatures later without touching any kernel.
// ---------------------------------------------------------------------------
inline void loadSpan(const uint16_t* src, float* dst, int floatCount)
{
    for (int i = 0; i < floatCount; ++i)
        dst[i] = drawing::detail::halfToFloat(src[i]);
}

inline void storeSpan(const float* src, uint16_t* dst, int floatCount)
{
    for (int i = 0; i < floatCount; ++i)
        dst[i] = drawing::detail::floatToHalf(src[i]);
}

// ---------------------------------------------------------------------------
// Axis-aligned bounds of a rect transformed by an affine matrix.
// (Drawing.h's transformRect maps only two corners, which is wrong under
// rotation or negative scale; clip math needs true bounds of all four.)
// ---------------------------------------------------------------------------
inline drawing::Rect transformBounds(const drawing::Matrix3x2& m, const drawing::Rect& r)
{
    const drawing::Point corners[4] = {
        drawing::transformPoint(m, {r.left, r.top}), drawing::transformPoint(m, {r.right, r.top}),
        drawing::transformPoint(m, {r.left, r.bottom}), drawing::transformPoint(m, {r.right, r.bottom}) };
    drawing::Rect out{ corners[0].x, corners[0].y, corners[0].x, corners[0].y };
    for (int i = 1; i < 4; ++i)
    {
        out.left = (std::min)(out.left, corners[i].x);
        out.top = (std::min)(out.top, corners[i].y);
        out.right = (std::max)(out.right, corners[i].x);
        out.bottom = (std::max)(out.bottom, corners[i].y);
    }
    return out;
}

// Integer pixel bound of an axis-aligned clip edge under Direct2D's ALIASED
// clip rule: a pixel belongs to the clip iff its centre is inside (top-left
// rule, so a centre exactly on the left/top edge counts as inside). The same
// formula serves as inclusive begin and exclusive end.
inline int aliasedCoord(float edge)
{
    return int(std::ceil(edge - 0.5f));
}

// sRGB transfer function, decode direction (IEC 61966-2-1). Negatives are
// clamped away first: std::pow of a negative base with a fractional exponent
// is NaN, and one NaN texel poisons a whole bilinear neighbourhood.
inline float srgbToLinear(float s)
{
    s = (std::max)(0.0f, s);
    return (s <= 0.04045f) ? s / 12.92f : std::pow((s + 0.055f) / 1.055f, 2.4f);
}

// ---------------------------------------------------------------------------
// Surface: owned fp16 RGBA pixel storage.
// Stride in pixels is a multiple of 8 so rows are 64-byte multiples, and the
// first pixel is 64-byte aligned: blend loops may process whole chunks that
// extend into the row padding (coverage there is zero -> identity blend).
// ---------------------------------------------------------------------------
struct Surface
{
    std::vector<uint16_t> storage;
    uint16_t* pixels{};
    int32_t width{}, height{}, stridePixels{};

    void init(int32_t w, int32_t h)
    {
        width = (std::max)(1, w);
        height = (std::max)(1, h);
        stridePixels = (width + 7) & ~7;
        storage.resize(size_t(stridePixels) * height * 4 + 32); // +32 uint16 = 64B alignment slack
        const auto addr = reinterpret_cast<uintptr_t>(storage.data());
        pixels = reinterpret_cast<uint16_t*>((addr + 63) & ~uintptr_t(63));
        std::memset(pixels, 0, size_t(stridePixels) * height * 8);
    }

    uint16_t* row(int32_t y) { return pixels + size_t(y) * stridePixels * 4; }
};

// A render target always rasterizes into the fp16 surface above. Two creation
// flags change only how lockPixels() PRESENTS those pixels to the CPU:
//
//   Mask       -> Alpha_8i:     one byte of LINEAR coverage per pixel
//   SRGBPixels -> BGRA_sRGB_8i: four bytes, premultiplied, sRGB-encoded — the
//                               name is the truth, and it means the same thing
//                               for a file-loaded bitmap as for a render target
//                               (see the contract note above loadImageU()).
//
// 8-bit colour is sRGB so the codes are spent perceptually; 8-bit coverage is
// linear because coverage has no transfer function. Those are the only two
// 8-bit forms, which is the whole point of keeping the format list this short.
//
// Nothing else in the renderer changes: one surface type, one set of blend
// loops, and the conversion confined to lock/unlock.
inline int32_t lockFormatFromFlags(int32_t flags)
{
    if (flags & int32_t(drawing::BitmapRenderTargetFlags::Mask))
        return drawing::api::IBitmapPixels::Alpha_8i;
    if (flags & int32_t(drawing::BitmapRenderTargetFlags::SRGBPixels))
        return drawing::api::IBitmapPixels::BGRA_sRGB_8i;
    return drawing::api::IBitmapPixels::RGBA_16f;
}

// ---------------------------------------------------------------------------
// Bitmap / BitmapPixels
// ---------------------------------------------------------------------------
class Bitmap final : public drawing::api::IBitmap
{
public:
    Surface surface;
    drawing::api::IFactory* factory{};

    // What lockPixels() hands out. Rendering is fp16 regardless.
    int32_t lockFormat{ drawing::api::IBitmapPixels::RGBA_16f };

    Bitmap(drawing::api::IFactory* pfactory, int32_t w, int32_t h, int32_t flags = 0)
        : factory(pfactory), lockFormat(lockFormatFromFlags(flags))
    {
        surface.init(w, h);
    }

    ReturnCode getSizeU(drawing::SizeU* returnSize) override
    {
        *returnSize = { uint32_t(surface.width), uint32_t(surface.height) };
        return ReturnCode::Ok;
    }

    ReturnCode lockPixels(drawing::api::IBitmapPixels** returnPixels, int32_t flags) override;

    ReturnCode getFactory(drawing::api::IFactory** returnFactory) override
    {
        *returnFactory = factory;
        return ReturnCode::Ok;
    }

    ReturnCode queryInterface(const gmpi::api::Guid* iid, void** returnInterface) override
    {
        *returnInterface = {};
        GMPI_QUERYINTERFACE(drawing::api::IBitmap);
        GMPI_QUERYINTERFACE(drawing::api::IResource);
        return ReturnCode::NoSupport;
    }
    GMPI_REFCOUNT;
};

class BitmapPixels final : public drawing::api::IBitmapPixels
{
    Bitmap* bitmap; // keeps the surface alive while locked
    int32_t lockFlags{};

    // Only used by the 8-bit lock formats. Tightly packed (no stride padding):
    // callers index it as width*height bytes, and the fp16 surface's padding is
    // an implementation detail they should not see.
    std::vector<uint8_t> staging;

    int32_t bytesPerPixel() const { return bitmap->lockFormat & 0xFF; }
    bool    converts()      const { return bitmap->lockFormat != drawing::api::IBitmapPixels::RGBA_16f; }

    // fp16 premultiplied linear RGBA -> the 8-bit lock format.
    void surfaceToStaging()
    {
        const auto& s = bitmap->surface;
        const int32_t bpp = bytesPerPixel();
        const bool mask = bitmap->lockFormat == drawing::api::IBitmapPixels::Alpha_8i;

        for (int32_t y = 0; y < s.height; ++y)
        {
            const uint16_t* src = bitmap->surface.pixels + size_t(y) * s.stridePixels * 4;
            uint8_t* dst = staging.data() + size_t(y) * s.width * bpp;

            for (int32_t x = 0; x < s.width; ++x, src += 4, dst += bpp)
            {
                const auto toByte = [](float v) {
                    return uint8_t(std::clamp(v * 255.0f + 0.5f, 0.0f, 255.0f));
                };

                // An 8-bit COLOUR face is sRGB, always — the file's curve for a
                // loaded image, and Direct2D's _UNORM_SRGB encode for a render
                // target. linearPixelToSRGB is the exact inverse of the LUT
                // loadImageU uses and the one DirectXGfx's premultiply
                // correction rounds through, so the bytes match Direct2D code
                // for code. A Mask is coverage, not colour: it stays linear.
                const auto toColourByte = [&](float v) {
                    return drawing::linearPixelToSRGB(v);
                };

                if (mask)
                {
                    dst[0] = toByte(drawing::detail::halfToFloat(src[3]));
                }
                else
                {
                    dst[0] = toColourByte(drawing::detail::halfToFloat(src[2])); // B
                    dst[1] = toColourByte(drawing::detail::halfToFloat(src[1])); // G
                    dst[2] = toColourByte(drawing::detail::halfToFloat(src[0])); // R
                    dst[3] = toByte(drawing::detail::halfToFloat(src[3]));       // A
                }
            }
        }
    }

    // The reverse, for locks that included Write.
    void stagingToSurface()
    {
        const auto& s = bitmap->surface;
        const int32_t bpp = bytesPerPixel();
        const bool mask = bitmap->lockFormat == drawing::api::IBitmapPixels::Alpha_8i;

        // Undo what surfaceToStaging applied, so a read-modify-write lock that
        // changes nothing leaves the surface untouched.
        const auto fromColourByte = [&](uint8_t v) {
            return drawing::SRGBPixelToLinear(v);
        };

        for (int32_t y = 0; y < s.height; ++y)
        {
            const uint8_t* src = staging.data() + size_t(y) * s.width * bpp;
            uint16_t* dst = bitmap->surface.pixels + size_t(y) * s.stridePixels * 4;

            for (int32_t x = 0; x < s.width; ++x, src += bpp, dst += 4)
            {
                if (mask)
                {
                    // A coverage mask has no colour of its own, so it comes back
                    // as premultiplied white: coverage c reads as (c,c,c,c),
                    // which is what "c of a white shape covers this pixel" means
                    // and round-trips the white brush the mask was drawn with.
                    const uint16_t c = drawing::detail::floatToHalf(src[0] / 255.0f);
                    dst[0] = dst[1] = dst[2] = dst[3] = c;
                }
                else
                {
                    dst[0] = drawing::detail::floatToHalf(fromColourByte(src[2])); // R
                    dst[1] = drawing::detail::floatToHalf(fromColourByte(src[1])); // G
                    dst[2] = drawing::detail::floatToHalf(fromColourByte(src[0])); // B
                    dst[3] = drawing::detail::floatToHalf(src[3] / 255.0f);        // A
                }
            }
        }
    }

public:
    BitmapPixels(Bitmap* pbitmap, int32_t flags) : bitmap(pbitmap), lockFlags(flags)
    {
        bitmap->addRef();

        if (converts())
        {
            const auto& s = bitmap->surface;
            staging.assign(size_t(s.width) * s.height * bytesPerPixel(), 0);

            // Write-only locks overwrite everything, so skip the read pass. A
            // lock with no flags at all is treated as a read: that is what the
            // platform backends do, and reading garbage is the worse failure.
            if (0 == (lockFlags & int32_t(drawing::BitmapLockFlags::Write)) ||
                0 != (lockFlags & int32_t(drawing::BitmapLockFlags::Read)))
            {
                surfaceToStaging();
            }
        }
    }
    ~BitmapPixels()
    {
        if (converts() && 0 != (lockFlags & int32_t(drawing::BitmapLockFlags::Write)))
            stagingToSurface();

        bitmap->release();
    }

    ReturnCode getAddress(uint8_t** returnAddress) override
    {
        *returnAddress = converts() ? staging.data()
                                    : reinterpret_cast<uint8_t*>(bitmap->surface.pixels);
        return ReturnCode::Ok;
    }
    ReturnCode getBytesPerRow(int32_t* returnBytesPerRow) override
    {
        *returnBytesPerRow = converts() ? bitmap->surface.width * bytesPerPixel()
                                        : bitmap->surface.stridePixels * 8;
        return ReturnCode::Ok;
    }
    ReturnCode getPixelFormat(int32_t* returnPixelFormat) override
    {
        *returnPixelFormat = bitmap->lockFormat;
        return ReturnCode::Ok;
    }

    GMPI_QUERYINTERFACE_METHOD(drawing::api::IBitmapPixels);
    GMPI_REFCOUNT;
};

inline ReturnCode Bitmap::lockPixels(drawing::api::IBitmapPixels** returnPixels, int32_t flags)
{
    *returnPixels = new BitmapPixels(this, flags);
    return ReturnCode::Ok;
}

// ---------------------------------------------------------------------------
// Text seam. Text needs shaping, font discovery and colour-glyph handling,
// none of which belong in a rasterizer — so this header knows nothing about
// HarfBuzz or any platform font API. A text engine turns a string into
// geometry through the PUBLIC drawing API, and the render target then fills
// that geometry by the ordinary path. Text therefore adds no pixel-touching
// code, exactly like every other primitive here.
//
// helpers/CpuTextEngine.h implements this over HarfBuzz.
// ---------------------------------------------------------------------------
class ICpuTextEngine
{
public:
    virtual ~ICpuTextEngine() = default;

    virtual ReturnCode createTextFormat(const char* fontFamilyName, drawing::FontWeight, drawing::FontStyle,
                                        drawing::FontStretch, float fontHeight, int32_t fontFlags,
                                        drawing::api::ITextFormat** returnTextFormat) = 0;

    // Lay out `utf8` inside `layoutRect` and draw it into `context` using the
    // ordinary public draw calls: glyph outlines are filled as path geometry,
    // and colour emoji are drawn as bitmaps. Colour glyphs ignore the brush and
    // carry their own colour, exactly as on the platform backends.
    virtual ReturnCode drawText(drawing::api::IDeviceContext* context, drawing::api::ITextFormat* textFormat,
                                const char* utf8, int32_t length, const drawing::Rect& layoutRect,
                                drawing::api::IBrush* brush, int32_t options) = 0;

    // Markdown. Optional: an engine that declines these still satisfies the
    // interface, and the backend reports NoSupport upward, which is how a
    // caller is expected to discover that rich text is unavailable.
    virtual ReturnCode createRichTextFormat(const char* /*markdownText*/, float /*fontHeight*/,
                                            const char* /*fontFamilyName*/, int32_t /*fontFlags*/,
                                            drawing::TextAlignment, drawing::ParagraphAlignment,
                                            drawing::WordWrapping, float /*lineSpacing*/, float /*baseline*/,
                                            drawing::api::IRichTextFormat** returnRichTextFormat)
    {
        *returnRichTextFormat = {};
        return ReturnCode::NoSupport;
    }

    virtual ReturnCode drawRichText(drawing::api::IDeviceContext* /*context*/,
                                    drawing::api::IRichTextFormat* /*richTextFormat*/,
                                    const drawing::Rect& /*layoutRect*/, drawing::api::IBrush* /*brush*/,
                                    int32_t /*options*/)
    {
        return ReturnCode::NoSupport;
    }
};

// ---------------------------------------------------------------------------
// Internal brush interface: fills a span of premultiplied linear RGBA for the
// blender. Everything the renderer paints with implements this, so the blend
// loop never branches on brush type.
// ---------------------------------------------------------------------------
class CpuBrush
{
public:
    virtual ~CpuBrush() = default;

    // Premultiplied linear RGBA for device pixels (x0 .. x0+count-1) on row y.
    // deviceToLocal maps device space to the space the geometry was given in
    // (i.e. the inverse of the context transform at draw time).
    virtual void evalSpan(const drawing::Matrix3x2& deviceToLocal, int x0, int y, int count, float* dst) const = 0;

    // False if this brush would paint non-finite values, which would poison
    // even zero-coverage pixels through the blend (NaN * 0 = NaN).
    virtual bool isPaintable() const = 0;
};

// ---------------------------------------------------------------------------
// Solid color brush. GMPI Color is linear, non-premultiplied.
// ---------------------------------------------------------------------------
class SolidColorBrush final : public drawing::api::ISolidColorBrush, public CpuBrush
{
public:
    drawing::api::IFactory* factory{};
    drawing::Color color;
    float opacity{ 1.0f };

    SolidColorBrush(drawing::api::IFactory* pfactory, const drawing::Color* pcolor, const drawing::BrushProperties* properties)
        : factory(pfactory), color(*pcolor)
    {
        if (properties)
            opacity = properties->opacity;
    }

    bool isPaintable() const override
    {
        return std::isfinite(color.r) && std::isfinite(color.g) && std::isfinite(color.b)
            && std::isfinite(color.a) && std::isfinite(opacity);
    }

    void evalSpan(const drawing::Matrix3x2&, int, int, int count, float* dst) const override
    {
        const float a = color.a * opacity;
        const float r = color.r * a, g = color.g * a, b = color.b * a;
        for (int i = 0; i < count; ++i, dst += 4)
        {
            dst[0] = r; dst[1] = g; dst[2] = b; dst[3] = a;
        }
    }

    void setColor(const drawing::Color* pcolor) override
    {
        color = *pcolor;
    }

    ReturnCode getFactory(drawing::api::IFactory** returnFactory) override
    {
        *returnFactory = factory;
        return ReturnCode::Ok;
    }

    ReturnCode queryInterface(const gmpi::api::Guid* iid, void** returnInterface) override
    {
        *returnInterface = {};
        GMPI_QUERYINTERFACE(drawing::api::ISolidColorBrush);
        GMPI_QUERYINTERFACE(drawing::api::IResource);
        return ReturnCode::NoSupport;
    }
    GMPI_REFCOUNT;
};

// ---------------------------------------------------------------------------
// Bitmap sampling. One sampler serves both drawBitmap and bitmap brushes; they
// differ only in how local space maps to source pixels, and in the edge rule.
// Sampling is done on premultiplied data, which is what makes bilinear
// filtering correct across varying alpha.
// ---------------------------------------------------------------------------
class BitmapSource final : public CpuBrush
{
    Bitmap* bitmap{};                 // owned reference
    drawing::Matrix3x2 localToSource;  // local space -> source pixel space
    float opacity{ 1.0f };
    bool wrap{ false };                // true = tile, false = clamp to sourceRect
    bool bilinear{ false };
    float clampLeft{}, clampTop{}, clampRight{}, clampBottom{}; // in source pixels

public:
    BitmapSource(Bitmap* pbitmap, const drawing::Matrix3x2& plocalToSource, float popacity,
                 bool pwrap, bool pbilinear, drawing::Rect sourceBounds)
        : bitmap(pbitmap), localToSource(plocalToSource), opacity(popacity)
        , wrap(pwrap), bilinear(pbilinear)
        // Normalise the clamp bounds. An inverted source rectangle is legal
        // input (and mirrors the image, which the mapping matrix handles on its
        // own), but feeding lo > hi to std::clamp is undefined behaviour — and
        // fatal in an MSVC debug build.
        , clampLeft((std::min)(sourceBounds.left, sourceBounds.right))
        , clampTop((std::min)(sourceBounds.top, sourceBounds.bottom))
        , clampRight((std::max)(sourceBounds.left, sourceBounds.right))
        , clampBottom((std::max)(sourceBounds.top, sourceBounds.bottom))
    {
        bitmap->addRef();
    }

    ~BitmapSource() override
    {
        bitmap->release();
    }

    // Owns a reference, so copying would double-release.
    BitmapSource(const BitmapSource&) = delete;
    BitmapSource& operator=(const BitmapSource&) = delete;

    bool isPaintable() const override
    {
        return bitmap && bitmap->surface.width > 0 && bitmap->surface.height > 0
            && std::isfinite(opacity) && drawing::isFinite(localToSource);
    }

private:
    // One texel, premultiplied linear, with the edge rule applied.
    void texel(int x, int y, float* out) const
    {
        const auto& s = bitmap->surface;
        if (wrap)
        {
            x %= s.width;  if (x < 0) x += s.width;
            y %= s.height; if (y < 0) y += s.height;
        }
        else
        {
            x = std::clamp(x, 0, s.width - 1);
            y = std::clamp(y, 0, s.height - 1);
        }
        const uint16_t* p = bitmap->surface.pixels + (size_t(y) * s.stridePixels + size_t(x)) * 4;
        for (int c = 0; c < 4; ++c)
            out[c] = drawing::detail::halfToFloat(p[c]);

        // No conversion here, whatever the lock format. The surface is linear and
        // sampling wants linear. Direct2D reaches the same place by a different
        // route: it binds any 32bppPBGRA bitmap as B8G8R8A8_UNORM_SRGB, so it
        // de-gammas on sample — but the bytes it de-gammas are genuinely sRGB
        // (the file's curve for a loaded image, the _UNORM_SRGB encode for a
        // render target), so it lands on the same linear value we already hold.
        //
        // This used to apply srgbToLinear for BGRA_sRGB_8i, to reproduce a real
        // Direct2D gamma error: an SRGBPixels render target stored LINEAR bytes
        // and D2D de-gammaed them anyway, so a cached bitmap came back darker
        // than the direct draw. That was fixed at the source — the render target
        // is created _UNORM_SRGB now — so there is no longer an error to copy.
    }

    void sample(float sx, float sy, float* out) const
    {
        if (!std::isfinite(sx) || !std::isfinite(sy))
        {
            out[0] = out[1] = out[2] = out[3] = 0.0f;
            return;
        }

        if (wrap)
        {
            // Reduce into the bitmap in FLOAT before any int conversion: a
            // brush transform with a tiny scale sends source coordinates far
            // outside int range, and float-to-int is undefined there.
            const float w = float(bitmap->surface.width);
            const float h = float(bitmap->surface.height);
            sx -= std::floor(sx / w) * w;
            sy -= std::floor(sy / h) * h;
            sx = std::clamp(sx, 0.0f, w); // guard the float division's rounding
            sy = std::clamp(sy, 0.0f, h);
        }
        else
        {
            // Stay inside the requested source rectangle, so a cropped draw
            // never bleeds in neighbouring texels.
            sx = std::clamp(sx, clampLeft, clampRight);
            sy = std::clamp(sy, clampTop, clampBottom);
        }

        if (!bilinear)
        {
            texel(int(std::floor(sx)), int(std::floor(sy)), out);
            return;
        }

        // Texel centres sit at (i + 0.5), so shift before splitting.
        const float fx = sx - 0.5f;
        const float fy = sy - 0.5f;
        const float x0f = std::floor(fx);
        const float y0f = std::floor(fy);
        const int x0 = int(x0f);
        const int y0 = int(y0f);
        const float tx = fx - x0f;
        const float ty = fy - y0f;

        float c00[4], c10[4], c01[4], c11[4];
        texel(x0, y0, c00);
        texel(x0 + 1, y0, c10);
        texel(x0, y0 + 1, c01);
        texel(x0 + 1, y0 + 1, c11);

        for (int c = 0; c < 4; ++c)
        {
            const float top = c00[c] + (c10[c] - c00[c]) * tx;
            const float bottom = c01[c] + (c11[c] - c01[c]) * tx;
            out[c] = top + (bottom - top) * ty;
        }
    }

    // A non-finite texel (only reachable if a caller wrote one through
    // lockPixels) would otherwise reach the blend and poison even zero-coverage
    // pixels, permanently — NaN * 0 is still NaN. Keep the sampler total.
    static void sanitize(float* c)
    {
        for (int i = 0; i < 4; ++i)
            if (!std::isfinite(c[i]))
                c[i] = 0.0f;
    }

public:
    void evalSpan(const drawing::Matrix3x2& deviceToLocal, int x0, int y, int count, float* dst) const override
    {
        const auto m = deviceToLocal * localToSource;

        // The mapping is affine, so step through source space incrementally.
        const auto p0 = drawing::transformPoint(m, { float(x0) + 0.5f, float(y) + 0.5f });
        const auto p1 = drawing::transformPoint(m, { float(x0) + 1.5f, float(y) + 0.5f });
        const float stepX = p1.x - p0.x;
        const float stepY = p1.y - p0.y;

        float sx = p0.x, sy = p0.y;
        for (int i = 0; i < count; ++i, dst += 4, sx += stepX, sy += stepY)
        {
            sample(sx, sy, dst);
            sanitize(dst);
            if (opacity != 1.0f)
                for (int c = 0; c < 4; ++c)
                    dst[c] *= opacity;
        }
    }
};

// A bitmap brush: tiles the bitmap over local space (WRAP + nearest, matching
// what the Direct2D backend hard-codes for macOS parity).
class BitmapBrush final : public drawing::api::IBitmapBrush, public CpuBrush
{
    drawing::api::IFactory* factory{};
    BitmapSource source; // immutable: IBitmapBrush has no setters

    static drawing::Matrix3x2 brushToSource(const drawing::BrushProperties* properties)
    {
        return properties ? drawing::invert(properties->transform) : drawing::Matrix3x2{};
    }

public:
    BitmapBrush(drawing::api::IFactory* pfactory, Bitmap* pbitmap, const drawing::BrushProperties* properties)
        : factory(pfactory)
        , source(pbitmap, brushToSource(properties), properties ? properties->opacity : 1.0f,
                 /*wrap*/ true, /*bilinear*/ false,
                 drawing::Rect{ 0.0f, 0.0f, float(pbitmap->surface.width), float(pbitmap->surface.height) })
    {
    }

    bool isPaintable() const override { return source.isPaintable(); }

    void evalSpan(const drawing::Matrix3x2& deviceToLocal, int x0, int y, int count, float* dst) const override
    {
        source.evalSpan(deviceToLocal, x0, y, count, dst);
    }

    ReturnCode getFactory(drawing::api::IFactory** returnFactory) override
    {
        *returnFactory = factory;
        return ReturnCode::Ok;
    }

    ReturnCode queryInterface(const gmpi::api::Guid* iid, void** returnInterface) override
    {
        *returnInterface = {};
        GMPI_QUERYINTERFACE(drawing::api::IBitmapBrush);
        GMPI_QUERYINTERFACE(drawing::api::IResource);
        return ReturnCode::NoSupport;
    }
    GMPI_REFCOUNT;
};

// ---------------------------------------------------------------------------
// Gradients
// ---------------------------------------------------------------------------
class GradientstopCollection final : public drawing::api::IGradientstopCollection
{
public:
    drawing::api::IFactory* factory{};
    std::vector<drawing::Gradientstop> stops; // sorted by position
    drawing::ExtendMode extendMode{ drawing::ExtendMode::Clamp };

    GradientstopCollection(drawing::api::IFactory* pfactory, const drawing::Gradientstop* pstops, uint32_t count, drawing::ExtendMode pextendMode)
        : factory(pfactory), stops(pstops, pstops + count), extendMode(pextendMode)
    {
        std::stable_sort(stops.begin(), stops.end(),
            [](const drawing::Gradientstop& a, const drawing::Gradientstop& b) { return a.position < b.position; });
    }

    bool isPaintable() const
    {
        if (stops.empty())
            return false;
        for (const auto& s : stops)
            if (!std::isfinite(s.position) || !std::isfinite(s.color.r) || !std::isfinite(s.color.g)
                || !std::isfinite(s.color.b) || !std::isfinite(s.color.a))
                return false;
        return true;
    }

    // Straight (non-premultiplied) linear colour at gradient parameter t.
    drawing::Color colorAt(float t) const
    {
        // A non-finite t makes every comparison below false, which would walk
        // the stop search off the end (out of bounds for a single-stop
        // collection). It is reachable: a near-singular transform can push a
        // device point to infinity, and Wrap then turns inf into NaN via
        // inf - floor(inf). Pin it to the start of the gradient instead.
        if (!std::isfinite(t))
            t = 0.0f;

        // Apply the extend mode first: it wraps the parameter, not the colours.
        switch (extendMode)
        {
        case drawing::ExtendMode::Wrap:
            t -= std::floor(t);
            break;
        case drawing::ExtendMode::Mirror:
        {
            const float f = std::fabs(t - 2.0f * std::floor(t * 0.5f)); // period 2
            t = f > 1.0f ? 2.0f - f : f;
            break;
        }
        default:
            t = std::clamp(t, 0.0f, 1.0f);
            break;
        }

        if (t <= stops.front().position)
            return stops.front().color;
        if (t >= stops.back().position)
            return stops.back().color;

        size_t i = 1;
        while (i < stops.size() && stops[i].position < t)
            ++i;
        if (i >= stops.size())
            return stops.back().color; // total function, whatever t is
        const auto& s0 = stops[i - 1];
        const auto& s1 = stops[i];
        const float span = s1.position - s0.position;
        const float k = span > 0.0f ? (t - s0.position) / span : 0.0f;
        return { s0.color.r + (s1.color.r - s0.color.r) * k,
                 s0.color.g + (s1.color.g - s0.color.g) * k,
                 s0.color.b + (s1.color.b - s0.color.b) * k,
                 s0.color.a + (s1.color.a - s0.color.a) * k };
    }

    ReturnCode getFactory(drawing::api::IFactory** returnFactory) override
    {
        *returnFactory = factory;
        return ReturnCode::Ok;
    }

    ReturnCode queryInterface(const gmpi::api::Guid* iid, void** returnInterface) override
    {
        *returnInterface = {};
        GMPI_QUERYINTERFACE(drawing::api::IGradientstopCollection);
        GMPI_QUERYINTERFACE(drawing::api::IResource);
        return ReturnCode::NoSupport;
    }
    GMPI_REFCOUNT;
};

// Shared plumbing for the two gradient brushes: stop lookup, brush transform,
// opacity, and turning a gradient parameter into a premultiplied span entry.
class GradientBrushBase : public CpuBrush
{
protected:
    drawing::api::IFactory* factory{};
    gmpi::shared_ptr<GradientstopCollection> stops;
    drawing::Matrix3x2 brushTransform;
    float opacity{ 1.0f };

    GradientBrushBase(drawing::api::IFactory* pfactory, const drawing::BrushProperties* properties, drawing::api::IGradientstopCollection* collection)
        : factory(pfactory)
    {
        if (auto* c = dynamic_cast<GradientstopCollection*>(collection))
        {
            c->addRef();
            stops.attach(c);
        }
        if (properties)
        {
            opacity = properties->opacity;
            brushTransform = properties->transform;
        }
    }

    // Device space -> the brush's own coordinate space.
    drawing::Matrix3x2 deviceToBrush(const drawing::Matrix3x2& deviceToLocal) const
    {
        return deviceToLocal * drawing::invert(brushTransform);
    }

    void writeStop(float t, float* dst) const
    {
        const auto c = stops->colorAt(t);
        const float a = c.a * opacity;
        dst[0] = c.r * a; dst[1] = c.g * a; dst[2] = c.b * a; dst[3] = a;
    }

public:
    bool isPaintable() const override
    {
        return stops.get() != nullptr && stops->isPaintable() && std::isfinite(opacity)
            && std::isfinite(brushTransform._11) && std::isfinite(brushTransform._12)
            && std::isfinite(brushTransform._21) && std::isfinite(brushTransform._22)
            && std::isfinite(brushTransform._31) && std::isfinite(brushTransform._32);
    }
};

class LinearGradientBrush final : public drawing::api::ILinearGradientBrush, public GradientBrushBase
{
    drawing::Point startPoint, endPoint;

public:
    LinearGradientBrush(drawing::api::IFactory* pfactory, const drawing::LinearGradientBrushProperties* props,
                        const drawing::BrushProperties* brushProperties, drawing::api::IGradientstopCollection* collection)
        : GradientBrushBase(pfactory, brushProperties, collection)
        , startPoint(props->startPoint), endPoint(props->endPoint)
    {
    }

    void setStartPoint(drawing::Point p) override { startPoint = p; }
    void setEndPoint(drawing::Point p) override { endPoint = p; }

    void evalSpan(const drawing::Matrix3x2& deviceToLocal, int x0, int y, int count, float* dst) const override
    {
        const auto m = deviceToBrush(deviceToLocal);
        const drawing::Point axis{ endPoint.x - startPoint.x, endPoint.y - startPoint.y };
        const float lenSq = axis.x * axis.x + axis.y * axis.y;

        if (!(lenSq > 0.0f))
        {
            // Degenerate axis: D2D paints the last stop's colour.
            for (int i = 0; i < count; ++i)
                writeStop(1.0f, dst + size_t(i) * 4);
            return;
        }

        // t is affine in device x, so walk it incrementally along the span
        // (this is the "per-row ramp" the plan calls for).
        const auto p0 = drawing::transformPoint(m, { float(x0) + 0.5f, float(y) + 0.5f });
        const auto p1 = drawing::transformPoint(m, { float(x0) + 1.5f, float(y) + 0.5f });
        float t = ((p0.x - startPoint.x) * axis.x + (p0.y - startPoint.y) * axis.y) / lenSq;
        const float dt = ((p1.x - p0.x) * axis.x + (p1.y - p0.y) * axis.y) / lenSq;

        for (int i = 0; i < count; ++i, dst += 4, t += dt)
            writeStop(t, dst);
    }

    ReturnCode getFactory(drawing::api::IFactory** returnFactory) override
    {
        *returnFactory = factory;
        return ReturnCode::Ok;
    }

    ReturnCode queryInterface(const gmpi::api::Guid* iid, void** returnInterface) override
    {
        *returnInterface = {};
        GMPI_QUERYINTERFACE(drawing::api::ILinearGradientBrush);
        GMPI_QUERYINTERFACE(drawing::api::IResource);
        return ReturnCode::NoSupport;
    }
    GMPI_REFCOUNT;
};

class RadialGradientBrush final : public drawing::api::IRadialGradientBrush, public GradientBrushBase
{
    drawing::Point center, originOffset;
    float radiusX{}, radiusY{};

public:
    RadialGradientBrush(drawing::api::IFactory* pfactory, const drawing::RadialGradientBrushProperties* props,
                        const drawing::BrushProperties* brushProperties, drawing::api::IGradientstopCollection* collection)
        : GradientBrushBase(pfactory, brushProperties, collection)
        , center(props->center), originOffset(props->gradientOriginOffset)
        , radiusX(props->radiusX), radiusY(props->radiusY)
    {
    }

    void setCenter(drawing::Point p) override { center = p; }
    void setGradientOriginOffset(drawing::Point p) override { originOffset = p; }
    void setRadiusX(float r) override { radiusX = r; }
    void setRadiusY(float r) override { radiusY = r; }

    void evalSpan(const drawing::Matrix3x2& deviceToLocal, int x0, int y, int count, float* dst) const override
    {
        const auto m = deviceToBrush(deviceToLocal);

        if (!(radiusX != 0.0f && radiusY != 0.0f) || !std::isfinite(radiusX) || !std::isfinite(radiusY))
        {
            for (int i = 0; i < count; ++i)
                writeStop(1.0f, dst + size_t(i) * 4);
            return;
        }

        // Work in a space where the gradient ellipse is the unit circle. The
        // focus is the gradient origin; t = 1 / k where k scales the ray from
        // the focus through the point out to the circle (the standard focal
        // radial gradient, as in SVG and Canvas).
        //
        // The solver needs |f| < 1. Clamp by LENGTH, not per-axis: a diagonal
        // offset such as (0.9, 0.9) survives a per-component clamp with
        // |f| = 1.27, which drives the discriminant negative and paints a wedge
        // of the plane a flat last-stop colour.
        float fx = originOffset.x / radiusX;
        float fy = originOffset.y / radiusY;
        const float fLen = std::sqrt(fx * fx + fy * fy);
        if (!(fLen < 0.999f))
        {
            const float k = std::isfinite(fLen) && fLen > 0.0f ? 0.999f / fLen : 0.0f;
            fx *= k;
            fy *= k;
        }
        const float fLenSq = fx * fx + fy * fy;

        const auto p0 = drawing::transformPoint(m, { float(x0) + 0.5f, float(y) + 0.5f });
        const auto p1 = drawing::transformPoint(m, { float(x0) + 1.5f, float(y) + 0.5f });
        const float stepX = (p1.x - p0.x) / radiusX;
        const float stepY = (p1.y - p0.y) / radiusY;
        float qx = (p0.x - center.x) / radiusX;
        float qy = (p0.y - center.y) / radiusY;

        for (int i = 0; i < count; ++i, dst += 4, qx += stepX, qy += stepY)
        {
            const float dx = qx - fx;
            const float dy = qy - fy;
            const float a = dx * dx + dy * dy;
            float t;
            if (a <= 0.0f)
            {
                t = 0.0f; // at the focus
            }
            else
            {
                const float b = fx * dx + fy * dy;              // half of the linear term
                const float disc = b * b + a * (1.0f - fLenSq); // always >= 0 for |f| < 1
                const float k = (-b + std::sqrt((std::max)(disc, 0.0f))) / a;
                t = (k > 0.0f) ? 1.0f / k : 1.0f;
            }
            writeStop(t, dst);
        }
    }

    ReturnCode getFactory(drawing::api::IFactory** returnFactory) override
    {
        *returnFactory = factory;
        return ReturnCode::Ok;
    }

    ReturnCode queryInterface(const gmpi::api::Guid* iid, void** returnInterface) override
    {
        *returnInterface = {};
        GMPI_QUERYINTERFACE(drawing::api::IRadialGradientBrush);
        GMPI_QUERYINTERFACE(drawing::api::IResource);
        return ReturnCode::NoSupport;
    }
    GMPI_REFCOUNT;
};

// ---------------------------------------------------------------------------
// PathGeometry: flattened polygons in local space.
// Curves never reach the rasterizer: the Gfx_base sink reduces arcs to cubics
// and cubics to line segments (AGG curve4_div); this sink only records points.
// ---------------------------------------------------------------------------
struct Figure
{
    std::vector<drawing::Point> points;
    bool filled{ true }; // FigureBegin::Filled; hollow figures are never filled (D2D semantics)
    bool closed{ false }; // FigureEnd::Closed; only strokes care (joins vs caps)
};

// ---------------------------------------------------------------------------
// Stroking: a stroke is geometry, not a raster operation. Each figure is
// widened into a set of convex pieces (segment quads, join wedges, caps) which
// are then nonzero-filled as a union. Every piece is emitted with the same
// winding direction, so overlaps reinforce instead of cancelling, and because
// the whole widened outline lands in one coverage buffer before a single
// blend, a translucent self-overlapping stroke does not double-darken.
//
// Widening happens in LOCAL space, before the transform, so a non-uniform
// scale produces an elliptical pen exactly like Direct2D.
// ---------------------------------------------------------------------------
namespace stroke
{

constexpr float kTwoPi = 6.28318530717959f;

inline drawing::Point sub(drawing::Point a, drawing::Point b) { return { a.x - b.x, a.y - b.y }; }
inline float length(drawing::Point d) { return std::sqrt(d.x * d.x + d.y * d.y); }

// Append a polygon, normalising its winding so that nonzero-filling the whole
// collection yields the union of the pieces (opposite windings would cancel).
inline void emitPolygon(std::vector<Figure>& out, std::vector<drawing::Point> pts)
{
    if (pts.size() < 3)
        return;

    float area2 = 0.0f;
    for (size_t i = 0; i < pts.size(); ++i)
    {
        const auto& a = pts[i];
        const auto& b = pts[(i + 1) % pts.size()];
        area2 += a.x * b.y - b.x * a.y;
    }
    if (area2 == 0.0f)
        return; // degenerate sliver contributes nothing
    if (area2 > 0.0f)
        std::reverse(pts.begin(), pts.end());

    out.emplace_back();
    out.back().points = std::move(pts);
    out.back().filled = true;
    out.back().closed = true;
}

// Segment count for a circle of radius r. The tolerance is tighter than the
// curve flattener's 0.25px because round caps/joins are compared directly
// against Direct2D's true circles, where 0.25px of flattening is visible.
inline int arcSegments(float r)
{
    constexpr float tol = 0.02f;
    if (r <= tol)
        return 4;
    const float step = 2.0f * std::acos(std::clamp(1.0f - tol / r, -1.0f, 1.0f));
    if (!(step > 0.0f))
        return 256;
    return std::clamp(int(std::ceil(kTwoPi / step)), 8, 256);
}

inline void emitDisc(std::vector<Figure>& out, drawing::Point c, float r)
{
    const int n = arcSegments(r);
    // Vertices sit ON the circle. Outsetting to centre the flattening error
    // (r / cos(pi/n)) is tempting but makes the disc bulge past the stroke
    // band at the tangent points, which reads worse than being a hair small.
    std::vector<drawing::Point> pts;
    pts.reserve(size_t(n));
    for (int i = 0; i < n; ++i)
    {
        const float a = kTwoPi * float(i) / float(n);
        pts.push_back({ c.x + r * std::cos(a), c.y + r * std::sin(a) });
    }
    emitPolygon(out, std::move(pts));
}

inline float signedArea2(const std::vector<drawing::Point>& pts)
{
    float area2 = 0.0f;
    for (size_t i = 0; i < pts.size(); ++i)
    {
        const auto& a = pts[i];
        const auto& b = pts[(i + 1) % pts.size()];
        area2 += a.x * b.y - b.x * a.y;
    }
    return area2;
}

// Append the polygon exactly as given (no winding normalisation). Used for
// contours whose relative orientation carries meaning — a closed figure's
// stroke is an annulus, and its inner ring must wind opposite the outer one to
// punch the hole under nonzero fill.
inline void emitContour(std::vector<Figure>& out, std::vector<drawing::Point> pts)
{
    if (pts.size() < 3)
        return;
    out.emplace_back();
    out.back().points = std::move(pts);
    out.back().filled = true;
    out.back().closed = true;
}

inline void appendArc(std::vector<drawing::Point>& contour, drawing::Point c, float r, float a0, float delta)
{
    const int full = arcSegments(r);
    const int steps = (std::max)(1, int(std::ceil(std::fabs(delta) / kTwoPi * float(full))));
    for (int i = 0; i <= steps; ++i)
    {
        const float a = a0 + delta * float(i) / float(steps);
        contour.push_back({ c.x + r * std::cos(a), c.y + r * std::sin(a) });
    }
}

// Append the join at vertex v (between incoming unit dir u0 and outgoing u1)
// to the contour being traced along the +normal side.
// innerLimit caps how far the inner corner may sit from the vertex — beyond
// the adjoining segments it would spike, so the crossover fallback is used.
inline void appendJoin(std::vector<drawing::Point>& contour, drawing::Point v, drawing::Point u0, drawing::Point u1,
                       float hw, drawing::LineJoin join, float miterLimit, float innerLimit)
{
    const drawing::Point n0{ -u0.y * hw, u0.x * hw };
    const drawing::Point n1{ -u1.y * hw, u1.x * hw };
    const drawing::Point a{ v.x + n0.x, v.y + n0.y };
    const drawing::Point b{ v.x + n1.x, v.y + n1.y };

    const float cross = u0.x * u1.y - u0.y * u1.x;
    const float dot = u0.x * u1.x + u0.y * u1.y;
    if (std::fabs(cross) < 1e-6f && dot > 0.0f)
    {
        contour.push_back(a); // collinear: the offset lines already meet
        return;
    }

    // This side is the outside of the turn when the path turns away from it.
    const bool outer = (cross < 0.0f);
    if (!outer)
    {
        // Inner side: the true corner is where the two offset lines meet, and
        // using it leaves the correct notch. Falling back to both offset points
        // instead closes a crossover loop over that notch, which nonzero fill
        // then paints — an inner corner comes out fully covered where it should
        // be partial (measured 1.0 against Direct2D's 0.75 on a stroked rect).
        const drawing::Point bis{ n0.x + n1.x, n0.y + n1.y };
        const float bisLen = length(bis);
        if (bisLen > 1e-6f)
        {
            const float cosHalf = (bis.x * n0.x + bis.y * n0.y) / (bisLen * hw);
            if (cosHalf > 1e-4f)
            {
                const float dist = hw / cosHalf;
                if (dist <= innerLimit) // otherwise it would spike past the segments
                {
                    const float k = dist / bisLen;
                    contour.push_back({ v.x + bis.x * k, v.y + bis.y * k });
                    return;
                }
            }
        }
        contour.push_back(a);
        contour.push_back(b);
        return;
    }

    switch (join)
    {
    case drawing::LineJoin::Round:
    {
        float a0 = std::atan2(n0.y, n0.x);
        float a1 = std::atan2(n1.y, n1.x);
        float delta = a1 - a0;
        while (delta > 3.14159265358979f) delta -= kTwoPi;
        while (delta < -3.14159265358979f) delta += kTwoPi;
        appendArc(contour, v, hw, a0, delta);
        return;
    }
    case drawing::LineJoin::Miter:
    case drawing::LineJoin::MiterOrBevel:
    {
        const drawing::Point bis{ n0.x + n1.x, n0.y + n1.y };
        const float bisLen = length(bis);
        if (bisLen > 1e-6f)
        {
            // cos of half the turn angle, from the unit bisector and unit normal
            const float cosHalf = (bis.x * n0.x + bis.y * n0.y) / (bisLen * hw);
            if (cosHalf > 1e-4f)
            {
                const float miterDist = hw / cosHalf;
                const float limit = miterLimit * hw; // D2D/SVG ratio: miterLength / strokeWidth
                if (miterDist <= limit)
                {
                    const float k = miterDist / bisLen;
                    contour.push_back(a);
                    contour.push_back({ v.x + bis.x * k, v.y + bis.y * k });
                    contour.push_back(b);
                    return;
                }
                if (join == drawing::LineJoin::Miter)
                {
                    // Past the limit, D2D's plain MITER keeps the spike but
                    // cuts it off flat at the limit distance (MITER_OR_BEVEL
                    // is the one that falls back to a bevel).
                    const float k = miterDist / bisLen;
                    const drawing::Point tip{ v.x + bis.x * k, v.y + bis.y * k };
                    const float atBis = hw * cosHalf; // distance along the bisector at a and b
                    const float denom = miterDist - atBis;
                    if (denom > 1e-6f)
                    {
                        const float t = std::clamp((limit - atBis) / denom, 0.0f, 1.0f);
                        contour.push_back(a);
                        contour.push_back({ a.x + (tip.x - a.x) * t, a.y + (tip.y - a.y) * t });
                        contour.push_back({ b.x + (tip.x - b.x) * t, b.y + (tip.y - b.y) * t });
                        contour.push_back(b);
                        return;
                    }
                }
            }
        }
        break; // MiterOrBevel past the limit, or a 180-degree reversal: bevel
    }
    default:
        break;
    }

    contour.push_back(a); // Bevel
    contour.push_back(b);
}

// Append the cap that turns the contour around at endpoint p, where u is the
// outward direction of travel.
inline void appendCap(std::vector<drawing::Point>& contour, drawing::Point p, drawing::Point u,
                      float hw, drawing::CapStyle cap)
{
    const drawing::Point n{ -u.y * hw, u.x * hw };
    switch (cap)
    {
    case drawing::CapStyle::Square:
        contour.push_back({ p.x + n.x + u.x * hw, p.y + n.y + u.y * hw });
        contour.push_back({ p.x - n.x + u.x * hw, p.y - n.y + u.y * hw });
        break;
    case drawing::CapStyle::Round:
        // Half turn from +normal to -normal, bulging through +u.
        appendArc(contour, p, hw, std::atan2(n.y, n.x), -3.14159265358979f);
        break;
    default: // Flat: the contour closes straight across
        break;
    }
}

// Trace one offset side of an open polyline, inserting joins at the interior
// vertices.
inline void appendOpenSide(std::vector<drawing::Point>& contour, const std::vector<drawing::Point>& pts,
                           const std::vector<drawing::Point>& dirs, float hw,
                           drawing::LineJoin join, float miterLimit)
{
    const size_t n = pts.size();
    contour.push_back({ pts[0].x - dirs[0].y * hw, pts[0].y + dirs[0].x * hw });
    for (size_t i = 1; i + 1 < n; ++i)
    {
        const float innerLimit = (std::min)(length(sub(pts[i], pts[i - 1])),
                                            length(sub(pts[i + 1], pts[i])));
        appendJoin(contour, pts[i], dirs[i - 1], dirs[i], hw, join, miterLimit, innerLimit);
    }
    const auto& uLast = dirs[n - 2];
    contour.push_back({ pts[n - 1].x - uLast.y * hw, pts[n - 1].y + uLast.x * hw });
}

// Widen one flattened figure into stroke outline contours.
//
// The outline is traced as a contour rather than stamped as overlapping
// quads/wedges/discs. A coverage rasterizer accumulates AREA per pixel, so
// overlapping pieces double-count inside a partially covered pixel (a round
// cap overlapping its segment quad reads as ~95% covered where the true union
// is 50%). Tracing a single boundary has no overlap to double-count.
inline void widenFigure(const Figure& fig, float halfWidth, drawing::CapStyle cap,
                        drawing::LineJoin join, float miterLimit, std::vector<Figure>& out)
{
    // Drop consecutive duplicates (zero-length segments have no direction, and
    // a closed figure repeats its start point).
    //
    // "Duplicate" has to be measured RELATIVE to the coordinates involved. A
    // fixed epsilon cannot do the job: float spacing at a device coordinate of
    // 500 is already ~6e-5, so an absolute 1e-6 test keeps hair-length segments
    // whose direction is nothing but rounding noise. One of those at a vertex
    // reads as a ~180-degree turn, and the miter join answers with a spike as
    // long as the miter limit allows. 1e-5 relative is still far below a
    // thousandth of a pixel at any plausible surface size, so nothing anyone
    // meant to draw is lost.
    const auto coincident = [](drawing::Point a, drawing::Point b) {
        const float scale = (std::max)((std::max)(std::fabs(a.x), std::fabs(a.y)),
                                       (std::max)((std::max)(std::fabs(b.x), std::fabs(b.y)), 1.0f));
        return length(sub(a, b)) <= 1e-5f * scale;
    };

    std::vector<drawing::Point> pts;
    pts.reserve(fig.points.size());
    for (const auto& p : fig.points)
    {
        if (!std::isfinite(p.x) || !std::isfinite(p.y))
            return; // poisoned figure: skip entirely
        if (pts.empty() || !coincident(p, pts.back()))
            pts.push_back(p);
    }
    const bool closed = fig.closed;
    // A loop, not an if: removing the explicit closing point can expose another
    // point that is itself a whisker from the start — a flattened circle ends on
    // one, since its two arcs meet where the figure began — and that one closes
    // the ring just as badly as the first.
    while (closed && pts.size() > 1 && coincident(pts.back(), pts.front()))
        pts.pop_back();

    if (pts.empty())
        return;

    if (pts.size() == 1)
    {
        // Degenerate figure: D2D still marks it with a round or square cap.
        if (cap == drawing::CapStyle::Round)
            emitDisc(out, pts[0], halfWidth);
        else if (cap == drawing::CapStyle::Square)
            emitPolygon(out, { { pts[0].x - halfWidth, pts[0].y - halfWidth },
                               { pts[0].x + halfWidth, pts[0].y - halfWidth },
                               { pts[0].x + halfWidth, pts[0].y + halfWidth },
                               { pts[0].x - halfWidth, pts[0].y + halfWidth } });
        return;
    }

    const size_t n = pts.size();
    const size_t segCount = closed ? n : n - 1;

    // Unit direction of each segment (duplicates were already dropped, so no
    // segment is zero-length). Computed straight from the point list so the
    // forward and reversed walks cannot disagree about indexing.
    const auto segmentDirs = [segCount](const std::vector<drawing::Point>& p, bool& ok) {
        std::vector<drawing::Point> d(segCount);
        for (size_t i = 0; i < segCount; ++i)
        {
            const auto v = sub(p[(i + 1) % p.size()], p[i]);
            const float len = length(v);
            if (!(len > 0.0f))
            {
                ok = false;
                return d;
            }
            d[i] = { v.x / len, v.y / len };
        }
        return d;
    };

    bool ok = true;
    const std::vector<drawing::Point> dirs = segmentDirs(pts, ok);
    std::vector<drawing::Point> reversedPts(pts.rbegin(), pts.rend());
    const std::vector<drawing::Point> reversedDirs = segmentDirs(reversedPts, ok);
    if (!ok)
        return; // shouldn't happen after dedupe; bail rather than divide by zero

    if (!closed)
    {
        // One contour: up one side, around the end cap, back the other side,
        // around the start cap.
        std::vector<drawing::Point> contour;
        appendOpenSide(contour, pts, dirs, halfWidth, join, miterLimit);
        appendCap(contour, pts.back(), dirs.back(), halfWidth, cap);
        appendOpenSide(contour, reversedPts, reversedDirs, halfWidth, join, miterLimit);
        appendCap(contour, pts.front(), reversedDirs.back(), halfWidth, cap);

        // Canonical winding, so strokes of separate figures reinforce rather
        // than cancel where they overlap under nonzero fill.
        if (signedArea2(contour) > 0.0f)
            std::reverse(contour.begin(), contour.end());
        emitContour(out, std::move(contour));
        return;
    }

    // Closed figure: an annulus — outer ring plus an oppositely wound inner
    // ring that punches the hole.
    const auto innerLimitAt = [&](const std::vector<drawing::Point>& p, size_t i) {
        const size_t count = p.size();
        return (std::min)(length(sub(p[i], p[(i + count - 1) % count])),
                          length(sub(p[(i + 1) % count], p[i])));
    };

    std::vector<drawing::Point> ring0, ring1;
    for (size_t i = 0; i < n; ++i)
        appendJoin(ring0, pts[i], dirs[(i + segCount - 1) % segCount], dirs[i % segCount], halfWidth, join, miterLimit, innerLimitAt(pts, i));
    for (size_t i = 0; i < n; ++i)
        appendJoin(ring1, reversedPts[i], reversedDirs[(i + segCount - 1) % segCount], reversedDirs[i % segCount], halfWidth, join, miterLimit, innerLimitAt(reversedPts, i));

    // When the pen is wider than the figure, the inward offset passes through
    // itself: the inner ring comes out inverted and punches a hole where the
    // stroke should read solid. Detect it geometrically — the ring's signed
    // area is not a usable signal, because the corner crossover loops subtract
    // enough to flip the sign of a perfectly valid small ring.
    {
        float minX = pts[0].x, maxX = pts[0].x, minY = pts[0].y, maxY = pts[0].y;
        for (const auto& p : pts)
        {
            minX = (std::min)(minX, p.x); maxX = (std::max)(maxX, p.x);
            minY = (std::min)(minY, p.y); maxY = (std::max)(maxY, p.y);
        }
        if (halfWidth >= 0.5f * (std::min)(maxX - minX, maxY - minY))
        {
            auto& inner = (signedArea2(pts) > 0.0f) ? ring0 : ring1;
            inner.clear();
        }
    }

    // No winding fix-up here, deliberately. Because +n is always the same
    // rotation of the travel direction, the OUTER ring always comes out
    // negatively wound whichever way the source figure is wound: for a
    // positively wound figure the forward walk offsets inward (ring0 = inner)
    // and the reversed walk outward (ring1 = outer, negative); for a
    // negatively wound one it is the other way round. That already matches the
    // canonical sign forced on open contours above, so every stroke band in a
    // geometry reinforces under nonzero fill. Canonicalising on ring0 instead
    // (as this once did) flips the outer ring positive for half of all input
    // windings, and overlapping bands then cancel to holes.
    emitContour(out, std::move(ring0));
    emitContour(out, std::move(ring1));
}

// Everything a stroke needs: the style properties plus any custom dash array.
struct Params
{
    drawing::StrokeStyleProperties props{};
    std::vector<float> dashes;
};

// Dash lengths in absolute units. The built-in patterns are Direct2D's,
// expressed in multiples of the stroke width. Empty result = solid.
inline std::vector<float> dashPattern(const Params& params, float strokeWidth)
{
    std::vector<float> p;
    switch (params.props.dashStyle)
    {
    case drawing::DashStyle::Dash:       p = { 2.0f, 2.0f }; break;
    case drawing::DashStyle::Dot:        p = { 0.0f, 2.0f }; break;
    case drawing::DashStyle::DashDot:    p = { 2.0f, 2.0f, 0.0f, 2.0f }; break;
    case drawing::DashStyle::DashDotDot: p = { 2.0f, 2.0f, 0.0f, 2.0f, 0.0f, 2.0f }; break;
    case drawing::DashStyle::Custom:     p = params.dashes; break;
    default:                             return {}; // Solid
    }

    float total = 0.0f;
    for (auto& v : p)
    {
        v = (v < 0.0f ? 0.0f : v) * strokeWidth;
        total += v;
    }
    if (!(total > 0.0f) || p.size() < 2)
        return {}; // degenerate pattern: draw solid rather than nothing at all
    return p;
}

// Split one figure into the "on" runs of the dash pattern, as open figures.
inline void dashFigure(const Figure& fig, const std::vector<float>& pattern, float offset,
                       std::vector<Figure>& out)
{
    std::vector<drawing::Point> pts;
    pts.reserve(fig.points.size() + 1);
    for (const auto& p : fig.points)
    {
        if (!std::isfinite(p.x) || !std::isfinite(p.y))
            return;
        if (pts.empty() || length(sub(p, pts.back())) > 1e-6f)
            pts.push_back(p);
    }
    if (fig.closed && pts.size() > 1 && length(sub(pts.back(), pts.front())) > 1e-6f)
        pts.push_back(pts.front()); // dashes run continuously around a closed figure
    if (pts.size() < 2)
        return;

    // Cumulative arc length at each vertex.
    std::vector<float> arc(pts.size(), 0.0f);
    for (size_t i = 1; i < pts.size(); ++i)
        arc[i] = arc[i - 1] + length(sub(pts[i], pts[i - 1]));
    const float totalLen = arc.back();
    if (!(totalLen > 0.0f))
        return;

    float cycle = 0.0f;
    for (float v : pattern)
        cycle += v;

    // Point at a given arc length along the polyline.
    const auto pointAtArc = [&](float d) {
        d = std::clamp(d, 0.0f, totalLen);
        size_t i = 1;
        while (i + 1 < pts.size() && arc[i] < d)
            ++i;
        const float segLen = arc[i] - arc[i - 1];
        const float t = segLen > 0.0f ? (d - arc[i - 1]) / segLen : 0.0f;
        return drawing::Point{ pts[i - 1].x + (pts[i].x - pts[i - 1].x) * t,
                               pts[i - 1].y + (pts[i].y - pts[i - 1].y) * t };
    };

    // Start phase within the pattern.
    float phase = std::fmod(offset, cycle);
    if (phase < 0.0f)
        phase += cycle;
    size_t idx = 0;
    while (phase >= pattern[idx] && pattern[idx] > 0.0f)
    {
        phase -= pattern[idx];
        idx = (idx + 1) % pattern.size();
    }

    // Walk the pattern along the polyline. Each cycle advances by `cycle` > 0,
    // so this always terminates even when some entries are zero-length.
    float pos = -phase;
    while (pos < totalLen)
    {
        const float len = pattern[idx];
        const float start = pos;
        const float end = pos + len;
        const bool on = (idx % 2) == 0;

        if (on && end >= 0.0f && start <= totalLen)
        {
            const float s = (std::max)(start, 0.0f);
            const float e = (std::min)(end, totalLen);
            Figure run;
            run.filled = true;
            run.closed = false;
            if (e <= s)
            {
                run.points.push_back(pointAtArc(s)); // zero-length dash: a dot
            }
            else
            {
                run.points.push_back(pointAtArc(s));
                for (size_t i = 0; i < pts.size(); ++i)
                    if (arc[i] > s && arc[i] < e)
                        run.points.push_back(pts[i]); // keep interior corners
                run.points.push_back(pointAtArc(e));
            }
            out.push_back(std::move(run));
        }

        pos = end;
        idx = (idx + 1) % pattern.size();
    }
}

inline void widen(const std::vector<Figure>& figures, float strokeWidth,
                  const Params& params, std::vector<Figure>& out)
{
    const float hw = strokeWidth * 0.5f;
    const float miterLimit = (std::max)(1.0f, params.props.miterLimit);
    const auto pattern = dashPattern(params, strokeWidth);

    if (pattern.empty())
    {
        for (const auto& fig : figures)
            widenFigure(fig, hw, params.props.lineCap, params.props.lineJoin, miterLimit, out);
        return;
    }

    std::vector<Figure> dashed;
    for (const auto& fig : figures)
        dashFigure(fig, pattern, params.props.dashOffset * strokeWidth, dashed);
    for (const auto& fig : dashed)
        widenFigure(fig, hw, params.props.lineCap, params.props.lineJoin, miterLimit, out);
}

} // namespace stroke

// Stroke parameters from an IStrokeStyle (null = Direct2D defaults).
inline stroke::Params strokeParams(drawing::api::IStrokeStyle* strokeStyle)
{
    stroke::Params params;
    if (auto* ss = dynamic_cast<se::generic_graphics::StrokeStyle*>(strokeStyle))
    {
        params.props = ss->strokeStyleProperties;
        params.dashes = ss->dashes;
    }
    return params;
}

class PathGeometry final : public drawing::api::IPathGeometry
{
public:
    drawing::api::IFactory* factory{};
    std::vector<Figure> figures;
    drawing::FillMode fillMode{ drawing::FillMode::Alternate }; // matches the D2D sink default

    PathGeometry(drawing::api::IFactory* pfactory) : factory(pfactory) {}

    ReturnCode open(drawing::api::IGeometrySink** returnGeometrySink) override;

    ReturnCode strokeContainsPoint(drawing::Point point, float strokeWidth, drawing::api::IStrokeStyle* strokeStyle, const drawing::Matrix3x2* worldTransform, bool* returnContains) override
    {
        *returnContains = false;
        if (!(strokeWidth > 0.0f) || !std::isfinite(strokeWidth))
            return ReturnCode::Ok;

        std::vector<Figure> widened;
        stroke::widen(figures, strokeWidth, strokeParams(strokeStyle), widened);

        // Sum the winding over ALL pieces, exactly as the nonzero fill does.
        // The pieces are not independent shapes: a closed figure's stroke is
        // an annulus whose inner ring must subtract, so testing each piece on
        // its own and OR-ing would report the shape's whole interior as being
        // on the stroke.
        const drawing::Matrix3x2 identity;
        const auto& m = worldTransform ? *worldTransform : identity;
        int winding = 0;
        for (const auto& piece : widened)
        {
            const size_t n = piece.points.size();
            for (size_t i = 0; i < n; ++i)
            {
                const auto a = drawing::transformPoint(m, piece.points[i]);
                const auto b = drawing::transformPoint(m, piece.points[(i + 1) % n]);
                if ((a.y <= point.y) != (b.y <= point.y))
                {
                    const float xCross = a.x + (point.y - a.y) * (b.x - a.x) / (b.y - a.y);
                    if (point.x < xCross)
                        winding += (b.y > a.y) ? 1 : -1;
                }
            }
        }
        *returnContains = (winding != 0);
        return ReturnCode::Ok;
    }

    ReturnCode fillContainsPoint(drawing::Point point, const drawing::Matrix3x2* worldTransform, bool* returnContains) override
    {
        const drawing::Matrix3x2 identity;
        const auto& m = worldTransform ? *worldTransform : identity;

        int winding = 0;
        for (const auto& fig : figures)
        {
            if (!fig.filled)
                continue;
            const size_t n = fig.points.size();
            for (size_t i = 0; i < n; ++i)
            {
                const auto a = drawing::transformPoint(m, fig.points[i]);
                const auto b = drawing::transformPoint(m, fig.points[(i + 1) % n]);
                if ((a.y <= point.y) != (b.y <= point.y))
                {
                    const float xCross = a.x + (point.y - a.y) * (b.x - a.x) / (b.y - a.y);
                    if (point.x < xCross)
                        winding += (b.y > a.y) ? 1 : -1;
                }
            }
        }
        *returnContains = (fillMode == drawing::FillMode::Winding) ? (winding != 0) : ((winding & 1) != 0);
        return ReturnCode::Ok;
    }

    ReturnCode getWidenedBounds(float strokeWidth, drawing::api::IStrokeStyle* strokeStyle, const drawing::Matrix3x2* worldTransform, drawing::Rect* returnBounds) override
    {
        *returnBounds = {};
        if (!(strokeWidth > 0.0f) || !std::isfinite(strokeWidth))
            return ReturnCode::Ok;

        std::vector<Figure> widened;
        stroke::widen(figures, strokeWidth, strokeParams(strokeStyle), widened);

        const drawing::Matrix3x2 identity;
        const auto& m = worldTransform ? *worldTransform : identity;
        bool any = false;
        drawing::Rect bounds{};
        for (const auto& piece : widened)
        {
            for (const auto& p : piece.points)
            {
                const auto tp = drawing::transformPoint(m, p);
                if (!any)
                {
                    bounds = { tp.x, tp.y, tp.x, tp.y };
                    any = true;
                }
                else
                {
                    bounds.left = (std::min)(bounds.left, tp.x);
                    bounds.top = (std::min)(bounds.top, tp.y);
                    bounds.right = (std::max)(bounds.right, tp.x);
                    bounds.bottom = (std::max)(bounds.bottom, tp.y);
                }
            }
        }
        *returnBounds = bounds;
        return ReturnCode::Ok;
    }

    ReturnCode getFactory(drawing::api::IFactory** returnFactory) override
    {
        *returnFactory = factory;
        return ReturnCode::Ok;
    }

    ReturnCode queryInterface(const gmpi::api::Guid* iid, void** returnInterface) override
    {
        *returnInterface = {};
        GMPI_QUERYINTERFACE(drawing::api::IPathGeometry);
        GMPI_QUERYINTERFACE(drawing::api::IResource);
        return ReturnCode::NoSupport;
    }
    GMPI_REFCOUNT;
};

class GeometrySink final : public se::generic_graphics::GeometrySink
{
    PathGeometry* path;

public:
    GeometrySink(PathGeometry* ppath) : path(ppath)
    {
        path->addRef();
    }
    ~GeometrySink()
    {
        path->release();
    }

    void beginFigure(drawing::Point pStartPoint, drawing::FigureBegin figureBegin) override
    {
        se::generic_graphics::GeometrySink::beginFigure(pStartPoint, figureBegin);
        path->figures.emplace_back();
        path->figures.back().points.push_back(pStartPoint);
        path->figures.back().filled = (figureBegin == drawing::FigureBegin::Filled);
    }

    void addLine(drawing::Point point) override
    {
        if (!path->figures.empty())
            path->figures.back().points.push_back(point);
        lastPoint = point;
    }

    void endFigure(drawing::FigureEnd figureEnd) override
    {
        if (!path->figures.empty())
            path->figures.back().closed = (figureEnd == drawing::FigureEnd::Closed);
        // The base class appends the closing segment; widenFigure drops the
        // duplicated start point.
        se::generic_graphics::GeometrySink::endFigure(figureEnd);
    }

    void setFillMode(drawing::FillMode fillMode) override
    {
        path->fillMode = fillMode;
    }
};

inline ReturnCode PathGeometry::open(drawing::api::IGeometrySink** returnGeometrySink)
{
    figures.clear();
    fillMode = drawing::FillMode::Alternate; // each sink starts from the D2D default
    *returnGeometrySink = new GeometrySink(this);
    return ReturnCode::Ok;
}

// ---------------------------------------------------------------------------
// Rasterizer: signed-area coverage accumulation (font-rs style).
// Walk each edge once, accumulating fractional signed coverage deltas into a
// float buffer; a per-row prefix sum then yields analytic antialiased
// coverage. Coordinates are relative to the raster-area origin; callers clamp
// x into [0, w]. Each row has (w + 2) cells: edges may spill one cell past the
// right boundary, and the spill cells never enter the prefix sum.
// ---------------------------------------------------------------------------
namespace raster
{

inline void accumulateEdge(float* acc, int w, int h, drawing::Point p0, drawing::Point p1)
{
    if (p0.y == p1.y)
        return;

    float dir = 1.0f;
    if (p0.y > p1.y)
    {
        std::swap(p0, p1);
        dir = -1.0f;
    }
    const float dxdy = (p1.x - p0.x) / (p1.y - p0.y);
    const int rowStride = w + 2;

    float x = p0.x;
    if (p0.y < 0.0f)
        x -= p0.y * dxdy; // advance x to the y=0 boundary

    // Clamp in float BEFORE the int conversions: float->int is undefined
    // behaviour when the value exceeds int range (|y| can be ~1e30 here).
    const int yFirst = int(std::floor(std::clamp(p0.y, 0.0f, float(h))));
    const int yLimit = int(std::ceil(std::clamp(p1.y, 0.0f, float(h))));

    for (int y = yFirst; y < yLimit; ++y)
    {
        float* row = acc + size_t(y) * rowStride;
        const float dy = (std::min)(float(y + 1), p1.y) - (std::max)(float(y), p0.y);
        const float xnext = x + dxdy * dy;
        const float d = dy * dir;

        float x0 = x, x1 = xnext;
        if (x0 > x1)
            std::swap(x0, x1);
        // The per-row interpolated x can round an ulp outside [0, w] even when
        // the segment endpoints were clamped; unclamped, floor() yields -1 and
        // row[-1] is a heap-underflow write on raster row 0 (found and
        // confirmed by the adversarial review with exact float32 repro).
        x0 = std::clamp(x0, 0.0f, float(w));
        x1 = std::clamp(x1, 0.0f, float(w));

        const float x0floor = std::floor(x0);
        const int x0i = int(x0floor);
        const float x1ceil = std::ceil(x1);
        const int x1i = int(x1ceil);

        if (x1i <= x0i + 1)
        {
            // Sub-edge fits within one pixel column: midpoint split.
            const float xmf = 0.5f * (x0 + x1) - x0floor;
            row[x0i] += d * (1.0f - xmf);
            row[x0i + 1] += d * xmf;
        }
        else
        {
            const float s = 1.0f / (x1 - x0);
            const float x0f = x0 - x0floor;
            const float a0 = 0.5f * s * (1.0f - x0f) * (1.0f - x0f);
            const float x1f = x1 - (x1ceil - 1.0f);
            const float am = 0.5f * s * x1f * x1f;

            row[x0i] += d * a0;
            if (x1i == x0i + 2)
            {
                row[x0i + 1] += d * (1.0f - a0 - am);
            }
            else
            {
                const float a1 = s * (1.5f - x0f);
                row[x0i + 1] += d * (a1 - a0);
                for (int xi = x0i + 2; xi < x1i - 1; ++xi)
                    row[xi] += d * s;
                const float a2 = a1 + float(x1i - x0i - 3) * s;
                row[x1i - 1] += d * (1.0f - a2 - am);
            }
            row[x1i] += d * am;
        }
        x = xnext;
    }
}

// Turn an accumulated winding number into coverage. Shared by filling and by
// clip-mask generation, so the two can never disagree about what a fill mode
// means.
inline float foldCoverage(float run, bool nonzero)
{
    if (nonzero)
        return (std::min)(1.0f, std::fabs(run));
    const float t = std::fmod(std::fabs(run), 2.0f);
    return 1.0f - std::fabs(t - 1.0f);
}

// Clip an edge to the raster area's horizontal range, preserving winding:
// portions left of x=0 are projected onto the x=0 boundary (their winding
// affects every visible cell on their rows); portions right of x=w cannot
// affect visible cells and are dropped. Vertical clipping is exact inside
// accumulateEdge's row loop. The segment is split at boundary crossings so
// slopes are never distorted (unlike endpoint clamping).
inline void accumulateEdgeClipped(float* acc, int w, int h, drawing::Point p0, drawing::Point p1)
{
    if (p0.y == p1.y)
        return;

    const float fw = float(w);

    // Fast path: the edge never leaves the horizontal range.
    if (p0.x >= 0.0f && p0.x <= fw && p1.x >= 0.0f && p1.x <= fw)
    {
        accumulateEdge(acc, w, h, p0, p1);
        return;
    }

    // Split at crossings of x=0 and x=w. Splits are computed in double and the
    // original endpoints are used verbatim at t=0/1: with far-off-surface
    // coordinates (|x|,|y| ~ 1e7) reconstructing an endpoint as p0 + t*(p1-p0)
    // in float32 can land a whole pixel away (ulp of 1e7 is 1.0), which
    // visibly moved fractional edges near the surface.
    const double x0d = p0.x, y0d = p0.y;
    const double dxd = double(p1.x) - x0d;
    const double dyd = double(p1.y) - y0d;

    double ts[4] = { 0.0, 1.0, 1.0, 1.0 };
    int n = 2;
    if (dxd != 0.0)
    {
        const double tLeft = (0.0 - x0d) / dxd;
        const double tRight = (double(fw) - x0d) / dxd;
        if (tLeft > 0.0 && tLeft < 1.0)
            ts[n++] = tLeft;
        if (tRight > 0.0 && tRight < 1.0)
            ts[n++] = tRight;
    }
    std::sort(ts, ts + n);

    const auto pointAt = [&](double t) -> drawing::Point {
        if (t <= 0.0) return p0;
        if (t >= 1.0) return p1;
        return { float(x0d + t * dxd), float(y0d + t * dyd) };
    };

    for (int i = 0; i + 1 < n; ++i)
    {
        const double ta = ts[i], tb = ts[i + 1];
        if (tb <= ta)
            continue;

        auto a = pointAt(ta);
        auto b = pointAt(tb);

        const double xMid = x0d + 0.5 * (ta + tb) * dxd; // region classifier
        if (xMid < 0.0)
        {
            a.x = b.x = 0.0f; // project onto the left boundary
        }
        else if (xMid > fw)
        {
            continue; // right of the raster area: invisible
        }
        else
        {
            // guard float jitter at the split points
            a.x = std::clamp(a.x, 0.0f, fw);
            b.x = std::clamp(b.x, 0.0f, fw);
        }
        accumulateEdge(acc, w, h, a, b);
    }
}

// Rasterize figures that are ALREADY IN DEVICE SPACE into a coverage mask
// covering `bounds`. One implementation shared by geometry clipping and the
// glyph atlas, so neither can drift from filling on what a fill mode means.
inline void rasterizeCoverage(const std::vector<Figure>& figures, drawing::FillMode fillMode,
                              const drawing::RectL& bounds, std::vector<float>& returnAlpha)
{
    const int w = bounds.right - bounds.left;
    const int h = bounds.bottom - bounds.top;
    returnAlpha.clear();
    if (w <= 0 || h <= 0)
        return;

    const int rowStride = w + 2;
    std::vector<float> acc(size_t(rowStride) * h, 0.0f);

    for (const auto& fig : figures)
    {
        const size_t n = fig.points.size();
        if (!fig.filled || n == 0)
            continue;
        for (size_t i = 0; i < n; ++i)
        {
            auto a = fig.points[i];
            auto b = fig.points[(i + 1) % n];
            a.x -= float(bounds.left); a.y -= float(bounds.top);
            b.x -= float(bounds.left); b.y -= float(bounds.top);
            accumulateEdgeClipped(acc.data(), w, h, a, b);
        }
    }

    returnAlpha.assign(size_t(w) * h, 0.0f);
    const bool nonzero = (fillMode == drawing::FillMode::Winding);
    for (int y = 0; y < h; ++y)
    {
        const float* accRow = acc.data() + size_t(y) * rowStride;
        float* out = returnAlpha.data() + size_t(y) * w;
        float run = 0.0f;
        for (int x = 0; x < w; ++x)
        {
            run += accRow[x];
            out[x] = foldCoverage(run, nonzero);
        }
    }
}

} // namespace raster

// ---------------------------------------------------------------------------
// RenderTarget: device context + offscreen bitmap in one.
// Standard inheritance of IBitmapRenderTarget via the templated Gfx_base
// context (no vtable-layout emulation). Rectangles, ellipses, rounded rects
// and lines all route through path geometry in the base class -- the only
// pixel-touching code is fillGeometry's coverage/blend below.
// ---------------------------------------------------------------------------
class RenderTarget final : public se::generic_graphics::GraphicsContextT<drawing::api::IBitmapRenderTarget>
{
    drawing::api::IFactory* factory{};
    Bitmap* bitmap{}; // owned (refcounted)

    // transform_ is the EFFECTIVE local -> device-pixel transform that every
    // drawing routine below uses: the client's transform followed by the DPI
    // scale, pre-multiplied so the hot paths stay a single matrix.
    //
    // clientTransform_ is what setTransform() was handed. getTransform() has to
    // return that, not the effective one - a client that reads the transform,
    // concatenates its own and writes it back is the normal idiom, and handing
    // back the DPI-scaled matrix would compound the scale on every round trip.
    drawing::Matrix3x2 transform_;
    drawing::Matrix3x2 clientTransform_;
    drawing::Matrix3x2 dpiTransform_;
    float              dpiScale_{ 1.0f };

    // The factory owns the text engine; reach it through the factory so a
    // render target never holds a stale pointer.
    ICpuTextEngine* textEngine() const;

    // A clip of arbitrary shape, as per-pixel coverage over a device-space box.
    // Outside the box the clip is empty, so callers must test bounds first.
    struct ClipMask
    {
        drawing::RectL bounds{};
        std::vector<float> alpha;

        float at(int x, int y) const
        {
            if (x < bounds.left || x >= bounds.right || y < bounds.top || y >= bounds.bottom)
                return 0.0f;
            return alpha[size_t(y - bounds.top) * size_t(bounds.right - bounds.left) + size_t(x - bounds.left)];
        }
        float& atMutable(int x, int y)
        {
            return alpha[size_t(y - bounds.top) * size_t(bounds.right - bounds.left) + size_t(x - bounds.left)];
        }
    };

    // Parallel to clipRectStack: null means "no shaped clip at this depth".
    std::vector<std::shared_ptr<const ClipMask>> clipMaskStack{ nullptr };

    const ClipMask* activeClipMask() const { return clipMaskStack.back().get(); }

    // Rasterize figures into a coverage mask, using the same edge accumulation
    // and the same fill-mode fold as ordinary filling.
    void buildClipMask(const std::vector<Figure>& figures, drawing::FillMode fillMode, ClipMask& mask)
    {
        mask.bounds = {};
        mask.alpha.clear();

        float minX = 1e30f, minY = 1e30f, maxX = -1e30f, maxY = -1e30f;
        std::vector<drawing::Point> points;
        std::vector<size_t> figureStart;
        for (const auto& fig : figures)
        {
            figureStart.push_back(points.size());
            for (const auto& p : fig.points)
            {
                const auto dp = drawing::transformPoint(transform_, p);
                if (!std::isfinite(dp.x) || !std::isfinite(dp.y))
                    return; // poisoned geometry clips everything out
                points.push_back(dp);
                minX = (std::min)(minX, dp.x); maxX = (std::max)(maxX, dp.x);
                minY = (std::min)(minY, dp.y); maxY = (std::max)(maxY, dp.y);
            }
        }
        if (points.empty() || minX > maxX)
            return;

        const auto clip = drawing::intersectRect(clipRectStack.back(), surfaceBounds());
        if (!(clip.left < clip.right) || !(clip.top < clip.bottom))
            return;
        minX = std::clamp(minX, clip.left, clip.right);
        maxX = std::clamp(maxX, clip.left, clip.right);
        minY = std::clamp(minY, clip.top, clip.bottom);
        maxY = std::clamp(maxY, clip.top, clip.bottom);

        const int ix0 = (std::max)(int(std::floor(minX)), aliasedCoord(clip.left));
        const int iy0 = (std::max)(int(std::floor(minY)), aliasedCoord(clip.top));
        const int ix1 = (std::min)(int(std::ceil(maxX)), aliasedCoord(clip.right));
        const int iy1 = (std::min)(int(std::ceil(maxY)), aliasedCoord(clip.bottom));
        const int w = ix1 - ix0;
        const int h = iy1 - iy0;
        if (w <= 0 || h <= 0)
            return;

        // Hand the device-space copy to the shared rasterizer.
        std::vector<Figure> deviceFigures;
        deviceFigures.reserve(figures.size());
        size_t figIndex = 0;
        for (const auto& fig : figures)
        {
            const size_t base = figureStart[figIndex++];
            if (!fig.filled || fig.points.empty())
                continue;
            Figure copy;
            copy.filled = true;
            copy.closed = true;
            copy.points.assign(points.begin() + base, points.begin() + base + fig.points.size());
            deviceFigures.push_back(std::move(copy));
        }

        mask.bounds = { ix0, iy0, ix1, iy1 };
        raster::rasterizeCoverage(deviceFigures, fillMode, mask.bounds, mask.alpha);
        if (mask.alpha.empty())
            mask.bounds = {};
    }

public:
    // Blend a precomputed coverage mask — the glyph atlas fast path. Coverage
    // arrives ready-made instead of being accumulated from edges, and from
    // there it takes exactly the same route to pixels as any fill: clip mask,
    // brush span, premultiplied over.
    ReturnCode blendCoverageMask(const float* alpha, int maskWidth, int maskHeight,
                                 int dstX, int dstY, const CpuBrush& brush)
    {
        if (!alpha || maskWidth <= 0 || maskHeight <= 0 || !brush.isPaintable())
            return ReturnCode::Ok;

        auto& s = bitmap->surface;
        const auto clip = drawing::intersectRect(clipRectStack.back(), surfaceBounds());
        if (!(clip.left < clip.right) || !(clip.top < clip.bottom))
            return ReturnCode::Ok;

        const int ix0 = (std::max)(dstX, aliasedCoord(clip.left));
        const int iy0 = (std::max)(dstY, aliasedCoord(clip.top));
        const int ix1 = (std::min)(dstX + maskWidth, aliasedCoord(clip.right));
        const int iy1 = (std::min)(dstY + maskHeight, aliasedCoord(clip.bottom));
        if (ix1 <= ix0 || iy1 <= iy0)
            return ReturnCode::Ok;

        const auto deviceToLocal = drawing::invert(transform_);
        const int ax0 = ix0 & ~3;
        const int ax1 = (std::min)(s.stridePixels, (ix1 + 3) & ~3);
        const int spanPx = ax1 - ax0;

        covBuf.assign(size_t(s.stridePixels), 0.0f);
        rowBuf.resize(size_t(spanPx) * 4);
        srcBuf.resize(size_t(spanPx) * 4);

        const auto* clipMask = activeClipMask();

        for (int y = iy0; y < iy1; ++y)
        {
            const float* maskRow = alpha + size_t(y - dstY) * size_t(maskWidth);
            float* cov = covBuf.data();
            for (int x = ix0; x < ix1; ++x)
                cov[x] = maskRow[x - dstX];
            if (clipMask)
                for (int x = ix0; x < ix1; ++x)
                    cov[x] *= clipMask->at(x, y);

            uint16_t* p = s.row(y) + size_t(ax0) * 4;
            loadSpan(p, rowBuf.data(), spanPx * 4);
            brush.evalSpan(deviceToLocal, ax0, y, spanPx, srcBuf.data());

            float* d = rowBuf.data();
            const float* sc = srcBuf.data();
            const float* cv = cov + ax0;
            for (int i = 0; i < spanPx; ++i, d += 4, sc += 4)
            {
                const float c = cv[i];
                if (c <= 0.0f)
                    continue;
                blendPixel(d, sc, c, gammaSpaceBlend);
            }
            storeSpan(rowBuf.data(), p, spanPx * 4);

            std::memset(cov + ix0, 0, size_t(ix1 - ix0) * sizeof(float));
        }
        return ReturnCode::Ok;
    }

private:

    // Set while filling glyph outlines, so text rendered as geometry composites
    // exactly as the glyph atlas does. Without it the two paths disagree, and
    // the atlas stops being a pure optimisation.
public:
    bool gammaSpaceBlend = false;

private:
    // scratch buffers, reused across fills
    std::vector<float> accBuf;      // (w + 2) * h coverage deltas
    std::vector<float> covBuf;      // per-row coverage, indexed by absolute x
    std::vector<float> rowBuf;      // fp32 staging for one row span
    std::vector<float> srcBuf;      // premultiplied source colour for one row span
    std::vector<drawing::Point> devicePoints;
    std::vector<char> figureValid;
    std::vector<Figure> strokeFigures; // widened stroke outline, reused per call

public:
    // `size` is in PIXELS; `dpi` says how many of those a DIP is worth, exactly
    // as it does for Direct2D. At the default 96 the scale is 1 and coordinates
    // are pixels, which is what every on-screen frame here wants. Above that,
    // drawing is in DIPs and the surface holds a higher-resolution rendering of
    // the same logical picture - what an offscreen thumbnail or a HiDPI export
    // needs. This argument used to be discarded, so such callers silently drew
    // at 1 DIP = 1 pixel into a surface sized for more, filling a corner of it.
    RenderTarget(drawing::api::IFactory* pfactory, drawing::SizeU size, int32_t flags = 0, float dpi = 96.0f)
        : factory(pfactory)
    {
        bitmap = new Bitmap(factory, int32_t(size.width), int32_t(size.height), flags);

        dpiScale_     = (dpi > 0.0f) ? dpi / 96.0f : 1.0f;
        dpiTransform_ = drawing::makeScale(dpiScale_);
        transform_    = dpiTransform_;   // clientTransform_ starts as identity

        // Clip stack is in device space; replace the base's huge default with the surface bounds.
        clipRectStack.clear();
        clipRectStack.push_back(surfaceBounds());
    }

    ~RenderTarget()
    {
        bitmap->release();
    }

    drawing::Rect surfaceBounds() const
    {
        return { 0.0f, 0.0f, float(bitmap->surface.width), float(bitmap->surface.height) };
    }

    // ---- resource creation -------------------------------------------------
    ReturnCode createSolidColorBrush(const drawing::Color* color, const drawing::BrushProperties* brushProperties, drawing::api::ISolidColorBrush** returnSolidColorBrush) override
    {
        *returnSolidColorBrush = new SolidColorBrush(factory, color, brushProperties);
        return ReturnCode::Ok;
    }

    ReturnCode createBitmapBrush(drawing::api::IBitmap* sourceBitmap, const drawing::BrushProperties* brushProperties, drawing::api::IBitmapBrush** returnBitmapBrush) override
    {
        *returnBitmapBrush = {};
        auto* bm = dynamic_cast<Bitmap*>(sourceBitmap);
        if (!bm)
            return ReturnCode::Fail;
        *returnBitmapBrush = new BitmapBrush(factory, bm, brushProperties);
        return ReturnCode::Ok;
    }

    ReturnCode drawBitmap(drawing::api::IBitmap* sourceBitmap, const drawing::Rect* destinationRectangle, float opacity, drawing::BitmapInterpolationMode interpolationMode, const drawing::Rect* sourceRectangle) override
    {
        auto* bm = dynamic_cast<Bitmap*>(sourceBitmap);
        if (!bm || !destinationRectangle)
            return ReturnCode::NoSupport;

        const drawing::Rect fullSource{ 0.0f, 0.0f, float(bm->surface.width), float(bm->surface.height) };
        const drawing::Rect src = sourceRectangle ? *sourceRectangle : fullSource;
        const auto& dst = *destinationRectangle;

        const float dstW = dst.right - dst.left;
        const float dstH = dst.bottom - dst.top;
        if (!(dstW != 0.0f) || !(dstH != 0.0f))
            return ReturnCode::Ok;

        // Map the destination rectangle onto the source rectangle. Everything
        // else — transform, clipping, antialiasing — comes from the ordinary
        // fill path, so drawBitmap needs no pixel-touching code of its own.
        const float scaleX = (src.right - src.left) / dstW;
        const float scaleY = (src.bottom - src.top) / dstH;
        const drawing::Matrix3x2 localToSource{
            scaleX, 0.0f,
            0.0f, scaleY,
            src.left - dst.left * scaleX,
            src.top - dst.top * scaleY };

        BitmapSource source(bm, localToSource, opacity, /*wrap*/ false,
                            interpolationMode == drawing::BitmapInterpolationMode::Linear,
                            src);

        auto geometry = createRectangleGeometry(destinationRectangle, true);
        auto* path = dynamic_cast<PathGeometry*>(drawing::AccessPtr::get(geometry));
        if (!path)
            return ReturnCode::Fail;
        return fillFigures(path->figures, drawing::FillMode::Winding, source);
    }

    ReturnCode createCompatibleRenderTarget(drawing::Size desiredSize, int32_t flags, drawing::api::IBitmapRenderTarget** returnBitmapRenderTarget) override
    {
        *returnBitmapRenderTarget = {};

        // desiredSize is in DIPs and the compatible target inherits this one's
        // DPI, as it does under Direct2D - so the surface needs dpiScale_ pixels
        // per DIP. At the default 96 dpi this is the previous behaviour exactly.
        const drawing::SizeU size{
            uint32_t((std::max)(0.0f, std::ceil(desiredSize.width  * dpiScale_))),
            uint32_t((std::max)(0.0f, std::ceil(desiredSize.height * dpiScale_))) };
        try
        {
            *returnBitmapRenderTarget = new RenderTarget(factory, size, flags, dpiScale_ * 96.0f);
        }
        catch (...)
        {
            return ReturnCode::Fail;
        }
        return ReturnCode::Ok;
    }

    ReturnCode createGradientstopCollection(const drawing::Gradientstop* gradientstops, uint32_t gradientstopsCount, drawing::ExtendMode extendMode, drawing::api::IGradientstopCollection** returnCollection) override
    {
        *returnCollection = {};
        if (!gradientstops || gradientstopsCount == 0)
            return ReturnCode::Fail;
        *returnCollection = new GradientstopCollection(factory, gradientstops, gradientstopsCount, extendMode);
        return ReturnCode::Ok;
    }

    ReturnCode createLinearGradientBrush(const drawing::LinearGradientBrushProperties* linearGradientBrushProperties, const drawing::BrushProperties* brushProperties, drawing::api::IGradientstopCollection* gradientstopCollection, drawing::api::ILinearGradientBrush** returnBrush) override
    {
        *returnBrush = {};
        if (!linearGradientBrushProperties || !dynamic_cast<GradientstopCollection*>(gradientstopCollection))
            return ReturnCode::Fail;
        *returnBrush = new LinearGradientBrush(factory, linearGradientBrushProperties, brushProperties, gradientstopCollection);
        return ReturnCode::Ok;
    }

    ReturnCode createRadialGradientBrush(const drawing::RadialGradientBrushProperties* radialGradientBrushProperties, const drawing::BrushProperties* brushProperties, drawing::api::IGradientstopCollection* gradientstopCollection, drawing::api::IRadialGradientBrush** returnBrush) override
    {
        *returnBrush = {};
        if (!radialGradientBrushProperties || !dynamic_cast<GradientstopCollection*>(gradientstopCollection))
            return ReturnCode::Fail;
        *returnBrush = new RadialGradientBrush(factory, radialGradientBrushProperties, brushProperties, gradientstopCollection);
        return ReturnCode::Ok;
    }

    // ---- state -------------------------------------------------------------
    ReturnCode setTransform(const drawing::Matrix3x2* ptransform) override
    {
        // Client space is DIPs, so its transform applies FIRST and the DPI scale
        // converts the result to pixels. Row-vector convention here: `a * b`
        // means a then b.
        clientTransform_ = *ptransform;
        transform_       = clientTransform_ * dpiTransform_;
        return ReturnCode::Ok;
    }

    ReturnCode getTransform(drawing::Matrix3x2* returnTransform) override
    {
        *returnTransform = clientTransform_;
        return ReturnCode::Ok;
    }

    ReturnCode pushAxisAlignedClip(const drawing::Rect* clipRect) override
    {
        // D2D semantics: the rect is in local space at push time; stack is device space.
        clipRectStack.push_back(drawing::intersectRect(clipRectStack.back(), transformBounds(transform_, *clipRect)));
        clipMaskStack.push_back(clipMaskStack.back()); // a rect clip needs no mask of its own
        return ReturnCode::Ok;
    }

    // Clip to an arbitrary geometry. Rasterized to a coverage mask and
    // intersected with whatever clip is already active, so the clip edge is
    // antialiased like any other edge rather than stair-stepped.
    ReturnCode pushClipGeometry(drawing::api::IPathGeometry* geometry) override
    {
        auto* path = dynamic_cast<PathGeometry*>(geometry);
        if (!path)
            return ReturnCode::NoSupport;

        auto mask = std::make_shared<ClipMask>();
        buildClipMask(path->figures, path->fillMode, *mask);

        // Intersect with the enclosing clip.
        if (const auto* previous = activeClipMask())
        {
            for (int y = mask->bounds.top; y < mask->bounds.bottom; ++y)
                for (int x = mask->bounds.left; x < mask->bounds.right; ++x)
                    mask->atMutable(x, y) *= previous->at(x, y);
        }

        clipRectStack.push_back(drawing::intersectRect(
            clipRectStack.back(),
            drawing::Rect{ float(mask->bounds.left), float(mask->bounds.top),
                           float(mask->bounds.right), float(mask->bounds.bottom) }));
        clipMaskStack.push_back(std::move(mask));
        return ReturnCode::Ok;
    }

    // Both stacks move together, so a pop always undoes exactly one push
    // whichever kind it was.
    ReturnCode popAxisAlignedClip() override
    {
        if (clipRectStack.size() > 1)
            clipRectStack.pop_back();
        if (clipMaskStack.size() > 1)
            clipMaskStack.pop_back();
        return ReturnCode::Ok;
    }

    ReturnCode getAxisAlignedClip(drawing::Rect* returnClipRect) override
    {
        *returnClipRect = transformBounds(drawing::invert(transform_), clipRectStack.back());
        return ReturnCode::Ok;
    }

    ReturnCode beginDraw() override { return ReturnCode::Ok; }
    ReturnCode endDraw() override
    {
        // A Mask or SRGBPixels target really is an 8-bit buffer on the DirectX
        // and JUCE backends. Rendering here stays fp16 — but the stored result
        // is rounded to what the flagged format can hold, so compositing this
        // bitmap matches the other backends instead of being quietly better
        // than them. The colour face rounds through sRGB (see surfaceToStaging),
        // so the levels land where the eye needs them rather than crowding into
        // the highlights the way 8-bit linear did.
        if (bitmap && bitmap->lockFormat != drawing::api::IBitmapPixels::RGBA_16f)
        {
            drawing::api::IBitmapPixels* raw{};
            if (bitmap->lockPixels(&raw, int32_t(drawing::BitmapLockFlags::ReadWrite)) == ReturnCode::Ok && raw)
                raw->release(); // the round trip through the lock format IS the quantization
        }
        return ReturnCode::Ok;
    }

    // ---- drawing -----------------------------------------------------------
    ReturnCode clear(const drawing::Color* clearColor) override
    {
        if (!(std::isfinite(clearColor->r) && std::isfinite(clearColor->g) &&
              std::isfinite(clearColor->b) && std::isfinite(clearColor->a)))
            return ReturnCode::Ok;

        auto& s = bitmap->surface;
        const auto clip = drawing::intersectRect(clipRectStack.back(), surfaceBounds());
        const int x0 = aliasedCoord(clip.left);
        const int x1 = aliasedCoord(clip.right);
        const int y0 = aliasedCoord(clip.top);
        const int y1 = aliasedCoord(clip.bottom);
        if (x1 <= x0 || y1 <= y0)
            return ReturnCode::Ok;

        const float a = clearColor->a;
        const float premultiplied[4] = { clearColor->r * a, clearColor->g * a, clearColor->b * a, a };
        const uint16_t pat[4] = {
            drawing::detail::floatToHalf(premultiplied[0]),
            drawing::detail::floatToHalf(premultiplied[1]),
            drawing::detail::floatToHalf(premultiplied[2]),
            drawing::detail::floatToHalf(premultiplied[3]) };
        uint64_t pat64;
        std::memcpy(&pat64, pat, 8);

        const auto* mask = activeClipMask();
        if (!mask)
        {
            for (int y = y0; y < y1; ++y)
            {
                uint16_t* row = s.row(y);
                for (int x = x0; x < x1; ++x)
                    std::memcpy(row + size_t(x) * 4, &pat64, 8);
            }
            return ReturnCode::Ok;
        }

        // Under a shaped clip, clear only replaces what the clip admits. It is
        // still a REPLACE rather than a blend — the clip's partial coverage is
        // what gets interpolated, so the shape's antialiased edge is preserved
        // instead of being squared off to its bounding box.
        for (int y = y0; y < y1; ++y)
        {
            uint16_t* row = s.row(y);
            for (int x = x0; x < x1; ++x)
            {
                const float m = mask->at(x, y);
                if (m <= 0.0f)
                    continue;
                uint16_t* p = row + size_t(x) * 4;
                if (m >= 1.0f)
                {
                    std::memcpy(p, &pat64, 8);
                    continue;
                }
                for (int c = 0; c < 4; ++c)
                {
                    const float dst = drawing::detail::halfToFloat(p[c]);
                    p[c] = drawing::detail::floatToHalf(dst + (premultiplied[c] - dst) * m);
                }
            }
        }
        return ReturnCode::Ok;
    }

    ReturnCode fillGeometry(drawing::api::IPathGeometry* pathGeometry, drawing::api::IBrush* brush, drawing::api::IBrush* opacityBrush) override
    {
        auto* path = dynamic_cast<PathGeometry*>(pathGeometry);
        auto* cpuBrush = dynamic_cast<CpuBrush*>(brush);
        if (!path || !cpuBrush)
            return ReturnCode::NoSupport; // e.g. bitmap brushes: milestone 6
        if (opacityBrush)
            return ReturnCode::NoSupport; // masked fills: fail loudly rather than render wrong

        return fillFigures(path->figures, path->fillMode, *cpuBrush);
    }

    ReturnCode drawGeometry(drawing::api::IPathGeometry* pathGeometry, drawing::api::IBrush* brush, float strokeWidth, drawing::api::IStrokeStyle* strokeStyle) override
    {
        auto* path = dynamic_cast<PathGeometry*>(pathGeometry);
        auto* cpuBrush = dynamic_cast<CpuBrush*>(brush);
        if (!path || !cpuBrush)
            return ReturnCode::NoSupport;
        if (!(strokeWidth > 0.0f) || !std::isfinite(strokeWidth))
            return ReturnCode::Ok; // D2D draws nothing for a zero-width stroke

        // Widen in local space (so a non-uniform transform yields an elliptical
        // pen, like D2D), then fill the union of the pieces.
        strokeFigures.clear();
        stroke::widen(path->figures, strokeWidth, strokeParams(strokeStyle), strokeFigures);
        return fillFigures(strokeFigures, drawing::FillMode::Winding, *cpuBrush);
    }

private:
    // Rasterize + blend a set of already-flattened figures. The single entry
    // point to pixels: both fillGeometry and drawGeometry come through here.
    ReturnCode fillFigures(const std::vector<Figure>& figures, drawing::FillMode fillMode, const CpuBrush& brush)
    {
        // A non-finite colour would poison even zero-coverage pixels through
        // the blend (NaN * 0 = NaN); render nothing instead.
        if (!brush.isPaintable())
            return ReturnCode::Ok;

        if (figures.empty())
            return ReturnCode::Ok;

        auto& s = bitmap->surface;

        // 1. Transform to device space; gather the bounding box.
        //    Figures containing a non-finite point (NaN/Inf inputs, or overflow
        //    from extreme coordinates x transform) are skipped entirely: they
        //    would poison the bbox and the winding sums.
        devicePoints.clear();
        figureValid.clear();
        float minX = 1e30f, minY = 1e30f, maxX = -1e30f, maxY = -1e30f;
        {
            size_t figStart = 0;
            for (const auto& fig : figures)
            {
                bool valid = fig.filled && !fig.points.empty();
                for (const auto& p : fig.points)
                {
                    const auto dp = drawing::transformPoint(transform_, p);
                    devicePoints.push_back(dp);
                    valid = valid && std::isfinite(dp.x) && std::isfinite(dp.y);
                }
                figureValid.push_back(valid ? 1 : 0);
                if (valid)
                {
                    for (size_t i = figStart; i < devicePoints.size(); ++i)
                    {
                        minX = (std::min)(minX, devicePoints[i].x);
                        minY = (std::min)(minY, devicePoints[i].y);
                        maxX = (std::max)(maxX, devicePoints[i].x);
                        maxY = (std::max)(maxY, devicePoints[i].y);
                    }
                }
                figStart = devicePoints.size();
            }
        }
        if (minX > maxX)
            return ReturnCode::Ok; // no valid figures

        // 2. Integer raster rect = bbox ∩ clip ∩ surface.
        //    Geometry bounds use floor/ceil (must cover the full antialiased
        //    extent); the clip contributes D2D ALIASED pixel-centre bounds.
        const auto clip = drawing::intersectRect(clipRectStack.back(), surfaceBounds());
        // Disjoint nested clips produce an inverted (empty) intersection;
        // std::clamp with lo > hi below would be undefined behaviour.
        if (!(clip.left < clip.right) || !(clip.top < clip.bottom))
            return ReturnCode::Ok;
        // Clamp the (possibly astronomically large, e.g. 1e30) bbox into the
        // clip rect in float BEFORE converting to int — float-to-int of an
        // out-of-range value is undefined behaviour.
        minX = std::clamp(minX, clip.left, clip.right);
        maxX = std::clamp(maxX, clip.left, clip.right);
        minY = std::clamp(minY, clip.top, clip.bottom);
        maxY = std::clamp(maxY, clip.top, clip.bottom);
        const int ix0 = (std::max)(int(std::floor(minX)), aliasedCoord(clip.left));
        const int iy0 = (std::max)(int(std::floor(minY)), aliasedCoord(clip.top));
        const int ix1 = (std::min)(int(std::ceil(maxX)), aliasedCoord(clip.right));
        const int iy1 = (std::min)(int(std::ceil(maxY)), aliasedCoord(clip.bottom));
        const int w = ix1 - ix0;
        const int h = iy1 - iy0;
        if (w <= 0 || h <= 0)
            return ReturnCode::Ok;

        // 3. Accumulate all figures' edges into the coverage-delta buffer,
        //    clipping each segment exactly at the raster bounds.
        const int rowStride = w + 2;
        accBuf.assign(size_t(rowStride) * h, 0.0f);

        size_t base = 0;
        size_t figIndex = 0;
        for (const auto& fig : figures)
        {
            const size_t n = fig.points.size();
            if (!figureValid[figIndex++])
            {
                base += n;
                continue;
            }
            for (size_t i = 0; i < n; ++i)
            {
                auto a = devicePoints[base + i];
                auto b = devicePoints[base + (i + 1) % n]; // implicit close for open figures
                a.x -= ix0;
                a.y -= iy0;
                b.x -= ix0;
                b.y -= iy0;
                raster::accumulateEdgeClipped(accBuf.data(), w, h, a, b);
            }
            base += n;
        }

        // 4. Per row: prefix-sum to coverage, ask the brush for the source
        //    colours, then blend. Premultiplied source, over with coverage:
        //    dst = src*cov + dst*(1 - srcA*cov)
        const bool nonzero = (fillMode == drawing::FillMode::Winding);
        const auto deviceToLocal = drawing::invert(transform_);

        // Chunk-aligned blend span; row padding absorbs the rounding (cov = 0 there).
        const int ax0 = ix0 & ~3;
        const int ax1 = (std::min)(s.stridePixels, (ix1 + 3) & ~3);
        const int spanPx = ax1 - ax0;

        covBuf.assign(size_t(s.stridePixels), 0.0f);
        rowBuf.resize(size_t(spanPx) * 4);
        srcBuf.resize(size_t(spanPx) * 4);

        for (int y = 0; y < h; ++y)
        {
            const float* acc = accBuf.data() + size_t(y) * rowStride;
            float* cov = covBuf.data();

            float run = 0.0f;
            for (int x = 0; x < w; ++x)
            {
                run += acc[x];
                cov[ix0 + x] = raster::foldCoverage(run, nonzero);
            }

            // An arbitrary-geometry clip is a coverage mask; intersecting is a
            // multiply, which also antialiases the clip edge.
            if (const auto* mask = activeClipMask())
                for (int x = 0; x < w; ++x)
                    cov[ix0 + x] *= mask->at(ix0 + x, iy0 + y);

            uint16_t* p = s.row(iy0 + y) + size_t(ax0) * 4;
            loadSpan(p, rowBuf.data(), spanPx * 4);
            brush.evalSpan(deviceToLocal, ax0, iy0 + y, spanPx, srcBuf.data());

            float* d = rowBuf.data();
            const float* sc = srcBuf.data();
            const float* cv = cov + ax0;
            for (int i = 0; i < spanPx; ++i, d += 4, sc += 4)
            {
                const float c = cv[i];
                if (c <= 0.0f)
                    continue;
                blendPixel(d, sc, c, gammaSpaceBlend);
            }

            storeSpan(rowBuf.data(), p, spanPx * 4);

            // reset the coverage cells we wrote
            std::memset(cov + ix0, 0, size_t(w) * sizeof(float));
        }

        return ReturnCode::Ok;
    }

public:
    ReturnCode fillRoundedRectangle(const drawing::RoundedRect* roundedRect, drawing::api::IBrush* brush) override
    {
        auto geometry = createRoundedRectGeometry(roundedRect);
        this->fillGeometry(drawing::AccessPtr::get(geometry), brush, nullptr);
        return ReturnCode::Ok;
    }

    ReturnCode drawRoundedRectangle(const drawing::RoundedRect* roundedRect, drawing::api::IBrush* brush, float strokeWidth, drawing::api::IStrokeStyle* strokeStyle) override
    {
        auto geometry = createRoundedRectGeometry(roundedRect);
        return this->drawGeometry(drawing::AccessPtr::get(geometry), brush, strokeWidth, strokeStyle);
    }

    ReturnCode drawTextU(const char* string, uint32_t stringLength, drawing::api::ITextFormat* textFormat,
                         const drawing::Rect* layoutRect, drawing::api::IBrush* brush, int32_t options) override
    {
        auto* engine = textEngine();
        if (!engine || !textFormat || !layoutRect)
            return ReturnCode::NoSupport;

        // The engine draws through this context's own public methods, so text
        // needs no special path to pixels.
        return engine->drawText(this, textFormat, string, int32_t(stringLength),
                                *layoutRect, brush, options);
    }

    ReturnCode drawRichTextU(drawing::api::IRichTextFormat* richTextFormat, const drawing::Rect* layoutRect,
                             drawing::api::IBrush* brush, int32_t options) override
    {
        auto* engine = textEngine();
        if (!engine || !richTextFormat || !layoutRect)
            return ReturnCode::NoSupport;

        return engine->drawRichText(this, richTextFormat, *layoutRect, brush, options);
    }

    // ---- target ------------------------------------------------------------
    ReturnCode getBitmap(drawing::api::IBitmap** returnBitmap) override
    {
        *returnBitmap = bitmap;
        bitmap->addRef();
        return ReturnCode::Ok;
    }

    ReturnCode getFactory(drawing::api::IFactory** returnFactory) override
    {
        *returnFactory = factory;
        return ReturnCode::Ok;
    }

    ReturnCode queryInterface(const gmpi::api::Guid* iid, void** returnInterface) override
    {
        *returnInterface = {};
        GMPI_QUERYINTERFACE(drawing::api::IBitmapRenderTarget);
        GMPI_QUERYINTERFACE(drawing::api::IDeviceContext);
        GMPI_QUERYINTERFACE(drawing::api::IResource);
        return ReturnCode::NoSupport;
    }
    GMPI_REFCOUNT;

private:
    drawing::PathGeometry createRoundedRectGeometry(const drawing::RoundedRect* rr)
    {
        drawing::Factory factoryWrapper;
        this->getFactory(drawing::AccessPtr::put(factoryWrapper));

        const auto& r = rr->rect;
        const float rx = (std::min)(rr->radiusX, (r.right - r.left) * 0.5f);
        const float ry = (std::min)(rr->radiusY, (r.bottom - r.top) * 0.5f);
        const drawing::Size radius{ rx, ry };

        auto geometry = factoryWrapper.createPathGeometry();
        auto sink = geometry.open();

        sink.beginFigure({ r.left + rx, r.top }, drawing::FigureBegin::Filled);
        sink.addLine({ r.right - rx, r.top });
        sink.addArc({ { r.right, r.top + ry }, radius });
        sink.addLine({ r.right, r.bottom - ry });
        sink.addArc({ { r.right - rx, r.bottom }, radius });
        sink.addLine({ r.left + rx, r.bottom });
        sink.addArc({ { r.left, r.bottom - ry }, radius });
        sink.addLine({ r.left, r.top + ry });
        sink.addArc({ { r.left + rx, r.top }, radius });
        sink.endFigure(drawing::FigureEnd::Closed);
        sink.close();

        return geometry;
    }
};

// ---------------------------------------------------------------------------
// Factory
// ---------------------------------------------------------------------------
class Factory final : public drawing::api::IFactory
{
public:
    // Decoding image files is the one job with no portable answer, so this
    // header stays free of platform code and takes a decoder instead. Wire it
    // up with helpers/DecodeImage.h:
    //
    //     factory.imageDecoder = gmpi::drawing::decodeImageFile;
    //
    // A host can equally supply its own (asset pipeline, in-memory cache, a
    // format we don't cover). Without one, loadImageU reports NoSupport.
    // Must return 8-bit sRGB RGBA with straight (non-premultiplied) alpha.
    std::function<bool(const std::filesystem::path&, drawing::DecodedImage&)> imageDecoder;

    ReturnCode createPathGeometry(drawing::api::IPathGeometry** returnPathGeometry) override
    {
        *returnPathGeometry = new PathGeometry(this);
        return ReturnCode::Ok;
    }

    // Text engine, injected like the image decoder so this header depends on
    // neither HarfBuzz nor any platform font API. helpers/CpuTextEngine.h
    // provides one; without it, text reports NoSupport.
    ICpuTextEngine* textEngine{};

    ReturnCode createTextFormat(const char* fontFamilyName, drawing::FontWeight fontWeight, drawing::FontStyle fontStyle,
                                drawing::FontStretch fontStretch, float fontHeight, int32_t fontFlags,
                                drawing::api::ITextFormat** returnTextFormat) override
    {
        *returnTextFormat = {};
        if (!textEngine)
            return ReturnCode::NoSupport;
        return textEngine->createTextFormat(fontFamilyName, fontWeight, fontStyle, fontStretch,
                                            fontHeight, fontFlags, returnTextFormat);
    }

    ReturnCode createImage(int32_t width, int32_t height, int32_t flags, drawing::api::IBitmap** returnBitmap) override
    {
        *returnBitmap = {};
        try
        {
            *returnBitmap = new Bitmap(this, width, height, flags);
        }
        catch (...)
        {
            return ReturnCode::Fail; // allocation failure must not cross the ABI
        }
        return ReturnCode::Ok;
    }

    // CONTRACT — BGRA_sRGB_8i means sRGB. One rule, no exceptions.
    //
    // A file-loaded bitmap locks as 32bpp premultiplied BGRA holding the file's
    // own sRGB bytes, passed through unchanged. That is what the DirectX backend
    // gives (WIC decodes a PNG to 32bppPBGRA and touches only the premultiply,
    // not the gamma) and what the format name and the bit-11 documentation in
    // GmpiApiDrawing.h both promise. Callers rely on it: gmpi_ui's own
    // helpers/ImageCache.cpp and SynthEditLib's ImageCache/ImageTinted2Gui all
    // branch on isSRGB() and then do sRGB-aware arithmetic on these bytes.
    //
    // An SRGBPixels RENDER TARGET now means the same thing. It used to be the
    // exception — same format, LINEAR bytes — because D2D rendered into a plain
    // B8G8R8A8_UNORM surface and then de-gammaed it on sample, which both
    // darkened a cached bitmap and spent the 8 bits linearly (a dark ramp kept
    // 14 of its 64 levels). Creating that target as _UNORM_SRGB instead removed
    // the exception, so provenance no longer decides anything and no
    // BGRA_Linear_8i split is needed. Only a Mask stays linear, because coverage
    // is not colour. Pinned by CpuVsD2D.LoadedImageLocksAsSRGB and
    // CpuVsD2D.SRGBRenderTargetRoundTripIsFaithful.
    ReturnCode loadImageU(const char* uri, drawing::api::IBitmap** returnBitmap) override
    {
        *returnBitmap = {};
        if (!imageDecoder || !uri)
            return ReturnCode::NoSupport;

        // The decoder is caller-supplied, so contain it: an exception must not
        // cross this C ABI boundary.
        drawing::DecodedImage decoded;
        gmpi::shared_ptr<Bitmap> bitmap;
        try
        {
            if (!imageDecoder(std::filesystem::path(uri), decoded) || !decoded)
                return ReturnCode::Fail;
            // SRGBPixels so lockPixels() presents 32bpp BGRA, matching what the
            // DirectX backend gives for a loaded image (WIC 32bppPBGRA). Only the
            // lock format is affected; rendering stays fp16 either way. Callers
            // that lock a file-loaded bitmap — including every legacy SDK3 module,
            // which hard-codes 4 bytes per pixel — would otherwise get a 64bpp
            // half-float buffer and walk off the end of it.
            bitmap.attach(new Bitmap(this, int32_t(decoded.width), int32_t(decoded.height),
                                     int32_t(drawing::BitmapRenderTargetFlags::SRGBPixels)));
        }
        catch (...)
        {
            return ReturnCode::Fail;
        }

        // 8-bit sRGB straight -> fp16 linear premultiplied, the one format the
        // renderer works in.
        auto& s = bitmap->surface;
        for (uint32_t y = 0; y < decoded.height; ++y)
        {
            const uint8_t* src = decoded.pixels.data() + size_t(y) * decoded.width * 4;
            uint16_t* dst = s.row(int32_t(y));
            for (uint32_t x = 0; x < decoded.width; ++x, src += 4, dst += 4)
            {
                const float a = src[3] * (1.0f / 255.0f);
                dst[0] = drawing::detail::floatToHalf(drawing::SRGBPixelToLinear(src[0]) * a);
                dst[1] = drawing::detail::floatToHalf(drawing::SRGBPixelToLinear(src[1]) * a);
                dst[2] = drawing::detail::floatToHalf(drawing::SRGBPixelToLinear(src[2]) * a);
                dst[3] = drawing::detail::floatToHalf(a);
            }
        }

        *returnBitmap = bitmap.get();
        bitmap->addRef();
        return ReturnCode::Ok;
    }

    ReturnCode createStrokeStyle(const drawing::StrokeStyleProperties* strokeStyleProperties, const float* dashes, int32_t dashesCount, drawing::api::IStrokeStyle** returnStrokeStyle) override
    {
        *returnStrokeStyle = new se::generic_graphics::StrokeStyle(this, strokeStyleProperties, dashes, dashesCount);
        return ReturnCode::Ok;
    }

    ReturnCode getFontFamilyName(int32_t, gmpi::api::IString*) override
    {
        return ReturnCode::NoSupport; // milestone 8 (text)
    }

    ReturnCode getPlatformPixelFormat(int32_t* returnPixelFormat) override
    {
        // The factory's own answer: what an unflagged target locks as.
        *returnPixelFormat = drawing::api::IBitmapPixels::RGBA_16f;
        return ReturnCode::Ok;
    }

    ReturnCode createRichTextFormat(const char* markdownText, float fontHeight, const char* fontFamilyName,
                                    int32_t fontFlags, drawing::TextAlignment textAlignment,
                                    drawing::ParagraphAlignment paragraphAlignment,
                                    drawing::WordWrapping wordWrapping, float lineSpacing, float baseline,
                                    drawing::api::IRichTextFormat** returnRichTextFormat) override
    {
        *returnRichTextFormat = {};
        if (!textEngine)
            return ReturnCode::NoSupport;

        return textEngine->createRichTextFormat(markdownText, fontHeight, fontFamilyName, fontFlags,
                                                textAlignment, paragraphAlignment, wordWrapping,
                                                lineSpacing, baseline, returnRichTextFormat);
    }

    ReturnCode createCpuRenderTarget(drawing::SizeU size, int32_t flags, drawing::api::IBitmapRenderTarget** returnBitmapRenderTarget, float dpi) override
    {
        *returnBitmapRenderTarget = {};

        try
        {
            *returnBitmapRenderTarget = new RenderTarget(this, size, flags, dpi); // refcount born at 1; caller owns it
        }
        catch (...)
        {
            return ReturnCode::Fail; // allocation failure must not cross the ABI
        }
        return ReturnCode::Ok;
    }

    GMPI_QUERYINTERFACE_METHOD(drawing::api::IFactory);
    GMPI_REFCOUNT_NO_DELETE; // typically owned on the stack by the host
};

inline ICpuTextEngine* RenderTarget::textEngine() const
{
    auto* cpuFactory = dynamic_cast<Factory*>(factory);
    return cpuFactory ? cpuFactory->textEngine : nullptr;
}

} // namespace cpugfx
} // namespace gmpi
