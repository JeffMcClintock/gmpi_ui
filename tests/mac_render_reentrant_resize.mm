// ---------------------------------------------------------------------------
// Does DrawingFrameCocoa::onRender survive a client that resizes its own view
// from inside render()?  Written for TIDE BACKLOG P7b.
//
// WHAT IS BEING TESTED
//
// onRender checks backBuffer, then hands control to the client:
//
//     if(!backBuffer) return;              // the check
//     CGContextSaveGState(backBuffer);
//     ...
//     {
//         GraphicsContext context(...);
//         context.setCGContext(backBuffer);   // <-- the context CACHES the pointer
//         context.pushAxisAlignedClip(...);
//         drawingClient->render(&context);    // <-- re-entrant: arbitrary client code
//         context.popAxisAlignedClip();       // <-- the use
//     }
//     CGContextRestoreGState(backBuffer);
//     CGBitmapContextCreateImage(backBuffer);
//
// A client that resizes its own view from inside render() reaches
// GMPI_VIEW_CLASS setFrame: -> DrawingFrameCocoa::onResize(), which does
// CGContextRelease(backBuffer); backBuffer = nullptr. Everything after the
// render call is then operating on a bitmap context that no longer exists. It is
// the P4 Windows defect exactly -- time of check, then a re-entrant call, then
// use -- on a path nobody had exercised.
//
// WHICH LINE ACTUALLY BITES, WHICH IS NOT THE ONE THE BACKLOG ROW NAMED
//
// The row predicted the fault at CGContextRestoreGState(backBuffer) and
// CGBitmapContextCreateImage(backBuffer). Those two read the *member*, and
// onResize sets the member to nullptr -- so they pass NULL to CoreGraphics,
// which complains on stderr and returns NULL. Ugly, but not a use-after-free.
//
// The genuine use-after-free is one line earlier, in popAxisAlignedClip():
// GraphicsContext keeps its OWN copy of the pointer in cgContext_, taken at
// setCGContext time, and nothing nulls that. So it calls
// CGContextRestoreGState on the freed context. That is why the fix has to sit
// INSIDE the scope, immediately after render() returns, rather than after the
// block closes: by the time the block closes the damage is done.
//
// HOW A FAILURE SHOWS UP
//
// Build with -fsanitize=address (run_mac_render_test.sh does): the unguarded
// build reports `heap-use-after-free ... READ of size 8` inside
// CGContextRestoreGState and aborts. Without ASan the freed memory is usually
// still intact and the run "passes" -- which is exactly why this test is not
// worth running without it.
//
// WHY A SYNTHETIC CLIENT RATHER THAN A REAL PLUGIN
//
// No shipping client resizes itself during render, which is why P7 filed this
// as latent rather than fixing it. A real plugin therefore cannot exercise the
// path at all. The client below does one thing no real one does, and is
// otherwise an ordinary IDrawingClient.
//
// Usage:  mac_render_reentrant_resize
//
// Exit status:
//     0  survived the re-entrant resize, and the renderer was provably live
//        before and after it
//     1  setup failed -- the message says which step
//     3  the renderer was never provably live, so surviving proves nothing
//
// A regression shows up as the process dying (ASan abort / SIGSEGV), not as an
// exit code.
// ---------------------------------------------------------------------------

#import <AppKit/AppKit.h>

#include <cstdio>
#include <set>

#include "helpers/NativeUi.h"
#include "Drawing.h"

using namespace gmpi;

// The backend's C++ entry points. Declared here rather than shared through a
// header because that is how they already cross this boundary -- see the note
// above gmpi_clampEditorSize in DrawingFrameMac.mm. `class IUnknown` is the
// same opaque forward declaration the definitions use; it must match for the
// C++ mangling to line up.
extern void* createNativeView(void* parent, class IUnknown* paramHost, class IUnknown* client, int width, int height);
extern void  resizeNativeView(void* ptr, int width, int height);
extern void  gmpi_onCloseNativeView(void* ptr);

namespace
{

int g_renderCalls = 0;

// An ordinary drawing client, with one abnormal habit: when armed, it resizes
// the view it is drawing into, from inside render(). Everything else here
// exists only so the paint is real -- a client that draws nothing would make
// the liveness probe meaningless.
class SelfResizingClient : public api::IDrawingClient
{
public:
    void*   view{};          // the NSView createNativeView returned
    bool    armed{};         // resize on the next render?
    int     resizeTo[2]{};   // width, height to resize to
    int     resizesDone{};
    drawing::Rect bounds{0, 0, 400, 300}; // whatever arrange() last said

    ReturnCode setHost(api::IUnknown*) override { return ReturnCode::Ok; }

    ReturnCode measure(const drawing::Size* available, drawing::Size* desired) override
    {
        *desired = *available;
        return ReturnCode::Ok;
    }
    ReturnCode arrange(const drawing::Rect* finalRect) override
    {
        bounds = *finalRect;
        return ReturnCode::Ok;
    }
    ReturnCode getClipArea(drawing::Rect*) override { return ReturnCode::Unhandled; }

    ReturnCode render(drawing::api::IDeviceContext* dc) override
    {
        ++g_renderCalls;

        drawing::Graphics g(dc);
        g.clear(drawing::Colors::YellowGreen);

        // STRIPES ACROSS THE WHOLE VIEW, not one rect in a corner.
        //
        // The probe samples a 200 x 200 tile and calls "more than one distinct
        // colour" proof the renderer ran. A single rect near the drawing
        // origin fails that for a perfectly healthy renderer: the flip in
        // onRender puts the drawing origin at the TOP, while AppKit's
        // visibleRect origin is the BOTTOM-left, so the tile lands somewhere
        // the rect is not and every pixel comes back background. Measured, not
        // guessed -- the first version of this file did exactly that and
        // reported NOT LIVE. Stripes make the count independent of where the
        // tile happens to land.
        auto brush = g.createSolidColorBrush(drawing::Colors::Crimson);
        for (float y = 0.0f; y < bounds.bottom; y += 20.0f)
            g.fillRectangle(bounds.left, y, bounds.right, y + 10.0f, brush);

        if (armed)
        {
            armed = false;
            ++resizesDone;

            // THE WHOLE POINT OF THIS FILE. setFrame: -> onResize() ->
            // CGContextRelease(backBuffer). The context `dc` above is now
            // holding a dangling CGContextRef, and so is onRender's local.
            std::printf("  client: resizing its own view to %d x %d from inside render()\n",
                        resizeTo[0], resizeTo[1]);
            resizeNativeView(view, resizeTo[0], resizeTo[1]);
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

void pump(double seconds)
{
    NSDate* until = [NSDate dateWithTimeIntervalSinceNow:seconds];
    while ([until timeIntervalSinceNow] > 0)
    {
        NSEvent* e = [NSApp nextEventMatchingMask:NSEventMaskAny
                                        untilDate:until
                                           inMode:NSDefaultRunLoopMode
                                          dequeue:YES];
        if (!e)
            break;
        [NSApp sendEvent:e];
    }
}

// Force a synchronous trip through drawRect: -> DrawingFrameCocoa::onRender,
// and report how much distinct colour came back.
//
// Distinct-colour count is the macOS answer to "did the renderer actually run".
// If the backing bitmap were missing, onRender bails at `if(!backBuffer)
// return;` having drawn nothing and every pixel comes back uniform. Sampling
// is capped at a small tile so the harness never becomes the thing that
// allocates hundreds of MiB.
int forcePaint(NSView* v)
{
    if (!v)
        return 0;

    const NSRect vis = [v visibleRect];
    if (vis.size.width < 1 || vis.size.height < 1)
        return 0;

    NSRect r = vis;
    r.size.width  = (r.size.width  > 200) ? 200 : r.size.width;
    r.size.height = (r.size.height > 200) ? 200 : r.size.height;

    NSBitmapImageRep* rep = [v bitmapImageRepForCachingDisplayInRect:r];
    if (!rep)
        return 0;

    [v cacheDisplayInRect:r toBitmapImageRep:rep];

    unsigned char* px = [rep bitmapData];
    if (!px)
        return 0;

    const long bpr = [rep bytesPerRow];
    const long spp = [rep samplesPerPixel];
    const long w   = [rep pixelsWide];
    const long h   = [rep pixelsHigh];

    std::set<unsigned int> seen;
    for (long y = 0; y < h && seen.size() <= 64; y += ((h > 32) ? h / 32 : 1))
    {
        for (long x = 0; x < w; x += ((w > 32) ? w / 32 : 1))
        {
            const unsigned char* p = px + y * bpr + x * spp;
            const unsigned int v32 = (unsigned int)p[0]
                                   | ((unsigned int)((spp > 1) ? p[1] : 0) << 8)
                                   | ((unsigned int)((spp > 2) ? p[2] : 0) << 16);
            seen.insert(v32);
            if (seen.size() > 64)
                break;
        }
    }
    return (int)seen.size();
}

void viewSize(NSView* v, int& w, int& h)
{
    const NSRect f = [v frame];
    w = (int)f.size.width;
    h = (int)f.size.height;
}

} // anonymous namespace

int main()
{
    // Unbuffered: on the unfixed sources this process dies mid-run, and a
    // buffered trace is lost exactly when it is the only evidence of how far
    // it got.
    setvbuf(stdout, nullptr, _IONBF, 0);

    // A command-line tool gets no NSApplication and no window server connection
    // unless it asks. cacheDisplayInRect: needs both.
    [NSApplication sharedApplication];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

    constexpr int w0 = 400;
    constexpr int h0 = 300;

    NSWindow* window = [[NSWindow alloc]
        initWithContentRect:NSMakeRect(100, 100, w0, h0)
                  styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskResizable)
                    backing:NSBackingStoreBuffered
                      defer:NO];
    if (!window)
    {
        std::fprintf(stderr, "FAIL: could not create an NSWindow -- no window server?\n");
        return 1;
    }
    [window setTitle:@"gmpi_ui re-entrant resize guard (P7b)"];
    [window makeKeyAndOrderFront:nil];

    NSView* content = [window contentView];
    if (!content)
    {
        std::fprintf(stderr, "FAIL: window has no content view\n");
        return 1;
    }

    SelfResizingClient client;
    auto* asUnknown = reinterpret_cast<class IUnknown*>(static_cast<api::IUnknown*>(&client));

    NSView* v = (NSView*)createNativeView((void*)content, nullptr, asUnknown, w0, h0);
    if (!v)
    {
        std::fprintf(stderr, "FAIL: createNativeView returned nothing\n");
        return 1;
    }
    client.view = (void*)v;
    std::printf("view: %p  %d x %d\n", (void*)v, w0, h0);

    pump(0.5);

    // -- liveness, before anything abnormal ---------------------------------
    // "It survived" is worthless unless the renderer was running in the first
    // place. Two facts, both needed: render() was called, and the paint
    // produced more than one distinct pixel value.
    const int distinctBefore = forcePaint(v);
    std::printf("liveness: render calls %d, distinct colours %d\n", g_renderCalls, distinctBefore);
    if (g_renderCalls == 0 || distinctBefore < 2)
    {
        std::fprintf(stderr, "NOT LIVE: the renderer never ran or drew nothing -- surviving proves nothing\n");
        return 3;
    }

    // -- the re-entrant resize ----------------------------------------------
    client.armed = true;
    client.resizeTo[0] = w0 / 2;
    client.resizeTo[1] = h0 / 2;

    std::printf("arming a resize inside render()...\n");
    const int distinctDuring = forcePaint(v);
    std::printf("  survived. resizes done %d, distinct colours %d\n",
                client.resizesDone, distinctDuring);

    if (client.resizesDone != 1)
    {
        std::fprintf(stderr, "FAIL: the client never got to resize -- the path was not exercised\n");
        return 1;
    }

    int cw = 0, ch = 0;
    viewSize(v, cw, ch);
    std::printf("  view after: %d x %d (asked for %d x %d)\n", cw, ch, client.resizeTo[0], client.resizeTo[1]);

    // -- recovery ------------------------------------------------------------
    // The next paint must reallocate the backing bitmap at the new size and
    // draw normally. Without this, "survived" could still mean the frame was
    // left permanently unable to draw.
    pump(0.3);
    const int distinctAfter = forcePaint(v);
    std::printf("recovery: render calls %d, distinct colours %d\n", g_renderCalls, distinctAfter);
    if (distinctAfter < 2)
    {
        std::fprintf(stderr, "FAIL: the frame never drew again after the re-entrant resize\n");
        return 1;
    }

    gmpi_onCloseNativeView((void*)v);

    std::printf("PASS: survived a re-entrant resize during render, and drew before and after\n");
    return 0;
}
