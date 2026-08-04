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
#include "backends/WaylandClipboard.h"

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
    bool hasLivePopups() const { return !livePopups_.empty(); }

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

        // Settle every pending request before the client goes away.
        //
        // This works around a compositor bug, not one of ours: mutter 46
        // (libmutter-14) segfaults inside its own resource-destroy handler during
        // wl_client_destroy, calling g_signal_handler_disconnect on an object it
        // has already finalised. It only bites once the session has created an
        // xdg_popup, and it takes the user's whole desktop with it.
        //
        //   #0 g_type_check_instance
        //   #1 g_signal_handler_disconnect
        //   #2 libmutter-14
        //   #3 wl_client_destroy
        //
        // Draining here makes mutter tear those resources down while we are still
        // connected, rather than all at once on disconnect. Measured: 2 of 2 runs
        // crashed the compositor without it, 0 of 2 with it.
        if (display_)
            wl_display_roundtrip(display_);

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
    wl_data_device_manager*         dataDeviceManager() const { return dataDeviceMgr_; }

    // Bind a FRESH wl_seat proxy for a caller that wants its own listener.
    //
    // wl_seat.capabilities is sent once, right after the bind. bindGlobals()
    // round-trips to discover the globals, so by the time anything else could add
    // a listener that event has already been dispatched and dropped - and without
    // capabilities we never call get_pointer, so no input arrives at all. Binding
    // again is legal and produces a fresh capabilities event for the new proxy.
    wl_seat* bindSeatProxy()
    {
        if (!registry_ || !seatName_)
            return nullptr;
        return static_cast<wl_seat*>(
            wl_registry_bind(registry_, seatName_, &wl_seat_interface, seatVersion_));
    }

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
    wl_data_device_manager*         dataDeviceMgr_{};
    uint32_t seatName_{};      // registry name, so a listener-owning proxy can be bound
    uint32_t seatVersion_ = 5;
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
    {
        c.seatName_ = name;
        c.seat_ = static_cast<wl_seat*>(wl_registry_bind(reg, name, &wl_seat_interface, c.seatVersion_));
    }
    else if (!strcmp(iface, wp_viewporter_interface.name))
        c.viewporter_ = static_cast<wp_viewporter*>(wl_registry_bind(reg, name, &wp_viewporter_interface, 1));
    else if (!strcmp(iface, wp_fractional_scale_manager_v1_interface.name))
        c.fracMgr_ = static_cast<wp_fractional_scale_manager_v1*>(wl_registry_bind(reg, name, &wp_fractional_scale_manager_v1_interface, 1));
    else if (!strcmp(iface, wp_cursor_shape_manager_v1_interface.name))
        c.cursorMgr_ = static_cast<wp_cursor_shape_manager_v1*>(wl_registry_bind(reg, name, &wp_cursor_shape_manager_v1_interface, 1));
    // xdg_foreign: exports a handle the portal can use as parent_window, so its
    // file chooser is modal to OUR window instead of floating unattached
    else if (!strcmp(iface, wl_data_device_manager_interface.name))
        c.dataDeviceMgr_ = static_cast<wl_data_device_manager*>(
            wl_registry_bind(reg, name, &wl_data_device_manager_interface, 3));
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

    // Content coordinates -> the parent's WINDOW GEOMETRY, which is what
    // xdg_positioner anchors against.
    //
    // With client-side decorations those are not the same origin: the frame
    // includes the titlebar and borders, so an anchor handed straight through
    // lands one titlebar too high. Every menu in the app was drawing over its
    // own titlebar because of it. Default is a no-op for anything undecorated.
    virtual gmpi::drawing::Rect toFrameCoordinates(const gmpi::drawing::Rect& r) const { return r; }

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
    // defined after WaylandTextEdit, which it constructs
    gmpi::ReturnCode createTextEdit(const gmpi::drawing::Rect* r,
                                    gmpi::api::IUnknown** returnTextEdit) override;
    // defined after WaylandKeyListener, which it constructs
    gmpi::ReturnCode createKeyListener(const gmpi::drawing::Rect* r,
                                       gmpi::api::IUnknown** returnKeyListener) override;
    // defined after WaylandStockDialog, which it constructs
    gmpi::ReturnCode createStockDialog(int32_t dialogType, const char* title, const char* text,
                                       gmpi::api::IUnknown** returnDialog) override;
    // defined after WaylandColorDialog, which it constructs
    gmpi::ReturnCode createColorDialog(gmpi::drawing::Color initialColor,
                                       gmpi::api::IUnknown** returnDialog) override;

    gmpi::ReturnCode queryInterface(const gmpi::api::Guid* iid, void** returnInterface) override
    {
        *returnInterface = {};
        GMPI_QUERYINTERFACE(gmpi::api::IDialogHost);
        return WaylandHostBase::queryInterface(iid, returnInterface);
    }
    GMPI_REFCOUNT_NO_DELETE;

    ~WaylandFrameBase() override
    {
        // NOT detachClient(): it reaches inputDispatch(), which is pure virtual.
        // By the time a base destructor runs the derived override is gone, so
        // that call lands on __cxa_pure_virtual and aborts. Derived classes
        // detach in their own destructor, while they are still whole; all that
        // is left here is the part that needs no virtual.
        if (client_)
            client_->setHost(nullptr);
        client_ = {};

        if (exported_) zxdg_exported_v2_destroy(exported_);
        buffer_.release();
        if (viewport_) wp_viewport_destroy(viewport_);
        if (surface_)  wl_surface_destroy(surface_);
    }

    // defined after InputDispatch, whose context-menu hook it installs
    void attachClient(gmpi::api::IDrawingClient* client);

    // defined after InputDispatch, whose input client it clears
    void detachClient();

    // defined after WaylandTextEdit
    void drawActiveTextEdit(gmpi::cpugfx::RenderTarget* rt);

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
    // Non-owning: the edit keeps itself alive until it completes, exactly like
    // the dialogs. The frame only needs to know where to draw it.
    class WaylandTextEdit* activeEdit_{};

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
    drawActiveTextEdit(rt);   // an in-place edit sits ON the client, after it
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
// Something that wants raw keys before the drawing client sees them - an
// IKeyListener, or a text edit. Popups still win: they hold the grab.
struct IKeySink
{
    virtual void onRawKey(uint32_t keysym, uint32_t utf32, int32_t flags, bool down) = 0;

    // Another sink has taken the keyboard. Whatever this was doing is over.
    virtual void onSinkReplaced() = 0;
protected:
    ~IKeySink() = default;
};

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
        if (pointerFocus_ == s)
            pointerFocus_ = nullptr;
        if (keyboardFocus_ == s)
            keyboardFocus_ = nullptr;
    }

    void bindSeat(Connection& connection);

    // Replacing a sink ENDS the old one. Silently overwriting it leaves an edit
    // that can never be finished: nothing routes keys to it any more, so the
    // reference it took in showAsync is never dropped.
    void setKeySink(IKeySink* sink)
    {
        if (keySink_ == sink)
            return;

        auto* old = keySink_;
        keySink_ = sink;          // clear first: onSinkReplaced may destroy `old`
        if (old)
            old->onSinkReplaced();
    }

    // Consulted before the drawing client on a button event; returns true if it
    // swallowed it. An in-place text edit lives on the client's own surface, so
    // there is no separate IPointerTarget for the dispatcher to route to.
    std::function<bool(double x, double y, bool pressed)> pointerPreFilter;
    IKeySink* keySink() const { return keySink_; }

    WaylandClipboard& clipboard() { return clipboard_; }

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

    // Is the pointer over the surface the client actually draws into?
    //
    // A window is more surfaces than its content: libdecor's title bar, borders
    // and shadow are subsurfaces of this same window, and their coordinates are
    // measured from the FRAME's top-left, not the content's. Forwarding those
    // unfiltered handed the client a position one title-bar too high - a click
    // on the close button arrived as a click in the menu bar, which is why the
    // window would not close and why the menu bar seemed to ignore the mouse.
    // Anything that is neither a registered target nor the content surface is
    // not ours to deliver.
    bool overContent() const { return pointerFocus_ == surface_; }

    gmpi::api::IInputClient* client_{};
    std::vector<std::pair<wl_surface*, IPointerTarget*>> targets_;
    // Pointer and keyboard focus are DIFFERENT surfaces and must not share a
    // variable: a popup or dialog takes the keyboard while the pointer is still
    // over the editor, and one overwriting the other sends mouse events to the
    // dialog and keystrokes to whatever was hovered.
    wl_surface*  pointerFocus_{};
    wl_surface*  keyboardFocus_{};
    wl_surface*  surface_{};
    IKeySink*        keySink_{};
    WaylandClipboard clipboard_;
    wl_seat*     seat_{};      // our own proxy: see bindSeat
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
    in.pointerFocus_ = surf;

    // Track the position for EVERY surface, before routing. onButton carries no
    // coordinates of its own, so a popup or dialog would otherwise be handed the
    // last position seen over the main window and hit-test against the wrong point.
    in.x_ = wl_fixed_to_double(sx);
    in.y_ = wl_fixed_to_double(sy);

    if (auto* t = in.targetFor(surf))
    {
        t->onEnter(in.x_, in.y_, serial);
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

    if (in.pointerFocus_ == surf)
        in.pointerFocus_ = nullptr;

    if (auto* t = in.targetFor(surf))
    {
        t->onLeave();
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

    in.x_ = wl_fixed_to_double(sx);
    in.y_ = wl_fixed_to_double(sy);

    if (auto* t = in.targetFor(in.pointerFocus_))
    {
        t->onMotion(in.x_, in.y_, in.modifierFlags());
        return;
    }

    if (!in.overContent())
        return;

    if (in.client_)
        in.client_->onPointerMove(in.pointerPos(), in.modifierFlags() | int32_t(in.buttons_));
}

inline void InputDispatch::pointerButton(void* data, wl_pointer*, uint32_t serial,
                                         uint32_t, uint32_t button, uint32_t state)
{
    auto& in = *static_cast<InputDispatch*>(data);
    if (state == WL_POINTER_BUTTON_STATE_PRESSED)
        in.lastGrabSerial_ = serial;

    if (auto* t = in.targetFor(in.pointerFocus_))
    {
        t->onButton(in.x_, in.y_, button, state == WL_POINTER_BUTTON_STATE_PRESSED, serial);
        return;
    }

    // The click landed on our own window while a menu is up, so dismiss it.
    //
    // The compositor does NOT send popup_done for a click on another surface of
    // the SAME client - it just delivers the click. Without this the menu stays
    // up holding the keyboard grab, and every keystroke after it silently
    // disappears into a popup nobody can see. It took a harness run to notice,
    // because the symptom is a key doing nothing rather than anything visible.
    if (state == WL_POINTER_BUTTON_STATE_PRESSED &&
        in.connection_ && in.connection_->hasLivePopups())
    {
        in.connection_->closeLivePopups();
        return;                 // the click dismissed the menu; it is not also a click
    }

    if (!in.overContent())
        return;

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

    if (in.pointerPreFilter && in.pointerPreFilter(in.x_, in.y_, pressed))
        return;

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
    if (!in.client_ || !in.overContent())
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

    if (!in.xkbState_)
        return;

    const bool down = (state == WL_KEYBOARD_KEY_STATE_PRESSED);

    // wayland reports evdev keycodes; xkb numbers them 8 higher
    const xkb_keycode_t code = key + 8;
    const uint32_t utf32  = xkb_state_key_get_utf32(in.xkbState_, code);
    const uint32_t keysym = xkb_state_key_get_one_sym(in.xkbState_, code);

    // a popup holds the grab while it is up, so it sees the keys first
    if (auto* t = in.targetFor(in.keyboardFocus_))
    {
        if (down)
            t->onKey(keysym, utf32);
        return;
    }

    // a key listener or text edit takes them next, down AND up: it is the only
    // thing here that needs key releases
    if (in.keySink_)
    {
        in.keySink_->onRawKey(keysym, utf32, in.modifierFlags(), down);
        return;
    }

    if (down && utf32 && in.client_)
        in.client_->onKeyPress(static_cast<wchar_t>(utf32));
}

inline void InputDispatch::keyboardEnter(void* data, wl_keyboard*, uint32_t, wl_surface* surf, wl_array*)
{
    // a grabbing popup gets keyboard focus; remember which surface so keys route there
    auto& in = *static_cast<InputDispatch*>(data);
    if (in.targetFor(surf))
        in.keyboardFocus_ = surf;
}
inline void InputDispatch::keyboardLeave(void* data, wl_keyboard*, uint32_t, wl_surface* surf)
{
    // Leaving it set means keys keep being routed at a surface that no longer has
    // the keyboard - and once that surface is destroyed, at nothing at all.
    auto& in = *static_cast<InputDispatch*>(data);
    if (in.keyboardFocus_ == surf || !surf)
        in.keyboardFocus_ = nullptr;
}

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

    // Our own proxy, so the listener is in place BEFORE capabilities is sent.
    // Adding a listener to the connection's seat instead is too late: that event
    // was already dispatched during bindGlobals' roundtrip and dropped, leaving us
    // with no pointer and no keyboard.
    seat_ = connection.bindSeatProxy();
    if (seat_)
    {
        wl_seat_add_listener(seat_, &seatListener, this);
        clipboard_.bind(connection.display(), connection.dataDeviceManager(), seat_);
    }
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


inline void WaylandFrameBase::detachClient()
{
    if (client_)
        client_->setHost(nullptr);
    client_ = {};
    inputDispatch().attachInputClient(nullptr);
}

inline void WaylandFrameBase::attachClient(gmpi::api::IDrawingClient* client)
{
    detachClient();
    client_ = client;
    if (client_)
        client_->setHost(static_cast<gmpi::api::IDrawingHost*>(this));

    // One object almost always implements both halves - the interfaces even share
    // a single setHost for that reason - so wire input here rather than making
    // every app remember a second call. Without this the client draws but never
    // hears a click.
    // NB queryInterface, not shared_ptr(raw): that constructor ATTACHES - it takes
    // ownership without addRef - so wrapping a borrowed pointer steals a reference
    // and frees the object early.
    gmpi::api::IInputClient* inputClient{};
    if (client_)
        client_->queryInterface(&gmpi::api::IInputClient::guid,
                                reinterpret_cast<void**>(&inputClient));
    inputDispatch().attachInputClient(inputClient);
    if (inputClient)
        inputClient->release();   // we keep a borrowed link, exactly as with client_

    // Right-click -> ask the client what belongs on the menu -> show it. The
    // seat reports the press serial, which has to travel all the way to
    // xdg_popup.grab: mutter grants the grab only for the serial of the event
    // that started it, so it cannot be looked up later.
    inputDispatch().onContextMenu =
        [this](gmpi::drawing::Point pt, uint32_t)
        {
            gmpi::api::IInputClient* rawInput{};
            if (client_)
                client_->queryInterface(&gmpi::api::IInputClient::guid,
                                        reinterpret_cast<void**>(&rawInput));
            if (!rawInput)
                return;

            gmpi::shared_ptr<gmpi::api::IInputClient> inputClient;
            inputClient.attach(rawInput);   // queryInterface already addRef'd

            gmpi::api::IUnknown* raw{};
            const gmpi::drawing::Rect anchor{ pt.x, pt.y, pt.x + 1.f, pt.y + 1.f };
            if (createPopupMenu(&anchor, &raw) != gmpi::ReturnCode::Ok || !raw)
                return;

            auto menu = gmpi::shared_ptr<gmpi::api::IUnknown>();
            menu.attach(raw);

            auto popup = menu.as<gmpi::api::IPopupMenu>();
            if (!popup)
                return;

            // The client fills the menu; an empty one is a client saying "not
            // here", so put nothing on screen rather than an empty box.
            if (inputClient->populateContextMenu(pt, popup.get()) != gmpi::ReturnCode::Ok)
                return;

            popup->showAsync();
        };
}

inline gmpi::ReturnCode WaylandFrameBase::createPopupMenu(const gmpi::drawing::Rect* r,
                                                          gmpi::api::IUnknown** returnPopupMenu)
{
    *returnPopupMenu = {};

    if (!popupParent())
        return gmpi::ReturnCode::Fail;

    const auto anchor = toFrameCoordinates(r ? *r : gmpi::drawing::Rect{});

    auto* menu = new WaylandPopupMenu(connection_, inputDispatch(), popupParent(),
                                      factory_, menuFont_, anchor);
    *returnPopupMenu = static_cast<gmpi::api::IPopupMenu*>(menu);
    return gmpi::ReturnCode::Ok;
}

// ---------------------------------------------------------------------------
// WaylandTextEdit - ITextEdit
// ---------------------------------------------------------------------------
// Unlike the dialogs this is not a window: an in-place edit belongs ON the
// client's surface, at the rect the caller asked for, so it moves and scales
// with whatever is underneath it. The frame draws it after the client, and
// routes keys and clicks to it while it is up.
//
// Text is held as UTF-8 but edited by CODEPOINT: stepping the caret a byte at a
// time would walk into the middle of any character above U+007F and produce
// invalid UTF-8 the moment the user pressed Backspace.
class WaylandTextEdit : public gmpi::api::ITextEdit, private IKeySink
{
public:
    WaylandTextEdit(Connection& connection, InputDispatch& input,
                    gmpi::drawing::api::ITextFormat* font, gmpi::drawing::Rect rect)
        : connection_(connection), input_(input), font_(font), rect_(rect) {}

    ~WaylandTextEdit()
    {
        if (input_.keySink() == this)
            input_.setKeySink(nullptr);
    }

    // --- ITextEdit ---
    gmpi::ReturnCode setText(const char* text) override
    {
        text_ = text ? text : "";
        caret_ = int32_t(text_.size());
        selectAll();
        return gmpi::ReturnCode::Ok;
    }
    gmpi::ReturnCode setAlignment(int32_t alignment) override
    {
        alignment_ = alignment;
        return gmpi::ReturnCode::Ok;
    }
    gmpi::ReturnCode setTextSize(float height) override
    {
        textHeight_ = height;
        return gmpi::ReturnCode::Ok;
    }
    gmpi::ReturnCode showAsync(gmpi::api::IUnknown* callback) override
    {
        callback_ = callback;
        input_.setKeySink(this);
        addRef();
        return gmpi::ReturnCode::Ok;
    }

    gmpi::ReturnCode queryInterface(const gmpi::api::Guid* iid, void** returnInterface) override
    {
        *returnInterface = {};
        GMPI_QUERYINTERFACE(gmpi::api::ITextEdit);
        return gmpi::ReturnCode::NoSupport;
    }
    GMPI_REFCOUNT;

    // Told when the edit completes, BEFORE it drops its own reference. Without
    // this the frame is left holding a pointer to a deleted object and the next
    // repaint reads freed memory.
    std::function<void(WaylandTextEdit*)> onFinished;

    // --- used by the frame ---
    bool active() const { return !finished_; }
    void commit() { finish(gmpi::ReturnCode::Ok); }
    const gmpi::drawing::Rect& rect() const { return rect_; }
    void render(gmpi::cpugfx::RenderTarget* rt);
    void onPointer(double x, double y, bool pressed);

    // --- exposed so the editing model can be checked without a compositor ----
    const std::string& text() const { return text_; }
    int32_t caret() const { return caret_; }
    std::pair<int32_t, int32_t> selection() const
    { return { (std::min)(selAnchor_, caret_), (std::max)(selAnchor_, caret_) }; }

    // UTF-8 aware stepping: continuation bytes are 10xxxxxx, so skip them
    int32_t nextCodepoint(int32_t i) const
    {
        const int32_t n = int32_t(text_.size());
        if (i >= n) return n;
        ++i;
        while (i < n && (uint8_t(text_[size_t(i)]) & 0xC0) == 0x80) ++i;
        return i;
    }
    int32_t prevCodepoint(int32_t i) const
    {
        if (i <= 0) return 0;
        --i;
        while (i > 0 && (uint8_t(text_[size_t(i)]) & 0xC0) == 0x80) --i;
        return i;
    }

    void insertUtf32(uint32_t cp);
    void handleKey(uint32_t keysym, uint32_t utf32, int32_t flags, bool down);

private:
    void selectAll() { selAnchor_ = 0; caret_ = int32_t(text_.size()); }
    bool hasSelection() const { return selAnchor_ != caret_; }
    void deleteSelection();
    std::string selectedText() const;
    void notifyChanged();
    void finish(gmpi::ReturnCode result);

    void onRawKey(uint32_t keysym, uint32_t utf32, int32_t flags, bool down) override
    { handleKey(keysym, utf32, flags, down); }

    // losing the keyboard commits, exactly as clicking away does
    void onSinkReplaced() override { finish(gmpi::ReturnCode::Ok); }

    Connection&    connection_;
    InputDispatch& input_;
    gmpi::drawing::api::ITextFormat* font_{};
    gmpi::drawing::Rect rect_{};

    std::string text_;
    int32_t caret_ = 0;
    int32_t selAnchor_ = 0;
    int32_t alignment_ = 0;
    float   textHeight_ = 0.f;
    bool    finished_ = false;

    gmpi::shared_ptr<gmpi::api::IUnknown> callback_;
};

inline std::string WaylandTextEdit::selectedText() const
{
    const auto [a, b] = selection();
    return text_.substr(size_t(a), size_t(b - a));
}

inline void WaylandTextEdit::deleteSelection()
{
    if (!hasSelection())
        return;
    const auto [a, b] = selection();
    text_.erase(size_t(a), size_t(b - a));
    caret_ = a;
    selAnchor_ = a;
}

inline void WaylandTextEdit::insertUtf32(uint32_t cp)
{
    deleteSelection();

    // encode as UTF-8; the edit buffer is bytes, the model is codepoints
    char buf[4];
    int n = 0;
    if (cp < 0x80) { buf[n++] = char(cp); }
    else if (cp < 0x800)
    {
        buf[n++] = char(0xC0 | (cp >> 6));
        buf[n++] = char(0x80 | (cp & 0x3F));
    }
    else if (cp < 0x10000)
    {
        buf[n++] = char(0xE0 | (cp >> 12));
        buf[n++] = char(0x80 | ((cp >> 6) & 0x3F));
        buf[n++] = char(0x80 | (cp & 0x3F));
    }
    else
    {
        buf[n++] = char(0xF0 | (cp >> 18));
        buf[n++] = char(0x80 | ((cp >> 12) & 0x3F));
        buf[n++] = char(0x80 | ((cp >> 6) & 0x3F));
        buf[n++] = char(0x80 | (cp & 0x3F));
    }

    text_.insert(size_t(caret_), buf, size_t(n));
    caret_ += n;
    selAnchor_ = caret_;
}

inline void WaylandTextEdit::notifyChanged()
{
    if (auto cb = callback_.as<gmpi::api::ITextEditCallback>(); cb)
        cb->onChanged(text_.c_str());
}

inline void WaylandTextEdit::finish(gmpi::ReturnCode result)
{
    if (finished_)
        return;
    finished_ = true;

    if (input_.keySink() == this)
        input_.setKeySink(nullptr);

    if (auto cb = callback_.as<gmpi::api::ITextEditCallback>(); cb)
        cb->onComplete(result);

    callback_ = {};

    // The frame drops ITS reference first and stops pointing at us; ours goes
    // last, so whichever release reaches zero does so after every observer has
    // let go.
    if (onFinished)
    {
        auto notify = onFinished;
        onFinished = nullptr;
        notify(this);
    }

    release();   // balances the addRef in showAsync
}

inline void WaylandTextEdit::handleKey(uint32_t keysym, uint32_t utf32, int32_t flags, bool down)
{
    if (!down || finished_)
        return;

    const bool ctrl  = (flags & int32_t(gmpi::api::PointerFlags::KeyControl)) != 0;
    const bool shift = (flags & int32_t(gmpi::api::PointerFlags::KeyShift)) != 0;

    // moving the caret collapses the selection unless shift is held
    auto moveTo = [&](int32_t pos)
    {
        caret_ = pos;
        if (!shift)
            selAnchor_ = pos;
    };

    if (ctrl)
    {
        switch (keysym)
        {
        case 'a': case 'A': selectAll(); return;
        case 'c': case 'C':
            if (hasSelection())
                input_.clipboard().setText(connection_.display(), selectedText(),
                                           input_.lastGrabSerial());
            return;
        case 'x': case 'X':
            if (hasSelection())
            {
                input_.clipboard().setText(connection_.display(), selectedText(),
                                           input_.lastGrabSerial());
                deleteSelection();
                notifyChanged();
            }
            return;
        case 'v': case 'V':
        {
            const std::string raw = input_.clipboard().getText(connection_.display());

            // Clipboard content is whatever some other program put there. A
            // newline or NUL in a single-line value corrupts both the display and
            // the string handed to the host, so apply the same filter as typing.
            std::string clip;
            clip.reserve(raw.size());
            for (const char ch : raw)
            {
                const auto byte = uint8_t(ch);
                if (byte == 0)
                    break;                       // NUL ends it; the rest is not text
                if (byte >= 0x20 && byte != 0x7f)
                    clip += ch;
            }

            if (!clip.empty())
            {
                deleteSelection();
                text_.insert(size_t(caret_), clip);
                caret_ += int32_t(clip.size());
                selAnchor_ = caret_;
                notifyChanged();
            }
            return;
        }
        default: return;   // any other ctrl chord is a shortcut, not text
        }
    }

    switch (keysym)
    {
    case 0xff08:                       // Backspace
        if (hasSelection())
            deleteSelection();
        else if (caret_ > 0)
        {
            const int32_t p = prevCodepoint(caret_);
            text_.erase(size_t(p), size_t(caret_ - p));
            caret_ = p;
            selAnchor_ = p;
        }
        notifyChanged();
        return;

    case 0xffff:                       // Delete
        if (hasSelection())
            deleteSelection();
        else if (caret_ < int32_t(text_.size()))
        {
            const int32_t nx = nextCodepoint(caret_);
            text_.erase(size_t(caret_), size_t(nx - caret_));
        }
        notifyChanged();
        return;

    // With a selection and no shift, an arrow collapses to that EDGE - it does
    // not step from the caret, which would drop a character off the end.
    case 0xff51:                                                     // Left
        if (!shift && hasSelection()) moveTo(selection().first);
        else                          moveTo(prevCodepoint(caret_));
        return;
    case 0xff53:                                                     // Right
        if (!shift && hasSelection()) moveTo(selection().second);
        else                          moveTo(nextCodepoint(caret_));
        return;
    case 0xff50: moveTo(0); return;                                  // Home
    case 0xff57: moveTo(int32_t(text_.size())); return;              // End

    case 0xff0d: case 0xff8d:          // Return / KP_Enter: commit
        finish(gmpi::ReturnCode::Ok);
        return;

    case 0xff1b:                       // Escape: abandon
        finish(gmpi::ReturnCode::Cancel);
        return;

    default: break;
    }

    // Anything that produced a printable character is text. Control codes below
    // space are not - they would otherwise be inserted as invisible rubbish.
    if (utf32 >= 0x20 && utf32 != 0x7f)
    {
        insertUtf32(utf32);
        notifyChanged();
    }
}

inline void WaylandTextEdit::onPointer(double x, double y, bool pressed)
{
    if (!pressed || !font_ || finished_)
        return;

    (void)y;

    // Place the caret at the character boundary nearest the click, by measuring
    // each prefix. Linear, but a text edit holds a name, not a document.
    const double target = x - rect_.left - 3.0;

    int32_t best = 0;
    double bestDist = 1e30;
    for (int32_t i = 0; i <= int32_t(text_.size()); i = nextCodepoint(i))
    {
        gmpi::drawing::Size sz{};
        if (i > 0)
            font_->getTextExtentU(text_.c_str(), i, 100000.f, &sz);

        const double d = std::fabs(double(sz.width) - target);
        if (d < bestDist)
        {
            bestDist = d;
            best = i;
        }
        if (i == int32_t(text_.size()))
            break;
    }

    caret_ = best;
    selAnchor_ = best;
}

inline void WaylandTextEdit::render(gmpi::cpugfx::RenderTarget* rt)
{
    if (finished_)
        return;

    auto brush = [&](gmpi::drawing::Color c)
    {
        gmpi::drawing::api::ISolidColorBrush* b{};
        rt->createSolidColorBrush(&c, nullptr, &b);
        return b;
    };

    auto* back = brush({ 0.10f, 0.11f, 0.13f, 1.f });
    auto* ink  = brush({ 0.94f, 0.95f, 0.97f, 1.f });
    auto* sel  = brush({ 0.20f, 0.40f, 0.70f, 1.f });
    auto* edge = brush({ 0.45f, 0.60f, 0.85f, 1.f });

    if (back) rt->fillRectangle(&rect_, back);
    if (edge) rt->drawRectangle(&rect_, edge, 1.f, nullptr);

    const float textLeft = rect_.left + 3.f;
    const float textTop  = rect_.top + 2.f;

    auto widthTo = [&](int32_t i) -> float
    {
        if (i <= 0 || !font_) return 0.f;
        gmpi::drawing::Size sz{};
        font_->getTextExtentU(text_.c_str(), i, 100000.f, &sz);
        return sz.width;
    };

    if (sel && hasSelection())
    {
        const auto [a, b] = selection();
        const gmpi::drawing::Rect r{ textLeft + widthTo(a), rect_.top + 1.f,
                                     textLeft + widthTo(b), rect_.bottom - 1.f };
        rt->fillRectangle(&r, sel);
    }

    if (font_ && ink && !text_.empty())
    {
        const gmpi::drawing::Rect tr{ textLeft, textTop, rect_.right - 2.f, rect_.bottom };
        rt->drawTextU(text_.c_str(), uint32_t(text_.size()), font_, &tr, ink, 0);
    }

    // Caret drawn solid rather than blinking: a blink needs a timer of its own,
    // and a steady caret is not the thing anyone notices about a rename box.
    if (ink)
    {
        const float cx = textLeft + widthTo(caret_);
        const gmpi::drawing::Rect c{ cx, rect_.top + 2.f, cx + 1.5f, rect_.bottom - 2.f };
        rt->fillRectangle(&c, ink);
    }

    if (back) back->release();
    if (ink)  ink->release();
    if (sel)  sel->release();
    if (edge) edge->release();
}

// ---------------------------------------------------------------------------
// WaylandColorDialog - IColorDialog, drawn by us
// ---------------------------------------------------------------------------
// No portal offers a colour chooser (Screenshot.PickColor picks a pixel off the
// screen, which is a different thing), so this is drawn like the message box.
//
// The colour space is the part worth getting right. Every surface in this
// library is LINEAR - the sRGB curve is applied once, on the way out to an
// 8-bit consumer. But hue/saturation/value are perceptual: a user dragging
// halfway down expects half brightness as their eye judges it, and a hex code
// means sRGB. So the model here is sRGB throughout and converts on both
// boundaries - in from initialColor, out to the callback and to every brush.
class WaylandColorDialog : public gmpi::api::IColorDialog, public IPointerTarget
{
public:
    WaylandColorDialog(Connection& connection, InputDispatch& input, libdecor* decor,
                       gmpi::cpugfx::Factory& factory, gmpi::drawing::api::ITextFormat* font,
                       gmpi::drawing::Color initialColor)
        : connection_(connection), input_(input), decor_(decor), factory_(factory), font_(font)
    {
        setFromLinear(initialColor);
        layout();
    }

    ~WaylandColorDialog() override { destroySurfaces(); }

    gmpi::ReturnCode showAsync(gmpi::api::IUnknown* callback) override;

    // --- IPointerTarget ---
    void onEnter(double x, double y, uint32_t) override { (void)x; (void)y; }
    void onLeave() override { hoveredButton_ = -1; present(); }
    void onMotion(double x, double y, int32_t) override;
    void onButton(double x, double y, uint32_t, bool pressed, uint32_t) override;
    void onWheel(double, double, int32_t, double) override {}
    void onKey(uint32_t keysym, uint32_t) override
    {
        if (keysym == 0xff1b)                            // Escape
            complete(gmpi::ReturnCode::Cancel);
        else if (keysym == 0xff0d || keysym == 0xff8d)   // Return
            complete(gmpi::ReturnCode::Ok);
    }

    gmpi::ReturnCode queryInterface(const gmpi::api::Guid* iid, void** returnInterface) override
    {
        *returnInterface = {};
        GMPI_QUERYINTERFACE(gmpi::api::IColorDialog);
        return gmpi::ReturnCode::NoSupport;
    }
    GMPI_REFCOUNT;

    // --- colour maths, exposed so it can be checked without a compositor ------

    // sRGB transfer function, decode direction. linearToSRGB01 is its inverse and
    // lives in GmpiApiDrawing.h; this is the only other copy in the library.
    static float srgbToLinear01(float c)
    {
        c = c < 0.f ? 0.f : (c > 1.f ? 1.f : c);
        return (c <= 0.04045f) ? c / 12.92f
                               : std::pow((c + 0.055f) / 1.055f, 2.4f);
    }

    struct Hsv { float h{}, s{}, v{}, a{ 1.f }; };   // h in [0,360), rest [0,1]

    static void hsvToSrgb(const Hsv& in, float& r, float& g, float& b)
    {
        const float h = std::fmod(std::fmod(in.h, 360.f) + 360.f, 360.f) / 60.f;
        const float c = in.v * in.s;
        const float x = c * (1.f - std::fabs(std::fmod(h, 2.f) - 1.f));
        const float m = in.v - c;

        float rr = 0, gg = 0, bb = 0;
        switch (int(h))
        {
        case 0: rr = c; gg = x; break;
        case 1: rr = x; gg = c; break;
        case 2: gg = c; bb = x; break;
        case 3: gg = x; bb = c; break;
        case 4: rr = x; bb = c; break;
        default: rr = c; bb = x; break;
        }
        r = rr + m; g = gg + m; b = bb + m;
    }

    static Hsv srgbToHsv(float r, float g, float b, float a)
    {
        const float mx = (std::max)(r, (std::max)(g, b));
        const float mn = (std::min)(r, (std::min)(g, b));
        const float d  = mx - mn;

        Hsv out;
        out.v = mx;
        out.s = (mx <= 0.f) ? 0.f : d / mx;
        out.a = a;

        if (d <= 0.f)
            out.h = 0.f;                       // grey: hue is arbitrary, keep it stable
        else if (mx == r) out.h = 60.f * std::fmod((g - b) / d, 6.f);
        else if (mx == g) out.h = 60.f * (((b - r) / d) + 2.f);
        else              out.h = 60.f * (((r - g) / d) + 4.f);

        if (out.h < 0.f) out.h += 360.f;
        return out;
    }

    // the picked colour, converted back to the linear space the library uses
    gmpi::drawing::Color color() const
    {
        float r, g, b;
        hsvToSrgb(hsv_, r, g, b);
        return { srgbToLinear01(r), srgbToLinear01(g), srgbToLinear01(b), hsv_.a };
    }

    void setFromLinear(gmpi::drawing::Color c)
    {
        hsv_ = srgbToHsv(gmpi::drawing::linearToSRGB01(c.r),
                         gmpi::drawing::linearToSRGB01(c.g),
                         gmpi::drawing::linearToSRGB01(c.b), c.a);
    }

    const Hsv& hsv() const { return hsv_; }
    int contentWidth()  const { return w_; }
    int contentHeight() const { return h_; }

    // which control a point lands on; exposed so hit-testing is checkable
    enum class Region { None, SatVal, Hue, Alpha, Ok, Cancel };
    Region regionAt(double x, double y) const;

private:
    struct Button { std::string label; bool ok; gmpi::drawing::Rect rect{}; };

    void layout();
    void present();
    void destroySurfaces();
    void complete(gmpi::ReturnCode result);
    void applyDrag(Region r, double x, double y);

    // a brush in the library's linear space, from an sRGB colour
    gmpi::drawing::api::ISolidColorBrush* srgbBrush(gmpi::cpugfx::RenderTarget* rt,
                                                    float r, float g, float b, float a = 1.f) const
    {
        gmpi::drawing::api::ISolidColorBrush* br{};
        const gmpi::drawing::Color c{ srgbToLinear01(r), srgbToLinear01(g), srgbToLinear01(b), a };
        rt->createSolidColorBrush(&c, nullptr, &br);
        return br;
    }

    static void configure(libdecor_frame* frame, libdecor_configuration* configuration, void* data);
    static void closed(libdecor_frame*, void* data)
    {
        static_cast<WaylandColorDialog*>(data)->complete(gmpi::ReturnCode::Cancel);
    }
    static void commitCb(libdecor_frame*, void* data)
    {
        wl_surface_commit(static_cast<WaylandColorDialog*>(data)->surface_);
    }
    static void dismissPopup(libdecor_frame*, const char*, void*) {}

    Connection&    connection_;
    InputDispatch& input_;
    libdecor*      decor_{};
    gmpi::cpugfx::Factory& factory_;
    gmpi::drawing::api::ITextFormat* font_{};

    Hsv hsv_;
    std::vector<Button> buttons_;
    gmpi::drawing::Rect svRect_{}, hueRect_{}, alphaRect_{}, previewRect_{};

    wl_surface*     surface_{};
    libdecor_frame* frame_{};
    ShmBuffer       buffer_;

    int  w_ = 430, h_ = 300;
    int  hoveredButton_ = -1;
    Region dragging_ = Region::None;
    bool configured_ = false;
    bool completed_  = false;

    gmpi::shared_ptr<gmpi::api::IUnknown> callback_;
};

constexpr int kColorMargin  = 20;
constexpr int kColorSquare  = 200;
constexpr int kColorStripW  = 26;

inline void WaylandColorDialog::layout()
{
    const float m = float(kColorMargin);

    svRect_    = { m, m, m + kColorSquare, m + kColorSquare };
    hueRect_   = { svRect_.right + 12.f, m, svRect_.right + 12.f + kColorStripW, m + kColorSquare };
    alphaRect_ = { hueRect_.right + 12.f, m, hueRect_.right + 12.f + kColorStripW, m + kColorSquare };

    w_ = int(alphaRect_.right + m);
    h_ = int(svRect_.bottom + m + 34 + m);

    previewRect_ = { m, svRect_.bottom + 10.f, m + 90.f, svRect_.bottom + 10.f + 24.f };

    buttons_ = { { "OK", true }, { "Cancel", false } };
    int x = w_ - kColorMargin;
    const int y = h_ - kColorMargin - 30;
    for (auto& b : buttons_)
    {
        x -= 92;
        b.rect = { float(x), float(y), float(x + 92), float(y + 30) };
        x -= 10;
    }
}

inline WaylandColorDialog::Region WaylandColorDialog::regionAt(double x, double y) const
{
    auto in = [&](const gmpi::drawing::Rect& r)
    { return x >= r.left && x < r.right && y >= r.top && y < r.bottom; };

    for (const auto& b : buttons_)
        if (in(b.rect))
            return b.ok ? Region::Ok : Region::Cancel;

    if (in(svRect_))    return Region::SatVal;
    if (in(hueRect_))   return Region::Hue;
    if (in(alphaRect_)) return Region::Alpha;
    return Region::None;
}

inline void WaylandColorDialog::applyDrag(Region r, double x, double y)
{
    auto clamp01 = [](double v) { return float(v < 0 ? 0 : (v > 1 ? 1 : v)); };

    switch (r)
    {
    case Region::SatVal:
        hsv_.s = clamp01((x - svRect_.left) / (svRect_.right - svRect_.left));
        hsv_.v = 1.f - clamp01((y - svRect_.top) / (svRect_.bottom - svRect_.top));
        break;
    case Region::Hue:
        hsv_.h = clamp01((y - hueRect_.top) / (hueRect_.bottom - hueRect_.top)) * 359.99f;
        break;
    case Region::Alpha:
        hsv_.a = 1.f - clamp01((y - alphaRect_.top) / (alphaRect_.bottom - alphaRect_.top));
        break;
    default: return;
    }
    present();
}

inline void WaylandColorDialog::onMotion(double x, double y, int32_t)
{
    if (dragging_ != Region::None)
    {
        // keep tracking outside the control: releasing the pointer is what ends a
        // drag, not wandering off the edge of the square
        applyDrag(dragging_, x, y);
        return;
    }

    int hit = -1;
    for (size_t i = 0; i < buttons_.size(); ++i)
        if (x >= buttons_[i].rect.left && x < buttons_[i].rect.right &&
            y >= buttons_[i].rect.top  && y < buttons_[i].rect.bottom)
            hit = int(i);

    if (hit != hoveredButton_)
    {
        hoveredButton_ = hit;
        present();
    }
}

inline void WaylandColorDialog::onButton(double x, double y, uint32_t, bool pressed, uint32_t)
{
    const Region r = regionAt(x, y);

    if (pressed)
    {
        if (r == Region::SatVal || r == Region::Hue || r == Region::Alpha)
        {
            dragging_ = r;
            applyDrag(r, x, y);
        }
        return;
    }

    if (dragging_ != Region::None)
    {
        dragging_ = Region::None;
        return;
    }

    if (r == Region::Ok)     complete(gmpi::ReturnCode::Ok);
    else if (r == Region::Cancel) complete(gmpi::ReturnCode::Cancel);
}

inline gmpi::ReturnCode WaylandColorDialog::showAsync(gmpi::api::IUnknown* callback)
{
    static libdecor_frame_interface frameInterface = { configure, closed, commitCb, dismissPopup };

    if (!decor_ || !connection_.compositor())
        return gmpi::ReturnCode::Fail;

    connection_.closeLivePopups();   // it takes the keyboard; see the stock dialog

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

    libdecor_frame_set_title(frame_, "Choose a Colour");
    libdecor_frame_map(frame_);

    input_.registerSurface(surface_, this);
    callback_ = callback;

    addRef();
    return gmpi::ReturnCode::Ok;
}

inline void WaylandColorDialog::configure(libdecor_frame* frame,
                                          libdecor_configuration* configuration, void* data)
{
    auto& d = *static_cast<WaylandColorDialog*>(data);

    int w = 0, h = 0;
    if (!libdecor_configuration_get_content_size(configuration, frame, &w, &h) || w <= 0 || h <= 0)
    {
        w = d.w_;
        h = d.h_;
    }

    libdecor_state* state = libdecor_state_new(w, h);
    libdecor_frame_commit(frame, state, configuration);
    libdecor_state_free(state);

    d.configured_ = true;
    d.present();
    wl_surface_commit(d.surface_);
}

inline void WaylandColorDialog::present()
{
    if (!configured_ || !surface_ || w_ <= 0 || h_ <= 0)
        return;
    if (!buffer_.matches(w_, h_) && !buffer_.create(connection_.shm(), w_, h_))
        return;

    gmpi::drawing::api::IBitmapRenderTarget* rtRaw{};
    factory_.createCpuRenderTarget({ uint32_t(w_), uint32_t(h_) }, 0, &rtRaw, 96.0f);
    auto* rt = dynamic_cast<gmpi::cpugfx::RenderTarget*>(rtRaw);
    if (!rt) { if (rtRaw) rtRaw->release(); return; }

    rt->beginDraw();
    const gmpi::drawing::Color bg{ 0.16f, 0.17f, 0.19f, 1.0f };
    rt->clear(&bg);

    // --- saturation / value square -----------------------------------------
    // Banded rather than per-pixel: 40000 one-pixel fills per repaint is not a
    // thing to do on a CPU renderer, and at this size the seams are invisible.
    constexpr int kBands = 48;
    const float bw = (svRect_.right - svRect_.left) / kBands;
    const float bh = (svRect_.bottom - svRect_.top) / kBands;
    for (int iy = 0; iy < kBands; ++iy)
    {
        for (int ix = 0; ix < kBands; ++ix)
        {
            float r, g, b;
            hsvToSrgb({ hsv_.h, (ix + 0.5f) / kBands, 1.f - (iy + 0.5f) / kBands }, r, g, b);
            if (auto* br = srgbBrush(rt, r, g, b))
            {
                const gmpi::drawing::Rect cell{ svRect_.left + ix * bw, svRect_.top + iy * bh,
                                                svRect_.left + (ix + 1) * bw + 1.f,
                                                svRect_.top + (iy + 1) * bh + 1.f };
                rt->fillRectangle(&cell, br);
                br->release();
            }
        }
    }

    // --- hue strip ----------------------------------------------------------
    for (int iy = 0; iy < kBands; ++iy)
    {
        float r, g, b;
        hsvToSrgb({ (iy + 0.5f) / kBands * 359.99f, 1.f, 1.f }, r, g, b);
        if (auto* br = srgbBrush(rt, r, g, b))
        {
            const gmpi::drawing::Rect cell{ hueRect_.left, hueRect_.top + iy * bh,
                                            hueRect_.right, hueRect_.top + (iy + 1) * bh + 1.f };
            rt->fillRectangle(&cell, br);
            br->release();
        }
    }

    // --- alpha strip: the colour over a chequer, so transparency reads -------
    {
        float r, g, b;
        hsvToSrgb(hsv_, r, g, b);
        for (int iy = 0; iy < kBands; ++iy)
        {
            const float a = 1.f - (iy + 0.5f) / kBands;
            const float chequer = ((iy / 4) % 2) ? 0.55f : 0.75f;
            const float mr = r * a + chequer * (1.f - a);
            const float mg = g * a + chequer * (1.f - a);
            const float mb = b * a + chequer * (1.f - a);
            if (auto* br = srgbBrush(rt, mr, mg, mb))
            {
                const gmpi::drawing::Rect cell{ alphaRect_.left, alphaRect_.top + iy * bh,
                                                alphaRect_.right, alphaRect_.top + (iy + 1) * bh + 1.f };
                rt->fillRectangle(&cell, br);
                br->release();
            }
        }
    }

    // --- preview + chrome ---------------------------------------------------
    {
        float r, g, b;
        hsvToSrgb(hsv_, r, g, b);
        if (auto* br = srgbBrush(rt, r, g, b, hsv_.a))
        {
            rt->fillRectangle(&previewRect_, br);
            br->release();
        }
    }

    gmpi::drawing::api::ISolidColorBrush* ink{};
    const gmpi::drawing::Color inkC{ 0.92f, 0.93f, 0.95f, 1.f };
    rt->createSolidColorBrush(&inkC, nullptr, &ink);

    gmpi::drawing::api::ISolidColorBrush* edge{};
    const gmpi::drawing::Color edgeC{ 0.38f, 0.39f, 0.43f, 1.f };
    rt->createSolidColorBrush(&edgeC, nullptr, &edge);

    if (edge)
    {
        rt->drawRectangle(&svRect_, edge, 1.f, nullptr);
        rt->drawRectangle(&hueRect_, edge, 1.f, nullptr);
        rt->drawRectangle(&alphaRect_, edge, 1.f, nullptr);
        rt->drawRectangle(&previewRect_, edge, 1.f, nullptr);
    }

    // markers for the current position in each control
    if (ink)
    {
        const float mx = svRect_.left + hsv_.s * (svRect_.right - svRect_.left);
        const float my = svRect_.top + (1.f - hsv_.v) * (svRect_.bottom - svRect_.top);
        const gmpi::drawing::Rect cross{ mx - 5.f, my - 5.f, mx + 5.f, my + 5.f };
        rt->drawRectangle(&cross, ink, 2.f, nullptr);

        const float hy = hueRect_.top + (hsv_.h / 360.f) * (hueRect_.bottom - hueRect_.top);
        const gmpi::drawing::Rect hmark{ hueRect_.left - 2.f, hy - 2.f, hueRect_.right + 2.f, hy + 2.f };
        rt->drawRectangle(&hmark, ink, 2.f, nullptr);

        const float ay = alphaRect_.top + (1.f - hsv_.a) * (alphaRect_.bottom - alphaRect_.top);
        const gmpi::drawing::Rect amark{ alphaRect_.left - 2.f, ay - 2.f, alphaRect_.right + 2.f, ay + 2.f };
        rt->drawRectangle(&amark, ink, 2.f, nullptr);
    }

    // --- buttons ------------------------------------------------------------
    gmpi::drawing::api::ISolidColorBrush* face{};
    const gmpi::drawing::Color faceC{ 0.24f, 0.25f, 0.28f, 1.f };
    rt->createSolidColorBrush(&faceC, nullptr, &face);
    gmpi::drawing::api::ISolidColorBrush* faceHot{};
    const gmpi::drawing::Color hotC{ 0.30f, 0.44f, 0.68f, 1.f };
    rt->createSolidColorBrush(&hotC, nullptr, &faceHot);

    for (size_t i = 0; i < buttons_.size(); ++i)
    {
        const auto& b = buttons_[i];
        if (auto* fill = (int(i) == hoveredButton_) ? faceHot : face; fill)
            rt->fillRectangle(&b.rect, fill);
        if (edge)
            rt->drawRectangle(&b.rect, edge, 1.f, nullptr);
        if (font_ && ink)
        {
            const gmpi::drawing::Rect lr{ b.rect.left, b.rect.top + 6.f, b.rect.right, b.rect.bottom };
            rt->drawTextU(b.label.c_str(), uint32_t(b.label.size()), font_, &lr, ink, 0);
        }
    }

    if (ink)     ink->release();
    if (edge)    edge->release();
    if (face)    face->release();
    if (faceHot) faceHot->release();

    rt->endDraw();

    gmpi::drawing::api::IBitmap* bmRaw{};
    rt->getBitmap(&bmRaw);
    if (auto* bm = dynamic_cast<gmpi::cpugfx::Bitmap*>(bmRaw))
    {
        const auto& sf = bm->surface;
        const gmpi::cpugfx::SourceSurface src{ sf.pixels, sf.stridePixels, sf.width, sf.height };
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

inline void WaylandColorDialog::destroySurfaces()
{
    if (surface_)
        input_.unregisterSurface(surface_);

    buffer_.release();

    if (frame_)   { libdecor_frame_unref(frame_); frame_ = nullptr; }
    if (surface_) { wl_surface_destroy(surface_); surface_ = nullptr; }
}

inline void WaylandColorDialog::complete(gmpi::ReturnCode result)
{
    if (completed_)
        return;
    completed_ = true;

    destroySurfaces();   // before the callback, as with the stock dialog

    if (auto cb = callback_.as<gmpi::api::IColorDialogCallback>(); cb)
        cb->onComplete(result, color());

    callback_ = {};
    release();
}

// ---------------------------------------------------------------------------
// WaylandKeyListener - IKeyListener
// ---------------------------------------------------------------------------
// "Effectively an invisible text-edit": it takes the keyboard while it is up and
// hands every press and release to its callback, plus the clipboard operations,
// which on Wayland the client has to implement itself.
class WaylandKeyListener : public gmpi::api::IKeyListener, private IKeySink
{
public:
    WaylandKeyListener(Connection& connection, InputDispatch& input)
        : connection_(connection), input_(input) {}

    ~WaylandKeyListener()
    {
        // Detach only. stop() calls release(), and by the time a destructor runs
        // the refcount is already zero - decrementing it again is how a tidy
        // teardown turns into a double free.
        if (input_.keySink() == this)
            input_.setKeySink(nullptr);
    }

    gmpi::ReturnCode showAsync(gmpi::api::IUnknown* callback) override
    {
        callback_ = callback;
        input_.setKeySink(this);

        addRef();   // alive until focus is lost, like every other dialog here
        return gmpi::ReturnCode::Ok;
    }

    gmpi::ReturnCode queryInterface(const gmpi::api::Guid* iid, void** returnInterface) override
    {
        *returnInterface = {};
        GMPI_QUERYINTERFACE(gmpi::api::IKeyListener);
        return gmpi::ReturnCode::NoSupport;
    }
    GMPI_REFCOUNT;

private:
    static bool isClipboardKey(uint32_t keysym)
    {
        switch (keysym)
        {
        case 'c': case 'C': case 'x': case 'X': case 'v': case 'V': return true;
        default: return false;
        }
    }

    void onRawKey(uint32_t keysym, uint32_t utf32, int32_t flags, bool down) override;
    void onSinkReplaced() override { stop(gmpi::ReturnCode::Cancel); }
    void stop(gmpi::ReturnCode result);

    Connection&    connection_;
    InputDispatch& input_;
    gmpi::shared_ptr<gmpi::api::IUnknown> callback_;
    bool stopped_ = false;
};

inline void WaylandKeyListener::stop(gmpi::ReturnCode result)
{
    if (stopped_)
        return;
    stopped_ = true;

    if (input_.keySink() == this)
        input_.setKeySink(nullptr);

    if (auto cb = callback_.as<gmpi::api::IKeyListenerCallback>(); cb)
        cb->onLostFocus(result);

    callback_ = {};
    release();   // balances the addRef in showAsync
}

inline void WaylandKeyListener::onRawKey(uint32_t keysym, uint32_t utf32, int32_t flags, bool down)
{
    auto cb = callback_.as<gmpi::api::IKeyListenerCallback>();
    if (!cb)
        return;

    const bool ctrl = (flags & int32_t(gmpi::api::PointerFlags::KeyControl)) != 0;

    // Clipboard first: the callback owns the text, we own the selection.
    //
    // Both edges are swallowed, but only the press acts. Acting on the release
    // as well would copy twice; delivering the release as an ordinary keystroke
    // would hand the caller an onKeyUp with no matching onKeyDown.
    if (ctrl && isClipboardKey(keysym))
    {
        if (!down)
            return;

        switch (keysym)
        {
        case 'c': case 'C':
        case 'x': case 'X':
        {
            gmpi::ReturnString text;
            if (keysym == 'x' || keysym == 'X')
                cb->cut(&text);
            else
                cb->copy(&text);

            input_.clipboard().setText(connection_.display(),
                                       std::string(text.getData(), size_t(text.getSize())),
                                       input_.lastGrabSerial());
            return;
        }
        case 'v': case 'V':
        {
            const std::string text = input_.clipboard().getText(connection_.display());
            if (!text.empty())
                cb->paste(text.c_str(), text.size());
            return;
        }
        default: break;
        }
    }

    // Escape gives the focus back rather than being delivered as a keystroke,
    // which is what every caller of this expects to end an edit.
    if (down && keysym == 0xff1b)
    {
        stop(gmpi::ReturnCode::Cancel);
        return;
    }

    const int32_t key = utf32 ? int32_t(utf32) : int32_t(keysym);
    if (down)
        cb->onKeyDown(key, flags);
    else
        cb->onKeyUp(key, flags);
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

inline void WaylandFrameBase::drawActiveTextEdit(gmpi::cpugfx::RenderTarget* rt)
{
    if (!activeEdit_)
        return;

    activeEdit_->render(rt);
}

inline gmpi::ReturnCode WaylandFrameBase::createTextEdit(
    const gmpi::drawing::Rect* r, gmpi::api::IUnknown** returnTextEdit)
{
    *returnTextEdit = {};

    if (!menuFont_)
        return gmpi::ReturnCode::NoSupport;   // no font, no caret placement

    auto* edit = new WaylandTextEdit(connection_, inputDispatch(), menuFont_,
                                     r ? *r : gmpi::drawing::Rect{});

    // Only one in-place edit at a time. Opening a second while the first is up
    // would strand it: nothing would draw it, route to it, or ever finish it.
    if (activeEdit_)
        activeEdit_->commit();

    // The frame takes a reference of its own: the caller may release the edit the
    // moment it has called showAsync, and the frame still has to draw it.
    edit->addRef();
    activeEdit_ = edit;

    edit->onFinished = [this](WaylandTextEdit* e)
    {
        if (activeEdit_ != e)
            return;
        activeEdit_ = nullptr;                      // clear BEFORE releasing
        inputDispatch().pointerPreFilter = nullptr;
        e->release();
    };

    // A click inside the edit places the caret; a click outside commits, which is
    // what every in-place rename box does.
    inputDispatch().pointerPreFilter =
        [this](double x, double y, bool pressed) -> bool
        {
            if (!activeEdit_ || !activeEdit_->active())
                return false;

            const auto& box = activeEdit_->rect();
            const bool inside = x >= box.left && x < box.right && y >= box.top && y < box.bottom;

            if (!inside)
            {
                // Clicking away commits, as every in-place rename box does.
                // Without this the edit is never finished: the typed value is
                // discarded and its references are stranded.
                if (pressed)
                    activeEdit_->commit();
                return false;          // and the click still reaches the client
            }

            activeEdit_->onPointer(x, y, pressed);
            invalidateRect(nullptr);
            return true;
        };

    *returnTextEdit = static_cast<gmpi::api::ITextEdit*>(edit);
    return gmpi::ReturnCode::Ok;
}

inline gmpi::ReturnCode WaylandFrameBase::createColorDialog(
    gmpi::drawing::Color initialColor, gmpi::api::IUnknown** returnDialog)
{
    *returnDialog = {};

    if (!decorContext())
        return gmpi::ReturnCode::NoSupport;

    auto* dlg = new WaylandColorDialog(connection_, inputDispatch(), decorContext(),
                                       factory_, menuFont_, initialColor);
    *returnDialog = static_cast<gmpi::api::IColorDialog*>(dlg);
    return gmpi::ReturnCode::Ok;
}

inline gmpi::ReturnCode WaylandFrameBase::createKeyListener(
    const gmpi::drawing::Rect*, gmpi::api::IUnknown** returnKeyListener)
{
    *returnKeyListener = {};

    auto* kl = new WaylandKeyListener(connection_, inputDispatch());
    *returnKeyListener = static_cast<gmpi::api::IKeyListener*>(kl);
    return gmpi::ReturnCode::Ok;
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
        // while this object is still complete, so inputDispatch() resolves
        detachClient();

        // Everything created ON the surface has to go BEFORE the surface does -
        // and the base destructor, which runs next, is what destroys it. Leaving
        // a wp_fractional_scale_v1 pointing at a destroyed wl_surface is enough
        // to segfault mutter 46.2 as the client disconnects, which the user sees
        // as their whole session dying.
        if (fracScale_) { wp_fractional_scale_v1_destroy(fracScale_); fracScale_ = nullptr; }

        // a frame callback still in flight would fire against a dead surface
        if (frameCb_) { wl_callback_destroy(frameCb_); frameCb_ = nullptr; }

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

    gmpi::drawing::Rect toFrameCoordinates(const gmpi::drawing::Rect& r) const override
    {
        if (!frame_)
            return r;

        int left = 0, top = 0, right = 0, bottom = 0;
        libdecor_frame_translate_coordinate(frame_, int(r.left),  int(r.top),    &left,  &top);
        libdecor_frame_translate_coordinate(frame_, int(r.right), int(r.bottom), &right, &bottom);

        return { float(left), float(top), float(right), float(bottom) };
    }

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
    wl_callback*            frameCb_{};   // tracked so teardown can cancel it
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
    self.frameCb_ = nullptr;   // requestFrameCallback below installs the next one

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
    frameCb_ = wl_surface_frame(surface_);
    wl_callback_add_listener(frameCb_, &listener, this);
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
        inputDispatch().clipboard().pump(connection_.display());

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
