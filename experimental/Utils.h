#pragma once

#define _USE_MATH_DEFINES
#include <math.h>

namespace gmpi_sdk
{
	inline float tsin(float turn)
	{
		constexpr float conv = static_cast<float>(M_PI * 2.0);
		return sinf(turn * conv);
	}

	inline float tcos(float turn)
	{
		constexpr float conv = static_cast<float>(M_PI * 2.0);
		return cosf(turn * conv);
	}

	inline auto Center(GmpiDrawing::Rect r) -> GmpiDrawing::Point
	{
		return { 0.5f * (r.left + r.right), 0.5f * (r.top + r.bottom) };
	}


// easy refcounting without macros
template<class T>
class GmpiBase : public T
{
	int32_t refCount2_ = 1;

public:
	virtual ~GmpiBase() {}

	int32_t queryInterface(const gmpi::MpGuid& iid, void** returnInterface) override
	{
		*returnInterface = 0;
		if (iid == T::guid || iid == gmpi::MP_IID_UNKNOWN)
		{
			*returnInterface = static_cast<T*>(this);
			addRef();
			return gmpi::MP_OK;
		}
		return gmpi::MP_NOSUPPORT;
	}
	int32_t addRef() override
	{
		return ++refCount2_;
	}
	int32_t release() override
	{
		if (--refCount2_ == 0)
		{
			delete this;
			return 0;
		}
		return refCount2_;
	}
};

template<class T, class T2>
class GmpiBase2 : public T, public T2
{
	int32_t refCount2_ = 1;

public:
	virtual ~GmpiBase2() {}

	int32_t queryInterface(const gmpi::MpGuid& iid, void** returnInterface) override
	{
		*returnInterface = 0;
		if (iid == T::guid || iid == gmpi::MP_IID_UNKNOWN)
		{
			*returnInterface = static_cast<T*>(this);
			addRef();
			return gmpi::MP_OK;
		}
		if (iid == T2::guid)
		{
			*returnInterface = static_cast<T2*>(this);
			addRef();
			return gmpi::MP_OK;
		}
		return gmpi::MP_NOSUPPORT;
	}
	int32_t addRef() override
	{
		return ++refCount2_;
	}
	int32_t release() override
	{
		if (--refCount2_ == 0)
		{
			delete this;
			return 0;
		}
		return refCount2_;
	}
};
}