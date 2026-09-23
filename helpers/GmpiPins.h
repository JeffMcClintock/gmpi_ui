#pragma once

/*
#include "helpers/GmpiPins.h"

Pins shared by GUI modules (PluginEditor) and Controllers (ControllerBase).
*/

#include <cassert>
#include <functional>
#include <span>
#include <vector>
#include "Core/Common.h"
#include "Core/GmpiApiEditor.h"

namespace gmpi
{
namespace editor
{

class PinBase
{
public:
	int idx{};
	gmpi::api::IEditorHost* host{};
	std::function<void(PinBase*)> onUpdate;

	PinBase();
	virtual ~PinBase() {}
	virtual void setFromHost(int32_t voice, std::span<const uint8_t> data) = 0;
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
			host->setPin(idx, 0, dataSize(value), dataPtr(value));
		}
		return value;
	}

	void setFromHost(int32_t voice, std::span<const uint8_t> data) override
	{
		valueFromData(data, value);
		if(onUpdate)
			onUpdate(this);
	}
};

// anything that has pins. Pins register themselves with the object under construction.
class PinOwner
{
public:
	std::vector<PinBase*> pins;
	inline static thread_local PinOwner* constructingInstance{};

	PinOwner()
	{
		constructingInstance = this;
	}

	void init(int pinIndex, PinBase& pin)
	{
		assert(0 <= pinIndex); // pin index must be positive.
		assert(pins.size() <= pinIndex); // did you init the same pin twice?

		pin.idx = pinIndex;
		pins.resize(pinIndex + 1);
		pins[pinIndex] = &pin;
	}

	void init(PinBase& pin)
	{
		init(static_cast<int32_t>(pins.size()), pin); // Automatic indexing.
	}

	void setPinsHost(gmpi::api::IEditorHost* host)
	{
		for (auto& pin : pins)
			pin->host = host;
	}
};

inline PinBase::PinBase()
{
	// register with the plugin editor or controller.
	if (PinOwner::constructingInstance)
		PinOwner::constructingInstance->init(*this);
}

} // namespace editor
} // namespace gmpi
