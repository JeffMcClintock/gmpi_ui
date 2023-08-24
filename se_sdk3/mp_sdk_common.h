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

#ifndef GMPI_SDK_REVISION
#define	GMPI_SDK_REVISION 30800 // 3.08
#endif

/* Version History
	 1/01/2007 - V3.00 : Official release.
	 2/10/2012 - V3.01 : Fixed bug in automatic sleep mode.
	11/10/2012 - V3.01 : Skips name-un-mangling on MP_GetFactory in 64-bit build.
	07/01/2013 - V3.02 : Removed dependence on memcmp().
	07/06/2013 - V3.03 : Ensure DSP output pins don't send random garbage at startup.
	07/06/2013 - V3.04 : SDK Now compiles on Mac OSX.
	24/10/2013 - V3.05 : Fix for Linux, include string.h for memcpy().
	14/11/2016 - V3.06 : New Graphics SDK Introduced.
	15/11/2016 - V3.07 : Fixed compatibility with GCC (Code-Blocks IDE).
	27/01/2017 - V3.08 : fix crash in SE 1.1 caused by DSP queryInterface.
*/

// To simplify plugin development. All the headers for the GMPI/SEM
// API and SDK are inlined together here. You need only include this one header to make a plugin.
#include <assert.h>
//#include <string.h> // for memcpy()
#include <string>	// for std::wstring
#include <stdint.h>
#include "gmpi_common.h"

//==== Globaly Unique Identifiers =====
#ifndef MP_SDK_GUID_H_INCLUDED
#define MP_SDK_GUID_H_INCLUDED

#if 0
namespace gmpi
{
	// GUID - Globally Unique Identifier, used to identify interfaces.
	struct MpGuid
	{
		uint32_t data1;
		uint16_t data2;
		uint16_t data3;
		unsigned char data4[8];
	};

	inline bool
	operator==( const gmpi::MpGuid& left, const gmpi::MpGuid& right )
	{
		if (left.data1 != right.data1 || left.data2 != right.data2 || left.data3 != right.data3)
			return false;

		for (size_t i = 0 ; i < sizeof(left.data4) ; ++i)
		{
			if (left.data4[i] != right.data4[i])
				return false;
		}

		return true;
	}
}
#endif

#endif // MP_SDK_GUID_H_INCLUDED

//==== API and Interfaces =====

#ifndef MP_API_H_INCLUDED
#define MP_API_H_INCLUDED

// Platform specific definitions.
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include "Windows.h"
//#define MP_PLATFORM_WIN32	1
//#define MP_STDCALL		__stdcall
#endif

#if 0


#if defined(__APPLE__)
#define MP_PLATFORM_MACOSX	1
#define MP_STDCALL
#endif


#if defined(__linux__)
//#define MP_PLATFORM_LINUX	1
#if defined(__i386__)
#define MP_STDCALL		__attribute__((stdcall))
#else
#define MP_STDCALL
#endif
#endif

#endif

#if defined __BORLANDC__
	#pragma -a8
#elif defined(_WIN32) || defined(__FLAT__) || defined (CBUILDER)
	#pragma pack(push,8)
#endif

#if 0

#ifndef DECLSPEC_NOVTABLE
#if (_MSC_VER >= 1100) && defined(__cplusplus)
#define DECLSPEC_NOVTABLE   __declspec(novtable)
#else
#define DECLSPEC_NOVTABLE
#endif
#endif

// Handy macro to save typing.
#define GMPI_QUERYINTERFACE1( INTERFACE_IID, CLASS_NAME ) \
	int32_t MP_STDCALL queryInterface(const gmpi::MpGuid& iid, void** returnInterface) override \
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
	int32_t MP_STDCALL addRef() override \
{ \
	return ++refCount2_; \
} \
	int32_t MP_STDCALL release() override \
{ \
	if (--refCount2_ == 0) \
	{ \
	delete this; \
	return 0; \
	} \
	return refCount2_; \
} \

#define GMPI_REFCOUNT_NO_DELETE	\
	int32_t MP_STDCALL addRef() override \
{ \
	return 1; \
} \
	int32_t MP_STDCALL release() override \
{ \
	return 1; \
} \

#define GMPI_QUERYINTERFACE2( INTERFACE_IID, CLASS_NAME, BASE_CLASS ) \
	int32_t MP_STDCALL queryInterface(const gmpi::MpGuid& iid, void** returnInterface) override \
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

#endif

namespace gmpi
{
	// Utility
	//std::wstring Utf8ToWstringDllSafe(const std::string p_string);
	//std::string WStringToUtf8DllSafe(const std::wstring& p_cstring);

#if 0
// All methods return an error code.
// An int32_t may take one of these values.
enum
{
	MP_HANDLED = 1,				// Success. In case of GUI - no further handing required.
	MP_OK = 0,					// Success. In case of GUI indicates successful mouse "hit".
	MP_FAIL			= -1,		// General failure.
	MP_UNHANDLED	= -1,		// Event not handled.
	MP_NOSUPPORT = -2,			// Interface not supported.
	MP_CANCEL = -3,				// Async operation cancelled.
};

// Silence detection - Audio pins may be in either of two states.
enum MpPinStatus{PIN_STATIC=1, PIN_STREAMING};

// Timestamped events.
enum MpEventType{EVENT_PIN_SET = 100, EVENT_PIN_STREAMING_START, EVENT_PIN_STREAMING_STOP, EVENT_MIDI, EVENT_GRAPH_START};

struct MpEvent
{
	int32_t timeDelta;	// Relative to block.
	int32_t eventType;	// See MpEventType enumeration.
	int32_t parm1;		// Pin index if needed.
	int32_t parm2;		// Sizeof additional data. >4 implies extraData points to value.
	int32_t parm3;		// Pin value (if 4 bytes or less).
	int32_t parm4;		// Voice ID.
	char* extraData;	// Additional data.
	MpEvent* next;		// Next event in list.
};

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

// GUID for IMpUnknown - {00000000-0000-C000-0000-000000000046}
static const MpGuid MP_IID_UNKNOWN =
{
	0x00000000, 0x0000, 0xC000,
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}
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

	// 'Backdoor' to UI class.  Not recommended.  WARNING: Called outside scope of Process function, you can't set pins or send MIDI during this call.
	virtual int32_t MP_STDCALL receiveMessageFromGui( int32_t id, int32_t size, void* messageData ) = 0;
};

// GUID for IMpPlugin  - {2B0DFC7E-A539-49cd-B72D-32FF9DF0D9E4}
static const MpGuid MP_IID_PLUGIN =
{ 0x2b0dfc7e, 0xa539, 0x49cd, { 0xb7, 0x2d, 0x32, 0xff, 0x9d, 0xf0, 0xd9, 0xe4 } };

// Helper for old hosts calling new-style plugins and vica-versa.
class IMpLegacyInitialization : public IMpUnknown
{
public:
	virtual int32_t MP_STDCALL setHost(gmpi::IMpUnknown* host) = 0;
};

// GUID for IMpLegacyInitialization  - {0B471002-5627-4D46-984E-C0DC79D0AD35}
static const MpGuid MP_IID_LEGACY_INITIALIZATION =
{ 0xb471002, 0x5627, 0x4d46, { 0x98, 0x4e, 0xc0, 0xdc, 0x79, 0xd0, 0xad, 0x35 } };

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

	// 'Backdoor' to UI class.  Not recommended.  WARNING: Called outside scope of Process function, you can't set pins or send MIDI during this call.
	virtual int32_t MP_STDCALL receiveMessageFromGui( int32_t id, int32_t size, const void* messageData ) = 0;
};


// GUID for IMpPlugin2
static const MpGuid MP_IID_PLUGIN2 =
// {1E07E3E8-8118-457F-A63C-D4F282A0F519}
{ 0x1e07e3e8, 0x8118, 0x457f, { 0xa6, 0x3c, 0xd4, 0xf2, 0x82, 0xa0, 0xf5, 0x19 } };

// Music plugin audio processing interface. simplified.
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
// TODO make a new iterator that can access pin ID, don't use this faulty version in GMPI.
class IMpPinIterator : public IMpUnknown
{
public:
	virtual int32_t MP_STDCALL getCount( int32_t &returnCount ) = 0;
	virtual int32_t MP_STDCALL first() = 0;
	virtual int32_t MP_STDCALL next() = 0;

	virtual int32_t MP_STDCALL getPinId( int32_t& returnValue ) = 0; // returns *index* not ID.
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
	// Backward compatibility,
	inline int32_t createPinIterator(void** returnIterator)
	{
		return createPinIterator((gmpi::IMpPinIterator**) returnIterator);
	}

	// Determin if multiple parallel instances (clones) in use. Clones share same control data and GUI.
	virtual int32_t MP_STDCALL isCloned( int32_t* returnValue ) = 0;

	// Provide list of 'clones' - plugins working in parallel to process multiple channels of audio, controled by same parameters/UI.
	virtual int32_t MP_STDCALL createCloneIterator( void** returnInterface ) = 0;

	// Provide named shared memory, primarily for sharing waveforms and lookup tables between multiple instances.
	// Use sampleRate of -1 to indicate data is independant of samplerate.
	virtual int32_t MP_STDCALL allocateSharedMemory( const wchar_t* id, void** returnPointer, float sampleRate, int32_t size, int32_t& returnNeedInitialise ) = 0;

	// SynthEdit-specific.  Determine file's location depending on host application's conventions. // e.g. "bell.wav" -> "C:/My Documents/bell.wav"
	virtual int32_t MP_STDCALL resolveFilename( const wchar_t* shortFilename, int32_t maxChars, wchar_t* returnFullFilename ) = 0;

	// DEPRECATED ref IEmbeddedFileSupport
	virtual int32_t MP_STDCALL openProtectedFile( const wchar_t* shortFilename, IProtectedFile **file ) = 0;
};

// GUID for IMpHost.
// {4F1B532F-3C46-4927-A498-614867425BE7}
static const MpGuid MP_IID_HOST =
{ 0x4f1b532f, 0x3c46, 0x4927, { 0xa4, 0x98, 0x61, 0x48, 0x67, 0x42, 0x5b, 0xe7 } };

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
	virtual int32_t MP_STDCALL openUri(const char* fullUri, gmpi::IMpUnknown** returnStream) = 0;
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

#endif

class DECLSPEC_NOVTABLE IMpContextItemSink : public gmpi::IMpUnknown
{
public:
	virtual int32_t MP_STDCALL AddItem(const char* text, int32_t id, int32_t flags = 0) = 0;
};

// GUID for IMpContextItemSink
// {BC152E7E-7FB8-4921-84EE-BED7CFD9A897}
static const gmpi::MpGuid MP_IID_CONTEXT_ITEMS_SINK =
{ 0xbc152e7e, 0x7fb8, 0x4921, { 0x84, 0xee, 0xbe, 0xd7, 0xcf, 0xd9, 0xa8, 0x97 } };

#if 0

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

#endif

// Note this is not for new GUI system.
enum MpContextMenuFlags { MI_NONE, MI_CHECKED, MI_SUB_MENU_BEGIN, MI_SUB_MENU_END, MI_SUB_MENU_SEPARATOR, MI_GRAYED };


#if 0

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
	// Backward compatibility,
	inline int32_t createPinIterator(void** returnIterator)
	{
		return createPinIterator((gmpi::IMpPinIterator**) returnIterator);
	}

	// SynthEdit-specific.  Determine file's location depending on host application's conventions. // e.g. "bell.wav" -> "C:/My Documents/bell.wav"
	virtual int32_t MP_STDCALL resolveFilename( const wchar_t* shortFilename, int32_t maxChars, wchar_t* returnFullFilename ) = 0;

	// SynthEdit-specific.  Add item to context (right-click) menu in reponse to onCreateContextMenu().
	// index must be between 0 - 1000.
	virtual int32_t MP_STDCALL addContextMenuItem( /*const*/ wchar_t* menuText, int32_t index, int32_t flags = MI_NONE ) = 0;

	// SynthEdit-specific.  Get count of UI's pins. Usefull with autoduplicate pins. UPDATE: Not suitable for autoduplicating pins becuase it's not avail in constructor when we need to init pins. open() is too late.
	virtual int32_t MP_STDCALL getPinCount( int32_t& returnCount ) = 0;

	// SynthEdit-specific.  Determine file's location depending on host application's conventions. // e.g. "bell.wav" -> "C:/My Documents/bell.wav"
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
	// Backward compatibility,
	inline int32_t createPinIterator(void** returnIterator)
	{
		return createPinIterator((gmpi::IMpPinIterator**) returnIterator);
	}

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
	//	virtual int32_t setParameter(int32_t parameterHandle, int32_t moduleFieldId, const void* data, int32_t size, int32_t voice) = 0;
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
	// plugin -> Patch-Manager.
	virtual int32_t MP_STDCALL setParameter( int32_t parameterHandle, int32_t paramFieldType, int32_t voice, const void* data, int32_t size) = 0;
//	virtual int32_t MP_STDCALL getParameter( int32_t paramId, int32_t paramFieldType, int32_t voice, gmpi::IString* value) = 0;
	virtual void updateParameter(int32_t paramId, int32_t paramFieldType, int32_t voice) = 0;
	virtual int32_t getParameterHandle(int32_t moduleHandle, int32_t moduleParameterId) = 0;
	virtual int32_t getParameterModuleAndParamId(int32_t parameterHandle, int32_t* returnModuleHandle, int32_t* returnModuleParameterId) = 0;
	//? why	paramFieldType? virtual int32_t MP_STDCALL setPinDefault( int32_t pinIndex, int32_t paramFieldType, const char* utf8Value) = 0;
	virtual int32_t MP_STDCALL setLatency(int32_t latency) = 0;

	// Backdoor to Audio class. Not recommended. Use Parameters instead to support proper automation.
	//virtual int32_t MP_STDCALL sendMessageToAudio(int32_t id, int32_t size, const void* messageData) = 0;
	virtual int32_t MP_STDCALL createControllerIterator(gmpi::IMpControllerIterator** returnIterator) = 0;
	/*
		// Example. Note: From open(), controller won't see itself as it's not added yet.
		gmpi_sdk::mp_shared_ptr<gmpi::IMpControllerIterator> it;
		gmpi::IMpControllerIteratorItem* item;
		if (gmpi::MP_OK == getHost()->createControllerIterator(it.getAddressOf()))
		{
			it->first();
			while (it->getCurrent(&item) == gmpi::MP_OK)
			{
				gmpi::IMpController* controller;
				item->getController(&controller);

				// Do something here.

				it->next();
			}
		}
	*/

	virtual int32_t MP_STDCALL pinTransmit(int32_t pinId, int32_t voice, int64_t size, const void* data) = 0;
};

// GUID for IMpControllerHost.
// {8BB0B474-D162-4A21-91A4-0AD2C8600EB2}
static const MpGuid MP_IID_CONTROLLER_HOST =
{ 0x8bb0b474, 0xd162, 0x4a21, { 0x91, 0xa4, 0xa, 0xd2, 0xc8, 0x60, 0xe, 0xb2 } };


// IMpFactory
// The factory creates plugin instances.

enum MpSubTypes { MP_SUB_TYPE_AUDIO, MP_SUB_TYPE_GUI, MP_SUB_TYPE_GUI2, MP_SUB_TYPE_UNUSED, MP_SUB_TYPE_CONTROLLER };

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

// GUID for IMpFactory
// {9A7904B7-7F7F-4d61-B558-C1A6695BB638}
static const MpGuid MP_IID_FACTORY =
{ 0x9a7904b7, 0x7f7f, 0x4d61, { 0xb5, 0x58, 0xc1, 0xa6, 0x69, 0x5b, 0xb6, 0x38 } };

// GUID for IMpFactory2 (plain constructor instansiation).
// {91C3D2EE-C701-4F35-91A6-094DC7E05976}
static const struct MpGuid MP_IID_FACTORY2 =
{ 0x91c3d2ee, 0xc701, 0x4f35,{ 0x91, 0xa6, 0x9, 0x4d, 0xc7, 0xe0, 0x59, 0x76 } };

// GUID for IMpShellFactory.
// {72148962-F9C9-4DB1-9F71-EB80CD6469FC}
static const struct MpGuid MP_IID_SHELLFACTORY =
{ 0x72148962, 0xf9c9, 0x4db1,{ 0x9f, 0x71, 0xeb, 0x80, 0xcd, 0x64, 0x69, 0xfc } };


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

#endif

} // namespace


#if defined(_WIN32) || defined(__FLAT__)
	#pragma pack(pop)
#elif defined __BORLANDC__
	#pragma -a-
#endif

#endif // MP_API.H INCLUDED
// end of API =====


#ifndef MP_SDK_FACTORY_H_INCLUDED
#define MP_SDK_FACTORY_H_INCLUDED

/* moved. MSVC LTCG fail.
class MpString : public gmpi::IString
{
	std::string cppString;

public:
	MpString();
	MpString(const std::string& other) : cppString(other)
	{
	}
	MpString(const char* pData, int32_t pSize) : cppString(pData, pSize)
	{
	}
	~MpString();
	virtual int32_t MP_STDCALL setData(const char* pData, int32_t pSize) override;
	virtual int32_t MP_STDCALL getSize();// override;
	virtual const char* MP_STDCALL getData();// override;

	gmpi::IMpUnknown* getUnknown();

	const char* c_str();

	const std::string& str();

	GMPI_QUERYINTERFACE1(gmpi::MP_IID_RETURNSTRING, gmpi::IString);
	GMPI_REFCOUNT_NO_DELETE;
};

// Presents string or data allocated independantly.
class StringView : public gmpi::IString
{
	const char* data;
	int size;
public:
	StringView(const char* pData, int pSize) :
		data(pData)
		, size(pSize)
	{};
	virtual int32_t MP_STDCALL setData(const char* pData, int32_t pSize)// override
	{
		return gmpi::MP_FAIL;
	}
	virtual int32_t MP_STDCALL getSize()// override
	{
		return size;
	}
	virtual const char* MP_STDCALL getData()
	{
		return data;
	}

	gmpi::IMpUnknown* getUnknown()
	{
		return static_cast<gmpi::IMpUnknown*>( this );
	}

	GMPI_QUERYINTERFACE1(gmpi::MP_IID_RETURNSTRING, gmpi::IString);
	GMPI_REFCOUNT_NO_DELETE;
};
*/

#if 0
// Plugin factory - holds a list of the plugins available in this dll. Creates plugin instances on request.

// type of function to instantiate a plugin component. New-style, no host ptr.
typedef class gmpi::IMpUnknown* ( *MP_CreateFunc2 )();

// type of function to instantiate a plugin audio object.
typedef class gmpi::IMpPlugin* ( *MP_PluginCreateFunc )( class gmpi::IMpUnknown* host );

// type of function to instantiate a plugin UI object.
typedef class gmpi::IMpUserInterface* ( *MP_GuiPluginCreateFunc )( class gmpi::IMpUnknown* host );

// New generic object register.
int32_t RegisterPlugin(int subType, const wchar_t* uniqueId, MP_CreateFunc2 create);
int32_t RegisterPluginWithXml(int subType, const char* xml, MP_CreateFunc2 create);

// Parse plugin XML to describe one or more plugins with the factory. Deprecated.
int32_t RegisterPluginXml( const char* xmlFile );

// macro for generating unique variable or function name.
#define PASTE_FUNC2(x,y,z) x##y##z
#define PASTE_FUNC1(x,y,z) PASTE_FUNC2(x,y,z)
#define PASTE_FUNC(x,y) PASTE_FUNC1(x,y,__LINE__)

// Old way using Macro. macros are evil.
#define GMPI_REGISTER( plugintype, className, pluginId ) namespace{ gmpi::IMpUnknown* PASTE_FUNC(create,className)(){ return static_cast<gmpi::IMpUnknown*> (new className()); }; int32_t PASTE_FUNC(r,className) = RegisterPlugin( plugintype, pluginId, &PASTE_FUNC(create,className) );}
#endif

// Ensure linker includes file in static-library. See also INIT_STATIC_FILE in UgDatabase.cpp
#ifndef SE_DECLARE_INIT_STATIC_FILE
#define SE_DECLARE_INIT_STATIC_FILE(filename) void se_static_library_init_##filename(){}
#endif

// Helper class to make registering concise.
/* e.g.
using namespace gmpi;

namespace
{
	auto r = Register< Oscillator >::withId(L"MY Oscillator");
}

*/


#endif	// MP_SDK_FACTORY_H_INCLUDED

#ifndef MP_SDK_PIN_TYPES_H_INCLUDED
#define MP_SDK_PIN_TYPES_H_INCLUDED

// BLOB - Binary datatype.
struct MpBlob
{
	MpBlob() {}
	// copy constructor. supports use in standard containers.
	MpBlob( const MpBlob& other )
	{
		Init(other.size_, other.data_);
	}
	// contruct from raw data.
	MpBlob(int size, const void* data)
	{
		Init((size_t) size, data);
	}
	MpBlob(size_t size, const void* data)
	{
		Init(size, data);
	}
	~MpBlob()
	{
		delete[] data_;
	}
	void setValueRaw(size_t size, const void* data)
	{
		if (size_ != size)
		{
			delete[] data_;
			size_ = size;
			data_ = new char[size_];
		}

		if (size_ > 0)
		{
			memcpy(data_, data, size_);
		}
	}
	inline void setValueRaw(int size, const void* data)
	{
		setValueRaw((size_t)size, data);
	}
	const MpBlob &operator=( const MpBlob& other )
	{
		setValueRaw(other.size_, other.data_);
		return other;
	}

	bool operator==(const MpBlob& other) const
	{
		if (size_ != other.size_)
			return false;

		for (size_t i = 0; i < size_; ++i)
		{
			if (data_[i] != other.data_[i])
				return false;
		}
		return true;
	}
	bool operator!=(const MpBlob& other)
	{
		return !operator==(other);
	}
	bool compare(char* data, int size)
	{
		if (size_ != static_cast<size_t>(size))
			return false;

		for (size_t i = 0; i < size_; ++i)
		{
			if (data_[i] != data[i])
				return false;
		}
		return true;
	}
	int32_t getSize() const
	{
		return (int32_t)size_;
	}

	char* getData() const
	{
		return data_;
	}

	void resize( int size )
	{
		if (size_ < static_cast<size_t>(size))
		{
			delete[] data_;
			if (size > 0)
			{
				data_ = new char[static_cast<size_t>(size)];
			}
			else
			{
				data_ = 0;
			}
		}

		size_ = static_cast<size_t>(size);
	}

private:
	inline void Init(size_t size, const void* data)
	{
		size_ = size;
		if (size_ > 0)
		{
			data_ = new char[size_];
			memcpy(data_, data, size_);
		}
		else
		{
			data_ = nullptr;
		}
	}

	size_t size_ = {};
	char* data_ = {};
};

template <typename T>
class MpTypeTraits
{
private:
	// convert type to int representing datatype. N is dummy to satisfy partial specialization rules enforced by GCC.
	template <class U, int N> struct PinDataTypeTraits
	{
	};
	template<int N> struct PinDataTypeTraits<int32_t, N>
	{
		enum { result = gmpi::MP_INT32 };
	};
	template<int N> struct PinDataTypeTraits<int64_t, N>
	{
		enum { result = gmpi::MP_INT64 };
	};
	template<int N> struct PinDataTypeTraits<bool,N>
	{
		enum { result = gmpi::MP_BOOL };
	};
	template<int N> struct PinDataTypeTraits<float, N>
	{
		enum { result = gmpi::MP_FLOAT32 };
	};
	template<int N> struct PinDataTypeTraits<double, N>
	{
		enum { result = gmpi::MP_FLOAT64 };
	};
	template<int N> struct PinDataTypeTraits<std::wstring,N>
	{
		enum { result = gmpi::MP_STRING };
	};
	template<int N> struct PinDataTypeTraits<std::string,N>
	{
		enum { result = gmpi::MP_STRING_UTF8 };
	};
	template<int N> struct PinDataTypeTraits<MpBlob, N>
	{
		enum { result = gmpi::MP_BLOB };
	};

public:
	enum{ PinDataType = PinDataTypeTraits<T,0>::result };
};


// Get size of variable's data.
template <typename T>
inline int variableRawSize( const T& /*value*/ )
{
	return sizeof(T);
}

template<>
inline int variableRawSize<std::wstring>( const std::wstring& value )
{
	return (int) sizeof(wchar_t) * (int) value.length();
}

template<>
inline int variableRawSize<std::string>( const std::string& value )
{
	return static_cast<int>(value.size());
}

template<>
inline int variableRawSize<MpBlob>( const MpBlob& value )
{
	return value.getSize();
}

// Serialize variable's value as bytes.
template <typename T>
inline void* variableRawData( const T &value )
{
	return (void*) &value;
}

template<>
inline void* variableRawData<std::wstring>( const std::wstring& value )
{
	return (void*) value.data();
}

template<>
inline void* variableRawData<std::string>( const std::string& value )
{
	return (void*) value.data();
}

template<>
inline void* variableRawData<MpBlob>( const MpBlob& value )
{
	return (void*) value.getData();
}


// Compare two instances of a type.
template <typename T>
inline bool variablesAreEqual( const T& a, const T& b )
{
	return a == b;
}

// De-serialize type.
template <typename T>
inline void VariableFromRaw( int size, const void* data, T& returnValue )
{
	assert( size == sizeof(T) && "check pin datatype matches XML" ); // Have you re-scanned modules since last change?
	memcpy( &returnValue, data, size );
}

template <>
inline void VariableFromRaw<struct MpBlob>( int size, const void* data, struct MpBlob& returnValue )
{
	returnValue.setValueRaw( size, data );
}

template <>
inline void VariableFromRaw<bool>( int size, const void* data, bool& returnValue )
{
	// bool is pased as int.
	if( size == 4 ) // DSP sends bool events as int.
	{
		returnValue = *((int*) data) != 0;
	}
	else
	{
		assert( size == 1 );
		returnValue = *((bool*) data);
	}
}

template <>
inline void VariableFromRaw<std::wstring>( int size, const void* data, std::wstring& returnValue )
{
	returnValue.assign( (wchar_t* ) data, size / sizeof(wchar_t) );
}

template <>
inline void VariableFromRaw<std::string>( int size, const void* data, std::string& returnValue )
{
	returnValue.assign( (const char*)data, (size_t) size );
}

// Specializations of above for various types.

#endif	// MP_SDK_PIN_TYPES_H_INCLUDED

//===== MpGraphicsExtensionWinGdi.h =====
#ifndef MP_GFX_WIN_GDI_H_INCLUDED
#define MP_GFX_WIN_GDI_H_INCLUDED


// Windows Graphics API, Plugin-side.
struct MpSize
{
	int32_t x;
	int32_t y;
};


struct MpRect
{
	MpRect() : top(0), bottom(0), left(0), right(0)
	{
	}
	MpRect(int32_t l, int32_t t, int32_t r, int32_t b) : top(t), bottom(b), left(l), right(r)
	{
	}
	int32_t top;
	int32_t bottom;
	int32_t left;
	int32_t right;
};

#if defined(_WIN32) && defined(_INC_WINDOWS)

class IMpGraphicsWinGdi : public gmpi::IMpUnknown
{
public:
	// First pass of layout update. Return minimum size required for given available size
	virtual int32_t MP_STDCALL measure( MpSize availableSize, MpSize& returnDesiredSize ) = 0;

	// Second pass of layout.
	virtual int32_t MP_STDCALL arrange( MpRect finalRect ) = 0;

	virtual int32_t MP_STDCALL openWindow( HWND parentWindow, HWND& returnWindowHandle ) = 0;

	virtual int32_t MP_STDCALL closeWindow( void ) = 0;
};

// GUID for IMpGraphicsWinGdi
// {76FAC807-0C89-465d-9FAF-49B12D964118}
static const gmpi::MpGuid MP_IID_GRAPHICS_WIN_GDI =
{ 0x76fac807, 0xc89, 0x465d, { 0x9f, 0xaf, 0x49, 0xb1, 0x2d, 0x96, 0x41, 0x18 } };


// Windows Graphics API, Host-side.
class IMpGraphicsHostWinGdi : public gmpi::IMpUnknown
{
public:
};

// GUID for IMpGraphicsHostWinGdi
// {E52FAA41-42C3-40a6-A11A-E69DB335CE50}
static const gmpi::MpGuid MP_IID_GRAPHICS_HOST_WIN_GDI =
{ 0xe52faa41, 0x42c3, 0x40a6, { 0xa1, 0x1a, 0xe6, 0x9d, 0xb3, 0x35, 0xce, 0x50 } };

#endif // _WIN32
#endif // MP_GFX_WIN_GDI_H_INCLUDED INCLUDED



//===== MpGraphicsExtensionSynthEdit.h =====
#ifndef MP_GFX_SE_H_INCLUDED
#define MP_GFX_SE_H_INCLUDED

// MPI supports multiple graphics APIs through optional interfaces.


#if defined(_WIN32) && defined(_INC_WINDOWS)

// SynthEdit Graphics API, plugin-side. Based on Windows GDI.
struct MpFontInfo
{
	int size;
	int color;
	int colorBackground;
	int flags;			  // alignment etc.
	int fontHeight;
	char future [100];
};

// Old-style composited (draw-on-window) graphics.
class ISeGraphicsComposited : public gmpi::IMpUnknown
{
public:
	virtual int32_t MP_STDCALL paint( HDC hDC ) = 0;

	// First pass of layout update. Return minimum size required for given available size
	virtual int32_t MP_STDCALL measure( MpSize availableSize, MpSize& returnDesiredSize ) = 0;

	// Second pass of layout.
	virtual int32_t MP_STDCALL arrange( MpRect finalRect ) = 0;

	virtual int32_t MP_STDCALL openWindow( void ) = 0;

	virtual int32_t MP_STDCALL closeWindow( void ) = 0;

	virtual int32_t MP_STDCALL onLButtonDown( UINT flags, POINT point ) = 0;

	virtual int32_t MP_STDCALL onMouseMove( UINT flags, POINT point ) = 0;

	virtual int32_t MP_STDCALL onLButtonUp( UINT flags, POINT point ) = 0;
};

// GUID for IMpGraphicsSynthEdit
// {0157347B-3FD3-4cee-947E-88F694349254}
static const gmpi::MpGuid SE_IID_GRAPHICS_COMPOSITED =
{ 0x157347b, 0x3fd3, 0x4cee, { 0x94, 0x7e, 0x88, 0xf6, 0x94, 0x34, 0x92, 0x54 } };


// SynthEdit's graphics API Host-side. New style.
class ISeGraphicsHostWinGdi : public IMpGraphicsHostWinGdi
{
public:
	// Get host's current skin's font information.
	virtual int32_t MP_STDCALL getFontInfo( wchar_t* style, MpFontInfo& fontInfo, HFONT& returnFontInformation ) = 0;
};

// GUID for IMpGraphicsHostWinGdiSynthEdit
// {B1EFF8C9-9ADC-450b-BCD6-D359CFDC8275}
static const gmpi::MpGuid SE_IID_GRAPHICS_HOST_WIN_GDI =
{ 0xb1eff8c9, 0x9adc, 0x450b, { 0xbc, 0xd6, 0xd3, 0x59, 0xcf, 0xdc, 0x82, 0x75 } };

// SynthEdit's graphics API Host-side. Old style.
class ISeGraphicsHostComposited : public gmpi::IMpUnknown
{
public:
	virtual int32_t MP_STDCALL setCapture( void ) = 0;

	virtual int32_t MP_STDCALL getCapture( int32_t& returnValue ) = 0;

	virtual int32_t MP_STDCALL releaseCapture( void ) = 0;

	virtual int32_t MP_STDCALL invalidateRect( RECT* rect ) = 0;

	// Map a point on plugin gui to the system screen (absolute co-ords).
	virtual int32_t MP_STDCALL mapPointToScreen( POINT& point ) = 0;

	// Get host's current skin's font information.
	virtual int32_t MP_STDCALL getFontInfo( wchar_t* style, MpFontInfo& fontInfo, HFONT& returnFontInformation ) = 0;
};

// GUID for ISeGraphicsHostComposited
// {2A3F4D28-52C5-4a31-BF85-B79853265F65}
static const gmpi::MpGuid MP_IID_GRAPHICS_HOST_SYNTHEDIT =
{ 0x2a3f4d28, 0x52c5, 0x4a31, { 0xbf, 0x85, 0xb7, 0x98, 0x53, 0x26, 0x5f, 0x65 } };

#endif // _WIN32


#endif // MP_GFX_SE_H_INCLUDED

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
		mutable wrappedObjT* obj = {};

	public:
		mp_shared_ptr(){}

		explicit mp_shared_ptr(wrappedObjT* newobj)
		{
			Assign(newobj);
		}
		mp_shared_ptr(const mp_shared_ptr<wrappedObjT>& value)
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
		bool operator==( const wrappedObjT* other ) const
		{
			return obj == other;
		}
		bool operator==( const mp_shared_ptr<wrappedObjT>& other ) const
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

		inline wrappedObjT** getAddressOf()
		{
			assert(obj == 0); // Free it before you re-use it!
			return &obj;
		}
		inline void** asIMpUnknownPtr()
		{
			assert(obj == 0); // Free it before you re-use it!
			return reinterpret_cast<void**>(&obj);
		}

		bool isNull() const
		{
			return obj == nullptr;
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
}

#endif // MP_SHARED_PTR_H_INCLUDED

#if 0
namespace GmpiSdk
{
	class UriFile
	{
		gmpi_sdk::mp_shared_ptr<gmpi::IProtectedFile2> file;

	public:
		UriFile() {}
		UriFile(gmpi_sdk::mp_shared_ptr<gmpi::IMpUnknown> obj)
		{
			if (!obj)
				return;

			obj->queryInterface(gmpi::MP_IID_PROTECTED_FILE2, file.asIMpUnknownPtr());
		}
		UriFile(gmpi_sdk::mp_shared_ptr<gmpi::IProtectedFile2> obj) :
			file(obj) {}

		int64_t size()
		{
			if (file.isNull())
				return 0;

			int64_t s;
			file->getSize(&s);
			return s;
		}

		int32_t read(char* dest, size_t size)
		{
			if (file.isNull())
				return gmpi::MP_FAIL;

			return file->read(dest, size);
		}

		int64_t seek(int64_t distanceToMove, int32_t moveMethod)
		{
			if (file.isNull())
				return -1;

			int64_t newPos{};
			file->seek(distanceToMove, moveMethod, &newPos);

			return newPos;
		}

		gmpi::IProtectedFile2* get()
		{
			return file.get();
		}

		explicit operator bool()
		{
			return !file.isNull();
		}
	};
}

#endif