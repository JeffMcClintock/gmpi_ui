#pragma once

/*
#include "backends/CpuPresent.h"
*/

// The present pipeline the X11 and Wayland CPU paths share: render into a
// fresh cpugfx target, encode the dirty part of it into the window system's
// 8-bit buffer. CpuEncode.h knows nothing of CpuGfx.h on purpose (see its
// header); this is the layer that knows both. The blit itself - XPutImage,
// wl_surface_attach - stays with the frame, as does the buffer.
//
// The GPU path has the same shape in gpugfx::WindowPresenter, which shares
// deviceArea() with this.

#include <chrono>
#include <cstdint>
#include <functional>
#include <utility>

#include "CpuGfx.h"
#include "CpuEncode.h"
#include "../helpers/NativeUi.h" // IDrawingClient

namespace gmpi::cpugfx {

// The device-pixel rectangle a frame has to repaint: the whole pw x ph
// surface, or `dirty` (logical units) scaled up and padded out by a pixel.
inline drawing::RectL deviceArea(bool dirtyAll, const drawing::Rect& dirty, float scale, int pw, int ph)
{
    drawing::RectL area{ 0, 0, pw, ph };
    if (!dirtyAll && dirty.right > dirty.left && dirty.bottom > dirty.top)
    {
        area.left   = (std::max)(0,  static_cast<int32_t>(dirty.left   * scale));
        area.top    = (std::max)(0,  static_cast<int32_t>(dirty.top    * scale));
        area.right  = (std::min)(pw, static_cast<int32_t>(dirty.right  * scale + 1.0f));
        area.bottom = (std::min)(ph, static_cast<int32_t>(dirty.bottom * scale + 1.0f));
    }
    return area;
}

// A w x h CPU render target for one present. cpugfx has no resize, so a size
// change means a new target; one per frame is deliberate - an interactive
// resize reallocates every frame, which measures fine and is far simpler
// than pooling.
class Painter
{
public:
    Painter(Factory& factory, int w, int h)
    {
        factory.createCpuRenderTarget({ static_cast<uint32_t>(w), static_cast<uint32_t>(h) }, 0, &rtRaw_, 96.0f);
        rt_ = dynamic_cast<RenderTarget*>(rtRaw_);
    }
    ~Painter()
    {
        if (bmRaw_) bmRaw_->release();
        if (rtRaw_) rtRaw_->release();
    }
    Painter(const Painter&) = delete;
    Painter& operator=(const Painter&) = delete;

    explicit operator bool() const { return rt_ != nullptr; }
    RenderTarget& target() const { return *rt_; }

    // `paint` draws between beginDraw and endDraw.
    void draw(const std::function<void(RenderTarget&)>& paint)
    {
        rt_->beginDraw();
        paint(*rt_);
        rt_->endDraw();
    }

    // After draw(): `area` of the result, converted to 8-bit into dst.
    void encode(const DestSurface& dst, drawing::RectL area)
    {
        if (!bmRaw_)
            rt_->getBitmap(&bmRaw_);
        auto* bm = dynamic_cast<Bitmap*>(bmRaw_);
        if (!bm)
            return;
        const auto& s = bm->surface;
        encodeDirtyRect({ s.pixels, s.stridePixels, s.width, s.height }, dst, area);
    }

private:
    drawing::api::IBitmapRenderTarget* rtRaw_{};
    RenderTarget* rt_{};
    drawing::api::IBitmap* bmRaw_{};
};

// Paints a whole w x h surface and encodes all of it into dst: a popup, a
// tooltip, a dialog. False when no target could be made.
inline bool paintInto(Factory& factory, const DestSurface& dst, int w, int h,
                      const std::function<void(RenderTarget&)>& paint)
{
    Painter p(factory, w, h);
    if (!p)
        return false;
    p.draw(paint);
    p.encode(dst, { 0, 0, w, h });
    return true;
}

// One frame of a window's drawing client.
struct FrameResult
{
    drawing::RectL area{};   // device pixels encoded; empty when the frame was dropped
    double renderMs{};       // client render
    double encodeMs{};       // 8-bit encode
    bool presented() const { return area.right > area.left && area.bottom > area.top; }
};

// Takes the frame's flags before the first client call (a client may
// invalidate while it is measured or rendered - SynthEdit's TiDE panel, told
// mid-render that a knob moved over it, asks for a full repaint so it can show
// its previous face while the new one traces - and clearing afterwards threw
// that away). Measures and arranges when asked, renders the client at
// `scale`, then `overlay` on top of it (an in-place text edit), and encodes
// the dirty area into the surface `dest` returns. `dirty` is in logical
// units; pw x ph is the buffer, logicalW x logicalH what the client is told.
//
// `dest` is called AFTER rendering: measure/arrange/render are client calls,
// and one may have re-entered the frame and resized its buffer (the X11
// nested-present guard, tests/x11_present_reentrant_resize.cpp). A dest
// surface whose size is not pw x ph drops the frame - the nested present
// already put the new size on screen, and encoding pw x ph into a smaller
// buffer is a heap overflow.
inline FrameResult presentFrame(Factory& factory, api::IDrawingClient* client, int pw, int ph,
                                float logicalW, float logicalH, float scale,
                                bool& needsMeasure, bool& dirtyAll, drawing::Rect& dirty,
                                const std::function<void(RenderTarget&)>& overlay,
                                const std::function<DestSurface()>& dest)
{
    FrameResult result{};
    Painter p(factory, pw, ph);
    if (!p)
        return result;

    const bool wantAll = std::exchange(dirtyAll, false);
    const drawing::Rect want = std::exchange(dirty, {});

    if (needsMeasure)
    {
        drawing::Size avail{ logicalW, logicalH };
        drawing::Size desired{};
        client->measure(&avail, &desired);
        const drawing::Rect all{ 0, 0, avail.width, avail.height };
        client->arrange(&all);
        needsMeasure = false;
    }

    const auto tRender = std::chrono::steady_clock::now();
    p.draw([&](RenderTarget& rt)
    {
        // scale through the transform, not by rendering small and stretching -
        // which is what makes fractional scale come out sharp
        if (scale != 1.0f)
        {
            const auto m = drawing::makeScale(scale, scale);
            rt.setTransform(&m);
        }
        client->render(static_cast<drawing::api::IDeviceContext*>(&rt));
        if (overlay)
            overlay(rt);
    });
    const auto tRendered = std::chrono::steady_clock::now();

    const DestSurface dst = dest();
    if (!dst.pixels || dst.width != pw || dst.height != ph)
        return result;

    result.area = deviceArea(wantAll, want, scale, pw, ph);
    p.encode(dst, result.area);

    using ms = std::chrono::duration<double, std::milli>;
    result.renderMs = ms(tRendered - tRender).count();
    result.encodeMs = ms(std::chrono::steady_clock::now() - tRendered).count();
    return result;
}

} // namespace gmpi::cpugfx
