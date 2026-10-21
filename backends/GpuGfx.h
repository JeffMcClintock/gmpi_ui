#pragma once

// GPU backend over the gmpi-ui graphics fork (origin: VitalAudio/visage, bgfx
// underneath: D3D11 / Metal / Vulkan). See VISAGE_BACKEND_PLAN.md.
//
// Milestone 1 walking skeleton: windowless render targets, solid brushes,
// clear, fillGeometry through the fork's analytic path-coverage atlas, and
// rectangles/ellipses via the Gfx_base geometry routing. Everything the
// skeleton does not do returns NoSupport rather than rendering wrong.
//
// Geometry, stroke styles and brushes are backend-neutral data, so they are
// the cpugfx classes: a PathGeometry is flattened figures whichever backend
// fills them, and the plan reuses cpugfx for CpuReadable targets and gradient
// strips anyway. Only the render target is new.
//
// Every surface is RGBA16F (the fork's default); lockPixels hands back
// premultiplied linear fp16, like the CPU backend and a D2D 64bppPRGBAHalf
// target.

#include "CpuGfx.h"

#include "visage_graphics/canvas.h"
#include "visage_graphics/path.h"
#include "visage_graphics/screenshot.h"

#include <vector>

namespace gmpi::gpugfx
{

class RenderTarget;

// ---------------------------------------------------------------------------
// The pixels of a GPU render target. Read-only: the data is a read-back copy
// taken when locked, so writes would go nowhere.
// ---------------------------------------------------------------------------
class BitmapPixels final : public drawing::api::IBitmapPixels
{
    std::vector<uint16_t> pixels; // RGBA fp16, premultiplied linear, tightly packed
    int32_t width{};

public:
    BitmapPixels(std::vector<uint16_t>&& ppixels, int32_t pwidth) : pixels(std::move(ppixels)), width(pwidth) {}

    ReturnCode getAddress(uint8_t** returnAddress) override
    {
        *returnAddress = reinterpret_cast<uint8_t*>(pixels.data());
        return ReturnCode::Ok;
    }

    ReturnCode getBytesPerRow(int32_t* returnBytesPerRow) override
    {
        *returnBytesPerRow = width * 8;
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

// ---------------------------------------------------------------------------
// A render target's bitmap. Lives on the GPU; lockPixels reads it back.
// ---------------------------------------------------------------------------
class Bitmap final : public drawing::api::IBitmap
{
    RenderTarget* target; // not owned: the target owns this bitmap

public:
    explicit Bitmap(RenderTarget* ptarget) : target(ptarget) {}

    ReturnCode getSizeU(drawing::SizeU* returnSize) override;
    ReturnCode lockPixels(drawing::api::IBitmapPixels** returnPixels, int32_t flags) override;
    ReturnCode getFactory(drawing::api::IFactory** returnFactory) override;

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
// Render target: one windowless Canvas. Draw calls are recorded as shapes and
// rendered at endDraw(); the target keeps its shapes between frames, so each
// endDraw renders everything drawn since creation (see note in VISAGE_BACKEND_
// PLAN.md on persistent targets - this is the skeleton's answer, not the final
// one).
// ---------------------------------------------------------------------------
class RenderTarget final : public se::generic_graphics::GraphicsContextT<drawing::api::IBitmapRenderTarget>
{
    drawing::api::IFactory* factory{};
    Bitmap* bitmap{}; // owned (refcounted)
    visage::Canvas canvas;
    int32_t width{}, height{};

    // Effective local -> device transform (client transform then DPI scale),
    // and what setTransform() was handed, which getTransform() must return.
    drawing::Matrix3x2 transform_;
    drawing::Matrix3x2 clientTransform_;
    drawing::Matrix3x2 dpiTransform_;

    bool rendered{ false }; // endDraw() has run since the last draw call

    // The device-space clip, as the canvas's clamp bounds.
    void applyClip()
    {
        const auto& c = clipRectStack.back();
        const auto l = std::max(0.0f, c.left), t = std::max(0.0f, c.top);
        const auto r = std::min(float(width), c.right), b = std::min(float(height), c.bottom);
        canvas.setClampBounds(l, t, std::max(0.0f, r - l), std::max(0.0f, b - t));
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

    bool setBrush(drawing::api::IBrush* brush)
    {
        auto* solid = dynamic_cast<cpugfx::SolidColorBrush*>(brush);
        if (!solid || !solid->isPaintable())
            return false;
        canvas.setColor(toVisage(solid->color, solid->opacity));
        return true;
    }

public:
    RenderTarget(drawing::api::IFactory* pfactory, drawing::SizeU size, int32_t /*flags*/, float dpi)
        : factory(pfactory), width(int32_t(size.width)), height(int32_t(size.height))
    {
        const float dpiScale = dpi / 96.0f;
        dpiTransform_._11 = dpiTransform_._22 = dpiScale;
        transform_ = dpiTransform_;

        canvas.setWindowless(width, height);
        bitmap = new Bitmap(this);
    }

    ~RenderTarget() override
    {
        bitmap->release();
    }

    int32_t surfaceWidth() const { return width; }
    int32_t surfaceHeight() const { return height; }
    drawing::api::IFactory* getFactoryRaw() const { return factory; }

    // Read the composited target back as premultiplied linear fp16 RGBA.
    std::vector<uint16_t> readBack()
    {
        if (!rendered)
            canvas.submit();
        const visage::Screenshot& shot = canvas.takeScreenshot();
        std::vector<uint16_t> out(size_t(width) * size_t(height) * 4, 0);
        if (shot.halfFloat() && shot.width() == width && shot.height() == height)
            std::copy_n(shot.halfData(), out.size(), out.data());
        return out;
    }

    // --- brushes ------------------------------------------------------------

    ReturnCode createSolidColorBrush(const drawing::Color* color, const drawing::BrushProperties* brushProperties, drawing::api::ISolidColorBrush** returnSolidColorBrush) override
    {
        *returnSolidColorBrush = new cpugfx::SolidColorBrush(factory, color, brushProperties);
        return ReturnCode::Ok;
    }

    ReturnCode createBitmapBrush(drawing::api::IBitmap*, const drawing::BrushProperties*, drawing::api::IBitmapBrush** returnBitmapBrush) override
    {
        *returnBitmapBrush = {};
        return ReturnCode::NoSupport; // milestone 3
    }

    ReturnCode createGradientstopCollection(const drawing::Gradientstop*, uint32_t, drawing::ExtendMode, drawing::api::IGradientstopCollection** returnCollection) override
    {
        *returnCollection = {};
        return ReturnCode::NoSupport; // milestone 2
    }

    ReturnCode createLinearGradientBrush(const drawing::LinearGradientBrushProperties*, const drawing::BrushProperties*, drawing::api::IGradientstopCollection*, drawing::api::ILinearGradientBrush** returnBrush) override
    {
        *returnBrush = {};
        return ReturnCode::NoSupport; // milestone 2
    }

    ReturnCode createRadialGradientBrush(const drawing::RadialGradientBrushProperties*, const drawing::BrushProperties*, drawing::api::IGradientstopCollection*, drawing::api::IRadialGradientBrush** returnBrush) override
    {
        *returnBrush = {};
        return ReturnCode::NoSupport; // milestone 2
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
        return ReturnCode::Ok;
    }

    ReturnCode pushClipGeometry(drawing::api::IPathGeometry*) override
    {
        return ReturnCode::NoSupport; // milestone 2
    }

    ReturnCode popAxisAlignedClip() override
    {
        if (clipRectStack.size() > 1)
            clipRectStack.pop_back();
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
        canvas.submit();
        rendered = true;
        return ReturnCode::Ok;
    }

    // --- drawing ------------------------------------------------------------

    ReturnCode clear(const drawing::Color* clearColor) override
    {
        if (!finite(*clearColor))
            return ReturnCode::Ok;

        applyClip();
        canvas.setBlendMode(visage::BlendMode::Opaque);
        canvas.setColor(toVisage(*clearColor, 1.0f));
        canvas.fill(); // the whole clamp area
        canvas.setBlendMode(visage::BlendMode::Alpha);
        rendered = false;
        return ReturnCode::Ok;
    }

    ReturnCode fillGeometry(drawing::api::IPathGeometry* pathGeometry, drawing::api::IBrush* brush, drawing::api::IBrush* opacityBrush) override
    {
        auto* path = dynamic_cast<cpugfx::PathGeometry*>(pathGeometry);
        if (!path)
            return ReturnCode::NoSupport;
        if (opacityBrush)
            return ReturnCode::NoSupport; // masked fills: fail loudly rather than render wrong
        if (!setBrush(brush))
            return ReturnCode::NoSupport;

        // Figures are already flat; transform to device space on the CPU.
        visage::Path vpath;
        for (const auto& figure : path->figures)
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

        vpath.setFillRule(path->fillMode == drawing::FillMode::Winding ? visage::Path::FillRule::NonZero
                                                                       : visage::Path::FillRule::EvenOdd);
        applyClip();
        canvas.fill(vpath, 0, 0);
        rendered = false;
        return ReturnCode::Ok;
    }

    ReturnCode drawGeometry(drawing::api::IPathGeometry*, drawing::api::IBrush*, float, drawing::api::IStrokeStyle*) override
    {
        return ReturnCode::NoSupport; // milestone 2
    }

    ReturnCode drawLine(drawing::Point, drawing::Point, drawing::api::IBrush*, float, drawing::api::IStrokeStyle*) override
    {
        return ReturnCode::NoSupport; // milestone 2
    }

    ReturnCode drawRectangle(const drawing::Rect*, drawing::api::IBrush*, float, drawing::api::IStrokeStyle*) override
    {
        return ReturnCode::NoSupport; // milestone 2
    }

    ReturnCode drawRoundedRectangle(const drawing::RoundedRect*, drawing::api::IBrush*, float, drawing::api::IStrokeStyle*) override
    {
        return ReturnCode::NoSupport; // milestone 2
    }

    // Same construction as the CPU backend: four arcs the sink flattens.
    ReturnCode fillRoundedRectangle(const drawing::RoundedRect* rr, drawing::api::IBrush* brush) override
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

        return fillGeometry(drawing::AccessPtr::get(geometry), brush, nullptr);
    }

    ReturnCode drawEllipse(const drawing::Ellipse*, drawing::api::IBrush*, float, drawing::api::IStrokeStyle*) override
    {
        return ReturnCode::NoSupport; // milestone 2
    }

    ReturnCode drawBitmap(drawing::api::IBitmap*, const drawing::Rect*, float, drawing::BitmapInterpolationMode, const drawing::Rect*) override
    {
        return ReturnCode::NoSupport; // milestone 3
    }

    // Text: the engine (helpers/CpuTextEngine.h, HarfBuzz) lays out and emits
    // glyphs as path geometry through this context's own public methods. On a
    // non-cpugfx context it takes its plain-geometry path, so every glyph is
    // re-flattened per draw; milestone 6 caches glyph coverage on the GPU.
    cpugfx::ICpuTextEngine* textEngine() const;

    ReturnCode drawTextU(const char* string, uint32_t stringLength, drawing::api::ITextFormat* textFormat,
                         const drawing::Rect* layoutRect, drawing::api::IBrush* brush, int32_t options) override
    {
        auto* engine = textEngine();
        if (!engine || !textFormat || !layoutRect)
            return ReturnCode::NoSupport;
        return engine->drawText(this, textFormat, string, int32_t(stringLength), *layoutRect, brush, options);
    }

    ReturnCode drawRichTextU(drawing::api::IRichTextFormat* richTextFormat, const drawing::Rect* layoutRect,
                             drawing::api::IBrush* brush, int32_t options) override
    {
        auto* engine = textEngine();
        if (!engine || !richTextFormat || !layoutRect)
            return ReturnCode::NoSupport;
        return engine->drawRichText(this, richTextFormat, *layoutRect, brush, options);
    }

    ReturnCode drawTextLayout(drawing::Point point, drawing::api::ITextLayout* textLayout,
                              drawing::api::IBrush* brush, int32_t options) override
    {
        auto* engine = textEngine();
        if (!engine || !textLayout)
            return ReturnCode::NoSupport;
        return engine->drawTextLayout(this, textLayout, point, brush, options);
    }

    // Offscreen targets are cpugfx targets for now: a CpuReadable one is by
    // decision (CachedBlur locks it synchronously, GPU read-back is async);
    // a GPU-only one becomes a fork layer in milestone 3, together with the
    // drawBitmap that uploads a cpugfx bitmap as a texture.
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
};

inline ReturnCode Bitmap::getSizeU(drawing::SizeU* returnSize)
{
    *returnSize = { uint32_t(target->surfaceWidth()), uint32_t(target->surfaceHeight()) };
    return ReturnCode::Ok;
}

inline ReturnCode Bitmap::lockPixels(drawing::api::IBitmapPixels** returnPixels, int32_t flags)
{
    *returnPixels = {};
    if (flags != int32_t(drawing::BitmapLockFlags::Read))
        return ReturnCode::NoSupport; // GPU pixels: read-back only
    *returnPixels = new BitmapPixels(target->readBack(), target->surfaceWidth());
    return ReturnCode::Ok;
}

inline ReturnCode Bitmap::getFactory(drawing::api::IFactory** returnFactory)
{
    *returnFactory = target->getFactoryRaw();
    return ReturnCode::Ok;
}

// ---------------------------------------------------------------------------
// Factory. Geometry and stroke styles are the backend-neutral cpugfx classes
// (handed this factory, so getFactory() leads back here).
// ---------------------------------------------------------------------------
class Factory final : public drawing::api::IFactory
{
public:
    // Images are CPU bitmaps until drawn (decision 3 in the plan): decoding,
    // createImage and lockPixels on loaded images all go through cpugfx. Wire
    // its decoder exactly as for the CPU backend:
    //
    //     factory.cpu.imageDecoder = gmpi::drawing::decodeImageFile;
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

        // Mask / SRGBPixels / CpuReadable targets are 8-bit or locked for
        // writing: cpugfx territory (plan, decision 3). The GPU renders the
        // unflagged fp16 case.
        if (flags != 0)
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

    // Text formats, rich text and retained layouts come from the text engine,
    // injected on the embedded cpugfx factory:
    //
    //     factory.cpu.textEngine = &textEngine;
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
