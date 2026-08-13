// ---------------------------------------------------------------------------
// X11DrawingFrame::Impl::present() caches the surface extents across three
// re-entrant client calls. TIDE BACKLOG P7c.
//
// present() reads pw/ph from d.width/d.height, calls ensureImage(pw, ph), and
// then hands control to the client three times - measure(), arrange() and
// render(). Only after all three does it build the destination surface, and it
// builds it from d.image->data and d.image->bytes_per_line (both RE-READ, so a
// replaced image is picked up) sized by pw/ph (both STALE). If anything in
// those three calls drove a nested present() at a smaller size, the image
// underneath was freed and reallocated smaller, and encodeDirtyRect then writes
// pw x ph pixels into it at the new, shorter stride - past the end of the
// allocation. A heap overflow, not a null dereference, so it is a worse outcome
// than the P4 crash it is a cousin of.
//
// WHAT THIS TEST IS, AND IS NOT
//
// It is a POSITIVE CONTROL for the guard, not a reproduction of something a
// host does today. The audit in docs/x11-present-extents.md concludes the
// nesting is unreachable through gmpi_ui's own code: there is exactly one
// XNextEvent in the backend, present() has exactly two callers (processEvents
// and onTimer), and neither is reachable from a client - IDrawingHost,
// IInputHost and IDialogHost expose no way to pump the loop. This test reaches
// it the only way anything can, by handing the client a pointer to the concrete
// frame, which a real client never has. That is the point: it proves the guard
// is load-bearing rather than decorative, on a path a future host or backend
// change could otherwise open up silently.
//
// THE DETECTOR
//
// Unfixed, this dies on a signal; fixed, it exits 0. Which signal depends on
// how the image was allocated, and BOTH paths are worth knowing:
//
//   MIT-SHM (the normal path on a local display, including Xvfb) - image->data
//   is a shmat() mapping of its own, so a large overflow walks off the end of
//   that mapping into unmapped address space: SIGSEGV, no sanitizer needed.
//   AddressSanitizer does NOT instrument a shm segment, so an ASan run of this
//   path is a false negative.
//
//   XCreateImage fallback (no MIT-SHM: a remote display) - image->data is
//   malloc'd, which ASan does cover.
//
// The sizes below are chosen so the SHM overflow is ~150 KB past a ~12 KB
// segment, far enough that it cannot land inside a neighbouring mapping and
// pass silently.
//
//   ./tests/run_x11_present_reentrant_test.sh
// ---------------------------------------------------------------------------

#include <cstdio>
#include <cstdlib>

// GMPI first, Xlib second - Xlib #defines Bool/None/Status/Success and
// gmpi::api::PinDatatype has a Bool enumerator. Same ordering rule as
// x11_menu_test.cpp, and the reason DrawingFrameX11.h keeps Xlib out of itself.
#include "backends/DrawingFrameX11.h"
#include "helpers/CpuTextEngine.h"
#include "helpers/DecodeImage.h"
#include "helpers/FontProvider.h"
#include "Drawing.h"

#include <X11/Xlib.h>

using namespace gmpi;

namespace
{

// Big enough that the stale-extent blit is unmistakable, and a 12.5:1 shrink so
// the overflow is hundreds of rows deep rather than a byte or two.
constexpr int kBigW   = 800;
constexpr int kBigH   = 600;
constexpr int kSmallW = 64;
constexpr int kSmallH = 48;

// A client that shrinks the frame and pumps the loop from inside render().
//
// A real client cannot do this - it has no handle to the concrete frame, only
// the three host interfaces. Handing it one here is how the test reaches a code
// path the API otherwise closes off.
class ReentrantClient : public api::IDrawingClient
{
public:
    hosting::X11DrawingFrame* frame{};

    bool armed = false;        // set only for the run under test
    int  depth = 0;            // re-entrancy guard for the TEST, not the frame
    int  renderCalls = 0;
    int  nestedPresents = 0;

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
        ++renderCalls;

        drawing::Graphics g(dc);
        g.clear(drawing::Colors::YellowGreen);

        // Only the OUTER render re-enters, and only once. Without this the
        // nested present's own render() would recurse forever.
        if (armed && depth == 0 && nestedPresents == 0)
        {
            ++depth;

            // reSize() moves d.width/d.height. On its own this is harmless -
            // it reallocates nothing - which is exactly why the defect needs
            // the second call as well.
            frame->reSize(kSmallW, kSmallH);

            // ...and this is the second call. processEvents() ends in
            // present(), which calls ensureImage(64, 48): the 800x600 image is
            // released and a 64x48 one takes its place, while the present()
            // below us on the stack still believes it is 800x600.
            frame->processEvents();

            ++nestedPresents;
            --depth;
        }

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

} // anonymous namespace

int main()
{
    Display* dpy = XOpenDisplay(nullptr);
    if (!dpy)
    {
        std::fprintf(stderr, "SKIP: no X display\n");
        return 77;
    }

    const int screen = DefaultScreen(dpy);
    Window parent = XCreateSimpleWindow(dpy, RootWindow(dpy, screen), 0, 0,
                                        unsigned(kBigW), unsigned(kBigH), 0,
                                        BlackPixel(dpy, screen), BlackPixel(dpy, screen));
    XSelectInput(dpy, parent, ExposureMask | StructureNotifyMask);
    XMapWindow(dpy, parent);
    XFlush(dpy);

    hosting::X11DrawingFrame frame;

    // The CPU backend ships no font code. Nothing here draws text, but the
    // factory is wired the way a wrapper wires it so the test exercises the
    // same configuration the plugin does.
    static drawing::CpuTextEngine textEngine{ drawing::findFont };
    textEngine.imageDecoder = drawing::decodeImageMemory;
    frame.drawingFactory().textEngine   = &textEngine;
    frame.drawingFactory().imageDecoder = drawing::decodeImageFile;

    ReentrantClient client;
    client.frame = &frame;
    frame.attachClient(&client);

    if (!frame.open(static_cast<uintptr_t>(parent), kBigW, kBigH))
    {
        std::fprintf(stderr, "FAIL: frame.open failed\n");
        return 1;
    }
    std::printf("frame open, %dx%d\n", frame.width(), frame.height());
    std::fflush(stdout);

    auto* drawingHost = static_cast<api::IDrawingHost*>(&frame);

    // Warm-up, unarmed: this is the present() that allocates the 800x600 image,
    // so the run under test starts from a known large surface rather than from
    // whatever the first paint happened to do.
    drawingHost->invalidateRect(nullptr);
    frame.onTimer();

    const int rendersAfterWarmup = client.renderCalls;
    if (rendersAfterWarmup == 0)
    {
        std::fprintf(stderr, "FAIL: the warm-up present never reached the client - "
                             "nothing below this point would mean anything\n");
        return 3;
    }
    std::printf("warm-up renders: %d, frame now %dx%d\n",
                rendersAfterWarmup, frame.width(), frame.height());
    std::fflush(stdout);

    // The run under test. onTimer() -> present() -> render() -> reSize +
    // processEvents -> nested present() -> ensureImage(64,48). Unfixed, the
    // outer present() then blits 800x600 into the 64x48 image and dies here.
    client.armed = true;
    drawingHost->invalidateRect(nullptr);
    std::printf("arming: outer present about to run at %dx%d\n",
                frame.width(), frame.height());
    std::fflush(stdout);

    frame.onTimer();

    std::printf("survived the outer present\n");
    std::fflush(stdout);

    frame.detachClient();
    frame.close();
    XDestroyWindow(dpy, parent);
    XCloseDisplay(dpy);

    // Surviving is necessary but not sufficient: a guard that made the whole
    // scenario not happen would also survive, and would prove nothing. These
    // two checks are what stop this test passing vacuously.
    if (client.nestedPresents != 1)
    {
        std::fprintf(stderr, "FAIL: the nested present never ran (%d) - the test did not "
                             "build the scenario it claims to test\n", client.nestedPresents);
        return 3;
    }

    if (frame.width() != kSmallW || frame.height() != kSmallH)
    {
        std::fprintf(stderr, "FAIL: frame is %dx%d, expected %dx%d - the shrink did not "
                             "take, so the extents never diverged\n",
                     frame.width(), frame.height(), kSmallW, kSmallH);
        return 3;
    }

    std::printf("renders: %d (warm-up %d, then outer + nested)\n",
                client.renderCalls, rendersAfterWarmup);
    std::printf("PASS: nested present at %dx%d under an outer present at %dx%d, no overflow\n",
                kSmallW, kSmallH, kBigW, kBigH);
    return 0;
}
