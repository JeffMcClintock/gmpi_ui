#pragma once

// GPU backend over gmpi_ui's GPU renderer, backends/gpu/ (absorbed from
// VitalAudio/visage; bgfx underneath: D3D11 / Metal / Vulkan). See
// VISAGE_BACKEND_PLAN.md. Build: backends/gpu/gmpi_gpu.cmake.
//
// Every surface is RGBA16F premultiplied linear, the working format of the
// CPU backend and a D2D 64bppPRGBAHalf target.
//
// Two paths to pixels:
//
//   GPU   fills and strokes with solid, linear and radial gradient brushes
//         go through the renderer's exact path-coverage atlas: figures are
//         flattened already (Gfx_base sink), strokes are widened by the
//         cpugfx contour stroker (D2D-exact caps, joins and dashes) and
//         filled nonzero. Axis-aligned rects and circles are SDF quads.
//         Bitmaps are retained half-float atlas images.
//
//   CPU   anything the GPU path cannot express yet is rendered by a cpugfx
//         render target the size of the draw's device bounds, uploaded as an
//         RGBA16F image and composited premultiplied: bitmap and opacity
//         brushes, text, the drawBitmap cases the atlas quad cannot match,
//         and every draw under a geometry clip. Correct by construction
//         (cpugfx matches D2D); GPU-native versions replace these one at a
//         time with the test suite as the oracle.
//
// Geometry, stroke styles, brushes, images and flagged (Mask / SRGB /
// CpuReadable) targets are the cpugfx classes: a PathGeometry is flattened
// figures whichever backend fills them, and a brush is "a thing that
// evaluates spans" to the fallback. Only the fp16 render target is new.

#include "CpuGfx.h"

#include "gpu/canvas.h"
#include "gpu/path.h"
#include "gpu/renderer.h"
#include "gpu/screenshot.h"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

namespace gmpi::gpugfx
{

class Factory;

// A host's own choice (a preference setting), made before its first
// window presents. Unset = the backend stays opt-in.
enum class HostPreference { Unset, Gpu, Software };
inline HostPreference& hostPreference()
{
    static HostPreference preference = HostPreference::Unset;
    return preference;
}

// Whether to present through the GPU. GMPI_UI_GPU in the environment
// overrides everything (1 = GPU, 0 = software), then the host's
// preference; otherwise off while the backend is young. A host that cannot
// initialise the renderer keeps its CPU path regardless.
inline bool wanted()
{
    if (const char* v = std::getenv("GMPI_UI_GPU"); v && *v)
        return *v != '0';
    return hostPreference() == HostPreference::Gpu;
}

// Initialise the (process-wide) renderer against a native window and
// display. Returns false if the device cannot do RGBA16F, which the
// backend requires. Safe to call more than once.
inline bool initialize(void* nativeWindow, void* nativeDisplay, bool wayland)
{
    auto& renderer = gmpi::gpu::Renderer::instance();
    if (!renderer.initialized())
    {
        renderer.setVsync(false); // the host paces frames (Wayland frame callbacks / X11 timer)
        // nativeWindow is a MODEL window the host keeps alive and never shows
        // (a role-less wl_surface, a hidden X11 window): bgfx's Vulkan renderer
        // only reports swap-chain support when initialised on some window,
        // and initialising on the host's real surface would put a second
        // Vulkan surface on it when its canvas pairs to it. Every real window
        // then gets its own swap chain through createWindowRenderTarget().
        renderer.initialize(nativeWindow, nativeDisplay,
                            wayland ? gmpi::gpu::Renderer::NativeWindowType::Wayland : gmpi::gpu::Renderer::NativeWindowType::Default);
    }
    if (!renderer.supported())
        std::fprintf(stderr, "gmpi_ui GPU backend: %s\n", renderer.errorMessage().c_str());
    else if (!renderer.swapChainSupported())
        std::fprintf(stderr, "gmpi_ui GPU backend: renderer has no swap-chain support\n");
    return renderer.supported() && renderer.swapChainSupported() && renderer.halfFloatFrameBuffers();
}

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
    gmpi::gpu::Canvas canvas;
    int32_t width{}, height{};
    bool rendered{ false }; // canvas.submit() has run since the last draw call

    // Raw fp16 buffers the canvas is drawing from. The atlas reads them again
    // whenever it repacks, so they live until the shapes using them are gone.
    std::vector<std::unique_ptr<std::vector<uint16_t>>> uploads;

    // A window bitmap is the swap chain of a native window: not lockable, and
    // drawn frame by frame (the host invalidates, the client redraws).
    void* window{};

    Bitmap(drawing::api::IFactory* pfactory, int32_t w, int32_t h, void* windowHandle = nullptr)
        : factory(pfactory), width((std::max)(1, w)), height((std::max)(1, h)), window(windowHandle)
    {
        if (window)
            canvas.pairToWindow(window, width, height);
        else
        {
            canvas.setWindowless(width, height);
            // Render-target semantics: pixels persist, each shape composites once.
            canvas.setRetainPixels(true);
        }
    }

    void resize(int32_t w, int32_t h)
    {
        width = (std::max)(1, w);
        height = (std::max)(1, h);
        canvas.setDimensions(width, height); // recreates the swap chain at the new size
    }

    // cpugfx bitmaps drawn into this target, resident in the canvas's
    // half-float atlas: uploaded once per modification (cpugfx::Bitmap::
    // generation), not per draw. The copy carries a one-texel replicated
    // border so bilinear sampling at the edge of a stretched draw clamps to
    // the edge texel, as Direct2D does, instead of blending with the atlas
    // padding. Entries unused for kImageCacheFrames submits are dropped.
    struct CachedImage
    {
        gmpi::shared_ptr<cpugfx::Bitmap> source;
        uint64_t generation{};
        std::vector<uint16_t> pixels; // (w+2) x (h+2) fp16 RGBA, never resized: the atlas keeps the pointer
        uint64_t id{};
        std::unique_ptr<gmpi::gpu::ImageAtlas::PackedImage> retained;
        uint64_t lastUsed{};
    };
    static constexpr uint64_t kImageCacheFrames = 120;
    std::map<cpugfx::Bitmap*, CachedImage> imageCache;
    uint64_t submits{};

    // Text rasterised by the CPU engine, resident in the atlas the same way.
    // Without it every drawTextU re-measured, re-rasterised and re-uploaded
    // its string every frame, as a NEW atlas image: hundreds of labels filled
    // the atlas each frame and every refill repacked and re-uploaded the lot
    // (wayland_bench --scene 5 --load 1024: 4.6 fps, CPU backend 59).
    //
    // The key is everything the pixels depend on. Position enters only as the
    // sub-pixel phase of the layout origin, quantised to kTextPhases steps,
    // so text that moves by whole pixels - scrolling, dragging - still hits;
    // the price is drawing up to half a step (1/8 px) from where it was asked.
    struct TextKey
    {
        std::string text;
        drawing::api::ITextFormat* format{};
        uint64_t formatState{};      // ICpuTextEngine::textFormatStateKey
        // r g b a opacity, as bits. std::array, not a C array: inside
        // std::tie a C array compares by ADDRESS, which made every key unique.
        std::array<uint32_t, 5> color{};
        float layoutW{}, layoutH{};  // wrapping and alignment depend on the box size
        float scaleX{}, scaleY{};
        int32_t options{};
        uint8_t phaseX{}, phaseY{};

        auto tie() const { return std::tie(text, format, formatState, color, layoutW, layoutH, scaleX, scaleY, options, phaseX, phaseY); }
        bool operator<(const TextKey& other) const { return tie() < other.tie(); }
    };
    struct CachedText
    {
        // Held so the format cannot be freed and another allocated at the
        // same address while the key names it.
        gmpi::shared_ptr<drawing::api::ITextFormat> format;
        std::vector<uint16_t> pixels; // w x h fp16 RGBA, never resized: the atlas keeps the pointer
        int32_t w{}, h{};
        int32_t offsetX{}, offsetY{}; // raster origin relative to the snapped layout origin, device px
        uint64_t id{};
        std::unique_ptr<gmpi::gpu::ImageAtlas::PackedImage> retained;
        uint64_t lastUsed{};
    };
    static constexpr int kTextPhases = 4;
    static constexpr size_t kTextCacheBudgetBytes = size_t(64) << 20;
    static constexpr int64_t kMaxCachedTextPixels = int64_t(1) << 20; // a bigger block is drawn uncached
    std::map<TextKey, CachedText> textCache;
    size_t textCacheBytes{};

    // Draw a cached raster at device (x, y), 1:1, through `clamp`.
    void drawCachedText(const CachedText& entry, float x, float y, const gmpi::gpu::ClampBounds& clamp)
    {
        canvas.setClampBounds(clamp);
        canvas.setBlendMode(gmpi::gpu::BlendMode::Composite);
        canvas.setColor(gmpi::gpu::Color(1.0f, 1.0f, 1.0f, 1.0f));
        canvas.halfImage(reinterpret_cast<const unsigned char*>(entry.pixels.data()), entry.w, entry.h,
                         x, y, float(entry.w), float(entry.h), entry.id);
        canvas.setBlendMode(gmpi::gpu::BlendMode::Alpha);
        rendered = false;
    }

    void trimTextCache()
    {
        for (auto it = textCache.begin(); it != textCache.end();)
        {
            if (it->second.lastUsed + kImageCacheFrames < submits)
            {
                textCacheBytes -= it->second.pixels.size() * sizeof(uint16_t);
                it = textCache.erase(it);
            }
            else
                ++it;
        }
        // Over budget (text that changes every frame, a scrolling log): drop
        // the least recently used until under. Rare, so a scan is fine.
        while (textCacheBytes > kTextCacheBudgetBytes && !textCache.empty())
        {
            auto oldest = textCache.begin();
            for (auto it = textCache.begin(); it != textCache.end(); ++it)
                if (it->second.lastUsed < oldest->second.lastUsed)
                    oldest = it;
            if (oldest->second.lastUsed == submits)
                break; // everything left is in this frame
            textCacheBytes -= oldest->second.pixels.size() * sizeof(uint16_t);
            textCache.erase(oldest);
        }
    }

    const CachedImage& cachedImage(cpugfx::Bitmap& bm)
    {
        auto it = imageCache.find(&bm);
        if (it != imageCache.end() && it->second.generation == bm.generation)
        {
            it->second.lastUsed = submits;
            return it->second;
        }

        static uint64_t nextId = 1;
        CachedImage entry;
        bm.addRef();
        entry.source.attach(&bm);
        entry.generation = bm.generation;
        entry.id = nextId++;
        entry.lastUsed = submits;

        const auto& sf = bm.surface;
        const int w = sf.width, h = sf.height;
        const int pw = w + 2;
        entry.pixels.resize(size_t(pw) * size_t(h + 2) * 4);
        auto* dst = entry.pixels.data();
        for (int y = -1; y <= h; ++y)
        {
            const int sy = (std::min)((std::max)(y, 0), h - 1);
            const uint16_t* srcRow = sf.pixels + size_t(sy) * sf.stridePixels * 4;
            uint16_t* dstRow = dst + size_t(y + 1) * pw * 4;
            std::memcpy(dstRow + 4, srcRow, size_t(w) * 8);
            std::memcpy(dstRow, srcRow, 8);                              // left border
            std::memcpy(dstRow + size_t(w + 1) * 4, srcRow + size_t(w - 1) * 4, 8); // right border
        }
        entry.retained = std::make_unique<gmpi::gpu::ImageAtlas::PackedImage>(
            canvas.retainHalfImage(reinterpret_cast<const unsigned char*>(entry.pixels.data()), pw, h + 2, entry.id));

        auto& slot = imageCache[&bm];
        slot = std::move(entry); // replacing an outdated copy frees its atlas rect
        return slot;
    }

    // Draw the shapes recorded since the last submit, once, then forget them.
    // Their uploads are safe to free: the atlas has copied them to the GPU
    // and never re-reads an image whose shape is gone.
    void submit()
    {
        if (!window)
            canvas.invalidateContent(); // a window's host invalidates what it wants redrawn
        canvas.submit();
        canvas.clearShapesKeepPixels();
        uploads.clear();
        rendered = true;

        ++submits;
        for (auto it = imageCache.begin(); it != imageCache.end();)
        {
            if (it->second.lastUsed + kImageCacheFrames < submits)
                it = imageCache.erase(it);
            else
                ++it;
        }
        trimTextCache();
    }

    // Composited pixels as premultiplied linear fp16 RGBA, tightly packed.
    std::vector<uint16_t> readBack()
    {
        if (!rendered)
            submit();
        const gmpi::gpu::Screenshot& shot = canvas.takeScreenshot(false);
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
                       const gmpi::gpu::ClampBounds& clamp, gmpi::gpu::BlendMode blend)
    {
        if (w <= 0 || h <= 0 || pixels.empty())
            return;
        uploads.push_back(std::make_unique<std::vector<uint16_t>>(std::move(pixels)));
        const auto* data = reinterpret_cast<const unsigned char*>(uploads.back()->data());

        static uint64_t nextImageId = 1; // the atlas caches by identity; a freed buffer's address is not one
        canvas.setClampBounds(clamp);
        canvas.setBlendMode(blend);
        canvas.setColor(gmpi::gpu::Color(opacity, opacity, opacity, opacity)); // premultiplied scale
        canvas.halfImage(data, w, h, x, y, dw, dh, nextImageId++);
        canvas.setBlendMode(gmpi::gpu::BlendMode::Alpha);
        rendered = false;
    }

    // Replace the whole target with these pixels (a write lock's result).
    void writeBack(std::vector<uint16_t>&& pixels)
    {
        drawHalfImage(std::move(pixels), width, height, 0.0f, 0.0f, float(width), float(height), 1.0f,
                      gmpi::gpu::ClampBounds{ 0.0f, 0.0f, float(width), float(height) }, gmpi::gpu::BlendMode::Opaque);
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
    *returnPixels = {};
    if (window)
        return ReturnCode::NoSupport; // a swap chain: "Not supported when Bitmap was created on GPU"
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

    gmpi::gpu::Canvas& canvas() { return bitmap->canvas; }

    drawing::Rect surfaceBounds() const { return { 0.0f, 0.0f, float(bitmap->width), float(bitmap->height) }; }

    // The device-space clip as the canvas's clamp bounds.
    gmpi::gpu::ClampBounds currentClamp() const
    {
        const auto c = drawing::intersectRect(clipRectStack.back(), surfaceBounds());
        return { c.left, c.top, (std::max)(c.left, c.right), (std::max)(c.top, c.bottom) };
    }

    static bool finite(const drawing::Color& c)
    {
        return std::isfinite(c.r) && std::isfinite(c.g) && std::isfinite(c.b) && std::isfinite(c.a);
    }

    // GMPI colour is linear straight RGBA; the fork takes alpha first.
    static gmpi::gpu::Color toVisage(const drawing::Color& c, float opacity)
    {
        return gmpi::gpu::Color(c.a * opacity, c.r, c.g, c.b);
    }

    // Draw tracing for hosts (GMPI_UI_GPU_TRACE_DRAWS=1): op, device rect, how it was drawn.
    static bool traceDraws()
    {
        static const bool on = std::getenv("GMPI_UI_GPU_TRACE_DRAWS") != nullptr;
        return on;
    }
    void trace(const char* op, const drawing::Rect& device, const char* how) const
    {
        if (traceDraws())
            std::fprintf(stderr, "[gpugfx] %-14s %7.1f,%7.1f - %7.1f,%7.1f  %s  clip %.0f,%.0f-%.0f,%.0f\n", op,
                         device.left, device.top, device.right, device.bottom, how,
                         clipRectStack.back().left, clipRectStack.back().top, clipRectStack.back().right, clipRectStack.back().bottom);
    }

    // --- GPU path -----------------------------------------------------------

    // The transform maps axis-aligned rects to axis-aligned rects (no
    // rotation or skew); uniform() also means circles stay circles.
    bool axisAligned() const { return transform_._12 == 0.0f && transform_._21 == 0.0f; }
    bool uniformScale() const { return axisAligned() && transform_._11 == transform_._22 && transform_._11 > 0.0f; }

    // Solid brush, no opacity brush, no geometry clip: the GPU-native cases.
    cpugfx::SolidColorBrush* gpuSolid(drawing::api::IBrush* brush, drawing::api::IBrush* opacityBrush = nullptr)
    {
        if (opacityBrush || geometryClipActive())
            return nullptr;
        auto* solid = dynamic_cast<cpugfx::SolidColorBrush*>(brush);
        return (solid && solid->isPaintable()) ? solid : nullptr;
    }

    void beginGpuShape(const gmpi::gpu::Brush& brush)
    {
        auto& c = canvas();
        c.setClampBounds(currentClamp());
        c.setBrush(brush);
        bitmap->rendered = false;
    }

    void beginGpuShape(const cpugfx::SolidColorBrush& brush)
    {
        beginGpuShape(gmpi::gpu::Brush::solid(toVisage(brush.color, brush.opacity)));
    }

    // --- gradients ----------------------------------------------------------
    // The fork samples a 512-texel RGBA16F strip per gradient: a lookup table
    // over t in [0, 1], filled here from cpugfx's stop interpolation so both
    // backends paint the same colours by construction. Straight colour; the
    // fork premultiplies when it blends. Wrap and Mirror are the fork's own
    // repeat/reflect (the strip spans a full atlas row and the sampler wraps).

    static gmpi::gpu::Gradient gradientStrip(const cpugfx::GradientstopCollection& stops, float opacity)
    {
        auto strip = gmpi::gpu::Gradient::fromSampleFunction(gmpi::gpu::Gradient::kMaxGradientResolution, [&](float t) {
            const auto c = stops.colorAt(t);
            return gmpi::gpu::Color(c.a * opacity, c.r, c.g, c.b);
        });
        strip.setRepeat(stops.extendMode == drawing::ExtendMode::Wrap);
        strip.setReflect(stops.extendMode == drawing::ExtendMode::Mirror);
        return strip;
    }

    static gmpi::gpu::Brush solidStop(const cpugfx::GradientstopCollection& stops, float t, float opacity)
    {
        return gmpi::gpu::Brush::solid(toVisage(stops.colorAt(t), opacity));
    }

    static gmpi::gpu::Transform toVisageTransform(const drawing::Matrix3x2& m)
    {
        // GMPI is row-vector (x' = x*_11 + y*_21 + _31); the fork is column form.
        return gmpi::gpu::Transform(m._11, m._21, m._31, m._12, m._22, m._32);
    }

    std::optional<gmpi::gpu::Brush> linearBrush(const cpugfx::LinearGradientBrush& b) const
    {
        const auto* stops = b.stopCollection();
        const float opacity = b.brushOpacity();
        const auto start = b.start();
        const drawing::Point axis{ b.end().x - start.x, b.end().y - start.y };
        const float lenSq = axis.x * axis.x + axis.y * axis.y;
        if (!(lenSq > 0.0f))
            return solidStop(*stops, 1.0f, opacity); // degenerate axis: D2D paints the last stop

        // t is affine in device space: t = p . g + c. The fork's linear
        // gradient is t = (p - p1) . (p2 - p1) / |p2 - p1|^2, so p1 is the
        // point where t = 0 and p2 - p1 = g / |g|^2.
        const auto inv = drawing::invert(b.brushTransformMatrix() * transform_); // device -> brush space
        const float gx = (inv._11 * axis.x + inv._12 * axis.y) / lenSq;
        const float gy = (inv._21 * axis.x + inv._22 * axis.y) / lenSq;
        const float c = (inv._31 * axis.x + inv._32 * axis.y - (start.x * axis.x + start.y * axis.y)) / lenSq;
        const float g2 = gx * gx + gy * gy;
        if (!(g2 > 0.0f) || !std::isfinite(g2) || !std::isfinite(c))
            return solidStop(*stops, 1.0f, opacity);
        const gmpi::gpu::Point p1(-c * gx / g2, -c * gy / g2);
        const gmpi::gpu::Point p2(p1.x + gx / g2, p1.y + gy / g2);
        return gmpi::gpu::Brush::linear(gradientStrip(*stops, opacity), p1, p2);
    }

    std::optional<gmpi::gpu::Brush> radialBrush(const cpugfx::RadialGradientBrush& b) const
    {
        const auto* stops = b.stopCollection();
        const float opacity = b.brushOpacity();
        const float rx = b.radiusXValue(), ry = b.radiusYValue();
        if (!(rx != 0.0f && ry != 0.0f) || !std::isfinite(rx) || !std::isfinite(ry))
            return solidStop(*stops, 1.0f, opacity);

        // As cpugfx: the focus (gradient origin) is kept strictly inside the
        // ellipse, clamped by length in the ellipse's unit space.
        const auto center = b.centerPoint();
        const auto offset = b.gradientOriginOffset();
        float fx = offset.x / rx, fy = offset.y / ry;
        const float fLen = std::sqrt(fx * fx + fy * fy);
        if (!(fLen < 0.999f))
        {
            const float k = std::isfinite(fLen) && fLen > 0.0f ? 0.999f / fLen : 0.0f;
            fx *= k;
            fy *= k;
        }
        const gmpi::gpu::Point c(center.x, center.y);
        const gmpi::gpu::Point focal(center.x + fx * std::fabs(rx), center.y + fy * std::fabs(ry));
        auto position = gmpi::gpu::GradientPosition::radial(c, std::fabs(rx), std::fabs(ry), focal);
        position = position.transformed(toVisageTransform(b.brushTransformMatrix() * transform_));
        return gmpi::gpu::Brush(gradientStrip(*stops, opacity), position);
    }

    // The brush as a fork brush, when the fork can paint it directly: solid,
    // linear or radial gradient; no opacity brush, no geometry clip (bitmap
    // brushes stay with cpugfx).
    std::optional<gmpi::gpu::Brush> gpuBrush(drawing::api::IBrush* brush, drawing::api::IBrush* opacityBrush = nullptr) const
    {
        if (opacityBrush || geometryClipActive())
            return std::nullopt;
        if (auto* solid = dynamic_cast<cpugfx::SolidColorBrush*>(brush))
        {
            if (!solid->isPaintable())
                return std::nullopt;
            return gmpi::gpu::Brush::solid(toVisage(solid->color, solid->opacity));
        }
        if (auto* linear = dynamic_cast<cpugfx::LinearGradientBrush*>(brush))
            return linear->isPaintable() ? linearBrush(*linear) : std::nullopt;
        if (auto* radial = dynamic_cast<cpugfx::RadialGradientBrush*>(brush))
            return radial->isPaintable() ? radialBrush(*radial) : std::nullopt;
        return std::nullopt;
    }

    // The path atlas stores a path's coverage at full pixel size, so a large
    // path costs its whole bounding box in the R16F atlas and a window's worth
    // of panels overflows it (seen: 32768x32768 requested, the editor's first
    // frame). Above this, the CPU rasteriser is the cheaper path anyway.
    static constexpr float kMaxAtlasPathTexels = 512.0f * 512.0f;

    // Four corners (a closing repeat of the first allowed) with every edge
    // horizontal or vertical.
    static bool isAxisAlignedRect(const cpugfx::Figure& figure)
    {
        const auto& p = figure.points;
        size_t n = p.size();
        if (n == 5 && p[4].x == p[0].x && p[4].y == p[0].y)
            n = 4;
        if (n != 4 || !figure.filled)
            return false;
        for (size_t i = 0; i < 4; ++i)
        {
            const auto& a = p[i];
            const auto& b = p[(i + 1) % 4];
            if ((a.x == b.x) == (a.y == b.y))
                return false; // diagonal, or a zero-length edge
        }
        return true;
    }

    static bool tooLargeForAtlas(const drawing::Rect& deviceBounds)
    {
        const float w = deviceBounds.right - deviceBounds.left;
        const float h = deviceBounds.bottom - deviceBounds.top;
        return w * h > kMaxAtlasPathTexels || w > 4096.0f || h > 4096.0f;
    }

    // Fill flattened figures through the path atlas. Points are transformed
    // to device space here, on the CPU. `cpuFill` draws the same thing through
    // cpugfx, for figures too large for the atlas.
    ReturnCode fillFiguresGpu(const std::vector<cpugfx::Figure>& figures, drawing::FillMode fillMode, const gmpi::gpu::Brush& brush,
                              const std::function<ReturnCode(cpugfx::RenderTarget&)>& cpuFill)
    {
        const auto deviceBounds = figuresDeviceBounds(figures);
        if (tooLargeForAtlas(deviceBounds))
        {
            // The editor fills its whole virtual canvas (8000x8000) as one
            // rectangle geometry every frame: an SDF rect, not a raster of
            // the clip's worth of it uploaded each time.
            if (axisAligned() && figures.size() == 1 && isAxisAlignedRect(figures[0]))
            {
                trace("figures", deviceBounds, "gpu sdf rect (rectangle figure)");
                beginGpuShape(brush);
                canvas().rectangle(deviceBounds.left, deviceBounds.top, deviceBounds.right - deviceBounds.left,
                                   deviceBounds.bottom - deviceBounds.top);
                return ReturnCode::Ok;
            }
            if (traceDraws())
            {
                size_t points = 0;
                for (const auto& f : figures)
                    points += f.points.size();
                char how[96];
                std::snprintf(how, sizeof(how), "cpu (too large for atlas; %zu figures, %zu points, first %s)",
                              figures.size(), points, figures.empty() ? "-" : figures[0].filled ? "filled" : "hollow");
                trace("figures", deviceBounds, how);
            }
            return viaCpu(deviceBounds, cpuFill, "figures too large for the atlas");
        }
        trace("figures", deviceBounds, "gpu path atlas");

        gmpi::gpu::Path vpath;
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

        vpath.setFillRule(fillMode == drawing::FillMode::Winding ? gmpi::gpu::Path::FillRule::NonZero
                                                                 : gmpi::gpu::Path::FillRule::EvenOdd);
        beginGpuShape(brush);
        canvas().fill(vpath, 0, 0);
        return ReturnCode::Ok;
    }

    // --- CPU fallback -------------------------------------------------------

    // Render `draw` through a cpugfx target covering `deviceBounds` (clipped
    // to the current clip and the surface), with this context's transform and
    // geometry clips replayed, then composite the result premultiplied.
    ReturnCode viaCpu(drawing::Rect deviceBounds, const std::function<ReturnCode(cpugfx::RenderTarget&)>& draw,
                      const char* what = "cpu fallback")
    {
        deviceBounds = drawing::intersectRect(deviceBounds, drawing::intersectRect(clipRectStack.back(), surfaceBounds()));
        if (!(deviceBounds.right > deviceBounds.left) || !(deviceBounds.bottom > deviceBounds.top))
            return ReturnCode::Ok;
        trace("viaCpu", deviceBounds, what);

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
                              1.0f, currentClamp(), gmpi::gpu::BlendMode::Composite);
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
    RenderTarget(drawing::api::IFactory* pfactory, drawing::SizeU size, int32_t /*flags*/, float dpi, void* windowHandle = nullptr)
        : factory(pfactory)
    {
        const float dpiScale = dpi / 96.0f;
        dpiTransform_._11 = dpiTransform_._22 = dpiScale;
        transform_ = dpiTransform_;
        bitmap = new Bitmap(factory, int32_t(size.width), int32_t(size.height), windowHandle);
    }

    // --- window targets (hosts only) ----------------------------------------

    bool isWindow() const { return bitmap->window != nullptr; }

    void resize(int32_t width, int32_t height) { bitmap->resize(width, height); }

    // Mark a device-space rect for redraw: it is cleared and the shapes the
    // client records this frame are drawn into it. The rest keeps its pixels.
    void invalidate(const drawing::RectL& r)
    {
        canvas().invalidateContentRect(r.left, r.top, r.right - r.left, r.bottom - r.top);
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

        trace("clear", clipRectStack.back(), "gpu opaque fill of clip");
        auto& c = canvas();
        const auto clamp = currentClamp();
        c.setClampBounds(clamp);
        c.setBlendMode(gmpi::gpu::BlendMode::Opaque);
        c.setColor(toVisage(*clearColor, 1.0f));
        // Explicit rect: the fork's fill() with no arguments puts the quad at
        // the canvas origin with the clamp's SIZE, which misses a clip that
        // does not start at the origin (a panel at x = 1010 cleared nothing).
        c.fill(clamp.left, clamp.top, clamp.right - clamp.left, clamp.bottom - clamp.top);
        c.setBlendMode(gmpi::gpu::BlendMode::Alpha);
        bitmap->rendered = false;
        return ReturnCode::Ok;
    }

    ReturnCode fillGeometry(drawing::api::IPathGeometry* pathGeometry, drawing::api::IBrush* brush, drawing::api::IBrush* opacityBrush) override
    {
        auto* path = dynamic_cast<cpugfx::PathGeometry*>(pathGeometry);
        if (!path || !dynamic_cast<cpugfx::CpuBrush*>(brush))
            return ReturnCode::NoSupport;

        auto cpuFill = [&](cpugfx::RenderTarget& cpu) { return cpu.fillGeometry(pathGeometry, brush, opacityBrush); };
        if (auto b = gpuBrush(brush, opacityBrush))
            return fillFiguresGpu(path->figures, path->fillMode, *b, cpuFill);
        return viaCpu(figuresDeviceBounds(path->figures), cpuFill, geometryClipActive() ? "fillGeometry under a geometry clip" : "fillGeometry with a CPU-only brush");
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

        auto cpuStroke = [&](cpugfx::RenderTarget& cpu) { return cpu.drawGeometry(pathGeometry, brush, strokeWidth, strokeStyle); };
        if (auto b = gpuBrush(brush))
            return fillFiguresGpu(widened, drawing::FillMode::Winding, *b, cpuStroke);
        return viaCpu(figuresDeviceBounds(widened), cpuStroke, geometryClipActive() ? "drawGeometry under a geometry clip" : "drawGeometry with a CPU-only brush");
    }

    // Axis-aligned rectangles and circles are the fork's analytic SDF
    // primitives: one quad each, nothing in the path atlas. Everything else
    // goes through path geometry.
    ReturnCode fillRectangle(const drawing::Rect* rect, drawing::api::IBrush* brush) override
    {
        if (auto b = gpuBrush(brush); b && axisAligned())
        {
            const auto r = drawing::transformRect(transform_, *rect);
            if (!(r.right > r.left) || !(r.bottom > r.top))
                return ReturnCode::Ok;
            trace("fillRectangle", r, "gpu sdf rect");
            beginGpuShape(*b);
            canvas().rectangle(r.left, r.top, r.right - r.left, r.bottom - r.top);
            return ReturnCode::Ok;
        }
        return se::generic_graphics::GraphicsContextT<drawing::api::IBitmapRenderTarget>::fillRectangle(rect, brush);
    }

    // Same shortcut as cpugfx: an axis-aligned solid line with the default
    // stroke style (solid, flat caps) is exactly an axis-aligned rectangle.
    // The editor's canvas grid is ~100 of these in a frame; as strokes they
    // were ~100 path-atlas entries, as rects they are one SDF quad each.
    ReturnCode drawLine(drawing::Point point0, drawing::Point point1, drawing::api::IBrush* brush,
                        float strokeWidth, drawing::api::IStrokeStyle* strokeStyle) override
    {
        if (!strokeStyle && strokeWidth > 0.0f && std::isfinite(strokeWidth)
            && (point0.x == point1.x) != (point0.y == point1.y))
        {
            const float h = strokeWidth * 0.5f;
            const bool horizontal = point0.y == point1.y;
            const drawing::Rect r{
                (std::min)(point0.x, point1.x) - (horizontal ? 0.0f : h),
                (std::min)(point0.y, point1.y) - (horizontal ? h : 0.0f),
                (std::max)(point0.x, point1.x) + (horizontal ? 0.0f : h),
                (std::max)(point0.y, point1.y) + (horizontal ? h : 0.0f) };
            return fillRectangle(&r, brush);
        }
        return se::generic_graphics::GraphicsContextT<drawing::api::IBitmapRenderTarget>::drawLine(point0, point1, brush, strokeWidth, strokeStyle);
    }

    // A solid axis-aligned stroked rectangle with the default stroke style
    // is four axis-aligned rectangles. As a geometry it widened to an
    // annulus whose bounding box is the whole rectangle: the editor's
    // border around its 8000x8000 canvas fell back to a CPU raster and
    // upload of every module's worth of clip, every frame, for ink that
    // was nowhere near.
    ReturnCode drawRectangle(const drawing::Rect* rect, drawing::api::IBrush* brush, float strokeWidth,
                             drawing::api::IStrokeStyle* strokeStyle) override
    {
        if (auto b = gpuBrush(brush); b && axisAligned() && !strokeStyle && strokeWidth > 0.0f
            && std::isfinite(strokeWidth) && rect->right > rect->left && rect->bottom > rect->top)
        {
            const float h = strokeWidth * 0.5f;
            const drawing::Rect top{ rect->left - h, rect->top - h, rect->right + h, rect->top + h };
            const drawing::Rect bottom{ rect->left - h, rect->bottom - h, rect->right + h, rect->bottom + h };
            const drawing::Rect left{ rect->left - h, rect->top + h, rect->left + h, rect->bottom - h };
            const drawing::Rect right{ rect->right - h, rect->top + h, rect->right + h, rect->bottom - h };
            if (top.bottom >= bottom.top || left.right >= right.left)
            {
                // Pen wider than the rectangle: the bands cover it entirely.
                const drawing::Rect outer{ rect->left - h, rect->top - h, rect->right + h, rect->bottom + h };
                return fillRectangle(&outer, brush);
            }
            fillRectangle(&top, brush);
            fillRectangle(&bottom, brush);
            fillRectangle(&left, brush);
            fillRectangle(&right, brush);
            return ReturnCode::Ok;
        }
        return se::generic_graphics::GraphicsContextT<drawing::api::IBitmapRenderTarget>::drawRectangle(rect, brush, strokeWidth, strokeStyle);
    }

    ReturnCode fillEllipse(const drawing::Ellipse* ellipse, drawing::api::IBrush* brush) override
    {
        if (auto b = gpuBrush(brush); b && uniformScale() && ellipse->radiusX == ellipse->radiusY && ellipse->radiusX > 0.0f)
        {
            const auto c = drawing::transformPoint(transform_, ellipse->point);
            const float r = ellipse->radiusX * transform_._11;
            beginGpuShape(*b);
            canvas().circle(c.x - r, c.y - r, 2.0f * r);
            return ReturnCode::Ok;
        }
        return se::generic_graphics::GraphicsContextT<drawing::api::IBitmapRenderTarget>::fillEllipse(ellipse, brush);
    }

    // Same construction as the CPU backend: four arcs the sink flattens.
    // (Not the fork's SDF rounded rectangle: its corners differ from D2D's
    // flattened arcs by ~20 code values - fillRoundedRectangle test.)
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

        if (drawBitmapGpu(*source, *destinationRectangle, opacity, interpolationMode, sourceRectangle))
            return ReturnCode::Ok;

        return viaCpu(drawing::transformRect(transform_, *destinationRectangle), [&](cpugfx::RenderTarget& cpu) {
            return cpu.drawBitmap(source.get(), destinationRectangle, opacity, interpolationMode, sourceRectangle);
        }, "drawBitmap");
    }

    // Bitmaps too big for the half-float atlas go through cpugfx.
    static constexpr int kMaxGpuImageSide = 2048;

    // Draw a cpugfx bitmap as a retained atlas image (see Bitmap::cachedImage)
    // when the fork's bilinear quad gives Direct2D's result: an axis-aligned
    // transform, and either a 1:1 integer-aligned draw (where nearest and
    // linear sampling agree) or a linear-interpolated stretch of the whole
    // bitmap (a cropped stretch would sample the neighbouring texels where
    // Direct2D clamps to the crop).
    bool drawBitmapGpu(cpugfx::Bitmap& bm, const drawing::Rect& dst, float opacity,
                       drawing::BitmapInterpolationMode interpolationMode, const drawing::Rect* sourceRectangle)
    {
        const auto d = drawing::transformRect(transform_, dst);
        auto no = [&](const char* why) {
            trace("drawBitmap", d, why);
            return false;
        };
        if (!axisAligned() || geometryClipActive() || !std::isfinite(opacity))
            return no("cpu: transform or geometry clip");
        const int w = bm.surface.width, h = bm.surface.height;
        if (w <= 0 || h <= 0 || w > kMaxGpuImageSide || h > kMaxGpuImageSide)
            return no("cpu: bitmap too large for the atlas");

        const drawing::Rect full{ 0.0f, 0.0f, float(w), float(h) };
        const drawing::Rect src = sourceRectangle ? *sourceRectangle : full;
        if (!(src.right > src.left) || !(src.bottom > src.top) || src.left < 0.0f || src.top < 0.0f
            || src.right > float(w) || src.bottom > float(h))
            return no("cpu: inverted or out-of-bitmap source rect"); // mirroring etc.: cpugfx's rules
        if (!(dst.right > dst.left) || !(dst.bottom > dst.top) || !(d.right > d.left) || !(d.bottom > d.top))
            return no("cpu: inverted destination");

        const float sw = src.right - src.left, sh = src.bottom - src.top;
        const float dw = d.right - d.left, dh = d.bottom - d.top;

        auto integral = [](float v) { return std::fabs(v - std::round(v)) < 1e-4f; };
        const bool oneToOne = std::fabs(dw - sw) < 1e-4f && std::fabs(dh - sh) < 1e-4f
                              && integral(d.left) && integral(d.top) && integral(src.left) && integral(src.top);
        const bool whole = src.left == 0.0f && src.top == 0.0f && src.right == float(w) && src.bottom == float(h);
        if (!oneToOne && (interpolationMode != drawing::BitmapInterpolationMode::Linear || !whole))
            return no(interpolationMode == drawing::BitmapInterpolationMode::Linear ? "cpu: cropped stretch"
                                                                                     : "cpu: nearest-neighbour stretch");

        const auto& image = bitmap->cachedImage(bm);
        trace("drawBitmap", d, oneToOne ? "gpu atlas image 1:1" : "gpu atlas image stretched");
        auto& c = canvas();
        c.setClampBounds(currentClamp());
        c.setBlendMode(gmpi::gpu::BlendMode::Composite);
        c.setColor(gmpi::gpu::Color(opacity, opacity, opacity, opacity)); // premultiplied scale
        c.halfImage(reinterpret_cast<const unsigned char*>(image.pixels.data()), w + 2, h + 2,
                    d.left, d.top, dw, dh, image.id, 1.0f + src.left, 1.0f + src.top, sw, sh);
        c.setBlendMode(gmpi::gpu::BlendMode::Alpha);
        bitmap->rendered = false;
        return true;
    }

    // Text: the engine (helpers/CpuTextEngine.h, HarfBuzz) renders through a
    // cpugfx context, where it takes its D2D-matching glyph-mask path (the
    // plain-geometry path it would take on this context draws measurably
    // lighter glyphs). The result is composited like any other fallback.
    // Milestone 6 moves the glyph masks to a GPU atlas. Text may extend past
    // its layout rect, so the fallback covers the whole clip.
    cpugfx::ICpuTextEngine* textEngine() const;

    // Device bounds for text drawn within `layoutRect` with extent `extent`.
    // Text may overflow its layout rect on any side (alignment, no clip
    // option), so the rect grows by the overflow on every side, plus a
    // line's height for ink beyond the typographic box. The CPU fallback
    // used to cover the whole clip for every text draw: the analyser's 26
    // axis labels were each a 400x200 raster and upload per frame.
    drawing::Rect textDeviceBounds(const drawing::Rect& layoutRect, drawing::Size extent) const
    {
        if (!std::isfinite(extent.width) || !std::isfinite(extent.height))
            return clipRectStack.back();
        const float overflowW = (std::max)(0.0f, extent.width - (layoutRect.right - layoutRect.left));
        const float overflowH = (std::max)(0.0f, extent.height - (layoutRect.bottom - layoutRect.top));
        const float margin = extent.height + 1.0f;
        const drawing::Rect grown{ layoutRect.left - overflowW - margin, layoutRect.top - overflowH - margin,
                                   layoutRect.right + overflowW + margin, layoutRect.bottom + overflowH + margin };
        return drawing::transformRect(transform_, grown);
    }

    ReturnCode drawTextU(const char* string, uint32_t stringLength, drawing::api::ITextFormat* textFormat,
                         const drawing::Rect* layoutRect, drawing::api::IBrush* brush, int32_t options) override
    {
        auto* engine = textEngine();
        if (!engine || !textFormat || !layoutRect)
            return ReturnCode::NoSupport;

        if (drawTextCached(*engine, string, stringLength, textFormat, *layoutRect, brush, options))
            return ReturnCode::Ok;

        // Measured both unwrapped and at the layout width: whichever way the
        // format wraps, the larger of the two bounds the ink.
        drawing::Size unwrapped{ 0, 0 }, wrapped{ 0, 0 };
        textFormat->getTextExtentU(string, int32_t(stringLength), 100000.0f, &unwrapped);
        textFormat->getTextExtentU(string, int32_t(stringLength), (std::max)(1.0f, layoutRect->right - layoutRect->left), &wrapped);
        const drawing::Size extent{ (std::max)(unwrapped.width, wrapped.width), (std::max)(unwrapped.height, wrapped.height) };

        return viaCpu(textDeviceBounds(*layoutRect, extent), [&](cpugfx::RenderTarget& cpu) {
            return engine->drawText(&cpu, textFormat, string, int32_t(stringLength), *layoutRect, brush, options);
        }, "drawTextU");
    }

    // drawTextU through Bitmap::textCache. False = not cacheable here (a
    // geometry clip, a rotated or skewed transform, a non-solid brush, an engine
    // that cannot key its formats, a huge block); the caller draws uncached.
    bool drawTextCached(cpugfx::ICpuTextEngine& engine, const char* string, uint32_t stringLength,
                        drawing::api::ITextFormat* textFormat, const drawing::Rect& layoutRect,
                        drawing::api::IBrush* brush, int32_t options)
    {
        // GMPI_UI_GPU_TEXT_CACHE=0 turns it off, to compare against.
        static const bool enabled = [] { const char* v = std::getenv("GMPI_UI_GPU_TEXT_CACHE"); return !(v && *v == '0'); }();
        if (!enabled || geometryClipActive())
            return false;
        const auto& m = transform_;
        if (m._12 != 0.0f || m._21 != 0.0f || !(m._11 > 0.0f) || !(m._22 > 0.0f))
            return false;
        auto* solid = dynamic_cast<cpugfx::SolidColorBrush*>(brush);
        if (!solid || !solid->isPaintable())
            return false;
        const uint64_t formatState = engine.textFormatStateKey(textFormat);
        if (formatState == 0)
            return false;

        const auto origin = drawing::transformPoint(m, { layoutRect.left, layoutRect.top });
        if (!std::isfinite(origin.x) || !std::isfinite(origin.y))
            return false;
        float snapX = std::floor(origin.x), snapY = std::floor(origin.y);
        int phaseX = int(std::lround((origin.x - snapX) * Bitmap::kTextPhases));
        int phaseY = int(std::lround((origin.y - snapY) * Bitmap::kTextPhases));
        if (phaseX == Bitmap::kTextPhases) { phaseX = 0; snapX += 1.0f; }
        if (phaseY == Bitmap::kTextPhases) { phaseY = 0; snapY += 1.0f; }

        Bitmap::TextKey key;
        key.text.assign(string, stringLength);
        key.format = textFormat;
        key.formatState = formatState;
        const float colorParts[5] = { solid->color.r, solid->color.g, solid->color.b, solid->color.a, solid->opacity };
        std::memcpy(key.color.data(), colorParts, sizeof colorParts);
        key.layoutW = layoutRect.right - layoutRect.left;
        key.layoutH = layoutRect.bottom - layoutRect.top;
        key.scaleX = m._11;
        key.scaleY = m._22;
        key.options = options;
        key.phaseX = uint8_t(phaseX);
        key.phaseY = uint8_t(phaseY);

        auto& cache = bitmap->textCache;
        auto it = cache.find(key);
        if (it == cache.end())
        {
            drawing::Size unwrapped{ 0, 0 }, wrapped{ 0, 0 };
            textFormat->getTextExtentU(string, int32_t(stringLength), 100000.0f, &unwrapped);
            textFormat->getTextExtentU(string, int32_t(stringLength), (std::max)(1.0f, key.layoutW), &wrapped);
            const drawing::Size extent{ (std::max)(unwrapped.width, wrapped.width), (std::max)(unwrapped.height, wrapped.height) };
            if (!std::isfinite(extent.width) || !std::isfinite(extent.height))
                return false;

            // Rasterise as if the origin sat exactly on its quantised phase.
            const float shiftX = snapX + float(phaseX) / Bitmap::kTextPhases - origin.x;
            const float shiftY = snapY + float(phaseY) / Bitmap::kTextPhases - origin.y;
            auto bounds = textDeviceBounds(layoutRect, extent);
            bounds = { bounds.left + shiftX, bounds.top + shiftY, bounds.right + shiftX, bounds.bottom + shiftY };
            const int32_t left = int32_t(std::floor(bounds.left));
            const int32_t top = int32_t(std::floor(bounds.top));
            const int32_t w = int32_t(std::ceil(bounds.right)) - left;
            const int32_t h = int32_t(std::ceil(bounds.bottom)) - top;
            if (w <= 0 || h <= 0)
                return true; // nothing to draw
            if (int64_t(w) * h > Bitmap::kMaxCachedTextPixels)
                return false;

            gmpi::shared_ptr<cpugfx::RenderTarget> cpu;
            cpu.attach(new cpugfx::RenderTarget(factory, { uint32_t(w), uint32_t(h) }, 0, 96.0f));
            const auto t = m * drawing::makeTranslation(shiftX - float(left), shiftY - float(top));
            cpu->setTransform(&t);
            cpu->beginDraw();
            engine.drawText(cpu.get(), textFormat, string, int32_t(stringLength), layoutRect, brush, options);
            cpu->endDraw();

            gmpi::shared_ptr<drawing::api::IBitmap> cpuBitmap;
            cpu->getBitmap(cpuBitmap.put());
            auto* cb = dynamic_cast<cpugfx::Bitmap*>(cpuBitmap.get());
            if (!cb)
                return false;

            // Keep only the ink. textDeviceBounds pads by a line height all
            // round, so the raster is mostly empty: a 140 x 20 label came out
            // near 200 KB of fp16, and a few hundred filled the budget.
            const auto& sf = cb->surface;
            int32_t inkL = w, inkT = h, inkR = -1, inkB = -1;
            for (int32_t y = 0; y < h; ++y)
            {
                const uint16_t* row = sf.pixels + size_t(y) * sf.stridePixels * 4;
                for (int32_t x = 0; x < w; ++x)
                {
                    if ((row[x * 4 + 3] & 0x7fff) == 0) // fp16 alpha, +0 or -0
                        continue;
                    inkL = (std::min)(inkL, x); inkR = (std::max)(inkR, x);
                    inkT = (std::min)(inkT, y); inkB = (std::max)(inkB, y);
                }
            }

            static uint64_t nextTextId = uint64_t(1) << 62; // apart from cachedImage()'s ids
            Bitmap::CachedText entry;
            textFormat->addRef();
            entry.format.attach(textFormat);
            if (inkR >= inkL) // otherwise nothing drew: cached as empty, so it stays a hit
            {
                entry.w = inkR - inkL + 1;
                entry.h = inkB - inkT + 1;
                entry.pixels = packSurface(sf, inkL, inkT, entry.w, entry.h);
                entry.offsetX = left + inkL - int32_t(snapX);
                entry.offsetY = top + inkT - int32_t(snapY);
                entry.id = nextTextId++;
                entry.retained = std::make_unique<gmpi::gpu::ImageAtlas::PackedImage>(
                    bitmap->canvas.retainHalfImage(reinterpret_cast<const unsigned char*>(entry.pixels.data()),
                                                   entry.w, entry.h, entry.id));
                bitmap->textCacheBytes += entry.pixels.size() * sizeof(uint16_t);
            }
            it = cache.emplace(std::move(key), std::move(entry)).first;
        }

        it->second.lastUsed = bitmap->submits;
        if (it->second.w > 0)
            bitmap->drawCachedText(it->second, snapX + float(it->second.offsetX), snapY + float(it->second.offsetY), currentClamp());
        return true;
    }

    ReturnCode drawRichTextU(drawing::api::IRichTextFormat* richTextFormat, const drawing::Rect* layoutRect,
                             drawing::api::IBrush* brush, int32_t options) override
    {
        auto* engine = textEngine();
        if (!engine || !richTextFormat || !layoutRect)
            return ReturnCode::NoSupport;
        drawing::Size extent{ 0, 0 };
        richTextFormat->getTextExtentU(&extent);
        return viaCpu(textDeviceBounds(*layoutRect, extent), [&](cpugfx::RenderTarget& cpu) {
            return engine->drawRichText(&cpu, richTextFormat, *layoutRect, brush, options);
        }, "drawRichTextU");
    }

    ReturnCode drawTextLayout(drawing::Point point, drawing::api::ITextLayout* textLayout,
                              drawing::api::IBrush* brush, int32_t options) override
    {
        auto* engine = textEngine();
        if (!engine || !textLayout)
            return ReturnCode::NoSupport;
        // ITextLayout exposes only the tight extent; the engine knows where
        // the aligned block lands (module titles are laid out in a box the
        // size of the module, and were costing a module's worth of raster
        // and upload each).
        drawing::Size extent{ 0, 0 };
        textLayout->getTextExtentU(&extent);
        drawing::Rect placed{};
        drawing::Rect bounds = clipRectStack.back();
        if (engine->getTextLayoutPlacement(textLayout, &placed) == ReturnCode::Ok)
            bounds = textDeviceBounds({ point.x + placed.left, point.y + placed.top, point.x + placed.right, point.y + placed.bottom }, extent);
        return viaCpu(bounds, [&](cpugfx::RenderTarget& cpu) {
            return engine->drawTextLayout(&cpu, textLayout, point, brush, options);
        }, "drawTextLayout");
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

    // Not an interface method: a host's swap-chain target for a native
    // window (HWND / NSView / X11 Window / wl_surface*), after initialize().
    RenderTarget* createWindowRenderTarget(void* nativeWindow, int32_t width, int32_t height, float dpi = 96.0f)
    {
        try
        {
            return new RenderTarget(this, { uint32_t(width), uint32_t(height) }, 0, dpi, nativeWindow);
        }
        catch (...)
        {
            return nullptr;
        }
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
