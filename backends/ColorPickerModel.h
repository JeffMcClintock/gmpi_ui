#pragma once

// ---------------------------------------------------------------------------
// The colour picker, less its window: the colour space conversions, the layout
// of the saturation square / hue strip / alpha strip / buttons, which of those
// a point lands on, and how the whole thing is drawn.
//
// Shared by the Wayland and X11 backends, and testable without a display -
// gmpi_ui/tests pins the round trip through HSV and every hit-test region.
//
// The colour handed in and out is LINEAR, because that is what the library
// uses; the picker works in sRGB, because that is the space a human is picking
// in. An evenly spaced hue strip in linear light is not evenly spaced to the
// eye, and a value slider in linear light spends most of its travel in the
// dark end.
// ---------------------------------------------------------------------------

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "GmpiApiDrawing.h"
#include "backends/CpuGfx.h"

namespace gmpi::colorpicker
{

constexpr int kMargin  = 20;
constexpr int kSquare  = 200;
constexpr int kStripW  = 26;
constexpr int kButtonW = 92;
constexpr int kButtonH = 30;

struct Hsv { float h{}, s{}, v{}, a{ 1.f }; };   // h in [0,360), rest [0,1]

// sRGB transfer function, decode direction. linearToSRGB01 is its inverse and
// lives in GmpiApiDrawing.h; this is the only other copy in the library.
inline float srgbToLinear01(float c)
{
    c = c < 0.f ? 0.f : (c > 1.f ? 1.f : c);
    return (c <= 0.04045f) ? c / 12.92f
                           : std::pow((c + 0.055f) / 1.055f, 2.4f);
}

inline void hsvToSrgb(const Hsv& in, float& r, float& g, float& b)
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

inline Hsv srgbToHsv(float r, float g, float b, float a)
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

    if (out.h < 0.f)
        out.h += 360.f;

    return out;
}

// ---------------------------------------------------------------------------

class Model
{
public:
    enum class Region { None, SatVal, Hue, Alpha, Ok, Cancel };

    struct Button { std::string label; bool ok; gmpi::drawing::Rect rect{}; };

    Model() { layout(); }

    void setFromLinear(gmpi::drawing::Color c)
    {
        hsv_ = srgbToHsv(gmpi::drawing::linearToSRGB01(c.r),
                         gmpi::drawing::linearToSRGB01(c.g),
                         gmpi::drawing::linearToSRGB01(c.b), c.a);
    }

    // the picked colour, converted back to the linear space the library uses
    gmpi::drawing::Color color() const
    {
        float r, g, b;
        hsvToSrgb(hsv_, r, g, b);
        return { srgbToLinear01(r), srgbToLinear01(g), srgbToLinear01(b), hsv_.a };
    }

    const Hsv& hsv() const { return hsv_; }

    int contentWidth()  const { return w_; }
    int contentHeight() const { return h_; }

    const gmpi::drawing::Rect& satValRect()  const { return svRect_; }
    const gmpi::drawing::Rect& hueRect()     const { return hueRect_; }
    const gmpi::drawing::Rect& alphaRect()   const { return alphaRect_; }
    const gmpi::drawing::Rect& previewRect() const { return previewRect_; }
    const std::vector<Button>& buttons()     const { return buttons_; }

    Region regionAt(double x, double y) const
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

    // Returns true if the colour changed, so the caller knows to repaint.
    bool applyDrag(Region r, double x, double y)
    {
        auto clamp01 = [](double v) { return float(v < 0 ? 0 : (v > 1 ? 1 : v)); };

        switch (r)
        {
        case Region::SatVal:
            hsv_.s = clamp01((x - svRect_.left) / (svRect_.right - svRect_.left));
            hsv_.v = 1.f - clamp01((y - svRect_.top) / (svRect_.bottom - svRect_.top));
            return true;
        case Region::Hue:
            hsv_.h = clamp01((y - hueRect_.top) / (hueRect_.bottom - hueRect_.top)) * 359.99f;
            return true;
        case Region::Alpha:
            hsv_.a = 1.f - clamp01((y - alphaRect_.top) / (alphaRect_.bottom - alphaRect_.top));
            return true;
        default:
            return false;
        }
    }

    int buttonAt(double x, double y) const
    {
        for (size_t i = 0; i < buttons_.size(); ++i)
        {
            const auto& r = buttons_[i].rect;
            if (x >= r.left && x < r.right && y >= r.top && y < r.bottom)
                return int(i);
        }
        return -1;
    }

private:
    void layout()
    {
        const float m = float(kMargin);

        svRect_    = { m, m, m + kSquare, m + kSquare };
        hueRect_   = { svRect_.right + 12.f, m, svRect_.right + 12.f + kStripW, m + kSquare };
        alphaRect_ = { hueRect_.right + 12.f, m, hueRect_.right + 12.f + kStripW, m + kSquare };

        w_ = int(alphaRect_.right + m);

        // Preview and buttons on SEPARATE rows. Sharing one row read fine in the
        // code and overlapped on screen: the preview's right edge (m + 90) sat
        // under Cancel's left edge once the buttons packed in from the right.
        previewRect_ = { m, svRect_.bottom + 10.f, m + 90.f, svRect_.bottom + 10.f + 24.f };

        h_ = int(previewRect_.bottom + 12.f + kButtonH + m);

        buttons_ = { { "OK", true }, { "Cancel", false } };
        int x = w_ - kMargin;
        const int y = int(previewRect_.bottom + 12.f);
        for (auto& b : buttons_)
        {
            x -= kButtonW;
            b.rect = { float(x), float(y), float(x + kButtonW), float(y + kButtonH) };
            x -= 10;
        }
    }

    Hsv hsv_;
    gmpi::drawing::Rect svRect_{}, hueRect_{}, alphaRect_{}, previewRect_{};
    std::vector<Button> buttons_;
    int w_ = 0, h_ = 0;
};

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------
// Shared too: it is a hundred lines of gradient banding and marker arithmetic
// operating purely on the Model, and two copies of that would drift.
//
// hoveredButton is -1 for none. font may be null - the swatches still draw,
// the button labels do not.
inline void render(gmpi::cpugfx::RenderTarget* rt, const Model& m,
                   gmpi::drawing::api::ITextFormat* font, int hoveredButton)
{
    // A brush in the library's linear space, from an sRGB colour.
    auto srgbBrush = [&](float r, float g, float b, float a = 1.f)
    {
        gmpi::drawing::api::ISolidColorBrush* br{};
        const gmpi::drawing::Color c{ srgbToLinear01(r), srgbToLinear01(g), srgbToLinear01(b), a };
        rt->createSolidColorBrush(&c, nullptr, &br);
        return br;
    };
    auto plainBrush = [&](gmpi::drawing::Color c)
    {
        gmpi::drawing::api::ISolidColorBrush* br{};
        rt->createSolidColorBrush(&c, nullptr, &br);
        return br;
    };

    const auto& sv    = m.satValRect();
    const auto& hue   = m.hueRect();
    const auto& alpha = m.alphaRect();
    const auto& prev  = m.previewRect();
    const auto& hsv   = m.hsv();

    const gmpi::drawing::Color bg{ 0.16f, 0.17f, 0.19f, 1.0f };
    rt->clear(&bg);

    // --- saturation / value square -----------------------------------------
    // Banded rather than per-pixel: 40000 one-pixel fills per repaint is not a
    // thing to do on a CPU renderer, and at this size the seams are invisible.
    constexpr int kBands = 48;
    const float bw = (sv.right - sv.left) / kBands;
    const float bh = (sv.bottom - sv.top) / kBands;

    for (int iy = 0; iy < kBands; ++iy)
    {
        for (int ix = 0; ix < kBands; ++ix)
        {
            float r, g, b;
            hsvToSrgb({ hsv.h, (ix + 0.5f) / kBands, 1.f - (iy + 0.5f) / kBands }, r, g, b);
            if (auto* br = srgbBrush(r, g, b))
            {
                const gmpi::drawing::Rect cell{ sv.left + ix * bw, sv.top + iy * bh,
                                                sv.left + (ix + 1) * bw + 1.f,
                                                sv.top + (iy + 1) * bh + 1.f };
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
        if (auto* br = srgbBrush(r, g, b))
        {
            const gmpi::drawing::Rect cell{ hue.left, hue.top + iy * bh,
                                            hue.right, hue.top + (iy + 1) * bh + 1.f };
            rt->fillRectangle(&cell, br);
            br->release();
        }
    }

    // --- alpha strip: the colour over a chequer, so transparency reads -------
    {
        float r, g, b;
        hsvToSrgb(hsv, r, g, b);
        for (int iy = 0; iy < kBands; ++iy)
        {
            const float a = 1.f - (iy + 0.5f) / kBands;
            const float chequer = ((iy / 4) % 2) ? 0.55f : 0.75f;
            if (auto* br = srgbBrush(r * a + chequer * (1.f - a),
                                     g * a + chequer * (1.f - a),
                                     b * a + chequer * (1.f - a)))
            {
                const gmpi::drawing::Rect cell{ alpha.left, alpha.top + iy * bh,
                                                alpha.right, alpha.top + (iy + 1) * bh + 1.f };
                rt->fillRectangle(&cell, br);
                br->release();
            }
        }
    }

    // --- preview + chrome ---------------------------------------------------
    {
        float r, g, b;
        hsvToSrgb(hsv, r, g, b);
        if (auto* br = srgbBrush(r, g, b, hsv.a))
        {
            rt->fillRectangle(&prev, br);
            br->release();
        }
    }

    auto* ink     = plainBrush({ 0.92f, 0.93f, 0.95f, 1.f });
    auto* edge    = plainBrush({ 0.38f, 0.39f, 0.43f, 1.f });
    auto* face    = plainBrush({ 0.24f, 0.25f, 0.28f, 1.f });
    auto* faceHot = plainBrush({ 0.30f, 0.44f, 0.68f, 1.f });

    if (edge)
    {
        rt->drawRectangle(&sv, edge, 1.f, nullptr);
        rt->drawRectangle(&hue, edge, 1.f, nullptr);
        rt->drawRectangle(&alpha, edge, 1.f, nullptr);
        rt->drawRectangle(&prev, edge, 1.f, nullptr);
    }

    // markers for the current position in each control
    if (ink)
    {
        const float mx = sv.left + hsv.s * (sv.right - sv.left);
        const float my = sv.top + (1.f - hsv.v) * (sv.bottom - sv.top);
        const gmpi::drawing::Rect cross{ mx - 5.f, my - 5.f, mx + 5.f, my + 5.f };
        rt->drawRectangle(&cross, ink, 2.f, nullptr);

        const float hy = hue.top + (hsv.h / 360.f) * (hue.bottom - hue.top);
        const gmpi::drawing::Rect hmark{ hue.left - 2.f, hy - 2.f, hue.right + 2.f, hy + 2.f };
        rt->drawRectangle(&hmark, ink, 2.f, nullptr);

        const float ay = alpha.top + (1.f - hsv.a) * (alpha.bottom - alpha.top);
        const gmpi::drawing::Rect amark{ alpha.left - 2.f, ay - 2.f, alpha.right + 2.f, ay + 2.f };
        rt->drawRectangle(&amark, ink, 2.f, nullptr);
    }

    // --- buttons ------------------------------------------------------------
    for (size_t i = 0; i < m.buttons().size(); ++i)
    {
        const auto& b = m.buttons()[i];
        if (auto* fill = (int(i) == hoveredButton) ? faceHot : face; fill)
            rt->fillRectangle(&b.rect, fill);
        if (edge)
            rt->drawRectangle(&b.rect, edge, 1.f, nullptr);
        if (font && ink)
        {
            const gmpi::drawing::Rect lr{ b.rect.left, b.rect.top + 6.f, b.rect.right, b.rect.bottom };
            rt->drawTextU(b.label.c_str(), uint32_t(b.label.size()), font, &lr, ink, 0);
        }
    }

    if (ink)     ink->release();
    if (edge)    edge->release();
    if (face)    face->release();
    if (faceHot) faceHot->release();
}

} // namespace gmpi::colorpicker
