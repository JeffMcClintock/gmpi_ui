//#include "pch.h"
#include "GuiPatchAutomator3.h"
#include "../SE_DSP_CORE/IGuiHost2.h"

//using namespace SynthEdit2;
using namespace gmpi;
using namespace std;

GMPI_REGISTER_GUI(MP_SUB_TYPE_GUI2, GuiPatchAutomator3, L"PatchAutomator" );

namespace
{
	int32_t r = RegisterPluginXml(
R"XML(
<?xml version="1.0" encoding="utf-8" ?>
<PluginList>
  <Plugin id="PatchAutomator" name="PatchAutomator3" category="Debug">
    <GUI/>
  </Plugin>
</PluginList>
)XML");

}

GuiPatchAutomator3::~GuiPatchAutomator3()
{
	if(patchManager_)
		patchManager_->UnRegisterGui2(this);
}

void GuiPatchAutomator3::Sethost( class IGuiHost2* host)
{
	patchManager_ = host;
	if (patchManager_)
		patchManager_->RegisterGui2(this);
}

int32_t GuiPatchAutomator3::initialize()
{
	auto res = SeGuiInvisibleBase::initialize();

	for (auto& p : parameterToPinIndex_)
	{
		patchManager_->initializeGui(this, p.first.first, (gmpi::FieldType)p.first.second);
	}

	return res;
}

int32_t GuiPatchAutomator3::setPin( int32_t pinId, int32_t voice, int32_t size, const void* data )
{
	assert( pinId >= 0 && pinId < (int) parameterToPinIndex_.size() );

#if 0 //def SE_TAR GET_PURE_UWP
//	auto p = parameterToPinIndex_[pinId];
	Presentor()->setParameterFromGui(container_json_, p.moduleHandle, p.moduleParamId, p.paramField, voice, size, data );
#else
	patchManager_->setParameterValue(RawView(data, size ), pinToParameterIndex_[pinId].first, (gmpi::FieldType) pinToParameterIndex_[pinId].second, voice);

#endif
	return GuiPinOwner::setPin2( pinId, voice, size, data );
}

// Host control indicated by negative parameter ID.
int GuiPatchAutomator3::Register(int moduleHandle, int moduleParamId, ParameterFieldType paramField /*, ModuleView* module, int pinIdx, bool isPolyphonic*/)
{
	auto parameterHandle = patchManager_->getParameterHandle(moduleHandle, moduleParamId);
	if (parameterHandle == -1) // not available
	{
		return -1;
	}

	auto it = parameterToPinIndex_.find(std::pair<int32_t, int32_t>(parameterHandle, paramField));
	if (it != parameterToPinIndex_.end())
	{
		return (*it).second;
	}

	auto pinId = (int) parameterToPinIndex_.size();
	parameterToPinIndex_.insert(std::pair< std::pair< int32_t, int32_t>, int32_t >(std::pair< int32_t, int32_t>(parameterHandle, paramField), pinId));

	pinToParameterIndex_.push_back(std::pair< int32_t, int32_t>(parameterHandle, paramField));

	return pinId;
}

int32_t GuiPatchAutomator3::setParameter(int32_t parameterHandle, int32_t fieldId, int32_t voice, const void* data, int32_t size)
{
	auto it = parameterToPinIndex_.find(std::pair<int32_t, int32_t>(parameterHandle, fieldId));
	if (it != parameterToPinIndex_.end())
	{
		getHost()->pinTransmit((*it).second, size, data, voice);
	}

	return gmpi::MP_OK;
}
