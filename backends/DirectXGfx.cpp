#ifdef _WIN32 // skip compilation on macOS

#include <sstream>
#include "./DirectXGfx.h"
#include "d2d1helper.h"

namespace gmpi
{
	namespace directx
	{
		inline std::wstring Utf8ToWstring(std::string_view str)
		{
			std::wstring res;
			const size_t size = MultiByteToWideChar(
				CP_UTF8,
				0,
				str.data(),
				static_cast<int>(str.size()),
				0,
				0
			);

			res.resize(size);

			MultiByteToWideChar(
				CP_UTF8,
				0,
				str.data(),
				static_cast<int>(str.size()),
				const_cast<LPWSTR>(res.data()),
				static_cast<int>(size)
			);

			return res;
		};
		inline std::string WStringToUtf8(const std::wstring& p_cstring)
		{
			std::string res;

			const size_t size = WideCharToMultiByte(
				CP_UTF8,
				0,
				p_cstring.data(),
				static_cast<int>(p_cstring.size()),
				0,
				0,
				NULL,
				NULL
			);

			res.resize(size);

			WideCharToMultiByte(
				CP_UTF8,
				0,
				p_cstring.data(),
				static_cast<int>(p_cstring.size()),
				const_cast<LPSTR>(res.data()),
				static_cast<int>(size),
				NULL,
				NULL
			);

			return res;
		}

		gmpi::ReturnCode Geometry::open(drawing::api::IGeometrySink** returnGeometrySink)
		{
			ID2D1GeometrySink* sink{};

			auto hr = geometry_->Open(&sink);

			if (hr == 0)
			{
				gmpi::shared_ptr<gmpi::api::IUnknown> b2;
				b2.attach(new gmpi::directx::GeometrySink(sink));

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
			const auto widestring = Utf8ToWstring({ utf8String, static_cast<size_t>(stringLength) });

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
		Factory_base::Factory_base(DxFactoryInfo& pinfo, gmpi::api::IUnknown* pfallback) :
			info(pinfo)
			, fallback(pfallback)
		{
		}

		Factory::Factory(gmpi::api::IUnknown* pfallback) : Factory_base(concreteInfo, pfallback)
		{
			{
				D2D1_FACTORY_OPTIONS o;
#ifdef _DEBUG
				o.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;// D2D1_DEBUG_LEVEL_WARNING; // Need to install special stuff. https://msdn.microsoft.com/en-us/library/windows/desktop/ee794278%28v=vs.85%29.aspx?f=255&MSPPError=-2147217396 
#else
				o.debugLevel = D2D1_DEBUG_LEVEL_NONE;
#endif
//				auto rs = D2D1CreateFactory(D2D1_FACTORY_TYPE_MULTI_THREADED, __uuidof(ID2D1Factory1), &o, (void**)&m_pDirect2dFactory);
				auto rs = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory1), &o, (void**)&info.m_pDirect2dFactory);

#ifdef _DEBUG
				if (FAILED(rs))
				{
					o.debugLevel = D2D1_DEBUG_LEVEL_NONE; // fallback
					rs = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory1), &o, (void**)&info.m_pDirect2dFactory);
				}
#endif
				if (FAILED(rs))
				{
					_RPT1(_CRT_WARN, "D2D1CreateFactory FAIL %d\n", rs);
					return;  // Fail.
				}
				//		_RPT2(_CRT_WARN, "D2D1CreateFactory OK %d : %x\n", rs, m_pDirect2dFactory);
			}

			info.writeFactory = nullptr;

			auto hr = DWriteCreateFactory(
				DWRITE_FACTORY_TYPE_SHARED, // no improvment to glitching DWRITE_FACTORY_TYPE_ISOLATED
				__uuidof(info.writeFactory),
				reinterpret_cast<::IUnknown**>(&info.writeFactory)
			);

			info.pIWICFactory = nullptr;

			hr = CoCreateInstance(
				CLSID_WICImagingFactory,
				NULL,
				CLSCTX_INPROC_SERVER,
				IID_IWICImagingFactory,
				(LPVOID*)&info.pIWICFactory
			);

			// Cache font family names
			{
				// TODO IDWriteFontSet is improved API, GetSystemFontSet()

				IDWriteFontCollection* fonts = nullptr;
				info.writeFactory->GetSystemFontCollection(&fonts, TRUE);

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

						info.supportedFontFamilies.push_back(WStringToUtf8(name));

						std::transform(name, name + wcslen(name), name, [](wchar_t c) { return static_cast<wchar_t>(std::tolower(c)); });
						info.supportedFontFamiliesLowerCase.push_back(name);
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
			SafeRelease(info.m_pDirect2dFactory);
			SafeRelease(info.writeFactory);
			SafeRelease(info.pIWICFactory);
		}

		gmpi::ReturnCode Factory_base::createPathGeometry(gmpi::drawing::api::IPathGeometry** pathGeometry)
		{
			*pathGeometry = nullptr;
			//*pathGeometry = new GmpiGuiHosting::PathGeometry();
			//return gmpi::ReturnCode::Ok;

			ID2D1PathGeometry* d2d_geometry = nullptr;
			HRESULT hr = info.m_pDirect2dFactory->CreatePathGeometry(&d2d_geometry);

			if (hr == 0)
			{
				gmpi::shared_ptr<gmpi::api::IUnknown> b2;
				b2.attach(new gmpi::directx::Geometry(d2d_geometry));

				b2->queryInterface(&drawing::api::IPathGeometry::guid, reinterpret_cast<void**>(pathGeometry));
			}

			return hr == 0 ? (gmpi::ReturnCode::Ok) : (gmpi::ReturnCode::Fail);
		}

		gmpi::ReturnCode Factory_base::createTextFormat(const char* fontFamilyName, /* void* unused fontCollection ,*/ gmpi::drawing::FontWeight fontWeight, gmpi::drawing::FontStyle fontStyle, gmpi::drawing::FontStretch fontStretch, float fontSize, /* void* unused2 localeName, */ gmpi::drawing::api::ITextFormat** textFormat)
		{
			*textFormat = nullptr;

			auto fontFamilyNameW = Utf8ToWstring(fontFamilyName);
			std::wstring lowercaseName(fontFamilyNameW);
			std::transform(lowercaseName.begin(), lowercaseName.end(), lowercaseName.begin(), [](wchar_t c) { return static_cast<wchar_t>(std::tolower(c)); });

			if (std::find(info.supportedFontFamiliesLowerCase.begin(), info.supportedFontFamiliesLowerCase.end(), lowercaseName) == info.supportedFontFamiliesLowerCase.end())
			{
				fontFamilyNameW = fontMatch(fontFamilyNameW, fontWeight, fontSize);
			}

			IDWriteTextFormat* dwTextFormat = nullptr;

			auto hr = info.writeFactory->CreateTextFormat(
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
				b2.attach(new gmpi::directx::TextFormat(/*&stringConverter,*/ dwTextFormat));

				b2->queryInterface(&drawing::api::ITextFormat::guid, reinterpret_cast<void**>(textFormat));
			}

			return hr == 0 ? (gmpi::ReturnCode::Ok) : (gmpi::ReturnCode::Fail);
		}

		// 2nd pass - GDI->DirectWrite conversion. "Arial Black" -> "Arial"
		std::wstring Factory_base::fontMatch(std::wstring fontFamilyNameW, gmpi::drawing::FontWeight fontWeight, float fontSize)
		{
			auto it = info.GdiFontConversions.find(fontFamilyNameW);
			if (it != info.GdiFontConversions.end())
			{
				return (*it).second;
			}

			IDWriteGdiInterop* interop = nullptr;
			info.writeFactory->GetGdiInterop(&interop);

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
					info.GdiFontConversions.insert(std::pair<std::wstring, std::wstring>(fontFamilyNameW, name));
					fontFamilyNameW = name;
				}

				names->Release();
				family->Release();

				font->Release();
			}

			interop->Release();
			return fontFamilyNameW;
		}

		gmpi::ReturnCode Factory_base::createImage(int32_t width, int32_t height, gmpi::drawing::api::IBitmap** returnDiBitmap)
		{
			IWICBitmap* wicBitmap{};
			auto hr = info.pIWICFactory->CreateBitmap(width, height, GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnLoad, &wicBitmap); // pre-muliplied alpha
		// nuh auto hr =   pIWICFactory->CreateBitmap(width, height, GUID_WICPixelFormat32bppBGRA, WICBitmapCacheOnLoad, &wicBitmap);

			if (hr == 0)
			{
				gmpi::shared_ptr<Bitmap> b2;
				b2.attach(new Bitmap(this, wicBitmap));

				b2->queryInterface(&gmpi::drawing::api::IBitmap::guid, (void**)returnDiBitmap);
			}

			SafeRelease(wicBitmap);

			return gmpi::ReturnCode::Ok;
		}

		gmpi::ReturnCode Factory_base::getFontFamilyName(int32_t fontIndex, gmpi::api::IString* returnName)
		{
			if (fontIndex < 0 || fontIndex >= info.supportedFontFamilies.size())
			{
				return gmpi::ReturnCode::Fail;
			}

			returnName->setData(info.supportedFontFamilies[fontIndex].data(), static_cast<int32_t>(info.supportedFontFamilies[fontIndex].size()));
			return gmpi::ReturnCode::Ok;
		}

		gmpi::ReturnCode Factory_base::loadImageU(const char* uri, drawing::api::IBitmap** returnBitmap)
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
				const auto uriW = Utf8ToWstring(uri);

				// To load a bitmap from a file, first use WIC objects to load the image and to convert it to a Direct2D-compatible format.
				hr = info.pIWICFactory->CreateDecoderFromFilename(
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
				hr = info.pIWICFactory->CreateFormatConverter(&pConverter);
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
				hr = info.pIWICFactory->CreateBitmapFromSource(
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
					b2.attach(bitmap);
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
			const auto widestring = Utf8ToWstring({ utf8String, static_cast<size_t>(stringLength) });
			
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

			return gmpi::ReturnCode::Ok;
		}

		gmpi::ReturnCode Bitmap::getFactory(gmpi::drawing::api::IFactory** returnFactory)
		{
			*returnFactory = factory;
			return gmpi::ReturnCode::Ok;
		}

		ID2D1Bitmap* bitmapToNative(
			ID2D1DeviceContext* nativeContext
			, gmpi::directx::ComPtr<ID2D1Bitmap>& nativeBitmap		// GPU bitmap, created from WIC bitmap or a GPU bitmap render target..
			, gmpi::directx::ComPtr<IWICBitmap>& diBitmap			// WIC bitmap, usually loaded from disk, or created by CPU.
			, ID2D1Factory1* direct2dFactory
			, IWICImagingFactory* wicFactory
		)
		{
			if (!nativeBitmap && diBitmap)
			{
				try
				{
					// images loaded from disk or created on CPU are 8-bit.
					// Images from createCompatibleREnderTarget might be 16-bit. ref: 'use8bit'
					WICPixelFormatGUID pixelFormat{};
					diBitmap->GetPixelFormat(&pixelFormat);

					WICPixelFormatGUID formatGuid;
					diBitmap->GetPixelFormat(&formatGuid);

					HRESULT hr{};

					D2D1_BITMAP_PROPERTIES props;
					props.dpiX = props.dpiY = 96;
					if (std::memcmp(&formatGuid, &GUID_WICPixelFormat32bppPBGRA, sizeof(formatGuid)) == 0)
					{
						props.pixelFormat.format = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
						props.pixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;
						// window-7 more uses DXGI_FORMAT_B8G8R8A8_UNORM, not implemented yet.

						hr = nativeContext->CreateBitmapFromWicBitmap(
							diBitmap,
							&props,
							nativeBitmap.put()
						);
					}
					else
					{
						// half-float pixels. default format should be fine.
						hr = nativeContext->CreateBitmapFromWicBitmap(
							diBitmap,
							nativeBitmap.put()
						);
					}

					assert(hr == 0); // Common failure is bitmap too big for D2D.
				}
				catch (...)
				{
					_RPT0(0, "Bitmap::GetNativeBitmap() - CreateBitmapFromWicBitmap() failed. Bitmap too big for D2D?\n");
				}
			}

			return nativeBitmap.get();
		}

		gmpi::ReturnCode Bitmap::lockPixels(gmpi::drawing::api::IBitmapPixels** returnInterface, int32_t flags)
		{
			*returnInterface = nullptr;

			// GPU-only bitpmaps can't be locked.
			if (!diBitmap_)
			{
				return gmpi::ReturnCode::Fail;
			}

			gmpi::shared_ptr<gmpi::api::IUnknown> b2;
			b2.attach(new BitmapPixels(nativeBitmap_, diBitmap_, true, flags));

			return b2->queryInterface(&drawing::api::IBitmapPixels::guid, (void**)(returnInterface));
		}

		ID2D1Bitmap* Bitmap::getNativeBitmap(ID2D1DeviceContext* nativeContext)
		{
			// Check for loss of surface. If so invalidate device-bitmap
			if (nativeContext != nativeContext_)
			{
				nativeContext_ = nativeContext;
				nativeBitmap_ = nullptr;
				assert(diBitmap_); // Is this a GPU-only bitmap?
			}

			auto& lfactory = static_cast<Factory_base&>(*factory);

			return bitmapToNative(
				nativeContext
				, nativeBitmap_
				, diBitmap_
				, lfactory.getFactory()
				, lfactory.getWicFactory()
			);
		}

		Bitmap::Bitmap(drawing::api::IFactory* pfactory, IWICBitmap* diBitmap) :
			nativeBitmap_(0)
			, nativeContext_(0)
			, diBitmap_(diBitmap)
			, factory(pfactory)
		{
			diBitmap->AddRef();

			// on Windows 7, leave image as-is
			drawing::api::IBitmapPixels::PixelFormat pixelFormat;
			factory->getPlatformPixelFormat(&pixelFormat);

			if (pixelFormat == gmpi::drawing::api::IBitmapPixels::kBGRA_SRGB)
			{
				applyPreMultiplyCorrection();
			}
		}

		// WIX premultiplies images automatically on load, but wrong (assumes linear not SRGB space). Fix it.
		void Bitmap::applyPreMultiplyCorrection()
		{
			drawing::Bitmap bitmap;
			*bitmap.put() = this;
			addRef(); // MESSY, fix. drawing::Bitmap dosn't incremnet ref count.

			auto pixelsSource = bitmap.lockPixels((int32_t) gmpi::drawing::BitmapLockFlags::ReadWrite);
			auto imageSize = bitmap.getSize();
			size_t totalPixels = imageSize.height * pixelsSource.getBytesPerRow() / sizeof(uint32_t);
			uint8_t* sourcePixels = pixelsSource.getAddress();

			// WIX currently not premultiplying correctly, so redo it respecting gamma.
			constexpr float over255 = 1.0f / 255.0f;
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
							auto cf = gmpi::drawing::SRGBPixelToLinear(static_cast<unsigned char>(originalPixel + 0.5f));

							cf *= AlphaNorm;						// pre-multiply (correctly).

							// back to SRGB
							sourcePixels[j] = gmpi::drawing::linearPixelToSRGB(cf);
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
				b2.attach(new SolidColorBrush(b, factory));

				b2->queryInterface(&drawing::api::ISolidColorBrush::guid, reinterpret_cast<void **>(returnSolidColorBrush));
			}

			return hr == 0 ? (gmpi::ReturnCode::Ok) : (gmpi::ReturnCode::Fail);
		}

		gmpi::ReturnCode GraphicsContext_base::createGradientstopCollection(const drawing::Gradientstop* gradientstops, uint32_t gradientstopsCount, drawing::ExtendMode extendMode, drawing::api::IGradientstopCollection** returnGradientstopCollection)
		{
			*returnGradientstopCollection = nullptr;

			ID2D1GradientStopCollection1* native2 = nullptr;

			HRESULT hr = context_->CreateGradientStopCollection(
				(const D2D1_GRADIENT_STOP*)gradientstops,
				gradientstopsCount,
				D2D1_COLOR_SPACE_SRGB,
				D2D1_COLOR_SPACE_SRGB,
				D2D1_BUFFER_PRECISION_16BPC_FLOAT, // the same in 8-bit, correct in HDR
				D2D1_EXTEND_MODE_CLAMP,
				D2D1_COLOR_INTERPOLATION_MODE_STRAIGHT,
				&native2);

			if (hr == 0)
			{
				gmpi::shared_ptr<gmpi::api::IUnknown> wrapper;
				wrapper.attach(new GradientstopCollection(native2, factory));

				wrapper->queryInterface(&drawing::api::IGradientstopCollection::guid, reinterpret_cast<void**>(returnGradientstopCollection));
			}

			return hr == 0 ? (gmpi::ReturnCode::Ok) : (gmpi::ReturnCode::Fail);
		}

		// todo : integer size?
		gmpi::ReturnCode GraphicsContext_base::createCompatibleRenderTarget(drawing::Size desiredSize, gmpi::drawing::api::IBitmapRenderTarget** bitmapRenderTarget)
		{
			*bitmapRenderTarget = nullptr;

			gmpi::shared_ptr<gmpi::api::IUnknown> b2;
			b2.attach(new BitmapRenderTarget(this, &desiredSize, factory));
			return b2->queryInterface(&drawing::api::IBitmapRenderTarget::guid, reinterpret_cast<void **>(bitmapRenderTarget));
		}

		void createBitmapRenderTarget(
			  UINT width
			, UINT height
			, bool isCpuReadable
			, ID2D1DeviceContext* outerDeviceContext
			, ID2D1Factory1* d2dFactory
			, IWICImagingFactory* wicFactory
			, gmpi::directx::ComPtr<IWICBitmap>& returnWicBitmap
			, gmpi::directx::ComPtr<ID2D1DeviceContext>& returnContext
		)
		{
			if (isCpuReadable) // TODO !!! wrong gamma.
			{
				// Create a WIC render target. Modifyable by CPU (lock pixels). More expensive.
				const bool use8bit = true; // note 16 bit giving completely weird results, 8-bit giving wrong gamma but consistent w SE 1.5.

				// Create a WIC bitmap to draw on.
				[[maybe_unused]] auto hr =
					wicFactory->CreateBitmap(
						width
						, height
						, use8bit ? GUID_WICPixelFormat32bppPBGRA : GUID_WICPixelFormat128bppPRGBAFloat //GUID_WICPixelFormat64bppPRGBAHalf // GUID_WICPixelFormat128bppPRGBAFloat // GUID_WICPixelFormat64bppPRGBAHalf // GUID_WICPixelFormat128bppPRGBAFloat
						, use8bit ? WICBitmapCacheOnDemand : WICBitmapCacheOnLoad //WICBitmapNoCache
						, returnWicBitmap.put()
					);

				D2D1_RENDER_TARGET_PROPERTIES renderTargetProperties = D2D1::RenderTargetProperties(
					D2D1_RENDER_TARGET_TYPE_DEFAULT,
					D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_UNKNOWN)
					//D2D1::PixelFormat(DXGI_FORMAT_R16G16B16A16_FLOAT, D2D1_ALPHA_MODE_PREMULTIPLIED)
				);

				gmpi::directx::ComPtr<ID2D1RenderTarget> wikBitmapRenderTarget;
				d2dFactory->CreateWicBitmapRenderTarget(
					returnWicBitmap.get(),
					renderTargetProperties,
					wikBitmapRenderTarget.put()
				);

				// wikBitmapRenderTarget->QueryInterface(IID_ID2D1DeviceContext, (void**)&context_);
				// hr = wikBitmapRenderTarget->QueryInterface(IID_PPV_ARGS(&context_));
				returnContext = wikBitmapRenderTarget.as<ID2D1DeviceContext>();

				//context_->AddRef(); //?????
			}
			else
			{
				const D2D1_SIZE_F desiredSize = D2D1::SizeF(static_cast<float>(width), static_cast<float>(height));

				// Create a render target on the GPU. Not modifyable by CPU.
				gmpi::directx::ComPtr<ID2D1BitmapRenderTarget> gpuBitmapRenderTarget;
				/* auto hr = */ outerDeviceContext->CreateCompatibleRenderTarget(desiredSize, gpuBitmapRenderTarget.put());
				returnContext = gpuBitmapRenderTarget.as<ID2D1DeviceContext>();
			}
		}

		BitmapRenderTarget::BitmapRenderTarget(GraphicsContext_base* g, const drawing::Size* desiredSize, drawing::api::IFactory* pfactory) :
			GraphicsContext_base(pfactory)
		{
			const bool enableLockPixels = false; // TODO

			auto& lfactory = static_cast<Factory_base&>(*factory);

			createBitmapRenderTarget(
				  static_cast<UINT>(desiredSize->width)
				, static_cast<UINT>(desiredSize->height)
				, enableLockPixels
				, g->native()
				, lfactory.getFactory()
				, lfactory.getWicFactory()
				, wicBitmap
				, context_
			);

			clipRectStack.push_back({ 0, 0, desiredSize->width, desiredSize->height });
		}

		gmpi::ReturnCode BitmapRenderTarget::getBitmap(gmpi::drawing::api::IBitmap** returnBitmap)
		{
			*returnBitmap = {};

			HRESULT hr{ E_FAIL };

			gmpi::shared_ptr<gmpi::api::IUnknown> b2;
			if (wicBitmap)
			{
				// Flush the device context to ensure all drawing commands are completed. testing
//hr = context_->Flush();
				context_ = nullptr;

// TODO				b2.Attach(new Bitmap(factory.getInfo(), factory.getPlatformPixelFormat(), wicBitmap)); //temp factory about to go out of scope (when using a bitmap render target)

				hr = S_OK;
			}
			else // if (wikBitmapRenderTarget)
			{
				gmpi::directx::ComPtr<ID2D1Bitmap> nativeBitmap;
				hr = context_.as<ID2D1BitmapRenderTarget>()->GetBitmap(nativeBitmap.put());

				if (hr == S_OK)
				{
//					b2.Attach(new Bitmap(factory.getInfo(), factory.getPlatformPixelFormat(), /*context_*/originalContext, nativeBitmap.get()));
					b2.attach(new Bitmap(factory, context_, nativeBitmap));
				}
			}

			if (hr == S_OK)
				b2->queryInterface(&drawing::api::IBitmap::guid, reinterpret_cast<void**>(returnBitmap));

			return hr ? ReturnCode::Fail : ReturnCode::Ok;
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

		gmpi::ReturnCode GraphicsContext_base::getAxisAlignedClip(gmpi::drawing::Rect* returnClipRect)
		{
			// Transform to original position.
			drawing::Matrix3x2 currentTransform;
			context_->GetTransform(reinterpret_cast<D2D1_MATRIX_3X2_F*>(&currentTransform));
			currentTransform = invert(currentTransform);
			auto r2 = transformRect(currentTransform, clipRectStack.back());

			*returnClipRect = r2;
			return gmpi::ReturnCode::Ok;
		}
	} // namespace
} // namespace


#endif // skip compilation on macOS
