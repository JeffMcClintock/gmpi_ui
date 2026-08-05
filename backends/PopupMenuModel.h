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

} // namespace gmpi::popupmenu
