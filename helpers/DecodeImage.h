#pragma once

/*
#include "helpers/DecodeImage.h"
*/

// Decoding an image file is the one part of the software renderer that has no
// portable answer, so it lives here rather than in the backend. Every platform
// implements the same single entry point and returns the same single format:
//
//     decodeImageFile(path, DecodedImage&) -> bool
//     DecodedImage::pixels = 8-bit sRGB RGBA, straight (non-premultiplied)
//
// backends/CpuGfx.h never includes this header. Wire it up explicitly:
//
//     gmpi::cpugfx::Factory factory;
//     factory.imageDecoder = gmpi::drawing::decodeImageFile;
//
// which keeps the backend free of platform code and lets a host substitute its
// own decoder (an asset pipeline, an in-memory cache, a format we don't cover).

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <vector>

#include "DecodedImage.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#undef  NOMINMAX
#define NOMINMAX
#include <Windows.h>
#include <wincodec.h>
#include "backends/DirectXGfx.h" // gmpi::directx::ComPtr
#endif

#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#include <ImageIO/ImageIO.h>
#endif

// Elsewhere (e.g. Linux), fall back to JUCE when the including target can see
// it — the same arrangement helpers/SavePng.h uses for encoding.
#if !defined(_WIN32) && !defined(__APPLE__)
#if __has_include(<JuceHeader.h>) || __has_include(<juce_gui_basics/juce_gui_basics.h>)
#define GMPI_UI_DECODE_IMAGE_JUCE 1
#include "backends/JuceGfx.h"
#endif
#endif

namespace gmpi { namespace drawing {

// Defined when this translation unit actually has a decodeImageFile() to call.
// Not every target does — the fallback needs JUCE headers in scope.
#if defined(_WIN32) || defined(__APPLE__) || GMPI_UI_DECODE_IMAGE_JUCE
#define GMPI_UI_HAVE_IMAGE_DECODER 1
#endif

#ifdef _WIN32
inline bool decodeImageFile(const std::filesystem::path& path, DecodedImage& returnImage)
{
    returnImage = {};

    // Requires COM to be initialised on this thread.
    gmpi::directx::ComPtr<IWICImagingFactory> wic;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                __uuidof(IWICImagingFactory), wic.put_void())) || !wic)
        return false;

    gmpi::directx::ComPtr<IWICBitmapDecoder> decoder;
    if (FAILED(wic->CreateDecoderFromFilename(path.wstring().c_str(), nullptr, GENERIC_READ,
                                              WICDecodeMetadataCacheOnLoad, decoder.put())) || !decoder)
        return false;

    gmpi::directx::ComPtr<IWICBitmapFrameDecode> frame;
    if (FAILED(decoder->GetFrame(0, frame.put())) || !frame)
        return false;

    // Normalise whatever the file held into our one interchange format.
    gmpi::directx::ComPtr<IWICFormatConverter> converter;
    if (FAILED(wic->CreateFormatConverter(converter.put())) || !converter)
        return false;
    if (FAILED(converter->Initialize(frame, GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone,
                                     nullptr, 0.0, WICBitmapPaletteTypeCustom)))
        return false;

    UINT w{}, h{};
    if (FAILED(converter->GetSize(&w, &h)) || w == 0 || h == 0)
        return false;

    const UINT stride = w * 4;
    std::vector<uint8_t> pixels(size_t(stride) * h);
    if (FAILED(converter->CopyPixels(nullptr, stride, static_cast<UINT>(pixels.size()), pixels.data())))
        return false;

    returnImage.width = w;
    returnImage.height = h;
    returnImage.pixels = std::move(pixels);
    return true;
}
#endif // _WIN32

#ifdef __APPLE__
inline bool decodeImageFile(const std::filesystem::path& path, DecodedImage& returnImage)
{
    returnImage = {};

    CFStringRef cfPath = CFStringCreateWithCString(kCFAllocatorDefault, path.c_str(), kCFStringEncodingUTF8);
    if (!cfPath)
        return false;
    CFURLRef url = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, cfPath, kCFURLPOSIXPathStyle, false);
    CFRelease(cfPath);
    if (!url)
        return false;

    CGImageSourceRef sourceRef = CGImageSourceCreateWithURL(url, nullptr);
    CFRelease(url);
    if (!sourceRef)
        return false;

    CGImageRef image = CGImageSourceCreateImageAtIndex(sourceRef, 0, nullptr);
    CFRelease(sourceRef);
    if (!image)
        return false;

    const size_t w = CGImageGetWidth(image);
    const size_t h = CGImageGetHeight(image);
    if (w == 0 || h == 0)
    {
        CGImageRelease(image);
        return false;
    }

    // Draw into a known layout rather than trusting the file's own.
    std::vector<uint8_t> pixels(w * h * 4);
    CGColorSpaceRef colorSpace = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
    CGContextRef ctx = CGBitmapContextCreate(pixels.data(), w, h, 8, w * 4, colorSpace,
                                             kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big);
    CGColorSpaceRelease(colorSpace);
    if (!ctx)
    {
        CGImageRelease(image);
        return false;
    }

    CGContextDrawImage(ctx, CGRectMake(0, 0, CGFloat(w), CGFloat(h)), image);
    CGContextRelease(ctx);
    CGImageRelease(image);

    // CoreGraphics has no straight-alpha RGBA8 context, so un-premultiply to
    // match the contract every platform here returns.
    for (size_t i = 0; i < pixels.size(); i += 4)
    {
        const uint8_t a = pixels[i + 3];
        if (a != 0 && a != 255)
        {
            for (int c = 0; c < 3; ++c)
                pixels[i + c] = static_cast<uint8_t>((std::min)(255, (pixels[i + c] * 255 + a / 2) / a));
        }
    }

    returnImage.width = static_cast<uint32_t>(w);
    returnImage.height = static_cast<uint32_t>(h);
    returnImage.pixels = std::move(pixels);
    return true;
}
#endif // __APPLE__

#if GMPI_UI_DECODE_IMAGE_JUCE
inline bool decodeImageFile(const std::filesystem::path& path, DecodedImage& returnImage)
{
    returnImage = {};

    const juce::File file(juce::String::fromUTF8(std::filesystem::absolute(path).c_str()));
    auto image = juce::ImageFileFormat::loadFrom(file);
    if (image.isNull())
        return false;

    const int w = image.getWidth();
    const int h = image.getHeight();
    if (w <= 0 || h <= 0)
        return false;

    image = image.convertedToFormat(juce::Image::ARGB);
    const juce::Image::BitmapData src(image, juce::Image::BitmapData::readOnly);

    std::vector<uint8_t> pixels(size_t(w) * h * 4);
    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            // getPixelColour un-premultiplies for us.
            const auto c = src.getPixelColour(x, y);
            uint8_t* d = pixels.data() + (size_t(y) * w + x) * 4;
            d[0] = c.getRed();
            d[1] = c.getGreen();
            d[2] = c.getBlue();
            d[3] = c.getAlpha();
        }
    }

    returnImage.width = static_cast<uint32_t>(w);
    returnImage.height = static_cast<uint32_t>(h);
    returnImage.pixels = std::move(pixels);
    return true;
}
#endif // GMPI_UI_DECODE_IMAGE_JUCE

}} // namespace gmpi::drawing
