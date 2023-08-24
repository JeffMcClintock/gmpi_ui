#pragma once

/*
#include "Controller.h"
*/

#include <map>
#include <memory>
#include "../se_sdk3/TimerManager.h"
#include "../se_sdk3/mp_gui.h"
#include "../shared/FileWatcher.h"
#include "../../IGuiHost2.h"
#include "../../interThreadQue.h"
#include "../../SeAudioMaster.h"
#include "../../mfc_emulation.h"
#include "MpParameter.h"
#include "../../modules/se_sdk3_hosting/ControllerHost.h"
#include "../../my_msg_que_output_stream.h"

namespace SynthEdit2
{
	class IPresenter;
}

// Manages SEM plugin's controllers.
class ControllerManager : public gmpi::IMpParameterObserver
{
public:
	std::vector< std::pair<int32_t, std::unique_ptr<ControllerHost> > > childPluginControllers;
	IGuiHost2* patchManager;

	virtual int32_t MP_STDCALL setParameter(int32_t parameterHandle, int32_t fieldId, int32_t voice, const void* data, int32_t size) override
	{
		int32_t moduleHandle = -1;
		int32_t moduleParameterId = -1;
		patchManager->getParameterModuleAndParamId(parameterHandle, &moduleHandle, &moduleParameterId);

		for (auto& m : childPluginControllers)
		{
			if (m.first == moduleHandle)
			{
				m.second->setPluginParameter(parameterHandle, fieldId, voice, data, size);
			}
		}

		return gmpi::MP_OK;
	}

	void addController(int32_t handle, gmpi_sdk::mp_shared_ptr<gmpi::IMpController> controller)
	{
		childPluginControllers.push_back({ handle, std::make_unique<ControllerHost>() });
		auto chost = childPluginControllers.back().second.get();

//		chost->controller_.Attach(controller);
		chost->controller_ = controller;
		chost->patchManager = patchManager;
		chost->handle = handle;
		controller->setHost(chost);

		/* TODO
		int indx = 0;
		auto p = GetParameter(this, indx);
		while (p != nullptr)
		{
		p->RegisterWatcher(chost);
		// possibly don't need to unregister. Param will get deleted when this does anyhow.
		p = GetParameter(this, ++indx);
		}
		*/
	}

	GMPI_QUERYINTERFACE1(gmpi::MP_IID_PARAMETER_OBSERVER, gmpi::IMpParameterObserver);
	GMPI_REFCOUNT;
};

class MpController;

class UndoManager
{
	//                      description   preset XML
	std::vector< std::pair< std::string, std::string> > history;
	int undoPosition = -1;
	bool AB_is_A = true;
	std::string AB_storage;

	int size()
	{
		return static_cast<int>(history.size());
	}

public:
	bool enabled = {};

	void initial(class MpController* controller);

	void initialFromXml(MpController* controller, std::string xml);

	void push(std::string description, const std::string& preset);

	void snapshot(class MpController* controller, std::string description);

	void undo(MpController* controller);
	void redo(MpController* controller);
	void getA(MpController* controller);
	void getB(MpController* controller);
	void copyAB(MpController* controller);
	void UpdateGui(MpController* controller);
	void debug();
	bool canUndo();
	bool canRedo();
};

class MpController : public IGuiHost2, public interThreadQueUser, public TimerClient
{
public:
	// presets from factory.xmlpreset resource.
	struct presetInfo
	{
		std::string name;
		std::string category;
		int index;			// Internal Factory presets only.
		std::wstring filename;	// External disk presets only.
		std::size_t hash;
		bool isFactory;
		bool isSession = false; // is temporary preset to accomodate preset loaded from DAW session (but not an existing preset)
	};

protected:
	static const int timerPeriodMs = 35;

private:
	ControllerManager semControllers;
	// When syncing Preset Browser to a preset from the DAW, inhibit normal preset loading behaviour. Else preset gets loaded twice.
	bool inhibitProgramChangeParameter = {};
    file_watcher::FileWatcher fileWatcher;

	// Ignore-Program-Change support
	static const int ignoreProgramChangeStartupTimeMs = 2000;
	static const int startupTimerInit = ignoreProgramChangeStartupTimeMs / timerPeriodMs;
	int startupTimerCounter = startupTimerInit;
	bool ignoreProgramChange = false;
	bool presetsFolderChanged = false;

protected:
	::UndoManager undoManager;
    bool isInitialized = {};

	std::vector< std::unique_ptr<MpParameter> > parameters_;
	std::map< std::pair<int, int>, int > moduleParameterIndex;		// Module Handle/ParamID to Param Handle.
	std::map< int, MpParameter* > ParameterHandleIndex;				// Param Handle to Parameter*.
	std::vector<gmpi::IMpParameterObserver*> m_guis2;
	SynthEdit2::IPresenter* presenter_ = nullptr;

	GmpiGui::FileDialog nativeFileDialog;

    interThreadQue message_que_dsp_to_ui;
	bool hasInternalPresets = false; // VST2 has internal preset. VST3 does not.
	bool isSemControllersInitialised = false;

	// see also VST3Controller.programNames
	std::vector< presetInfo > presets;
	std::string session_preset_xml;

	GmpiGui::OkCancelDialog okCancelDialog;

	void OnFileDialogComplete(int mode, int32_t result);
	virtual void OnStartupTimerExpired();

public:
	MpController() :
		message_que_dsp_to_ui(SeAudioMaster::UI_MESSAGE_QUE_SIZE2)
	{
		semControllers.patchManager = this;
		RegisterGui2(&semControllers);
	}
    
    ~MpController();

	void ScanPresets();
	void setPresetFromDaw(const std::string& xml, bool updateProcessor);
	void SavePreset(int32_t presetIndex);
	void SavePresetAs(const std::string& presetName);
	void DeletePreset(int presetIndex);
	void UpdatePresetBrowser();

	void Initialize();

	void initSemControllers();

	int32_t getController(int32_t moduleHandle, gmpi::IMpController ** returnController) override;

	void setMainPresenter(SynthEdit2::IPresenter* presenter) override
	{
		presenter_ = presenter;
	}

	// Override these
	virtual void ParamGrabbed(MpParameter_native* param, int32_t voice = 0) = 0;

	// Presets
	virtual std::string loadNativePreset(std::wstring sourceFilename) = 0;
	virtual void saveNativePreset(const char* filename, const std::string& presetName, const std::string& xml) = 0;
	virtual std::wstring getNativePresetExtension() = 0;
	int getPresetCount()
	{
		return static_cast<int>(presets.size());
	}
	presetInfo getPresetInfo(int index) // return a copy on purpose so we can rescan presets from inside PresetMenu.callbackOnDeleteClicked lambda
	{
		return presets[index];
	}
	bool isPresetModified();
	void FileToString(const platform_string& path, std::string& buffer);

	MpController::presetInfo parsePreset(const std::wstring& filename, const std::string& xml);
	std::vector< MpController::presetInfo > scanNativePresets();
	virtual std::vector< MpController::presetInfo > scanFactoryPresets() = 0;
	virtual void loadFactoryPreset(int index, bool fromDaw) = 0;
	std::vector<MpController::presetInfo> scanPresetFolder(platform_string PresetFolder, platform_string extension);

	void ParamToDsp(MpParameter* param, int32_t voice = 0);
//	void HostControlToDsp(MpParameter* param, int32_t voice = 0);
	void SerialiseParameterValueToDsp(my_msg_que_output_stream& stream, MpParameter* param);
	void UpdateProgramCategoriesHc(MpParameter * param);
	MpParameter* createHostParameter(int32_t hostControl);
	virtual int32_t sendSdkMessageToAudio(int32_t handle, int32_t id, int32_t size, const void* messageData) override;
	void OnSetHostControl(int hostControl, int32_t paramField, int32_t size, const void * data, int32_t voice);

	// IGuiHost2
	virtual int32_t RegisterGui2(gmpi::IMpParameterObserver* gui) override
	{
		m_guis2.push_back(gui);
		return gmpi::MP_OK;
	}
	virtual int32_t UnRegisterGui2(gmpi::IMpParameterObserver* gui) override
	{
		for (auto it = m_guis2.begin(); it != m_guis2.end(); ++it)
		{
			if (*it == gui)
			{
				m_guis2.erase(it);
				break;
			}
		}

		return gmpi::MP_OK;
	}
	void initializeGui(gmpi::IMpParameterObserver* gui, int32_t parameterHandle, gmpi::FieldType FieldId) override;
	int32_t getParameterHandle(int32_t moduleHandle, int32_t moduleParameterId) override;
	int32_t getParameterModuleAndParamId(int32_t parameterHandle, int32_t* returnModuleHandle, int32_t* returnModuleParameterId) override;
	RawView getParameterValue(int32_t parameterHandle, int32_t fieldId, int32_t voice = 0) override;

	MpParameter* getHostParameter(int32_t hostControl);

	void ImportPresetXml(const char* filename, int presetIndex = -1);
	std::string getPresetXml(std::string presetNameOverride = {});
	void setPreset(class TiXmlNode* parentXml, bool updateProcessor, int preset);
	void setPreset(const std::string& xml, bool updateProcessor = true, int preset = 0);
	void ExportPresetXml(const char* filename, std::string presetNameOverride = {});
	void ImportBankXml(const char * filename);
	void setModified(bool presetIsModified);
	void ExportBankXml(const char * filename);

	void setParameterValue(RawView value, int32_t parameterHandle, gmpi::FieldType moduleFieldId = gmpi::MP_FT_VALUE, int32_t voice = 0) override;
	gmpi_gui::IMpGraphicsHost * getGraphicsHost();
	virtual int32_t resolveFilename(const wchar_t* shortFilename, int32_t maxChars, wchar_t* returnFullFilename) override;

	void updateGuis(MpParameter* parameter, int voice)
	{
		const auto rawValue = parameter->getValueRaw(gmpi::MP_FT_VALUE, voice);
		const float normalized = parameter->getNormalized(); // voice !!!?

		for (auto pa : m_guis2)
		{
			// Update value.
			pa->setParameter(parameter->parameterHandle_, gmpi::MP_FT_VALUE, voice, rawValue.data(), (int32_t)rawValue.size());

			// Update normalized.
			pa->setParameter(parameter->parameterHandle_, gmpi::MP_FT_NORMALIZED, voice, &normalized, (int32_t)sizeof(normalized));
		}
	}

	void updateGuis(MpParameter* parameter, gmpi::FieldType fieldType, int voice = 0 )
	{
		auto rawValue = parameter->getValueRaw(fieldType, voice);

		for (auto pa : m_guis2)
		{
			pa->setParameter(parameter->parameterHandle_, fieldType, voice, rawValue.data(), (int32_t)rawValue.size());
		}
	}

	// interThreadQueUser
	bool OnTimer() override;
	bool onQueMessageReady(int handle, int msg_id, class my_input_stream& p_stream) override;

	void serviceGuiQueue() override
	{
		message_que_dsp_to_ui.pollMessage(this);
	}

	virtual IWriteableQue* getQueueToDsp() = 0;

	interThreadQue* getQueueToGui()
	{
		return &message_que_dsp_to_ui;
	}

	virtual void ResetProcessor() {}
	virtual void OnStartPresetChange() {}
	virtual void OnEndPresetChange();
	virtual void OnLatencyChanged() {}
	virtual MpParameter_native* makeNativeParameter(int ParameterTag, bool isInverted = false) = 0;
};
