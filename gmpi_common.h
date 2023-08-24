#pragma once
#ifndef GMPI_COMMON_INCLUDED
#define GMPI_COMMON_INCLUDED

//#include "gmpi_common.h"
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
////#define MP_PLATFORM_WIN32	1
//#define MP_STDCALL		__stdcall
//#else
//#define MP_STDCALL
#endif
#define MP_STDCALL

// MODIFICATION FOR GMPI_UI !!!
namespace gmpi
{

// All methods return an error code.
// An int32_t return code may take one of these values.
enum
{
	MP_HANDLED = 1,				// Success. In case of GUI - no further handing required.
	MP_OK = 0,					// Success. In case of GUI indicates successful mouse "hit".
	MP_FAIL = -1,				// General failure.
	MP_UNHANDLED = -1,			// Event not handled.
	MP_NOSUPPORT = -2,			// Interface not supported.
	MP_CANCEL = -3,				// Async operation cancelled.
};

// GUID - Globally Unique Identifier, used to identify interfaces.
struct MpGuid
{
	uint32_t data1;
	uint16_t data2;
	uint16_t data3;
	unsigned char data4[8];
};

inline bool
operator==(const gmpi::MpGuid& left, const gmpi::MpGuid& right)
{
	if (left.data1 != right.data1 || left.data2 != right.data2 || left.data3 != right.data3)
		return false;

	for (size_t i = 0; i < sizeof(left.data4); ++i)
	{
		if (left.data4[i] != right.data4[i])
			return false;
	}

	return true;
}



// IMpUnknown Interface.
// This is the base interface.  All objects support these 3 methods.
class DECLSPEC_NOVTABLE IMpUnknown
{
public:
	// Query the object for a supported interface.
	virtual int32_t MP_STDCALL queryInterface(const MpGuid& iid, void** returnInterface) = 0;

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

} // namespace gmpi

#endif //include
