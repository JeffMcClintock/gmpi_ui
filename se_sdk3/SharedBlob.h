#pragma once

#include "mp_api.h"
#include <string>

// todo ensure 'sent' blob waiting to be processed by destination module can't be modified.
class SharedBlobView : public gmpi::ISharedBlob
{
	uint8_t* data_ = nullptr;
	int64_t size_ = 0;

public:
	SharedBlobView() {}

	SharedBlobView(uint8_t* data, int64_t size) :
		data_(data), size_(size)
	{
	}

	int32_t MP_STDCALL read(uint8_t** returnData, int64_t* returnSize) override
	{
		*returnData = data_;
		*returnSize = size_;

		return gmpi::MP_OK;
	}

	int32_t set(uint8_t* data, int64_t size)
	{
		// can't modify a locked object. Allocate a new one.
		if (refCount2_ > 1)
			return gmpi::MP_FAIL;

		data_ = data;
		size_ = size;

		return gmpi::MP_OK;
	}
	int32_t set(const char* data, int64_t size)
	{
		return set((uint8_t*)data, size);
	}

	bool inUse()
	{
		return refCount2_ > 1;
	}

	GMPI_QUERYINTERFACE1(gmpi::ISharedBlob::guid, gmpi::ISharedBlob);

	// reference counted, but does not automatically delete itself.
	int32_t refCount2_ = 1;
	int32_t MP_STDCALL addRef() override
	{
		return ++refCount2_;
	}
	int32_t MP_STDCALL release() override
	{
		return --refCount2_;
	}
};
