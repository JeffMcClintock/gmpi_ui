#pragma once

// ---------------------------------------------------------------------------
// Tooltip timing and layout, shared by the Linux backends.
//
// The rule comes from DrawingFrameWin: a countdown reset by any mouse activity,
// firing ONCE when the pointer has been still long enough, and hidden again the
// moment anything moves. Windows counts 40 ticks of a 60Hz timer; this counts
// milliseconds instead, so a backend with a different tick rate still waits the
// same length of time.
//
// The frames ask IInputClient::getToolTip only when ready() says so - that is
// the whole point of the delay. Asking on every motion would have the client
// hit-testing its whole tree at pointer rate.
// ---------------------------------------------------------------------------

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <string>

#include "GmpiApiDrawing.h"
#include "backends/CpuGfx.h"

namespace gmpi::tooltip
{

constexpr int64_t kHoverDelayMs = 650;   // ~40 ticks at 60Hz, as on Windows

// One clock for every backend, so the delay means the same thing everywhere.
inline int64_t nowMs()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}
constexpr float   kPadX = 6.f;
constexpr float   kPadY = 3.f;
constexpr float   kCursorOffsetX = 12.f;
constexpr float   kCursorOffsetY = 20.f;

class Model
{
public:
    // Any pointer motion, button, or the pointer leaving. Restarts the wait and
    // takes down whatever is showing.
    void onActivity(int64_t nowMs)
    {
        lastActivityMs_ = nowMs;
        armed_ = true;
        wantHide_ = shown_;
        shown_ = false;
    }

    // The pointer left, or a drag started: no tooltip until activity resumes.
    void suppress()
    {
        armed_ = false;
        wantHide_ = shown_;
        shown_ = false;
    }

    // True exactly once per hover, when the pointer has been still long enough.
    // The caller then asks the client for text and, if it gets any, calls
    // shown().
    bool ready(int64_t nowMs)
    {
        if (!armed_ || shown_)
            return false;
        if (nowMs - lastActivityMs_ < kHoverDelayMs)
            return false;

        armed_ = false;   // one shot; re-armed by the next activity
        return true;
    }

    void markShown() { shown_ = true; }

    // True once when something needs taking off screen.
    bool takeHideRequest()
    {
        const bool h = wantHide_;
        wantHide_ = false;
        return h;
    }

    bool isShown() const { return shown_; }

    // Box big enough for the text, placed below-right of the cursor and kept on
    // screen. Coordinates are whatever the caller passes in - screen pixels for
    // an X11 toplevel, surface-local for a Wayland popup.
    static gmpi::drawing::Rect layout(const std::string& text,
                                      gmpi::drawing::api::ITextFormat* font,
                                      float cursorX, float cursorY,
                                      float screenW, float screenH)
    {
        gmpi::drawing::Size sz{ 80.f, 16.f };
        if (font)
            font->getTextExtentU(text.c_str(), int32_t(text.size()), 600.f, &sz);

        // Ceil, plus a pixel of slack. The caller turns this into an integer
        // window size, and truncating left the text area a fraction NARROWER
        // than the text: drawTextU then wrapped, and since only one line fits
        // vertically the tooltip showed the first word or two and silently lost
        // the rest.
        const float w = std::ceil(sz.width) + 2 * kPadX + 1.f;
        const float h = std::ceil(sz.height) + 2 * kPadY;

        float x = cursorX + kCursorOffsetX;
        float y = cursorY + kCursorOffsetY;

        // Flip rather than clamp on the right: a tooltip shoved left to fit
        // would sit under the pointer and be the thing it is describing.
        if (x + w > screenW) x = (std::max)(0.f, cursorX - w - 4.f);
        if (y + h > screenH) y = (std::max)(0.f, cursorY - h - 4.f);

        return { x, y, x + w, y + h };
    }

    static void render(gmpi::cpugfx::RenderTarget* rt, const std::string& text,
                       gmpi::drawing::api::ITextFormat* font, float w, float h)
    {
        auto brush = [&](gmpi::drawing::Color c)
        {
            gmpi::drawing::api::ISolidColorBrush* b{};
            rt->createSolidColorBrush(&c, nullptr, &b);
            return b;
        };

        // Same palette as the menus, so a plugin's chrome is all of a piece.
        const gmpi::drawing::Color bg{ 0.16f, 0.17f, 0.19f, 1.0f };
        rt->clear(&bg);

        auto* ink  = brush({ 0.92f, 0.93f, 0.95f, 1.0f });
        auto* edge = brush({ 0.38f, 0.39f, 0.43f, 1.0f });

        if (edge)
        {
            const gmpi::drawing::Rect all{ 0.f, 0.f, w, h };
            rt->drawRectangle(&all, edge, 1.f, nullptr);
        }

        if (font && ink)
        {
            const gmpi::drawing::Rect tr{ kPadX, kPadY, w - kPadX, h };
            rt->drawTextU(text.c_str(), uint32_t(text.size()), font, &tr, ink, 0);
        }

        if (ink)  ink->release();
        if (edge) edge->release();
    }

private:
    int64_t lastActivityMs_ = 0;
    bool    armed_ = false;
    bool    shown_ = false;
    bool    wantHide_ = false;
};

} // namespace gmpi::tooltip
