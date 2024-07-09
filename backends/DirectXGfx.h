#pragma once

/*
#include "DirectXGfx.h"
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

//#include <codecvt>
#include <map>
#include <d2d1_2.h>
#include <dwrite.h>
#include <Wincodec.h>

#include "../Drawing.h"
#include "RefCountMacros.h"

#pragma warning(disable : 4996) // "codecvt deprecated in C++17"

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
    drawing::api::IFactory* factory_;

public:
    GmpiDXResourceWrapper(DxType* native, drawing::api::IFactory* factory) : GmpiDXWrapper<MpInterface, DxType>(native), factory_(factory) {}
    GmpiDXResourceWrapper(drawing::api::IFactory* factory) : factory_(factory) {}

    ReturnCode getFactory(drawing::api::IFactory** factory) override
    {
        *factory = factory_;
        return ReturnCode::Ok;
    }
};

class TextFormat final : public GmpiDXWrapper<drawing::api::ITextFormat, IDWriteTextFormat>
{
//    std::wstring_convert<std::codecvt_utf8<wchar_t>>* stringConverter = {}; // constructed once is much faster.
    bool useLegacyBaseLineSnapping = true;
    float topAdjustment = {};
    float fontMetrics_ascent = {};

    void CalculateTopAdjustment()
    {
        assert(topAdjustment == 0.0f); // else boundingBoxSize calculation will be affected, and won't be actual native size.

        drawing::FontMetrics fontMetrics;
        getFontMetrics(&fontMetrics);

        drawing::Size boundingBoxSize;
        getTextExtentU("A", 1, &boundingBoxSize);

        topAdjustment = boundingBoxSize.height - (fontMetrics.ascent + fontMetrics.descent);
        fontMetrics_ascent = fontMetrics.ascent;
    }

public:
    TextFormat(/*std::wstring_convert<std::codecvt_utf8<wchar_t>>* pstringConverter,*/ IDWriteTextFormat* native) :
        GmpiDXWrapper<drawing::api::ITextFormat, IDWriteTextFormat>(native)
//        , stringConverter(pstringConverter)
    {
        CalculateTopAdjustment();
    }

    ReturnCode setTextAlignment(drawing::TextAlignment textAlignment) override
    {
        const auto r = native()->SetTextAlignment((DWRITE_TEXT_ALIGNMENT) textAlignment);
        return r == S_OK ? ReturnCode::Ok : ReturnCode::Fail;
    }

    ReturnCode setParagraphAlignment(drawing::ParagraphAlignment paragraphAlignment) override
    {
        const auto r = native()->SetParagraphAlignment((DWRITE_PARAGRAPH_ALIGNMENT) paragraphAlignment);
        return r == S_OK ? ReturnCode::Ok : ReturnCode::Fail;
    }

    ReturnCode setWordWrapping(drawing::WordWrapping wordWrapping) override
    {
        const auto r = native()->SetWordWrapping((DWRITE_WORD_WRAPPING) wordWrapping);
        return r == S_OK ? ReturnCode::Ok : ReturnCode::Fail;
    }

    // TODO!!!: Probably needs to accept constraint rect like DirectWrite. !!!
    ReturnCode getTextExtentU(const char* utf8String, int32_t stringLength, drawing::Size* returnSize) override;

    ReturnCode getFontMetrics(drawing::FontMetrics* returnFontMetrics) override;

    ReturnCode setLineSpacing(float lineSpacing, float baseline) override
    {
        // Hack, reuse this method to enable legacy-mode.
        if (static_cast<float>(ITextFormat::ImprovedVerticalBaselineSnapping) == lineSpacing)
        {
            useLegacyBaseLineSnapping = false;
            return ReturnCode::Ok;
        }

        // For the default method, spacing depends solely on the content. For uniform spacing, the specified line height overrides the content.
        DWRITE_LINE_SPACING_METHOD method = lineSpacing < 0.0f ? DWRITE_LINE_SPACING_METHOD_DEFAULT : DWRITE_LINE_SPACING_METHOD_UNIFORM;
        const auto r = native()->SetLineSpacing(method, fabsf(lineSpacing), baseline);
        return r == S_OK ? ReturnCode::Ok : ReturnCode::Fail;
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

    GMPI_QUERYINTERFACE_METHOD(drawing::api::ITextFormat);
    GMPI_REFCOUNT;
};
#if 0
class Resource final : public GmpiDXWrapper<drawing::api::IResource, ID2D1Resource>
{
public:
    ReturnCode getFactory(drawing::api::IFactory** returnGetFactory) override
    {
        const auto r = native()->getFactory((D2D1_GET_FACTORY) returnGetFactory);
        return r == S_OK ? ReturnCode::Ok : ReturnCode::Fail;
    }

    GMPI_QUERYINTERFACE_METHOD(drawing::api::IResource);
    GMPI_REFCOUNT;
};
#endif
class BitmapPixels final : public drawing::api::IBitmapPixels
{
    bool alphaPremultiplied;
    IWICBitmap* bitmap;
    UINT bytesPerRow{};
    BYTE* ptr{};
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
        else
        {
            alphaPremultiplied = true; // prevent possible null deference of 'bitmap' in destructor
        }
    }

    ~BitmapPixels()
    {
        if (!alphaPremultiplied)
            premultiplyAlpha();

        if (nativeBitmap_)
        {
#if 1
            if (0 != (flags & (int32_t) drawing::BitmapLockFlags::Write))
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

    ReturnCode getAddress(uint8_t** returnAddress) override
    {
        *returnAddress = ptr;
        return ReturnCode::Ok;
    }

    ReturnCode getBytesPerRow(int32_t* returnBytesPerRow) override
    {
        *returnBytesPerRow = bytesPerRow;
        return ReturnCode::Ok;
    }

    ReturnCode getPixelFormat(int32_t* returnPixelFormat) override
    {
        *returnPixelFormat = pixelFormat;
        return ReturnCode::Ok;
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

    GMPI_QUERYINTERFACE_METHOD(drawing::api::IBitmapPixels);
    GMPI_REFCOUNT;
};

class Bitmap final : public drawing::api::IBitmap
{
public:
    ID2D1Bitmap* nativeBitmap_;
    ID2D1DeviceContext* nativeContext_;
    IWICBitmap* diBitmap_ = {};
    drawing::api::IFactory* factory;
#ifdef _DEBUG
    std::string debugFilename;
#endif
    Bitmap(drawing::api::IFactory* pfactory, IWICBitmap* diBitmap);

    Bitmap(drawing::api::IFactory* pfactory, ID2D1DeviceContext* nativeContext, ID2D1Bitmap* nativeBitmap) :
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

    ID2D1Bitmap* getNativeBitmap(ID2D1DeviceContext* nativeContext);

    ReturnCode getSizeU(drawing::SizeU* returnSize) override
    {
        const auto r = diBitmap_->GetSize(&returnSize->width, &returnSize->height);
        return r == S_OK ? ReturnCode::Ok : ReturnCode::Fail;
    }

    ReturnCode lockPixels(drawing::api::IBitmapPixels** returnPixels, int32_t flags) override;

    void applyPreMultiplyCorrection();

//	ReturnCode getFactory(drawing::api::IFactory** pfactory) override;
    ReturnCode getFactory(drawing::api::IFactory** returnFactory) override;

    GMPI_QUERYINTERFACE_METHOD(drawing::api::IBitmap);
    GMPI_REFCOUNT;
};

class GradientstopCollection final : public GmpiDXResourceWrapper<drawing::api::IGradientstopCollection, ID2D1GradientStopCollection>
{
public:
    GradientstopCollection(ID2D1GradientStopCollection* native, drawing::api::IFactory* factory) : GmpiDXResourceWrapper(native, factory) {}

    GMPI_QUERYINTERFACE_METHOD(drawing::api::IGradientstopCollection);
    GMPI_REFCOUNT;
};

// never instansiated or inherited, just used to simulate a common ancestor for all brushes, that can quickly C cast to get hold of the D2D brush.
struct Brush : public gmpi::api::IUnknown
{
    ID2D1Brush* native_ = {};

    auto native() -> ID2D1Brush* const
    {
        return native_;
    }
};

class BitmapBrush final : public drawing::api::IBitmapBrush
{
    ID2D1BitmapBrush* native_ = {}; // MUST be first so at same relative memory as Brush::native_
    drawing::api::IFactory* factory_ = {};

public:
    BitmapBrush(
        drawing::api::IFactory* factory,
        ID2D1DeviceContext* context,
        const drawing::api::IBitmap* bitmap,
        const drawing::BrushProperties* brushProperties
    )
        : factory_(factory)
    {
        auto bm = ((Bitmap*)bitmap);
        auto nativeBitmap = bm->getNativeBitmap(context);

        // not supported on macOS, so hardcode them.
        D2D1_BITMAP_BRUSH_PROPERTIES bitmapBrushProperties{
            D2D1_EXTEND_MODE_WRAP,
            D2D1_EXTEND_MODE_WRAP,
            D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR,
        };

        [[maybe_unused]] const auto hr = context->CreateBitmapBrush(nativeBitmap, &bitmapBrushProperties, (D2D1_BRUSH_PROPERTIES*)brushProperties, (ID2D1BitmapBrush**)&native_);
        assert(hr == 0);
    }

    ~BitmapBrush()
    {
        if (native_)
            native_->Release();
    }

    ReturnCode queryInterface(const gmpi::api::Guid* iid, void** returnInterface) override
    {
        *returnInterface = {};
        if (*iid == drawing::api::IBitmapBrush::guid || /**iid == drawing::api::IBrush::guid ||*/ *iid == drawing::api::IResource::guid || *iid == gmpi::api::IUnknown::guid)
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
        return ReturnCode::Ok;
    }

    GMPI_REFCOUNT;
};

class SolidColorBrush final : public drawing::api::ISolidColorBrush
{
    ID2D1SolidColorBrush* native_ = {}; // MUST be first so at same relative memory as Brush::native_
    drawing::api::IFactory* factory_ = {};

public:
    SolidColorBrush(ID2D1SolidColorBrush* b, drawing::api::IFactory *factory) : native_(b), factory_(factory) {}

    ~SolidColorBrush()
    {
        if (native_)
            native_->Release();
    }

    ID2D1SolidColorBrush* native()
    {
        return (ID2D1SolidColorBrush*)native_;
    }

    ReturnCode setColor(const drawing::Color* color) override
    {
        native()->SetColor((D2D1::ColorF*) color);
        return ReturnCode::Ok;
    }

    // IResource
    ReturnCode getFactory(drawing::api::IFactory** factory) override
    {
        *factory = factory_;
        return ReturnCode::Ok;
    }

    ReturnCode queryInterface(const gmpi::api::Guid* iid, void** returnInterface) override
    {
        *returnInterface = {};
        if (*iid == drawing::api::ISolidColorBrush::guid || /**iid == drawing::api::IBrush::guid ||*/ *iid == drawing::api::IResource::guid || *iid == gmpi::api::IUnknown::guid)
        {
            *returnInterface = this;
            addRef();
            return ReturnCode::Ok;
        }
        return ReturnCode::NoSupport;
    }

    GMPI_REFCOUNT;
};

// Windows7 has less support for sRGB
inline drawing::Color colorWithoutGammaAdjustment(drawing::Color)
{
    return drawing::Color{};
}

class SolidColorBrush_Win7 final : public drawing::api::ISolidColorBrush
{
    ID2D1SolidColorBrush* native_ = {}; // MUST be first so at same relative memory as Brush::native_
    drawing::api::IFactory* factory_ = {};

public:
    SolidColorBrush_Win7(ID2D1RenderTarget* context, const drawing::Color* color, drawing::api::IFactory* factory) : factory_(factory)
    {
        constexpr float c = 1.0f / 255.0f;

        const drawing::Color modified
        {
            c * static_cast<float>(drawing::linearPixelToSRGB(color->r)),
            c * static_cast<float>(drawing::linearPixelToSRGB(color->g)),
            c * static_cast<float>(drawing::linearPixelToSRGB(color->b)),
            color->a
        };

        /*HRESULT hr =*/ context->CreateSolidColorBrush(*(D2D1_COLOR_F*)&modified, (ID2D1SolidColorBrush**) &native_);
    }

    ~SolidColorBrush_Win7()
    {
        if (native_)
            native_->Release();
    }

    inline ID2D1SolidColorBrush* nativeSolidColorBrush()
    {
        return (ID2D1SolidColorBrush*)native_;
    }

    // IMPORTANT: Virtual functions must 100% match simulated interface (drawing::api::ISolidColorBrush)
    ReturnCode setColor(const drawing::Color* color) override
    {
        constexpr float c = 1.0f / 255.0f;

        const drawing::Color modified
        {
            c * static_cast<float>(drawing::linearPixelToSRGB(color->r)),
            c * static_cast<float>(drawing::linearPixelToSRGB(color->g)),
            c * static_cast<float>(drawing::linearPixelToSRGB(color->b)),
            color->a
        };

        nativeSolidColorBrush()->SetColor((D2D1::ColorF*) &modified);
        return ReturnCode::Ok;
    }

    ReturnCode queryInterface(const gmpi::api::Guid* iid, void** returnInterface) override
    {
        *returnInterface = {};
        if (*iid == drawing::api::ISolidColorBrush::guid || /**iid == drawing::api::IBrush::guid ||*/ *iid == drawing::api::IResource::guid || *iid == gmpi::api::IUnknown::guid)
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
        return ReturnCode::Ok;
    }
    GMPI_REFCOUNT;
};

class LinearGradientBrush final : public drawing::api::ILinearGradientBrush
{
    ID2D1LinearGradientBrush* native_ = {}; // MUST be first so at same relative memory as Brush::native_
    drawing::api::IFactory* factory_ = {};

public:
    LinearGradientBrush(drawing::api::IFactory *factory, ID2D1RenderTarget* context, const drawing::LinearGradientBrushProperties* linearGradientBrushProperties, const drawing::BrushProperties* brushProperties, const  drawing::api::IGradientstopCollection* gradientStopCollection)
        : factory_(factory)
    {
        [[maybe_unused]] HRESULT hr = context->CreateLinearGradientBrush((D2D1_LINEAR_GRADIENT_BRUSH_PROPERTIES*)linearGradientBrushProperties, (D2D1_BRUSH_PROPERTIES*)brushProperties, ((GradientstopCollection*)gradientStopCollection)->native(), (ID2D1LinearGradientBrush **)&native_);
        assert(hr == 0);
    }

    ~LinearGradientBrush()
    {
        if (native_)
            native_->Release();
    }

    inline ID2D1LinearGradientBrush* native()
    {
        return (ID2D1LinearGradientBrush*)native_;
    }

    void setStartPoint(drawing::Point startPoint) override
    {
        native()->SetStartPoint(*reinterpret_cast<D2D1_POINT_2F*>(&startPoint));
    }
    void setEndPoint(drawing::Point endPoint) override
    {
        native()->SetEndPoint(*reinterpret_cast<D2D1_POINT_2F*>(&endPoint));
    }

    ReturnCode queryInterface(const gmpi::api::Guid* iid, void** returnInterface) override
    {
        *returnInterface = {};
        if (*iid == drawing::api::ILinearGradientBrush::guid || /**iid == drawing::api::IBrush::guid ||*/ *iid == drawing::api::IResource::guid || *iid == gmpi::api::IUnknown::guid)
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
        return ReturnCode::Ok;
    }
    GMPI_REFCOUNT;
};

class RadialGradientBrush final : public drawing::api::IRadialGradientBrush
{
    ID2D1RadialGradientBrush* native_ = {}; // MUST be first so at same relative memory as Brush::native_
    drawing::api::IFactory* factory_ = {};

public:
    RadialGradientBrush(drawing::api::IFactory *factory, ID2D1RenderTarget* context, const drawing::RadialGradientBrushProperties* linearGradientBrushProperties, const drawing::BrushProperties* brushProperties, const drawing::api::IGradientstopCollection* gradientStopCollection)
        : factory_(factory)
    {
        [[maybe_unused]] HRESULT hr = context->CreateRadialGradientBrush((D2D1_RADIAL_GRADIENT_BRUSH_PROPERTIES*)linearGradientBrushProperties, (D2D1_BRUSH_PROPERTIES*)brushProperties, ((GradientstopCollection*)gradientStopCollection)->native(), (ID2D1RadialGradientBrush **)&native_);
        assert(hr == 0);
    }

    ~RadialGradientBrush()
    {
        if (native_)
            native_->Release();
    }

    inline ID2D1RadialGradientBrush* native()
    {
        return (ID2D1RadialGradientBrush*)native_;
    }

    void setCenter(drawing::Point center) override
    {
        native()->SetCenter(*reinterpret_cast<D2D1_POINT_2F*>(&center));
    }

    void setGradientOriginOffset(drawing::Point gradientOriginOffset) override
    {
        native()->SetGradientOriginOffset(*reinterpret_cast<D2D1_POINT_2F*>(&gradientOriginOffset));
    }

    void setRadiusX(float radiusX) override
    {
        native()->SetRadiusX(radiusX);
    }

    void setRadiusY(float radiusY) override
    {
        native()->SetRadiusY(radiusY);
    }

    ReturnCode queryInterface(const gmpi::api::Guid* iid, void** returnInterface) override
    {
        *returnInterface = {};
        if (*iid == drawing::api::IRadialGradientBrush::guid || /**iid == drawing::api::IBrush::guid ||*/ *iid == drawing::api::IResource::guid || *iid == gmpi::api::IUnknown::guid)
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
        return ReturnCode::Ok;
    }
    GMPI_REFCOUNT;
};

class StrokeStyle final : public GmpiDXResourceWrapper<drawing::api::IStrokeStyle, ID2D1StrokeStyle>
{
public:
    StrokeStyle(ID2D1StrokeStyle* native, drawing::api::IFactory* factory) : GmpiDXResourceWrapper(native, factory) {}

    GMPI_QUERYINTERFACE_METHOD(drawing::api::IStrokeStyle);
    GMPI_REFCOUNT;
};

inline ID2D1StrokeStyle* toNative(const drawing::api::IStrokeStyle* strokeStyle)
{
    if (strokeStyle)
    {
        return ((StrokeStyle*)strokeStyle)->native();
    }
    return nullptr;
}

class GeometrySink final : public drawing::api::IGeometrySink
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
        }
    }
    void beginFigure(drawing::Point startPoint, drawing::FigureBegin figureBegin) override
    {
        native()->BeginFigure(*reinterpret_cast<const D2D1_POINT_2F*>(&startPoint), (D2D1_FIGURE_BEGIN)figureBegin);
    }

    void endFigure(drawing::FigureEnd figureEnd) override
    {
        native()->EndFigure((D2D1_FIGURE_END) figureEnd);
    }

    void setFillMode(drawing::FillMode fillMode) override
    {
        native()->SetFillMode((D2D1_FILL_MODE) fillMode);
    }

    ReturnCode close() override
    {
        const auto r = native()->Close();
        return r == S_OK ? ReturnCode::Ok : ReturnCode::Fail;
    }

    void addLine(drawing::Point point) override
    {
        native()->AddLine(*reinterpret_cast<const D2D1_POINT_2F*>(&point));
    }

    void addLines(const drawing::Point* points, uint32_t pointsCount) override
    {
        native()->AddLines(reinterpret_cast<const D2D1_POINT_2F*>(points), pointsCount);
    }

    void addBezier(const drawing::BezierSegment* bezier) override
    {
        native()->AddBezier(reinterpret_cast<const D2D1_BEZIER_SEGMENT*>(bezier));
    }

    void addBeziers(const drawing::BezierSegment* beziers, uint32_t beziersCount) override
    {
        native()->AddBeziers(reinterpret_cast<const D2D1_BEZIER_SEGMENT*>(beziers), beziersCount);
    }

    void addQuadraticBezier(const drawing::QuadraticBezierSegment* bezier) override
    {
        native()->AddQuadraticBezier(reinterpret_cast<const D2D1_QUADRATIC_BEZIER_SEGMENT*>(bezier));
    }

    void addQuadraticBeziers(const drawing::QuadraticBezierSegment* beziers, uint32_t beziersCount) override
    {
        native()->AddQuadraticBeziers(reinterpret_cast<const D2D1_QUADRATIC_BEZIER_SEGMENT*>(beziers), beziersCount);
    }

    void addArc(const drawing::ArcSegment* arc) override
    {
        native()->AddArc(reinterpret_cast<const D2D1_ARC_SEGMENT*>(arc));
    }

    GMPI_QUERYINTERFACE_METHOD(drawing::api::IGeometrySink);
    GMPI_REFCOUNT;
};


class Geometry final : public drawing::api::IPathGeometry
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
        }
    }

    ID2D1PathGeometry* native()
    {
        return geometry_;
    }

    ReturnCode open(drawing::api::IGeometrySink** returnGeometrySink) override;

    ReturnCode getFactory(drawing::api::IFactory** factory) override
    {
        assert(false); // not implemented.
        //		native_->GetFactory((ID2D1Factory**)factory);
        return ReturnCode::Ok;
    }

    ReturnCode strokeContainsPoint(drawing::Point point, float strokeWidth, drawing::api::IStrokeStyle* strokeStyle, const drawing::Matrix3x2* worldTransform, bool* returnContains) override
    {
        BOOL result = FALSE;
        const auto r = native()->StrokeContainsPoint(*(D2D1_POINT_2F*)&point, strokeWidth, toNative(strokeStyle), (const D2D1_MATRIX_3X2_F*)worldTransform, &result);
        *returnContains = result == TRUE;
        return r == S_OK ? ReturnCode::Ok : ReturnCode::Fail;
    }

    ReturnCode fillContainsPoint(drawing::Point point, const drawing::Matrix3x2* worldTransform, bool* returnContains) override
    {
        BOOL result = FALSE;
        const auto r = native()->FillContainsPoint(*(D2D1_POINT_2F*)&point, (const D2D1_MATRIX_3X2_F*)worldTransform, &result);
        *returnContains = result == TRUE;
        return r == S_OK ? ReturnCode::Ok : ReturnCode::Fail;
    }

    ReturnCode getWidenedBounds(float strokeWidth, drawing::api::IStrokeStyle* strokeStyle, const drawing::Matrix3x2* worldTransform, drawing::Rect* returnBounds) override
    {
        const auto r = native()->GetWidenedBounds(strokeWidth, toNative(strokeStyle), (const D2D1_MATRIX_3X2_F*)worldTransform, (D2D_RECT_F*)returnBounds);
        return r == S_OK ? ReturnCode::Ok : ReturnCode::Fail;
    }

    GMPI_QUERYINTERFACE_METHOD(drawing::api::IPathGeometry);
    GMPI_REFCOUNT;
};


class Factory_base : public drawing::api::IFactory
{
protected:
    ID2D1Factory1* m_pDirect2dFactory = {};
    IDWriteFactory* writeFactory = {};
    IWICImagingFactory* pIWICFactory = {};
    std::vector<std::wstring>& supportedFontFamiliesLowerCase;
    std::vector<std::string>& supportedFontFamilies;
    std::map<std::wstring, std::wstring>& GdiFontConversions;
    bool DX_support_sRGB = true;

public:
    Factory_base(
        std::vector<std::wstring>& supportedFontFamiliesLowerCase,
        std::vector<std::string>& supportedFontFamilies,
        std::map<std::wstring, std::wstring>& GdiFontConversions
    );
    ~Factory_base();

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
            
    ReturnCode getPlatformPixelFormat(drawing::api::IBitmapPixels::PixelFormat* returnPixelFormat) override
    {
        *returnPixelFormat = DX_support_sRGB ? drawing::api::IBitmapPixels::kBGRA_SRGB : drawing::api::IBitmapPixels::kBGRA;
        return ReturnCode::Ok;
    }

    ID2D1Factory1* getD2dFactory()
    {
        return m_pDirect2dFactory;
    }
    std::wstring fontMatch(std::wstring fontName, drawing::FontWeight fontWeight, float fontSize);
    ReturnCode createPathGeometry(drawing::api::IPathGeometry** returnPathGeometry) override;
    ReturnCode createTextFormat(const char* fontFamilyName, drawing::FontWeight fontWeight, drawing::FontStyle fontStyle, drawing::FontStretch fontStretch, float fontHeight, drawing::api::ITextFormat** returnTextFormat) override;
    ReturnCode createImage(int32_t width, int32_t height, drawing::api::IBitmap** returnBitmap) override;
    ReturnCode loadImageU(const char* uri, drawing::api::IBitmap** returnBitmap) override;
    ReturnCode createStrokeStyle(const drawing::StrokeStyleProperties* strokeStyleProperties, const float* dashes, int32_t dashesCount, drawing::api::IStrokeStyle** returnStrokeStyle) override
    {
        *returnStrokeStyle = nullptr;

        const D2D1_STROKE_STYLE_PROPERTIES nativeProperties
        {
            (D2D1_CAP_STYLE) strokeStyleProperties->lineCap,
            (D2D1_CAP_STYLE) strokeStyleProperties->lineCap,
            (D2D1_CAP_STYLE) strokeStyleProperties->lineCap,
            (D2D1_LINE_JOIN) strokeStyleProperties->lineJoin,
            strokeStyleProperties->miterLimit,
            (D2D1_DASH_STYLE)strokeStyleProperties->dashStyle,
            strokeStyleProperties->dashOffset
		};

        ID2D1StrokeStyle* b = nullptr;
        auto r = m_pDirect2dFactory->CreateStrokeStyle(&nativeProperties, dashes, dashesCount, &b);

        if (r == S_OK)
        {
            gmpi::shared_ptr<gmpi::api::IUnknown> wrapper;
            wrapper.Attach(new StrokeStyle(b, this));

            return wrapper->queryInterface(&drawing::api::IStrokeStyle::guid, reinterpret_cast<void**>(returnStrokeStyle));
        }

        return r == S_OK ? ReturnCode::Ok : ReturnCode::Fail;
    }

    IWICBitmap* CreateDiBitmapFromNative(ID2D1Bitmap* D2D_Bitmap);

    ReturnCode getFontFamilyName(int32_t fontIndex, gmpi::api::IString* returnName) override;

    GMPI_QUERYINTERFACE_METHOD(drawing::api::IFactory);
    GMPI_REFCOUNT_NO_DELETE;
};

class Factory : public Factory_base
{
    std::vector<std::wstring> supportedFontFamiliesLowerCase;
    std::vector<std::string> supportedFontFamilies;
    std::map<std::wstring, std::wstring> GdiFontConversions;

public:
    Factory();
};

class GraphicsContext_base : public drawing::api::IDeviceContext
{
protected:
    ID2D1DeviceContext* context_{};

    drawing::api::IFactory* factory{};
    std::vector<drawing::Rect> clipRectStack;

    GraphicsContext_base(ID2D1DeviceContext* deviceContext, drawing::api::IFactory* pfactory) :
        context_(deviceContext)
        , factory(pfactory)
    {
        context_->AddRef();
    }

    // for BitmapRenderTarget which populates context in it's constructor
    GraphicsContext_base(drawing::api::IFactory* pfactory) :
        factory(pfactory)
    {
    }

public:
    virtual ~GraphicsContext_base()
    {
        context_->Release();
    }

    ID2D1DeviceContext* native()
    {
        return context_;
    }

    ReturnCode getFactory(drawing::api::IFactory** pfactory) override
    {
        *pfactory = factory;
        return ReturnCode::Ok;
    }

    ReturnCode drawRectangle(const drawing::Rect* rect, drawing::api::IBrush* brush, float strokeWidth, drawing::api::IStrokeStyle* strokeStyle) override
    {
        context_->DrawRectangle(D2D1::RectF(rect->left, rect->top, rect->right, rect->bottom), ((Brush*)brush)->native(), strokeWidth, toNative(strokeStyle) );
        return ReturnCode::Ok;
    }

    ReturnCode fillRectangle(const drawing::Rect* rect, drawing::api::IBrush* brush) override
    {
        context_->FillRectangle((D2D1_RECT_F*)rect, ((Brush*)brush)->native());
        return ReturnCode::Ok;
    }

    ReturnCode drawRoundedRectangle(const drawing::RoundedRect* roundedRect, drawing::api::IBrush* brush, float strokeWidth, drawing::api::IStrokeStyle* strokeStyle) override
    {
        context_->DrawRoundedRectangle((D2D1_ROUNDED_RECT*)roundedRect, ((Brush*)brush)->native(), (FLOAT)strokeWidth, toNative(strokeStyle));
        return ReturnCode::Ok;
    }

    ReturnCode fillRoundedRectangle(const drawing::RoundedRect* roundedRect, drawing::api::IBrush* brush) override
    {
        context_->FillRoundedRectangle((D2D1_ROUNDED_RECT*)roundedRect, ((Brush*)brush)->native());
        return ReturnCode::Ok;
    }

    ReturnCode drawEllipse(const drawing::Ellipse* ellipse, drawing::api::IBrush* brush, float strokeWidth, drawing::api::IStrokeStyle* strokeStyle) override
    {
        context_->DrawEllipse((D2D1_ELLIPSE*)ellipse, ((Brush*)brush)->native(), (FLOAT)strokeWidth, toNative(strokeStyle));
        return ReturnCode::Ok;
    }

    ReturnCode fillEllipse(const drawing::Ellipse* ellipse, drawing::api::IBrush* brush) override
    {
        context_->FillEllipse((D2D1_ELLIPSE*)ellipse, ((Brush*)brush)->native());
        return ReturnCode::Ok;
    }

    ReturnCode drawLine(drawing::Point point0, drawing::Point point1, drawing::api::IBrush* brush, float strokeWidth, drawing::api::IStrokeStyle* strokeStyle) override
    {
        context_->DrawLine(*((D2D_POINT_2F*)&point0), *((D2D_POINT_2F*)&point1), ((Brush*)brush)->native(), strokeWidth, toNative(strokeStyle));
        return ReturnCode::Ok;
    }

    ReturnCode drawGeometry(drawing::api::IPathGeometry* pathGeometry, drawing::api::IBrush* brush, float strokeWidth, drawing::api::IStrokeStyle* strokeStyle) override;

    ReturnCode fillGeometry(drawing::api::IPathGeometry* pathGeometry, drawing::api::IBrush* brush, drawing::api::IBrush* opacityBrush) override
    {
        auto d2d_geometry = ((Geometry*)pathGeometry)->native();

        ID2D1Brush* opacityBrushNative{};
        if (opacityBrush)
        {
            opacityBrushNative = ((Brush*)brush)->native();
        }

        context_->FillGeometry(d2d_geometry, ((Brush*)brush)->native(), opacityBrushNative);
        return ReturnCode::Ok;
    }

    ReturnCode drawTextU(const char* string, uint32_t stringLength, drawing::api::ITextFormat* textFormat, const drawing::Rect* layoutRect, drawing::api::IBrush* defaultForegroundBrush, int32_t options) override;

    ReturnCode drawBitmap(drawing::api::IBitmap* bitmap, const drawing::Rect* destinationRectangle, float opacity, drawing::BitmapInterpolationMode interpolationMode, const drawing::Rect* sourceRectangle) override
    {
        auto bm = ((Bitmap*)bitmap);
        auto native = bm->getNativeBitmap(context_);
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
        return ReturnCode::Ok;
    }

    ReturnCode setTransform(const drawing::Matrix3x2* transform) override
    {
        context_->SetTransform(reinterpret_cast<const D2D1_MATRIX_3X2_F*>(transform));
        return ReturnCode::Ok;
    }

    ReturnCode getTransform(drawing::Matrix3x2* transform) override
    {
        context_->GetTransform(reinterpret_cast<D2D1_MATRIX_3X2_F*>(transform));
        return ReturnCode::Ok;
    }

    ReturnCode createSolidColorBrush(const drawing::Color* color, const drawing::BrushProperties* brushProperties, drawing::api::ISolidColorBrush** returnSolidColorBrush) override;

    ReturnCode createGradientstopCollection(const drawing::Gradientstop* gradientstops, uint32_t gradientstopsCount, drawing::ExtendMode extendMode, drawing::api::IGradientstopCollection** returnGradientstopCollection) override;

    template <typename T>
    ReturnCode make_wrapped(gmpi::api::IUnknown* object, const gmpi::api::Guid& iid, T** returnObject)
    {
        *returnObject = nullptr;
        gmpi::shared_ptr<gmpi::api::IUnknown> b2;
        b2.Attach(object);
        return b2->queryInterface(&iid, reinterpret_cast<void**>(returnObject));
    };

    ReturnCode createLinearGradientBrush(const drawing::LinearGradientBrushProperties* linearGradientBrushProperties, const drawing::BrushProperties* brushProperties, drawing::api::IGradientstopCollection* gradientstopCollection, drawing::api::ILinearGradientBrush** returnLinearGradientBrush) override
    {
        return make_wrapped(
            new LinearGradientBrush(factory, context_, linearGradientBrushProperties, brushProperties, gradientstopCollection),
            drawing::api::ILinearGradientBrush::guid,
            returnLinearGradientBrush);
    }

    ReturnCode createBitmapBrush(drawing::api::IBitmap* bitmap, /*const drawing::BitmapBrushProperties* bitmapBrushProperties, */const drawing::BrushProperties* brushProperties, drawing::api::IBitmapBrush** returnBitmapBrush) override
    {
        *returnBitmapBrush = nullptr;
        gmpi::shared_ptr<gmpi::api::IUnknown> b2;
        b2.Attach(new BitmapBrush(factory, context_, bitmap, /*bitmapBrushProperties, */brushProperties));
        return b2->queryInterface(&drawing::api::IBitmapBrush::guid, reinterpret_cast<void **>(returnBitmapBrush));
    }
    ReturnCode createRadialGradientBrush(const drawing::RadialGradientBrushProperties* radialGradientBrushProperties, const drawing::BrushProperties* brushProperties, drawing::api::IGradientstopCollection* gradientstopCollection, drawing::api::IRadialGradientBrush** returnRadialGradientBrush) override
    {
        *returnRadialGradientBrush = nullptr;
        gmpi::shared_ptr<gmpi::api::IUnknown> b2;
        b2.Attach(new RadialGradientBrush(factory, context_, radialGradientBrushProperties, brushProperties, gradientstopCollection));
        return b2->queryInterface(&drawing::api::IRadialGradientBrush::guid, reinterpret_cast<void **>(returnRadialGradientBrush));
    }

    ReturnCode createCompatibleRenderTarget(drawing::Size desiredSize, drawing::api::IBitmapRenderTarget** bitmapRenderTarget) override;

    ReturnCode pushAxisAlignedClip(const drawing::Rect* clipRect) override;

    ReturnCode popAxisAlignedClip() override
    {
        context_->PopAxisAlignedClip();
        clipRectStack.pop_back();
        return ReturnCode::Ok;
    }

    ReturnCode getAxisAlignedClip(drawing::Rect* returnClipRect) override;

    ReturnCode clear(const drawing::Color* clearColor) override
    {
        native()->Clear(*(D2D1_COLOR_F*)clearColor);
        return ReturnCode::Ok;
    }

    ReturnCode beginDraw() override
    {
        native()->BeginDraw();
        return ReturnCode::Ok;
    }

    ReturnCode endDraw() override
    {
        native()->EndDraw();
        return ReturnCode::Ok;
    }

    bool SupportSRGB()
    {
        drawing::api::IBitmapPixels::PixelFormat pixelFormat;
        factory->getPlatformPixelFormat(&pixelFormat);
        return pixelFormat == drawing::api::IBitmapPixels::kBGRA_SRGB;
    }

    GMPI_QUERYINTERFACE_METHOD(drawing::api::IDeviceContext);
};

class GraphicsContext final : public GraphicsContext_base
{
public:
    GraphicsContext(ID2D1DeviceContext* deviceContext, drawing::api::IFactory* pfactory) : GraphicsContext_base(deviceContext, pfactory) {}

    GMPI_REFCOUNT_NO_DELETE;
};

class BitmapRenderTarget final : public GraphicsContext_base // emulated by careful layout: public IBitmapRenderTarget
{
    ID2D1BitmapRenderTarget* nativeBitmapRenderTarget = {};

public:
    BitmapRenderTarget(GraphicsContext_base* g, const drawing::Size* desiredSize, drawing::api::IFactory* pfactory) :
        GraphicsContext_base(pfactory)
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
    virtual ReturnCode getBitmap(drawing::api::IBitmap** returnBitmap);

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

// Direct2D context tailored to devices without sRGB high-color support. i.e. Windows 7.
class GraphicsContext_Win7 : public GraphicsContext_base
{
public:
    GraphicsContext_Win7(ID2D1DeviceContext* context, Factory* pfactory) :
        GraphicsContext_base(context, pfactory)
    {}

    ReturnCode createSolidColorBrush(const drawing::Color* color, const drawing::BrushProperties* brushProperties, drawing::api::ISolidColorBrush** returnSolidColorBrush) override
    {
        *returnSolidColorBrush = nullptr;
        gmpi::shared_ptr<gmpi::api::IUnknown> b;
        b.Attach(new SolidColorBrush_Win7(context_, color, factory));
        return b->queryInterface(&drawing::api::ISolidColorBrush::guid, reinterpret_cast<void **>(returnSolidColorBrush));
    }

    //ReturnCode createGradientStopCollection(const drawing::api::MP1_GRADIENT_STOP* gradientStops, uint32_t gradientStopsCount, /* drawing::api::MP1_GAMMA colorInterpolationGamma, drawing::api::MP1_EXTEND_MODE extendMode,*/ drawing::api::IGradientstopCollection** gradientStopCollection) override
    ReturnCode createGradientstopCollection(const drawing::Gradientstop* gradientstops, uint32_t gradientstopsCount, drawing::ExtendMode extendMode, drawing::api::IGradientstopCollection** returnGradientstopCollection) override
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
            //dest.color.r = se_sdk::FastGamma::pixelToNormalised(se_sdk::FastGamma::float_to_sRGB(srce.color.r));
            //dest.color.g = se_sdk::FastGamma::pixelToNormalised(se_sdk::FastGamma::float_to_sRGB(srce.color.g));
            //dest.color.b = se_sdk::FastGamma::pixelToNormalised(se_sdk::FastGamma::float_to_sRGB(srce.color.b));

            constexpr float c = 1.0f / 255.0f;
            dest.color.r = c * static_cast<float>(drawing::linearPixelToSRGB(srce.color.r));
            dest.color.g = c * static_cast<float>(drawing::linearPixelToSRGB(srce.color.g));
            dest.color.b = c * static_cast<float>(drawing::linearPixelToSRGB(srce.color.b));
        }

        return GraphicsContext_base::createGradientstopCollection(stops.data(), gradientstopsCount, extendMode, returnGradientstopCollection);
    }

    ReturnCode clear(const drawing::Color* clearColor) override
    {
        constexpr float c = 1.0f / 255.0f;

        const drawing::Color modified
        {
            c * static_cast<float>(drawing::linearPixelToSRGB(clearColor->r)),
            c * static_cast<float>(drawing::linearPixelToSRGB(clearColor->g)),
            c * static_cast<float>(drawing::linearPixelToSRGB(clearColor->b)),
            clearColor->a
        };

        context_->Clear((D2D1_COLOR_F*)&modified);

        return ReturnCode::Ok;
    }
    GMPI_REFCOUNT;
};
} // Namespace
} // Namespace