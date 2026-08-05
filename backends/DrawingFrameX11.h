#pragma once

// ---------------------------------------------------------------------------
// X11DrawingFrame - an IDrawingClient painted into a window EMBEDDED in some
// other application's window.
//
// This exists for plugins. VST3 defines two Linux embeddings:
// kPlatformTypeX11EmbedWindowID (an X11 Window id, available in every version)
// and kPlatformTypeWaylandSurfaceID (added in 3.8.0, handled by
// WaylandSubsurfaceFrame in DrawingFrameWayland.h). This is the X11 one, and it
// remains the path for any host that offers no Wayland - including a
// Wayland-native host that embeds its plugins through XWayland. See
// docs/vst3-linux-editor.md.
//
// Two rules follow from being a guest inside a host process, and they shape the
// whole design:
//
//  1. NO EVENT LOOP OF OUR OWN. The host owns the loop. It polls connectionFd()
//     and calls processEvents(); it ticks onTimer(). Nothing here ever blocks,
//     and nothing here calls XNextEvent speculatively.
//
//  2. NO GLOBAL STATE. A host may load several plugins, each with its own
//     frame, all sharing one process. Every frame opens its own Display.
//
// All Xlib lives in the .cpp. Xlib's headers #define None, Status, Bool and
// friends, which detonate on contact with almost any C++ enum - keeping them
// out of this header keeps them out of every consumer.
// ---------------------------------------------------------------------------

#include <memory>

#include "backends/CpuGfx.h"
#include "backends/PopupMenuModel.h"
#include "helpers/NativeUi.h"
#include "GmpiSdkCommon.h"

namespace gmpi::hosting
{

class X11DrawingFrame : public gmpi::api::IDrawingHost,
                        public gmpi::api::IInputHost,
                        public gmpi::api::IDialogHost
{
public:
    X11DrawingFrame();
    ~X11DrawingFrame();

    X11DrawingFrame(const X11DrawingFrame&) = delete;
    X11DrawingFrame& operator=(const X11DrawingFrame&) = delete;

    // The CPU backend ships no font, shaping or image-decode code, so the caller
    // wires those in - same contract as the Wayland frame:
    //
    //     frame.drawingFactory().textEngine   = &textEngine;
    //     frame.drawingFactory().imageDecoder = gmpi::drawing::decodeImageFile;
    //
    // Without a text engine geometry draws and text silently does not.
    gmpi::cpugfx::Factory& drawingFactory() { return factory_; }

    // Anything this frame does not implement itself - IEditorHost above all -
    // is forwarded here. A plugin's parameter pins take their host from a
    // queryInterface for IEditorHost during setHost; without this they are left
    // null and the first knob drag dereferences one. Same arrangement as
    // DxDrawingFrameBase::setFallbackHost on Windows.
    void setFallbackHost(gmpi::api::IUnknown* paramHost) { parameterHost_ = paramHost; }

    void attachClient(gmpi::api::IDrawingClient* client);
    void detachClient();

    // parentWindowId is an X11 Window (XID), widened to uintptr_t so this header
    // need not name Xlib's types. Returns false if there is no usable X display -
    // a headless host, or a Wayland session with no XWayland.
    bool open(uintptr_t parentWindowId, int width, int height);
    void close();
    bool isOpen() const;

    // Register this with the host's run loop (VST3: Linux::IRunLoop). -1 when closed.
    int connectionFd() const;

    // Drain every X event that has arrived, then repaint if anything went dirty.
    // Safe to call when nothing is pending.
    void processEvents();

    // Periodic tick from the host's run loop. Drives the hover/tooltip delay and
    // flushes any invalidation the plugin made from outside an input event (a
    // parameter moving under automation, say). ~16ms is a good period.
    void onTimer();

    // Host told us the view changed size.
    void reSize(int width, int height);

    // Physical pixel size the client asked for, after measure(). Used by
    // IPlugView::getSize.
    int width() const;
    int height() const;

    // --- IDrawingHost ---
    gmpi::ReturnCode getDrawingFactory(gmpi::api::IUnknown** returnFactory) override
    {
        *returnFactory = static_cast<gmpi::api::IUnknown*>(&factory_);
        return gmpi::ReturnCode::Ok;
    }
    void invalidateRect(const gmpi::drawing::Rect* invalidRect) override;
    void invalidateMeasure() override;
    float getRasterizationScale() override;

    // --- IInputHost ---
    // Unlike Wayland, X11 has a real pointer grab, so capture is honoured
    // properly: a drag that leaves the plugin window keeps delivering motion.
    gmpi::ReturnCode setCapture() override;
    gmpi::ReturnCode getCapture(bool& returnValue) override;
    gmpi::ReturnCode releaseCapture() override;

    // --- IDialogHost ---
    // Menus are DRAWN here, not handed to a widget: X11 supplies an
    // override-redirect window and a pointer grab and nothing else, exactly as
    // Wayland supplies a positioned grabbing surface and nothing else. GTK and
    // Qt draw their own for the same reason.
    //
    // Text needs a format the CPU backend cannot invent - see setMenuFont. A
    // menu with no font draws its boxes and highlight and no labels, which is
    // a confusing enough failure to be worth stating.
    gmpi::ReturnCode createPopupMenu(const gmpi::drawing::Rect* r,
                                     gmpi::api::IUnknown** returnPopupMenu) override;

    // Not implemented on X11 yet. Explicitly NoSupport with the out-param
    // cleared, rather than left to a null dereference in the caller: a plugin
    // that asks for an in-place edit should find out, not crash. The colour
    // picker is deliberately last in the queue - plugins rarely want one.
    gmpi::ReturnCode createTextEdit(const gmpi::drawing::Rect*, gmpi::api::IUnknown** r) override
    { *r = {}; return gmpi::ReturnCode::NoSupport; }
    gmpi::ReturnCode createKeyListener(const gmpi::drawing::Rect*, gmpi::api::IUnknown** r) override
    { *r = {}; return gmpi::ReturnCode::NoSupport; }
    gmpi::ReturnCode createFileDialog(int32_t, gmpi::api::IUnknown** r) override
    { *r = {}; return gmpi::ReturnCode::NoSupport; }
    // A message box: a transient-for toplevel, decorated by the window manager.
    gmpi::ReturnCode createStockDialog(int32_t dialogType, const char* title, const char* text,
                                       gmpi::api::IUnknown** returnDialog) override;
    gmpi::ReturnCode createColorDialog(gmpi::drawing::Color, gmpi::api::IUnknown** r) override
    { *r = {}; return gmpi::ReturnCode::NoSupport; }

    // The font menus draw their labels with. The CPU backend ships no font code,
    // so the application or plugin wrapper supplies this, exactly as it supplies
    // the text engine.
    void setMenuFont(gmpi::drawing::api::ITextFormat* font);

    // Right-click that the drawing client did not claim: ask it to populate a
    // menu and show it. Called from the button handler; exposed so an app can
    // raise the same menu from a keyboard shortcut.
    void showContextMenu(gmpi::drawing::Point point);

    // --- IUnknown ---
    // The editor owns the frame for its whole life, so it is not refcounted -
    // same arrangement as the Windows and Wayland frames.
    gmpi::ReturnCode queryInterface(const gmpi::api::Guid* iid, void** returnInterface) override
    {
        *returnInterface = {};
        GMPI_QUERYINTERFACE(gmpi::api::IDrawingHost);
        GMPI_QUERYINTERFACE(gmpi::api::IInputHost);
        GMPI_QUERYINTERFACE(gmpi::api::IDialogHost);

        if (parameterHost_)
            return parameterHost_->queryInterface(iid, returnInterface);

        return gmpi::ReturnCode::NoSupport;
    }
    GMPI_REFCOUNT_NO_DELETE;

private:
    // Menus are drawn by this backend, share the frame's Display, and are
    // serviced from its event pump - so they need at the frame's internals.
    friend class X11PopupMenu;
    friend class X11StockDialog;

    struct Impl;
    std::unique_ptr<Impl> impl_;
    gmpi::cpugfx::Factory factory_;
    gmpi::api::IUnknown* parameterHost_{};
};

} // namespace gmpi::hosting
