#pragma once

#include <functional>
#include <map>
#include "GmpiApiEditor.h"
#include "RefCountMacros.h"
#include "Common.h"
#include "../Drawing.h" // .. to force use of gmpi-ui not SE SDK
#include "helpers/NativeUi.h"

namespace gmpi
{
namespace editor
{

class PinBase
{
public:
	int id{};
	gmpi::api::IEditorHost* host{};
	std::function<void(PinBase*)> onUpdate;

	PinBase()
	{
		//_RPT0(_CRT_WARN, "PinBase() constructor\n");
	}
	virtual ~PinBase() {}
	virtual void setFromHost(int32_t voice, int32_t size, const void* data) = 0;
};

template<typename T>
class Pin : public PinBase
{
public:
	T value{};

	Pin()
	{
		//_RPT0(_CRT_WARN, "Pin() constructor\n");
	}
	const T& operator=(const T& pvalue)
	{
		if (pvalue != value)
		{
			value = pvalue;
			host->setPin(id, 0, dataSize(value), dataPtr(value));
		}
		return value;
	}

	void setFromHost(int32_t voice, int32_t size, const void* data) override
	{
		valueFromData(size, data, value);
		if(onUpdate)
			onUpdate(this);
	}
};

class PluginEditorBase : public gmpi::api::IEditor
{
public:
	gmpi::shared_ptr<gmpi::api::IEditorHost> editorHost;
	std::map<int, PinBase*> pins;

	void init(int id, PinBase& pin)
	{
		assert(pins.find(id) == pins.end()); // init two pins with same id?
		pin.id = id;
		pins[id] = &pin;
	}

	void init(PinBase& pin)
	{
		const int nextId = pins.empty() ? 0 : pins.rbegin()->first + 1;
		init(nextId, pin);
	}

	// Confused with OPEN. TODO RESOLVE Duplication without messing up gmpi drawing
	ReturnCode setHost(gmpi::api::IUnknown* phost) override
	{
		gmpi::shared_ptr<gmpi::api::IUnknown> unknown(phost);
		editorHost = unknown.as<gmpi::api::IEditorHost>();

		for (auto& p : pins)
		{
			p.second->host = editorHost.get();
		}
		return ReturnCode::Ok;
	}

	ReturnCode initialize() override
	{
		return ReturnCode::Ok;
	}

	ReturnCode setPin(int32_t pinId, int32_t voice, int32_t size, const void* data) override
	{
		if (auto it = pins.find(pinId); it != pins.end())
		{
			it->second->setFromHost(voice, size, data);
		}
		return ReturnCode::Ok;
	}

	ReturnCode notifyPin(int32_t pinId, int32_t voice) override
	{
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
	GMPI_REFCOUNT;
};

class PluginEditor : public PluginEditorBase, public gmpi::api::IDrawingClient, public gmpi::api::IInputClient
{
protected:
	gmpi::drawing::Rect bounds;

public:
	gmpi::shared_ptr<gmpi::api::IInputHost> inputHost;
	gmpi::shared_ptr<gmpi::api::IDialogHost> dialogHost;
	gmpi::shared_ptr<gmpi::api::IDrawingHost> drawingHost;


	virtual ~PluginEditor(){}

	// IEditor
	// called right after constructor
    // Confused with OPEN. TODO RESOLVE Duplication without messing up gmpi drawing
	ReturnCode setHost(gmpi::api::IUnknown* phost) override
	{
		PluginEditorBase::setHost(phost);

		gmpi::shared_ptr<gmpi::api::IUnknown> unknown(phost);

		// TODO:: None of these are provided by SynthEdit as a host
//		phost->queryInterface(&gmpi::api::IDrawingHost::guid, drawingHost.put_void());
		inputHost = unknown.as<gmpi::api::IInputHost>();
		drawingHost = unknown.as<gmpi::api::IDrawingHost>();
		dialogHost = unknown.as<gmpi::api::IDialogHost>();

		return ReturnCode::Ok;
	}

	ReturnCode measure(const gmpi::drawing::Size* availableSize, gmpi::drawing::Size* returnDesiredSize) override
	{
		*returnDesiredSize = *availableSize;
		return ReturnCode::Ok;
	}

	ReturnCode arrange(const gmpi::drawing::Rect* finalRect) override
	{
		bounds = *finalRect;
		return ReturnCode::Ok;
	}

	// IDrawingClient
	ReturnCode open(gmpi::api::IUnknown* host) override
	{
		// TOO LAte, measure and arrange need the drawing factory to size text correctly
  //      gmpi::shared_ptr<gmpi::api::IUnknown> unknown(host);

  //      host->queryInterface(&gmpi::api::IDrawingHost::guid, drawingHost.put_void());
  //      inputHost = unknown.as<gmpi::api::IInputHost>();
  //      editorHost = unknown.as<gmpi::api::IEditorHost>();
  //      
		//for (auto& p : pins)
		//{
		//	p.second->host = editorHost.get();
		//}

		return ReturnCode::Ok;
	}

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
	gmpi::ReturnCode OnKeyPress(wchar_t c) override
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
	gmpi::ReturnCode onContextMenu(int32_t idx) override
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
	GMPI_REFCOUNT;
};

} // namespace gmpi
} // namespace editor
