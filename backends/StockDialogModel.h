#pragma once

// ---------------------------------------------------------------------------
// The stock dialog (message box) the Linux backends draw themselves: layout,
// hit-testing and drawing over a Button list the window owns. Pure, so the
// headless tests can pin the layout without a compositor.
//
// Button metrics follow Adwaita, which is what a GNOME/Ubuntu desktop puts
// either side of this dialog: 16px of padding left and right of the label, the
// label itself deciding the width from there, a 34px row, and 6px between
// buttons in the action area. The floor keeps "OK" and "No" from shrinking to
// stubs - Adwaita has one too, for the same reason.
// ---------------------------------------------------------------------------

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "GmpiApiDrawing.h"
#include "helpers/NativeUi.h"
#include "backends/CpuGfx.h"

namespace gmpi::stockdialog
{

constexpr int kDialogMargin     = 20;
constexpr int kDialogButtonPadH = 16;
constexpr int kDialogButtonMinW = 64;
constexpr int kDialogButtonH    = 34;
constexpr int kDialogButtonGap  = 6;
constexpr int kDialogTextWidth  = 380;   // content width before margins

struct Button
{
    std::string label;
    gmpi::api::StockDialogButton id{};
    gmpi::drawing::Rect rect{};
    gmpi::drawing::Size labelSize{};   // measured in layout(), centres the label
};

// Where a button's label draws: centred both ways on the measured extent.
inline gmpi::drawing::Point labelOrigin(const Button& b)
{
    return { b.rect.left + 0.5f * ((b.rect.right - b.rect.left) - b.labelSize.width),
             b.rect.top  + 0.5f * ((b.rect.bottom - b.rect.top) - b.labelSize.height) };
}

// Sizes the box to the message and lays the buttons along its bottom edge,
// right-aligned with the first one rightmost so the default sits where the eye
// lands. Without a font nothing can be measured: fixed height, floor widths.
inline void layout(const std::string& text, gmpi::drawing::api::ITextFormat* font,
                   std::vector<Button>& buttons, int& w, int& h)
{
    float textH = 40.0f;
    if (font)
    {
        gmpi::drawing::Size sz{};
        if (font->getTextExtentU(text.c_str(), int32_t(text.size()),
                                 float(kDialogTextWidth), &sz) == gmpi::ReturnCode::Ok)
            textH = (std::max)(20.0f, sz.height);
    }

    w = kDialogTextWidth + 2 * kDialogMargin;
    h = kDialogMargin + int(textH) + 24 + kDialogButtonH + kDialogMargin;

    int x = w - kDialogMargin;
    const int y = h - kDialogMargin - kDialogButtonH;
    for (auto& b : buttons)
    {
        b.labelSize = {};
        if (font)
            font->getTextExtentU(b.label.c_str(), int32_t(b.label.size()),
                                 float(kDialogTextWidth), &b.labelSize);

        const int bw = (std::max)(kDialogButtonMinW,
                                  int(std::ceil(b.labelSize.width)) + 2 * kDialogButtonPadH);
        x -= bw;
        b.rect = { float(x), float(y), float(x + bw), float(y + kDialogButtonH) };
        x -= kDialogButtonGap;
    }
}

// Index of the button under (x, y), or -1.
inline int buttonAt(const std::vector<Button>& buttons, double x, double y)
{
    for (size_t i = 0; i < buttons.size(); ++i)
    {
        const auto& r = buttons[i].rect;
        if (x >= r.left && x < r.right && y >= r.top && y < r.bottom)
            return int(i);
    }
    return -1;
}

// hovered is -1 for none. font may be null - the buttons draw, their labels
// and the message do not.
inline void render(gmpi::cpugfx::RenderTarget* rt, const std::string& text,
                   const std::vector<Button>& buttons, gmpi::drawing::api::ITextFormat* font,
                   int hovered, int h)
{
    auto brush = [&](gmpi::drawing::Color c)
    {
        gmpi::drawing::api::ISolidColorBrush* b{};
        rt->createSolidColorBrush(&c, nullptr, &b);
        return b;
    };

    const gmpi::drawing::Color bg{ 0.16f, 0.17f, 0.19f, 1.0f };
    rt->clear(&bg);

    auto* ink     = brush({ 0.92f, 0.93f, 0.95f, 1.0f });
    auto* face    = brush({ 0.24f, 0.25f, 0.28f, 1.0f });
    auto* faceHot = brush({ 0.30f, 0.44f, 0.68f, 1.0f });
    auto* edge    = brush({ 0.38f, 0.39f, 0.43f, 1.0f });

    if (font && ink)
    {
        const gmpi::drawing::Rect tr{ float(kDialogMargin), float(kDialogMargin),
                                      float(kDialogMargin + kDialogTextWidth),
                                      float(h - kDialogMargin - kDialogButtonH - 12) };
        rt->drawTextU(text.c_str(), uint32_t(text.size()), font, &tr, ink, 0);
    }

    for (size_t i = 0; i < buttons.size(); ++i)
    {
        const auto& b = buttons[i];
        if (auto* fill = (int(i) == hovered) ? faceHot : face; fill)
            rt->fillRectangle(&b.rect, fill);
        if (edge)
            rt->drawRectangle(&b.rect, edge, 1.0f, nullptr);

        if (font && ink)
        {
            // The shared menu font is left alone deliberately - it draws the
            // menus and the text edit too, so setting Center alignment on it
            // would recentre those as well; offsetting the layout rect keeps
            // the centring to this dialog.
            const auto o = labelOrigin(b);
            const gmpi::drawing::Rect lr{ o.x, o.y, b.rect.right, b.rect.bottom };
            rt->drawTextU(b.label.c_str(), uint32_t(b.label.size()), font, &lr, ink, 0);
        }
    }

    if (ink)     ink->release();
    if (face)    face->release();
    if (faceHot) faceHot->release();
    if (edge)    edge->release();
}

} // namespace gmpi::stockdialog
