#pragma once
/*
#include "RefCountMacros.h"
*/

// macros to save typing the reference counting.
#define GMPI_QUERYINTERFACE( INTERFACE_IID, CLASS_NAME ) \
	gmpi::ReturnCode queryInterface(const gmpi::api::Guid* iid, void** returnInterface) override{ \
	*returnInterface = {}; \
	if ((*iid) == INTERFACE_IID || (*iid) == gmpi::api::IUnknown::guid ){ \
	*returnInterface = static_cast<CLASS_NAME*>(this); addRef(); \
	return gmpi::ReturnCode::Ok;} \
	return gmpi::ReturnCode::NoSupport;}

// TODO -phase out the old one and rename
#define GMPI_QUERYINTERFACE_NEW( CLASS_NAME ) \
	gmpi::ReturnCode queryInterface(const gmpi::api::Guid* iid, void** returnInterface) override{ \
	*returnInterface = {}; \
	if ((*iid) == CLASS_NAME::guid || (*iid) == gmpi::api::IUnknown::guid ){ \
	*returnInterface = static_cast<CLASS_NAME*>(this); addRef(); \
	return gmpi::ReturnCode::Ok;} \
	return gmpi::ReturnCode::NoSupport;}

#ifndef GMPI_REFCOUNT
#define GMPI_REFCOUNT int refCount2_ = 1; \
	int32_t addRef() override {return ++refCount2_;} \
	int32_t release() override {if (--refCount2_ == 0){delete this;return 0;}return refCount2_;}

#define GMPI_REFCOUNT_NO_DELETE \
	int32_t addRef() override{return 1;} \
	int32_t release() override {return 1;}
#endif

#if 0
// old crap

// Handy macro to save typing.
#define GMPI_QUERYINTERFACE1( INTERFACE_IID, CLASS_NAME ) \
	int32_t MP_STDCALL queryInterface(const gmpi::api::Guid& iid, void** returnInterface) override \
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

#define GMPI_QUERYINTERFACE2( INTERFACE_IID, CLASS_NAME, BASE_CLASS ) \
	int32_t MP_STDCALL queryInterface(const gmpi::api::Guid& iid, void** returnInterface) override \
{ \
	*returnInterface = 0; \
	if (iid == INTERFACE_IID ) \
{ \
	*returnInterface = static_cast<CLASS_NAME*>(this); \
	addRef(); \
	return gmpi::MP_OK; \
} \
return BASE_CLASS::queryInterface(iid, returnInterface); \
}
#endif
