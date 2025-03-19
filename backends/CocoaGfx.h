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
#include <map>
//#include <locale>
#include <codecvt>
#include "../Drawing.h"
#include "../backends/Gfx_base.h"
#define USE_BACKING_BUFFER 1

/* TODO
investigate CGContextSetShouldSmoothFonts, CGContextSetAllowsFontSmoothing (for less heavy fonts)
*/

namespace gmpi
{
namespace cocoa
{
class Factory;
    
// Conversion utilities.
    
inline NSPoint toNative(gmpi::drawing::Point p)
{
    return NSMakePoint(p.x, p.y);
}
    
//inline gmpi::drawing::Rect RectFromNSRect(NSRect nsr)
//{
//    return{
//        static_cast<float>(nsr.origin.x),
//        static_cast<float>(nsr.origin.y),
 //       static_cast<float>(nsr.origin.x + nsr.size.width),
//        static_cast<float>(nsr.origin.y + nsr.size.height)
//    };
//}

inline NSRect NSRectFromRect(gmpi::drawing::Rect rect)
{
	return NSMakeRect(rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top);
}
        
CGMutablePathRef NsToCGPath(NSBezierPath* geometry) // could be cached in some cases.
{
    CGMutablePathRef cgPath = CGPathCreateMutable();
    NSInteger n = [geometry elementCount];
            
    for (NSInteger i = 0; i < n; i++) {
        NSPoint ps[3];
        switch ([geometry elementAtIndex:i associatedPoints:ps]) {
            case NSMoveToBezierPathElement: {
                CGPathMoveToPoint(cgPath, NULL, ps[0].x, ps[0].y);
                break;
            }
            case NSLineToBezierPathElement: {
                CGPathAddLineToPoint(cgPath, NULL, ps[0].x, ps[0].y);
                break;
            }
            case NSCurveToBezierPathElement: {
                CGPathAddCurveToPoint(cgPath, NULL, ps[0].x, ps[0].y, ps[1].x, ps[1].y, ps[2].x, ps[2].y);
                break;
            }
            case NSClosePathBezierPathElement: {
                CGPathCloseSubpath(cgPath);
                break;
            }
            default:
                assert(false && @"Invalid NSBezierPathElement");
        }
    }
    return cgPath;
}
        
// helper
void setNativePenStrokeStyle(NSBezierPath * path, gmpi::drawing::api::IStrokeStyle* strokeStyle)
{
    gmpi::drawing::CapStyle capstyle = strokeStyle == nullptr ? gmpi::drawing::CapStyle::Flat : ((se::generic_graphics::StrokeStyle*)strokeStyle)->strokeStyleProperties.lineCap;
            
    switch(capstyle)
    {
        default:
        case gmpi::drawing::CapStyle::Flat:
            [ path setLineCapStyle:NSLineCapStyleButt ];
            break;
                    
        case gmpi::drawing::CapStyle::Square:
            [ path setLineCapStyle:NSLineCapStyleSquare ];
            break;
                    
//        case gmpi::drawing::CapStyle::Triangle:
        case gmpi::drawing::CapStyle::Round:
            [ path setLineCapStyle:NSLineCapStyleRound ];
            break;
    }
}
    
void applyDashStyleToPath(NSBezierPath* path, const gmpi::drawing::api::IStrokeStyle* strokeStyleIm, float strokeWidth)
{
    gmpi::drawing::StrokeStyleProperties style;

    auto strokeStyle = (se::generic_graphics::StrokeStyle*)strokeStyleIm;
    if(strokeStyle)
        style = strokeStyle->strokeStyleProperties;
    
//    const auto dashStyle = strokeStyle ? strokeStyle->getDashStyle() : gmpi::drawing::DashStyle::Solid;
//    const auto phase = strokeStyle ? strokeStyle->getDashOffset() : 0.0f;
            
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
                        
    [path setLineDash: dashes.data() count: dashes.size() phase: style.dashOffset];
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
#if !__has_feature(objc_arc)
        if (native_)
		{
//					[native_ release];
		}
#endif
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

class TextFormat final : public gmpi::drawing::api::ITextFormat // : public CocoaWrapper<drawing::api::ITextFormat, const __CFDictionary>
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

    NSMutableDictionary* native2 = {};
    NSMutableParagraphStyle* nativeStyle = {};

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
            //nope                    {"Verdana UI",          "Verdana"},
        };

        const auto it = substitutes.find(windowsFont);
        if (it != substitutes.end())
        {
            return (*it).second.c_str();
        }

        return windowsFont;
    }

    TextFormat(/*std::wstring_convert<std::codecvt_utf8<wchar_t>>* pstringConverter,*/ const char* pfontFamilyName, gmpi::drawing::FontWeight pfontWeight, gmpi::drawing::FontStyle pfontStyle, gmpi::drawing::FontStretch pfontStretch, float pfontSize) :
        //				CocoaWrapper<drawing::api::ITextFormat, const __CFDictionary>(nullptr)
        fontWeight(pfontWeight)
        , fontStyle(pfontStyle)
        , fontStretch(pfontStretch)
        , fontSize(pfontSize)
    {
        //_RPT1(0, "TextFormat() %d\n", this);

        fontFamilyName = fontSubstitute(pfontFamilyName);

        NSFontTraitMask fontTraits = 0;
        if (pfontWeight >= gmpi::drawing::FontWeight::DemiBold)
            fontTraits |= NSBoldFontMask;

        if (pfontStyle == gmpi::drawing::FontStyle::Italic || pfontStyle == gmpi::drawing::FontStyle::Oblique)
            fontTraits |= NSItalicFontMask;

        auto nsFontName = [NSString stringWithCString : fontFamilyName.c_str() encoding : NSUTF8StringEncoding];
        NSFont* nativefont = {};

        // AU plugin on Logic Pro (ARM) asserts first time here with a memory error.
        // not sure why, but ignoring it seems to work OK.
        try
        {
            /*
                weight
                A hint for the weight desired, on a scale of 0 to 15: a value of 5 indicates a normal or book weight, and 9 or more a bold or heavier weight. The weight is ignored if fontTraitMask includes NSBoldFontMask.

                // from chrome

                NSInteger ToNSFontManagerWeight(Weight weight) {
                switch (weight) {
                    case Weight::THIN:
                    return 2;
                    case Weight::EXTRA_LIGHT:
                    return 3;
                    case Weight::LIGHT:
                    return 4;
                    case Weight::INVALID:
                    case Weight::NORMAL:
                    return 5;
                    case Weight::MEDIUM:
                    return 6;
                    case Weight::SEMIBOLD:
                    return 8;
                    case Weight::BOLD:
                    return 9;
                    case Weight::EXTRA_BOLD:
                    return 10;
                    case Weight::BLACK:
                    return 11;
                }
                */

            const int roundNearest = 50;
            const int nativeFontWeight = 1 + ((int) pfontWeight + roundNearest) / 100;

            nativefont = [[NSFontManager sharedFontManager]fontWithFamily:nsFontName traits : fontTraits weight : nativeFontWeight size : fontSize];
        }
        catch (...)
        {
            //_RPT0(0, "NSFontManager threw!");
        } // Logic Pro may throw an Memory exception here. Don't know why. Maybe due to it using a AU2 wrapper.

        // fallback to system font if nesc.
        if (!nativefont)
        {
            static const CGFloat weightConversion[] = {
                NSFontWeightUltraLight, // FontWeight_THIN = 100
                NSFontWeightThin,       // FontWeight_ULTRA_LIGHT = 200
                NSFontWeightLight,      // FontWeight_LIGHT = 300
                NSFontWeightRegular,    // FontWeight_NORMAL = 400
                NSFontWeightMedium,     // FontWeight_MEDIUM = 500
                NSFontWeightSemibold,   // FontWeight_SEMI_BOLD = 600
                NSFontWeightBold,       // FontWeight_BOLD = 700
                NSFontWeightHeavy,      // FontWeight_BLACK = 900
                NSFontWeightBlack       // FontWeight_ULTRA_BLACK = 950
            };

            const int arrMax = std::size(weightConversion) - 1;
            const int roundNearest = 50;
            const int nativeFontWeightIndex
                = std::max(0, std::min(arrMax, -1 + ((int) pfontWeight + roundNearest) / 100));
            const auto nativeFontWeight = weightConversion[nativeFontWeightIndex];

            // final fallback. system font.
            nativefont = [NSFont systemFontOfSize : fontSize weight : nativeFontWeight];
        }

        nativeStyle = [[NSMutableParagraphStyle alloc]init];
        [nativeStyle setAlignment : NSTextAlignmentLeft] ;

        native2 = [[NSMutableDictionary alloc]initWithObjectsAndKeys:
        nativefont, NSFontAttributeName,
            nativeStyle, NSParagraphStyleAttributeName,
            nil];

        CalculateTopAdjustment();
    }

    ~TextFormat()
    {
        //_RPT1(0, "~TextFormat() %d\n", this);

#if !__has_feature(objc_arc)
//                [native2 release];
#endif
        [native2 release];
        [nativeStyle release] ;
    }

    void CalculateTopAdjustment()
    {
        // Calculate compensation for different bounding box height between mac and direct2D.
        // On Direct2D boudning rect height is typicaly much less than Cocoa.
        // I don't know any algorithm for converting the extra height.
        // Fix is to disregard extra height on both platforms.
        gmpi::drawing::FontMetrics fontMetrics{};
        getFontMetrics(&fontMetrics);

        auto boundingBoxSize = [@"A" sizeWithAttributes : native2];

        topAdjustment = boundingBoxSize.height - (fontMetrics.ascent + fontMetrics.descent);
        ascent = fontMetrics.ascent;

        baselineCorrection = 0.5f;
        if ((fontMetrics.descent - floor(fontMetrics.descent)) <= 0.5f)
        {
            baselineCorrection += 0.5f;
        }
    }

	gmpi::ReturnCode setTextAlignment(gmpi::drawing::TextAlignment ptextAlignment) override
    {
        textAlignment = ptextAlignment;

        switch (textAlignment)
        {
            case gmpi::drawing::TextAlignment::Leading: // Left.
            [native2[NSParagraphStyleAttributeName] setAlignment:NSTextAlignmentLeft] ;
            break;
            case gmpi::drawing::TextAlignment::Trailing: // Right
            [native2[NSParagraphStyleAttributeName] setAlignment:NSTextAlignmentRight] ;
            break;
            case gmpi::drawing::TextAlignment::Center:
            [native2[NSParagraphStyleAttributeName] setAlignment:NSTextAlignmentCenter] ;
            break;
        }

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
        return gmpi::ReturnCode::Ok;
    }


    gmpi::ReturnCode getFontMetrics(gmpi::drawing::FontMetrics* returnFontMetrics) override
    {
        NSFont* font = native2[NSFontAttributeName];  // get font from dictionary.
        returnFontMetrics->xHeight = [font xHeight];
        returnFontMetrics->ascent = [font ascender];
        returnFontMetrics->descent = -[font descender]; // Descent is negative on OSX (positive on Windows)
        returnFontMetrics->lineGap = [font leading];
        returnFontMetrics->capHeight = [font capHeight];
        returnFontMetrics->underlinePosition = [font underlinePosition];
        returnFontMetrics->underlineThickness = [font underlineThickness];
        returnFontMetrics->strikethroughPosition = returnFontMetrics->xHeight / 2;
        returnFontMetrics->strikethroughThickness = returnFontMetrics->underlineThickness;
        /* same
                    auto fontRef = CGFontCreateWithFontName((CFStringRef) [font fontName]);
                    float d = CGFontGetDescent(fontRef);
                    float x = CGFontGetUnitsPerEm(fontRef);
                    float descent = d/x;
                    float descent2 = descent * fontSize;
                    CFRelease(fontRef);
        */
        return gmpi::ReturnCode::Ok;
    }

    // TODO!!!: Probly needs to accept constraint rect like DirectWrite. !!!
    gmpi::ReturnCode getTextExtentU(const char* utf8String, int32_t stringLength, gmpi::drawing::Size* returnSize) override
    {
        auto str = [NSString stringWithCString : utf8String encoding : NSUTF8StringEncoding];

        auto r = [str sizeWithAttributes : native2];
        returnSize->width = r.width;
        returnSize->height = r.height;// - topAdjustment;

//      if (!useLegacyBaseLineSnapping)
        returnSize->height -= topAdjustment;
    }
	
    gmpi::ReturnCode setLineSpacing(float lineSpacing, float baseline) override
    {
        return gmpi::ReturnCode::NoSupport;
    }

	GMPI_QUERYINTERFACE_METHOD(gmpi::drawing::api::ITextFormat);
    GMPI_REFCOUNT;
};

class BitmapPixels final : public gmpi::drawing::api::IBitmapPixels
{
    int bytesPerRow;
    class Bitmap* seBitmap = {};
    NSImage** inBitmap_;
    NSBitmapImageRep* bitmap2 = {};
    int32_t flags;

public:
    BitmapPixels(Bitmap* bitmap /*NSImage** inBitmap*/, bool _alphaPremultiplied, int32_t pflags);
    ~BitmapPixels();

    gmpi::ReturnCode getAddress(uint8_t** returnAddress) override
    {
        *returnAddress = static_cast<uint8_t*>([bitmap2 bitmapData]);
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
        return (uint8_t)((t + 1 + (t >> 8)) >> 8); // fast way to divide by 255
    }

    GMPI_QUERYINTERFACE_METHOD(gmpi::drawing::api::IBitmapPixels);
    GMPI_REFCOUNT;
};

class Bitmap final : public gmpi::drawing::api::IBitmap
{
    gmpi::drawing::api::IFactory* factory = nullptr;

public:
    NSImage* nativeBitmap_ = nullptr;
    NSBitmapImageRep* additiveBitmap_ = nullptr;
    int32_t creationFlags = (int32_t)drawing::BitmapRenderTargetFlags::EightBitPixels;
    
    Bitmap(gmpi::drawing::api::IFactory* pfactory, const char* utf8Uri) :
        nativeBitmap_(nullptr)
        , factory(pfactory)
    {
        // is this an in-memory resource?
        std::string uriString(utf8Uri);
        std::string binaryData;
#if 0 // TODO !!!
        if (uriString.find(BundleInfo::resourceTypeScheme) == 0)
        {
            //                    _RPT1(0, "Bitmap() A1: %d\n", this);

            binaryData = BundleInfo::instance()->getResource(utf8Uri + strlen(BundleInfo::resourceTypeScheme));

            nativeBitmap_ = [[NSImage alloc]initWithData:[NSData dataWithBytes : (binaryData.data())
                length : binaryData.size()] ];
        }
        else
#endif
        {
            //                    _RPT1(0, "Bitmap() A2: %d\n", this);

            NSString* url = [NSString stringWithCString : utf8Uri encoding : NSUTF8StringEncoding];
            nativeBitmap_ = [[NSImage alloc]initWithContentsOfFile:url];
        }

        // undo scaling of image (which reports scaled size, screwing up animation frame).
        NSSize max = NSZeroSize;
        for (NSObject* o in nativeBitmap_.representations) {
            if ([o isKindOfClass : NSImageRep.class]) {
                NSImageRep* r = (NSImageRep*)o;
                if (r.pixelsWide != NSImageRepMatchesDevice && r.pixelsHigh != NSImageRepMatchesDevice) {
                    max.width = MAX(max.width, r.pixelsWide);
                    max.height = MAX(max.height, r.pixelsHigh);
                }
            }
        }
        if (max.width > 0 && max.height > 0) {
            nativeBitmap_.size = max;
        }
    }

    Bitmap(gmpi::drawing::api::IFactory* pfactory, int32_t width, int32_t height)
        : factory(pfactory)
    {
        //                _RPT1(0, "Bitmap() B: %d\n", this);

        nativeBitmap_ = [[NSImage alloc]initWithSize:NSMakeSize((CGFloat)width, (CGFloat)height)];
        // not sure yet                [nativeBitmap_ setFlipped:TRUE];

#if !__has_feature(objc_arc)
//                [nativeBitmap_ retain];
#endif
    }

    // from a bitmap render target
    Bitmap(
           gmpi::drawing::api::IFactory* pfactory
           , NSImage* native
           , int32_t pCreationFlags = (int32_t)gmpi::drawing::BitmapRenderTargetFlags::EightBitPixels
           )
        : nativeBitmap_(native)
        , factory(pfactory)
        , creationFlags(pCreationFlags)
    {
        //               _RPT1(0, "Bitmap() C: %d\n", this);
        [nativeBitmap_ retain] ;
#if !__has_feature(objc_arc)
        //                [nativeBitmap_ retain];
#endif
    }

    bool isLoaded()
    {
        return nativeBitmap_ != nil;
    }

    virtual ~Bitmap()
    {
        //                _RPT1(0, "~Bitmap() %d\n", this);

#if !__has_feature(objc_arc)
//                [nativeBitmap_ release];
#endif
        if (nativeBitmap_)
        {
            [nativeBitmap_ release] ;
        }
        if (additiveBitmap_)
        {
            [additiveBitmap_ release] ;
        }
    }

    inline NSImage* getNativeBitmap()
    {
        return nativeBitmap_;
    }
/*
    gmpi::drawing::Size getSizeF() override
    {
        NSSize s = [nativeBitmap_ size];
        return Size(s.width, s.height);
    }
*/

    gmpi::ReturnCode getSizeU(gmpi::drawing::SizeU* returnSize) override
    {
        NSSize s = [nativeBitmap_ size];

        returnSize->width = static_cast<uint32_t>(0.5f + s.width);
        returnSize->height = static_cast<uint32_t>(0.5f + s.height);

        /* hmm, assumes image representation at index 0 is actual size. size is already set correctly in constructor.
            *
#ifdef _DEBUG
            // Check images NOT scaled by cocoa.
            // https://stackoverflow.com/questions/2190027/nsimage-acting-weird
            NSImageRep *rep = [[nativeBitmap_ representations] objectAtIndex:0];

            assert(returnSize->width == rep.pixelsWide);
            assert(returnSize->height == rep.pixelsHigh);
#endif
                */
        return gmpi::ReturnCode::Ok;
    }
#if 0
    gmpi::ReturnCode lockPixelsOld(gmpi::drawing::api::IBitmapPixels** returnInterface, bool alphaPremultiplied) override
    {
        *returnInterface = 0;
        return gmpi::ReturnCode::Fail;
        /* TODO!!!
                    gmpi::shared_ptr<gmpi::api::IUnknown> b2;
                    b2.Attach(new bitmapPixels(&nativeBitmap_, alphaPremultiplied, gmpi::drawing::MP1_BITMAP_LOCK_READ | gmpi::drawing::MP1_BITMAP_LOCK_WRITE));

                    return b2->queryInterface(gmpi::drawing::SE_IID_BITMAP_PIXELS_MPGUI, (void**)(returnInterface));
        */
    }
#endif

    gmpi::ReturnCode lockPixels(gmpi::drawing::api::IBitmapPixels** returnPixels, int32_t flags) override
    {
        //               _RPT1(0, "Bitmap() lockPixels: %d\n", this);
        *returnPixels = 0;

        gmpi::shared_ptr<gmpi::api::IUnknown> b2;
        b2.attach(new BitmapPixels(this /*&nativeBitmap_*/, true, flags));

        return b2->queryInterface(&gmpi::drawing::api::IBitmapPixels::guid, (void**)(returnPixels));
    }

//    void ApplyAlphaCorrection() override {}

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

// shared beween SDK3 and GMPI-UI factorys
struct FactoryInfo
{
    std::vector<std::string> supportedFontFamilies;
    std::wstring_convert<std::codecvt_utf8<wchar_t>> stringConverter; // cached, as constructor is super-slow.
    NSColorSpace* gmpiColorSpace{};
};

inline void initFactoryHelper(FactoryInfo& info)
{
    auto temp = CGColorSpaceCreateWithName(kCGColorSpaceLinearSRGB); // kCGColorSpaceExtendedLinearSRGB);
    info.gmpiColorSpace = [[NSColorSpace alloc] initWithCGColorSpace:temp];
        
    if(temp)
        CFRelease(temp);
}

class Factory : public gmpi::drawing::api::IFactory
{
public:
    FactoryInfo info;

    Factory()
    {
        initFactoryHelper(info);
    }
#if 0
    void setBestColorSpace(NSWindow* window)
    {
        /* even non-wide displays benifit from kCGColorSpaceExtendedLinearSRGB, they appear to dither to approximate it

        bool hasWideGamutScreen = true;
        for ( NSScreen* screen in [NSScreen screens] )
        {
            if ( FALSE == [screen canRepresentDisplayGamut:NSDisplayGamutP3] )
            {
                hasWideGamutScreen = false;
            }
        }
            */
            
#if 0
        //colorSpace = CGColorSpaceCreateWithName(kCGColorSpaceExtendedLinearSRGB); // bitmap washed-out on big sur
        // colorSpace = CGColorSpaceCreateWithName(kCGColorSpaceLinearSRGB); // too dark

/* The name of the "Generic" linear RGB color space. This is the same as
    `kCGColorSpaceGenericRGB' but with a 1.0 gamma. */

CG_EXTERN const CFStringRef kCGColorSpaceGenericRGBLinear
CG_AVAILABLE_STARTING(10.5, 9.0);

CG_EXTERN const CFStringRef kCGColorSpaceLinearDisplayP3
CG_AVAILABLE_STARTING(12.0, 15.0);

CG_EXTERN const CFStringRef kCGColorSpaceExtendedLinearDisplayP3
CG_AVAILABLE_STARTING(10.14.3, 12.3);
/*  The name of the sRGB color space variant with linear gamma */

CG_EXTERN const CFStringRef kCGColorSpaceLinearSRGB
CG_AVAILABLE_STARTING(10.12, 10.0);

/*  The name of the extended sRGB color space variant with linear gamma */

CG_EXTERN const CFStringRef kCGColorSpaceExtendedLinearSRGB
CG_AVAILABLE_STARTING(10.12, 10.0);

#endif

#if 0
// !!!DISABLE WINDOW COLORSPACE AS IT CAUSES FLICKERING TITLE BAR. !!! TODO research problem, log bug with Apple whatever

#if 0 // ndef _DEBUG
        NSOperatingSystemVersion minimumSupportedOSVersion = { .majorVersion = 10, .minorVersion = 12, .patchVersion = 2 };
        const BOOL isSupported = [NSProcessInfo.processInfo isOperatingSystemAtLeastVersion:minimumSupportedOSVersion];
        colorSpace = CGColorSpaceCreateWithName(isSupported ? kCGColorSpaceGenericRGBLinear : kCGColorSpaceSRGB);
#else

        const CFStringRef preferedColorSpaces[] =
        {                                                   // BIGSUR (Intel)   MONT (M1)         CATALINA (Mini Intel)
//              CFSTR("kCGColorSpaceExtendedLinearDisplayP3"),  // too bright       *good             *good
            CFSTR("kCGColorSpaceExtendedLinearSRGB"),       // too bright       *good             *good
            kCGColorSpaceGenericRGBLinear, // AKA Gen HDR   // *good (16bit)    good/banded(8bit) same
            CFSTR("kCGColorSpaceLinearSRGB"), // sRGB IEC   // *good 8b         good/banded 8b    good/banded (8bit)
            CFSTR("kCGColorSpaceSRGB"),// most generic      // brushes OK, gradients very bad. mild-banding all systems
        };
        const int fallbackCount = sizeof(preferedColorSpaces)/sizeof(preferedColorSpaces[0]);
/*
        static int test = 0;
        const int safeindex = test % fallbackCount;
        colorSpace = CGColorSpaceCreateWithName(preferedColorSpaces[safeindex]);
        ++test;
*/
        NSOperatingSystemVersion minimumSupportedOSVersion = { .majorVersion = 12, .minorVersion = 0, .patchVersion = 1 };
        const BOOL isMonterey = [NSProcessInfo.processInfo isOperatingSystemAtLeastVersion:minimumSupportedOSVersion];
        const int bestColorspaceIndex = isMonterey ? 0 : 1;
        CGColorSpaceRef colorSpace = {};
        for(int i = bestColorspaceIndex ; !colorSpace && i < fallbackCount; ++i)
        {
            colorSpace = CGColorSpaceCreateWithName(preferedColorSpaces[i]);
        }
#endif

        if(colorSpace)
        {
            nsColorSpace = [[NSColorSpace alloc] initWithCGColorSpace:colorSpace];
            [window setColorSpace:nsColorSpace];
        }
#endif
/* don't work

        const bool failed = (colorSpace != [[window colorSpace] CGColorSpace]);
            
        if(!failed)
        {
            return;
        }
            
        colorSpace = CGColorSpaceCreateWithName(kCGColorSpaceGenericRGBLinear);
        nsColorSpace = [[NSColorSpace alloc] initWithCGColorSpace:colorSpace];
        [window setColorSpace:nsColorSpace];
*/
/*
//nope            const auto bpp = NSBitsPerSampleFromDepth( [window depthLimit] );
        // hasWideGamutScreen works, but kCGColorSpaceExtendedLinearSRGB seems fine on those screens anyhow (Waves laptop)
        if(true) // hasWideGamutScreen) //8 < bpp)
        {
            // linear gradients, no banding.
            colorSpace = CGColorSpaceCreateWithName(kCGColorSpaceExtendedLinearSRGB);
        }
        else
        {
            // proper linear gradients, but banding on dark-grey gradient bitmap due to lack of precision.
            colorSpace = CGColorSpaceCreateWithName(kCGColorSpaceGenericRGBLinear);
        }
            
        nsColorSpace = [[NSColorSpace alloc] initWithCGColorSpace:colorSpace];
            
        [window setColorSpace:nsColorSpace];
            
        auto test = [window colorSpace];
        auto cgcs = [test CGColorSpace];
        */
//          return nsColorSpace;
    }
#endif
    
    // utility
    inline NSColor* toNative(const gmpi::drawing::Color& color)
    {
/*
        // This should be equivalent to sRGB, except it allows extended color components beyond 0.0 - 1.0
        // for plain old sRGB, colorWithCalibratedRed would be next best.
            
        return [NSColor colorWithDisplayP3Red:
                (CGFloat)se_sdk::FastGamma::pixelToNormalised(se_sdk::FastGamma::float_to_sRGB(color.r))
                green : (CGFloat)se_sdk::FastGamma::pixelToNormalised(se_sdk::FastGamma::float_to_sRGB(color.g))
                blue : (CGFloat)se_sdk::FastGamma::pixelToNormalised(se_sdk::FastGamma::float_to_sRGB(color.b))
                alpha : (CGFloat)color.a];
            
        // nativec_ = [NSColor colorWithRed:color.r green:color.g blue:color.b alpha:color.a]; // wrong gamma
*/
//            native2 = [[NSGradient alloc] initWithColors:colors atLocations: locations2.data() colorSpace: nsColorSpace];
            
        // same as manual sRGB conversion, but not linear alpha blending
//           return [NSColor colorWithCalibratedRed:color.r green:color.g blue:color.b alpha:color.a];
// too dark in all screen spaces            return [NSColor colorWithCalibratedRed:color.r green:color.g blue:color.b alpha:color.a ];
#if 0
        // looks correct in all screen spaces, at least for brushes. shows quantization from conversion routines is only weakness.
        return [NSColor colorWithSRGBRed:
                (CGFloat)se_sdk::FastGamma::pixelToNormalised(se_sdk::FastGamma::float_to_sRGB(color.r))
                green : (CGFloat)se_sdk::FastGamma::pixelToNormalised(se_sdk::FastGamma::float_to_sRGB(color.g))
                blue : (CGFloat)se_sdk::FastGamma::pixelToNormalised(se_sdk::FastGamma::float_to_sRGB(color.b))
                alpha : (CGFloat)color.a];
        
        // or, without quantization, but slower.
        return [NSColor colorWithSRGBRed:
                        (CGFloat)se_sdk::FastGamma::linearToSrgb(color.r)
                green : (CGFloat)se_sdk::FastGamma::linearToSrgb(color.g)
                blue  : (CGFloat)se_sdk::FastGamma::linearToSrgb(color.b)
                alpha : (CGFloat)color.a];
#endif
        const CGFloat components[4] = {color.r, color.g, color.b, color.a};
        return [NSColor colorWithColorSpace:info.gmpiColorSpace components:components count:4];
            
// I think this is wrong because it's saying that the gmpi color is in the screen's color space. Which is a crapshoot out of our control.
// we need to supply our color in a known color space, then let the OS convert it to whatever unknown color-space the screen is using.
//            return [NSColor colorWithColorSpace:nsColorSpace components:components count:4]; // washed-out on big Sur (with extended linier) good with genericlinier
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

    // IMpFactory2
    gmpi::ReturnCode getFontFamilyName(int32_t fontIndex, gmpi::api::IString* returnString) override
    {
        if(info.supportedFontFamilies.empty())
        {
            NSFontManager* fontManager = [NSFontManager sharedFontManager];
/*
            auto fontNames = [fontManager availableFonts]; // tends to add "-bold" or "-italic" on end of name
*/
            //for IOS see [UIFont familynames]
            NSArray* fontFamilies = nil;
            try
            {
                fontFamilies = [fontManager availableFontFamilies];
            }
            catch(...)
            {
                //_RPT0(0, "NSFontManager threw!");
            } // Logic Pro may throw an Memory exception here. Don't know why. Maybe due to it using a AU2 wrapper.

            for( NSString* familyName in fontFamilies)
            {
                info.supportedFontFamilies.push_back([familyName UTF8String]);
            }
        }
            
        if (fontIndex < 0 || fontIndex >= info.supportedFontFamilies.size())
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

    virtual void fillPath(class GraphicsContext* context, NSBezierPath* nsPath) const = 0;

    // Default to black fill for fancy brushes that don't implement line drawing yet.
    virtual void strokePath(NSBezierPath* nsPath, float strokeWidth, const gmpi::drawing::api::IStrokeStyle* strokeStyle = nullptr) const
    {
        [[NSColor blackColor]set]; /// !!!TODO!!!, color set to black always.

        [nsPath setLineWidth : strokeWidth] ;
        setNativePenStrokeStyle(nsPath, (gmpi::drawing::api::IStrokeStyle*)strokeStyle);

        [nsPath stroke] ;
    }
};


class BitmapBrush final : public gmpi::drawing::api::IBitmapBrush, public CocoaBrushBase
{
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
    }

    void strokePath(NSBezierPath* nsPath, float strokeWidth, const gmpi::drawing::api::IStrokeStyle* strokeStyle = nullptr) const override
    {
        [[NSColor colorWithPatternImage : bitmap_.nativeBitmap_]set];

        [nsPath setLineWidth : strokeWidth] ;
        setNativePenStrokeStyle(nsPath, (gmpi::drawing::api::IStrokeStyle*)strokeStyle);

        [nsPath stroke] ;
    }
    void fillPath(GraphicsContext* context, NSBezierPath* nsPath) const override;

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
    NSColor* nativec_ = nullptr;

    inline void setNativeColor()
    {
        nativec_ = factory_->toNative(color);
        [nativec_ retain] ;
    }

public:
    SolidColorBrush(const gmpi::drawing::Color* pcolor, cocoa::Factory* factory) : CocoaBrushBase(factory)
        , color(*pcolor)
    {
        setNativeColor();
    }

    inline NSColor* nativeColor() const
    {
        return nativec_;
    }

    void fillPath(GraphicsContext* context, NSBezierPath* nsPath) const override
    {
        [nativec_ set] ;
        [nsPath fill] ;
    }

    void strokePath(NSBezierPath* nsPath, float strokeWidth, const gmpi::drawing::api::IStrokeStyle* strokeStyle = nullptr) const override
    {
        [nativec_ set] ;
        [nsPath setLineWidth : strokeWidth] ;
        setNativePenStrokeStyle(nsPath, (gmpi::drawing::api::IStrokeStyle*)strokeStyle);

        [nsPath stroke] ;
    }

    ~SolidColorBrush()
    {
        [nativec_ release] ;
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

        GMPI_QUERYINTERFACE(gmpi::drawing::api::IBrush);
        GMPI_QUERYINTERFACE(gmpi::drawing::api::IResource);
        GMPI_QUERYINTERFACE(gmpi::drawing::api::ISolidColorBrush);

		return gmpi::ReturnCode::NoSupport;
	}

    GMPI_REFCOUNT;
};

class Gradient
{
protected:
    NSGradient* native2 = {};

public:
    Gradient(cocoa::Factory* factory, const gmpi::drawing::api::IGradientstopCollection* gradientStopCollection)
    {
        auto stops = static_cast<const GradientstopCollection*>(gradientStopCollection);

        NSMutableArray* colors = [NSMutableArray array];
        std::vector<CGFloat> locations2;

        // reversed, so radial gradient draws same as PC
        for (auto it = stops->gradientstops.rbegin(); it != stops->gradientstops.rend(); ++it)//  auto& stop : stops->gradientstops)
        {
            const auto& stop = *it;
            [colors addObject : factory->toNative(stop.color)] ;
            locations2.push_back(1.0 - stop.position);
        }
        /* faded on big sur
                CGColorSpaceRef colorSpace = CGColorSpaceCreateWithName(kCGColorSpaceGenericRGBLinear);
                NSColorSpace* nsColorSpace = [[NSColorSpace alloc] initWithCGColorSpace:colorSpace];

                native2 = [[NSGradient alloc] initWithColors:colors atLocations: locations2.data() colorSpace: nsColorSpace];
        */
        //native2 = [[NSGradient alloc] initWithColors:colors atLocations: locations2.data() colorSpace: [NSColorSpace genericRGBColorSpace]]; // too dark on genericlinier window cs

//                CFRelease(colorSpace);

        native2 = [[NSGradient alloc]initWithColors:colors atLocations : locations2.data() colorSpace : factory->info.gmpiColorSpace];
    }

    virtual void drawGradient() const = 0;

    ~Gradient()
    {
        [native2 release] ;
    }
    
    void fillPath(NSBezierPath* nsPath) const
    {
        // If you plan to do more drawing later, it's a good idea
        // to save the graphics state before clipping.
        [NSGraphicsContext saveGraphicsState];

        // clip following output to the path
        [nsPath addClip];
        
        drawGradient();

        // restore clip region
        [NSGraphicsContext restoreGraphicsState] ;
    }

    void strokePath(NSBezierPath* nsPath, float strokeWidth, const gmpi::drawing::api::IStrokeStyle* strokeStyle = nullptr) const
    {
        setNativePenStrokeStyle(nsPath, (gmpi::drawing::api::IStrokeStyle*)strokeStyle);

        // convert NSPath to CGPath
        CGPathRef strokePath;
        {
            CGMutablePathRef cgPath = NsToCGPath(nsPath);

            strokePath = CGPathCreateCopyByStrokingPath(cgPath, NULL, strokeWidth, (CGLineCap)[nsPath lineCapStyle],
                (CGLineJoin)[nsPath lineJoinStyle], [nsPath miterLimit]);
            CGPathRelease(cgPath);
        }

        // If you plan to do more drawing later, it's a good idea
        // to save the graphics state before clipping.
        [NSGraphicsContext saveGraphicsState];

        // clip following output to the path
        CGContextRef ctx = (CGContextRef) [[NSGraphicsContext currentContext]graphicsPort];

        CGContextAddPath(ctx, strokePath);
        CGContextClip(ctx);

        drawGradient();
        
        // restore clip region
        [NSGraphicsContext restoreGraphicsState] ;

        CGPathRelease(strokePath);
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

    float getAngle() const
    {
        // TODO cache. e.g. nan = not calculated yet.
        return (180.0f / M_PI) * atan2(brushProperties.endPoint.y - brushProperties.startPoint.y, brushProperties.endPoint.x - brushProperties.startPoint.x);
    }

	void setStartPoint(gmpi::drawing::Point startPoint) override
    {
        brushProperties.startPoint = startPoint;
    }
    void setEndPoint(gmpi::drawing::Point endPoint) override
    {
        brushProperties.endPoint = endPoint;
    }

    void drawGradient() const override
    {
        // assuming the caller has applied a clipping path, just draw the gradient.
        [native2 drawFromPoint : toNative(brushProperties.endPoint) toPoint : toNative(brushProperties.startPoint) options : NSGradientDrawsBeforeStartingLocation | NSGradientDrawsAfterEndingLocation] ;
    }

    void fillPath(GraphicsContext*, NSBezierPath* nsPath) const override
    {
        Gradient::fillPath(nsPath);
#if 0
        [NSGraphicsContext saveGraphicsState];

        // clip following output to the path
        [nsPath addClip] ;

        drawGradient();

        // restore clip region
        [NSGraphicsContext restoreGraphicsState] ;
#endif
    }

    void strokePath(NSBezierPath* nsPath, float strokeWidth, const gmpi::drawing::api::IStrokeStyle* strokeStyle = nullptr) const override
    {
        Gradient::strokePath(nsPath, strokeWidth, strokeStyle);
#if 0
        setNativePenStrokeStyle(nsPath, (gmpi::drawing::api::IStrokeStyle*)strokeStyle);

        // convert NSPath to CGPath
        CGPathRef strokePath;
        {
            CGMutablePathRef cgPath = NsToCGPath(nsPath);

            strokePath = CGPathCreateCopyByStrokingPath(cgPath, NULL, strokeWidth, (CGLineCap)[nsPath lineCapStyle],
                (CGLineJoin)[nsPath lineJoinStyle], [nsPath miterLimit]);
            CGPathRelease(cgPath);
        }


        // If you plan to do more drawing later, it's a good idea
        // to save the graphics state before clipping.
        [NSGraphicsContext saveGraphicsState];

        // clip following output to the path
        CGContextRef ctx = (CGContextRef) [[NSGraphicsContext currentContext]graphicsPort];

        CGContextAddPath(ctx, strokePath);
        CGContextClip(ctx);

        drawGradient();
        
        // restore clip region
        [NSGraphicsContext restoreGraphicsState] ;

        CGPathRelease(strokePath);
#endif
        
    }

	gmpi::ReturnCode getFactory(gmpi::drawing::api::IFactory** factory) override
    {
        *factory = factory_;
		return gmpi::ReturnCode::Ok;
    }

    GMPI_REFCOUNT;
    GMPI_QUERYINTERFACE_METHOD(gmpi::drawing::api::ILinearGradientBrush);
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
    
    void drawGradient() const override
    {
        const auto origin = NSMakePoint(
            gradientProperties.center.x + gradientProperties.gradientOriginOffset.x,
            gradientProperties.center.y + gradientProperties.gradientOriginOffset.y);

        // assuming the caller has applied a clipping path, just draw the gradient.
        [native2 drawFromCenter : toNative(gradientProperties.center)
            radius : gradientProperties.radiusX
            toCenter : origin
            radius : 0.0
            options : NSGradientDrawsBeforeStartingLocation | NSGradientDrawsAfterEndingLocation];
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
    void fillPath(GraphicsContext*, NSBezierPath* nsPath) const override
    {
        Gradient::fillPath(nsPath);
    }
    void strokePath(NSBezierPath* nsPath, float strokeWidth, const gmpi::drawing::api::IStrokeStyle* strokeStyle = nullptr) const override
    {
        Gradient::strokePath(nsPath, strokeWidth, strokeStyle);
    }
    
	gmpi::ReturnCode queryInterface(const gmpi::api::Guid* iid, void** returnInterface) override
	{
		*returnInterface = {};

        GMPI_QUERYINTERFACE(gmpi::drawing::api::IBrush);
        GMPI_QUERYINTERFACE(gmpi::drawing::api::IResource);
        GMPI_QUERYINTERFACE(gmpi::drawing::api::IRadialGradientBrush);

        return gmpi::ReturnCode::NoSupport;
	}

	// IResource
	gmpi::ReturnCode getFactory(gmpi::drawing::api::IFactory** factory) override
    {
        *factory = factory_;
    }

    GMPI_REFCOUNT;
};

class GeometrySink final : public se::generic_graphics::GeometrySink
{
	NSBezierPath* geometry_;

public:
	GeometrySink(NSBezierPath* geometry) : geometry_(geometry)
	{}

	void setFillMode(gmpi::drawing::FillMode fillMode) override
	{
		switch (fillMode)
		{
            case gmpi::drawing::FillMode::Alternate:
			[geometry_ setWindingRule : NSEvenOddWindingRule] ;
			break;

            case gmpi::drawing::FillMode::Winding:
			[geometry_ setWindingRule : NSNonZeroWindingRule] ;
			break;
		}
	}
#if 0
	void setSegmentFlags(gmpi::drawing::MP1_PATH_SEGMENT vertexFlags)
	{
		//		geometrysink_->setSegmentFlags((D2D1_PATH_SEGMENT)vertexFlags);
	}
#endif
	void beginFigure(gmpi::drawing::Point startPoint, gmpi::drawing::FigureBegin figureBegin) override
	{
		[geometry_ moveToPoint : NSMakePoint(startPoint.x, startPoint.y)] ;

		lastPoint = startPoint;
	}

	void endFigure(gmpi::drawing::FigureEnd figureEnd) override
	{
		if (figureEnd == gmpi::drawing::FigureEnd::Closed)
		{
			[geometry_ closePath] ;
		}
	}

	void addLine(gmpi::drawing::Point point) override
	{
		[geometry_ lineToPoint : NSMakePoint(point.x, point.y)] ;

		lastPoint = point;
	}

	void addBezier(const gmpi::drawing::BezierSegment* bezier) override
	{
		[geometry_ curveToPoint : NSMakePoint(bezier->point3.x, bezier->point3.y)
			controlPoint1 : NSMakePoint(bezier->point1.x, bezier->point1.y)
			controlPoint2 : NSMakePoint(bezier->point2.x, bezier->point2.y)] ;

		lastPoint = bezier->point3;
	}

	GMPI_QUERYINTERFACE_METHOD(gmpi::drawing::api::IGeometrySink);

	GMPI_REFCOUNT;
};


class PathGeometry final : public gmpi::drawing::api::IPathGeometry
{
	NSBezierPath* geometry_ = {};

    gmpi::drawing::DashStyle currentDashStyle = gmpi::drawing::DashStyle::Solid;
	std::vector<float> currentCustomDashStyle;
	float currentDashPhase = {};

public:
	PathGeometry()
	{
#if !__has_feature(objc_arc)
		//                [geometry_ retain];
#endif
	}

	~PathGeometry()
	{
		//	auto release pool handles it?
#if !__has_feature(objc_arc)
//                [geometry_ release];
#endif
		if (geometry_)
		{
			[geometry_ release] ;
		}
	}

	inline NSBezierPath* native()
	{
		return geometry_;
	}

	gmpi::ReturnCode open(gmpi::drawing::api::IGeometrySink** returnGeometrySink) override
	{
		if (geometry_)
		{
			[geometry_ release] ;
		}
		geometry_ = [NSBezierPath bezierPath];
		[geometry_ retain] ;

		gmpi::shared_ptr<gmpi::api::IUnknown> b2;
		b2.attach(new GeometrySink(geometry_));

		return b2->queryInterface(&gmpi::drawing::api::IGeometrySink::guid, reinterpret_cast<void**>(returnGeometrySink));
	}

	gmpi::ReturnCode getFactory(gmpi::drawing::api::IFactory** factory) override
	{
		//		native_->getFactory((ID2D1Factory**)factory);
	}

	gmpi::ReturnCode strokeContainsPoint(gmpi::drawing::Point point, float strokeWidth, gmpi::drawing::api::IStrokeStyle* strokeStyle, const gmpi::drawing::Matrix3x2* worldTransform, bool* returnContains) override
	{
		auto cgPath2 = NsToCGPath(geometry_);

		CGPathRef hitTargetPath = CGPathCreateCopyByStrokingPath(cgPath2, NULL, (CGFloat)strokeWidth, (CGLineCap)[geometry_ lineCapStyle], (CGLineJoin)[geometry_ lineJoinStyle], [geometry_ miterLimit]);

		CGPathRelease(cgPath2);

		CGPoint cgpoint = CGPointMake(point.x, point.y);
		*returnContains = (bool)CGPathContainsPoint(hitTargetPath, NULL, cgpoint, (bool)[geometry_ windingRule]);

		CGPathRelease(hitTargetPath);
		return gmpi::ReturnCode::Ok;
	}

	gmpi::ReturnCode fillContainsPoint(gmpi::drawing::Point point, const gmpi::drawing::Matrix3x2* worldTransform, bool* returnContains) override
	{
		*returnContains = [geometry_ containsPoint : NSMakePoint(point.x, point.y)];
		return gmpi::ReturnCode::Ok;
	}

	gmpi::ReturnCode getWidenedBounds(float strokeWidth, gmpi::drawing::api::IStrokeStyle* strokeStyle, const gmpi::drawing::Matrix3x2* worldTransform, gmpi::drawing::Rect* returnBounds) override
	{
		const float radius = ceilf(strokeWidth * 0.5f);
		auto nativeRect = [geometry_ bounds];
		returnBounds->left = nativeRect.origin.x - radius;
		returnBounds->top = nativeRect.origin.y - radius;
		returnBounds->right = nativeRect.origin.x + nativeRect.size.width + radius;
		returnBounds->bottom = nativeRect.origin.y + nativeRect.size.height + radius;

		return gmpi::ReturnCode::Ok;
	}

	void applyDashStyle(const gmpi::drawing::api::IStrokeStyle* strokeStyleIm, float strokeWidth)
	{
        gmpi::drawing::StrokeStyleProperties style;

        auto strokeStyle = (se::generic_graphics::StrokeStyle*)strokeStyleIm;
        if(strokeStyle)
            style = strokeStyle->strokeStyleProperties;

		bool changed = currentDashStyle != style.dashStyle;
		currentDashStyle = style.dashStyle;

		if (!changed && currentDashStyle == gmpi::drawing::DashStyle::Custom)
		{
            changed |= currentCustomDashStyle != strokeStyle->dashes;
		}

		if (currentDashStyle != gmpi::drawing::DashStyle::Solid) // i.e. 'none'
		{
			changed |= style.dashOffset != currentDashPhase;
			currentDashPhase = style.dashOffset;
		}

		if (!changed)
		{
			return;
		}

		applyDashStyleToPath(geometry_, strokeStyle, strokeWidth);
	}

	GMPI_QUERYINTERFACE_METHOD(gmpi::drawing::api::IPathGeometry);
	GMPI_REFCOUNT;
};

struct ContextInfo
{
	std::vector<gmpi::drawing::Rect> clipRectStack;
    NSAffineTransform* currentTransform{};
    NSView* view{};
};

class GraphicsContext : public gmpi::drawing::api::IDeviceContext
{
protected:
    cocoa::Factory* factory{};
    ContextInfo info;

public:
    inline static int logicProFix = -1;

	GraphicsContext(NSView* pview, cocoa::Factory* pfactory) :
		factory(pfactory)
	{
        info.view = pview;
		info.currentTransform = [NSAffineTransform transform];
    }

	~GraphicsContext()
	{
    // BitmapRenderTarget has one rect pushed. not sure if it should remove it in destructor?    assert(info.clipRectStack.size() == 0);
	}

	gmpi::ReturnCode getFactory(gmpi::drawing::api::IFactory** pfactory) override
	{
		*pfactory = factory;
		return gmpi::ReturnCode::Ok;
	}

	gmpi::ReturnCode drawRectangle(const gmpi::drawing::Rect* rect, gmpi::drawing::api::IBrush* brush, float strokeWidth, gmpi::drawing::api::IStrokeStyle* strokeStyle) override
	{
		/*
		auto scb = dynamic_cast<const SolidColorBrush*>(brush);
		if (scb)
		{
			[scb->nativeColor() set];
		}
		NSBezierPath *bp = [NSBezierPath bezierPathWithRect : cocoa::NSRectFromRect(*rect)];
		[bp stroke];
		*/

		NSBezierPath* path = [NSBezierPath bezierPathWithRect : cocoa::NSRectFromRect(*rect)];
		applyDashStyleToPath(path, strokeStyle, strokeWidth);
		auto cocoabrush = dynamic_cast<const CocoaBrushBase*>(brush);
		if (cocoabrush)
		{
			cocoabrush->strokePath(path, strokeWidth, strokeStyle);
		}
		return gmpi::ReturnCode::Ok;
	}

	gmpi::ReturnCode fillRectangle(const gmpi::drawing::Rect* rect, gmpi::drawing::api::IBrush* brush) override
	{
		NSBezierPath* rectPath = [NSBezierPath bezierPathWithRect : cocoa::NSRectFromRect(*rect)];

		auto cocoabrush = dynamic_cast<const CocoaBrushBase*>(brush);
		if (cocoabrush)
		{
			cocoabrush->fillPath(this, rectPath);
		}
		return gmpi::ReturnCode::Ok;
	}

	gmpi::ReturnCode clear(const gmpi::drawing::Color* clearColor) override
	{
		SolidColorBrush brush(clearColor, factory);
        gmpi::drawing::Rect r;
		getAxisAlignedClip(&r);
		NSBezierPath* rectPath = [NSBezierPath bezierPathWithRect : NSRectFromRect(r)];
		brush.fillPath(this, rectPath);
		return gmpi::ReturnCode::Ok;
	}

	gmpi::ReturnCode drawLine(gmpi::drawing::Point point0, gmpi::drawing::Point point1, gmpi::drawing::api::IBrush* brush, float strokeWidth, gmpi::drawing::api::IStrokeStyle* strokeStyle) override
	{
		NSBezierPath* path = [NSBezierPath bezierPath];
		[path moveToPoint : NSMakePoint(point0.x, point0.y)] ;
		[path lineToPoint : NSMakePoint(point1.x, point1.y)] ;

		auto cocoabrush = dynamic_cast<const CocoaBrushBase*>(brush);
		if (cocoabrush)
		{
			applyDashStyleToPath(path, const_cast<gmpi::drawing::api::IStrokeStyle*>(strokeStyle), strokeWidth);
			cocoabrush->strokePath(path, strokeWidth, strokeStyle);
		}
		return gmpi::ReturnCode::Ok;
	}

	gmpi::ReturnCode drawGeometry(gmpi::drawing::api::IPathGeometry* pathGeometry, gmpi::drawing::api::IBrush* brush, float strokeWidth, gmpi::drawing::api::IStrokeStyle* strokeStyle) override
	{
		auto pg = (PathGeometry*)pathGeometry;
		pg->applyDashStyle(strokeStyle, strokeWidth);

		auto cocoabrush = dynamic_cast<const CocoaBrushBase*>(brush);
		if (cocoabrush)
		{
			cocoabrush->strokePath(pg->native(), strokeWidth, strokeStyle);
		}
		return gmpi::ReturnCode::Ok;
	}

	gmpi::ReturnCode fillGeometry(gmpi::drawing::api::IPathGeometry* pathGeometry, gmpi::drawing::api::IBrush* brush, gmpi::drawing::api::IBrush* opacityBrush) override
	{
		auto nsPath = ((PathGeometry*)pathGeometry)->native();

		auto cocoabrush = dynamic_cast<const CocoaBrushBase*>(brush);
		if (cocoabrush)
		{
			cocoabrush->fillPath(this, nsPath);
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
			// Vertical text alignment.
			switch (textformat->paragraphAlignment)
			{
                case gmpi::drawing::ParagraphAlignment::Far:    // Bottom
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

		NSString* str = [NSString stringWithCString : string encoding : NSUTF8StringEncoding];

		const bool clipToRect = options & gmpi::drawing::DrawTextOptions::Clip;

		if (textformat->wordWrapping == gmpi::drawing::WordWrapping::NoWrap)
		{
			// Mac will always clip to rect, so we turn 'off' clipping by extending rect to the right.
			if (!clipToRect)
			{
				const auto extendWidth = static_cast<double>(textSize.width) - bounds.size.width;
				if (extendWidth > 0.0)
				{
					bounds.size.width += extendWidth;

					// fake no-clip by extending text box.
					switch (textformat->textAlignment)
					{
                        case gmpi::drawing::TextAlignment::Center:
						bounds.origin.x -= 0.5 * extendWidth;
						break;
                        case gmpi::drawing::TextAlignment::Trailing: // Right
						bounds.origin.x -= extendWidth;
                        case gmpi::drawing::TextAlignment::Leading: // Left
					default:
						break;
					}
				}
			}

			[textformat->native2[NSParagraphStyleAttributeName] setLineBreakMode:NSLineBreakByClipping];
		}
		else
		{
			// 'wrap'.
			[textformat->native2[NSParagraphStyleAttributeName] setLineBreakMode:NSLineBreakByWordWrapping] ;
		}

		//                gmpi::drawing::FontMETRICS fontMetrics;
		//               ((gmpi::drawing::api::ITextFormat*) textFormat)->getFontMetrics(&fontMetrics);

		//                float testLineHeightMultiplier = 0.5f;
		//                float shiftUp = testLineHeightMultiplier * fontMetrics.bodyHeight();

				// macOS draws extra padding at top of text bounds. Compensate for it.
//		if (!textformat->getUseLegacyBaseLineSnapping())
		{
			bounds.origin.y -= textformat->topAdjustment;
			bounds.size.height += textformat->topAdjustment;

			// Adjust text baseline vertically to match Windows.
#if 1
			const float scale = 0.5f; // Hi DPI x2

			float winBaseline{};
			{
				// Windows baseline
				/*
				const float offsetHinted = -0.25f;
				const float offsetVertAA = -0.125f;
				const float useHintingThreshold = 10.0f;
				const float offset =
					fontMetrics.bodyHeight() < useHintingThreshold ? offsetHinted : offsetVertAA;
				*/
				const float offset = -0.25f;;
				winBaseline = layoutRect->top + textformat->ascent;
				winBaseline = floorf((offset + winBaseline) / scale) * scale;
			}
#if 0
			float macBaseline{};
			{
				// Mac baseline
				// Correct, but given the assumption we intend to snap baseline o pixel grid - we
				// can use the simpler version below.
				float predictedBt = layoutRect->top + fontMetrics.ascent + fontMetrics.descent;
				predictedBt = truncatePixel(predictedBt, scale);
				macBaseline = predictedBt - fontMetrics.descent;
				macBaseline = truncatePixel(macBaseline, scale);

				if ((fontMetrics.descent - floor(fontMetrics.descent)) <= 0.5f)
				{
					macBaseline -= 0.5f;
				}
			}

			float macBaselineCorrection = roundPixel(winBaseline - macBaseline, scale);
#else
            const float baseline = layoutRect->top + textformat->ascent;
            float macBaselineCorrection{};
            if (logicProFix)
            {
                macBaselineCorrection = winBaseline - baseline - textformat->baselineCorrection + 1;
            }
            else
            {
                macBaselineCorrection = winBaseline - baseline + textformat->baselineCorrection;
            }
#endif

			bounds.origin.y += macBaselineCorrection;
#endif
#if 0
			if (false) // Visualize Mac Baseline prediction.
			{
				Graphics g(this);
				static auto brush = g.CreateSolidColorBrush(Color::Lime);
				g.DrawLine(Point(layoutRect->left, macBaseline + 0.25), Point(layoutRect->left + 2, macBaseline + 0.25), brush, 0.25f);
			}
#endif
		}

#if 0
		if (true) // Visualize adjusted bounds.
		{
			Graphics g(this);
			static auto brush = g.CreateSolidColorBrush(Color::Lime);
			g.DrawRectangle(Rect(bounds.origin.x, bounds.origin.y, bounds.origin.x + bounds.size.width, bounds.origin.y + bounds.size.height), brush);
		}
#endif

		// do last so don't affect font metrics.
//              [textformat->native2[NSParagraphStyleAttributeName] setLineHeightMultiple:testLineHeightMultiplier];
//               textformat->native2[NSBaselineOffsetAttributeName] = [NSNumber numberWithFloat:shiftUp];
#if 0
        if(auto lgb = dynamic_cast<const Gradient*>(brush); lgb)
        {
            [textformat->native2 setObject : [NSColor whiteColor] forKey : NSForegroundColorAttributeName];

            // Create a grayscale context for the mask
            CGColorSpaceRef colorspace = CGColorSpaceCreateDeviceGray();
            CGContextRef maskContext =
            CGBitmapContextCreate(
                NULL,
                bounds.size.width,
                bounds.size.height,
                8,
                bounds.size.width,
                colorspace,
                0);
            CGColorSpaceRelease(colorspace);

            // Switch to the context for drawing
            NSGraphicsContext *maskGraphicsContext =
                [NSGraphicsContext
                    graphicsContextWithGraphicsPort:maskContext
                    flipped:YES];
            [NSGraphicsContext saveGraphicsState];
            [NSGraphicsContext setCurrentContext:maskGraphicsContext];
            
            [str drawInRect: NSMakeRect(0, 0, bounds.size.width, bounds.size.height) withAttributes : textformat->native2];

            // Switch back to the window's context
            [NSGraphicsContext restoreGraphicsState];

            // Create an image mask from what we've drawn so far
            CGImageRef alphaMask = CGBitmapContextCreateImage(maskContext);
            
            CGContextRef windowContext = (CGContextRef) [[NSGraphicsContext currentContext] graphicsPort];

            // Draw the fill, clipped by the mask
            CGContextSaveGState(windowContext);
            
            CGContextClipToMask(windowContext, NSRectToCGRect(bounds), alphaMask);

            lgb->drawGradient();
            
            CGContextRestoreGState(windowContext);
            CGImageRelease(alphaMask);
        }
        else
        {
#endif
            if(auto scb = dynamic_cast<const SolidColorBrush*>(brush) ; scb)
            {
                [textformat->native2 setObject : scb->nativeColor() forKey : NSForegroundColorAttributeName];
                
#if USE_BACKING_BUFFER
                // Create a flipped coordinate system
                [[NSGraphicsContext currentContext] saveGraphicsState];
                NSAffineTransform *transform = [NSAffineTransform transform];
                [transform scaleXBy:1 yBy:-1];
                [transform translateXBy:0 yBy:-2 * bounds.origin.y - bounds.size.height];

                [transform concat];

                // Draw in the flipped coordinate system
                [str drawInRect : bounds withAttributes : textformat->native2];

                // Restore the original graphics state
                [[NSGraphicsContext currentContext] restoreGraphicsState];
#else
                [str drawInRect : bounds withAttributes : textformat->native2];
#endif
                
            }
            else // if(auto bmb = dynamic_cast<const BitmapBrush*>(brush) ; bmb)
            {
                auto lgb = dynamic_cast<const Gradient*>(brush);
                auto bmb = dynamic_cast<const BitmapBrush*>(brush);
                
                if( lgb || bmb)
                {
                    [textformat->native2 setObject : [NSColor whiteColor] forKey : NSForegroundColorAttributeName];
                    
                    NSSize logicalsize = bounds.size;
                    NSSize pysicalsize = [info.view convertRectToBacking:bounds].size;
                    
                    // Create a grayscale context for the mask
                    CGColorSpaceRef colorspace = CGColorSpaceCreateDeviceGray();
                    CGContextRef maskContext =
                    CGBitmapContextCreate(
                                          NULL,
                                          pysicalsize.width,
                                          pysicalsize.height,
                                          8,
                                          pysicalsize.width,
                                          colorspace,
                                          0);
                    CGColorSpaceRelease(colorspace);
                    
                    // Switch to the context for drawing
                    NSGraphicsContext *maskGraphicsContext =
                    [NSGraphicsContext
                     graphicsContextWithGraphicsPort:maskContext
                     flipped:YES];
                    [NSGraphicsContext saveGraphicsState];
                    [NSGraphicsContext setCurrentContext:maskGraphicsContext];
                    
                    if(pysicalsize.width != logicalsize.width)
                    {
                        auto scale = pysicalsize.width / logicalsize.width;
                        NSAffineTransform *transform = [NSAffineTransform transform];
                        [transform scaleXBy:scale yBy:scale];
                        [transform concat];
                    }
                    
                    [str drawInRect: NSMakeRect(0, 0, bounds.size.width, bounds.size.height) withAttributes : textformat->native2];
                    
                    // Switch back to the window's context
                    [NSGraphicsContext restoreGraphicsState];
                    
                    // Create an image mask from what we've drawn so far
                    CGImageRef alphaMask = CGBitmapContextCreateImage(maskContext);
                    
                    CGContextRef windowContext = (CGContextRef) [[NSGraphicsContext currentContext] graphicsPort];
                    
                    // Draw the fill, clipped by the mask
                    CGContextSaveGState(windowContext);
                    
                    CGContextClipToMask(windowContext, NSRectToCGRect(bounds), alphaMask);
                    
                    if(bmb)
                    {
                        NSBezierPath* rectPath = [NSBezierPath bezierPathWithRect : bounds];
                        bmb->fillPath(this, rectPath);
                    }
                    else if(lgb)
                    {
                        lgb->drawGradient();
                    }
                    
                    CGContextRestoreGState(windowContext);
                    CGImageRelease(alphaMask);
#if 0
                    
                    [NSGraphicsContext saveGraphicsState];
                    
                    auto view = /*context->*/ getNativeView();
                    
#if USE_BACKING_BUFFER
                    gmpi::drawing::SizeU bitmapSize{};
                    const_cast<Bitmap&>(bmb->bitmap_).getSizeU(&bitmapSize);
                    // Adjust offset to be relative to the top (Windows) not bottom (mac)
                    CGFloat yOffset = view.bounds.size.height - bitmapSize.height;
#else
                    // convert to Core Grapics co-ords
                    CGFloat yOffset = NSMaxY([view convertRect:view.bounds toView:nil]);
#endif
                    gmpi::drawing::Matrix3x2 moduleTransform;
                    /*context->*/getTransform(&moduleTransform);
                    auto offset = gmpi::drawing::transformPoint(moduleTransform, {0.0f, 0.0f});
                    // apply brushes transfer. we support only translation on mac
                    offset = gmpi::drawing::transformPoint(bmb->brushProperties_.transform, offset);
                    
                    // also need to apply current drawing transform for modules not at [0,0]
                    
                    [[NSGraphicsContext currentContext] setPatternPhase:NSMakePoint(offset.x, yOffset - offset.y)];
                    [[NSColor colorWithPatternImage:bmb->bitmap_.nativeBitmap_] set];
                    //                [nsPath fill];
                    
                    //               [NSGraphicsContext restoreGraphicsState];
                    extraRestoreState = true;
#endif
                }
            }
            

            
//        }
		//               [textformat->native2[NSParagraphStyleAttributeName] setLineHeightMultiple:1.0f];

#if 0
		{
			Graphics g(this);
			drawing::FontMETRICS fontMetrics;
			((gmpi::drawing::api::ITextFormat*)textFormat)->getFontMetrics(&fontMetrics);
			float pixelScale = 2.0; // DPI ish

			if (true)
			{
				// nailed it.
				float predictedBt = layoutRect->top + fontMetrics.ascent + fontMetrics.descent;
				predictedBt = floor(predictedBt * pixelScale) / pixelScale;
				float predictedBaseline = predictedBt - fontMetrics.descent;
				predictedBaseline = floor(predictedBaseline * pixelScale) / pixelScale;

				if ((fontMetrics.descent - floor(fontMetrics.descent)) < 0.5f)
				{
					predictedBaseline -= 0.5f;
				}

				static auto brush = g.CreateSolidColorBrush(Color::Red);
				g.DrawLine(Point(layoutRect->left, predictedBaseline + 0.25), Point(layoutRect->left + 2, predictedBaseline + 0.25), brush, 0.25f);
			}

			// Absolute vertical position does not matter, because entire row has same error,
			// despite different vertical offsets. Therefore error comes from font metrics.
			if (false)
			{
				float topGap = textformat->topAdjustment;
				float renderTop = layoutRect->top - topGap;

				renderTop = truncatePixel(renderTop, 0.5f);

				float lineHeight = fontMetrics.ascent + fontMetrics.descent;

				lineHeight = truncatePixel(lineHeight, 0.5f);

				float predictedBt = renderTop + topGap + lineHeight;

				//predictedBt = floor(predictedBt * pixelScale) / pixelScale;
				float predictedBaseline = truncatePixel(predictedBt - fontMetrics.descent, 0.5f);

				predictedBaseline = truncatePixel(predictedBaseline, 0.5f);

				static auto brush = g.CreateSolidColorBrush(Color::Red);
				g.DrawLine(Point(layoutRect->left + 2, predictedBaseline + 0.25), Point(layoutRect->left + 4, predictedBaseline + 0.25), brush, 0.25f);
			}

			{
				static auto brush1 = g.CreateSolidColorBrush(Color::Red);
				static auto brush2 = g.CreateSolidColorBrush(Color::Lime);
				static auto brush3 = g.CreateSolidColorBrush(Color::Blue);

				float v = fontMetrics.descent;
				float x = layoutRect->left + (v - floor(v)) * 10.0f;
				float y = layoutRect->top;
				g.DrawLine(Point(x, y), Point(x, y + 5), brush1, 0.25f);

				v = fontMetrics.descent + fontMetrics.ascent;
				x = layoutRect->left + (v - floor(v)) * 10.0f;
				y = layoutRect->top;

				g.DrawLine(Point(x, y), Point(x, y + 5), brush2, 0.25f);

				v = fontMetrics.capHeight;
				x = layoutRect->left + (v - floor(v)) * 10.0f;
				y = layoutRect->top;

				g.DrawLine(Point(x, y), Point(x, y + 5), brush3, 0.25f);

			}
		}
#endif
        return gmpi::ReturnCode::Ok;
	}

	gmpi::ReturnCode drawBitmap(gmpi::drawing::api::IBitmap* mpBitmap, const gmpi::drawing::Rect* destinationRectangle, float opacity, gmpi::drawing::BitmapInterpolationMode interpolationMode, const gmpi::drawing::Rect* sourceRectangle) override
	{
		auto bm = ((Bitmap*)mpBitmap);
				auto bitmap = bm->getNativeBitmap();

                gmpi::drawing::SizeU imageSize;
                bm->getSizeU(&imageSize);

                auto destRect = gmpi::cocoa::NSRectFromRect(*destinationRectangle);
                
#if USE_BACKING_BUFFER
                auto sourceRect = gmpi::cocoa::NSRectFromRect(*sourceRectangle);
                
                // mirror source rectangle
                sourceRect.origin.y = imageSize.height - (sourceRect.origin.y + sourceRect.size.height);
                
                // Create a flipped coordinate system
                [[NSGraphicsContext currentContext] saveGraphicsState];
                NSAffineTransform *transform = [NSAffineTransform transform];
                [transform scaleXBy:1 yBy:-1];
                [transform translateXBy:0 yBy:-2 * destRect.origin.y - destRect.size.height];

                [transform concat];

                // Draw in the flipped coordinate system
#else
                GmpiDrawing::Rect sourceRectangleFlipped(*sourceRectangle);
		sourceRectangleFlipped.bottom = imageSize.height - sourceRectangle->top;
		sourceRectangleFlipped.top = imageSize.height - sourceRectangle->bottom;

                auto sourceRect = gmpi::cocoa::NSRectFromRect(sourceRectangleFlipped);

#endif
		if (bitmap)
		{
			[bitmap drawInRect : destRect fromRect : sourceRect operation : NSCompositingOperationSourceOver fraction : opacity respectFlipped : TRUE hints : nil] ;
		}
		if (bm->additiveBitmap_)
		{
#if 1
			[bm->additiveBitmap_ drawInRect : destRect fromRect : sourceRect operation : NSCompositingOperationPlusLighter fraction : opacity respectFlipped : TRUE hints : nil];

#else // imagerep (don't work due to flip
			auto rect = cocoa::NSRectFromRect(*destinationRectangle);

			// Create a flipped coordinate system (imagerep don't understand flipped)
			[[NSGraphicsContext currentContext]saveGraphicsState];
			NSAffineTransform* transform = [NSAffineTransform transform];
			[transform translateXBy : 0 yBy : rect.size.height] ;
			[transform scaleXBy : 1 yBy : -1] ;
			[transform concat] ;

			// Draw the image in the flipped coordinate system
			[bm->additiveBitmap_ drawInRect : rect fromRect : sourceRect operation : NSCompositingOperationPlusLighter fraction : opacity respectFlipped : TRUE hints : nil] ;

			// Restore the original graphics state
			[[NSGraphicsContext currentContext]restoreGraphicsState];
#endif
		}
#if USE_BACKING_BUFFER
                // Restore the original graphics state
                [[NSGraphicsContext currentContext] restoreGraphicsState];
#endif

        return gmpi::ReturnCode::Ok;
	}

	gmpi::ReturnCode setTransform(const gmpi::drawing::Matrix3x2* transform) override
	{
		// Remove the current transformations by applying the inverse transform.
		try
		{
			[info.currentTransform invert] ;
			[info.currentTransform concat] ;
		}
		catch (...)
		{
			// some transforms are not reversible. e.g. scaling everything down to a point.
			// int test = 9;
		};

		NSAffineTransformStruct transformStruct{
			transform->_11,
			transform->_12,
			transform->_21,
			transform->_22,
			transform->_31,
			transform->_32
		};

		[info.currentTransform setTransformStruct : transformStruct] ;

		[info.currentTransform concat] ;
		return gmpi::ReturnCode::Ok;
	}

	gmpi::ReturnCode getTransform(gmpi::drawing::Matrix3x2* transform) override
	{
		NSAffineTransformStruct
			transformStruct = [info.currentTransform transformStruct];

		transform->_11 = transformStruct.m11;
		transform->_12 = transformStruct.m12;
		transform->_21 = transformStruct.m21;
		transform->_22 = transformStruct.m22;
		transform->_31 = transformStruct.tX;
		transform->_32 = transformStruct.tY;
		return gmpi::ReturnCode::Ok;
	}

	gmpi::ReturnCode createSolidColorBrush(const gmpi::drawing::Color* color, const gmpi::drawing::BrushProperties* brushProperties, gmpi::drawing::api::ISolidColorBrush** returnSolidColorBrush) override
	{
		gmpi::shared_ptr<gmpi::api::IUnknown> b2;
		b2.attach(new SolidColorBrush(color, factory));

		return b2->queryInterface(&gmpi::drawing::api::ISolidColorBrush::guid, reinterpret_cast<void**>(returnSolidColorBrush));
		return gmpi::ReturnCode::Ok;
	}

	gmpi::ReturnCode createGradientstopCollection(const gmpi::drawing::Gradientstop* gradientstops, uint32_t gradientstopsCount, gmpi::drawing::ExtendMode extendMode, gmpi::drawing::api::IGradientstopCollection** returnGradientstopCollection) override
	{
		gmpi::shared_ptr<gmpi::api::IUnknown> b2;
		b2.attach(new GradientstopCollection(factory, gradientstops, gradientstopsCount));

		return b2->queryInterface(&gmpi::drawing::api::IGradientstopCollection::guid, reinterpret_cast<void**>(returnGradientstopCollection));
		return gmpi::ReturnCode::Ok;
	}

	gmpi::ReturnCode createLinearGradientBrush(const gmpi::drawing::LinearGradientBrushProperties* linearGradientBrushProperties, const gmpi::drawing::BrushProperties* brushProperties, gmpi::drawing::api::IGradientstopCollection* gradientstopCollection, gmpi::drawing::api::ILinearGradientBrush** returnLinearGradientBrush) override
	{
		gmpi::shared_ptr<gmpi::api::IUnknown> b2;
		b2.attach(new LinearGradientBrush(factory, linearGradientBrushProperties, brushProperties, gradientstopCollection));

		return b2->queryInterface(&gmpi::drawing::api::ILinearGradientBrush::guid, reinterpret_cast<void**>(returnLinearGradientBrush));
		return gmpi::ReturnCode::Ok;
	}

	gmpi::ReturnCode createBitmapBrush(gmpi::drawing::api::IBitmap* bitmap, /*const gmpi::drawing::BitmapBrushProperties* bitmapBrushProperties,*/ const gmpi::drawing::BrushProperties* brushProperties, gmpi::drawing::api::IBitmapBrush** returnBitmapBrush) override
	{
		*returnBitmapBrush = nullptr;
		gmpi::shared_ptr<gmpi::api::IUnknown> b2;
		b2.attach(new BitmapBrush(factory, bitmap, /*bitmapBrushProperties,*/ brushProperties));
		return b2->queryInterface(&gmpi::drawing::api::IBitmapBrush::guid, reinterpret_cast<void**>(returnBitmapBrush));
		return gmpi::ReturnCode::Ok;
	}

	gmpi::ReturnCode createRadialGradientBrush(const gmpi::drawing::RadialGradientBrushProperties* radialGradientBrushProperties, const gmpi::drawing::BrushProperties* brushProperties, gmpi::drawing::api::IGradientstopCollection* gradientstopCollection, gmpi::drawing::api::IRadialGradientBrush** returnRadialGradientBrush) override
	{
		gmpi::shared_ptr<gmpi::api::IUnknown> b2;
		b2.attach(new RadialGradientBrush(factory, radialGradientBrushProperties, brushProperties, gradientstopCollection));

		return b2->queryInterface(&gmpi::drawing::api::IRadialGradientBrush::guid, reinterpret_cast<void**>(returnRadialGradientBrush));
		return gmpi::ReturnCode::Ok;
	}

	gmpi::ReturnCode createCompatibleRenderTarget(gmpi::drawing::Size desiredSize, int32_t flags, gmpi::drawing::api::IBitmapRenderTarget** bitmapRenderTarget) override;

	gmpi::ReturnCode drawRoundedRectangle(const gmpi::drawing::RoundedRect* roundedRect, gmpi::drawing::api::IBrush* brush, float strokeWidth, gmpi::drawing::api::IStrokeStyle* strokeStyle) override
	{
		NSRect r = cocoa::NSRectFromRect(roundedRect->rect);
		NSBezierPath* path = [NSBezierPath bezierPathWithRoundedRect : r xRadius : roundedRect->radiusX yRadius : roundedRect->radiusY];
		applyDashStyleToPath(path, strokeStyle, strokeWidth);

		auto cocoabrush = dynamic_cast<const CocoaBrushBase*>(brush);
		if (cocoabrush)
		{
			cocoabrush->strokePath(path, strokeWidth, strokeStyle);
		}
		return gmpi::ReturnCode::Ok;
	}

	gmpi::ReturnCode fillRoundedRectangle(const gmpi::drawing::RoundedRect* roundedRect, gmpi::drawing::api::IBrush* brush) override
	{
		NSRect r = cocoa::NSRectFromRect(roundedRect->rect);
		NSBezierPath* rectPath = [NSBezierPath bezierPathWithRoundedRect : r xRadius : roundedRect->radiusX yRadius : roundedRect->radiusY];

		auto cocoabrush = dynamic_cast<const CocoaBrushBase*>(brush);
		if (cocoabrush)
		{
			cocoabrush->fillPath(this, rectPath);
		}
		return gmpi::ReturnCode::Ok;
	}

	gmpi::ReturnCode drawEllipse(const gmpi::drawing::Ellipse* ellipse, gmpi::drawing::api::IBrush* brush, float strokeWidth, gmpi::drawing::api::IStrokeStyle* strokeStyle) override
	{
		NSRect r = NSMakeRect(ellipse->point.x - ellipse->radiusX, ellipse->point.y - ellipse->radiusY, ellipse->radiusX * 2.0f, ellipse->radiusY * 2.0f);

		NSBezierPath* path = [NSBezierPath bezierPathWithOvalInRect : r];
		applyDashStyleToPath(path, strokeStyle, strokeWidth);

		auto cocoabrush = dynamic_cast<const CocoaBrushBase*>(brush);
		if (cocoabrush)
		{
			cocoabrush->strokePath(path, strokeWidth, strokeStyle);
		}
		return gmpi::ReturnCode::Ok;
	}

	gmpi::ReturnCode fillEllipse(const gmpi::drawing::Ellipse* ellipse, gmpi::drawing::api::IBrush* brush) override
	{
		NSRect r = NSMakeRect(ellipse->point.x - ellipse->radiusX, ellipse->point.y - ellipse->radiusY, ellipse->radiusX * 2.0f, ellipse->radiusY * 2.0f);

		NSBezierPath* rectPath = [NSBezierPath bezierPathWithOvalInRect : r];

		auto cocoabrush = dynamic_cast<const CocoaBrushBase*>(brush);
		if (cocoabrush)
		{
			cocoabrush->fillPath(this, rectPath);
		}
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
        
        // Save the current clipping region
        [NSGraphicsContext saveGraphicsState];
        
        NSRectClip(NSRectFromRect(*clipRect));

        return gmpi::ReturnCode::Ok;
	}

	gmpi::ReturnCode popAxisAlignedClip() override
	{
        info.clipRectStack.pop_back();

		// Restore the clipping region for further drawing
		[NSGraphicsContext restoreGraphicsState] ;
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
		//		context_->BeginDraw();
		return gmpi::ReturnCode::Ok;
	}

	gmpi::ReturnCode endDraw() override
	{
		//		auto hr = context_->EndDraw();

		//		return hr == S_OK ? (MP_OK) : (gmpi::ReturnCode::Fail);
		return gmpi::ReturnCode::Ok;
	}

	NSView* getNativeView()
	{
		return info.view;
	}

	//	void InsetNewMethodHere(){}

	GMPI_QUERYINTERFACE_METHOD(gmpi::drawing::api::IDeviceContext);
	GMPI_REFCOUNT_NO_DELETE;
};

// https://stackoverflow.com/questions/10627557/mac-os-x-drawing-into-an-offscreen-nsgraphicscontext-using-cgcontextref-c-funct
class BitmapRenderTarget : public GraphicsContext // emulated by carefull layout public gmpi::drawing::api::IBitmapRenderTarget
{
	NSImage* image{};
    int32_t creationFlags = (int32_t)drawing::BitmapRenderTargetFlags::EightBitPixels;
    
public:
	BitmapRenderTarget(const gmpi::drawing::Size* size, int32_t flags, cocoa::Factory* pfactory) :
		GraphicsContext(nullptr, pfactory)
        ,creationFlags(flags)
	{
        const bool oneChannelMask = flags & (int32_t)gmpi::drawing::BitmapRenderTargetFlags::Mask;
        const bool lockAblePixels = flags & (int32_t)gmpi::drawing::BitmapRenderTargetFlags::CpuReadable;
        const bool eightBitPixels = flags & (int32_t)gmpi::drawing::BitmapRenderTargetFlags::EightBitPixels;

        NSSize s = NSMakeSize(size->width, size->height);
        NSRect r = NSMakeRect(0.0, 0.0, s.width, s.height);
        
        if(oneChannelMask)
        {
            NSBitmapImageRep* bitmap =
            [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:NULL
            pixelsWide:s.width pixelsHigh:s.height bitsPerSample:8
            samplesPerPixel:1 hasAlpha:NO isPlanar:NO
            colorSpaceName:NSCalibratedWhiteColorSpace bytesPerRow:0
            bitsPerPixel:8];
            
            image = [[NSImage alloc] initWithSize:s];
            [image addRepresentation:bitmap];
        }
        else
        {
            image = [[NSImage alloc]initWithSize:s];
        }
        
        info.currentTransform = [NSAffineTransform transform];
        info.clipRectStack.push_back({ 0, 0, size->width, size->height });
	}

    gmpi::ReturnCode beginDraw() override
	{
		// To match Flipped View, Flip Bitmap Context too.
		// (Alternative is [image setFlipped:TRUE] in constructor, but that method is deprected).
		[image lockFocusFlipped : TRUE] ;

		return GraphicsContext::beginDraw();
	}

	gmpi::ReturnCode endDraw() override
	{
		auto r = GraphicsContext::endDraw();
		[image unlockFocus];
		return r;
	}

#if !__has_feature(objc_arc)
	~BitmapRenderTarget()
	{
		[image release] ;
	}
#endif
	// MUST BE FIRST VIRTUAL FUNCTION!
	virtual gmpi::ReturnCode getBitmap(gmpi::drawing::api::IBitmap** returnBitmap)
	{
		gmpi::shared_ptr<gmpi::api::IUnknown> b;
		b.attach(new Bitmap(factory, image, creationFlags));
		return b->queryInterface(&gmpi::drawing::api::IBitmap::guid, reinterpret_cast<void**>(returnBitmap));
	}

	gmpi::ReturnCode queryInterface(const gmpi::api::Guid* iid, void** returnInterface) override
	{
		*returnInterface = {};
        if (*iid == gmpi::drawing::api::IBitmapRenderTarget::guid)
        {
            // non-standard. Forcing this class (which has the correct vtable) to pretend it's the emulated interface.
            *returnInterface = reinterpret_cast<gmpi::drawing::api::IBitmapRenderTarget*>(this);
            addRef();
            return gmpi::ReturnCode::Ok;
        }
		return GraphicsContext::queryInterface(iid, returnInterface);
	}

	GMPI_REFCOUNT;
};

gmpi::ReturnCode GraphicsContext::createCompatibleRenderTarget(gmpi::drawing::Size desiredSize, int32_t flags, gmpi::drawing::api::IBitmapRenderTarget** bitmapRenderTarget)
{
	gmpi::shared_ptr<gmpi::api::IUnknown> b2;
	b2.attach(new class BitmapRenderTarget(&desiredSize, flags, factory));

	return b2->queryInterface(&gmpi::drawing::api::IBitmapRenderTarget::guid, reinterpret_cast<void**>(bitmapRenderTarget));
}

inline gmpi::ReturnCode Factory::createTextFormat(const char* fontFamilyName, gmpi::drawing::FontWeight fontWeight, gmpi::drawing::FontStyle fontStyle, gmpi::drawing::FontStretch fontStretch, float fontSize, int32_t fontFlags, gmpi::drawing::api::ITextFormat** textFormat)
{
	gmpi::shared_ptr<gmpi::api::IUnknown> b2;
	b2.attach(new TextFormat(/*&stringConverter,*/ fontFamilyName, fontWeight, fontStyle, fontStretch, fontSize));

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
	bm.attach(new Bitmap(this, width, height));

	return bm->queryInterface(&gmpi::drawing::api::IBitmap::guid, (void**)returnDiBitmap);
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

void BitmapBrush::fillPath(GraphicsContext* context, NSBezierPath* nsPath) const
{
    [NSGraphicsContext saveGraphicsState];

    auto view = context->getNativeView();
    
#if USE_BACKING_BUFFER
    gmpi::drawing::SizeU bitmapSize{};
    const_cast<Bitmap&>(bitmap_).getSizeU(&bitmapSize);
    // Adjust offset to be relative to the top (Windows) not bottom (mac)
    CGFloat yOffset = view.bounds.size.height - bitmapSize.height;
#else
    // convert to Core Grapics co-ords
    CGFloat yOffset = NSMaxY([view convertRect:view.bounds toView:nil]);
#endif
    gmpi::drawing::Matrix3x2 moduleTransform;
    context->getTransform(&moduleTransform);
    auto offset = gmpi::drawing::transformPoint(moduleTransform, {0.0f, 0.0f});
    // apply brushes transfer. we support only translation on mac
    offset = gmpi::drawing::transformPoint(brushProperties_.transform, offset);
    
    // also need to apply current drawing transform for modules not at [0,0]
    
    [[NSGraphicsContext currentContext] setPatternPhase:NSMakePoint(offset.x, yOffset - offset.y)];
    [[NSColor colorWithPatternImage:bitmap_.nativeBitmap_] set];
    [nsPath fill];
    
    [NSGraphicsContext restoreGraphicsState];
}

inline BitmapPixels::BitmapPixels(Bitmap* sebitmap /*NSImage** inBitmap*/, bool _alphaPremultiplied, int32_t pflags) :
	inBitmap_(&sebitmap->nativeBitmap_ /*inBitmap*/)
	, flags(pflags)
	, seBitmap(sebitmap)
{
	NSSize s = [*inBitmap_ size];
    
    int samplesPerPixel = 4;
    auto hasAlpha = YES;
    auto colorSpace = NSCalibratedRGBColorSpace;
    const bool isMask = 0 != (flags & (int32_t)gmpi::drawing::BitmapRenderTargetFlags::Mask );
    
    if(isMask)
    {
        samplesPerPixel = 1;
        hasAlpha = NO;
        colorSpace = NSCalibratedWhiteColorSpace;
    }

    {
        bytesPerRow = s.width * samplesPerPixel;
        
        constexpr int bitsPerSample = 8;
        const int bitsPerPixel = bitsPerSample * samplesPerPixel;
        
        auto initial_bitmap = [[NSBitmapImageRep alloc]initWithBitmapDataPlanes:nil
                                                                    pixelsWide : s.width
                                                                    pixelsHigh : s.height
                                                                 bitsPerSample : bitsPerSample
                                                               samplesPerPixel : samplesPerPixel
                                                                      hasAlpha : hasAlpha
                                                                      isPlanar : NO
                                                                colorSpaceName : colorSpace
                                                                  bitmapFormat : 0
                                                                   bytesPerRow : bytesPerRow
                                                                  bitsPerPixel : bitsPerPixel];
        if(isMask)
            bitmap2 = initial_bitmap; //[initial_bitmap bitmapImageRepByRetaggingWithColorSpace : NSColorSpace.sRGBColorSpace];
       else
            bitmap2 = [initial_bitmap bitmapImageRepByRetaggingWithColorSpace : NSColorSpace.sRGBColorSpace];
        
        [bitmap2 retain] ;
        
        // Copy the image to the new imageRep (effectivly converts it to correct pixel format/brightness etc)
        if (0 != (flags & (int) gmpi::drawing::BitmapLockFlags::Read))
        {
            NSGraphicsContext* context;
            context = [NSGraphicsContext graphicsContextWithBitmapImageRep : bitmap2];
            [NSGraphicsContext saveGraphicsState] ;
            [NSGraphicsContext setCurrentContext : context] ;
            [*inBitmap_ drawAtPoint : NSZeroPoint fromRect : NSZeroRect operation : NSCompositingOperationCopy fraction : 1.0] ;
            
            [NSGraphicsContext restoreGraphicsState] ;
        }
    }
}

inline BitmapPixels::~BitmapPixels()
{
    const bool isMask = 0 != (flags & (int32_t)gmpi::drawing::BitmapRenderTargetFlags::Mask );

	if (!isMask && 0 != (flags & (int) gmpi::drawing::BitmapLockFlags::Write))
	{
		// scan for overbright pixels
		bool hasOverbright = false;
		{
			gmpi::drawing::SizeU imageSize{};
			seBitmap->getSizeU(&imageSize);
            
            int32_t bytesPerRow{};
            getBytesPerRow(&bytesPerRow);
            
			const int totalPixels = imageSize.height * bytesPerRow / sizeof(uint32_t);

            uint32_t* destPixels{};
            getAddress((uint8_t**) &destPixels);

			for (int i = 0; i < totalPixels; ++i)
			{
				uint8_t* p = (uint8_t*)destPixels;
				auto& alpha = p[3];
				if (alpha != 255 && *destPixels != 0) // skip solid or blank pixels.
				{
					if (p[0] > alpha || p[1] > alpha || p[2] > alpha)
					{
						hasOverbright = true;
						break;
					}
				}

				++destPixels;
			}
		}

		if (hasOverbright)
		{
			// create and populate the additive bitmap
			NSSize s = [bitmap2 size];

			constexpr int bitsPerSample = 8;
			constexpr int samplesPerPixel = 4;
			constexpr int bitsPerPixel = bitsPerSample * samplesPerPixel;

			NSBitmapImageRep* initial_bitmap = [[NSBitmapImageRep alloc]initWithBitmapDataPlanes:nil
				pixelsWide : s.width
				pixelsHigh : s.height
				bitsPerSample : bitsPerSample
				samplesPerPixel : samplesPerPixel
				hasAlpha : YES
				isPlanar : NO
				colorSpaceName : NSCalibratedRGBColorSpace
				bitmapFormat : 0
				bytesPerRow : bytesPerRow
				bitsPerPixel : bitsPerPixel];

			auto source = (uint32_t*)([bitmap2 bitmapData]);
			auto dest = (uint32_t*)([initial_bitmap bitmapData]);

			const int totalPixels = s.height * bytesPerRow / sizeof(uint32_t);

			for (int i = 0; i < totalPixels; ++i)
			{
				// Retain Alpha of bitmap2, but black-out RGB. Copy RGB to additiveBitmap_ and set alpha to 255.
				*dest = 0xFF000000 | *source; //0x88008888;
				*source &= 0xFF000000;

				++source;
				++dest;
			}
			seBitmap->additiveBitmap_ = [initial_bitmap bitmapImageRepByRetaggingWithColorSpace : NSColorSpace.sRGBColorSpace];

			[seBitmap->additiveBitmap_ retain] ;
		}

		// replace bitmap with a fresh one, and add pixels to it.
		*inBitmap_ = [[NSImage alloc]init];
		[*inBitmap_ addRepresentation : bitmap2] ;
	}
	else
	{
		[bitmap2 release];
	}
}
} // namespace
} // namespace
