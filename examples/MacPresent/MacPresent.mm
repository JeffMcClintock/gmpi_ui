// Does DrawingFrameMac present the same picture through the GPU backend (Metal)
// as through CoreGraphics? Opens one gmpi_ui view per backend in a borderless
// window placed beyond every display (nothing reaches the screen), lets AppKit
// present it, reads the window back from the window server and compares the two.
//
//     mac_present [--out DIR] [--backend both|cg|gpu] [--size WxH] [--vsync]
//
// GMPI_UI_GPU selects the path inside the frame, exactly as it does for a real
// host: this sets it to 0 for the CoreGraphics window and 1 for the GPU one.
// GMPI_UI_GPU_TRACE=1 adds the presenter's own per-frame timing.
//
// Prints, per backend: time to the first present, render calls, the scale the
// frame drew at, and a partial repaint; then the per-channel difference between
// the two captures, a background pixel and the darkest text pixel. Writes
// cg.png and gpu.png into DIR when given.
//
// build: see CMakeLists.txt beside this file.

#import <AppKit/AppKit.h>
#import <ImageIO/ImageIO.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#include <dlfcn.h>

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

#include "Drawing.h"
#include "backends/GpuGfx.h"

using namespace gmpi;

extern void* createNativeView(void* parent, class IUnknown* paramHost, class IUnknown* client, int width, int height);
extern void  resizeNativeView(void* ptr, int width, int height);
extern void  gmpi_onCloseNativeView(void* ptr);

namespace
{

using Clock = std::chrono::steady_clock;
double msSince(Clock::time_point a) { return std::chrono::duration<double, std::milli>(Clock::now() - a).count(); }

// A small, fixed UI: a mid-grey panel, text, flat and gradient fills, strokes.
class DemoClient : public api::IDrawingClient
{
public:
    api::IDrawingHost* host{};
    drawing::Rect bounds{};
    int renders{};
    int counter{};
    Clock::time_point firstRender{};
    float lastScale{};

    static constexpr drawing::Rect counterRect{ 300.0f, 20.0f, 380.0f, 44.0f };

    ReturnCode setHost(api::IUnknown* h) override
    {
        host = {};
        if (h)
            h->queryInterface(&api::IDrawingHost::guid, reinterpret_cast<void**>(&host));
        if (host)
            host->release(); // the frame owns this object; no cycle
        return ReturnCode::Ok;
    }
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
        if (renders++ == 0)
            firstRender = Clock::now();
        if (host)
            lastScale = host->getRasterizationScale();

        drawing::Graphics g(dc);
        g.clear(drawing::colorFromSrgba(128, 128, 128)); // mid-grey: sRGB 128

        auto black = g.createSolidColorBrush(drawing::Colors::Black);
        auto white = g.createSolidColorBrush(drawing::Colors::White);
        auto field = g.createSolidColorBrush(drawing::colorFromSrgba(56, 56, 56));
        g.fillRectangle({ 20.0f, 60.0f, 220.0f, 90.0f }, field);
        g.drawRectangle({ 20.0f, 60.0f, 220.0f, 90.0f }, white, 1.0f);

        const std::string_view family = "Helvetica";
        auto font = g.getFactory().createTextFormat(18.0f, { &family, 1 });
        g.drawTextU("Metal vs CoreGraphics", font, { 20.0f, 20.0f, 300.0f, 50.0f }, black);
        g.drawTextU("Field 0.123 dB", font, { 26.0f, 62.0f, 220.0f, 90.0f }, white);
        g.drawTextU(std::to_string(counter), font, counterRect, black);

        g.fillRectangle({ 240.0f, 60.0f, 280.0f, 100.0f }, g.createSolidColorBrush(drawing::Colors::Red));
        g.fillEllipse({ { 330.0f, 120.0f }, 40.0f, 25.0f }, g.createSolidColorBrush(drawing::Colors::Blue));
        g.fillRectangle({ 20.0f, 120.0f, 260.0f, 160.0f },
                        g.createLinearGradientBrush(drawing::Point{ 20.0f, 0.0f }, drawing::Point{ 260.0f, 0.0f },
                                                    drawing::Colors::Black, drawing::Colors::White));
        g.drawRoundedRectangle({ { 20.0f, 180.0f, 380.0f, 280.0f }, 12.0f, 12.0f },
                               g.createSolidColorBrush(drawing::colorFromSrgba(255, 200, 0)), 3.0f);
        g.drawLine({ 30.0f, 270.0f }, { 370.0f, 190.0f }, white, 2.0f);
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

void pumpUntil(double seconds, const std::function<bool()>& done = {})
{
    NSDate* until = [NSDate dateWithTimeIntervalSinceNow:seconds];
    while ([until timeIntervalSinceNow] > 0 && !(done && done()))
    {
        NSEvent* e = [NSApp nextEventMatchingMask:NSEventMaskAny
                                        untilDate:[NSDate dateWithTimeIntervalSinceNow:0.005]
                                           inMode:NSDefaultRunLoopMode
                                          dequeue:YES];
        if (e)
            [NSApp sendEvent:e];
    }
}

// CGWindowListCreateImage is obsoleted in the macOS 15 SDK, but still answers a
// process asking for its own windows (no screen-recording permission needed).
CGImageRef captureWindow(NSWindow* window)
{
    using Fn = CGImageRef (*)(CGRect, CGWindowListOption, CGWindowID, CGWindowImageOption);
    static auto fn = reinterpret_cast<Fn>(dlsym(RTLD_DEFAULT, "CGWindowListCreateImage"));
    if (!fn)
        return nullptr;
    return fn(CGRectNull, kCGWindowListOptionIncludingWindow, CGWindowID([window windowNumber]),
              kCGWindowImageBoundsIgnoreFraming | kCGWindowImageBestResolution);
}

// RGBA8, in sRGB, so the two backends are compared in one known space.
struct Image
{
    int w{}, h{};
    std::vector<uint8_t> px;
    const uint8_t* at(int x, int y) const { return px.data() + (size_t(y) * w + x) * 4; }
};

Image toSrgb(CGImageRef src)
{
    Image img;
    if (!src)
        return img;
    img.w = int(CGImageGetWidth(src));
    img.h = int(CGImageGetHeight(src));
    img.px.resize(size_t(img.w) * img.h * 4);
    CGColorSpaceRef cs = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
    CGContextRef ctx = CGBitmapContextCreate(img.px.data(), img.w, img.h, 8, img.w * 4, cs,
                                             kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big);
    CGContextDrawImage(ctx, CGRectMake(0, 0, img.w, img.h), src);
    CGContextRelease(ctx);
    CGColorSpaceRelease(cs);
    return img;
}

void savePng(CGImageRef image, const std::string& path)
{
    NSURL* url = [NSURL fileURLWithPath:[NSString stringWithUTF8String:path.c_str()]];
    CGImageDestinationRef dest = CGImageDestinationCreateWithURL((__bridge CFURLRef)url, (__bridge CFStringRef)UTTypePNG.identifier, 1, nullptr);
    if (!dest)
        return;
    CGImageDestinationAddImage(dest, image, nullptr);
    CGImageDestinationFinalize(dest);
    CFRelease(dest);
}

struct Run
{
    std::string backend;
    Image image;
    double firstPresentMs{};
    double partialMs{};
    double burstMs{};    // mean wall time per repaint over a burst of back-to-back repaints
    int renders{};
    float scale{};
    bool resized{ true }; // the client was arranged at each new size
    bool ok{};
};

Run runBackend(const std::string& backend, int w, int h, const std::string& outDir)
{
    Run run;
    run.backend = backend;
    setenv("GMPI_UI_GPU", backend == "gpu" ? "1" : "0", 1);

    // Beyond the right-hand edge of every display.
    CGFloat right = 0;
    for (NSScreen* s in [NSScreen screens])
        right = std::max(right, NSMaxX([s frame]));
    NSWindow* window = [[NSWindow alloc] initWithContentRect:NSMakeRect(right + 2000, 0, w, h)
                                                   styleMask:NSWindowStyleMaskBorderless
                                                     backing:NSBackingStoreBuffered
                                                       defer:NO];
    [window setReleasedWhenClosed:NO];
    [window setFrameOrigin:NSMakePoint(right + 2000, 0)];

    DemoClient client;
    auto* asUnknown = reinterpret_cast<class IUnknown*>(static_cast<api::IUnknown*>(&client));

    const auto t0 = Clock::now();
    NSView* v = (NSView*)createNativeView((void*)[window contentView], nullptr, asUnknown, w, h);
    [window orderFrontRegardless]; // composited by the window server, but on no display
    pumpUntil(5.0, [&] { return client.renders > 0; });
    run.firstPresentMs = client.renders ? std::chrono::duration<double, std::milli>(client.firstRender - t0).count() : -1;
    pumpUntil(0.3);

    // A partial repaint: just the counter.
    {
        const int before = client.renders;
        client.counter = 42;
        const auto t1 = Clock::now();
        client.host->invalidateRect(&DemoClient::counterRect);
        pumpUntil(2.0, [&] { return client.renders > before; });
        run.partialMs = msSince(t1);
        pumpUntil(0.3);
    }

    // Back-to-back repaints, as an animating control makes: does a present block?
    {
        constexpr int kBurst = 60;
        const auto t1 = Clock::now();
        for (int i = 0; i < kBurst; ++i)
        {
            const int before = client.renders;
            client.counter = i;
            client.host->invalidateRect(&DemoClient::counterRect);
            pumpUntil(2.0, [&] { return client.renders > before; });
        }
        run.burstMs = msSince(t1) / kBurst;
        client.counter = 42;
        client.host->invalidateRect(&DemoClient::counterRect);
        pumpUntil(0.3);
    }

    // Resize down and back: the target follows the view and the client is
    // laid out again each time. Captured afterwards, at the original size.
    for (const auto [rw, rh] : { std::pair{ w * 3 / 4, h * 2 / 3 }, std::pair{ w, h } })
    {
        const int before = client.renders;
        [window setContentSize:NSMakeSize(rw, rh)];
        resizeNativeView((void*)v, rw, rh);
        pumpUntil(2.0, [&] { return client.renders > before; });
        pumpUntil(0.2);
        run.resized = run.resized && client.bounds.right == float(rw) && client.bounds.bottom == float(rh);
    }

    CGImageRef shot = captureWindow(window);
    run.image = toSrgb(shot);
    if (shot && !outDir.empty())
        savePng(shot, outDir + "/" + backend + ".png");
    if (shot)
        CGImageRelease(shot);

    run.renders = client.renders;
    run.scale = client.lastScale;
    run.ok = client.renders > 0 && run.image.w > 0 && run.resized;

    gmpi_onCloseNativeView((void*)v);
    [v removeFromSuperview];
    [window orderOut:nil];
    [window close];
    [window release];
    pumpUntil(0.1);
    return run;
}

void printRun(const Run& r)
{
    printf("%-4s first present %.1f ms, partial repaint %.1f ms, burst %.2f ms/repaint, renders %d, scale %.1f, resize %s, capture %dx%d\n",
           r.backend.c_str(), r.firstPresentMs, r.partialMs, r.burstMs, r.renders, r.scale, r.resized ? "ok" : "NOT ARRANGED", r.image.w, r.image.h);
}

// True when the GPU frame matches: flat colours within 3, the mean (text included) under 8.
bool compare(const Run& cg, const Run& gpu)
{
    if (cg.image.w != gpu.image.w || cg.image.h != gpu.image.h)
    {
        printf("captures differ in size: %dx%d vs %dx%d\n", cg.image.w, cg.image.h, gpu.image.w, gpu.image.h);
        return false;
    }
    const int W = cg.image.w, H = cg.image.h;
    const float s = float(W) / 400.0f; // device pixels per logical unit, for the probes below
    double sum = 0;
    int maxDiff = 0;
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
            for (int c = 0; c < 3; ++c)
            {
                const int d = std::abs(int(cg.image.at(x, y)[c]) - int(gpu.image.at(x, y)[c]));
                sum += d;
                maxDiff = std::max(maxDiff, d);
            }
    const double mean = sum / (double(W) * H * 3);
    printf("mean per-channel difference %.2f / 255 (max %d) over %dx%d\n", mean, maxDiff, W, H);
    bool pass = mean < 8.0;

    auto probe = [&](const char* what, float lx, float ly) {
        const int x = int(lx * s), y = int(ly * s);
        const auto* a = cg.image.at(x, y);
        const auto* b = gpu.image.at(x, y);
        printf("%-12s (%3.0f,%3.0f)  cg %3d %3d %3d   gpu %3d %3d %3d\n", what, lx, ly, a[0], a[1], a[2], b[0], b[1], b[2]);
        for (int c = 0; c < 3; ++c)
            pass = pass && std::abs(int(a[c]) - int(b[c])) <= 3;
    };
    probe("background", 10, 10);
    probe("field", 210, 85);
    probe("red", 260, 80);
    probe("blue", 330, 120);
    probe("gradient", 140, 140);
    probe("repainted", 378, 24); // inside the partial repaint, clear of the digits

    // The darkest pixel of the title text in each.
    auto darkest = [&](const Image& img, int& bx, int& by) {
        int best = 999;
        for (int y = int(20 * s); y < int(45 * s); ++y)
            for (int x = int(20 * s); x < int(240 * s); ++x)
            {
                const int v = img.at(x, y)[1];
                if (v < best) { best = v; bx = x; by = y; }
            }
        return best;
    };
    int ax = 0, ay = 0, bx = 0, by = 0;
    const int da = darkest(cg.image, ax, ay), db = darkest(gpu.image, bx, by);
    printf("text darkest  cg %3d at %d,%d   gpu %3d at %d,%d\n", da, ax, ay, db, bx, by);

    // Text coverage: mean darkening of the title row relative to the background.
    auto ink = [&](const Image& img) {
        double t = 0;
        int n = 0;
        for (int y = int(20 * s); y < int(45 * s); ++y)
            for (int x = int(20 * s); x < int(240 * s); ++x, ++n)
                t += 128 - img.at(x, y)[1];
        return t / n;
    };
    printf("text ink (mean darkening of the title box)  cg %.2f   gpu %.2f\n", ink(cg.image), ink(gpu.image));
    printf("%s\n", pass ? "MATCH" : "MISMATCH");
    return pass;
}

} // namespace

int main(int argc, char** argv)
{
    setvbuf(stdout, nullptr, _IONBF, 0);
    std::string out, which = "both";
    int w = 400, h = 300;
    bool vsync = false;
    for (int i = 1; i < argc; ++i)
    {
        const bool more = i + 1 < argc;
        if (!strcmp(argv[i], "--out") && more)          out = argv[++i];
        else if (!strcmp(argv[i], "--backend") && more) which = argv[++i];
        else if (!strcmp(argv[i], "--size") && more)    sscanf(argv[++i], "%dx%d", &w, &h);
        else if (!strcmp(argv[i], "--vsync"))           vsync = true;
        else { fprintf(stderr, "usage: mac_present [--out DIR] [--backend both|cg|gpu] [--size WxH] [--vsync]\n"); return 2; }
    }

    @autoreleasepool
    {
        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory]; // no Dock icon, never takes focus
        [NSApp finishLaunching];

        Run cg, gpu;
        if (which != "gpu")
        {
            cg = runBackend("cg", w, h, out);
            printRun(cg);
        }
        if (which != "cg")
        {
            if (vsync && gmpi::gpugfx::initialize())
            {
                // initialize() turns vsync off for every host; turn it back on to see what it costs.
                gmpi::gpu::Renderer::instance().setVsync(true);
                gmpi::gpu::Renderer::reset(1, 1);
            }
            gpu = runBackend("gpu", w, h, out);
            printRun(gpu);
        }
        if (which == "both")
            return cg.ok && gpu.ok && compare(cg, gpu) ? 0 : 1;
        return cg.ok || gpu.ok ? 0 : 1;
    }
}
