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
#include <dwrite_2.h> // IDWriteFactory2 / IDWriteFontFallback, for font fallback
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

#ifdef _WIN32
// A text source holding exactly one string, which is all
// IDWriteFontFallback::MapCharacters needs to answer "which font covers this".
class SingleStringTextSource final : public IDWriteTextAnalysisSource
{
    const wchar_t* text_;
    UINT32 length_;
    ULONG refCount_{ 1 };

public:
    SingleStringTextSource(const wchar_t* text, UINT32 length) : text_(text), length_(length) {}

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** object) override
    {
        if (iid == __uuidof(IUnknown) || iid == __uuidof(IDWriteTextAnalysisSource))
        {
            *object = this;
            AddRef();
            return S_OK;
        }
        *object = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++refCount_; }
    ULONG STDMETHODCALLTYPE Release() override { return --refCount_; }

    HRESULT STDMETHODCALLTYPE GetTextAtPosition(UINT32 position, const WCHAR** text, UINT32* length) override
    {
        if (position >= length_) { *text = nullptr; *length = 0; return S_OK; }
        *text = text_ + position;
        *length = length_ - position;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetTextBeforePosition(UINT32 position, const WCHAR** text, UINT32* length) override
    {
        if (position == 0 || position > length_) { *text = nullptr; *length = 0; return S_OK; }
        *text = text_;
        *length = position;
        return S_OK;
    }
    DWRITE_READING_DIRECTION STDMETHODCALLTYPE GetParagraphReadingDirection() override
    {
        return DWRITE_READING_DIRECTION_LEFT_TO_RIGHT;
    }
    HRESULT STDMETHODCALLTYPE GetLocaleName(UINT32, UINT32* length, const WCHAR** localeName) override
    {
        *length = length_;
        *localeName = nullptr;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetNumberSubstitution(UINT32, UINT32* length, IDWriteNumberSubstitution** substitution) override
    {
        *length = length_;
        *substitution = nullptr;
        return S_OK;
    }
};

inline bool mapFallbackFont(IDWriteFactory* writeFactory, const std::wstring& baseFamily,
                            const FontRequest& request, gmpi::directx::ComPtr<IDWriteFont>& returnFont)
{
    gmpi::directx::ComPtr<IDWriteFactory2> factory2;
    if (FAILED(writeFactory->QueryInterface(__uuidof(IDWriteFactory2),
                                            reinterpret_cast<void**>(factory2.put_void()))) || !factory2)
        return false;

    gmpi::directx::ComPtr<IDWriteFontFallback> fallback;
    if (FAILED(factory2->GetSystemFontFallback(fallback.put())) || !fallback)
        return false;

    // The codepoint as UTF-16, since that is what DirectWrite analyses.
    wchar_t buffer[3]{};
    UINT32 bufferLength{};
    const uint32_t c = request.mustCoverCodepoint;
    if (c >= 0x10000)
    {
        const uint32_t v = c - 0x10000;
        buffer[0] = wchar_t(0xD800 + (v >> 10));
        buffer[1] = wchar_t(0xDC00 + (v & 0x3FF));
        bufferLength = 2;
    }
    else
    {
        buffer[0] = wchar_t(c);
        bufferLength = 1;
    }

    SingleStringTextSource source(buffer, bufferLength);
    UINT32 mappedLength{};
    FLOAT scale{};
    // Write straight into put(): MapCharacters hands over a reference, and the
    // ComPtr raw constructor would AddRef it a second time.
    if (FAILED(fallback->MapCharacters(&source, 0, bufferLength, nullptr, baseFamily.c_str(),
                                       static_cast<DWRITE_FONT_WEIGHT>(request.weight),
                                       static_cast<DWRITE_FONT_STYLE>(request.style),
                                       static_cast<DWRITE_FONT_STRETCH>(request.stretch),
                                       &mappedLength, returnFont.put(), &scale)) || !returnFont)
        return false;

    return true;
}
#endif // _WIN32

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

    gmpi::directx::ComPtr<IDWriteFont> font;

    if (request.mustCoverCodepoint != 0)
    {
        // Fallback: ask DirectWrite which font can actually render this
        // character. No single font covers Latin, CJK and emoji, so mixed text
        // has to be split into runs by covering font.
        if (!detail::mapFallbackFont(writeFactory.get(), familyName, request, font))
            return false;
    }
    else
    {
        UINT32 index{};
        BOOL exists{};
        if (FAILED(collection->FindFamilyName(familyName.c_str(), &index, &exists)) || !exists)
            return false;

        gmpi::directx::ComPtr<IDWriteFontFamily> family;
        if (FAILED(collection->GetFontFamily(index, family.put())) || !family)
            return false;

        if (FAILED(family->GetFirstMatchingFont(static_cast<DWRITE_FONT_WEIGHT>(request.weight),
                                                static_cast<DWRITE_FONT_STRETCH>(request.stretch),
                                                static_cast<DWRITE_FONT_STYLE>(request.style),
                                                font.put())) || !font)
            return false;
    }
    if (!font)
        return false;

    gmpi::directx::ComPtr<IDWriteFontFace> face;
    if (FAILED(font->CreateFontFace(face.put())) || !face)
        return false;

    // Identify what was actually resolved, so a caller can tell that two
    // requests landed on the same font file and share one loaded copy.
    {
        gmpi::directx::ComPtr<IDWriteFontFamily> resolvedFamily;
        gmpi::directx::ComPtr<IDWriteLocalizedStrings> names;
        if (SUCCEEDED(font->GetFontFamily(resolvedFamily.put())) && resolvedFamily &&
            SUCCEEDED(resolvedFamily->GetFamilyNames(names.put())) && names)
        {
            UINT32 length{};
            if (SUCCEEDED(names->GetStringLength(0, &length)) && length > 0)
            {
                std::wstring wide(length + 1, L'\0');
                if (SUCCEEDED(names->GetString(0, wide.data(), length + 1)))
                {
                    wide.resize(length);
                    const int needed = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
                    if (needed > 1)
                    {
                        returnFont.resolvedName.resize(needed - 1);
                        WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, returnFont.resolvedName.data(),
                                            needed, nullptr, nullptr);
                    }
                }
            }
        }
    }

    UINT32 fileCount{ 1 };
    // Written straight into put(): GetFiles hands over a reference, and the
    // ComPtr raw constructor would AddRef it a second time and leak the file
    // object along with its backing stream.
    gmpi::directx::ComPtr<IDWriteFontFile> fontFile;
    if (FAILED(face->GetFiles(&fileCount, fontFile.put())) || fileCount == 0 || !fontFile)
        return false;

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

    try
    {
        returnFont.bytes.assign(static_cast<const uint8_t*>(fragment),
                                static_cast<const uint8_t*>(fragment) + fileSize);
    }
    catch (...)
    {
        stream->ReleaseFileFragment(context); // a throwing copy must not leak the mapping
        throw;
    }
    stream->ReleaseFileFragment(context);

    returnFont.faceIndex = face->GetIndex();
    if (returnFont.resolvedName.empty())
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

    // Fallback: ask CoreText for a font that can actually render this
    // character, since no single font covers Latin, CJK and emoji.
    if (request.mustCoverCodepoint != 0)
    {
        UniChar utf16[2]{};
        CFIndex utf16Length{};
        const uint32_t c = request.mustCoverCodepoint;
        if (c >= 0x10000)
        {
            const uint32_t v = c - 0x10000;
            utf16[0] = UniChar(0xD800 + (v >> 10));
            utf16[1] = UniChar(0xDC00 + (v & 0x3FF));
            utf16Length = 2;
        }
        else
        {
            utf16[0] = UniChar(c);
            utf16Length = 1;
        }

        CFStringRef query = CFStringCreateWithCharactersNoCopy(kCFAllocatorDefault, utf16,
                                                               utf16Length, kCFAllocatorNull);
        if (query)
        {
            CTFontRef covering = CTFontCreateForString(font, query, CFRangeMake(0, utf16Length));
            CFRelease(query); // owned here, not by CTFontCreateForString
            if (covering)
            {
                CFRelease(font);
                font = covering;
            }
        }
    }

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
    // The file path identifies what was actually resolved, so two requests that
    // land on the same font share one loaded copy.
    returnFont.resolvedName = pathBuffer;
    return true;
}
#endif // __APPLE__

#if GMPI_UI_FONT_PROVIDER_FONTCONFIG
namespace detail {

// Codepoints that default to EMOJI presentation, i.e. must come from a colour
// font to look right. Deliberately a coarse range test rather than the full
// UTS #51 property: the blocks below are emoji-presentation almost throughout,
// while the older BMP symbols (U+2600 etc.) default to TEXT presentation and so
// belong on the ordinary text font.
//
// Not handled: U+FE0F, which flips an otherwise-text codepoint to emoji
// presentation. Fallback here is resolved per codepoint and never sees the
// variation selector that follows.
inline bool hasEmojiPresentation(uint32_t c)
{
    return (c >= 0x1F300 && c <= 0x1FAFF)   // pictographs, emoticons, transport, extended-A
        || (c >= 0x1F1E6 && c <= 0x1F1FF);  // regional indicators (flags)
}

// One FcFontMatch. Returns the matched file, and whether it really covers the
// codepoint — FcFontMatch always returns something, coverage or not.
inline bool matchFontFile(const FontRequest& request, const std::string& family,
                          std::string& returnPath, int& returnFaceIndex, bool& returnCovers)
{
    returnCovers = false;

    FcPattern* pattern = FcPatternCreate();
    if (!pattern)
        return false;

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
    const bool gotFile = FcPatternGetString(matched, FC_FILE, 0, &file) == FcResultMatch && file;
    returnFaceIndex = 0;
    FcPatternGetInteger(matched, FC_INDEX, 0, &returnFaceIndex);
    returnPath = gotFile ? reinterpret_cast<const char*>(file) : std::string{};

    if (request.mustCoverCodepoint != 0)
    {
        FcCharSet* got{};
        if (FcPatternGetCharSet(matched, FC_CHARSET, 0, &got) == FcResultMatch && got)
            returnCovers = FcCharSetHasChar(got, request.mustCoverCodepoint) != FcFalse;
    }

    FcPatternDestroy(matched);
    return !returnPath.empty();
}

} // namespace detail

inline bool findFont(const FontRequest& request, FontData& returnFont)
{
    returnFont = {};

    if (!FcInit())
        return false;

    const std::string requested = (request.familyName == "system-ui") ? "sans-serif" : request.familyName;

    // fontconfig ranks family similarity ABOVE coverage, so a fallback request
    // for U+1F600 while the base family is Arial lands on DejaVu Sans, which
    // does cover it — as a monochrome outline. Ask fontconfig's `emoji` generic
    // family first for emoji-presentation codepoints, the same thing browsers
    // and GTK do, so a colour font wins.
    std::vector<std::string> families;
    if (request.mustCoverCodepoint != 0 && detail::hasEmojiPresentation(request.mustCoverCodepoint))
        families.push_back("emoji");
    families.push_back(requested);

    std::string path;
    int faceIndex{};
    for (size_t i = 0; i < families.size(); ++i)
    {
        std::string candidate;
        int candidateIndex{};
        bool covers{};
        if (!detail::matchFontFile(request, families[i], candidate, candidateIndex, covers))
            continue;

        // Keep the first answer as a floor, then hold out for real coverage:
        // if no emoji font is installed, the monochrome glyph beats nothing.
        if (path.empty())
        {
            path = candidate;
            faceIndex = candidateIndex;
        }
        if (covers || request.mustCoverCodepoint == 0)
        {
            path = candidate;
            faceIndex = candidateIndex;
            break;
        }
    }

    if (path.empty())
        return false;

    if (!detail::readWholeFile(path, returnFont.bytes))
        return false;
    returnFont.faceIndex = static_cast<uint32_t>((std::max)(0, faceIndex));
    // The file path identifies what was actually resolved, so two requests that
    // land on the same font share one loaded copy.
    returnFont.resolvedName = path;
    return true;
}
#endif // GMPI_UI_FONT_PROVIDER_FONTCONFIG

}} // namespace gmpi::drawing
