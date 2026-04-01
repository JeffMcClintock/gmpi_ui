#pragma once

/*
#include "Cocoa_Gfx.h"
*/
/*
  GMPI - Generalized Music Plugin Interface specification.
  Copyright 2023 Jeff McClintock.

  Permission to use, copy, modify, and/or distribute this software for any
  purpose with or without fee is hereby granted, provided that the above
  copyright notice and this permission notice appear in all copies.

  THIS SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
#include <cmath>
#include <map>
#include <set>
#include <codecvt>
#include <cstring>
#include <TargetConditionals.h>
#include <CoreGraphics/CoreGraphics.h>
#include <CoreText/CoreText.h>
#include <ImageIO/ImageIO.h>
#include "../Drawing.h"
#include "../backends/Gfx_base.h"
#define USE_BACKING_BUFFER 1

#if TARGET_OS_OSX
#import <AppKit/AppKit.h>
#endif

/* TODO
investigate CGContextSetShouldSmoothFonts, CGContextSetAllowsFontSmoothing (for less heavy fonts)
*/

namespace gmpi
{
namespace cocoa
{
class Factory;

// Conversion utilities.

inline CGPoint toNative(gmpi::drawing::Point p)
{
    return CGPointMake(p.x, p.y);
}

inline CGRect CGRectFromRect(gmpi::drawing::Rect rect)
{
	return CGRectMake(rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top);
}

#if TARGET_OS_OSX
// Legacy helper for SDK3 backward compatibility — converts NSBezierPath to CGPath.
inline CGMutablePathRef NsToCGPath(NSBezierPath* geometry)
{
    CGMutablePathRef cgPath = CGPathCreateMutable();
    NSInteger n = [geometry elementCount];

    for (NSInteger i = 0; i < n; i++) {
        NSPoint ps[3];
        switch ([geometry elementAtIndex:i associatedPoints:ps]) {
            case NSBezierPathElementMoveTo: {
                CGPathMoveToPoint(cgPath, NULL, ps[0].x, ps[0].y);
                break;
            }
            case NSBezierPathElementLineTo: {
                CGPathAddLineToPoint(cgPath, NULL, ps[0].x, ps[0].y);
                break;
            }
            case NSBezierPathElementCurveTo: {
                CGPathAddCurveToPoint(cgPath, NULL, ps[0].x, ps[0].y, ps[1].x, ps[1].y, ps[2].x, ps[2].y);
                break;
            }
            case NSBezierPathElementClosePath: {
                CGPathCloseSubpath(cgPath);
                break;
            }
            default:
                assert(false && @"Invalid NSBezierPathElement");
        }
    }
    return cgPath;
}

// Legacy helper for SDK3 backward compatibility
inline NSPoint toNativeNS(gmpi::drawing::Point p)
{
    return NSMakePoint(p.x, p.y);
}

inline NSRect NSRectFromRect(gmpi::drawing::Rect rect)
{
    return NSMakeRect(rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top);
}
#endif

// helper: apply stroke style properties to a CGContext
inline void applyCGStrokeStyle(CGContextRef ctx, gmpi::drawing::api::IStrokeStyle* strokeStyle)
{
    gmpi::drawing::CapStyle capstyle = strokeStyle == nullptr ? gmpi::drawing::CapStyle::Flat : ((se::generic_graphics::StrokeStyle*)strokeStyle)->strokeStyleProperties.lineCap;

    switch(capstyle)
    {
        default:
        case gmpi::drawing::CapStyle::Flat:
            CGContextSetLineCap(ctx, kCGLineCapButt);
            break;

        case gmpi::drawing::CapStyle::Square:
            CGContextSetLineCap(ctx, kCGLineCapSquare);
            break;

        case gmpi::drawing::CapStyle::Round:
            CGContextSetLineCap(ctx, kCGLineCapRound);
            break;
    }

    if(strokeStyle != nullptr)
    {
        gmpi::drawing::LineJoin lineJoin = ((se::generic_graphics::StrokeStyle*)strokeStyle)->strokeStyleProperties.lineJoin;
        switch(lineJoin)
        {
            default:
            case gmpi::drawing::LineJoin::Miter:
            case gmpi::drawing::LineJoin::MiterOrBevel:
                CGContextSetLineJoin(ctx, kCGLineJoinMiter);
                break;

            case gmpi::drawing::LineJoin::Bevel:
                CGContextSetLineJoin(ctx, kCGLineJoinBevel);
                break;

            case gmpi::drawing::LineJoin::Round:
                CGContextSetLineJoin(ctx, kCGLineJoinRound);
                break;
        }
    }
}

inline void applyCGDashStyle(CGContextRef ctx, const gmpi::drawing::api::IStrokeStyle* strokeStyleIm, float strokeWidth)
{
    gmpi::drawing::StrokeStyleProperties style;

    auto strokeStyle = (se::generic_graphics::StrokeStyle*)strokeStyleIm;
    if(strokeStyle)
        style = strokeStyle->strokeStyleProperties;

    std::vector<CGFloat> dashes;

    switch(style.dashStyle)
    {
        case gmpi::drawing::DashStyle::Solid:
            break;

        case gmpi::drawing::DashStyle::Custom:
        {
            for(auto d : strokeStyle->dashes)
            {
                dashes.push_back((CGFloat) (d * strokeWidth));
            }
        }
        break;

        case gmpi::drawing::DashStyle::Dot:
            dashes.push_back(0.0f);
            dashes.push_back(strokeWidth * 2.f);
            break;

        case gmpi::drawing::DashStyle::DashDot:
            dashes.push_back(strokeWidth * 2.f);
            dashes.push_back(strokeWidth * 2.f);
            dashes.push_back(0.f);
            dashes.push_back(strokeWidth * 2.f);
            break;

        case gmpi::drawing::DashStyle::DashDotDot:
            dashes.push_back(strokeWidth * 2.f);
            dashes.push_back(strokeWidth * 2.f);
            dashes.push_back(0.f);
            dashes.push_back(strokeWidth * 2.f);
            dashes.push_back(0.f);
            dashes.push_back(strokeWidth * 2.f);
            break;

        case gmpi::drawing::DashStyle::Dash:
        default:
            dashes.push_back(strokeWidth * 2.f);
            dashes.push_back(strokeWidth * 2.f);
            break;
    };

    CGContextSetLineDash(ctx, style.dashOffset, dashes.data(), dashes.size());
}

// CGLineCap/CGLineJoin from stroke style
inline CGLineCap cgLineCapFromStyle(gmpi::drawing::api::IStrokeStyle* strokeStyle)
{
    if (!strokeStyle) return kCGLineCapButt;
    auto cap = ((se::generic_graphics::StrokeStyle*)strokeStyle)->strokeStyleProperties.lineCap;
    switch(cap)
    {
        case gmpi::drawing::CapStyle::Square: return kCGLineCapSquare;
        case gmpi::drawing::CapStyle::Round:  return kCGLineCapRound;
        default: return kCGLineCapButt;
    }
}

inline CGLineJoin cgLineJoinFromStyle(gmpi::drawing::api::IStrokeStyle* strokeStyle)
{
    if (!strokeStyle) return kCGLineJoinMiter;
    auto join = ((se::generic_graphics::StrokeStyle*)strokeStyle)->strokeStyleProperties.lineJoin;
    switch(join)
    {
        case gmpi::drawing::LineJoin::Bevel: return kCGLineJoinBevel;
        case gmpi::drawing::LineJoin::Round: return kCGLineJoinRound;
        default: return kCGLineJoinMiter;
    }
}

#if 1
// Classes without getFactory()
template<class MpInterface, class CocoaType>
class CocoaWrapper : public MpInterface
{
protected:
	CocoaType* native_;

	virtual ~CocoaWrapper()
	{
	}

public:
	CocoaWrapper(CocoaType* native) : native_(native) {}

	inline CocoaType* native()
	{
		return native_;
	}

	GMPI_REFCOUNT;
};

// Classes with getFactory()
template<class MpInterface, class CocoaType>
class CocoaWrapperWithFactory : public CocoaWrapper<MpInterface, CocoaType>
{
protected:
    gmpi::drawing::api::IFactory* factory_;

public:
	CocoaWrapperWithFactory(CocoaType* native, gmpi::drawing::api::IFactory* factory) : CocoaWrapper<MpInterface, CocoaType>(native), factory_(factory) {}

    gmpi::ReturnCode getFactory(gmpi::drawing::api::IFactory** factory) override
	{
		*factory = factory_;
		return gmpi::ReturnCode::Ok;
	}
};

class nothing
{
};
#endif

class TextFormat final : public gmpi::drawing::api::ITextFormat
{
public:
    std::string fontFamilyName;
    gmpi::drawing::FontWeight fontWeight;
    gmpi::drawing::FontStyle fontStyle;
    gmpi::drawing::FontStretch fontStretch;
    gmpi::drawing::TextAlignment textAlignment;
    gmpi::drawing::ParagraphAlignment paragraphAlignment;
    gmpi::drawing::WordWrapping wordWrapping = gmpi::drawing::WordWrapping::Wrap;
    float fontSize;

    // Cache some constants to make snap calculation faster.
    float topAdjustment = {}; // Mac includes extra space at top in font bounding box.
    float ascent = {};
    float baselineCorrection = {};

    // Custom line spacing (negative = use natural/default).
    float lineSpacing_ = -1.f;
    float customBaseline_ = 0.f;

    // Windows-equivalent natural line height, computed once from OS/2 font table.
    float winNaturalLineHeight_    = -1.f;
    float winNaturalBaseline_      = 0.f;
    float winNaturalTopAdjustment_ = 0.f;

    CTFontRef ctFont_ = nullptr;
    CTParagraphStyleRef ctParagraphStyle_ = nullptr;
    CTTextAlignment ctAlignment_ = kCTTextAlignmentLeft;

    // cached line spacing for paragraph style
    CGFloat ctLineHeightMin_ = 0;
    CGFloat ctLineHeightMax_ = 0;

    static const char* fontSubstitute(const char* windowsFont)
    {
        static std::map< std::string, std::string > substitutes =
        {
            // Windows, macOS
            {"Tahoma",              "Geneva"},
            {"MS Sans Serif",       "Geneva"},
            {"MS Serif",            "New York"},
            {"Lucida Console",      "Monaco"},
            {"Lucida Sans Unicode", "Lucida Grande"},
            {"Palatino Linotype",   "Palatino"},
            {"Book Antiqua",        "Palatino"},
        };

        const auto it = substitutes.find(windowsFont);
        if (it != substitutes.end())
        {
            return (*it).second.c_str();
        }

        return windowsFont;
    }

    TextFormat(const char* pfontFamilyName, gmpi::drawing::FontWeight pfontWeight, gmpi::drawing::FontStyle pfontStyle, gmpi::drawing::FontStretch pfontStretch, float pfontSize) :
        fontWeight(pfontWeight)
        , fontStyle(pfontStyle)
        , fontStretch(pfontStretch)
        , fontSize(pfontSize)
    {
        fontFamilyName = fontSubstitute(pfontFamilyName);

        textAlignment = gmpi::drawing::TextAlignment::Leading;
        paragraphAlignment = gmpi::drawing::ParagraphAlignment::Near;

        // Create CTFont using CoreText
        CFStringRef cfFamilyName = CFStringCreateWithCString(kCFAllocatorDefault, fontFamilyName.c_str(), kCFStringEncodingUTF8);

        // Build font traits dictionary
        CTFontSymbolicTraits symbolicTraits = 0;
        if (pfontWeight >= gmpi::drawing::FontWeight::DemiBold)
            symbolicTraits |= kCTFontBoldTrait;
        if (pfontStyle == gmpi::drawing::FontStyle::Italic || pfontStyle == gmpi::drawing::FontStyle::Oblique)
            symbolicTraits |= kCTFontItalicTrait;

        // Map GMPI font weight to CoreText weight
        static const CGFloat weightConversion[] = {
            -0.8,  // FontWeight_THIN = 100
            -0.6,  // FontWeight_ULTRA_LIGHT = 200
            -0.4,  // FontWeight_LIGHT = 300
             0.0,  // FontWeight_NORMAL = 400
             0.23, // FontWeight_MEDIUM = 500
             0.3,  // FontWeight_SEMI_BOLD = 600
             0.4,  // FontWeight_BOLD = 700
             0.56, // FontWeight_BLACK = 900
             0.62  // FontWeight_ULTRA_BLACK = 950
        };

        const int arrMax = (int)std::size(weightConversion) - 1;
        const int roundNearest = 50;
        const int nativeFontWeightIndex
            = std::max(0, std::min(arrMax, -1 + ((int) pfontWeight + roundNearest) / 100));
        const CGFloat ctWeight = weightConversion[nativeFontWeightIndex];

        // Create font descriptor with traits
        CFMutableDictionaryRef traits = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
            &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

        CFNumberRef weightNum = CFNumberCreate(kCFAllocatorDefault, kCFNumberCGFloatType, &ctWeight);
        CFDictionarySetValue(traits, kCTFontWeightTrait, weightNum);
        CFRelease(weightNum);

        if (symbolicTraits != 0) {
            CFNumberRef symTraitsNum = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &symbolicTraits);
            CFDictionarySetValue(traits, kCTFontSymbolicTrait, symTraitsNum);
            CFRelease(symTraitsNum);
        }

        CFMutableDictionaryRef attrs = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
            &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        CFDictionarySetValue(attrs, kCTFontFamilyNameAttribute, cfFamilyName);
        CFDictionarySetValue(attrs, kCTFontTraitsAttribute, traits);
        CFRelease(traits);

        CTFontDescriptorRef descriptor = CTFontDescriptorCreateWithAttributes(attrs);
        CFRelease(attrs);

        try
        {
            ctFont_ = CTFontCreateWithFontDescriptor(descriptor, fontSize, nullptr);
        }
        catch (...)
        {
        }
        CFRelease(descriptor);

        // fallback to system font if necessary
        if (!ctFont_)
        {
            ctFont_ = CTFontCreateUIFontForLanguage(kCTFontUIFontSystem, fontSize, nullptr);
        }

        CFRelease(cfFamilyName);

        rebuildParagraphStyle();
        CalculateTopAdjustment();
    }

    ~TextFormat()
    {
        if (ctFont_) CFRelease(ctFont_);
        if (ctParagraphStyle_) CFRelease(ctParagraphStyle_);
    }

    void rebuildParagraphStyle()
    {
        if (ctParagraphStyle_) CFRelease(ctParagraphStyle_);

        std::vector<CTParagraphStyleSetting> settings;

        settings.push_back({kCTParagraphStyleSpecifierAlignment, sizeof(ctAlignment_), &ctAlignment_});

        if (ctLineHeightMin_ > 0) {
            settings.push_back({kCTParagraphStyleSpecifierMinimumLineHeight, sizeof(ctLineHeightMin_), &ctLineHeightMin_});
        }
        if (ctLineHeightMax_ > 0) {
            settings.push_back({kCTParagraphStyleSpecifierMaximumLineHeight, sizeof(ctLineHeightMax_), &ctLineHeightMax_});
        }

        CTLineBreakMode lineBreak = (wordWrapping == gmpi::drawing::WordWrapping::NoWrap) ? kCTLineBreakByClipping : kCTLineBreakByWordWrapping;
        settings.push_back({kCTParagraphStyleSpecifierLineBreakMode, sizeof(lineBreak), &lineBreak});

        ctParagraphStyle_ = CTParagraphStyleCreate(settings.data(), settings.size());
    }

    // Build a CFAttributedString for measuring/drawing
    CFDictionaryRef createAttributes() const
    {
        const void* keys[] = { kCTFontAttributeName, kCTParagraphStyleAttributeName };
        const void* values[] = { ctFont_, ctParagraphStyle_ };
        return CFDictionaryCreate(kCFAllocatorDefault, keys, values, 2,
            &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    }

    // Build attributes with a foreground color
    CFDictionaryRef createAttributesWithColor(CGColorRef color) const
    {
        const void* keys[] = { kCTFontAttributeName, kCTParagraphStyleAttributeName, kCTForegroundColorAttributeName };
        const void* values[] = { ctFont_, ctParagraphStyle_, color };
        return CFDictionaryCreate(kCFAllocatorDefault, keys, values, 3,
            &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    }

    void CalculateTopAdjustment()
    {
        gmpi::drawing::FontMetrics fontMetrics{};
        getFontMetrics(&fontMetrics);

        // Measure "A" to get bounding box height
        CFDictionaryRef attributes = createAttributes();
        CFStringRef str = CFSTR("A");
        CFAttributedStringRef attrStr = CFAttributedStringCreate(kCFAllocatorDefault, str, attributes);
        CTLineRef line = CTLineCreateWithAttributedString(attrStr);

        CGFloat ascent_ct, descent_ct, leading_ct;
        CTLineGetTypographicBounds(line, &ascent_ct, &descent_ct, &leading_ct);
        CGFloat boundingBoxHeight = ascent_ct + descent_ct + leading_ct;

        CFRelease(line);
        CFRelease(attrStr);
        CFRelease(attributes);

        topAdjustment = boundingBoxHeight - (fontMetrics.ascent + fontMetrics.descent);
        ascent = fontMetrics.ascent;

        baselineCorrection = 0.5f;
        if ((fontMetrics.descent - floor(fontMetrics.descent)) <= 0.5f)
        {
            baselineCorrection += 0.5f;
        }

        // Compute the Windows-equivalent natural line height from the font's OS/2 sTypo metrics.
        {
            CFDataRef os2Data  = CTFontCopyTable(ctFont_, kCTFontTableOS2,  kCTFontTableOptionNoOptions);
            CFDataRef headData = CTFontCopyTable(ctFont_, kCTFontTableHead, kCTFontTableOptionNoOptions);
            CFDataRef hheaData = CTFontCopyTable(ctFont_, kCTFontTableHhea, kCTFontTableOptionNoOptions);
            if (os2Data && headData)
            {
                const uint8_t* os2  = CFDataGetBytePtr(os2Data);
                const uint8_t* head = CFDataGetBytePtr(headData);

                const int16_t  sTypoAscender  = static_cast<int16_t>((os2[68]  << 8) | os2[69]);
                const int16_t  sTypoDescender = static_cast<int16_t>((os2[70]  << 8) | os2[71]);
                const int16_t  sTypoLineGap   = static_cast<int16_t>((os2[72]  << 8) | os2[73]);
                const uint16_t unitsPerEm     = static_cast<uint16_t>((head[18] << 8) | head[19]);

                const uint16_t fsSelection = static_cast<uint16_t>((os2[62] << 8) | os2[63]);
                const bool useTypoMetrics  = (fsSelection & 0x0080) != 0;

                const float scale = fontSize / unitsPerEm;

                if (!useTypoMetrics && hheaData)
                {
                    const uint8_t* hhea = CFDataGetBytePtr(hheaData);
                    const int16_t  hheaAscender  = static_cast<int16_t>((hhea[4] << 8) | hhea[5]);
                    const int16_t  hheaDescender = static_cast<int16_t>((hhea[6] << 8) | hhea[7]);
                    const int16_t  hheaLineGap   = static_cast<int16_t>((hhea[8] << 8) | hhea[9]);
                    winNaturalLineHeight_ = (hheaAscender - hheaDescender + hheaLineGap) * scale;
                    winNaturalBaseline_   = hheaAscender * scale;
                }
                else
                {
                    winNaturalLineHeight_ = (sTypoAscender - sTypoDescender + sTypoLineGap) * scale;
                    winNaturalBaseline_   = sTypoAscender * scale;
                }
            }
            else
            {
                winNaturalLineHeight_ = boundingBoxHeight;
                winNaturalBaseline_   = fontMetrics.ascent;
            }
            if (os2Data)  CFRelease(os2Data);
            if (headData) CFRelease(headData);
            if (hheaData) CFRelease(hheaData);

            // Compute topAdjustment for the Windows natural line height.
            {
                // Temporarily apply line height to measure
                CGFloat savedMin = ctLineHeightMin_;
                CGFloat savedMax = ctLineHeightMax_;
                ctLineHeightMin_ = winNaturalLineHeight_;
                ctLineHeightMax_ = winNaturalLineHeight_;
                rebuildParagraphStyle();

                CFDictionaryRef winAttrs = createAttributes();
                CFAttributedStringRef winAttrStr = CFAttributedStringCreate(kCFAllocatorDefault, CFSTR("A"), winAttrs);
                CTLineRef winLine = CTLineCreateWithAttributedString(winAttrStr);
                CGFloat wa, wd, wl;
                CTLineGetTypographicBounds(winLine, &wa, &wd, &wl);
                CGFloat winBoxHeight = wa + wd + wl;
                CFRelease(winLine);
                CFRelease(winAttrStr);
                CFRelease(winAttrs);

                ctLineHeightMin_ = savedMin;
                ctLineHeightMax_ = savedMax;
                rebuildParagraphStyle();

                winNaturalTopAdjustment_ = static_cast<float>(winBoxHeight)
                                           - (fontMetrics.ascent + fontMetrics.descent);
            }
        }
    }

	gmpi::ReturnCode setTextAlignment(gmpi::drawing::TextAlignment ptextAlignment) override
    {
        textAlignment = ptextAlignment;

        switch (textAlignment)
        {
            case gmpi::drawing::TextAlignment::Leading:
                ctAlignment_ = kCTTextAlignmentLeft;
                break;
            case gmpi::drawing::TextAlignment::Trailing:
                ctAlignment_ = kCTTextAlignmentRight;
                break;
            case gmpi::drawing::TextAlignment::Center:
                ctAlignment_ = kCTTextAlignmentCenter;
                break;
        }

        rebuildParagraphStyle();
        return gmpi::ReturnCode::Ok;
    }

	gmpi::ReturnCode setParagraphAlignment(gmpi::drawing::ParagraphAlignment pparagraphAlignment) override
    {
        paragraphAlignment = pparagraphAlignment;
        return gmpi::ReturnCode::Ok;
    }

	gmpi::ReturnCode setWordWrapping(gmpi::drawing::WordWrapping pwordWrapping) override
    {
        wordWrapping = pwordWrapping;
        rebuildParagraphStyle();
        return gmpi::ReturnCode::Ok;
    }


    gmpi::ReturnCode getFontMetrics(gmpi::drawing::FontMetrics* returnFontMetrics) override
    {
        returnFontMetrics->ascent = static_cast<float>(CTFontGetAscent(ctFont_));
        returnFontMetrics->descent = static_cast<float>(CTFontGetDescent(ctFont_));
        returnFontMetrics->lineGap = static_cast<float>(CTFontGetLeading(ctFont_));
        returnFontMetrics->capHeight = static_cast<float>(CTFontGetCapHeight(ctFont_));
        returnFontMetrics->xHeight = static_cast<float>(CTFontGetXHeight(ctFont_));
        returnFontMetrics->underlinePosition = static_cast<float>(CTFontGetUnderlinePosition(ctFont_));
        returnFontMetrics->underlineThickness = static_cast<float>(CTFontGetUnderlineThickness(ctFont_));
        returnFontMetrics->strikethroughPosition = returnFontMetrics->xHeight / 2;
        returnFontMetrics->strikethroughThickness = returnFontMetrics->underlineThickness;
        return gmpi::ReturnCode::Ok;
    }

    gmpi::ReturnCode getTextExtentU(const char* utf8String, int32_t stringLength, gmpi::drawing::Size* returnSize) override
    {
        CFStringRef str = CFStringCreateWithBytes(kCFAllocatorDefault,
            (const UInt8*)utf8String, stringLength, kCFStringEncodingUTF8, false);

        CFDictionaryRef attributes = createAttributes();
        CFAttributedStringRef attrStr = CFAttributedStringCreate(kCFAllocatorDefault, str, attributes);
        CTLineRef line = CTLineCreateWithAttributedString(attrStr);

        CGFloat ascent_ct, descent_ct, leading_ct;
        double width = CTLineGetTypographicBounds(line, &ascent_ct, &descent_ct, &leading_ct);

        returnSize->width = static_cast<float>(width);
        returnSize->height = static_cast<float>(ascent_ct + descent_ct + leading_ct);

        if (lineSpacing_ < 0.f)
        {
            const float cocoaLineH = static_cast<float>(ascent_ct + descent_ct + leading_ct);
            const float numLines   = std::round(returnSize->height / cocoaLineH);
            returnSize->height = numLines * winNaturalLineHeight_;
        }

        CFRelease(line);
        CFRelease(attrStr);
        CFRelease(attributes);
        CFRelease(str);

        return gmpi::ReturnCode::Ok;
    }

    gmpi::ReturnCode setLineSpacing(float lineSpacing, float baseline) override
    {
        lineSpacing_ = lineSpacing;
        customBaseline_ = baseline;

        if (lineSpacing >= 0.f)
        {
            ctLineHeightMin_ = lineSpacing;
            ctLineHeightMax_ = lineSpacing;
        }
        else
        {
            ctLineHeightMin_ = 0;
            ctLineHeightMax_ = 0;
        }

        rebuildParagraphStyle();
        CalculateTopAdjustment();
        return gmpi::ReturnCode::Ok;
    }

	GMPI_QUERYINTERFACE_METHOD(gmpi::drawing::api::ITextFormat);
    GMPI_REFCOUNT;
};

class BitmapPixels final : public gmpi::drawing::api::IBitmapPixels
{
    int bytesPerRow;
    class Bitmap* seBitmap = {};
    CGImageRef* inImage_;
    CGContextRef pixelContext_ = nullptr;
    int32_t flags;

public:
    BitmapPixels(Bitmap* bitmap, bool _alphaPremultiplied, int32_t pflags);
    ~BitmapPixels();

    gmpi::ReturnCode getAddress(uint8_t** returnAddress) override
    {
        *returnAddress = static_cast<uint8_t*>(CGBitmapContextGetData(pixelContext_));
		return gmpi::ReturnCode::Ok;
    }
    gmpi::ReturnCode getBytesPerRow(int32_t* returnBytesPerRow) override
    {
        *returnBytesPerRow = bytesPerRow;
        return gmpi::ReturnCode::Ok;
    }

    gmpi::ReturnCode getPixelFormat(int32_t* returnPixelFormat) override
    {
        *returnPixelFormat = kRGBA;
        return gmpi::ReturnCode::Ok;
    }

    inline uint8_t fast8bitScale(uint8_t a, uint8_t b) const
    {
        int t = (int)a * (int)b;
        return (uint8_t)((t + 1 + (t >> 8)) >> 8);
    }

    GMPI_QUERYINTERFACE_METHOD(gmpi::drawing::api::IBitmapPixels);
    GMPI_REFCOUNT;
};

class Bitmap final : public gmpi::drawing::api::IBitmap
{
public:
    gmpi::drawing::api::IFactory* factory = nullptr;
    CGImageRef nativeBitmap_ = nullptr;
    CGImageRef additiveBitmap_ = nullptr;
    int32_t creationFlags = (int32_t)drawing::BitmapRenderTargetFlags::SRGBPixels;
    uint32_t pixelWidth_ = 0;
    uint32_t pixelHeight_ = 0;

    Bitmap(gmpi::drawing::api::IFactory* pfactory, const char* utf8Uri) :
        nativeBitmap_(nullptr)
        , factory(pfactory)
    {
        std::string uriString(utf8Uri);

        // Load using ImageIO (cross-platform macOS/iOS)
        CFStringRef cfPath = CFStringCreateWithCString(kCFAllocatorDefault, utf8Uri, kCFStringEncodingUTF8);
        CFURLRef url = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, cfPath, kCFURLPOSIXPathStyle, false);
        CGImageSourceRef source = CGImageSourceCreateWithURL(url, nullptr);
        CFRelease(url);
        CFRelease(cfPath);

        if (source)
        {
            nativeBitmap_ = CGImageSourceCreateImageAtIndex(source, 0, nullptr);
            CFRelease(source);
        }

        if (nativeBitmap_)
        {
            pixelWidth_ = static_cast<uint32_t>(CGImageGetWidth(nativeBitmap_));
            pixelHeight_ = static_cast<uint32_t>(CGImageGetHeight(nativeBitmap_));
        }
    }

    Bitmap(gmpi::drawing::api::IFactory* pfactory, int32_t width, int32_t height,
           int32_t pCreationFlags = (int32_t)gmpi::drawing::BitmapRenderTargetFlags::SRGBPixels)
        : factory(pfactory)
        , creationFlags(pCreationFlags)
        , pixelWidth_(width)
        , pixelHeight_(height)
    {
        // Create an empty bitmap via CGBitmapContext
        CGColorSpaceRef colorSpace = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
        CGContextRef ctx = CGBitmapContextCreate(NULL, width, height, 8, 0, colorSpace,
            kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big);
        CGColorSpaceRelease(colorSpace);

        if (ctx)
        {
            // Clear to transparent
            CGContextClearRect(ctx, CGRectMake(0, 0, width, height));
            nativeBitmap_ = CGBitmapContextCreateImage(ctx);
            CGContextRelease(ctx);
        }
    }

    // from a bitmap render target (takes ownership of the CGImage)
    Bitmap(
           gmpi::drawing::api::IFactory* pfactory
           , CGImageRef native
           , int32_t pCreationFlags = (int32_t)gmpi::drawing::BitmapRenderTargetFlags::SRGBPixels
           )
        : nativeBitmap_(native)
        , factory(pfactory)
        , creationFlags(pCreationFlags)
    {
        if (nativeBitmap_)
        {
            CGImageRetain(nativeBitmap_);
            pixelWidth_ = static_cast<uint32_t>(CGImageGetWidth(nativeBitmap_));
            pixelHeight_ = static_cast<uint32_t>(CGImageGetHeight(nativeBitmap_));
        }
    }

    bool isLoaded()
    {
        return nativeBitmap_ != nullptr;
    }

    virtual ~Bitmap()
    {
        if (nativeBitmap_)
            CGImageRelease(nativeBitmap_);
        if (additiveBitmap_)
            CGImageRelease(additiveBitmap_);
    }

    inline CGImageRef getNativeBitmap()
    {
        return nativeBitmap_;
    }

    gmpi::ReturnCode getSizeU(gmpi::drawing::SizeU* returnSize) override
    {
        returnSize->width = pixelWidth_;
        returnSize->height = pixelHeight_;
        return gmpi::ReturnCode::Ok;
    }

    gmpi::ReturnCode lockPixels(gmpi::drawing::api::IBitmapPixels** returnPixels, int32_t flags) override
    {
        *returnPixels = 0;

        gmpi::shared_ptr<gmpi::api::IUnknown> b2;
        b2.attach(new BitmapPixels(this, true, flags));

        return b2->queryInterface(&gmpi::drawing::api::IBitmapPixels::guid, (void**)(returnPixels));
    }

    void ApplyAlphaCorrection2() {}

    gmpi::ReturnCode getFactory(gmpi::drawing::api::IFactory** returnFactory) override
    {
        *returnFactory = factory;
        return gmpi::ReturnCode::Ok;
    }

    GMPI_QUERYINTERFACE_METHOD(gmpi::drawing::api::IBitmap);
    GMPI_REFCOUNT;
};

class GradientstopCollection : public CocoaWrapperWithFactory<gmpi::drawing::api::IGradientstopCollection, nothing>
{
public:
    std::vector<gmpi::drawing::Gradientstop> gradientstops;

    GradientstopCollection(gmpi::drawing::api::IFactory* factory, const gmpi::drawing::Gradientstop* gradientStops, uint32_t gradientStopsCount) : CocoaWrapperWithFactory(nullptr, factory)
    {
        for (uint32_t i = 0; i < gradientStopsCount; ++i)
        {
            gradientstops.push_back(gradientStops[i]);
        }
    }
    GMPI_QUERYINTERFACE_METHOD(gmpi::drawing::api::IGradientstopCollection);
};

// shared between SDK3 and GMPI-UI factories
struct FactoryInfo
{
    std::vector<std::string> supportedFontFamilies;
    CGColorSpaceRef cgColorSpace = nullptr;
#if TARGET_OS_OSX
    // SDK3 backward compatibility — legacy code references gmpiColorSpace as NSColorSpace*
    NSColorSpace* gmpiColorSpace = nullptr;
#endif
};

inline void initFactoryHelper(FactoryInfo& info)
{
    info.cgColorSpace = CGColorSpaceCreateWithName(kCGColorSpaceLinearSRGB);
#if TARGET_OS_OSX
    info.gmpiColorSpace = [[NSColorSpace alloc] initWithCGColorSpace:info.cgColorSpace];
#endif
}

class Factory : public gmpi::drawing::api::IFactory
{
public:
    FactoryInfo info;

    Factory()
    {
        initFactoryHelper(info);
    }

    ~Factory()
    {
        if (info.cgColorSpace)
            CGColorSpaceRelease(info.cgColorSpace);
    }

    // utility: create a CGColor in the linear sRGB color space
    inline CGColorRef toNativeCGColor(const gmpi::drawing::Color& color)
    {
        const CGFloat components[4] = {color.r, color.g, color.b, color.a};
        return CGColorCreate(info.cgColorSpace, components);
    }

    gmpi::ReturnCode createPathGeometry(gmpi::drawing::api::IPathGeometry** pathGeometry) override;

    gmpi::ReturnCode createTextFormat(const char* fontFamilyName, gmpi::drawing::FontWeight fontWeight, gmpi::drawing::FontStyle fontStyle, gmpi::drawing::FontStretch fontStretch, float fontSize, int32_t fontFlags, gmpi::drawing::api::ITextFormat** textFormat) override;

    gmpi::ReturnCode createImage(int32_t width, int32_t height, int32_t flags, gmpi::drawing::api::IBitmap** returnDiBitmap) override;

    gmpi::ReturnCode loadImageU(const char* utf8Uri, gmpi::drawing::api::IBitmap** returnDiBitmap) override;

    gmpi::ReturnCode createStrokeStyle(const gmpi::drawing::StrokeStyleProperties* strokeStyleProperties, const float* dashes, int32_t dashesCount, gmpi::drawing::api::IStrokeStyle** returnValue) override
    {
        *returnValue = nullptr;

        gmpi::shared_ptr<gmpi::api::IUnknown> b2;
        b2.attach(new se::generic_graphics::StrokeStyle(this, strokeStyleProperties, dashes, dashesCount));

        return b2->queryInterface(&gmpi::drawing::api::IStrokeStyle::guid, reinterpret_cast<void **>(returnValue));
    }

    gmpi::ReturnCode getFontFamilyName(int32_t fontIndex, gmpi::api::IString* returnString) override
    {
        if(info.supportedFontFamilies.empty())
        {
            // Use CoreText to enumerate font families (works on both macOS and iOS)
            CTFontCollectionRef collection = CTFontCollectionCreateFromAvailableFonts(nullptr);
            CFArrayRef descriptors = CTFontCollectionCreateMatchingFontDescriptors(collection);

            if (descriptors)
            {
                std::set<std::string> familySet;
                CFIndex count = CFArrayGetCount(descriptors);
                for (CFIndex i = 0; i < count; ++i)
                {
                    CTFontDescriptorRef desc = (CTFontDescriptorRef)CFArrayGetValueAtIndex(descriptors, i);
                    CFStringRef familyName = (CFStringRef)CTFontDescriptorCopyAttribute(desc, kCTFontFamilyNameAttribute);
                    if (familyName)
                    {
                        char buf[256];
                        if (CFStringGetCString(familyName, buf, sizeof(buf), kCFStringEncodingUTF8))
                        {
                            familySet.insert(buf);
                        }
                        CFRelease(familyName);
                    }
                }
                CFRelease(descriptors);

                for (const auto& name : familySet)
                    info.supportedFontFamilies.push_back(name);
            }
            CFRelease(collection);
        }

        if (fontIndex < 0 || fontIndex >= (int32_t)info.supportedFontFamilies.size())
        {
            return gmpi::ReturnCode::Fail;
        }

        returnString->setData(info.supportedFontFamilies[fontIndex].data(), static_cast<int32_t>(info.supportedFontFamilies[fontIndex].size()));
        return gmpi::ReturnCode::Ok;
    }

    gmpi::ReturnCode getPlatformPixelFormat(gmpi::drawing::api::IBitmapPixels::PixelFormat* returnPixelFormat) override
    {
        *returnPixelFormat = gmpi::drawing::api::IBitmapPixels::kBGRA;
        return gmpi::ReturnCode::Ok;
    }

    gmpi::ReturnCode createCpuRenderTarget(gmpi::drawing::SizeU size, int32_t flags, gmpi::drawing::api::IBitmapRenderTarget** returnBitmapRenderTarget) override;

    GMPI_QUERYINTERFACE_METHOD(gmpi::drawing::api::IFactory);
    GMPI_REFCOUNT_NO_DELETE;
};

class CocoaBrushBase
{
protected:
    gmpi::cocoa::Factory* factory_;

public:
    CocoaBrushBase(gmpi::cocoa::Factory* pfactory) :
        factory_(pfactory)
    {}

    virtual ~CocoaBrushBase() {}

    virtual void fillPath(class GraphicsContext* context, CGPathRef cgPath) const = 0;

    virtual void strokePath(CGContextRef ctx, CGPathRef cgPath, float strokeWidth, const gmpi::drawing::api::IStrokeStyle* strokeStyle = nullptr) const
    {
        CGContextSaveGState(ctx);

        const CGFloat black[] = {0, 0, 0, 1};
        CGContextSetStrokeColor(ctx, black);
        CGContextSetLineWidth(ctx, strokeWidth);
        applyCGStrokeStyle(ctx, (gmpi::drawing::api::IStrokeStyle*)strokeStyle);
        applyCGDashStyle(ctx, strokeStyle, strokeWidth);
        CGContextAddPath(ctx, cgPath);
        CGContextStrokePath(ctx);

        CGContextRestoreGState(ctx);
    }
};


class BitmapBrush final : public gmpi::drawing::api::IBitmapBrush, public CocoaBrushBase
{
    CGImageRef patternImage_ = nullptr;

    static CGImageRef makePatternImage(CGImageRef src)
    {
        size_t w = CGImageGetWidth(src);
        size_t h = CGImageGetHeight(src);

        CGColorSpaceRef srgb = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
        CGContextRef ctx = CGBitmapContextCreate(NULL, w, h, 8, 0, srgb,
            kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big);
        CGColorSpaceRelease(srgb);

        CGContextDrawImage(ctx, CGRectMake(0, 0, w, h), src);
        CGImageRef result = CGBitmapContextCreateImage(ctx);
        CGContextRelease(ctx);
        return result;
    }

public:
    gmpi::cocoa::Bitmap bitmap_;
    gmpi::drawing::BrushProperties brushProperties_;

    BitmapBrush(
        gmpi::cocoa::Factory* factory,
        const gmpi::drawing::api::IBitmap* bitmap,
        const gmpi::drawing::BrushProperties* brushProperties
    )
        : CocoaBrushBase(factory),
        bitmap_(factory, ((Bitmap*)bitmap)->nativeBitmap_),
        brushProperties_(*brushProperties)
    {
        patternImage_ = makePatternImage(bitmap_.nativeBitmap_);
    }

    ~BitmapBrush()
    {
        if (patternImage_) CGImageRelease(patternImage_);
    }

    void strokePath(CGContextRef ctx, CGPathRef cgPath, float strokeWidth, const gmpi::drawing::api::IStrokeStyle* strokeStyle = nullptr) const override
    {
        // For pattern brush stroke, we use clip-to-stroke approach
        CGContextSaveGState(ctx);

        CGContextSetLineWidth(ctx, strokeWidth);
        applyCGStrokeStyle(ctx, (gmpi::drawing::api::IStrokeStyle*)strokeStyle);
        applyCGDashStyle(ctx, strokeStyle, strokeWidth);

        // Create stroke path and clip to it
        CGContextAddPath(ctx, cgPath);
        CGContextReplacePathWithStrokedPath(ctx);
        CGContextClip(ctx);

        // Tile the pattern image
        CGRect bounds = CGPathGetBoundingBox(cgPath);
        CGFloat tileW = CGImageGetWidth(patternImage_);
        CGFloat tileH = CGImageGetHeight(patternImage_);

        for (CGFloat y = floor(bounds.origin.y / tileH) * tileH; y < CGRectGetMaxY(bounds); y += tileH) {
            for (CGFloat x = floor(bounds.origin.x / tileW) * tileW; x < CGRectGetMaxX(bounds); x += tileW) {
                CGContextSaveGState(ctx);
                CGContextTranslateCTM(ctx, x, y + tileH);
                CGContextScaleCTM(ctx, 1.0, -1.0);
                CGContextDrawImage(ctx, CGRectMake(0, 0, tileW, tileH), patternImage_);
                CGContextRestoreGState(ctx);
            }
        }

        CGContextRestoreGState(ctx);
    }

    void fillPath(GraphicsContext* context, CGPathRef cgPath) const override;

    gmpi::ReturnCode getFactory(gmpi::drawing::api::IFactory** factory) override
    {
        *factory = factory_;
        return gmpi::ReturnCode::Ok;
    }

    GMPI_REFCOUNT;
    GMPI_QUERYINTERFACE_METHOD(gmpi::drawing::api::IBitmapBrush);
};

class SolidColorBrush : public gmpi::drawing::api::ISolidColorBrush, public CocoaBrushBase
{
    gmpi::drawing::Color color;
    CGColorRef nativec_ = nullptr;

    inline void setNativeColor()
    {
        if (nativec_) CGColorRelease(nativec_);
        nativec_ = factory_->toNativeCGColor(color);
    }

public:
    SolidColorBrush(const gmpi::drawing::Color* pcolor, cocoa::Factory* factory) : CocoaBrushBase(factory)
        , color(*pcolor)
    {
        setNativeColor();
    }

    inline CGColorRef nativeColor() const
    {
        return nativec_;
    }

    void fillPath(GraphicsContext* context, CGPathRef cgPath) const override;

    void strokePath(CGContextRef ctx, CGPathRef cgPath, float strokeWidth, const gmpi::drawing::api::IStrokeStyle* strokeStyle = nullptr) const override
    {
        CGContextSaveGState(ctx);
        CGContextSetStrokeColorWithColor(ctx, nativec_);
        CGContextSetLineWidth(ctx, strokeWidth);
        applyCGStrokeStyle(ctx, (gmpi::drawing::api::IStrokeStyle*)strokeStyle);
        applyCGDashStyle(ctx, strokeStyle, strokeWidth);
        CGContextAddPath(ctx, cgPath);
        CGContextStrokePath(ctx);
        CGContextRestoreGState(ctx);
    }

    ~SolidColorBrush()
    {
        if (nativec_) CGColorRelease(nativec_);
    }

	void setColor(const gmpi::drawing::Color* pcolor) override
    {
        color = *pcolor;
        setNativeColor();
    }

	gmpi::ReturnCode getFactory(gmpi::drawing::api::IFactory** factory) override
    {
        *factory = factory_;
		return gmpi::ReturnCode::Ok;
    }

	gmpi::ReturnCode queryInterface(const gmpi::api::Guid* iid, void** returnInterface) override
	{
		*returnInterface = {};

        GMPI_QUERYINTERFACE(gmpi::drawing::api::IResource);
        GMPI_QUERYINTERFACE(gmpi::drawing::api::ISolidColorBrush);

		return gmpi::ReturnCode::NoSupport;
	}

    GMPI_REFCOUNT;
};

class Gradient
{
protected:
    CGGradientRef cgGradient_ = nullptr;
    cocoa::Factory* gradientFactory_;

public:
    Gradient(cocoa::Factory* factory, const gmpi::drawing::api::IGradientstopCollection* gradientStopCollection) :
        gradientFactory_(factory)
    {
        auto stops = static_cast<const GradientstopCollection*>(gradientStopCollection);

        const size_t count = stops->gradientstops.size();
        std::vector<CGFloat> components(count * 4);
        std::vector<CGFloat> locations(count);

        // reversed, so radial gradient draws same as PC
        for (size_t i = 0; i < count; ++i)
        {
            const auto& stop = stops->gradientstops[count - 1 - i];
            components[i * 4 + 0] = stop.color.r;
            components[i * 4 + 1] = stop.color.g;
            components[i * 4 + 2] = stop.color.b;
            components[i * 4 + 3] = stop.color.a;
            locations[i] = 1.0 - stop.position;
        }

        cgGradient_ = CGGradientCreateWithColorComponents(factory->info.cgColorSpace,
            components.data(), locations.data(), count);
    }

    virtual void drawGradient(CGContextRef ctx) const = 0;

    ~Gradient()
    {
        if (cgGradient_) CGGradientRelease(cgGradient_);
    }

    void fillPath(CGContextRef ctx, CGPathRef cgPath) const
    {
        CGContextSaveGState(ctx);
        CGContextAddPath(ctx, cgPath);
        CGContextClip(ctx);
        drawGradient(ctx);
        CGContextRestoreGState(ctx);
    }

    void strokePath(CGContextRef ctx, CGPathRef cgPath, float strokeWidth, const gmpi::drawing::api::IStrokeStyle* strokeStyle = nullptr) const
    {
        CGContextSaveGState(ctx);

        // Create stroked version of path and clip to it
        CGContextSetLineWidth(ctx, strokeWidth);
        applyCGStrokeStyle(ctx, (gmpi::drawing::api::IStrokeStyle*)strokeStyle);
        applyCGDashStyle(ctx, strokeStyle, strokeWidth);
        CGContextAddPath(ctx, cgPath);
        CGContextReplacePathWithStrokedPath(ctx);
        CGContextClip(ctx);

        drawGradient(ctx);

        CGContextRestoreGState(ctx);
    }
};

class LinearGradientBrush final : public gmpi::drawing::api::ILinearGradientBrush, public CocoaBrushBase, public Gradient
{
    gmpi::drawing::LinearGradientBrushProperties brushProperties;

public:
    LinearGradientBrush(
        cocoa::Factory* factory,
        const gmpi::drawing::LinearGradientBrushProperties* linearGradientBrushProperties,
        const gmpi::drawing::BrushProperties* brushProperties,
        const gmpi::drawing::api::IGradientstopCollection* gradientStopCollection) :
        CocoaBrushBase(factory)
        , Gradient(factory, gradientStopCollection)
        , brushProperties(*linearGradientBrushProperties)
    {
    }

public:

	void setStartPoint(gmpi::drawing::Point startPoint) override
    {
        brushProperties.startPoint = startPoint;
    }
    void setEndPoint(gmpi::drawing::Point endPoint) override
    {
        brushProperties.endPoint = endPoint;
    }

    void drawGradient(CGContextRef ctx) const override
    {
        CGContextDrawLinearGradient(ctx, cgGradient_,
            toNative(brushProperties.endPoint),
            toNative(brushProperties.startPoint),
            kCGGradientDrawsBeforeStartLocation | kCGGradientDrawsAfterEndLocation);
    }

    void fillPath(GraphicsContext*, CGPathRef cgPath) const override;

    void strokePath(CGContextRef ctx, CGPathRef cgPath, float strokeWidth, const gmpi::drawing::api::IStrokeStyle* strokeStyle = nullptr) const override
    {
        Gradient::strokePath(ctx, cgPath, strokeWidth, strokeStyle);
    }

	gmpi::ReturnCode getFactory(gmpi::drawing::api::IFactory** factory) override
    {
        *factory = factory_;
		return gmpi::ReturnCode::Ok;
    }

    ReturnCode queryInterface(const gmpi::api::Guid* iid, void** returnInterface) override
    {
        *returnInterface = {};
        GMPI_QUERYINTERFACE(drawing::api::IResource);
        GMPI_QUERYINTERFACE(drawing::api::ILinearGradientBrush);
        return ReturnCode::NoSupport;
    }
    GMPI_REFCOUNT;
};

class RadialGradientBrush : public gmpi::drawing::api::IRadialGradientBrush, public CocoaBrushBase, public Gradient
{
    gmpi::drawing::RadialGradientBrushProperties gradientProperties;

public:
    RadialGradientBrush(cocoa::Factory* factory, const gmpi::drawing::RadialGradientBrushProperties* radialGradientBrushProperties, const gmpi::drawing::BrushProperties* brushProperties, const  gmpi::drawing::api::IGradientstopCollection* gradientStopCollection) :
        CocoaBrushBase(factory)
        , Gradient(factory, gradientStopCollection)
        , gradientProperties(*radialGradientBrushProperties)
    {
    }

    void drawGradient(CGContextRef ctx) const override
    {
        const CGPoint center = toNative(gradientProperties.center);
        const CGPoint origin = CGPointMake(
            gradientProperties.center.x + gradientProperties.gradientOriginOffset.x,
            gradientProperties.center.y + gradientProperties.gradientOriginOffset.y);

        CGContextDrawRadialGradient(ctx, cgGradient_,
            center, gradientProperties.radiusX,
            origin, 0.0,
            kCGGradientDrawsBeforeStartLocation | kCGGradientDrawsAfterEndLocation);
    }

    void setCenter(gmpi::drawing::Point center) override
    {
        gradientProperties.center = center;
    }

    void setGradientOriginOffset(gmpi::drawing::Point gradientOriginOffset) override
    {
    }

    void setRadiusX(float radiusX) override
    {
        gradientProperties.radiusX = radiusX;
    }

    void setRadiusY(float radiusY) override
    {
        gradientProperties.radiusY = radiusY;
    }

    void fillPath(GraphicsContext*, CGPathRef cgPath) const override;

    void strokePath(CGContextRef ctx, CGPathRef cgPath, float strokeWidth, const gmpi::drawing::api::IStrokeStyle* strokeStyle = nullptr) const override
    {
        Gradient::strokePath(ctx, cgPath, strokeWidth, strokeStyle);
    }

	gmpi::ReturnCode queryInterface(const gmpi::api::Guid* iid, void** returnInterface) override
	{
		*returnInterface = {};
        GMPI_QUERYINTERFACE(gmpi::drawing::api::IResource);
        GMPI_QUERYINTERFACE(gmpi::drawing::api::IRadialGradientBrush);
        return gmpi::ReturnCode::NoSupport;
	}

	gmpi::ReturnCode getFactory(gmpi::drawing::api::IFactory** factory) override
    {
        *factory = factory_;
        return gmpi::ReturnCode::Ok;
    }

    GMPI_REFCOUNT;
};

class GeometrySink final : public se::generic_graphics::GeometrySink
{
	CGMutablePathRef geometry_;

public:
	GeometrySink(CGMutablePathRef geometry) : geometry_(geometry)
	{}

	void setFillMode(gmpi::drawing::FillMode fillMode) override
	{
		// Fill mode is applied at fill time via CGContext, not stored on the path.
        // We store it in the PathGeometry instead.
	}

	void beginFigure(gmpi::drawing::Point startPoint, gmpi::drawing::FigureBegin figureBegin) override
	{
		CGPathMoveToPoint(geometry_, nullptr, startPoint.x, startPoint.y);
		lastPoint = startPoint;
	}

	void endFigure(gmpi::drawing::FigureEnd figureEnd) override
	{
		if (figureEnd == gmpi::drawing::FigureEnd::Closed)
		{
			CGPathCloseSubpath(geometry_);
		}
	}

	void addLine(gmpi::drawing::Point point) override
	{
		CGPathAddLineToPoint(geometry_, nullptr, point.x, point.y);
		lastPoint = point;
	}

	void addBezier(const gmpi::drawing::BezierSegment* bezier) override
	{
		CGPathAddCurveToPoint(geometry_, nullptr,
			bezier->point1.x, bezier->point1.y,
			bezier->point2.x, bezier->point2.y,
			bezier->point3.x, bezier->point3.y);
		lastPoint = bezier->point3;
	}

	GMPI_QUERYINTERFACE_METHOD(gmpi::drawing::api::IGeometrySink);
	GMPI_REFCOUNT;
};


class PathGeometry final : public gmpi::drawing::api::IPathGeometry
{
	CGMutablePathRef geometry_ = nullptr;

    gmpi::drawing::FillMode fillMode_ = gmpi::drawing::FillMode::Alternate;
    gmpi::drawing::DashStyle currentDashStyle = gmpi::drawing::DashStyle::Solid;
	std::vector<float> currentCustomDashStyle;
	float currentDashPhase = {};

public:
	PathGeometry()
	{
	}

	~PathGeometry()
	{
		if (geometry_)
		{
			CGPathRelease(geometry_);
		}
	}

	inline CGPathRef native() const
	{
		return geometry_;
	}

    gmpi::drawing::FillMode getFillMode() const { return fillMode_; }

	gmpi::ReturnCode open(gmpi::drawing::api::IGeometrySink** returnGeometrySink) override
	{
		if (geometry_)
		{
			CGPathRelease(geometry_);
		}
		geometry_ = CGPathCreateMutable();

		gmpi::shared_ptr<gmpi::api::IUnknown> b2;
		auto sink = new GeometrySink(geometry_);
		b2.attach(sink);

		return b2->queryInterface(&gmpi::drawing::api::IGeometrySink::guid, reinterpret_cast<void**>(returnGeometrySink));
	}

	gmpi::ReturnCode getFactory(gmpi::drawing::api::IFactory** factory) override
	{
        return gmpi::ReturnCode::NoSupport;
    }

	gmpi::ReturnCode strokeContainsPoint(gmpi::drawing::Point point, float strokeWidth, gmpi::drawing::api::IStrokeStyle* strokeStyle, const gmpi::drawing::Matrix3x2* worldTransform, bool* returnContains) override
	{
		CGPathRef hitTargetPath = CGPathCreateCopyByStrokingPath(geometry_, NULL, (CGFloat)strokeWidth,
            cgLineCapFromStyle(strokeStyle), cgLineJoinFromStyle(strokeStyle), 10.0);

		CGPoint cgpoint = CGPointMake(point.x, point.y);
		bool useEOFill = (fillMode_ == gmpi::drawing::FillMode::Alternate);
		*returnContains = (bool)CGPathContainsPoint(hitTargetPath, NULL, cgpoint, useEOFill);

		CGPathRelease(hitTargetPath);
		return gmpi::ReturnCode::Ok;
	}

	gmpi::ReturnCode fillContainsPoint(gmpi::drawing::Point point, const gmpi::drawing::Matrix3x2* worldTransform, bool* returnContains) override
	{
		bool useEOFill = (fillMode_ == gmpi::drawing::FillMode::Alternate);
		*returnContains = CGPathContainsPoint(geometry_, NULL, CGPointMake(point.x, point.y), useEOFill);
		return gmpi::ReturnCode::Ok;
	}

	gmpi::ReturnCode getWidenedBounds(float strokeWidth, gmpi::drawing::api::IStrokeStyle* strokeStyle, const gmpi::drawing::Matrix3x2* worldTransform, gmpi::drawing::Rect* returnBounds) override
	{
		const float radius = ceilf(strokeWidth * 0.5f);
		auto nativeRect = CGPathGetBoundingBox(geometry_);
		returnBounds->left = nativeRect.origin.x - radius;
		returnBounds->top = nativeRect.origin.y - radius;
		returnBounds->right = nativeRect.origin.x + nativeRect.size.width + radius;
		returnBounds->bottom = nativeRect.origin.y + nativeRect.size.height + radius;

		return gmpi::ReturnCode::Ok;
	}

	GMPI_QUERYINTERFACE_METHOD(gmpi::drawing::api::IPathGeometry);
	GMPI_REFCOUNT;
};

struct ContextInfo
{
	std::vector<gmpi::drawing::Rect> clipRectStack;
#if TARGET_OS_OSX
    // SDK3 backward compatibility — legacy code sends Obj-C messages to this.
    NSAffineTransform* currentTransform = nullptr;
    NSView* view = nullptr;
#endif
};

class GraphicsContext : public gmpi::drawing::api::IDeviceContext
{
protected:
    cocoa::Factory* factory{};
    ContextInfo info;
    CGContextRef cgContext_ = nullptr;
    CGAffineTransform cgCurrentTransform_ = CGAffineTransformIdentity;

    // For macOS, we need the view to get DPI for mask rendering
#if TARGET_OS_OSX
    NSView* view_ = nullptr;
#endif

public:
    inline static int logicProFix = -1;

#if TARGET_OS_OSX
	GraphicsContext(NSView* pview, cocoa::Factory* pfactory) :
		factory(pfactory)
    {
        view_ = pview;
        info.view = pview;
    }
#endif

    GraphicsContext(cocoa::Factory* pfactory) :
        factory(pfactory)
    {
    }

	~GraphicsContext()
	{
    }

    // Set the CGContext for this graphics context
    void setCGContext(CGContextRef ctx)
    {
        cgContext_ = ctx;
    }

    CGContextRef getCGContext() const
    {
        return cgContext_;
    }

	gmpi::ReturnCode getFactory(gmpi::drawing::api::IFactory** pfactory) override
	{
		*pfactory = factory;
		return gmpi::ReturnCode::Ok;
	}

	gmpi::ReturnCode drawRectangle(const gmpi::drawing::Rect* rect, gmpi::drawing::api::IBrush* brush, float strokeWidth, gmpi::drawing::api::IStrokeStyle* strokeStyle) override
	{
		CGMutablePathRef path = CGPathCreateMutable();
		CGPathAddRect(path, nullptr, cocoa::CGRectFromRect(*rect));

		auto cocoabrush = dynamic_cast<const CocoaBrushBase*>(brush);
		if (cocoabrush)
		{
			cocoabrush->strokePath(cgContext_, path, strokeWidth, strokeStyle);
		}
		CGPathRelease(path);
		return gmpi::ReturnCode::Ok;
	}

	gmpi::ReturnCode fillRectangle(const gmpi::drawing::Rect* rect, gmpi::drawing::api::IBrush* brush) override
	{
		CGMutablePathRef rectPath = CGPathCreateMutable();
		CGPathAddRect(rectPath, nullptr, cocoa::CGRectFromRect(*rect));

		auto cocoabrush = dynamic_cast<const CocoaBrushBase*>(brush);
		if (cocoabrush)
		{
			cocoabrush->fillPath(this, rectPath);
		}
		CGPathRelease(rectPath);
		return gmpi::ReturnCode::Ok;
	}

	gmpi::ReturnCode clear(const gmpi::drawing::Color* clearColor) override
	{
		SolidColorBrush brush(clearColor, factory);
        gmpi::drawing::Rect r;
		getAxisAlignedClip(&r);
		CGMutablePathRef rectPath = CGPathCreateMutable();
		CGPathAddRect(rectPath, nullptr, CGRectFromRect(r));
		brush.fillPath(this, rectPath);
		CGPathRelease(rectPath);
		return gmpi::ReturnCode::Ok;
	}

	gmpi::ReturnCode drawLine(gmpi::drawing::Point point0, gmpi::drawing::Point point1, gmpi::drawing::api::IBrush* brush, float strokeWidth, gmpi::drawing::api::IStrokeStyle* strokeStyle) override
	{
		CGMutablePathRef path = CGPathCreateMutable();
		CGPathMoveToPoint(path, nullptr, point0.x, point0.y);
		CGPathAddLineToPoint(path, nullptr, point1.x, point1.y);

		auto cocoabrush = dynamic_cast<const CocoaBrushBase*>(brush);
		if (cocoabrush)
		{
			cocoabrush->strokePath(cgContext_, path, strokeWidth, strokeStyle);
		}
		CGPathRelease(path);
		return gmpi::ReturnCode::Ok;
	}

	gmpi::ReturnCode drawGeometry(gmpi::drawing::api::IPathGeometry* pathGeometry, gmpi::drawing::api::IBrush* brush, float strokeWidth, gmpi::drawing::api::IStrokeStyle* strokeStyle) override
	{
		auto pg = (PathGeometry*)pathGeometry;

		auto cocoabrush = dynamic_cast<const CocoaBrushBase*>(brush);
		if (cocoabrush)
		{
			cocoabrush->strokePath(cgContext_, pg->native(), strokeWidth, strokeStyle);
		}
		return gmpi::ReturnCode::Ok;
	}

	gmpi::ReturnCode fillGeometry(gmpi::drawing::api::IPathGeometry* pathGeometry, gmpi::drawing::api::IBrush* brush, gmpi::drawing::api::IBrush* opacityBrush) override
	{
		auto cocoabrush = dynamic_cast<const CocoaBrushBase*>(brush);
		if (cocoabrush)
		{
			cocoabrush->fillPath(this, ((PathGeometry*)pathGeometry)->native());
		}
		return gmpi::ReturnCode::Ok;
	}

	inline float truncatePixel(float y, float stepSize)
	{
		return floor(y / stepSize) * stepSize;
	}

	inline float roundPixel(float y, float stepSize)
	{
		return floor(y / stepSize + 0.5f) * stepSize;
	}

	gmpi::ReturnCode drawTextU(const char* string, uint32_t stringLength, gmpi::drawing::api::ITextFormat* textFormat, const gmpi::drawing::Rect* layoutRect, gmpi::drawing::api::IBrush* brush, int32_t options) override
	{
		auto textformat = reinterpret_cast<const TextFormat*>(textFormat);

        auto cocoabrush = dynamic_cast<const CocoaBrushBase*>(brush);
        if (!cocoabrush)
            return gmpi::ReturnCode::Fail;

		CGRect bounds = CGRectMake(layoutRect->left, layoutRect->top, layoutRect->right - layoutRect->left, layoutRect->bottom - layoutRect->top);

        gmpi::drawing::Size textSize{};
		if (textformat->paragraphAlignment != gmpi::drawing::ParagraphAlignment::Near
			|| options != (int32_t) gmpi::drawing::DrawTextOptions::Clip)
		{
			const_cast<gmpi::drawing::api::ITextFormat*>(textFormat)->getTextExtentU(string, (int32_t)strlen(string), &textSize);
		}

		if (textformat->paragraphAlignment != gmpi::drawing::ParagraphAlignment::Near)
		{
			switch (textformat->paragraphAlignment)
			{
                case gmpi::drawing::ParagraphAlignment::Far:
				bounds.origin.y += bounds.size.height - textSize.height;
				bounds.size.height = textSize.height;
				break;

                case gmpi::drawing::ParagraphAlignment::Center:
				bounds.origin.y += (bounds.size.height - textSize.height) / 2;
				bounds.size.height = textSize.height;
				break;

			default:
				break;
			}
		}

		const bool clipToRect = options & gmpi::drawing::DrawTextOptions::Clip;

		if (!clipToRect && textSize.height > bounds.size.height)
		{
			bounds.size.height = textSize.height;
		}

		if (textformat->wordWrapping == gmpi::drawing::WordWrapping::NoWrap)
		{
			if (!clipToRect)
			{
				const auto extendWidth = static_cast<double>(textSize.width) - bounds.size.width;
				if (extendWidth > 0.0)
				{
					bounds.size.width += extendWidth;

					switch (textformat->textAlignment)
					{
                        case gmpi::drawing::TextAlignment::Center:
						bounds.origin.x -= 0.5 * extendWidth;
						break;
                        case gmpi::drawing::TextAlignment::Trailing:
						bounds.origin.x -= extendWidth;
                        case gmpi::drawing::TextAlignment::Leading:
					default:
						break;
					}
				}
			}
		}

		{
			const float effectiveLayoutTop = static_cast<float>(bounds.origin.y);

			const float effectiveTopAdj = (textformat->lineSpacing_ < 0.f)
				? textformat->winNaturalTopAdjustment_
				: textformat->topAdjustment;
			bounds.origin.y -= effectiveTopAdj;
			bounds.size.height += effectiveTopAdj;

			if (textformat->lineSpacing_ >= 0.f)
			{
				bounds.origin.y += textformat->customBaseline_ - textformat->ascent;
			}

#if 1
			const float scale = 0.5f;

			const float effectiveAscent = (textformat->lineSpacing_ >= 0.f)
				? textformat->customBaseline_
				: textformat->ascent;

			float winBaseline{};
			{
				const float offset = -0.25f;;
				winBaseline = effectiveLayoutTop + effectiveAscent;
				winBaseline = floorf((offset + winBaseline) / scale) * scale;
			}
            const float baseline = effectiveLayoutTop + effectiveAscent;
            float macBaselineCorrection{};
            if (logicProFix)
            {
                const float naturalSpacingFix = (textformat->lineSpacing_ >= 0.f) ? 0.f : 1.f;
                macBaselineCorrection = winBaseline - baseline - textformat->baselineCorrection + naturalSpacingFix;
            }
            else
            {
                macBaselineCorrection = winBaseline - baseline + textformat->baselineCorrection;
            }

			bounds.origin.y += macBaselineCorrection;
#endif
		}

        // Build CoreText attributed string for drawing
        auto scb = dynamic_cast<const SolidColorBrush*>(brush);
        auto lgb = dynamic_cast<const Gradient*>(brush);
        auto bmb = dynamic_cast<const BitmapBrush*>(brush);

        // For natural spacing, temporarily apply the Windows-equivalent line height
        CGFloat savedMin = textformat->ctLineHeightMin_;
        CGFloat savedMax = textformat->ctLineHeightMax_;
        if (textformat->lineSpacing_ < 0.f)
        {
            const_cast<TextFormat*>(textformat)->ctLineHeightMin_ = textformat->winNaturalLineHeight_;
            const_cast<TextFormat*>(textformat)->ctLineHeightMax_ = textformat->winNaturalLineHeight_;
            const_cast<TextFormat*>(textformat)->rebuildParagraphStyle();
        }

        if (scb)
        {
            // Simple solid color text drawing
            CFDictionaryRef attrs = textformat->createAttributesWithColor(scb->nativeColor());
            CFStringRef cfStr = CFStringCreateWithBytes(kCFAllocatorDefault,
                (const UInt8*)string, stringLength, kCFStringEncodingUTF8, false);
            CFAttributedStringRef attrStr = CFAttributedStringCreate(kCFAllocatorDefault, cfStr, attrs);

            CTFramesetterRef framesetter = CTFramesetterCreateWithAttributedString(attrStr);
            CGMutablePathRef textPath = CGPathCreateMutable();
            CGPathAddRect(textPath, nullptr, bounds);
            CTFrameRef frame = CTFramesetterCreateFrame(framesetter, CFRangeMake(0, 0), textPath, nullptr);

            CGContextSaveGState(cgContext_);

            // CoreText draws in bottom-up coordinates. We're in a flipped (top-down) context,
            // so flip just for text rendering.
            CGContextTranslateCTM(cgContext_, 0, bounds.origin.y + bounds.size.height);
            CGContextScaleCTM(cgContext_, 1.0, -1.0);
            CGContextTranslateCTM(cgContext_, 0, -bounds.origin.y);

            // Disable font smoothing to match Windows Direct2D text weight.
            CGContextSetShouldSmoothFonts(cgContext_, false);

            CTFrameDraw(frame, cgContext_);

            CGContextRestoreGState(cgContext_);

            CFRelease(frame);
            CGPathRelease(textPath);
            CFRelease(framesetter);
            CFRelease(attrStr);
            CFRelease(cfStr);
            CFRelease(attrs);
        }
        else if (lgb || bmb)
        {
            // Gradient or bitmap brush text: render white text into a mask, then clip & draw
            CGColorRef white = CGColorCreateSRGB(1, 1, 1, 1);
            CFDictionaryRef attrs = textformat->createAttributesWithColor(white);
            CFStringRef cfStr = CFStringCreateWithBytes(kCFAllocatorDefault,
                (const UInt8*)string, stringLength, kCFStringEncodingUTF8, false);
            CFAttributedStringRef attrStr = CFAttributedStringCreate(kCFAllocatorDefault, cfStr, attrs);

            CTFramesetterRef framesetter = CTFramesetterCreateWithAttributedString(attrStr);
            CGMutablePathRef textPath = CGPathCreateMutable();
            CGPathAddRect(textPath, nullptr, bounds);
            CTFrameRef frame = CTFramesetterCreateFrame(framesetter, CFRangeMake(0, 0), textPath, nullptr);

            // Determine physical pixel size for the mask
            CGSize logicalSize = bounds.size;
            CGSize physicalSize = logicalSize;
#if TARGET_OS_OSX
            if (view_) {
                NSRect lr = NSMakeRect(0, 0, logicalSize.width, logicalSize.height);
                physicalSize = [view_ convertRectToBacking:lr].size;
            }
#endif

            // Create grayscale mask context
            CGColorSpaceRef graySpace = CGColorSpaceCreateDeviceGray();
            CGContextRef maskCtx = CGBitmapContextCreate(NULL,
                (size_t)physicalSize.width, (size_t)physicalSize.height,
                8, (size_t)physicalSize.width, graySpace, 0);
            CGColorSpaceRelease(graySpace);

            if (maskCtx)
            {
                if (physicalSize.width != logicalSize.width) {
                    CGFloat s = physicalSize.width / logicalSize.width;
                    CGContextScaleCTM(maskCtx, s, s);
                }

                CGContextSetShouldSmoothFonts(maskCtx, false);

                // CoreText draws bottom-up; set up coords for the mask
                CGContextTranslateCTM(maskCtx, 0, bounds.size.height);
                CGContextScaleCTM(maskCtx, 1.0, -1.0);

                // Translate so drawing lands at (0,0) in the mask
                CGContextTranslateCTM(maskCtx, -bounds.origin.x, -bounds.origin.y + bounds.origin.y);
                // Actually we need to draw at origin in the mask's local coords
                CGMutablePathRef maskTextPath = CGPathCreateMutable();
                CGPathAddRect(maskTextPath, nullptr, CGRectMake(0, 0, bounds.size.width, bounds.size.height));
                CTFrameRef maskFrame = CTFramesetterCreateFrame(framesetter, CFRangeMake(0, 0), maskTextPath, nullptr);
                CTFrameDraw(maskFrame, maskCtx);
                CFRelease(maskFrame);
                CGPathRelease(maskTextPath);

                CGImageRef alphaMask = CGBitmapContextCreateImage(maskCtx);

                CGContextSaveGState(cgContext_);
                CGContextClipToMask(cgContext_, bounds, alphaMask);

                if (bmb) {
                    CGMutablePathRef fillPath = CGPathCreateMutable();
                    CGPathAddRect(fillPath, nullptr, bounds);
                    bmb->fillPath(this, fillPath);
                    CGPathRelease(fillPath);
                } else if (lgb) {
                    lgb->drawGradient(cgContext_);
                }

                CGContextRestoreGState(cgContext_);
                CGImageRelease(alphaMask);
                CGContextRelease(maskCtx);
            }

            CFRelease(frame);
            CGPathRelease(textPath);
            CFRelease(framesetter);
            CFRelease(attrStr);
            CFRelease(cfStr);
            CFRelease(attrs);
            CGColorRelease(white);
        }

        // Restore natural line spacing after drawing.
        if (textformat->lineSpacing_ < 0.f)
        {
            const_cast<TextFormat*>(textformat)->ctLineHeightMin_ = savedMin;
            const_cast<TextFormat*>(textformat)->ctLineHeightMax_ = savedMax;
            const_cast<TextFormat*>(textformat)->rebuildParagraphStyle();
        }

        return gmpi::ReturnCode::Ok;
	}

	gmpi::ReturnCode drawBitmap(gmpi::drawing::api::IBitmap* mpBitmap, const gmpi::drawing::Rect* destinationRectangle, float opacity, gmpi::drawing::BitmapInterpolationMode interpolationMode, const gmpi::drawing::Rect* sourceRectangle) override
	{
		auto bm = ((Bitmap*)mpBitmap);
		auto bitmap = bm->getNativeBitmap();

        gmpi::drawing::SizeU imageSize;
        bm->getSizeU(&imageSize);

        CGRect destRect = cocoa::CGRectFromRect(*destinationRectangle);

        // Set interpolation quality
        CGInterpolationQuality prevInterp = CGContextGetInterpolationQuality(cgContext_);
        CGContextSetInterpolationQuality(cgContext_,
            (interpolationMode == gmpi::drawing::BitmapInterpolationMode::NearestNeighbor)
            ? kCGInterpolationNone : kCGInterpolationHigh);

#if USE_BACKING_BUFFER
        // Source rect in CGImage coordinates (bottom-up)
        CGRect sourceRect = cocoa::CGRectFromRect(*sourceRectangle);
        sourceRect.origin.y = imageSize.height - (sourceRect.origin.y + sourceRect.size.height);

        CGContextSaveGState(cgContext_);

        // Flip for drawing the image (CGImage is bottom-up, our context is top-down)
        CGContextTranslateCTM(cgContext_, 0, destRect.origin.y * 2 + destRect.size.height);
        CGContextScaleCTM(cgContext_, 1, -1);
#else
        CGRect sourceRect = cocoa::CGRectFromRect(*sourceRectangle);
        sourceRect.origin.y = imageSize.height - sourceRectangle->bottom;

        CGContextSaveGState(cgContext_);
#endif

        if (bitmap)
        {
            // If source rect covers the whole image, draw directly.
            // Otherwise, create a sub-image.
            if (sourceRect.origin.x == 0 && sourceRect.origin.y == 0 &&
                sourceRect.size.width == imageSize.width && sourceRect.size.height == imageSize.height)
            {
                CGContextSetAlpha(cgContext_, opacity);
                CGContextDrawImage(cgContext_, destRect, bitmap);
            }
            else
            {
                CGImageRef subImage = CGImageCreateWithImageInRect(bitmap, sourceRect);
                if (subImage)
                {
                    CGContextSetAlpha(cgContext_, opacity);
                    CGContextDrawImage(cgContext_, destRect, subImage);
                    CGImageRelease(subImage);
                }
            }
        }

        if (bm->additiveBitmap_)
        {
            CGContextSetBlendMode(cgContext_, kCGBlendModePlusLighter);
            CGImageRef subImage = CGImageCreateWithImageInRect(bm->additiveBitmap_, sourceRect);
            if (subImage) {
                CGContextSetAlpha(cgContext_, opacity);
                CGContextDrawImage(cgContext_, destRect, subImage);
                CGImageRelease(subImage);
            }
            CGContextSetBlendMode(cgContext_, kCGBlendModeNormal);
        }

        CGContextRestoreGState(cgContext_);

        CGContextSetInterpolationQuality(cgContext_, prevInterp);

        return gmpi::ReturnCode::Ok;
	}

	gmpi::ReturnCode setTransform(const gmpi::drawing::Matrix3x2* transform) override
	{
		// Remove the current transform by applying the inverse
        CGAffineTransform inverse = CGAffineTransformInvert(cgCurrentTransform_);
        CGContextConcatCTM(cgContext_, inverse);

        cgCurrentTransform_ = CGAffineTransformMake(
            transform->_11, transform->_12,
            transform->_21, transform->_22,
            transform->_31, transform->_32);

        CGContextConcatCTM(cgContext_, cgCurrentTransform_);
		return gmpi::ReturnCode::Ok;
	}

	gmpi::ReturnCode getTransform(gmpi::drawing::Matrix3x2* transform) override
	{
		transform->_11 = cgCurrentTransform_.a;
		transform->_12 = cgCurrentTransform_.b;
		transform->_21 = cgCurrentTransform_.c;
		transform->_22 = cgCurrentTransform_.d;
		transform->_31 = cgCurrentTransform_.tx;
		transform->_32 = cgCurrentTransform_.ty;
		return gmpi::ReturnCode::Ok;
	}

	gmpi::ReturnCode createSolidColorBrush(const gmpi::drawing::Color* color, const gmpi::drawing::BrushProperties* brushProperties, gmpi::drawing::api::ISolidColorBrush** returnSolidColorBrush) override
	{
		gmpi::shared_ptr<gmpi::api::IUnknown> b2;
		b2.attach(new SolidColorBrush(color, factory));

		return b2->queryInterface(&gmpi::drawing::api::ISolidColorBrush::guid, reinterpret_cast<void**>(returnSolidColorBrush));
	}

	gmpi::ReturnCode createGradientstopCollection(const gmpi::drawing::Gradientstop* gradientstops, uint32_t gradientstopsCount, gmpi::drawing::ExtendMode extendMode, gmpi::drawing::api::IGradientstopCollection** returnGradientstopCollection) override
	{
		gmpi::shared_ptr<gmpi::api::IUnknown> b2;
		b2.attach(new GradientstopCollection(factory, gradientstops, gradientstopsCount));

		return b2->queryInterface(&gmpi::drawing::api::IGradientstopCollection::guid, reinterpret_cast<void**>(returnGradientstopCollection));
	}

	gmpi::ReturnCode createLinearGradientBrush(const gmpi::drawing::LinearGradientBrushProperties* linearGradientBrushProperties, const gmpi::drawing::BrushProperties* brushProperties, gmpi::drawing::api::IGradientstopCollection* gradientstopCollection, gmpi::drawing::api::ILinearGradientBrush** returnLinearGradientBrush) override
	{
		gmpi::shared_ptr<gmpi::api::IUnknown> b2;
		b2.attach(new LinearGradientBrush(factory, linearGradientBrushProperties, brushProperties, gradientstopCollection));

		return b2->queryInterface(&gmpi::drawing::api::ILinearGradientBrush::guid, reinterpret_cast<void**>(returnLinearGradientBrush));
	}

	gmpi::ReturnCode createBitmapBrush(gmpi::drawing::api::IBitmap* bitmap, const gmpi::drawing::BrushProperties* brushProperties, gmpi::drawing::api::IBitmapBrush** returnBitmapBrush) override
	{
		*returnBitmapBrush = nullptr;
		gmpi::shared_ptr<gmpi::api::IUnknown> b2;
		b2.attach(new BitmapBrush(factory, bitmap, brushProperties));
		return b2->queryInterface(&gmpi::drawing::api::IBitmapBrush::guid, reinterpret_cast<void**>(returnBitmapBrush));
	}

	gmpi::ReturnCode createRadialGradientBrush(const gmpi::drawing::RadialGradientBrushProperties* radialGradientBrushProperties, const gmpi::drawing::BrushProperties* brushProperties, gmpi::drawing::api::IGradientstopCollection* gradientstopCollection, gmpi::drawing::api::IRadialGradientBrush** returnRadialGradientBrush) override
	{
		gmpi::shared_ptr<gmpi::api::IUnknown> b2;
		b2.attach(new RadialGradientBrush(factory, radialGradientBrushProperties, brushProperties, gradientstopCollection));

		return b2->queryInterface(&gmpi::drawing::api::IRadialGradientBrush::guid, reinterpret_cast<void**>(returnRadialGradientBrush));
	}

	gmpi::ReturnCode createCompatibleRenderTarget(gmpi::drawing::Size desiredSize, int32_t flags, gmpi::drawing::api::IBitmapRenderTarget** bitmapRenderTarget) override;

	gmpi::ReturnCode drawRoundedRectangle(const gmpi::drawing::RoundedRect* roundedRect, gmpi::drawing::api::IBrush* brush, float strokeWidth, gmpi::drawing::api::IStrokeStyle* strokeStyle) override
	{
		CGRect r = cocoa::CGRectFromRect(roundedRect->rect);
		CGMutablePathRef path = CGPathCreateMutable();
		CGPathAddRoundedRect(path, nullptr, r, roundedRect->radiusX, roundedRect->radiusY);

		auto cocoabrush = dynamic_cast<const CocoaBrushBase*>(brush);
		if (cocoabrush)
		{
			cocoabrush->strokePath(cgContext_, path, strokeWidth, strokeStyle);
		}
		CGPathRelease(path);
		return gmpi::ReturnCode::Ok;
	}

	gmpi::ReturnCode fillRoundedRectangle(const gmpi::drawing::RoundedRect* roundedRect, gmpi::drawing::api::IBrush* brush) override
	{
		CGRect r = cocoa::CGRectFromRect(roundedRect->rect);
		CGMutablePathRef rectPath = CGPathCreateMutable();
		CGPathAddRoundedRect(rectPath, nullptr, r, roundedRect->radiusX, roundedRect->radiusY);

		auto cocoabrush = dynamic_cast<const CocoaBrushBase*>(brush);
		if (cocoabrush)
		{
			cocoabrush->fillPath(this, rectPath);
		}
		CGPathRelease(rectPath);
		return gmpi::ReturnCode::Ok;
	}

	gmpi::ReturnCode drawEllipse(const gmpi::drawing::Ellipse* ellipse, gmpi::drawing::api::IBrush* brush, float strokeWidth, gmpi::drawing::api::IStrokeStyle* strokeStyle) override
	{
		CGRect r = CGRectMake(ellipse->point.x - ellipse->radiusX, ellipse->point.y - ellipse->radiusY, ellipse->radiusX * 2.0f, ellipse->radiusY * 2.0f);
		CGMutablePathRef path = CGPathCreateMutable();
		CGPathAddEllipseInRect(path, nullptr, r);

		auto cocoabrush = dynamic_cast<const CocoaBrushBase*>(brush);
		if (cocoabrush)
		{
			cocoabrush->strokePath(cgContext_, path, strokeWidth, strokeStyle);
		}
		CGPathRelease(path);
		return gmpi::ReturnCode::Ok;
	}

	gmpi::ReturnCode fillEllipse(const gmpi::drawing::Ellipse* ellipse, gmpi::drawing::api::IBrush* brush) override
	{
		CGRect r = CGRectMake(ellipse->point.x - ellipse->radiusX, ellipse->point.y - ellipse->radiusY, ellipse->radiusX * 2.0f, ellipse->radiusY * 2.0f);
		CGMutablePathRef rectPath = CGPathCreateMutable();
		CGPathAddEllipseInRect(rectPath, nullptr, r);

		auto cocoabrush = dynamic_cast<const CocoaBrushBase*>(brush);
		if (cocoabrush)
		{
			cocoabrush->fillPath(this, rectPath);
		}
		CGPathRelease(rectPath);
		return gmpi::ReturnCode::Ok;
	}

	gmpi::ReturnCode pushAxisAlignedClip(const gmpi::drawing::Rect* clipRect) override
    {
        gmpi::drawing::Matrix3x2 currentTransform;
        getTransform(&currentTransform);
        auto absClipRect = transformRect(currentTransform, *clipRect);

        if (!info.clipRectStack.empty())
            absClipRect = intersectRect(absClipRect, info.clipRectStack.back());

        info.clipRectStack.push_back(absClipRect);

        CGContextSaveGState(cgContext_);
        CGContextClipToRect(cgContext_, CGRectFromRect(*clipRect));

        return gmpi::ReturnCode::Ok;
	}

	gmpi::ReturnCode popAxisAlignedClip() override
	{
        info.clipRectStack.pop_back();
		CGContextRestoreGState(cgContext_);
		return gmpi::ReturnCode::Ok;
	}

    gmpi::ReturnCode getAxisAlignedClip(gmpi::drawing::Rect* returnClipRect) override
	{
        gmpi::drawing::Matrix3x2 currentTransform;
		getTransform(&currentTransform);
		currentTransform = invert(currentTransform);
		*returnClipRect = transformRect(currentTransform, info.clipRectStack.back());
        return gmpi::ReturnCode::Ok;
	}

	gmpi::ReturnCode beginDraw() override
	{
		return gmpi::ReturnCode::Ok;
	}

	gmpi::ReturnCode endDraw() override
	{
		return gmpi::ReturnCode::Ok;
	}

#if TARGET_OS_OSX
	NSView* getNativeView()
	{
		return view_;
	}
#endif

	GMPI_QUERYINTERFACE_METHOD(gmpi::drawing::api::IDeviceContext);
	GMPI_REFCOUNT_NO_DELETE;
};

// Implementations that need GraphicsContext to be fully defined

inline void SolidColorBrush::fillPath(GraphicsContext* context, CGPathRef cgPath) const
{
    CGContextRef ctx = context->getCGContext();
    CGContextSaveGState(ctx);
    CGContextSetFillColorWithColor(ctx, nativec_);
    CGContextAddPath(ctx, cgPath);
    CGContextFillPath(ctx);
    CGContextRestoreGState(ctx);
}

inline void LinearGradientBrush::fillPath(GraphicsContext* context, CGPathRef cgPath) const
{
    Gradient::fillPath(context->getCGContext(), cgPath);
}

inline void RadialGradientBrush::fillPath(GraphicsContext* context, CGPathRef cgPath) const
{
    Gradient::fillPath(context->getCGContext(), cgPath);
}

// macOS-internal creationFlags extension (not part of the public BitmapRenderTargetFlags API).
static constexpr int32_t kMacFloatRT = 0x80;

class BitmapRenderTarget : public GraphicsContext
{
	CGContextRef backingContext_ = nullptr;
    int32_t creationFlags = (int32_t)drawing::BitmapRenderTargetFlags::SRGBPixels;
    CGFloat width_ = 0;
    CGFloat height_ = 0;

public:
	BitmapRenderTarget(const gmpi::drawing::Size* size, int32_t flags, cocoa::Factory* pfactory) :
		GraphicsContext(pfactory)
        ,creationFlags(flags)
        ,width_(size->width)
        ,height_(size->height)
	{
        const bool oneChannelMask = flags & (int32_t)gmpi::drawing::BitmapRenderTargetFlags::Mask;
        const bool eightBitPixels = flags & (int32_t)gmpi::drawing::BitmapRenderTargetFlags::SRGBPixels;

        size_t w = (size_t)size->width;
        size_t h = (size_t)size->height;

        if(oneChannelMask)
        {
            CGColorSpaceRef graySpace = CGColorSpaceCreateDeviceGray();
            backingContext_ = CGBitmapContextCreate(NULL, w, h, 8, w, graySpace, kCGImageAlphaNone);
            CGColorSpaceRelease(graySpace);
        }
        else if(eightBitPixels)
        {
            CGColorSpaceRef srgb = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
            backingContext_ = CGBitmapContextCreate(NULL, w, h, 8, 0, srgb,
                kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big);
            CGColorSpaceRelease(srgb);
        }
        else
        {
            // 32-bit float linear sRGB for gamma-correct compositing
            backingContext_ = CGBitmapContextCreate(NULL, w, h, 32, 0,
                pfactory->info.cgColorSpace,
                kCGImageAlphaPremultipliedLast | kCGBitmapFloatComponents | kCGBitmapByteOrder32Host);
            creationFlags |= kMacFloatRT;
        }

        // Zero-initialize pixel data
        if (backingContext_)
        {
            size_t bpr = CGBitmapContextGetBytesPerRow(backingContext_);
            memset(CGBitmapContextGetData(backingContext_), 0, bpr * h);
        }

        info.clipRectStack.push_back({ 0, 0, size->width, size->height });
	}

    gmpi::ReturnCode beginDraw() override
	{
        cgContext_ = backingContext_;

		// Flip coordinate system to match Direct2D (top-down).
        CGContextSaveGState(cgContext_);
        CGContextTranslateCTM(cgContext_, 0, height_);
        CGContextScaleCTM(cgContext_, 1.0, -1.0);

		return GraphicsContext::beginDraw();
	}

	gmpi::ReturnCode endDraw() override
	{
		auto r = GraphicsContext::endDraw();
        CGContextRestoreGState(cgContext_);
        cgContext_ = nullptr;
		return r;
	}

	~BitmapRenderTarget()
	{
        if (backingContext_) CGContextRelease(backingContext_);
	}

	// MUST BE FIRST VIRTUAL FUNCTION!
	virtual gmpi::ReturnCode getBitmap(gmpi::drawing::api::IBitmap** returnBitmap)
	{
        CGImageRef image = CGBitmapContextCreateImage(backingContext_);
		gmpi::shared_ptr<gmpi::api::IUnknown> b;
		b.attach(new Bitmap(factory, image, creationFlags));
        CGImageRelease(image);
		return b->queryInterface(&gmpi::drawing::api::IBitmap::guid, reinterpret_cast<void**>(returnBitmap));
	}

	gmpi::ReturnCode queryInterface(const gmpi::api::Guid* iid, void** returnInterface) override
	{
		*returnInterface = {};
        if (*iid == gmpi::drawing::api::IBitmapRenderTarget::guid)
        {
            *returnInterface = reinterpret_cast<gmpi::drawing::api::IBitmapRenderTarget*>(this);
            addRef();
            return gmpi::ReturnCode::Ok;
        }
		return GraphicsContext::queryInterface(iid, returnInterface);
	}

	GMPI_REFCOUNT;
};

inline gmpi::ReturnCode GraphicsContext::createCompatibleRenderTarget(gmpi::drawing::Size desiredSize, int32_t flags, gmpi::drawing::api::IBitmapRenderTarget** bitmapRenderTarget)
{
	gmpi::shared_ptr<gmpi::api::IUnknown> b2;
	b2.attach(new class BitmapRenderTarget(&desiredSize, flags, factory));

	return b2->queryInterface(&gmpi::drawing::api::IBitmapRenderTarget::guid, reinterpret_cast<void**>(bitmapRenderTarget));
}

inline gmpi::ReturnCode Factory::createTextFormat(const char* fontFamilyName, gmpi::drawing::FontWeight fontWeight, gmpi::drawing::FontStyle fontStyle, gmpi::drawing::FontStretch fontStretch, float fontSize, int32_t fontFlags, gmpi::drawing::api::ITextFormat** textFormat)
{
	gmpi::shared_ptr<gmpi::api::IUnknown> b2;
	b2.attach(new TextFormat(fontFamilyName, fontWeight, fontStyle, fontStretch, fontSize));

	return b2->queryInterface(&gmpi::drawing::api::ITextFormat::guid, reinterpret_cast<void**>(textFormat));
}

inline gmpi::ReturnCode Factory::createPathGeometry(gmpi::drawing::api::IPathGeometry** pathGeometry)
{
	gmpi::shared_ptr<gmpi::api::IUnknown> b2;
	b2.attach(new PathGeometry());

	return b2->queryInterface(&gmpi::drawing::api::IPathGeometry::guid, reinterpret_cast<void**>(pathGeometry));
}

inline gmpi::ReturnCode Factory::createImage(int32_t width, int32_t height, int32_t flags, gmpi::drawing::api::IBitmap** returnDiBitmap)
{
	*returnDiBitmap = nullptr;

	gmpi::shared_ptr<gmpi::api::IUnknown> bm;
	bm.attach(new Bitmap(this, width, height, flags));

	return bm->queryInterface(&gmpi::drawing::api::IBitmap::guid, (void**)returnDiBitmap);
}

inline gmpi::ReturnCode Factory::createCpuRenderTarget(gmpi::drawing::SizeU size, int32_t flags, gmpi::drawing::api::IBitmapRenderTarget** returnBitmapRenderTarget)
{
    gmpi::shared_ptr<gmpi::api::IUnknown> b2;
    gmpi::drawing::Size sizeF{ static_cast<float>(size.width), static_cast<float>(size.height) };
    b2.attach(new BitmapRenderTarget(&sizeF, flags, this));

    return b2->queryInterface(&gmpi::drawing::api::IBitmapRenderTarget::guid, reinterpret_cast<void**>(returnBitmapRenderTarget));
}

inline gmpi::ReturnCode Factory::loadImageU(const char* utf8Uri, gmpi::drawing::api::IBitmap** returnDiBitmap)
{
	*returnDiBitmap = nullptr;

	gmpi::shared_ptr<gmpi::api::IUnknown> bm;
	auto temp = new Bitmap(this, utf8Uri);
	bm.attach(temp);

	if (temp->isLoaded())
	{
		return bm->queryInterface(&gmpi::drawing::api::IBitmap::guid, (void**)returnDiBitmap);
	}

	return gmpi::ReturnCode::Fail;
}

inline void BitmapBrush::fillPath(GraphicsContext* context, CGPathRef cgPath) const
{
    CGContextRef cgCtx = context->getCGContext();
    CGContextSaveGState(cgCtx);

    CGContextAddPath(cgCtx, cgPath);
    CGContextClip(cgCtx);

    gmpi::drawing::Matrix3x2 moduleTransform;
    context->getTransform(&moduleTransform);
    auto offset = gmpi::drawing::transformPoint(moduleTransform, {0.0f, 0.0f});
    offset = gmpi::drawing::transformPoint(brushProperties_.transform, offset);

    const CGFloat tileW = CGImageGetWidth(patternImage_);
    const CGFloat tileH = CGImageGetHeight(patternImage_);
    const CGRect bounds = CGPathGetBoundingBox(cgPath);

    const CGFloat startX = offset.x + floor((CGRectGetMinX(bounds) - offset.x) / tileW) * tileW;
    const CGFloat startY = offset.y + floor((CGRectGetMinY(bounds) - offset.y) / tileH) * tileH;

    for (CGFloat y = startY; y < CGRectGetMaxY(bounds); y += tileH) {
        for (CGFloat x = startX; x < CGRectGetMaxX(bounds); x += tileW) {
            CGContextSaveGState(cgCtx);
            CGContextTranslateCTM(cgCtx, x, y + tileH);
            CGContextScaleCTM(cgCtx, 1.0, -1.0);
            CGContextDrawImage(cgCtx, CGRectMake(0, 0, tileW, tileH), patternImage_);
            CGContextRestoreGState(cgCtx);
        }
    }

    CGContextRestoreGState(cgCtx);
}

inline BitmapPixels::BitmapPixels(Bitmap* sebitmap, bool _alphaPremultiplied, int32_t pflags) :
	inImage_(&sebitmap->nativeBitmap_)
	, flags(pflags)
	, seBitmap(sebitmap)
{
    uint32_t w = sebitmap->pixelWidth_;
    uint32_t h = sebitmap->pixelHeight_;

    const bool isMask = 0 != (seBitmap->creationFlags & (int32_t)gmpi::drawing::BitmapRenderTargetFlags::Mask );
    const bool isSRGB = 0 != (seBitmap->creationFlags & (int32_t)gmpi::drawing::BitmapRenderTargetFlags::SRGBPixels );
    const bool wantLinearFloat = 0 != (seBitmap->creationFlags & kMacFloatRT);
    const bool wantHalfFloat   = !isMask && !isSRGB && !wantLinearFloat;

    if(isMask)
    {
        CGColorSpaceRef graySpace = CGColorSpaceCreateDeviceGray();
        bytesPerRow = w;
        pixelContext_ = CGBitmapContextCreate(NULL, w, h, 8, bytesPerRow, graySpace, kCGImageAlphaNone);
        CGColorSpaceRelease(graySpace);
    }
    else if (wantLinearFloat)
    {
        bytesPerRow = (int32_t)(w * 4 * sizeof(float));
        pixelContext_ = CGBitmapContextCreate(NULL, w, h, 32, bytesPerRow,
            static_cast<cocoa::Factory*>(sebitmap->factory)->info.cgColorSpace,
            kCGImageAlphaPremultipliedLast | kCGBitmapFloatComponents | kCGBitmapByteOrder32Host);
    }
    else if (wantHalfFloat)
    {
        // 64bppPRGBAHalf: use 16-bit per component bitmap context
        bytesPerRow = (int32_t)(w * 4 * sizeof(uint16_t));
        // CoreGraphics doesn't support 16-bit float directly for all operations,
        // so create as 16-bit integer and let callers treat as half-float
        CGColorSpaceRef cs = static_cast<cocoa::Factory*>(sebitmap->factory)->info.cgColorSpace;
        pixelContext_ = CGBitmapContextCreate(NULL, w, h, 16, bytesPerRow, cs,
            kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder16Host);
    }
    else
    {
        // 8-bit sRGB
        bytesPerRow = w * 4;
        CGColorSpaceRef srgb = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
        pixelContext_ = CGBitmapContextCreate(NULL, w, h, 8, bytesPerRow, srgb,
            kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big);
        CGColorSpaceRelease(srgb);
    }

    // Copy the image to the pixel buffer (read)
    if (pixelContext_ && 0 != (flags & (int) gmpi::drawing::BitmapLockFlags::Read))
    {
        if (*inImage_)
        {
            CGContextDrawImage(pixelContext_, CGRectMake(0, 0, w, h), *inImage_);
        }
    }
}

inline BitmapPixels::~BitmapPixels()
{
    const bool isMask        = 0 != (seBitmap->creationFlags & (int32_t)gmpi::drawing::BitmapRenderTargetFlags::Mask);
    const bool isSRGB        = 0 != (seBitmap->creationFlags & (int32_t)gmpi::drawing::BitmapRenderTargetFlags::SRGBPixels);
    const bool wantLinearFloat = 0 != (seBitmap->creationFlags & kMacFloatRT);
    const bool wantHalfFloat = !isMask && !isSRGB && !wantLinearFloat;

	if (!isMask && 0 != (flags & (int) gmpi::drawing::BitmapLockFlags::Write))
	{
        if (wantHalfFloat)
        {
            // Scan for "overbright" pixels
            auto h2f = [](uint16_t h) -> float {
                const uint32_t sign = (h >> 15) & 0x1u;
                const uint32_t exp  = (h >> 10) & 0x1fu;
                const uint32_t mant = h & 0x3ffu;
                uint32_t bits;
                if (exp == 0) {
                    if (mant == 0) { bits = sign << 31; }
                    else {
                        uint32_t e = 0, m = mant;
                        while (!(m & 0x400u)) { m <<= 1; ++e; }
                        bits = (sign << 31) | ((127 - 15 - e + 1) << 23) | ((m & 0x3ffu) << 13);
                    }
                } else if (exp == 31) {
                    bits = (sign << 31) | 0x7f800000u | (mant << 13);
                } else {
                    bits = (sign << 31) | ((exp + (127 - 15)) << 23) | (mant << 13);
                }
                float f; std::memcpy(&f, &bits, 4); return f;
            };

            const size_t w = seBitmap->pixelWidth_;
            const size_t h = seBitmap->pixelHeight_;
            const size_t srcBpr = CGBitmapContextGetBytesPerRow(pixelContext_);

            bool hasOverbright = false;
            uint8_t* pixelData = (uint8_t*)CGBitmapContextGetData(pixelContext_);
            for (size_t row = 0; row < h && !hasOverbright; ++row)
            {
                const uint16_t* p = reinterpret_cast<const uint16_t*>(pixelData + row * srcBpr);
                for (size_t col = 0; col < w; ++col, p += 4)
                {
                    if (p[3] == 0 && (p[0] | p[1] | p[2]) != 0) { hasOverbright = true; break; }
                    if (p[3] != 0 && (p[0] > p[3] || p[1] > p[3] || p[2] > p[3])) { hasOverbright = true; break; }
                }
            }

            if (hasOverbright)
            {
                // Build a 32-bit float additive bitmap
                CGContextRef addCtx = CGBitmapContextCreate(NULL, w, h, 32, 0,
                    static_cast<cocoa::Factory*>(seBitmap->factory)->info.cgColorSpace,
                    kCGImageAlphaPremultipliedLast | kCGBitmapFloatComponents | kCGBitmapByteOrder32Host);

                const size_t dstBpr = CGBitmapContextGetBytesPerRow(addCtx);
                uint8_t* dstData = (uint8_t*)CGBitmapContextGetData(addCtx);

                for (size_t row = 0; row < h; ++row)
                {
                    uint16_t* src = reinterpret_cast<uint16_t*>(pixelData + row * srcBpr);
                    float* dst = reinterpret_cast<float*>(dstData + row * dstBpr);

                    for (size_t col = 0; col < w; ++col, src += 4, dst += 4)
                    {
                        dst[0] = h2f(src[0]);
                        dst[1] = h2f(src[1]);
                        dst[2] = h2f(src[2]);
                        dst[3] = 1.0f;
                        src[0] = src[1] = src[2] = 0;
                    }
                }

                if (seBitmap->additiveBitmap_) CGImageRelease(seBitmap->additiveBitmap_);
                seBitmap->additiveBitmap_ = CGBitmapContextCreateImage(addCtx);
                CGContextRelease(addCtx);
            }
        }

        // Convert half-float to 32-bit float for rendering if needed
        if (wantHalfFloat)
        {
            const size_t w = seBitmap->pixelWidth_;
            const size_t h = seBitmap->pixelHeight_;
            const size_t srcBpr = CGBitmapContextGetBytesPerRow(pixelContext_);

            auto halfToFloat = [](uint16_t h) -> float {
                const uint32_t sign = (h >> 15) & 0x1u;
                const uint32_t exp  = (h >> 10) & 0x1fu;
                const uint32_t mant = h & 0x3ffu;
                uint32_t bits;
                if (exp == 0) {
                    if (mant == 0) { bits = sign << 31; }
                    else {
                        uint32_t e = 0, m = mant;
                        while (!(m & 0x400u)) { m <<= 1; ++e; }
                        bits = (sign << 31) | ((127 - 15 - e + 1) << 23) | ((m & 0x3ffu) << 13);
                    }
                } else if (exp == 31) {
                    bits = (sign << 31) | 0x7f800000u | (mant << 13);
                } else {
                    bits = (sign << 31) | ((exp + (127 - 15)) << 23) | (mant << 13);
                }
                float f; std::memcpy(&f, &bits, 4); return f;
            };

            CGContextRef floatCtx = CGBitmapContextCreate(NULL, w, h, 32, 0,
                static_cast<cocoa::Factory*>(seBitmap->factory)->info.cgColorSpace,
                kCGImageAlphaPremultipliedLast | kCGBitmapFloatComponents | kCGBitmapByteOrder32Host);

            const size_t dstBpr = CGBitmapContextGetBytesPerRow(floatCtx);
            uint8_t* srcData = (uint8_t*)CGBitmapContextGetData(pixelContext_);
            uint8_t* dstData = (uint8_t*)CGBitmapContextGetData(floatCtx);

            for (size_t row = 0; row < h; ++row)
            {
                const uint16_t* src = reinterpret_cast<const uint16_t*>(srcData + row * srcBpr);
                float* dst = reinterpret_cast<float*>(dstData + row * dstBpr);
                for (size_t col = 0; col < w * 4; ++col)
                    dst[col] = halfToFloat(src[col]);
            }

            // Replace the bitmap's CGImage
            if (*inImage_) CGImageRelease(*inImage_);
            *inImage_ = CGBitmapContextCreateImage(floatCtx);
            CGContextRelease(floatCtx);
        }
        else
        {
            // Write back: create a CGImage from the pixel context
            if (*inImage_) CGImageRelease(*inImage_);
            *inImage_ = CGBitmapContextCreateImage(pixelContext_);
        }

        seBitmap->pixelWidth_ = (uint32_t)CGImageGetWidth(*inImage_);
        seBitmap->pixelHeight_ = (uint32_t)CGImageGetHeight(*inImage_);
	}

    if (pixelContext_) CGContextRelease(pixelContext_);
}
} // namespace cocoa
} // namespace gmpi
