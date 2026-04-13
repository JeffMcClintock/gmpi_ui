#pragma once
#include "GmpiUiDrawing.h"
#include "BitmapMask.h"  // for detail::halfToFloat

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <wincodec.h>
#include "backends/DirectXGfx.h"  // for gmpi::directx::ComPtr
#endif

#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#include <ImageIO/ImageIO.h>
#endif

namespace gmpi { namespace drawing {

namespace detail {

// Linear float [0,1] → sRGB uint8_t [0,255].
inline uint8_t linearToSRGB_f(float c)
{
    c = std::clamp(c, 0.0f, 1.0f);
    float s = (c <= 0.0031308f)
        ? c * 12.92f
        : 1.055f * std::pow(c, 1.0f / 2.4f) - 0.055f;
    return static_cast<uint8_t>(std::clamp(s * 255.0f + 0.5f, 0.0f, 255.0f));
}

#ifdef _WIN32
// Linear uint8_t [0,255] → sRGB uint8_t [0,255].
inline uint8_t linearToSRGB(uint8_t v)
{
    return linearToSRGB_f(v / 255.0f);
}
#endif

} // namespace detail

// Save a GMPI Bitmap to a PNG file.
//
// REQUIREMENT: The bitmap must have been created with the CpuReadable flag
// (e.g. BitmapRenderTargetFlags::CpuReadable), so that lockPixels() can
// return a valid CPU-accessible pointer.
//
// Handles both render-target pixel formats:
//   Windows: 64bppPRGBAHalf (8 bytes/pixel) and 32bppPBGRA (4 bytes/pixel)
//   macOS:   128bpp float (16 bytes/pixel)  and 32bpp (4 bytes/pixel)
// Output is always a 32bpp sRGB PNG.
//
// Returns true on success.

#ifdef _WIN32
inline bool savePng(const std::filesystem::path& path, Bitmap& bitmap)
{
    std::filesystem::create_directories(path.parent_path());

    auto pixels = bitmap.lockPixels(BitmapLockFlags::Read);
    if (!pixels)
        return false;

    const uint8_t* srcData = pixels.getAddress();
    const int32_t  srcBpr  = pixels.getBytesPerRow();
    const SizeU    size    = bitmap.getSize();
    const int32_t  srcBpp  = srcBpr / static_cast<int32_t>(size.width); // 4 or 8

    // Output is always 32bpp BGRA sRGB.
    const int32_t outBpr = static_cast<int32_t>(size.width) * 4;
    std::vector<uint8_t> srgbData(static_cast<size_t>(outBpr) * size.height);

    for (uint32_t y = 0; y < size.height; ++y)
    {
        const uint8_t* src = srcData + y * srcBpr;
        uint8_t*       dst = srgbData.data() + y * outBpr;

        for (uint32_t x = 0; x < size.width; ++x)
        {
            uint8_t r, g, b, a;

            if (srcBpp == 8)
            {
                // 64bppPRGBAHalf: linear premultiplied RGBA half-float.
                const uint16_t* h = reinterpret_cast<const uint16_t*>(src);
                float fr = detail::halfToFloat(h[0]);
                float fg = detail::halfToFloat(h[1]);
                float fb = detail::halfToFloat(h[2]);
                float fa = detail::halfToFloat(h[3]);

                a = static_cast<uint8_t>(std::clamp(fa * 255.0f + 0.5f, 0.0f, 255.0f));
                if (fa > 0.0f)
                {
                    fr = std::clamp(fr / fa, 0.0f, 1.0f);
                    fg = std::clamp(fg / fa, 0.0f, 1.0f);
                    fb = std::clamp(fb / fa, 0.0f, 1.0f);
                }
                else { fr = fg = fb = 0.0f; }

                r = detail::linearToSRGB_f(fr);
                g = detail::linearToSRGB_f(fg);
                b = detail::linearToSRGB_f(fb);
            }
            else
            {
                // 32bppPBGRA: linear premultiplied BGRA 8-bit.
                a = src[3];
                if (a == 0)
                {
                    dst[0] = dst[1] = dst[2] = dst[3] = 0;
                    src += 4; dst += 4;
                    continue;
                }
                float fa = a / 255.0f;
                b = detail::linearToSRGB(static_cast<uint8_t>(std::clamp(src[0] / fa + 0.5f, 0.0f, 255.0f)));
                g = detail::linearToSRGB(static_cast<uint8_t>(std::clamp(src[1] / fa + 0.5f, 0.0f, 255.0f)));
                r = detail::linearToSRGB(static_cast<uint8_t>(std::clamp(src[2] / fa + 0.5f, 0.0f, 255.0f)));
            }

            // Store sRGB BGRA, re-premultiplied.
            if (a == 255)
            {
                dst[0] = b; dst[1] = g; dst[2] = r; dst[3] = 255;
            }
            else
            {
                float fa = a / 255.0f;
                dst[0] = static_cast<uint8_t>(b * fa + 0.5f);
                dst[1] = static_cast<uint8_t>(g * fa + 0.5f);
                dst[2] = static_cast<uint8_t>(r * fa + 0.5f);
                dst[3] = a;
            }
            src += srcBpp;
            dst += 4;
        }
    }

    // Encode to PNG via WIC.
    IWICImagingFactory* rawWic{};
    CoCreateInstance(
        CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
        __uuidof(IWICImagingFactory), reinterpret_cast<void**>(&rawWic));
    gmpi::directx::ComPtr<IWICImagingFactory> wic(rawWic);
    if (!wic) return false;

    IWICStream* rawStream{};
    wic->CreateStream(&rawStream);
    gmpi::directx::ComPtr<IWICStream> stream(rawStream);
    if (!stream) return false;
    stream->InitializeFromFilename(path.wstring().c_str(), GENERIC_WRITE);

    IWICBitmapEncoder* rawEncoder{};
    wic->CreateEncoder(GUID_ContainerFormatPng, nullptr, &rawEncoder);
    gmpi::directx::ComPtr<IWICBitmapEncoder> encoder(rawEncoder);
    if (!encoder) return false;
    encoder->Initialize(stream, WICBitmapEncoderNoCache);

    IWICBitmapFrameEncode* rawFrame{};
    IPropertyBag2* props{};
    encoder->CreateNewFrame(&rawFrame, &props);
    gmpi::directx::ComPtr<IWICBitmapFrameEncode> frame(rawFrame);
    if (props) props->Release();
    if (!frame) return false;

    frame->Initialize(nullptr);
    frame->SetSize(size.width, size.height);
    WICPixelFormatGUID fmt = GUID_WICPixelFormat32bppPBGRA;
    frame->SetPixelFormat(&fmt);
    frame->WritePixels(size.height, outBpr, static_cast<UINT>(srgbData.size()), srgbData.data());
    frame->Commit();
    encoder->Commit();

    return true;
}
#endif // _WIN32

#ifdef __APPLE__
inline bool savePng(const std::filesystem::path& path, Bitmap& bitmap)
{
    std::filesystem::create_directories(path.parent_path());

    auto pixels = bitmap.lockPixels(BitmapLockFlags::Read);
    if (!pixels)
        return false;

    uint8_t* data = pixels.getAddress();
    int32_t  bpr  = pixels.getBytesPerRow();
    SizeU    size = bitmap.getSize();
    int32_t  bpp  = bpr / static_cast<int32_t>(size.width);

    // If float-linear (128bpp), convert to 8-bit sRGB RGBA for PNG output.
    std::vector<uint8_t> srgbBuf;
    int32_t  srgbBpr = bpr;
    uint8_t* pngData = data;

    if (bpp == 16)
    {
        srgbBpr = static_cast<int32_t>(size.width) * 4;
        srgbBuf.resize(srgbBpr * size.height);
        for (uint32_t y = 0; y < size.height; ++y)
        {
            for (uint32_t x = 0; x < size.width; ++x)
            {
                const float* f = reinterpret_cast<const float*>(data + y * bpr + x * 16);
                float fr = f[0], fg = f[1], fb = f[2], fa = f[3];
                uint8_t a = static_cast<uint8_t>(std::clamp(fa * 255.0f + 0.5f, 0.0f, 255.0f));
                if (fa > 0.0f)
                {
                    fr = std::clamp(fr / fa, 0.0f, 1.0f);
                    fg = std::clamp(fg / fa, 0.0f, 1.0f);
                    fb = std::clamp(fb / fa, 0.0f, 1.0f);
                }
                else { fr = fg = fb = 0.0f; }

                uint8_t* dst = srgbBuf.data() + y * srgbBpr + x * 4;
                // Re-premultiply in sRGB for PNG.
                float aNorm = a / 255.0f;
                dst[0] = static_cast<uint8_t>(detail::linearToSRGB_f(fr) * aNorm + 0.5f);
                dst[1] = static_cast<uint8_t>(detail::linearToSRGB_f(fg) * aNorm + 0.5f);
                dst[2] = static_cast<uint8_t>(detail::linearToSRGB_f(fb) * aNorm + 0.5f);
                dst[3] = a;
            }
        }
        pngData = srgbBuf.data();
    }

    CGColorSpaceRef colorSpace = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
    CGContextRef ctx = CGBitmapContextCreate(
        pngData, size.width, size.height, 8, srgbBpr,
        colorSpace,
        kCGImageAlphaPremultipliedLast); // RGBA premultiplied
    CGColorSpaceRelease(colorSpace);
    if (!ctx) return false;

    CGImageRef image = CGBitmapContextCreateImage(ctx);
    CGContextRelease(ctx);
    if (!image) return false;

    CFStringRef cfPath = CFStringCreateWithCString(kCFAllocatorDefault, path.c_str(), kCFStringEncodingUTF8);
    CFURLRef url = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, cfPath, kCFURLPOSIXPathStyle, false);
    CFRelease(cfPath);
    CGImageDestinationRef dest = CGImageDestinationCreateWithURL(url, CFSTR("public.png"), 1, nullptr);
    CFRelease(url);
    if (!dest) { CGImageRelease(image); return false; }

    CGImageDestinationAddImage(dest, image, nullptr);
    bool ok = CGImageDestinationFinalize(dest);
    CFRelease(dest);
    CGImageRelease(image);

    return ok;
}
#endif // __APPLE__

}} // namespace gmpi::drawing
