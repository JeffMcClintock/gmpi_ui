#pragma once

/*
#include "helpers/FontProvider.h"
*/

// Finding a font is the one part of text with no portable answer, so it lives
// here rather than in the backend. Every platform implements the same single
// entry point and returns the same thing:
//
//     findFont(FontRequest, FontData&) -> bool
//
// backends/CpuGfx.h never includes this header. Wire it up explicitly:
//
//     gmpi::cpugfx::Factory factory;
//     textEngine.fontProvider = gmpi::drawing::findFont;
//
// which keeps the backend free of platform code and lets a host substitute its
// own (bundled fonts shipped with a plugin, an asset pipeline, a test fixture
// that wants one deterministic font on every machine).

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

#include "FontFile.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#undef  NOMINMAX
#define NOMINMAX
#include <Windows.h>
#include <dwrite.h>
#include "backends/DirectXGfx.h" // gmpi::directx::ComPtr
#endif

#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#include <CoreText/CoreText.h>
#endif

#if !defined(_WIN32) && !defined(__APPLE__)
#if __has_include(<fontconfig/fontconfig.h>)
#define GMPI_UI_FONT_PROVIDER_FONTCONFIG 1
#include <fontconfig/fontconfig.h>
#endif
#endif

namespace gmpi { namespace drawing {

// Defined when this translation unit actually has a findFont() to call.
#if defined(_WIN32) || defined(__APPLE__) || GMPI_UI_FONT_PROVIDER_FONTCONFIG
#define GMPI_UI_HAVE_FONT_PROVIDER 1
#endif

namespace detail {

inline bool readWholeFile(const std::string& path, std::vector<uint8_t>& returnBytes)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file)
        return false;
    const auto size = file.tellg();
    if (size <= 0)
        return false;
    returnBytes.resize(static_cast<size_t>(size));
    file.seekg(0);
    return static_cast<bool>(file.read(reinterpret_cast<char*>(returnBytes.data()), size));
}

} // namespace detail

#ifdef _WIN32
inline bool findFont(const FontRequest& request, FontData& returnFont)
{
    returnFont = {};

    gmpi::directx::ComPtr<IDWriteFactory> writeFactory;
    if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                                   reinterpret_cast<IUnknown**>(writeFactory.put()))) || !writeFactory)
        return false;

    // "system-ui" means the OS UI font, resolved the same way the Direct2D
    // backend resolves it, so both backends pick the same face.
    std::wstring familyName;
    if (_stricmp(request.familyName.c_str(), "system-ui") == 0)
    {
        NONCLIENTMETRICSW ncm{};
        ncm.cbSize = sizeof(ncm);
        if (SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0))
            familyName = ncm.lfMessageFont.lfFaceName;
        else
            familyName = L"Segoe UI";
    }
    else
    {
        const int needed = MultiByteToWideChar(CP_UTF8, 0, request.familyName.c_str(), -1, nullptr, 0);
        familyName.resize(needed > 0 ? needed - 1 : 0);
        if (needed > 1)
            MultiByteToWideChar(CP_UTF8, 0, request.familyName.c_str(), -1, familyName.data(), needed);
    }

    gmpi::directx::ComPtr<IDWriteFontCollection> collection;
    if (FAILED(writeFactory->GetSystemFontCollection(collection.put())) || !collection)
        return false;

    UINT32 index{};
    BOOL exists{};
    if (FAILED(collection->FindFamilyName(familyName.c_str(), &index, &exists)) || !exists)
        return false;

    gmpi::directx::ComPtr<IDWriteFontFamily> family;
    if (FAILED(collection->GetFontFamily(index, family.put())) || !family)
        return false;

    gmpi::directx::ComPtr<IDWriteFont> font;
    if (FAILED(family->GetFirstMatchingFont(static_cast<DWRITE_FONT_WEIGHT>(request.weight),
                                            static_cast<DWRITE_FONT_STRETCH>(request.stretch),
                                            static_cast<DWRITE_FONT_STYLE>(request.style),
                                            font.put())) || !font)
        return false;

    gmpi::directx::ComPtr<IDWriteFontFace> face;
    if (FAILED(font->CreateFontFace(face.put())) || !face)
        return false;

    UINT32 fileCount{ 1 };
    IDWriteFontFile* rawFile{};
    if (FAILED(face->GetFiles(&fileCount, &rawFile)) || fileCount == 0 || !rawFile)
        return false;
    gmpi::directx::ComPtr<IDWriteFontFile> fontFile(rawFile);

    // Read the bytes through DirectWrite's own loader rather than assuming a
    // filesystem path: a font may come from memory or a custom collection.
    const void* referenceKey{};
    UINT32 referenceKeySize{};
    gmpi::directx::ComPtr<IDWriteFontFileLoader> loader;
    if (FAILED(fontFile->GetReferenceKey(&referenceKey, &referenceKeySize)) ||
        FAILED(fontFile->GetLoader(loader.put())) || !loader)
        return false;

    gmpi::directx::ComPtr<IDWriteFontFileStream> stream;
    if (FAILED(loader->CreateStreamFromKey(referenceKey, referenceKeySize, stream.put())) || !stream)
        return false;

    UINT64 fileSize{};
    if (FAILED(stream->GetFileSize(&fileSize)) || fileSize == 0)
        return false;

    const void* fragment{};
    void* context{};
    if (FAILED(stream->ReadFileFragment(&fragment, 0, fileSize, &context)) || !fragment)
        return false;

    returnFont.bytes.assign(static_cast<const uint8_t*>(fragment),
                            static_cast<const uint8_t*>(fragment) + fileSize);
    stream->ReleaseFileFragment(context);

    returnFont.faceIndex = face->GetIndex();
    returnFont.resolvedName = request.familyName;
    return true;
}
#endif // _WIN32

#ifdef __APPLE__
inline bool findFont(const FontRequest& request, FontData& returnFont)
{
    returnFont = {};

    std::string family = request.familyName;
    if (family == "system-ui")
        family = ".AppleSystemUIFont";

    CFStringRef name = CFStringCreateWithCString(kCFAllocatorDefault, family.c_str(), kCFStringEncodingUTF8);
    if (!name)
        return false;

    // Ask for the traits we want; CoreText picks the closest face in the family.
    CFMutableDictionaryRef traits = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    const float weight = (float(request.weight) - 400.0f) / 400.0f; // CoreText uses -1..1
    CFNumberRef weightNumber = CFNumberCreate(kCFAllocatorDefault, kCFNumberFloatType, &weight);
    CFDictionarySetValue(traits, kCTFontWeightTrait, weightNumber);
    CFRelease(weightNumber);
    if (request.style != FontStyle::Normal)
    {
        const float slant = 0.05f;
        CFNumberRef slantNumber = CFNumberCreate(kCFAllocatorDefault, kCFNumberFloatType, &slant);
        CFDictionarySetValue(traits, kCTFontSlantTrait, slantNumber);
        CFRelease(slantNumber);
    }

    CFMutableDictionaryRef attributes = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(attributes, kCTFontFamilyNameAttribute, name);
    CFDictionarySetValue(attributes, kCTFontTraitsAttribute, traits);
    CFRelease(traits);
    CFRelease(name);

    CTFontDescriptorRef descriptor = CTFontDescriptorCreateWithAttributes(attributes);
    CFRelease(attributes);
    if (!descriptor)
        return false;

    CTFontRef font = CTFontCreateWithFontDescriptor(descriptor, 12.0, nullptr);
    CFRelease(descriptor);
    if (!font)
        return false;

    CFURLRef url = static_cast<CFURLRef>(CTFontCopyAttribute(font, kCTFontURLAttribute));
    CFRelease(font);
    if (!url)
        return false;

    char pathBuffer[1024]{};
    const bool gotPath = CFURLGetFileSystemRepresentation(
        url, true, reinterpret_cast<UInt8*>(pathBuffer), sizeof(pathBuffer));
    CFRelease(url);
    if (!gotPath)
        return false;

    if (!detail::readWholeFile(pathBuffer, returnFont.bytes))
        return false;
    returnFont.resolvedName = request.familyName;
    return true;
}
#endif // __APPLE__

#if GMPI_UI_FONT_PROVIDER_FONTCONFIG
inline bool findFont(const FontRequest& request, FontData& returnFont)
{
    returnFont = {};

    if (!FcInit())
        return false;

    FcPattern* pattern = FcPatternCreate();
    if (!pattern)
        return false;

    const std::string family = (request.familyName == "system-ui") ? "sans-serif" : request.familyName;
    FcPatternAddString(pattern, FC_FAMILY, reinterpret_cast<const FcChar8*>(family.c_str()));

    // fontconfig's weight scale is its own; map the common stops.
    int fcWeight = FC_WEIGHT_REGULAR;
    const int w = static_cast<int>(request.weight);
    if (w >= 900)      fcWeight = FC_WEIGHT_BLACK;
    else if (w >= 700) fcWeight = FC_WEIGHT_BOLD;
    else if (w >= 600) fcWeight = FC_WEIGHT_DEMIBOLD;
    else if (w >= 500) fcWeight = FC_WEIGHT_MEDIUM;
    else if (w <= 200) fcWeight = FC_WEIGHT_EXTRALIGHT;
    else if (w <= 300) fcWeight = FC_WEIGHT_LIGHT;
    FcPatternAddInteger(pattern, FC_WEIGHT, fcWeight);
    FcPatternAddInteger(pattern, FC_SLANT,
        request.style == FontStyle::Normal ? FC_SLANT_ROMAN : FC_SLANT_ITALIC);

    // Fallback: ask for a font that actually covers the codepoint.
    FcCharSet* charSet{};
    if (request.mustCoverCodepoint != 0)
    {
        charSet = FcCharSetCreate();
        FcCharSetAddChar(charSet, request.mustCoverCodepoint);
        FcPatternAddCharSet(pattern, FC_CHARSET, charSet);
    }

    FcConfigSubstitute(nullptr, pattern, FcMatchPattern);
    FcDefaultSubstitute(pattern);

    FcResult result{};
    FcPattern* matched = FcFontMatch(nullptr, pattern, &result);
    FcPatternDestroy(pattern);
    if (charSet)
        FcCharSetDestroy(charSet);
    if (!matched)
        return false;

    FcChar8* file{};
    int faceIndex{};
    const bool gotFile = FcPatternGetString(matched, FC_FILE, 0, &file) == FcResultMatch && file;
    FcPatternGetInteger(matched, FC_INDEX, 0, &faceIndex);

    std::string path = gotFile ? reinterpret_cast<const char*>(file) : std::string{};
    FcPatternDestroy(matched);
    if (path.empty())
        return false;

    if (!detail::readWholeFile(path, returnFont.bytes))
        return false;
    returnFont.faceIndex = static_cast<uint32_t>((std::max)(0, faceIndex));
    returnFont.resolvedName = request.familyName;
    return true;
}
#endif // GMPI_UI_FONT_PROVIDER_FONTCONFIG

}} // namespace gmpi::drawing
