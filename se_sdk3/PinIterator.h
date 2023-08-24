// Copyright 2012 SynthEdit Limited.

/*
#include "PinIterator.h"

// Example. (see WaveRecorder2)

PinIterator it(this);
for( it.first() ; !it.isDone() ; it.next() )
{
	if( it.getDirection() == gmpi::MP_IN && it.getDatatype() == 5 )
	{
		AudioIns.push_back(std::unique_ptr<AudioInPin>(new AudioInPin()));
		initializePin(idx, *( AudioIns.back() ));
	}
}

*/

#ifndef MP_SDK_PIN_ITERATOR_INCLUDED
#define MP_SDK_PIN_ITERATOR_INCLUDED

#include "./mp_sdk_audio.h"

class IPinProperties
{
public:
	virtual int getUniqueId() = 0;
	virtual int getDirection() = 0;
	virtual int getDatatype() = 0;
};

// Helper class to make iterating the module's pins easier.
class PinIterator : public IPinProperties
{
public:
	PinIterator(MpPluginBase* module ) :
		done_(true)
	{
		module->getHost()->createPinIterator(pinIterator_.getAddressOf());
	}

	void first()
	{
		done_ = pinIterator_->first() != gmpi::MP_OK;
	}

	void next()
	{
		done_ = pinIterator_->next() != gmpi::MP_OK;
	}

	bool isDone()
	{
		return done_;
	}

	int size()
	{
		int32_t count =0;
		pinIterator_->getCount( count );
		return count;
	}

	PinIterator& operator++() // prefix version: ++c
	{
		next();
		return *this;
	}

	IPinProperties* operator*()
	{
		return this;
	}

	// IPinProperties
	// for legacy reasons, actually returns pin index.
	virtual int getUniqueId()
	{
		int32_t id;
		pinIterator_->getPinId(id);
		return id;
	}
	virtual int getDirection()
	{
		int32_t id;
		pinIterator_->getPinDirection(id);
		return id;
	}
	virtual int getDatatype()
	{
		int32_t id;
		pinIterator_->getPinDatatype(id);
		return id;
	}

private:
	gmpi_sdk::mp_shared_ptr<gmpi::IMpPinIterator> pinIterator_;
	bool done_;
};


#endif
