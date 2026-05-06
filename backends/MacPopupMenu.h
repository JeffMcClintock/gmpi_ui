#ifndef GMPI_MAC_POPUPMENU_H
#define GMPI_MAC_POPUPMENU_H

// Single-header gmpi::api::IPopupMenu implementation for macOS.
// Depends on the EventHelperClient / GMPI_EVENT_HELPER_CLASSNAME_03 declarations
// from MacTextEdit.h. The @implementation for those Obj-C classes must be
// emitted in exactly one .mm per binary by defining GMPI_MAC_TEXTEDIT_IMPLEMENTATION
// before including MacTextEdit.h.

#import <Cocoa/Cocoa.h>
#include <algorithm>
#include <string>
#include <vector>
#include "GmpiSdkCommon.h"
#include "helpers/NativeUi.h"
#include "MacTextEdit.h"

class GMPI_MAC_PopupMenu : public gmpi::api::IPopupMenu, public EventHelperClient
{
    struct MenuCallback
    {
        int32_t localId{};
        gmpi::shared_ptr<gmpi::api::IUnknown> callback;
    };

    NSView* view;
    NSPopUpButton* button = nil;
    GMPI_EVENT_HELPER_CLASSNAME_03* eventHelper = nil;
    gmpi::drawing::Rect rect;
    std::vector<MenuCallback> callbacks;
    std::vector<NSMenu*> menuStack;

public:
    GMPI_MAC_PopupMenu(NSView* pview, gmpi::drawing::Rect prect)
        : view(pview)
        , rect(prect)
    {
        eventHelper = [GMPI_EVENT_HELPER_CLASSNAME_03 alloc];
        [eventHelper initWithClient:this];

        button = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(10, 1000, 30, 30)];
        menuStack.push_back([button menu]);
    }

    ~GMPI_MAC_PopupMenu()
    {
        if (button)
        {
            [button removeFromSuperview];
            button = nil;
        }
    }

    void CallbackFromCocoa(NSObject* sender) override
    {
        const int index = static_cast<int>([((NSMenuItem*)sender) tag]) - 1;
        [button removeFromSuperview];

        if (index < 0 || index >= static_cast<int>(callbacks.size()))
            return;

        if (auto cb = callbacks[index].callback.as<gmpi::api::IPopupMenuCallback>(); cb)
            cb->onComplete(gmpi::ReturnCode::Ok, callbacks[index].localId);
    }

    gmpi::ReturnCode addItem(const char* text, int32_t id, int32_t flags, gmpi::api::IUnknown* itemCallback) override
    {
        if ((flags & static_cast<int32_t>(gmpi::api::PopupMenuFlags::Separator)) != 0)
        {
            [menuStack.back() addItem:[NSMenuItem separatorItem]];
            return gmpi::ReturnCode::Ok;
        }

        std::string stripped(text);
        stripped.erase(std::remove(stripped.begin(), stripped.end(), '&'), stripped.end());
        NSString* nsstr = [NSString stringWithCString:stripped.c_str() encoding:NSUTF8StringEncoding];

        const bool isSubMenuStart = (flags & static_cast<int32_t>(gmpi::api::PopupMenuFlags::SubMenuBegin)) != 0;
        const bool isSubMenuEnd = (flags & static_cast<int32_t>(gmpi::api::PopupMenuFlags::SubMenuEnd)) != 0;

        if (isSubMenuStart)
        {
            auto menuItem = [menuStack.back() addItemWithTitle:nsstr action:nil keyEquivalent:@""];
            NSMenu* subMenu = [[NSMenu alloc] init];
            [menuItem setSubmenu:subMenu];
            menuStack.push_back(subMenu);
        }
        else if (isSubMenuEnd)
        {
            menuStack.pop_back();
        }
        else
        {
            // operator= AddRefs; the explicit shared_ptr(T*) ctor does not (it assumes refcount=1 from `new`).
            gmpi::shared_ptr<gmpi::api::IUnknown> cb;
            cb = itemCallback;
            callbacks.push_back({id, cb});

            NSMenuItem* menuItem;
            if ((flags & static_cast<int32_t>(gmpi::api::PopupMenuFlags::Grayed)) != 0)
                menuItem = [menuStack.back() addItemWithTitle:nsstr action:nil keyEquivalent:@""];
            else
                menuItem = [menuStack.back() addItemWithTitle:nsstr action:@selector(menuItemSelected:) keyEquivalent:@""];

            [menuItem setTarget:eventHelper];
            [menuItem setTag:callbacks.size()]; // 1-based tags

            if ((flags & static_cast<int32_t>(gmpi::api::PopupMenuFlags::Ticked)) != 0)
                [menuItem setState:NSControlStateValueOn];
        }

        return gmpi::ReturnCode::Ok;
    }

    gmpi::ReturnCode showAsync() override
    {
        [[button cell] setAltersStateOfSelectedItem:NO];
        [[button cell] attachPopUpWithFrame:NSMakeRect(0, 0, 1, 1) inView:view];
        [[button cell] performClickWithFrame:gmpiRectToViewRect(view.bounds, &rect) inView:view];
        return gmpi::ReturnCode::Ok;
    }

    gmpi::ReturnCode setAlignment(int32_t alignment) override
    {
        return gmpi::ReturnCode::Ok;
    }

    gmpi::ReturnCode queryInterface(const gmpi::api::Guid* iid, void** returnInterface) override
    {
        *returnInterface = {};
        GMPI_QUERYINTERFACE(gmpi::api::IPopupMenu);
        GMPI_QUERYINTERFACE(gmpi::api::IContextItemSink);
        return gmpi::ReturnCode::NoSupport;
    }
    GMPI_REFCOUNT;
};

#endif // GMPI_MAC_POPUPMENU_H
