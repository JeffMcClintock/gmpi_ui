#pragma once

// ---------------------------------------------------------------------------
// The menu tree, shared by every backend that has to draw its own menus.
//
// Items arrive FLAT through IContextItemSink::addItem, with SubMenuBegin /
// SubMenuEnd markers delimiting nesting and each item carrying its own
// IPopupMenuCallback. Rebuilding a tree from that stream is the part most
// likely to be wrong and the part that needs no display server to check, so it
// lives here rather than once per platform.
//
// Wayland and X11 both use it. Windows and macOS do not - they hand the flat
// stream to a native menu widget and never build a tree at all.
// ---------------------------------------------------------------------------

#include <string>
#include <vector>

#include "helpers/NativeUi.h"
#include "GmpiSdkCommon.h"
#include "backends/CpuGfx.h"

namespace gmpi::popupmenu
{

struct Item
{
    std::string label;
    int32_t     id{};
    bool        separator{};
    bool        ticked{};
    bool        grayed{};
    std::vector<Item> submenu;              // non-empty => opens a child popup
    gmpi::shared_ptr<gmpi::api::IUnknown> callback;
};

class Builder
{
public:
    Builder() { stack_.push_back(&root_); }

    gmpi::ReturnCode addItem(const char* text, int32_t id, int32_t flags,
                             gmpi::api::IUnknown* itemCallback)
    {
        using F = gmpi::api::PopupMenuFlags;

        const bool subBegin  = (flags & int32_t(F::SubMenuBegin)) != 0;
        const bool subEnd    = (flags & int32_t(F::SubMenuEnd)) != 0;
        const bool separator = (flags & (int32_t(F::Separator) | int32_t(F::Break))) != 0;

        if (subBegin)
        {
            stack_.back()->push_back(Item{ strip(text), id });
            stack_.push_back(&stack_.back()->back().submenu);
            pendingSeparator_ = false;
            return gmpi::ReturnCode::Ok;
        }

        if (subEnd)
        {
            if (stack_.size() > 1)
                stack_.pop_back();
            pendingSeparator_ = false;
            return gmpi::ReturnCode::Ok;
        }

        if (separator)
        {
            pendingSeparator_ = true;   // committed only if a real item follows
            return gmpi::ReturnCode::Ok;
        }

        // A separator with nothing before it in this menu is dropped, and so is
        // one with nothing after - which is what deferring the commit achieves.
        if (pendingSeparator_)
        {
            if (!stack_.back()->empty())
                stack_.back()->push_back(Item{ {}, 0, true });
            pendingSeparator_ = false;
        }

        Item item{ strip(text), id };
        item.ticked = (flags & int32_t(F::Ticked)) != 0;
        item.grayed = (flags & int32_t(F::Grayed)) != 0;
        item.callback = itemCallback;   // operator= addRefs; the T* ctor does not

        stack_.back()->push_back(std::move(item));
        return gmpi::ReturnCode::Ok;
    }

    const std::vector<Item>& items() const { return root_; }

    // Seed a child menu with a subtree already built by its parent. The flat
    // stream is only parsed once, at the top level; a submenu popup is handed
    // the branch rather than replaying the items into it.
    void adopt(std::vector<Item> items)
    {
        root_ = std::move(items);
        stack_.clear();
        stack_.push_back(&root_);
        pendingSeparator_ = false;
    }

private:
    // '&' marks a mnemonic; no platform here renders one, so drop it as the
    // Win32, Cocoa and JUCE backends all do.
    static std::string strip(const char* s)
    {
        std::string r;
        for (const char* p = s ? s : ""; *p; ++p)
            if (*p != '&') r += *p;
        return r;
    }

    std::vector<Item> root_;
    std::vector<std::vector<Item>*> stack_;
    bool pendingSeparator_ = false;
};

// ---------------------------------------------------------------------------
// Geometry and drawing, shared the same way: a plugin looks the same whichever
// way the host embeds it, and two copies of this had already drifted.
// ---------------------------------------------------------------------------
constexpr int kMenuItemHeight      = 24;
constexpr int kMenuSeparatorHeight = 9;
constexpr int kMenuTickGutter      = 26;
constexpr int kMenuArrowGutter     = 22;
constexpr int kMenuPadV            = 4;
// Inset of the label from the tick gutter. A width measurement has to reserve
// the SAME value the draw uses, or the widest item is handed a rect narrower
// than the text it was measured from and drawTextU wraps it onto the item below.
constexpr int kMenuLabelInset      = 2;

// Top of item `index`; items.size() gives the bottom of the last one.
inline int itemTop(const std::vector<Item>& items, int index)
{
    int top = kMenuPadV;
    for (int i = 0; i < index && i < int(items.size()); ++i)
        top += items[i].separator ? kMenuSeparatorHeight : kMenuItemHeight;
    return top;
}

// hovered is -1 for none. font may be null - the structure still draws.
inline void render(gmpi::cpugfx::RenderTarget* rt, const std::vector<Item>& items,
                   gmpi::drawing::api::ITextFormat* font, int width, int hovered)
{
    auto brush = [&](gmpi::drawing::Color c)
    {
        gmpi::drawing::api::ISolidColorBrush* b{};
        rt->createSolidColorBrush(&c, nullptr, &b);
        return b;
    };

    const gmpi::drawing::Color bg{ 0.16f, 0.17f, 0.19f, 1.0f };
    rt->clear(&bg);

    auto* ink    = brush({ 0.92f, 0.93f, 0.95f, 1.0f });
    auto* grey   = brush({ 0.45f, 0.46f, 0.50f, 1.0f });
    auto* hilite = brush({ 0.16f, 0.42f, 0.75f, 1.0f });
    auto* rule   = brush({ 0.28f, 0.29f, 0.32f, 1.0f });
    const float w = float(width);

    for (size_t i = 0; i < items.size(); ++i)
    {
        const auto& item = items[i];
        const float top = float(itemTop(items, int(i)));

        if (item.separator)
        {
            if (rule)
            {
                const gmpi::drawing::Rect r{ 6.f, top + kMenuSeparatorHeight * 0.5f,
                                             w - 6.f, top + kMenuSeparatorHeight * 0.5f + 1.f };
                rt->fillRectangle(&r, rule);
            }
            continue;
        }

        if (int(i) == hovered && !item.grayed && hilite)
        {
            const gmpi::drawing::Rect r{ 3.f, top, w - 3.f, top + kMenuItemHeight };
            rt->fillRectangle(&r, hilite);
        }

        auto* fg = item.grayed ? grey : ink;
        if (!fg || !font)
            continue;

        if (item.ticked)
        {
            const char* tick = "\xe2\x9c\x93";
            const gmpi::drawing::Rect r{ 8.f, top + 3.f, 26.f, top + kMenuItemHeight };
            rt->drawTextU(tick, 3, font, &r, fg, 0);
        }

        const gmpi::drawing::Rect r{ float(kMenuTickGutter + kMenuLabelInset), top + 3.f,
                                     w - kMenuArrowGutter, top + kMenuItemHeight };
        rt->drawTextU(item.label.c_str(), uint32_t(item.label.size()), font, &r, fg, 0);

        if (!item.submenu.empty())
        {
            const char* arrow = "\xe2\x80\xba";
            const gmpi::drawing::Rect a{ w - 20.f, top + 3.f, w - 6.f, top + kMenuItemHeight };
            rt->drawTextU(arrow, 3, font, &a, fg, 0);
        }
    }

    if (ink)    ink->release();
    if (grey)   grey->release();
    if (hilite) hilite->release();
    if (rule)   rule->release();
}

} // namespace gmpi::popupmenu
