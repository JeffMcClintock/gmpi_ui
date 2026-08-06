#pragma once

// ---------------------------------------------------------------------------
// The in-place text editor's MODEL: the buffer, the caret, the selection, and
// what a keystroke does to them. No windowing, no drawing, no clipboard - those
// arrive as callbacks.
//
// Shared by the Wayland and X11 backends. This is the part that took the real
// debugging - UTF-8 stepping that does not land mid-codepoint, arrows that
// collapse a selection to its EDGE rather than stepping from the caret, a paste
// filter that survives whatever another program put on the clipboard - and it
// is the part that needs no display server to test. gmpi_ui/tests exercises it
// through WaylandTextEdit, which forwards straight here.
//
// Keysyms are the X11 values throughout. xkbcommon uses the same numbers
// (XKB_KEY_Left == XK_Left), so one table serves both backends.
// ---------------------------------------------------------------------------

#include <algorithm>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <utility>

#include "helpers/NativeUi.h"

namespace gmpi::textedit
{

class Model
{
public:
    // --- wired by the backend ---------------------------------------------
    // Clipboard access. Left unset, the copy/cut/paste chords do nothing rather
    // than misbehave.
    std::function<void(const std::string&)> setClipboard;
    std::function<std::string()>            getClipboard;

    // The text changed. The backend forwards this to ITextEditCallback::onChanged.
    std::function<void()> onChanged;

    // Return committed the edit, or Escape abandoned it.
    std::function<void(gmpi::ReturnCode)> onFinish;

    // --- state -------------------------------------------------------------
    const std::string& text() const { return text_; }
    void setText(std::string t)
    {
        text_ = std::move(t);
        caret_ = int32_t(text_.size());
        selAnchor_ = caret_;
    }

    int32_t caret() const { return caret_; }

    std::pair<int32_t, int32_t> selection() const
    { return { (std::min)(selAnchor_, caret_), (std::max)(selAnchor_, caret_) }; }

    bool hasSelection() const { return selAnchor_ != caret_; }

    std::string selectedText() const
    {
        const auto [a, b] = selection();
        return text_.substr(size_t(a), size_t(b - a));
    }

    void selectAll() { selAnchor_ = 0; caret_ = int32_t(text_.size()); }

    // Both ends together - a click places the caret and drops any selection.
    void placeCaret(int32_t pos)
    {
        caret_ = std::clamp(pos, 0, int32_t(text_.size()));
        selAnchor_ = caret_;
    }

    // UTF-8 aware stepping: continuation bytes are 10xxxxxx, so skip them.
    int32_t nextCodepoint(int32_t i) const
    {
        const int32_t n = int32_t(text_.size());
        if (i >= n)
            return n;
        ++i;
        while (i < n && (uint8_t(text_[size_t(i)]) & 0xC0) == 0x80)
            ++i;
        return i;
    }

    int32_t prevCodepoint(int32_t i) const
    {
        if (i <= 0)
            return 0;
        --i;
        while (i > 0 && (uint8_t(text_[size_t(i)]) & 0xC0) == 0x80)
            --i;
        return i;
    }

    void deleteSelection()
    {
        if (!hasSelection())
            return;
        const auto [a, b] = selection();
        text_.erase(size_t(a), size_t(b - a));
        caret_ = a;
        selAnchor_ = a;
    }

    void insertUtf32(uint32_t cp)
    {
        deleteSelection();

        // encode as UTF-8; the edit buffer is bytes, the model is codepoints
        char buf[4];
        int n = 0;
        if (cp < 0x80) { buf[n++] = char(cp); }
        else if (cp < 0x800)
        {
            buf[n++] = char(0xC0 | (cp >> 6));
            buf[n++] = char(0x80 | (cp & 0x3F));
        }
        else if (cp < 0x10000)
        {
            buf[n++] = char(0xE0 | (cp >> 12));
            buf[n++] = char(0x80 | ((cp >> 6) & 0x3F));
            buf[n++] = char(0x80 | (cp & 0x3F));
        }
        else
        {
            buf[n++] = char(0xF0 | (cp >> 18));
            buf[n++] = char(0x80 | ((cp >> 12) & 0x3F));
            buf[n++] = char(0x80 | ((cp >> 6) & 0x3F));
            buf[n++] = char(0x80 | (cp & 0x3F));
        }

        text_.insert(size_t(caret_), buf, size_t(n));
        caret_ += n;
        selAnchor_ = caret_;
    }

    // Horizontal scroll that keeps the caret in view. Pure arithmetic, so the
    // backends do not each get it subtly wrong.
    static float scrollFor(float caretX, float textWidth, float span, float current)
    {
        float sx = current;
        if (caretX - sx > span) sx = caretX - span;
        if (caretX < sx)        sx = caretX;
        if (sx > 0.f && textWidth - sx < span)
            sx = (std::max)(0.f, textWidth - span);
        return sx;
    }

    // --- the keystroke table ----------------------------------------------
    void handleKey(uint32_t keysym, uint32_t utf32, int32_t flags, bool down)
    {
        if (!down)
            return;

        const bool ctrl  = (flags & int32_t(gmpi::api::PointerFlags::KeyControl)) != 0;
        const bool shift = (flags & int32_t(gmpi::api::PointerFlags::KeyShift)) != 0;

        // moving the caret collapses the selection unless shift is held
        auto moveTo = [&](int32_t pos)
        {
            caret_ = pos;
            if (!shift)
                selAnchor_ = pos;
        };

        if (ctrl)
        {
            switch (keysym)
            {
            case 'a': case 'A': selectAll(); return;
            case 'c': case 'C':
                if (hasSelection() && setClipboard)
                    setClipboard(selectedText());
                return;
            case 'x': case 'X':
                if (hasSelection() && setClipboard)
                {
                    setClipboard(selectedText());
                    deleteSelection();
                    changed();
                }
                return;
            case 'v': case 'V':
            {
                if (!getClipboard)
                    return;

                // Clipboard content is whatever some other program put there. A
                // newline or NUL in a single-line value corrupts both the display
                // and the string handed to the host, so apply the same filter as
                // typing.
                const std::string raw = getClipboard();
                std::string clip;
                clip.reserve(raw.size());
                for (const char ch : raw)
                {
                    const auto byte = uint8_t(ch);
                    if (byte == 0)
                        break;                       // NUL ends it; the rest is not text
                    if (byte >= 0x20 && byte != 0x7f)
                        clip += ch;
                }

                if (!clip.empty())
                {
                    deleteSelection();
                    text_.insert(size_t(caret_), clip);
                    caret_ += int32_t(clip.size());
                    selAnchor_ = caret_;
                    changed();
                }
                return;
            }
            default: return;   // any other ctrl chord is a shortcut, not text
            }
        }

        switch (keysym)
        {
        case 0xff08:                       // Backspace
            if (hasSelection())
                deleteSelection();
            else if (caret_ > 0)
            {
                const int32_t p = prevCodepoint(caret_);
                text_.erase(size_t(p), size_t(caret_ - p));
                caret_ = p;
                selAnchor_ = p;
            }
            changed();
            return;

        case 0xffff:                       // Delete
            if (hasSelection())
                deleteSelection();
            else if (caret_ < int32_t(text_.size()))
            {
                const int32_t nx = nextCodepoint(caret_);
                text_.erase(size_t(caret_), size_t(nx - caret_));
            }
            changed();
            return;

        // With a selection and no shift, an arrow collapses to that EDGE - it
        // does not step from the caret, which would drop a character off the end.
        case 0xff51:                                                     // Left
            if (!shift && hasSelection()) moveTo(selection().first);
            else                          moveTo(prevCodepoint(caret_));
            return;
        case 0xff53:                                                     // Right
            if (!shift && hasSelection()) moveTo(selection().second);
            else                          moveTo(nextCodepoint(caret_));
            return;
        case 0xff50: moveTo(0); return;                                  // Home
        case 0xff57: moveTo(int32_t(text_.size())); return;              // End

        case 0xff0d: case 0xff8d:          // Return / KP_Enter: commit
            if (onFinish) onFinish(gmpi::ReturnCode::Ok);
            return;

        case 0xff1b:                       // Escape: abandon
            if (onFinish) onFinish(gmpi::ReturnCode::Cancel);
            return;

        default: break;
        }

        // Anything that produced a printable character is text. Control codes
        // below space are not - they would otherwise be inserted as invisible
        // rubbish.
        if (utf32 >= 0x20 && utf32 != 0x7f)
        {
            insertUtf32(utf32);
            changed();
        }
    }

private:
    void changed() { if (onChanged) onChanged(); }

    std::string text_;
    int32_t caret_ = 0;
    int32_t selAnchor_ = 0;
};

} // namespace gmpi::textedit
