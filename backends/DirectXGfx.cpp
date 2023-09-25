#ifdef _WIN32 // skip compilation on macOS

#include <sstream>
#include "./DirectXGfx.h"
#include "../shared/xplatform.h"
#include "../shared/xp_simd.h"
#include "../shared/fast_gamma.h"
#include "../shared/unicode_conversion.h"
#include "d2d1helper.h"

namespace gmpi
{
	namespace directx
	{
		std::wstring_convert<std::codecvt_utf8<wchar_t>> Factory::stringConverter;

		gmpi::ReturnCode Geometry::open(drawing::api::IGeometrySink** returnGeometrySink)
		{
			ID2D1GeometrySink* sink{};

			auto hr = geometry_->Open(&sink);

			if (hr == 0)
			{
				gmpi::shared_ptr<gmpi::api::IUnknown> b2;
				b2.Attach(new gmpi::directx::GeometrySink(sink));

				b2->queryInterface(&drawing::api::IGeometrySink::guid, reinterpret_cast<void**>(returnGeometrySink));
			}

			return hr == 0 ? (gmpi::ReturnCode::Ok) : (gmpi::ReturnCode::Fail);
		}

		gmpi::ReturnCode TextFormat::getFontMetrics(gmpi::drawing::FontMetrics* returnFontMetrics)
		{
			IDWriteFontCollection *collection;
			IDWriteFontFamily *family;
			IDWriteFontFace *fontface;
			IDWriteFont *font;
			WCHAR nameW[255];
			UINT32 index;
			BOOL exists;
			HRESULT hr;

			hr = native()->GetFontCollection(&collection);
			//	ok(hr == S_OK, "got 0x%08x\n", hr);

			hr = native()->GetFontFamilyName(nameW, sizeof(nameW) / sizeof(WCHAR));
			//	ok(hr == S_OK, "got 0x%08x\n", hr);

			hr = collection->FindFamilyName(nameW, &index, &exists);
			if (exists == 0) // font not available. Fallback.
			{
				index = 0;
			}

			hr = collection->GetFontFamily(index, &family);
			//	ok(hr == S_OK, "got 0x%08x\n", hr);
			collection->Release();

			hr = family->GetFirstMatchingFont(
				native()->GetFontWeight(),
				native()->GetFontStretch(),
				native()->GetFontStyle(),
				&font);
			//	ok(hr == S_OK, "got 0x%08x\n", hr);

			hr = font->CreateFontFace(&fontface);
			//	ok(hr == S_OK, "got 0x%08x\n", hr);

			font->Release();
			family->Release();

			DWRITE_FONT_METRICS metrics;
			fontface->GetMetrics(&metrics);
			fontface->Release();

			// Sizes returned must always be in DIPs.
			float emsToDips = native()->GetFontSize() / metrics.designUnitsPerEm;

			returnFontMetrics->ascent = emsToDips * metrics.ascent;
			returnFontMetrics->descent = emsToDips * metrics.descent;
			returnFontMetrics->lineGap = emsToDips * metrics.lineGap;
			returnFontMetrics->capHeight = emsToDips * metrics.capHeight;
			returnFontMetrics->xHeight = emsToDips * metrics.xHeight;
			returnFontMetrics->underlinePosition = emsToDips * metrics.underlinePosition;
			returnFontMetrics->underlineThickness = emsToDips * metrics.underlineThickness;
			returnFontMetrics->strikethroughPosition = emsToDips * metrics.strikethroughPosition;
			returnFontMetrics->strikethroughThickness = emsToDips * metrics.strikethroughThickness;

			return gmpi::ReturnCode::Ok;
		}

		gmpi::ReturnCode TextFormat::getTextExtentU(const char* utf8String, int32_t stringLength, gmpi::drawing::Size* returnSize)
		{
			const auto widestring = JmUnicodeConversions::Utf8ToWstring(utf8String, stringLength);

			IDWriteFactory* writeFactory = 0;
			auto hr = DWriteCreateFactory(
				DWRITE_FACTORY_TYPE_SHARED,
				__uuidof(writeFactory),
				reinterpret_cast<::IUnknown **>(&writeFactory)
			);

			IDWriteTextLayout* pTextLayout_ = 0;

			hr = writeFactory->CreateTextLayout(
				widestring.data(),      // The string to be laid out and formatted.
				(UINT32)widestring.size(),  // The length of the string.
				native(),  // The text format to apply to the string (contains font information, etc).
				100000,         // The width of the layout box.
				100000,        // The height of the layout box.
				&pTextLayout_  // The IDWriteTextLayout interface pointer.
			);

			DWRITE_TEXT_METRICS textMetrics;
			pTextLayout_->GetMetrics(&textMetrics);

			returnSize->height = textMetrics.height;
			returnSize->width = textMetrics.widthIncludingTrailingWhitespace;

			if (!useLegacyBaseLineSnapping)
			{
				returnSize->height -= topAdjustment;
			}

			SafeRelease(pTextLayout_);
			SafeRelease(writeFactory);
			return gmpi::ReturnCode::Ok;
		}

		// Create factory myself;
		Factory::Factory() :
			writeFactory(nullptr)
			, pIWICFactory(nullptr)
			, m_pDirect2dFactory(nullptr)
			, DX_support_sRGB(true)
		{
		}

		void Factory::Init(ID2D1Factory1* existingFactory)
		{
			if (existingFactory)
			{
				m_pDirect2dFactory = existingFactory;
				m_pDirect2dFactory->AddRef();
			}
			else
			{
				D2D1_FACTORY_OPTIONS o;
#ifdef _DEBUG
				o.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;// D2D1_DEBUG_LEVEL_WARNING; // Need to install special stuff. https://msdn.microsoft.com/en-us/library/windows/desktop/ee794278%28v=vs.85%29.aspx?f=255&MSPPError=-2147217396 
#else
				o.debugLevel = D2D1_DEBUG_LEVEL_NONE;
#endif
//				auto rs = D2D1CreateFactory(D2D1_FACTORY_TYPE_MULTI_THREADED, __uuidof(ID2D1Factory1), &o, (void**)&m_pDirect2dFactory);
				auto rs = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory1), &o, (void**)&m_pDirect2dFactory);

#ifdef _DEBUG
				if (FAILED(rs))
				{
					o.debugLevel = D2D1_DEBUG_LEVEL_NONE; // fallback
					rs = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory1), &o, (void**)&m_pDirect2dFactory);
				}
#endif
				if (FAILED(rs))
				{
					_RPT1(_CRT_WARN, "D2D1CreateFactory FAIL %d\n", rs);
					return;  // Fail.
				}
				//		_RPT2(_CRT_WARN, "D2D1CreateFactory OK %d : %x\n", rs, m_pDirect2dFactory);
			}

			writeFactory = nullptr;

			auto hr = DWriteCreateFactory(
				DWRITE_FACTORY_TYPE_SHARED, // no improvment to glitching DWRITE_FACTORY_TYPE_ISOLATED
				__uuidof(writeFactory),
				reinterpret_cast<::IUnknown**>(&writeFactory)
			);

			pIWICFactory = nullptr;

			hr = CoCreateInstance(
				CLSID_WICImagingFactory,
				NULL,
				CLSCTX_INPROC_SERVER,
				IID_IWICImagingFactory,
				(LPVOID*)&pIWICFactory
			);

			// Cache font family names
			{
				// TODO IDWriteFontSet is improved API, GetSystemFontSet()

				IDWriteFontCollection* fonts = nullptr;
				writeFactory->GetSystemFontCollection(&fonts, TRUE);

				auto count = fonts->GetFontFamilyCount();

				for (int index = 0; index < (int)count; ++index)
				{
					IDWriteFontFamily* family = nullptr;
					fonts->GetFontFamily(index, &family);

					IDWriteLocalizedStrings* names = nullptr;
					family->GetFamilyNames(&names);

					BOOL exists;
					unsigned int nameIndex;
					names->FindLocaleName(L"en-us", &nameIndex, &exists);
					if (exists)
					{
						wchar_t name[64];
						names->GetString(nameIndex, name, sizeof(name) / sizeof(name[0]));

						supportedFontFamilies.push_back(stringConverter.to_bytes(name));

						std::transform(name, name + wcslen(name), name, [](wchar_t c) { return static_cast<wchar_t>(std::tolower(c)); });
						supportedFontFamiliesLowerCase.push_back(name);
					}

					names->Release();
					family->Release();
				}

				fonts->Release();
			}
#if 0
			// test matrix rotation calc
			for (int rot = 0; rot < 8; ++rot)
			{
				const float angle = (rot / 8.f) * 2.f * 3.14159274101257324219f;
				auto test = drawing::Matrix3x2::Rotation(angle, { 23, 7 });
				auto test2 = D2D1::Matrix3x2F::Rotation(angle * 180.f / 3.14159274101257324219f, { 23, 7 });

				_RPTN(0, "\nangle=%f\n", angle);
				_RPTN(0, "%f, %f\n", test._11, test._12);
				_RPTN(0, "%f, %f\n", test._21, test._22);
				_RPTN(0, "%f, %f\n", test._31, test._32);
		}

			// test matrix scaling
			const auto test = drawing::Matrix3x2::Scale({ 3, 5 }, { 7, 9 });
			const auto test2 = D2D1::Matrix3x2F::Scale({ 3, 5 }, { 7, 9 });
			const auto breakpointer = test._11 + test2._11;
#endif
		}

		Factory::~Factory()
		{ 
			SafeRelease(m_pDirect2dFactory);
			SafeRelease(writeFactory);
			SafeRelease(pIWICFactory);
		}

		gmpi::ReturnCode Factory::createPathGeometry(gmpi::drawing::api::IPathGeometry** pathGeometry)
		{
			*pathGeometry = nullptr;
			//*pathGeometry = new GmpiGuiHosting::PathGeometry();
			//return gmpi::ReturnCode::Ok;

			ID2D1PathGeometry* d2d_geometry = nullptr;
			HRESULT hr = m_pDirect2dFactory->CreatePathGeometry(&d2d_geometry);

			if (hr == 0)
			{
				gmpi::shared_ptr<gmpi::api::IUnknown> b2;
				b2.Attach(new gmpi::directx::Geometry(d2d_geometry));

				b2->queryInterface(&drawing::api::IPathGeometry::guid, reinterpret_cast<void**>(pathGeometry));

#ifdef LOG_DIRECTX_CALLS
				_RPT1(_CRT_WARN, "ID2D1PathGeometry* geometry%x = nullptr;\n", (int)*pathGeometry);
				_RPT0(_CRT_WARN, "{\n");
				_RPT1(_CRT_WARN, "factory->CreatePathGeometry(&geometry%x);\n", (int)*pathGeometry);
				_RPT0(_CRT_WARN, "}\n");
#endif
			}

			return hr == 0 ? (gmpi::ReturnCode::Ok) : (gmpi::ReturnCode::Fail);
		}

		gmpi::ReturnCode Factory::createTextFormat(const char* fontFamilyName, /* void* unused fontCollection ,*/ gmpi::drawing::FontWeight fontWeight, gmpi::drawing::FontStyle fontStyle, gmpi::drawing::FontStretch fontStretch, float fontSize, /* void* unused2 localeName, */ gmpi::drawing::api::ITextFormat** textFormat)
		{
			*textFormat = nullptr;

			//auto fontFamilyNameW = stringConverter.from_bytes(fontFamilyName);
			auto fontFamilyNameW = JmUnicodeConversions::Utf8ToWstring(fontFamilyName);
			std::wstring lowercaseName(fontFamilyNameW);
			std::transform(lowercaseName.begin(), lowercaseName.end(), lowercaseName.begin(), [](wchar_t c) { return static_cast<wchar_t>(std::tolower(c)); });

			if (std::find(supportedFontFamiliesLowerCase.begin(), supportedFontFamiliesLowerCase.end(), lowercaseName) == supportedFontFamiliesLowerCase.end())
			{
				fontFamilyNameW = fontMatch(fontFamilyNameW, fontWeight, fontSize);
			}

			IDWriteTextFormat* dwTextFormat = nullptr;

			auto hr = writeFactory->CreateTextFormat(
				fontFamilyNameW.c_str(),
				NULL,
				(DWRITE_FONT_WEIGHT)fontWeight,
				(DWRITE_FONT_STYLE)fontStyle,
				(DWRITE_FONT_STRETCH)fontStretch,
				fontSize,
				L"", //locale
				&dwTextFormat
			);

			if (hr == 0)
			{
				gmpi::shared_ptr<gmpi::api::IUnknown> b2;
				b2.Attach(new gmpi::directx::TextFormat(&stringConverter, dwTextFormat));

				b2->queryInterface(&drawing::api::ITextFormat::guid, reinterpret_cast<void**>(textFormat));

//				_RPT2(_CRT_WARN, "factory.CreateTextFormat() -> %x %S\n", (int)dwTextFormat, fontFamilyNameW.c_str());
#ifdef LOG_DIRECTX_CALLS
//				_RPT4(_CRT_WARN, "auto c = D2D1::ColorF(%.3f, %.3f, %.3f, %.3f);\n", color->r, color->g, color->b, color->a);
				_RPT1(_CRT_WARN, "IDWriteTextFormat* textformat%x = nullptr;\n", (int)*TextFormat);
				_RPT4(_CRT_WARN, "writeFactory->CreateTextFormat(L\"%S\",NULL, (DWRITE_FONT_WEIGHT)%d, (DWRITE_FONT_STYLE)%d, DWRITE_FONT_STRETCH_NORMAL, %f, L\"\",", fontFamilyNameW.c_str(), fontWeight, fontStyle, fontSize);
				_RPT1(_CRT_WARN, "&textformat%x);\n", (int)*TextFormat);
#endif
			}

			return hr == 0 ? (gmpi::ReturnCode::Ok) : (gmpi::ReturnCode::Fail);
		}

		// 2nd pass - GDI->DirectWrite conversion. "Arial Black" -> "Arial"
		std::wstring Factory::fontMatch(std::wstring fontFamilyNameW, gmpi::drawing::FontWeight fontWeight, float fontSize)
		{
			auto it = GdiFontConversions.find(fontFamilyNameW);
			if (it != GdiFontConversions.end())
			{
				return (*it).second;
			}

			IDWriteGdiInterop* interop = nullptr;
			writeFactory->GetGdiInterop(&interop);

			LOGFONT lf;
			memset(&lf, 0, sizeof(LOGFONT));   // Clear out structure.
			lf.lfHeight = (LONG) -fontSize;
			lf.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
			const wchar_t* actual_facename = fontFamilyNameW.c_str();

			if (fontFamilyNameW == L"serif")
			{
				actual_facename = L"Times New Roman";
				lf.lfPitchAndFamily = DEFAULT_PITCH | FF_ROMAN;
			}

			if (fontFamilyNameW == L"sans-serif")
			{
				//		actual_facename = _L"Helvetica");
				actual_facename = L"Arial"; // available on all version of windows
				lf.lfPitchAndFamily = DEFAULT_PITCH | FF_SWISS;
			}

			if (fontFamilyNameW == L"cursive")
			{
				actual_facename = L"Zapf-Chancery";
				lf.lfPitchAndFamily = DEFAULT_PITCH | FF_SCRIPT;
			}

			if (fontFamilyNameW == L"fantasy")
			{
				actual_facename = L"Western";
				lf.lfPitchAndFamily = DEFAULT_PITCH | FF_DECORATIVE;
			}

			if (fontFamilyNameW == L"monospace")
			{
				actual_facename = L"Courier New";
				lf.lfPitchAndFamily = DEFAULT_PITCH | FF_MODERN;
			}
			wcscpy_s(lf.lfFaceName, 32, actual_facename);
			/*
			if ((p_desc->flags & TTL_UNDERLINE) != 0)
			{
			lf.lfUnderline = 1;
			}
			*/
			if (fontWeight > gmpi::drawing::FontWeight::SemiBold)
			{
				lf.lfWeight = FW_BOLD;
			}
			else
			{
				if ((int) fontWeight < 350)
				{
					lf.lfWeight = FW_LIGHT;
				}
			}

			IDWriteFont* font = nullptr;
			auto hr = interop->CreateFontFromLOGFONT(&lf, &font);

			if (font && hr == 0)
			{
				IDWriteFontFamily* family = nullptr;
				font->GetFontFamily(&family);

				IDWriteLocalizedStrings* names = nullptr;
				family->GetFamilyNames(&names);

				BOOL exists;
				unsigned int nameIndex;
				names->FindLocaleName(L"en-us", &nameIndex, &exists);
				if (exists)
				{
					wchar_t name[64];
					names->GetString(nameIndex, name, sizeof(name) / sizeof(name[0]));
					std::transform(name, name + wcslen(name), name, [](wchar_t c) { return static_cast<wchar_t>(std::tolower(c));});

					//						supportedFontFamiliesLowerCase.push_back(name);
					GdiFontConversions.insert(std::pair<std::wstring, std::wstring>(fontFamilyNameW, name));
					fontFamilyNameW = name;
				}

				names->Release();
				family->Release();

				font->Release();
			}

			interop->Release();
			return fontFamilyNameW;
		}

		gmpi::ReturnCode Factory::createImage(int32_t width, int32_t height, gmpi::drawing::api::IBitmap** returnDiBitmap)
		{
			IWICBitmap* wicBitmap{};
			auto hr = pIWICFactory->CreateBitmap(width, height, GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnLoad, &wicBitmap); // pre-muliplied alpha
	// nuh	auto hr = pIWICFactory->CreateBitmap(width, height, GUID_WICPixelFormat32bppBGRA, WICBitmapCacheOnLoad, &wicBitmap);

			if (hr == 0)
			{
				gmpi::shared_ptr<Bitmap> b2;
				b2.Attach(new Bitmap(this, wicBitmap));

				b2->queryInterface(&gmpi::drawing::api::IBitmap::guid, (void**)returnDiBitmap);
			}

			SafeRelease(wicBitmap);

			return gmpi::ReturnCode::Ok;
		}

		IWICBitmap* Factory::CreateDiBitmapFromNative(ID2D1Bitmap* D2D_Bitmap)
		{
			return {};
#if 0

/*
The reason it doesn't work is that you are trying to use a resource created with one context/rendertarget with a different context/rendertarget.
There are only a few situations in which that is possible and this isn't one of them.
If all you want to do is save the bitmap to a file using WIC, follow this sample: http://code.msdn.microsoft.com/windowsapps/SaveAsImageFile-68073cb0 .
If you really want a WIC render target you can do it with ID2D1Bitmap1::Map by copying the data out and then creating a new bitmap using an overload that
lets you specify the initial data. If the original ID2D1Bitmap1 wasn't created with the D2D1_BITMAP_OPTIONS_CPU_READ flag then you'll need to
create an intermediate bitmap rendertarget that has that flag set and render the original bitmap to it or else using ID2D1DeviceContext::CreateBitmapFromDxgiSurface
[you should be able to QueryInterface (or use ComPtr<T>::As) to get an IDxgiSurface interface from an ID2D1Bitmap1].
Once you have a bitmap with that flag set then you can use Map on it, copy out its data,
and then use http://msdn.microsoft.com/en-us/library/dd371803(v=vs.85).aspx to create the bitmap with initial data (i.e. the data you copied out).
Make sure that you call Unmap when you are done reading the data and that you free any memory you copied (preferably you'd use a std::unique_ptr to store the copied data.
You might even be able to get away with just using the data pointer that you get from calling Map directly rather than copying the data out.
If so that'd be far more efficient so do that.)
*/

			/*
			probly need to:
			cast bitmap to rendertarget,
			copy bits out with ID2D1Bitmap::CopyFromRenderTarget 
			create a new IWICBitmap
			create a WicBitmapRenderTarget
			create a 2nd bitmap FROM THE wic render target
			copy bits into new bitmap
			draw new bitmap on rendertarget.
			*/

			// Don't work, can't draw from bitmap belonging to different render target. !!!
			auto size = D2D_Bitmap->GetSize();

			// Create a WIC bitmap.
			IWICBitmap* wicBitmap = nullptr;
			auto hr = pIWICFactory->CreateBitmap((UINT) size.width, (UINT) size.height, GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnLoad, &wicBitmap); // pre-muliplied alpha

			// Get a rendertarget on wic bitmap.
			D2D1_RENDER_TARGET_PROPERTIES props;
			memset(&props, 0, sizeof(props));
			props.type = D2D1_RENDER_TARGET_TYPE_SOFTWARE;
			props.dpiX = 0.0f;
			props.dpiY = 0.0f;
			props.pixelFormat.format = DXGI_FORMAT_B8G8R8A8_UNORM; // DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
			props.pixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;

			ID2D1RenderTarget* wicRenderTarget = nullptr;
			hr = m_pDirect2dFactory->CreateWicBitmapRenderTarget(
				wicBitmap,
				&props,
				&wicRenderTarget);

			// Draw pixels to wic bitmap.
			wicRenderTarget->BeginDraw();

			Rect r(0,0, size.width, size.height);

			wicRenderTarget->DrawBitmap(D2D_Bitmap, (D2D1_RECT_F*)&r, 1.0f, D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR, (D2D1_RECT_F*)&r);
			
			ID2D1SolidColorBrush *solidColorBrush;
			D2D1_COLOR_F color;
			D2D1_RECT_F rect;
			rect.left = rect.top = 0;
			rect.right = rect.bottom = 10;
			color.r = color.g = color.b = color.a = 0.8f;
			wicRenderTarget->CreateSolidColorBrush(color, &solidColorBrush);
			wicRenderTarget->FillRectangle(rect, solidColorBrush);

			auto r2 = wicRenderTarget->EndDraw();

			// release render target.
			SafeRelease(wicRenderTarget);

			return wicBitmap;
#endif
		}

		gmpi::ReturnCode Factory::getFontFamilyName(int32_t fontIndex, gmpi::api::IString* returnName)
		{
			if (fontIndex < 0 || fontIndex >= supportedFontFamilies.size())
			{
				return gmpi::ReturnCode::Fail;
			}

			returnName->setData(supportedFontFamilies[fontIndex].data(), static_cast<int32_t>(supportedFontFamilies[fontIndex].size()));
			return gmpi::ReturnCode::Ok;
		}

		gmpi::ReturnCode Factory::loadImageU(const char* uri, drawing::api::IBitmap** returnBitmap)
		{
			*returnBitmap = nullptr;

			HRESULT hr{};
			IWICBitmapDecoder* pDecoder{};
			IWICStream* pIWICStream{};

			// is this an in-memory resource?
			std::string uriString(uri);
			std::string binaryData;
#if 0 // TODO
			if (uriString.find(BundleInfo::resourceTypeScheme) == 0)
			{
				binaryData = BundleInfo::instance()->getResource(utf8Uri + strlen(BundleInfo::resourceTypeScheme));

				// Create a WIC stream to map onto the memory.
				hr = pIWICFactory->CreateStream(&pIWICStream);

				// Initialize the stream with the memory pointer and size.
				if (SUCCEEDED(hr)) {
					hr = pIWICStream->InitializeFromMemory(
						(WICInProcPointer)(binaryData.data()),
						(DWORD) binaryData.size());
				}

				// Create a decoder for the stream.
				if (SUCCEEDED(hr)) {
					hr = pIWICFactory->CreateDecoderFromStream(
						pIWICStream,                   // The stream to use to create the decoder
						NULL,                          // Do not prefer a particular vendor
						WICDecodeMetadataCacheOnLoad,  // Cache metadata when needed
						&pDecoder);                    // Pointer to the decoder
				}
			}
			else
#endif
			{
				const auto uriW = JmUnicodeConversions::Utf8ToWstring(uri);

				// To load a bitmap from a file, first use WIC objects to load the image and to convert it to a Direct2D-compatible format.
				hr = pIWICFactory->CreateDecoderFromFilename(
					uriW.c_str(),
					NULL,
					GENERIC_READ,
					WICDecodeMetadataCacheOnLoad,
					&pDecoder
				);
			}

			IWICBitmapFrameDecode *pSource = NULL;
			if (hr == 0)
			{
				// 2.Retrieve a frame from the image and store the frame in an IWICBitmapFrameDecode object.
				hr = pDecoder->GetFrame(0, &pSource);
			}

			IWICFormatConverter *pConverter = NULL;
			if (hr == 0)
			{
				// 3.The bitmap must be converted to a format that Direct2D can use.
				hr = pIWICFactory->CreateFormatConverter(&pConverter);
			}
			if (hr == 0)
			{
				hr = pConverter->Initialize(
					pSource,
					GUID_WICPixelFormat32bppPBGRA, //Premultiplied
					WICBitmapDitherTypeNone,
					NULL,
					0.f,
					WICBitmapPaletteTypeCustom
				);
			}

			IWICBitmap* wicBitmap = nullptr;
			if (hr == 0)
			{
				hr = pIWICFactory->CreateBitmapFromSource(
					pConverter,
					WICBitmapCacheOnLoad,
					&wicBitmap);
			}
			/*
D3D11 ERROR: ID3D11Device::CreateTexture2D: The Dimensions are invalid. For feature level D3D_FEATURE_LEVEL_11_0, the Width (value = 32) must be between 1 and 16384, inclusively. The Height (value = 60000) must be between 1 and 16384, inclusively. And, the ArraySize (value = 1) must be between 1 and 2048, inclusively. [ STATE_CREATION ERROR #101: CREATETEXTURE2D_INVALIDDIMENSIONS]
			*/
			if (hr == 0)
			{
				// I've removed this as the max size can vary depending on hardware.
				// So we need to have a robust check elsewhere anyhow.

#if 0
				UINT width, height;

				wicBitmap->GetSize(&width, &height);

				const int maxDirectXImageSize = 16384; // TODO can be smaller. Query hardware.
				if (width > maxDirectXImageSize || height > maxDirectXImageSize)
				{
					hr = E_FAIL; // fail, too big for DirectX.
				}
				else
#endif
				{
					auto bitmap = new Bitmap(this, wicBitmap);
#ifdef _DEBUG
					bitmap->debugFilename = uri;
#endif
					gmpi::shared_ptr<gmpi::drawing::api::IBitmap> b2;
					b2.Attach(bitmap);
					b2->queryInterface(&drawing::api::IBitmap::guid, (void**)returnBitmap);
				}
			}

			SafeRelease(pDecoder);
			SafeRelease(pSource);
			SafeRelease(pConverter);
			SafeRelease(wicBitmap);
			SafeRelease(pIWICStream);
			
			return hr == 0 ? (gmpi::ReturnCode::Ok) : (gmpi::ReturnCode::Fail);
		}

		gmpi::ReturnCode GraphicsContext_base::drawGeometry(drawing::api::IPathGeometry* pathGeometry, drawing::api::IBrush* brush, float strokeWidth, drawing::api::IStrokeStyle* strokeStyle)
		{
			auto* d2d_geometry = ((gmpi::directx::Geometry*)pathGeometry)->native();
			context_->DrawGeometry(d2d_geometry, ((Brush*)brush)->native(), (FLOAT)strokeWidth, toNative(strokeStyle));
			return gmpi::ReturnCode::Ok;
		}

		gmpi::ReturnCode GraphicsContext_base::drawTextU(const char* utf8String, uint32_t stringLength, drawing::api::ITextFormat* textFormat, const drawing::Rect* layoutRect, drawing::api::IBrush* brush, int32_t options)
		{
			const auto widestring = JmUnicodeConversions::Utf8ToWstring(utf8String, stringLength);
			
			auto DxTextFormat = reinterpret_cast<const TextFormat*>(textFormat);
			auto b = ((Brush*)brush)->native();
			auto tf = DxTextFormat->native();

			// Don't draw bounding box padding that some fonts have above ascent.
			auto adjusted = *layoutRect;
			if (!DxTextFormat->getUseLegacyBaseLineSnapping())
			{
				adjusted.top -= DxTextFormat->getTopAdjustment();

				// snap to pixel to match Mac.
                const float scale = 0.5f; // Hi DPI x2
				const float offset = -0.25f;
                const auto winBaseline = layoutRect->top + DxTextFormat->getAscent();
                const auto winBaselineSnapped = floorf((offset + winBaseline) / scale) * scale;
				const auto adjust = winBaselineSnapped - winBaseline + scale;

				adjusted.top += adjust;
				adjusted.bottom += adjust;
			}

			context_->DrawText(widestring.data(), (UINT32)widestring.size(), tf, reinterpret_cast<const D2D1_RECT_F*>(&adjusted), b, (D2D1_DRAW_TEXT_OPTIONS)options | D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT);

#ifdef LOG_DIRECTX_CALLS
			{
				std::wstring widestring2 = widestring;
				replacein( widestring2, L"\n", L"\\n");
				_RPT0(_CRT_WARN, "{\n");
				_RPT4(_CRT_WARN, "auto r = D2D1::RectF(%.3f, %.3f, %.3f, %.3ff);\n", layoutRect->left, layoutRect->top, layoutRect->right, layoutRect->bottom);
				_RPT4(_CRT_WARN, "context_->DrawTextW(L\"%S\", %d, textformat%x, &r, brush%x,", widestring2.c_str(), (int)widestring.size(), (int)textFormat, (int) brush);
				_RPT1(_CRT_WARN, " (D2D1_DRAW_TEXT_OPTIONS) %d);\n}\n", flags);
			}
#endif
/*

#if 0
			{
				gmpi::drawing::api::MP1_FONT_METRICS fontMetrics;
				((gmpi::drawing::api::ITextFormat*)textFormat)->GetFontMetrics(&fontMetrics);

				float predictedBaseLine = layoutRect->top + fontMetrics.ascent;
				const float scale = 0.5f;
				predictedBaseLine = floorf(-0.5 + predictedBaseLine / scale) * scale;

				Graphics g(this);
				auto brush = g.CreateSolidColorBrush(Color::Lime);
				g.DrawLine(Point(layoutRect->left, predictedBaseLine + 0.25f), Point(layoutRect->left + 2, predictedBaseLine + 0.25f), brush, 0.5);
			}
#endif
*/
			return gmpi::ReturnCode::Ok;
		}

		gmpi::ReturnCode Bitmap::getFactory(gmpi::drawing::api::IFactory** returnFactory)
		{
			*returnFactory = factory;
			return gmpi::ReturnCode::Ok;
		}

		gmpi::ReturnCode Bitmap::lockPixels(gmpi::drawing::api::IBitmapPixels** returnInterface, int32_t flags)
		{
			*returnInterface = nullptr;

			// If image was not loaded from a WicBitmap (i.e. was created from device context), then need to write it to WICBitmap first.
			if (diBitmap_ == nullptr)
			{
				if (!nativeBitmap_)
				{
					return gmpi::ReturnCode::Fail;
				}
			}
			return gmpi::ReturnCode::Fail; // creating WIC from D2DBitmap not implemented fully.
#if 0
				const auto size = nativeBitmap_->GetPixelSize();
				D2D1_BITMAP_PROPERTIES1 props = {};
				props.pixelFormat = nativeBitmap_->GetPixelFormat();
				nativeBitmap_->GetDpi(&props.dpiX, &props.dpiY);
				props.bitmapOptions = D2D1_BITMAP_OPTIONS_CPU_READ | D2D1_BITMAP_OPTIONS_CANNOT_DRAW;

				HRESULT res;
				ID2D1Bitmap1* tempBitmap = {};
				res = nativeContext_->CreateBitmap(size, nullptr, 0, props, &tempBitmap);

				D2D1_POINT_2U destPoint = {};
				D2D1_RECT_U srcRect = {0,0,size.width, size.height};
				res = tempBitmap->CopyFromBitmap(&destPoint, nativeBitmap_, &srcRect);

				{
					D2D1_MAPPED_RECT map;
					tempBitmap->Map(D2D1_MAP_OPTIONS_READ, &map);

// move to factory					res = pIWICFactory->CreateBitmap(size.width, size.height, GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnLoad, &diBitmap_); // pre-muliplied alpha

//					diBitmap_->CopyPixels()

					tempBitmap->Unmap();
				}

				tempBitmap->Release();

				return gmpi::ReturnCode::Fail; // creating WIC from D2DBitmap not implemented.
/*
				assert(nativeBitmap_);
*/
// clean this up				diBitmap_ = factory->CreateDiBitmapFromNative(nativeBitmap_);
			}
/*
			if (!nativeBitmap_)
			{
				// need to access native bitmap already to lazy-load it. Only can be done from RenderContext.
				assert(false && "Can't lock pixels before native bitmap created in onRender()");
				return gmpi::ReturnCode::Fail;
			}
*/
			gmpi::shared_ptr<gmpi::api::IUnknown> b2;
			b2.Attach(new BitmapPixels(nativeBitmap_, diBitmap_, true, flags));

			return b2->queryInterface(&drawing::api::IBitmapPixels::guid, (void**)(returnInterface));
#endif
		}

		ID2D1Bitmap* Bitmap::getNativeBitmap(ID2D1DeviceContext* nativeContext)
		{
			// Check for loss of surface.
			if (nativeContext != nativeContext_ && diBitmap_ != nullptr)
			{
				if (nativeBitmap_)
				{
					nativeBitmap_->Release();
					nativeBitmap_ = nullptr;
				}

				nativeContext_ = nativeContext;
#if 0 //defined(_DEBUG)
				// moved failure to cheCking error code on CreateBitmapFromWicBitmap()
				{
					auto maxSize = nativeContext_->GetMaximumBitmapSize();
					UINT imageW, imageH;
					diBitmap_->GetSize(&imageW, &imageH);

					if (imageW > maxSize || imageH > maxSize)
					{
						assert(false); // IMAGE TOO BIG!
						return nullptr;
					}
				}
#endif
				D2D1_BITMAP_PROPERTIES props;
				props.dpiX = props.dpiY = 96;
				if (factory->getPlatformPixelFormat() == gmpi::drawing::api::IBitmapPixels::kBGRA_SRGB) //  graphics->SupportSRGB())
				{
					props.pixelFormat.format = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB; // no good with DXGI_FORMAT_R16G16B16A16_FLOAT: nativeContext_->GetPixelFormat().format;
				}
				else
				{
					props.pixelFormat.format = DXGI_FORMAT_B8G8R8A8_UNORM; // no good with DXGI_FORMAT_R16G16B16A16_FLOAT: nativeContext_->GetPixelFormat().format;
				}
				props.pixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;

				try
				{
					// Convert to D2D format and cache.
					auto hr = nativeContext_->CreateBitmapFromWicBitmap(
						diBitmap_,
						&props, //NULL,
						&nativeBitmap_
					);

					if (hr) // Common failure is bitmap too big for D2D.
					{
						return nullptr;
					}
				}
				catch (...)
				{
					return nullptr;
				}
			}

			return nativeBitmap_;
		}

		Bitmap::Bitmap(Factory* pfactory, IWICBitmap* diBitmap) :
			nativeBitmap_(0)
			, nativeContext_(0)
			, diBitmap_(diBitmap)
			, factory(pfactory)
		{
			diBitmap->AddRef();

			// on Windows 7, leave image as-is
			if (factory->getPlatformPixelFormat() == gmpi::drawing::api::IBitmapPixels::kBGRA_SRGB)
			{
				applyPreMultiplyCorrection();
			}
		}

		// WIX premultiplies images automatically on load, but wrong (assumes linear not SRGB space). Fix it.
		void Bitmap::applyPreMultiplyCorrection()
		{
			drawing::Bitmap bitmap;
			*bitmap.put() = this;
			
			auto pixelsSource = bitmap.lockPixels((int32_t) gmpi::drawing::BitmapLockFlags::ReadWrite);
			auto imageSize = bitmap.getSize();
			size_t totalPixels = imageSize.height * pixelsSource.getBytesPerRow() / sizeof(uint32_t);
			uint8_t* sourcePixels = pixelsSource.getAddress();

			// WIX currently not premultiplying correctly, so redo it respecting gamma.
			const float over255 = 1.0f / 255.0f;
			for (size_t i = 0; i < totalPixels; ++i)
			{
				int alpha = sourcePixels[3];

				if (alpha != 255 && alpha != 0)
				{
					float AlphaNorm = alpha * over255;
					float overAlphaNorm = 1.f / AlphaNorm;

					for (int j = 0; j < 3; ++j)
					{
						int p = sourcePixels[j];
						if (p != 0)
						{
							float originalPixel = p * overAlphaNorm; // un-premultiply.

							// To linear
							auto cf = se_sdk::FastGamma::sRGB_to_float(static_cast<unsigned char>(FastRealToIntTruncateTowardZero(originalPixel + 0.5f)));

							cf *= AlphaNorm;						// pre-multiply (correctly).

							// back to SRGB
							sourcePixels[j] = se_sdk::FastGamma::float_to_sRGB(cf);
						}
					}
				}

				sourcePixels += sizeof(uint32_t);
			}
		}

		gmpi::ReturnCode GraphicsContext_base::createSolidColorBrush(const drawing::Color* color, const drawing::BrushProperties* brushProperties, drawing::api::ISolidColorBrush** returnSolidColorBrush)
		{
			*returnSolidColorBrush = nullptr;

			ID2D1SolidColorBrush* b = nullptr;
			HRESULT hr = context_->CreateSolidColorBrush(*(D2D1_COLOR_F*)color, &b);

			if (hr == 0)
			{
				gmpi::shared_ptr<gmpi::api::IUnknown> b2;
				b2.Attach(new SolidColorBrush(b, factory));

				b2->queryInterface(&drawing::api::ISolidColorBrush::guid, reinterpret_cast<void **>(returnSolidColorBrush));
			}

			return hr == 0 ? (gmpi::ReturnCode::Ok) : (gmpi::ReturnCode::Fail);
		}

		gmpi::ReturnCode GraphicsContext_base::createGradientstopCollection(const drawing::Gradientstop* gradientstops, uint32_t gradientstopsCount, drawing::ExtendMode extendMode, drawing::api::IGradientstopCollection** returnGradientstopCollection)
		{
			*returnGradientstopCollection = nullptr;

			HRESULT hr = 0;

			ID2D1GradientStopCollection1* native2 = nullptr;

			hr = context_->CreateGradientStopCollection(
				(D2D1_GRADIENT_STOP*)gradientstops,
				gradientstopsCount,
				D2D1_COLOR_SPACE_SRGB,
				D2D1_COLOR_SPACE_SRGB,
				D2D1_BUFFER_PRECISION_8BPC_UNORM_SRGB, // Buffer precision. D2D1_BUFFER_PRECISION_16BPC_FLOAT seems the same
				D2D1_EXTEND_MODE_CLAMP,
				D2D1_COLOR_INTERPOLATION_MODE_STRAIGHT,
				&native2);

			if (hr == 0)
			{
				gmpi::shared_ptr<gmpi::api::IUnknown> wrapper;
				wrapper.Attach(new GradientstopCollection(native2, factory));

				wrapper->queryInterface(&drawing::api::IGradientstopCollection::guid, reinterpret_cast<void**>(returnGradientstopCollection));
			}

			return hr == 0 ? (gmpi::ReturnCode::Ok) : (gmpi::ReturnCode::Fail);
		}

		// todo : integer size?
		gmpi::ReturnCode GraphicsContext_base::createCompatibleRenderTarget(drawing::Size desiredSize, gmpi::drawing::api::IBitmapRenderTarget** bitmapRenderTarget)
		{
			*bitmapRenderTarget = nullptr;

			gmpi::shared_ptr<gmpi::api::IUnknown> b2;
			b2.Attach(new BitmapRenderTarget(this, &desiredSize, factory));
			return b2->queryInterface(&drawing::api::IBitmapRenderTarget::guid, reinterpret_cast<void **>(bitmapRenderTarget));
		}

		gmpi::ReturnCode BitmapRenderTarget::getBitmap(gmpi::drawing::api::IBitmap** returnBitmap)
		{
			*returnBitmap = nullptr;

			ID2D1Bitmap* nativeBitmap;
			auto hr = nativeBitmapRenderTarget->GetBitmap(&nativeBitmap);

			if (hr == 0)
			{
				gmpi::shared_ptr<gmpi::api::IUnknown> b2;
				b2.Attach(new Bitmap(factory, context_, nativeBitmap));
				nativeBitmap->Release();

				b2->queryInterface(&drawing::api::IBitmap::guid, reinterpret_cast<void **>(returnBitmap));
			}

			return hr == 0 ? (gmpi::ReturnCode::Ok) : (gmpi::ReturnCode::Fail);
		}

		gmpi::ReturnCode GraphicsContext_base::pushAxisAlignedClip(const drawing::Rect* clipRect)
		{
			context_->PushAxisAlignedClip((D2D1_RECT_F*)clipRect, D2D1_ANTIALIAS_MODE_ALIASED);

			// Transform to original position.
			drawing::Matrix3x2 currentTransform;
			context_->GetTransform(reinterpret_cast<D2D1_MATRIX_3X2_F*>(&currentTransform));
			auto r2 = transformRect(currentTransform , *clipRect);
			clipRectStack.push_back(r2);

//			_RPT4(_CRT_WARN, "                 PushAxisAlignedClip( %f %f %f %f)\n", r2.left, r2.top, r2.right , r2.bottom);
			return gmpi::ReturnCode::Ok;
		}

		void GraphicsContext_base::getAxisAlignedClip(gmpi::drawing::Rect* returnClipRect)
		{
#ifdef LOG_DIRECTX_CALLS
			_RPT0(_CRT_WARN, "{\n");
			_RPT0(_CRT_WARN, "D2D1_MATRIX_3X2_F t;\n");
			_RPT0(_CRT_WARN, "context_->GetTransform(&t);\n");
			_RPT0(_CRT_WARN, "}\n");
#endif

			// Transform to original position.
			drawing::Matrix3x2 currentTransform;
			context_->GetTransform(reinterpret_cast<D2D1_MATRIX_3X2_F*>(&currentTransform));
			currentTransform = invert(currentTransform);
			auto r2 = transformRect(currentTransform, clipRectStack.back());

			*returnClipRect = r2;
		}
	} // namespace
} // namespace


#endif // skip compilation on macOS
