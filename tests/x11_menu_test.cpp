// ---------------------------------------------------------------------------
// Does a right-click actually put a context menu on screen, on the X11 backend?
//
// Nothing else can answer that. No GMPI example plugin implements
// populateContextMenu - the PluginEditor base returns Unhandled - so driving
// this through a real plugin would only ever exercise the "client declined"
// path. So the client here is a stub that DOES populate a menu, and the test
// drives the backend directly.
//
//   x11_menu_test [seconds]
//
// It maps a window, waits, and lets an external xdotool right-click it (see
// x11_menu_test.sh). Prints what it observed; the script diffs screenshots.
// ---------------------------------------------------------------------------

#include <cstdio>
#include <cstring>
#include <span>
#include <string_view>
#include <unistd.h>

// GMPI FIRST, Xlib after. Xlib's headers #define Bool, None, Status and Success,
// and gmpi::api::PinDatatype has a `Bool` enumerator - including Xlib first turns
// it into `int` and the error lands twenty headers away from the cause. This is
// the whole reason DrawingFrameX11.h keeps Xlib out of its own header.
#include "backends/DrawingFrameX11.h"
#include "helpers/CpuTextEngine.h"
#include "helpers/DecodeImage.h"
#include "helpers/FontProvider.h"
#include "Drawing.h"

#include <X11/Xlib.h>

using namespace gmpi;

namespace
{

// Counts what the frame asked of us, so the test can distinguish "the menu
// never opened" from "the client was never asked".
int g_populateCalls = 0;
int g_chosenId = -1;
int g_dialogAnswer = -1;

class MenuCallback : public api::IPopupMenuCallback
{
public:
    void onComplete(ReturnCode result, int32_t selectedID) override
    {
        if (result == ReturnCode::Ok)
            g_chosenId = selectedID;
    }
    ReturnCode queryInterface(const api::Guid* iid, void** returnInterface) override
    {
        *returnInterface = {};
        GMPI_QUERYINTERFACE(api::IPopupMenuCallback);
        return ReturnCode::NoSupport;
    }
    GMPI_REFCOUNT_NO_DELETE;
};

MenuCallback g_callback;

class DialogCallback : public api::IStockDialogCallback
{
public:
    void onComplete(api::StockDialogButton button) override
    {
        g_dialogAnswer = int32_t(button);
    }
    ReturnCode queryInterface(const api::Guid* iid, void** returnInterface) override
    {
        *returnInterface = {};
        GMPI_QUERYINTERFACE(api::IStockDialogCallback);
        return ReturnCode::NoSupport;
    }
    GMPI_REFCOUNT_NO_DELETE;
};

DialogCallback g_dialogCallback;

// A client that draws a flat colour and offers a context menu. Deliberately
// minimal: this is a test of the frame, not of a widget toolkit.
class StubClient : public api::IDrawingClient, public api::IInputClient
{
public:
    ReturnCode setHost(api::IUnknown*) override { return ReturnCode::Ok; }

    ReturnCode measure(const drawing::Size* available, drawing::Size* desired) override
    {
        *desired = *available;
        return ReturnCode::Ok;
    }
    ReturnCode arrange(const drawing::Rect*) override { return ReturnCode::Ok; }
    ReturnCode getClipArea(drawing::Rect*) override { return ReturnCode::Unhandled; }

    ReturnCode render(drawing::api::IDeviceContext* dc) override
    {
        drawing::Graphics g(dc);
        g.clear(drawing::Colors::YellowGreen);   // never the menu's dark grey
        return ReturnCode::Ok;
    }

    ReturnCode setHover(bool) override { return ReturnCode::Ok; }
    ReturnCode hitTest(drawing::Point, int32_t) override { return ReturnCode::Ok; }
    ReturnCode onPointerMove(drawing::Point, int32_t) override { return ReturnCode::Ok; }
    ReturnCode onPointerUp(drawing::Point, int32_t) override { return ReturnCode::Ok; }
    ReturnCode onMouseWheel(drawing::Point, int32_t, int32_t) override { return ReturnCode::Ok; }
    ReturnCode onKeyPress(wchar_t) override { return ReturnCode::Ok; }
    ReturnCode getToolTip(drawing::Point, api::IString*) override { return ReturnCode::Unhandled; }

    // Unhandled is what lets the frame offer a context menu - the same gate the
    // Windows backend uses.
    ReturnCode onPointerDown(drawing::Point, int32_t) override { return ReturnCode::Unhandled; }

    ReturnCode populateContextMenu(drawing::Point, api::IUnknown* sink) override
    {
        ++g_populateCalls;

        auto* menu = static_cast<api::IContextItemSink*>(nullptr);
        if (!sink || sink->queryInterface(&api::IContextItemSink::guid,
                                          reinterpret_cast<void**>(&menu)) != ReturnCode::Ok || !menu)
            return ReturnCode::Fail;

        using F = api::PopupMenuFlags;
        menu->addItem("Copy",       1, 0, &g_callback);
        menu->addItem("Paste",      2, int32_t(F::Grayed), &g_callback);
        menu->addItem("", 0, int32_t(F::Separator), nullptr);
        menu->addItem("Show Grid",  3, int32_t(F::Ticked), &g_callback);
        menu->addItem("Arrange",    0, int32_t(F::SubMenuBegin), nullptr);
        menu->addItem("Align Left", 10, 0, &g_callback);
        menu->addItem("Align Top",  11, 0, &g_callback);
        menu->addItem("", 0, int32_t(F::SubMenuEnd), nullptr);
        menu->addItem("Properties...", 4, 0, &g_callback);
        menu->release();

        return ReturnCode::Ok;
    }

    ReturnCode queryInterface(const api::Guid* iid, void** returnInterface) override
    {
        *returnInterface = {};
        GMPI_QUERYINTERFACE(api::IDrawingClient);
        GMPI_QUERYINTERFACE(api::IInputClient);
        return ReturnCode::NoSupport;
    }
    GMPI_REFCOUNT_NO_DELETE;
};

} // anonymous namespace

int main(int argc, char** argv)
{
    const int seconds = (argc > 1) ? atoi(argv[1]) : 6;

    Display* dpy = XOpenDisplay(nullptr);
    if (!dpy)
    {
        std::fprintf(stderr, "SKIP: no X display\n");
        return 77;
    }

    const int screen = DefaultScreen(dpy);
    Window parent = XCreateSimpleWindow(dpy, RootWindow(dpy, screen), 0, 0, 300, 200, 0,
                                        BlackPixel(dpy, screen), BlackPixel(dpy, screen));
    XSelectInput(dpy, parent, ExposureMask | StructureNotifyMask);
    XMapWindow(dpy, parent);
    XFlush(dpy);

    hosting::X11DrawingFrame frame;

    // The CPU backend brings no font code; wire the same stack the plugin
    // wrappers do, or the menu draws its boxes and no labels.
    static drawing::CpuTextEngine textEngine{ drawing::findFont };
    textEngine.imageDecoder = drawing::decodeImageMemory;
    frame.drawingFactory().textEngine   = &textEngine;
    frame.drawingFactory().imageDecoder = drawing::decodeImageFile;

    drawing::Factory facade;
    *drawing::AccessPtr::put(facade) = &frame.drawingFactory();
    const std::string_view family{ "sans-serif" };
    auto menuFont = facade.createTextFormat(14.0f, std::span{ &family, 1 });
    frame.setMenuFont(drawing::AccessPtr::get(menuFont));

    StubClient client;
    frame.attachClient(&client);

    if (!frame.open(static_cast<uintptr_t>(parent), 300, 200))
    {
        std::fprintf(stderr, "FAIL: frame.open failed\n");
        return 1;
    }
    std::printf("frame open, %dx%d\n", frame.width(), frame.height());
    std::fflush(stdout);

    // No event loop of our own beyond this: the frame never blocks, so a plain
    // poll-and-sleep stands in for the host's run loop.
    const int ticks = seconds * 100;
    bool dialogRaised = false;

    for (int i = 0; i < ticks; ++i)
    {
        // Two thirds of the way through, after the menu has been exercised,
        // raise a message box so the same run covers both. A plugin asking its
        // host for one is the case that used to return NoSupport.
        if (!dialogRaised && i == (ticks * 2) / 3)
        {
            dialogRaised = true;

            api::IUnknown* raw{};
            auto* dialogHost = static_cast<api::IDialogHost*>(&frame);
            if (dialogHost->createStockDialog(int32_t(api::StockDialogType::YesNo),
                                              "Question", "Discard changes?", &raw) == ReturnCode::Ok && raw)
            {
                shared_ptr<api::IUnknown> owner;
                owner.attach(raw);
                if (auto dlg = owner.as<api::IStockDialog>(); dlg)
                {
                    std::printf("stock dialog created\n");
                    std::fflush(stdout);
                    dlg->showAsync(&g_dialogCallback);
                }
            }
            else
            {
                std::printf("stock dialog NOT created\n");
                std::fflush(stdout);
            }
        }

        frame.processEvents();
        frame.onTimer();
        usleep(10 * 1000);
    }

    std::printf("populateContextMenu called: %d\n", g_populateCalls);
    std::printf("item chosen: %d\n", g_chosenId);
    std::printf("dialog answer: %d\n", g_dialogAnswer);

    // The file chooser goes through the XDG portal - D-Bus, not X11. Check the
    // part this backend actually owns: that createFileDialog hands back a
    // dialog and that the bus it will answer on is live and therefore being
    // pumped. Deliberately NOT calling showAsync: the portal renders on the
    // real desktop session, and a test must not put a window on the user's
    // screen. The request/response half is PortalFileDialog, unchanged and
    // already exercised by the Wayland frame.
    {
        auto* dialogHost = static_cast<api::IDialogHost*>(&frame);
        api::IUnknown* raw{};
        const auto rc = dialogHost->createFileDialog(0, &raw);
        std::printf("createFileDialog: %s, portal fd %d\n",
                    rc == ReturnCode::Ok ? "Ok" : "FAILED", frame.portalFd());
        if (raw)
        {
            shared_ptr<api::IUnknown> owner;
            owner.attach(raw);
            std::printf("IFileDialog interface: %s\n",
                        owner.as<api::IFileDialog>() ? "yes" : "no");
        }
    }

    frame.close();
    XDestroyWindow(dpy, parent);
    XCloseDisplay(dpy);

    if (g_populateCalls == 0)
    {
        std::fprintf(stderr, "FAIL: the frame never asked the client for menu items\n");
        return 1;
    }

    std::printf("PASS\n");
    return 0;
}
