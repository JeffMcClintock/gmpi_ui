#include "GdiPlusGfx.h"

namespace gmpi
{
	namespace gdiplus
	{
		PGCC_textFormat::PGCC_textFormat(const char* fontFamilyName, int fontWeight, int fontStyle, int fontStretch, float fontSize, float pDpiScaling) :
			_fontFamilyName(fontFamilyName)
			, _fontWeight(fontWeight)
			, _fontStyle(fontStyle)
			, _fontStretch(fontStretch)
			, dpiScaling(pDpiScaling)
			, cachedGdiPlusFontPadding(-100, -100)
		{
			fontSize *= dpiScaling;

			//	_RPT3(_CRT_WARN, "new PGCC_textFormat %x (%s %f)\n", this, fontFamilyName, fontSize);
			//		_stringFormat.SetTrimming(Gdiplus::StringTrimmingNone);
			_stringFormat.SetFormatFlags(defaultFlags); // Don't clip to rect like DirectDraw.
			_stringFormat.SetAlignment(Gdiplus::StringAlignmentNear); //top
																	  // Check cache first.
			std::wstring fontFamilyNameW = Utf8ToWstring(fontFamilyName);
			for(auto it = fontCache_.fonts_.begin(); it != fontCache_.fonts_.end(); ++it)
			{
				if((*it)->fontFamilyName_ == fontFamilyNameW && (*it)->fontSize_ == fontSize)
				{
					fontHolder_ = *it;
					return;
				}
			}

			int gdiplusFontStyle = Gdiplus::FontStyle::FontStyleRegular;

			if(_fontWeight >= GmpiDrawing_API::MP1_FONT_WEIGHT_DEMI_BOLD) // gmpi_gui::Font::Weight::W_SemiBold)
			{
				gdiplusFontStyle = Gdiplus::FontStyle::FontStyleBold;
			}

			if(_fontStyle == GmpiDrawing_API::MP1_FONT_STYLE_ITALIC)// gmpi_gui::Font::Style::S_Italic)
			{
				gdiplusFontStyle |= Gdiplus::FontStyle::FontStyleItalic;
			}

			fontHolder_.Attach(new fontObject(fontFamilyNameW.c_str(), fontSize, gdiplusFontStyle));
			//	fontHolder_->add Ref(); // my reference.

			// clear any cached objects no longer needed.
			for(auto it = fontCache_.fonts_.begin(); it != fontCache_.fonts_.end(); ++it)
			{
				// Test if cache has only reference to bitmap.
				int c1 = (*it)->addRef(); // Should increment to 2
				int c2 = (*it)->release(); // should decrement to 1.
				if(c1 == 2 && c2 == 1)
				{
					fontCache_.fonts_.erase(it);
					break;
				}
			}

			//	fontHolder_->add Ref(); // fontCache_ reference.
			fontCache_.fonts_.push_back(fontHolder_);
		}

		int32_t PGCC_textFormat::GetFontMetrics(GmpiDrawing_API::MP1_FONT_METRICS* returnFontMetrics)
		{
			// TODO: Could be cached. 

			GmpiDrawing::Size cellSize;
			GetTextExtentU("M", 1, &cellSize);

			Gdiplus::FontFamily ff;

			fontHolder_->font_.GetFamily(&ff);

			int style = fontHolder_->font_.GetStyle();

			float emHeight = ff.GetEmHeight(style); // Em square is a typography term that refers to the rectangle occupied by the font's widest letter, traditionally the letter M.
		//	float emToDips = /*dpiScaling * */ (96.0f / 72.0f) * fontHolder_->font_.GetSize() / (emHeight * dpiScaling);
			float emToDips = fontHolder_->font_.GetSize() / (emHeight * dpiScaling);

			returnFontMetrics->descent = ff.GetCellDescent(style) * emToDips;
			// GDI+ ascent seems to be more like DX capheight. Calc ascent from cellsize.
			returnFontMetrics->ascent = cellSize.height - returnFontMetrics->descent;
			returnFontMetrics->capHeight = emToDips * ff.GetEmHeight(style); //ff.GetCellAscent(style);
			returnFontMetrics->lineGap = 0.0f; // ff.GetLineSpacing(style) * emToDips; // vertical distance between the base lines of two consecutive lines of text. 
			returnFontMetrics->xHeight = returnFontMetrics->capHeight * 0.65f; // estimate
			returnFontMetrics->strikethroughPosition = returnFontMetrics->xHeight * 0.5f;
			returnFontMetrics->strikethroughThickness = 1.0f;
			returnFontMetrics->underlineThickness = returnFontMetrics->strikethroughThickness;
			returnFontMetrics->underlinePosition = returnFontMetrics->descent * 0.5f;

			return gmpi::MP_OK;
		}

		GmpiDrawing::Size PGCC_textFormat::getGdiPlusTextPadding()
		{
			if(cachedGdiPlusFontPadding.width >= 0.0f)
			{
				return cachedGdiPlusFontPadding;
			}

#pragma push_macro("new")
#undef new
			Gdiplus::Graphics graphics(new Gdiplus::Bitmap(1, 1));
#pragma pop_macro("new")

			graphics.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAlias);
			auto font = &(fontHolder_->font_);

			// GDI+ insists on drawing space before and after string. identify actual text padding.
			const wchar_t* text = L"M";
			Gdiplus::RectF boundingRect;
			Gdiplus::RectF constraintRect(0, 0, 10000, 10000);
			Gdiplus::CharacterRange crange(0, 1);
			Gdiplus::Region region;

			Gdiplus::StringFormat tempStringFormat;
			tempStringFormat.SetMeasurableCharacterRanges(1, &crange);
			graphics.MeasureCharacterRanges(text, (INT)1,
				font, constraintRect, &tempStringFormat, 1, &region);

			region.GetBounds(&boundingRect, &graphics);

			// identify actual required size including space.
			Gdiplus::PointF origin(0, 0);
			Gdiplus::RectF boundingRect2;
			graphics.MeasureString(text, (INT)1, font, origin, &tempStringFormat, &boundingRect2);

			cachedGdiPlusFontPadding.width = (boundingRect2.Width - boundingRect.Width) * 0.5f;
			cachedGdiPlusFontPadding.height = (float)(std::min)(4, (int)(boundingRect2.Height / 6)); //  GDI has whitespace at top. Direct2d Fits hard against top.

			return cachedGdiPlusFontPadding;
		}


		//void PortableGraphicsContext_SE_Composited::DrawTextU(const char* utf8String, int32_t stringLength, GmpiDrawing_API::IMpTextFormat* textFormat, GmpiDrawing::Rect rect, GmpiDrawing_API::IMpBrush* brush, int32_t flags)
		void GraphicsContext_base::DrawTextU(const char* utf8String, int32_t stringLength, const GmpiDrawing_API::IMpTextFormat* textFormat, const GmpiDrawing_API::MP1_RECT* layoutRect, const GmpiDrawing_API::IMpBrush* brush, int32_t flags)
		{
			std::wstring widestring = Utf8ToWstring(utf8String);
			auto tf = (PGCC_textFormat*)textFormat;
			auto stringFormat = &(tf->_stringFormat);
			auto font = &(tf->fontHolder_->font_);
			GdiPlusContext_->SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAlias);

			float averagePadding = tf->getGdiPlusTextPadding().width;
			float verticalOffset = tf->getGdiPlusTextPadding().height;

			auto r = Gdiplus::RectF(layoutRect->left - averagePadding, layoutRect->top + verticalOffset, layoutRect->right - layoutRect->left + averagePadding + averagePadding, layoutRect->bottom - layoutRect->top - verticalOffset);

			auto b = ((hasGdiPlusBrush*) dynamic_cast<const hasGdiPlusBrush*>(brush))->native();

			GdiPlusContext_->DrawString(widestring.data(), (INT)widestring.size(), font, r, stringFormat, b);
		}

		int32_t GraphicsContext_base::CreateSolidColorBrush(const GmpiDrawing_API::MP1_COLOR* color, GmpiDrawing_API::IMpSolidColorBrush** solidColorBrush)
		{
			*solidColorBrush = 0;

			gmpi_sdk::mp_shared_ptr<gmpi::IMpUnknown> b2;
			b2.Attach(new PGCC_solidColorBrush(*color));
			b2->queryInterface(GmpiDrawing_API::SE_IID_SOLIDCOLORBRUSH_MPGUI, reinterpret_cast<void**>(solidColorBrush));

			return b2 != 0 ? (gmpi::MP_OK) : (gmpi::MP_FAIL);
		}

		int32_t GraphicsContext_base::CreateBitmapBrush(const GmpiDrawing_API::IMpBitmap* bitmap, const GmpiDrawing_API::MP1_BITMAP_BRUSH_PROPERTIES* bitmapBrushProperties, const GmpiDrawing_API::MP1_BRUSH_PROPERTIES* brushProperties, GmpiDrawing_API::IMpBitmapBrush** bitmapBrush)
		{
			*bitmapBrush = nullptr;

			gmpi_sdk::mp_shared_ptr<gmpi::IMpUnknown> b2;
			b2.Attach(new PGCC_fakeBitmapBrush());
			b2->queryInterface(GmpiDrawing_API::SE_IID_BITMAPBRUSH_MPGUI, reinterpret_cast<void**>(bitmapBrush));

			return b2 != 0 ? (gmpi::MP_OK) : (gmpi::MP_FAIL);
		}

		int32_t GraphicsContext_base::CreateRadialGradientBrush(
			const GmpiDrawing_API::MP1_RADIAL_GRADIENT_BRUSH_PROPERTIES* radialGradientBrushProperties,
			const GmpiDrawing_API::MP1_BRUSH_PROPERTIES* brushProperties,
			const GmpiDrawing_API::IMpGradientStopCollection* gradientStopCollection,
			GmpiDrawing_API::IMpRadialGradientBrush** radialGradientBrush
		)
		{
			*radialGradientBrush = nullptr;

			gmpi_sdk::mp_shared_ptr<gmpi::IMpUnknown> b2;
			b2.Attach(new PGCC_fakeRadialGradientBrush());
			b2->queryInterface(GmpiDrawing_API::SE_IID_RADIALGRADIENTBRUSH_MPGUI, reinterpret_cast<void**>(radialGradientBrush));

			return b2 != 0 ? (gmpi::MP_OK) : (gmpi::MP_FAIL);
		}

		int32_t GraphicsContext::CreateCompatibleRenderTarget(const GmpiDrawing_API::MP1_SIZE* desiredSize, GmpiDrawing_API::IMpBitmapRenderTarget** bitmapRenderTarget)
		{
			*bitmapRenderTarget = nullptr;

			gmpi_sdk::mp_shared_ptr<gmpi::IMpUnknown> b2;
			b2.Attach(new BitmapRenderTarget(desiredSize, GdiPlusContext_, factory));

			b2->queryInterface(GmpiDrawing_API::SE_IID_BITMAP_RENDERTARGET_MPGUI, reinterpret_cast<void**>(bitmapRenderTarget));

			return gmpi::MP_OK;
		}

		int32_t BitmapRenderTarget::GetBitmap(GmpiDrawing_API::IMpBitmap** returnBitmap)
		{
			*returnBitmap = nullptr;

			gmpi_sdk::mp_shared_ptr<gmpi::IMpUnknown> b2;

			b2.Attach(new PGCC_bitmap2(factory, this));

			return b2->queryInterface(GmpiDrawing_API::SE_IID_BITMAP_MPGUI, reinterpret_cast<void**>(returnBitmap));
		}

		PGCC_bitmap2::PGCC_bitmap2(GmpiDrawing_API::IMpFactory* pFactory, BitmapRenderTarget* bitmapRenderTarget) :
			bitmapRenderTarget_(bitmapRenderTarget)
			, PGCC_bitmap_base(pFactory)
		{
			nativeBitmap_ = &(bitmapRenderTarget_->bitmap_);
			bitmapRenderTarget_->addRef(); // ensure owning rensercontext not deleted till I am.
		}

		PGCC_bitmap2::~PGCC_bitmap2()
		{
			bitmapRenderTarget_->release();
		}

		void PGCC_bitmap_base::GetFactory(GmpiDrawing_API::IMpFactory** rfactory)
		{
			*rfactory = factory;
		}

		void GraphicsContext_base::DrawRectangle(const GmpiDrawing_API::MP1_RECT* rect, const GmpiDrawing_API::IMpBrush* brush, float strokeWidth, const GmpiDrawing_API::IMpStrokeStyle* strokeStyle)
		{
			auto c = brushToColor(brush);
			Gdiplus::Pen myPen(c, strokeWidth);
			SetNativePenStrokeStyle(myPen, (GmpiDrawing_API::IMpStrokeStyle*) strokeStyle);
			GdiPlusContext_->DrawRectangle(&myPen, rect->left /* * dpiScale_*/, rect->top /* * dpiScale_*/, (rect->right - rect->left) /* * dpiScale_*/, (rect->bottom - rect->top) /* * dpiScale_*/);
		}

		void GraphicsContext_base::FillRectangle(const GmpiDrawing_API::MP1_RECT* rect, const GmpiDrawing_API::IMpBrush* brush)
		{
			auto solidBrush = ((hasGdiPlusBrush*) dynamic_cast<const hasGdiPlusBrush*>(brush))->native();
			if(solidBrush)
			{
				GdiPlusContext_->FillRectangle(solidBrush, rect->left /* * dpiScale_*/, rect->top /* * dpiScale_*/, (rect->right - rect->left) /* * dpiScale_*/, (rect->bottom - rect->top) /* * dpiScale_*/);
			}
		}

		void GraphicsContext_base::DrawLine(GmpiDrawing_API::MP1_POINT point0, GmpiDrawing_API::MP1_POINT point1, const GmpiDrawing_API::IMpBrush* brush, float strokeWidth, const GmpiDrawing_API::IMpStrokeStyle* strokeStyle)
		{
			auto c = brushToColor(brush);

			Gdiplus::Pen myPen(c, strokeWidth /* * dpiScale_*/);
			SetNativePenStrokeStyle(myPen, (GmpiDrawing_API::IMpStrokeStyle*)strokeStyle);

			GdiPlusContext_->DrawLine(&myPen, point0.x /* * dpiScale_*/, point0.y /* * dpiScale_*/, point1.x /* * dpiScale_*/, point1.y /* * dpiScale_*/);
		}

		void GraphicsContext_base::DrawGeometry(const GmpiDrawing_API::IMpPathGeometry* geometry, const GmpiDrawing_API::IMpBrush* brush, float strokeWidth, const GmpiDrawing_API::IMpStrokeStyle* strokeStyle)
		{
			auto pg = (PathGeometry*)geometry;
			if(pg)
			{
				auto c = brushToColor(brush);
				Gdiplus::Pen myPen(c, strokeWidth);
				SetNativePenStrokeStyle(myPen, (GmpiDrawing_API::IMpStrokeStyle*) strokeStyle);
				GdiPlusContext_->DrawPath(&myPen, &(pg->native)); // &(points[0]), (INT)points.size() );
			}
		}

		PGCC_FontCache PGCC_textFormat::fontCache_;

		// Meyer's singleton
		GdiPlusHelper* GdiPlusHelper::Instance()
		{
			static GdiPlusHelper obj; // dll safe??? !!!
			return &obj;
		}

		void GdiPlusHelper::GetTextExtentU(Gdiplus::Font* font, Gdiplus::StringFormat* stringformat, const char* utf8String, int32_t stringLength, GmpiDrawing_API::MP1_SIZE* returnSize)
		{
			std::wstring widestring = Utf8ToWstring(utf8String);

			Gdiplus::RectF boundingRect;
			Gdiplus::PointF origin(0, 0);
			graphics_.MeasureString(widestring.data(), (INT)widestring.size(), font, origin, stringformat, &boundingRect);

			int lines = 1;
			for(int i = 0; i < stringLength; ++i)
			{
				if(utf8String[i] == '\n')
				{
					++lines;
				}
			}

			returnSize->width = boundingRect.Width;
			returnSize->height = font->GetHeight(&graphics_) * lines;
			//  NOTE: boundingRect.Height includes extra spacing below decenders

			/* includes unwanted bounding whitespace
			Gdiplus::RectF constraintRect(0, 0, 100000, 100000);
			graphics_.MeasureString(widestring.data(), (INT)widestring.size(), font, constraintRect, &boundingRect);
			*/

			/* also includes whitespace
			Gdiplus::PointF origin(0, 0);
			//	graphics_.MeasureString(widestring.data(), (INT)widestring.size(), font, origin, Gdiplus::StringFormat::GenericTypographic(), &boundingRect);
			//	graphics_.MeasureString(widestring.data(), (INT)widestring.size(), font, origin, stringformat, &boundingRect);
			Gdiplus::StringFormat format;
			format.SetAlignment(Gdiplus::StringAlignmentCenter);
			graphics_.MeasureString(widestring.data(), (INT)widestring.size(), font, origin, stringformat, &boundingRect);

			returnSize.x = boundingRect.Width;
			returnSize.y = boundingRect.Height;
			*/

			//	_RPT2(_CRT_WARN, "GetTextExtentU (%f %f)\n", returnSize.x, returnSize.y);

			// EXACT method. not needed now we pre-calculate the padding  elsewhere.
			/*
			Gdiplus::RectF constraintRect(0, 0, 10000, 10000);
			Gdiplus::CharacterRange crange(0, stringLength);
			Gdiplus::Region region;

			stringformat->SetMeasurableCharacterRanges(1, &crange);
			graphics_.MeasureCharacterRanges(widestring.data(), (INT)widestring.size(),
			font, constraintRect, stringformat, 1, &region);

			region.GetBounds(&boundingRect, &graphics_);

			returnSize.x = boundingRect.Width + 1;
			returnSize.y = boundingRect.Height;
			*/
		}
	} // namespace
} // namespace
  //////////////////////////////////////////////////////////////////////////////////////
