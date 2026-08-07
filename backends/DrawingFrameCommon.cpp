#include <functional>
#include "DrawingFrameCommon.h"

using namespace gmpi;

// Right-click context-menu fallback.
//
// Deliberately does NOT dispatch onPointerDown: callers press the client
// themselves and only reach here when it returned Unhandled - same contract as
// Win32 (DxDrawingFrameBase::doContextMenu) and Wayland. Doing it here too gave
// macOS two presses per right-click, and a client that branches on "was I
// already selected?" reads the second press as a different gesture than the
// first (SynthEdit turned it into a connector node insertion).
void DrawingFrameCommon::doContextMenu(gmpi::drawing::Point point, int32_t flags)
{
	// Handle right-click on background. (right-click on objects is handled by object itself).
	if ((flags & static_cast<int32_t>(gmpi::api::PointerFlags::SecondButton)) != 0 && inputClient)
	{
		gmpi::drawing::Rect rect{ point.x, point.y, point.x + 120, point.y + 20 };

		// create menu popup
		gmpi::shared_ptr<gmpi::api::IUnknown> unknown;
		createPopupMenu(&rect, unknown.put());
		contextMenu = unknown.as<gmpi::api::IPopupMenu>();

		// populate menu
		inputClient->populateContextMenu(point, contextMenu.get());

		// show menu
		contextMenu->showAsync();
	}
}
