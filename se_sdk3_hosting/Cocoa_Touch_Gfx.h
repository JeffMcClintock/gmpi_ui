#pragma once

/*
#include "Cocoa_Gfx.h"
*/

#include <codecvt>
#include <map>
#include "../se_sdk3/Drawing.h"
#include "../shared/xp_simd.h"
#include "./Gfx_base.h"

namespace gmpi
{
	namespace cocoa_touch
	{
		// Conversion utilities.
        inline UIColor* toNative(const GmpiDrawing_API::MP1_COLOR& color)
        {
            // This should be equivalent to sRGB, except it allows extended color components beyond 0.0 - 1.0
            // for plain old sRGB, colorWithCalibratedRed would be next best.
            
            return [UIColor colorWithDisplayP3Red:
                    (CGFloat)se_sdk::FastGamma::pixelToNormalised(se_sdk::FastGamma::float_to_sRGB(color.r))
                    green : (CGFloat)se_sdk::FastGamma::pixelToNormalised(se_sdk::FastGamma::float_to_sRGB(color.g))
                    blue : (CGFloat)se_sdk::FastGamma::pixelToNormalised(se_sdk::FastGamma::float_to_sRGB(color.b))
                    alpha : (CGFloat)color.a];
            
            // nativec_ = [UIColor colorWithRed:color.r green:color.g blue:color.b alpha:color.a]; // wrong gamma
        }
    
        inline CGPoint toNative(GmpiDrawing_API::MP1_POINT p)
        {
            return CGPointMake(p.x, p.y);
        }
    
		inline GmpiDrawing::Rect RectFromNSRect(CGRect nsr)
		{
			GmpiDrawing::Rect r(nsr.origin.x, nsr.origin.y, nsr.origin.x + nsr.size.width, nsr.origin.y + nsr.size.height);
			return r;
		}

		inline CGRect NSRectFromRect(GmpiDrawing_API::MP1_RECT rect)
		{
			return CGRectMake(rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top);
		}
        
        // helper
        void SetNativePenStrokeStyle(UIBezierPath* path, GmpiDrawing_API::IMpStrokeStyle* strokeStyle)
        {
            GmpiDrawing_API::MP1_CAP_STYLE capstyle = strokeStyle == nullptr ? GmpiDrawing_API::MP1_CAP_STYLE_FLAT : strokeStyle->GetStartCap();
            
            switch(capstyle)
            {
                default:
                case GmpiDrawing_API::MP1_CAP_STYLE_FLAT:
                    [ path setLineCapStyle:kCGLineCapButt];
                    break;
                    
                case GmpiDrawing_API::MP1_CAP_STYLE_SQUARE:
                    [ path setLineCapStyle:kCGLineCapSquare ];
                    break;
                    
                case GmpiDrawing_API::MP1_CAP_STYLE_TRIANGLE:
                case GmpiDrawing_API::MP1_CAP_STYLE_ROUND:
                    [ path setLineCapStyle:kCGLineCapRound ];
                    break;
            }
        }

		// Classes without GetFactory()
		template<class MpInterface, class CocoaType>
		class CocoaWrapper : public MpInterface
		{
		protected:
			CocoaType* native_;

			virtual ~CocoaWrapper()
			{
				if (native_)
				{
//					[native_ release];
				}
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
			GmpiDrawing_API::IMpFactory* factory_;

		public:
			CocoaWrapperWithFactory(CocoaType* native, GmpiDrawing_API::IMpFactory* factory) : CocoaWrapper<MpInterface, CocoaType>(native), factory_(factory) {}

			virtual void MP_STDCALL GetFactory(GmpiDrawing_API::IMpFactory **factory) override
			{
				*factory = factory_;
			}
		};

		class nothing
		{

		};
/*
		class Brush : / * public GmpiDrawing_API::IMpBrush,* / public CocoaWrapperWithFactory<GmpiDrawing_API::IMpBrush, nothing> // Resource
		{
		public:
			Brush(GmpiDrawing_API::IMpFactory* factory) : CocoaWrapperWithFactory(nullptr, factory) {}
		};
*/
        
        class CocoaBrushBase
        {
		protected:
			GmpiDrawing_API::IMpFactory* factory_;

        public:
			CocoaBrushBase(GmpiDrawing_API::IMpFactory* pfactory) : factory_(pfactory){}
            virtual ~CocoaBrushBase(){}
            
			virtual void FillPath(UIBezierPath* nsPath) const = 0;

            // Default to black fill for fancy brushes that don't implement line drawing yet.
            virtual void StrokePath(UIBezierPath* nsPath, float strokeWidth, const GmpiDrawing_API::IMpStrokeStyle* strokeStyle = nullptr) const
            {
                [[UIColor blackColor] set]; /// !!!TODO!!!, color set to black always.
                
                [nsPath setLineWidth : strokeWidth];
                SetNativePenStrokeStyle(nsPath, (GmpiDrawing_API::IMpStrokeStyle*) strokeStyle);
                
                [nsPath stroke];
            }
        };

		class SolidColorBrush : public GmpiDrawing_API::IMpSolidColorBrush, public CocoaBrushBase
		{
			GmpiDrawing_API::MP1_COLOR color;
			UIColor* nativec_ = nullptr;
            
            inline void setNativeColor()
            {
                nativec_ = toNative(color);
            }
            
		public:
			SolidColorBrush(const GmpiDrawing_API::MP1_COLOR* pcolor, GmpiDrawing_API::IMpFactory *factory) : CocoaBrushBase(factory)
				,color(*pcolor)
			{
                setNativeColor();
			}

            inline UIColor* nativeColor() const
            {
                return nativec_;
            }

            void FillPath(UIBezierPath* nsPath) const override
            {
                [nativec_ setFill];
                [nsPath fill];
 /*
                UIBezierPath* rectanglePath = [UIBezierPath bezierPathWithRect: CGRectMake(68, 38, 81, 40)];
//                [UIColor.grayColor setFill];
                [rectanglePath fill];
  */
            }

			virtual void StrokePath(UIBezierPath* nsPath, float strokeWidth, const GmpiDrawing_API::IMpStrokeStyle* strokeStyle = nullptr) const override
			{
				[nativec_ set];
				[nsPath setLineWidth : strokeWidth];
                SetNativePenStrokeStyle(nsPath, (GmpiDrawing_API::IMpStrokeStyle*) strokeStyle);
                
                [nsPath stroke];
			}

			~SolidColorBrush()
			{
				// crash       [nativec_ release];
			}

			// IMPORTANT: Virtual functions much 100% match GmpiDrawing_API::IMpSolidColorBrush to simulate inheritance.
			virtual void MP_STDCALL SetColor(const GmpiDrawing_API::MP1_COLOR* pcolor) override
			{
				color = *pcolor;
                setNativeColor();
			}

			virtual GmpiDrawing_API::MP1_COLOR MP_STDCALL GetColor() override
			{
				return color;
			}

            virtual void MP_STDCALL GetFactory(GmpiDrawing_API::IMpFactory **factory) override
            {
                *factory = factory_;
            }
            
            GMPI_QUERYINTERFACE1(GmpiDrawing_API::SE_IID_SOLIDCOLORBRUSH_MPGUI, GmpiDrawing_API::IMpSolidColorBrush);
			GMPI_REFCOUNT;
		};

		class GradientStopCollection : public CocoaWrapperWithFactory<GmpiDrawing_API::IMpGradientStopCollection, nothing>
		{
		public:
			std::vector<GmpiDrawing_API::MP1_GRADIENT_STOP> gradientstops;

			GradientStopCollection(GmpiDrawing_API::IMpFactory* factory, const GmpiDrawing_API::MP1_GRADIENT_STOP* gradientStops, uint32_t gradientStopsCount) : CocoaWrapperWithFactory(nullptr, factory)
			{
				for (uint32_t i = 0; i < gradientStopsCount; ++i)
				{
					gradientstops.push_back(gradientStops[i]);
				}
			}
			GMPI_QUERYINTERFACE1(GmpiDrawing_API::SE_IID_GRADIENTSTOPCOLLECTION_MPGUI, GmpiDrawing_API::IMpGradientStopCollection);
		};

        class Gradient
        {
        protected:
            CGGradientRef native2 = {};
            
        public:
            Gradient(const GmpiDrawing_API::IMpGradientStopCollection* gradientStopCollection)
            {
                auto stops = static_cast<const GradientStopCollection*>(gradientStopCollection);

 //               NSMutableArray* colors = [NSMutableArray array];
                std::vector<CGFloat> components;
                std::vector<CGFloat> locations2;
                const auto count = stops->gradientstops.size();
                locations2.reserve(count);
                locations2.reserve(count * 4);
                
                // reversed, so radial gradient draws same as PC
                for (auto it = stops->gradientstops.rbegin() ; it != stops->gradientstops.rend() ; ++it)//  auto& stop : stops->gradientstops)
                {
                    const auto& stop = *it;
 //                   [colors addObject: toNative(stop.color)];
                    components.push_back(stop.color.r);
                    components.push_back(stop.color.g);
                    components.push_back(stop.color.b);
                    components.push_back(stop.color.a);
                    locations2.push_back(1.0 - stop.position);
                }
                
                CGColorSpaceRef colorSpace = CGColorSpaceCreateWithName(kCGColorSpaceGenericRGBLinear);

//                native2 = [[UIGradient alloc] initWithColors:colors atLocations: locations2.data() colorSpace: colorSpace];
                
                native2 = CGGradientCreateWithColorComponents( //CGGradientCreateWithColors(
                    colorSpace,
                    components.data(),
                    locations2.data(),
                    locations2.size()
                );
                
                CFRelease(colorSpace);
            }
            
            ~Gradient()
            {
                CFRelease(native2);
            }
        };
    
		class LinearGradientBrush : public GmpiDrawing_API::IMpLinearGradientBrush, public CocoaBrushBase, public Gradient
		{
            GmpiDrawing_API::MP1_LINEAR_GRADIENT_BRUSH_PROPERTIES brushProperties;
            
		public:
			LinearGradientBrush(
                GmpiDrawing_API::IMpFactory *factory,
                const GmpiDrawing_API::MP1_LINEAR_GRADIENT_BRUSH_PROPERTIES* linearGradientBrushProperties,
                const GmpiDrawing_API::MP1_BRUSH_PROPERTIES* brushProperties,
                const GmpiDrawing_API::IMpGradientStopCollection* gradientStopCollection) :
                    CocoaBrushBase(factory)
                    , Gradient(gradientStopCollection)
                    , brushProperties(*linearGradientBrushProperties)
			{
			}

			inline CGGradientRef native() const
			{
				return native2;
			}
/*
			float getAngle() const
			{
				// TODO cache. e.g. nan = not calculated yet.
				return (180.0f / M_PI) * atan2(brushProperties.endPoint.y - brushProperties.startPoint.y, brushProperties.endPoint.x - brushProperties.startPoint.x);
			}
*/
			// IMPORTANT: Virtual functions much 100% match simulated interface.
			virtual void MP_STDCALL SetStartPoint(GmpiDrawing_API::MP1_POINT pstartPoint) override
			{
                brushProperties.startPoint = pstartPoint;
			}
			virtual void MP_STDCALL SetEndPoint(GmpiDrawing_API::MP1_POINT pendPoint) override
			{
                brushProperties.endPoint = pendPoint;
			}

			virtual void FillPath(UIBezierPath* nsPath) const override
			{
//				[native2 drawInBezierPath:nsPath angle : getAngle()];
                
                // If you plan to do more drawing later, it's a good idea
                // to save the graphics state before clipping.
                UIGraphicsPushContext(UIGraphicsGetCurrentContext());
                
                // clip following output to the path
                [nsPath addClip];
                
//                [native2 drawFromPoint:toNative(brushProperties.endPoint) toPoint:toNative(brushProperties.startPoint) options:UIGradientDrawsBeforeStartingLocation|UIGradientDrawsAfterEndingLocation];
                
                CGContextDrawLinearGradient(UIGraphicsGetCurrentContext(), native2, toNative(brushProperties.endPoint), toNative(brushProperties.startPoint), 0);

                // restore clip region
                UIGraphicsPopContext();
			}
/* TODO (convert to outline, use fill functions).
			virtual void StrokePath(UIBezierPath* nsPath, float strokeWidth, const GmpiDrawing_API::IMpStrokeStyle* strokeStyle = nullptr) const override
			{
				[[UIColor blackColor] set]; /// !!!TODO!!!, color set to black always.

				[nsPath setLineWidth : strokeWidth];
				[nsPath stroke];
			}
*/
            
            virtual void MP_STDCALL GetFactory(GmpiDrawing_API::IMpFactory **factory) override
            {
                *factory = factory_;
            }

			GMPI_REFCOUNT;
			GMPI_QUERYINTERFACE1(GmpiDrawing_API::SE_IID_LINEARGRADIENTBRUSH_MPGUI, GmpiDrawing_API::IMpLinearGradientBrush);
		};

		class RadialGradientBrush : public GmpiDrawing_API::IMpRadialGradientBrush, public CocoaBrushBase, public Gradient
		{
            GmpiDrawing_API::MP1_RADIAL_GRADIENT_BRUSH_PROPERTIES gradientProperties;

		public:
			RadialGradientBrush(GmpiDrawing_API::IMpFactory *factory, const GmpiDrawing_API::MP1_RADIAL_GRADIENT_BRUSH_PROPERTIES* radialGradientBrushProperties, const GmpiDrawing_API::MP1_BRUSH_PROPERTIES* brushProperties, const  GmpiDrawing_API::IMpGradientStopCollection* gradientStopCollection) :
                CocoaBrushBase(factory)
                ,Gradient(gradientStopCollection)
                ,gradientProperties(*radialGradientBrushProperties)
			{
			}

			virtual void FillPath(UIBezierPath* nsPath) const override
			{
                const auto bounds = [nsPath bounds];
                
                const auto centerX = bounds.origin.x + 0.5 * std::max(0.1, bounds.size.width);
                const auto centerY = bounds.origin.y + 0.5 * std::max(0.1, bounds.size.height);
                
                auto relativeX = (gradientProperties.center.x - centerX) / (0.5 * bounds.size.width);
                auto relativeY = (gradientProperties.center.y - centerY) / (0.5 * bounds.size.height);
                
                relativeX = std::max(-1.0, std::min(1.0, relativeX));
                relativeY = std::max(-1.0, std::min(1.0, relativeY));
                
                const auto origin = CGPointMake(
                    gradientProperties.center.x + gradientProperties.gradientOriginOffset.x,
                    gradientProperties.center.y + gradientProperties.gradientOriginOffset.y);
                
                // If you plan to do more drawing later, it's a good idea
                // to save the graphics state before clipping.
                // UIGraphicsPushContext(UIGraphicsGetCurrentContext());
                
                UIGraphicsPushContext(UIGraphicsGetCurrentContext());
                
                // clip following output to the path
                [nsPath addClip];
/*
                [native2 drawFromCenter:toNative(gradientProperties.center)
                    radius:gradientProperties.radiusX
                    toCenter:origin
                    radius:0.0
                    options:UIGradientDrawsBeforeStartingLocation|UIGradientDrawsAfterEndingLocation];
 */
                CGContextDrawRadialGradient(UIGraphicsGetCurrentContext(), native2, toNative(gradientProperties.center), gradientProperties.radiusX, origin, 0.f, 0);
                
//                CGContextDrawRadialGradient (myContext, myGradient, toNative(brushProperties.endPoint),
  //                                       myStartRadius, toNative(brushProperties.startPoint), myEndRadius,
    //                                     kCGGradientDrawsAfterEndLocation);


                
                // restore clip region
                //UIGraphicsPopContext();;
                UIGraphicsPopContext();
			}

			virtual void MP_STDCALL SetCenter(GmpiDrawing_API::MP1_POINT pcenter) override
			{
                gradientProperties.center = pcenter;
			}

			virtual void MP_STDCALL SetGradientOriginOffset(GmpiDrawing_API::MP1_POINT gradientOriginOffset) override
			{
			}

			virtual void MP_STDCALL SetRadiusX(float radiusX) override
			{
                gradientProperties.radiusX = radiusX;
			}

			virtual void MP_STDCALL SetRadiusY(float radiusY) override
			{
                gradientProperties.radiusY = radiusY;
			}
            
            virtual void MP_STDCALL GetFactory(GmpiDrawing_API::IMpFactory **factory) override
            {
                *factory = factory_;
            }
            
			GMPI_REFCOUNT;
			GMPI_QUERYINTERFACE1(GmpiDrawing_API::SE_IID_RADIALGRADIENTBRUSH_MPGUI, GmpiDrawing_API::IMpRadialGradientBrush);
		};

		class TextFormat : public CocoaWrapper<GmpiDrawing_API::IMpTextFormat, const __CFDictionary>
		{
		public:
			std::string fontFamilyName;
			GmpiDrawing_API::MP1_FONT_WEIGHT fontWeight;
			GmpiDrawing_API::MP1_FONT_STYLE fontStyle;
			GmpiDrawing_API::MP1_FONT_STRETCH fontStretch;
			GmpiDrawing_API::MP1_TEXT_ALIGNMENT textAlignment;
			GmpiDrawing_API::MP1_PARAGRAPH_ALIGNMENT paragraphAlignment;
            GmpiDrawing_API::MP1_WORD_WRAPPING wordWrapping = GmpiDrawing_API::MP1_WORD_WRAPPING_WRAP;
			float fontSize;
			bool useLegacyBaseLineSnapping = true;
            
            // Cache some constants to make snap calculation faster.
            float topAdjustment = {}; // Mac includes extra space at top in font bounding box.
            float ascent = {};
            float baselineCorrection = {};

            NSMutableDictionary* native2 = {};
            
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

			TextFormat(std::wstring_convert<std::codecvt_utf8<wchar_t>>* pstringConverter, const char* pfontFamilyName, GmpiDrawing_API::MP1_FONT_WEIGHT pfontWeight, GmpiDrawing_API::MP1_FONT_STYLE pfontStyle, GmpiDrawing_API::MP1_FONT_STRETCH pfontStretch, float pfontSize) :
				CocoaWrapper<GmpiDrawing_API::IMpTextFormat, const __CFDictionary>(nullptr)
				, fontWeight(pfontWeight)
				, fontStyle(pfontStyle)
				, fontStretch(pfontStretch)
				, fontSize(pfontSize)
			{
				fontFamilyName = fontSubstitute(pfontFamilyName);
                
                uint32_t fontTraits = 0;
                if(pfontWeight >= GmpiDrawing_API::MP1_FONT_WEIGHT_DEMI_BOLD)
                    fontTraits |= UIFontDescriptorTraitBold;
                
                if(pfontStyle == GmpiDrawing_API::MP1_FONT_STYLE_ITALIC)
                    fontTraits |= UIFontDescriptorTraitItalic;
                
                static const CGFloat weightConversion[] = {
                    UIFontWeightUltraLight, // MP1_FONT_WEIGHT_THIN = 100
                    UIFontWeightThin,       // MP1_FONT_WEIGHT_ULTRA_LIGHT = 200
                    UIFontWeightLight,      // MP1_FONT_WEIGHT_LIGHT = 300
                    UIFontWeightRegular,    // MP1_FONT_WEIGHT_NORMAL = 400
                    UIFontWeightMedium,     // MP1_FONT_WEIGHT_MEDIUM = 500
                    UIFontWeightSemibold,   // MP1_FONT_WEIGHT_SEMI_BOLD = 600
                    UIFontWeightBold,       // MP1_FONT_WEIGHT_BOLD = 700
                    UIFontWeightHeavy,      // MP1_FONT_WEIGHT_BLACK = 900
                    UIFontWeightBlack       // MP1_FONT_WEIGHT_ULTRA_BLACK = 950
                };

                const int arrMax = static_cast<int>(sizeof(weightConversion)/sizeof(weightConversion[0])) - 1;
                const int roundNearest = 50;
                const int nativeFontWeightIndex
                    = std::max(0, std::min(arrMax, -1 + (pfontWeight + roundNearest) / 100));
                
                const auto nativeFontWeight = weightConversion[nativeFontWeightIndex];
                
//                UIFontManager* fontManager = [UIFontManager sharedFontManager];
//                UIFont* nativefont = [fontManager fontWithFamily:[NSString stringWithCString: fontFamilyName.c_str() encoding: NSUTF8StringEncoding ] traits:fontTraits weight:nativeFontWeight size:fontSize ];
                
                auto familyname = [NSString stringWithCString: fontFamilyName.c_str() encoding: NSUTF8StringEncoding ];
/*
                auto descriptor = [UIFontDescriptor preferredFontDescriptorWithTextStyle:@"what?"];
                descriptor = [UIFontDescriptor fontDescriptorWithName:familyname size:descriptor.pointSize];
                descriptor = [descriptor
                            fontDescriptorWithSymbolicTraits:fontTraits];
                auto nativefont = [UIFont fontWithDescriptor:descriptor size:fontSize];
   */
                
                BOOL isBold = pfontWeight >= GmpiDrawing_API::MP1_FONT_WEIGHT_DEMI_BOLD ? YES : NO;
                BOOL isItalic = pfontStyle == GmpiDrawing_API::MP1_FONT_STYLE_ITALIC ? YES: NO;
                UIFontDescriptor *fontDescriptor = [UIFontDescriptor fontDescriptorWithFontAttributes:
                                  @{
                                    @"NSFontFamilyAttribute" : familyname,
                                    @"NSFontFaceAttribute" : (isBold && isItalic ? @"Bold Italic" : (isBold ? @"Bold" : (isItalic ? @"Italic" : @"Regular")))
                                    }];
                auto nativefont = [UIFont fontWithDescriptor:fontDescriptor size:fontSize];
                
                // fallback to system font
                if(!nativefont)
                {
                    nativefont = [UIFont systemFontOfSize:fontSize weight:nativeFontWeight];
                }

				NSMutableParagraphStyle* style = [[NSMutableParagraphStyle alloc] init];
				[style setAlignment : NSTextAlignmentLeft];

				native2 = [[NSMutableDictionary alloc] initWithObjectsAndKeys:
				nativefont, NSFontAttributeName,
					style, NSParagraphStyleAttributeName,
					nil];
                
                CalculateTopAdjustment();
            }
            
            ~TextFormat()
            {
 //               [native2 release];
            }
            
            void CalculateTopAdjustment()
            {
                // Calculate compensation for different bounding box height between mac and direct2D.
                // On Direct2D boudning rect height is typicaly much less than Cocoa.
                // I don't know any algorithm for converting the extra height.
                // Fix is to disregard extra height on both platforms.
                GmpiDrawing_API::MP1_FONT_METRICS fontMetrics {};
                GetFontMetrics(&fontMetrics);
                    
                auto boundingBoxSize = [@"A" sizeWithAttributes : native2];
                
                topAdjustment = boundingBoxSize.height - (fontMetrics.ascent + fontMetrics.descent);
                ascent = fontMetrics.ascent;
                
                baselineCorrection = 0.5f;
                if((fontMetrics.descent - floor(fontMetrics.descent)) <= 0.5f)
                {
                    baselineCorrection += 0.5f;
                }
			}

			virtual int32_t MP_STDCALL SetTextAlignment(GmpiDrawing_API::MP1_TEXT_ALIGNMENT ptextAlignment) override
			{
				textAlignment = ptextAlignment;

				switch (textAlignment)
				{
				case (int)GmpiDrawing::TextAlignment::Leading: // Left.
					[native2[NSParagraphStyleAttributeName] setAlignment:NSTextAlignmentLeft];
					break;
				case (int)GmpiDrawing::TextAlignment::Trailing: // Right
					[native2[NSParagraphStyleAttributeName] setAlignment:NSTextAlignmentRight];
					break;
				case (int)GmpiDrawing::TextAlignment::Center:
					[native2[NSParagraphStyleAttributeName] setAlignment:NSTextAlignmentCenter];
					break;
				}

				return gmpi::MP_OK;
			}

			virtual int32_t MP_STDCALL SetParagraphAlignment(GmpiDrawing_API::MP1_PARAGRAPH_ALIGNMENT pparagraphAlignment) override
			{
				paragraphAlignment = pparagraphAlignment;
				return gmpi::MP_OK;
			}

			virtual int32_t MP_STDCALL SetWordWrapping(GmpiDrawing_API::MP1_WORD_WRAPPING pwordWrapping) override
			{
                wordWrapping = pwordWrapping;
				return gmpi::MP_OK;
			}

			virtual int32_t MP_STDCALL SetLineSpacing(float lineSpacing, float baseline) override
			{
				// Hack, reuse this method to enable legacy-mode.
				if (IMpTextFormat::ImprovedVerticalBaselineSnapping == lineSpacing)
				{
					useLegacyBaseLineSnapping = false;
					return gmpi::MP_OK;
				}

				return gmpi::MP_NOSUPPORT;
			}

			virtual int32_t MP_STDCALL GetFontMetrics(GmpiDrawing_API::MP1_FONT_METRICS* returnFontMetrics) override
			{
                UIFont* font = native2[NSFontAttributeName];  // Get font from dictionary.
                returnFontMetrics->xHeight = [font xHeight];
				returnFontMetrics->ascent = [font ascender];
				returnFontMetrics->descent = -[font descender]; // Descent is negative on OSX (positive on Windows)
				returnFontMetrics->lineGap = [font leading];
				returnFontMetrics->capHeight = [font capHeight];
                returnFontMetrics->underlinePosition = 0.0f;//[font underlinePosition];
                returnFontMetrics->underlineThickness = 1.0f;//[font underlineThickness];
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
				return gmpi::MP_OK;
			}

			// TODO!!!: Probly needs to accept constraint rect like DirectWrite. !!!
			virtual void MP_STDCALL GetTextExtentU(const char* utf8String, int32_t stringLength, GmpiDrawing_API::MP1_SIZE* returnSize) override
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
			
			bool getUseLegacyBaseLineSnapping() const
			{
				return useLegacyBaseLineSnapping;
			}

			GMPI_QUERYINTERFACE1(GmpiDrawing_API::SE_IID_TEXTFORMAT_MPGUI, GmpiDrawing_API::IMpTextFormat);
			GMPI_REFCOUNT;
		};


		class bitmapPixels : public GmpiDrawing_API::IMpBitmapPixels
		{
			//	bool alphaPremultiplied;
			int bytesPerRow;
			std::vector<int32_t> pixels;
//TODO            NSImageRep* bitmapPixels_;
            UIImage* inBitmap_;
//TODO			NSBitmapImageRep * bitmap2;
			int32_t flags;
            
        public:
            bitmapPixels(UIImage* inBitmap, bool _alphaPremultiplied, int32_t pflags) :
				flags(pflags)
			{
                /* TODO
                inBitmap_ = inBitmap;
				bitmapPixels_ = nil; // [[inBitmap representations] objectAtIndex:0];

				{
					// Get largest bitmap representation. (most detail).
					for (UIImageRep* imagerep in[inBitmap_ representations])
					{
						if ([imagerep isKindOfClass : [NSBitmapImageRep class]])
						{
							if (bitmapPixels_ == nil) {
								bitmapPixels_ = imagerep;
							}
							if ([bitmapPixels_ pixelsHigh]<[imagerep pixelsHigh]) {
								bitmapPixels_ = imagerep;
							}
						}
					}
				}

				NSSize s = [inBitmap size];
				bytesPerRow = s.width * 4;

				pixels.assign(s.width * s.height, 0);
				unsigned char* pixelDest = (unsigned char*)pixels.data();
                
				bitmap2 = [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:&pixelDest
					pixelsWide : s.width
					pixelsHigh : s.height
					bitsPerSample : 8
					samplesPerPixel : 4
					hasAlpha : YES
					isPlanar : NO
                           // The “Device” color-space names represent color spaces in which component values are applied to devices as specified. There is no optimization or adjustment for differences between devices in how they render colors.
					colorSpaceName : NSDeviceRGBColorSpace // NSCalibratedRGBColorSpace
                    bitmapFormat : 0
					bytesPerRow : bytesPerRow
					bitsPerPixel : 32];

				NSGraphicsContext * context;
				context = [NSGraphicsContext graphicsContextWithBitmapImageRep : bitmap2];
				UIGraphicsPushContext(UIGraphicsGetCurrentContext());
				[NSGraphicsContext setCurrentContext : context];
//				[inBitmap drawInRect : CGRectMake(0, 0, s.width, s.height)];
                
                // Mac stored images "upside down" compared to Windows.
                / *
                NSAffineTransform *t = [NSAffineTransform transform];
                [t translateXBy:0 yBy:s.height];
                [t scaleXBy:1 yBy:-1];
                [t concat];
* /
                [inBitmap drawAtPoint: NSZeroPoint fromRect: NSZeroRect operation: NSCompositeCopy fraction: 1.0];
				UIGraphicsPopContext();
                
                */
			}

			~bitmapPixels()
			{
                /* TODO
                // TODO, don't work. seems to blur it, really need to ber able to lock/discard (for hit testing) without wasting all this effort.
				if (0 != (flags & GmpiDrawing_API::MP1_BITMAP_LOCK_WRITE))
				{
                    UIImage *producedImage = [[UIImage alloc] init];
                    [producedImage addRepresentation:bitmap2];
//                    [bitmapPixels_ release];
                    
                    [inBitmap_ lockFocus];
                    //    [bitmap2 drawInRect: CGRectMake(0,0,producedImage.size.width, producedImage.size.height)];
                    // fromRect:NSZeroRect => entire image is drawn.
                    [producedImage drawAtPoint: NSZeroPoint fromRect: NSZeroRect operation: NSCompositeCopy fraction: 1.0];
                    
                    [inBitmap_ unlockFocus];
                    
                    [bitmap2 release];
				}
                 */
			}

			virtual uint8_t* MP_STDCALL getAddress() const override { return (uint8_t*)pixels.data(); };
			virtual int32_t MP_STDCALL getBytesPerRow() const override { return bytesPerRow; };
			virtual int32_t MP_STDCALL getPixelFormat() const override { return kRGBA; };

			inline uint8_t fast8bitScale(uint8_t a, uint8_t b)
			{
				int t = (int)a * (int)b;
				return (uint8_t)((t + 1 + (t >> 8)) >> 8); // fast way to divide by 255
			}

			void premultiplyAlpha()
			{
				for (auto& p : pixels)
				{
					auto pixel = reinterpret_cast<uint8_t*>(&p);
					if (pixel[3] == 0)
					{
						pixel[0] = 0;
						pixel[1] = 0;
						pixel[2] = 0;
					}
					else
					{
						pixel[0] = fast8bitScale(pixel[0], pixel[3]);
						pixel[1] = fast8bitScale(pixel[1], pixel[3]);
						pixel[2] = fast8bitScale(pixel[2], pixel[3]);
					}
				}
			}

			//-----------------------------------------------------------------------------
			void unpremultiplyAlpha()
			{
				for (auto& p : pixels)
				{
					auto pixel = reinterpret_cast<uint8_t*>(&p);
					if (pixel[3] == 0)
					{
						pixel[0] = (uint32_t)(pixel[0] * 255) / pixel[3];
						pixel[1] = (uint32_t)(pixel[1] * 255) / pixel[3];
						pixel[2] = (uint32_t)(pixel[2] * 255) / pixel[3];
					}
				}
			}

			GMPI_QUERYINTERFACE1(GmpiDrawing_API::SE_IID_BITMAP_PIXELS_MPGUI, GmpiDrawing_API::IMpBitmapPixels);
			GMPI_REFCOUNT;
		};

		class Bitmap : public GmpiDrawing_API::IMpBitmap
		{
			GmpiDrawing_API::IMpFactory* factory = nullptr;

		public:
			UIImage* nativeBitmap_;

			Bitmap(GmpiDrawing_API::IMpFactory* pfactory, const char* utf8Uri) :
				nativeBitmap_(nullptr)
				, factory(pfactory)
			{
#if 1
                /* TODO
				NSString * url = [NSString stringWithCString : utf8Uri encoding : NSUTF8StringEncoding];
				nativeBitmap_ = [[UIImage alloc] initWithContentsOfFile:url];
//				[nativeBitmap_ setFlipped : TRUE]; // cache bitmap up correct way (else OSX flips it).
                
                // undo scaling of image (which reports scaled size, screwing up animation frame).
                NSSize max = NSZeroSize;
                for (NSObject* o in nativeBitmap_.representations) {
                    if ([o isKindOfClass: NSImageRep.class]) {
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
                */
#else
                // On windows bitmaps are stored the other way up.
                // For direct memory access functions to work correctly, we
                // have to flip the image unnaturally (for mac). We also have to draw it upside down later to compensate.
                NSString * url = [NSString stringWithCString : utf8Uri encoding : NSUTF8StringEncoding];
                auto inputImage = [[UIImage alloc] initWithContentsOfFile:url];

                NSSize dimensions = [inputImage size];
                
                nativeBitmap_ = [[UIImage alloc] initWithSize:dimensions];
                [nativeBitmap_ lockFocus];
                
                NSAffineTransform *t = [NSAffineTransform transform];
                [t translateXBy:dimensions.width yBy:dimensions.height];
                [t scaleXBy:-1 yBy:-1];
                [t concat];
                
                [inputImage drawAtPoint:CGPointMake(0,0)
                               fromRect:CGRectMake(0, 0, dimensions.width, dimensions.height)
                              operation:NSCompositeCopy fraction:1.0];
                [nativeBitmap_ unlockFocus];
               
#endif
			}
            
            Bitmap(GmpiDrawing_API::IMpFactory* pfactory, int32_t width, int32_t height)
				: factory(pfactory)
            {
//TODO                nativeBitmap_ = [[UIImage alloc] initWithSize:NSMakeSize((CGFloat)width, (CGFloat)height)];
                //TODO                 [nativeBitmap_ retain];
            }
            
			Bitmap(GmpiDrawing_API::IMpFactory* pfactory, UIImage* native) : nativeBitmap_(native)
				, factory(pfactory)
			{
                //TODO 				[nativeBitmap_ retain];
			}

			bool isLoaded()
			{
				return nativeBitmap_ != nil;
			}

			virtual ~Bitmap()
			{
                //TODO 				[nativeBitmap_ release];
			}

			inline UIImage* GetNativeBitmap()
			{
				return nativeBitmap_;
			}

			virtual GmpiDrawing_API::MP1_SIZE MP_STDCALL GetSizeF() override
			{
				auto s = [nativeBitmap_ size];
				return GmpiDrawing::Size(s.width, s.height);
			}

			virtual int32_t MP_STDCALL GetSize(GmpiDrawing_API::MP1_SIZE_U* returnSize) override
			{
                auto s = [nativeBitmap_ size];

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
				return gmpi::MP_OK;
			}

			virtual int32_t MP_STDCALL lockPixelsOld(GmpiDrawing_API::IMpBitmapPixels** returnInterface, bool alphaPremultiplied) override
			{
				*returnInterface = 0;

				gmpi_sdk::mp_shared_ptr<gmpi::IMpUnknown> b2;
				b2.Attach(new bitmapPixels(nativeBitmap_, alphaPremultiplied, GmpiDrawing_API::MP1_BITMAP_LOCK_READ | GmpiDrawing_API::MP1_BITMAP_LOCK_WRITE));

				return b2->queryInterface(GmpiDrawing_API::SE_IID_BITMAP_PIXELS_MPGUI, (void**)(returnInterface));
			}

			virtual int32_t MP_STDCALL lockPixels(GmpiDrawing_API::IMpBitmapPixels** returnInterface, int32_t flags) override
			{
				*returnInterface = 0;

				gmpi_sdk::mp_shared_ptr<gmpi::IMpUnknown> b2;
				b2.Attach(new bitmapPixels(nativeBitmap_, true, flags));

				return b2->queryInterface(GmpiDrawing_API::SE_IID_BITMAP_PIXELS_MPGUI, (void**)(returnInterface));
			}

			virtual void MP_STDCALL ApplyAlphaCorrection() override {};

			void ApplyAlphaCorrection2() {};

			virtual void MP_STDCALL GetFactory(GmpiDrawing_API::IMpFactory** rfactory) override
			{
				*rfactory = factory;
			}

			GMPI_QUERYINTERFACE1(GmpiDrawing_API::SE_IID_BITMAP_MPGUI, GmpiDrawing_API::IMpBitmap);
			GMPI_REFCOUNT;
		};

        class BitmapBrush : public GmpiDrawing_API::IMpBitmapBrush, public CocoaBrushBase
        {
            Bitmap bitmap_;
        public:
            BitmapBrush(
                GmpiDrawing_API::IMpFactory* factory,
                const GmpiDrawing_API::IMpBitmap* bitmap,
                const GmpiDrawing_API::MP1_BITMAP_BRUSH_PROPERTIES* bitmapBrushProperties,
                const GmpiDrawing_API::MP1_BRUSH_PROPERTIES* brushProperties
            )
             : CocoaBrushBase(factory),
            bitmap_(factory, ((Bitmap*)bitmap)->nativeBitmap_)
            {
            }
            
            void StrokePath(UIBezierPath* nsPath, float strokeWidth, const GmpiDrawing_API::IMpStrokeStyle* strokeStyle = nullptr) const override
            {
                [[UIColor colorWithPatternImage:bitmap_.nativeBitmap_] set];
                
                [nsPath setLineWidth : strokeWidth];
                SetNativePenStrokeStyle(nsPath, (GmpiDrawing_API::IMpStrokeStyle*) strokeStyle);
                
                [nsPath stroke];
            }
            
            void FillPath(UIBezierPath* nsPath) const override
            {
                [[UIColor colorWithPatternImage:bitmap_.nativeBitmap_] set];
                [nsPath fill];
            }

            void MP_STDCALL SetExtendModeX(GmpiDrawing_API::MP1_EXTEND_MODE extendModeX) override
            {
//                native()->SetExtendModeX((D2D1_EXTEND_MODE)extendModeX);
            }

            void MP_STDCALL SetExtendModeY(GmpiDrawing_API::MP1_EXTEND_MODE extendModeY) override
            {
 //               native()->SetExtendModeY((D2D1_EXTEND_MODE)extendModeY);
            }

            void MP_STDCALL SetInterpolationMode(GmpiDrawing_API::MP1_BITMAP_INTERPOLATION_MODE interpolationMode) override
            {
  //              native()->SetInterpolationMode((D2D1_BITMAP_INTERPOLATION_MODE)interpolationMode);
            }

            void MP_STDCALL GetFactory(GmpiDrawing_API::IMpFactory **factory) override
            {
                *factory = factory_;
            }
            
            GMPI_REFCOUNT;
            GMPI_QUERYINTERFACE1(GmpiDrawing_API::SE_IID_BITMAPBRUSH_MPGUI, GmpiDrawing_API::IMpBitmapBrush);
        };

		class GeometrySink : public gmpi::generic_graphics::GeometrySink
		{
			UIBezierPath* geometry_;

		public:
			GeometrySink(UIBezierPath* geometry) : geometry_(geometry) {}

			virtual void MP_STDCALL SetFillMode(GmpiDrawing_API::MP1_FILL_MODE fillMode)
			{
				switch (fillMode)
				{
				case GmpiDrawing_API::MP1_FILL_MODE_ALTERNATE:
//					[geometry_ setWindingRule : NSEvenOddWindingRule];
					break;

				case GmpiDrawing_API::MP1_FILL_MODE_WINDING:
//					[geometry_ setWindingRule : NSNonZeroWindingRule];
					break;
				}
			}
			virtual void MP_STDCALL SetSegmentFlags(GmpiDrawing_API::MP1_PATH_SEGMENT vertexFlags)
			{
				//		geometrysink_->SetSegmentFlags((D2D1_PATH_SEGMENT)vertexFlags);
			}
			virtual void MP_STDCALL BeginFigure(GmpiDrawing_API::MP1_POINT startPoint, GmpiDrawing_API::MP1_FIGURE_BEGIN figureBegin) override
			{
				[geometry_ moveToPoint : CGPointMake(startPoint.x, startPoint.y)];

				lastPoint = startPoint;
			}

			virtual void MP_STDCALL EndFigure(GmpiDrawing_API::MP1_FIGURE_END figureEnd) override
			{
				if (figureEnd == GmpiDrawing_API::MP1_FIGURE_END_CLOSED)
				{
					[geometry_ closePath];
				}
			}

			virtual void MP_STDCALL AddLine(GmpiDrawing_API::MP1_POINT point) override
			{
				[geometry_ addLineToPoint : CGPointMake(point.x, point.y)];

				lastPoint = point;
			}

			virtual void MP_STDCALL AddBezier(const GmpiDrawing_API::MP1_BEZIER_SEGMENT* bezier) override
			{
				[geometry_ addCurveToPoint : CGPointMake(bezier->point3.x, bezier->point3.y)
					controlPoint1 : CGPointMake(bezier->point1.x, bezier->point1.y)
					controlPoint2 : CGPointMake(bezier->point2.x, bezier->point2.y)];

				lastPoint = bezier->point3;
			}

			GMPI_QUERYINTERFACE1(GmpiDrawing_API::SE_IID_GEOMETRYSINK_MPGUI, GmpiDrawing_API::IMpGeometrySink);
			GMPI_REFCOUNT;
		};


		class PathGeometry final : public GmpiDrawing_API::IMpPathGeometry
		{
			UIBezierPath* geometry_;

		public:
			PathGeometry()
			{
				geometry_ = [UIBezierPath bezierPath];
                //TODO               [geometry_ retain]; // TODO: Why only UIBezierPath needs manual retain??? !!!
			}

			~PathGeometry()
			{
				//	auto release pool handles it?
                //TODO                 [geometry_ release];
			}

			inline UIBezierPath* native()
			{
				return geometry_;
			}

			virtual int32_t MP_STDCALL Open(GmpiDrawing_API::IMpGeometrySink** geometrySink) override
			{
				gmpi_sdk::mp_shared_ptr<gmpi::IMpUnknown> b2;
				b2.Attach(new GeometrySink(geometry_));

				return b2->queryInterface(GmpiDrawing_API::SE_IID_GEOMETRYSINK_MPGUI, reinterpret_cast<void**>(geometrySink));
			}

			virtual void MP_STDCALL GetFactory(GmpiDrawing_API::IMpFactory** factory) override
			{
				//		native_->GetFactory((ID2D1Factory**)factory);
			}
            
            CGPathRef getCGPath() // could be cached.
            {
                return [geometry_ CGPath];
/*
                CGMutablePathRef cgPath = CGPathCreateMutable();
                NSInteger n = [geometry_ elementCount];
                
                for (NSInteger i = 0; i < n; i++) {
                    CGPoint ps[3];
                    switch ([geometry_ elementAtIndex:i associatedPoints:ps]) {
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
                            assert(false && "Invalid UIBezierPathElement");
                    }
                }
                return cgPath;
 */
            }

			virtual int32_t MP_STDCALL StrokeContainsPoint(GmpiDrawing_API::MP1_POINT point, float strokeWidth, GmpiDrawing_API::IMpStrokeStyle* strokeStyle, const GmpiDrawing_API::MP1_MATRIX_3X2* worldTransform, bool* returnContains) override
			{
                auto cgPath2 = getCGPath();
                
                CGPathRef hitTargetPath = CGPathCreateCopyByStrokingPath(cgPath2, NULL, (CGFloat) strokeWidth, (CGLineCap) [geometry_ lineCapStyle], (CGLineJoin)[geometry_ lineJoinStyle], [geometry_ miterLimit] );
                
                CGPathRelease(cgPath2);
                
                CGPoint cgpoint = CGPointMake(point.x, point.y);
                *returnContains = (bool) CGPathContainsPoint(hitTargetPath, NULL, cgpoint, false);//(bool)[geometry_ windingRule]);
                
                CGPathRelease(hitTargetPath);
				return gmpi::MP_OK;
			}
            
			virtual int32_t MP_STDCALL FillContainsPoint(GmpiDrawing_API::MP1_POINT point, const GmpiDrawing_API::MP1_MATRIX_3X2* worldTransform, bool* returnContains) override
			{
                *returnContains = [geometry_ containsPoint:CGPointMake(point.x, point.y)];
				return gmpi::MP_OK;
			}
            
			virtual int32_t MP_STDCALL GetWidenedBounds(float strokeWidth, GmpiDrawing_API::IMpStrokeStyle* strokeStyle, const GmpiDrawing_API::MP1_MATRIX_3X2* worldTransform, GmpiDrawing_API::MP1_RECT* returnBounds) override
			{
                const float radius = ceilf(strokeWidth * 0.5f);
                auto nativeRect = [geometry_ bounds];
                returnBounds->left = nativeRect.origin.x - radius;
                returnBounds->top = nativeRect.origin.y - radius;
                returnBounds->right = nativeRect.origin.x + nativeRect.size.width + radius;
                returnBounds->bottom = nativeRect.origin.y + nativeRect.size.height + radius;

				return gmpi::MP_OK;
			}
			GMPI_QUERYINTERFACE1(GmpiDrawing_API::SE_IID_PATHGEOMETRY_MPGUI, GmpiDrawing_API::IMpPathGeometry);
			GMPI_REFCOUNT;
		};

		class DrawingFactory : public GmpiDrawing_API::IMpFactory2
		{
			std::vector<std::string> supportedFontFamilies;

		public:
			std::wstring_convert<std::codecvt_utf8<wchar_t>> stringConverter; // cached, as constructor is super-slow.

			DrawingFactory()
			{
//				UIFontManager* fontManager = [UIFontManager sharedFontManager];
/*
                auto fontNames = [fontManager availableFonts]; // tends to add "-bold" or "-italic" on end of name
                for( NSString* fontName in fontNames)
                {
                    supportedFontFamilies.push_back([fontName UTF8String]);
                }

                //for IOS see [UIFont familynames]
                auto fontFamilies = [fontManager availableFontFamilies];
                for( NSString* familyName in fontFamilies)
                {
                    supportedFontFamilies.push_back([familyName UTF8String]);
                }
 */
                auto fontFamilyNames = [UIFont familyNames];
                for (auto familyName in fontFamilyNames) {
 //                       NSLog(@"Font Family Name = %@", familyName);
 //                       NSArray *names = [UIFont fontNamesForFamilyName:familyName];
 //                       NSLog(@"Font Names = %@", names);
                    supportedFontFamilies.push_back([familyName UTF8String]);
                }
			}

			virtual int32_t MP_STDCALL CreatePathGeometry(GmpiDrawing_API::IMpPathGeometry** pathGeometry) override
			{
				gmpi_sdk::mp_shared_ptr<gmpi::IMpUnknown> b2;
				b2.Attach(new PathGeometry());

				return b2->queryInterface(GmpiDrawing_API::SE_IID_PATHGEOMETRY_MPGUI, reinterpret_cast<void **>(pathGeometry));
			}

			virtual int32_t MP_STDCALL CreateTextFormat(const char* fontFamilyName, void* unused /* fontCollection */, GmpiDrawing_API::MP1_FONT_WEIGHT fontWeight, GmpiDrawing_API::MP1_FONT_STYLE fontStyle, GmpiDrawing_API::MP1_FONT_STRETCH fontStretch, float fontSize, void* unused2 /* localeName */, GmpiDrawing_API::IMpTextFormat** textFormat) override
			{
				gmpi_sdk::mp_shared_ptr<gmpi::IMpUnknown> b2;
				b2.Attach(new TextFormat(&stringConverter, fontFamilyName, fontWeight, fontStyle, fontStretch, fontSize));

				return b2->queryInterface(GmpiDrawing_API::SE_IID_TEXTFORMAT_MPGUI, reinterpret_cast<void **>(textFormat));
			}

			virtual int32_t MP_STDCALL CreateImage(int32_t width, int32_t height, GmpiDrawing_API::IMpBitmap** returnDiBitmap) override
			{
                *returnDiBitmap = nullptr;
                
                auto bm = new Bitmap(this, width, height);
                return bm->queryInterface(GmpiDrawing_API::SE_IID_BITMAP_MPGUI, (void**)returnDiBitmap);
			}

			virtual int32_t MP_STDCALL LoadImageU(const char* utf8Uri, GmpiDrawing_API::IMpBitmap** returnDiBitmap) override
			{
				*returnDiBitmap = nullptr;

				auto bm = new Bitmap(this, utf8Uri);
				if (bm->isLoaded())
				{
					bm->queryInterface(GmpiDrawing_API::SE_IID_BITMAP_MPGUI, (void**)returnDiBitmap);
					return gmpi::MP_OK;
				}

				delete bm;
				return gmpi::MP_FAIL;
			}

			virtual int32_t CreateStrokeStyle(const GmpiDrawing_API::MP1_STROKE_STYLE_PROPERTIES* strokeStyleProperties, float* dashes, int32_t dashesCount, GmpiDrawing_API::IMpStrokeStyle** returnValue) override
			{
				*returnValue = nullptr;

				gmpi_sdk::mp_shared_ptr<gmpi::IMpUnknown> b2;
				b2.Attach(new gmpi::generic_graphics::StrokeStyle(this, strokeStyleProperties, dashes, dashesCount));

				return b2->queryInterface(GmpiDrawing_API::SE_IID_STROKESTYLE_MPGUI, reinterpret_cast<void **>(returnValue));
			}

			// IMpFactory2
			int32_t MP_STDCALL GetFontFamilyName(int32_t fontIndex, gmpi::IString* returnString) override
			{
				if (fontIndex < 0 || fontIndex >= supportedFontFamilies.size())
				{
					return gmpi::MP_FAIL;
				}

				returnString->setData(supportedFontFamilies[fontIndex].data(), static_cast<int32_t>(supportedFontFamilies[fontIndex].size()));
				return gmpi::MP_OK;
			}

			int32_t MP_STDCALL queryInterface(const gmpi::MpGuid& iid, void** returnInterface) override
			{
				*returnInterface = 0;
				if ( iid == GmpiDrawing_API::SE_IID_FACTORY2_MPGUI || iid == GmpiDrawing_API::SE_IID_FACTORY_MPGUI || iid == gmpi::MP_IID_UNKNOWN)
				{
					*returnInterface = reinterpret_cast<GmpiDrawing_API::IMpFactory2*>(this);
					addRef();
					return gmpi::MP_OK;
				}
				return gmpi::MP_NOSUPPORT;
			}

			GMPI_REFCOUNT_NO_DELETE;
		};


		class GraphicsContext : public GmpiDrawing_API::IMpDeviceContext
		{
		protected:
			std::wstring_convert<std::codecvt_utf8<wchar_t>>* stringConverter; // cached, as constructor is super-slow.
			GmpiDrawing_API::IMpFactory* factory;
			std::vector<GmpiDrawing_API::MP1_RECT> clipRectStack;
			CGAffineTransform currentTransform;
			UIView* view_;
            
		public:

			GraphicsContext(UIView* pview, GmpiDrawing_API::IMpFactory* pfactory) :
				factory(pfactory)
				, view_(pview)
			{
                currentTransform = CGAffineTransformMakeTranslation(0.0f, 0.0f);// transform];

				/*
				context_->AddRef();
				stringConverter = &(dynamic_cast<DX_Factory*>(pfactory)->stringConverter);
				 */
			}

			~GraphicsContext()
			{
				//?        [currentTransform release];
			}

			virtual void MP_STDCALL GetFactory(GmpiDrawing_API::IMpFactory** pfactory) override
			{
				*pfactory = factory;
			}

			virtual void MP_STDCALL DrawRectangle(const GmpiDrawing_API::MP1_RECT* rect, const GmpiDrawing_API::IMpBrush* brush, float strokeWidth, const GmpiDrawing_API::IMpStrokeStyle* strokeStyle) override
			{
                /*
				auto scb = dynamic_cast<const SolidColorBrush*>(brush);
				if (scb)
				{
					[scb->nativeColor() set];
				}
				UIBezierPath *bp = [UIBezierPath bezierPathWithRect : gmpi::cocoa::NSRectFromRect(*rect)];
				[bp stroke];
                */
                
                UIBezierPath* path = [UIBezierPath bezierPathWithRect : gmpi::cocoa_touch::NSRectFromRect(*rect)];
                auto cocoabrush = dynamic_cast<const CocoaBrushBase*>(brush);
                if (cocoabrush)
                {
                    cocoabrush->StrokePath(path, strokeWidth, strokeStyle);
                }
			}

			virtual void MP_STDCALL FillRectangle(const GmpiDrawing_API::MP1_RECT* rect, const GmpiDrawing_API::IMpBrush* brush) override
			{
				UIBezierPath* rectPath = [UIBezierPath bezierPathWithRect : gmpi::cocoa_touch::NSRectFromRect(*rect)];

				auto cocoabrush = dynamic_cast<const CocoaBrushBase*>(brush);
				if (cocoabrush)
				{
					cocoabrush->FillPath(rectPath);
				}
			}

			virtual void MP_STDCALL Clear(const GmpiDrawing_API::MP1_COLOR* clearColor) override
			{
                SolidColorBrush brush(clearColor, factory);
                GmpiDrawing::Rect r;
                GetAxisAlignedClip(&r);
                UIBezierPath* rectPath = [UIBezierPath bezierPathWithRect : NSRectFromRect(r)];
                brush.FillPath(rectPath);
			}

			virtual void MP_STDCALL DrawLine(GmpiDrawing_API::MP1_POINT point0, GmpiDrawing_API::MP1_POINT point1, const GmpiDrawing_API::IMpBrush* brush, float strokeWidth, const GmpiDrawing_API::IMpStrokeStyle* strokeStyle) override
			{
                /*
				auto scb = dynamic_cast<const SolidColorBrush*>(brush);
				if (scb)
				{
					[scb->nativeColor() set];
				}

				UIBezierPath* line = [UIBezierPath bezierPath];
				[line moveToPoint : CGPointMake(point0.x, point0.y)];
				[line lineToPoint : CGPointMake(point1.x, point1.y)];
				[line setLineWidth : strokeWidth];
                
                SetNativePenStrokeStyle(line, (GmpiDrawing_API::IMpStrokeStyle*) strokeStyle);
                
				[line stroke];
                */
                
                UIBezierPath* path = [UIBezierPath bezierPath];
                [path moveToPoint : CGPointMake(point0.x, point0.y)];
                [path addLineToPoint : CGPointMake(point1.x, point1.y)];

                auto cocoabrush = dynamic_cast<const CocoaBrushBase*>(brush);
                if (cocoabrush)
                {
                    cocoabrush->StrokePath(path, strokeWidth, strokeStyle);
                }
			}

			virtual void MP_STDCALL DrawGeometry(const GmpiDrawing_API::IMpPathGeometry* geometry, const GmpiDrawing_API::IMpBrush* brush, float strokeWidth = 1.0f, const GmpiDrawing_API::IMpStrokeStyle* strokeStyle = 0) override
			{
				auto nsPath = ((PathGeometry*)geometry)->native();

				auto cocoabrush = dynamic_cast<const CocoaBrushBase*>(brush);
				if (cocoabrush)
				{
					cocoabrush->StrokePath(nsPath, strokeWidth, strokeStyle);
				}
			}

			virtual void MP_STDCALL FillGeometry(const GmpiDrawing_API::IMpPathGeometry* geometry, const GmpiDrawing_API::IMpBrush* brush, const GmpiDrawing_API::IMpBrush* opacityBrush) override
			{
				auto nsPath = ((PathGeometry*)geometry)->native();

				auto cocoabrush = dynamic_cast<const CocoaBrushBase*>(brush);
				if (cocoabrush)
				{
					cocoabrush->FillPath(nsPath);
				}
			}
            
            inline float truncatePixel(float y, float stepSize)
            {
                return floor(y / stepSize) * stepSize;
            }
            
            inline float roundPixel(float y, float stepSize)
            {
                return floor(y / stepSize + 0.5f) * stepSize;
            }
            
			virtual void MP_STDCALL DrawTextU(const char* utf8String, int32_t stringLength, const GmpiDrawing_API::IMpTextFormat* textFormat, const GmpiDrawing_API::MP1_RECT* layoutRect, const GmpiDrawing_API::IMpBrush* brush, int32_t flags) override
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
                GmpiDrawing::Size textSize {};
                if (textformat->paragraphAlignment != (int)GmpiDrawing::TextAlignment::Leading
                    || flags != GmpiDrawing_API::MP1_DRAW_TEXT_OPTIONS_CLIP)
                {
                    const_cast<GmpiDrawing_API::IMpTextFormat*>(textFormat)->GetTextExtentU(utf8String, (int32_t)strlen(utf8String), &textSize);
                }
                
				if (textformat->paragraphAlignment != (int)GmpiDrawing::TextAlignment::Leading)
				{
					// Vertical text alignment.
					switch (textformat->paragraphAlignment)
					{
					case (int)GmpiDrawing::TextAlignment::Trailing:    // Bottom
						bounds.origin.y += bounds.size.height - textSize.height;
						bounds.size.height = textSize.height;
						break;

					case (int)GmpiDrawing::TextAlignment::Center:
						bounds.origin.y += (bounds.size.height - textSize.height) / 2;
						bounds.size.height = textSize.height;
						break;

					default:
						break;
					}
				}

				NSString* str = [NSString stringWithCString : utf8String encoding : NSUTF8StringEncoding];
                
                [textformat->native2 setObject:scb->nativeColor() forKey:NSForegroundColorAttributeName];

                const bool clipToRect = flags & GmpiDrawing_API::MP1_DRAW_TEXT_OPTIONS_CLIP;

                if (textformat->wordWrapping == GmpiDrawing_API::MP1_WORD_WRAPPING_NO_WRAP)
                {
                    // Mac will always clip to rect, so we turn 'off' clipping by extending rect to the right.
                    if(!clipToRect)
                    {
                        const auto extendWidth = static_cast<double>(textSize.width) - bounds.size.width;
                        if(extendWidth > 0.0)
                        {
                            bounds.size.width += extendWidth;
                            
                            // fake no-clip by extending text box.
                            switch(textformat->textAlignment)
                            {
                                case GmpiDrawing_API::MP1_TEXT_ALIGNMENT_CENTER:
                                    bounds.origin.x -= 0.5 * extendWidth;
                                    break;
                                case GmpiDrawing_API::MP1_TEXT_ALIGNMENT_TRAILING: // Right
                                    bounds.origin.x -= extendWidth;
                                case GmpiDrawing_API::MP1_TEXT_ALIGNMENT_LEADING: // Left
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
                    [textformat->native2[NSParagraphStyleAttributeName] setLineBreakMode:NSLineBreakByWordWrapping];
                }
                 
//                GmpiDrawing_API::MP1_FONT_METRICS fontMetrics;
 //               ((GmpiDrawing_API::IMpTextFormat*) textFormat)->GetFontMetrics(&fontMetrics);
                
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

                    float winBaseline {};
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
                    float macBaseline {};
                    {
                        // Mac baseline
                        // Correct, but given the assumption we intend to snap baseline o pixel grid - we
                        // can use the simpler version below.
                        float predictedBt = layoutRect->top + fontMetrics.ascent + fontMetrics.descent;
                        predictedBt = truncatePixel(predictedBt, scale);
                        macBaseline = predictedBt - fontMetrics.descent;
                        macBaseline = truncatePixel(macBaseline, scale);
                    
                        if((fontMetrics.descent - floor(fontMetrics.descent)) <= 0.5f)
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
                    if(false) // Visualize Mac Baseline prediction.
                    {
                        GmpiDrawing::Graphics g(this);
                        static auto brush = g.CreateSolidColorBrush(GmpiDrawing::Color::Lime);
                         g.DrawLine(GmpiDrawing::Point(layoutRect->left,macBaseline + 0.25), GmpiDrawing::Point(layoutRect->left + 2,macBaseline + 0.25),brush, 0.25f);
                    }
#endif
                }
                
                #if 0
                    if(true) // Visualize adjusted bounds.
                    {
                        GmpiDrawing::Graphics g(this);
                        static auto brush = g.CreateSolidColorBrush(GmpiDrawing::Color::Lime);
                        g.DrawRectangle(GmpiDrawing::Rect(bounds.origin.x, bounds.origin.y, bounds.origin.x+bounds.size.width, bounds.origin.y+bounds.size.height), brush) ;
                    }
                #endif

                // do last so don't affect font metrics.
  //              [textformat->native2[NSParagraphStyleAttributeName] setLineHeightMultiple:testLineHeightMultiplier];
 //               textformat->native2[NSBaselineOffsetAttributeName] = [NSNumber numberWithFloat:shiftUp];

				[str drawInRect : bounds withAttributes : textformat->native2];
                
 //               [textformat->native2[NSParagraphStyleAttributeName] setLineHeightMultiple:1.0f];

#if 0
                {
                    GmpiDrawing::Graphics g(this);
                    GmpiDrawing_API::MP1_FONT_METRICS fontMetrics;
                    ((GmpiDrawing_API::IMpTextFormat*) textFormat)->GetFontMetrics(&fontMetrics);
                    float pixelScale=2.0; // DPI ish
                
                    if(true)
                    {
                        // nailed it.
                        float predictedBt = layoutRect->top + fontMetrics.ascent + fontMetrics.descent;
                        predictedBt = floor(predictedBt * pixelScale) / pixelScale;
                        float predictedBaseline = predictedBt - fontMetrics.descent;
                        predictedBaseline = floor(predictedBaseline * pixelScale) / pixelScale;
                        
                        if((fontMetrics.descent - floor(fontMetrics.descent)) < 0.5f)
                        {
                            predictedBaseline -= 0.5f;
                        }

                        static auto brush = g.CreateSolidColorBrush(GmpiDrawing::Color::Red);
                        g.DrawLine(GmpiDrawing::Point(layoutRect->left,predictedBaseline + 0.25), GmpiDrawing::Point(layoutRect->left + 2,predictedBaseline+0.25),brush, 0.25f);
                    }
                    
                    // Absolute vertical position does not matter, because entire row has same error,
                    // despite different vertical offsets. Therefore error comes from font metrics.
                    if(false)
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
                        
                        static auto brush = g.CreateSolidColorBrush(GmpiDrawing::Color::Red);
                        g.DrawLine(GmpiDrawing::Point(layoutRect->left +2,predictedBaseline + 0.25), GmpiDrawing::Point(layoutRect->left + 4,predictedBaseline+0.25),brush, 0.25f);
                    }
                    
                    {
                        static auto brush1 = g.CreateSolidColorBrush(GmpiDrawing::Color::Red);
                        static auto brush2 = g.CreateSolidColorBrush(GmpiDrawing::Color::Lime);
                        static auto brush3 = g.CreateSolidColorBrush(GmpiDrawing::Color::Blue);

                        float v = fontMetrics.descent;
                        float x = layoutRect->left + (v - floor(v)) * 10.0f;
                        float y = layoutRect->top;
                        g.DrawLine(GmpiDrawing::Point(x,y), GmpiDrawing::Point(x,y+5),brush1, 0.25f);
                        
                         v = fontMetrics.descent + fontMetrics.ascent;
                         x = layoutRect->left + (v - floor(v)) * 10.0f;
                         y = layoutRect->top;

                        g.DrawLine(GmpiDrawing::Point(x,y), GmpiDrawing::Point(x,y+5),brush2, 0.25f);

                        v = fontMetrics.capHeight;
                         x = layoutRect->left + (v - floor(v)) * 10.0f;
                         y = layoutRect->top;

                        g.DrawLine(GmpiDrawing::Point(x,y), GmpiDrawing::Point(x,y+5),brush3, 0.25f);

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

			virtual void MP_STDCALL DrawBitmap(const GmpiDrawing_API::IMpBitmap* mpBitmap, const GmpiDrawing_API::MP1_RECT* destinationRectangle, float opacity, /* MP1_BITMAP_INTERPOLATION_MODE*/ int32_t interpolationMode, const GmpiDrawing_API::MP1_RECT* sourceRectangle) override
			{
				auto bm = ((Bitmap*)mpBitmap);
				auto bitmap = bm->GetNativeBitmap();
				if (bitmap)
				{
                    GmpiDrawing_API::MP1_SIZE_U imageSize;
                    bm->GetSize(&imageSize);
                    
                    GmpiDrawing::Rect sourceRectangleFlipped(*sourceRectangle);
                    sourceRectangleFlipped.bottom = imageSize.height - sourceRectangle->top;
                    sourceRectangleFlipped.top = imageSize.height - sourceRectangle->bottom;

//todo					[bitmap drawInRect : gmpi::cocoa_touch::NSRectFromRect(*destinationRectangle)  fromRect : gmpi::cocoa_touch::NSRectFromRect(sourceRectangleFlipped) operation : NSCompositeSourceOver fraction : opacity respectFlipped : TRUE hints : nil];
				}
			}

			virtual void MP_STDCALL SetTransform(const GmpiDrawing_API::MP1_MATRIX_3X2* transform) override
			{
                auto context = UIGraphicsGetCurrentContext();
				// Remove the transformations by applying the inverse transform.
//				[currentTransform invert];
//				[currentTransform concat];
                
                CGContextConcatCTM(context, CGAffineTransformInvert(currentTransform));
/*
				NSAffineTransformStruct
					transformStruct = [currentTransform transformStruct];

				transformStruct.m11 = transform->_11;
				transformStruct.m12 = transform->_12;
				transformStruct.m21 = transform->_21;
				transformStruct.m22 = transform->_22;
				transformStruct.tX = transform->_31;
				transformStruct.tY = transform->_32;
                
                typedef struct {
                    CGFloat m11, m12, m21, m22;
                    CGFloat tX, tY;
                } NSAffineTransformStruct;
*/
                currentTransform = CGAffineTransform {
                    transform->_11,
                    transform->_12,
                    transform->_21,
                    transform->_22,
                    transform->_31,
                    transform->_32
                };

//				[currentTransform setTransformStruct : transformStruct];

//				[currentTransform concat];
                CGContextConcatCTM(context, currentTransform);
			}

			virtual void MP_STDCALL GetTransform(GmpiDrawing_API::MP1_MATRIX_3X2* transform) override
			{
//				NSAffineTransformStruct
//					transformStruct = [currentTransform transformStruct];

				transform->_11 = currentTransform.a;
				transform->_12 = currentTransform.b;
				transform->_21 = currentTransform.c;
				transform->_22 = currentTransform.d;
				transform->_31 = currentTransform.tx;
				transform->_32 = currentTransform.ty;
			}

			virtual int32_t MP_STDCALL CreateSolidColorBrush(const GmpiDrawing_API::MP1_COLOR* color, GmpiDrawing_API::IMpSolidColorBrush **solidColorBrush) override
			{
				gmpi_sdk::mp_shared_ptr<gmpi::IMpUnknown> b2;
				b2.Attach(new SolidColorBrush(color, factory));

				b2->queryInterface(GmpiDrawing_API::SE_IID_SOLIDCOLORBRUSH_MPGUI, reinterpret_cast<void **>(solidColorBrush));

				return gmpi::MP_OK;
			}

			virtual int32_t MP_STDCALL CreateGradientStopCollection(const GmpiDrawing_API::MP1_GRADIENT_STOP* gradientStops, uint32_t gradientStopsCount, /* GmpiDrawing_API::MP1_GAMMA colorInterpolationGamma, GmpiDrawing_API::MP1_EXTEND_MODE extendMode,*/ GmpiDrawing_API::IMpGradientStopCollection** gradientStopCollection) override
			{
				gmpi_sdk::mp_shared_ptr<gmpi::IMpUnknown> b2;
				b2.Attach(new GradientStopCollection(factory, gradientStops, gradientStopsCount));

				b2->queryInterface(GmpiDrawing_API::SE_IID_GRADIENTSTOPCOLLECTION_MPGUI, reinterpret_cast<void **>(gradientStopCollection));

				return gmpi::MP_OK;
			}

			virtual int32_t MP_STDCALL CreateLinearGradientBrush(const GmpiDrawing_API::MP1_LINEAR_GRADIENT_BRUSH_PROPERTIES* linearGradientBrushProperties, const GmpiDrawing_API::MP1_BRUSH_PROPERTIES* brushProperties, const  GmpiDrawing_API::IMpGradientStopCollection* gradientStopCollection, GmpiDrawing_API::IMpLinearGradientBrush** linearGradientBrush) override
			{
				gmpi_sdk::mp_shared_ptr<gmpi::IMpUnknown> b2;
				b2.Attach(new LinearGradientBrush(factory, linearGradientBrushProperties, brushProperties, gradientStopCollection));

				return b2->queryInterface(GmpiDrawing_API::SE_IID_LINEARGRADIENTBRUSH_MPGUI, reinterpret_cast<void **>(linearGradientBrush));
			}

			virtual int32_t MP_STDCALL CreateBitmapBrush(const GmpiDrawing_API::IMpBitmap* bitmap, const GmpiDrawing_API::MP1_BITMAP_BRUSH_PROPERTIES* bitmapBrushProperties, const GmpiDrawing_API::MP1_BRUSH_PROPERTIES* brushProperties, GmpiDrawing_API::IMpBitmapBrush** returnBrush) override
			{
                *returnBrush = nullptr;
                gmpi_sdk::mp_shared_ptr<gmpi::IMpUnknown> b2;
                b2.Attach(new BitmapBrush(factory, bitmap, bitmapBrushProperties, brushProperties));
                return b2->queryInterface(GmpiDrawing_API::SE_IID_BITMAPBRUSH_MPGUI, reinterpret_cast<void **>(returnBrush));
			}

			virtual int32_t MP_STDCALL CreateRadialGradientBrush(const GmpiDrawing_API::MP1_RADIAL_GRADIENT_BRUSH_PROPERTIES* radialGradientBrushProperties, const GmpiDrawing_API::MP1_BRUSH_PROPERTIES* brushProperties, const GmpiDrawing_API::IMpGradientStopCollection* gradientStopCollection, GmpiDrawing_API::IMpRadialGradientBrush** radialGradientBrush) override
			{
				gmpi_sdk::mp_shared_ptr<gmpi::IMpUnknown> b2;
				b2.Attach(new RadialGradientBrush(factory, radialGradientBrushProperties, brushProperties, gradientStopCollection ));

				return b2->queryInterface(GmpiDrawing_API::SE_IID_RADIALGRADIENTBRUSH_MPGUI, reinterpret_cast<void **>(radialGradientBrush));
			}

			virtual int32_t MP_STDCALL CreateCompatibleRenderTarget(const GmpiDrawing_API::MP1_SIZE* desiredSize, GmpiDrawing_API::IMpBitmapRenderTarget** bitmapRenderTarget) override;

			virtual void MP_STDCALL DrawRoundedRectangle(const GmpiDrawing_API::MP1_ROUNDED_RECT* roundedRect, const GmpiDrawing_API::IMpBrush* brush, float strokeWidth, const GmpiDrawing_API::IMpStrokeStyle* strokeStyle) override
			{
				auto r = gmpi::cocoa_touch::NSRectFromRect(roundedRect->rect);
                UIBezierPath* path = [UIBezierPath bezierPathWithRoundedRect:r cornerRadius:roundedRect->radiusX];// bezierPathWithRoundedRect : r xRadius : roundedRect->radiusX yRadius : roundedRect->radiusY];

                auto cocoabrush = dynamic_cast<const CocoaBrushBase*>(brush);
                if (cocoabrush)
                {
                    cocoabrush->StrokePath(path, strokeWidth, strokeStyle);
                }
            }

			virtual void MP_STDCALL FillRoundedRectangle(const GmpiDrawing_API::MP1_ROUNDED_RECT* roundedRect, const GmpiDrawing_API::IMpBrush* brush) override
			{
				auto r = gmpi::cocoa_touch::NSRectFromRect(roundedRect->rect);
				UIBezierPath* rectPath = [UIBezierPath bezierPathWithRoundedRect:r cornerRadius:roundedRect->radiusX];//bezierPathWithRoundedRect : r xRadius:roundedRect->radiusX yRadius: roundedRect->radiusY];

				auto cocoabrush = dynamic_cast<const CocoaBrushBase*>(brush);
				if (cocoabrush)
				{
					cocoabrush->FillPath(rectPath);
				}
			}

			virtual void MP_STDCALL DrawEllipse(const GmpiDrawing_API::MP1_ELLIPSE* ellipse, const GmpiDrawing_API::IMpBrush* brush, float strokeWidth, const GmpiDrawing_API::IMpStrokeStyle* strokeStyle) override
			{
				auto r = CGRectMake(ellipse->point.x - ellipse->radiusX, ellipse->point.y - ellipse->radiusY, ellipse->radiusX * 2.0f, ellipse->radiusY * 2.0f);

				UIBezierPath* path = [UIBezierPath bezierPathWithOvalInRect : r];

				auto cocoabrush = dynamic_cast<const CocoaBrushBase*>(brush);
				if (cocoabrush)
				{
                    cocoabrush->StrokePath(path, strokeWidth, strokeStyle);
				}
			}

			virtual void MP_STDCALL FillEllipse(const GmpiDrawing_API::MP1_ELLIPSE* ellipse, const GmpiDrawing_API::IMpBrush* brush) override
			{
                auto r = CGRectMake(ellipse->point.x - ellipse->radiusX, ellipse->point.y - ellipse->radiusY, ellipse->radiusX * 2.0f, ellipse->radiusY * 2.0f);

				UIBezierPath* rectPath = [UIBezierPath bezierPathWithOvalInRect : r];

				auto cocoabrush = dynamic_cast<const CocoaBrushBase*>(brush);
				if (cocoabrush)
				{
					cocoabrush->FillPath(rectPath);
				}
			}

			virtual void MP_STDCALL PushAxisAlignedClip(const GmpiDrawing_API::MP1_RECT* clipRect/*, GmpiDrawing_API::MP1_ANTIALIAS_MODE antialiasMode*/) override
			{
				clipRectStack.push_back(*clipRect);

				// Save the current clipping region
				UIGraphicsPushContext(UIGraphicsGetCurrentContext());

				UIRectClip(NSRectFromRect(*clipRect));
			}

			virtual void MP_STDCALL PopAxisAlignedClip() override
			{
				clipRectStack.pop_back();

				// Restore the clipping region for further drawing
				UIGraphicsPopContext();;
			}

			virtual void MP_STDCALL GetAxisAlignedClip(GmpiDrawing_API::MP1_RECT* returnClipRect) override
			{
                GmpiDrawing::Matrix3x2 currentTransform;
                GetTransform(&currentTransform);
                currentTransform.Invert();
				*returnClipRect = currentTransform.TransformRect(clipRectStack.back());
			}

			virtual void MP_STDCALL BeginDraw() override
			{
				//		context_->BeginDraw();
			}

			virtual int32_t MP_STDCALL EndDraw() override
			{
				//		auto hr = context_->EndDraw();

				//		return hr == S_OK ? (gmpi::MP_OK) : (gmpi::MP_FAIL);
				return gmpi::MP_OK;
			}
/*
            virtual int32_t MP_STDCALL CreateMesh(GmpiDrawing_API::IMpMesh** returnObject) override
            {
                *returnObject = nullptr;
                return gmpi::MP_FAIL;
            }
            
            virtual void MP_STDCALL FillMesh(const GmpiDrawing_API::IMpMesh* mesh, const GmpiDrawing_API::IMpBrush* brush) override
            {
                
            }
*/
			//	virtual void MP_STDCALL InsetNewMethodHere(){}

			GMPI_QUERYINTERFACE1(GmpiDrawing_API::SE_IID_DEVICECONTEXT_MPGUI, GmpiDrawing_API::IMpDeviceContext);
			GMPI_REFCOUNT_NO_DELETE;
		};

		// https://stackoverflow.com/questions/10627557/mac-os-x-drawing-into-an-offscreen-nsgraphicscontext-using-cgcontextref-c-funct
		class bitmapRenderTarget : public GraphicsContext // emulated by carefull layout public GmpiDrawing_API::IMpBitmapRenderTarget
		{
			UIImage* image;

		public:
			bitmapRenderTarget(GmpiDrawing_API::IMpFactory* pfactory, const GmpiDrawing_API::MP1_SIZE* desiredSize) :
				GraphicsContext(nullptr, pfactory)
			{
				//auto r = CGRectMake(0.0, 0.0, desiredSize->width, desiredSize->height);
                //todo                image = [[UIImage alloc] initWithSize:r.size];

				clipRectStack.push_back({ 0, 0, desiredSize->width, desiredSize->height });
			}

            virtual void MP_STDCALL BeginDraw() override
            {
                // To match Flipped View, Flip Bitmap Context too.
                // (Alternative is [image setFlipped:TRUE] in contructor, but that method is deprected).
                //todo                [image lockFocusFlipped:TRUE];

                GraphicsContext::BeginDraw();
            }
            
            virtual int32_t MP_STDCALL EndDraw() override
            {
                auto r = GraphicsContext::EndDraw();
//todo                [image unlockFocus];
                return r;
            }
            
			~bitmapRenderTarget()
			{
//todo				[image release];
			}

            // MUST BE FIRST VIRTUAL FUNCTION!
			virtual int32_t MP_STDCALL GetBitmap(GmpiDrawing_API::IMpBitmap** returnBitmap)
			{
				gmpi_sdk::mp_shared_ptr<gmpi::IMpUnknown> b;
				b.Attach(new Bitmap(factory, image));
				return b->queryInterface(GmpiDrawing_API::SE_IID_BITMAP_MPGUI, reinterpret_cast<void**>(returnBitmap));
			}

			virtual int32_t MP_STDCALL queryInterface(const gmpi::MpGuid& iid, void** returnInterface) override
			{
				*returnInterface = 0;
				if (iid == GmpiDrawing_API::SE_IID_BITMAP_RENDERTARGET_MPGUI)
				{
					// non-standard. Forcing this class (which has the correct vtable) to pretend it's the emulated interface.
					*returnInterface = reinterpret_cast<GmpiDrawing_API::IMpBitmapRenderTarget*>(this);
					addRef();
					return gmpi::MP_OK;
				}
				else
				{
					return GraphicsContext::queryInterface(iid, returnInterface);
				}
			}

			GMPI_REFCOUNT;
		};

		int32_t MP_STDCALL GraphicsContext::CreateCompatibleRenderTarget(const GmpiDrawing_API::MP1_SIZE* desiredSize, GmpiDrawing_API::IMpBitmapRenderTarget** bitmapRenderTarget)
		{
			gmpi_sdk::mp_shared_ptr<gmpi::IMpUnknown> b2;
			b2.Attach(new class bitmapRenderTarget(factory, desiredSize));

			return b2->queryInterface(GmpiDrawing_API::SE_IID_BITMAP_RENDERTARGET_MPGUI, reinterpret_cast<void **>(bitmapRenderTarget));
		}

	} // namespace
} // namespace
