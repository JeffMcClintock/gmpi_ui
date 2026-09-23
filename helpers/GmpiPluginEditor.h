#pragma once

#include <functional>
#include <map>
#include "Core/Common.h"
#include "Core/GmpiApiEditor.h"
#include "RefCountMacros.h"
#include "GmpiUiDrawing.h"
#include "helpers/NativeUi.h"
#include "helpers/GmpiPins.h"

namespace gmpi
{
namespace editor
{

class PluginEditorBase : public gmpi::api::IEditor, public PinOwner
{
public:
	gmpi::shared_ptr<gmpi::api::IEditorHost> editorHost;
	gmpi::shared_ptr<gmpi::api::IDialogHost> dialogHost;

	// One setHost override per plugin satisfies IEditor, IDrawingClient and IInputClient
	// simultaneously (all three declare identical-signature pure virtuals).
	ReturnCode setHost(gmpi::api::IUnknown* phost) override
	{
		gmpi::shared_ptr<gmpi::api::IUnknown> unknown;
		unknown = phost;
		editorHost = unknown.as<gmpi::api::IEditorHost>();
		dialogHost = unknown.as<gmpi::api::IDialogHost>();

		setPinsHost(editorHost.get());

		return ReturnCode::Ok;
	}

	ReturnCode initialize() override
	{
		return ReturnCode::Ok;
	}

	ReturnCode setPin(int32_t PinIndex, int32_t voice, int32_t size, const uint8_t* data) override
	{
		if (PinIndex < 0 || PinIndex >= pins.size())
			return ReturnCode::Fail;

		pins[PinIndex]->setFromHost(voice, {data, static_cast<size_t>(size)} );
		return ReturnCode::Ok;
	}

	ReturnCode notifyPin(int32_t PinIndex, int32_t voice) override
	{
        // todo: should be notifiing from here? not setPin?
		return ReturnCode::Ok;
	}
};

class PluginEditorNoGui : public PluginEditorBase
{
public:
	ReturnCode queryInterface(const gmpi::api::Guid* iid, void** returnInterface) override
	{
		GMPI_QUERYINTERFACE(gmpi::api::IEditor);
		return ReturnCode::NoSupport;
	}
	GMPI_REFCOUNT
};

class PluginEditor : public PluginEditorBase, public gmpi::api::IDrawingClient, public gmpi::api::IInputClient
{
protected:
	gmpi::drawing::Rect bounds;

public:
	gmpi::shared_ptr<gmpi::api::IInputHost> inputHost;
	gmpi::shared_ptr<gmpi::api::IDrawingHost> drawingHost;

	virtual ~PluginEditor(){}

	// Single setHost serves IEditor, IDrawingClient and IInputClient; called right after
	// constructor (and again with nullptr at shutdown to release host references).
	ReturnCode setHost(gmpi::api::IUnknown* phost) override
	{
		PluginEditorBase::setHost(phost);

		gmpi::shared_ptr<gmpi::api::IUnknown> unknown;
		unknown = phost; // asign assuming ownership is already managed from caller.

		inputHost = unknown.as<gmpi::api::IInputHost>();
		drawingHost = unknown.as<gmpi::api::IDrawingHost>();

		return ReturnCode::Ok;
	}

	// "Given at most this much room, how much do you want?"
	//
	// There are exactly TWO correct shapes of answer, and which one you give is
	// how you declare whether your editor resizes:
	//
	//   RESIZABLE   echo availableSize back — "I'll fill whatever you give me".
	//               That is what this default does.
	//   FIXED SIZE  return the same constant every time, IGNORING availableSize.
	//               Not "clamped to availableSize" — ignoring it.
	//
	// This is load-bearing, not stylistic: VST3EditorBase::canResize() measures
	// you twice, against 0x0 and 10000x10000, and calls you resizable only if
	// the two answers DIFFER. A constant is therefore the only way to say "fixed".
	//
	// Two plausible-looking answers are wrong, and each one hides in a different
	// host, so testing in only one will not find them:
	//
	//   max(preferred, available)  The VST3 wrapper probes with an unbounded
	//       availableSize to discover your preferred size, so this asks for a
	//       99999 x 99999 window. DXGI refuses to create a swap chain that big
	//       and the frame dies in tempSharedD2DBase::CreateSwapPanel — a stack
	//       full of graphics calls that never mentions layout or the number.
	//   min(preferred, available)  Collapses to whatever a host happens to offer
	//       first. SynthEdit hands out a small default rect, so a fixed-size
	//       editor shrinks to a ~100px box while VST3 looks fine.
	//
	// Both failures are at the swap chain, in opposite directions: too big is
	// refused, and zero is refused. Hence the guard below.
	ReturnCode measure(const gmpi::drawing::Size* availableSize, gmpi::drawing::Size* returnDesiredSize) override
	{
		*returnDesiredSize = *availableSize;

		// a size of zero will crash the swapchain creation in Direct2D.
		if(returnDesiredSize->width <= 0.0f || returnDesiredSize->height <= 0.0f)
		{
			returnDesiredSize->width = 100.0f;
			returnDesiredSize->height = 100.0f;
		}

		return ReturnCode::Ok;
	}

	ReturnCode arrange(const gmpi::drawing::Rect* finalRect) override
	{
		bounds = *finalRect;
		return ReturnCode::Ok;
	}

	// IDrawingClient (setHost above already covers the connection-establishment role
	// that the legacy IDrawingClient::open(host) used to play).
	ReturnCode render(gmpi::drawing::api::IDeviceContext* drawingContext) override
	{
		return ReturnCode::Ok;
	}

	ReturnCode getClipArea(drawing::Rect* returnRect) override
	{
		*returnRect = bounds;
		return ReturnCode::Ok;
	}

	// IInputClient
	ReturnCode hitTest(gmpi::drawing::Point point, int32_t flags) override
	{
		return ReturnCode::Ok; // Ok = hit
	}
	gmpi::ReturnCode onPointerDown(gmpi::drawing::Point point, int32_t flags) override
	{
		return ReturnCode::Unhandled;
	}
	gmpi::ReturnCode onPointerMove(gmpi::drawing::Point point, int32_t flags) override
	{
		return ReturnCode::Unhandled;
	}
	gmpi::ReturnCode onPointerUp(gmpi::drawing::Point point, int32_t flags) override
	{
		return ReturnCode::Unhandled;
	}
	gmpi::ReturnCode onKeyPress(wchar_t c) override
	{
		return ReturnCode::Unhandled;
	}
	gmpi::ReturnCode onMouseWheel(gmpi::drawing::Point point, int32_t flags, int32_t delta) override
	{
		return ReturnCode::Ok;
	}
	gmpi::ReturnCode setHover(bool isMouseOverMe) override
	{
		return ReturnCode::Ok;
	}
	// right-click menu
	gmpi::ReturnCode populateContextMenu(gmpi::drawing::Point point, gmpi::api::IUnknown* contextMenuItemsSink) override
	{
		return ReturnCode::Unhandled;
	}
	gmpi::ReturnCode getToolTip(gmpi::drawing::Point point, gmpi::api::IString* returnString) override
	{
		return ReturnCode::Unhandled;
	}

	ReturnCode queryInterface(const gmpi::api::Guid* iid, void** returnInterface) override
	{
		GMPI_QUERYINTERFACE(gmpi::api::IEditor);
		GMPI_QUERYINTERFACE(gmpi::api::IInputClient);
		GMPI_QUERYINTERFACE(gmpi::api::IDrawingClient);
		return ReturnCode::NoSupport;
	}
	GMPI_REFCOUNT
};

} // namespace gmpi
} // namespace editor
