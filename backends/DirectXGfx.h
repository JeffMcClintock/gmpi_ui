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

#include <map>
#include <d2d1_2.h>
#include <dwrite.h>
#include <Wincodec.h>
#include "../Drawing.h"

namespace gmpi
{
namespace directx
{
    // helper for return codes
    inline gmpi::ReturnCode toReturnCode(HRESULT r)
    {
        return r == 0 ? gmpi::ReturnCode::Ok : gmpi::ReturnCode::Fail;
    }

    // helpers for wide strings
    std::wstring Utf8ToWstring(std::string_view str);
    std::string WStringToUtf8(std::wstring_view p_cstring);

    // Direct2D context tailored to devices without sRGB high-color support. i.e. Windows 7.
    inline D2D1_COLOR_F toNativeWin7(const drawing::Color* c)
    {
        constexpr float k = 1.0f / 255.0f;
        return D2D1_COLOR_F
        {
            k * static_cast<float>(drawing::linearPixelToSRGB(c->r)),
            k * static_cast<float>(drawing::linearPixelToSRGB(c->g)),
            k * static_cast<float>(drawing::linearPixelToSRGB(c->b)),
            c->a
        };
    }

    // Helper for managing lifetime of Direct2D interface pointers
    template<class wrappedObjT>
    class ComPtr
    {
        mutable wrappedObjT* obj = {};

    public:
        ComPtr() {}

        explicit ComPtr(wrappedObjT* newobj)
        {
            Assign(newobj);
        }
        ComPtr(const ComPtr<wrappedObjT>& value)
        {
            Assign(value.obj);
        }
        // Attach object without incrementing ref count. For objects created with new.
        void Attach(wrappedObjT* newobj)
        {
            wrappedObjT* old = obj;
            obj = newobj;

            if (old)
            {
                old->Release();
            }
        }

        ~ComPtr()
        {
            if (obj)
            {
                obj->Release();
            }
        }
        inline operator wrappedObjT* ()
        {
            return obj;
        }
        const wrappedObjT* operator=(wrappedObjT* value)
        {
            Assign(value);
            return value;
        }
        ComPtr<wrappedObjT>& operator=(ComPtr<wrappedObjT>& value)
        {
            Assign(value.get());
            return *this;
        }
        bool operator==(const wrappedObjT* other) const
        {
            return obj == other;
        }
        bool operator==(const ComPtr<wrappedObjT>& other) const
        {
            return obj == other.obj;
        }
        wrappedObjT* operator->() const
        {
            return obj;
        }

        wrappedObjT*& get()
        {
            return obj;
        }

        wrappedObjT** getAddressOf()
        {
            assert(obj == 0); // Free it before you re-use it!
            return &obj;
        }
        wrappedObjT** put()
        {
            if (obj)
            {
                obj->Release();
                obj = {};
            }

            return &obj;
        }

        void** put_void()
        {
            return (void**)put();
        }

        bool isNull() const
        {
            return obj == nullptr;
        }

        template<typename I>
        ComPtr<I> as()
        {
            ComPtr<I> returnInterface;
            if (obj)
            {
                obj->QueryInterface(__uuidof(I), returnInterface.put_void());
            }
            return returnInterface;
        }

    private:
        // Attach object and increment ref count.
        inline void Assign(wrappedObjT* newobj)
        {
            Attach(newobj);
            if (newobj)
            {
                newobj->AddRef();
            }
        }
    };

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

gmpi::drawing::FontMetrics getFontMetricsHelper(IDWriteTextFormat* textFormat);
gmpi::drawing::Size getTextExtentHelper(IDWriteFactory* writeFactory, IDWriteTextFormat* textFormat, std::string_view s, float topAdjustment, bool useLegacyBaseLineSnapping);

class TextFormat final : public GmpiDXWrapper<drawing::api::ITextFormat, IDWriteTextFormat>
{
    float topAdjustment = {};
    float fontMetrics_ascent = {};
    IDWriteFactory* writeFactory{};

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
    TextFormat(IDWriteFactory* pwriteFactory, IDWriteTextFormat* native) :
        GmpiDXWrapper<drawing::api::ITextFormat, IDWriteTextFormat>(native)
        , writeFactory(pwriteFactory)
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

    GMPI_QUERYINTERFACE_METHOD(drawing::api::ITextFormat);
    GMPI_REFCOUNT
};

inline uint8_t fast8bitScale(uint8_t a, uint8_t b)
{
    const int t = (int)a * (int)b;
    return (uint8_t)((t + 1 + (t >> 8)) >> 8); // fast way to divide by 255
}

inline void premultiplyAlpha(IWICBitmapLock* bitmapLock)
{
    uint8_t* pixel{};
    UINT bufferSize{};
    UINT bytesPerRow{};
	bitmapLock->GetStride(&bytesPerRow);
    bitmapLock->GetDataPointer(&bufferSize, &pixel);

	const auto height = bufferSize / bytesPerRow;
    const auto totalPixels = height * bytesPerRow / sizeof(uint32_t);

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

inline void unpremultiplyAlpha(IWICBitmapLock* bitmapLock)
{
    uint8_t* pixel{};
    UINT bufferSize{};
    UINT bytesPerRow{};
    bitmapLock->GetStride(&bytesPerRow);
    bitmapLock->GetDataPointer(&bufferSize, &pixel);

    const auto height = bufferSize / bytesPerRow;
    const auto totalPixels = height * bytesPerRow / sizeof(uint32_t);

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

class BitmapPixels final : public drawing::api::IBitmapPixels
{
    gmpi::directx::ComPtr<IWICBitmap> bitmap;
    gmpi::directx::ComPtr<IWICBitmapLock> bitmapLock;
    bool alphaPremultiplied{};
    UINT bytesPerRow{};
    BYTE* ptr{};
    int flags{};
    IBitmapPixels::PixelFormat pixelFormat = kBGRA; // default to non-SRGB Win7 (not tested)

public:
    BitmapPixels(ID2D1Bitmap* nativeBitmap, IWICBitmap* inBitmap, bool _alphaPremultiplied, int32_t pflags)
    {
        assert(inBitmap);

        UINT w{}, h{};
        inBitmap->GetSize(&w, &h);

        {
            WICPixelFormatGUID formatGuid{};
            inBitmap->GetPixelFormat(&formatGuid);

            // premultiplied BGRA (default)
            if (std::memcmp(&formatGuid, &GUID_WICPixelFormat32bppPBGRA, sizeof(formatGuid)) == 0)
            {
                pixelFormat = kBGRA_SRGB;
            }
        }

        WICRect rcLock = { 0, 0, (INT)w, (INT)h };
        flags = pflags;

        if (0 <= inBitmap->Lock(&rcLock, flags, bitmapLock.put()))
        {
            bitmapLock->GetStride(&bytesPerRow);
            UINT bufferSize{};
            bitmapLock->GetDataPointer(&bufferSize, &ptr);

            bitmap = inBitmap;

            alphaPremultiplied = _alphaPremultiplied;
            if (!alphaPremultiplied)
                unpremultiplyAlpha(bitmapLock.get());
        }
        else
        {
            alphaPremultiplied = true; // prevent possible null deference of 'bitmap' in destructor
        }
    }

    ~BitmapPixels()
    {
        if (!alphaPremultiplied)
            premultiplyAlpha(bitmapLock.get());
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

    GMPI_QUERYINTERFACE_METHOD(drawing::api::IBitmapPixels);
    GMPI_REFCOUNT
};

// helper function.
ID2D1Bitmap* bitmapToNative(
    ID2D1DeviceContext* nativeContext
    , gmpi::directx::ComPtr<ID2D1Bitmap>& nativeBitmap		// GPU bitmap, created from WIC bitmap or a GPU bitmap render target..
    , gmpi::directx::ComPtr<IWICBitmap>& diBitmap			// WIC bitmap, usually loaded from disk, or created by CPU.
    , ID2D1Factory1* direct2dFactory
    , IWICImagingFactory* wicFactory
);

// helper functions.
gmpi::directx::ComPtr<IWICBitmap> loadWicBitmap(IWICImagingFactory* WICFactory, IWICBitmapDecoder* pDecoder);
void applyPreMultiplyCorrection(IWICBitmap* bitmap);

class Bitmap final : public drawing::api::IBitmap
{
public:
    gmpi::directx::ComPtr<ID2D1Bitmap> nativeBitmap_;
    gmpi::directx::ComPtr<IWICBitmap> diBitmap_;
    ID2D1DeviceContext* nativeContext_;
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
    }

    ~Bitmap(){}

    ID2D1Bitmap* getNativeBitmap(ID2D1DeviceContext* nativeContext);

    ReturnCode getSizeU(drawing::SizeU* returnSize) override
    {
        HRESULT r{ LONG_ERROR };

        if (diBitmap_)
        {
            r = diBitmap_->GetSize(&returnSize->width, &returnSize->height);
        }
        else if (nativeBitmap_)
        {
            const auto sizeU = nativeBitmap_->GetPixelSize(); // ->GetSize(&returnSize->width, &returnSize->height);
			returnSize->width = sizeU.width;
			returnSize->height = sizeU.height;

            r = S_OK;
        }
        else
        {
            *returnSize = {};
        }

        return r == S_OK ? ReturnCode::Ok : ReturnCode::Fail;
    }

    ReturnCode lockPixels(drawing::api::IBitmapPixels** returnPixels, int32_t flags) override;

    ReturnCode getFactory(drawing::api::IFactory** returnFactory) override;

    GMPI_QUERYINTERFACE_METHOD(drawing::api::IBitmap);
    GMPI_REFCOUNT
};

class GradientstopCollection final : public GmpiDXResourceWrapper<drawing::api::IGradientstopCollection, ID2D1GradientStopCollection>
{
public:
    GradientstopCollection(ID2D1GradientStopCollection* native, drawing::api::IFactory* factory) : GmpiDXResourceWrapper(native, factory) {}

    GMPI_QUERYINTERFACE_METHOD(drawing::api::IGradientstopCollection);
    GMPI_REFCOUNT
};

struct Brush
{
    gmpi::directx::ComPtr<ID2D1Brush> native_;
    drawing::api::IFactory* factory_ = {};

    auto native() -> ID2D1Brush* const
    {
        return native_.get();
    }
};

// a imaginary common ancestor for all brushes, that can quickly cast to get hold of the D2D brush.
// only works so long as all bushes are derived *first* from 'Brush'.
// Then we can pretend for the purpose of getting the native brush that all brushes are BrushCommon
struct BrushCommon : public Brush, public drawing::api::IBrush
{};

class BitmapBrush final : public Brush, public drawing::api::IBitmapBrush
{
public:
    BitmapBrush(
        drawing::api::IFactory* factory,
        ID2D1DeviceContext* context,
        const drawing::api::IBitmap* bitmap,
        const drawing::BrushProperties* brushProperties
    )
    {
        factory_ = factory;

        auto bm = ((Bitmap*)bitmap);
        auto nativeBitmap = bm->getNativeBitmap(context);

        // not supported on macOS, so hardcode them.
        D2D1_BITMAP_BRUSH_PROPERTIES bitmapBrushProperties{
            D2D1_EXTEND_MODE_WRAP,
            D2D1_EXTEND_MODE_WRAP,
            D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR,
        };

        [[maybe_unused]] const auto hr = context->CreateBitmapBrush(nativeBitmap, &bitmapBrushProperties, (D2D1_BRUSH_PROPERTIES*)brushProperties, (ID2D1BitmapBrush**)native_.put());
        assert(hr == 0);
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
        // GMPI_QUERYINTERFACE(drawing::api::IBrush);
        GMPI_QUERYINTERFACE(drawing::api::IResource);
        GMPI_QUERYINTERFACE(drawing::api::IBitmapBrush);
        return ReturnCode::NoSupport;
    }

    GMPI_REFCOUNT
};

class SolidColorBrush : public Brush, public drawing::api::ISolidColorBrush
{
public:
	SolidColorBrush(drawing::api::IFactory* factory, ID2D1RenderTarget* context, const drawing::Color* color)
    {
        factory_ = factory;
        context->CreateSolidColorBrush(*(D2D1_COLOR_F*)color, (ID2D1SolidColorBrush**) native_.put());
    }

    ID2D1SolidColorBrush* mynative()
    {
        return (ID2D1SolidColorBrush*) native_.get();
    }

    void setColor(const drawing::Color* color) override
    {
        mynative()->SetColor(*reinterpret_cast<const D2D1_COLOR_F*>(color));
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
        // GMPI_QUERYINTERFACE(drawing::api::IBrush);
        GMPI_QUERYINTERFACE(drawing::api::IResource);
        GMPI_QUERYINTERFACE(drawing::api::ISolidColorBrush);
        return ReturnCode::NoSupport;
    }

    GMPI_REFCOUNT
};

class SolidColorBrush_Win7 final : public SolidColorBrush
{
public:
    SolidColorBrush_Win7(drawing::api::IFactory* factory, ID2D1RenderTarget* context, const drawing::Color* color) : SolidColorBrush(factory, context, color)
    {
        const auto nativeColor = toNativeWin7(color);
        context->CreateSolidColorBrush(nativeColor, (ID2D1SolidColorBrush**)native_.put());
    }

    void setColor(const drawing::Color* color) override
    {
        const auto nativeColor = toNativeWin7(color);
        mynative()->SetColor(&nativeColor);
    }
};

class LinearGradientBrush final : public Brush, public drawing::api::ILinearGradientBrush
{
public:
    LinearGradientBrush(drawing::api::IFactory* factory, ID2D1RenderTarget* context, const drawing::LinearGradientBrushProperties* linearGradientBrushProperties, const drawing::BrushProperties* brushProperties, const  drawing::api::IGradientstopCollection* gradientStopCollection)
    {
        factory_ = factory;
        [[maybe_unused]] HRESULT hr = context->CreateLinearGradientBrush((D2D1_LINEAR_GRADIENT_BRUSH_PROPERTIES*)linearGradientBrushProperties, (D2D1_BRUSH_PROPERTIES*)brushProperties, ((GradientstopCollection*)gradientStopCollection)->native(), (ID2D1LinearGradientBrush **)native_.put());
        assert(hr == 0);
    }

    ID2D1LinearGradientBrush* mynative()
    {
        return (ID2D1LinearGradientBrush*)native_.get();
    }

    void setStartPoint(drawing::Point startPoint) override
    {
        mynative()->SetStartPoint(*reinterpret_cast<D2D1_POINT_2F*>(&startPoint));
    }
    void setEndPoint(drawing::Point endPoint) override
    {
        mynative()->SetEndPoint(*reinterpret_cast<D2D1_POINT_2F*>(&endPoint));
    }

    ReturnCode queryInterface(const gmpi::api::Guid* iid, void** returnInterface) override
    {
        *returnInterface = {};
        // GMPI_QUERYINTERFACE(drawing::api::IBrush);
        GMPI_QUERYINTERFACE(drawing::api::IResource);
        GMPI_QUERYINTERFACE(drawing::api::ILinearGradientBrush);
        return ReturnCode::NoSupport;
    }

    // IResource
    ReturnCode getFactory(drawing::api::IFactory** factory) override
    {
        *factory = factory_;
        return ReturnCode::Ok;
    }
    GMPI_REFCOUNT
};

class RadialGradientBrush final : public Brush, public drawing::api::IRadialGradientBrush
{
public:
    RadialGradientBrush(drawing::api::IFactory* factory, ID2D1RenderTarget* context, const drawing::RadialGradientBrushProperties* linearGradientBrushProperties, const drawing::BrushProperties* brushProperties, const drawing::api::IGradientstopCollection* gradientStopCollection)
    {
        factory_ = factory;
        [[maybe_unused]] HRESULT hr = context->CreateRadialGradientBrush((D2D1_RADIAL_GRADIENT_BRUSH_PROPERTIES*)linearGradientBrushProperties, (D2D1_BRUSH_PROPERTIES*)brushProperties, ((GradientstopCollection*)gradientStopCollection)->native(), (ID2D1RadialGradientBrush **)native_.put());
        assert(hr == 0);
    }

    inline ID2D1RadialGradientBrush* mynative()
    {
        return (ID2D1RadialGradientBrush*)native_.get();
    }

    void setCenter(drawing::Point center) override
    {
        mynative()->SetCenter(*reinterpret_cast<D2D1_POINT_2F*>(&center));
    }

    void setGradientOriginOffset(drawing::Point gradientOriginOffset) override
    {
        mynative()->SetGradientOriginOffset(*reinterpret_cast<D2D1_POINT_2F*>(&gradientOriginOffset));
    }

    void setRadiusX(float radiusX) override
    {
        mynative()->SetRadiusX(radiusX);
    }

    void setRadiusY(float radiusY) override
    {
        mynative()->SetRadiusY(radiusY);
    }

    ReturnCode queryInterface(const gmpi::api::Guid* iid, void** returnInterface) override
    {
        *returnInterface = {};
        // GMPI_QUERYINTERFACE(drawing::api::IBrush);
        GMPI_QUERYINTERFACE(drawing::api::IResource);
        GMPI_QUERYINTERFACE(drawing::api::IRadialGradientBrush);
        return ReturnCode::NoSupport;
    }

    // IResource
    ReturnCode getFactory(drawing::api::IFactory** factory) override
    {
        *factory = factory_;
        return ReturnCode::Ok;
    }
    GMPI_REFCOUNT
};

class StrokeStyle final : public GmpiDXResourceWrapper<drawing::api::IStrokeStyle, ID2D1StrokeStyle>
{
public:
    StrokeStyle(ID2D1StrokeStyle* native, drawing::api::IFactory* factory) : GmpiDXResourceWrapper(native, factory) {}

    GMPI_QUERYINTERFACE_METHOD(drawing::api::IStrokeStyle);
    GMPI_REFCOUNT
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
    GMPI_REFCOUNT
};


class Geometry final : public drawing::api::IPathGeometry
{
    friend class GraphicsContext;

    gmpi::directx::ComPtr<ID2D1PathGeometry> geometry_;

public:
    Geometry(ID2D1PathGeometry* geom)
    {
		geometry_.Attach(geom);
    }

    ID2D1PathGeometry* native()
    {
        return geometry_.get();
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
    GMPI_REFCOUNT
};

struct fontScaling
{
    std::wstring systemFontName; // mixed-case
    float bodyHeight{};
    float capHeight{};
};

struct DxFactoryInfo
{
    gmpi::directx::ComPtr<ID2D1Factory1> d2dFactory;
    gmpi::directx::ComPtr<IDWriteFactory> writeFactory;
    gmpi::directx::ComPtr<IWICImagingFactory> wicFactory;

    std::unordered_map<std::string, fontScaling> availableFonts; // lowercase name mapping to scaling information.
    std::vector<std::string> supportedFontFamilies;              // actual system font names, mixed case.
    std::map<std::string, std::wstring> GdiFontConversions;
//    bool DX_support_sRGB = true;
};

void initFactoryHelper(DxFactoryInfo& info);

std::wstring fontMatchHelper(
      IDWriteFactory* writeFactory
    , std::map<std::string, std::wstring>& GdiFontConversions
    , std::string fontName
    , drawing::FontWeight fontWeight
);

class Factory_base : public drawing::api::IFactory
{
protected:
    DxFactoryInfo& info;

public:
    Factory_base(DxFactoryInfo& pinfo);

    gmpi::directx::DxFactoryInfo& getInfo() { return info; }

    auto getWicFactory()
	{
		return info.wicFactory;
	}
    auto getDirectWriteFactory()
    {
        return info.writeFactory;
    }
    auto getFactory()
    {
        return info.d2dFactory;
    }

    //void setSrgbSupport(bool s)
    //{
    //    info.DX_support_sRGB = s;
    //}
            
    ReturnCode getPlatformPixelFormat(drawing::api::IBitmapPixels::PixelFormat* returnPixelFormat) override
    {
//        *returnPixelFormat = info.DX_support_sRGB ? drawing::api::IBitmapPixels::kBGRA_SRGB : drawing::api::IBitmapPixels::kBGRA;
        *returnPixelFormat = drawing::api::IBitmapPixels::kBGRA_SRGB;
        return ReturnCode::Ok;
    }

    ID2D1Factory1* getD2dFactory()
    {
        return info.d2dFactory;
    }
    ReturnCode createPathGeometry(drawing::api::IPathGeometry** returnPathGeometry) override;
    ReturnCode createCpuRenderTarget(drawing::SizeU size, int32_t flags, drawing::api::IBitmapRenderTarget** returnBitmapRenderTarget) override;
    ReturnCode createTextFormat(const char* fontFamilyName, drawing::FontWeight fontWeight, drawing::FontStyle fontStyle, drawing::FontStretch fontStretch, float fontHeight, int32_t fontFlags, drawing::api::ITextFormat** returnTextFormat) override;
    ReturnCode createImage(int32_t width, int32_t height, int32_t flags, drawing::api::IBitmap** returnBitmap) override;
    ReturnCode loadImageU(const char* uri, drawing::api::IBitmap** returnBitmap) override;
    ReturnCode createStrokeStyle(const drawing::StrokeStyleProperties* strokeStyleProperties, const float* dashes, int32_t dashesCount, drawing::api::IStrokeStyle** returnStrokeStyle) override
    {
        *returnStrokeStyle = {};

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

        ID2D1StrokeStyle* b{};
        auto r = info.d2dFactory->CreateStrokeStyle(&nativeProperties, dashes, dashesCount, &b);

        if (r == S_OK)
        {
            gmpi::shared_ptr<gmpi::api::IUnknown> wrapper;
            wrapper.attach(new StrokeStyle(b, this));

            return wrapper->queryInterface(&drawing::api::IStrokeStyle::guid, reinterpret_cast<void**>(returnStrokeStyle));
        }

        return r == S_OK ? ReturnCode::Ok : ReturnCode::Fail;
    }

    ReturnCode getFontFamilyName(int32_t fontIndex, gmpi::api::IString* returnName) override;

	gmpi::ReturnCode queryInterface(const gmpi::api::Guid* iid, void** returnInterface) override {
		*returnInterface = {};
		GMPI_QUERYINTERFACE(drawing::api::IFactory)
		return gmpi::ReturnCode::NoSupport;
	}

    GMPI_REFCOUNT_NO_DELETE;
};

class Factory : public Factory_base
{
    DxFactoryInfo concreteInfo;

public:
    Factory();
};

class GraphicsContext_base : public drawing::api::IDeviceContext
{
protected:
    gmpi::directx::ComPtr<ID2D1DeviceContext> context_;

    drawing::api::IFactory* factory{};
    std::vector<drawing::Rect> clipRectStack;

    GraphicsContext_base(drawing::api::IFactory* pfactory, ID2D1DeviceContext* deviceContext = {}) :
        context_(deviceContext)
        , factory(pfactory)
    {
        const float defaultClipBounds = 100000.0f;
        drawing::Rect r;
        r.top = r.left = -defaultClipBounds;
        r.bottom = r.right = defaultClipBounds;
        clipRectStack.push_back(r);
    }

public:
//    virtual ~GraphicsContext_base(){}

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
        context_->DrawRectangle(
            D2D1::RectF(rect->left, rect->top, rect->right, rect->bottom)
            , static_cast<BrushCommon*>(brush)->native()
            , strokeWidth, toNative(strokeStyle)
        );
        return ReturnCode::Ok;
    }

    ReturnCode fillRectangle(const drawing::Rect* rect, drawing::api::IBrush* brush) override
    {
        context_->FillRectangle(
            (D2D1_RECT_F*)rect
            , static_cast<BrushCommon*>(brush)->native()
        );
        return ReturnCode::Ok;
    }

    ReturnCode drawRoundedRectangle(const drawing::RoundedRect* roundedRect, drawing::api::IBrush* brush, float strokeWidth, drawing::api::IStrokeStyle* strokeStyle) override
    {
        context_->DrawRoundedRectangle(
            (D2D1_ROUNDED_RECT*)roundedRect
            , static_cast<BrushCommon*>(brush)->native()
            , (FLOAT)strokeWidth
            , toNative(strokeStyle)
        );
        return ReturnCode::Ok;
    }

    ReturnCode fillRoundedRectangle(const drawing::RoundedRect* roundedRect, drawing::api::IBrush* brush) override
    {
        context_->FillRoundedRectangle(
            (D2D1_ROUNDED_RECT*)roundedRect
            , static_cast<BrushCommon*>(brush)->native()
        );
        return ReturnCode::Ok;
    }

    ReturnCode drawEllipse(const drawing::Ellipse* ellipse, drawing::api::IBrush* brush, float strokeWidth, drawing::api::IStrokeStyle* strokeStyle) override
    {
        context_->DrawEllipse(
            (D2D1_ELLIPSE*)ellipse
            , static_cast<BrushCommon*>(brush)->native()
            , (FLOAT)strokeWidth
            , toNative(strokeStyle)
        );
        return ReturnCode::Ok;
    }

    ReturnCode fillEllipse(const drawing::Ellipse* ellipse, drawing::api::IBrush* brush) override
    {
        context_->FillEllipse(
            (D2D1_ELLIPSE*)ellipse
            , static_cast<BrushCommon*>(brush)->native()
        );
        return ReturnCode::Ok;
    }

    ReturnCode drawLine(drawing::Point point0, drawing::Point point1, drawing::api::IBrush* brush, float strokeWidth, drawing::api::IStrokeStyle* strokeStyle) override
    {
        context_->DrawLine(
            *((D2D_POINT_2F*)&point0)
            , *((D2D_POINT_2F*)&point1)
            , static_cast<BrushCommon*>(brush)->native()
            , strokeWidth
            , toNative(strokeStyle)
        );
        return ReturnCode::Ok;
    }

    ReturnCode drawGeometry(drawing::api::IPathGeometry* pathGeometry, drawing::api::IBrush* brush, float strokeWidth, drawing::api::IStrokeStyle* strokeStyle) override;

    ReturnCode fillGeometry(drawing::api::IPathGeometry* pathGeometry, drawing::api::IBrush* brush, drawing::api::IBrush* opacityBrush) override
    {
        auto d2d_geometry = ((Geometry*)pathGeometry)->native();

        ID2D1Brush* opacityBrushNative{};
        if (opacityBrush)
        {
            opacityBrushNative = static_cast<BrushCommon*>(brush)->native();
        }

        context_->FillGeometry(
            d2d_geometry
            , static_cast<BrushCommon*>(brush)->native()
            , opacityBrushNative
        );
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
        *returnObject = {};
        gmpi::shared_ptr<gmpi::api::IUnknown> b2;
        b2.attach(object);
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
        *returnBitmapBrush = {};
        gmpi::shared_ptr<gmpi::api::IUnknown> b2;
        b2.attach(new BitmapBrush(factory, context_, bitmap, /*bitmapBrushProperties, */brushProperties));
        return b2->queryInterface(&drawing::api::IBitmapBrush::guid, reinterpret_cast<void **>(returnBitmapBrush));
    }
    ReturnCode createRadialGradientBrush(const drawing::RadialGradientBrushProperties* radialGradientBrushProperties, const drawing::BrushProperties* brushProperties, drawing::api::IGradientstopCollection* gradientstopCollection, drawing::api::IRadialGradientBrush** returnRadialGradientBrush) override
    {
        *returnRadialGradientBrush = {};
        gmpi::shared_ptr<gmpi::api::IUnknown> b2;
        b2.attach(new RadialGradientBrush(factory, context_, radialGradientBrushProperties, brushProperties, gradientstopCollection));
        return b2->queryInterface(&drawing::api::IRadialGradientBrush::guid, reinterpret_cast<void **>(returnRadialGradientBrush));
    }

    ReturnCode createCompatibleRenderTarget(drawing::Size desiredSize, int32_t flags, drawing::api::IBitmapRenderTarget** bitmapRenderTarget) override;

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
    GraphicsContext(ID2D1DeviceContext* deviceContext, drawing::api::IFactory* pfactory) : GraphicsContext_base(pfactory, deviceContext) {}

    GMPI_REFCOUNT_NO_DELETE;
};

// helper function
void createBitmapRenderTarget(
      UINT width
    , UINT height
    , int32_t flags
    , ID2D1DeviceContext* outerDeviceContext
    , ID2D1Factory1* d2dFactory
    , IWICImagingFactory* wicFactory
    , gmpi::directx::ComPtr<IWICBitmap>& returnWicBitmap
    , gmpi::directx::ComPtr<ID2D1DeviceContext>& returnContext
);

class BitmapRenderTarget final : public GraphicsContext_base // emulated by careful layout: public IBitmapRenderTarget
{
    gmpi::directx::ComPtr<IWICBitmap> wicBitmap;
    ID2D1DeviceContext* originalContext{};

public:
    BitmapRenderTarget(GraphicsContext_base* g, const drawing::Size* desiredSize, int32_t flags, drawing::api::IFactory* pfactory);

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

    GMPI_REFCOUNT
};

#if 0
class GraphicsContext_Win7 : public GraphicsContext_base
{
public:
    GraphicsContext_Win7(ID2D1DeviceContext* context, Factory* pfactory) :
        GraphicsContext_base(pfactory, context)
    {}

    ReturnCode createSolidColorBrush(const drawing::Color* color, const drawing::BrushProperties* brushProperties, drawing::api::ISolidColorBrush** returnSolidColorBrush) override
    {
        *returnSolidColorBrush = {};
        gmpi::shared_ptr<gmpi::api::IUnknown> b;
        b.attach(new SolidColorBrush_Win7(factory, context_, color));
        return b->queryInterface(&drawing::api::ISolidColorBrush::guid, reinterpret_cast<void **>(returnSolidColorBrush));
    }

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

            (*(D2D1_COLOR_F*) &dest.color) = toNativeWin7(&srce.color);
        }

        return GraphicsContext_base::createGradientstopCollection(stops.data(), gradientstopsCount, extendMode, returnGradientstopCollection);
    }

    ReturnCode clear(const drawing::Color* color) override
    {
        const auto native = toNativeWin7((const drawing::Color*)color);
        context_->Clear(&native);
        return ReturnCode::Ok;
    }
    GMPI_REFCOUNT
};
#endif
} // Namespace
} // Namespace