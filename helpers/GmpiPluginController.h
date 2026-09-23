#pragma once

/*
#include "helpers/GmpiPluginController.h"

Base class for Controllers, handles the common chores. c.f. PluginEditor.
*/

#include "RefCountMacros.h"
#include "helpers/GmpiPins.h"

namespace gmpi
{
namespace controller
{

using gmpi::editor::PinBase;
using gmpi::editor::Pin;

// Pins are bound to the module's own parameters, in order. i.e. the first pin is parameter 0.
// Setting a pin sets the parameter, and a parameter change updates the pin (and calls its onUpdate).
class ControllerBase : public gmpi::api::IController, public gmpi::editor::PinOwner
{
	// routes pin writes to the module's parameters.
	class PinHost final : public gmpi::api::IEditorHost
	{
		ControllerBase& controller;

	public:
		PinHost(ControllerBase& pcontroller) : controller(pcontroller) {}

		ReturnCode setPin(int32_t pinIndex, int32_t voice, int32_t size, const uint8_t* data) override
		{
			return controller.host->setParameter(pinIndex, gmpi::Field::Value, voice, size, data);
		}
		int32_t getHandle() override
		{
			return controller.handle;
		}

		GMPI_QUERYINTERFACE_METHOD(gmpi::api::IEditorHost);
		GMPI_REFCOUNT_NO_DELETE;
	};

	PinHost pinHost{ *this };
	std::vector<int32_t> pinParameterHandles; // the parameter handle of each pin.

protected:
	int32_t handle{};
	gmpi::shared_ptr<gmpi::api::IControllerHost> host;

	// the handle of the parameter a pin is bound to. valid after initialize().
	int32_t getParameterHandle(const PinBase& pin) const
	{
		return pin.idx < static_cast<int>(pinParameterHandles.size()) ? pinParameterHandles[pin.idx] : -1;
	}

	// changes to parameters not bound to a pin. e.g. other modules' parameters, when subscribed.
	virtual void onParameter(int32_t parameterHandle, gmpi::Field fieldId, int32_t voice, std::span<const uint8_t> data) {}

public:
	virtual ~ControllerBase() = default;

	// IController
	ReturnCode initialize(gmpi::api::IUnknown* phost, int32_t phandle) override
	{
		handle = phandle;
		phost->queryInterface(&gmpi::api::IControllerHost::guid, host.put_void());
		setPinsHost(&pinHost);

		// the host identifies parameters by handle, look up each pin's.
		gmpi::shared_ptr<gmpi::api::IParameterSetter> parameterSetter;
		phost->queryInterface(&gmpi::api::IParameterSetter::guid, parameterSetter.put_void());

		pinParameterHandles.assign(pins.size(), -1);
		if(parameterSetter)
		{
			for(int32_t i = 0; i < static_cast<int32_t>(pins.size()); ++i)
				parameterSetter->getParameterHandle(i, pinParameterHandles[i]);
		}

		return ReturnCode::Ok;
	}

	ReturnCode syncState() override
	{
		return ReturnCode::Ok;
	}

	// IParameterObserver
	ReturnCode setParameter(int32_t parameterHandle, gmpi::Field fieldId, int32_t voice, int32_t size, const uint8_t* data) override
	{
		const std::span<const uint8_t> raw{ data, static_cast<size_t>(size) };

		for(size_t i = 0; i < pinParameterHandles.size(); ++i)
		{
			if(pinParameterHandles[i] == parameterHandle && pins[i])
			{
				if(gmpi::Field::Value == fieldId)
					pins[i]->setFromHost(voice, raw);

				return ReturnCode::Ok;
			}
		}

		onParameter(parameterHandle, fieldId, voice, raw);
		return ReturnCode::Ok;
	}

	ReturnCode queryInterface(const gmpi::api::Guid* iid, void** returnInterface) override
	{
		*returnInterface = {};
		GMPI_QUERYINTERFACE(gmpi::api::IController);
		GMPI_QUERYINTERFACE(gmpi::api::IParameterObserver);
		return ReturnCode::NoSupport;
	}
	GMPI_REFCOUNT
};

} // namespace controller
} // namespace gmpi
