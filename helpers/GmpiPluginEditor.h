#pragma once

#include <functional>
#include "GmpiApiEditor.h"
#include "RefCountMacros.h"
#include "Common.h"
#include "Drawing.h"
#include "helpers/GraphicsRedrawClient.h"

namespace gmpi
{

struct PinBase
{
	int id{};
	gmpi::api::IEditorHost* host{};
	std::function<void(PinBase*)> onUpdate;

	virtual void setFromHost(int32_t voice, int32_t size, const void* data) = 0;
};

template<typename T>
class Pin : public PinBase
{
public:
	T value{};

	const T& operator=(const T& pvalue)
	{
		if (pvalue != value)
		{
			value = pvalue;
			host->setPin(id, 0, variableRawSize(value), variableRawData(value));
		}
		return value;
	}

	void setFromHost(int32_t voice, int32_t size, const void* data)
	{
		VariableFromRaw(size, data, value);
		if(onUpdate)
			onUpdate(this);
	}
};

class PluginEditor : public gmpi::api::IEditor, public gmpi::api::IDrawingClient, public gmpi::api::IInputClient
{
protected:
	gmpi::drawing::Rect bounds;

	gmpi::shared_ptr<gmpi::api::IInputHost> inputHost;
	gmpi::shared_ptr<gmpi::api::IEditorHost> editorHost;
	gmpi::shared_ptr<gmpi::api::IDrawingHost> drawingHost;

	std::unordered_map<int, PinBase*> pins;

public:
	PluginEditor(){}

	void init(int id, PinBase& pin)
	{
		assert(pins.find(id) == pins.end()); // init two pins with same id?
		pin.id = id;
		pins[id] = &pin;
	}

	void init(PinBase& pin)
	{
		const int nextId = pins.empty() ? 0 : pins.end()->first + 1;
		init(nextId, pin);
	}

	// IEditor
	ReturnCode setHost(gmpi::api::IUnknown* phost) override
	{
        // MOVED TO OPEN. TODO RESOLVE Duplication without messing up gmpi drawing
        /*
         
         
		gmpi::shared_ptr<gmpi::api::IUnknown> unknown(phost);

		phost->queryInterface(&gmpi::api::IDrawingHost::guid, drawingHost.asIMpUnknownPtr());
		inputHost = unknown.As<gmpi::api::IInputHost>();
		editorHost = unknown.As<gmpi::api::IEditorHost>();
*/

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
        gmpi::shared_ptr<gmpi::api::IUnknown> unknown(host);

        host->queryInterface(&gmpi::api::IDrawingHost::guid, drawingHost.asIMpUnknownPtr());
        inputHost = unknown.As<gmpi::api::IInputHost>();
        editorHost = unknown.As<gmpi::api::IEditorHost>();
        
		for (auto& p : pins)
		{
			p.second->host = editorHost.get();
		}

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

	gmpi::ReturnCode onMouseWheel(gmpi::drawing::Point point, int32_t flags, int32_t delta) override
	{
		return ReturnCode::Ok;
	}
	gmpi::ReturnCode setHover(bool isMouseOverMe) override
	{
		return ReturnCode::Ok;
	}

	// IInputClient
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

	// IUnknown
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
