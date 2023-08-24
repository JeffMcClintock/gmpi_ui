#pragma once

/* Copyright (c) 2007-2021 SynthEdit Ltd
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*     * Redistributions of source code must retain the above copyright
*       notice, this list of conditions and the following disclaimer.
*     * Redistributions in binary form must reproduce the above copyright
*       notice, this list of conditions and the following disclaimer in the
*       documentation and/or other materials provided with the distribution.
*     * Neither the name SEM, nor SynthEdit, nor 'Music Plugin Interface' nor the
*       names of its contributors may be used to endorse or promote products
*       derived from this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY SynthEdit Ltd ``AS IS'' AND ANY
* EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL SynthEdit Ltd BE LIABLE FOR ANY
* DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
* LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
* ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
* SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef MP_API_H_INCLUDED
#define MP_API_H_INCLUDED

#include "mp_sdk_stdint.h"
#include "mp_sdk_guid.h"

// Platform specific definitions.
#if defined(_WIN32)

#define MP_PLATFORM_WIN32	1
#define MP_STDCALL		__stdcall
#endif


#if defined(__APPLE__)
#define MP_PLATFORM_MACOSX	1
#define MP_STDCALL		
#endif


#if defined(__linux__)
#define MP_PLATFORM_LINUX	1
#if defined(__i386__)
#define MP_STDCALL		__attribute__((stdcall))
#else
#define MP_STDCALL		
#endif
#endif

#ifdef __cplusplus

#ifndef DECLSPEC_NOVTABLE
#if (_MSC_VER >= 1100) && defined(__cplusplus)
#define DECLSPEC_NOVTABLE   __declspec(novtable)
#else
#define DECLSPEC_NOVTABLE
#endif
#endif

// Handy macro to save typing.
#define GMPI_QUERYINTERFACE1( INTERFACE_IID, CLASS_NAME ) \
	virtual int32_t MP_STDCALL queryInterface(const gmpi::MpGuid& iid, void** returnInterface) override \
{ \
	*returnInterface = 0; \
	if (iid == INTERFACE_IID || iid == gmpi::MP_IID_UNKNOWN ) \
{ \
	*returnInterface = static_cast<CLASS_NAME*>(this); \
	addRef(); \
	return gmpi::MP_OK; \
} \
	return gmpi::MP_NOSUPPORT; \
}

#define GMPI_REFCOUNT int32_t refCount2_ = 1; \
	virtual int32_t MP_STDCALL addRef() override \
{ \
	return ++refCount2_; \
} \
	virtual int32_t MP_STDCALL release() override \
{ \
	if (--refCount2_ == 0) \
	{ \
	delete this; \
	return 0; \
	} \
	return refCount2_; \
} \

#define GMPI_REFCOUNT_NO_DELETE	\
	virtual int32_t MP_STDCALL addRef() override \
{ \
	return 1; \
} \
	virtual int32_t MP_STDCALL release() override \
{ \
	return 1; \
} \

#define GMPI_QUERYINTERFACE2( INTERFACE_IID, CLASS_NAME, BASE_CLASS ) \
	virtual int32_t MP_STDCALL queryInterface(const gmpi::MpGuid& iid, void** returnInterface) override \
{ \
	*returnInterface = 0; \
	if (iid == INTERFACE_IID ) \
{ \
	*returnInterface = static_cast<CLASS_NAME*>(this); \
	addRef(); \
	return gmpi::MP_OK; \
} \
return BASE_CLASS::queryInterface(iid, returnInterface); \
} \

/*
// Same but for stack-based object (no delete when refcount goes zero)
#define GMPI_INTERFACE_METHODS_NODELETE( INTERFACE_IID, CLASS_NAME ) \
	virtual int32_t MP_STDCALL queryInterface(const gmpi::MpGuid& iid, void** returnInterface) \
{ \
	*returnInterface = 0; \
	if (iid == INTERFACE_IID || iid == gmpi::MP_IID_UNKNOWN ) \
{ \
	*returnInterface = static_cast<CLASS_NAME*>(this); \
	return gmpi::MP_OK; \
} \
	return gmpi::MP_NOSUPPORT; \
} \
	virtual int32_t MP_STDCALL addRef() \
{ \
	return 1; \
} \
	virtual int32_t MP_STDCALL release() \
{ \
	return 1; \
} \
*/
namespace gmpi
{
#endif // __cplusplus

	// All methods return an error code.
	// One of these values.
	enum
	{
		MP_HANDLED = 1,		// Success. In case of GUI - no further handing required.
		MP_OK = 0,			// Success. In case of GUI indicates successful mouse "hit".
		MP_FAIL = -1,		// General failure.
		MP_UNHANDLED = -1,	// Event not handled.
		MP_NOSUPPORT = -2,	// Interface not supported.
		MP_CANCEL = -3,		// Async operation cancelled.
	};

	// Silence detection - Audio pins may be in either of two states.
	enum MpPinStatus{ PIN_STATIC = 1, PIN_STREAMING };

	// Timestamped events.
	enum MpEventType{ EVENT_PIN_SET = 100, EVENT_PIN_STREAMING_START, EVENT_PIN_STREAMING_STOP, EVENT_MIDI, EVENT_GRAPH_START };

	struct MpEvent
	{
		int32_t timeDelta;	// Relative to block.
		int32_t eventType;	// See MpEventType enumeration.
		int32_t parm1;		// Pin index if needed.
		int32_t parm2;		// Sizeof additional data. >4 implies extraData points to value.
		int32_t parm3;		// Pin value (if 4 bytes or less).
		int32_t parm4;		// Voice ID. or additional pin value data
		char* extraData;	// Additional data.
		struct MpEvent* next;		// Next event in list.
	};

	// Versioning of the plugin and host APIs are handled by unique IDs.

	// GUID for IMpPlugin  - {2B0DFC7E-A539-49cd-B72D-32FF9DF0D9E4}
	static const struct MpGuid MP_IID_PLUGIN =
	{ 0x2b0dfc7e, 0xa539, 0x49cd, { 0xb7, 0x2d, 0x32, 0xff, 0x9d, 0xf0, 0xd9, 0xe4 } };

	// GUID for IMpLegacyInitialization  - {0B471002-5627-4D46-984E-C0DC79D0AD35}
	static const MpGuid MP_IID_LEGACY_INITIALIZATION =
	{ 0xb471002, 0x5627, 0x4d46, { 0x98, 0x4e, 0xc0, 0xdc, 0x79, 0xd0, 0xad, 0x35 } };

	// GUID for IMpPlugin2
	static const MpGuid MP_IID_PLUGIN2 =
		// {1E07E3E8-8118-457F-A63C-D4F282A0F519}
	{ 0x1e07e3e8, 0x8118, 0x457f, { 0xa6, 0x3c, 0xd4, 0xf2, 0x82, 0xa0, 0xf5, 0x19 } };

	// GUID for IMpAudioPlugin
	// {23835D7E-DCEB-4B08-A9E7-B43F8465939E}
	static const MpGuid MP_IID_AUDIO_PLUGIN = // MP_IID_PLUGIN2 =
	{ 0x23835d7e, 0xdceb, 0x4b08, { 0xa9, 0xe7, 0xb4, 0x3f, 0x84, 0x65, 0x93, 0x9e } };

	// GUID for Host.
	// {4F1B532F-3C46-4927-A498-614867425BE7}
	static const struct MpGuid MP_IID_HOST =
	{ 0x4f1b532f, 0x3c46, 0x4927, { 0xa4, 0x98, 0x61, 0x48, 0x67, 0x42, 0x5b, 0xe7 } };

	// GUID for Unknown - {00000000-0000-C000-0000-000000000046}
	static const struct MpGuid MP_IID_UNKNOWN =
	{
		0x00000000, 0x0000, 0xC000,
		{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 }
	};

	// GUID for IMpFactory 
	// {9A7904B7-7F7F-4d61-B558-C1A6695BB638}
	static const struct MpGuid MP_IID_FACTORY =
	{ 0x9a7904b7, 0x7f7f, 0x4d61, { 0xb5, 0x58, 0xc1, 0xa6, 0x69, 0x5b, 0xb6, 0x38 } };

	// GUID for IMpFactory2 (plain constructor instansiation). 
	// {91C3D2EE-C701-4F35-91A6-094DC7E05976}
	static const struct MpGuid MP_IID_FACTORY2 =
	{ 0x91c3d2ee, 0xc701, 0x4f35, { 0x91, 0xa6, 0x9, 0x4d, 0xc7, 0xe0, 0x59, 0x76 } };

	// GUID for IMpShellFactory. 
	// {72148962-F9C9-4DB1-9F71-EB80CD6469FC}
	static const struct MpGuid MP_IID_SHELLFACTORY =
	{ 0x72148962, 0xf9c9, 0x4db1,{ 0x9f, 0x71, 0xeb, 0x80, 0xcd, 0x64, 0x69, 0xfc } };

#ifdef __cplusplus
} // namespace gmpi
#endif

// GMPI API's are provided in plain C and also C++ versions.
#ifndef __cplusplus 

// Plain C API.

// All Plugins support 3 fundamental functions. Querying the API version, plus creation and destruction via reference counting.
struct IMpUnknown
{
	int32_t(MP_STDCALL *queryInterface)(struct void**, const struct MpGuid* iid, struct void*** iobject);
	int32_t(MP_STDCALL *addRef)(struct void**);
	int32_t(MP_STDCALL *release)(struct void**);
};

struct IMpPlugin
{
	// IMpUnknown functions.
	int32_t(MP_STDCALL *queryInterface)(struct IMpPlugin**, const struct MpGuid* iid, struct void*** iobject);
	int32_t(MP_STDCALL *addRef)(struct IMpPlugin**);
	int32_t(MP_STDCALL *release)(struct IMpPlugin**);

	// Processing about to start.  Allocate resources here.
	int32_t(MP_STDCALL *open)(struct IMpPlugin** plugin);

	// Notify plugin of audio buffer address, one pin at a time. Address may change between process() calls.
	int32_t(MP_STDCALL *setBuffer)(struct IMpPlugin** plugin, int32_t pinId, float* buffer);

	// Process a time slice. No Return code, must always succeed.
	void (MP_STDCALL* process)(struct IMpPlugin** plugin, uint32_t count, struct MpEvent* events);

	// 'Backdoor' to UI class.  Not recommended. WARNING: Called outside scope of Process function, you can't set pins or send MIDI during this call.
	int32_t(MP_STDCALL* receiveMessageFromGui)(struct IMpPlugin** plugin, int32_t id, int32_t size, void* messageData);
};

struct IMpFactory
{
	// IMpUnknown functions.
	int32_t(MP_STDCALL *queryInterface)(struct IMpFactory**, const struct MpGuid* iid, struct void*** iobject);
	int32_t(MP_STDCALL *addRef)(struct IMpFactory**);
	int32_t(MP_STDCALL *release)(struct IMpFactory**);

	// Instantiate a plugin.
	int32_t(MP_STDCALL* createInstance)(struct IMpFactory**,
		const wchar_t* iid, int32_t subType, struct void** host, struct void*** returnInterface);

	// Allows host to display plugin's SDK version and compiler settings to user.
	int32_t(MP_STDCALL* getSdkInformation)(struct IMpFactory**, int32_t* returnSdkVersion, int32_t maxChars, wchar_t* returnCompilerInformation);
};


struct IMpHost
{
	// IMpUnknown functions.
	int32_t(MP_STDCALL *queryInterface)(struct IMpHost**, const struct MpGuid* iid, struct void*** iobject);
	int32_t(MP_STDCALL *addRef)(struct IMpHost**);
	int32_t(MP_STDCALL *release)(struct IMpHost**);

	// Plugin sending out control data.
	int32_t(MP_STDCALL *setPin)(struct IMpHost**, int32_t blockRelativeTimestamp, int32_t pinId, int32_t size, const void* data);

	// Plugin audio output start/stop (silence detection).
	int32_t(MP_STDCALL *setPinStreaming)(struct IMpHost**, int32_t blockRelativeTimestamp, int32_t pinId, int32_t isStreaming);

	// Plugin indicates no processing needed until input state changes. 
	int32_t(MP_STDCALL *sleep)(struct IMpHost**);

	// Backdoor to GUI. Not recommended. Use Parameters instead to support proper automation.
	int32_t(MP_STDCALL *sendMessageToGui)(struct IMpHost**, int32_t id, int32_t size, const void* messageData);

	// Query audio buffer size.
	int32_t(MP_STDCALL *getBlockSize)(struct IMpHost**, int32_t* returnBlockSize);

	// Query sample-rate.
	int32_t(MP_STDCALL *getSampleRate)(struct IMpHost**, float* returnSampleRate);

	// Each plugin instance has a host-assigned unique handle shared by UI and Audio class.
	int32_t(MP_STDCALL *getHandle)(struct IMpHost**, int32_t* returnHandle);

	// Identify Host. e.g. "SynthEdit".
	int32_t(MP_STDCALL *getHostId)(struct IMpHost**, int32_t maxChars, wchar_t* returnString);

	// Query Host version number. e.g. returnValue of 400000 is Version 4.0
	int32_t(MP_STDCALL *getHostVersion)(struct IMpHost**, int32_t* returnVersion);

	// Query Host registered user name. "John Smith"
	int32_t(MP_STDCALL *getRegisteredName)(struct IMpHost**, int32_t maxChars, wchar_t* returnName);

	// Provide list of audio pins.
	int32_t(MP_STDCALL *createPinIterator)(struct IMpHost**, struct void** returnInterface);

	// Determin if multiple parallel instances (clones) in use. Clones share same control data and GUI.
	int32_t(MP_STDCALL *isCloned)(struct IMpHost**, int32_t* returnValue);

	// Provide list of 'clones' - plugins working in parallel to process multiple channels of audio, controled by same parameters/UI. 
	int32_t(MP_STDCALL *createCloneIterator)(struct IMpHost**, struct void** returnInterface);

	// Provide named shared memory, primarily for sharing lookup tables between multiple instances.
	int32_t(MP_STDCALL *allocateSharedMemory)(struct IMpHost**, const wchar_t* id, void** returnPointer, float sampleRate, int32_t size, int32_t* returnNeedInitialise);

	// SynthEdit-specific.  Determine files location depending on host application's conventions. // e.g. "bell.wav" -> "C:/My Documents/bell.wav"
	int32_t(MP_STDCALL *resolveFilename)(struct IMpHost**, const wchar_t* shortFilename, int32_t maxChars, wchar_t* returnFullFilename);

	// SynthEdit-specific.  Open file depending on host application's conventions. // e.g. "bell.wav" -> "C:/My Documents/bell.wav"
	// DEPRECATED: see IMpUserInterfaceHost2
	int32_t(MP_STDCALL *openProtectedFile)(struct IMpHost**, const wchar_t* shortFilename, struct IProtectedFile **file);
};

#else

#include <string>
#include <assert.h>

#if defined(_WIN32) || defined(__FLAT__)
	#pragma pack(push,8)
#elif defined __BORLANDC__
	#pragma -a8
#endif

namespace gmpi
{

// IMpUnknown Interface.
// This is the base interface.  All objects support these 3 methods.
class DECLSPEC_NOVTABLE IMpUnknown
{
public:
	// Query the object for a supported interface.
	virtual int32_t MP_STDCALL queryInterface( const MpGuid& iid, void** returnInterface ) = 0;

	// Increment the reference count of an object.
	virtual int32_t MP_STDCALL addRef() = 0;

	// Decrement the reference count of an object and possibly destroy.
	virtual int32_t MP_STDCALL release() = 0;
};


// Handy way to return string from method.
class DECLSPEC_NOVTABLE IString : public IMpUnknown
{
public:
	virtual int32_t MP_STDCALL setData(const char* pData, int32_t pSize) = 0;
	virtual int32_t MP_STDCALL getSize() = 0;
	virtual const char* MP_STDCALL getData() = 0;
};

// GUID for IString
// {AB8FFB21-44FF-42B7-8885-29431399E7E4}
static const MpGuid MP_IID_RETURNSTRING =
{ 0xab8ffb21, 0x44ff, 0x42b7,{ 0x88, 0x85, 0x29, 0x43, 0x13, 0x99, 0xe7, 0xe4 } };


// IMpPlugin
// Music plugin audio processing interface.
class IMpPlugin : public IMpUnknown
{
public:
	// Processing about to start.  Allocate resources here.
	virtual int32_t MP_STDCALL open() = 0;

	// Notify plugin of audio buffer address, one pin at a time. Address may change between process() calls.
	virtual int32_t MP_STDCALL setBuffer( int32_t pinId, float* buffer ) = 0;

	// Process a time slice. No Return code, must always succeed.
	virtual void MP_STDCALL process( int32_t count, MpEvent* events ) = 0;

	// 'Backdoor' to UI class.  Not recommended.
	virtual int32_t MP_STDCALL receiveMessageFromGui( int32_t id, int32_t size, void* messageData ) = 0;
};

// Helper for old hosts calling new-style plugins and vica-versa.
class IMpLegacyInitialization : public IMpUnknown
{
public:
	virtual int32_t MP_STDCALL setHost(gmpi::IMpUnknown* host) = 0;
};

// IMpPlugin2
// Music plugin audio processing interface V2. 2-stage construction.
class IMpPlugin2 : public IMpUnknown
{
public:
	// Establish connection to host.
	virtual int32_t MP_STDCALL setHost(IMpUnknown* host) = 0;

	// Processing about to start.  Allocate resources here.
	virtual int32_t MP_STDCALL open() = 0;

	// Notify plugin of audio buffer address, one pin at a time. Address may change between process() calls.
	virtual int32_t MP_STDCALL setBuffer( int32_t pinId, float* buffer ) = 0;

	// Process a time slice. No Return code, must always succeed.
	virtual void MP_STDCALL process( int32_t count, const MpEvent* events ) = 0;

	// 'Backdoor' to UI class.  Not recommended. WARNING: Called outside scope of Process function, you can't set pins or send MIDI during this call.
	virtual int32_t MP_STDCALL receiveMessageFromGui( int32_t id, int32_t size, const void* messageData ) = 0;
};

// Music plugin audio processing interface. simplified. compatible with IMpPlugin2.
class IMpAudioPlugin : public IMpUnknown
{
public:
	// Establish connection to host.
	virtual int32_t setHost(IMpUnknown* host) = 0;

	// Processing about to start.  Allocate resources here.
	virtual int32_t open() = 0;

	// Notify plugin of audio buffer address, one pin at a time. Address may change between process() calls.
	virtual int32_t setBuffer(int32_t pinId, float* buffer) = 0;

	// Process a time slice. No Return code, must always succeed.
	virtual void process(int32_t count, const MpEvent* events) = 0;
};

// IMpHost - The audio host interface.

enum MP_PinDirection{ MP_IN, MP_OUT };

enum MP_PinDatatype{ MP_ENUM=0, MP_STRING=1, MP_MIDI=2, MP_FLOAT64, MP_BOOL=4, MP_AUDIO=5, MP_FLOAT32=6, MP_INT32=8, MP_INT64=9, MP_BLOB=10, MP_STRING_UTF8=12, MP_BLOB2 };

// SynthEdit imbedded file.
class IProtectedFile
{
public:
	virtual int32_t MP_STDCALL close() = 0;

//	virtual int32_t MP_STDCALL getSize( int32_t& returnValue ) = 0;
	virtual int32_t MP_STDCALL getSize32( int32_t& returnValue ) = 0;

	virtual int32_t MP_STDCALL read( char* buffer, int32_t size ) = 0;
};

// TODO Beutify !!!
enum MP_ProtectedFileSeekType{ PFST_BEGIN = 0, PFST_CURRENT = 1, PFST_END = 2 };
class IProtectedFile2 : public IMpUnknown
{
public:
	virtual int32_t MP_STDCALL read(void* buffer, int64_t size, int64_t* returnBytesRead = 0) = 0;
	virtual int32_t MP_STDCALL seek(int64_t distanceToMove, int32_t moveMethod, int64_t* newStreamPosition) = 0;
	virtual int32_t MP_STDCALL getSize(int64_t* returnValue) = 0;
};

// GUID for IProtectedFile2.
// {CEA854B2-E0B9-4858-9231-77F20EB01020}
static const MpGuid MP_IID_PROTECTED_FILE2 =
{ 0xcea854b2, 0xe0b9, 0x4858, { 0x92, 0x31, 0x77, 0xf2, 0xe, 0xb0, 0x10, 0x20 } };


// DSP pin iterator.
class IMpPinIterator : public IMpUnknown
{
public:
	virtual int32_t MP_STDCALL getCount( int32_t &returnCount ) = 0;
	virtual int32_t MP_STDCALL first() = 0;
	virtual int32_t MP_STDCALL next() = 0;

	virtual int32_t MP_STDCALL getPinId( int32_t& returnValue ) = 0;
	virtual int32_t MP_STDCALL getPinDirection( int32_t& returnValue ) = 0;
	virtual int32_t MP_STDCALL getPinDatatype( int32_t& returnValue ) = 0;
};

// GUID for IMpPinIterator.
// {647969F3-7C78-49B9-B73F-21431FAD751A}
static const MpGuid MP_IID_PIN_ITERATOR =
{ 0x647969f3, 0x7c78, 0x49b9, { 0xb7, 0x3f, 0x21, 0x43, 0x1f, 0xad, 0x75, 0x1a } };

class IMpHost : public IMpUnknown
{
public:
	// Plugin sending out control data.
	virtual int32_t MP_STDCALL setPin( int32_t blockRelativeTimestamp, int32_t pinId, int32_t size, const void* data ) = 0;

	// Plugin audio output start/stop (silence detection).
	virtual int32_t MP_STDCALL setPinStreaming( int32_t blockRelativeTimestamp, int32_t pinId, int32_t isStreaming ) = 0;

	// Plugin indicates no processing needed until input state changes. 
	virtual int32_t MP_STDCALL sleep() = 0;

	// Backdoor to GUI. Not recommended. Use Parameters instead to support proper automation.
	virtual int32_t MP_STDCALL sendMessageToGui( int32_t id, int32_t size, const void* messageData ) = 0;

	// Query audio buffer size.
	virtual int32_t MP_STDCALL getBlockSize( int32_t& returnBlockSize ) = 0;

	// Query sample-rate.
	virtual int32_t MP_STDCALL getSampleRate( float& returnSampleRate ) = 0;

	// Each plugin instance has a host-assigned unique handle shared by UI and Audio class.
	virtual int32_t MP_STDCALL getHandle( int32_t& returnHandle ) = 0;

	// Identify Host. e.g. "SynthEdit".
	virtual int32_t MP_STDCALL getHostId( int32_t maxChars, wchar_t* returnString ) = 0;

	// Query Host version number. e.g. returnValue of 400000 is Version 4.0
	virtual int32_t MP_STDCALL getHostVersion( int32_t& returnVersion ) = 0;

	// Query Host registered user name. "John Smith"
	virtual int32_t MP_STDCALL getRegisteredName( int32_t maxChars, wchar_t* returnName ) = 0;

	// Provide list of audio pins.
	virtual int32_t MP_STDCALL createPinIterator(gmpi::IMpPinIterator** returnIterator) = 0;

	// Determin if multiple parallel instances (clones) in use. Clones share same control data and GUI.
	virtual int32_t MP_STDCALL isCloned( int32_t* returnValue ) = 0;

	// Provide list of 'clones' - plugins working in parallel to process multiple channels of audio, controled by same parameters/UI. 
	virtual int32_t MP_STDCALL createCloneIterator( void** returnInterface ) = 0;

	// Provide named shared memory, primarily for sharing waveforms and lookup tables between multiple instances.
	// Use sampleRate of -1 to indicate data is independant of samplerate.
	virtual int32_t MP_STDCALL allocateSharedMemory( const wchar_t* id, void** returnPointer, float sampleRate, int32_t size, int32_t& returnNeedInitialise ) = 0;

	// SynthEdit-specific.  Determine file's location depending on host application's conventions. // e.g. "bell.wav" -> "C:/My Documents/bell.wav"
	virtual int32_t MP_STDCALL resolveFilename( const wchar_t* shortFilename, int32_t maxChars, wchar_t* returnFullFilename ) = 0;

	// SynthEdit-specific.  Determine file's location depending on host application's conventions. // e.g. "bell.wav" -> "C:/My Documents/bell.wav"
	// DEPRECATED: see IEmbeddedFileSupport
	virtual int32_t MP_STDCALL openProtectedFile( const wchar_t* shortFilename, IProtectedFile **file ) = 0;
};

class IGmpiHost : public IMpUnknown
{
public:
	// Plugin sending out control data.
	virtual int32_t MP_STDCALL setPin( int32_t blockRelativeTimestamp, int32_t pinId, int32_t size, const void* data ) = 0;

	// Plugin audio output start/stop (silence detection).
	virtual int32_t MP_STDCALL setPinStreaming( int32_t blockRelativeTimestamp, int32_t pinId, bool isStreaming ) = 0;

	// PDC (Plugin Delay Compensation) support.
	virtual int32_t MP_STDCALL setLatency( int32_t latency ) = 0;

	// Plugin indicates no processing needed until input state changes.
	virtual int32_t MP_STDCALL sleep() = 0;

	// Query audio buffer size.
	virtual int32_t MP_STDCALL getBlockSize() = 0;

	// Query sample-rate.
	virtual float MP_STDCALL getSampleRate() = 0;

	// Each plugin instance has a host-assigned unique handle shared by UI and Audio class.
	virtual int32_t MP_STDCALL getHandle() = 0;
};

// GUID for IGmpiHost.
// {87CCD426-71D7-414E-A9A6-5ADCA81C7420}
static const MpGuid MP_IID_PROCESSOR_HOST =
{ 0x87ccd426, 0x71d7, 0x414e, { 0xa9, 0xa6, 0x5a, 0xdc, 0xa8, 0x1c, 0x74, 0x20 } };


// SynthEdit-specific.
// extension to GMPI to provide support for loading files from the plugins resources (be they embedded or file-based).
// Corrects error in IMpHost that there is a memory leak, due to having no way to free the file object, and no way to query file object for updated interfaces.
class IEmbeddedFileSupport : public IMpUnknown
{
public:
	// Determine file's location depending on host application's conventions. // e.g. "bell.wav" -> "C:/My Documents/bell.wav"
	virtual int32_t MP_STDCALL resolveFilename(const char* fileName, gmpi::IString* returnFullUri) = 0;

	// open a file, usually returns a IProtectedFile2 interface.
	virtual int32_t MP_STDCALL openUri(const char* fullUri, gmpi::IMpUnknown** returnStream ) = 0;
};

// GUID for IEmbeddedFileSupport.
// {B486F4DE-9010-4AA0-9D0C-DCD9F8879257}
static const MpGuid MP_IID_HOST_EMBEDDED_FILE_SUPPORT =
{ 0xb486f4de, 0x9010, 0x4aa0, { 0x9d, 0xc, 0xdc, 0xd9, 0xf8, 0x87, 0x92, 0x57 } };

// SynthEdit-specific.
class ISharedBlob : public IMpUnknown
{
public:
	virtual int32_t MP_STDCALL read(uint8_t** returnData, int64_t* returnSize) = 0;

	// {770D50E5-796D-4495-9C3E-6C21EBEA7F72}
	inline static const MpGuid guid =
	{ 0x770d50e5, 0x796d, 0x4495, { 0x9c, 0x3e, 0x6c, 0x21, 0xeb, 0xea, 0x7f, 0x72 } };
};


// GUI PLUGIN

// UI support split into two parts:
//  -Parameter and Patch Management (IMpUserInterfaceHost, IMpUserInterface)
//  -Optional Platform-specific graphics (ISeGraphicsHostComposited, IMpGraphicsSynthEdit etc)

class IMpUserInterface : public IMpUnknown
{
public:
	// Object created and connections established.
	virtual int32_t MP_STDCALL initialize() = 0;

	// Host setting a pin due to patch-change or automation.
	virtual int32_t MP_STDCALL setPin( int32_t pinId, int32_t voice, int32_t size, void* data ) = 0;

	// Host indicates UI should act on pin update.
	virtual int32_t MP_STDCALL notifyPin( int32_t pinId, int32_t voice ) = 0;

	// 'Backdoor' to audio class.  Not recommended.
	virtual int32_t MP_STDCALL receiveMessageFromAudio( int32_t id, int32_t size, void* messageData ) = 0;

	// Regular timer for driving animation.
	//virtual int32_t MP_STDCALL onIdleTimer() = 0;

	// SynthEdit-specific.  Populate context (right-click) menu. see IMpUserInterfaceHost.addContextMenuItem()
	virtual int32_t MP_STDCALL onCreateContextMenu() = 0;

	virtual int32_t MP_STDCALL onContextMenu( int32_t selection ) = 0;
};

// GUID for IMpUserInterface
// {CACB3203-4A57-4209-9E00-0C4B950457F6}
static const MpGuid MP_IID_GUI_PLUGIN =
{ 0xcacb3203, 0x4a57, 0x4209, { 0x9e, 0x0, 0xc, 0x4b, 0x95, 0x4, 0x57, 0xf6 } };


class DECLSPEC_NOVTABLE IMpContextItemSink : public gmpi::IMpUnknown
{
public:
	virtual int32_t MP_STDCALL AddItem(const char* text, int32_t id, int32_t flags = 0) = 0;
};

// GUID for IMpContextItemSink
// {BC152E7E-7FB8-4921-84EE-BED7CFD9A897}
static const gmpi::MpGuid MP_IID_CONTEXT_ITEMS_SINK =
{ 0xbc152e7e, 0x7fb8, 0x4921, { 0x84, 0xee, 0xbe, 0xd7, 0xcf, 0xd9, 0xa8, 0x97 } };


class IMpUserInterface2 : public IMpUnknown
{
public:
	// Establish connection to host.
	virtual int32_t MP_STDCALL setHost( gmpi::IMpUnknown* host ) = 0;

	// Object created and connections established.
	virtual int32_t MP_STDCALL initialize() = 0;

	// Host setting a pin due to patch-change or automation.
	virtual int32_t MP_STDCALL setPin( int32_t pinId, int32_t voice, int32_t size, const void* data ) = 0;

	/* can't see need for two-stage method. UPATE: was needed, see IMpUserInterface2B
	   This is now difficult becuase simply setting pin does not trigger notification (unless it changed).
	// Host indicates UI should act on pin update. (difficulty was triggering updates to zeroed pins).
	virtual int32_t MP_STDCALL notifyPin( int32_t pinId, int32_t voice ) = 0;
	*/

	// 'Backdoor' to audio class.  Not recommended.
	virtual int32_t MP_STDCALL receiveMessageFromAudio( int32_t id, int32_t size, const void* messageData ) = 0;

	// Integration with host's context (right-click) menu.
	virtual int32_t MP_STDCALL populateContextMenu( float x, float y, gmpi::IMpUnknown* contextMenuItemsSink ) = 0;
	virtual int32_t MP_STDCALL onContextMenu( int32_t selection ) = 0;

	virtual int32_t MP_STDCALL getToolTip(float x, float y, gmpi::IMpUnknown* returnToolTipString) = 0;
};

// GUID for IMpUserInterface2
// {9CD06775-F5C2-4321-812D-0958153294B8}
static const MpGuid MP_IID_GUI_PLUGIN2 =
{ 0x9cd06775, 0xf5c2, 0x4321, { 0x81, 0x2d, 0x9, 0x58, 0x15, 0x32, 0x94, 0xb8 } };

class IMpUserInterface2B : public IMpUserInterface2
{
public:
	// During initialisation it's nesc to notify all input pins.
	// setPin() will not do so in the case where the pin is already set to that value by default (i.e. zero).
	// A second reason is self-notification, which needs to happen only on input pins only, when the module updates them.
	// Given that the module can only detect input pins though costly iteration, better that host handles this case.
	virtual int32_t MP_STDCALL notifyPin(int32_t pinId, int32_t voice) = 0;
};

// GUID for IMpUserInterface2B
// {21B0EC11-6B66-4B72-928E-77E93A31A628}
static const MpGuid MP_IID_GUI_PLUGIN2B =
{ 0x21b0ec11, 0x6b66, 0x4b72,{ 0x92, 0x8e, 0x77, 0xe9, 0x3a, 0x31, 0xa6, 0x28 } };


enum MpContextMenuFlags { MI_NONE, MI_CHECKED, MI_SUB_MENU_BEGIN, MI_SUB_MENU_END, MI_SUB_MENU_SEPARATOR, MI_GRAYED };


// Generic UI host features (graphics independant).
class IMpUserInterfaceHost : public IMpUnknown
{
public:
	// Plugin UI updates a parameter.
	virtual int32_t MP_STDCALL pinTransmit( int32_t pinId, int32_t size, /*const*/ void* data, int32_t voice = 0 ) = 0;

	// Backdoor to Audio class. Not recommended. Use Parameters instead to support proper automation.
	virtual int32_t MP_STDCALL sendMessageToAudio( int32_t id, int32_t size, /*const*/ void* messageData ) = 0;

	// Start/Stop regular idle timer calls to plugin UI. Deprecated. Don't use.
	virtual int32_t MP_STDCALL setIdleTimer( int32_t active ) = 0;

	// Each plugin instance has unique handle shared by UI and Audio class.
	virtual int32_t MP_STDCALL getHandle( int32_t& returnValue ) = 0;

	// Identify Host. e.g. "SynthEdit"
	virtual int32_t MP_STDCALL getHostId( int32_t maxChars, wchar_t* returnString ) = 0;

	// Query Host version number. e.g. returnValue of 400000 is Version 4.0
	virtual int32_t MP_STDCALL getHostVersion( int32_t& returnValue ) = 0;

	// Get list of UI's pins.
	virtual int32_t MP_STDCALL createPinIterator(gmpi::IMpPinIterator** returnIterator) = 0;

	// SynthEdit-specific.  Determine file's location depending on host application's conventions. // e.g. "bell.wav" -> "C:/My Documents/bell.wav"
	virtual int32_t MP_STDCALL resolveFilename( const wchar_t* shortFilename, int32_t maxChars, wchar_t* returnFullFilename ) = 0;

	// SynthEdit-specific.  Add item to context (right-click) menu in reponse to onCreateContextMenu().
	// index must be between 0 - 1000.
	virtual int32_t MP_STDCALL addContextMenuItem( /*const*/ wchar_t* menuText, int32_t index, int32_t flags = MI_NONE ) = 0;

	// SynthEdit-specific.  Get count of UI's pins. Usefull with autoduplicate pins. UPDATE: Not suitable for autoduplicating pins becuase it's not avail in constructor when we need to init pins. open() is too late.
	virtual int32_t MP_STDCALL getPinCount( int32_t& returnCount ) = 0;

	// SynthEdit-specific.  Determine file's location depending on host application's conventions. // e.g. "bell.wav" -> "C:/My Documents/bell.wav"
	// DEPRECATED: see IMpUserInterfaceHost2
	virtual int32_t MP_STDCALL openProtectedFile( const wchar_t* shortFilename, IProtectedFile **file ) = 0;
};

// GUID for IMpUserInterfaceHost
// {174F9DDC-1850-40db-BAF0-E0B41D079645}
static const MpGuid MP_IID_UI_HOST =
{ 0x174f9ddc, 0x1850, 0x40db, { 0xba, 0xf0, 0xe0, 0xb4, 0x1d, 0x7, 0x96, 0x45 } };


// Generic UI host features (graphics independent).
class IMpUserInterfaceHost2 : public IMpUnknown
{
public:
	// Plugin UI updates a pin.
	virtual int32_t MP_STDCALL pinTransmit(int32_t pinId, int32_t size, const void* data, int32_t voice = 0) = 0;

	// Get information about UI's pins.
	virtual int32_t MP_STDCALL createPinIterator(gmpi::IMpPinIterator** returnIterator) = 0;

	// Each plugin instance has unique handle shared by UI and Audio class.
	virtual int32_t MP_STDCALL getHandle(int32_t& returnValue) = 0;

	// Backdoor to Audio class. Not recommended. Use Parameters instead to support proper automation.
	virtual int32_t MP_STDCALL sendMessageToAudio(int32_t id, int32_t size, const void* messageData) = 0;

	/* !!! TODO. split GMPI vs SynthEdit cleanly. !!!
class IMpUserInterfaceHostSynthEdit : public IMpUserInterfaceHost2
{
public:
	*/

	// SynthEdit-specific.  Locate resources and make SynthEdit imbedd them during save-as-vst.
	virtual int32_t MP_STDCALL ClearResourceUris() = 0;
	virtual int32_t MP_STDCALL RegisterResourceUri(const char* resourceName, const char* resourceType, gmpi::IString* returnString) = 0;
	virtual int32_t MP_STDCALL OpenUri(const char* fullUri, gmpi::IProtectedFile2** returnStream ) = 0;
	// Locate resources without imbedding them during save-as-vst.
	virtual int32_t MP_STDCALL FindResourceU(const char* resourceName, const char* resourceType, gmpi::IString* returnString) = 0;

	// Added 19/10/18 (SE 1.4 B271)
	virtual int32_t MP_STDCALL LoadPresetFile_DEPRECATED(const char* presetFilePath) = 0;
};

// GUID for IMpUserInterfaceHost2
// {DE009D92-E281-4F92-9996-200F49690028}
static const MpGuid MP_IID_UI_HOST2 =
{ 0xde009d92, 0xe281, 0x4f92, { 0x99, 0x96, 0x20, 0xf, 0x49, 0x69, 0x0, 0x28 } };

enum FieldType {
	MP_FT_VALUE
	, MP_FT_SHORT_NAME
	, MP_FT_LONG_NAME
	, MP_FT_MENU_ITEMS
	, MP_FT_MENU_SELECTION
	, MP_FT_RANGE_LO
	, MP_FT_RANGE_HI
	, MP_FT_ENUM_LIST
	, MP_FT_FILE_EXTENSION
	, MP_FT_IGNORE_PROGRAM_CHANGE
	, MP_FT_PRIVATE
	, MP_FT_AUTOMATION				// int
	, MP_FT_AUTOMATION_SYSEX			// STRING
	, MP_FT_DEFAULT					// same type as parameter
	, MP_FT_GRAB						// (mouse down) bool
	, MP_FT_NORMALIZED				// float
};

class IMpParameterObserver : public IMpUnknown
{
public:
	virtual int32_t MP_STDCALL setParameter(int32_t parameterHandle, int32_t fieldId, int32_t voice, const void* data, int32_t size) = 0;
};
// GUID for IMpParameterObserver.
// {38C033A3-A64C-42CA-A377-14A50816EE02}
static const MpGuid MP_IID_PARAMETER_OBSERVER =
{ 0x38c033a3, 0xa64c, 0x42ca,{ 0xa3, 0x77, 0x14, 0xa5, 0x8, 0x16, 0xee, 0x2 } };

// Controller interface.
class IMpController : public IMpParameterObserver
{
public:
	// Establish connection to host.
	virtual int32_t MP_STDCALL setHost(gmpi::IMpUnknown* host) = 0;
	// Patch-Manager -> plugin.
	virtual int32_t MP_STDCALL preSaveState() = 0;
	virtual int32_t MP_STDCALL open() = 0;

	// Pins.
	virtual int32_t MP_STDCALL setPinDefault(int32_t pinType, int32_t pinId, const char* defaultValue) = 0;
	virtual int32_t MP_STDCALL setPin(int32_t pinId, int32_t voice, int64_t size, const void* data) = 0;
	virtual int32_t MP_STDCALL notifyPin(int32_t pinId, int32_t voice) = 0;

	virtual int32_t MP_STDCALL onDelete() = 0;
};

// GUID for IMpController.
// {2FE42141-0F86-4582-AC4F-04386BD792DE}
static const MpGuid MP_IID_CONTROLLER =
{ 0x2fe42141, 0xf86, 0x4582, { 0xac, 0x4f, 0x4, 0x38, 0x6b, 0xd7, 0x92, 0xde } };

// Modules Controllers iterator current item. Should return various information about the module.
class IMpControllerIteratorItem : public IMpUnknown
{
public:
	// Return module controller class interface.
	virtual int32_t MP_STDCALL getController(gmpi::IMpController** returnController) = 0;
};

// GUID for IMpControllerIteratorCurrent.
// {DA18C4ED-3A4F-476B-A1B0-68E5435BC572}
static const MpGuid MP_IID_CONTROLLER_ITERATOR_ITEM =
{ 0xda18c4ed, 0x3a4f, 0x476b,{ 0xa1, 0xb0, 0x68, 0xe5, 0x43, 0x5b, 0xc5, 0x72 } };

class IMpControllerIterator : public IMpUnknown
{
public:
	virtual int32_t MP_STDCALL getCount() = 0;
	virtual int32_t MP_STDCALL first() = 0;
	virtual int32_t MP_STDCALL next() = 0;
	/*
	virtual int32_t MP_STDCALL getPinId(int32_t& returnValue) = 0;
	virtual int32_t MP_STDCALL getPinDirection(int32_t& returnValue) = 0;
	virtual int32_t MP_STDCALL getPinDatatype(int32_t& returnValue) = 0;
	*/
	virtual int32_t MP_STDCALL getCurrent(gmpi::IMpControllerIteratorItem** returnCurent) = 0;
};

// GUID for IMpControllerIterator.
// {FA03B47C-1C42-437E-AA4D-8F3AEEFC6BC3}
static const MpGuid MP_IID_CONTROLLER_ITERATOR =
{ 0xfa03b47c, 0x1c42, 0x437e,{ 0xaa, 0x4d, 0x8f, 0x3a, 0xee, 0xfc, 0x6b, 0xc3 } };

// Controller host interface.
class IMpControllerHost : public IMpUnknown
{
public:
	// Each plugin instance has a host-assigned unique handle shared by UI and Audio class.
	virtual int32_t MP_STDCALL getHandle(int32_t& returnHandle) = 0;
	virtual int32_t MP_STDCALL setParameter(int32_t parameterHandle, int32_t paramFieldType, int32_t voice, const void* data, int32_t size) = 0;
	virtual void updateParameter(int32_t parameterHandle, int32_t paramFieldType, int32_t voice) = 0;
	virtual int32_t getParameterHandle(int32_t moduleHandle, int32_t moduleParameterId) = 0;
	virtual int32_t getParameterModuleAndParamId(int32_t parameterHandle, int32_t* returnModuleHandle, int32_t* returnModuleParameterId) = 0;
	//?	virtual int32_t MP_STDCALL setPinDefault(int32_t pinIndex, int32_t paramFieldType, const char* utf8Value) = 0;
	virtual int32_t MP_STDCALL setLatency(int32_t latency) = 0;

	// Backdoor to Audio class. Not recommended. Use Parameters instead to support proper automation.
	//virtual int32_t MP_STDCALL sendMessageToAudio(int32_t id, int32_t size, const void* messageData) = 0;
	virtual int32_t MP_STDCALL createControllerIterator(gmpi::IMpControllerIterator** returnIterator) = 0;
	virtual int32_t MP_STDCALL pinTransmit(int32_t pinId, int32_t voice, int64_t size, const void* data) = 0;
};

// GUID for IMpControllerHost.
// {8BB0B474-D162-4A21-91A4-0AD2C8600EB2}
static const MpGuid MP_IID_CONTROLLER_HOST =
{ 0x8bb0b474, 0xd162, 0x4a21, { 0x91, 0xa4, 0xa, 0xd2, 0xc8, 0x60, 0xe, 0xb2 } };


// IMpFactory
// The factory creates plugin instances.

enum MpSubTypes{ MP_SUB_TYPE_AUDIO, MP_SUB_TYPE_GUI, MP_SUB_TYPE_GUI2, MP_SUB_TYPE_UNUSED, MP_SUB_TYPE_CONTROLLER };

class IMpFactory : public IMpUnknown
{
public:
	// Instantiate a plugin.
	virtual int32_t MP_STDCALL createInstance(
		const wchar_t* iid, int32_t subType, IMpUnknown* host, void** returnInterface ) = 0;

	// Allows host to display plugin's SDK version and compiler settings to user.
	virtual int32_t MP_STDCALL getSdkInformation( int32_t& returnSdkVersion, int32_t maxChars, wchar_t* returnCompilerInformation ) = 0;
};

// New, allow instansiation without having host pointer.
class IMpFactory2 : public IMpFactory
{
public:
	// Instantiate a plugin.
	virtual int32_t MP_STDCALL createInstance2(
		const wchar_t* iid, int32_t subType, void** returnInterface ) = 0;
};

class IMpShellFactory : public IMpFactory2
{
public:
	// Query a plugin's info.
	virtual int32_t MP_STDCALL getPluginIdentification(int32_t index, IMpUnknown* iReturnXml) = 0;	// ID and name only.
	virtual int32_t MP_STDCALL getPluginInformation(const wchar_t* iid, IMpUnknown* iReturnXml) = 0;		// Full pin details.
};

// adapted from GMPI SDK, keep in sync.
struct DECLSPEC_NOVTABLE GMPI_IPluginFactory : public IMpUnknown
{
	virtual int32_t createInstance(const char* id, int32_t subtype, void** returnInterface) = 0;
	virtual int32_t getPluginInformation(int32_t index, IString* returnXml) = 0;

	inline static const MpGuid guid = // {066C55EB-0EA8-4D73-A6F3-06D948D9E232}
	{ 0x66c55eb, 0xea8, 0x4d73, { 0xa6, 0xf3, 0x6, 0xd9, 0x48, 0xd9, 0xe2, 0x32 } };
};

// The DLL entrypoint signature.
typedef int32_t (MP_STDCALL *MP_DllEntry)(void**);


class IMpCloneIterator : public IMpUnknown
{
public:
	virtual int32_t MP_STDCALL first() = 0;
	virtual int32_t MP_STDCALL next(gmpi::IMpUnknown** returnInterface ) = 0;
};

// {D880BA72-8C7E-4D9E-92BA-644231E373C1}
static const MpGuid MP_IID_CLONE_ITERATOR = 
{ 0xd880ba72, 0x8c7e, 0x4d9e, { 0x92, 0xba, 0x64, 0x42, 0x31, 0xe3, 0x73, 0xc1 } };

} // namespace


#if defined(_WIN32) || defined(__FLAT__)
	#pragma pack(pop)
#elif defined __BORLANDC__
	#pragma -a-
#endif

#endif // C++ header.

#endif // MP_API.H INCLUDED

//===== mp_shared_ptr.h =====
#ifndef MP_SHARED_PTR_H_INCLUDED
#define MP_SHARED_PTR_H_INCLUDED

namespace gmpi_sdk
{
	struct selfInitializingInt
	{
		int32_t value_;

		selfInitializingInt(int32_t initalValue = 1) : value_(initalValue)
		{}
	};

	// Helper for managing lifetime of interface pointer
	template<class wrappedObjT>
	class mp_shared_ptr
	{
		wrappedObjT* obj;

	public:
		mp_shared_ptr() : obj(0)
		{
		}

		explicit mp_shared_ptr(wrappedObjT* newobj) : obj(0)
		{
			Assign(newobj);
		}
		mp_shared_ptr(const mp_shared_ptr<wrappedObjT>& value) : obj(0)
		{
			Assign(value.obj);
		}
		// Attach object without incrementing ref count. For objects created with new.
		inline void Attach(wrappedObjT* newobj)
		{
			wrappedObjT* old = obj;
			obj = newobj;

			if( old )
			{
				old->release();
			}
		}

		~mp_shared_ptr()
		{
			if( obj )
			{
				obj->release();
			}
		}
		inline operator wrappedObjT*( )
		{
			return obj;
		}
		const wrappedObjT* operator=( wrappedObjT* value )
		{
			Assign(value);
			return value;
		}
		mp_shared_ptr<wrappedObjT>& operator=( mp_shared_ptr<wrappedObjT>& value )
		{
			Assign(value.get());
			return *this;
		}
		bool operator==( const wrappedObjT* other )
		{
			return obj == other;
		}
		bool operator==( const mp_shared_ptr<wrappedObjT>& other )
		{
			return obj == other.obj;
		}
		inline wrappedObjT* operator->( ) const
		{
			return obj;
		}

		wrappedObjT*& get()
		{
			return obj;
		}

		wrappedObjT** getAddressOf()
		{
			assert(obj == 0); // Free it before you re-use it!
//			return reinterpret_cast<void**>(&obj);
			return &obj;
		}
		void** asIMpUnknownPtr()
		{
			assert(obj == 0); // Free it before you re-use it!
							  //			return reinterpret_cast<void**>(&obj);
			return reinterpret_cast<void**>(&obj);
		}

		bool isNull()
		{
			return obj == 0;
		}

	private:
		// Attach object and increment ref count.
		inline void Assign(wrappedObjT* newobj)
		{
			Attach(newobj);
			if( newobj )
			{
				newobj->addRef();
			}
		}
	};
/* hmm, not right yet
	template<class wrappedObjT>
	gmpi_sdk::mp_shared_ptr<wrappedObjT> make_shared_ptr(const wrappedObjT* rawPointer)
	{
		gmpi_sdk::mp_shared_ptr<wrappedObjT> wrapper;
		wrapper.Attach(rawPointer);
		return wrapper;
	}
	*/
}

#endif // MP_SHARED_PTR_H_INCLUDED

// end of API =====


