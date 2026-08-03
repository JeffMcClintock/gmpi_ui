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
#include <algorithm>
#include <utility>
#include <vector>

#include "backends/CpuGfx.h"
#include "backends/CpuEncode.h"
#include "helpers/NativeUi.h"
#include "RefCountMacros.h"
#include "backends/PortalFileDialog.h"

#include "xdg-shell-client-protocol.h"
#include "viewporter-client-protocol.h"
#include "fractional-scale-v1-client-protocol.h"
#include "cursor-shape-v1-client-protocol.h"
#include "xdg-foreign-unstable-v2-client-protocol.h"

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
// Anything holding compositor surfaces that MUST be gone before the connection
// is dropped. A client that vanishes while a popup grab is live makes mutter
// 46.2 crash or hang, taking the whole desktop session with it - so this is not
// tidiness, it is the difference between closing a plugin window and logging the
// user out.
struct IPopupTeardown
{
    virtual void closeSurfaces() = 0;
protected:
    ~IPopupTeardown() = default;
};

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

    // Popups register while they hold surfaces. Order matters: they are closed
    // topmost-first, which is what xdg-shell requires for nested popups.
    void registerPopup(IPopupTeardown* p)   { livePopups_.push_back(p); }
    void unregisterPopup(IPopupTeardown* p) { std::erase(livePopups_, p); }

    // Close every popup still standing and let the compositor process it. Safe to
    // call more than once; closeSurfaces() is idempotent.
    void closeLivePopups()
    {
        if (livePopups_.empty())
            return;

        auto pending = livePopups_;           // closeSurfaces() unregisters
        for (auto it = pending.rbegin(); it != pending.rend(); ++it)
            (*it)->closeSurfaces();
        livePopups_.clear();

        if (display_)
            wl_display_roundtrip(display_);   // do not race ahead of the destroys
    }

    ~Connection()
    {
        // Runs whether or not we own the display: an orphaned grabbing popup is
        // just as fatal on a host-owned connection.
        closeLivePopups();

        if (owned_ && display_)
            wl_display_disconnect(display_);
    }

    std::vector<IPopupTeardown*> livePopups_;

    wl_display*    display()    const { return display_; }
    wl_compositor* compositor() const { return compositor_; }
    wl_shm*        shm()        const { return shm_; }
    xdg_wm_base*   wmBase()     const { return wmBase_; }
    wl_seat*       seat()       const { return seat_; }
    wp_viewporter* viewporter() const { return viewporter_; }
    wp_fractional_scale_manager_v1* fractionalScale() const { return fracMgr_; }
    wp_cursor_shape_manager_v1*     cursorShape()     const { return cursorMgr_; }
    zxdg_exporter_v2*               exporter()        const { return exporter_; }

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
    zxdg_exporter_v2*               exporter_{};
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
    // xdg_foreign: exports a handle the portal can use as parent_window, so its
    // file chooser is modal to OUR window instead of floating unattached
    else if (!strcmp(iface, zxdg_exporter_v2_interface.name))
        c.exporter_ = static_cast<zxdg_exporter_v2*>(wl_registry_bind(reg, name, &zxdg_exporter_v2_interface, 1));
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

class InputDispatch;      // defined below; the frame only needs the name here
class WaylandPopupMenu;

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
class WaylandFrameBase : public WaylandHostBase, public gmpi::api::IDialogHost
{
public:
    explicit WaylandFrameBase(Connection& connection) : connection_(connection) {}

    // The xdg_surface popups hang off. A toplevel returns libdecor's; a plugin view
    // returns the HOST's, from VST3's IWaylandFrame::getParentSurface() - which is
    // why this is asked for rather than assumed.
    virtual xdg_surface* popupParent() const { return nullptr; }

    // Message boxes are windows of our own, so they need the app's libdecor
    // context: one context, many frames, one dispatch loop servicing them all.
    virtual libdecor* decorContext() const { return nullptr; }

    static void onExported(void* data, zxdg_exported_v2*, const char* handle)
    {
        auto& f = *static_cast<WaylandFrameBase*>(data);
        f.parentWindowHandle_ = std::string("wayland:") + (handle ? handle : "");
    }

    // Menus draw their own text, so the host needs a format. The app supplies it for
    // the same reason it supplies the text engine: no font code in this layer.
    void setMenuFont(gmpi::drawing::api::ITextFormat* font) { menuFont_ = font; }

    virtual InputDispatch& inputDispatch() = 0;

    PortalBus& portalBus() { return portalBus_; }

    // Hand the compositor's exported handle to the portal so its dialog is parented
    // to our window. Without it the chooser floats unattached and is not modal.
    void exportWindowHandle()
    {
        static const zxdg_exported_v2_listener listener = { onExported };
        if (exported_ || !connection_.exporter() || !surface_)
            return;
        exported_ = zxdg_exporter_v2_export_toplevel(connection_.exporter(), surface_);
        if (exported_)
            zxdg_exported_v2_add_listener(exported_, &listener, this);
    }

    // --- IDialogHost ---
    // defined after WaylandPopupMenu, which it constructs
    gmpi::ReturnCode createPopupMenu(const gmpi::drawing::Rect* r, gmpi::api::IUnknown** returnPopupMenu) override;

    // dialogType: 0 = open, 1 = save. Runs in the portal process over D-Bus;
    // Wayland has no file chooser of its own and a client may not place a window.
    gmpi::ReturnCode createFileDialog(int32_t dialogType, gmpi::api::IUnknown** returnDialog) override
    {
        *returnDialog = {};

        // A portal dialog takes the keyboard. Doing that while we hold a popup
        // grab leaves the compositor with a grab nobody is servicing, which looks
        // exactly like the desktop having frozen.
        connection_.closeLivePopups();

        auto* dlg = new PortalFileDialog(portalBus_, dialogType, parentWindowHandle_);
        *returnDialog = static_cast<gmpi::api::IFileDialog*>(dlg);
        return gmpi::ReturnCode::Ok;
    }

    // Still to come; each needs a drawn window of its own, since Wayland provides
    // no stock dialogs and the portal has no message-box API.
    gmpi::ReturnCode createTextEdit(const gmpi::drawing::Rect*, gmpi::api::IUnknown**) override
    { return gmpi::ReturnCode::NoSupport; }
    gmpi::ReturnCode createKeyListener(const gmpi::drawing::Rect*, gmpi::api::IUnknown**) override
    { return gmpi::ReturnCode::NoSupport; }
    // defined after WaylandStockDialog, which it constructs
    gmpi::ReturnCode createStockDialog(int32_t dialogType, const char* title, const char* text,
                                       gmpi::api::IUnknown** returnDialog) override;
    gmpi::ReturnCode createColorDialog(gmpi::drawing::Color, gmpi::api::IUnknown**) override
    { return gmpi::ReturnCode::NoSupport; }

    gmpi::ReturnCode queryInterface(const gmpi::api::Guid* iid, void** returnInterface) override
    {
        *returnInterface = {};
        GMPI_QUERYINTERFACE(gmpi::api::IDialogHost);
        return WaylandHostBase::queryInterface(iid, returnInterface);
    }
    GMPI_REFCOUNT_NO_DELETE;

    ~WaylandFrameBase() override
    {
        detachClient();
        if (exported_) zxdg_exported_v2_destroy(exported_);
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

    // The portal answers on the session bus, so its fd joins the same poll() as
    // wayland - a file dialog must never nest a modal loop here.
    PortalBus    portalBus_;
    zxdg_exported_v2* exported_{};
    std::string  parentWindowHandle_;   // "wayland:<handle>", empty until exported

    gmpi::api::IDrawingClient* client_{};
    gmpi::drawing::api::ITextFormat* menuFont_{};
    bool surfaceConfigured_ = false;

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
    // see WaylandPopupMenu::present - attaching before the first configure is a
    // protocol error and the compositor disconnects us for it
    if (!surfaceConfigured_ || !surface_ || !client_ || logicalW_ <= 0 || logicalH_ <= 0)
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
// There is exactly one wl_pointer and one wl_keyboard per seat, no matter how many
// surfaces a client owns, so events must be routed by the wl_surface they name.
// A popup registers itself and takes over while it is up.
struct IPointerTarget
{
    virtual ~IPointerTarget() = default;
    virtual void onEnter(double x, double y, uint32_t serial) = 0;
    virtual void onLeave() = 0;
    virtual void onMotion(double x, double y, int32_t modifiers) = 0;
    virtual void onButton(double x, double y, uint32_t button, bool pressed, uint32_t serial) = 0;
    virtual void onWheel(double x, double y, int32_t modifiers, double delta) = 0;
    virtual void onKey(uint32_t keysym, uint32_t utf32) = 0;
};
// Turns seat events into IInputClient calls. Pointer coordinates arrive in
// LOGICAL units and are handed on unscaled, because that is the space the client
// laid itself out in - scaling happens in the render transform, not here.
class InputDispatch
{
public:
    void attachInputClient(gmpi::api::IInputClient* client) { client_ = client; }

    // the frame's own surface; events naming it go to the IInputClient
    void setSurface(wl_surface* s) { surface_ = s; }

    void registerSurface(wl_surface* s, IPointerTarget* target)
    {
        if (s && target)
            targets_.emplace_back(s, target);
    }

    void unregisterSurface(wl_surface* s)
    {
        std::erase_if(targets_, [s](const auto& e) { return e.first == s; });
        if (focus_ == s)
            focus_ = nullptr;
    }

    void bindSeat(Connection& connection);

    // Serial of the last event that can START a grab - button press, key press,
    // touch down. NOT merely "the most recent serial": mutter grants
    // xdg_popup.grab only when the serial is exactly the seat's current implicit
    // grab serial, so enter/leave/motion/release serials are all rejected and the
    // popup is dismissed the instant it maps. Cursor-shape requests want the
    // enter serial instead and take it directly from the event.
    uint32_t lastGrabSerial() const { return lastGrabSerial_; }

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

    IPointerTarget* targetFor(wl_surface* s) const
    {
        for (const auto& e : targets_)
            if (e.first == s)
                return e.second;
        return nullptr;
    }

    gmpi::api::IInputClient* client_{};
    std::vector<std::pair<wl_surface*, IPointerTarget*>> targets_;
    wl_surface*  focus_{};   // surface the last enter named
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
    uint32_t lastGrabSerial_ = 0;
};

inline void InputDispatch::pointerEnter(void* data, wl_pointer*, uint32_t serial,
                                        wl_surface* surf, wl_fixed_t sx, wl_fixed_t sy)
{
    auto& in = *static_cast<InputDispatch*>(data);
    in.focus_ = surf;

    if (auto* t = in.targetFor(surf))
    {
        t->onEnter(wl_fixed_to_double(sx), wl_fixed_to_double(sy), serial);
        return;
    }

    if (surf != in.surface_)
        return;

    in.inside_ = true;
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

    if (auto* t = in.targetFor(surf))
    {
        t->onLeave();
        if (in.focus_ == surf)
            in.focus_ = nullptr;
        return;
    }

    if (surf != in.surface_)
        return;

    in.inside_ = false;

    if (in.client_)
        in.client_->setHover(false);
    if (in.onNeedsRedraw)
        in.onNeedsRedraw();
}

inline void InputDispatch::pointerMotion(void* data, wl_pointer*, uint32_t,
                                         wl_fixed_t sx, wl_fixed_t sy)
{
    auto& in = *static_cast<InputDispatch*>(data);

    if (auto* t = in.targetFor(in.focus_))
    {
        t->onMotion(wl_fixed_to_double(sx), wl_fixed_to_double(sy), in.modifierFlags());
        return;
    }

    in.x_ = wl_fixed_to_double(sx);
    in.y_ = wl_fixed_to_double(sy);

    if (in.client_)
        in.client_->onPointerMove(in.pointerPos(), in.modifierFlags() | int32_t(in.buttons_));
}

inline void InputDispatch::pointerButton(void* data, wl_pointer*, uint32_t serial,
                                         uint32_t, uint32_t button, uint32_t state)
{
    auto& in = *static_cast<InputDispatch*>(data);
    if (state == WL_POINTER_BUTTON_STATE_PRESSED)
        in.lastGrabSerial_ = serial;

    if (auto* t = in.targetFor(in.focus_))
    {
        t->onButton(in.x_, in.y_, button, state == WL_POINTER_BUTTON_STATE_PRESSED, serial);
        return;
    }

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
    if (state == WL_KEYBOARD_KEY_STATE_PRESSED)
        in.lastGrabSerial_ = serial;

    if (!in.xkbState_ || state != WL_KEYBOARD_KEY_STATE_PRESSED)
        return;
    if (!in.client_ && !in.targetFor(in.focus_))
        return;

    // wayland reports evdev keycodes; xkb numbers them 8 higher
    const xkb_keycode_t code = key + 8;
    const uint32_t utf32 = xkb_state_key_get_utf32(in.xkbState_, code);

    // a popup holds the grab while it is up, so it sees the keys
    if (auto* t = in.targetFor(in.focus_))
    {
        t->onKey(xkb_state_key_get_one_sym(in.xkbState_, code), utf32);
        return;
    }

    if (utf32)
        in.client_->onKeyPress(static_cast<wchar_t>(utf32));
}

inline void InputDispatch::keyboardEnter(void* data, wl_keyboard*, uint32_t, wl_surface* surf, wl_array*)
{
    // a grabbing popup gets keyboard focus; remember which surface so keys route there
    auto& in = *static_cast<InputDispatch*>(data);
    if (in.targetFor(surf))
        in.focus_ = surf;
}
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
// WaylandPopupMenu - IPopupMenu on xdg_popup
// ---------------------------------------------------------------------------
// Wayland supplies a positioned, grabbing surface and nothing else: there is no
// menu widget, so the items are drawn here. GTK and Qt do the same.
//
// Items arrive FLAT through IContextItemSink::addItem, with SubMenuBegin /
// SubMenuEnd markers delimiting nesting and each item carrying its own
// IPopupMenuCallback - so the first job is rebuilding a tree from that stream.
class WaylandPopupMenu : public gmpi::api::IPopupMenu, public IPointerTarget,
                         public IPopupTeardown
{
public:
    struct Item
    {
        std::string label;
        int32_t     id{};
        bool        separator{};
        bool        ticked{};
        bool        grayed{};
        std::vector<Item> submenu;              // non-empty => opens a child popup
        gmpi::shared_ptr<gmpi::api::IUnknown> callback;
    };

    WaylandPopupMenu(Connection& connection, InputDispatch& input,
                     xdg_surface* parentXdg, gmpi::cpugfx::Factory& factory,
                     gmpi::drawing::api::ITextFormat* font,
                     gmpi::drawing::Rect anchor)
        : connection_(connection), input_(input), parentXdg_(parentXdg),
          factory_(factory), font_(font), anchor_(anchor)
    {
        stack_.push_back(&root_);
    }

    ~WaylandPopupMenu() override { destroySurfaces(); }

    // IPopupTeardown: drop compositor surfaces without destroying the object,
    // which the connection may do while the client still holds a reference.
    void closeSurfaces() override { destroySurfaces(); }

    // --- IContextItemSink / IPopupMenu ---
    gmpi::ReturnCode addItem(const char* text, int32_t id, int32_t flags,
                             gmpi::api::IUnknown* itemCallback) override;
    gmpi::ReturnCode setAlignment(int32_t) override { return gmpi::ReturnCode::Ok; }
    gmpi::ReturnCode showAsync() override;

    // --- IPointerTarget (the popup's own surface) ---
    void onEnter(double x, double y, uint32_t serial) override;
    void onLeave() override;
    void onMotion(double x, double y, int32_t modifiers) override;
    void onButton(double x, double y, uint32_t button, bool pressed, uint32_t serial) override;
    void onWheel(double, double, int32_t, double) override {}
    void onKey(uint32_t keysym, uint32_t) override;

    gmpi::ReturnCode queryInterface(const gmpi::api::Guid* iid, void** returnInterface) override
    {
        *returnInterface = {};
        GMPI_QUERYINTERFACE(gmpi::api::IPopupMenu);
        GMPI_QUERYINTERFACE(gmpi::api::IContextItemSink);
        return gmpi::ReturnCode::NoSupport;
    }
    GMPI_REFCOUNT;

    // exposed for headless tests: the tree rebuilt from the flat stream
    const std::vector<Item>& items() const { return root_; }

    // True once the menu has completed or been dismissed. The distinction that
    // matters: clicking a submenu header must NOT dismiss, clicking an item must.
    bool isDismissed() const { return dismissed_; }

private:
    void  present();
    void  destroySurfaces();
    void  closeChild();
    void  openChildFor(int index);
    void  onChildDismissed(WaylandPopupMenu* c);
    WaylandPopupMenu* rootMenu();
    void  choose(Item& item);
    void  dismiss();
    int   itemAt(double y) const;
    int   itemTop(int index) const;
    int   measuredWidth() const;
    int   measuredHeight() const;

    static void popupConfigure(void*, xdg_popup*, int32_t, int32_t, int32_t, int32_t) {}
    static void popupDone(void* data, xdg_popup*);
    static void popupRepositioned(void*, xdg_popup*, uint32_t) {}
    static void surfaceConfigure(void* data, xdg_surface* s, uint32_t serial);

    Connection&    connection_;
    InputDispatch& input_;
    xdg_surface*   parentXdg_{};
    gmpi::cpugfx::Factory& factory_;
    gmpi::drawing::api::ITextFormat* font_{};
    gmpi::drawing::Rect anchor_{};

    std::vector<Item>   root_;
    std::vector<std::vector<Item>*> stack_;
    bool pendingSeparator_ = false;   // deferred, so leading/doubled ones vanish

    wl_surface*  surface_{};
    xdg_surface* xdgSurface_{};
    xdg_popup*   popup_{};
    ShmBuffer    buffer_;
    int  hovered_ = -1;
    // A submenu is a child xdg_popup anchored to its parent item. The chain is
    // non-owning in both directions: a shown popup keeps itself alive (showAsync
    // takes a reference it drops in dismiss), so these are links, not ownership.
    WaylandPopupMenu* child_{};
    WaylandPopupMenu* parentMenu_{};
    int  childItem_ = -1;
    bool isSubmenu_ = false;
    bool dismissed_ = false;
    bool configured_ = false;   // no buffer may be attached before the first configure
};

constexpr int kMenuItemHeight = 24;
constexpr int kMenuSeparatorHeight = 9;
constexpr int kMenuTickGutter = 26;
constexpr int kMenuArrowGutter = 22;

inline gmpi::ReturnCode WaylandPopupMenu::addItem(const char* text, int32_t id, int32_t flags,
                                                  gmpi::api::IUnknown* itemCallback)
{
    using F = gmpi::api::PopupMenuFlags;

    const bool subBegin = (flags & int32_t(F::SubMenuBegin)) != 0;
    const bool subEnd   = (flags & int32_t(F::SubMenuEnd)) != 0;
    const bool separator = (flags & (int32_t(F::Separator) | int32_t(F::Break))) != 0;

    auto strip = [](const char* s)
    {
        // '&' marks a mnemonic; no platform here renders one, so drop it as the
        // Win32, Cocoa and JUCE backends all do
        std::string r;
        for (const char* p = s ? s : ""; *p; ++p)
            if (*p != '&') r += *p;
        return r;
    };

    if (subBegin)
    {
        stack_.back()->push_back(Item{ strip(text), id });
        stack_.push_back(&stack_.back()->back().submenu);
        pendingSeparator_ = false;
        return gmpi::ReturnCode::Ok;
    }

    if (subEnd)
    {
        if (stack_.size() > 1)
            stack_.pop_back();
        pendingSeparator_ = false;
        return gmpi::ReturnCode::Ok;
    }

    if (separator)
    {
        pendingSeparator_ = true;   // committed only if a real item follows
        return gmpi::ReturnCode::Ok;
    }

    if (pendingSeparator_)
    {
        if (!stack_.back()->empty())
            stack_.back()->push_back(Item{ {}, 0, true });
        pendingSeparator_ = false;
    }

    Item item{ strip(text), id };
    item.ticked = (flags & int32_t(F::Ticked)) != 0;
    item.grayed = (flags & int32_t(F::Grayed)) != 0;
    item.callback = itemCallback;   // operator= addRefs; the T* ctor does not

    stack_.back()->push_back(std::move(item));
    return gmpi::ReturnCode::Ok;
}

inline int WaylandPopupMenu::itemTop(int index) const
{
    int top = 4;
    for (int i = 0; i < index && i < (int)root_.size(); ++i)
        top += root_[i].separator ? kMenuSeparatorHeight : kMenuItemHeight;
    return top;
}

inline int WaylandPopupMenu::itemAt(double y) const
{
    int top = 4;
    for (size_t i = 0; i < root_.size(); ++i)
    {
        const int h = root_[i].separator ? kMenuSeparatorHeight : kMenuItemHeight;
        if (y >= top && y < top + h)
            return root_[i].separator ? -1 : int(i);
        top += h;
    }
    return -1;
}

inline int WaylandPopupMenu::measuredHeight() const
{
    int h = 8;
    for (const auto& i : root_)
        h += i.separator ? kMenuSeparatorHeight : kMenuItemHeight;
    return h;
}

inline int WaylandPopupMenu::measuredWidth() const
{
    float widest = 90.0f;

    for (const auto& i : root_)
    {
        if (i.separator || !font_)
            continue;

        gmpi::drawing::Size sz{};
        font_->getTextExtentU(i.label.c_str(), int32_t(i.label.size()), 10000.0f, &sz);
        widest = (std::max)(widest, sz.width);
    }

    return int(widest) + kMenuTickGutter + kMenuArrowGutter;
}

inline void WaylandPopupMenu::present()
{
    // Attaching a buffer to an xdg_surface that has not been configured is
    // xdg_surface.error.unconfigured_buffer - a PROTOCOL ERROR, for which the
    // compositor disconnects the client. That is fatal in the worst way here,
    // because the client dies holding a popup grab and mutter can leak it, which
    // looks to the user like the whole desktop has frozen.
    //
    // The race is real and easy to lose: a popup appears under the cursor, so the
    // compositor may deliver pointer enter before the configure arrives, and enter
    // wants to repaint the hover highlight.
    if (!configured_)
        return;

    const int w = measuredWidth();
    const int h = measuredHeight();
    if (w <= 0 || h <= 0 || !surface_)
        return;

    if (!buffer_.matches(w, h) && !buffer_.create(connection_.shm(), w, h))
        return;

    gmpi::drawing::api::IBitmapRenderTarget* rtRaw{};
    factory_.createCpuRenderTarget({ uint32_t(w), uint32_t(h) }, 0, &rtRaw, 96.0f);
    auto* rt = dynamic_cast<gmpi::cpugfx::RenderTarget*>(rtRaw);
    if (!rt) { if (rtRaw) rtRaw->release(); return; }

    rt->beginDraw();

    auto brush = [&](gmpi::drawing::Color c)
    {
        gmpi::drawing::api::ISolidColorBrush* b{};
        rt->createSolidColorBrush(&c, nullptr, &b);
        return b;
    };

    const gmpi::drawing::Color bg{ 0.16f, 0.17f, 0.19f, 1.0f };
    rt->clear(&bg);

    auto* ink    = brush({ 0.92f, 0.93f, 0.95f, 1.0f });
    auto* grey   = brush({ 0.45f, 0.46f, 0.50f, 1.0f });
    auto* hilite = brush({ 0.16f, 0.42f, 0.75f, 1.0f });
    auto* rule   = brush({ 0.28f, 0.29f, 0.32f, 1.0f });

    for (size_t i = 0; i < root_.size(); ++i)
    {
        const auto& item = root_[i];
        const float top = float(itemTop(int(i)));

        if (item.separator)
        {
            if (rule)
            {
                const gmpi::drawing::Rect r{ 6.f, top + kMenuSeparatorHeight * 0.5f,
                                             float(w) - 6.f, top + kMenuSeparatorHeight * 0.5f + 1.f };
                rt->fillRectangle(&r, rule);
            }
            continue;
        }

        if (int(i) == hovered_ && !item.grayed && hilite)
        {
            const gmpi::drawing::Rect r{ 3.f, top, float(w) - 3.f, top + kMenuItemHeight };
            rt->fillRectangle(&r, hilite);
        }

        auto* fg = item.grayed ? grey : ink;
        if (!fg || !font_)
            continue;

        if (item.ticked)
        {
            const char* tick = "\xe2\x9c\x93";
            const gmpi::drawing::Rect r{ 8.f, top + 3.f, 26.f, top + kMenuItemHeight };
            rt->drawTextU(tick, 3, font_, &r, fg, 0);
        }

        const gmpi::drawing::Rect r{ float(kMenuTickGutter) + 2.f, top + 3.f,
                                     float(w) - kMenuArrowGutter, top + kMenuItemHeight };
        rt->drawTextU(item.label.c_str(), uint32_t(item.label.size()), font_, &r, fg, 0);

        if (!item.submenu.empty())
        {
            const char* arrow = "\xe2\x80\xba";
            const gmpi::drawing::Rect a{ float(w) - 20.f, top + 3.f, float(w) - 6.f, top + kMenuItemHeight };
            rt->drawTextU(arrow, 3, font_, &a, fg, 0);
        }
    }

    if (ink)    ink->release();
    if (grey)   grey->release();
    if (hilite) hilite->release();
    if (rule)   rule->release();

    rt->endDraw();

    gmpi::drawing::api::IBitmap* bmRaw{};
    rt->getBitmap(&bmRaw);
    if (auto* bm = dynamic_cast<gmpi::cpugfx::Bitmap*>(bmRaw))
    {
        const auto& s = bm->surface;
        const gmpi::cpugfx::SourceSurface src{ s.pixels, s.stridePixels, s.width, s.height };
        const gmpi::cpugfx::DestSurface   dst{ buffer_.pixels(), buffer_.stride(), w, h,
                                               gmpi::cpugfx::PixelEncoding::Bgra8888 };
        gmpi::cpugfx::encodeDirtyRect(src, dst, gmpi::drawing::RectL{ 0, 0, w, h });
    }
    if (bmRaw) bmRaw->release();
    if (rtRaw) rtRaw->release();

    wl_surface_attach(surface_, buffer_.buffer(), 0, 0);
    wl_surface_damage_buffer(surface_, 0, 0, w, h);
    wl_surface_commit(surface_);
}

inline void WaylandPopupMenu::surfaceConfigure(void* data, xdg_surface* s, uint32_t serial)
{
    auto& self = *static_cast<WaylandPopupMenu*>(data);
    xdg_surface_ack_configure(s, serial);
    self.configured_ = true;
    self.present();
}

inline void WaylandPopupMenu::popupDone(void* data, xdg_popup*)
{
    // dismissed by the compositor: clicked away, Escape, focus lost
    static_cast<WaylandPopupMenu*>(data)->dismiss();
}

inline gmpi::ReturnCode WaylandPopupMenu::showAsync()
{
    static const xdg_popup_listener popupListener = { popupConfigure, popupDone, popupRepositioned };
    static const xdg_surface_listener surfaceListener = { surfaceConfigure };

    if (!parentXdg_ || !connection_.wmBase() || !connection_.compositor())
        return gmpi::ReturnCode::Fail;

    // Stay alive until the menu completes, independent of the caller's reference -
    // same contract as the JUCE backend.
    addRef();

    surface_    = wl_compositor_create_surface(connection_.compositor());
    xdgSurface_ = xdg_wm_base_get_xdg_surface(connection_.wmBase(), surface_);
    xdg_surface_add_listener(xdgSurface_, &surfaceListener, this);

    const int w = measuredWidth();
    const int h = measuredHeight();

    xdg_positioner* pos = xdg_wm_base_create_positioner(connection_.wmBase());
    xdg_positioner_set_size(pos, w, h);
    // anchor rect in the PARENT surface's coordinates
    xdg_positioner_set_anchor_rect(pos, int32_t(anchor_.left), int32_t(anchor_.top),
                                   (std::max)(1, int32_t(anchor_.right - anchor_.left)),
                                   (std::max)(1, int32_t(anchor_.bottom - anchor_.top)));
    // a context menu drops from the click; a submenu flies out beside its item
    xdg_positioner_set_anchor(pos, isSubmenu_ ? XDG_POSITIONER_ANCHOR_TOP_RIGHT
                                              : XDG_POSITIONER_ANCHOR_BOTTOM_LEFT);
    xdg_positioner_set_gravity(pos, XDG_POSITIONER_GRAVITY_BOTTOM_RIGHT);
    // the compositor keeps it on screen; a client cannot, having no idea where its
    // own window is
    xdg_positioner_set_constraint_adjustment(pos,
        XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_FLIP_X |
        XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_FLIP_Y |
        XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_SLIDE_X |
        XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_SLIDE_Y);

    popup_ = xdg_surface_get_popup(xdgSurface_, parentXdg_, pos);

    // From here we hold compositor surfaces (and shortly a grab): the connection
    // must not be dropped without closing us first.
    connection_.registerPopup(this);
    xdg_positioner_destroy(pos);
    xdg_popup_add_listener(popup_, &popupListener, this);

    // Must be a real user-action serial: the compositor refuses the grab otherwise
    // and dismisses the popup immediately.
    xdg_popup_grab(popup_, connection_.seat(), input_.lastGrabSerial());

    input_.registerSurface(surface_, this);
    wl_surface_commit(surface_);

    return gmpi::ReturnCode::Ok;
}

inline void WaylandPopupMenu::onEnter(double, double y, uint32_t)
{
    hovered_ = itemAt(y);
    present();
}

inline void WaylandPopupMenu::onLeave()
{
    // The pointer leaving us for our own submenu is not "no item hovered" - the
    // parent item stays highlighted for as long as its child is up.
    if (child_)
        return;

    hovered_ = -1;
    present();
}

inline void WaylandPopupMenu::onMotion(double, double y, int32_t)
{
    const int was = hovered_;
    hovered_ = itemAt(y);
    if (hovered_ == was)
        return;

    // moving onto a different item closes the old submenu and opens the new one;
    // openChildFor() closes any existing child first, including for a plain item
    if (hovered_ != childItem_)
        openChildFor(hovered_);

    present();
}

inline void WaylandPopupMenu::onButton(double, double y, uint32_t, bool pressed, uint32_t)
{
    if (pressed)
        return;

    const int index = itemAt(y);

    // A submenu header has no id of its own: clicking it opens (or keeps) the
    // child rather than completing the menu with a meaningless value.
    if (index >= 0 && !root_[index].submenu.empty())
    {
        if (childItem_ != index)
            openChildFor(index);
        return;
    }

    if (index >= 0 && !root_[index].grayed)
        choose(root_[index]);
    else
        dismiss();
}

inline void WaylandPopupMenu::onKey(uint32_t keysym, uint32_t)
{
    if (keysym == 0xff1b) // XKB_KEY_Escape
    {
        // close the innermost level first, as every other menu on every platform does
        if (child_)
            closeChild();
        else
            dismiss();
    }
}

inline void WaylandPopupMenu::choose(Item& item)
{
    if (auto cb = item.callback.as<gmpi::api::IPopupMenuCallback>(); cb)
        cb->onComplete(gmpi::ReturnCode::Ok, item.id);

    // Picking from a submenu closes the entire menu, not just that level. Dismiss
    // from the root so teardown runs top-down; `this` may be gone after it, so
    // touch nothing below.
    rootMenu()->dismiss();
}

inline WaylandPopupMenu* WaylandPopupMenu::rootMenu()
{
    auto* m = this;
    while (m->parentMenu_)
        m = m->parentMenu_;
    return m;
}

inline void WaylandPopupMenu::closeChild()
{
    // Copy the pointer and clear the link BEFORE dismissing: dismiss() can run
    // this object's destructor via release(), and a callback back into us must
    // not find a dangling child_.
    auto* c = child_;
    child_ = nullptr;
    childItem_ = -1;

    if (c)
        c->dismiss();       // recurses: the deepest popup goes first
}

inline void WaylandPopupMenu::onChildDismissed(WaylandPopupMenu* c)
{
    if (child_ == c)
    {
        child_ = nullptr;
        childItem_ = -1;
    }
}

inline void WaylandPopupMenu::openChildFor(int index)
{
    closeChild();

    if (index < 0 || index >= (int)root_.size() || root_[index].submenu.empty())
        return;
    if (!xdgSurface_)
        return;   // we are not mapped; a popup parent must be a live xdg_surface

    // anchor rect is in OUR surface coordinates: the right edge of the item
    const int w   = measuredWidth();
    const int top = itemTop(index);
    const gmpi::drawing::Rect itemRect{ float(w - 6), float(top),
                                        float(w),     float(top + kMenuItemHeight) };

    auto* c = new WaylandPopupMenu(connection_, input_, xdgSurface_, factory_, font_, itemRect);
    c->root_       = root_[index].submenu;
    c->parentMenu_ = this;
    c->isSubmenu_  = true;

    child_     = c;
    childItem_ = index;

    if (c->showAsync() != gmpi::ReturnCode::Ok)
    {
        child_ = nullptr;
        childItem_ = -1;
    }

    c->release();   // showAsync took its own reference; drop the construction one
}

inline void WaylandPopupMenu::dismiss()
{
    if (dismissed_)
        return;
    dismissed_ = true;

    closeChild();               // nested popups must be destroyed topmost-first

    if (parentMenu_)
    {
        parentMenu_->onChildDismissed(this);
        parentMenu_ = nullptr;
    }

    destroySurfaces();
    release();   // balances the addRef in showAsync
}

inline void WaylandPopupMenu::destroySurfaces()
{
    connection_.unregisterPopup(this);

    if (surface_)
        input_.unregisterSurface(surface_);

    buffer_.release();

    // nested popups must go topmost-first; this class owns only its own
    if (popup_)      { xdg_popup_destroy(popup_);      popup_ = nullptr; }
    if (xdgSurface_) { xdg_surface_destroy(xdgSurface_); xdgSurface_ = nullptr; }
    if (surface_)    { wl_surface_destroy(surface_);   surface_ = nullptr; }
}


inline gmpi::ReturnCode WaylandFrameBase::createPopupMenu(const gmpi::drawing::Rect* r,
                                                          gmpi::api::IUnknown** returnPopupMenu)
{
    *returnPopupMenu = {};

    if (!popupParent())
        return gmpi::ReturnCode::Fail;

    auto* menu = new WaylandPopupMenu(connection_, inputDispatch(), popupParent(),
                                      factory_, menuFont_, r ? *r : gmpi::drawing::Rect{});
    *returnPopupMenu = static_cast<gmpi::api::IPopupMenu*>(menu);
    return gmpi::ReturnCode::Ok;
}

// ---------------------------------------------------------------------------
// WaylandStockDialog - IStockDialog, drawn by us
// ---------------------------------------------------------------------------
// There is nothing to call. Wayland has no stock dialogs, and the FileChooser
// portal - which is how we get a file picker - has no message-box equivalent.
// So a message box is a real toplevel of our own, drawn with the same cpugfx
// path as the menus, sharing the app's libdecor context so one dispatch loop
// services every window.
class WaylandStockDialog : public gmpi::api::IStockDialog, public IPointerTarget
{
public:
    WaylandStockDialog(Connection& connection, InputDispatch& input, libdecor* decor,
                       gmpi::cpugfx::Factory& factory, gmpi::drawing::api::ITextFormat* font,
                       int32_t dialogType, std::string title, std::string text)
        : connection_(connection), input_(input), decor_(decor), factory_(factory),
          font_(font), title_(std::move(title)), text_(std::move(text))
    {
        using T = gmpi::api::StockDialogType;
        using B = gmpi::api::StockDialogButton;

        switch (static_cast<T>(dialogType))
        {
        case T::OkCancel:    buttons_ = { { "OK", B::Ok }, { "Cancel", B::Cancel } };
                             escape_ = B::Cancel; break;
        case T::YesNo:       buttons_ = { { "Yes", B::Yes }, { "No", B::No } };
                             escape_ = B::No; break;
        case T::YesNoCancel: buttons_ = { { "Yes", B::Yes }, { "No", B::No }, { "Cancel", B::Cancel } };
                             escape_ = B::Cancel; break;
        case T::Ok:
        default:             buttons_ = { { "OK", B::Ok } };
                             escape_ = B::Ok; break;
        }

        layout();   // geometry up front, so it is valid (and inspectable) before mapping
    }

    ~WaylandStockDialog() override { destroySurfaces(); }

    gmpi::ReturnCode showAsync(gmpi::api::IUnknown* callback) override;

    // --- IPointerTarget ---
    void onEnter(double x, double y, uint32_t) override { hovered_ = buttonAt(x, y); present(); }
    void onLeave() override { hovered_ = -1; present(); }
    void onMotion(double x, double y, int32_t) override
    {
        const int was = hovered_;
        hovered_ = buttonAt(x, y);
        if (hovered_ != was)
            present();
    }
    void onButton(double x, double y, uint32_t, bool pressed, uint32_t) override
    {
        if (pressed)
            return;
        const int i = buttonAt(x, y);
        if (i >= 0)
            complete(buttons_[size_t(i)].id);
    }
    void onWheel(double, double, int32_t, double) override {}
    void onKey(uint32_t keysym, uint32_t) override
    {
        if (keysym == 0xff1b)                              // Escape
            complete(escape_);
        else if (keysym == 0xff0d || keysym == 0xff8d)     // Return, KP_Enter
            complete(buttons_.empty() ? gmpi::api::StockDialogButton::Ok : buttons_.front().id);
    }

    gmpi::ReturnCode queryInterface(const gmpi::api::Guid* iid, void** returnInterface) override
    {
        *returnInterface = {};
        GMPI_QUERYINTERFACE(gmpi::api::IStockDialog);
        return gmpi::ReturnCode::NoSupport;
    }
    GMPI_REFCOUNT;

    struct Button
    {
        std::string label;
        gmpi::api::StockDialogButton id;
        gmpi::drawing::Rect rect{};
    };

    // exposed for headless tests: which buttons this dialog type offers, where
    // they sit, and what Escape maps to
    const std::vector<Button>& buttons() const { return buttons_; }
    gmpi::api::StockDialogButton escapeButton() const { return escape_; }
    int contentWidth()  const { return w_; }
    int contentHeight() const { return h_; }

private:
    void layout();
    void present();
    void complete(gmpi::api::StockDialogButton b);
    void destroySurfaces();
    int  buttonAt(double x, double y) const;

    static void configure(libdecor_frame* frame, libdecor_configuration* configuration, void* data);
    static void closed(libdecor_frame*, void* data)
    {
        // the window manager's close button is a cancel, not a silent disappearance
        auto& d = *static_cast<WaylandStockDialog*>(data);
        d.complete(d.escape_);
    }
    static void commitCb(libdecor_frame*, void* data)
    {
        wl_surface_commit(static_cast<WaylandStockDialog*>(data)->surface_);
    }
    static void dismissPopup(libdecor_frame*, const char*, void*) {}

    Connection&    connection_;
    InputDispatch& input_;
    libdecor*      decor_{};
    gmpi::cpugfx::Factory& factory_;
    gmpi::drawing::api::ITextFormat* font_{};

    std::string title_, text_;
    std::vector<Button> buttons_;
    gmpi::api::StockDialogButton escape_ = gmpi::api::StockDialogButton::Ok;

    wl_surface*     surface_{};
    libdecor_frame* frame_{};
    ShmBuffer       buffer_;

    int  w_ = 420, h_ = 150;
    int  hovered_ = -1;
    bool configured_ = false;
    bool completed_  = false;

    gmpi::shared_ptr<gmpi::api::IUnknown> callback_;
};

constexpr int kDialogMargin     = 20;
constexpr int kDialogButtonW    = 92;
constexpr int kDialogButtonH    = 30;
constexpr int kDialogButtonGap  = 10;
constexpr int kDialogTextWidth  = 380;   // content width before margins

inline void WaylandStockDialog::layout()
{
    // Measure the message so a long one gets a taller box rather than a clipped
    // one. Without a font we cannot measure, so fall back to a fixed height.
    float textH = 40.0f;
    if (font_)
    {
        gmpi::drawing::Size sz{};
        if (font_->getTextExtentU(text_.c_str(), int32_t(text_.size()),
                                  float(kDialogTextWidth), &sz) == gmpi::ReturnCode::Ok)
            textH = (std::max)(20.0f, sz.height);
    }

    w_ = kDialogTextWidth + 2 * kDialogMargin;
    h_ = kDialogMargin + int(textH) + 24 + kDialogButtonH + kDialogMargin;

    // buttons right-aligned along the bottom, first one rightmost so the default
    // sits where the eye lands
    int x = w_ - kDialogMargin;
    const int y = h_ - kDialogMargin - kDialogButtonH;
    for (auto& b : buttons_)
    {
        x -= kDialogButtonW;
        b.rect = { float(x), float(y), float(x + kDialogButtonW), float(y + kDialogButtonH) };
        x -= kDialogButtonGap;
    }
}

inline int WaylandStockDialog::buttonAt(double x, double y) const
{
    for (size_t i = 0; i < buttons_.size(); ++i)
    {
        const auto& r = buttons_[i].rect;
        if (x >= r.left && x < r.right && y >= r.top && y < r.bottom)
            return int(i);
    }
    return -1;
}

inline gmpi::ReturnCode WaylandStockDialog::showAsync(gmpi::api::IUnknown* callback)
{
    static libdecor_frame_interface frameInterface = { configure, closed, commitCb, dismissPopup };

    if (!decor_ || !connection_.compositor())
        return gmpi::ReturnCode::Fail;

    // A message box takes the keyboard; leaving a popup grab up while it does
    // strands the compositor with a grab nobody services.
    connection_.closeLivePopups();

    layout();

    surface_ = wl_compositor_create_surface(connection_.compositor());
    if (!surface_)
        return gmpi::ReturnCode::Fail;

    frame_ = libdecor_decorate(decor_, surface_, &frameInterface, this);
    if (!frame_)
    {
        wl_surface_destroy(surface_);
        surface_ = nullptr;
        return gmpi::ReturnCode::Fail;
    }

    libdecor_frame_set_title(frame_, title_.c_str());
    libdecor_frame_map(frame_);

    input_.registerSurface(surface_, this);
    callback_ = callback;

    // stay alive until the user answers, independent of the caller's reference
    addRef();
    return gmpi::ReturnCode::Ok;
}

inline void WaylandStockDialog::configure(libdecor_frame* frame,
                                          libdecor_configuration* configuration, void* data)
{
    auto& d = *static_cast<WaylandStockDialog*>(data);

    int w = 0, h = 0;
    if (!libdecor_configuration_get_content_size(configuration, frame, &w, &h) || w <= 0 || h <= 0)
    {
        w = d.w_;
        h = d.h_;
    }

    libdecor_state* state = libdecor_state_new(w, h);
    libdecor_frame_commit(frame, state, configuration);
    libdecor_state_free(state);

    d.configured_ = true;   // only now may a buffer be attached
    d.present();
    wl_surface_commit(d.surface_);
}

inline void WaylandStockDialog::present()
{
    // attaching before the first configure is a protocol error that disconnects us
    if (!configured_ || !surface_ || w_ <= 0 || h_ <= 0)
        return;

    if (!buffer_.matches(w_, h_) && !buffer_.create(connection_.shm(), w_, h_))
        return;

    gmpi::drawing::api::IBitmapRenderTarget* rtRaw{};
    factory_.createCpuRenderTarget({ uint32_t(w_), uint32_t(h_) }, 0, &rtRaw, 96.0f);
    auto* rt = dynamic_cast<gmpi::cpugfx::RenderTarget*>(rtRaw);
    if (!rt) { if (rtRaw) rtRaw->release(); return; }

    rt->beginDraw();

    auto brush = [&](gmpi::drawing::Color c)
    {
        gmpi::drawing::api::ISolidColorBrush* b{};
        rt->createSolidColorBrush(&c, nullptr, &b);
        return b;
    };

    const gmpi::drawing::Color bg{ 0.16f, 0.17f, 0.19f, 1.0f };
    rt->clear(&bg);

    auto* ink      = brush({ 0.92f, 0.93f, 0.95f, 1.0f });
    auto* face     = brush({ 0.24f, 0.25f, 0.28f, 1.0f });
    auto* faceHot  = brush({ 0.30f, 0.44f, 0.68f, 1.0f });
    auto* edge     = brush({ 0.38f, 0.39f, 0.43f, 1.0f });

    if (font_ && ink)
    {
        const gmpi::drawing::Rect tr{ float(kDialogMargin), float(kDialogMargin),
                                      float(kDialogMargin + kDialogTextWidth),
                                      float(h_ - kDialogMargin - kDialogButtonH - 12) };
        rt->drawTextU(text_.c_str(), uint32_t(text_.size()), font_, &tr, ink, 0);
    }

    for (size_t i = 0; i < buttons_.size(); ++i)
    {
        const auto& b = buttons_[i];
        if (auto* fill = (int(i) == hovered_) ? faceHot : face; fill)
            rt->fillRectangle(&b.rect, fill);
        if (edge)
            rt->drawRectangle(&b.rect, edge, 1.0f, nullptr);

        if (font_ && ink)
        {
            // nudged down so the glyphs sit optically centred in the button
            const gmpi::drawing::Rect lr{ b.rect.left, b.rect.top + 6.f,
                                          b.rect.right, b.rect.bottom };
            rt->drawTextU(b.label.c_str(), uint32_t(b.label.size()), font_, &lr, ink, 0);
        }
    }

    if (ink)     ink->release();
    if (face)    face->release();
    if (faceHot) faceHot->release();
    if (edge)    edge->release();

    rt->endDraw();

    gmpi::drawing::api::IBitmap* bmRaw{};
    rt->getBitmap(&bmRaw);
    if (auto* bm = dynamic_cast<gmpi::cpugfx::Bitmap*>(bmRaw))
    {
        const auto& s = bm->surface;
        const gmpi::cpugfx::SourceSurface src{ s.pixels, s.stridePixels, s.width, s.height };
        const gmpi::cpugfx::DestSurface   dst{ buffer_.pixels(), buffer_.stride(), w_, h_,
                                               gmpi::cpugfx::PixelEncoding::Bgra8888 };
        gmpi::cpugfx::encodeDirtyRect(src, dst, gmpi::drawing::RectL{ 0, 0, w_, h_ });
    }
    if (bmRaw) bmRaw->release();
    if (rtRaw) rtRaw->release();

    wl_surface_attach(surface_, buffer_.buffer(), 0, 0);
    wl_surface_damage_buffer(surface_, 0, 0, w_, h_);
    wl_surface_commit(surface_);
}

inline void WaylandStockDialog::destroySurfaces()
{
    if (surface_)
        input_.unregisterSurface(surface_);

    buffer_.release();

    if (frame_)   { libdecor_frame_unref(frame_); frame_ = nullptr; }
    if (surface_) { wl_surface_destroy(surface_); surface_ = nullptr; }
}

inline void WaylandStockDialog::complete(gmpi::api::StockDialogButton b)
{
    if (completed_)
        return;
    completed_ = true;

    // Tear the window down BEFORE the callback: it commonly opens the next dialog,
    // and leaving this one on screen behind it reads as the click not registering.
    destroySurfaces();

    if (auto cb = callback_.as<gmpi::api::IStockDialogCallback>(); cb)
        cb->onComplete(b);

    callback_ = {};
    release();   // balances the addRef in showAsync
}

inline gmpi::ReturnCode WaylandFrameBase::createStockDialog(
    int32_t dialogType, const char* title, const char* text, gmpi::api::IUnknown** returnDialog)
{
    *returnDialog = {};

    // Without a libdecor context there is no way to put a decorated window on
    // screen; better to say so than to map an undecorated, unclosable box.
    if (!decorContext())
        return gmpi::ReturnCode::NoSupport;

    auto* dlg = new WaylandStockDialog(connection_, inputDispatch(), decorContext(),
                                       factory_, menuFont_, dialogType,
                                       title ? title : "", text ? text : "");
    *returnDialog = static_cast<gmpi::api::IStockDialog*>(dlg);
    return gmpi::ReturnCode::Ok;
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
    xdg_surface* popupParent() const override
    {
        return frame_ ? libdecor_frame_get_xdg_surface(frame_) : nullptr;
    }

    // share our context so a message box is dispatched by this window's loop
    libdecor* decorContext() const override { return decor_; }

    InputDispatch& input() { return input_; }
    InputDispatch& inputDispatch() override { return input_; }

    bool running() const { return running_; }
    void close() { running_ = false; }

    // Fds the host's loop should poll. Standalone, runEventLoop does it for you;
    // in a plugin the host owns the loop and just needs these.
    int  displayFd() const { return libdecor_get_fd(decor_); }
    void dispatch()  { libdecor_dispatch(decor_, 0); }

    void runEventLoop(int tickMs, const std::function<void(int)>& onTick);

    // True if the connection has died. Prints the offending interface and opcode -
    // wayland reports these once and then the connection is unusable.
    bool reportProtocolError();

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
    self.surfaceConfigured_ = true;   // buffers are legal from here on

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

    // Ask for the handle now: the compositor answers asynchronously, so it is
    // ready long before the user reaches a File menu.
    exportWindowHandle();

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

inline bool WaylandToplevel::reportProtocolError()
{
    const int err = wl_display_get_error(connection_.display());
    if (!err)
        return false;

    if (err == EPROTO)
    {
        const wl_interface* iface{};
        uint32_t id = 0;
        const uint32_t code = wl_display_get_protocol_error(connection_.display(), &iface, &id);
        fprintf(stderr, "wayland protocol error: %s.%u on object %u\n",
                iface && iface->name ? iface->name : "?", code, id);
    }
    else
    {
        fprintf(stderr, "wayland connection error: %s\n", strerror(err));
    }

    running_ = false;
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

        // The portal replies on the session bus. It joins the same poll() rather
        // than getting a nested loop of its own: blocking here while a popup grab
        // is up wedges the compositor.
        const int busFd = portalBus().fd();

        pollfd fds[3]{};
        int nfds = 1;
        fds[0].fd = displayFd();
        fds[0].events = POLLIN;
        if (timerFd >= 0)
        {
            fds[nfds].fd = timerFd;
            fds[nfds].events = POLLIN;
            ++nfds;
        }
        if (busFd >= 0)
        {
            fds[nfds].fd = busFd;
            fds[nfds].events = POLLIN;
            ++nfds;
        }

        if (poll(fds, nfds, 100) < 0 && errno != EINTR)
            break;

        // Pump unconditionally: libdbus may already hold a decoded message, in
        // which case the fd never becomes readable again and a revents check
        // would wait forever.
        portalBus().pump();

        // libdecor owns the dispatch: it must see configure events before we do,
        // so it can size the decorations.
        if (libdecor_dispatch(decor_, 0) < 0)
            break;

        // A protocol error disconnects us, and if it happens while a popup grab is
        // up, a compositor that leaks the grab takes the desktop's input with it.
        // Silence here means guessing later, so say exactly what was violated.
        if (reportProtocolError())
            break;

        // A protocol error disconnects us with no other symptom, so say which one.
        // Worth the two lines: the failure it diagnoses (attaching to an
        // unconfigured surface) presents as the whole desktop hanging, because the
        // client dies holding a popup grab.
        if (const int err = wl_display_get_error(connection_.display()); err != 0)
        {
            uint32_t id = 0;
            const wl_interface* iface = nullptr;
            const uint32_t code = wl_display_get_protocol_error(connection_.display(), &iface, &id);

            if (iface)
                fprintf(stderr, "wayland protocol error: %s id=%u code=%u\n", iface->name, id, code);
            else
                fprintf(stderr, "wayland connection error: %d (%s)\n", err, strerror(err));

            running_ = false;
            break;
        }

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
