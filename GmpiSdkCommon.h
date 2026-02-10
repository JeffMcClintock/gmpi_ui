#pragma once
// WARNING: This source file is duplicated in GMPI SDK and GMPI-UI SDK. keep them in sync.
#ifndef _GMPI_SDK_COMMON_H_INCLUDED // ignore source file in multiple locations.
#define _GMPI_SDK_COMMON_H_INCLUDED

// SPDX-License-Identifier: ISC
// Copyright 2007-2026 Jeff McClintock.

#include <assert.h>
#include <string>
#include "GmpiApiCommon.h"
#include "RefCountMacros.h"

namespace gmpi
{
// Helper for managing lifetime of reference-counted interface pointer
template<class wrappedObjT>
class shared_ptr
{
	mutable wrappedObjT* obj{};

public:
	shared_ptr() noexcept = default;

	explicit shared_ptr(wrappedObjT* newobj)
	{
		assign(newobj);
	}
	shared_ptr(shared_ptr<wrappedObjT> const& value) noexcept // copy
	{
		assign(value.obj);
	}
	shared_ptr(shared_ptr<wrappedObjT>&& value) // move
	{
		attach(value.obj);
		value.obj = {};
	}

	// Attach object without incrementing ref count. For objects created with new.
	void attach(wrappedObjT* newobj)
	{
		auto old = obj;
		obj = newobj;

		if( old )
		{
			old->release();
		}
	}

	~shared_ptr()
	{
		if( auto temp = obj ; temp)
		{
			obj = nullptr;
			temp->release();
		}
	}
	operator wrappedObjT*( )
	{
		return obj;
	}
	const wrappedObjT* operator=( wrappedObjT* value )
	{
		assign(value);
		return value;
	}
	shared_ptr<wrappedObjT>& operator=( shared_ptr<wrappedObjT> const& value ) // copy 
	{
		assign(value.get());
		return *this;
	}
	shared_ptr<wrappedObjT>& operator=(shared_ptr<wrappedObjT>&& value) // move
	{
		std::swap(obj, value.obj);
		return *this;
	}
	bool operator==( const wrappedObjT* other ) const
	{
		return obj == other;
	}
	bool operator==( const shared_ptr<wrappedObjT>& other ) const
	{
		return obj == other.obj;
	}
	wrappedObjT* operator->( ) const
	{
		return obj;
	}

	wrappedObjT*& get() const
	{
		return obj;
	}

	wrappedObjT** getAddressOf()
	{
		assert(obj == nullptr); // Free it before you re-use it!
		return &obj;
	}

	wrappedObjT** put()
	{
		// Free it before you re-use it
		if (obj){obj->release();}
		obj = nullptr;
		return &obj;
	}

	void** put_void() noexcept
	{
		return reinterpret_cast<void**>(put());
	}

	//void** asIUnknownPtr()
	//{
	//	assert(obj == 0); // Free it before you re-use it!
	//	return reinterpret_cast<void**>(&obj);
	//}

	template<typename I>
	shared_ptr<I> as()
	{
		shared_ptr<I> returnInterface;
		if (obj)
		{
			obj->queryInterface(&I::guid, returnInterface.put_void());
		}
		return returnInterface;
	}

	bool isNull() const
	{
		return obj == nullptr;
	}

private:
	// Attach object and increment ref count.
	void assign(wrappedObjT* newobj) // use operator= to use this
	{
		if (newobj != obj) // skip self-asignment.
		{
			attach(newobj);
			if( newobj )
			{
				newobj->addRef();
			}
		}
	}
};

template<typename INTERFACE>
INTERFACE* as(api::IUnknown* com_object)
{
	INTERFACE* result{};
	com_object->queryInterface(&INTERFACE::guid, (void**)&result);
	return result;
}

// Helper for returning strings.
struct ReturnString : public api::IString
{
	std::string cppString;

	ReturnCode setData(const char* data, int32_t size) override
	{
		if (size < 1)
		{
			cppString.clear();
		}
		else
		{
			cppString.assign(data, static_cast<size_t>(size));
		}
		return ReturnCode::Ok;
	}

	int32_t getSize() override
	{
		return static_cast<int32_t>(cppString.size());
	}
	const char* getData() override
	{
		return cppString.data();
	}

	const char* c_str() const
	{
		return cppString.c_str();
	}

	const std::string& str() const
	{
		return cppString;
	}

//	api::IString* get() // should be put()?????
//	{
//		cppString.clear();
//		return static_cast<api::IString*>(this);
//	}

	GMPI_QUERYINTERFACE_METHOD(gmpi::api::IString);
	GMPI_REFCOUNT_NO_DELETE;
};

} // namespace gmpi
#endif // _GMPI_SDK_COMMON_H_