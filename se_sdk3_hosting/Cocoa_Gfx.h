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
#include <codecvt>
#include <map>
#include "../Drawing.h"
#include "../shared/xp_simd.h"
#include "../backends/Gfx_base.h"

/* TODO
investigate CGContextSetShouldSmoothFonts, CGContextSetAllowsFontSmoothing (for less heavy fonts)
*/

namespace gmpi
{
namespace cocoa
{
class DrawingFactory;
    
// Conversion utilities.
    
inline NSPoint toNative(drawing::Point p)
{
    return NSMakePoint(p.x, p.y);
}
    
inline drawing::Rect RectFromNSRect(NSRect nsr)
{
    return{
        static_cast<float>(nsr.origin.x),
        static_cast<float>(nsr.origin.y),
        static_cast<float>(nsr.origin.x + nsr.size.width),
        static_cast<float>(nsr.origin.y + nsr.size.height)
    };
}

inline NSRect NSRectFromRect(drawing::Rect rect)
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
void SetNativePenStrokeStyle(NSBezierPath * path, drawing::api::IStrokeStyle* strokeStyle)
{
    drawing::MP1_CAP_STYLE capstyle = strokeStyle == nullptr ? drawing::MP1_CAP_STYLE_FLAT : strokeStyle->GetStartCap();
            
    switch(capstyle)
    {
        default:
        case drawing::MP1_CAP_STYLE_FLAT:
            [ path setLineCapStyle:NSLineCapStyleButt ];
            break;
                    
        case drawing::MP1_CAP_STYLE_SQUARE:
            [ path setLineCapStyle:NSLineCapStyleSquare ];
            break;
                    
        case drawing::MP1_CAP_STYLE_TRIANGLE:
        case drawing::MP1_CAP_STYLE_ROUND:
            [ path setLineCapStyle:NSLineCapStyleRound ];
            break;
    }
}
    
void applyDashStyleToPath(NSBezierPath* path, const drawing::api::IStrokeStyle* strokeStyleIm, float strokeWidth)
{
    auto strokeStyle = const_cast<drawing::api::IStrokeStyle*>(strokeStyleIm); // work arround const-correctness issues.

    const auto dashStyle = strokeStyle ? strokeStyle->GetDashStyle() : drawing::MP1_DASH_STYLE_SOLID;
    const auto phase = strokeStyle ? strokeStyle->GetDashOffset() : 0.0f;
            
    std::vector<CGFloat> dashes;
            
    switch(dashStyle)
    {
        case drawing::MP1_DASH_STYLE_SOLID:
            break;
                    
        case drawing::MP1_DASH_STYLE_CUSTOM:
        {
            std::vector<float> dashesf;
            dashesf.resize(strokeStyle->GetDashesCount());
            strokeStyle->GetDashes(dashesf.data(), static_cast<uint32_t>(dashesf.size()));
                    
            for(auto d : dashesf)
            {
                dashes.push_back((CGFloat) (d * strokeWidth));
            }
        }
        break;
                    
        case drawing::MP1_DASH_STYLE_DOT:
            dashes.push_back(0.0f);
            dashes.push_back(strokeWidth * 2.f);
            break;
                    
        case drawing::MP1_DASH_STYLE_DASH_DOT:
            dashes.push_back(strokeWidth * 2.f);
            dashes.push_back(strokeWidth * 2.f);
            dashes.push_back(0.f);
            dashes.push_back(strokeWidth * 2.f);
            break;
                    
        case drawing::MP1_DASH_STYLE_DASH_DOT_DOT:
            dashes.push_back(strokeWidth * 2.f);
            dashes.push_back(strokeWidth * 2.f);
            dashes.push_back(0.f);
            dashes.push_back(strokeWidth * 2.f);
            dashes.push_back(0.f);
            dashes.push_back(strokeWidth * 2.f);
            break;
                    
        case drawing::MP1_DASH_STYLE_DASH:
        default:
            dashes.push_back(strokeWidth * 2.f);
            dashes.push_back(strokeWidth * 2.f);
            break;
    };
                        
    [path setLineDash: dashes.data() count: dashes.size() phase: phase];
}

// Classes without GetFactory()
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

// Classes with GetFactory()
template<class MpInterface, class CocoaType>
class CocoaWrapperWithFactory : public CocoaWrapper<MpInterface, CocoaType>
{
protected:
	drawing::api::IFactory* factory_;

public:
	CocoaWrapperWithFactory(CocoaType* native, drawing::api::IFactory* factory) : CocoaWrapper<MpInterface, CocoaType>(native), factory_(factory) {}

	ReturnCode getFactory(drawing::api::IFactory** factory) override
	{
		*factory = factory_;
		return ReturnCode::Ok;
	}
};

class nothing
{
};
    










class TextFormat final : public drawing::api::ITextFormat // : public CocoaWrapper<drawing::api::ITextFormat, const __CFDictionary>
{
public:
    std::string fontFamilyName;
    drawing::MP1_FONT_WEIGHT fontWeight;
    drawing::MP1_FONT_STYLE fontStyle;
    drawing::MP1_FONT_STRETCH fontStretch;
    drawing::MP1_TEXT_ALIGNMENT textAlignment;
    drawing::MP1_PARAGRAPH_ALIGNMENT paragraphAlignment;
    drawing::MP1_WORD_WRAPPING wordWrapping = drawing::MP1_WORD_WRAPPING_WRAP;
    float fontSize;
    bool useLegacyBaseLineSnapping = true;

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

    TextFormat(std::wstring_convert<std::codecvt_utf8<wchar_t>>* pstringConverter, const char* pfontFamilyName, drawing::MP1_FONT_WEIGHT pfontWeight, drawing::MP1_FONT_STYLE pfontStyle, drawing::MP1_FONT_STRETCH pfontStretch, float pfontSize) :
        //				CocoaWrapper<drawing::api::ITextFormat, const __CFDictionary>(nullptr)
        fontWeight(pfontWeight)
        , fontStyle(pfontStyle)
        , fontStretch(pfontStretch)
        , fontSize(pfontSize)
    {
        //_RPT1(0, "TextFormat() %d\n", this);

        fontFamilyName = fontSubstitute(pfontFamilyName);

        NSFontTraitMask fontTraits = 0;
        if (pfontWeight >= drawing::MP1_FONT_WEIGHT_DEMI_BOLD)
            fontTraits |= NSBoldFontMask;

        if (pfontStyle == drawing::MP1_FONT_STYLE_ITALIC)
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
            const int nativeFontWeight = 1 + (pfontWeight + roundNearest) / 100;

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
                NSFontWeightUltraLight, // MP1_FONT_WEIGHT_THIN = 100
                NSFontWeightThin,       // MP1_FONT_WEIGHT_ULTRA_LIGHT = 200
                NSFontWeightLight,      // MP1_FONT_WEIGHT_LIGHT = 300
                NSFontWeightRegular,    // MP1_FONT_WEIGHT_NORMAL = 400
                NSFontWeightMedium,     // MP1_FONT_WEIGHT_MEDIUM = 500
                NSFontWeightSemibold,   // MP1_FONT_WEIGHT_SEMI_BOLD = 600
                NSFontWeightBold,       // MP1_FONT_WEIGHT_BOLD = 700
                NSFontWeightHeavy,      // MP1_FONT_WEIGHT_BLACK = 900
                NSFontWeightBlack       // MP1_FONT_WEIGHT_ULTRA_BLACK = 950
            };

            const int arrMax = std::size(weightConversion) - 1;
            const int roundNearest = 50;
            const int nativeFontWeightIndex
                = std::max(0, std::min(arrMax, -1 + (pfontWeight + roundNearest) / 100));
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
        drawing::MP1_FONT_METRICS fontMetrics{};
        GetFontMetrics(&fontMetrics);

        auto boundingBoxSize = [@"A" sizeWithAttributes : native2];

        topAdjustment = boundingBoxSize.height - (fontMetrics.ascent + fontMetrics.descent);
        ascent = fontMetrics.ascent;

        baselineCorrection = 0.5f;
        if ((fontMetrics.descent - floor(fontMetrics.descent)) <= 0.5f)
        {
            baselineCorrection += 0.5f;
        }
    }

	ReturnCode setTextAlignment(drawing::TextAlignment textAlignment) override
    {
        textAlignment = ptextAlignment;

        switch (textAlignment)
        {
        case (int)TextAlignment::Leading: // Left.
            [native2[NSParagraphStyleAttributeName] setAlignment:NSTextAlignmentLeft] ;
            break;
        case (int)TextAlignment::Trailing: // Right
            [native2[NSParagraphStyleAttributeName] setAlignment:NSTextAlignmentRight] ;
            break;
        case (int)TextAlignment::Center:
            [native2[NSParagraphStyleAttributeName] setAlignment:NSTextAlignmentCenter] ;
            break;
        }

        return ReturnCode::Ok;
    }

	ReturnCode setParagraphAlignment(drawing::ParagraphAlignment paragraphAlignment) override
    {
        paragraphAlignment = pparagraphAlignment;
        return ReturnCode::Ok;
    }

	ReturnCode setWordWrapping(drawing::WordWrapping wordWrapping) override
    {
        wordWrapping = pwordWrapping;
        return ReturnCode::Ok;
    }


    ReturnCode getFontMetrics(drawing::FontMetrics* returnFontMetrics) override
    {
        NSFont* font = native2[NSFontAttributeName];  // Get font from dictionary.
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
        return ReturnCode::Ok;
    }

    // TODO!!!: Probly needs to accept constraint rect like DirectWrite. !!!
    ReturnCode getTextExtentU(const char* utf8String, int32_t stringLength, drawing::Size* returnSize) override
    {
        auto str = [NSString stringWithCString : utf8String encoding : NSUTF8StringEncoding];

        auto r = [str sizeWithAttributes : native2];
        returnSize->width = r.width;
        returnSize->height = r.height;// - topAdjustment;

        if (!useLegacyBaseLineSnapping)
        {
            returnSize->height -= topAdjustment;
        }
    }
	
    ReturnCode setLineSpacing(float lineSpacing, float baseline) override
    {
        // Hack, reuse this method to enable legacy-mode.
		if (static_cast<float>(ITextFormat::ImprovedVerticalBaselineSnapping) == lineSpacing)
        {
            useLegacyBaseLineSnapping = false;
            return ReturnCode::Ok;
        }

        return ReturnCode::NoSupport;
    }

    bool getUseLegacyBaseLineSnapping() const
    {
        return useLegacyBaseLineSnapping;
    }

	GMPI_QUERYINTERFACE_NEW(drawing::api::ITextFormat);
    GMPI_REFCOUNT;
};

class BitmapPixels final : public drawing::api::IBitmapPixels
{
    int bytesPerRow;
    class Bitmap* seBitmap = {};
    NSImage** inBitmap_;
    NSBitmapImageRep* bitmap2 = {};
    int32_t flags;

public:
    bitmapPixels(Bitmap* bitmap /*NSImage** inBitmap*/, bool _alphaPremultiplied, int32_t pflags);
    ~bitmapPixels();

    ReturnCode getAddress(uint8_t** returnAddress) override
    {
        *returnAddress = static_cast<uint8_t*>([bitmap2 bitmapData]);
		return ReturnCode::Ok;
    }
    ReturnCode getBytesPerRow(int32_t* returnBytesPerRow) override
    {
        *returnBytesPerRow = bytesPerRow;
        return ReturnCode::Ok;
    }

    ReturnCode getPixelFormat(int32_t* returnPixelFormat) override
    {
        *returnPixelFormat = kRGBA;
        return ReturnCode::Ok;
    }

    inline uint8_t fast8bitScale(uint8_t a, uint8_t b) const
    {
        int t = (int)a * (int)b;
        return (uint8_t)((t + 1 + (t >> 8)) >> 8); // fast way to divide by 255
    }

    GMPI_QUERYINTERFACE_NEW(drawing::api::IBitmapPixels);
    GMPI_REFCOUNT;
};

class Bitmap final : public drawing::api::IBitmap
{
    drawing::api::IFactory* factory = nullptr;

public:
    NSImage* nativeBitmap_ = nullptr;
    NSBitmapImageRep* additiveBitmap_ = nullptr;

    Bitmap(drawing::api::IFactory* pfactory, const char* utf8Uri) :
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

    Bitmap(drawing::api::IFactory* pfactory, int32_t width, int32_t height)
        : factory(pfactory)
    {
        //                _RPT1(0, "Bitmap() B: %d\n", this);

        nativeBitmap_ = [[NSImage alloc]initWithSize:NSMakeSize((CGFloat)width, (CGFloat)height)];
        // not sure yet                [nativeBitmap_ setFlipped:TRUE];

#if !__has_feature(objc_arc)
//                [nativeBitmap_ retain];
#endif
    }

    Bitmap(drawing::api::IFactory* pfactory, NSImage* native) : nativeBitmap_(native)
        , factory(pfactory)
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

    inline NSImage* GetNativeBitmap()
    {
        return nativeBitmap_;
    }
/*
    drawing::Size GetSizeF() override
    {
        NSSize s = [nativeBitmap_ size];
        return Size(s.width, s.height);
    }
*/

    ReturnCode getSizeU(drawing::SizeU* returnSize) override
    {
        NSSize s = [nativeBitmap_ size];

        returnSize->width = FastRealToIntTruncateTowardZero(0.5f + s.width);
        returnSize->height = FastRealToIntTruncateTowardZero(0.5f + s.height);

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
        return ReturnCode::Ok;
    }
#if 0
    int32_t lockPixelsOld(drawing::api::IBitmapPixels** returnInterface, bool alphaPremultiplied) override
    {
        *returnInterface = 0;
        return MP_FAIL;
        /* TODO!!!
                    gmpi_sdk::mp_shared_ptr<api::IUnknown> b2;
                    b2.Attach(new bitmapPixels(&nativeBitmap_, alphaPremultiplied, drawing::MP1_BITMAP_LOCK_READ | drawing::MP1_BITMAP_LOCK_WRITE));

                    return b2->queryInterface(drawing::SE_IID_BITMAP_PIXELS_MPGUI, (void**)(returnInterface));
        */
    }
#endif

	ReturnCode lockPixels(drawing::api::IBitmapPixels** returnPixels, int32_t flags) override;
    {
        //               _RPT1(0, "Bitmap() lockPixels: %d\n", this);
        *returnPixels = 0;

        gmpi_sdk::mp_shared_ptr<api::IUnknown> b2;
        b2.Attach(new bitmapPixels(this /*&nativeBitmap_*/, true, flags));

        return b2->queryInterface(drawing::api::IBitmapPixels::guid, (void**)(returnPixels));
    }

    void ApplyAlphaCorrection() override {}

    void ApplyAlphaCorrection2() {}

    ReturnCode getFactory(drawing::api::IFactory** returnFactory) override
    {
        *returnFactory = factory;
        return ReturnCode::Ok;
    }

    GMPI_QUERYINTERFACE_NEW(drawing::api::IBitmap);
    GMPI_REFCOUNT;
};

class GradientstopCollection : public CocoaWrapperWithFactory<drawing::api::IGradientstopCollection, nothing>
{
public:
    std::vector<drawing::MP1_GRADIENT_STOP> gradientstops;

    GradientstopCollection(drawing::api::IFactory* factory, const drawing::MP1_GRADIENT_STOP* gradientStops, uint32_t gradientStopsCount) : CocoaWrapperWithFactory(nullptr, factory)
    {
        for (uint32_t i = 0; i < gradientStopsCount; ++i)
        {
            gradientstops.push_back(gradientStops[i]);
        }
    }
    GMPI_QUERYINTERFACE_NEW(drawing::api::IGradientstopCollection);
};

class BitmapBrush final : public drawing::api::IBitmapBrush, public CocoaBrushBase
{
    Bitmap bitmap_;
    drawing::MP1_BITMAP_BRUSH_PROPERTIES bitmapBrushProperties_;
    drawing::MP1_BRUSH_PROPERTIES brushProperties_;

public:
    BitmapBrush(
        cocoa::DrawingFactory* factory,
        const drawing::api::IBitmap* bitmap,
        const drawing::MP1_BITMAP_BRUSH_PROPERTIES* bitmapBrushProperties,
        const drawing::MP1_BRUSH_PROPERTIES* brushProperties
    )
        : CocoaBrushBase(factory),
        bitmap_(factory, ((Bitmap*)bitmap)->nativeBitmap_),
        bitmapBrushProperties_(*bitmapBrushProperties),
        brushProperties_(*brushProperties)
    {
    }

    void StrokePath(NSBezierPath* nsPath, float strokeWidth, const drawing::api::IStrokeStyle* strokeStyle = nullptr) const override
    {
        [[NSColor colorWithPatternImage : bitmap_.nativeBitmap_]set];

        [nsPath setLineWidth : strokeWidth] ;
        SetNativePenStrokeStyle(nsPath, (drawing::api::IStrokeStyle*)strokeStyle);

        [nsPath stroke] ;
    }
    void FillPath(GraphicsContext* context, NSBezierPath* nsPath) const override;

	ReturnCode setExtendModeX(drawing::ExtendMode extendModeX) override
    {
        //                native()->SetExtendModeX((D2D1_EXTEND_MODE)extendModeX);
		return ReturnCode::Ok;
    }

	ReturnCode setExtendModeY(drawing::ExtendMode extendModeY) override
    {
        //               native()->SetExtendModeY((D2D1_EXTEND_MODE)extendModeY);
		return ReturnCode::Ok;
    }

    ReturnCode setInterpolationMode(drawing::BitmapInterpolationMode bitmapInterpolationMode) override
    {
        //              native()->SetInterpolationMode((D2D1_BITMAP_INTERPOLATION_MODE)interpolationMode);
		return ReturnCode::Ok;
    }

    void GetFactory(drawing::api::IFactory** factory) override
    {
        *factory = factory_;
    }

    GMPI_REFCOUNT;
    GMPI_QUERYINTERFACE_NEW(drawing::api::IBitmapBrush);
};

/*
class Brush : / * public drawing::api::IBrush,* / public CocoaWrapperWithFactory<drawing::api::IBrush, nothing> // Resource
{
public:
    Brush(drawing::api::IFactory* factory) : CocoaWrapperWithFactory(nullptr, factory) {}
};
*/

class CocoaBrushBase
{
protected:
    cocoa::DrawingFactory* factory_;

public:
    CocoaBrushBase(cocoa::DrawingFactory* pfactory) :
        factory_(pfactory)
    {}

    virtual ~CocoaBrushBase() {}

    virtual void FillPath(class GraphicsContext* context, NSBezierPath* nsPath) const = 0;

    // Default to black fill for fancy brushes that don't implement line drawing yet.
    virtual void StrokePath(NSBezierPath* nsPath, float strokeWidth, const drawing::api::IStrokeStyle* strokeStyle = nullptr) const
    {
        [[NSColor blackColor]set]; /// !!!TODO!!!, color set to black always.

        [nsPath setLineWidth : strokeWidth] ;
        SetNativePenStrokeStyle(nsPath, (drawing::api::IStrokeStyle*)strokeStyle);

        [nsPath stroke] ;
    }
};

class SolidColorBrush : public drawing::api::ISolidColorBrush, public CocoaBrushBase
{
    drawing::MP1_COLOR color;
    NSColor* nativec_ = nullptr;

    inline void setNativeColor()
    {
        nativec_ = factory_->toNative(color);
        [nativec_ retain] ;
    }

public:
    SolidColorBrush(const drawing::MP1_COLOR* pcolor, cocoa::DrawingFactory* factory) : CocoaBrushBase(factory)
        , color(*pcolor)
    {
        setNativeColor();
    }

    inline NSColor* nativeColor() const
    {
        return nativec_;
    }

    void FillPath(GraphicsContext* context, NSBezierPath* nsPath) const override
    {
        [nativec_ set] ;
        [nsPath fill] ;
    }

    void StrokePath(NSBezierPath* nsPath, float strokeWidth, const drawing::api::IStrokeStyle* strokeStyle = nullptr) const override
    {
        [nativec_ set] ;
        [nsPath setLineWidth : strokeWidth] ;
        SetNativePenStrokeStyle(nsPath, (drawing::api::IStrokeStyle*)strokeStyle);

        [nsPath stroke] ;
    }

    ~SolidColorBrush()
    {
        [nativec_ release] ;
    }

    // IMPORTANT: Virtual functions much 100% match drawing::api::ISolidColorBrush to simulate inheritance.
	ReturnCode setColor(const drawing::Color* color) override
    {
        color = *pcolor;
        setNativeColor();
		return ReturnCode::Ok;
    }

    drawing::MP1_COLOR GetColor() override
    {
        return color;
    }

	ReturnCode getFactory(drawing::api::IFactory** factory) override
    {
        *factory = factory_;
		return ReturnCode::Ok;
    }

	ReturnCode queryInterface(const gmpi::api::Guid* iid, void** returnInterface) override
	{
		*returnInterface = {};
		if (*iid == drawing::api::ISolidColorBrush::guid || *iid == drawing::api::IBrush::guid || *iid == drawing::api::IResource::guid || *iid == gmpi::api::IUnknown::guid)
		{
			*returnInterface = this;
			addRef();
			return ReturnCode::Ok;
		}
		return ReturnCode::NoSupport;
	}

	GMPI_REFCOUNT;
};
class LinearGradientBrush final : public drawing::api::ILinearGradientBrush, public CocoaBrushBase, public Gradient
{
    drawing::MP1_LINEAR_GRADIENT_BRUSH_PROPERTIES brushProperties;

public:
    LinearGradientBrush(
        cocoa::DrawingFactory* factory,
        const drawing::MP1_LINEAR_GRADIENT_BRUSH_PROPERTIES* linearGradientBrushProperties,
        const drawing::MP1_BRUSH_PROPERTIES* brushProperties,
        const drawing::api::IGradientstopCollection* gradientStopCollection) :
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

	void setStartPoint(drawing::Point startPoint) override
    {
        brushProperties.startPoint = startPoint;
    }
    void setEndPoint(drawing::Point endPoint) override
    {
        brushProperties.endPoint = endPoint;
    }

    void FillPath(GraphicsContext* context, NSBezierPath* nsPath) const override
    {
        //				[native2 drawInBezierPath:nsPath angle : getAngle()];

                    // If you plan to do more drawing later, it's a good idea
                    // to save the graphics state before clipping.
        [NSGraphicsContext saveGraphicsState];

        // clip following output to the path
        [nsPath addClip] ;

        [native2 drawFromPoint : toNative(brushProperties.endPoint) toPoint : toNative(brushProperties.startPoint) options : NSGradientDrawsBeforeStartingLocation | NSGradientDrawsAfterEndingLocation] ;

        // restore clip region
        [NSGraphicsContext restoreGraphicsState] ;
    }

    void StrokePath(NSBezierPath* nsPath, float strokeWidth, const drawing::api::IStrokeStyle* strokeStyle = nullptr) const override
    {
        SetNativePenStrokeStyle(nsPath, (drawing::api::IStrokeStyle*)strokeStyle);

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

        [native2 drawFromPoint : toNative(brushProperties.endPoint) toPoint : toNative(brushProperties.startPoint) options : NSGradientDrawsBeforeStartingLocation | NSGradientDrawsAfterEndingLocation] ;

        // restore clip region
        [NSGraphicsContext restoreGraphicsState] ;

        CGPathRelease(strokePath);
    }

	ReturnCode getFactory(drawing::api::IFactory** factory) override
    {
        *factory = factory_;
		return ReturnCode::Ok;
    }

    GMPI_REFCOUNT;
    GMPI_QUERYINTERFACE1(drawing::SE_IID_LINEARGRADIENTBRUSH_MPGUI, drawing::api::ILinearGradientBrush);
};

class RadialGradientBrush : public drawing::api::IRadialGradientBrush, public CocoaBrushBase, public Gradient
{
    drawing::MP1_RADIAL_GRADIENT_BRUSH_PROPERTIES gradientProperties;

public:
    RadialGradientBrush(cocoa::DrawingFactory* factory, const drawing::MP1_RADIAL_GRADIENT_BRUSH_PROPERTIES* radialGradientBrushProperties, const drawing::MP1_BRUSH_PROPERTIES* brushProperties, const  drawing::api::IGradientstopCollection* gradientStopCollection) :
        CocoaBrushBase(factory)
        , Gradient(factory, gradientStopCollection)
        , gradientProperties(*radialGradientBrushProperties)
    {
    }

    void FillPath(GraphicsContext* context, NSBezierPath* nsPath) const override
    {
        const auto bounds = [nsPath bounds];

        const auto centerX = bounds.origin.x + 0.5 * std::max(0.1, bounds.size.width);
        const auto centerY = bounds.origin.y + 0.5 * std::max(0.1, bounds.size.height);

        auto relativeX = (gradientProperties.center.x - centerX) / (0.5 * bounds.size.width);
        auto relativeY = (gradientProperties.center.y - centerY) / (0.5 * bounds.size.height);

        relativeX = std::max(-1.0, std::min(1.0, relativeX));
        relativeY = std::max(-1.0, std::min(1.0, relativeY));

        const auto origin = NSMakePoint(
            gradientProperties.center.x + gradientProperties.gradientOriginOffset.x,
            gradientProperties.center.y + gradientProperties.gradientOriginOffset.y);

        // If you plan to do more drawing later, it's a good idea
        // to save the graphics state before clipping.
        [NSGraphicsContext saveGraphicsState];

        // clip following output to the path
        [nsPath addClip] ;
        /*
                    [native2 drawFromCenter:origin
                        radius:0.0
                        toCenter:toNative(gradientProperties.center)
                        radius:gradientProperties.radiusX
                        options:NSGradientDrawsAfterEndingLocation];
        */

        [native2 drawFromCenter : toNative(gradientProperties.center)
            radius : gradientProperties.radiusX
            toCenter : origin
            radius : 0.0
            options : NSGradientDrawsBeforeStartingLocation | NSGradientDrawsAfterEndingLocation];

        // restore clip region
        [NSGraphicsContext restoreGraphicsState] ;
    }

    void StrokePath(NSBezierPath* nsPath, float strokeWidth, const drawing::api::IStrokeStyle* strokeStyle = nullptr) const override
    {
        SetNativePenStrokeStyle(nsPath, (drawing::api::IStrokeStyle*)strokeStyle);

        // convert NSPath to CGPath
        CGPathRef strokePath;
        {
            CGMutablePathRef cgPath = NsToCGPath(nsPath);

            strokePath = CGPathCreateCopyByStrokingPath(cgPath, NULL, strokeWidth, (CGLineCap)[nsPath lineCapStyle],
                (CGLineJoin)[nsPath lineJoinStyle], [nsPath miterLimit]);
            CGPathRelease(cgPath);
        }

        const auto origin = NSMakePoint(
            gradientProperties.center.x + gradientProperties.gradientOriginOffset.x,
            gradientProperties.center.y + gradientProperties.gradientOriginOffset.y);

        // If you plan to do more drawing later, it's a good idea
        // to save the graphics state before clipping.
        [NSGraphicsContext saveGraphicsState];

        // clip following output to the path
        CGContextRef ctx = (CGContextRef) [[NSGraphicsContext currentContext]graphicsPort];

        CGContextAddPath(ctx, strokePath);
        CGContextClip(ctx);

        [native2 drawFromCenter : toNative(gradientProperties.center)
            radius : gradientProperties.radiusX
            toCenter : origin
            radius : 0.0
            options : NSGradientDrawsBeforeStartingLocation | NSGradientDrawsAfterEndingLocation] ;

        // restore clip region
        [NSGraphicsContext restoreGraphicsState] ;

        CGPathRelease(strokePath);
    }

    void setCenter(drawing::Point center) override
    {
        gradientProperties.center = center;
    }

    void setGradientOriginOffset(drawing::Point gradientOriginOffset) override
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

	ReturnCode queryInterface(const gmpi::api::Guid* iid, void** returnInterface) override
	{
		*returnInterface = {};
		if (*iid == drawing::api::IRadialGradientBrush::guid || *iid == drawing::api::IBrush::guid || *iid == drawing::api::IResource::guid || *iid == gmpi::api::IUnknown::guid)
		{
			*returnInterface = this;
			addRef();
			return ReturnCode::Ok;
		}
		return ReturnCode::NoSupport;
	}

	// IResource
	ReturnCode getFactory(drawing::api::IFactory** factory) override
    {
        *factory = factory_;
    }

    GMPI_REFCOUNT;
};


class DrawingFactory : public drawing::api::IFactory2
{
    std::vector<std::string> supportedFontFamilies;
        
public:
    std::wstring_convert<std::codecvt_utf8<wchar_t>> stringConverter; // cached, as constructor is super-slow.
    NSColorSpace* gmpiColorSpace = {};
        
    DrawingFactory()
    {
#if 0
        int maxPixelDepth = 0;
        for(auto windowDepth = NSAvailableWindowDepths() ; *windowDepth ; ++windowDepth)
        {
            maxPixelDepth = std::max(maxPixelDepth, (int) NSBitsPerSampleFromDepth(*windowDepth));
        }
            
        if(maxPixelDepth > 8)
        {
                
            colorSpace = CGColorSpaceCreateWithName(kCGColorSpaceExtendedLinearSRGB);
        }
        else
        {
                
            colorSpace = CGColorSpaceCreateWithName(kCGColorSpaceGenericRGBLinear);
        }
        // kCGColorSpaceGenericRGBLinear - proper linear gradients, banding on dark-grey gradient bitmap.
        // kCGColorSpaceExtendedLinearDisplayP3, kCGColorSpaceExtendedLinearSRGB
         
        nsColorSpace = [[NSColorSpace alloc] initWithCGColorSpace:colorSpace];
#endif
    }
        
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
        {
            auto temp = CGColorSpaceCreateWithName(kCGColorSpaceExtendedLinearSRGB);
            // auto temp = CGColorSpaceCreateWithName(kCGColorSpaceLinearSRGB); // no difference on big sur
            gmpiColorSpace = [[NSColorSpace alloc] initWithCGColorSpace:temp];
                
// no diff on big sur               CGContextRef ctx = (CGContextRef) [[NSGraphicsContext currentContext] graphicsPort];
//                CGContextSetFillColorSpace(ctx, temp);
                
            if(temp)
                CFRelease(temp);
        }
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
        
    // utility
    inline NSColor* toNative(const drawing::MP1_COLOR& color)
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
#endif
        const CGFloat components[4] = {color.r, color.g, color.b, color.a};
        return [NSColor colorWithColorSpace:gmpiColorSpace components:components count:4];
            
// I think this is wrong because it's saying that the gmpi color is in the screen's color space. Which is a crapshoot out of our control.
// we need to supply our color in a known color space, then let the OS convert it to whatever unknown color-space the screen is using.
//            return [NSColor colorWithColorSpace:nsColorSpace components:components count:4]; // washed-out on big Sur (with extended linier) good with genericlinier
    }

    int32_t CreatePathGeometry(drawing::api::IPathGeometry** pathGeometry) override;

    int32_t CreateTextFormat(const char* fontFamilyName, void* unused /* fontCollection */, drawing::MP1_FONT_WEIGHT fontWeight, drawing::MP1_FONT_STYLE fontStyle, drawing::MP1_FONT_STRETCH fontStretch, float fontSize, void* unused2 /* localeName */, drawing::api::ITextFormat** textFormat) override;

    int32_t CreateImage(int32_t width, int32_t height, drawing::api::IBitmap** returnDiBitmap) override;

    int32_t LoadImageU(const char* utf8Uri, drawing::api::IBitmap** returnDiBitmap) override;

    int32_t CreateStrokeStyle(const drawing::MP1_STROKE_STYLE_PROPERTIES* strokeStyleProperties, float* dashes, int32_t dashesCount, drawing::api::IStrokeStyle** returnValue) override
    {
        *returnValue = nullptr;

        gmpi_sdk::mp_shared_ptr<api::IUnknown> b2;
        b2.Attach(new generic_graphics::StrokeStyle(this, strokeStyleProperties, dashes, dashesCount));

        return b2->queryInterface(drawing::SE_IID_STROKESTYLE_MPGUI, reinterpret_cast<void **>(returnValue));
    }

    // IMpFactory2
    int32_t GetFontFamilyName(int32_t fontIndex, IString* returnString) override
    {
        if(supportedFontFamilies.empty())
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
                supportedFontFamilies.push_back([familyName UTF8String]);
            }
        }
            
        if (fontIndex < 0 || fontIndex >= supportedFontFamilies.size())
        {
            return MP_FAIL;
        }

        returnString->setData(supportedFontFamilies[fontIndex].data(), static_cast<int32_t>(supportedFontFamilies[fontIndex].size()));
        return ReturnCode::Ok;
    }

    int32_t queryInterface(const MpGuid& iid, void** returnInterface) override
    {
        *returnInterface = 0;
        if ( iid == drawing::SE_IID_FACTORY2_MPGUI || iid == GmpiDrawing_API::SE_IID_FACTORY_MPGUI || iid == MP_IID_UNKNOWN)
        {
            *returnInterface = reinterpret_cast<drawing::api::IFactory2*>(this);
            addRef();
            return ReturnCode::Ok;
        }
        return MP_NOSUPPORT;
    }

    GMPI_REFCOUNT_NO_DELETE;
};
class Gradient
{
protected:
	NSGradient* native2 = {};

public:
	Gradient(cocoa::DrawingFactory* factory, const drawing::api::IGradientstopCollection* gradientStopCollection)
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

		native2 = [[NSGradient alloc]initWithColors:colors atLocations : locations2.data() colorSpace : factory->gmpiColorSpace];
	}

	~Gradient()
	{
		[native2 release] ;
	}
};

class GeometrySink final : public generic_graphics::GeometrySink
{
	NSBezierPath* geometry_;

public:
	GeometrySink(NSBezierPath* geometry) : geometry_(geometry)
	{}

	void SetFillMode(drawing::MP1_FILL_MODE fillMode) override
	{
		switch (fillMode)
		{
		case drawing::MP1_FILL_MODE_ALTERNATE:
		case drawing::MP1_FILL_MODE_FORCE_DWORD:
			[geometry_ setWindingRule : NSEvenOddWindingRule] ;
			break;

		case drawing::MP1_FILL_MODE_WINDING:
			[geometry_ setWindingRule : NSNonZeroWindingRule] ;
			break;
		}
	}
#if 0
	void SetSegmentFlags(drawing::MP1_PATH_SEGMENT vertexFlags)
	{
		//		geometrysink_->SetSegmentFlags((D2D1_PATH_SEGMENT)vertexFlags);
	}
#endif
	void beginFigure(drawing::Point startPoint, drawing::FigureBegin figureBegin) override
	{
		[geometry_ moveToPoint : NSMakePoint(startPoint.x, startPoint.y)] ;

		lastPoint = startPoint;
	}

	void endFigure(drawing::FigureEnd figureEnd) override
	{
		if (figureEnd == drawing::MP1_FIGURE_END_CLOSED)
		{
			[geometry_ closePath] ;
		}
	}

	void addLine(drawing::Point point) override
	{
		[geometry_ lineToPoint : NSMakePoint(point.x, point.y)] ;

		lastPoint = point;
	}

	void addBezier(const drawing::BezierSegment* bezier) override
	{
		[geometry_ curveToPoint : NSMakePoint(bezier->point3.x, bezier->point3.y)
			controlPoint1 : NSMakePoint(bezier->point1.x, bezier->point1.y)
			controlPoint2 : NSMakePoint(bezier->point2.x, bezier->point2.y)] ;

		lastPoint = bezier->point3;
	}

	GMPI_QUERYINTERFACE_NEW(drawing::api::IGeometrySink);

	GMPI_REFCOUNT;
};


class PathGeometry final : public drawing::api::IPathGeometry
{
	NSBezierPath* geometry_ = {};

	drawing::MP1_DASH_STYLE currentDashStyle = drawing::MP1_DASH_STYLE_SOLID;
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

	ReturnCode open(drawing::api::IGeometrySink** returnGeometrySink) override
	{
		if (geometry_)
		{
			[geometry_ release] ;
		}
		geometry_ = [NSBezierPath bezierPath];
		[geometry_ retain] ;

		gmpi_sdk::mp_shared_ptr<api::IUnknown> b2;
		b2.Attach(new GeometrySink(geometry_));

		return b2->queryInterface(GmpiDrawing_API::SE_IID_GEOMETRYSINK_MPGUI, reinterpret_cast<void**>(geometrySink));
	}

	ReturnCode getFactory(drawing::api::IFactory** factory) override
	{
		//		native_->GetFactory((ID2D1Factory**)factory);
	}

	ReturnCode strokeContainsPoint(drawing::Point point, float strokeWidth, drawing::api::IStrokeStyle* strokeStyle, const drawing::Matrix3x2* worldTransform, bool* returnContains) override
	{
		auto cgPath2 = NsToCGPath(geometry_);

		CGPathRef hitTargetPath = CGPathCreateCopyByStrokingPath(cgPath2, NULL, (CGFloat)strokeWidth, (CGLineCap)[geometry_ lineCapStyle], (CGLineJoin)[geometry_ lineJoinStyle], [geometry_ miterLimit]);

		CGPathRelease(cgPath2);

		CGPoint cgpoint = CGPointMake(point.x, point.y);
		*returnContains = (bool)CGPathContainsPoint(hitTargetPath, NULL, cgpoint, (bool)[geometry_ windingRule]);

		CGPathRelease(hitTargetPath);
		return ReturnCode::Ok;
	}

	ReturnCode fillContainsPoint(drawing::Point point, const drawing::Matrix3x2* worldTransform, bool* returnContains) override
	{
		*returnContains = [geometry_ containsPoint : NSMakePoint(point.x, point.y)];
		return ReturnCode::Ok;
	}

	ReturnCode getWidenedBounds(float strokeWidth, drawing::api::IStrokeStyle* strokeStyle, const drawing::Matrix3x2* worldTransform, drawing::Rect* returnBounds) override
	{
		const float radius = ceilf(strokeWidth * 0.5f);
		auto nativeRect = [geometry_ bounds];
		returnBounds->left = nativeRect.origin.x - radius;
		returnBounds->top = nativeRect.origin.y - radius;
		returnBounds->right = nativeRect.origin.x + nativeRect.size.width + radius;
		returnBounds->bottom = nativeRect.origin.y + nativeRect.size.height + radius;

		return ReturnCode::Ok;
	}

	void applyDashStyle(const drawing::api::IStrokeStyle* strokeStyleIm, float strokeWidth)
	{
		auto strokeStyle = const_cast<drawing::api::IStrokeStyle*>(strokeStyleIm); // work arround const-correctness issues.

		const auto dashStyle = strokeStyle ? strokeStyle->GetDashStyle() : drawing::MP1_DASH_STYLE_SOLID;
		const auto phase = strokeStyle ? strokeStyle->GetDashOffset() : 0.0f;

		bool changed = currentDashStyle != dashStyle;
		currentDashStyle = dashStyle;

		if (currentDashStyle == drawing::MP1_DASH_STYLE_CUSTOM)
		{
			const auto customDashesCount = strokeStyle->GetDashesCount();
			std::vector<float> dashesf;
			dashesf.resize(customDashesCount);
			strokeStyle->GetDashes(dashesf.data(), static_cast<int>(dashesf.size()));

			changed |= customDashesCount != currentCustomDashStyle.size();
			currentCustomDashStyle.resize(customDashesCount);

			for (int i = 0; i < customDashesCount; ++i)
			{
				changed |= currentCustomDashStyle[i] != dashesf[i];
				currentCustomDashStyle[i] = dashesf[i];
			}
		}

		if (currentDashStyle != drawing::MP1_DASH_STYLE_SOLID) // i.e. 'none'
		{
			changed |= phase != currentDashPhase;
			currentDashPhase = phase;
		}

		if (!changed)
		{
			return;
		}

		applyDashStyleToPath(geometry_, strokeStyle, strokeWidth);
	}

	GMPI_QUERYINTERFACE_NEW(drawing::api::IPathGeometry);
	GMPI_REFCOUNT;
};

class GraphicsContext : public drawing::api::IDeviceContext
{
protected:
	std::wstring_convert<std::codecvt_utf8<wchar_t>>* stringConverter; // cached, as constructor is super-slow.
	cocoa::DrawingFactory* factory;
	std::vector<drawing::Rect> clipRectStack;
	NSAffineTransform* currentTransform;
	NSView* view_;

public:
	GraphicsContext(NSView* pview, cocoa::DrawingFactory* pfactory) :
		factory(pfactory)
		, view_(pview)
	{
		currentTransform = [NSAffineTransform transform];
	}

	~GraphicsContext()
	{
	}

	ReturnCode getFactory(drawing::api::IFactory** pfactory) override
	{
		*pfactory = factory;
		return ReturnCode::Ok;
	}

	ReturnCode drawRectangle(const drawing::Rect* rect, drawing::api::IBrush* brush, float strokeWidth, drawing::api::IStrokeStyle* strokeStyle) override
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
			cocoabrush->StrokePath(path, strokeWidth, strokeStyle);
		}
		return ReturnCode::Ok;
	}

	ReturnCode fillRectangle(const drawing::Rect* rect, drawing::api::IBrush* brush) override
	{
		NSBezierPath* rectPath = [NSBezierPath bezierPathWithRect : cocoa::NSRectFromRect(*rect)];

		auto cocoabrush = dynamic_cast<const CocoaBrushBase*>(brush);
		if (cocoabrush)
		{
			cocoabrush->FillPath(this, rectPath);
		}
		return ReturnCode::Ok;
	}

	ReturnCode clear(const drawing::Color* clearColor) override
	{
		SolidColorBrush brush(clearColor, factory);
		Rect r;
		GetAxisAlignedClip(&r);
		NSBezierPath* rectPath = [NSBezierPath bezierPathWithRect : NSRectFromRect(r)];
		brush.FillPath(this, rectPath);
		return ReturnCode::Ok;
	}

	ReturnCode drawLine(drawing::Point point0, drawing::Point point1, drawing::api::IBrush* brush, float strokeWidth, drawing::api::IStrokeStyle* strokeStyle) override
	{
		NSBezierPath* path = [NSBezierPath bezierPath];
		[path moveToPoint : NSMakePoint(point0.x, point0.y)] ;
		[path lineToPoint : NSMakePoint(point1.x, point1.y)] ;

		auto cocoabrush = dynamic_cast<const CocoaBrushBase*>(brush);
		if (cocoabrush)
		{
			applyDashStyleToPath(path, const_cast<drawing::api::IStrokeStyle*>(strokeStyle), strokeWidth);
			cocoabrush->StrokePath(path, strokeWidth, strokeStyle);
		}
		return ReturnCode::Ok;
	}

	ReturnCode drawGeometry(drawing::api::IPathGeometry* pathGeometry, drawing::api::IBrush* brush, float strokeWidth, drawing::api::IStrokeStyle* strokeStyle) override;
	{
		auto pg = (PathGeometry*)geometry;
		pg->applyDashStyle(strokeStyle, strokeWidth);

		auto cocoabrush = dynamic_cast<const CocoaBrushBase*>(brush);
		if (cocoabrush)
		{
			cocoabrush->StrokePath(pg->native(), strokeWidth, strokeStyle);
		}
		return ReturnCode::Ok;
	}

	ReturnCode fillGeometry(drawing::api::IPathGeometry* pathGeometry, drawing::api::IBrush* brush, drawing::api::IBrush* opacityBrush) override
	{
		auto nsPath = ((PathGeometry*)geometry)->native();

		auto cocoabrush = dynamic_cast<const CocoaBrushBase*>(brush);
		if (cocoabrush)
		{
			cocoabrush->FillPath(this, nsPath);
		}
		return ReturnCode::Ok;
	}

	inline float truncatePixel(float y, float stepSize)
	{
		return floor(y / stepSize) * stepSize;
	}

	inline float roundPixel(float y, float stepSize)
	{
		return floor(y / stepSize + 0.5f) * stepSize;
	}

	ReturnCode drawTextU(const char* string, uint32_t stringLength, drawing::api::ITextFormat* textFormat, const drawing::Rect* layoutRect, drawing::api::IBrush* defaultForegroundBrush, int32_t options) override;
	{
		auto textformat = reinterpret_cast<const TextFormat*>(textFormat);

		auto scb = dynamic_cast<const SolidColorBrush*>(brush);

		CGRect bounds = CGRectMake(layoutRect->left, layoutRect->top, layoutRect->right - layoutRect->left, layoutRect->bottom - layoutRect->top);
		/*
				if (stringLength > 4 && utf8String[4] == 'q')
				{
					int test=3;
					bounds.size.height = 100.0f;
				}
		*/
		Size textSize{};
		if (textformat->paragraphAlignment != (int)TextAlignment::Leading
			|| flags != drawing::MP1_DRAW_TEXT_OPTIONS_CLIP)
		{
			const_cast<drawing::api::ITextFormat*>(textFormat)->GetTextExtentU(utf8String, (int32_t)strlen(utf8String), &textSize);
		}

		if (textformat->paragraphAlignment != (int)TextAlignment::Leading)
		{
			// Vertical text alignment.
			switch (textformat->paragraphAlignment)
			{
			case (int)TextAlignment::Trailing:    // Bottom
				bounds.origin.y += bounds.size.height - textSize.height;
				bounds.size.height = textSize.height;
				break;

			case (int)TextAlignment::Center:
				bounds.origin.y += (bounds.size.height - textSize.height) / 2;
				bounds.size.height = textSize.height;
				break;

			default:
				break;
			}
		}

		NSString* str = [NSString stringWithCString : utf8String encoding : NSUTF8StringEncoding];

		[textformat->native2 setObject : scb->nativeColor() forKey : NSForegroundColorAttributeName] ;

		const bool clipToRect = flags & drawing::MP1_DRAW_TEXT_OPTIONS_CLIP;

		if (textformat->wordWrapping == drawing::MP1_WORD_WRAPPING_NO_WRAP)
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
					case drawing::MP1_TEXT_ALIGNMENT_CENTER:
						bounds.origin.x -= 0.5 * extendWidth;
						break;
					case drawing::MP1_TEXT_ALIGNMENT_TRAILING: // Right
						bounds.origin.x -= extendWidth;
					case drawing::MP1_TEXT_ALIGNMENT_LEADING: // Left
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

		//                drawing::MP1_FONT_METRICS fontMetrics;
		//               ((drawing::api::ITextFormat*) textFormat)->GetFontMetrics(&fontMetrics);

		//                float testLineHeightMultiplier = 0.5f;
		//                float shiftUp = testLineHeightMultiplier * fontMetrics.bodyHeight();

				// macOS draws extra padding at top of text bounds. Compensate for it.
		if (!textformat->getUseLegacyBaseLineSnapping())
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
			const float macBaselineCorrection = winBaseline - baseline + textformat->baselineCorrection;
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

		[str drawInRect : bounds withAttributes : textformat->native2];

		//               [textformat->native2[NSParagraphStyleAttributeName] setLineHeightMultiple:1.0f];

#if 0
		{
			Graphics g(this);
			drawing::MP1_FONT_METRICS fontMetrics;
			((drawing::api::ITextFormat*)textFormat)->GetFontMetrics(&fontMetrics);
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

			/*
						// clip baseline, then bottom. Not close
						float predictedBaseline = layoutRect->top + fontMetrics.ascent;
						predictedBaseline = floor(predictedBaseline * pixelScale) / pixelScale;

						float predictedBt = predictedBaseline + fontMetrics.descent;
						predictedBt = floor(predictedBt * pixelScale) / pixelScale;
						predictedBaseline = predictedBt - fontMetrics.descent;
			*/

		}
#endif
	}

	ReturnCode drawBitmap(drawing::api::IBitmap* bitmap, const drawing::Rect* destinationRectangle, float opacity, drawing::BitmapInterpolationMode interpolationMode, const drawing::Rect* sourceRectangle) override
	{
		auto bm = ((Bitmap*)mpBitmap);
		auto bitmap = bm->GetNativeBitmap();

		drawing::MP1_SIZE_U imageSize;
		bm->GetSize(&imageSize);

		Rect sourceRectangleFlipped(*sourceRectangle);
		sourceRectangleFlipped.bottom = imageSize.height - sourceRectangle->top;
		sourceRectangleFlipped.top = imageSize.height - sourceRectangle->bottom;

		auto sourceRect = cocoa::NSRectFromRect(sourceRectangleFlipped);
		auto destRect = cocoa::NSRectFromRect(*destinationRectangle);

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
		return ReturnCode::Ok;
	}

	ReturnCode setTransform(const drawing::Matrix3x2* transform) override
	{
		// Remove the current transformations by applying the inverse transform.
		try
		{
			[currentTransform invert] ;
			[currentTransform concat] ;
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

		[currentTransform setTransformStruct : transformStruct] ;

		[currentTransform concat] ;
		return ReturnCode::Ok;
	}

	ReturnCode getTransform(drawing::Matrix3x2* transform) override
	{
		NSAffineTransformStruct
			transformStruct = [currentTransform transformStruct];

		transform->_11 = transformStruct.m11;
		transform->_12 = transformStruct.m12;
		transform->_21 = transformStruct.m21;
		transform->_22 = transformStruct.m22;
		transform->_31 = transformStruct.tX;
		transform->_32 = transformStruct.tY;
		return ReturnCode::Ok;
	}

	ReturnCode createSolidColorBrush(const drawing::Color* color, const drawing::BrushProperties* brushProperties, drawing::api::ISolidColorBrush** returnSolidColorBrush) override;
	{
		gmpi_sdk::mp_shared_ptr<api::IUnknown> b2;
		b2.Attach(new SolidColorBrush(color, factory));

		return b2->queryInterface(GmpiDrawing_API::SE_IID_SOLIDCOLORBRUSH_MPGUI, reinterpret_cast<void**>(solidColorBrush));
		return ReturnCode::Ok;
	}

	ReturnCode createGradientstopCollection(const drawing::Gradientstop* gradientstops, uint32_t gradientstopsCount, drawing::ExtendMode extendMode, drawing::api::IGradientstopCollection** returnGradientstopCollection) override;
	{
		gmpi_sdk::mp_shared_ptr<api::IUnknown> b2;
		b2.Attach(new GradientstopCollection(factory, gradientStops, gradientStopsCount));

		return b2->queryInterface(GmpiDrawing_API::SE_IID_GRADIENTSTOPCOLLECTION_MPGUI, reinterpret_cast<void**>(gradientStopCollection));
		return ReturnCode::Ok;
	}

	ReturnCode createLinearGradientBrush(const drawing::LinearGradientBrushProperties* linearGradientBrushProperties, const drawing::BrushProperties* brushProperties, drawing::api::IGradientstopCollection* gradientstopCollection, drawing::api::ILinearGradientBrush** returnLinearGradientBrush) override
	{
		gmpi_sdk::mp_shared_ptr<api::IUnknown> b2;
		b2.Attach(new LinearGradientBrush(factory, linearGradientBrushProperties, brushProperties, gradientStopCollection));

		return b2->queryInterface(GmpiDrawing_API::SE_IID_LINEARGRADIENTBRUSH_MPGUI, reinterpret_cast<void**>(linearGradientBrush));
		return ReturnCode::Ok;
	}

	ReturnCode createBitmapBrush(drawing::api::IBitmap* bitmap, const drawing::BitmapBrushProperties* bitmapBrushProperties, const drawing::BrushProperties* brushProperties, drawing::api::IBitmapBrush** returnBitmapBrush) override
	{
		*returnBrush = nullptr;
		gmpi_sdk::mp_shared_ptr<api::IUnknown> b2;
		b2.Attach(new BitmapBrush(factory, bitmap, bitmapBrushProperties, brushProperties));
		return b2->queryInterface(GmpiDrawing_API::SE_IID_BITMAPBRUSH_MPGUI, reinterpret_cast<void**>(returnBrush));
		return ReturnCode::Ok;
	}

	ReturnCode createRadialGradientBrush(const drawing::RadialGradientBrushProperties* radialGradientBrushProperties, const drawing::BrushProperties* brushProperties, drawing::api::IGradientstopCollection* gradientstopCollection, drawing::api::IRadialGradientBrush** returnRadialGradientBrush) override
	{
		gmpi_sdk::mp_shared_ptr<api::IUnknown> b2;
		b2.Attach(new RadialGradientBrush(factory, radialGradientBrushProperties, brushProperties, gradientStopCollection));

		return b2->queryInterface(GmpiDrawing_API::SE_IID_RADIALGRADIENTBRUSH_MPGUI, reinterpret_cast<void**>(radialGradientBrush));
		return ReturnCode::Ok;
	}

	ReturnCode createCompatibleRenderTarget(drawing::Size desiredSize, drawing::api::IBitmapRenderTarget** bitmapRenderTarget) override;

	ReturnCode drawRoundedRectangle(const drawing::RoundedRect* roundedRect, drawing::api::IBrush* brush, float strokeWidth, drawing::api::IStrokeStyle* strokeStyle) override
	{
		NSRect r = cocoa::NSRectFromRect(roundedRect->rect);
		NSBezierPath* path = [NSBezierPath bezierPathWithRoundedRect : r xRadius : roundedRect->radiusX yRadius : roundedRect->radiusY];
		applyDashStyleToPath(path, strokeStyle, strokeWidth);

		auto cocoabrush = dynamic_cast<const CocoaBrushBase*>(brush);
		if (cocoabrush)
		{
			cocoabrush->StrokePath(path, strokeWidth, strokeStyle);
		}
		return ReturnCode::Ok;
	}

	ReturnCode fillRoundedRectangle(const drawing::RoundedRect* roundedRect, drawing::api::IBrush* brush) override
	{
		NSRect r = cocoa::NSRectFromRect(roundedRect->rect);
		NSBezierPath* rectPath = [NSBezierPath bezierPathWithRoundedRect : r xRadius : roundedRect->radiusX yRadius : roundedRect->radiusY];

		auto cocoabrush = dynamic_cast<const CocoaBrushBase*>(brush);
		if (cocoabrush)
		{
			cocoabrush->FillPath(this, rectPath);
		}
		return ReturnCode::Ok;
	}

	ReturnCode drawEllipse(const drawing::Ellipse* ellipse, drawing::api::IBrush* brush, float strokeWidth, drawing::api::IStrokeStyle* strokeStyle) override
	{
		NSRect r = NSMakeRect(ellipse->point.x - ellipse->radiusX, ellipse->point.y - ellipse->radiusY, ellipse->radiusX * 2.0f, ellipse->radiusY * 2.0f);

		NSBezierPath* path = [NSBezierPath bezierPathWithOvalInRect : r];
		applyDashStyleToPath(path, strokeStyle, strokeWidth);

		auto cocoabrush = dynamic_cast<const CocoaBrushBase*>(brush);
		if (cocoabrush)
		{
			cocoabrush->StrokePath(path, strokeWidth, strokeStyle);
		}
		return ReturnCode::Ok;
	}

	ReturnCode fillEllipse(const drawing::Ellipse* ellipse, drawing::api::IBrush* brush) override
	{
		NSRect r = NSMakeRect(ellipse->point.x - ellipse->radiusX, ellipse->point.y - ellipse->radiusY, ellipse->radiusX * 2.0f, ellipse->radiusY * 2.0f);

		NSBezierPath* rectPath = [NSBezierPath bezierPathWithOvalInRect : r];

		auto cocoabrush = dynamic_cast<const CocoaBrushBase*>(brush);
		if (cocoabrush)
		{
			cocoabrush->FillPath(this, rectPath);
		}
		return ReturnCode::Ok;
	}

	ReturnCode pushAxisAlignedClip(const drawing::Rect* clipRect) override
	{
		Matrix3x2 currentTransform;
		GetTransform(&currentTransform);
		auto absClipRect = currentTransform.TransformRect(*clipRect);

		if (!clipRectStack.empty())
			absClipRect = Intersect(absClipRect, Rect(clipRectStack.back()));

		clipRectStack.push_back(absClipRect);

		// Save the current clipping region
		[NSGraphicsContext saveGraphicsState] ;

		NSRectClip(NSRectFromRect(*clipRect));
		return ReturnCode::Ok;
	}

	ReturnCode popAxisAlignedClip() override
	{
		clipRectStack.pop_back();

		// Restore the clipping region for further drawing
		[NSGraphicsContext restoreGraphicsState] ;
		return ReturnCode::Ok;
	}

	void getAxisAlignedClip(drawing::Rect* returnClipRect) override
	{
		Matrix3x2 currentTransform;
		GetTransform(&currentTransform);
		currentTransform.Invert();
		*returnClipRect = currentTransform.TransformRect(clipRectStack.back());
	}

	ReturnCode beginDraw() override
	{
		//		context_->BeginDraw();
		return ReturnCode::Ok;
	}

	ReturnCode endDraw() override
	{
		//		auto hr = context_->EndDraw();

		//		return hr == S_OK ? (MP_OK) : (MP_FAIL);
		return ReturnCode::Ok;
	}

	NSView* getNativeView()
	{
		return view_;
	}
	/*
		int getQuartzYorigin()
		{
			const auto frameSize = [view_ frame];
			return frameSize.size.height;
		}

		int32_t CreateMesh(drawing::api::IMesh** returnObject) override
		{
			*returnObject = nullptr;
			return MP_FAIL;
		}

		void FillMesh(const drawing::api::IMesh* mesh, const drawing::api::IBrush* brush) override
		{

		}
	*/
	//	void InsetNewMethodHere(){}

	GMPI_QUERYINTERFACE1(GmpiDrawing_API::SE_IID_DEVICECONTEXT_MPGUI, drawing::api::IDeviceContext);
	GMPI_REFCOUNT_NO_DELETE;
};

// https://stackoverflow.com/questions/10627557/mac-os-x-drawing-into-an-offscreen-nsgraphicscontext-using-cgcontextref-c-funct
class BitmapRenderTarget : public GraphicsContext // emulated by carefull layout public drawing::api::IBitmapRenderTarget
{
	NSImage* image = {};

public:
	BitmapRenderTarget(cocoa::DrawingFactory* pfactory, const drawing::Size* desiredSize) :
		GraphicsContext(nullptr, pfactory)
	{
		NSRect r = NSMakeRect(0.0, 0.0, desiredSize->width, desiredSize->height);
		image = [[NSImage alloc]initWithSize:r.size];

		clipRectStack.push_back({ 0, 0, desiredSize->width, desiredSize->height });
	}

	void BeginDraw() override
	{
		// To match Flipped View, Flip Bitmap Context too.
		// (Alternative is [image setFlipped:TRUE] in constructor, but that method is deprected).
		[image lockFocusFlipped : TRUE] ;

		GraphicsContext::BeginDraw();
	}

	int32_t EndDraw() override
	{
		auto r = GraphicsContext::EndDraw();
		[image unlockFocus] ;
		return r;
	}

#if !__has_feature(objc_arc)
	~bitmapRenderTarget()
	{
		[image release] ;
	}
#endif
	// MUST BE FIRST VIRTUAL FUNCTION!
	virtual ReturnCode getBitmap(drawing::api::IBitmap** returnBitmap)
	{
		gmpi_sdk::mp_shared_ptr<api::IUnknown> b;
		b.Attach(new Bitmap(factory, image));
		return b->queryInterface(GmpiDrawing_API::SE_IID_BITMAP_MPGUI, reinterpret_cast<void**>(returnBitmap));
	}

	ReturnCode queryInterface(const gmpi::api::Guid* iid, void** returnInterface) override
	{
		*returnInterface = {};
		if (*iid == drawing::api::IBitmapRenderTarget::guid)
		{
			// non-standard. Forcing this class (which has the correct vtable) to pretend it's the emulated interface.
			*returnInterface = reinterpret_cast<drawing::api::IBitmapRenderTarget*>(this);
			addRef();
			return ReturnCode::Ok;
		}

		return GraphicsContext_base::queryInterface(iid, returnInterface);
	}

	GMPI_REFCOUNT;
};

int32_t GraphicsContext::createCompatibleRenderTarget(const drawing::Size* desiredSize, drawing::api::IBitmapRenderTarget** bitmapRenderTarget)
{
	gmpi_sdk::mp_shared_ptr<api::IUnknown> b2;
	b2.Attach(new class bitmapRenderTarget(factory, desiredSize));

	return b2->queryInterface(GmpiDrawing_API::SE_IID_BITMAP_RENDERTARGET_MPGUI, reinterpret_cast<void**>(bitmapRenderTarget));
}


inline int32_t DrawingFactory::CreateTextFormat(const char* fontFamilyName, void* unused /* fontCollection */, drawing::MP1_FONT_WEIGHT fontWeight, drawing::MP1_FONT_STYLE fontStyle, drawing::MP1_FONT_STRETCH fontStretch, float fontSize, void* unused2 /* localeName */, drawing::api::ITextFormat** textFormat)
{
	gmpi_sdk::mp_shared_ptr<api::IUnknown> b2;
	b2.Attach(new TextFormat(&stringConverter, fontFamilyName, fontWeight, fontStyle, fontStretch, fontSize));

	return b2->queryInterface(GmpiDrawing_API::SE_IID_TEXTFORMAT_MPGUI, reinterpret_cast<void**>(textFormat));
}

inline int32_t DrawingFactory::CreatePathGeometry(drawing::api::IPathGeometry** pathGeometry)
{
	gmpi_sdk::mp_shared_ptr<api::IUnknown> b2;
	b2.Attach(new PathGeometry());

	return b2->queryInterface(GmpiDrawing_API::SE_IID_PATHGEOMETRY_MPGUI, reinterpret_cast<void**>(pathGeometry));
}

inline int32_t DrawingFactory::CreateImage(int32_t width, int32_t height, drawing::api::IBitmap** returnDiBitmap)
{
	*returnDiBitmap = nullptr;

	gmpi_sdk::mp_shared_ptr<api::IUnknown> bm;
	bm.Attach(new Bitmap(this, width, height));

	return bm->queryInterface(GmpiDrawing_API::SE_IID_BITMAP_MPGUI, (void**)returnDiBitmap);
}

inline int32_t DrawingFactory::LoadImageU(const char* utf8Uri, drawing::api::IBitmap** returnDiBitmap)
{
	*returnDiBitmap = nullptr;

	gmpi_sdk::mp_shared_ptr<api::IUnknown> bm;
	auto temp = new Bitmap(this, utf8Uri);
	bm.Attach(temp);

	if (temp->isLoaded())
	{
		return bm->queryInterface(GmpiDrawing_API::SE_IID_BITMAP_MPGUI, (void**)returnDiBitmap);
	}

	return MP_FAIL;
}

void BitmapBrush::FillPath(GraphicsContext* context, NSBezierPath* nsPath) const
{
	[NSGraphicsContext saveGraphicsState] ;

#if 0 // nope in Reaper
	// calc image offset considering that Quartz origin is at bottom of View
	const auto correction = context->getQuartzYorigin() - const_cast<Bitmap&>(bitmap_).GetSizeF().height;

	// we handle only translation on mac
	const auto offset = TransformPoint(brushProperties_.transform, {});

	CGContextRef ctx = (CGContextRef) [[NSGraphicsContext currentContext]graphicsPort];
	CGContextSetPatternPhase(ctx, CGSize{ offset.x, -offset.y + correction });
#endif
	auto view = context->getNativeView();

	// convert to Core Grapics co-ords
	CGFloat yOffset = NSMaxY([view convertRect : view.bounds toView : nil]);
	// CGFloat xOffset = -NSMinX([view convertRect:view.bounds toView:nil]);

	// apply brushes transfer. we support only translation on mac
	const auto offset = TransformPoint(brushProperties_.transform, { 0.0f, static_cast<float>(yOffset) });

	[[NSGraphicsContext currentContext]setPatternPhase:NSMakePoint(offset.x, offset.y)];
	[[NSColor colorWithPatternImage : bitmap_.nativeBitmap_]set];
	[nsPath fill] ;

	[NSGraphicsContext restoreGraphicsState] ;
}

inline bitmapPixels::bitmapPixels(Bitmap* sebitmap /*NSImage** inBitmap*/, bool _alphaPremultiplied, int32_t pflags) :
	inBitmap_(&sebitmap->nativeBitmap_ /*inBitmap*/)
	, flags(pflags)
	, seBitmap(sebitmap)
{
	NSSize s = [*inBitmap_ size];
	bytesPerRow = s.width * 4;

	constexpr int bitsPerSample = 8;
	constexpr int samplesPerPixel = 4;
	constexpr int bitsPerPixel = bitsPerSample * samplesPerPixel;

	auto initial_bitmap = [[NSBitmapImageRep alloc]initWithBitmapDataPlanes:nil
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

	bitmap2 = [initial_bitmap bitmapImageRepByRetaggingWithColorSpace : NSColorSpace.sRGBColorSpace];
	[bitmap2 retain] ;

	// Copy the image to the new imageRep (effectivly converts it to correct pixel format/brightness etc)
	if (0 != (flags & drawing::MP1_BITMAP_LOCK_READ))
	{
		NSGraphicsContext* context;
		context = [NSGraphicsContext graphicsContextWithBitmapImageRep : bitmap2];
		[NSGraphicsContext saveGraphicsState] ;
		[NSGraphicsContext setCurrentContext : context] ;
		[*inBitmap_ drawAtPoint : NSZeroPoint fromRect : NSZeroRect operation : NSCompositingOperationCopy fraction : 1.0] ;

		[NSGraphicsContext restoreGraphicsState] ;
	}
}

inline bitmapPixels::~bitmapPixels()
{
	if (0 != (flags & drawing::MP1_BITMAP_LOCK_WRITE))
	{
		// scan for overbright pixels
		bool hasOverbright = false;
		{
			drawing::MP1_SIZE_U imageSize{};
			seBitmap->GetSize(&imageSize);
			const int totalPixels = imageSize.height * getBytesPerRow() / sizeof(uint32_t);

			uint32_t* destPixels = (uint32_t*)getAddress();

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
		[bitmap2 release] ;
	}
}
} // namespace
} // namespace
