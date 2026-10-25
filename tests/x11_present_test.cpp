// ---------------------------------------------------------------------------
// Does the X11 frame put the client's drawing in its window? Built twice by
// x11_present_test.sh: the CPU present (XPutImage of a cpugfx target) and,
// with -DGMPI_UI_GPU_BACKEND=1 and GMPI_UI_GPU=1, the GPU present (a Vulkan
// swap chain on the window, through gpugfx::WindowPresenter).
//
//   x11_present_test <out.ppm>
//
// Draws a known picture - flat background, an opaque rectangle, a
// translucent path fill over both - presents it, reads the window back with
// XGetImage, checks the flat colours and writes the pixels for the script to
// compare across the two builds.
// ---------------------------------------------------------------------------

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>

// GMPI first, Xlib after (see x11_menu_test.cpp).
#include "backends/DrawingFrameX11.h"
#include "Drawing.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>

using namespace gmpi;
using gmpi::hosting::X11DrawingFrame;

namespace
{

int g_renders = 0;

class StubClient : public api::IDrawingClient
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
        ++g_renders;
        drawing::Graphics g(dc);
        g.clear(drawing::Color{ 0.2f, 0.4f, 0.1f, 1.0f });
        auto red = g.createSolidColorBrush(drawing::Color{ 1.0f, 0.0f, 0.0f, 1.0f });
        g.fillRectangle({ 20.0f, 20.0f, 120.0f, 100.0f }, red);

        auto geometry = g.getFactory().createPathGeometry();
        auto sink = geometry.open();
        for (int k = 0; k < 64; ++k)
        {
            const float a = float(k) * 6.2831853f / 64.0f;
            const drawing::Point p{ 200.0f + 70.0f * std::cos(a), 110.0f + 60.0f * std::sin(a) };
            if (k == 0)
                sink.beginFigure(p, drawing::FigureBegin::Filled);
            else
                sink.addLine(p);
        }
        sink.endFigure(drawing::FigureEnd::Closed);
        sink.close();
        auto blue = g.createSolidColorBrush(drawing::Color{ 0.0f, 0.2f, 1.0f, 0.5f });
        g.fillGeometry(geometry, blue);
        return ReturnCode::Ok;
    }
    ReturnCode queryInterface(const api::Guid* iid, void** returnInterface) override
    {
        *returnInterface = {};
        GMPI_QUERYINTERFACE(api::IDrawingClient);
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

struct Rgb { int r, g, b; };

Rgb pixel(XImage* image, int x, int y)
{
    const unsigned long p = XGetPixel(image, x, y);
    return { int((p & image->red_mask) >> __builtin_ctzl(image->red_mask)),
             int((p & image->green_mask) >> __builtin_ctzl(image->green_mask)),
             int((p & image->blue_mask) >> __builtin_ctzl(image->blue_mask)) };
}

int g_xErrors = 0;
int countXError(Display*, XErrorEvent* e)
{
    ++g_xErrors;
    std::printf("X error: request %d, code %d\n", int(e->request_code), int(e->error_code));
    return 0;
}

// What we read back of `window`, through a pixmap of our own: on a rootless
// Xwayland each top-level is redirected and GetImage of the window itself is
// a BadMatch, while CopyArea from it is allowed.
XImage* readBack(Display* q, Window window, int w, int h)
{
    XWindowAttributes a{};
    XGetWindowAttributes(q, window, &a);
    const Pixmap copy = XCreatePixmap(q, window, unsigned(w), unsigned(h), unsigned(a.depth));
    GC gc = XCreateGC(q, copy, 0, nullptr);
    XCopyArea(q, window, copy, gc, 0, 0, unsigned(w), unsigned(h), 0, 0);
    XImage* image = XGetImage(q, copy, 0, 0, unsigned(w), unsigned(h), AllPlanes, ZPixmap);
    XFreeGC(q, gc);
    XFreePixmap(q, copy);
    return image;
}

// Does this server give back what a window holds? Xvfb does. A rootless
// Xwayland with no compositor running for it does not: its windows have no
// shell role, and what is read back is black.
bool serverReadsBackWindows(Display* q)
{
    const Window w = XCreateSimpleWindow(q, DefaultRootWindow(q), 0, 0, 16, 16, 0, 0, 0);
    XMapWindow(q, w);
    XSync(q, False);
    usleep(100 * 1000);
    GC gc = XCreateGC(q, w, 0, nullptr);
    XSetForeground(q, gc, 0xff3366);
    XFillRectangle(q, w, gc, 0, 0, 16, 16);
    XFreeGC(q, gc);
    XSync(q, False);
    bool ok = false;
    if (XImage* image = readBack(q, w, 16, 16))
    {
        ok = (XGetPixel(image, 8, 8) & 0xffffff) == 0xff3366;
        XDestroyImage(image);
    }
    XDestroyWindow(q, w);
    XSync(q, False);
    return ok;
}

bool near(Rgb a, Rgb b, int tolerance)
{
    return std::abs(a.r - b.r) <= tolerance && std::abs(a.g - b.g) <= tolerance && std::abs(a.b - b.b) <= tolerance;
}

} // namespace

int main(int argc, char** argv)
{
    const char* outPath = argc > 1 ? argv[1] : nullptr;
    Display* q = XOpenDisplay(nullptr);
    if (!q)
    {
        std::fprintf(stderr, "SKIP: no X display\n");
        return 77;
    }

    // Mapped, so the window is viewable and a swap chain on it presents. On
    // the private server the script runs this on, mapped is not on screen.
    XSetErrorHandler(countXError);
    const bool readsBack = serverReadsBackWindows(q);

    Display* dpy = XOpenDisplay(nullptr);
    const Window parent = XCreateSimpleWindow(dpy, DefaultRootWindow(dpy), 0, 0, 300, 200, 0, 0, 0);
    XMapWindow(dpy, parent);
    XSync(dpy, False);

    StubClient client;
    X11DrawingFrame frame;
    frame.attachClient(&client);
    if (!frame.open(static_cast<uintptr_t>(parent), 300, 200))
    {
        std::fprintf(stderr, "FAIL: frame.open failed\n");
        return 1;
    }

    for (int i = 0; i < 100; ++i)
    {
        frame.processEvents();
        frame.onTimer();
        usleep(10 * 1000);
    }
    check(g_renders > 0, "the client was asked to render");

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
    if (!readsBack)
    {
        std::printf("note: this X server gives back no window contents (rootless Xwayland with no compositor); "
                    "pixels unchecked\n");
        check(g_xErrors == 0, "presenting raised no X errors");
        frame.close();
        XDestroyWindow(dpy, parent);
        XCloseDisplay(dpy);
        XCloseDisplay(q);
        std::printf(g_failures ? "FAILED (%d)\n" : "PASSED\n", g_failures);
        return g_failures ? 1 : 0;
    }
    XImage* image = frameWindow ? readBack(q, frameWindow, 300, 200) : nullptr;
    check(image != nullptr, "the frame's window can be read back");
    if (image)
    {
        // sRGB 8-bit of the linear colours drawn: 0.2/0.4/0.1 -> 124,170,89;
        // red is red. Loose: the CPU and GPU paths dither differently.
        const Rgb background = pixel(image, 280, 10);
        const Rgb rect = pixel(image, 60, 60);
        std::printf("background %d,%d,%d  rect %d,%d,%d  ellipse %d,%d,%d\n",
                    background.r, background.g, background.b, rect.r, rect.g, rect.b,
                    pixel(image, 200, 110).r, pixel(image, 200, 110).g, pixel(image, 200, 110).b);
        check(near(background, { 124, 170, 89 }, 4), "the background is the cleared colour");
        check(near(rect, { 255, 0, 0 }, 2), "the rectangle is red");
        check(!near(pixel(image, 200, 110), background, 10), "the translucent fill shows over the background");

        if (outPath)
        {
            if (FILE* f = std::fopen(outPath, "wb"))
            {
                std::fprintf(f, "P6\n300 200\n255\n");
                for (int y = 0; y < 200; ++y)
                    for (int x = 0; x < 300; ++x)
                    {
                        const Rgb c = pixel(image, x, y);
                        std::fputc(c.r, f);
                        std::fputc(c.g, f);
                        std::fputc(c.b, f);
                    }
                std::fclose(f);
            }
        }
        XDestroyImage(image);
    }
    check(g_xErrors == 0, "presenting raised no X errors");

    frame.close();
    XDestroyWindow(dpy, parent);
    XCloseDisplay(dpy);
    XCloseDisplay(q);

    std::printf(g_failures ? "FAILED (%d)\n" : "PASSED\n", g_failures);
    return g_failures ? 1 : 0;
}
