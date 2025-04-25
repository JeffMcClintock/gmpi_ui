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
namespace editor2
{

class PinBase
{
public:
	int id{};
	bool dirty{true};
	gmpi::api::IEditorHost* host{};
	std::function<void(PinBase*)> onUpdate;

	PinBase();
	virtual ~PinBase() {}
	virtual void setFromHost(int32_t voice, int32_t size, const void* data) = 0;
	virtual void clearDirty()
	{
		dirty = false;
	}
};

template<typename T>
class Pin : public PinBase
{
	friend class PluginEditorBase;

	void setFromHost(int32_t voice, int32_t size, const void* data) override
	{
		dirty = true;
		valueFromData(size, data, value);
	}

public:
	T value{};

	const T& operator=(const T& pvalue)
	{
		if (pvalue != value)
		{
			value = pvalue;
			host->setPin(id, 0, dataSize(value), dataPtr(value));
		}
		return value;
	}
};

template<typename T>
class PolyPin : public PinBase
{
	friend class PluginEditorBase;

	void setFromHost(int32_t voice, int32_t size, const void* data) override
	{
		dirty = true;
		dirtyVoice[voice] = true;
		valueFromData(size, data, value[voice]);
	}
	
	void clearDirty() override
	{
		PinBase::clearDirty();
		std::fill(std::begin(dirtyVoice), std::end(dirtyVoice), false);
	}

public:
	T value[128]{};
	bool dirtyVoice[128]{};

	void set(int voice, const T& pvalue)
	{
		if (pvalue != value[voice])
		{
			value[voice] = pvalue;
			host->setPin(id, voice, dataSize(pvalue), dataPtr(pvalue));
		}
	}
};

class PluginEditorBase : public gmpi::api::IEditor, public gmpi::api::IEditor2_x
{
public:
	inline static thread_local PluginEditorBase* constructingEditor = nullptr;
	gmpi::shared_ptr<gmpi::api::IEditorHost> editorHost;
	gmpi::shared_ptr<gmpi::api::IEditorHost2_x> editorHost2;
	
	std::map<int, PinBase*> pins;

	PluginEditorBase()
	{
		constructingEditor = this;
	}
	virtual ~PluginEditorBase() {}

	void init(int id, PinBase& pin)
	{
		assert(pins.find(id) == pins.end()); // init two pins with same id?
		assert(pins.end() == std::find_if(pins.begin(), pins.end(), [&pin](const auto& pair) {
			return pair.second == &pin;
			}));

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
		editorHost2 = unknown.as<gmpi::api::IEditorHost2_x>();

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

// obsolete
ReturnCode notifyPin(int32_t pinId, int32_t voice) override
{
	return ReturnCode::NoSupport;
}

	// IEditor2_x
	ReturnCode process() override
	{
		for (auto& p : pins)
		{
			if (p.second->dirty)
			{
				if (p.second->onUpdate)
					p.second->onUpdate(p.second);

				p.second->clearDirty();
			}
		}
		return ReturnCode::Ok;
	}
};

class PluginEditorNoGui : public PluginEditorBase
{
public:
	ReturnCode queryInterface(const gmpi::api::Guid* iid, void** returnInterface) override
	{
		GMPI_QUERYINTERFACE(gmpi::api::IEditor);
		GMPI_QUERYINTERFACE(gmpi::api::IEditor2_x);
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
		GMPI_QUERYINTERFACE(gmpi::api::IEditor2_x);
		GMPI_QUERYINTERFACE(gmpi::api::IInputClient);
		GMPI_QUERYINTERFACE(gmpi::api::IDrawingClient);
		return ReturnCode::NoSupport;
	}
	GMPI_REFCOUNT;
};

PinBase::PinBase()
{
	// when contructing a pin, register it with the plugin editor.
	if (PluginEditorBase::constructingEditor)
		PluginEditorBase::constructingEditor->init(*this);
}


} // namespace gmpi
} // namespace editor
