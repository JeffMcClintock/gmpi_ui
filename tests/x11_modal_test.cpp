// ---------------------------------------------------------------------------
// One modal dialog per window at a time, on the X11 backend.
//
// Two stock dialogs asked for at once must not both be on screen: the second
// waits until the first is answered. While either is up, the window behind
// takes no pointer or keyboard input. Closing the window drops dialogs still
// waiting without answering them.
//
//   x11_modal_test        (DISPLAY = a private server; see x11_modal_test.sh)
//
// Self-driving: it answers the dialogs and clicks the window itself, with
// XSendEvent on a second connection, and asks the server which windows are
// mapped. No xdotool, no window manager needed.
// ---------------------------------------------------------------------------

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <unistd.h>

// GMPI first, Xlib after (see x11_menu_test.cpp).
#include "backends/DrawingFrameX11.h"
#include "Drawing.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

using namespace gmpi;
using gmpi::hosting::X11DrawingFrame;

namespace
{

int g_pointerDowns = 0;
std::vector<std::string> g_answers; // "<title>:<button>", in the order they came

class DialogCallback : public api::IStockDialogCallback
{
public:
    explicit DialogCallback(const char* title) : title_(title) {}
    void onComplete(api::StockDialogButton button) override
    {
        g_answers.push_back(title_ + ":" + std::to_string(int32_t(button)));
    }
    ReturnCode queryInterface(const api::Guid* iid, void** returnInterface) override
    {
        *returnInterface = {};
        GMPI_QUERYINTERFACE(api::IStockDialogCallback);
        return ReturnCode::NoSupport;
    }
    GMPI_REFCOUNT_NO_DELETE;
private:
    std::string title_;
};

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
        g.clear(drawing::Colors::YellowGreen);
        return ReturnCode::Ok;
    }
    ReturnCode setHover(bool) override { return ReturnCode::Ok; }
    ReturnCode hitTest(drawing::Point, int32_t) override { return ReturnCode::Ok; }
    ReturnCode onPointerDown(drawing::Point, int32_t) override
    {
        ++g_pointerDowns;
        return ReturnCode::Ok;
    }
    ReturnCode onPointerMove(drawing::Point, int32_t) override { return ReturnCode::Ok; }
    ReturnCode onPointerUp(drawing::Point, int32_t) override { return ReturnCode::Ok; }
    ReturnCode onMouseWheel(drawing::Point, int32_t, int32_t) override { return ReturnCode::Ok; }
    ReturnCode onKeyPress(wchar_t) override { return ReturnCode::Ok; }
    ReturnCode getToolTip(drawing::Point, api::IString*) override { return ReturnCode::Unhandled; }
    ReturnCode populateContextMenu(drawing::Point, api::IUnknown*) override { return ReturnCode::Unhandled; }

    ReturnCode queryInterface(const api::Guid* iid, void** returnInterface) override
    {
        *returnInterface = {};
        GMPI_QUERYINTERFACE(api::IDrawingClient);
        GMPI_QUERYINTERFACE(api::IInputClient);
        return ReturnCode::NoSupport;
    }
    GMPI_REFCOUNT_NO_DELETE;
};

int g_failures = 0;
void check(bool ok, const char* what)
{
    std::printf("%s: %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok)
        ++g_failures;
}

// The host's run loop stand-in: events when there are any, then the timer.
void pump(X11DrawingFrame& frame)
{
    for (int i = 0; i < 20; ++i)
    {
        frame.processEvents();
        frame.onTimer();
        usleep(10 * 1000);
    }
}

// A mapped top-level window with this name, or 0.
Window viewableNamed(Display* q, const char* name)
{
    Window rootRet{}, parentRet{};
    Window* children{};
    unsigned int n{};
    Window found = 0;
    if (!XQueryTree(q, DefaultRootWindow(q), &rootRet, &parentRet, &children, &n))
        return 0;
    for (unsigned int i = 0; i < n && !found; ++i)
    {
        char* wmName{};
        if (XFetchName(q, children[i], &wmName) && wmName)
        {
            XWindowAttributes a{};
            if (!std::strcmp(wmName, name) && XGetWindowAttributes(q, children[i], &a) && a.map_state == IsViewable)
                found = children[i];
            XFree(wmName);
        }
    }
    if (children)
        XFree(children);
    return found;
}

void sendKey(Display* q, Window w, KeySym sym)
{
    for (int type : { KeyPress, KeyRelease })
    {
        XEvent e{};
        e.xkey.type = type;
        e.xkey.display = q;
        e.xkey.window = w;
        e.xkey.root = DefaultRootWindow(q);
        e.xkey.keycode = XKeysymToKeycode(q, sym);
        e.xkey.same_screen = True;
        XSendEvent(q, w, True, type == KeyPress ? KeyPressMask : KeyReleaseMask, &e);
    }
    XSync(q, False);
}

void sendClick(Display* q, Window w, int x, int y)
{
    for (int type : { ButtonPress, ButtonRelease })
    {
        XEvent e{};
        e.xbutton.type = type;
        e.xbutton.display = q;
        e.xbutton.window = w;
        e.xbutton.root = DefaultRootWindow(q);
        e.xbutton.x = x;
        e.xbutton.y = y;
        e.xbutton.button = Button1;
        e.xbutton.same_screen = True;
        XSendEvent(q, w, True, type == ButtonPress ? ButtonPressMask : ButtonReleaseMask, &e);
    }
    XSync(q, False);
}

bool showStock(X11DrawingFrame& frame, const char* title, DialogCallback* callback)
{
    auto* dialogHost = static_cast<api::IDialogHost*>(&frame);
    api::IUnknown* raw{};
    if (dialogHost->createStockDialog(int32_t(api::StockDialogType::YesNo), title, "Modal?", &raw) != ReturnCode::Ok || !raw)
        return false;
    shared_ptr<api::IUnknown> owner;
    owner.attach(raw);
    auto dlg = owner.as<api::IStockDialog>();
    return dlg && dlg->showAsync(callback) == ReturnCode::Ok;
}

} // namespace

int main()
{
    Display* dpy = XOpenDisplay(nullptr);
    Display* q = XOpenDisplay(nullptr); // the "user": probes and sends events
    if (!dpy || !q)
    {
        std::fprintf(stderr, "SKIP: no X display\n");
        return 77;
    }

    // Never mapped: the frame's window is a child of it, so it gets events
    // but nothing of it is drawn. Only the dialogs (top-level) map.
    const Window parent = XCreateSimpleWindow(dpy, DefaultRootWindow(dpy), 0, 0, 300, 200, 0, 0, 0);
    XSync(dpy, False);

    StubClient client;
    X11DrawingFrame frame;
    frame.attachClient(&client);
    if (!frame.open(static_cast<uintptr_t>(parent), 300, 200))
    {
        std::fprintf(stderr, "FAIL: frame.open failed\n");
        return 1;
    }
    pump(frame);

    Window frameWindow = 0;
    {
        Window r{}, p{};
        Window* children{};
        unsigned int n{};
        if (XQueryTree(q, parent, &r, &p, &children, &n) && n > 0)
            frameWindow = children[0];
        if (children)
            XFree(children);
    }
    check(frameWindow != 0, "the frame made a window under the parent");

    sendClick(q, frameWindow, 50, 50);
    pump(frame);
    check(g_pointerDowns == 1, "with no dialog up, a click reaches the client");

    DialogCallback first("First"), second("Second");
    check(showStock(frame, "First", &first), "first dialog shown");
    check(showStock(frame, "Second", &second), "second dialog asked for while the first is up");
    pump(frame);

    const Window firstWindow = viewableNamed(q, "First");
    check(firstWindow != 0, "the first dialog is on screen");
    check(viewableNamed(q, "Second") == 0, "the second waits: not on screen yet");

    sendClick(q, frameWindow, 50, 50);
    pump(frame);
    check(g_pointerDowns == 1, "behind a dialog, a click does not reach the client");

    sendKey(q, firstWindow, XK_Escape);
    pump(frame);
    check(g_answers.size() == 1 && g_answers[0].rfind("First:", 0) == 0, "answering the first answers only the first");
    check(viewableNamed(q, "First") == 0, "the first dialog is gone");
    const Window secondWindow = viewableNamed(q, "Second");
    check(secondWindow != 0, "the second dialog is on screen once the first is answered");

    sendClick(q, frameWindow, 50, 50);
    pump(frame);
    check(g_pointerDowns == 1, "still blocked while the second is up");

    sendKey(q, secondWindow, XK_Return);
    pump(frame);
    check(g_answers.size() == 2 && g_answers[1].rfind("Second:", 0) == 0, "the second is answered, in order");

    sendClick(q, frameWindow, 50, 50);
    pump(frame);
    check(g_pointerDowns == 2, "with the last answered, clicks reach the client again");

    // Closing the window with one up and one waiting: the waiting one is
    // dropped unanswered, and nothing crashes.
    DialogCallback third("Third"), fourth("Fourth");
    check(showStock(frame, "Third", &third) && showStock(frame, "Fourth", &fourth), "two more asked for");
    pump(frame);
    check(viewableNamed(q, "Third") != 0 && viewableNamed(q, "Fourth") == 0, "again only the first of them is up");
    frame.close();
    pump(frame);
    bool fourthAnswered = false;
    for (const auto& a : g_answers)
        fourthAnswered |= a.rfind("Fourth:", 0) == 0;
    check(!fourthAnswered, "closing the window drops the waiting dialog without answering it");

    for (const auto& a : g_answers)
        std::printf("answer %s\n", a.c_str());

    XDestroyWindow(dpy, parent);
    XCloseDisplay(q);
    XCloseDisplay(dpy);

    std::printf(g_failures ? "FAILED (%d)\n" : "PASSED\n", g_failures);
    return g_failures ? 1 : 0;
}
