#pragma once
/*
#include "GdiPlusGfx.h"
*/

#include "./gmpi_gui_hosting.h"
#include <windows.h>
#define _USE_MATH_DEFINES
#include <math.h>
#include <gdiplus.h>
#include <memory>
#include "../shared/fast_gamma.h"
#include "./Gfx_base.h"

using namespace se_sdk;

namespace Gdiplus
{
	class Graphics;
}

namespace gmpi
{
	namespace gdiplus
	{
		inline Gdiplus::PointF ToGdiPlusType(GmpiDrawing_API::MP1_POINT p)
		{
			return Gdiplus::PointF(p.x, p.y);
		}

		inline Gdiplus::Color ToGdiPlusType(GmpiDrawing_API::MP1_COLOR color)
		{
			//	return Gdiplus::Color((BYTE)(color.a * 255), (BYTE)(color.r * 255), (BYTE)(color.g * 255), (BYTE)(color.b * 255));
			return Gdiplus::Color((BYTE)(color.a * 255), FastGamma::float_to_sRGB(color.r), FastGamma::float_to_sRGB(color.g), FastGamma::float_to_sRGB(color.b));
		}

		//inline Gdiplus::Color toGdiPlusCol(GmpiDrawing::Color color)
		//{
		//	return Gdiplus::Color((BYTE)(color.a * 255), (BYTE)(color.r * 255), (BYTE)(color.g * 255), (BYTE)(color.b * 255));
		//}
		inline GmpiDrawing::PathGeometry createRoundedRectangleGeometry(GmpiDrawing::Factory factory, const GmpiDrawing_API::MP1_ROUNDED_RECT* roundedRect)
		{
			const float rightAngle = M_PI * 0.5f;
			const auto radius = roundedRect->radiusX;

			// create geometry
			auto geometry = factory.CreatePathGeometry();
			auto sink = geometry.Open();

			// top left
			sink.BeginFigure(GmpiDrawing::Point(roundedRect->rect.left, roundedRect->rect.top + radius), GmpiDrawing::FigureBegin::Filled);
			{
				GmpiDrawing::ArcSegment as(GmpiDrawing::Point(roundedRect->rect.left + radius, roundedRect->rect.top), GmpiDrawing::Size(radius, radius), rightAngle);
				sink.AddArc(as);
			}

			// top right
			sink.AddLine(GmpiDrawing::Point(roundedRect->rect.right - radius, roundedRect->rect.top));
			{
				GmpiDrawing::ArcSegment as(GmpiDrawing::Point(roundedRect->rect.right, roundedRect->rect.top + radius), GmpiDrawing::Size(radius, radius), rightAngle);
				sink.AddArc(as);
			}

			sink.AddLine(GmpiDrawing::Point(roundedRect->rect.right, roundedRect->rect.bottom - radius));
			{
				GmpiDrawing::ArcSegment as(GmpiDrawing::Point(roundedRect->rect.right - radius, roundedRect->rect.bottom), GmpiDrawing::Size(radius, radius), rightAngle);
				sink.AddArc(as);
			}
			sink.AddLine(GmpiDrawing::Point(roundedRect->rect.left + radius, roundedRect->rect.bottom));
			{
				GmpiDrawing::ArcSegment as(GmpiDrawing::Point(roundedRect->rect.left, roundedRect->rect.bottom - radius), GmpiDrawing::Size(radius, radius), rightAngle);
				sink.AddArc(as);
			}

			sink.EndFigure(GmpiDrawing::FigureEnd::Closed);
			sink.Close();
			
			return geometry;
		}

		class GdiPlusHelper
		{
			Gdiplus::Graphics graphics_;

			GdiPlusHelper() : graphics_(new Gdiplus::Bitmap(1, 1))
			{
				graphics_.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAlias);
			}

		public:
			static GdiPlusHelper* Instance();

			void GetTextExtentU(Gdiplus::Font* font, Gdiplus::StringFormat* stringformat, const char* utf8String, int32_t stringLength, GmpiDrawing_API::MP1_SIZE* returnSize);
		};

		class GradientStopCollection : public GmpiDrawing_API::IMpGradientStopCollection
		{
		public:
			std::vector<GmpiDrawing_API::MP1_GRADIENT_STOP> gradientstops;

			GradientStopCollection(const GmpiDrawing_API::MP1_GRADIENT_STOP* gradientStops, uint32_t gradientStopsCount)
			{
				for (uint32_t i = 0; i < gradientStopsCount; ++i)
				{
					gradientstops.push_back(gradientStops[i]);
				}
			}

			virtual void MP_STDCALL GetFactory(GmpiDrawing_API::IMpFactory** factory) override
			{
				assert(false); // implement this!
				return;
			}

			GMPI_QUERYINTERFACE1(GmpiDrawing_API::SE_IID_GRADIENTSTOPCOLLECTION_MPGUI, GmpiDrawing_API::IMpGradientStopCollection);
			GMPI_REFCOUNT;
		};

		class hasGdiPlusBrush
		{
		public:
			virtual const Gdiplus::Brush* native() = 0;
		};

		class PGCC_LinearGradientBrush : public GmpiDrawing_API::IMpLinearGradientBrush, public hasGdiPlusBrush
		{
		public:
			std::unique_ptr<Gdiplus::LinearGradientBrush> Brush;
			GmpiDrawing_API::MP1_POINT startPoint;
			GmpiDrawing_API::MP1_POINT endPoint;
			//	const GradientStopCollection* mGradientStopCollection;
			gmpi_sdk::mp_shared_ptr<GradientStopCollection> mGradientStopCollection;

			PGCC_LinearGradientBrush(const GmpiDrawing_API::MP1_LINEAR_GRADIENT_BRUSH_PROPERTIES* linearGradientBrushProperties, const GmpiDrawing_API::MP1_BRUSH_PROPERTIES* brushProperties, GmpiDrawing_API::IMpGradientStopCollection* gradientStopCollection)
			{
				mGradientStopCollection = dynamic_cast<GradientStopCollection*>(gradientStopCollection);
				startPoint = linearGradientBrushProperties->startPoint;
				endPoint = linearGradientBrushProperties->endPoint;
			}

			virtual void MP_STDCALL SetStartPoint(GmpiDrawing_API::MP1_POINT pstartPoint) override
			{
				startPoint = pstartPoint;
			}

			virtual void MP_STDCALL SetEndPoint(GmpiDrawing_API::MP1_POINT pendPoint) override
			{
				endPoint = pendPoint;
			}

			virtual void MP_STDCALL GetFactory(GmpiDrawing_API::IMpFactory** factory) override
			{
				assert(false); // implement this!
				return;
			}

			virtual const Gdiplus::Brush* native() override
			{
				if (Brush.get() == nullptr)
				{
					auto native = new Gdiplus::LinearGradientBrush(ToGdiPlusType(startPoint), ToGdiPlusType(endPoint), ToGdiPlusType(mGradientStopCollection->gradientstops.front().color), ToGdiPlusType(mGradientStopCollection->gradientstops.back().color));
					Brush.reset(native);
				}
				return Brush.get();
			}

			GMPI_QUERYINTERFACE1(GmpiDrawing_API::SE_IID_LINEARGRADIENTBRUSH_MPGUI, GmpiDrawing_API::IMpLinearGradientBrush);
			GMPI_REFCOUNT;
		};

		class PGCC_solidColorBrush : public GmpiDrawing_API::IMpSolidColorBrush, public hasGdiPlusBrush
		{
		public:
			Gdiplus::SolidBrush Brush;

			PGCC_solidColorBrush(GmpiDrawing::Color color) :
				Brush(Gdiplus::Color(ToGdiPlusType(color)))
			{}

			virtual const Gdiplus::Brush* native() override
			{
				return &Brush;
			}

			virtual void MP_STDCALL SetColor(const GmpiDrawing_API::MP1_COLOR* color) override
			{
				Brush.SetColor(ToGdiPlusType(*color));
			}
			virtual GmpiDrawing_API::MP1_COLOR MP_STDCALL GetColor() override
			{
				Gdiplus::Color c;
				Brush.GetColor(&c);
				return GmpiDrawing::Color(c.GetRed() / 255.0f, c.GetGreen() / 255.0f, c.GetBlue() / 255.0f, c.GetAlpha() / 255.0f);
			}

			virtual void MP_STDCALL GetFactory(GmpiDrawing_API::IMpFactory** factory) override
			{
				assert(false); // implement this!
				return;
			}

			GMPI_QUERYINTERFACE1(GmpiDrawing_API::SE_IID_SOLIDCOLORBRUSH_MPGUI, GmpiDrawing_API::IMpSolidColorBrush)
			GMPI_REFCOUNT
		};

		class PGCC_fakeBitmapBrush : public GmpiDrawing_API::IMpBitmapBrush, public hasGdiPlusBrush
		{
		public:
			Gdiplus::SolidBrush Brush;

			PGCC_fakeBitmapBrush() : Brush(Gdiplus::Color::Black)
			{}

			void MP_STDCALL GetFactory(GmpiDrawing_API::IMpFactory** factory) override
			{
				assert(false); // implement this!
				return;
			}

			// hasGdiPlusBrush
			const Gdiplus::Brush* native() override
			{
				return &Brush;
			}

			// IMpBitmapBrush
			void MP_STDCALL SetExtendModeX(GmpiDrawing_API::MP1_EXTEND_MODE extendModeX) override {}
			void MP_STDCALL SetExtendModeY(GmpiDrawing_API::MP1_EXTEND_MODE extendModeY) override {}
			void MP_STDCALL SetInterpolationMode(GmpiDrawing_API::MP1_BITMAP_INTERPOLATION_MODE interpolationMode) override {}

			GMPI_QUERYINTERFACE1(GmpiDrawing_API::SE_IID_BITMAPBRUSH_MPGUI, GmpiDrawing_API::IMpBitmapBrush);
			GMPI_REFCOUNT;
		};

		class PGCC_fakeRadialGradientBrush : public GmpiDrawing_API::IMpRadialGradientBrush, public hasGdiPlusBrush
		{
		public:
			Gdiplus::SolidBrush Brush;

			PGCC_fakeRadialGradientBrush() : Brush(Gdiplus::Color::Black)
			{}

			void MP_STDCALL GetFactory(GmpiDrawing_API::IMpFactory** factory) override
			{
				assert(false); // implement this!
				return;
			}

			// hasGdiPlusBrush
			const Gdiplus::Brush* native() override
			{
				return &Brush;
			}

			// IMpRadialGradientBrush
			void MP_STDCALL SetCenter(GmpiDrawing_API::MP1_POINT center) override {}
			void MP_STDCALL SetGradientOriginOffset(GmpiDrawing_API::MP1_POINT gradientOriginOffset) override {}
			void MP_STDCALL SetRadiusX(float radiusX) override {}
			void MP_STDCALL SetRadiusY(float radiusY) override {}

			GMPI_QUERYINTERFACE1(GmpiDrawing_API::SE_IID_RADIALGRADIENTBRUSH_MPGUI, GmpiDrawing_API::IMpRadialGradientBrush);
			GMPI_REFCOUNT;
		};

		// GDI+ font
		class fontObject : public gmpi::IMpUnknown
		{
		public:
			Gdiplus::Font font_;
			std::wstring fontFamilyName_;
			float fontSize_;

			fontObject(const wchar_t* fontFamilyName, float fontSize, int style) :
				font_(fontFamilyName, fontSize * (72.f / 96.f), style) // convert to DirectX units.
				, fontFamilyName_(fontFamilyName)
				, fontSize_(fontSize)
			{
				//		_RPTW3(_CRT_WARN, L"new fontObject %x (%s %f)\n", this, fontFamilyName, fontSize);
			}

			~fontObject()
			{
				//		_RPT1(_CRT_WARN, "delete fontObject %x\n", this);
			}

			//	GMPI_INTERFACE_METHODS(gmpi::MP_IID_UNKNOWN, gmpi::IMpUnknown)
			GMPI_QUERYINTERFACE1(gmpi::MP_IID_UNKNOWN, gmpi::IMpUnknown)
				GMPI_REFCOUNT
		};

		class PGCC_FontCache
		{
		public:
			std::vector< gmpi_sdk::mp_shared_ptr<fontObject> > fonts_;
			std::shared_ptr<int> test;
		};

		class PGCC_textFormat : public GmpiDrawing_API::IMpTextFormat
		{
			static PGCC_FontCache fontCache_;
			static const int defaultFlags = Gdiplus::StringFormatFlagsNoClip | Gdiplus::StringFormatFlagsMeasureTrailingSpaces;
		public:
			std::string _fontFamilyName;
			int _fontWeight;
			int _fontStyle;
			int _fontStretch;
			float dpiScaling;
			GmpiDrawing::Size cachedGdiPlusFontPadding;

			Gdiplus::StringFormat _stringFormat;
			gmpi_sdk::mp_shared_ptr<fontObject> fontHolder_;

			PGCC_textFormat(const char* fontFamilyName, int fontWeight, int fontStyle, int fontStretch, float fontSize, float pDpiScaling);

			~PGCC_textFormat()
			{
				//		_RPT1(_CRT_WARN, "delete PGCC_textFormat %x\n", this);
				//if( fontHolder_ )
				//{
				//	fontHolder_->rele ase();
				//}
			}

			//	virtual void MP_STDCALL SetTextAlignment(int32_t alignment)
			virtual int32_t MP_STDCALL SetTextAlignment(GmpiDrawing_API::MP1_TEXT_ALIGNMENT textAlignment) override
			{
				switch (textAlignment)
				{
				case GmpiDrawing_API::MP1_TEXT_ALIGNMENT_LEADING: // gmpi_gui::Font::Alignment::A_Near: // DWRITE_TEXT_ALIGNMENT_LEADING
					_stringFormat.SetAlignment(Gdiplus::StringAlignmentNear);
					break;

				case GmpiDrawing_API::MP1_TEXT_ALIGNMENT_TRAILING: // gmpi_gui::Font::Alignment::A_Far: // DWRITE_TEXT_ALIGNMENT_TRAILING
					_stringFormat.SetAlignment(Gdiplus::StringAlignmentFar);
					break;

				default: // DWRITE_TEXT_ALIGNMENT_CENTER
					_stringFormat.SetAlignment(Gdiplus::StringAlignmentCenter);
					break;
				}
				return gmpi::MP_OK;
			}

			//	virtual void MP_STDCALL SetParagraphAlignment(int32_t alignment)
			virtual int32_t MP_STDCALL SetParagraphAlignment(GmpiDrawing_API::MP1_PARAGRAPH_ALIGNMENT paragraphAlignment) override
			{
				switch (paragraphAlignment)
				{
				case GmpiDrawing_API::MP1_PARAGRAPH_ALIGNMENT_NEAR: // gmpi_gui::Font::Alignment::A_Near: // DWRITE_TEXT_ALIGNMENT_LEADING
					_stringFormat.SetLineAlignment(Gdiplus::StringAlignmentNear); // Bottom.
					break;

				case GmpiDrawing_API::MP1_PARAGRAPH_ALIGNMENT_FAR: // gmpi_gui::Font::Alignment::A_Far: // DWRITE_TEXT_ALIGNMENT_TRAILING
					_stringFormat.SetLineAlignment(Gdiplus::StringAlignmentFar); // Top.
					break;

				default: // DWRITE_TEXT_ALIGNMENT_CENTER
					_stringFormat.SetLineAlignment(Gdiplus::StringAlignmentCenter);
					break;
				}
				return gmpi::MP_OK;
			}

			virtual int32_t MP_STDCALL SetWordWrapping(GmpiDrawing_API::MP1_WORD_WRAPPING wordWrapping) override
			{
				if(wordWrapping == GmpiDrawing_API::MP1_WORD_WRAPPING::MP1_WORD_WRAPPING_NO_WRAP)
				{
					_stringFormat.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap|defaultFlags);
				}
				else
				{
					_stringFormat.SetFormatFlags(defaultFlags);
				}
				return gmpi::MP_NOSUPPORT;
			}

			virtual int32_t MP_STDCALL SetLineSpacing(float lineSpacing, float baseline) override
			{
				return gmpi::MP_NOSUPPORT;
			}

			//	virtual void MP_STDCALL GetTextExtentU(const char* utf8String, int32_t stringLength, GmpiDrawing::Size& returnSize)
			virtual void MP_STDCALL GetTextExtentU(const char* utf8String, int32_t stringLength, GmpiDrawing_API::MP1_SIZE* returnSize) override
			{
				GdiPlusHelper::Instance()->GetTextExtentU(&(fontHolder_->font_), &_stringFormat, utf8String, stringLength, returnSize);

				returnSize->width -= 2.0f * getGdiPlusTextPadding().width;

				//returnSize.x *= dpiScaling;
				//returnSize.y *= dpiScaling;
			}

			virtual int32_t MP_STDCALL GetFontMetrics(GmpiDrawing_API::MP1_FONT_METRICS* returnFontMetrics) override;

			GmpiDrawing::Size getGdiPlusTextPadding();

			GMPI_QUERYINTERFACE1(GmpiDrawing_API::SE_IID_TEXTFORMAT_MPGUI, GmpiDrawing_API::IMpTextFormat)
				GMPI_REFCOUNT
		};

		class PGCC_bitmapPixels : public GmpiDrawing_API::IMpBitmapPixels
		{
			Gdiplus::Bitmap* bitmap;
			Gdiplus::Image* nativeBitmap_;
			Gdiplus::BitmapData data;

		public:
			PGCC_bitmapPixels(Gdiplus::Bitmap* nativeBitmap, bool _alphaPremultiplied, int32_t pflags)
			{
				bitmap = nullptr;

				Gdiplus::Rect r(0, 0, nativeBitmap->GetWidth(), nativeBitmap->GetHeight());
				Gdiplus::PixelFormat pixelFormat = _alphaPremultiplied ? PixelFormat32bppPARGB : PixelFormat32bppARGB;

				{
					int gdiflags = 0;
					if ((pflags & GmpiDrawing_API::MP1_BITMAP_LOCK_READ) != 0)
					{
						gdiflags |= Gdiplus::ImageLockModeRead;
					}
					if ((pflags & GmpiDrawing_API::MP1_BITMAP_LOCK_WRITE) != 0)
					{
						gdiflags |= Gdiplus::ImageLockModeWrite;
					}

					if (nativeBitmap->LockBits(&r, gdiflags, pixelFormat, &data) == Gdiplus::Ok)
					{
						bitmap = nativeBitmap;
					}
				}
			}

			~PGCC_bitmapPixels()
			{
				if (bitmap)
					bitmap->UnlockBits(&data);
			}

			virtual uint8_t* MP_STDCALL getAddress() const { return (uint8_t*)data.Scan0; };
			virtual int32_t MP_STDCALL getBytesPerRow() const { return data.Stride; };
			virtual int32_t MP_STDCALL getPixelFormat() const { return kBGRA; }// data.PixelFormat; // probly needs translations. TODO!!!

			GMPI_QUERYINTERFACE1(GmpiDrawing_API::SE_IID_BITMAP_PIXELS_MPGUI, GmpiDrawing_API::IMpBitmapPixels);
			GMPI_REFCOUNT;
		};

		class PGCC_bitmap_base : public GmpiDrawing_API::IMpBitmap
		{
			GmpiDrawing_API::IMpFactory* factory = nullptr;

		public:
			PGCC_bitmap_base(GmpiDrawing_API::IMpFactory* pfactory) :
				factory(pfactory)
			{}

			virtual Gdiplus::Bitmap* getNativeBitmap() = 0;

			virtual GmpiDrawing_API::MP1_SIZE MP_STDCALL GetSizeF() override
			{
				GmpiDrawing_API::MP1_SIZE returnSize;
				returnSize.width = (float)getNativeBitmap()->GetWidth();
				returnSize.height = (float)getNativeBitmap()->GetHeight();
				return returnSize;
			}
			virtual int32_t MP_STDCALL GetSize(GmpiDrawing_API::MP1_SIZE_U* returnSize) override
			{
				returnSize->width = getNativeBitmap()->GetWidth();
				returnSize->height = getNativeBitmap()->GetHeight();

				return gmpi::MP_OK;
			}

			virtual int32_t MP_STDCALL lockPixelsOld(GmpiDrawing_API::IMpBitmapPixels** returnInterface, bool alphaPremultiplied) override
			{
				*returnInterface = 0;

				gmpi_sdk::mp_shared_ptr<gmpi::IMpUnknown> b2;
				b2.Attach(new PGCC_bitmapPixels(getNativeBitmap(), alphaPremultiplied, GmpiDrawing_API::MP1_BITMAP_LOCK_READ | GmpiDrawing_API::MP1_BITMAP_LOCK_WRITE));

				return b2->queryInterface(GmpiDrawing_API::SE_IID_BITMAP_PIXELS_MPGUI, (void**)returnInterface);
			}

			virtual int32_t MP_STDCALL lockPixels(GmpiDrawing_API::IMpBitmapPixels** returnInterface, int32_t flags)
			{
				*returnInterface = 0;

				gmpi_sdk::mp_shared_ptr<gmpi::IMpUnknown> b2;
				b2.Attach(new PGCC_bitmapPixels(getNativeBitmap(), true, flags));

				return b2->queryInterface(GmpiDrawing_API::SE_IID_BITMAP_PIXELS_MPGUI, (void**)(returnInterface));
			}

			virtual void MP_STDCALL ApplyAlphaCorrection() // deprecated
			{
				// N/A
			}

			virtual void MP_STDCALL GetFactory(GmpiDrawing_API::IMpFactory** rfactory) override;

			GMPI_QUERYINTERFACE1(GmpiDrawing_API::SE_IID_BITMAP_MPGUI, GmpiDrawing_API::IMpBitmap);
			GMPI_REFCOUNT;
		};

		class PGCC_bitmap : public PGCC_bitmap_base
		{
			Gdiplus::Bitmap nativeBitmap_;

		public:
			virtual Gdiplus::Bitmap* getNativeBitmap() override
			{
				return &nativeBitmap_;
			}

			PGCC_bitmap(GmpiDrawing_API::IMpFactory* pFactory, IStream* stream) :
				nativeBitmap_(stream)
				, PGCC_bitmap_base(pFactory)
			{
			}

			PGCC_bitmap(GmpiDrawing_API::IMpFactory* pFactory, const wchar_t* uri) :
				nativeBitmap_(uri)
				, PGCC_bitmap_base(pFactory)
			{
				//		_RPT1(_CRT_WARN, "new PGCC_bitmap %x\n", this);
				//		nativeBitmap_.ConvertFormat(PixelFormat32bppPARGB, Gdiplus::DitherTypeNone, Gdiplus::PaletteTypeCustom, 0, 0.0f);
			}

			PGCC_bitmap(GmpiDrawing_API::IMpFactory* pFactory, int32_t width, int32_t height) :
				nativeBitmap_(width, height)
				, PGCC_bitmap_base(pFactory)
			{
			}

			GMPI_QUERYINTERFACE1(GmpiDrawing_API::SE_IID_BITMAP_MPGUI, GmpiDrawing_API::IMpBitmap);
			GMPI_REFCOUNT;
		};

		class BitmapRenderTarget;

		class PGCC_bitmap2 : public PGCC_bitmap_base
		{
			Gdiplus::Bitmap* nativeBitmap_;
			class BitmapRenderTarget* bitmapRenderTarget_;
		public:

			PGCC_bitmap2(GmpiDrawing_API::IMpFactory* pFactory, BitmapRenderTarget* bitmapRenderTarget);
			~PGCC_bitmap2();

			virtual Gdiplus::Bitmap* getNativeBitmap() override
			{
				return nativeBitmap_;
			}

			GMPI_QUERYINTERFACE1(GmpiDrawing_API::SE_IID_BITMAP_MPGUI, GmpiDrawing_API::IMpBitmap);
			GMPI_REFCOUNT;
		};

		class PathGeometry : public GmpiDrawing_API::IMpPathGeometry
		{
		public:
			Gdiplus::GraphicsPath native;
			Gdiplus::PointF startPoint;
			bool closed_;

			virtual int32_t MP_STDCALL Open(GmpiDrawing_API::IMpGeometrySink **geometrySink) override;

			virtual void MP_STDCALL GetFactory(GmpiDrawing_API::IMpFactory** factory) override
			{
				assert(false); // implement this!
				return;
			}
			virtual int32_t MP_STDCALL StrokeContainsPoint(GmpiDrawing_API::MP1_POINT point, float strokeWidth, GmpiDrawing_API::IMpStrokeStyle* strokeStyle, const GmpiDrawing_API::MP1_MATRIX_3X2* worldTransform, bool* returnContains) override
			{
				// Approximation only for rough forward compatibility.
				*returnContains = true;

				return gmpi::MP_FAIL;
			}
			virtual int32_t MP_STDCALL FillContainsPoint(GmpiDrawing_API::MP1_POINT point, const GmpiDrawing_API::MP1_MATRIX_3X2* worldTransform, bool* returnContains) override
			{
				// Approximation only for rough forward compatibility.
				*returnContains = true;

				return gmpi::MP_FAIL;
			}
			virtual int32_t MP_STDCALL GetWidenedBounds(float strokeWidth, GmpiDrawing_API::IMpStrokeStyle* strokeStyle, const GmpiDrawing_API::MP1_MATRIX_3X2* worldTransform, GmpiDrawing_API::MP1_RECT* returnBounds) override
			{
				// Return empty rect as bare minium support for compatibility with newer modules.
				returnBounds->left = returnBounds->right = returnBounds->top = returnBounds->bottom = 0.0f;
				return gmpi::MP_FAIL;
			}

			GMPI_QUERYINTERFACE1(GmpiDrawing_API::SE_IID_PATHGEOMETRY_MPGUI, GmpiDrawing_API::IMpPathGeometry);
			GMPI_REFCOUNT;
		};


		class GeometrySink : public gmpi::generic_graphics::GeometrySink
		{
		public:
			class PathGeometry* pathGeometry_;

			GeometrySink(PathGeometry* pathGeometry) : pathGeometry_(pathGeometry)
			{}

			virtual void MP_STDCALL BeginFigure(GmpiDrawing_API::MP1_POINT startPoint, GmpiDrawing_API::MP1_FIGURE_BEGIN figureBegin) override
			{
				pathGeometry_->native.Reset();
				pathGeometry_->startPoint = ToGdiPlusType(startPoint);

				lastPoint = startPoint;
			}

			virtual void MP_STDCALL AddLine(GmpiDrawing_API::MP1_POINT point) override
			{
				pathGeometry_->native.AddLine(ToGdiPlusType(lastPoint), ToGdiPlusType(point));
				lastPoint = point;
			}

			virtual void MP_STDCALL EndFigure(GmpiDrawing_API::MP1_FIGURE_END figureEnd) override
			{
				pathGeometry_->closed_ = figureEnd == GmpiDrawing_API::MP1_FIGURE_END_CLOSED;

				if (pathGeometry_->closed_)
				{
					pathGeometry_->native.AddLine(ToGdiPlusType(lastPoint), pathGeometry_->startPoint);
				}
			}

			virtual void MP_STDCALL AddBezier(const GmpiDrawing_API::MP1_BEZIER_SEGMENT* bezier) override
			{
				pathGeometry_->native.AddBezier(ToGdiPlusType(lastPoint), ToGdiPlusType(bezier->point1), ToGdiPlusType(bezier->point2), ToGdiPlusType(bezier->point3));

				lastPoint = bezier->point3;
			}

			GMPI_QUERYINTERFACE1(GmpiDrawing_API::SE_IID_GEOMETRYSINK_MPGUI, GmpiDrawing_API::IMpGeometrySink);
			GMPI_REFCOUNT;
		};

		inline int32_t PathGeometry::Open(GmpiDrawing_API::IMpGeometrySink **geometrySink)
		{
			closed_ = false;

			*geometrySink = new GeometrySink(this);
			return gmpi::MP_OK;
		}


		class Factory : public GmpiDrawing_API::IMpFactory
		{
			float panelViewFontScaling;

		public:
			Factory()
			{
				/*
				CDC dcBuffer;
				// create a memory DC for buffer
				dcBuffer.CreateCompatibleDC(NULL);

				int dpiY = dcBuffer.GetDeviceCaps(LOGPIXELSY);
				*/

				HDC hdc = ::GetDC(NULL);
				//		int dpiX = GetDeviceCaps(hdc, LOGPIXELSX);
				int dpiY = GetDeviceCaps(hdc, LOGPIXELSY);
				::ReleaseDC(NULL, hdc);

				panelViewFontScaling = 96.0f / (float)dpiY;
			}

			virtual int32_t MP_STDCALL CreatePathGeometry(GmpiDrawing_API::IMpPathGeometry **pathGeometry) override
			{
				*pathGeometry = new PathGeometry();
				return gmpi::MP_OK;
			}

			virtual int32_t MP_STDCALL CreateTextFormat(const char* fontFamilyName, void* unused /* fontCollection */, GmpiDrawing_API::MP1_FONT_WEIGHT fontWeight, GmpiDrawing_API::MP1_FONT_STYLE fontStyle, GmpiDrawing_API::MP1_FONT_STRETCH fontStretch, float fontSize, void* unused2 /* localeName */, GmpiDrawing_API::IMpTextFormat** textFormat)
			{
				// GDI plus takes system DPI from device context into account when drawing text.
				// However Panel view currently is not DPI aware. So need to fool GDI+ by reducing font size.
				//	fontSize *= panelViewFontScaling;

				gmpi_sdk::mp_shared_ptr<gmpi::IMpUnknown> b2;
				b2.Attach(new PGCC_textFormat(fontFamilyName, fontWeight, fontStyle, fontStretch, fontSize, panelViewFontScaling));

				b2->queryInterface(GmpiDrawing_API::SE_IID_TEXTFORMAT_MPGUI, reinterpret_cast<void**>(textFormat));

				return textFormat != 0 ? (gmpi::MP_OK) : (gmpi::MP_FAIL);
			}

			virtual int32_t MP_STDCALL CreateImage(int32_t width, int32_t height, GmpiDrawing_API::IMpBitmap** returnDiBitmap) override
			{
				*returnDiBitmap = nullptr;

				auto bitmap = new PGCC_bitmap(this, width, height);
				gmpi_sdk::mp_shared_ptr<GmpiDrawing_API::IMpBitmap> b2;
				b2.Attach(bitmap);

				if(bitmap->getNativeBitmap()->GetLastStatus() == Gdiplus::Status::Ok)
				{
					b2->queryInterface(GmpiDrawing_API::SE_IID_BITMAP_MPGUI, (void**)returnDiBitmap);
					return gmpi::MP_OK;
				}

				return gmpi::MP_FAIL;
			}

			virtual int32_t MP_STDCALL LoadImageU(const char* utf8Uri, GmpiDrawing_API::IMpBitmap** returnDiBitmap)
			{
				*returnDiBitmap = 0;

				gmpi_sdk::mp_shared_ptr<GmpiDrawing_API::IMpBitmap> b2;

				if (strstr(utf8Uri, "res:/") == nullptr) // file is on disk.
				{
					auto bm = new PGCC_bitmap(this, Utf8ToWstring(utf8Uri).c_str());
					if (bm->getNativeBitmap()->GetLastStatus() == 0)
					{
						b2.Attach(bm);
					}
					else
					{
						delete bm;
						return gmpi::MP_FAIL;
					}
				}
				else
				{
					assert(false);
					// TODO
					/*
					// file is in resources.
					// allocate buffer and create stream. Inefficiant as creates extra copy in memory.
					IStream *pStream = NULL;
					int64_t size;
					pf->getSize(&size);
					HGLOBAL m_hBuffer = ::GlobalAlloc(GMEM_FIXED, (SIZE_T)size);

					// fill the buffer with image data
					pf->read((char*)m_hBuffer, size);

					::CreateStreamOnHGlobal(m_hBuffer, TRUE, &pStream); // TRUE indicates pStream->Release() should free HGLOBAL automatically.

					_RPT1(_CRT_WARN, "new PGCC_bitmap %s\n", uri.c_str());
					GmpiDrawing::Bitmap b2;
					b2.Attach(new PGCC_bitmap(pStream));

					pStream->Release(); // may not release HGLOBAL because GDI plus maintains a reference or 2.
					*/
				}
				/*
				//	if (b2.get() != nullptr)
				{
				// clear any bitmap no longer needed.
				for (auto it = bitmapCache_.bitmaps_.begin(); it != bitmapCache_.bitmaps_.end(); ++it)
				{
				// Test if cache has only reference to bitmap.
				int c1 = (*it).second.Get()->addRef(); // Should increment to 2
				int c2 = (*it).second.Get()->release(); // should decrement to 1.
				if (c1 == 2 && c2 == 1)
				{
				bitmapCache_.bitmaps_.erase(it);
				break;
				}
				}

				bitmapCache_.bitmaps_.push_back(std::pair< std::string, GmpiDrawing::Bitmap >(utf8Uri, b2.get()));
				}
				*/

				b2->queryInterface(GmpiDrawing_API::SE_IID_BITMAP_MPGUI, (void**)returnDiBitmap);

				return *returnDiBitmap != 0 ? (gmpi::MP_OK) : (gmpi::MP_FAIL);
			}

			virtual int32_t MP_STDCALL CreateStrokeStyle(const GmpiDrawing_API::MP1_STROKE_STYLE_PROPERTIES* strokeStyleProperties, float* dashes, int32_t dashesCount, GmpiDrawing_API::IMpStrokeStyle** returnValue) override
			{
				*returnValue = nullptr;

				gmpi_sdk::mp_shared_ptr<gmpi::IMpUnknown> b2;
				b2.Attach(new gmpi::generic_graphics::StrokeStyle(this, strokeStyleProperties, dashes, dashesCount));

				return b2->queryInterface(GmpiDrawing_API::SE_IID_STROKESTYLE_MPGUI, reinterpret_cast<void **>(returnValue));
			}

			GMPI_QUERYINTERFACE1(GmpiDrawing_API::SE_IID_FACTORY_MPGUI, GmpiDrawing_API::IMpFactory);
			GMPI_REFCOUNT_NO_DELETE;
		};

		class GraphicsContext_base : public GmpiDrawing_API::IMpDeviceContext
		{
		protected:
			Gdiplus::Graphics* GdiPlusContext_;
			GmpiDrawing_API::IMpFactory* factory;
			std::vector<GmpiDrawing_API::MP1_RECT> clipRectStack;

			// convert a brush to a GDI+ color
			// for bitmap and gradient brushes, just return black.
			Gdiplus::Color brushToColor(const GmpiDrawing_API::IMpBrush* brush)
			{
				Gdiplus::Color c = Gdiplus::Color::Black;
				auto solidBrush = const_cast<PGCC_solidColorBrush*>(dynamic_cast<const PGCC_solidColorBrush*>(brush));
				if (solidBrush)
				{
					solidBrush->Brush.GetColor(&c);
				}

				return c;
			}

			inline Gdiplus::LineCap toNative(GmpiDrawing_API::MP1_CAP_STYLE capStyle) const
			{
				switch (capStyle)
				{
				default:
				case GmpiDrawing_API::MP1_CAP_STYLE_FLAT:
					return Gdiplus::LineCap::LineCapFlat;
					break;

				case GmpiDrawing_API::MP1_CAP_STYLE_SQUARE:
					return Gdiplus::LineCap::LineCapSquare;
					break;

				case GmpiDrawing_API::MP1_CAP_STYLE_ROUND:
					return Gdiplus::LineCap::LineCapRound;
					break;

				case GmpiDrawing_API::MP1_CAP_STYLE_TRIANGLE:
					return Gdiplus::LineCap::LineCapTriangle;
					break;
				}
			}

			void SetNativePenStrokeStyle(Gdiplus::Pen& pen, GmpiDrawing_API::IMpStrokeStyle* strokeStyle) const
			{
				if (strokeStyle)
				{
					pen.SetEndCap(toNative(strokeStyle->GetEndCap()));
					pen.SetStartCap(toNative(strokeStyle->GetStartCap()));

					Gdiplus::DashCap nativeDashCap;

					switch (strokeStyle->GetDashCap())
					{
					default:
					case GmpiDrawing_API::MP1_CAP_STYLE_SQUARE:
					case GmpiDrawing_API::MP1_CAP_STYLE_FLAT:
						nativeDashCap = Gdiplus::DashCap::DashCapFlat;
						break;

					case GmpiDrawing_API::MP1_CAP_STYLE_ROUND:
						nativeDashCap = Gdiplus::DashCap::DashCapRound;
						break;

					case GmpiDrawing_API::MP1_CAP_STYLE_TRIANGLE:
						nativeDashCap = Gdiplus::DashCap::DashCapTriangle;
						break;
					}

					pen.SetDashCap(nativeDashCap);
				}
			}

		public:
			GraphicsContext_base(Gdiplus::Graphics* dc, GmpiDrawing_API::IMpFactory* pfactory) :
				GdiPlusContext_(dc)
				, factory(pfactory)
			{
				const float defaultClipBounds = 100000.0f;
				GmpiDrawing_API::MP1_RECT r;
				r.top = r.left = -defaultClipBounds;
				r.bottom = r.right = defaultClipBounds;
				clipRectStack.push_back(r);
			}

			// GmpiDrawing_API::IMpDeviceContext
			virtual void MP_STDCALL GetFactory(GmpiDrawing_API::IMpFactory **returnFactory) override
			{
				*returnFactory = factory;
			}

			//	virtual int32_t MP_STDCALL CreateBitmap(GmpiDrawing_API::MP1_SIZE_U size, const GmpiDrawing_API::MP1_BITMAP_PROPERTIES* bitmapProperties, GmpiDrawing_API::IMpBitmap** bitmap) override;
			virtual int32_t MP_STDCALL CreateSolidColorBrush(const GmpiDrawing_API::MP1_COLOR* color, GmpiDrawing_API::IMpSolidColorBrush **solidColorBrush) override;
			virtual void MP_STDCALL DrawRectangle(const GmpiDrawing_API::MP1_RECT* rect, const GmpiDrawing_API::IMpBrush* brush, float strokeWidth, const GmpiDrawing_API::IMpStrokeStyle* strokeStyle) override;
			virtual void MP_STDCALL FillRectangle(const GmpiDrawing_API::MP1_RECT* rect, const GmpiDrawing_API::IMpBrush* brush) override;
			virtual void MP_STDCALL DrawLine(GmpiDrawing_API::MP1_POINT point0, GmpiDrawing_API::MP1_POINT point1, const GmpiDrawing_API::IMpBrush* brush, float strokeWidth, const GmpiDrawing_API::IMpStrokeStyle* strokeStyle) override;
			virtual void MP_STDCALL DrawGeometry(const GmpiDrawing_API::IMpPathGeometry* geometry, const GmpiDrawing_API::IMpBrush* brush, float strokeWidth, const GmpiDrawing_API::IMpStrokeStyle* strokeStyle) override;
			virtual void MP_STDCALL DrawTextU(const char* utf8String, int32_t stringLength, const GmpiDrawing_API::IMpTextFormat* textFormat, const GmpiDrawing_API::MP1_RECT* layoutRect, const GmpiDrawing_API::IMpBrush* brush, int32_t flags) override;

			virtual void MP_STDCALL DrawBitmap(const GmpiDrawing_API::IMpBitmap* bitmap, const GmpiDrawing_API::MP1_RECT* destinationRectangle, float opacity, /* MP1_BITMAP_INTERPOLATION_MODE*/ int32_t interpolationMode, const GmpiDrawing_API::MP1_RECT* sourceRectangle) override
			{
				auto bm = static_cast<const PGCC_bitmap_base*>(bitmap);

				auto dest = Gdiplus::RectF(destinationRectangle->left, destinationRectangle->top, (destinationRectangle->right - destinationRectangle->left), (destinationRectangle->bottom - destinationRectangle->top));

				GdiPlusContext_->DrawImage((Gdiplus::Image*) ((PGCC_bitmap_base*)bm)->getNativeBitmap(),
					dest,
					sourceRectangle->left,
					sourceRectangle->top,
					sourceRectangle->right - sourceRectangle->left,
					sourceRectangle->bottom - sourceRectangle->top,
					Gdiplus::UnitPixel);
			}

			virtual void MP_STDCALL SetTransform(const GmpiDrawing_API::MP1_MATRIX_3X2* transform) override
			{
				Gdiplus::Matrix t;
				t.SetElements(
				transform->_11
				,transform->_12
				,transform->_21
				,transform->_22
				,transform->_31
				,transform->_32);

				GdiPlusContext_->SetTransform(&t);
			}

			virtual void MP_STDCALL GetTransform(GmpiDrawing_API::MP1_MATRIX_3X2* transform) override
			{
				Gdiplus::Matrix t;
				GdiPlusContext_->GetTransform(&t);

				Gdiplus::REAL elements[6];
				t.GetElements(elements);

				transform->_11 = elements[0];
				transform->_12 = elements[1];
				transform->_21 = elements[2];
				transform->_22 = elements[3];
				transform->_31 = elements[4];
				transform->_32 = elements[5];
			}

			virtual void MP_STDCALL FillGeometry(const GmpiDrawing_API::IMpPathGeometry* geometry, const GmpiDrawing_API::IMpBrush* brush, const GmpiDrawing_API::IMpBrush* opacityBrush) override
			{
				auto solidBrush = (hasGdiPlusBrush*) dynamic_cast<const hasGdiPlusBrush*>(brush);

				if (solidBrush->native())
				{
					auto pg = (PathGeometry*)geometry;
					if (pg)
					{
						GdiPlusContext_->FillPath(solidBrush->native(), &(pg->native));
					}
				}
			}

			//	virtual void MP_STDCALL InsetNewMethodHere() {}

			int32_t MP_STDCALL CreateBitmapBrush(const GmpiDrawing_API::IMpBitmap* bitmap, const GmpiDrawing_API::MP1_BITMAP_BRUSH_PROPERTIES* bitmapBrushProperties, const GmpiDrawing_API::MP1_BRUSH_PROPERTIES* brushProperties, GmpiDrawing_API::IMpBitmapBrush** bitmapBrush) override;

			virtual int32_t MP_STDCALL CreateGradientStopCollection(const GmpiDrawing_API::MP1_GRADIENT_STOP* gradientStops, uint32_t gradientStopsCount/*, GmpiDrawing_API::MP1_GAMMA colorInterpolationGamma, GmpiDrawing_API::MP1_EXTEND_MODE extendMode*/, GmpiDrawing_API::IMpGradientStopCollection** gradientStopCollection) override
			{
				*gradientStopCollection = nullptr;

				gmpi_sdk::mp_shared_ptr<gmpi::IMpUnknown> b2;
				b2.Attach(new GradientStopCollection(gradientStops, gradientStopsCount));
				b2->queryInterface(GmpiDrawing_API::SE_IID_GRADIENTSTOPCOLLECTION_MPGUI, reinterpret_cast<void**>(gradientStopCollection));

				return b2 != 0 ? (gmpi::MP_OK) : (gmpi::MP_FAIL);
			}

			virtual int32_t MP_STDCALL CreateLinearGradientBrush(const GmpiDrawing_API::MP1_LINEAR_GRADIENT_BRUSH_PROPERTIES* linearGradientBrushProperties, const GmpiDrawing_API::MP1_BRUSH_PROPERTIES* brushProperties, const GmpiDrawing_API::IMpGradientStopCollection* gradientStopCollection, GmpiDrawing_API::IMpLinearGradientBrush** linearGradientBrush) override
			{
				*linearGradientBrush = nullptr;

				gmpi_sdk::mp_shared_ptr<gmpi::IMpUnknown> b2;
				b2.Attach(new PGCC_LinearGradientBrush(linearGradientBrushProperties, brushProperties, (GmpiDrawing_API::IMpGradientStopCollection *) gradientStopCollection));
				b2->queryInterface(GmpiDrawing_API::SE_IID_LINEARGRADIENTBRUSH_MPGUI, reinterpret_cast<void**>(linearGradientBrush));

				return b2 != 0 ? (gmpi::MP_OK) : (gmpi::MP_FAIL);
			}

			int32_t MP_STDCALL CreateRadialGradientBrush(
				const GmpiDrawing_API::MP1_RADIAL_GRADIENT_BRUSH_PROPERTIES* radialGradientBrushProperties,
				const GmpiDrawing_API::MP1_BRUSH_PROPERTIES* brushProperties,
				const GmpiDrawing_API::IMpGradientStopCollection* gradientStopCollection,
				GmpiDrawing_API::IMpRadialGradientBrush** radialGradientBrush
			) override;

			//int32_t MP_STDCALL CreateMesh(GmpiDrawing_API::IMpMesh** returnObject) override
			//{
			//	*returnObject = nullptr;
			//	return MP_FAIL;
			//}

			//virtual void MP_STDCALL FillMesh(const GmpiDrawing_API::IMpMesh* mesh, const GmpiDrawing_API::IMpBrush* brush) override
			//{
			//}

			virtual void MP_STDCALL DrawRoundedRectangle(const GmpiDrawing_API::MP1_ROUNDED_RECT* roundedRect, const GmpiDrawing_API::IMpBrush* brush, float strokeWidth, const GmpiDrawing_API::IMpStrokeStyle* strokeStyle) override
			{
				GmpiDrawing::Factory factory(factory);
				auto geometry = gdiplus::createRoundedRectangleGeometry(factory, roundedRect);
				DrawGeometry(geometry.Get(), brush, strokeWidth, strokeStyle);
			}

			virtual void MP_STDCALL FillRoundedRectangle(const GmpiDrawing_API::MP1_ROUNDED_RECT* roundedRect, const GmpiDrawing_API::IMpBrush* brush) override
			{
				GmpiDrawing::Factory factory(factory);
				auto geometry = gdiplus::createRoundedRectangleGeometry(factory, roundedRect);
				FillGeometry(geometry.Get(), brush, nullptr);
			}

			virtual void MP_STDCALL DrawEllipse(const GmpiDrawing_API::MP1_ELLIPSE* ellipse, const GmpiDrawing_API::IMpBrush* brush, float strokeWidth, const GmpiDrawing_API::IMpStrokeStyle* strokeStyle) override
			{
				auto c = brushToColor(brush);
				Gdiplus::Pen myPen(c, strokeWidth);
				SetNativePenStrokeStyle(myPen, (GmpiDrawing_API::IMpStrokeStyle*) strokeStyle);
				GdiPlusContext_->DrawEllipse(&myPen, ellipse->point.x - ellipse->radiusX, ellipse->point.y - ellipse->radiusY, ellipse->radiusX * 2.0f, ellipse->radiusY * 2.0f);
			}

			virtual void MP_STDCALL FillEllipse(const GmpiDrawing_API::MP1_ELLIPSE* ellipse, const GmpiDrawing_API::IMpBrush* brush) override
			{
				auto solidBrush = ((hasGdiPlusBrush*) dynamic_cast<const hasGdiPlusBrush*>(brush))->native();
				if (solidBrush)
				{
					GdiPlusContext_->FillEllipse(solidBrush, ellipse->point.x - ellipse->radiusX, ellipse->point.y - ellipse->radiusY, ellipse->radiusX * 2.0f, ellipse->radiusY * 2.0f);
				}
			}

			virtual void MP_STDCALL Clear(const GmpiDrawing_API::MP1_COLOR* clearColor) override
			{
				GmpiDrawing::Rect r(0, 0, 100000, 1000000);
				GmpiDrawing_API::IMpSolidColorBrush* brush;
				CreateSolidColorBrush(clearColor, &brush);
				FillRectangle(&r, brush);
				brush->release();
			}

			void applyCombinedClipRect()
			{
				if (clipRectStack.size() <= 1)
				{
					GdiPlusContext_->ResetClip();
				}
				else
				{
					GmpiDrawing::Rect compositClipRect(-100000, -100000, 100000, 100000);

					for (auto &r : clipRectStack)
					{
						compositClipRect.Intersect(r);
					}

					auto r = Gdiplus::RectF(compositClipRect.left, compositClipRect.top, (compositClipRect.right - compositClipRect.left), (compositClipRect.bottom - compositClipRect.top));
					GdiPlusContext_->SetClip(r);
				}
			}

			virtual void MP_STDCALL PushAxisAlignedClip(const GmpiDrawing_API::MP1_RECT* clipRect/*, GmpiDrawing_API::MP1_ANTIALIAS_MODE antialiasMode*/) override
			{
				clipRectStack.push_back(*clipRect);
				applyCombinedClipRect();
			}

			virtual void MP_STDCALL PopAxisAlignedClip() override
			{
				clipRectStack.pop_back();
				applyCombinedClipRect();
			}

			virtual void MP_STDCALL GetAxisAlignedClip(GmpiDrawing_API::MP1_RECT* returnClipRect)
			{
				*returnClipRect = clipRectStack.back();
			}

			virtual void MP_STDCALL BeginDraw() override
			{
			}

			virtual int32_t MP_STDCALL EndDraw() override
			{
				return gmpi::MP_OK;
			}
/*
			virtual int32_t MP_STDCALL GetUpdateRegion(GmpiDrawing_API::IUpdateRegion** returnUpdateRegion) override
			{
//				*returnUpdateRegion = updateRegion_;
				assert(false);
				return gmpi::MP_NOSUPPORT;
			}
*/

		public:
			virtual int32_t MP_STDCALL queryInterface(const gmpi::MpGuid& iid, void** returnInterface)
			{
				*returnInterface = 0;

				if (iid == GmpiDrawing_API::SE_IID_DEVICECONTEXT_MPGUI || iid == gmpi::MP_IID_UNKNOWN)
				{
					*returnInterface = static_cast<GmpiDrawing_API::IMpDeviceContext*>(this);
					addRef();
					return gmpi::MP_OK;
				}

				//if (iid == GmpiDrawing_API::SE_IID_GRAPHICS_FACTORY || iid == gmpi::MP_IID_UNKNOWN )
				//{
				//	*returnInterface = static_cast<GmpiDrawing_API::IMpFactory*>(this);
				//	addRef();
				//	return gmpi::MP_OK;
				//}

				return gmpi::MP_NOSUPPORT;
			}
		};


		class GraphicsContext : public GraphicsContext_base
		{
		public:
			GraphicsContext(Gdiplus::Graphics* dc, GmpiDrawing_API::IMpFactory* pfactory) : //, GmpiDrawing_API::IUpdateRegion* updateRegion) :
				GraphicsContext_base(dc, pfactory)
			{
			}

			virtual int32_t MP_STDCALL CreateCompatibleRenderTarget(const GmpiDrawing_API::MP1_SIZE* desiredSize, GmpiDrawing_API::IMpBitmapRenderTarget** bitmapRenderTarget) override;

			GMPI_REFCOUNT_NO_DELETE
		};

		class BitmapRenderTarget : public GraphicsContext
		{
		public:
			Gdiplus::Bitmap bitmap_;

			BitmapRenderTarget(const GmpiDrawing_API::MP1_SIZE* desiredSize, Gdiplus::Graphics* GdiPlusContext, GmpiDrawing_API::IMpFactory* pfactory) :
				GraphicsContext(0, pfactory)
				, bitmap_(static_cast<INT>(desiredSize->width), static_cast<INT>(desiredSize->height), GdiPlusContext)
			{
				GdiPlusContext_ = Gdiplus::Graphics::FromImage(&bitmap_);
			}

			~BitmapRenderTarget()
			{
				delete GdiPlusContext_;
			}

			virtual int32_t MP_STDCALL GetBitmap(GmpiDrawing_API::IMpBitmap** returnBitmap);
			/*
			int GetEncoderClsid(const WCHAR* format, CLSID* pClsid)
			{
			UINT  num = 0;          // number of image encoders
			UINT  size = 0;         // size of the image encoder array in bytes

			Gdiplus::ImageCodecInfo* pImageCodecInfo = NULL;

			Gdiplus::GetImageEncodersSize(&num, &size);
			if (size == 0)
			return -1;  // Failure

			pImageCodecInfo = (Gdiplus::ImageCodecInfo*)(malloc(size));
			if (pImageCodecInfo == NULL)
			return -1;  // Failure

			GetImageEncoders(num, size, pImageCodecInfo);

			for (UINT j = 0; j < num; ++j)
			{
			if (wcscmp(pImageCodecInfo[j].MimeType, format) == 0)
			{
			*pClsid = pImageCodecInfo[j].Clsid;
			free(pImageCodecInfo);
			return j;  // Success
			}
			}

			free(pImageCodecInfo);
			return -1;  // Failure
			}

			virtual void MP_STDCALL Clear(const GmpiDrawing_API::MP1_COLOR* clearColor) override
			{
			PortableGraphicsContext_SE_Composited::Clear(clearColor);

			CLSID pngClsid;
			GetEncoderClsid(L"image/png", &pngClsid);
			bitmap_.Save(L"C:\\temp\\test.bmp", &pngClsid);
			}
			*/

			//	GMPI_QUERYINTERFACE1(GmpiDrawing_API::SE_IID_BITMAP_RENDERTARGET_MPGUI, GmpiDrawing_API::IMpBitmapRenderTarget);
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
	}
}