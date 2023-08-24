#pragma once
/*
#include "../se_sdk3_hosting/ControllerHost.h"
*/
#include "../se_sdk3/mp_sdk_common.h"
#include "../../IGuiHost2.h"
//#include "../se_sdk3_hosting/Controller.h"

class ControllerIterator :
	public gmpi::IMpControllerIterator
{
public:
	ControllerIterator() {}

	// IMpPinIterator support.
	virtual int32_t MP_STDCALL getCount() override
	{
		return 0;
	}
	virtual int32_t MP_STDCALL first() override
	{
		return gmpi::MP_OK;
	}
	virtual int32_t MP_STDCALL next() override
	{
		return gmpi::MP_OK;
	}
	virtual int32_t MP_STDCALL getCurrent(gmpi::IMpControllerIteratorItem** returnCurent) override
	{
		return gmpi::MP_OK;
	}

	GMPI_QUERYINTERFACE1(gmpi::MP_IID_CONTROLLER_ITERATOR, gmpi::IMpControllerIterator);
	GMPI_REFCOUNT;
};

// Acts as host for an instance of a SDK controller object.
class ControllerHost final : public gmpi::IMpControllerHost
{
public:
	// Plugin asks host for parameter value. Host will call back indirectly via setParameter.
	void updateParameter(int32_t parameterHandle, int32_t paramFieldType, int32_t voice) override
	{
		// apply filter/ lookup 
		assert(voice == 0); // patchManager now updates ALL voices
		return patchManager->initializeGui(controller_.get(), parameterHandle, (gmpi::FieldType)paramFieldType);
	}

	// plugin -> Patch-Manager.
	int32_t MP_STDCALL setParameter(int32_t parameterHandle, int32_t paramFieldType, int32_t voice, const void* data, int32_t size) override
	{
		patchManager->setParameterValue(RawView(data, size), parameterHandle, (gmpi::FieldType) paramFieldType, voice);
		return gmpi::MP_OK;
	}

	//  Patch-Manager -> plugin.
	int32_t setPluginParameter(int32_t parameterHandle, int32_t paramFieldType, int32_t voice, const void* data, int32_t size)
	{
		return controller_->setParameter(parameterHandle, paramFieldType, voice, data, size );
	}

	int32_t MP_STDCALL getHandle(int32_t& returnHandle) override
	{
		returnHandle = handle;
		return gmpi::MP_OK;
	}
//	int32_t MP_STDCALL getParameter(int32_t paramId, int32_t paramFieldType, int32_t voice, gmpi::IString* value) override

	int32_t getParameterHandle(int32_t moduleHandle, int32_t moduleParameterId) override
	{
		return patchManager->getParameterHandle(moduleHandle, moduleParameterId);
	}
	int32_t getParameterModuleAndParamId(int32_t parameterHandle, int32_t* returnModuleHandle, int32_t* returnModuleParameterId) override
	{
		return patchManager->getParameterModuleAndParamId(parameterHandle, returnModuleHandle, returnModuleParameterId);
	}

	int32_t MP_STDCALL createControllerIterator(gmpi::IMpControllerIterator** returnIterator) override
	{
		*returnIterator = new ControllerIterator();
		return gmpi::MP_OK;
	}

	int32_t MP_STDCALL setLatency(int32_t latency) override
	{
		// TODO !!!!!
		return gmpi::MP_OK;
	}

	// DEPRECATED.
	int32_t MP_STDCALL pinTransmit(int32_t pinId, int32_t voice, int64_t size, const void* data) override
	{
		assert(false); // not implemented(can't determin which parameter). Module should call setParameter()
		return gmpi::MP_FAIL;
/*
		// get param handle from pinId
		int32_t parameterHandle = -1;
		auto fieldId = gmpi::MP_FT_VALUE;
		patchManager->setParameterValue(RawView(data, size), parameterHandle, fieldId, voice);
		return gmpi::MP_OK;
*/
	}

	gmpi_sdk::mp_shared_ptr<gmpi::IMpController> controller_;
	int handle;
	class IGuiHost2* patchManager;

	GMPI_QUERYINTERFACE1(gmpi::MP_IID_CONTROLLER_HOST, gmpi::IMpControllerHost);
	GMPI_REFCOUNT_NO_DELETE;
};