// A frame-rate benchmark for gmpi_ui's Wayland backends, CPU and GPU.
//
// The default scene is the one JUCE's GraphicsDemo made familiar: a filled
// path bouncing round the window, spinning and pulsing, so a dropped frame is
// something you SEE as well as read. Double the number of paths until the
// frame rate falls, then relaunch on the other backend and compare.
//
//     wayland_bench [--cpu | --gpu] [--scene N] [--load N] [--detail N]
//                   [--fill solid|linear|radial] [--no-outline] [--static-colors]
//                   [--size WxH] [--frames N] [--report] [--paused] [--dump FILE]
//
//     --cpu / --gpu   the backend, chosen at launch because a frame picks its
//                     renderer once (every brush and bitmap belongs to the
//                     factory that made it). GMPI_UI_GPU in the environment
//                     overrides both. Default: GPU when built with it.
//     --report        one line of stats a second on stdout, for scripts
//     --frames N      quit after N presented frames
//
// Keys:
//     1-6     scene: bounce, lines, rects, ellipses, text, gradients
//     + / -   double / halve the load (the number of things drawn)
//     ] / [   double / halve the path detail (bounce scene)
//     f       bounce fill: solid, linear gradient, radial gradient
//     o       bounce outline on / off
//     space   freeze the animation; frames keep coming, so a static
//             scene is measured (what caching helps)
//     q       quit
//
// READING THE NUMBERS. Presents are paced by the compositor's frame callbacks,
// so fps tops out at the display's refresh rate however light the scene is;
// below that, it is the backend's real throughput. "render" is the client's
// own drawing calls - on the GPU path that is only recording, which is why the
// frame also reports "present": the shm encode on the CPU path, the submit and
// swap-chain present on the GPU path.
//
// build: see README.md beside this file.

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <span>
#include <string>
#include <vector>

#include "backends/DrawingFrameWayland.h"
#include "helpers/CpuTextEngine.h"
#include "helpers/DecodeImage.h"
#include "helpers/FontProvider.h"
#include "Drawing.h"

using namespace gmpi;
using namespace gmpi::drawing;

namespace
{

constexpr float kPi = 3.14159265f;

enum Scene { Bounce, Lines, Rects, Ellipses, Text, Gradients, SceneCount };
const char* const kSceneNames[SceneCount] = { "bounce", "lines", "rects", "ellipses", "text", "gradients" };

enum BounceFill { Solid, Linear, Radial, FillCount };
const char* const kFillNames[FillCount] = { "solid", "linear gradient", "radial gradient" };

// Deterministic per-item variety: the same scene draws the same picture on
// both backends, so a comparison is like for like.
float hash01(uint32_t i, uint32_t salt)
{
    uint32_t x = i * 747796405u + salt * 2891336453u;
    x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15; x *= 0x846ca68bu; x ^= x >> 16;
    return float(x & 0xffffff) / float(0x1000000);
}

Color hue(float h, float a = 1.f)
{
    const float r = 0.5f + 0.5f * cosf(2 * kPi * (h + 0.0f));
    const float g = 0.5f + 0.5f * cosf(2 * kPi * (h + 0.33f));
    const float b = 0.5f + 0.5f * cosf(2 * kPi * (h + 0.67f));
    return { r, g, b, a };
}

// Wall-to-wall cycles per second for the primitive scenes, times 1-2 per item:
// a crossing every two to four seconds, fast enough to read as motion at a
// glance (at a tenth of this they looked frozen).
constexpr float kDrift = 0.125f;

// A triangle wave in [0, 1): the bounce off a wall, without per-item state.
float bounce(float t)
{
    const float f = t - floorf(t);
    return f < 0.5f ? f * 2.f : 2.f - f * 2.f;
}

// Frame-to-frame statistics over a sliding window.
class Stats
{
public:
    void add(double intervalMs, double renderMs, double presentMs)
    {
        if (interval_.size() == kWindow)
        {
            interval_.erase(interval_.begin());
            render_.erase(render_.begin());
            present_.erase(present_.begin());
        }
        interval_.push_back(intervalMs);
        render_.push_back(renderMs);
        present_.push_back(presentMs);
    }
    void clear() { interval_.clear(); render_.clear(); present_.clear(); }

    double fps() const           { const double m = mean(interval_); return m > 0 ? 1000.0 / m : 0; }
    double frameMs() const       { return mean(interval_); }
    double frameP95() const      { return p95(interval_); }
    double renderMs() const      { return mean(render_); }
    double presentMs() const     { return mean(present_); }

private:
    static constexpr size_t kWindow = 120;
    static double mean(const std::vector<double>& v)
    {
        if (v.empty()) return 0;
        double s = 0; for (double x : v) s += x;
        return s / double(v.size());
    }
    static double p95(std::vector<double> v)
    {
        if (v.empty()) return 0;
        std::sort(v.begin(), v.end());
        return v[std::min(v.size() - 1, size_t(double(v.size()) * 0.95))];
    }
    std::vector<double> interval_, render_, present_;
};

class BenchClient : public gmpi::api::IDrawingClient, public gmpi::api::IInputClient
{
public:
    BenchClient(wayland::WaylandToplevel& frame, TextFormat small, TextFormat overlay)
        : frame_(frame), small_(small), overlay_(overlay) {}

    Scene scene = Bounce;
    int   load = 16;
    int   detail = 8;
    BounceFill fill = Linear;
    bool  outline = true;
    // The bounce colours drift by default, so every gradient is a NEW one each
    // frame - the GPU atlas churn that exposed the gradient-atlas resize bug.
    // --static-colors holds them, to measure gradients that are reused.
    bool  staticColors = false;
    bool  paused = false;
    bool  report = false;
    int   framesPresented = 0;


    // --- IDrawingClient ---
    ReturnCode setHost(gmpi::api::IUnknown*) override { return ReturnCode::Ok; }
    ReturnCode measure(const Size* available, Size* returnDesiredSize) override
    {
        *returnDesiredSize = *available;
        return ReturnCode::Ok;
    }
    ReturnCode arrange(const Rect* finalRect) override { bounds_ = *finalRect; return ReturnCode::Ok; }
    ReturnCode getClipArea(Rect* returnRect) override { *returnRect = bounds_; return ReturnCode::Ok; }
    ReturnCode render(drawing::api::IDeviceContext* dc) override;

    // --- IInputClient ---
    ReturnCode setHover(bool) override { return ReturnCode::Ok; }
    ReturnCode hitTest(Point, int32_t) override { return ReturnCode::Ok; }
    ReturnCode onPointerDown(Point, int32_t) override { return ReturnCode::Ok; }
    ReturnCode onPointerMove(Point, int32_t) override { return ReturnCode::Ok; }
    ReturnCode onPointerUp(Point, int32_t) override { return ReturnCode::Ok; }
    ReturnCode onMouseWheel(Point, int32_t, int32_t) override { return ReturnCode::Unhandled; }
    ReturnCode populateContextMenu(Point, gmpi::api::IUnknown*) override { return ReturnCode::Unhandled; }
    ReturnCode getToolTip(Point, gmpi::api::IString*) override { return ReturnCode::Unhandled; }
    ReturnCode onKeyPress(wchar_t c) override;

    ReturnCode queryInterface(const gmpi::api::Guid* iid, void** returnInterface) override
    {
        *returnInterface = {};
        GMPI_QUERYINTERFACE(gmpi::api::IDrawingClient);
        GMPI_QUERYINTERFACE(gmpi::api::IInputClient);
        return ReturnCode::NoSupport;
    }
    GMPI_REFCOUNT_NO_DELETE;   // lives on the stack in main()

    // Once a second when --report is on; called from the loop's tick.
    void maybeReport()
    {
        const auto now = Clock::now();
        if (!report || now - lastReport_ < std::chrono::seconds(1))
            return;
        lastReport_ = now;
        printf("%s\n", statsLine().c_str());
        fflush(stdout);
    }

private:
    using Clock = std::chrono::steady_clock;

public:
    std::string dumpPath;
    int dumpAtFrame = 0;

private:
    void drawScene(drawing::api::IDeviceContext* dc);
    void dumpOffscreen(drawing::api::IDeviceContext* dc);
    void drawBounce(Graphics& g, const Matrix3x2& base, float t);
    void drawLines(Graphics& g, float t);
    void drawRects(Graphics& g, float t);
    void drawEllipses(Graphics& g, float t);
    void drawText(Graphics& g, float t);
    void drawGradients(Graphics& g, float t);
    void drawOverlay(Graphics& g);
    void restartStats() { stats_.clear(); lastFrame_ = {}; }
    std::string statsLine() const;

    // The star path, in a unit box centred on the origin. Rebuilt when the
    // detail changes; created from the device context's own factory, since a
    // geometry belongs to the backend that made it.
    PathGeometry star_;
    int starDetail_ = -1;

    wayland::WaylandToplevel& frame_;
    TextFormat small_;
    TextFormat overlay_;
    Rect bounds_{ 0, 0, 1000, 700 };

    Stats stats_;
    Clock::time_point start_ = Clock::now();
    Clock::time_point lastFrame_{};
    Clock::time_point lastReport_{};
    float animTime_ = 0;          // seconds of animation, frozen while paused
    Clock::time_point lastAnim_ = Clock::now();
};

ReturnCode BenchClient::onKeyPress(wchar_t c)
{
    switch (c)
    {
    case '1': case '2': case '3': case '4': case '5': case '6':
        scene = Scene(c - '1');
        break;
    case '+': case '=': load = std::min(load * 2, 1 << 20); break;
    case '-': case '_': load = std::max(load / 2, 1);       break;
    case ']':           detail = std::min(detail * 2, 4096); break;
    case '[':           detail = std::max(detail / 2, 3);    break;
    case 'f': case 'F': fill = BounceFill((fill + 1) % FillCount); break;
    case 'o': case 'O': outline = !outline; break;
    case ' ':           paused = !paused; lastAnim_ = Clock::now(); break;
    case 'q': case 'Q': frame_.close(); return ReturnCode::Ok;
    default: return ReturnCode::Unhandled;
    }
    restartStats();
    frame_.invalidateRect(nullptr);
    return ReturnCode::Ok;
}

std::string BenchClient::statsLine() const
{
    const auto& timing = frame_.lastPresentTiming();
    char buf[320];
    snprintf(buf, sizeof buf,
             "%s  scene %s  load %d%s  |  %.1f fps  frame %.2f ms (p95 %.2f)  render %.2f ms  present %.2f ms",
             timing.gpu ? "GPU" : "CPU", kSceneNames[scene], load,
             scene == Bounce ? (std::string("  detail ") + std::to_string(detail) + "  " + kFillNames[fill]
                                + (outline ? " +outline" : "")).c_str() : "",
             stats_.fps(), stats_.frameMs(), stats_.frameP95(), stats_.renderMs(), stats_.presentMs());
    return buf;
}

ReturnCode BenchClient::render(drawing::api::IDeviceContext* dc)
{
    const auto now = Clock::now();
    if (lastFrame_ != Clock::time_point{})
    {
        // The frame's own timing is for the PREVIOUS present: this render is
        // inside the current one. Recording it with the interval that ended
        // here keeps each sample's parts from the same frame.
        using ms = std::chrono::duration<double, std::milli>;
        const auto& timing = frame_.lastPresentTiming();
        stats_.add(ms(now - lastFrame_).count(), timing.renderMs, timing.finishMs);
    }
    lastFrame_ = now;
    ++framesPresented;

    if (!paused)
    {
        animTime_ += std::chrono::duration<float>(now - lastAnim_).count();
        lastAnim_ = now;
    }

    drawScene(dc);

    if (!dumpPath.empty() && framesPresented == dumpAtFrame)
        dumpOffscreen(dc);
    return ReturnCode::Ok;
}

void BenchClient::drawScene(drawing::api::IDeviceContext* dc)
{
    Graphics g(dc);
    const auto base = g.getTransform();   // the frame's display scale
    g.clear(Color{ 0.07f, 0.08f, 0.10f, 1.f });

    switch (scene)
    {
    case Bounce:    drawBounce(g, base, animTime_); break;
    case Lines:     drawLines(g, animTime_);        break;
    case Rects:     drawRects(g, animTime_);        break;
    case Ellipses:  drawEllipses(g, animTime_);     break;
    case Text:      drawText(g, animTime_);         break;
    case Gradients: drawGradients(g, animTime_);    break;
    default: break;
    }

    g.setTransform(base);
    drawOverlay(g);
}

// --dump: draw the same frame into an offscreen target from the window's own
// factory - on the GPU path a GPU target, whose pixels can be read back
// synchronously, which a swap chain's cannot - and write it out.
void BenchClient::dumpOffscreen(drawing::api::IDeviceContext* dc)
{
    Graphics g(dc);
    auto factory = g.getFactory();
    const SizeU size{ uint32_t(bounds_.right - bounds_.left), uint32_t(bounds_.bottom - bounds_.top) };
    auto target = factory.createCpuRenderTarget(size, 0);
    if (!AccessPtr::get(target))
    {
        fprintf(stderr, "--dump: could not create an offscreen target\n");
        return;
    }
    target.beginDraw();
    drawScene(AccessPtr::get(target));
    target.endDraw();

    auto bitmap = target.getBitmap();
    auto pixels = bitmap.lockPixels(BitmapLockFlags::Read);
    if (!pixels)
    {
        fprintf(stderr, "--dump: could not read the offscreen target back\n");
        return;
    }
    if (FILE* f = fopen(dumpPath.c_str(), "wb"))
    {
        const int32_t header[3] = { int32_t(size.width), int32_t(size.height), pixels.getPixelFormat() };
        fwrite(header, sizeof header, 1, f);
        const uint8_t* base = pixels.getAddress();
        const int32_t rowBytes = pixels.getBytesPerRow();
        const size_t pixelBytes = size_t(pixels.getBytesPerPixel());
        for (uint32_t y = 0; y < size.height; ++y)
            fwrite(base + size_t(y) * rowBytes, pixelBytes, size.width, f);
        fclose(f);
        printf("dumped %ux%u (format %d) to %s\n", size.width, size.height, header[2], dumpPath.c_str());
        fflush(stdout);
    }
}

void BenchClient::drawBounce(Graphics& g, const Matrix3x2& base, float t)
{
    if (starDetail_ != detail)
    {
        // A star whose points are bezier lobes: `detail` lobes, each one curve,
        // so the detail knob scales the tessellation work directly.
        star_ = g.getFactory().createPathGeometry();
        auto sink = star_.open();
        const int n = detail;
        auto at = [](float a, float r) { return Point{ r * cosf(a), r * sinf(a) }; };
        sink.beginFigure(at(0, 0.5f), FigureBegin::Filled);
        for (int i = 0; i < n; ++i)
        {
            const float a0 = 2 * kPi * float(i) / float(n);
            const float a1 = 2 * kPi * float(i + 1) / float(n);
            const float am = 0.5f * (a0 + a1);
            sink.addBezier({ at(a0 + 0.15f * (a1 - a0), 0.95f), at(am, 0.15f), at(a1, 0.5f) });
        }
        sink.endFigure(FigureEnd::Closed);
        sink.close();
        starDetail_ = detail;
    }

    const float w = bounds_.right - bounds_.left;
    const float h = bounds_.bottom - bounds_.top;
    const float size = std::clamp(std::min(w, h) / (2.f + sqrtf(float(load)) * 0.6f), 24.f, 260.f);

    auto stroke = g.createSolidColorBrush(Color{ 1.f, 1.f, 1.f, 0.85f });

    for (int i = 0; i < load; ++i)
    {
        const float sx = 0.07f + 0.10f * hash01(i, 1);
        const float sy = 0.07f + 0.10f * hash01(i, 2);
        const float cx = size * 0.5f + bounce(t * sx + hash01(i, 3)) * (w - size);
        const float cy = size * 0.5f + bounce(t * sy + hash01(i, 4)) * (h - size);
        const float spin = (hash01(i, 5) - 0.5f) * 3.f * t;
        const float pulse = size * (0.75f + 0.25f * sinf(t * (1.f + hash01(i, 6)) + float(i)));

        g.setTransform(makeScale(pulse, pulse) * makeRotation(spin) * makeTranslation(cx, cy) * base);

        const float h0 = hash01(i, 7) + (staticColors ? 0.f : 0.05f * t);
        switch (fill)
        {
        case Solid:
            g.fillGeometry(star_, g.createSolidColorBrush(hue(h0, 0.85f)));
            break;
        case Linear:
            g.fillGeometry(star_, g.createLinearGradientBrush(Point{ -0.5f, -0.5f }, Point{ 0.5f, 0.5f },
                                                              hue(h0, 0.9f), hue(h0 + 0.4f, 0.9f)));
            break;
        case Radial:
            g.fillGeometry(star_, g.createRadialGradientBrush(Point{ 0, 0 }, 0.6f,
                                                              hue(h0, 0.95f), hue(h0 + 0.5f, 0.6f)));
            break;
        default: break;
        }
        if (outline)
            g.drawGeometry(star_, stroke, 2.f / pulse);
    }
}

void BenchClient::drawLines(Graphics& g, float t)
{
    const float w = bounds_.right, h = bounds_.bottom;
    for (int i = 0; i < load; ++i)
    {
        const float a = t * (0.6f + 1.8f * hash01(i, 1)) + 6.f * hash01(i, 2);
        const Point c{ w * bounce(t * kDrift * (1 + hash01(i, 8)) + hash01(i, 3)),
                       h * bounce(t * kDrift * (1 + hash01(i, 9)) + hash01(i, 4)) };
        const float len = 20.f + 120.f * hash01(i, 5);
        const Point d{ cosf(a) * len, sinf(a) * len };
        g.drawLine(Point{ c.x - d.x, c.y - d.y }, Point{ c.x + d.x, c.y + d.y },
                   g.createSolidColorBrush(hue(hash01(i, 6), 0.8f)), 1.f + 3.f * hash01(i, 7));
    }
}

void BenchClient::drawRects(Graphics& g, float t)
{
    const float w = bounds_.right, h = bounds_.bottom;
    auto stroke = g.createSolidColorBrush(Color{ 1.f, 1.f, 1.f, 0.5f });
    for (int i = 0; i < load; ++i)
    {
        const float s = 10.f + 70.f * hash01(i, 1);
        const float x = bounce(t * kDrift * (1 + hash01(i, 2)) + hash01(i, 3)) * (w - s);
        const float y = bounce(t * kDrift * (1 + hash01(i, 4)) + hash01(i, 5)) * (h - s);
        const RoundedRect r{ Rect{ x, y, x + s, y + s * 0.7f }, 6.f, 6.f };
        g.fillRoundedRectangle(r, g.createSolidColorBrush(hue(hash01(i, 6), 0.7f)));
        g.drawRoundedRectangle(r, stroke, 1.f);
    }
}

void BenchClient::drawEllipses(Graphics& g, float t)
{
    const float w = bounds_.right, h = bounds_.bottom;
    for (int i = 0; i < load; ++i)
    {
        const float r = 5.f + 45.f * hash01(i, 1);
        const float x = r + bounce(t * kDrift * (1 + hash01(i, 2)) + hash01(i, 3)) * (w - 2 * r);
        const float y = r + bounce(t * kDrift * (1 + hash01(i, 4)) + hash01(i, 5)) * (h - 2 * r);
        const Ellipse e{ Point{ x, y }, r, r * (0.6f + 0.4f * hash01(i, 6)) };
        g.fillEllipse(e, g.createSolidColorBrush(hue(hash01(i, 7), 0.6f)));
        g.drawEllipse(e, g.createSolidColorBrush(hue(hash01(i, 8), 1.f)), 1.5f);
    }
}

void BenchClient::drawText(Graphics& g, float t)
{
    if (!small_)
        return;
    const float w = bounds_.right, h = bounds_.bottom;
    static const char* const words[] = { "Oscillator", "Filter Cutoff", "Envelope", "LFO Rate", "Resonance",
                                         "Gain -6.0 dB", "440.00 Hz", "Attack 12 ms", "Mix 50%", "Voice 7" };
    for (int i = 0; i < load; ++i)
    {
        const float x = bounce(t * kDrift * (1 + hash01(i, 1)) + hash01(i, 2)) * (w - 120.f);
        const float y = bounce(t * kDrift * (1 + hash01(i, 3)) + hash01(i, 4)) * (h - 20.f);
        g.drawTextU(words[i % 10], small_, Rect{ x, y, x + 140.f, y + 20.f },
                    g.createSolidColorBrush(hue(hash01(i, 5), 1.f)));
    }
}

void BenchClient::drawGradients(Graphics& g, float t)
{
    const float w = bounds_.right, h = bounds_.bottom;
    for (int i = 0; i < load; ++i)
    {
        const float s = 20.f + 100.f * hash01(i, 1);
        const float x = bounce(t * kDrift * (1 + hash01(i, 2)) + hash01(i, 3)) * (w - s);
        const float y = bounce(t * kDrift * (1 + hash01(i, 4)) + hash01(i, 5)) * (h - s);
        const Rect r{ x, y, x + s, y + s };
        if (i & 1)
            g.fillRectangle(r, g.createLinearGradientBrush(Point{ x, y }, Point{ x + s, y + s },
                                                           hue(hash01(i, 6), 0.9f), hue(hash01(i, 7), 0.3f)));
        else
            g.fillEllipse(Ellipse{ Point{ x + s / 2, y + s / 2 }, s / 2, s / 2 },
                          g.createRadialGradientBrush(Point{ x + s / 2, y + s / 2 }, s / 2,
                                                      hue(hash01(i, 6), 1.f), hue(hash01(i, 7), 0.f)));
    }
}

void BenchClient::drawOverlay(Graphics& g)
{
    if (!overlay_)
        return;

    const auto& timing = frame_.lastPresentTiming();
    char line1[200], line2[200];
    snprintf(line1, sizeof line1, "%s   %.1f fps   frame %.2f ms (p95 %.2f)   render %.2f ms   present %.2f ms",
             timing.gpu ? "GPU (bgfx / Vulkan)" : "CPU (software)",
             stats_.fps(), stats_.frameMs(), stats_.frameP95(), stats_.renderMs(), stats_.presentMs());
    std::string scene2 = std::string("scene ") + std::to_string(scene + 1) + " " + kSceneNames[scene]
                       + "   load " + std::to_string(load);
    if (scene == Bounce)
        scene2 += std::string("   detail ") + std::to_string(detail) + "   " + kFillNames[fill]
                + (outline ? "   outline" : "");
    if (paused)
        scene2 += "   PAUSED";
    snprintf(line2, sizeof line2, "%s", scene2.c_str());
    const char* line3 = "1-6 scene   +/- load   [/] detail   f fill   o outline   space pause   q quit   "
                        "(backend: relaunch with --cpu / --gpu)";

    g.fillRectangle(Rect{ 8, 8, 8 + 760, 8 + 66 }, g.createSolidColorBrush(Color{ 0, 0, 0, 0.7f }));
    auto ink = g.createSolidColorBrush(Color{ 1.f, 1.f, 1.f, 1.f });
    auto dim = g.createSolidColorBrush(Color{ 0.7f, 0.72f, 0.75f, 1.f });
    g.drawTextU(line1, overlay_, Rect{ 16, 12, 760, 30 }, ink);
    g.drawTextU(line2, overlay_, Rect{ 16, 32, 760, 50 }, ink);
    g.drawTextU(line3, overlay_, Rect{ 16, 52, 760, 70 }, dim);

}

} // namespace

int main(int argc, char** argv)
{
#if GMPI_UI_GPU_BACKEND
    bool wantGpu = true;
#else
    bool wantGpu = false;
#endif
    bool paused = false;
    int scene = Bounce, load = 16, detail = 8, maxFrames = 0, width = 1000, height = 700;
    bool report = false, outline = true, staticColors = false;
    const char* dumpPath = nullptr;
    int fill = Linear;

    for (int i = 1; i < argc; ++i)
    {
        const char* a = argv[i];
        const bool hasNext = i + 1 < argc;
        if      (!strcmp(a, "--cpu")) wantGpu = false;
        else if (!strcmp(a, "--gpu")) wantGpu = true;
        else if (!strcmp(a, "--report")) report = true;
        else if (!strcmp(a, "--no-outline")) outline = false;
        else if (!strcmp(a, "--static-colors")) staticColors = true;
        else if (!strcmp(a, "--paused")) paused = true;
        else if (!strcmp(a, "--dump") && hasNext) dumpPath = argv[++i];
        else if (!strcmp(a, "--fill") && hasNext)
        {
            const char* f = argv[++i];
            fill = !strcmp(f, "solid") ? Solid : !strcmp(f, "radial") ? Radial : Linear;
        }
        else if (!strcmp(a, "--scene") && hasNext)  scene = std::clamp(atoi(argv[++i]) - 1, 0, SceneCount - 1);
        else if (!strcmp(a, "--load") && hasNext)   load = std::max(1, atoi(argv[++i]));
        else if (!strcmp(a, "--detail") && hasNext) detail = std::max(3, atoi(argv[++i]));
        else if (!strcmp(a, "--frames") && hasNext) maxFrames = atoi(argv[++i]);
        else if (!strcmp(a, "--size") && hasNext)   sscanf(argv[++i], "%dx%d", &width, &height);
        else
        {
            fprintf(stderr, "usage: %s [--cpu | --gpu] [--scene 1-6] [--load N] [--detail N] "
                            "[--fill solid|linear|radial] [--no-outline] [--static-colors] [--paused] [--dump FILE] [--size WxH] [--frames N] [--report]\n", argv[0]);
            return 2;
        }
    }

#if GMPI_UI_GPU_BACKEND
    gmpi::gpugfx::hostPreference() = wantGpu ? gmpi::gpugfx::HostPreference::Gpu
                                             : gmpi::gpugfx::HostPreference::Software;
#else
    if (wantGpu)
    {
        fprintf(stderr, "--gpu: this build has no GPU backend (configure with -DWAYLAND_BENCH_GPU=ON)\n");
        return 2;
    }
#endif

    wayland::Connection connection;
    if (!connection.open())
    {
        fprintf(stderr, "no wayland display\n");
        return 1;
    }

    wayland::WaylandToplevel frame(connection);

    // The CPU backend has no font or shaping code of its own; wire in the same
    // pieces every other host does. The GPU factory embeds this one.
    static CpuTextEngine textEngine{ findFont };
    textEngine.imageDecoder             = decodeImageMemory;
    frame.drawingFactory().imageDecoder = decodeImageFile;
    frame.drawingFactory().textEngine   = &textEngine;

    Factory facade;
    *AccessPtr::put(facade) = &frame.drawingFactory();

    const std::string_view families[] = { "Ubuntu Sans", "DejaVu Sans", "Noto Sans", "Liberation Sans" };
    auto overlayFont = facade.createTextFormat(13.0f, families);
    auto smallFont   = facade.createTextFormat(12.0f, families);
    if (!overlayFont)
        fprintf(stderr, "warning: no font found; text will not draw\n");

    if (!frame.create("gmpi_ui bench", "gmpi_ui.bench", width, height))
    {
        fprintf(stderr, "could not create window\n");
        return 1;
    }

    BenchClient client(frame, smallFont, overlayFont);
    client.scene  = Scene(scene);
    client.load   = load;
    client.detail = detail;
    client.report = report;
    client.fill   = BounceFill(fill);
    client.outline = outline;
    client.staticColors = staticColors;
    client.paused = paused;
    if (dumpPath)
    {
        client.dumpPath = dumpPath;
        client.dumpAtFrame = maxFrames > 0 ? maxFrames : 3;
    }
    frame.attachClient(&client);

    // A short tick keeps a repaint always pending, so every frame callback
    // presents and the compositor sets the pace.
    frame.runEventLoop(2, [&](int)
    {
        frame.invalidateRect(nullptr);
        client.maybeReport();
        if (maxFrames && client.framesPresented >= maxFrames)
            frame.close();
    });


    return 0;
}
