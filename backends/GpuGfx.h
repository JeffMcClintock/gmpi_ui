#pragma once

// GPU backend over the gmpi-ui graphics fork (origin: VitalAudio/visage, bgfx
// underneath: D3D11 / Metal / Vulkan). See VISAGE_BACKEND_PLAN.md.
//
// Every surface is RGBA16F premultiplied linear (the fork's default), the
// working format of the CPU backend and a D2D 64bppPRGBAHalf target.
//
// Two paths to pixels:
//
//   GPU   solid-brush fills and strokes go through the fork's analytic
//         path-coverage atlas: figures are flattened already (Gfx_base sink),
//         strokes are widened by the cpugfx contour stroker (D2D-exact caps,
//         joins and dashes) and filled nonzero.
//
//   CPU   anything the GPU path cannot express yet is rendered by a cpugfx
//         render target the size of the draw's device bounds, uploaded as an
//         RGBA16F image and composited premultiplied: gradient and bitmap
//         brushes, opacity brushes, drawBitmap, and every draw under a
//         geometry clip. Correct by construction (cpugfx matches D2D); the
//         GPU-native versions replace these one at a time with the test
//         suite as the oracle. See VISAGE_BACKEND_PLAN.md, milestone 2.
//
// Geometry, stroke styles, brushes, images and flagged (Mask / SRGB /
// CpuReadable) targets are the cpugfx classes: a PathGeometry is flattened
// figures whichever backend fills them, and a brush is "a thing that
// evaluates spans" to the fallback. Only the fp16 render target is new.

#include "CpuGfx.h"

#include "visage_graphics/canvas.h"
#include "visage_graphics/path.h"
#include "visage_graphics/screenshot.h"

#include <functional>
#include <memory>
#include <vector>

namespace gmpi::gpugfx
{

class Factory;

// Bytes of a cpugfx surface (fp16 premultiplied, rows padded to 8 px) packed
// into width*height*8 bytes, optionally cropped.
inline std::vector<uint16_t> packSurface(const cpugfx::Surface& s, int32_t left, int32_t top, int32_t w, int32_t h)
{
    std::vector<uint16_t> out(size_t(w) * size_t(h) * 4);
    for (int32_t y = 0; y < h; ++y)
        std::memcpy(out.data() + size_t(y) * w * 4, s.pixels + (size_t(top + y) * s.stridePixels + left) * 4, size_t(w) * 8);
    return out;
}

// ---------------------------------------------------------------------------
// The bitmap of a GPU render target: owns the canvas. A target draws into it;
// lockPixels reads it back (and writes it back on release).
// ---------------------------------------------------------------------------
class Bitmap final : public drawing::api::IBitmap
{
public:
    drawing::api::IFactory* factory{};
    visage::Canvas canvas;
    int32_t width{}, height{};
    bool rendered{ false }; // canvas.submit() has run since the last draw call

    // Raw fp16 buffers the canvas is drawing from. The atlas reads them again
    // whenever it repacks, so they live as long as the bitmap does.
    std::vector<std::unique_ptr<std::vector<uint16_t>>> uploads;

    Bitmap(drawing::api::IFactory* pfactory, int32_t w, int32_t h)
        : factory(pfactory), width((std::max)(1, w)), height((std::max)(1, h))
    {
        canvas.setWindowless(width, height);
    }

    void submit()
    {
        canvas.submit();
        rendered = true;
    }

    // Composited pixels as premultiplied linear fp16 RGBA, tightly packed.
    std::vector<uint16_t> readBack()
    {
        if (!rendered)
            submit();
        const visage::Screenshot& shot = canvas.takeScreenshot();
        std::vector<uint16_t> out(size_t(width) * size_t(height) * 4, 0);
        if (shot.halfFloat() && shot.width() == width && shot.height() == height)
            std::copy_n(shot.halfData(), out.size(), out.data());
        return out;
    }

    // Draw tightly packed fp16 premultiplied pixels (w x h) at device
    // position (x, y), scaled to (dw, dh), scaled by `opacity`, through the
    // given clamp. Composite = premultiplied over; Opaque = replace.
    void drawHalfImage(std::vector<uint16_t>&& pixels, int32_t w, int32_t h,
                       float x, float y, float dw, float dh, float opacity,
                       const visage::ClampBounds& clamp, visage::BlendMode blend)
    {
        if (w <= 0 || h <= 0 || pixels.empty())
            return;
        uploads.push_back(std::make_unique<std::vector<uint16_t>>(std::move(pixels)));
        const auto* data = reinterpret_cast<const unsigned char*>(uploads.back()->data());

        canvas.setClampBounds(clamp);
        canvas.setBlendMode(blend);
        canvas.setColor(visage::Color(opacity, opacity, opacity, opacity)); // premultiplied scale
        canvas.halfImage(data, w, h, x, y, dw, dh);
        canvas.setBlendMode(visage::BlendMode::Alpha);
        rendered = false;
    }

    // Replace the whole target with these pixels (a write lock's result).
    void writeBack(std::vector<uint16_t>&& pixels)
    {
        drawHalfImage(std::move(pixels), width, height, 0.0f, 0.0f, float(width), float(height), 1.0f,
                      visage::ClampBounds{ 0.0f, 0.0f, float(width), float(height) }, visage::BlendMode::Opaque);
    }

    ReturnCode getSizeU(drawing::SizeU* returnSize) override
    {
        *returnSize = { uint32_t(width), uint32_t(height) };
        return ReturnCode::Ok;
    }

    ReturnCode lockPixels(drawing::api::IBitmapPixels** returnPixels, int32_t flags) override;

    ReturnCode getFactory(drawing::api::IFactory** returnFactory) override
    {
        *returnFactory = factory;
        return ReturnCode::Ok;
    }

    ReturnCode queryInterface(const gmpi::api::Guid* iid, void** returnInterface) override
    {
        *returnInterface = {};
        GMPI_QUERYINTERFACE(drawing::api::IBitmap);
        GMPI_QUERYINTERFACE(drawing::api::IResource);
        return ReturnCode::NoSupport;
    }
    GMPI_REFCOUNT;
};

// ---------------------------------------------------------------------------
// Locked pixels: a read-back copy, written back to the GPU on release when
// the lock allowed writing.
// ---------------------------------------------------------------------------
class BitmapPixels final : public drawing::api::IBitmapPixels
{
    Bitmap* bitmap; // keeps the canvas alive while locked
    std::vector<uint16_t> pixels;
    bool writable{};

public:
    BitmapPixels(Bitmap* pbitmap, int32_t flags) : bitmap(pbitmap), writable(flags != int32_t(drawing::BitmapLockFlags::Read))
    {
        bitmap->addRef();
        pixels = bitmap->readBack();
    }

    ~BitmapPixels()
    {
        if (writable)
            bitmap->writeBack(std::move(pixels));
        bitmap->release();
    }

    ReturnCode getAddress(uint8_t** returnAddress) override
    {
        *returnAddress = reinterpret_cast<uint8_t*>(pixels.data());
        return ReturnCode::Ok;
    }

    ReturnCode getBytesPerRow(int32_t* returnBytesPerRow) override
    {
        *returnBytesPerRow = bitmap->width * 8;
        return ReturnCode::Ok;
    }

    ReturnCode getPixelFormat(int32_t* returnPixelFormat) override
    {
        *returnPixelFormat = drawing::api::IBitmapPixels::RGBA_16f;
        return ReturnCode::Ok;
    }

    GMPI_QUERYINTERFACE_METHOD(drawing::api::IBitmapPixels);
    GMPI_REFCOUNT;
};

inline ReturnCode Bitmap::lockPixels(drawing::api::IBitmapPixels** returnPixels, int32_t flags)
{
    *returnPixels = new BitmapPixels(this, flags);
    return ReturnCode::Ok;
}

// ---------------------------------------------------------------------------
// Render target.
// ---------------------------------------------------------------------------
class RenderTarget final : public se::generic_graphics::GraphicsContextT<drawing::api::IBitmapRenderTarget>
{
    drawing::api::IFactory* factory{};
    Bitmap* bitmap{}; // owned (refcounted)

    // Effective local -> device transform (client transform then DPI scale),
    // and what setTransform() was handed, which getTransform() must return.
    drawing::Matrix3x2 transform_;
    drawing::Matrix3x2 clientTransform_;
    drawing::Matrix3x2 dpiTransform_;

    // Geometry clips, parallel to clipRectStack: null means "rect clip only
    // at this depth". Each remembers the transform it was pushed under, so
    // the fallback can replay it exactly.
    struct GeometryClip
    {
        gmpi::shared_ptr<cpugfx::PathGeometry> geometry;
        drawing::Matrix3x2 transform;
    };
    std::vector<std::shared_ptr<const GeometryClip>> clipGeometryStack{ nullptr };

    bool geometryClipActive() const { return clipGeometryStack.back() != nullptr; }

    visage::Canvas& canvas() { return bitmap->canvas; }

    drawing::Rect surfaceBounds() const { return { 0.0f, 0.0f, float(bitmap->width), float(bitmap->height) }; }

    // The device-space clip as the canvas's clamp bounds.
    visage::ClampBounds currentClamp() const
    {
        const auto c = drawing::intersectRect(clipRectStack.back(), surfaceBounds());
        return { c.left, c.top, (std::max)(c.left, c.right), (std::max)(c.top, c.bottom) };
    }

    static bool finite(const drawing::Color& c)
    {
        return std::isfinite(c.r) && std::isfinite(c.g) && std::isfinite(c.b) && std::isfinite(c.a);
    }

    // GMPI colour is linear straight RGBA; the fork takes alpha first.
    static visage::Color toVisage(const drawing::Color& c, float opacity)
    {
        return visage::Color(c.a * opacity, c.r, c.g, c.b);
    }

    // --- GPU path -----------------------------------------------------------

    // Fill flattened figures through the path atlas. Points are transformed
    // to device space here, on the CPU.
    ReturnCode fillFiguresGpu(const std::vector<cpugfx::Figure>& figures, drawing::FillMode fillMode, const cpugfx::SolidColorBrush& brush)
    {
        if (!brush.isPaintable())
            return ReturnCode::Ok;

        visage::Path vpath;
        for (const auto& figure : figures)
        {
            if (!figure.filled || figure.points.size() < 2)
                continue;
            bool first = true;
            for (const auto& p : figure.points)
            {
                const auto d = drawing::transformPoint(transform_, p);
                if (!std::isfinite(d.x) || !std::isfinite(d.y))
                    return ReturnCode::Ok; // D2D draws nothing for a non-finite figure
                if (first)
                    vpath.moveTo(d.x, d.y);
                else
                    vpath.lineTo(d.x, d.y);
                first = false;
            }
            vpath.close(); // a fill is always closed, whatever the sink said
        }
        if (vpath.numPoints() == 0)
            return ReturnCode::Ok;

        vpath.setFillRule(fillMode == drawing::FillMode::Winding ? visage::Path::FillRule::NonZero
                                                                 : visage::Path::FillRule::EvenOdd);
        auto& c = canvas();
        c.setClampBounds(currentClamp());
        c.setColor(toVisage(brush.color, brush.opacity));
        c.fill(vpath, 0, 0);
        bitmap->rendered = false;
        return ReturnCode::Ok;
    }

    // --- CPU fallback -------------------------------------------------------

    // Render `draw` through a cpugfx target covering `deviceBounds` (clipped
    // to the current clip and the surface), with this context's transform and
    // geometry clips replayed, then composite the result premultiplied.
    ReturnCode viaCpu(drawing::Rect deviceBounds, const std::function<ReturnCode(cpugfx::RenderTarget&)>& draw)
    {
        deviceBounds = drawing::intersectRect(deviceBounds, drawing::intersectRect(clipRectStack.back(), surfaceBounds()));
        if (!(deviceBounds.right > deviceBounds.left) || !(deviceBounds.bottom > deviceBounds.top))
            return ReturnCode::Ok;

        const int32_t left = int32_t(std::floor(deviceBounds.left));
        const int32_t top = int32_t(std::floor(deviceBounds.top));
        const int32_t w = int32_t(std::ceil(deviceBounds.right)) - left;
        const int32_t h = int32_t(std::ceil(deviceBounds.bottom)) - top;
        if (w <= 0 || h <= 0)
            return ReturnCode::Ok;

        gmpi::shared_ptr<cpugfx::RenderTarget> cpu;
        cpu.attach(new cpugfx::RenderTarget(factory, { uint32_t(w), uint32_t(h) }, 0, 96.0f));

        const auto offset = drawing::makeTranslation(float(-left), float(-top));

        // Replay geometry clips under the transforms they were pushed with.
        for (const auto& clip : clipGeometryStack)
        {
            if (!clip)
                continue;
            const auto t = clip->transform * offset;
            cpu->setTransform(&t);
            cpu->pushClipGeometry(clip->geometry.get());
        }
        const auto t = transform_ * offset;
        cpu->setTransform(&t);

        cpu->beginDraw();
        const auto result = draw(*cpu);
        cpu->endDraw();

        gmpi::shared_ptr<drawing::api::IBitmap> cpuBitmap;
        cpu->getBitmap(cpuBitmap.put());
        auto* cb = dynamic_cast<cpugfx::Bitmap*>(cpuBitmap.get());
        if (!cb)
            return ReturnCode::Fail;

        bitmap->drawHalfImage(packSurface(cb->surface, 0, 0, w, h), w, h, float(left), float(top), float(w), float(h),
                              1.0f, currentClamp(), visage::BlendMode::Composite);
        return result;
    }

    drawing::Rect figuresDeviceBounds(const std::vector<cpugfx::Figure>& figures) const
    {
        float minX = 1e30f, minY = 1e30f, maxX = -1e30f, maxY = -1e30f;
        for (const auto& fig : figures)
            for (const auto& p : fig.points)
            {
                const auto d = drawing::transformPoint(transform_, p);
                minX = (std::min)(minX, d.x); minY = (std::min)(minY, d.y);
                maxX = (std::max)(maxX, d.x); maxY = (std::max)(maxY, d.y);
            }
        if (!(maxX >= minX) || !(maxY >= minY))
            return {};
        return { minX - 1.0f, minY - 1.0f, maxX + 1.0f, maxY + 1.0f }; // a pixel of AA slack
    }

    // A source bitmap as a cpugfx bitmap: itself, or a GPU target read back.
    gmpi::shared_ptr<cpugfx::Bitmap> asCpuBitmap(drawing::api::IBitmap* source)
    {
        gmpi::shared_ptr<cpugfx::Bitmap> result;
        if (auto* cb = dynamic_cast<cpugfx::Bitmap*>(source))
        {
            cb->addRef();
            result.attach(cb);
            return result;
        }
        if (auto* gb = dynamic_cast<Bitmap*>(source))
        {
            result.attach(new cpugfx::Bitmap(factory, gb->width, gb->height));
            const auto pixels = gb->readBack();
            auto& s = result->surface;
            for (int32_t y = 0; y < gb->height; ++y)
                std::memcpy(s.row(y), pixels.data() + size_t(y) * gb->width * 4, size_t(gb->width) * 8);
        }
        return result;
    }

public:
    RenderTarget(drawing::api::IFactory* pfactory, drawing::SizeU size, int32_t /*flags*/, float dpi)
        : factory(pfactory)
    {
        const float dpiScale = dpi / 96.0f;
        dpiTransform_._11 = dpiTransform_._22 = dpiScale;
        transform_ = dpiTransform_;
        bitmap = new Bitmap(factory, int32_t(size.width), int32_t(size.height));
    }

    ~RenderTarget() override
    {
        bitmap->release();
    }

    // --- brushes ------------------------------------------------------------

    ReturnCode createSolidColorBrush(const drawing::Color* color, const drawing::BrushProperties* brushProperties, drawing::api::ISolidColorBrush** returnSolidColorBrush) override
    {
        *returnSolidColorBrush = new cpugfx::SolidColorBrush(factory, color, brushProperties);
        return ReturnCode::Ok;
    }

    ReturnCode createBitmapBrush(drawing::api::IBitmap* sourceBitmap, const drawing::BrushProperties* brushProperties, drawing::api::IBitmapBrush** returnBitmapBrush) override
    {
        *returnBitmapBrush = {};
        auto source = asCpuBitmap(sourceBitmap);
        if (!source)
            return ReturnCode::NoSupport;
        *returnBitmapBrush = new cpugfx::BitmapBrush(factory, source.get(), brushProperties);
        return ReturnCode::Ok;
    }

    ReturnCode createGradientstopCollection(const drawing::Gradientstop* gradientstops, uint32_t gradientstopsCount, drawing::ExtendMode extendMode, drawing::api::IGradientstopCollection** returnCollection) override
    {
        *returnCollection = new cpugfx::GradientstopCollection(factory, gradientstops, gradientstopsCount, extendMode);
        return ReturnCode::Ok;
    }

    ReturnCode createLinearGradientBrush(const drawing::LinearGradientBrushProperties* props, const drawing::BrushProperties* brushProperties, drawing::api::IGradientstopCollection* collection, drawing::api::ILinearGradientBrush** returnBrush) override
    {
        *returnBrush = new cpugfx::LinearGradientBrush(factory, props, brushProperties, collection);
        return ReturnCode::Ok;
    }

    ReturnCode createRadialGradientBrush(const drawing::RadialGradientBrushProperties* props, const drawing::BrushProperties* brushProperties, drawing::api::IGradientstopCollection* collection, drawing::api::IRadialGradientBrush** returnBrush) override
    {
        *returnBrush = new cpugfx::RadialGradientBrush(factory, props, brushProperties, collection);
        return ReturnCode::Ok;
    }

    // --- state --------------------------------------------------------------

    ReturnCode setTransform(const drawing::Matrix3x2* ptransform) override
    {
        clientTransform_ = *ptransform;
        transform_       = clientTransform_ * dpiTransform_;
        return ReturnCode::Ok;
    }

    ReturnCode getTransform(drawing::Matrix3x2* returnTransform) override
    {
        *returnTransform = clientTransform_;
        return ReturnCode::Ok;
    }

    ReturnCode pushAxisAlignedClip(const drawing::Rect* clipRect) override
    {
        clipRectStack.push_back(drawing::intersectRect(clipRectStack.back(), drawing::transformRect(transform_, *clipRect)));
        clipGeometryStack.push_back(clipGeometryStack.back());
        return ReturnCode::Ok;
    }

    // Clip to an arbitrary geometry. Draws under it take the CPU fallback,
    // where cpugfx rasterises the clip as antialiased coverage like D2D.
    ReturnCode pushClipGeometry(drawing::api::IPathGeometry* geometry) override
    {
        auto* path = dynamic_cast<cpugfx::PathGeometry*>(geometry);
        if (!path)
            return ReturnCode::NoSupport;

        auto clip = std::make_shared<GeometryClip>();
        path->addRef();
        clip->geometry.attach(path);
        clip->transform = transform_;

        clipRectStack.push_back(drawing::intersectRect(clipRectStack.back(), figuresDeviceBounds(path->figures)));
        clipGeometryStack.push_back(std::move(clip));
        return ReturnCode::Ok;
    }

    ReturnCode popAxisAlignedClip() override
    {
        if (clipRectStack.size() > 1)
            clipRectStack.pop_back();
        if (clipGeometryStack.size() > 1)
            clipGeometryStack.pop_back();
        return ReturnCode::Ok;
    }

    ReturnCode getAxisAlignedClip(drawing::Rect* returnClipRect) override
    {
        *returnClipRect = drawing::transformRect(drawing::invert(transform_), clipRectStack.back());
        return ReturnCode::Ok;
    }

    ReturnCode beginDraw() override { return ReturnCode::Ok; }

    ReturnCode endDraw() override
    {
        bitmap->submit();
        return ReturnCode::Ok;
    }

    // --- drawing ------------------------------------------------------------

    ReturnCode clear(const drawing::Color* clearColor) override
    {
        if (!finite(*clearColor))
            return ReturnCode::Ok;

        auto& c = canvas();
        c.setClampBounds(currentClamp());
        c.setBlendMode(visage::BlendMode::Opaque);
        c.setColor(toVisage(*clearColor, 1.0f));
        c.fill(); // the whole clamp area
        c.setBlendMode(visage::BlendMode::Alpha);
        bitmap->rendered = false;
        return ReturnCode::Ok;
    }

    ReturnCode fillGeometry(drawing::api::IPathGeometry* pathGeometry, drawing::api::IBrush* brush, drawing::api::IBrush* opacityBrush) override
    {
        auto* path = dynamic_cast<cpugfx::PathGeometry*>(pathGeometry);
        if (!path || !dynamic_cast<cpugfx::CpuBrush*>(brush))
            return ReturnCode::NoSupport;

        auto* solid = dynamic_cast<cpugfx::SolidColorBrush*>(brush);
        if (solid && !opacityBrush && !geometryClipActive())
            return fillFiguresGpu(path->figures, path->fillMode, *solid);

        return viaCpu(figuresDeviceBounds(path->figures), [&](cpugfx::RenderTarget& cpu) {
            return cpu.fillGeometry(pathGeometry, brush, opacityBrush);
        });
    }

    ReturnCode drawGeometry(drawing::api::IPathGeometry* pathGeometry, drawing::api::IBrush* brush, float strokeWidth, drawing::api::IStrokeStyle* strokeStyle) override
    {
        auto* path = dynamic_cast<cpugfx::PathGeometry*>(pathGeometry);
        if (!path || !dynamic_cast<cpugfx::CpuBrush*>(brush))
            return ReturnCode::NoSupport;
        if (!(strokeWidth > 0.0f) || !std::isfinite(strokeWidth))
            return ReturnCode::Ok; // D2D draws nothing for a zero-width stroke

        // Widen in local space (so a non-uniform transform yields an elliptical
        // pen, like D2D), then fill the union of the pieces.
        std::vector<cpugfx::Figure> widened;
        cpugfx::stroke::widen(path->figures, strokeWidth, cpugfx::strokeParams(strokeStyle), widened);

        auto* solid = dynamic_cast<cpugfx::SolidColorBrush*>(brush);
        if (solid && !geometryClipActive())
            return fillFiguresGpu(widened, drawing::FillMode::Winding, *solid);

        return viaCpu(figuresDeviceBounds(widened), [&](cpugfx::RenderTarget& cpu) {
            return cpu.drawGeometry(pathGeometry, brush, strokeWidth, strokeStyle);
        });
    }

    // Same construction as the CPU backend: four arcs the sink flattens.
    ReturnCode fillRoundedRectangle(const drawing::RoundedRect* rr, drawing::api::IBrush* brush) override
    {
        auto geometry = roundedRectGeometry(rr);
        return fillGeometry(drawing::AccessPtr::get(geometry), brush, nullptr);
    }

    ReturnCode drawRoundedRectangle(const drawing::RoundedRect* rr, drawing::api::IBrush* brush, float strokeWidth, drawing::api::IStrokeStyle* strokeStyle) override
    {
        auto geometry = roundedRectGeometry(rr);
        return drawGeometry(drawing::AccessPtr::get(geometry), brush, strokeWidth, strokeStyle);
    }

    ReturnCode drawBitmap(drawing::api::IBitmap* sourceBitmap, const drawing::Rect* destinationRectangle, float opacity, drawing::BitmapInterpolationMode interpolationMode, const drawing::Rect* sourceRectangle) override
    {
        if (!destinationRectangle)
            return ReturnCode::NoSupport;
        auto source = asCpuBitmap(sourceBitmap);
        if (!source)
            return ReturnCode::NoSupport;

        return viaCpu(drawing::transformRect(transform_, *destinationRectangle), [&](cpugfx::RenderTarget& cpu) {
            return cpu.drawBitmap(source.get(), destinationRectangle, opacity, interpolationMode, sourceRectangle);
        });
    }

    // Text: the engine (helpers/CpuTextEngine.h, HarfBuzz) renders through a
    // cpugfx context, where it takes its D2D-matching glyph-mask path (the
    // plain-geometry path it would take on this context draws measurably
    // lighter glyphs). The result is composited like any other fallback.
    // Milestone 6 moves the glyph masks to a GPU atlas. Text may extend past
    // its layout rect, so the fallback covers the whole clip.
    cpugfx::ICpuTextEngine* textEngine() const;

    ReturnCode drawTextU(const char* string, uint32_t stringLength, drawing::api::ITextFormat* textFormat,
                         const drawing::Rect* layoutRect, drawing::api::IBrush* brush, int32_t options) override
    {
        auto* engine = textEngine();
        if (!engine || !textFormat || !layoutRect)
            return ReturnCode::NoSupport;
        return viaCpu(clipRectStack.back(), [&](cpugfx::RenderTarget& cpu) {
            return engine->drawText(&cpu, textFormat, string, int32_t(stringLength), *layoutRect, brush, options);
        });
    }

    ReturnCode drawRichTextU(drawing::api::IRichTextFormat* richTextFormat, const drawing::Rect* layoutRect,
                             drawing::api::IBrush* brush, int32_t options) override
    {
        auto* engine = textEngine();
        if (!engine || !richTextFormat || !layoutRect)
            return ReturnCode::NoSupport;
        return viaCpu(clipRectStack.back(), [&](cpugfx::RenderTarget& cpu) {
            return engine->drawRichText(&cpu, richTextFormat, *layoutRect, brush, options);
        });
    }

    ReturnCode drawTextLayout(drawing::Point point, drawing::api::ITextLayout* textLayout,
                              drawing::api::IBrush* brush, int32_t options) override
    {
        auto* engine = textEngine();
        if (!engine || !textLayout)
            return ReturnCode::NoSupport;
        return viaCpu(clipRectStack.back(), [&](cpugfx::RenderTarget& cpu) {
            return engine->drawTextLayout(&cpu, textLayout, point, brush, options);
        });
    }

    // Offscreen targets are cpugfx targets: a CpuReadable one by decision
    // (CachedBlur locks it synchronously; GPU read-back is asynchronous), and
    // the rest until milestone 3 gives them a fork layer.
    ReturnCode createCompatibleRenderTarget(drawing::Size desiredSize, int32_t flags, drawing::api::IBitmapRenderTarget** returnBitmapRenderTarget) override;

    // --- IBitmapRenderTarget / IResource ------------------------------------

    ReturnCode getBitmap(drawing::api::IBitmap** returnBitmap) override
    {
        *returnBitmap = bitmap;
        bitmap->addRef();
        return ReturnCode::Ok;
    }

    ReturnCode getFactory(drawing::api::IFactory** returnFactory) override
    {
        *returnFactory = factory;
        return ReturnCode::Ok;
    }

    ReturnCode queryInterface(const gmpi::api::Guid* iid, void** returnInterface) override
    {
        *returnInterface = {};
        GMPI_QUERYINTERFACE(drawing::api::IBitmapRenderTarget);
        GMPI_QUERYINTERFACE(drawing::api::IDeviceContext);
        GMPI_QUERYINTERFACE(drawing::api::IResource);
        return ReturnCode::NoSupport;
    }
    GMPI_REFCOUNT;

private:
    drawing::PathGeometry roundedRectGeometry(const drawing::RoundedRect* rr)
    {
        drawing::Factory factoryWrapper;
        this->getFactory(drawing::AccessPtr::put(factoryWrapper));

        const auto& r = rr->rect;
        const float rx = (std::min)(rr->radiusX, (r.right - r.left) * 0.5f);
        const float ry = (std::min)(rr->radiusY, (r.bottom - r.top) * 0.5f);
        const drawing::Size radius{ rx, ry };

        auto geometry = factoryWrapper.createPathGeometry();
        auto sink = geometry.open();
        sink.beginFigure({ r.left + rx, r.top }, drawing::FigureBegin::Filled);
        sink.addLine({ r.right - rx, r.top });
        sink.addArc({ { r.right, r.top + ry }, radius });
        sink.addLine({ r.right, r.bottom - ry });
        sink.addArc({ { r.right - rx, r.bottom }, radius });
        sink.addLine({ r.left + rx, r.bottom });
        sink.addArc({ { r.left, r.bottom - ry }, radius });
        sink.addLine({ r.left, r.top + ry });
        sink.addArc({ { r.left + rx, r.top }, radius });
        sink.endFigure(drawing::FigureEnd::Closed);
        sink.close();
        return geometry;
    }
};

// ---------------------------------------------------------------------------
// Factory. Geometry, stroke styles, images, text formats and flagged targets
// are cpugfx's (handed this factory, so getFactory() leads back here).
// ---------------------------------------------------------------------------
class Factory final : public drawing::api::IFactory
{
public:
    // Images are CPU bitmaps until drawn: decoding, createImage and
    // lockPixels on loaded images all go through cpugfx. Wire its decoder and
    // text engine exactly as for the CPU backend:
    //
    //     factory.cpu.imageDecoder = gmpi::drawing::decodeImageFile;
    //     factory.cpu.textEngine   = &textEngine;
    cpugfx::Factory cpu;

    ReturnCode createPathGeometry(drawing::api::IPathGeometry** returnPathGeometry) override
    {
        *returnPathGeometry = new cpugfx::PathGeometry(this);
        return ReturnCode::Ok;
    }

    ReturnCode createStrokeStyle(const drawing::StrokeStyleProperties* strokeStyleProperties, const float* dashes, int32_t dashesCount, drawing::api::IStrokeStyle** returnStrokeStyle) override
    {
        *returnStrokeStyle = new se::generic_graphics::StrokeStyle(this, strokeStyleProperties, dashes, dashesCount);
        return ReturnCode::Ok;
    }

    ReturnCode createCpuRenderTarget(drawing::SizeU size, int32_t flags, drawing::api::IBitmapRenderTarget** returnBitmapRenderTarget, float dpi) override
    {
        *returnBitmapRenderTarget = {};

        // Mask / SRGBPixels targets are 8-bit faces: cpugfx territory. The
        // GPU renders the unflagged fp16 case (CpuReadable is implied there:
        // lockPixels reads back and writes back).
        if (flags & ~int32_t(drawing::BitmapRenderTargetFlags::CpuReadable))
            return cpu.createCpuRenderTarget(size, flags, returnBitmapRenderTarget, dpi);

        try
        {
            *returnBitmapRenderTarget = new RenderTarget(this, size, flags, dpi);
        }
        catch (...)
        {
            return ReturnCode::Fail; // allocation failure must not cross the ABI
        }
        return ReturnCode::Ok;
    }

    ReturnCode getPlatformPixelFormat(int32_t* returnPixelFormat) override
    {
        *returnPixelFormat = drawing::api::IBitmapPixels::RGBA_16f;
        return ReturnCode::Ok;
    }

    ReturnCode createTextFormat(const char* fontFamilyName, drawing::FontWeight fontWeight, drawing::FontStyle fontStyle,
                                drawing::FontStretch fontStretch, float fontHeight, int32_t fontFlags,
                                drawing::api::ITextFormat** returnTextFormat) override
    {
        return cpu.createTextFormat(fontFamilyName, fontWeight, fontStyle, fontStretch, fontHeight, fontFlags, returnTextFormat);
    }

    ReturnCode createImage(int32_t width, int32_t height, int32_t flags, drawing::api::IBitmap** returnBitmap) override
    {
        return cpu.createImage(width, height, flags, returnBitmap);
    }

    ReturnCode loadImageU(const char* uri, drawing::api::IBitmap** returnBitmap) override
    {
        return cpu.loadImageU(uri, returnBitmap);
    }

    ReturnCode getFontFamilyName(int32_t fontIndex, gmpi::api::IString* returnName) override
    {
        return cpu.getFontFamilyName(fontIndex, returnName);
    }

    ReturnCode createRichTextFormat(const char* markdownText, float fontHeight, const char* fontFamilyName,
                                    int32_t fontFlags, drawing::TextAlignment textAlignment,
                                    drawing::ParagraphAlignment paragraphAlignment,
                                    drawing::WordWrapping wordWrapping, float lineSpacing, float baseline,
                                    drawing::api::IRichTextFormat** returnRichTextFormat) override
    {
        return cpu.createRichTextFormat(markdownText, fontHeight, fontFamilyName, fontFlags, textAlignment,
                                        paragraphAlignment, wordWrapping, lineSpacing, baseline, returnRichTextFormat);
    }

    ReturnCode createTextLayout(const char* utf8String, int32_t stringLength, drawing::api::ITextFormat* baseFormat,
                                float maxWidth, float maxHeight, const drawing::TextStyleRun* runs, int32_t runCount,
                                const char* const* runFamilies, int32_t runFamilyCount,
                                drawing::api::ITextLayout** returnTextLayout) override
    {
        return cpu.createTextLayout(utf8String, stringLength, baseFormat, maxWidth, maxHeight, runs, runCount,
                                    runFamilies, runFamilyCount, returnTextLayout);
    }

    GMPI_QUERYINTERFACE_METHOD(drawing::api::IFactory);
    GMPI_REFCOUNT_NO_DELETE;
};

inline cpugfx::ICpuTextEngine* RenderTarget::textEngine() const
{
    auto* f = dynamic_cast<Factory*>(factory);
    return f ? f->cpu.textEngine : nullptr;
}

inline ReturnCode RenderTarget::createCompatibleRenderTarget(drawing::Size desiredSize, int32_t flags, drawing::api::IBitmapRenderTarget** returnBitmapRenderTarget)
{
    *returnBitmapRenderTarget = {};
    auto* f = dynamic_cast<Factory*>(factory);
    if (!f)
        return ReturnCode::NoSupport;
    const drawing::SizeU size{ uint32_t(std::ceil(desiredSize.width)), uint32_t(std::ceil(desiredSize.height)) };
    return f->cpu.createCpuRenderTarget(size, flags, returnBitmapRenderTarget, 96.0f);
}

} // namespace gmpi::gpugfx
