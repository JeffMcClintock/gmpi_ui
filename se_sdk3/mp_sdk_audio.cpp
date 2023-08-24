// Copyright 2006-2020 SynthEdit Ltd
// MpPluginBase - implements the IMpPlugin2 interface.
#include "mp_sdk_audio.h"
#include <assert.h>
#include "MpString.h"
#include "../shared/unicode_conversion.h"

using namespace gmpi;

int32_t MpPluginBase::queryInterface( const gmpi::MpGuid& iid, void** returnInterface )
{
	*returnInterface = 0;

	if( iid == MP_IID_PLUGIN2 )
	{
		*returnInterface = static_cast<IMpPlugin2*>(this);
		addRef();
		return gmpi::MP_OK;
	}

	if( iid == MP_IID_PLUGIN || iid == MP_IID_UNKNOWN )
	{
		*returnInterface = static_cast<IMpPlugin*>(this);
		addRef();
		return gmpi::MP_OK;
	}

	if( iid == MP_IID_LEGACY_INITIALIZATION )
	{
		*returnInterface = static_cast<gmpi::IMpLegacyInitialization*>(this);
		addRef();
		return gmpi::MP_OK;
	}

	return MP_NOSUPPORT;
}

int32_t MpPluginBase::open()
{
	int32_t r = gmpi::MP_OK;

#if defined(_DEBUG)
	debugIsOpen_ = true;
#endif

	return r;
}

void MpBase::process( int32_t count, const gmpi::MpEvent* events )
{
	//_RPT1(_CRT_WARN, "MpPluginBase::process( count=%3d )\n", count );
	assert(count > 0);

	#if defined(_DEBUG)
		blockPosExact_ = false;
	#endif

	blockPos_ = 0; // Solve bug where blockPos_ remained at random previous value, and was not corrected until an event arives, leaving getBlockPosition() invalid within curSubProcess_().
	int bufferOffset_ = 0;
	int remain = count;
	const MpEvent* next_event = events;

	for(;;)
	{
		if( next_event == 0 ) // fast version, when no events on list.
		{
			(this->*( curSubProcess_ ))( bufferOffset_, remain );
			break;
		}

		assert( next_event->timeDelta < count ); // Event will happen in this block

		int delta_time = next_event->timeDelta - bufferOffset_;

		if( delta_time > 0 ) // then process intermediate samples
		{
			eventsComplete_ = false;

			(this->*( curSubProcess_ ))( bufferOffset_, delta_time );
			remain -= delta_time;

			eventsComplete_ = true;

			assert( remain != 0 ); // BELOW NEEDED?, seems non sense. If we are here, there is a event to process. Don't want to exit!
			if( remain == 0 ) // done
			{
				return;
			}

			bufferOffset_ += delta_time;
		}

		int cur_timeStamp = next_event->timeDelta;

		blockPos_ = cur_timeStamp;
		#if defined(_DEBUG)
			blockPosExact_ = true;
			assert(blockPos_ == bufferOffset_); // seems local variable is duplicating  member variable? Remove it.
		#endif
		// PRE-PROCESS EVENT
		bool pins_set_flag = false;
		const MpEvent* e = next_event;
		do
		{
			preProcessEvent( e ); // updates all pins_ values
			pins_set_flag = pins_set_flag || e->eventType == EVENT_PIN_SET || e->eventType == EVENT_PIN_STREAMING_START || e->eventType == EVENT_PIN_STREAMING_STOP;
			e = e->next;
		}
		while( e != 0 && e->timeDelta == cur_timeStamp );

		// PROCESS EVENT
		e = next_event;
		do
		{
			/* Problem with MIDI: MIDI can arrive before pins notified on same timestamp. TODO consider moving MIDI notification to post-process-event() */
			processEvent( e ); // notify all pins_ values.
			e = e->next;
		}
		while( e != 0 && e->timeDelta == cur_timeStamp );

		if( pins_set_flag )
		{
			onSetPins();
		}

		// POST-PROCESS EVENT
		do
		{
			postProcessEvent( next_event );
			next_event = next_event->next;
		}
		while( next_event != 0 && next_event->timeDelta == cur_timeStamp );

		#if defined(_DEBUG)
			blockPosExact_ = false;
		#endif
	}
}

void MpBase2::process( int32_t count, const MpEvent* events )
{
	//_RPT1(_CRT_WARN, "MpPluginBase::process( count=%3d )\n", count );
	assert(count > 0);

	#if defined(_DEBUG)
		blockPosExact_ = false;
	#endif

	blockPos_ = 0;
	int remain = count;
	const MpEvent* next_event = events;

	for(;;)
	{
		if( next_event == 0 ) // fast version, when no events on list.
		{
			(this->*( curSubProcess_ ))( remain );
			break;
		}

		assert( next_event->timeDelta < count ); // Event will happen in this block

		int delta_time = next_event->timeDelta - blockPos_;

		if( delta_time > 0 ) // then process intermediate samples
		{
			eventsComplete_ = false;

			(this->*( curSubProcess_ ))( delta_time );
			remain -= delta_time;

			eventsComplete_ = true;

			assert( remain != 0 ); // BELOW NEEDED?, seems non sense. If we are here, there is a event to process. Don't want to exit!
			if( remain == 0 ) // done
			{
				return;
			}

			blockPos_ += delta_time;
		}

		//int cur_timeStamp = next_event->timeDelta;
		#if defined(_DEBUG)
			blockPosExact_ = true;
		#endif
		assert( blockPos_ == next_event->timeDelta );

		// PRE-PROCESS EVENT
		bool pins_set_flag = false;
		const MpEvent* e = next_event;
		do
		{
			preProcessEvent( e ); // updates all pins_ values
			pins_set_flag = pins_set_flag || e->eventType == EVENT_PIN_SET || e->eventType == EVENT_PIN_STREAMING_START || e->eventType == EVENT_PIN_STREAMING_STOP;
			e = e->next;
		}
		while( e != 0 && e->timeDelta == blockPos_ ); // cur_timeStamp );

		// PROCESS EVENT
		e = next_event;
		do
		{
			processEvent( e ); // notify all pins_ values
			e = e->next;
		}
		while( e != 0 && e->timeDelta == blockPos_ ); //cur_timeStamp );

		if( pins_set_flag )
		{
			onSetPins();
		}

		// POST-PROCESS EVENT
		do
		{
			postProcessEvent( next_event );
			next_event = next_event->next;
		}
		while( next_event != 0 && next_event->timeDelta == blockPos_ ); // cur_timeStamp );

		#if defined(_DEBUG)
			blockPosExact_ = false;
		#endif
	}
}

void MpPluginBase::processEvent( const MpEvent* e )
{
	switch(e->eventType)
	{
		// pin events redirect to pin
	case EVENT_PIN_SET:
	case EVENT_PIN_STREAMING_START:
	case EVENT_PIN_STREAMING_STOP:
	case EVENT_MIDI:
		{
			Pins_t::iterator it = pins_.find(e->parm1);
			if(it != pins_.end())
			{
				(*it).second->processEvent(e);
			}
		}
		break;
	case EVENT_GRAPH_START:
		onGraphStart();
		assert( debugGraphStartCalled_ && "Please call the base class's MpPluginBase::onGraphStart(); from your overloaded member." );
		break;
	default:
		// MSCPP only _RPT1(_CRT_WARN, "MpPluginBase::processEvent - unhandled event type %d\n", e->eventType);

		break;
	};
}

void MpPluginBase::preProcessEvent( const MpEvent* e )
{
	switch( e->eventType )
	{

	case EVENT_PIN_STREAMING_START:
	case EVENT_PIN_STREAMING_STOP:
	case EVENT_PIN_SET:
	case EVENT_MIDI:
		{
			// pin events redirect to pin
			Pins_t::iterator it = pins_.find( e->parm1 );
			if( it != pins_.end() )
			{
				(*it).second->preProcessEvent(e);
			}
		}

		//
		break;
	case EVENT_GRAPH_START:
	default:
		// MSCPP only _RPT1(_CRT_WARN, "MpPluginBase::processEvent - unhandled event type %d\n", e->eventType);
		break;
	};
}

void MpPluginBase::postProcessEvent( const MpEvent* e )
{
	switch( e->eventType )
	{
	// pin events redirect to pin
	case EVENT_PIN_SET:
	case EVENT_PIN_STREAMING_START:
	case EVENT_PIN_STREAMING_STOP:
	case EVENT_MIDI:
		{
			Pins_t::iterator it = pins_.find( e->parm1 );
			if( it != pins_.end() )
			{
				(*it).second->postProcessEvent(e);
			}
		}
		break;
	case EVENT_GRAPH_START:
	default:
		// MSCPP only _RPT1(_CRT_WARN, "MpPluginBase::processEvent - unhandled event type %d\n", e->eventType);
		break;
	};
}

/* don't work if last pin has different handler func
// provides logic for calling onSetPins() (at most) once per timestamp
// even when several setpin events occur at once.
void MpPluginBase::SetPinsHelper(MpEvent* e)
{
	int cur_timeStamp = block Position();

	if(SetPinsHelperLast == 0) // then this is first stat change this timestamp
	{
		// find last event
		MpEvent* e2 = e;
		do
		{
			if(e2->eventType == EVENT_PIN_SET)
			{
				SetPinsHelperLast = e2;
			}
			e2 = e2->next;
		}
		while(e2 != 0 && e2->timeDelta == cur_timeStamp);

		// call OnPinUpdate *once* only per timestamp, even if several pins_ updated
		onSetPins();
	}
	else
	{
		if(e == SetPinsHelperLast) // then we've finnished for this timestamp, reset.
		{
			SetPinsHelperLast = 0;
		}
	}
}
*/
void MpPluginBase::midiHelper( const MpEvent* e )
{
	assert(e->eventType == EVENT_MIDI);
	if(e->extraData == 0) // short msg
	{
		onMidiMessage(e->parm1 // pin
			, (const unsigned char*) &(e->parm3), e->parm2); // midi bytes (short msg)
	}
	else
	{
		onMidiMessage(e->parm1 // pin
						, (const unsigned char*) e->extraData, e->parm2); // midi bytes (sysex)
	}
}

MpPluginBase::MpPluginBase( ) :
	blockPos_( 0 )
	,sleepCount_( 0 )
	,streamingPinCount_( 0 )
	,canSleepManualOverride_( SLEEP_AUTO )
	,eventsComplete_( true )
	,recursionFix(false)
{
}

int32_t MpPluginBase::setHost( gmpi::IMpUnknown* phost )
{
	host.Init(phost);
	return phost->queryInterface(MP_IID_HOST, host.asIMpUnknownPtr());
}

// specialised for audio pins_
float MpAudioPinBase::getValue(int bufferPos) const
{
	if(bufferPos < 0)
	{
		assert(plugin_ != nullptr && "err: This pin was not initialized.");
		assert(plugin_->blockPosExact_ && "err: Please use - pin.getValue( someBufferPosition );");
		bufferPos = plugin_->getBlockPosition();
	}

	assert(bufferPos < plugin_->getBlockSize() && "err: Don't access past end of buffer");

	return *(getBuffer() + bufferPos);
}

void MpPinBase::initialize( MpPluginBase* plugin, int PinId, MpBaseMemberPtr handler )
{
	assert( id_ == -1 && "pin initialized twice?" ); // check your constructor's calls to initializePin() for duplicates.

	id_ = PinId;
	plugin_ = plugin;
	eventHandler_ = handler;

	if( eventHandler_ == 0 && getDirection() == MP_IN )
	{
		eventHandler_ = getDefaultEventHandler();
	}
}

void MpPinBase::processEvent( const MpEvent* e)
{
	if(eventHandler_)
		(plugin_->*eventHandler_)(e);
}

// Pins
void MpPinBase::sendPinUpdate( int32_t rawSize, void* rawData, int32_t blockPosition )
{
	assert(plugin_ != nullptr && "err: Please don't forgot to call initializePin(pinWhatever) in contructor.");
	assert(plugin_->debugIsOpen_ && "err: Please don't update output pins in constructor or open().");

	if(blockPosition == -1)
	{
		assert( plugin_->blockPosExact_ && "err: Please use - pin.setValue( value, someBufferPosition );");
		blockPosition = plugin_->getBlockPosition();
	}
	plugin_->getHost()->setPin( blockPosition, getId(), rawSize, rawData );
}

MpBaseMemberPtr MidiInPin::getDefaultEventHandler()
{
	return &MpPluginBase::midiHelper;
}

const std::wstring& setTextPin(MpControlPinBase<std::wstring, gmpi::MP_STRING>& pin, const std::wstring& text)
{
	pin.setValue(text);
	return text;
}

const std::string& setTextPin(MpControlPinBase<std::wstring, gmpi::MP_STRING>& pin, const std::string& text)
{
	const auto wtext = JmUnicodeConversions::Utf8ToWstring(text);
	pin.setValue(wtext);
	return text;
}

const char* setTextPin(MpControlPinBase<std::wstring, gmpi::MP_STRING>& pin, const char* text)
{
	const auto wtext = JmUnicodeConversions::Utf8ToWstring(text);
	pin.setValue(wtext);
	return text;
}

const wchar_t* setTextPin(MpControlPinBase<std::wstring, gmpi::MP_STRING>& pin, const wchar_t* text)
{
	std::wstring wtext(text);
	pin.setValue(wtext);
	return text;
}

StringInPin::operator std::string()
{
	return JmUnicodeConversions::WStringToUtf8(getValue());
}

StringOutPin::operator std::string()
{
	return JmUnicodeConversions::WStringToUtf8(getValue());
}

std::string StringOutPin::operator=(std::string valueUtf8)
{
	return setTextPin(*this, valueUtf8);
}

std::wstring StringOutPin::operator=(std::wstring value)
{
	return setTextPin(*this, value);
}

const char* StringOutPin::operator=(const char* valueUtf8)
{
	return setTextPin(*this, valueUtf8);
}

const wchar_t* StringOutPin::operator=(const wchar_t* valueUtf16)
{
	return setTextPin(*this, valueUtf16);
}

void MpPluginBase::initializePin( int PinId, MpPinBase &pin, MpBaseMemberPtr handler )
{
	pin.initialize( this, PinId, handler );

#if defined(_DEBUG)
	std::pair< Pins_t::iterator, bool > r =
#endif
    pins_.insert( std::pair<int,MpPinBase *>( PinId, &pin ) );

	assert( r.second && "Did you initializePin() with the same index twice?" );

#if defined(_DEBUG)
// TODO	host_->verifyPinMatchesXml( PinId, pin.getDirection(), pin.getDatatype() );
#endif
}

int32_t MpPluginBase::setBuffer( int32_t pinId, float* buffer )
{
	Pins_t::iterator it = pins_.find(pinId);
	if ( it != pins_.end() )
	{
		(*it).second->setBuffer(buffer);
		return gmpi::MP_OK;
	}

	return MP_FAIL;
}

void MidiOutPin::send(const unsigned char* data, int size, int blockPosition)
{
	assert( blockPosition >= -1 && "MIDI Out pin can't use negative timestamps" );
	if( blockPosition == -1 )
	{
		assert( plugin_->blockPosExact_ && "err: Please use - midiPin.send( data, size, someBufferPosition );");
		blockPosition = plugin_->getBlockPosition();
	}
	plugin_->getHost()->setPin(blockPosition, getId(), size, data);
}

void AudioOutPin::setStreaming(bool isStreaming, int blockPosition)
{
	assert( plugin_ && "Don't forget initializePin() in your DSP class constructor for each pin" );
	assert( plugin_->debugIsOpen_ && "Can't setStreaming() until after module open()." );

	if( blockPosition == -1 )
	{
		assert( plugin_->blockPosExact_ && "err: Please use - pin.setStreaming( isStreaming, someBufferPosition );");
		blockPosition = plugin_->getBlockPosition();
	}

	assert( blockPosition >= 0 && blockPosition < plugin_->getBlockSize() && "block posistion must be within current block");

	// note if streaming has changed (not just repeated one-offs).
	if( isStreaming_ != isStreaming )
	{
		isStreaming_ = isStreaming;
		plugin_->OnPinStreamingChange( isStreaming );
	}

	// Always reset sleep counter on streaming stop or one-off level changes.
	// Do this regardless of what pins are streaming. Else any later change in input pin streaming could switch to sleep mode without counter set.
	if( !isStreaming_ )
	{
		plugin_->resetSleepCounter();
	}

	plugin_->getHost()->setPinStreaming( blockPosition, getId(), (int) isStreaming_ );
};

void MpBase::setSubProcess( SubProcess_ptr func )
{
	// under normal use, curSubProcess_ and saveSubProcess_ are the same.
	if( curSubProcess_ == saveSubProcess_ )
	{
		curSubProcess_ = saveSubProcess_= func;
	}
	else // in sleep mode.
	{
		assert( func != &MpBase::subProcessPreSleep );
		saveSubProcess_ = func;
	}
}


void MpBase::setSleep( bool isOkToSleep )
{
	if( isOkToSleep )
	{
		canSleepManualOverride_ = SLEEP_ENABLE;
		curSubProcess_ = &MpBase::subProcessPreSleep;
	}
	else
	{
		canSleepManualOverride_ = SLEEP_DISABLE;
	}
}

void MpBase2::setSleep( bool isOkToSleep )
{
	if( isOkToSleep )
	{
		canSleepManualOverride_ = SLEEP_ENABLE;
		curSubProcess_ = &MpBase2::subProcessPreSleep;
	}
	else
	{
		canSleepManualOverride_ = SLEEP_DISABLE;
	}
}

// module is OK to sleep, but first needs to output enough samples to clear the output buffers.
void MpBase::subProcessPreSleep( int bufferOffset, int sampleFrames )
{
	// this routine used whenever we MAYBY need to sleep.
	bool canSleep;
	int sampleFramesRemain = sampleFrames;

	do{
		// if sleep mode no longer appropriate, perform normal processing and exit.
		if( canSleepManualOverride_ == SLEEP_AUTO )
		{
			canSleep = streamingPinCount_ == 0;
		}
		else
		{
			canSleep = canSleepManualOverride_ == SLEEP_ENABLE;
		}

		if( !canSleep )
		{
			curSubProcess_ = saveSubProcess_;

			if( sampleFramesRemain > 0 )
			{
				(this->*(saveSubProcess_))( bufferOffset, sampleFramesRemain );
			}
			return;
		}

		if( sleepCount_ <= 0 )
		{
			if( eventsComplete_ )
			{
#if 0 // def _DEBUG
				DebugVerifyOutputsConsistant();
#endif
				getHost()->sleep();
				return;
			}

			// we have to proces more than expected, at least till the next event.
			sleepCount_ = sampleFramesRemain;
		}

		if( sampleFramesRemain == 0 )
		{
			return;
		}

		// only process minimum samples.
		// determin how many samples remain to be processed.
		int toDo = sampleFramesRemain;
		if( sleepCount_ < toDo )
		{
			toDo = sleepCount_;
		}

		sleepCount_ -= toDo; // must be before sub-processs in case sub-process resets it.
		sampleFramesRemain -= toDo;

		(this->*(saveSubProcess_))( bufferOffset, toDo );

		bufferOffset += toDo; // !! Messey, may screw up main process loop if not restored on exit.

	}while( true );
}

// module is OK to sleep, but first needs to output enough samples to clear the output buffers.
void MpBase2::subProcessPreSleep( int sampleFrames )
{
	// this routine used whenever we MAYBY need to sleep.
	bool canSleep;
	int sampleFramesRemain = sampleFrames;
	int saveBlockPos = blockPos_;

	do{
		// if sleep mode no longer appropriate, perform normal processing and exit.
		if( canSleepManualOverride_ == SLEEP_AUTO )
		{
			canSleep = streamingPinCount_ == 0;
		}
		else
		{
			canSleep = canSleepManualOverride_ == SLEEP_ENABLE;
		}

		if( !canSleep )
		{
			curSubProcess_ = saveSubProcess_;

			if( sampleFramesRemain > 0 )
			{
				(this->*(saveSubProcess_))( sampleFramesRemain );
			}
			blockPos_ = saveBlockPos; // restore bufferOffset_, else may be incremented twice.
			return;
		}

		if( sleepCount_ <= 0 )
		{
			if( eventsComplete_ )
			{
#if 0 //def _DEBUG
				DebugVerifyOutputsConsistant();
#endif
				getHost()->sleep();
				blockPos_ = saveBlockPos; // restore bufferOffset_, else may be incremented twice.
				return;
			}

			// we have to proces more than expected, at least till the next event.
			sleepCount_ = sampleFramesRemain;
		}

		if( sampleFramesRemain == 0 )
		{
			blockPos_ = saveBlockPos; // restore bufferOffset_, else may be incremented twice.
			return;
		}

		// only process minimum samples.
		// determin how many samples remain to be processed.
		int toDo = sampleFramesRemain;
		if( sleepCount_ < toDo )
		{
			toDo = sleepCount_;
		}

		sleepCount_ -= toDo; // must be before sub-processs in case sub-process resets it.
		sampleFramesRemain -= toDo;

		(this->*(saveSubProcess_))( toDo );

		blockPos_ += toDo; // !! Messey, may screw up main process loop if not restored on exit.

	}while( true );
}

void MpPluginBase::onGraphStart()	// called on very first sample.
{
	// Send initial update on all output pins.
	for( Pins_t::iterator it = pins_.begin() ; it != pins_.end() ; ++it )
	{
		MpPinBase* pin = (*it).second;
		if( pin->getDirection() == MP_OUT )
		{
			pin->sendFirstUpdate();
		}
	}

	#if defined(_DEBUG)
	debugGraphStartCalled_ = true;
	#endif
}

void MpBase::OnPinStreamingChange( bool isStreaming )
{
	if( isStreaming )
	{
		++streamingPinCount_;
	}
	else
	{
		--streamingPinCount_;
	}

	if( streamingPinCount_ == 0 || canSleepManualOverride_ == SLEEP_ENABLE )
	{
		curSubProcess_ = &MpBase::subProcessPreSleep;
	}
	else // no need for sleep mode.
	{
		curSubProcess_ = saveSubProcess_;
	}
}

void MpBase2::OnPinStreamingChange( bool isStreaming )
{
	if( isStreaming )
	{
		++streamingPinCount_;
	}
	else
	{
		--streamingPinCount_;
	}

	if( streamingPinCount_ == 0 || canSleepManualOverride_ == SLEEP_ENABLE )
	{
		curSubProcess_ = &MpBase2::subProcessPreSleep;
	}
	else // no need for sleep mode.
	{
		curSubProcess_ = saveSubProcess_;
	}
}

void MpPluginBase::resetSleepCounter()
{
	getHost()->getBlockSize( sleepCount_ );
}

// when updating a non-audio output pin, subProcess is not resumed.
// however, we may want to resume subProcess to support pulsing the output pin. (e.g. BPM Tempo)
void MpBase::wakeSubProcessAtLeastOnce()
{
	sleepCount_ = (std::max)(1, sleepCount_);
	curSubProcess_ = saveSubProcess_;
}

#ifdef _DEBUG
	/* Seems not to work when host splits blocks?
// Ensure it's safe to sleep and all output buffers are 'flat lined' (silent).
void MpPluginBase::DebugVerifyOutputsConsistant()
{
	for (Pins_t::iterator it = pins_.begin(); it != pins_.end(); ++it)
	{
		auto op = dynamic_cast<AudioOutPin*>( (*it).second );
		if (op)
		{
			float* buffer = op->getBuffer();
			for (int i = 1; i < getBlockSize(); ++i)
			{
				assert(buffer[0] == buffer[i]); // You have a problem with sleeping the module too soon before output buffers have 'settled' to silence.
			}
		}
	}
}
	*/
#endif

void AudioInPin::preProcessEvent( const gmpi::MpEvent* e )
{
	switch(e->eventType)
	{
		case gmpi::EVENT_PIN_STREAMING_START:
		case gmpi::EVENT_PIN_STREAMING_STOP:
			freshValue_ = true;
			bool isStreaming = e->eventType == gmpi::EVENT_PIN_STREAMING_START;
			if( isStreaming_ != isStreaming )
			{
				isStreaming_ = isStreaming;
				plugin_->OnPinStreamingChange( isStreaming_ );
			}

			// For modules with no output pins, it's important to process that one last sample after processing the event,
			// else the module will sleep immediatly without processing the sample associated with the event.
			if (!isStreaming_)
			{
				plugin_->nudgeSleepCounter();
			}
			break;
	};
}

namespace GmpiSdk
{
	
std::string ProcessorHost::resolveFilename(std::wstring filenameW)
{
	gmpi_sdk::mp_shared_ptr<gmpi::IEmbeddedFileSupport> dspHost;
	Get()->queryInterface(gmpi::MP_IID_HOST_EMBEDDED_FILE_SUPPORT, dspHost.asIMpUnknownPtr());
	assert(dspHost); // new way

	const auto filename = JmUnicodeConversions::WStringToUtf8(filenameW);

	gmpi_sdk::MpString fullFilename;
	dspHost->resolveFilename(filename.c_str(), &fullFilename);

	return fullFilename.c_str();
}

UriFile ProcessorHost::openUri(std::string uri)
{
	gmpi_sdk::mp_shared_ptr<gmpi::IEmbeddedFileSupport> dspHost;
	Get()->queryInterface(gmpi::MP_IID_HOST_EMBEDDED_FILE_SUPPORT, dspHost.asIMpUnknownPtr());
	assert(dspHost); // new way

	gmpi_sdk::mp_shared_ptr<gmpi::IMpUnknown> obj;
	dspHost->openUri(uri.c_str(), &obj.get());

	return { obj };
}

}
