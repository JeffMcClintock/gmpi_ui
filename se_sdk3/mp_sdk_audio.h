// Copyright 2006 Jeff McClintock

#ifndef MP_SDK_AUDIO_H_INCLUDED
#define MP_SDK_AUDIO_H_INCLUDED

#include <map>
#include <vector>
#include <algorithm>
#include <assert.h>
#include "mp_sdk_common.h"
#include "mp_interface_wrapper.h"

class MpBase;
class MpBase2;
class MpPluginBase;

// Pointer to sound processing member function.
typedef void ( MpBase::* SubProcess_ptr)(int bufferOffset, int sampleFrames);
typedef void ( MpBase2::* SubProcess_ptr2)(int sampleFrames);

#define SET_PROCESS(func)	setSubProcess(static_cast <SubProcess_ptr> (func));
#define SET_PROCESS2(func)	setSubProcess(static_cast <SubProcess_ptr2> (func));

// Deprecated macros for registering plugin with factory. Prefer: 	auto r = gmpi::Register<MyClass>::withId(L"JM My Name");
#define REGISTER_PLUGIN( className, pluginId ) namespace{ gmpi::IMpUnknown* PASTE_FUNC(create,className)(){ return static_cast<gmpi::IMpPlugin*> (new className(0)); }; int32_t PASTE_FUNC(r,className) = RegisterPlugin( gmpi::MP_SUB_TYPE_AUDIO, pluginId, &PASTE_FUNC(create,className) );}
#define REGISTER_PLUGIN2( className, pluginId ) namespace{ gmpi::IMpUnknown* PASTE_FUNC(create,className)(){ return static_cast<gmpi::IMpPlugin2*> (new className()); }; int32_t PASTE_FUNC(r,className) = RegisterPlugin( gmpi::MP_SUB_TYPE_AUDIO, pluginId, &PASTE_FUNC(create,className) );}
// Alias for consistancy with "GMPI_REGISTER_GUI"
#define GMPI_REGISTER_PROCESSOR REGISTER_PLUGIN2

// Pointer to event processing member function.
typedef void (MpPluginBase::* MpBaseMemberPtr)(const gmpi::MpEvent*);


namespace GmpiSdk
{
	class ProcessorHost : public Internal::GmpiIWrapper<gmpi::IMpHost>
	{
		gmpi::IGmpiHost* host2 = {};

	public:
		int32_t Init(gmpi::IMpUnknown* phost)
		{
			return phost->queryInterface(gmpi::MP_IID_PROCESSOR_HOST, (void**) &host2);
		}

		// Plugin sending out control data.
		inline int32_t setPin(int32_t blockRelativeTimestamp, int32_t pinId, int32_t size, const void* data)
		{
			return Get()->setPin(blockRelativeTimestamp, pinId, size, data);
		}

		// Plugin audio output start/stop (silence detection).
		inline int32_t setPinStreaming(int32_t blockRelativeTimestamp, int32_t pinId, int32_t isStreaming)
		{
			return Get()->setPinStreaming(blockRelativeTimestamp, pinId, isStreaming);
		}

		// Plugin indicates no processing needed until input state changes.
		inline int32_t sleep()
		{
			return Get()->sleep();
		}

		/*
				// Backdoor to GUI. Not recommended. Use Parameters instead to support proper automation.
				virtual int32_t MP_STDCALL sendMessageToGui(int32_t id, int32_t size, const void* messageData) = 0;
		*/
		// Query audio buffer size.
		inline int32_t getBlockSize() const
		{
			int32_t s{};
			Get()->getBlockSize(s);
			return s;
		}

		inline float getSampleRate() const
		{
			float s{};
			Get()->getSampleRate(s);
			return s;
		}

		inline int32_t getHandle() const
		{
			int32_t s{};
			Get()->getHandle(s);
			return s;
		}

		void SetLatency(int32_t latency)
		{
			if(host2)
			{
				host2->setLatency(latency);
			}
		}
		/*

		// Query sample-rate.
		virtual int32_t MP_STDCALL getSampleRate(float& returnSampleRate) = 0;

		// Each plugin instance has a host-assigned unique handle shared by UI and Audio class.
		virtual int32_t MP_STDCALL getHandle(int32_t& returnHandle) = 0;

		// Identify Host. e.g. "SynthEdit".
		virtual int32_t MP_STDCALL getHostId(int32_t maxChars, wchar_t* returnString) = 0;

		// Query Host version number. e.g. returnValue of 400000 is Version 4.0
		virtual int32_t MP_STDCALL getHostVersion(int32_t& returnVersion) = 0;

		// Query Host registered user name. "John Smith"
		virtual int32_t MP_STDCALL getRegisteredName(int32_t maxChars, wchar_t* returnName) = 0;

		// Provide list of audio pins.
		virtual int32_t MP_STDCALL createPinIterator(void** returnInterface) = 0;

		// Determin if multiple parallel instances (clones) in use. Clones share same control data and GUI.
		virtual int32_t MP_STDCALL isCloned(int32_t* returnValue) = 0;

		// Provide list of 'clones' - plugins working in parallel to process multiple channels of audio, controled by same parameters/UI.
		virtual int32_t MP_STDCALL createCloneIterator(void** returnInterface) = 0;

		// Provide named shared memory, primarily for sharing waveforms and lookup tables between multiple instances.
		// Use sampleRate of -1 to indicate data is independant of samplerate.
		virtual int32_t MP_STDCALL allocateSharedMemory(const wchar_t* id, void** returnPointer, float sampleRate, int32_t size, int32_t& returnNeedInitialise) = 0;

		// SynthEdit-specific.  Determine file's location depending on host application's conventions. // e.g. "bell.wav" -> "C:/My Documents/bell.wav"
		virtual int32_t MP_STDCALL resolveFilename(const wchar_t* shortFilename, int32_t maxChars, wchar_t* returnFullFilename) = 0;

		// SynthEdit-specific.  Determine file's location depending on host application's conventions. // e.g. "bell.wav" -> "C:/My Documents/bell.wav"
		virtual int32_t MP_STDCALL openProtectedFile(const wchar_t* shortFilename, IProtectedFile **file) = 0;

		inline int32_t setLatency(int32_t latencySamples)
		{
			return Get()->setLatency(latencySamples);
		}
		*/

		std::wstring resolveFilename_old(std::wstring filename)
		{
			wchar_t fullFilename[500] = L"";
			Get()->resolveFilename(filename.c_str(), static_cast<int32_t>(std::size(fullFilename)), fullFilename);
			return fullFilename;
		}
		
		std::string resolveFilename(std::wstring filenameW);
		
		UriFile openUri(std::string uri);
	};

	class ProcessorPins
	{
		struct pindata
		{
			int32_t id;
			int32_t direction;
			int32_t datatype;
		};
		gmpi_sdk::mp_shared_ptr<gmpi::IMpPinIterator> it;
		std::vector<pindata> pins;

	public:
		using iterator = std::vector<pindata>::iterator;

		ProcessorPins(gmpi::IMpHost* dspHost)
		{
			auto result = dspHost->createPinIterator(it.getAddressOf());
			assert(gmpi::MP_OK == result);

			if (gmpi::MP_OK == result)
			{
				const auto pincount = size();
				pins.reserve(pincount);

				it->first();
				for (size_t i = 0; i < pincount; ++i)
				{
					pindata p{};
					it->getPinDatatype(p.datatype);
					it->getPinDirection(p.direction);
					it->getPinId(p.id);

					pins.push_back(p);

					it->next();
				}
			}
		}

		size_t size()
		{
			int32_t count = 0;
			it->getCount(count);
			return static_cast<size_t>(count);
		}

		iterator begin()
		{
			return pins.begin();
		}

		iterator end()
		{
			return pins.end();
		}
	};
}

class MpPinBase
{
public:
	MpPinBase() : id_(-1), plugin_(0), eventHandler_(0) {}
	virtual ~MpPinBase() {}
	void initialize( MpPluginBase* plugin, int PinId, MpBaseMemberPtr handler = 0 );

	// overrides for audio pins_
	virtual void setBuffer( float* buffer ) = 0;
	virtual void preProcessEvent( const gmpi::MpEvent* ) {}

	virtual void processEvent( const gmpi::MpEvent* e );
	virtual void postProcessEvent( const gmpi::MpEvent* ) {}

	int getId(){return id_;};
	virtual int getDatatype() const = 0;
	virtual int getDirection() const = 0;
	virtual MpBaseMemberPtr getDefaultEventHandler() = 0;
	virtual void sendFirstUpdate() = 0;

protected:
	void sendPinUpdate( int32_t rawSize, void* rawData, int32_t blockPosition = - 1 );
	int id_;
	class MpPluginBase* plugin_;
	MpBaseMemberPtr eventHandler_;
};

template
<typename T, int pinDatatype = MpTypeTraits<T>::PinDataType>
class MpControlPinBase : public MpPinBase
{
public:
	MpControlPinBase()
	{
	}
	MpControlPinBase( T initialValue ) :freshValue_(false), value_( initialValue )
	{
	}
	void sendPinUpdate(int blockPosition = -1)
	{
		MpPinBase::sendPinUpdate( rawSize(), rawData(), blockPosition );
	}
	void setBuffer( float* /*buffer*/ ) override
	{
		assert(false && "Control-rate pins_ don't have a buffer");
	}
	const T& getValue() const
	{
		assert( id_ != -1 && "remember initializePin() in constructor?" );
		return value_;
	}
	operator T()
	{
		assert( id_ != -1 && "remember initializePin() in constructor?" );
		return value_;
	}
	void setValue( const T &value, int blockPosition = -1 )
	{
		if( !variablesAreEqual<T>(value, value_) ) // avoid unnesc updates
		{
			value_ = value;
			sendPinUpdate( blockPosition );
		}
	}
	const T& operator=(const T &value)
	{
		if( !variablesAreEqual<T>(value, value_) ) // avoid unnesc updates
		{
			value_ = value;
			sendPinUpdate();
		}
		return value_;
	}
	bool operator==(T other)
	{
		assert( plugin_ != 0 && "Don't forget initializePin() on each pin in your constructor." );
		//return other == value_;
		return variablesAreEqual<T>(other, value_);
	}
	virtual void setValueRaw(int size, const void* data)
	{
		//MpTypeTraits<T>::fromRaw(size, data, value_);
		VariableFromRaw<T>(size, data, value_);
	}
	virtual void setValueRaw(size_t size, const void* data)
	{
		VariableFromRaw<T>(static_cast<int>(size), data, value_);
	}
	virtual int rawSize() const
	{
		return variableRawSize<T>(value_);
	}
	virtual void* rawData()
	{
		return variableRawData<T>(value_);
	}
	virtual int getDatatype() const override
	{
		return pinDatatype; //MpTypeTraits<T>::PinDataType;
	}
	void preProcessEvent( const gmpi::MpEvent* e ) override
	{
		switch(e->eventType)
		{
			case gmpi::EVENT_PIN_SET:
				if(e->extraData)
				{
					setValueRaw(e->parm2, e->extraData);
				}
				else
				{
					setValueRaw(e->parm2, &(e->parm3));
				}
				freshValue_ = true;
				break;
		};
	}
	void postProcessEvent( const gmpi::MpEvent* e ) override
	{
		switch(e->eventType)
		{
			case gmpi::EVENT_PIN_SET:
				freshValue_ = false;
				break;
		};
	}
	MpBaseMemberPtr getDefaultEventHandler() override
	{
		return {};
	}
	inline bool isUpdated() const
	{
		return freshValue_;
	}
	void sendFirstUpdate() override
	{
		sendPinUpdate();
	}

protected:
	T value_ = {};
	bool freshValue_ = false; // true = value_ has been updated by host on current sample_clock
};

template
<typename T, int pinDirection_, int pinDatatype = MpTypeTraits<T>::PinDataType>
class MpControlPin : public MpControlPinBase< T, pinDatatype >
{
public:
	MpControlPin() : MpControlPinBase< T, pinDatatype >()
	{
	}
	MpControlPin( T initialValue ) : MpControlPinBase< T, pinDatatype >( initialValue )
	{
	}
	virtual int getDirection() const override
	{
		return pinDirection_;
	}
	const T& operator=(const T &value)
	{
		// GCC don't like using plugin_ in this scope. assert( plugin_ != 0 && "Don't forget initializePin() on each pin in your constructor." );
		return MpControlPinBase< T, pinDatatype> ::operator=(value);
	}
	// todo: specialise for value_ vs ref types !!!
	const T& operator=(const MpControlPin<T, gmpi::MP_IN, pinDatatype> &other)
	{
		return operator=(other.getValue());
	}
	const T& operator=(const MpControlPin<T, gmpi::MP_OUT, pinDatatype> &other)
	{
		return operator=(other.getValue());
	}
};

class MpAudioPinBase : public MpPinBase
{
public:
	MpAudioPinBase(){}
	void setBuffer( float* buffer ) override
	{
		buffer_ = buffer;
	}
	inline float* getBuffer( void ) const
	{
		return buffer_;
	}
	float getValue(int bufferPos = -1) const;
	operator float() const
	{
		return getValue();
	}
	bool operator==(float other)
	{
		return other == getValue();
	}
	virtual void setValueRaw(int /*size*/, void* /*data*/)
	{
		assert(false && "Audio-rate pins_ don't support setValueRaw");
	}
	virtual int getDatatype() const override
	{
		return gmpi::MP_AUDIO;
	}
	MpBaseMemberPtr getDefaultEventHandler() override
	{
		return 0;
	}
	bool isStreaming()
	{
		return isStreaming_;
	}

protected:
	float* buffer_ = {};
	bool isStreaming_ = false; // true = active audio, false = silence or constant input level
};

class AudioInPin : public MpAudioPinBase
{
public:
	AudioInPin() : freshValue_(true) {}
	virtual int getDirection() const
	{
		return gmpi::MP_IN;
	}
	virtual void preProcessEvent( const gmpi::MpEvent* e );
	virtual void postProcessEvent( const gmpi::MpEvent* e )
	{
		switch(e->eventType)
		{
			case gmpi::EVENT_PIN_STREAMING_START:
			case gmpi::EVENT_PIN_STREAMING_STOP:
				freshValue_ = false;
				break;
		};
	}

	bool isUpdated()
	{
		return freshValue_;
	}
	virtual void sendFirstUpdate() {} // N/A.

private:
	bool freshValue_; // true = value_ has been updated on current sample_clock
};

class AudioOutPin : public MpAudioPinBase
{
public:
	virtual int getDirection() const
	{
		return gmpi::MP_OUT;
	}

	// Indicate output pin's value changed, but it's not streaming (a 'one-off' change).
	void setUpdated(int blockPosition = -1)
	{
		setStreaming( false, blockPosition );
	}
	void setStreaming(bool isStreaming, int blockPosition = -1);
	virtual void sendFirstUpdate()
	{
		setStreaming( true );
	}
};

typedef MpControlPin<int, gmpi::MP_IN>			IntInPin;
typedef MpControlPin<int, gmpi::MP_OUT>			IntOutPin;
typedef MpControlPin<float, gmpi::MP_IN>		FloatInPin;
typedef MpControlPin<float, gmpi::MP_OUT>		FloatOutPin;
typedef MpControlPin<MpBlob, gmpi::MP_IN>		BlobInPin;
typedef MpControlPin<MpBlob, gmpi::MP_OUT>		BlobOutPin;

typedef MpControlPin<bool, gmpi::MP_IN>			BoolInPin;
typedef MpControlPin<bool, gmpi::MP_OUT>		BoolOutPin;

// enum (List) pin based on Int Pin.
// WARNING: The pins value is not guaranteed to be in-range. Please clamp to range before use.
typedef MpControlPin<int, gmpi::MP_IN, gmpi::MP_ENUM>	EnumInPin;
typedef MpControlPin<int, gmpi::MP_OUT, gmpi::MP_ENUM>	EnumOutPin;

class StringInPin : public MpControlPin<std::wstring, gmpi::MP_IN>
{
public:
	explicit operator std::string(); // UTF8 encoded.
};

class StringOutPin : public MpControlPin<std::wstring, gmpi::MP_OUT>
{
public:
	explicit operator std::string(); // UTF8 encoded.
	std::string operator=(std::string valueUtf8);
	std::wstring operator=(std::wstring value);
	const char* operator=(const char* valueUtf8);
	const wchar_t* operator=(const wchar_t* valueUtf16);
};

// BLOB2 - Binary datatype. except memory is shared and ref counted
class Blob2PinBase : public MpPinBase
{
protected:
	gmpi::ISharedBlob* value_ = nullptr;

public:
	// overrides for audio pins_
	void setBuffer(float*) override {}
	void preProcessEvent(const gmpi::MpEvent*) override {}
	void processEvent(const gmpi::MpEvent*) override {}
	void postProcessEvent(const gmpi::MpEvent*) override {}

	int getDatatype() const override { return gmpi::MP_BLOB2; }
	MpBaseMemberPtr getDefaultEventHandler() override { return {}; }
	void sendFirstUpdate() override {}

	gmpi::ISharedBlob* getValue()
	{
		return value_;// value_.get();
	}
};

class Blob2InPin : public Blob2PinBase
{
	friend class Blob2OutPin;

	bool freshValue_{};

public:
	int getDirection() const override { return gmpi::MP_IN; }
	void preProcessEvent(const gmpi::MpEvent* e) override
	{
		switch (e->eventType)
		{
		case gmpi::EVENT_PIN_SET:
		{
			// release previous value.
			if (value_)
			{
				value_->release();
			}

			if (e->extraData)
			{
				value_ = *reinterpret_cast<gmpi::ISharedBlob**>(e->extraData);
			}
			else
			{
				value_ = *reinterpret_cast<gmpi::ISharedBlob**>(const_cast<int32_t*>(&(e->parm3)));
			}

			freshValue_ = true;
		}
		break;
		};
	}
	void postProcessEvent(const gmpi::MpEvent* e) override
	{
		switch (e->eventType)
		{
		case gmpi::EVENT_PIN_SET:
			freshValue_ = false;
			break;
		};
	}
	bool isUpdated() const
	{
		return freshValue_;
	}
};

class Blob2OutPin : public Blob2PinBase
{
public:
	int getDirection() const override { return gmpi::MP_OUT; }

	const Blob2InPin& operator=(Blob2InPin& pin)
	{
//		if (value_/*.get()*/ != pin.value_/*.get()*/) // avoid unnesc updates (would need to compare bytes)
		{
			value_ = pin.value_;

			// send out only the pointer, not the data.
			sendPinUpdate(sizeof(value_), &value_, -1);
		}
		return pin;
	}

	const gmpi::ISharedBlob* operator=(gmpi::ISharedBlob* value)
	{
		value_ = value;

		// send out only the pointer, not the data.
		sendPinUpdate(sizeof(value_), &value_, -1);

		return value;
	}
};


class MidiInPin : public MpPinBase
{
public:
	virtual int getDatatype() const
	{
		return gmpi::MP_MIDI;
	}
	virtual int getDirection() const
	{
		return gmpi::MP_IN;
	}

	// These members not needed for MIDI.
	virtual void setBuffer(float* /*buffer*/)
	{
		assert( false && "MIDI pins_ don't have a buffer" );
	}
	virtual MpBaseMemberPtr getDefaultEventHandler();
	virtual void sendFirstUpdate()
	{
	}
};

class MidiOutPin : public MidiInPin
{
public:
	virtual int getDirection() const
	{
		return gmpi::MP_OUT;
	}
	virtual MpBaseMemberPtr getDefaultEventHandler()
	{
		assert( false && "output pins don't need event handler" );
		return 0;
	}

	virtual void send(const unsigned char* data, int size, int blockPosition = -1);

	// convenience method (pass uint8_t array, deduce size). can't pass block position, else compiler chooses other overload.
	template <int N>
	void send(const uint8_t(&data)[N]) //, int blockPosition = -1)
	{
		send((const unsigned char*)&data, static_cast<int>(N));// , blockPosition);
	}
};

/*
template<>
MpBaseMemberPtr MpPinMidi<gmpi::MP_IN>::getDefaultEventHandler();
template<>
MpBaseMemberPtr MpPinMidi<gmpi::MP_OUT>::getDefaultEventHandler();
*/

typedef	std::map<int, MpPinBase*> Pins_t;

class MpPluginBase : public gmpi::IMpPlugin, public gmpi::IMpPlugin2, public IoldSchoolInitialisation, public gmpi::IMpLegacyInitialization
{
	friend class TempBlockPositionSetter;

public:
	MpPluginBase();
	virtual ~MpPluginBase() {}

	// IMpUnknown methods
	int32_t MP_STDCALL queryInterface(const gmpi::MpGuid& iid, void** returnInterface) override;
	GMPI_REFCOUNT

	int32_t MP_STDCALL open() override;

	// IMpPlugin methods, forwarded to IMpPlugin2 equivalent.
	int32_t MP_STDCALL receiveMessageFromGui( int32_t id, int32_t size, void* messageData ) override
	{
		int32_t r = gmpi::MP_OK;
		if( !recursionFix )
		{
			recursionFix = true;
			// When hosted as IMpPlugin, forward call to IMpPlugin2 version.
			r = receiveMessageFromGui( id, size, (const void*) messageData );
			recursionFix = false;
		}
		return r;
	}

	void MP_STDCALL process( int32_t count, gmpi::MpEvent* events ) override
	{
		// When hosted as IMpPlugin, forward call to IMpPlugin2 version.
		process(count, ( const gmpi::MpEvent* ) events);
	}

	// IMpPlugin2 methods
	int32_t MP_STDCALL setHost(IMpUnknown* host) override;
	int32_t MP_STDCALL setBuffer(int32_t pinId, float* buffer) override;
	virtual void MP_STDCALL process( int32_t count, const gmpi::MpEvent* events ) override = 0;
	int32_t MP_STDCALL receiveMessageFromGui( int32_t id, int32_t size, const void* messageData ) override
	{
		int32_t r = gmpi::MP_OK;
		if( !recursionFix )
		{
			recursionFix = true;
			// When hosted as IMpPlugin, forward call to IMpPlugin2 version.
			r = receiveMessageFromGui(id, size, (void*)messageData);
			recursionFix = false;
		}
		return r;
	}
	// Methods
	void sendEvent(){}
	void initializePin( int PinId, MpPinBase& pin, MpBaseMemberPtr handler = 0 );
	void initializePin( MpPinBase &pin, MpBaseMemberPtr handler = 0 )
	{
		int idx;
		if (!pins_.empty())
		{
			idx = pins_.rbegin()->first + 1;
		}
		else
		{
			idx = 0;
		}
		initializePin(idx, pin, handler); // Automatic indexing.
	}

	inline int getBlockPosition()
	{
		return blockPos_;
	}
	void preProcessEvent( const gmpi::MpEvent* e );
	void processEvent( const gmpi::MpEvent* e );
	void postProcessEvent( const gmpi::MpEvent* e );
	virtual void onMidiMessage(int pin, const unsigned char* midiMessage, int size) // size < 4 for short msg, or > 4 for sysex.
    {
        onMidiMessage( pin, ( unsigned char*) midiMessage, size ); // cope seamlessly with older plugins.
    }
    virtual void onMidiMessage( int /*pin*/, unsigned char* /*midiMessage*/, int /*size*/ ){} // deprecated, non-const correct version of above.
	virtual void onSetPins(){}  // one or more pins_ updated.  Check pin update flags to determine which ones.
	virtual void OnPinStreamingChange( bool isStreaming ) = 0;
	inline gmpi::IMpHost* getHost( void ) const
	{
		assert(host.Get() != nullptr); // Please don't call host from contructor. Use initialize() instead.
		return host.Get();
	}

	void midiHelper( const gmpi::MpEvent* e );
	int getBlockSize() const
	{
		return host.getBlockSize();
	}
	float getSampleRate() const
	{
		return host.getSampleRate();
	}
	void resetSleepCounter();
	void wakeSubProcessAtLeastOnce();
	void nudgeSleepCounter()
	{
		sleepCount_ = (std::max)(sleepCount_, 1);
	}
	virtual void onGraphStart();	// called on very first sample.

protected:
	GmpiSdk::ProcessorHost host;
	Pins_t pins_;

	int blockPos_;				// valid only during processEvent()
	int sleepCount_;			// sleep countdown timer.
	int streamingPinCount_;		// tracks how many pins streaming.
	enum { SLEEP_AUTO = -1, SLEEP_DISABLE, SLEEP_ENABLE } canSleepManualOverride_;
	bool eventsComplete_;		// Flag indicates all events have been delt with, and module is safe to sleep.
	bool recursionFix;

public:
#if defined(_DEBUG)
	bool debugIsOpen_ = false;
	bool blockPosExact_ = true;
	bool debugGraphStartCalled_ = false;
#endif
};

// When sending values out a pin, it's cleaner if the block-position is known,
// however in subProcess(), we can't usually identify a specific block position automatically.
// This helper class lets you set the block-position manually in able to use shorthand pin setting syntax. e.g. pinOut = 3;
class TempBlockPositionSetter
{
#if defined(_DEBUG)
	bool saveBlockPosExact_;
#endif
	int saveBlockPosition_;
	MpPluginBase* module_;

public:
	TempBlockPositionSetter(MpPluginBase* module, int currentBlockPosition)
	{
		module_ = module;
		saveBlockPosition_ = module_->getBlockPosition();
		module_->blockPos_ = currentBlockPosition;

#if defined(_DEBUG)
		saveBlockPosExact_ = module->blockPosExact_;
		module_->blockPosExact_ = true;
#endif
	}

	~TempBlockPositionSetter()
	{
		module_->blockPos_ = saveBlockPosition_;
#if defined(_DEBUG)
		module_->blockPosExact_ = saveBlockPosExact_;
#endif
	}
};

class MpBase: public MpPluginBase
{
public:
	MpBase(gmpi::IMpUnknown* /*unused*/) : MpPluginBase()
	,curSubProcess_( &MpBase::subProcessPreSleep )
	,saveSubProcess_( &MpBase::subProcessNothing )
	{
	}

	virtual void MP_STDCALL process(int32_t count, const gmpi::MpEvent* events);
	void setSubProcess(SubProcess_ptr func);
	inline int blockPosition() // wrong coding standard, retained for backward compatibility. Use getBlockPosition() instead.
	{
		return blockPos_;
	}
	SubProcess_ptr getSubProcess()
	{
		return saveSubProcess_;
	}
	void setSleep(bool isOkToSleep);
	void OnPinStreamingChange( bool isStreaming );
	void wakeSubProcessAtLeastOnce();

protected:
	// the default routine, if none set by module.
	void subProcessNothing(int /*bufferOffset*/, int /*sampleFrames*/) {}
	void subProcessPreSleep(int bufferOffset, int sampleFrames);

private:
	SubProcess_ptr curSubProcess_;
	SubProcess_ptr saveSubProcess_; // saves curSubProcess_ while sleeping
};

// Advanced plugin base-class with simplified sub_process method.
/*
 To upgrade:
 * Replace references to MpBase with MpBase2
 * change REGISTER_PLUGIN to REGISTER_PLUGIN2
 * remove "IMpUnknown* host" from constructors.
 * Remove "int bufferOffset," parameter from processing methods.
 * change assignments in processing -
		float* signal = bufferOffset + pinSignal.getBuffer();
	to
		float* signal = getBuffer( pinSignal );

 * Change any "SET_PROCESS" to "SET_PROCESS2"
 * Change any "SubProcess_ptr" to "SubProcess_ptr2"

 getBlockPosition() supplies buffer offset if needed.
 */

class MpBase2: public MpPluginBase
{
public:
	MpBase2() : MpPluginBase()
	,curSubProcess_( &MpBase2::subProcessPreSleep )
	,saveSubProcess_( &MpBase2::subProcessNothing )
	{
	}

	virtual void MP_STDCALL process(int32_t count, const gmpi::MpEvent* events);
	// TODO: Should return const float* for input pins to prevent inadvertant modification of input buffer.
	inline float* getBuffer( const MpAudioPinBase& pin ) const
	{
		return blockPos_ + pin.getBuffer();
	}

	template <typename PointerToMember>
	void setSubProcess(PointerToMember functionPointer)
	{
		// under normal use, curSubProcess_ and saveSubProcess_ are the same.
		if (curSubProcess_ == saveSubProcess_)
		{
			curSubProcess_ = saveSubProcess_ = static_cast <SubProcess_ptr2> (functionPointer);
		}
		else // in sleep mode.
		{
			saveSubProcess_ = static_cast <SubProcess_ptr2> (functionPointer);
			assert(saveSubProcess_ != &MpBase2::subProcessPreSleep);
		}
	}

	inline SubProcess_ptr2 getSubProcess()
	{
		return saveSubProcess_;
	}
	void setSleep(bool isOkToSleep);
	void OnPinStreamingChange( bool isStreaming );

	// the default routine, if none set by module.
	void subProcessNothing(int /*sampleFrames*/){}
	void subProcessPreSleep(int sampleFrames);

	SubProcess_ptr2 curSubProcess_;

private:
	SubProcess_ptr2 saveSubProcess_; // saves curSubProcess_ while sleeping
};

#endif // .H INCLUDED
