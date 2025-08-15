#include <functional>
#include "DrawingFrameCommon.h"

using namespace gmpi;

// Accepts context-menu items via IMpContextItemSink, and populates a popup menu with them.
// Also suppresses redundant separators
class ContextItemsSinkAdaptor : public gmpi::api::IContextItemSink
{
	gmpi::shared_ptr<gmpi::api::IPopupMenu> contextMenu;
	bool pendingSeperator = false;

public:
	ContextItemsSinkAdaptor(gmpi::shared_ptr<gmpi::api::IUnknown> unknown)
	{
		contextMenu = unknown.as<gmpi::api::IPopupMenu>();
	}

	ReturnCode addItem(const char* text, int32_t id, int32_t flags = 0) override
	{
		// suppress redundant or repeated separators.
		if (0 != (flags & (int32_t)gmpi::api::PopupMenuFlags::Separator))
		{
			pendingSeperator = true;
		}
		else
		{
			if (0 != (flags & (int32_t)gmpi::api::PopupMenuFlags::SubMenuEnd))
			{
				pendingSeperator = false;
			}

			if (pendingSeperator)
			{
				contextMenu->addItem("", 0, (int32_t)gmpi::api::PopupMenuFlags::Separator);
				pendingSeperator = false;
			}

			contextMenu->addItem(text, id, flags);
		}

		return gmpi::ReturnCode::Ok;
	}

	GMPI_QUERYINTERFACE_METHOD(gmpi::api::IContextItemSink);
	GMPI_REFCOUNT_NO_DELETE;
};

class PopupMenuCallback : public gmpi::api::IPopupMenuCallback
{
	std::function<void(gmpi::ReturnCode, int32_t)> callback;
public:
	PopupMenuCallback(std::function<void(gmpi::ReturnCode, int32_t)> callback) : callback(callback) {}

	void onComplete(gmpi::ReturnCode result, int32_t selectedID) override
	{
		callback(result, selectedID);
	}

	GMPI_QUERYINTERFACE_METHOD(gmpi::api::IPopupMenuCallback);
	GMPI_REFCOUNT
};

void DrawingFrameCommon::doContextMenu(gmpi::drawing::Point point, int32_t flags)
{
	auto r = inputClient->onPointerDown(point, flags);

	// Handle right-click on background. (right-click on objects is handled by object itself).
	if (r == gmpi::ReturnCode::Unhandled && (flags & gmpi::api::GG_POINTER_FLAG_SECONDBUTTON) != 0 && inputClient)
	{
		gmpi::drawing::Rect rect{ point.x, point.y, point.x + 120, point.y + 20 };

		gmpi::shared_ptr<gmpi::api::IUnknown> unknown;
		createPopupMenu(&rect, unknown.put());

		auto lcontextMenu = unknown.as<gmpi::api::IPopupMenu>();

		ContextItemsSinkAdaptor sink(unknown);

		r = inputClient->populateContextMenu(point, &sink);

		lcontextMenu->showAsync(/*&rect,*/
			new PopupMenuCallback(
				[this](gmpi::ReturnCode res, int32_t commandId) -> void
				{
					if (res == gmpi::ReturnCode::Ok)
					{
						inputClient->onContextMenu(commandId);
					}
				}
			)
		);
	}
}
