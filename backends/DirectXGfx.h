#pragma once

/*
#include "DirectXGfx.h"
*/

#include <map>
#include <d2d1_2.h>
#include <dwrite.h>
#include <codecvt>
#include <Wincodec.h>
#include "../Drawing.h"
//#include "./gmpi_gui_hosting.h"
#include "RefCountMacros.h"

// MODIFICATION FOR GMPI_UI !!!

// #define LOG_DIRECTX_CALLS

namespace gmpi
{
	namespace directx
	{
		inline void SafeRelease(IUnknown* object)
		{
			if (object)
				object->Release();
		}

		// Classes without GetFactory()
		template<class MpInterface, class DxType>
		class GmpiDXWrapper : public MpInterface
		{
		protected:
			DxType* native_;

			~GmpiDXWrapper()
			{
				if (native_)
				{
					native_->Release();
//					_RPT1(_CRT_WARN, "Release() -> %x\n", (int)native_);
				}
			}

		public:
			GmpiDXWrapper(DxType* native = nullptr) : native_(native) {}

			inline DxType* native() const
			{
				return native_;
			}
		};

		// Classes with GetFactory()
		template<class MpInterface, class DxType>
		class GmpiDXResourceWrapper : public GmpiDXWrapper<MpInterface, DxType>
		{
		protected:
			gmpi::drawing::api::IFactory* factory_;

		public:
			GmpiDXResourceWrapper(DxType* native, gmpi::drawing::api::IFactory* factory) : GmpiDXWrapper<MpInterface, DxType>(native), factory_(factory) {}
			GmpiDXResourceWrapper(gmpi::drawing::api::IFactory* factory) : factory_(factory) {}

			void GetFactory(gmpi::drawing::api::IFactory **factory) override
			{
				*factory = factory_;
			}
		};

		class Brush : /* public gmpi::drawing::api::IBrush,*/ public GmpiDXResourceWrapper<gmpi::drawing::api::IBrush, ID2D1Brush> // Resource
		{
		public:
			Brush(ID2D1Brush* native, gmpi::drawing::api::IFactory* factory) : GmpiDXResourceWrapper(native, factory) {}

#ifdef LOG_DIRECTX_CALLS
			~Brush()
			{
				_RPT1(_CRT_WARN, "brush%x->Release();\n", (int)this);
				_RPT1(_CRT_WARN, "brush%x = nullptr;\n", (int)this);
			}
#endif

			inline ID2D1Brush* nativeBrush()
			{
				return (ID2D1Brush*)native_;
			}
		};

		class SolidColorBrush final : /* Simulated: public gmpi::drawing::api::ISolidColorBrush,*/ public Brush
		{
		public:
			SolidColorBrush(ID2D1SolidColorBrush* b, gmpi::drawing::api::IFactory *factory) : Brush(b, factory) {}

			inline ID2D1SolidColorBrush* nativeSolidColorBrush()
			{
				return (ID2D1SolidColorBrush*)native_;
			}

			// IMPORTANT: Virtual functions must 100% match simulated interface (gmpi::drawing::api::ISolidColorBrush)
			virtual void SetColor(const gmpi::drawing::Color* color) // simulated: override
			{
//				D2D1::ConvertColorSpace(D2D1::ColorF*) color);
				nativeSolidColorBrush()->SetColor((D2D1::ColorF*) color);
			}
			gmpi::drawing::Color GetColor() // simulated:  override
			{
				auto b = nativeSolidColorBrush()->GetColor();
				//		return gmpi::drawing::Color(b.r, b.g, b.b, b.a);
				gmpi::drawing::Color c;
				c.a = b.a;
				c.r = b.r;
				c.g = b.g;
				c.b = b.b;
				return c;
			}

			//	GMPI_QUERYINTERFACE1(gmpi::drawing::api::SE_IID_SOLIDCOLORBRUSH_MPGUI, gmpi::drawing::api::ISolidColorBrush);

			int32_t queryInterface(const gmpi::MpGuid& iid, void** returnInterface) override
			{
				*returnInterface = 0;
				if (iid == gmpi::drawing::api::SE_IID_SOLIDCOLORBRUSH_MPGUI || iid == gmpi::MP_IID_UNKNOWN)
				{
					// non-standard. Forcing this class (which has the correct vtable) to pretend it's the emulated interface.
					*returnInterface = reinterpret_cast<gmpi::drawing::api::ISolidColorBrush*>(this);
					addRef();
					return gmpi::MP_OK;
				}
				return gmpi::MP_NOSUPPORT;
			}

			GMPI_REFCOUNT;
		};

		class SolidColorBrush_Win7 final : /* Simulated: public gmpi::drawing::api::ISolidColorBrush,*/ public Brush
		{
		public:
			SolidColorBrush_Win7(ID2D1RenderTarget* context, const gmpi::drawing::Color* color, gmpi::drawing::api::IFactory* factory) : Brush(nullptr, factory)
			{
				const gmpi::drawing::Color modified
				{
					se_sdk::FastGamma::pixelToNormalised(se_sdk::FastGamma::float_to_sRGB(color->r)),
					se_sdk::FastGamma::pixelToNormalised(se_sdk::FastGamma::float_to_sRGB(color->g)),
					se_sdk::FastGamma::pixelToNormalised(se_sdk::FastGamma::float_to_sRGB(color->b)),
					color->a
				};
//				modified = gmpi::drawing::Color::Orange;

				/*HRESULT hr =*/ context->CreateSolidColorBrush(*(D2D1_COLOR_F*)&modified, (ID2D1SolidColorBrush**) &native_);
			}

			inline ID2D1SolidColorBrush* nativeSolidColorBrush()
			{
				return (ID2D1SolidColorBrush*)native_;
			}

			// IMPORTANT: Virtual functions must 100% match simulated interface (gmpi::drawing::api::ISolidColorBrush)
			virtual void SetColor(const gmpi::drawing::Color* color) // simulated: override
			{
				//				D2D1::ConvertColorSpace(D2D1::ColorF*) color);
				gmpi::drawing::Color modified
				{
					se_sdk::FastGamma::pixelToNormalised(se_sdk::FastGamma::float_to_sRGB(color->r)),
					se_sdk::FastGamma::pixelToNormalised(se_sdk::FastGamma::float_to_sRGB(color->g)),
					se_sdk::FastGamma::pixelToNormalised(se_sdk::FastGamma::float_to_sRGB(color->b)),
					color->a
				};
				nativeSolidColorBrush()->SetColor((D2D1::ColorF*) &modified);
			}

			virtual gmpi::drawing::Color GetColor() // simulated:  override
			{
				auto b = nativeSolidColorBrush()->GetColor();
				//		return gmpi::drawing::Color(b.r, b.g, b.b, b.a);
				gmpi::drawing::Color c;
				c.a = b.a;
				c.r = b.r;
				c.g = b.g;
				c.b = b.b;
				return c;
			}

			//	GMPI_QUERYINTERFACE1(gmpi::drawing::api::SE_IID_SOLIDCOLORBRUSH_MPGUI, gmpi::drawing::api::ISolidColorBrush);

			int32_t queryInterface(const gmpi::MpGuid& iid, void** returnInterface) override
			{
				*returnInterface = 0;
				if (iid == gmpi::drawing::api::SE_IID_SOLIDCOLORBRUSH_MPGUI || iid == gmpi::MP_IID_UNKNOWN)
				{
					// non-standard. Forcing this class (which has the correct vtable) to pretend it's the emulated interface.
					*returnInterface = reinterpret_cast<gmpi::drawing::api::ISolidColorBrush*>(this);
					addRef();
					return gmpi::MP_OK;
				}
				return gmpi::MP_NOSUPPORT;
			}

			GMPI_REFCOUNT;
		};

		class GradientStopCollection final : public GmpiDXResourceWrapper<gmpi::drawing::api::IGradientstopCollection, ID2D1GradientStopCollection>
		{
		public:
			GradientStopCollection(ID2D1GradientStopCollection* native, gmpi::drawing::api::IFactory* factory) : GmpiDXResourceWrapper(native, factory) {}

			GMPI_QUERYINTERFACE1(gmpi::drawing::api::SE_IID_GRADIENTSTOPCOLLECTION_MPGUI, gmpi::drawing::api::IGradientstopCollection);
			GMPI_REFCOUNT;
		};

		class GradientStopCollection1 final : public GmpiDXResourceWrapper<gmpi::drawing::api::IGradientstopCollection, ID2D1GradientStopCollection1>
		{
		public:
			GradientStopCollection1(ID2D1GradientStopCollection1* native, gmpi::drawing::api::IFactory* factory) : GmpiDXResourceWrapper(native, factory) {}

			GMPI_QUERYINTERFACE1(gmpi::drawing::api::SE_IID_GRADIENTSTOPCOLLECTION_MPGUI, gmpi::drawing::api::IGradientstopCollection);
			GMPI_REFCOUNT;
		};

		class LinearGradientBrush final : /* Simulated: public gmpi::drawing::api::ILinearGradientBrush,*/ public Brush
		{
		public:
			LinearGradientBrush(gmpi::drawing::api::IFactory *factory, ID2D1RenderTarget* context, const gmpi::drawing::api::MP1_LINEAR_GRADIENT_BRUSH_PROPERTIES* linearGradientBrushProperties, const gmpi::drawing::api::MP1_BRUSH_PROPERTIES* brushProperties, const  gmpi::drawing::api::IGradientstopCollection* gradientStopCollection)
			 : Brush(nullptr, factory)
			{
				[[maybe_unused]] HRESULT hr = context->CreateLinearGradientBrush((D2D1_LINEAR_GRADIENT_BRUSH_PROPERTIES*)linearGradientBrushProperties, (D2D1_BRUSH_PROPERTIES*)brushProperties, ((GradientStopCollection*)gradientStopCollection)->native(), (ID2D1LinearGradientBrush **)&native_);
				assert(hr == 0);
			}

			inline ID2D1LinearGradientBrush* native()
			{
				return (ID2D1LinearGradientBrush*)native_;
			}

			// IMPORTANT: Virtual functions must 100% match simulated interface (gmpi::drawing::api::ILinearGradientBrush)
			virtual void SetStartPoint(gmpi::drawing::Point startPoint) // simulated: override
			{
				native()->SetStartPoint(*reinterpret_cast<D2D1_POINT_2F*>(&startPoint));
			}
			virtual void SetEndPoint(gmpi::drawing::Point endPoint) // simulated: override
			{
				native()->SetEndPoint(*reinterpret_cast<D2D1_POINT_2F*>(&endPoint));
			}

			//	GMPI_QUERYINTERFACE1(gmpi::drawing::api::SE_IID_LINEARGRADIENTBRUSH_MPGUI, gmpi::drawing::api::ILinearGradientBrush);
			int32_t queryInterface(const gmpi::MpGuid& iid, void** returnInterface) override
			{
				*returnInterface = 0;
				if (iid == gmpi::drawing::api::SE_IID_LINEARGRADIENTBRUSH_MPGUI || iid == gmpi::MP_IID_UNKNOWN)
				{
					// non-standard. Forcing this class (which has the correct vtable) to pretend it's the emulated interface.
					*returnInterface = reinterpret_cast<gmpi::drawing::api::ILinearGradientBrush*>(this);
					addRef();
					return gmpi::MP_OK;
				}
				return gmpi::MP_NOSUPPORT;
			}

			GMPI_REFCOUNT;
		};

		class RadialGradientBrush final : /* Simulated: public gmpi::drawing::api::IRadialGradientBrush,*/ public Brush
		{
		public:
			RadialGradientBrush(gmpi::drawing::api::IFactory *factory, ID2D1RenderTarget* context, const gmpi::drawing::api::MP1_RADIAL_GRADIENT_BRUSH_PROPERTIES* linearGradientBrushProperties, const gmpi::drawing::api::MP1_BRUSH_PROPERTIES* brushProperties, const gmpi::drawing::api::IGradientstopCollection* gradientStopCollection)
			 : Brush(nullptr, factory)
			{
				[[maybe_unused]] HRESULT hr = context->CreateRadialGradientBrush((D2D1_RADIAL_GRADIENT_BRUSH_PROPERTIES*)linearGradientBrushProperties, (D2D1_BRUSH_PROPERTIES*)brushProperties, ((GradientStopCollection*)gradientStopCollection)->native(), (ID2D1RadialGradientBrush **)&native_);
				assert(hr == 0);
			}

			inline ID2D1RadialGradientBrush* native()
			{
				return (ID2D1RadialGradientBrush*)native_;
			}

			// IMPORTANT: Virtual functions must 100% match simulated interface.
			virtual void SetCenter(gmpi::drawing::Point center)  // simulated: override
			{
				native()->SetCenter(*reinterpret_cast<D2D1_POINT_2F*>(&center));
			}

			virtual void SetGradientOriginOffset(gmpi::drawing::Point gradientOriginOffset) // simulated: override
			{
				native()->SetGradientOriginOffset(*reinterpret_cast<D2D1_POINT_2F*>(&gradientOriginOffset));
			}

			virtual void SetRadiusX(float radiusX) // simulated: override
			{
				native()->SetRadiusX(radiusX);
			}

			virtual void SetRadiusY(float radiusY) // simulated: override
			{
				native()->SetRadiusY(radiusY);
			}

			int32_t queryInterface(const gmpi::MpGuid& iid, void** returnInterface) override
			{
				*returnInterface = 0;
				if (iid == gmpi::drawing::api::SE_IID_RADIALGRADIENTBRUSH_MPGUI || iid == gmpi::MP_IID_UNKNOWN)
				{
					// non-standard. Forcing this class (which has the correct vtable) to pretend it's the emulated interface.
					*returnInterface = reinterpret_cast<gmpi::drawing::api::IRadialGradientBrush*>(this);
					addRef();
					return gmpi::MP_OK;
				}
				return gmpi::MP_NOSUPPORT;
			}

			GMPI_REFCOUNT;
		};

		class StrokeStyle final : public GmpiDXResourceWrapper<gmpi::drawing::api::IStrokeStyle, ID2D1StrokeStyle>
		{
		public:
			StrokeStyle(ID2D1StrokeStyle* native, gmpi::drawing::api::IFactory* factory) : GmpiDXResourceWrapper(native, factory) {}

			gmpi::drawing::CapStyle GetStartCap() override
			{
				return (gmpi::drawing::CapStyle) native()->GetStartCap();
			}

			gmpi::drawing::CapStyle GetEndCap() override
			{
				return (gmpi::drawing::CapStyle) native()->GetEndCap();
			}

			gmpi::drawing::CapStyle GetDashCap() override
			{
				return (gmpi::drawing::CapStyle) native()->GetDashCap();
			}

			float GetMiterLimit() override
			{
				return native()->GetMiterLimit();
			}

			gmpi::drawing::api::MP1_LINE_JOIN GetLineJoin() override
			{
				return (gmpi::drawing::api::MP1_LINE_JOIN) native()->GetLineJoin();
			}

			float GetDashOffset() override
			{
				return native()->GetDashOffset();
			}

			gmpi::drawing::api::MP1_DASH_STYLE GetDashStyle() override
			{
				return (gmpi::drawing::api::MP1_DASH_STYLE) native()->GetDashStyle();
			}

			uint32_t GetDashesCount() override
			{
				return native()->GetDashesCount();
			}

			void GetDashes(float* dashes, uint32_t dashesCount) override
			{
				return native()->GetDashes(dashes, dashesCount);
			}

			GMPI_QUERYINTERFACE1(gmpi::drawing::api::SE_IID_STROKESTYLE_MPGUI, gmpi::drawing::api::IStrokeStyle);
			GMPI_REFCOUNT;
		};

		inline ID2D1StrokeStyle* toNative(const gmpi::drawing::api::IStrokeStyle* strokeStyle)
		{
			if (strokeStyle)
			{
				return ((StrokeStyle*)strokeStyle)->native();
			}
			return nullptr;
		}

#if 0
		class TessellationSink final : public GmpiDXWrapper<gmpi::drawing::api::ITessellationSink, ID2D1TessellationSink>
		{
		public:
			TessellationSink(ID2D1Mesh* mesh)
			{
				[[maybe_unused]] HRESULT hr = mesh->Open(&native_);
				assert(hr == S_OK);
			}

			void AddTriangles(const gmpi::drawing::api::MP1_TRIANGLE* triangles, uint32_t trianglesCount) override
			{
				native_->AddTriangles((const D2D1_TRIANGLE*) triangles, trianglesCount);
			}

			int32_t Close() override
			{
				native_->Close();
				return MP_OK;
			}

			GMPI_QUERYINTERFACE1(gmpi::drawing::api::SE_IID_TESSELLATIONSINK_MPGUI, gmpi::drawing::api::ITessellationSink);
			GMPI_REFCOUNT;
		};
		
		class Mesh final : public GmpiDXResourceWrapper<gmpi::drawing::api::IMesh, ID2D1Mesh>
		{
		public:
			Mesh(gmpi::drawing::api::IFactory* factory, ID2D1RenderTarget* context) :
				GmpiDXResourceWrapper(factory)
			{
				[[maybe_unused]] HRESULT hr = context->CreateMesh(&native_);
				assert(hr == S_OK);
			}

			// IMpMesh
			int32_t Open(gmpi::drawing::api::ITessellationSink** returnObject) override
			{
				*returnObject = nullptr;
				gmpi_sdk::mp_shared_ptr<gmpi::IMpUnknown> wrapper;
				wrapper.Attach(new TessellationSink(native_));
				return wrapper->queryInterface(gmpi::drawing::api::SE_IID_TESSELLATIONSINK_MPGUI, reinterpret_cast<void **>(returnObject));
			}

			GMPI_QUERYINTERFACE1(gmpi::drawing::api::SE_IID_MESH_MPGUI, gmpi::drawing::api::IMesh);
			GMPI_REFCOUNT;
		};
#endif
		class TextFormat final : public GmpiDXWrapper<gmpi::drawing::api::ITextFormat, IDWriteTextFormat>
		{
			std::wstring_convert<std::codecvt_utf8<wchar_t>>* stringConverter = {}; // constructed once is much faster.
			bool useLegacyBaseLineSnapping = true;
			float topAdjustment = {};
			float fontMetrics_ascent = {};

			void CalculateTopAdjustment()
			{
				assert(topAdjustment == 0.0f); // else boundingBoxSize calculation will be affected, and won't be actual native size.

				gmpi::drawing::api::MP1_FONT_METRICS fontMetrics;
				GetFontMetrics(&fontMetrics);

				gmpi::drawing::Size boundingBoxSize;
				GetTextExtentU("A", 1, &boundingBoxSize);

				topAdjustment = boundingBoxSize.height - (fontMetrics.ascent + fontMetrics.descent);
				fontMetrics_ascent = fontMetrics.ascent;
			}

		public:
			TextFormat(std::wstring_convert<std::codecvt_utf8<wchar_t>>* pstringConverter, IDWriteTextFormat* native) :
				GmpiDXWrapper<gmpi::drawing::api::ITextFormat, IDWriteTextFormat>(native)
				, stringConverter(pstringConverter)
			{
				CalculateTopAdjustment();
			}
#ifdef LOG_DIRECTX_CALLS
			~TextFormat()
			{
				_RPT1(_CRT_WARN, "textformat%x->Release();\n", (int)this);
				_RPT1(_CRT_WARN, "textformat%x = nullptr;\n", (int)this);
			}
#endif

			int32_t SetTextAlignment(gmpi::drawing::api::MP1_TEXT_ALIGNMENT textAlignment) override
			{
				native()->SetTextAlignment((DWRITE_TEXT_ALIGNMENT)textAlignment);
				return gmpi::MP_OK;
			}

			int32_t SetParagraphAlignment(gmpi::drawing::api::MP1_PARAGRAPH_ALIGNMENT paragraphAlignment) override
			{
				native()->SetParagraphAlignment((DWRITE_PARAGRAPH_ALIGNMENT)paragraphAlignment);
				return gmpi::MP_OK;
			}

			int32_t SetWordWrapping(gmpi::drawing::api::MP1_WORD_WRAPPING wordWrapping) override
			{
				return native()->SetWordWrapping((DWRITE_WORD_WRAPPING)wordWrapping);
			}

			int32_t SetLineSpacing(float lineSpacing, float baseline) override
			{
				// Hack, reuse this method to enable legacy-mode.
				if (static_cast<float>(IMpTextFormat::ImprovedVerticalBaselineSnapping) == lineSpacing)
				{
					useLegacyBaseLineSnapping = false;
					return gmpi::MP_OK;
				}

				// For the default method, spacing depends solely on the content. For uniform spacing, the specified line height overrides the content.
				DWRITE_LINE_SPACING_METHOD method = lineSpacing < 0.0f ? DWRITE_LINE_SPACING_METHOD_DEFAULT : DWRITE_LINE_SPACING_METHOD_UNIFORM;
				return native()->SetLineSpacing(method, fabsf(lineSpacing), baseline);
			}

			int32_t GetFontMetrics(gmpi::drawing::api::MP1_FONT_METRICS* returnFontMetrics) override;

			// TODO!!!: Probably needs to accept constraint rect like DirectWrite. !!!
			//	void GetTextExtentU(const char* utf8String, int32_t stringLength, gmpi::drawing::Size& returnSize)
			void GetTextExtentU(const char* utf8String, int32_t stringLength, gmpi::drawing::Size* returnSize) override;

			float getTopAdjustment() const
			{
				return topAdjustment;
			}

			float getAscent() const
			{
				return fontMetrics_ascent;
			}

			bool getUseLegacyBaseLineSnapping() const
			{
				return useLegacyBaseLineSnapping;
			}

			GMPI_QUERYINTERFACE1(gmpi::drawing::api::SE_IID_TEXTFORMAT_MPGUI, gmpi::drawing::api::ITextFormat);
			GMPI_REFCOUNT;
		};

		class bitmapPixels : public gmpi::drawing::api::IBitmapPixels
		{
			bool alphaPremultiplied;
			IWICBitmap* bitmap;
			UINT bytesPerRow;
			BYTE *ptr;
			IWICBitmapLock* pBitmapLock;
			ID2D1Bitmap* nativeBitmap_;
			int flags;
			IMpBitmapPixels::PixelFormat pixelFormat = kBGRA; // default to non-SRGB Win7 (not tested)

		public:
			bitmapPixels(ID2D1Bitmap* nativeBitmap, IWICBitmap* inBitmap, bool _alphaPremultiplied, int32_t pflags)
			{
				nativeBitmap_ = nativeBitmap;
				assert(inBitmap);

				UINT w, h;
				inBitmap->GetSize(&w, &h);

				{
					WICPixelFormatGUID formatGuid;
					inBitmap->GetPixelFormat(&formatGuid);

					// premultiplied BGRA (default)
					if (std::memcmp(&formatGuid, &GUID_WICPixelFormat32bppPBGRA, sizeof(formatGuid)) == 0)
					{
						pixelFormat = kBGRA_SRGB;
					}
				}

				bitmap = nullptr;
				pBitmapLock = nullptr;
				WICRect rcLock = { 0, 0, (INT)w, (INT)h };
				flags = pflags;

				if (0 <= inBitmap->Lock(&rcLock, flags, &pBitmapLock))
				{
					pBitmapLock->GetStride(&bytesPerRow);
					UINT bufferSize;
					pBitmapLock->GetDataPointer(&bufferSize, &ptr);

					bitmap = inBitmap;
					bitmap->AddRef();

					alphaPremultiplied = _alphaPremultiplied;
					if (!alphaPremultiplied)
						unpremultiplyAlpha();
				}
			}

			~bitmapPixels()
			{
				if (!alphaPremultiplied)
					premultiplyAlpha();

				if (nativeBitmap_)
				{
#if 1
					if (0 != (flags & gmpi::drawing::api::MP1_BITMAP_LOCK_WRITE))
					{
						D2D1_RECT_U r;
						r.left = r.top = 0;
						bitmap->GetSize(&r.right, &r.bottom);

						nativeBitmap_->CopyFromMemory(&r, ptr, bytesPerRow);
					}
#else
					nativeBitmap_->Release();
					nativeBitmap_ = nullptr;
#endif
				}

				SafeRelease(pBitmapLock);
				SafeRelease(bitmap);
			}

			virtual uint8_t* getAddress() const override { return ptr; }
			int32_t getBytesPerRow() const override{ return bytesPerRow; }
			int32_t getPixelFormat() const override
			{
				return pixelFormat;
/*
				nativeBitmap_->GetPixelFormat().format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB ? kBGRA_SRGB : kRGBA;

				WICPixelFormatGUID pixelFormat = 0;
				bitmap->GetPixelFormat(&pixelFormat);

				return pixelFormat == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB ? kBGRA_SRGB : kRGBA;
*/
			}

			inline uint8_t fast8bitScale(uint8_t a, uint8_t b)
			{
				int t = (int)a * (int)b;
				return (uint8_t)((t + 1 + (t >> 8)) >> 8); // fast way to divide by 255
			}

			void premultiplyAlpha()
			{
				UINT w, h;
				bitmap->GetSize(&w, &h);
				int totalPixels = h * bytesPerRow / sizeof(uint32_t);

				uint8_t* pixel = ptr;

				for (int i = 0; i < totalPixels; ++i)
				{
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

					pixel += sizeof(uint32_t);
				}
			}

			//-----------------------------------------------------------------------------
			void unpremultiplyAlpha()
			{
				UINT w, h;
				bitmap->GetSize(&w, &h);
				int totalPixels = h * bytesPerRow / sizeof(uint32_t);

				uint8_t* pixel = ptr;

				for (int i = 0; i < totalPixels; ++i)
				{
					if (pixel[3] != 0)
					{
						pixel[0] = (uint32_t)(pixel[0] * 255) / pixel[3];
						pixel[1] = (uint32_t)(pixel[1] * 255) / pixel[3];
						pixel[2] = (uint32_t)(pixel[2] * 255) / pixel[3];
					}
					pixel += sizeof(uint32_t);
				}
			}

			GMPI_QUERYINTERFACE1(gmpi::drawing::api::SE_IID_BITMAP_PIXELS_MPGUI, gmpi::drawing::api::IBitmapPixels);
			GMPI_REFCOUNT;
		};

		class Bitmap : public gmpi::drawing::api::IBitmap
		{
		public:
			ID2D1Bitmap* nativeBitmap_;
			ID2D1DeviceContext* nativeContext_;
			IWICBitmap* diBitmap_ = {};
			class Factory* factory;
#ifdef _DEBUG
			std::string debugFilename;
#endif
			Bitmap(Factory* pfactory, IWICBitmap* diBitmap);

			Bitmap(Factory* pfactory, ID2D1DeviceContext* nativeContext, ID2D1Bitmap* nativeBitmap) :
				nativeBitmap_(nativeBitmap)
				, nativeContext_(nativeContext)
				, factory(pfactory)
			{
				nativeBitmap->AddRef();
			}

			~Bitmap()
			{
				if (nativeBitmap_)
				{
					nativeBitmap_->Release();
				}
				if (diBitmap_)
				{
					diBitmap_->Release();
				}
			}

			ID2D1Bitmap* GetNativeBitmap(ID2D1DeviceContext* nativeContext);

			gmpi::drawing::Size GetSizeF() override
			{
				gmpi::drawing::Size returnSize;
				UINT w, h;
				diBitmap_->GetSize(&w, &h);
				returnSize.width = (float)w;
				returnSize.height = (float)h;

				return returnSize;
			}

			int32_t GetSize(gmpi::drawing::api::MP1_SIZE_U* returnSize) override
			{
				diBitmap_->GetSize(&returnSize->width, &returnSize->height);

				return gmpi::MP_OK;
			}

			int32_t lockPixelsOld(gmpi::drawing::api::IBitmapPixels** returnInterface, bool alphaPremultiplied) override
			{
				*returnInterface = 0;

				gmpi_sdk::mp_shared_ptr<gmpi::IMpUnknown> b2;
				b2.Attach(new bitmapPixels(nativeBitmap_, diBitmap_, alphaPremultiplied, gmpi::drawing::api::MP1_BITMAP_LOCK_READ | gmpi::drawing::api::MP1_BITMAP_LOCK_WRITE));

				return b2->queryInterface(gmpi::drawing::api::SE_IID_BITMAP_PIXELS_MPGUI, (void**)(returnInterface));
			}

			int32_t lockPixels(gmpi::drawing::api::IBitmapPixels** returnInterface, int32_t flags) override;

			void ApplyAlphaCorrection() override{} // deprecated
//			void ApplyAlphaCorrection_win7();
			void ApplyPreMultiplyCorrection();

			void GetFactory(gmpi::drawing::api::IFactory** pfactory) override;

			GMPI_QUERYINTERFACE1(gmpi::drawing::api::SE_IID_BITMAP_MPGUI, gmpi::drawing::api::IBitmap);
			GMPI_REFCOUNT;
		};

		class BitmapBrush final : /* Simulated: public gmpi::drawing::api::IBitmapBrush,*/ public Brush
		{
		public:
			BitmapBrush(
				gmpi::drawing::api::IFactory *factory,
				ID2D1DeviceContext* context,
				const gmpi::drawing::api::IBitmap* bitmap,
				const gmpi::drawing::api::MP1_BITMAP_BRUSH_PROPERTIES* bitmapBrushProperties,
				const gmpi::drawing::api::MP1_BRUSH_PROPERTIES* brushProperties
			)
			 : Brush(nullptr, factory)
			{
				auto bm = ((Bitmap*)bitmap);
				auto nativeBitmap = bm->GetNativeBitmap(context);

				[[maybe_unused]] const auto hr = context->CreateBitmapBrush(nativeBitmap, (D2D1_BITMAP_BRUSH_PROPERTIES*)bitmapBrushProperties, (D2D1_BRUSH_PROPERTIES*)brushProperties, (ID2D1BitmapBrush**)&native_);
				assert(hr == 0);
			}

			inline ID2D1BitmapBrush* native()
			{
				return (ID2D1BitmapBrush*)native_;
			}

			// IMPORTANT: Virtual functions must 100% match simulated interface.
			virtual void SetExtendModeX(gmpi::drawing::api::MP1_EXTEND_MODE extendModeX)
			{
				native()->SetExtendModeX((D2D1_EXTEND_MODE)extendModeX);
			}

			virtual void SetExtendModeY(gmpi::drawing::api::MP1_EXTEND_MODE extendModeY)
			{
				native()->SetExtendModeY((D2D1_EXTEND_MODE)extendModeY);
			}

			virtual void SetInterpolationMode(gmpi::drawing::api::MP1_BITMAP_INTERPOLATION_MODE interpolationMode)
			{
				native()->SetInterpolationMode((D2D1_BITMAP_INTERPOLATION_MODE)interpolationMode);
			}

			int32_t queryInterface(const gmpi::MpGuid& iid, void** returnInterface) override
			{
				*returnInterface = 0;
				if (iid == gmpi::drawing::api::SE_IID_BITMAPBRUSH_MPGUI || iid == gmpi::MP_IID_UNKNOWN)
				{
					// non-standard. Forcing this class (which has the correct vtable) to pretend it's the emulated interface.
					*returnInterface = reinterpret_cast<gmpi::drawing::api::ILinearGradientBrush*>(this);
					addRef();
					return gmpi::MP_OK;
				}
				return gmpi::MP_NOSUPPORT;
			}

			GMPI_REFCOUNT;
		};

		class GeometrySink : public gmpi::drawing::api::IGeometrySink2
		{
			ID2D1GeometrySink* geometrysink_;

		public:
			GeometrySink(ID2D1GeometrySink* context) : geometrysink_(context) {}
			~GeometrySink()
			{
				if (geometrysink_)
				{
					geometrysink_->Release();

#ifdef LOG_DIRECTX_CALLS
					_RPT1(_CRT_WARN, "sink%x->Release();\n", (int)this);
					_RPT1(_CRT_WARN, "sink%x = nullptr;\n", (int)this);
#endif
				}
			}
			void SetFillMode(gmpi::drawing::api::MP1_FILL_MODE fillMode) override
			{
				geometrysink_->SetFillMode((D2D1_FILL_MODE)fillMode);
			}
#if 0
			void SetSegmentFlags(gmpi::drawing::api::MP1_PATH_SEGMENT vertexFlags) override
			{
				geometrysink_->SetSegmentFlags((D2D1_PATH_SEGMENT)vertexFlags);
			}
#endif
			void BeginFigure(gmpi::drawing::Point startPoint, gmpi::drawing::api::MP1_FIGURE_BEGIN figureBegin) override
			{
#ifdef LOG_DIRECTX_CALLS
				_RPT4(_CRT_WARN, "sink%x->BeginFigure(D2D1::Point2F(%f,%f), (D2D1_FIGURE_BEGIN)%d);\n", (int)this, startPoint.x, startPoint.y, figureBegin);
#endif
				geometrysink_->BeginFigure(*reinterpret_cast<D2D1_POINT_2F*>(&startPoint), (D2D1_FIGURE_BEGIN)figureBegin);
			}
			void AddLines(const gmpi::drawing::Point* points, uint32_t pointsCount) override
			{
				geometrysink_->AddLines(reinterpret_cast<const D2D1_POINT_2F*>(points), pointsCount);
			}
			void AddBeziers(const gmpi::drawing::api::MP1_BEZIER_SEGMENT* beziers, uint32_t beziersCount) override
			{
				geometrysink_->AddBeziers(reinterpret_cast<const D2D1_BEZIER_SEGMENT*>(beziers), beziersCount);
			}
			void EndFigure(gmpi::drawing::api::MP1_FIGURE_END figureEnd) override
			{
#ifdef LOG_DIRECTX_CALLS
				_RPT2(_CRT_WARN, "sink%x->EndFigure((D2D1_FIGURE_END)%d);\n", (int)this, figureEnd);
#endif
				geometrysink_->EndFigure((D2D1_FIGURE_END)figureEnd);
			}
			int32_t Close() override
			{
#ifdef LOG_DIRECTX_CALLS
				_RPT1(_CRT_WARN, "sink%x->Close();\n", (int)this);
#endif
				auto hr = geometrysink_->Close();
				return hr == 0 ? (gmpi::MP_OK) : (gmpi::MP_FAIL);
			}
			void AddLine(gmpi::drawing::Point point) override
			{
#ifdef LOG_DIRECTX_CALLS
				_RPT4(_CRT_WARN, "sink%x->AddLine(D2D1::Point2F(%f,%f));\n", (int)this, point.x, point.y);
#endif
				geometrysink_->AddLine(*reinterpret_cast<D2D1_POINT_2F*>(&point));
			}
			void AddBezier(const gmpi::drawing::api::MP1_BEZIER_SEGMENT* bezier) override
			{
				geometrysink_->AddBezier(reinterpret_cast<const D2D1_BEZIER_SEGMENT*>(bezier));
			}
			void AddQuadraticBezier(const gmpi::drawing::api::MP1_QUADRATIC_BEZIER_SEGMENT* bezier) override
			{
				geometrysink_->AddQuadraticBezier(reinterpret_cast<const D2D1_QUADRATIC_BEZIER_SEGMENT*>(bezier));
			}
			void AddQuadraticBeziers(const gmpi::drawing::api::MP1_QUADRATIC_BEZIER_SEGMENT* beziers, uint32_t beziersCount) override
			{
				geometrysink_->AddQuadraticBeziers(reinterpret_cast<const D2D1_QUADRATIC_BEZIER_SEGMENT*>(beziers), beziersCount);
			}
			void AddArc(const gmpi::drawing::api::MP1_ARC_SEGMENT* arc) override
			{
				geometrysink_->AddArc(reinterpret_cast<const D2D1_ARC_SEGMENT*>(arc));
			}

			int32_t queryInterface(const gmpi::MpGuid& iid, void** returnInterface) override
			{
				if (iid == gmpi::drawing::api::SE_IID_GEOMETRYSINK2_MPGUI || iid == gmpi::drawing::api::SE_IID_GEOMETRYSINK_MPGUI || iid == gmpi::MP_IID_UNKNOWN)
				{
					*returnInterface = reinterpret_cast<void*>(static_cast<gmpi::drawing::api::IGeometrySink2*>(this));
					addRef();
					return gmpi::MP_OK;
				}

				*returnInterface = 0;
				return gmpi::MP_NOSUPPORT;
			}
			GMPI_REFCOUNT;
		};


		class Geometry : public gmpi::drawing::api::IPathGeometry
		{
			friend class GraphicsContext;

			ID2D1PathGeometry* geometry_;

		public:
			Geometry(ID2D1PathGeometry* context) : geometry_(context)
			{}
			~Geometry()
			{
				if (geometry_)
				{
					geometry_->Release();
#ifdef LOG_DIRECTX_CALLS
					_RPT1(_CRT_WARN, "geometry%x->Release();\n", (int)this);
					_RPT1(_CRT_WARN, "geometry%x = nullptr;\n", (int)this);
#endif
				}
			}

			ID2D1PathGeometry* native()
			{
				return geometry_;
			}

			int32_t Open(gmpi::drawing::api::IGeometrySink** geometrySink) override;
			void GetFactory(gmpi::drawing::api::IFactory** factory) override
			{
				//		native_->GetFactory((ID2D1Factory**)factory);
			}

			int32_t StrokeContainsPoint(gmpi::drawing::Point point, float strokeWidth, gmpi::drawing::api::IStrokeStyle* strokeStyle, const gmpi::drawing::api::MP1_MATRIX_3X2* worldTransform, bool* returnContains) override
			{
				BOOL result = FALSE;
				geometry_->StrokeContainsPoint(*(D2D1_POINT_2F*)&point, strokeWidth, toNative(strokeStyle), (const D2D1_MATRIX_3X2_F *)worldTransform, &result);
				*returnContains = result == TRUE;

				return gmpi::MP_OK;
			}
			int32_t FillContainsPoint(gmpi::drawing::Point point, const gmpi::drawing::api::MP1_MATRIX_3X2* worldTransform, bool* returnContains) override
			{
				BOOL result = FALSE;
				geometry_->FillContainsPoint(*(D2D1_POINT_2F*)&point, (const D2D1_MATRIX_3X2_F *)worldTransform, &result);
				*returnContains = result == TRUE;

				return gmpi::MP_OK;
			}
			int32_t GetWidenedBounds(float strokeWidth, gmpi::drawing::api::IStrokeStyle* strokeStyle, const gmpi::drawing::api::MP1_MATRIX_3X2* worldTransform, gmpi::drawing::api::MP1_RECT* returnBounds) override
			{
				geometry_->GetWidenedBounds(strokeWidth, toNative(strokeStyle), (const D2D1_MATRIX_3X2_F *)worldTransform, (D2D_RECT_F*)returnBounds);
				return gmpi::MP_OK;
			}

			GMPI_QUERYINTERFACE1(gmpi::drawing::api::SE_IID_PATHGEOMETRY_MPGUI, gmpi::drawing::api::IPathGeometry);
			GMPI_REFCOUNT;
		};


		class Factory : public gmpi::drawing::api::IFactory2
		{
			ID2D1Factory1* m_pDirect2dFactory = {};
			IDWriteFactory* writeFactory = {};
			IWICImagingFactory* pIWICFactory = {};
			std::vector<std::wstring> supportedFontFamiliesLowerCase;
			std::vector<std::string> supportedFontFamilies;
			std::map<std::wstring, std::wstring> GdiFontConversions;
			bool DX_support_sRGB = true;

		public:
			static std::wstring_convert<std::codecvt_utf8<wchar_t>> stringConverter; // cached, as constructor is super-slow.

			// for diagnostics only.
			auto getDirectWriteFactory()
			{
				return writeFactory;
			}
			auto getFactory()
			{
				return m_pDirect2dFactory;
			}

			void setSrgbSupport(bool s)
			{
				DX_support_sRGB = s;
			}
			
			gmpi::drawing::api::IBitmapPixels::PixelFormat getPlatformPixelFormat()
			{
				return DX_support_sRGB ? gmpi::drawing::api::IBitmapPixels::kBGRA_SRGB : gmpi::drawing::api::IBitmapPixels::kBGRA;
			}

			Factory();
			void Init(ID2D1Factory1* existingFactory = nullptr);
			~Factory();

			ID2D1Factory1* getD2dFactory()
			{
				return m_pDirect2dFactory;
			}
			std::wstring fontMatch(std::wstring fontName, gmpi::drawing::api::MP1_FONT_WEIGHT fontWeight, float fontSize);

			int32_t CreatePathGeometry(gmpi::drawing::api::IPathGeometry** pathGeometry) override;
			int32_t CreateTextFormat(const char* fontFamilyName, void* unused /* fontCollection */, gmpi::drawing::api::MP1_FONT_WEIGHT fontWeight, gmpi::drawing::api::MP1_FONT_STYLE fontStyle, gmpi::drawing::api::MP1_FONT_STRETCH fontStretch, float fontSize, void* unused2 /* localeName */, gmpi::drawing::api::ITextFormat** textFormat) override;
			int32_t CreateImage(int32_t width, int32_t height, gmpi::drawing::api::IBitmap** returnDiBitmap) override;
			int32_t LoadImageU(const char* utf8Uri, gmpi::drawing::api::IBitmap** returnDiBitmap) override;
			int32_t CreateStrokeStyle(const gmpi::drawing::api::MP1_STROKE_STYLE_PROPERTIES* strokeStyleProperties, float* dashes, int32_t dashesCount, gmpi::drawing::api::IStrokeStyle** returnValue) override
			{
				*returnValue = nullptr;

				ID2D1StrokeStyle* b = nullptr;

				auto hr = m_pDirect2dFactory->CreateStrokeStyle((const D2D1_STROKE_STYLE_PROPERTIES*) strokeStyleProperties, dashes, dashesCount, &b);

				if (hr == 0)
				{
					gmpi_sdk::mp_shared_ptr<gmpi::IMpUnknown> wrapper;
					wrapper.Attach(new StrokeStyle(b, this));

//					auto wrapper = gmpi_sdk::make_shared_ptr<StrokeStyle>(b, this);

					return wrapper->queryInterface(gmpi::drawing::api::SE_IID_STROKESTYLE_MPGUI, reinterpret_cast<void**>(returnValue));
				}

				return hr == 0 ? (gmpi::MP_OK) : (gmpi::MP_FAIL);
			}

			IWICBitmap* CreateDiBitmapFromNative(ID2D1Bitmap* D2D_Bitmap);

			// IMpFactory2
			int32_t GetFontFamilyName(int32_t fontIndex, gmpi::IString* returnString) override;

			int32_t queryInterface(const gmpi::MpGuid& iid, void** returnInterface) override
			{
				*returnInterface = 0;
				if ( iid == gmpi::drawing::api::SE_IID_FACTORY2_MPGUI || iid == gmpi::drawing::api::SE_IID_FACTORY_MPGUI || iid == gmpi::MP_IID_UNKNOWN)
				{
					*returnInterface = reinterpret_cast<gmpi::drawing::api::IFactory2*>(this);
					addRef();
					return gmpi::MP_OK;
				}
				return gmpi::MP_NOSUPPORT;
			}

			GMPI_REFCOUNT_NO_DELETE;
		};

		class GraphicsContext : public gmpi::drawing::api::IDeviceContext
		{
		protected:
			ID2D1DeviceContext* context_;

			Factory* factory;
			std::vector<gmpi::drawing::api::MP1_RECT> clipRectStack;
			std::wstring_convert<std::codecvt_utf8<wchar_t>>* stringConverter; // cached, as constructor is super-slow.

			void Init()
			{
				stringConverter = &(factory->stringConverter);
			}

		public:
			GraphicsContext(ID2D1DeviceContext* deviceContext, Factory* pfactory) :
				context_(deviceContext)
				, factory(pfactory)
			{
				context_->AddRef();
				Init();
			}

			GraphicsContext(Factory* pfactory) :
				context_(nullptr)
				, factory(pfactory)
			{
				Init();
			}

			~GraphicsContext()
			{
				context_->Release();
			}

			ID2D1DeviceContext* native()
			{
				return context_;
			}

			void GetFactory(gmpi::drawing::api::IFactory** pfactory) override
			{
				*pfactory = factory;
			}

			void DrawRectangle(const gmpi::drawing::api::MP1_RECT* rect, const gmpi::drawing::api::IBrush* brush, float strokeWidth, const gmpi::drawing::api::IStrokeStyle* strokeStyle) override
			{
				context_->DrawRectangle(D2D1::RectF(rect->left, rect->top, rect->right, rect->bottom), ((Brush*)brush)->nativeBrush(), strokeWidth, toNative(strokeStyle) );
			}

			void FillRectangle(const gmpi::drawing::api::MP1_RECT* rect, const gmpi::drawing::api::IBrush* brush) override
			{
				context_->FillRectangle((D2D1_RECT_F*)rect, (ID2D1Brush*)((Brush*)brush)->nativeBrush());
			}

			void Clear(const gmpi::drawing::Color* clearColor) override
			{
#ifdef LOG_DIRECTX_CALLS
				_RPT0(_CRT_WARN, "{\n");
				_RPT4(_CRT_WARN, "auto c = D2D1::ColorF(%.3ff, %.3ff, %.3ff, %.3ff);\n", clearColor->r, clearColor->g, clearColor->b, clearColor->a);
				_RPT0(_CRT_WARN, "context_->Clear(c);\n");
				_RPT0(_CRT_WARN, "}\n");
#endif
				context_->Clear((D2D1_COLOR_F*)clearColor);
			}

			void DrawLine(gmpi::drawing::Point point0, gmpi::drawing::Point point1, const gmpi::drawing::api::IBrush* brush, float strokeWidth, const gmpi::drawing::api::IStrokeStyle* strokeStyle) override
			{
				context_->DrawLine(*((D2D_POINT_2F*)&point0), *((D2D_POINT_2F*)&point1), ((Brush*)brush)->nativeBrush(), strokeWidth, toNative(strokeStyle));
			}

			void DrawGeometry(const gmpi::drawing::api::IPathGeometry* geometry, const gmpi::drawing::api::IBrush* brush, float strokeWidth = 1.0f, const gmpi::drawing::api::IStrokeStyle* strokeStyle = 0) override;

			void FillGeometry(const gmpi::drawing::api::IPathGeometry* geometry, const gmpi::drawing::api::IBrush* brush, const gmpi::drawing::api::IBrush* opacityBrush) override
			{
#ifdef LOG_DIRECTX_CALLS
				_RPT3(_CRT_WARN, "context_->FillGeometry(geometry%x, brush%x, nullptr);\n", (int)geometry, (int)brush);
#endif
				auto d2d_geometry = ((Geometry*)geometry)->geometry_;

				ID2D1Brush* opacityBrushNative;
				if (opacityBrush)
				{
					opacityBrushNative = ((Brush*)brush)->nativeBrush();
				}
				else
				{
					opacityBrushNative = nullptr;
				}

				context_->FillGeometry(d2d_geometry, ((Brush*)brush)->nativeBrush(), opacityBrushNative);
			}

			//void FillMesh(const gmpi::drawing::api::IMesh* mesh, const gmpi::drawing::api::IBrush* brush) override
			//{
			//	auto nativeMesh = ((Mesh*)mesh)->native();
			//	context_->FillMesh(nativeMesh, ((Brush*)brush)->nativeBrush());
			//}

			void DrawTextU(const char* utf8String, int32_t stringLength, const gmpi::drawing::api::ITextFormat* textFormat, const gmpi::drawing::api::MP1_RECT* layoutRect, const gmpi::drawing::api::IBrush* brush, int32_t flags) override;

			//	void DrawBitmap( gmpi::drawing::api::IBitmap* mpBitmap, gmpi::drawing::Rect destinationRectangle, float opacity, int32_t interpolationMode, gmpi::drawing::Rect sourceRectangle) override
			void DrawBitmap(const gmpi::drawing::api::IBitmap* mpBitmap, const gmpi::drawing::api::MP1_RECT* destinationRectangle, float opacity, /* MP1_BITMAP_INTERPOLATION_MODE*/ int32_t interpolationMode, const gmpi::drawing::api::MP1_RECT* sourceRectangle) override
			{
				auto bm = ((Bitmap*)mpBitmap);
				auto bitmap = bm->GetNativeBitmap(context_);
				if (bitmap)
				{
					context_->DrawBitmap(
						bitmap,
						(D2D1_RECT_F*)destinationRectangle,
						opacity,
						(D2D1_BITMAP_INTERPOLATION_MODE) interpolationMode,
						(D2D1_RECT_F*)sourceRectangle
					);
				}
			}

			void SetTransform(const gmpi::drawing::api::MP1_MATRIX_3X2* transform) override
			{
#ifdef LOG_DIRECTX_CALLS
				_RPT0(_CRT_WARN, "{\n");
				_RPT4(_CRT_WARN, "auto t = D2D1::Matrix3x2F(%.3f, %.3f, %.3f, %.3f, ", transform->_11, transform->_12, transform->_21, transform->_22);
				_RPT4(_CRT_WARN, "%.3f, %.3f);\n", transform->_31, transform->_32);
				_RPT0(_CRT_WARN, "context_->SetTransform(t);\n");
				_RPT0(_CRT_WARN, "}\n");
#endif
				context_->SetTransform(reinterpret_cast<const D2D1_MATRIX_3X2_F*>(transform));
			}

			void GetTransform(gmpi::drawing::api::MP1_MATRIX_3X2* transform) override
			{
#ifdef LOG_DIRECTX_CALLS
				_RPT0(_CRT_WARN, "{\n");
				_RPT0(_CRT_WARN, "D2D1_MATRIX_3X2_F t;\n");
				_RPT0(_CRT_WARN, "context_->GetTransform(&t);\n");
				_RPT0(_CRT_WARN, "}\n");
#endif
				context_->GetTransform(reinterpret_cast<D2D1_MATRIX_3X2_F*>(transform));
			}

			int32_t CreateSolidColorBrush(const gmpi::drawing::Color* color, gmpi::drawing::api::ISolidColorBrush **solidColorBrush) override;

			int32_t CreateGradientStopCollection(const gmpi::drawing::api::MP1_GRADIENT_STOP* gradientStops, uint32_t gradientStopsCount, /* gmpi::drawing::api::MP1_GAMMA colorInterpolationGamma, gmpi::drawing::api::MP1_EXTEND_MODE extendMode,*/ gmpi::drawing::api::IGradientstopCollection** gradientStopCollection) override;

			template <typename T>
			int32_t make_wrapped(gmpi::IMpUnknown* object, const gmpi::MpGuid& iid, T** returnObject)
			{
				*returnObject = nullptr;
				gmpi_sdk::mp_shared_ptr<gmpi::IMpUnknown> b2;
				b2.Attach(object);
				return b2->queryInterface(iid, reinterpret_cast<void**>(returnObject));
			};

			int32_t CreateLinearGradientBrush(const gmpi::drawing::api::MP1_LINEAR_GRADIENT_BRUSH_PROPERTIES* linearGradientBrushProperties, const gmpi::drawing::api::MP1_BRUSH_PROPERTIES* brushProperties, const  gmpi::drawing::api::IGradientstopCollection* gradientStopCollection, gmpi::drawing::api::ILinearGradientBrush** linearGradientBrush) override
			{
				//*linearGradientBrush = nullptr;
				//gmpi_sdk::mp_shared_ptr<gmpi::IMpUnknown> b2;
				//b2.Attach(new LinearGradientBrush(factory, context_, linearGradientBrushProperties, brushProperties, gradientStopCollection));
				//return b2->queryInterface(gmpi::drawing::api::SE_IID_LINEARGRADIENTBRUSH_MPGUI, reinterpret_cast<void **>(linearGradientBrush));

				return make_wrapped(
					new LinearGradientBrush(factory, context_, linearGradientBrushProperties, brushProperties, gradientStopCollection),
					gmpi::drawing::api::SE_IID_LINEARGRADIENTBRUSH_MPGUI,
					linearGradientBrush);
			}

			int32_t CreateBitmapBrush(const gmpi::drawing::api::IBitmap* bitmap, const gmpi::drawing::api::MP1_BITMAP_BRUSH_PROPERTIES* bitmapBrushProperties, const gmpi::drawing::api::MP1_BRUSH_PROPERTIES* brushProperties, gmpi::drawing::api::IBitmapBrush** returnBrush) override
			{
				*returnBrush = nullptr;
				gmpi_sdk::mp_shared_ptr<gmpi::IMpUnknown> b2;
				b2.Attach(new BitmapBrush(factory, context_, bitmap, bitmapBrushProperties, brushProperties));
				return b2->queryInterface(gmpi::drawing::api::SE_IID_BITMAPBRUSH_MPGUI, reinterpret_cast<void **>(returnBrush));
			}
			int32_t CreateRadialGradientBrush(const gmpi::drawing::api::MP1_RADIAL_GRADIENT_BRUSH_PROPERTIES* radialGradientBrushProperties, const gmpi::drawing::api::MP1_BRUSH_PROPERTIES* brushProperties, const gmpi::drawing::api::IGradientstopCollection* gradientStopCollection, gmpi::drawing::api::IRadialGradientBrush** radialGradientBrush) override
			{
				*radialGradientBrush = nullptr;
				gmpi_sdk::mp_shared_ptr<gmpi::IMpUnknown> b2;
				b2.Attach(new RadialGradientBrush(factory, context_, radialGradientBrushProperties, brushProperties, gradientStopCollection));
				return b2->queryInterface(gmpi::drawing::api::SE_IID_RADIALGRADIENTBRUSH_MPGUI, reinterpret_cast<void **>(radialGradientBrush));
			}

			int32_t CreateCompatibleRenderTarget(const gmpi::drawing::Size* desiredSize, gmpi::drawing::api::IBitmapRenderTarget** bitmapRenderTarget) override;

			void DrawRoundedRectangle(const gmpi::drawing::api::MP1_ROUNDED_RECT* roundedRect, const gmpi::drawing::api::IBrush* brush, float strokeWidth, const gmpi::drawing::api::IStrokeStyle* strokeStyle) override
			{
				context_->DrawRoundedRectangle((D2D1_ROUNDED_RECT*)roundedRect, (ID2D1Brush*)((Brush*)brush)->nativeBrush(), (FLOAT)strokeWidth, toNative(strokeStyle));
			}

//			int32_t CreateMesh(gmpi::drawing::api::IMesh** returnObject) override;

			void FillRoundedRectangle(const gmpi::drawing::api::MP1_ROUNDED_RECT* roundedRect, const gmpi::drawing::api::IBrush* brush) override
			{
				context_->FillRoundedRectangle((D2D1_ROUNDED_RECT*)roundedRect, (ID2D1Brush*)((Brush*)brush)->nativeBrush());
			}

			void DrawEllipse(const gmpi::drawing::api::MP1_ELLIPSE* ellipse, const gmpi::drawing::api::IBrush* brush, float strokeWidth, const gmpi::drawing::api::IStrokeStyle* strokeStyle) override
			{
				context_->DrawEllipse((D2D1_ELLIPSE*)ellipse, (ID2D1Brush*)((Brush*)brush)->nativeBrush(), (FLOAT)strokeWidth, toNative(strokeStyle));
			}

			void FillEllipse(const gmpi::drawing::api::MP1_ELLIPSE* ellipse, const gmpi::drawing::api::IBrush* brush) override
			{
				context_->FillEllipse((D2D1_ELLIPSE*)ellipse, (ID2D1Brush*)((Brush*)brush)->nativeBrush());
			}

			void PushAxisAlignedClip(const gmpi::drawing::api::MP1_RECT* clipRect/*, gmpi::drawing::api::MP1_ANTIALIAS_MODE antialiasMode*/) override;

			void PopAxisAlignedClip() override
			{
//				_RPT0(_CRT_WARN, "                 PopAxisAlignedClip()\n");
#ifdef LOG_DIRECTX_CALLS
				_RPT0(_CRT_WARN, "context_->PopAxisAlignedClip();\n");
#endif
				context_->PopAxisAlignedClip();
				clipRectStack.pop_back();
			}

			void GetAxisAlignedClip(gmpi::drawing::api::MP1_RECT* returnClipRect) override;

			void BeginDraw() override
			{
#ifdef LOG_DIRECTX_CALLS
				_RPT0(_CRT_WARN, "\n\n// ==================================================\n");
				_RPT0(_CRT_WARN, "context_->BeginDraw();\n");
#endif
				context_->BeginDraw();
			}

			int32_t EndDraw() override
			{
#ifdef LOG_DIRECTX_CALLS
				_RPT0(_CRT_WARN, "context_->EndDraw();\n");
#endif
				auto hr = context_->EndDraw();

				return hr == S_OK ? (gmpi::MP_OK) : (gmpi::MP_FAIL);
			}

//			int32_t GetUpdateRegion(gmpi::drawing::api::IUpdateRegion** returnUpdateRegion) override;

			//	void InsetNewMethodHere(){}

			bool SupportSRGB()
			{
				return factory->getPlatformPixelFormat() == gmpi::drawing::api::IBitmapPixels::kBGRA_SRGB;
			}

			GMPI_QUERYINTERFACE1(gmpi::drawing::api::SE_IID_DEVICECONTEXT_MPGUI, gmpi::drawing::api::IDeviceContext);
			GMPI_REFCOUNT_NO_DELETE;
		};

		class BitmapRenderTarget : public GraphicsContext
		{
			ID2D1BitmapRenderTarget* nativeBitmapRenderTarget = {};

		public:
			BitmapRenderTarget(GraphicsContext* g, const gmpi::drawing::Size* desiredSize, Factory* pfactory) :
				GraphicsContext(pfactory)
			{
				/* auto hr = */ g->native()->CreateCompatibleRenderTarget(*(D2D1_SIZE_F*)desiredSize, &nativeBitmapRenderTarget);
				nativeBitmapRenderTarget->QueryInterface(IID_ID2D1DeviceContext, (void**)&context_);

				clipRectStack.push_back({ 0, 0, desiredSize->width, desiredSize->height });
			}

			~BitmapRenderTarget()
			{
				if(nativeBitmapRenderTarget)
				{
					nativeBitmapRenderTarget->Release();
				}
			}

			// HACK, to be ABI compatible with IMpBitmapRenderTarget we need this virtual function,
			// and it needs to be in the vtable right after all virtual functions of GraphicsContext
			virtual int32_t GetBitmap(gmpi::drawing::api::IBitmap** returnBitmap);

			int32_t queryInterface(const gmpi::MpGuid& iid, void** returnInterface) override
			{
				*returnInterface = 0;
				if (iid == gmpi::drawing::api::SE_IID_BITMAP_RENDERTARGET_MPGUI)
				{
					// non-standard. Forcing this class (which has the correct vtable) to pretend it's the emulated interface.
					*returnInterface = reinterpret_cast<gmpi::drawing::api::IBitmapRenderTarget*>(this);
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

		// Direct2D context tailored to devices without sRGB high-color support. i.e. Windows 7.
		class GraphicsContext_Win7 : public GraphicsContext
		{
		public:

			GraphicsContext_Win7(ID2D1DeviceContext* context, Factory* pfactory) :
				GraphicsContext(context, pfactory)
			{}

			int32_t CreateSolidColorBrush(const gmpi::drawing::Color* color, gmpi::drawing::api::ISolidColorBrush **solidColorBrush) override
			{
				*solidColorBrush = nullptr;
				gmpi_sdk::mp_shared_ptr<gmpi::IMpUnknown> b;
				b.Attach(new SolidColorBrush_Win7(context_, color, factory));
				return b->queryInterface(gmpi::drawing::api::SE_IID_SOLIDCOLORBRUSH_MPGUI, reinterpret_cast<void **>(solidColorBrush));
			}

			int32_t CreateGradientStopCollection(const gmpi::drawing::api::MP1_GRADIENT_STOP* gradientStops, uint32_t gradientStopsCount, /* gmpi::drawing::api::MP1_GAMMA colorInterpolationGamma, gmpi::drawing::api::MP1_EXTEND_MODE extendMode,*/ gmpi::drawing::api::IGradientstopCollection** gradientStopCollection) override
			{
				// Adjust gradient gamma.
				std::vector<gmpi::drawing::api::MP1_GRADIENT_STOP> stops;
				stops.assign(gradientStopsCount, gmpi::drawing::api::MP1_GRADIENT_STOP());

				for(uint32_t i = 0 ; i < gradientStopsCount ; ++i)
				{
					auto& srce = gradientStops[i];
					auto& dest = stops[i];
					dest.position = srce.position;
					dest.color.a = srce.color.a;
					dest.color.r = se_sdk::FastGamma::pixelToNormalised(se_sdk::FastGamma::float_to_sRGB(srce.color.r));
					dest.color.g = se_sdk::FastGamma::pixelToNormalised(se_sdk::FastGamma::float_to_sRGB(srce.color.g));
					dest.color.b = se_sdk::FastGamma::pixelToNormalised(se_sdk::FastGamma::float_to_sRGB(srce.color.b));
				}

				return GraphicsContext::CreateGradientStopCollection(stops.data(), gradientStopsCount, gradientStopCollection);
			}

			void Clear(const gmpi::drawing::Color* clearColor) override
			{
				gmpi::drawing::Color color(*clearColor);
				color.r = se_sdk::FastGamma::pixelToNormalised(se_sdk::FastGamma::float_to_sRGB(color.r));
				color.g = se_sdk::FastGamma::pixelToNormalised(se_sdk::FastGamma::float_to_sRGB(color.g));
				color.b = se_sdk::FastGamma::pixelToNormalised(se_sdk::FastGamma::float_to_sRGB(color.b));
				context_->Clear((D2D1_COLOR_F*)&color);
			}
		};
	} // Namespace
} // Namespace