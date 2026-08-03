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
#include <sys/timerfd.h>
#include <poll.h>
#include <unistd.h>
#include <cerrno>
#include <cstdio>

#include <cstring>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "backends/CpuGfx.h"
#include "backends/CpuEncode.h"
#include "helpers/NativeUi.h"
#include "RefCountMacros.h"

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

    // The CPU backend deliberately contains no font, shaping or image-decode code,
    // so the host cannot supply them either without dragging fontconfig, HarfBuzz and
    // libpng into gmpi_ui's Wayland layer. The application wires them, exactly as
    // DrawingFrame2_cpu.h and the test fixtures do:
    //
    //     host.drawingFactory().textEngine   = &textEngine;
    //     host.drawingFactory().imageDecoder = gmpi::drawing::decodeImageFile;
    //
    // Without a text engine the backend draws geometry fine and text not at all,
    // which is a silent and confusing failure - hence saying so here.
    gmpi::cpugfx::Factory& drawingFactory() { return factory_; }

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

    // --- IUnknown ---
    // The application owns the host for its whole life, so it is not refcounted -
    // same arrangement as DxDrawingFrameBase on Windows.
    gmpi::ReturnCode queryInterface(const gmpi::api::Guid* iid, void** returnInterface) override
    {
        *returnInterface = {};
        GMPI_QUERYINTERFACE(gmpi::api::IDrawingHost);
        GMPI_QUERYINTERFACE(gmpi::api::IInputHost);
        return gmpi::ReturnCode::NoSupport;
    }
    GMPI_REFCOUNT_NO_DELETE;

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

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------
// Turns seat events into IInputClient calls. Pointer coordinates arrive in
// LOGICAL units and are handed on unscaled, because that is the space the client
// laid itself out in - scaling happens in the render transform, not here.
class InputDispatch
{
public:
    void attachInputClient(gmpi::api::IInputClient* client) { client_ = client; }

    // the surface these events belong to; a popup has its own dispatcher
    void setSurface(wl_surface* s) { surface_ = s; }

    void bindSeat(Connection& connection);

    // last serial from a real user action - popup grabs and cursor changes are
    // only valid with one of these, and the compositor rejects anything else
    uint32_t lastSerial() const { return lastSerial_; }

    bool pointerInside() const { return inside_; }
    gmpi::drawing::Point pointerPos() const { return { float(x_), float(y_) }; }

    std::function<void()> onNeedsRedraw;
    std::function<void(gmpi::drawing::Point, uint32_t serial)> onContextMenu;

private:
    int32_t modifierFlags() const
    {
        int32_t f = 0;
        if (!xkbState_) return f;
        if (xkb_state_mod_name_is_active(xkbState_, XKB_MOD_NAME_SHIFT, XKB_STATE_MODS_EFFECTIVE) > 0)
            f |= int32_t(gmpi::api::PointerFlags::KeyShift);
        if (xkb_state_mod_name_is_active(xkbState_, XKB_MOD_NAME_CTRL, XKB_STATE_MODS_EFFECTIVE) > 0)
            f |= int32_t(gmpi::api::PointerFlags::KeyControl);
        if (xkb_state_mod_name_is_active(xkbState_, XKB_MOD_NAME_ALT, XKB_STATE_MODS_EFFECTIVE) > 0)
            f |= int32_t(gmpi::api::PointerFlags::KeyAlt);
        return f;
    }

    static void pointerEnter(void*, wl_pointer*, uint32_t, wl_surface*, wl_fixed_t, wl_fixed_t);
    static void pointerLeave(void*, wl_pointer*, uint32_t, wl_surface*);
    static void pointerMotion(void*, wl_pointer*, uint32_t, wl_fixed_t, wl_fixed_t);
    static void pointerButton(void*, wl_pointer*, uint32_t, uint32_t, uint32_t, uint32_t);
    static void pointerAxis(void*, wl_pointer*, uint32_t, uint32_t, wl_fixed_t);
    static void pointerNop(void*, wl_pointer*) {}
    static void pointerU32(void*, wl_pointer*, uint32_t) {}
    static void pointerU32U32(void*, wl_pointer*, uint32_t, uint32_t) {}
    static void pointerU32I32(void*, wl_pointer*, uint32_t, int32_t) {}

    static void keyboardKeymap(void*, wl_keyboard*, uint32_t, int32_t, uint32_t);
    static void keyboardEnter(void*, wl_keyboard*, uint32_t, wl_surface*, wl_array*);
    static void keyboardLeave(void*, wl_keyboard*, uint32_t, wl_surface*);
    static void keyboardKey(void*, wl_keyboard*, uint32_t, uint32_t, uint32_t, uint32_t);
    static void keyboardModifiers(void*, wl_keyboard*, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
    static void keyboardRepeat(void*, wl_keyboard*, int32_t, int32_t) {}

    static void seatCapabilities(void*, wl_seat*, uint32_t);
    static void seatName(void*, wl_seat*, const char*) {}

    gmpi::api::IInputClient* client_{};
    wl_surface*  surface_{};
    wl_pointer*  pointer_{};
    wl_keyboard* keyboard_{};
    wp_cursor_shape_device_v1* cursorShape_{};
    Connection*  connection_{};

    xkb_context* xkb_{};
    xkb_keymap*  keymap_{};
    xkb_state*   xkbState_{};

    double   x_ = 0, y_ = 0;
    bool     inside_ = false;
    uint32_t buttons_ = 0;
    uint32_t lastSerial_ = 0;
};

inline void InputDispatch::pointerEnter(void* data, wl_pointer*, uint32_t serial,
                                        wl_surface* surf, wl_fixed_t sx, wl_fixed_t sy)
{
    auto& in = *static_cast<InputDispatch*>(data);
    if (surf != in.surface_)
        return;

    in.inside_ = true;
    in.lastSerial_ = serial;
    in.x_ = wl_fixed_to_double(sx);
    in.y_ = wl_fixed_to_double(sy);

    // cursor-shape-v1: name a shape and let the compositor theme it. The classic
    // route - load an XCursor theme, build a wl_surface, attach the image - is
    // about a hundred lines for the same result.
    if (in.cursorShape_)
        wp_cursor_shape_device_v1_set_shape(in.cursorShape_, serial,
            WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_DEFAULT);

    if (in.client_)
        in.client_->setHover(true);
    if (in.onNeedsRedraw)
        in.onNeedsRedraw();
}

inline void InputDispatch::pointerLeave(void* data, wl_pointer*, uint32_t serial, wl_surface* surf)
{
    auto& in = *static_cast<InputDispatch*>(data);
    if (surf != in.surface_)
        return;

    in.inside_ = false;
    in.lastSerial_ = serial;

    if (in.client_)
        in.client_->setHover(false);
    if (in.onNeedsRedraw)
        in.onNeedsRedraw();
}

inline void InputDispatch::pointerMotion(void* data, wl_pointer*, uint32_t,
                                         wl_fixed_t sx, wl_fixed_t sy)
{
    auto& in = *static_cast<InputDispatch*>(data);
    in.x_ = wl_fixed_to_double(sx);
    in.y_ = wl_fixed_to_double(sy);

    if (in.client_)
        in.client_->onPointerMove(in.pointerPos(), in.modifierFlags() | int32_t(in.buttons_));
}

inline void InputDispatch::pointerButton(void* data, wl_pointer*, uint32_t serial,
                                         uint32_t, uint32_t button, uint32_t state)
{
    auto& in = *static_cast<InputDispatch*>(data);
    in.lastSerial_ = serial;

    // evdev button codes; there is no wl_pointer enum for them
    int32_t bit = 0;
    switch (button)
    {
    case 272: bit = int32_t(gmpi::api::PointerFlags::FirstButton);  break; // BTN_LEFT
    case 273: bit = int32_t(gmpi::api::PointerFlags::SecondButton); break; // BTN_RIGHT
    case 274: bit = int32_t(gmpi::api::PointerFlags::ThirdButton);  break; // BTN_MIDDLE
    default: return;
    }

    const bool pressed = (state == WL_POINTER_BUTTON_STATE_PRESSED);
    if (pressed) in.buttons_ |= uint32_t(bit);
    else         in.buttons_ &= ~uint32_t(bit);

    const int32_t flags = in.modifierFlags() | bit |
                          (pressed ? int32_t(gmpi::api::PointerFlags::InContact) : 0);

    if (in.client_)
    {
        if (pressed) in.client_->onPointerDown(in.pointerPos(), flags);
        else         in.client_->onPointerUp(in.pointerPos(), flags);
    }

    // A context menu must be opened from a real input serial or the compositor
    // refuses the popup grab, so the serial travels with the request.
    if (pressed && button == 273 && in.onContextMenu)
        in.onContextMenu(in.pointerPos(), serial);
}

inline void InputDispatch::pointerAxis(void* data, wl_pointer*, uint32_t,
                                       uint32_t axis, wl_fixed_t value)
{
    auto& in = *static_cast<InputDispatch*>(data);
    if (!in.client_)
        return;

    int32_t flags = in.modifierFlags();
    if (axis == WL_POINTER_AXIS_HORIZONTAL_SCROLL)
        flags |= int32_t(gmpi::api::PointerFlags::ScrollHoriz);

    // wheel deltas arrive as a scroll distance; SE expects notches of 120
    const double d = wl_fixed_to_double(value);
    in.client_->onMouseWheel(in.pointerPos(), flags, static_cast<int32_t>(-d * 10.0));
}

inline void InputDispatch::keyboardKeymap(void* data, wl_keyboard*, uint32_t format,
                                          int32_t fd, uint32_t size)
{
    auto& in = *static_cast<InputDispatch*>(data);

    if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) { close(fd); return; }

    char* map = static_cast<char*>(mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0));
    if (map == MAP_FAILED) { close(fd); return; }

    if (in.keymap_)   xkb_keymap_unref(in.keymap_);
    if (in.xkbState_) xkb_state_unref(in.xkbState_);

    in.keymap_ = xkb_keymap_new_from_string(in.xkb_, map, XKB_KEYMAP_FORMAT_TEXT_V1,
                                            XKB_KEYMAP_COMPILE_NO_FLAGS);
    munmap(map, size);
    close(fd);

    in.xkbState_ = in.keymap_ ? xkb_state_new(in.keymap_) : nullptr;
}

inline void InputDispatch::keyboardKey(void* data, wl_keyboard*, uint32_t serial,
                                       uint32_t, uint32_t key, uint32_t state)
{
    auto& in = *static_cast<InputDispatch*>(data);
    in.lastSerial_ = serial;

    if (!in.xkbState_ || !in.client_ || state != WL_KEYBOARD_KEY_STATE_PRESSED)
        return;

    // wayland reports evdev keycodes; xkb numbers them 8 higher
    const xkb_keycode_t code = key + 8;

    uint32_t utf32 = xkb_state_key_get_utf32(in.xkbState_, code);
    if (utf32)
        in.client_->onKeyPress(static_cast<wchar_t>(utf32));
}

inline void InputDispatch::keyboardEnter(void*, wl_keyboard*, uint32_t, wl_surface*, wl_array*) {}
inline void InputDispatch::keyboardLeave(void*, wl_keyboard*, uint32_t, wl_surface*) {}

inline void InputDispatch::keyboardModifiers(void* data, wl_keyboard*, uint32_t,
                                             uint32_t depressed, uint32_t latched,
                                             uint32_t locked, uint32_t group)
{
    auto& in = *static_cast<InputDispatch*>(data);
    if (in.xkbState_)
        xkb_state_update_mask(in.xkbState_, depressed, latched, locked, 0, 0, group);
}

inline void InputDispatch::seatCapabilities(void* data, wl_seat* seat, uint32_t caps)
{
    auto& in = *static_cast<InputDispatch*>(data);

    static const wl_pointer_listener pointerListener = {
        pointerEnter, pointerLeave, pointerMotion, pointerButton, pointerAxis,
        pointerNop, pointerU32, pointerU32U32, pointerU32I32, pointerU32I32, pointerU32U32
    };
    static const wl_keyboard_listener keyboardListener = {
        keyboardKeymap, keyboardEnter, keyboardLeave, keyboardKey,
        keyboardModifiers, keyboardRepeat
    };

    if ((caps & WL_SEAT_CAPABILITY_POINTER) && !in.pointer_)
    {
        in.pointer_ = wl_seat_get_pointer(seat);
        wl_pointer_add_listener(in.pointer_, &pointerListener, &in);

        if (in.connection_ && in.connection_->cursorShape())
            in.cursorShape_ = wp_cursor_shape_manager_v1_get_pointer(
                in.connection_->cursorShape(), in.pointer_);
    }

    if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !in.keyboard_)
    {
        in.keyboard_ = wl_seat_get_keyboard(seat);
        wl_keyboard_add_listener(in.keyboard_, &keyboardListener, &in);
    }
}

inline void InputDispatch::bindSeat(Connection& connection)
{
    static const wl_seat_listener seatListener = { seatCapabilities, seatName };

    connection_ = &connection;
    xkb_ = xkb_context_new(XKB_CONTEXT_NO_FLAGS);

    if (connection.seat())
        wl_seat_add_listener(connection.seat(), &seatListener, this);
}

// ---------------------------------------------------------------------------
// WaylandToplevel - a decorated window of our own
// ---------------------------------------------------------------------------
// The standalone case. A plugin view is a SIBLING of this, not a mode inside it:
// it subsurfaces onto the host's wl_surface and never touches libdecor.
class WaylandToplevel : public WaylandFrameBase
{
public:
    explicit WaylandToplevel(Connection& connection) : WaylandFrameBase(connection) {}

    ~WaylandToplevel() override
    {
        if (frame_) libdecor_frame_unref(frame_);
        if (decor_) libdecor_unref(decor_);
    }

    bool create(const char* title, const char* appId, int w, int h);

    // The parent an xdg_popup must be given. In a plugin this is the host's,
    // from IWaylandFrame::getParentSurface() - which is why menus take it as an
    // argument instead of reaching for a global.
    xdg_surface* popupParent() const
    {
        return frame_ ? libdecor_frame_get_xdg_surface(frame_) : nullptr;
    }

    InputDispatch& input() { return input_; }

    bool running() const { return running_; }
    void close() { running_ = false; }

    // Fds the host's loop should poll. Standalone, runEventLoop does it for you;
    // in a plugin the host owns the loop and just needs these.
    int  displayFd() const { return libdecor_get_fd(decor_); }
    void dispatch()  { libdecor_dispatch(decor_, 0); }

    void runEventLoop(int tickMs, const std::function<void(int)>& onTick);

private:
    static void configure(libdecor_frame*, libdecor_configuration*, void*);
    static void closed(libdecor_frame*, void*);
    static void commit(libdecor_frame*, void*);
    static void dismissPopup(libdecor_frame*, const char*, void*) {}
    static void decorError(libdecor*, libdecor_error, const char* message);

    static void preferredScale(void* data, wp_fractional_scale_v1*, uint32_t scale120);
    static void frameDone(void* data, wl_callback* cb, uint32_t);

    void requestFrameCallback();

    libdecor*       decor_{};
    libdecor_frame* frame_{};
    wp_fractional_scale_v1* fracScale_{};
    InputDispatch   input_;
    bool            running_ = true;
    bool            configured_ = false;
};

inline void WaylandToplevel::decorError(libdecor*, libdecor_error, const char* message)
{
    fprintf(stderr, "libdecor: %s\n", message ? message : "?");
}

inline void WaylandToplevel::configure(libdecor_frame* frame,
                                       libdecor_configuration* configuration, void* data)
{
    auto& self = *static_cast<WaylandToplevel*>(data);

    int w = 0, h = 0;
    if (!libdecor_configuration_get_content_size(configuration, frame, &w, &h) || w <= 0 || h <= 0)
    {
        w = self.logicalW_;
        h = self.logicalH_;
    }

    self.setLogicalSize(w, h);

    libdecor_state* state = libdecor_state_new(w, h);
    libdecor_frame_commit(frame, state, configuration);
    libdecor_state_free(state);

    const bool first = !self.configured_;
    self.configured_ = true;

    self.present();
    if (first)
        self.requestFrameCallback();

    wl_surface_commit(self.surface_);
}

inline void WaylandToplevel::closed(libdecor_frame*, void* data)
{
    static_cast<WaylandToplevel*>(data)->running_ = false;
}

inline void WaylandToplevel::commit(libdecor_frame*, void* data)
{
    wl_surface_commit(static_cast<WaylandToplevel*>(data)->surface_);
}

inline void WaylandToplevel::preferredScale(void* data, wp_fractional_scale_v1*, uint32_t scale120)
{
    // 120ths, so 1.25x arrives as 150 - the protocol avoids floats on the wire
    static_cast<WaylandToplevel*>(data)->setScale(static_cast<float>(scale120) / 120.0f);
}

inline void WaylandToplevel::frameDone(void* data, wl_callback* cb, uint32_t)
{
    auto& self = *static_cast<WaylandToplevel*>(data);
    wl_callback_destroy(cb);

    if (self.hasPendingPaint())
        self.present();

    // A frame callback only takes effect on the NEXT commit. Requesting one and
    // not committing silently ends the chain and the window stops updating for
    // good, with no error anywhere - so the commit is unconditional.
    self.requestFrameCallback();
    wl_surface_commit(self.surface_);
}

inline void WaylandToplevel::requestFrameCallback()
{
    static const wl_callback_listener listener = { frameDone };
    wl_callback* cb = wl_surface_frame(surface_);
    wl_callback_add_listener(cb, &listener, this);
}

inline bool WaylandToplevel::create(const char* title, const char* appId, int w, int h)
{
    static libdecor_interface decorInterface = { decorError };
    static libdecor_frame_interface frameInterface = { configure, closed, commit, dismissPopup };
    static const wp_fractional_scale_v1_listener scaleListener = { preferredScale };

    logicalW_ = w;
    logicalH_ = h;

    createSurface();
    if (!surface_)
        return false;

    // GNOME does not do server-side decorations, so without libdecor the window
    // has no title bar, no border and no close button.
    decor_ = libdecor_new(connection_.display(), &decorInterface);
    if (!decor_)
        return false;

    frame_ = libdecor_decorate(decor_, surface_, &frameInterface, this);
    if (!frame_)
        return false;

    libdecor_frame_set_app_id(frame_, appId);
    libdecor_frame_set_title(frame_, title);
    libdecor_frame_map(frame_);

    if (connection_.fractionalScale())
    {
        fracScale_ = wp_fractional_scale_manager_v1_get_fractional_scale(
            connection_.fractionalScale(), surface_);
        wp_fractional_scale_v1_add_listener(fracScale_, &scaleListener, this);
    }

    input_.setSurface(surface_);
    input_.bindSeat(connection_);
    input_.onNeedsRedraw = [this] { invalidateRect(nullptr); };

    return true;
}

inline void WaylandToplevel::runEventLoop(int tickMs, const std::function<void(int)>& onTick)
{
    // Both of SynthEdit's timer frameworks are host-pumped, so the loop owes them a
    // tick. A timerfd in the same poll() as the display keeps that on one thread.
    const int timerFd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC | TFD_NONBLOCK);
    if (timerFd >= 0)
    {
        itimerspec spec{};
        spec.it_interval.tv_nsec = tickMs * 1000000L;
        spec.it_value.tv_nsec    = tickMs * 1000000L;
        timerfd_settime(timerFd, 0, &spec, nullptr);
    }

    while (running_)
    {
        wl_display_flush(connection_.display());

        pollfd fds[2]{};
        fds[0].fd = displayFd();
        fds[0].events = POLLIN;
        fds[1].fd = timerFd;
        fds[1].events = POLLIN;

        if (poll(fds, timerFd >= 0 ? 2 : 1, 100) < 0 && errno != EINTR)
            break;

        // libdecor owns the dispatch: it must see configure events before we do,
        // so it can size the decorations.
        if (libdecor_dispatch(decor_, 0) < 0)
            break;

        if (timerFd >= 0 && (fds[1].revents & POLLIN))
        {
            uint64_t expiries = 0;
            if (read(timerFd, &expiries, sizeof(expiries)) == sizeof(expiries) && onTick)
                onTick(static_cast<int>(expiries) * tickMs);
        }
    }

    if (timerFd >= 0)
        ::close(timerFd);   // ::, or this resolves to our own close() member
}

} // namespace wayland
} // namespace gmpi
