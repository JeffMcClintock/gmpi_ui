#pragma once
#include <vector>
#include "./NativeUi.h"

class ContextMenuHelper
{
    gmpi::api::IContextItemSink* sink{};

public:
    std::function<void(int32_t selectedId)> currentCallback;

    ContextMenuHelper(gmpi::api::IContextItemSink* psink) : sink(psink) {}

    gmpi::ReturnCode addItem(const char* text, int32_t id, int32_t flags = 0)
    {
        gmpi::api::IUnknown* callback = {};
        if(currentCallback)
			callback = new gmpi::sdk::PopupMenuCallback(currentCallback);

        return sink->addItem(text, id, flags, callback);
    }

    gmpi::ReturnCode addSeparator()
    {
        return addItem("", 0, (int32_t)gmpi::api::PopupMenuFlags::Separator);
    }

    gmpi::ReturnCode beginSubMenu(const char* text)
    {
        return addItem(text, 0, (int32_t)gmpi::api::PopupMenuFlags::SubMenuBegin);
    }

    gmpi::ReturnCode endSubMenu()
    {
        return addItem("", 0, (int32_t)gmpi::api::PopupMenuFlags::SubMenuEnd);
    }

    gmpi::ReturnCode addItem(const char* text, int32_t id, std::function<void(int32_t selectedId)> pcallback, int32_t flags = 0)
    {
        gmpi::api::IUnknown* callback = {};
        if(pcallback)
            callback = new gmpi::sdk::PopupMenuCallback(pcallback);

        return sink->addItem(text, id, flags, callback);
    }

    gmpi::ReturnCode addItem(const char* text, std::function<void(int32_t selectedId)> pcallback, int32_t flags = 0)
    {
		return addItem(text, 0, pcallback, flags);
    }
};
