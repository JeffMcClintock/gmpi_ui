#ifndef MP_SDK_GUID_H_INCLUDED
#define MP_SDK_GUID_H_INCLUDED

#include <string.h>
#include "mp_sdk_stdint.h"

#ifdef __cplusplus 
namespace gmpi
{
#endif

	// GUID - Globally Unique Identifier, used to identify interfaces.
	struct MpGuid
	{
		uint32_t data1;
		uint16_t data2;
		uint16_t data3;
		unsigned char data4[8];
	};

	// GUID comparison
#ifdef __cplusplus
	inline
#endif
	int MpGuidCompare(const struct MpGuid* a, const struct MpGuid* b)
	{
		if (a->data1 != b->data1)
			return (a->data1 - b->data1);
		if (a->data2 != b->data2)
			return (a->data2 - b->data2);
		if (a->data3 != b->data3)
			return (a->data3 - b->data3);
		return memcmp(a->data4, b->data4, sizeof(a->data4));
	}

#ifdef __cplusplus
	inline
	bool MpGuidEqual( const gmpi::MpGuid* a, const gmpi::MpGuid* b )
	{
		return ( MpGuidCompare(a, b) == 0 );
	}
#else
	int MpGuidEqual(const struct MpGuid* a, const struct MpGuid* b)
	{
		return ( MpGuidCompare(a, b) == 0 );
	}
#endif


#ifdef __cplusplus 
}

inline bool
operator==( const gmpi::MpGuid& left, const gmpi::MpGuid& right )
{
	return ( gmpi::MpGuidEqual(&left, &right) != 0 );
}

#endif

#endif // MP_SDK_GUID_H_INCLUDED


