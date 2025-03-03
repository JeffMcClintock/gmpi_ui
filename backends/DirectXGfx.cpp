#ifdef _WIN32 // skip compilation on macOS

#include <sstream>
#include <optional>
#include "./DirectXGfx.h"
#include "d2d1helper.h"

namespace gmpi
{
	namespace directx
	{
		std::wstring Utf8ToWstring(std::string_view str)
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
		std::string WStringToUtf8(std::wstring_view p_cstring)
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

			return toReturnCode(hr);
		}

		gmpi::drawing::FontMetrics getFontMetricsHelper(IDWriteTextFormat* textFormat)
		{
			gmpi::directx::ComPtr<IDWriteFontCollection> collection;
			gmpi::directx::ComPtr<IDWriteFontFace> fontface;
			WCHAR nameW[255];
			UINT32 index{};
			BOOL exists{};
			HRESULT hr{};

			hr = textFormat->GetFontCollection(collection.put());
			//	ok(hr == S_OK, "got 0x%08x\n", hr);

			hr = textFormat->GetFontFamilyName(nameW, std::size(nameW));
			//	ok(hr == S_OK, "got 0x%08x\n", hr);

			hr = collection->FindFamilyName(nameW, &index, &exists);
			if (exists == 0) // font not available. Fallback.
			{
				index = 0;
			}

			{
				gmpi::directx::ComPtr<IDWriteFontFamily> family;
				hr = collection->GetFontFamily(index, family.put());
				//	ok(hr == S_OK, "got 0x%08x\n", hr);

				gmpi::directx::ComPtr<IDWriteFont> font;
				hr = family->GetFirstMatchingFont(
					textFormat->GetFontWeight(),
					textFormat->GetFontStretch(),
					textFormat->GetFontStyle(),
					font.put());
				//	ok(hr == S_OK, "got 0x%08x\n", hr);

				hr = font->CreateFontFace(fontface.put());
				//	ok(hr == S_OK, "got 0x%08x\n", hr);
			}

			DWRITE_FONT_METRICS metrics{};
			fontface->GetMetrics(&metrics);

			// Sizes returned must always be in DIPs.
			const float emsToDips = textFormat->GetFontSize() / metrics.designUnitsPerEm;

			return gmpi::drawing::FontMetrics{
				emsToDips * metrics.ascent,
				emsToDips * metrics.descent,
				emsToDips * metrics.lineGap,
				emsToDips * metrics.capHeight,
				emsToDips * metrics.xHeight,
				emsToDips * metrics.underlinePosition,
				emsToDips * metrics.underlineThickness,
				emsToDips * metrics.strikethroughPosition,
				emsToDips * metrics.strikethroughThickness
			};
		}

		gmpi::ReturnCode TextFormat::getFontMetrics(gmpi::drawing::FontMetrics* returnFontMetrics)
		{
			*returnFontMetrics = getFontMetricsHelper(native());
			return gmpi::ReturnCode::Ok;
		}

		gmpi::drawing::Size getTextExtentHelper(IDWriteFactory* writeFactory, IDWriteTextFormat* textFormat, std::string_view s, float topAdjustment, bool useLegacyBaseLineSnapping)
		{
			const auto widestring = Utf8ToWstring(s);

			gmpi::directx::ComPtr<IDWriteTextLayout> textLayout;

			auto hr = writeFactory->CreateTextLayout(
				widestring.data(),      // The string to be laid out and formatted.
				(UINT32)widestring.size(),  // The length of the string.
				textFormat,     // The text format to apply to the string (contains font information, etc).
				100000,         // The width of the layout box.
				100000,         // The height of the layout box.
				textLayout.put()   // The IDWriteTextLayout interface pointer.
			);

			DWRITE_TEXT_METRICS textMetrics;
			textLayout->GetMetrics(&textMetrics);

			return
			{
				textMetrics.widthIncludingTrailingWhitespace,
				useLegacyBaseLineSnapping ? textMetrics.height : textMetrics.height - topAdjustment
			};
		}

		gmpi::ReturnCode TextFormat::getTextExtentU(const char* utf8String, int32_t stringLength, gmpi::drawing::Size* returnSize)
		{
			*returnSize = getTextExtentHelper(writeFactory, native(), { utf8String, static_cast<size_t>(stringLength) }, topAdjustment, useLegacyBaseLineSnapping);
			return gmpi::ReturnCode::Ok;
		}

		// Create factory myself;
		Factory_base::Factory_base(DxFactoryInfo& pinfo, gmpi::api::IUnknown* pfallback) :
			info(pinfo)
			, fallback(pfallback)
		{
		}

		void initFactoryHelper(
			  gmpi::directx::ComPtr<IDWriteFactory>& writeFactory
			, gmpi::directx::ComPtr<IWICImagingFactory>& wicFactory
			, gmpi::directx::ComPtr<ID2D1Factory1>& direct2dFactory
			, std::vector<std::string>& supportedFontFamilies
			, std::vector<std::wstring>& supportedFontFamiliesLowerCase
		)
		{
			{
				D2D1_FACTORY_OPTIONS o;
#ifdef _DEBUG
				o.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION; // Need to install special stuff. https://msdn.microsoft.com/en-us/library/windows/desktop/ee794278%28v=vs.85%29.aspx?f=255&MSPPError=-2147217396 
#else
				o.debugLevel = D2D1_DEBUG_LEVEL_NONE;
#endif
				auto rs = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory1), &o, direct2dFactory.put_void());

#ifdef _DEBUG
				if (FAILED(rs))
				{
					o.debugLevel = D2D1_DEBUG_LEVEL_NONE; // fallback
					rs = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory1), &o, direct2dFactory.put_void());
				}
#endif
				if (FAILED(rs))
				{
					_RPT1(_CRT_WARN, "D2D1CreateFactory FAIL %d\n", rs);
					return;  // Fail.
				}
				//		_RPT2(_CRT_WARN, "D2D1CreateFactory OK %d : %x\n", rs, *direct2dFactory);
			}

			auto hr = DWriteCreateFactory(
				DWRITE_FACTORY_TYPE_SHARED, // no improvment to glitching DWRITE_FACTORY_TYPE_ISOLATED
				__uuidof(IDWriteFactory),
				reinterpret_cast<::IUnknown**>(writeFactory.put())
			);

			hr = CoCreateInstance(
				CLSID_WICImagingFactory,
				NULL,
				CLSCTX_INPROC_SERVER,
				IID_IWICImagingFactory,
				wicFactory.put_void()
			);

			// Cache font family names
			{
				// TODO IDWriteFontSet is improved API, GetSystemFontSet()

				gmpi::directx::ComPtr<IDWriteFontCollection> fonts;
				writeFactory->GetSystemFontCollection(fonts.put(), TRUE);

				const auto count = fonts->GetFontFamilyCount();

				for (int index = 0; index < (int)count; ++index)
				{
					gmpi::directx::ComPtr<IDWriteFontFamily> family;
					fonts->GetFontFamily(index, family.put());

					gmpi::directx::ComPtr<IDWriteLocalizedStrings> names;
					family->GetFamilyNames(names.put());

					BOOL exists{};
					unsigned int nameIndex{};
					names->FindLocaleName(L"en-us", &nameIndex, &exists);
					if (exists)
					{
						wchar_t name[64];
						names->GetString(nameIndex, name, std::size(name));

						supportedFontFamilies.push_back(WStringToUtf8(name));

						std::transform(name, name + wcslen(name), name, [](wchar_t c) { return static_cast<wchar_t>(std::tolower(c)); });
						supportedFontFamiliesLowerCase.push_back(name);
					}
				}
			}
		}

		Factory::Factory(gmpi::api::IUnknown* pfallback) : Factory_base(concreteInfo, pfallback)
		{
			initFactoryHelper(
				  info.writeFactory
				, info.wicFactory
				, info.d2dFactory
				, info.supportedFontFamilies
				, info.supportedFontFamiliesLowerCase
			);
		}

		gmpi::ReturnCode Factory_base::createPathGeometry(gmpi::drawing::api::IPathGeometry** pathGeometry)
		{
			*pathGeometry = {};

			ID2D1PathGeometry* d2d_geometry{};
			HRESULT hr = info.d2dFactory->CreatePathGeometry(&d2d_geometry);

			if (hr == 0)
			{
				gmpi::shared_ptr<gmpi::api::IUnknown> b2;
				b2.attach(new gmpi::directx::Geometry(d2d_geometry));

				b2->queryInterface(&drawing::api::IPathGeometry::guid, reinterpret_cast<void**>(pathGeometry));
			}

			return toReturnCode(hr);
		}

		gmpi::ReturnCode Factory_base::createTextFormat(const char* fontFamilyName, /* void* unused fontCollection ,*/ gmpi::drawing::FontWeight fontWeight, gmpi::drawing::FontStyle fontStyle, gmpi::drawing::FontStretch fontStretch, float fontSize, /* void* unused2 localeName, */ gmpi::drawing::api::ITextFormat** textFormat)
		{
			*textFormat = {};

			auto fontFamilyNameW = Utf8ToWstring(fontFamilyName);
			std::wstring lowercaseName(fontFamilyNameW);
			std::transform(lowercaseName.begin(), lowercaseName.end(), lowercaseName.begin(), [](wchar_t c) { return static_cast<wchar_t>(std::tolower(c)); });

			if (std::find(info.supportedFontFamiliesLowerCase.begin(), info.supportedFontFamiliesLowerCase.end(), lowercaseName) == info.supportedFontFamiliesLowerCase.end())
			{
				fontFamilyNameW = fontMatchHelper(
					  info.writeFactory
					, info.GdiFontConversions
					, fontFamilyNameW
					, fontWeight
					, fontSize
				);
			}

			IDWriteTextFormat* dwTextFormat{};

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
				b2.attach(new gmpi::directx::TextFormat(info.writeFactory, dwTextFormat));

				b2->queryInterface(&drawing::api::ITextFormat::guid, reinterpret_cast<void**>(textFormat));
			}

			return toReturnCode(hr);
		}

		// 2nd pass - GDI->DirectWrite conversion. "Arial Black" -> "Arial"
		std::wstring fontMatchHelper(
			  IDWriteFactory* writeFactory
			, std::map<std::wstring, std::wstring>& GdiFontConversions
			, std::wstring fontFamilyNameW
			, gmpi::drawing::FontWeight fontWeight
			, float fontSize
		)
		{
			if (auto it = GdiFontConversions.find(fontFamilyNameW); it != GdiFontConversions.end())
			{
				return (*it).second;
			}

			gmpi::directx::ComPtr<IDWriteGdiInterop> interop;
			writeFactory->GetGdiInterop(interop.put());

			LOGFONT lf{};
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

			wcscpy_s(lf.lfFaceName, std::size(lf.lfFaceName), actual_facename);

			if ((int) fontWeight > (int) gmpi::drawing::FontWeight::SemiBold)
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

			gmpi::directx::ComPtr<IDWriteFont> font;
			auto hr = interop->CreateFontFromLOGFONT(&lf, font.put());

			if (font && hr == 0)
			{
				gmpi::directx::ComPtr<IDWriteFontFamily> family;
				font->GetFontFamily(family.put());

				gmpi::directx::ComPtr<IDWriteLocalizedStrings> names;
				family->GetFamilyNames(names.put());

				BOOL exists;
				unsigned int nameIndex;
				names->FindLocaleName(L"en-us", &nameIndex, &exists);
				if (exists)
				{
					wchar_t name[64];
					names->GetString(nameIndex, name, std::size(name));
					std::transform(name, name + wcslen(name), name, [](wchar_t c) { return static_cast<wchar_t>(std::tolower(c));});

					GdiFontConversions.insert(std::pair<std::wstring, std::wstring>(fontFamilyNameW, name));
					fontFamilyNameW = name;
				}
			}

			return fontFamilyNameW;
		}

		gmpi::ReturnCode Factory_base::createImage(int32_t width, int32_t height, gmpi::drawing::api::IBitmap** returnDiBitmap)
		{
			IWICBitmap* wicBitmap{};
			auto hr = info.wicFactory->CreateBitmap(width, height, GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnLoad, &wicBitmap); // pre-muliplied alpha
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

		gmpi::directx::ComPtr<IWICBitmap> loadWicBitmap(IWICImagingFactory* WICFactory, IWICBitmapDecoder* pDecoder)
		{
			gmpi::directx::ComPtr<IWICBitmapFrameDecode> pSource;

			// 2.Retrieve a frame from the image and store the frame in an IWICBitmapFrameDecode object.
			auto hr = pDecoder->GetFrame(0, pSource.put());

			gmpi::directx::ComPtr<IWICFormatConverter> pConverter;
			if (hr == 0)
			{
				// 3.The bitmap must be converted to a format that Direct2D can use.
				hr = WICFactory->CreateFormatConverter(pConverter.put());
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

			gmpi::directx::ComPtr<IWICBitmap> wicBitmap;
			if (hr == 0)
			{
				hr = WICFactory->CreateBitmapFromSource(
					pConverter,
					WICBitmapCacheOnLoad,
					wicBitmap.put());
			}

			return wicBitmap;
		}

		gmpi::ReturnCode Factory_base::loadImageU(const char* uri, drawing::api::IBitmap** returnBitmap)
		{
			*returnBitmap = {};

			HRESULT hr{};
			gmpi::directx::ComPtr<IWICBitmapDecoder> pDecoder;
			gmpi::directx::ComPtr<IWICStream> pIWICStream;

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
						pDecoder.put());               // Pointer to the decoder
				}
			}
			else
#endif
			{
				const auto uriW = Utf8ToWstring(uri);

				// To load a bitmap from a file, first use WIC objects to load the image and to convert it to a Direct2D-compatible format.
				hr = info.wicFactory->CreateDecoderFromFilename(
					uriW.c_str(),
					NULL,
					GENERIC_READ,
					WICDecodeMetadataCacheOnLoad,
					pDecoder.put()
				);
			}

			auto wicBitmap = loadWicBitmap(info.wicFactory, pDecoder.get());

			if (wicBitmap)
			{
				auto bitmap = new Bitmap(this, wicBitmap);
#ifdef _DEBUG
				bitmap->debugFilename = uri;
#endif
				gmpi::shared_ptr<gmpi::drawing::api::IBitmap> b2;
				b2.attach(bitmap);

				// on Windows 7, leave image as-is
//					if (getPlatformPixelFormat() == GmpiDrawing_API::IMpBitmapPixels::kBGRA_SRGB)
				{
					applyPreMultiplyCorrection(wicBitmap.get());
				}

				b2->queryInterface(&drawing::api::IBitmap::guid, (void**)returnBitmap);
			}

			return toReturnCode(hr);
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
						// 8-bit formats
						props.pixelFormat.format = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
						props.pixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;
						// window-7 more uses DXGI_FORMAT_B8G8R8A8_UNORM, not implemented yet.

						hr = nativeContext->CreateBitmapFromWicBitmap(
							diBitmap,
							&props,
							nativeBitmap.put()
						);
					}
					else if(std::memcmp(&formatGuid, &GUID_WICPixelFormat64bppPRGBAHalf, sizeof(formatGuid)) == 0)
					{
						// half-float pixels. default format should be fine.
						hr = nativeContext->CreateBitmapFromWicBitmap(
							diBitmap,
							nativeBitmap.put()
						);
					}
					else
					{
						assert(false); // TODO. Add support for other formats.
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
			*returnInterface = {};

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
				nativeBitmap_ = {};
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
		}

		// WIX currently not premultiplying correctly, so redo it respecting gamma.
		void applyPreMultiplyCorrection(IWICBitmap* bitmap)
		{
			gmpi::directx::ComPtr<IWICBitmapLock> lock;

			// get the full rect of the image
			WICRect rcLock = { 0, 0, 0, 0 };
			bitmap->GetSize((UINT*) &rcLock.Width, (UINT*) &rcLock.Height);
			bitmap->Lock(&rcLock, WICBitmapLockRead|WICBitmapLockWrite, lock.put());

			uint8_t* sourcePixels{};
			UINT stride{};
			UINT bufferSize{};
			lock->GetDataPointer(&bufferSize, &sourcePixels);
			lock->GetStride(&stride);

			size_t totalPixels = rcLock.Height * stride / sizeof(uint32_t);

			constexpr float over255 = 1.0f / 255.0f;
			for (size_t i = 0; i < totalPixels; ++i)
			{
				const int alpha = sourcePixels[3];

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
			*returnSolidColorBrush = {};

			ID2D1SolidColorBrush* b{};
			HRESULT hr = context_->CreateSolidColorBrush(*(D2D1_COLOR_F*)color, &b);

			if (hr == 0)
			{
				gmpi::shared_ptr<gmpi::api::IUnknown> b2;
				b2.attach(new SolidColorBrush(b, factory));

				b2->queryInterface(&drawing::api::ISolidColorBrush::guid, reinterpret_cast<void **>(returnSolidColorBrush));
			}

			return toReturnCode(hr);
		}

		gmpi::ReturnCode GraphicsContext_base::createGradientstopCollection(const drawing::Gradientstop* gradientstops, uint32_t gradientstopsCount, drawing::ExtendMode extendMode, drawing::api::IGradientstopCollection** returnGradientstopCollection)
		{
			*returnGradientstopCollection = {};

			ID2D1GradientStopCollection1* native2{};

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

			return toReturnCode(hr);
		}

		// todo : integer size?
		gmpi::ReturnCode GraphicsContext_base::createCompatibleRenderTarget(drawing::Size desiredSize, gmpi::drawing::api::IBitmapRenderTarget** bitmapRenderTarget)
		{
			*bitmapRenderTarget = {};

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
				const bool use8bit = false; // 8-bit giving wrong gamma but consistent w SE 1.5. 16-bit much nicer and constistant colors with GPU rendering.

				// Create a WIC bitmap to draw on.
				[[maybe_unused]] auto hr =
					wicFactory->CreateBitmap(
						width
						, height
						, use8bit ? GUID_WICPixelFormat32bppPBGRA : GUID_WICPixelFormat64bppPRGBAHalf //GUID_WICPixelFormat128bppRGBAFloat //GUID_WICPixelFormat64bppPRGBAHalf // GUID_WICPixelFormat128bppPRGBAFloat // GUID_WICPixelFormat64bppPRGBAHalf // GUID_WICPixelFormat128bppPRGBAFloat
						, use8bit ? WICBitmapCacheOnDemand : WICBitmapCacheOnLoad
						, returnWicBitmap.put()
					);

				D2D1_RENDER_TARGET_PROPERTIES renderTargetProperties = D2D1::RenderTargetProperties(
					D2D1_RENDER_TARGET_TYPE_DEFAULT,
					D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_UNKNOWN) //use8bit ? D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_UNKNOWN) : D2D1::PixelFormat(DXGI_FORMAT_R16G16B16A16_FLOAT, D2D1_ALPHA_MODE_PREMULTIPLIED)
				);

				gmpi::directx::ComPtr<ID2D1RenderTarget> wikBitmapRenderTarget;
				d2dFactory->CreateWicBitmapRenderTarget(
					returnWicBitmap.get(),
					renderTargetProperties,
					wikBitmapRenderTarget.put()
				);

				returnContext = wikBitmapRenderTarget.as<ID2D1DeviceContext>();
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
				context_ = {};

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
