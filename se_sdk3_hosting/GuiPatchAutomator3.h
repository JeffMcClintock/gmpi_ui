#pragma once
#include <vector>
#include "mp_sdk_gui.h"
#include "IPluginGui.h"

/*
#include "GuiPatchAutomator3.h"
*/

// Provides a standard GUI object as a window into the patch-manager.
class GuiPatchAutomator3 : public SeGuiInvisibleBase, public gmpi::IMpParameterObserver
{
	std::map< std::pair< int32_t, int32_t>, int32_t> parameterToPinIndex_;	// pins are indexed by parameter-handle, field. value is pinId
	std::vector< std::pair< int32_t, int32_t> > pinToParameterIndex_;		// lookup by pin Index, returns paramter ahndle, field id.

	class IGuiHost2* patchManager_ = {};

public:
	virtual ~GuiPatchAutomator3();

	void Sethost(class IGuiHost2* host);

	virtual int32_t MP_STDCALL initialize() override;
	virtual int32_t MP_STDCALL setPin( int32_t pinId, int32_t voice, int32_t size, const void* data ) override;

	int Register(int moduleHandle, int moduleParamId, ParameterFieldType paramField);
	virtual int32_t MP_STDCALL setParameter(int32_t parameterHandle, int32_t fieldId, int32_t voice, const void* data, int32_t size) override;

	GMPI_QUERYINTERFACE2(gmpi::MP_IID_PARAMETER_OBSERVER, gmpi::IMpParameterObserver, SeGuiInvisibleBase);
	GMPI_REFCOUNT; // not deleting in SE _NO_DELETE;
};

