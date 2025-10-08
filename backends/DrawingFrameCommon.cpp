#include <functional>
#include "DrawingFrameCommon.h"

using namespace gmpi;

void DrawingFrameCommon::doContextMenu(gmpi::drawing::Point point, int32_t flags)
{
	auto r = inputClient->onPointerDown(point, flags);

	// Handle right-click on background. (right-click on objects is handled by object itself).
	if (r == gmpi::ReturnCode::Unhandled && (flags & gmpi::api::GG_POINTER_FLAG_SECONDBUTTON) != 0 && inputClient)
	{
		gmpi::drawing::Rect rect{ point.x, point.y, point.x + 120, point.y + 20 };

		gmpi::shared_ptr<gmpi::api::IUnknown> unknown;
		createPopupMenu(&rect, unknown.put());

		auto contextMenu = unknown.as<gmpi::api::IPopupMenu>();

		gmpi::sdk::ContextItemsSinkAdaptor sink(contextMenu.get());

		r = inputClient->populateContextMenu(point, &sink);

		contextMenu->showAsync(
			new gmpi::sdk::PopupMenuCallback(
				[this](gmpi::ReturnCode ret, int32_t commandId) -> gmpi::ReturnCode
				{
					if (gmpi::ReturnCode::Ok == ret)
					{
						ret = inputClient->onContextMenu(commandId);
					}
					return ret;
				}
			)
		);
	}
}
