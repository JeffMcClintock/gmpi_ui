// The scenes and the timing loop of the headless backend benchmarks
// (examples/WinBench, examples/MacBench): those of examples/WaylandBench, with
// the same hash-driven motion, so every platform's numbers are like for like.
// A benchmark sets up its backend's factory and calls run().
//
// One line per (scene, load):
//
//     backend scene load  cpu_ms  frame_ms  fps
//
// cpu_ms   mean time the client spends inside beginDraw..endDraw: the cost of
//          recording and submitting a frame
// frame_ms mean wall time per frame over the whole run, with the device drained
//          (a one-pixel readback) every kSyncEvery frames: what a frame really
//          costs once the GPU has finished it

#pragma once

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <string>
#include <string_view>

#include "Drawing.h"

namespace gmpi_bench
{

using namespace gmpi::drawing;
// macOS declares the QuickDraw Point and Rect at global scope (MacTypes.h);
// these win over them inside this namespace.
using gmpi::drawing::Point;
using gmpi::drawing::Rect;

constexpr float kPi = 3.14159265f;
constexpr float kDrift = 0.125f;
constexpr int   kSyncEvery = 30;

inline float hash01(uint32_t i, uint32_t salt)
{
    uint32_t x = i * 747796405u + salt * 2891336453u;
    x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15; x *= 0x846ca68bu; x ^= x >> 16;
    return float(x & 0xffffff) / float(0x1000000);
}

inline Color hue(float h, float a = 1.f)
{
    return { 0.5f + 0.5f * cosf(2 * kPi * (h + 0.0f)),
             0.5f + 0.5f * cosf(2 * kPi * (h + 0.33f)),
             0.5f + 0.5f * cosf(2 * kPi * (h + 0.67f)), a };
}

inline float bounce(float t)
{
    const float f = t - floorf(t);
    return f < 0.5f ? f * 2.f : 2.f - f * 2.f;
}

struct Scenes
{
    Rect       bounds{ 0, 0, 1000, 700 };
    TextFormat text;
    PathGeometry star;
    int        starDetail = -1;
    int        load = 16;
    int        detail = 8;
    bool       staticColors = false; // else the bounce colours drift, so every gradient is new each frame

    void bounceScene(Graphics& g, float t, int fill /*0 solid, 1 linear, 2 radial*/, bool outline)
    {
        if (starDetail != detail)
        {
            star = g.getFactory().createPathGeometry();
            auto sink = star.open();
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
            starDetail = detail;
        }

        const float w = bounds.right - bounds.left, h = bounds.bottom - bounds.top;
        const float size = std::clamp((std::min)(w, h) / (2.f + sqrtf(float(load)) * 0.6f), 24.f, 260.f);
        auto stroke = g.createSolidColorBrush(Color{ 1.f, 1.f, 1.f, 0.85f });
        const auto base = g.getTransform();

        for (int i = 0; i < load; ++i)
        {
            const float sx = 0.07f + 0.10f * hash01(i, 1), sy = 0.07f + 0.10f * hash01(i, 2);
            const float cx = size * 0.5f + bounce(t * sx + hash01(i, 3)) * (w - size);
            const float cy = size * 0.5f + bounce(t * sy + hash01(i, 4)) * (h - size);
            const float spin = (hash01(i, 5) - 0.5f) * 3.f * t;
            const float pulse = size * (0.75f + 0.25f * sinf(t * (1.f + hash01(i, 6)) + float(i)));
            g.setTransform(makeScale(pulse, pulse) * makeRotation(spin) * makeTranslation(cx, cy) * base);

            const float h0 = hash01(i, 7) + (staticColors ? 0.f : 0.05f * t);
            switch (fill)
            {
            case 0: g.fillGeometry(star, g.createSolidColorBrush(hue(h0, 0.85f))); break;
            case 1: g.fillGeometry(star, g.createLinearGradientBrush(Point{ -0.5f, -0.5f }, Point{ 0.5f, 0.5f }, hue(h0, 0.9f), hue(h0 + 0.4f, 0.9f))); break;
            default: g.fillGeometry(star, g.createRadialGradientBrush(Point{ 0, 0 }, 0.6f, hue(h0, 0.95f), hue(h0 + 0.5f, 0.6f))); break;
            }
            if (outline)
                g.drawGeometry(star, stroke, 2.f / pulse);
        }
        g.setTransform(base);
    }

    void lines(Graphics& g, float t)
    {
        const float w = bounds.right, h = bounds.bottom;
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

    void rects(Graphics& g, float t)
    {
        const float w = bounds.right, h = bounds.bottom;
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

    void ellipses(Graphics& g, float t)
    {
        const float w = bounds.right, h = bounds.bottom;
        for (int i = 0; i < load; ++i)
        {
            const float r = 5.f + 45.f * hash01(i, 1);
            const float x = r + bounce(t * kDrift * (1 + hash01(i, 2)) + hash01(i, 3)) * (w - 2 * r);
            const float y = r + bounce(t * kDrift * (1 + hash01(i, 4)) + hash01(i, 5)) * (h - 2 * r);
            const gmpi::drawing::Ellipse e{ Point{ x, y }, r, r * (0.6f + 0.4f * hash01(i, 6)) };
            g.fillEllipse(e, g.createSolidColorBrush(hue(hash01(i, 7), 0.6f)));
            g.drawEllipse(e, g.createSolidColorBrush(hue(hash01(i, 8), 1.f)), 1.5f);
        }
    }

    void texts(Graphics& g, float t)
    {
        if (!text)
            return;
        const float w = bounds.right, h = bounds.bottom;
        static const char* const words[] = { "Oscillator", "Filter Cutoff", "Envelope", "LFO Rate", "Resonance",
                                             "Gain -6.0 dB", "440.00 Hz", "Attack 12 ms", "Mix 50%", "Voice 7" };
        for (int i = 0; i < load; ++i)
        {
            const float x = bounce(t * kDrift * (1 + hash01(i, 1)) + hash01(i, 2)) * (w - 120.f);
            const float y = bounce(t * kDrift * (1 + hash01(i, 3)) + hash01(i, 4)) * (h - 20.f);
            g.drawTextU(words[i % 10], text, Rect{ x, y, x + 140.f, y + 20.f }, g.createSolidColorBrush(hue(hash01(i, 5), 1.f)));
        }
    }

    void gradients(Graphics& g, float t)
    {
        const float w = bounds.right, h = bounds.bottom;
        for (int i = 0; i < load; ++i)
        {
            const float s = 20.f + 100.f * hash01(i, 1);
            const float x = bounce(t * kDrift * (1 + hash01(i, 2)) + hash01(i, 3)) * (w - s);
            const float y = bounce(t * kDrift * (1 + hash01(i, 4)) + hash01(i, 5)) * (h - s);
            const Rect r{ x, y, x + s, y + s };
            if (i & 1)
                g.fillRectangle(r, g.createLinearGradientBrush(Point{ x, y }, Point{ x + s, y + s }, hue(hash01(i, 6), 0.9f), hue(hash01(i, 7), 0.3f)));
            else
                g.fillEllipse(gmpi::drawing::Ellipse{ Point{ x + s / 2, y + s / 2 }, s / 2, s / 2 },
                              g.createRadialGradientBrush(Point{ x + s / 2, y + s / 2 }, s / 2, hue(hash01(i, 6), 1.f), hue(hash01(i, 7), 0.f)));
        }
    }
};

struct Case
{
    const char* scene;
    int         load;
};

// Light to heavy, per scene.
inline constexpr Case kCases[] = {
    { "bounce", 4 },     { "bounce", 16 },    { "bounce", 64 },
    { "lines", 100 },    { "lines", 1000 },   { "lines", 5000 },
    { "rects", 100 },    { "rects", 1000 },   { "rects", 5000 },
    { "ellipses", 100 }, { "ellipses", 1000 }, { "ellipses", 5000 },
    { "text", 50 },      { "text", 200 },     { "text", 1000 },
    { "gradients", 50 }, { "gradients", 200 }, { "gradients", 1000 },
};

inline void drawCase(Scenes& s, Graphics& g, const std::string& scene, float t)
{
    g.clear(Color{ 0.07f, 0.08f, 0.10f, 1.f });
    if (scene == "bounce")         s.bounceScene(g, t, 1, true);
    else if (scene == "lines")     s.lines(g, t);
    else if (scene == "rects")     s.rects(g, t);
    else if (scene == "ellipses")  s.ellipses(g, t);
    else if (scene == "text")      s.texts(g, t);
    else if (scene == "gradients") s.gradients(g, t);
}

using Clock = std::chrono::steady_clock;
inline double msSince(Clock::time_point a) { return std::chrono::duration<double, std::milli>(Clock::now() - a).count(); }

// A one-pixel read: on the GPU it waits for the device, on Direct2D it flushes.
inline void drain(BitmapRenderTarget& rt)
{
    auto bitmap = rt.getBitmap();
    auto pixels = bitmap.lockPixels(BitmapLockFlags::Read);
    if (pixels)
    {
        volatile uint8_t sink = *pixels.getAddress();
        (void)sink;
    }
}

// Every case (or only `only`'s) on an offscreen target of `factory`'s backend.
// staticColors holds the bounce scene's colours still: by default they drift, so
// every gradient is new each frame (a stress test of the GPU's gradient atlas);
// held, the gradients are reused, as in a UI.
inline int run(Factory& factory, const char* backend, std::string_view fontFamily,
               int width, int height, int frames, const std::string& only, bool staticColors = false)
{
    frames = (std::max)(kSyncEvery, (frames / kSyncEvery) * kSyncEvery);

    Scenes scenes;
    scenes.bounds = Rect{ 0, 0, float(width), float(height) };
    scenes.staticColors = staticColors;
    scenes.text = factory.createTextFormat(14.f, { &fontFamily, 1 });

    auto rt = factory.createCpuRenderTarget({ uint32_t(width), uint32_t(height) }, 0);
    if (!AccessPtr::get(rt))
    {
        fprintf(stderr, "no offscreen target\n");
        return 1;
    }

    for (const auto& c : kCases)
    {
        if (!only.empty() && only != c.scene)
            continue;
        scenes.load = c.load;
        const std::string scene = c.scene;

        auto frame = [&](int n, double* recordMs) {
            const auto t0 = Clock::now();
            rt.beginDraw();
            {
                Graphics g(AccessPtr::get(rt));
                drawCase(scenes, g, scene, float(n) / 60.f);
            }
            rt.endDraw();
            if (recordMs)
                *recordMs += msSince(t0);
        };

        for (int n = 0; n < 30; ++n) // warm-up: pipelines, glyph and gradient caches
            frame(n, nullptr);
        drain(rt);

        double recordMs = 0;
        const auto start = Clock::now();
        for (int n = 0; n < frames; ++n)
        {
            frame(30 + n, &recordMs);
            if ((n + 1) % kSyncEvery == 0)
                drain(rt);
        }
        const double total = msSince(start);
        printf("%s %s %d %.3f %.3f %.1f\n", backend, c.scene, c.load, recordMs / frames, total / frames, 1000.0 * frames / total);
        fflush(stdout);
    }
    return 0;
}

} // namespace gmpi_bench
