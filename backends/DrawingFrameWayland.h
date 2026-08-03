#pragma once

/*
#include "backends/DrawingFrameWayland.h"
*/

// Wayland host for gmpi_ui, drawing through the CPU backend.
//
// A fifth peer to DrawingFrameWin.h / DrawingFrameMac.mm / the JUCE host, against
// the same contract in helpers/NativeUi.h, and layered the same way:
//
//   WaylandHostBase        IDrawingHost + IInputHost
//     WaylandFrameBase     + the present pipeline (wl_shm + CpuEncode)
//       WaylandToplevel    + a libdecor-decorated window of its own
//
// The split is not cosmetic. NOTHING above WaylandToplevel creates a toplevel or
// owns the display connection, because in a plugin it cannot have either: VST3
// hands the plugin the HOST's wl_display (IWaylandHost::openWaylandConnection),
// the host's parent wl_surface to make a subsurface against, and the host's
// xdg_surface to hang popups on (IWaylandFrame::getParentSurface). So the
// connection and the parent are injected, and a plugin view is a sibling of
// WaylandToplevel rather than a special case inside it.
//
// BUILD: the generated protocol headers are the consumer's job, since gmpi_ui has
// no build system of its own. Run wayland-scanner over, at minimum:
//     stable/xdg-shell/xdg-shell
//     stable/viewporter/viewporter
//     staging/fractional-scale/fractional-scale-v1
//     staging/cursor-shape/cursor-shape-v1
// and link wayland-client, xkbcommon and libdecor-0.

#include <wayland-client.h>
#include <libdecor.h>
#include <xkbcommon/xkbcommon.h>

#include <sys/mman.h>
#include <unistd.h>

#include <cstring>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "backends/CpuGfx.h"
#include "backends/CpuEncode.h"
#include "helpers/NativeUi.h"

#include "xdg-shell-client-protocol.h"
#include "viewporter-client-protocol.h"
#include "fractional-scale-v1-client-protocol.h"
#include "cursor-shape-v1-client-protocol.h"

namespace gmpi
{
namespace wayland
{

// ---------------------------------------------------------------------------
// Shared-memory buffer
// ---------------------------------------------------------------------------
// XRGB8888 throughout: SynthEdit presents an opaque surface, so there is no alpha
// for the compositor to blend, and that is exactly what CpuEncode's Bgra8888
// encoding produces (premultiplied source composited over black, stored as-is).
class ShmBuffer
{
public:
    ~ShmBuffer() { release(); }

    bool create(wl_shm* shm, int w, int h)
    {
        release();

        width_  = w;
        height_ = h;
        stride_ = w * 4;
        size_   = static_cast<size_t>(stride_) * h;

        const int fd = memfd_create("gmpi-wl", MFD_CLOEXEC);
        if (fd < 0)
            return false;

        if (ftruncate(fd, static_cast<off_t>(size_)) < 0) { close(fd); return false; }

        pixels_ = static_cast<uint8_t*>(
            mmap(nullptr, size_, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0));
        if (pixels_ == MAP_FAILED) { pixels_ = nullptr; close(fd); return false; }

        wl_shm_pool* pool = wl_shm_create_pool(shm, fd, static_cast<int32_t>(size_));
        buffer_ = wl_shm_pool_create_buffer(pool, 0, w, h, stride_, WL_SHM_FORMAT_XRGB8888);
        wl_shm_pool_destroy(pool);
        close(fd);

        return buffer_ != nullptr;
    }

    void release()
    {
        if (buffer_) { wl_buffer_destroy(buffer_); buffer_ = nullptr; }
        if (pixels_) { munmap(pixels_, size_); pixels_ = nullptr; }
        size_ = 0;
        width_ = height_ = stride_ = 0;
    }

    bool matches(int w, int h) const { return pixels_ && width_ == w && height_ == h; }

    wl_buffer* buffer() const { return buffer_; }
    uint8_t*   pixels() const { return pixels_; }
    int        stride() const { return stride_; }
    int        width()  const { return width_; }
    int        height() const { return height_; }

private:
    wl_buffer* buffer_{};
    uint8_t*   pixels_{};
    size_t     size_{};
    int        width_{}, height_{}, stride_{};
};

// ---------------------------------------------------------------------------
// Connection
// ---------------------------------------------------------------------------
// Owned when standalone, borrowed when hosted in a plugin. Everything else takes
// this by reference and never asks where it came from.
class Connection
{
public:
    // standalone: open our own
    bool open()
    {
        display_ = wl_display_connect(nullptr);
        owned_ = display_ != nullptr;
        return owned_ && bindGlobals();
    }

    // plugin: use the host's, and never close it
    bool adopt(wl_display* hostDisplay)
    {
        display_ = hostDisplay;
        owned_ = false;
        return display_ && bindGlobals();
    }

    ~Connection()
    {
        if (owned_ && display_)
            wl_display_disconnect(display_);
    }

    wl_display*    display()    const { return display_; }
    wl_compositor* compositor() const { return compositor_; }
    wl_shm*        shm()        const { return shm_; }
    xdg_wm_base*   wmBase()     const { return wmBase_; }
    wl_seat*       seat()       const { return seat_; }
    wp_viewporter* viewporter() const { return viewporter_; }
    wp_fractional_scale_manager_v1* fractionalScale() const { return fracMgr_; }
    wp_cursor_shape_manager_v1*     cursorShape()     const { return cursorMgr_; }

    bool ownsDisplay() const { return owned_; }

private:
    bool bindGlobals();

    wl_display*    display_{};
    wl_registry*   registry_{};
    wl_compositor* compositor_{};
    wl_shm*        shm_{};
    xdg_wm_base*   wmBase_{};
    wl_seat*       seat_{};
    wp_viewporter* viewporter_{};
    wp_fractional_scale_manager_v1* fracMgr_{};
    wp_cursor_shape_manager_v1*     cursorMgr_{};
    bool owned_{};

    static void globalAdd(void* data, wl_registry* reg, uint32_t name,
                          const char* iface, uint32_t version);
    static void globalRemove(void*, wl_registry*, uint32_t) {}
    static void ping(void*, xdg_wm_base* b, uint32_t serial) { xdg_wm_base_pong(b, serial); }
};

inline void Connection::globalAdd(void* data, wl_registry* reg, uint32_t name,
                                  const char* iface, uint32_t)
{
    auto& c = *static_cast<Connection*>(data);

    if (!strcmp(iface, wl_compositor_interface.name))
        c.compositor_ = static_cast<wl_compositor*>(wl_registry_bind(reg, name, &wl_compositor_interface, 4));
    else if (!strcmp(iface, wl_shm_interface.name))
        c.shm_ = static_cast<wl_shm*>(wl_registry_bind(reg, name, &wl_shm_interface, 1));
    else if (!strcmp(iface, xdg_wm_base_interface.name))
        c.wmBase_ = static_cast<xdg_wm_base*>(wl_registry_bind(reg, name, &xdg_wm_base_interface, 1));
    else if (!strcmp(iface, wl_seat_interface.name))
        c.seat_ = static_cast<wl_seat*>(wl_registry_bind(reg, name, &wl_seat_interface, 5));
    else if (!strcmp(iface, wp_viewporter_interface.name))
        c.viewporter_ = static_cast<wp_viewporter*>(wl_registry_bind(reg, name, &wp_viewporter_interface, 1));
    else if (!strcmp(iface, wp_fractional_scale_manager_v1_interface.name))
        c.fracMgr_ = static_cast<wp_fractional_scale_manager_v1*>(wl_registry_bind(reg, name, &wp_fractional_scale_manager_v1_interface, 1));
    else if (!strcmp(iface, wp_cursor_shape_manager_v1_interface.name))
        c.cursorMgr_ = static_cast<wp_cursor_shape_manager_v1*>(wl_registry_bind(reg, name, &wp_cursor_shape_manager_v1_interface, 1));
}

inline bool Connection::bindGlobals()
{
    static const wl_registry_listener registryListener = { globalAdd, globalRemove };
    static const xdg_wm_base_listener wmListener = { ping };

    registry_ = wl_display_get_registry(display_);
    wl_registry_add_listener(registry_, &registryListener, this);
    wl_display_roundtrip(display_);

    if (wmBase_)
        xdg_wm_base_add_listener(wmBase_, &wmListener, this);

    // A compositor without these is not one we can draw on at all.
    return compositor_ && shm_;
}

inline bool isEmpty(const gmpi::drawing::Rect& r)
{
    return r.right <= r.left || r.bottom <= r.top;
}

// ---------------------------------------------------------------------------
// WaylandHostBase - IDrawingHost + IInputHost
// ---------------------------------------------------------------------------
class WaylandHostBase : public gmpi::api::IDrawingHost, public gmpi::api::IInputHost
{
public:
    virtual ~WaylandHostBase() = default;

    // --- IDrawingHost ---
    gmpi::ReturnCode getDrawingFactory(gmpi::api::IUnknown** returnFactory) override
    {
        *returnFactory = static_cast<gmpi::api::IUnknown*>(&factory_);
        return gmpi::ReturnCode::Ok;
    }

    void invalidateRect(const gmpi::drawing::Rect* invalidRect) override
    {
        if (!invalidRect)
        {
            dirtyAll_ = true;
            return;
        }

        // One union rather than a list: the encode is cheap and a second surface
        // damage costs a round trip, so tracking many small rects loses.
        if (isEmpty(dirty_))
            dirty_ = *invalidRect;
        else
        {
            dirty_.left   = (std::min)(dirty_.left,   invalidRect->left);
            dirty_.top    = (std::min)(dirty_.top,    invalidRect->top);
            dirty_.right  = (std::max)(dirty_.right,  invalidRect->right);
            dirty_.bottom = (std::max)(dirty_.bottom, invalidRect->bottom);
        }
    }

    void invalidateMeasure() override { needsMeasure_ = true; }

    float getRasterizationScale() override { return scale_; }

    // --- IInputHost ---
    // Wayland has no pointer capture and no pointer warp. Tracking it as a flag is
    // enough for in-window drags, because the compositor keeps delivering motion to
    // the surface that saw the button press until release. It is NOT enough for
    // SynthEdit's wire pickup (drag with no button held), which is why the plan
    // lists this as an open item needing pointer-constraints or a gesture change.
    gmpi::ReturnCode setCapture() override      { captured_ = true;  return gmpi::ReturnCode::Ok; }
    gmpi::ReturnCode getCapture(bool& v) override { v = captured_;   return gmpi::ReturnCode::Ok; }
    gmpi::ReturnCode releaseCapture() override  { captured_ = false; return gmpi::ReturnCode::Ok; }

protected:
    gmpi::cpugfx::Factory factory_;
    gmpi::drawing::Rect   dirty_{};
    bool  dirtyAll_ = true;
    bool  needsMeasure_ = true;
    bool  captured_ = false;
    float scale_ = 1.0f;

};

// ---------------------------------------------------------------------------
// WaylandFrameBase - the present pipeline
// ---------------------------------------------------------------------------
// Owns a wl_surface and paints an IDrawingClient into it. Knows nothing about
// where the surface sits: a toplevel and a plugin's subsurface both land here.
class WaylandFrameBase : public WaylandHostBase
{
public:
    explicit WaylandFrameBase(Connection& connection) : connection_(connection) {}

    ~WaylandFrameBase() override
    {
        detachClient();
        buffer_.release();
        if (viewport_) wp_viewport_destroy(viewport_);
        if (surface_)  wl_surface_destroy(surface_);
    }

    void attachClient(gmpi::api::IDrawingClient* client)
    {
        detachClient();
        client_ = client;
        if (client_)
            client_->setHost(static_cast<gmpi::api::IDrawingHost*>(this));
    }

    void detachClient()
    {
        if (client_)
            client_->setHost(nullptr);
        client_ = {};
    }

    wl_surface* surface() const { return surface_; }

    void setLogicalSize(int w, int h)
    {
        if (w == logicalW_ && h == logicalH_)
            return;
        logicalW_ = w;
        logicalH_ = h;
        dirtyAll_ = true;
        needsMeasure_ = true;
    }

    void setScale(float s)
    {
        if (s <= 0.0f || s == scale_)
            return;
        scale_ = s;
        dirtyAll_ = true;
    }

    bool hasPendingPaint() const { return dirtyAll_ || !isEmpty(dirty_); }

    // Render whatever is dirty and hand the pixels to the compositor.
    void present();

protected:
    void createSurface();

    Connection&  connection_;
    wl_surface*  surface_{};
    wp_viewport* viewport_{};
    ShmBuffer    buffer_;

    gmpi::api::IDrawingClient* client_{};

    int logicalW_ = 0, logicalH_ = 0;
};

inline void WaylandFrameBase::createSurface()
{
    surface_ = wl_compositor_create_surface(connection_.compositor());

    if (connection_.viewporter())
        viewport_ = wp_viewporter_get_viewport(connection_.viewporter(), surface_);
}

inline void WaylandFrameBase::present()
{
    if (!surface_ || !client_ || logicalW_ <= 0 || logicalH_ <= 0)
        return;

    const int pw = static_cast<int>(logicalW_ * scale_ + 0.5f);
    const int ph = static_cast<int>(logicalH_ * scale_ + 0.5f);
    if (pw <= 0 || ph <= 0)
        return;

    if (!buffer_.matches(pw, ph))
    {
        if (!buffer_.create(connection_.shm(), pw, ph))
            return;
        dirtyAll_ = true;
    }

    // cpugfx has no resize, so a size change means a new target. Deliberately not
    // cached across sizes: an interactive resize reallocates every frame, which is
    // measurably fine and much simpler than pooling.
    gmpi::drawing::api::IBitmapRenderTarget* rtRaw{};
    factory_.createCpuRenderTarget({ static_cast<uint32_t>(pw), static_cast<uint32_t>(ph) },
                                   0, &rtRaw, 96.0f);
    auto* rt = dynamic_cast<gmpi::cpugfx::RenderTarget*>(rtRaw);
    if (!rt) { if (rtRaw) rtRaw->release(); return; }

    if (needsMeasure_)
    {
        gmpi::drawing::Size avail{ static_cast<float>(logicalW_), static_cast<float>(logicalH_) };
        gmpi::drawing::Size desired{};
        client_->measure(&avail, &desired);

        const gmpi::drawing::Rect all{ 0, 0, avail.width, avail.height };
        client_->arrange(&all);
        needsMeasure_ = false;
    }

    rt->beginDraw();
    // scale is applied through the transform, not by rendering small and stretching -
    // which is what makes fractional scale come out sharp.
    if (scale_ != 1.0f)
    {
        const auto m = gmpi::drawing::makeScale(scale_, scale_);
        rt->setTransform(&m);
    }
    client_->render(static_cast<gmpi::drawing::api::IDeviceContext*>(rt));
    rt->endDraw();

    gmpi::drawing::api::IBitmap* bmRaw{};
    rt->getBitmap(&bmRaw);
    if (auto* bm = dynamic_cast<gmpi::cpugfx::Bitmap*>(bmRaw))
    {
        const auto& s = bm->surface;

        const gmpi::cpugfx::SourceSurface src{ s.pixels, s.stridePixels, s.width, s.height };
        const gmpi::cpugfx::DestSurface   dst{ buffer_.pixels(), buffer_.stride(), pw, ph,
                                               gmpi::cpugfx::PixelEncoding::Bgra8888 };

        gmpi::drawing::RectL area{ 0, 0, pw, ph };
        if (!dirtyAll_ && !isEmpty(dirty_))
        {
            area.left   = (std::max)(0,  static_cast<int32_t>(dirty_.left   * scale_));
            area.top    = (std::max)(0,  static_cast<int32_t>(dirty_.top    * scale_));
            area.right  = (std::min)(pw, static_cast<int32_t>(dirty_.right  * scale_ + 1.0f));
            area.bottom = (std::min)(ph, static_cast<int32_t>(dirty_.bottom * scale_ + 1.0f));
        }

        gmpi::cpugfx::encodeDirtyRect(src, dst, area);

        wl_surface_attach(surface_, buffer_.buffer(), 0, 0);
        wl_surface_damage_buffer(surface_, area.left, area.top,
                                 area.right - area.left, area.bottom - area.top);
    }

    if (bmRaw) bmRaw->release();
    if (rtRaw) rtRaw->release();

    // viewporter maps the physical-pixel buffer onto the logical size, so the
    // compositor never rescales what we drew
    if (viewport_)
        wp_viewport_set_destination(viewport_, logicalW_, logicalH_);

    dirtyAll_ = false;
    dirty_ = {};
}

} // namespace wayland
} // namespace gmpi
