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
	virtual ~GmpiDXResourceWrapper() {}
	gmpi::drawing::api::IFactory* factory_;

public:
	GmpiDXResourceWrapper(DxType* native, gmpi::drawing::api::IFactory* factory) : GmpiDXWrapper<MpInterface, DxType>(native), factory_(factory) {}
	GmpiDXResourceWrapper(gmpi::drawing::api::IFactory* factory) : factory_(factory) {}

	gmpi::ReturnCode getFactory(gmpi::drawing::api::IFactory** factory) override
	{
		*factory = factory_;
	}
};













































class TextFormat final : public GmpiDXWrapper<gmpi::drawing::api::ITextFormat, IDWriteTextFormat>
{
	std::wstring_convert<std::codecvt_utf8<wchar_t>>* stringConverter = {}; // constructed once is much faster.
	bool useLegacyBaseLineSnapping = true;
	float topAdjustment = {};
	float fontMetrics_ascent = {};

	void CalculateTopAdjustment()
	{
		assert(topAdjustment == 0.0f); // else boundingBoxSize calculation will be affected, and won't be actual native size.

		gmpi::drawing::FontMetrics fontMetrics;
		getFontMetrics(&fontMetrics);

		gmpi::drawing::Size boundingBoxSize;
		getTextExtentU("A", 1, &boundingBoxSize);

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

	gmpi::ReturnCode setTextAlignment(gmpi::drawing::TextAlignment textAlignment) override
	{
        const auto r = native()->SetTextAlignment((DWRITE_TEXT_ALIGNMENT) textAlignment);
        return r == S_OK ? gmpi::ReturnCode::Ok : gmpi::ReturnCode::Fail;
	}

	gmpi::ReturnCode setParagraphAlignment(gmpi::drawing::ParagraphAlignment paragraphAlignment) override
	{
        const auto r = native()->SetParagraphAlignment((DWRITE_PARAGRAPH_ALIGNMENT) paragraphAlignment);
        return r == S_OK ? gmpi::ReturnCode::Ok : gmpi::ReturnCode::Fail;
	}

	gmpi::ReturnCode setWordWrapping(gmpi::drawing::WordWrapping wordWrapping) override
	{
        const auto r = native()->SetWordWrapping((DWRITE_WORD_WRAPPING) wordWrapping);
        return r == S_OK ? gmpi::ReturnCode::Ok : gmpi::ReturnCode::Fail;
	}

	// TODO!!!: Probably needs to accept constraint rect like DirectWrite. !!!
	gmpi::ReturnCode getTextExtentU(const char* utf8String, int32_t stringLength, gmpi::drawing::Size* returnSize) override;

	gmpi::ReturnCode getFontMetrics(gmpi::drawing::FontMetrics* returnFontMetrics) override;

	gmpi::ReturnCode setLineSpacing(float lineSpacing, float baseline) override
	{
		// Hack, reuse this method to enable legacy-mode.
		if (static_cast<float>(ITextFormat::ImprovedVerticalBaselineSnapping) == lineSpacing)
		{
			useLegacyBaseLineSnapping = false;
			return gmpi::ReturnCode::Ok;
		}

		// For the default method, spacing depends solely on the content. For uniform spacing, the specified line height overrides the content.
		DWRITE_LINE_SPACING_METHOD method = lineSpacing < 0.0f ? DWRITE_LINE_SPACING_METHOD_DEFAULT : DWRITE_LINE_SPACING_METHOD_UNIFORM;
		const auto r = native()->SetLineSpacing(method, fabsf(lineSpacing), baseline);
        return r == S_OK ? gmpi::ReturnCode::Ok : gmpi::ReturnCode::Fail;
	}

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

	GMPI_QUERYINTERFACE_NEW(gmpi::drawing::api::ITextFormat);
	GMPI_REFCOUNT;
};
#if 0
class Resource final : public GmpiDXWrapper<gmpi::drawing::api::IResource, ID2D1Resource>
{
public:
	gmpi::ReturnCode getFactory(IFactory** returnGetFactory) override
	{
        const auto r = native()->getFactory((D2D1_GET_FACTORY) returnGetFactory);
		return r == S_OK ? gmpi::ReturnCode::Ok : gmpi::ReturnCode::Fail;
	}

	GMPI_QUERYINTERFACE_NEW(gmpi::drawing::api::IResource);
	GMPI_REFCOUNT;
};
#endif
class BitmapPixels final : public gmpi::drawing::api::IBitmapPixels
{
	bool alphaPremultiplied;
	IWICBitmap* bitmap;
	UINT bytesPerRow;
	BYTE* ptr;
	IWICBitmapLock* pBitmapLock;
	ID2D1Bitmap* nativeBitmap_;
	int flags;
	IBitmapPixels::PixelFormat pixelFormat = kBGRA; // default to non-SRGB Win7 (not tested)

public:
	BitmapPixels(ID2D1Bitmap* nativeBitmap, IWICBitmap* inBitmap, bool _alphaPremultiplied, int32_t pflags)
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

	~BitmapPixels()
	{
		if (!alphaPremultiplied)
			premultiplyAlpha();

		if (nativeBitmap_)
		{
#if 1
			if (0 != (flags & (int32_t) gmpi::drawing::BitmapLockFlags::Write))
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

	gmpi::ReturnCode getAddress(uint8_t** returnGetAddress) override
	{
		*returnGetAddress = ptr;
		return gmpi::ReturnCode::Ok;
	}

	gmpi::ReturnCode getBytesPerRow(int32_t* returnGetBytesPerRow) override
	{
		*returnGetBytesPerRow = bytesPerRow;
		return gmpi::ReturnCode::Ok;
	}

	gmpi::ReturnCode getPixelFormat(int32_t* returnGetPixelFormat) override
	{
		*returnGetPixelFormat = pixelFormat;
		return gmpi::ReturnCode::Ok;
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

    GMPI_QUERYINTERFACE_NEW(gmpi::drawing::api::IBitmapPixels);
	GMPI_REFCOUNT;
};

class Bitmap final : public gmpi::drawing::api::IBitmap
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

    gmpi::ReturnCode getSizeU(drawing::SizeU* returnSize) override
	{
		const auto r = diBitmap_->GetSize(&returnSize->width, &returnSize->height);
        return r == S_OK ? gmpi::ReturnCode::Ok : gmpi::ReturnCode::Fail;
	}

	gmpi::ReturnCode lockPixels(gmpi::drawing::api::IBitmapPixels** returnInterface, int32_t flags) override;

	void ApplyPreMultiplyCorrection();

//	gmpi::ReturnCode getFactory(gmpi::drawing::api::IFactory** pfactory) override;
	gmpi::ReturnCode getFactory(gmpi::drawing::api::IFactory** returnFactory) override;

    GMPI_QUERYINTERFACE_NEW(gmpi::drawing::api::IBitmap);
	GMPI_REFCOUNT;
};

class GradientStopCollection final : public GmpiDXResourceWrapper<gmpi::drawing::api::IGradientstopCollection, ID2D1GradientStopCollection>
{
public:
	GradientStopCollection(ID2D1GradientStopCollection* native, gmpi::drawing::api::IFactory* factory) : GmpiDXResourceWrapper(native, factory) {}

	GMPI_QUERYINTERFACE_NEW(gmpi::drawing::api::IGradientstopCollection);
	GMPI_REFCOUNT;
};

class GradientStopCollection1 final : public GmpiDXResourceWrapper<gmpi::drawing::api::IGradientstopCollection, ID2D1GradientStopCollection1>
{
public:
	GradientStopCollection1(ID2D1GradientStopCollection1* native, gmpi::drawing::api::IFactory* factory) : GmpiDXResourceWrapper(native, factory) {}

	GMPI_QUERYINTERFACE_NEW(gmpi::drawing::api::IGradientstopCollection);
	GMPI_REFCOUNT;
};

class Brush : public gmpi::drawing::api::IBrush/*, public GmpiDXResourceWrapper<gmpi::drawing::api::IBrush, ID2D1Brush>*/ // Resource
{
protected:
	ID2D1Brush* native_ = {};
	gmpi::drawing::api::IFactory* factory_ = {};

public:
	Brush(ID2D1Brush* native, gmpi::drawing::api::IFactory* factory) : native_(native), factory_(factory) {}

#ifdef LOG_DIRECTX_CALLS
	~Brush()
	{
		_RPT1(_CRT_WARN, "brush%x->Release();\n", (int)this);
		_RPT1(_CRT_WARN, "brush%x = nullptr;\n", (int)this);
	}
#endif

	ID2D1Brush* native()
	{
		return native_;
	}

	// IResource
	gmpi::ReturnCode getFactory(gmpi::drawing::api::IFactory** factory) override
	{
		*factory = factory_;
	}

    GMPI_QUERYINTERFACE_NEW(gmpi::drawing::api::IBrush);
};

class BitmapBrush final : /* Simulated: public gmpi::drawing::api::IBitmapBrush,*/ public Brush
{
public:
	BitmapBrush(
		gmpi::drawing::api::IFactory* factory,
		ID2D1DeviceContext* context,
		const gmpi::drawing::api::IBitmap* bitmap,
		const gmpi::drawing::BitmapBrushProperties* bitmapBrushProperties,
		const gmpi::drawing::BrushProperties* brushProperties
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
	virtual void setExtendModeX(gmpi::drawing::ExtendMode extendModeX)
	{
		native()->SetExtendModeX((D2D1_EXTEND_MODE)extendModeX);
	}

	virtual void setExtendModeY(gmpi::drawing::ExtendMode extendModeY)
	{
		native()->SetExtendModeY((D2D1_EXTEND_MODE)extendModeY);
	}

	virtual void setInterpolationMode(gmpi::drawing::BitmapInterpolationMode interpolationMode)
	{
		native()->SetInterpolationMode((D2D1_BITMAP_INTERPOLATION_MODE)interpolationMode);
	}

	gmpi::ReturnCode queryInterface(const gmpi::api::Guid* iid, void** returnInterface) override
	{
		*returnInterface = {};
		if (*iid == gmpi::drawing::api::IBitmapBrush::guid || *iid == gmpi::api::IUnknown::guid)
		{
			// non-standard. Forcing this class (which has the correct vtable) to pretend it's the emulated interface.
			*returnInterface = reinterpret_cast<gmpi::drawing::api::IBitmapBrush*>(this);
			addRef();
			return gmpi::ReturnCode::Ok;
		}
		return gmpi::ReturnCode::NoSupport;
	}

	GMPI_REFCOUNT;
};

class SolidColorBrush final : /* Simulated: public gmpi::drawing::api::ISolidColorBrush,*/ public Brush
{
public:
	SolidColorBrush(ID2D1SolidColorBrush* b, gmpi::drawing::api::IFactory *factory) : Brush(b, factory) {}

	ID2D1SolidColorBrush* nativeSolidColorBrush()
	{
		return (ID2D1SolidColorBrush*)native();
	}

	// IMPORTANT: Virtual functions must 100% match simulated interface (gmpi::drawing::api::ISolidColorBrush)
	virtual void setColor(const gmpi::drawing::Color* color) // simulated: override
	{
//				D2D1::ConvertColorSpace(D2D1::ColorF*) color);
		nativeSolidColorBrush()->SetColor((D2D1::ColorF*) color);
	}
	virtual gmpi::drawing::Color GetColor() // simulated:  override
	{
		const auto b = nativeSolidColorBrush()->GetColor();
		return 
		{
			b.a,
			b.r,
			b.g,
			b.b
		};
	}

	gmpi::ReturnCode queryInterface(const gmpi::api::Guid* iid, void** returnInterface) override
	{
		*returnInterface = {};
		if (*iid == gmpi::drawing::api::ISolidColorBrush::guid)
		{
			// non-standard. Forcing this class (which has the correct vtable) to pretend it's the emulated interface.
			*returnInterface = reinterpret_cast<gmpi::drawing::api::ISolidColorBrush*>(this);
			addRef();
			return gmpi::ReturnCode::Ok;
		}
		return Brush::queryInterface(iid, returnInterface);
	}

	GMPI_REFCOUNT;
};

class SolidColorBrush_Win7 final : /* Simulated: ISolidColorBrush,*/ public Brush
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
		return (ID2D1SolidColorBrush*)native();
	}

	// IMPORTANT: Virtual functions must 100% match simulated interface (gmpi::drawing::api::ISolidColorBrush)
	virtual void setColor(const gmpi::drawing::Color* color) // simulated: override
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

	gmpi::ReturnCode queryInterface(const gmpi::api::Guid* iid, void** returnInterface) override
	{
		*returnInterface = {};
		if (*iid == gmpi::drawing::api::ISolidColorBrush::guid)
		{
			// non-standard. Forcing this class (which has the correct vtable) to pretend it's the emulated interface.
			*returnInterface = reinterpret_cast<gmpi::drawing::api::ISolidColorBrush*>(this);
			addRef();
			return gmpi::ReturnCode::Ok;
		}
		return Brush::queryInterface(iid, returnInterface);
	}
	GMPI_REFCOUNT;
};

class LinearGradientBrush final : /* Simulated: ILinearGradientBrush,*/ public Brush
{
public:
	LinearGradientBrush(gmpi::drawing::api::IFactory *factory, ID2D1RenderTarget* context, const gmpi::drawing::LinearGradientBrushProperties* linearGradientBrushProperties, const gmpi::drawing::BrushProperties* brushProperties, const  gmpi::drawing::api::IGradientstopCollection* gradientStopCollection)
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
	virtual void setStartPoint(gmpi::drawing::Point startPoint) // simulated: override
	{
		native()->SetStartPoint(*reinterpret_cast<D2D1_POINT_2F*>(&startPoint));
	}
	virtual void setEndPoint(gmpi::drawing::Point endPoint) // simulated: override
	{
		native()->SetEndPoint(*reinterpret_cast<D2D1_POINT_2F*>(&endPoint));
	}

	//	GMPI_QUERYINTERFACE1(gmpi::drawing::api::SE_IID_LINEARGRADIENTBRUSH_MPGUI, gmpi::drawing::api::ILinearGradientBrush);
	gmpi::ReturnCode queryInterface(const gmpi::api::Guid* iid, void** returnInterface) override
	{
		*returnInterface = {};
		if (*iid == gmpi::drawing::api::ILinearGradientBrush::guid || *iid == gmpi::api::IUnknown::guid)
		{
			// non-standard. Forcing this class (which has the correct vtable) to pretend it's the emulated interface.
			*returnInterface = reinterpret_cast<gmpi::drawing::api::ILinearGradientBrush*>(this);
			addRef();
			return gmpi::ReturnCode::Ok;
		}
		return gmpi::ReturnCode::NoSupport;
	}

	GMPI_REFCOUNT;
};

class RadialGradientBrush final : /* Simulated: IRadialGradientBrush,*/ public Brush
{
public:
	RadialGradientBrush(gmpi::drawing::api::IFactory *factory, ID2D1RenderTarget* context, const gmpi::drawing::RadialGradientBrushProperties* linearGradientBrushProperties, const gmpi::drawing::BrushProperties* brushProperties, const gmpi::drawing::api::IGradientstopCollection* gradientStopCollection)
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
	virtual void setCenter(gmpi::drawing::Point center)  // simulated: override
	{
		native()->SetCenter(*reinterpret_cast<D2D1_POINT_2F*>(&center));
	}

	virtual void setGradientOriginOffset(gmpi::drawing::Point gradientOriginOffset) // simulated: override
	{
		native()->SetGradientOriginOffset(*reinterpret_cast<D2D1_POINT_2F*>(&gradientOriginOffset));
	}

	virtual void setRadiusX(float radiusX) // simulated: override
	{
		native()->SetRadiusX(radiusX);
	}

	virtual void setRadiusY(float radiusY) // simulated: override
	{
		native()->SetRadiusY(radiusY);
	}

	gmpi::ReturnCode queryInterface(const gmpi::api::Guid* iid, void** returnInterface) override
	{
		*returnInterface = {};
		if (*iid == gmpi::drawing::api::IRadialGradientBrush::guid || *iid == gmpi::api::IUnknown::guid)
		{
			// non-standard. Forcing this class (which has the correct vtable) to pretend it's the emulated interface.
			*returnInterface = reinterpret_cast<gmpi::drawing::api::IRadialGradientBrush*>(this);
			addRef();
			return gmpi::ReturnCode::Ok;
		}
		return gmpi::ReturnCode::NoSupport;
	}

	GMPI_REFCOUNT;
};

class StrokeStyle final : public GmpiDXResourceWrapper<gmpi::drawing::api::IStrokeStyle, ID2D1StrokeStyle>
{
public:
	StrokeStyle(ID2D1StrokeStyle* native, gmpi::drawing::api::IFactory* factory) : GmpiDXResourceWrapper(native, factory) {}

#if 0
	gmpi::drawing::CapStyle GetStartCap() override
	{
		return (gmpi::drawing::CapStyle) native()->getStartCap();
	}

	gmpi::drawing::CapStyle GetEndCap() override
	{
		return (gmpi::drawing::CapStyle) native()->getEndCap();
	}

	gmpi::drawing::CapStyle GetDashCap() override
	{
		return (gmpi::drawing::CapStyle) native()->getDashCap();
	}

	float GetMiterLimit() override
	{
		return native()->getMiterLimit();
	}

	gmpi::drawing::api::MP1_LINE_JOIN GetLineJoin() override
	{
		return (gmpi::drawing::api::MP1_LINE_JOIN) native()->getLineJoin();
	}

	float GetDashOffset() override
	{
		return native()->getDashOffset();
	}

	gmpi::drawing::api::MP1_DASH_STYLE GetDashStyle() override
	{
		return (gmpi::drawing::api::MP1_DASH_STYLE) native()->getDashStyle();
	}

	uint32_t GetDashesCount() override
	{
		return native()->getDashesCount();
	}

	void GetDashes(float* dashes, uint32_t dashesCount) override
	{
		return native()->getDashes(dashes, dashesCount);
	}
#endif

	GMPI_QUERYINTERFACE_NEW(gmpi::drawing::api::IStrokeStyle);
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

	// IMesh
	int32_t Open(gmpi::drawing::api::ITessellationSink** returnObject) override
	{
		*returnObject = nullptr;
		gmpi::shared_ptr<gmpi::api::IUnknown> wrapper;
		wrapper.Attach(new TessellationSink(native_));
		return wrapper->queryInterface(gmpi::drawing::api::SE_IID_TESSELLATIONSINK_MPGUI, reinterpret_cast<void **>(returnObject));
	}

	GMPI_QUERYINTERFACE1(gmpi::drawing::api::SE_IID_MESH_MPGUI, gmpi::drawing::api::IMesh);
	GMPI_REFCOUNT;
};
#endif

class GeometrySink : public gmpi::drawing::api::IGeometrySink
{
	ID2D1GeometrySink* geometrysink_;

	auto native()
	{
		return geometrysink_;
	}

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
	void setFillMode(gmpi::drawing::FillMode fillMode) override
	{
		native()->SetFillMode((D2D1_FILL_MODE)fillMode);
	}

	void beginFigure(const gmpi::drawing::Point startPoint, gmpi::drawing::FigureBegin figureBegin) override
	{
		native()->BeginFigure(*reinterpret_cast<const D2D1_POINT_2F*>(&startPoint), (D2D1_FIGURE_BEGIN)figureBegin);
	}

	void addLines(const gmpi::drawing::Point* points, uint32_t pointsCount) override
	{
		native()->AddLines(reinterpret_cast<const D2D1_POINT_2F*>(points), pointsCount);
	}

	void addBeziers(const gmpi::drawing::BezierSegment* beziers, uint32_t beziersCount) override
	{
		native()->AddBeziers(reinterpret_cast<const D2D1_BEZIER_SEGMENT*>(beziers), beziersCount);
	}

	void endFigure(gmpi::drawing::FigureEnd figureEnd) override
	{
		native()->EndFigure((D2D1_FIGURE_END)figureEnd);
	}

	gmpi::ReturnCode close() override
	{
		const auto r = native()->Close();
		return r == S_OK ? gmpi::ReturnCode::Ok : gmpi::ReturnCode::Fail;
	}

	void addLine(gmpi::drawing::Point point) override //TODO ref? forwarding ref? pointer?
	{
		native()->AddLine(*reinterpret_cast<const D2D1_POINT_2F*>(&point));
	}

	void addBezier(const gmpi::drawing::BezierSegment* bezier) override
	{
		native()->AddBezier(reinterpret_cast<const D2D1_BEZIER_SEGMENT*>(bezier));
	}

	void addQuadraticBezier(const gmpi::drawing::QuadraticBezierSegment* bezier) override
	{
		native()->AddQuadraticBezier(reinterpret_cast<const D2D1_QUADRATIC_BEZIER_SEGMENT*>(bezier));
	}

	void addQuadraticBeziers(const gmpi::drawing::QuadraticBezierSegment* beziers, uint32_t beziersCount) override
	{
		native()->AddQuadraticBeziers(reinterpret_cast<const D2D1_QUADRATIC_BEZIER_SEGMENT*>(beziers), beziersCount);
	}

	void addArc(const gmpi::drawing::ArcSegment* arc) override
	{
		native()->AddArc(reinterpret_cast<const D2D1_ARC_SEGMENT*>(arc));
	}

	GMPI_QUERYINTERFACE_NEW(gmpi::drawing::api::IGeometrySink);
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

	gmpi::ReturnCode open(drawing::api::IGeometrySink** returnGeometrySink) override;

	gmpi::ReturnCode getFactory(gmpi::drawing::api::IFactory** factory) override
	{
		//		native_->GetFactory((ID2D1Factory**)factory);
	}

	gmpi::ReturnCode strokeContainsPoint(const drawing::Point* point, float strokeWidth, drawing::api::IStrokeStyle* strokeStyle, const drawing::Matrix3x2* worldTransform, bool* returnContains) override
	{
		BOOL result = FALSE;
		const auto r = native()->StrokeContainsPoint(*(D2D1_POINT_2F*)point, strokeWidth, toNative(strokeStyle), (const D2D1_MATRIX_3X2_F*)worldTransform, &result);
		*returnContains = result == TRUE;
		return r == S_OK ? gmpi::ReturnCode::Ok : gmpi::ReturnCode::Fail;
	}

	gmpi::ReturnCode fillContainsPoint(const drawing::Point* point, const drawing::Matrix3x2* worldTransform, bool* returnContains) override
	{
		BOOL result = FALSE;
		const auto r = native()->FillContainsPoint(*(D2D1_POINT_2F*)&point, (const D2D1_MATRIX_3X2_F*)worldTransform, &result);
		*returnContains = result == TRUE;
		return r == S_OK ? gmpi::ReturnCode::Ok : gmpi::ReturnCode::Fail;
	}

	gmpi::ReturnCode getWidenedBounds(float strokeWidth, drawing::api::IStrokeStyle* strokeStyle, const drawing::Matrix3x2* worldTransform, const drawing::Rect* bounds) override
	{
		const auto r = native()->GetWidenedBounds(strokeWidth, toNative(strokeStyle), (const D2D1_MATRIX_3X2_F*)worldTransform, (D2D_RECT_F*)bounds);
		return r == S_OK ? gmpi::ReturnCode::Ok : gmpi::ReturnCode::Fail;
	}
	GMPI_QUERYINTERFACE_NEW(gmpi::drawing::api::IPathGeometry);
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
	std::wstring fontMatch(std::wstring fontName, gmpi::drawing::FontWeight fontWeight, float fontSize);

	gmpi::ReturnCode createPathGeometry(gmpi::drawing::api::IPathGeometry** pathGeometry) override;
	gmpi::ReturnCode createTextFormat(const char* fontFamilyName, /* void* unused fontCollection ,*/ gmpi::drawing::FontWeight fontWeight, gmpi::drawing::FontStyle fontStyle, gmpi::drawing::FontStretch fontStretch, float fontSize, /* void* unused2 localeName, */ gmpi::drawing::api::ITextFormat** textFormat) override;
	gmpi::ReturnCode createImage(int32_t width, int32_t height, gmpi::drawing::api::IBitmap** returnDiBitmap) override;
	gmpi::ReturnCode loadImageU(const char* uri, drawing::api::IBitmap** returnBitmap) override;
	gmpi::ReturnCode createStrokeStyle(const drawing::StrokeStyleProperties* strokeStyleProperties, const float* dashes, int32_t dashesCount, drawing::api::IStrokeStyle** returnStrokeStyle) override
	{
		*returnStrokeStyle = nullptr;

		ID2D1StrokeStyle* b = nullptr;

		auto hr = m_pDirect2dFactory->CreateStrokeStyle((const D2D1_STROKE_STYLE_PROPERTIES*) strokeStyleProperties, dashes, dashesCount, &b);

		if (hr == 0)
		{
			gmpi::shared_ptr<gmpi::api::IUnknown> wrapper;
			wrapper.Attach(new StrokeStyle(b, this));

			return wrapper->queryInterface(&drawing::api::IStrokeStyle::guid, reinterpret_cast<void**>(returnStrokeStyle));
		}

		return hr == 0 ? (gmpi::ReturnCode::Ok) : (gmpi::ReturnCode::Fail);
	}

	IWICBitmap* CreateDiBitmapFromNative(ID2D1Bitmap* D2D_Bitmap);

	gmpi::ReturnCode getFontFamilyName(int32_t fontIndex, gmpi::api::IString* returnName) override;

	GMPI_QUERYINTERFACE_NEW(gmpi::drawing::api::IFactory);
	GMPI_REFCOUNT_NO_DELETE;
};

class GraphicsContext : public gmpi::drawing::api::IDeviceContext
{
protected:
	ID2D1DeviceContext* context_;

	Factory* factory;
	std::vector<gmpi::drawing::Rect> clipRectStack;
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

	gmpi::ReturnCode getFactory(gmpi::drawing::api::IFactory** pfactory) override
	{
		*pfactory = factory;
	}

	gmpi::ReturnCode drawRectangle(const drawing::Rect* rect, drawing::api::IBrush* brush, float strokeWidth, drawing::api::IStrokeStyle* strokeStyle) override
	{
		context_->DrawRectangle(D2D1::RectF(rect->left, rect->top, rect->right, rect->bottom), ((Brush*)brush)->native(), strokeWidth, toNative(strokeStyle) );
	}

	gmpi::ReturnCode fillRectangle(const drawing::Rect* rect, drawing::api::IBrush* brush) override
	{
		context_->FillRectangle((D2D1_RECT_F*)rect, (ID2D1Brush*)((Brush*)brush)->native());
	}

	gmpi::ReturnCode clear(const drawing::Color* clearColor) override
	{
		native()->Clear(*(D2D1_COLOR_F*)clearColor);
		return ReturnCode::Ok;
	}

	//void DrawLine(gmpi::drawing::Point point0, gmpi::drawing::Point point1, const gmpi::drawing::api::IBrush* brush, float strokeWidth, const gmpi::drawing::api::IStrokeStyle* strokeStyle) override
	gmpi::ReturnCode drawLine(const drawing::Point* point0, const drawing::Point* point1, drawing::api::IBrush* brush, float strokeWidth, drawing::api::IStrokeStyle* strokeStyle) override
	{
		context_->DrawLine(*((D2D_POINT_2F*)point0), *((D2D_POINT_2F*)point1), ((Brush*)brush)->native(), strokeWidth, toNative(strokeStyle));
	}

//	void DrawGeometry(const gmpi::drawing::api::IPathGeometry* geometry, const gmpi::drawing::api::IBrush* brush, float strokeWidth = 1.0f, const gmpi::drawing::api::IStrokeStyle* strokeStyle = 0) override;
	gmpi::ReturnCode drawGeometry(drawing::api::IPathGeometry* pathGeometry, drawing::api::IBrush* brush, float strokeWidth, drawing::api::IStrokeStyle* strokeStyle) override;

	gmpi::ReturnCode fillGeometry(drawing::api::IPathGeometry* pathGeometry, drawing::api::IBrush* brush, drawing::api::IBrush* opacityBrush) override
	//void FillGeometry(const gmpi::drawing::api::IPathGeometry* geometry, const gmpi::drawing::api::IBrush* brush, const gmpi::drawing::api::IBrush* opacityBrush) override
	{
#ifdef LOG_DIRECTX_CALLS
		_RPT3(_CRT_WARN, "context_->FillGeometry(geometry%x, brush%x, nullptr);\n", (int)geometry, (int)brush);
#endif
		auto d2d_geometry = ((Geometry*)pathGeometry)->geometry_;

		ID2D1Brush* opacityBrushNative{};
		if (opacityBrush)
		{
			opacityBrushNative = ((Brush*)brush)->native();
		}

		context_->FillGeometry(d2d_geometry, ((Brush*)brush)->native(), opacityBrushNative);
	}

	gmpi::ReturnCode drawTextU(const char* string, uint32_t stringLength, drawing::api::ITextFormat* textFormat, const drawing::Rect* layoutRect, drawing::api::IBrush* defaultForegroundBrush, int32_t options) override;
	//void DrawTextU(const char* utf8String, int32_t stringLength, const gmpi::drawing::api::ITextFormat* textFormat, const gmpi::drawing::Rect* layoutRect, const gmpi::drawing::api::IBrush* brush, int32_t flags) override;

	//	void DrawBitmap( gmpi::drawing::api::IBitmap* mpBitmap, gmpi::drawing::Rect destinationRectangle, float opacity, int32_t interpolationMode, gmpi::drawing::Rect sourceRectangle) override
//	void DrawBitmap(const gmpi::drawing::api::IBitmap* mpBitmap, const gmpi::drawing::Rect* destinationRectangle, float opacity, /* MP1_BITMAP_INTERPOLATION_MODE*/ int32_t interpolationMode, const gmpi::drawing::Rect* sourceRectangle) override
	gmpi::ReturnCode drawBitmap(drawing::api::IBitmap* bitmap, const drawing::Rect* destinationRectangle, float opacity, drawing::BitmapInterpolationMode interpolationMode, const drawing::Rect* sourceRectangle) override
	{
		auto bm = ((Bitmap*)bitmap);
		auto native = bm->GetNativeBitmap(context_);
		if (native)
		{
			context_->DrawBitmap(
				native,
				(D2D1_RECT_F*)destinationRectangle,
				opacity,
				(D2D1_BITMAP_INTERPOLATION_MODE) interpolationMode,
				(D2D1_RECT_F*)sourceRectangle
			);
		}
	}

	gmpi::ReturnCode setTransform(const drawing::Matrix3x2* transform) override
	{
		context_->SetTransform(reinterpret_cast<const D2D1_MATRIX_3X2_F*>(transform));
		return ReturnCode::Ok;
	}

	gmpi::ReturnCode getTransform(drawing::Matrix3x2* transform) override
	{
		context_->GetTransform(reinterpret_cast<D2D1_MATRIX_3X2_F*>(transform));
		return ReturnCode::Ok;
	}

	gmpi::ReturnCode createSolidColorBrush(const drawing::Color* color, const drawing::BrushProperties* brushProperties, drawing::api::ISolidColorBrush** returnSolidColorBrush) = 0;

	gmpi::ReturnCode createGradientstopCollection(const drawing::Gradientstop* gradientstops, uint32_t gradientstopsCount, drawing::ExtendMode extendMode, drawing::api::IGradientstopCollection** returnGradientstopCollection) override;

	template <typename T>
	gmpi::ReturnCode make_wrapped(gmpi::api::IUnknown* object, const gmpi::api::Guid& iid, T** returnObject)
	{
		*returnObject = nullptr;
		gmpi::shared_ptr<gmpi::api::IUnknown> b2;
		b2.Attach(object);
		return b2->queryInterface(iid, reinterpret_cast<void**>(returnObject));
	};

	gmpi::ReturnCode createLinearGradientBrush(const drawing::LinearGradientBrushProperties* linearGradientBrushProperties, const drawing::BrushProperties* brushProperties, drawing::api::IGradientstopCollection* gradientstopCollection, drawing::api::ILinearGradientBrush** returnLinearGradientBrush) override
	{
		//*linearGradientBrush = nullptr;
		//gmpi::shared_ptr<gmpi::api::IUnknown> b2;
		//b2.Attach(new LinearGradientBrush(factory, context_, linearGradientBrushProperties, brushProperties, gradientStopCollection));
		//return b2->queryInterface(gmpi::drawing::api::SE_IID_LINEARGRADIENTBRUSH_MPGUI, reinterpret_cast<void **>(linearGradientBrush));

		return make_wrapped(
			new LinearGradientBrush(factory, context_, linearGradientBrushProperties, brushProperties, gradientstopCollection),
			drawing::api::ILinearGradientBrush::guid,
			returnLinearGradientBrush);
	}

	gmpi::ReturnCode createBitmapBrush(drawing::api::IBitmap* bitmap, const drawing::BitmapBrushProperties* bitmapBrushProperties, const drawing::BrushProperties* brushProperties, drawing::api::IBitmapBrush** returnBitmapBrush) override
	{
		*returnBitmapBrush = nullptr;
		gmpi::shared_ptr<gmpi::api::IUnknown> b2;
		b2.Attach(new BitmapBrush(factory, context_, bitmap, bitmapBrushProperties, brushProperties));
		return b2->queryInterface(&drawing::api::IBitmapBrush::guid, reinterpret_cast<void **>(returnBitmapBrush));
	}
	gmpi::ReturnCode createRadialGradientBrush(const drawing::RadialGradientBrushProperties* radialGradientBrushProperties, const drawing::BrushProperties* brushProperties, drawing::api::IGradientstopCollection* gradientstopCollection, drawing::api::IRadialGradientBrush** returnRadialGradientBrush) override
	{
		*returnRadialGradientBrush = nullptr;
		gmpi::shared_ptr<gmpi::api::IUnknown> b2;
		b2.Attach(new RadialGradientBrush(factory, context_, radialGradientBrushProperties, brushProperties, gradientstopCollection));
		return b2->queryInterface(&drawing::api::IRadialGradientBrush::guid, reinterpret_cast<void **>(returnRadialGradientBrush));
	}

	gmpi::ReturnCode createCompatibleRenderTarget(const gmpi::drawing::Size* desiredSize, gmpi::drawing::api::IBitmapRenderTarget** bitmapRenderTarget) override;

	gmpi::ReturnCode drawRoundedRectangle(const drawing::RoundedRect* roundedRect, drawing::api::IBrush* brush, float strokeWidth, drawing::api::IStrokeStyle* strokeStyle) override
	{
		context_->DrawRoundedRectangle((D2D1_ROUNDED_RECT*)roundedRect, (ID2D1Brush*)((Brush*)brush)->native(), (FLOAT)strokeWidth, toNative(strokeStyle));
	}

	gmpi::ReturnCode fillRoundedRectangle(const drawing::RoundedRect* roundedRect, drawing::api::IBrush* brush) override
	{
		context_->FillRoundedRectangle((D2D1_ROUNDED_RECT*)roundedRect, (ID2D1Brush*)((Brush*)brush)->native());
	}

	gmpi::ReturnCode drawEllipse(const drawing::Ellipse* ellipse, drawing::api::IBrush* brush, float strokeWidth, drawing::api::IStrokeStyle* strokeStyle) override
	{
		context_->DrawEllipse((D2D1_ELLIPSE*)ellipse, (ID2D1Brush*)((Brush*)brush)->native(), (FLOAT)strokeWidth, toNative(strokeStyle));
	}

	gmpi::ReturnCode fillEllipse(const drawing::Ellipse* ellipse, drawing::api::IBrush* brush) override
	{
		context_->FillEllipse((D2D1_ELLIPSE*)ellipse, (ID2D1Brush*)((Brush*)brush)->native());
	}

	gmpi::ReturnCode pushAxisAlignedClip(const drawing::Rect* clipRect) override;

	gmpi::ReturnCode popAxisAlignedClip() override
	{
//				_RPT0(_CRT_WARN, "                 PopAxisAlignedClip()\n");
#ifdef LOG_DIRECTX_CALLS
		_RPT0(_CRT_WARN, "context_->PopAxisAlignedClip();\n");
#endif
		context_->PopAxisAlignedClip();
		clipRectStack.pop_back();
	}

	void getAxisAlignedClip(gmpi::drawing::Rect* returnClipRect) override;

	gmpi::ReturnCode beginDraw() override
	{
		native()->BeginDraw();
		return gmpi::ReturnCode::Ok;
	}

	gmpi::ReturnCode endDraw() override
	{
		native()->EndDraw();
		return gmpi::ReturnCode::Ok;
	}

	bool SupportSRGB()
	{
		return factory->getPlatformPixelFormat() == gmpi::drawing::api::IBitmapPixels::kBGRA_SRGB;
	}

	GMPI_QUERYINTERFACE_NEW(gmpi::drawing::api::IDeviceContext);
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

	// HACK, to be ABI compatible with IBitmapRenderTarget we need this virtual function,
	// and it needs to be in the vtable right after all virtual functions of GraphicsContext
	virtual int32_t GetBitmap(gmpi::drawing::api::IBitmap** returnBitmap);

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

// Direct2D context tailored to devices without sRGB high-color support. i.e. Windows 7.
class GraphicsContext_Win7 : public GraphicsContext
{
public:

	GraphicsContext_Win7(ID2D1DeviceContext* context, Factory* pfactory) :
		GraphicsContext(context, pfactory)
	{}

	gmpi::ReturnCode createSolidColorBrush(const drawing::Color* color, const drawing::BrushProperties* brushProperties, drawing::api::ISolidColorBrush** returnSolidColorBrush)
//	gmpi::ReturnCode createSolidColorBrush(const gmpi::drawing::Color* color, gmpi::drawing::api::ISolidColorBrush **solidColorBrush) override
	{
		*returnSolidColorBrush = nullptr;
		gmpi::shared_ptr<gmpi::api::IUnknown> b;
		b.Attach(new SolidColorBrush_Win7(context_, color, factory));
		return b->queryInterface(&drawing::api::ISolidColorBrush::guid, reinterpret_cast<void **>(returnSolidColorBrush));
	}

	//gmpi::ReturnCode createGradientStopCollection(const gmpi::drawing::api::MP1_GRADIENT_STOP* gradientStops, uint32_t gradientStopsCount, /* gmpi::drawing::api::MP1_GAMMA colorInterpolationGamma, gmpi::drawing::api::MP1_EXTEND_MODE extendMode,*/ gmpi::drawing::api::IGradientstopCollection** gradientStopCollection) override
	gmpi::ReturnCode createGradientstopCollection(const drawing::Gradientstop* gradientstops, uint32_t gradientstopsCount, drawing::ExtendMode extendMode, drawing::api::IGradientstopCollection** returnGradientstopCollection) override
	{
		// Adjust gradient gamma.
		std::vector<drawing::Gradientstop> stops;
		stops.assign(gradientstopsCount, {});

		for(uint32_t i = 0 ; i < gradientstopsCount; ++i)
		{
			auto& srce = gradientstops[i];
			auto& dest = stops[i];
			dest.position = srce.position;
			dest.color.a = srce.color.a;
			dest.color.r = se_sdk::FastGamma::pixelToNormalised(se_sdk::FastGamma::float_to_sRGB(srce.color.r));
			dest.color.g = se_sdk::FastGamma::pixelToNormalised(se_sdk::FastGamma::float_to_sRGB(srce.color.g));
			dest.color.b = se_sdk::FastGamma::pixelToNormalised(se_sdk::FastGamma::float_to_sRGB(srce.color.b));
		}

		return GraphicsContext::createGradientstopCollection(stops.data(), gradientstopsCount, extendMode, returnGradientstopCollection);
	}

	gmpi::ReturnCode clear(const drawing::Color* clearColor) override
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