#include <codecvt>
#include <locale>
#include <thread>
#include <filesystem>
#include "Controller.h"
#include "../../tinyxml/tinyxml.h"
#include "../tinyXml2/tinyxml2.h"
#include "../../RawConversions.h"
#include "../../midi_defs.h"
#include "../../modules/shared/FileFinder.h"
#include "../shared/unicode_conversion.h"
#include "../shared/ListBuilder.h"
#include "../../UgDatabase.h"
#include "./Presenter.h"
#include "../../modules/shared/string_utilities.h"
#include "../shared/unicode_conversion.h"
#include "../../UniqueSnowflake.h"
#include "../shared/FileWatcher.h"
#include "mfc_emulation.h"
#if !defined(SE_USE_JUCE_UI)
#include "GuiPatchAutomator3.h"
#endif
#include "BundleInfo.h"
#include "PresetReader.h"

using namespace std;

MpController::~MpController()
{
    if(presenter_)
    {
        presenter_->OnControllerDeleted();
    }
}

void MpController::ScanPresets()
{
	presets.clear(); // fix crash on JUCE
	presets = scanNativePresets(); // Scan VST3 presets (both VST2 and VST3)

	// Factory presets from bundles presets folder.
	{
		auto presets2 = scanFactoryPresets();

		// skip duplicates of disk presets (because VST3 plugin will have them all on disk, and VST2 will scan same folder.
		const auto nativePresetsCount = presets.size();
		for (auto& preset : presets2)
		{
			// Is this a duplicate?
			bool isDuplicate = false;
			for (size_t i = 0; i < nativePresetsCount; ++i)
			{
				if (presets[i].name == preset.name)
				{
					// preset occurs in VST presets folder and ALSO in preset XML.
					isDuplicate = true;
					//presets[i].index = preset.index; // Don't insert it twice, but note that it is an internal preset. (VST3 Preset will be ignored).
					presets[i].isFactory = true;
					break;
				}
			}

			if (!isDuplicate)
			{
				preset.isFactory = true;
				presets.push_back({ preset });
			}
		}
	}
	{
#if 0
	// Factory presets from factory.xmlpreset resource.
		auto nativePresetsCount = presets.size();

		// Harvest factory preset names.
		auto factoryPresetFolder = ToPlatformString(BundleInfo::instance()->getImbeddedFileFolder());
		string filenameUtf8 = ToUtf8String(factoryPresetFolder) + "factory.xmlpreset";

		TiXmlDocument doc;
		doc.LoadFile(filenameUtf8);

		if (!doc.Error()) // if file does not exist, that's OK. Only means we're a VST3 plugin and don't have internal presets.
		{
			TiXmlHandle hDoc(&doc);
			TiXmlElement* pElem;
			{
				pElem = hDoc.FirstChildElement().Element();

				// should always have a valid root but handle gracefully if it does not.
				if (!pElem)
					return;
			}

			const char* pKey = pElem->Value();
			assert(strcmp(pKey, "Presets") == 0);

			int i = 0;
			for (auto preset_xml = pElem->FirstChildElement("Preset"); preset_xml; preset_xml = preset_xml->NextSiblingElement())
			{
				presetInfo preset;
				preset.index = i++;

				preset_xml->QueryStringAttribute("name", &preset.name);
				preset_xml->QueryStringAttribute("category", &preset.category);

				// skip duplicates of disk presets (because VST3 plugin will have them all on disk, and VST2 will scan same folder.
				// Is this a duplicate?
				bool isDuplicate = false;
				for (size_t i = 0; i < nativePresetsCount; ++i)
				{
					if (presets[i].name == preset.name)
					{
						// preset occurs in VST presets folder and ALSO in preset XML.
						isDuplicate = true;
						presets[i].index = preset.index; // Don't insert it twice, but note that it is an internal preset. (VST3 Preset will be ignored).
						break;
					}
				}

				if (!isDuplicate)
				{
					presets.push_back(preset);
				}
			}
		}
#endif
		// sort all presets by category.
		std::sort(presets.begin(), presets.end(),
			[=](const presetInfo& a, const presetInfo& b) -> bool
			{
				// Sort by category
				if (a.category != b.category)
				{
					// blank category last
					if (a.category.empty() != b.category.empty())
						return a.category.empty() < b.category.empty();

					return a.category < b.category;
				}

				// ..then by index
				if (a.index != b.index)
					return a.index < b.index;

				return a.name < b.name;
			});
	}

#ifdef _DEBUG
	for (auto& preset : presets)
	{
		assert(!preset.filename.empty() || preset.isSession || preset.isFactory);
	}
#endif
}

void MpController::UpdatePresetBrowser()
{
	// Update preset browser
	for (auto& p : parameters_)
	{
		if (p->getHostControl() == HC_PROGRAM_CATEGORIES_LIST || p->getHostControl() == HC_PROGRAM_NAMES_LIST)
		{
			UpdateProgramCategoriesHc(p.get());
			updateGuis(p.get(), gmpi::FieldType::MP_FT_VALUE);
		}
	}
}

void MpController::Initialize()
{
    if(isInitialized)
    {
        return; // Prevent double-up on parameters.
    }
    
    // Ensure we can access SEM Controllers info
	ModuleFactory()->RegisterExternalPluginsXmlOnce(nullptr);

	TiXmlDocument doc;
	{
		const auto xml = BundleInfo::instance()->getResource("parameters.se.xml");
		doc.Parse(xml.c_str());
		assert(!doc.Error());
	}

	TiXmlHandle hDoc(&doc);

	auto controllerE = hDoc.FirstChildElement("Controller").Element();
	assert(controllerE);

	auto patchManagerE = controllerE->FirstChildElement();
	assert(strcmp(patchManagerE->Value(), "PatchManager") == 0);

	std::wstring_convert<std::codecvt_utf8<wchar_t>> convert;

	auto parameters_xml = patchManagerE->FirstChildElement("Parameters");

	for (auto parameter_xml = parameters_xml->FirstChildElement("Parameter"); parameter_xml; parameter_xml = parameter_xml->NextSiblingElement("Parameter"))
	{
		int dataType = DT_FLOAT;
		int ParameterTag = -1;
		int ParameterHandle = -1;
		int Private = 0;

		std::string Name = parameter_xml->Attribute("Name");
		parameter_xml->QueryIntAttribute("ValueType", &dataType);
		parameter_xml->QueryIntAttribute("Index", &ParameterTag);
		parameter_xml->QueryIntAttribute("Handle", &ParameterHandle);
		parameter_xml->QueryIntAttribute("Private", &Private);

		if (dataType == DT_TEXT || dataType == DT_BLOB)
		{
			Private = 1; // VST and AU can't handle this type of parameter.
		}
		else
		{
			if (Private != 0)
			{
				// Check parameter is numeric and a valid type.
				assert(dataType == DT_ENUM || dataType == DT_DOUBLE || dataType == DT_BOOL || dataType == DT_FLOAT || dataType == DT_INT || dataType == DT_INT64);
			}
		}

		int stateful_ = 1;
		parameter_xml->QueryIntAttribute("persistant", &stateful_);
		int hostControl = -1;
		parameter_xml->QueryIntAttribute("HostControl", &hostControl);
		int ignorePc = 0;
		parameter_xml->QueryIntAttribute("ignoreProgramChange", &ignorePc);

		double pminimum = 0.0;
		double pmaximum = 10.0;

		parameter_xml->QueryDoubleAttribute("RangeMinimum", &pminimum);
		parameter_xml->QueryDoubleAttribute("RangeMaximum", &pmaximum);

		int moduleHandle_ = -1;
		int moduleParamId_ = 0;
		bool isPolyphonic_ = false;
		wstring enumList_;

		parameter_xml->QueryIntAttribute("Module", &(moduleHandle_));
		parameter_xml->QueryIntAttribute("ModuleParamId", &(moduleParamId_));
		parameter_xml->QueryBoolAttribute("isPolyphonic", &(isPolyphonic_));

		if (dataType == DT_INT || dataType == DT_TEXT /*|| dataType == DT_ENUM */)
		{
			auto s = parameter_xml->Attribute("MetaData");
			if (s)
				enumList_ = convert.from_bytes(s);
		}

		MpParameter_base* seParameter = nullptr;

		if (Private == 0)
		{
			assert(ParameterTag >= 0);
			seParameter = makeNativeParameter(ParameterTag, pminimum > pmaximum);
		}
		else
		{
			auto param = new MpParameter_private(this);
			seParameter = param;
			param->isPolyphonic_ = isPolyphonic_;
		}

		seParameter->hostControl_ = hostControl;
		seParameter->minimum = pminimum;
		seParameter->maximum = pmaximum;

		parameter_xml->QueryIntAttribute("MIDI", &(seParameter->MidiAutomation));
		if (seParameter->MidiAutomation != -1)
		{
			std::string temp;
			parameter_xml->QueryStringAttribute("MIDI_SYSEX", &temp);
			seParameter->MidiAutomationSysex = Utf8ToWstring(temp);
		}

		// Preset values from patch list.
		ParseXmlPreset(
			parameter_xml,
			[seParameter, dataType](int voiceId, int preset, const char* xmlvalue)
			{
				seParameter->rawValues_.push_back(ParseToRaw(dataType, xmlvalue));
			}
		);

		// no patch-list?, init to zero.
		if (!parameter_xml->FirstChildElement("patch-list"))
		{
			assert(!stateful_);

			// Special case HC_VOICE_PITCH needs to be initialized to standard western scale
			if (HC_VOICE_PITCH == hostControl)
			{
				const int middleA = 69;
				constexpr float invNotesPerOctave = 1.0f / 12.0f;
				seParameter->rawValues_.reserve(128);
				for (float key = 0; key < 128; ++key)
				{
					const float pitch = 5.0f + static_cast<float>(key - middleA) * invNotesPerOctave;
					std::string raw((const char*) &pitch, sizeof(pitch));
					seParameter->rawValues_.push_back(raw);
				}
			}
			else
			{
				// init to zero
				const char* nothing = "\0\0\0\0\0\0\0\0";
				std::string raw(nothing, getDataTypeSize(dataType));
				seParameter->rawValues_.push_back(raw);
			}
		}

		seParameter->parameterHandle_ = ParameterHandle;
		seParameter->datatype_ = dataType;
		seParameter->moduleHandle_ = moduleHandle_;
		seParameter->moduleParamId_ = moduleParamId_;
		seParameter->stateful_ = stateful_;
		seParameter->name_ = convert.from_bytes(Name);
		seParameter->enumList_ = enumList_;
		seParameter->ignorePc_ = ignorePc != 0;

		parameters_.push_back(std::unique_ptr<MpParameter>(seParameter));
		ParameterHandleIndex.insert(std::make_pair(ParameterHandle, seParameter));
		moduleParameterIndex.insert(std::make_pair(std::make_pair(moduleHandle_, moduleParamId_), ParameterHandle));
        
        // Ensure host queries return correct value.
        seParameter->upDateImmediateValue();
	}

	// SEM Controllers.
	{
		assert(controllerE);

		auto childPluginsE = controllerE->FirstChildElement("ChildControllers");
		for (const TiXmlElement* childE = childPluginsE->FirstChildElement("ChildController"); childE; childE = childE->NextSiblingElement("ChildController"))
		{
			std::string typeId = childE->Attribute("Type");

			auto mi = ModuleFactory()->GetById(Utf8ToWstring(typeId));

			if (!mi)
			{
				continue;
			}

			gmpi_sdk::mp_shared_ptr<gmpi::IMpUnknown> obj;
			obj.Attach(mi->Build(gmpi::MP_SUB_TYPE_CONTROLLER, true));

			if (obj)
			{
				gmpi_sdk::mp_shared_ptr<gmpi::IMpController> controller;
				/*auto r = */ obj->queryInterface(gmpi::MP_IID_CONTROLLER, controller.asIMpUnknownPtr());

				if (controller)
				{
					int32_t handle = 0;
					childE->QueryIntAttribute("Handle", &(handle));
					semControllers.addController(handle, controller);

					// Duplicating all the pins and defaults seems a bit redundant, they may not even be needed.
					// Perhaps controller needs own dedicated pins????

					// Create IO and autoduplicating Plugs. Set defaults.
					auto plugsElement = childE->FirstChildElement("Pins");

					if (plugsElement)
					{
						int32_t i = 0;
						for (auto plugElement = plugsElement->FirstChildElement(); plugElement; plugElement = plugElement->NextSiblingElement())
						{
							assert(strcmp(plugElement->Value(), "Pin") == 0);

							plugElement->QueryIntAttribute("idx", &i);
							int32_t pinType = 0;
							plugElement->QueryIntAttribute("type", &pinType);
							auto d = plugElement->Attribute("default");

							if (!d)
								d = "";

							controller->setPinDefault(pinType, i, d);

							++i;
						}
					}
				}
			}
		}
	}

	fileWatcher.Start(
		toPlatformString(BundleInfo::instance()->getPresetFolder()),
		[this]()
			{
				// note: called from background thread.
				presetsFolderChanged = true;
			}
	);
    
	undoManager.initial(this);

    isInitialized = true;
}

void MpController::initSemControllers()
{
	if (!isSemControllersInitialised)
	{
		//		_RPT0(_CRT_WARN, "ADelayController::initSemControllers\n");

		for (auto& cp : semControllers.childPluginControllers)
		{
			cp.second->controller_->open();
		}

		isSemControllersInitialised = true;
	}
}

int32_t MpController::getController(int32_t moduleHandle, gmpi::IMpController** returnController)
{
	for (auto& m : semControllers.childPluginControllers)
	{
		if (m.first == moduleHandle)
		{
			*returnController = m.second->controller_;
			break;
		}
	}

	return gmpi::MP_OK;
}

std::vector< MpController::presetInfo > MpController::scanNativePresets()
{
	platform_string PresetFolder = toPlatformString(BundleInfo::instance()->getPresetFolder());

	auto extension = ToPlatformString(getNativePresetExtension());

	return scanPresetFolder(PresetFolder, extension);
}

void MpController::FileToString(const platform_string& path, std::string& buffer)
{
#if 0
	FILE* fp = fopen(path.c_str(), "rb");

	if(fp != NULL)
	{
		/* Go to the end of the file. */
		if(fseek(fp, 0L, SEEK_END) == 0) {
			/* Get the size of the file. */
			auto bufsize = ftell(fp);
			if(bufsize == -1) { /* Error */ }

			/* Allocate our buffer to that size. */
			buffer.resize(bufsize);

			/* Go back to the start of the file. */
			if(fseek(fp, 0L, SEEK_SET) == 0) { /* Error */ }

			/* Read the entire file into memory. */
			size_t newLen = fread((void*)buffer.data(), sizeof(char), bufsize, fp);
			if(newLen == 0) {
				fputs("Error reading file", stderr);
			}
		}
		fclose(fp);
	}
#else
	// fast file read.
	std::ifstream t(path, std::ifstream::in | std::ifstream::binary);
	t.seekg(0, std::ios::end);
	const size_t size = t.tellg();
	if (t.fail())
	{
		buffer.clear();
	}
	else
	{
		buffer.resize(size);
		t.seekg(0);
		t.read((char*)buffer.data(), buffer.size());
	}
#endif
}

MpController::presetInfo MpController::parsePreset(const std::wstring& filename, const std::string& xml)
{
	std::string presetName;

	std::wstring shortName, path_unused, extension;
	decompose_filename(filename, shortName, path_unused, extension);

	{
		presetName = JmUnicodeConversions::WStringToUtf8(shortName);

		// Remove preset number prefix if present. "0023_Sax" -> "Sax"
		if (presetName.size() > 6
			&& presetName[4] == '_'
			&& isdigit(presetName[0])
			&& isdigit(presetName[1])
			&& isdigit(presetName[2])
			&& isdigit(presetName[3])
			)
		{
			presetName = presetName.substr(5);
		}
	}

	TiXmlDocument doc;
	doc.Parse(xml.c_str());

	if (doc.Error())
	{
		return {};
	}

	std::string category;
	auto preset_e = doc.FirstChild("Preset"); // format 3.
	if (!preset_e)
	{
		preset_e = doc.FirstChild("Parameters"); // Format 4. "Parameters" in place of "Preset".
	}
	if (preset_e)
	{
		auto node = preset_e->ToElement();
		node->QueryStringAttribute("category", &category);
		// Name must come from filename, else DAW could save presets with same XML name as another.	node->QueryStringAttribute("name", &presetName);
		// Maybe allow filenames with 4 digits in from to have number truncated 0001_BassMan -> BassMan
	}
	//                    _RPT3(_CRT_WARN, "%s, hash=%d\nXML\n%s\n", presetName.c_str(), (int) std::hash<std::string>{}(xml), xml.c_str());

	return
	{
		presetName,
		category,
		-1,
		ToWstring(filename),
		std::hash<std::string>{}(xml),
		false, // isFactory
		false // isSession
	};
}

std::vector< MpController::presetInfo > MpController::scanPresetFolder(platform_string PresetFolder, platform_string extension)
{
	std::vector< MpController::presetInfo > returnValues;

	const auto searchString = PresetFolder + platform_string(_T("*.")) + extension;
	const bool isXmlPreset = ToUtf8String(extension) == "xmlpreset";

	FileFinder it(searchString.c_str());
	for (; !it.done(); ++it)
	{
		if (!(*it).isFolder)
		{
			const auto sourceFilename = (*it).fullPath;

			std::string xml;
			if (isXmlPreset)
			{
				FileToString(sourceFilename, xml);
            }
			else
			{
                xml = loadNativePreset(ToWstring(sourceFilename));
			}

			returnValues.push_back(parsePreset(ToWstring(sourceFilename), xml));
		}
	}

	return returnValues;
}

void MpController::setParameterValue(RawView value, int32_t parameterHandle, gmpi::FieldType paramField, int32_t voice)
{
	auto it = ParameterHandleIndex.find(parameterHandle);
	if (it == ParameterHandleIndex.end())
	{
		return;
	}

	auto seParameter = (*it).second;

	bool takeUndoSnapshot = false;

	// Special case for MIDI Learn
	if (paramField == gmpi::MP_FT_MENU_SELECTION)
	{
		auto choice = (int32_t)value;// RawToValue<int32_t>(value.data(), value.size());

			// 0 not used as it gets passed erroneously during init
		int cc = 0;
		if (choice == 1) // learn
		{
			cc = ControllerType::Learn;
		}
		else
		{
			if (choice == 2) // un-learn
			{
				cc = ControllerType::None;

				// set automation on GUI to 'none'
				seParameter->MidiAutomation = cc;
				updateGuis(seParameter, gmpi::MP_FT_AUTOMATION);
			}
		}
		/*
		if( choice == 3 ) // Set via dialog
		{
		dlg_assign_controller dlg(getPatchManager(), this, CWnd::GetDesktopWindow());
		dlg.DoModal();
		}
		*/
		// Send MIDI learn message to DSP.
		//---send a binary message
		if (cc != 0)
		{
			my_msg_que_output_stream s(getQueueToDsp(), parameterHandle, "CCID");

			s << (int)sizeof(int);
			s << cc;
			s.Send();
		}
	}
	else
	{
		if (seParameter->setParameterRaw(paramField, value.size(), value.data(), voice))
		{
			seParameter->updateProcessor(paramField, voice);

			if (seParameter->stateful_ && paramField == gmpi::MP_FT_VALUE)
			{
				if (!seParameter->isGrabbed()) // e.g. momentary button
				{
					takeUndoSnapshot = true;
				}
			}
		}
	}

	// take an undo snapshot anytime a knob is released
	if (paramField == gmpi::MP_FT_GRAB)
	{
		const bool grabbed = (bool)value;
		if (!grabbed && seParameter->stateful_)
		{
			takeUndoSnapshot = true;
		}
	}

	if (takeUndoSnapshot)
	{
		setModified(true);

		const auto paramName = WStringToUtf8((std::wstring)seParameter->getValueRaw(gmpi::MP_FT_SHORT_NAME, 0));

		const std::string desc = "Changed parameter: " + paramName;
		undoManager.snapshot(this, desc);
	}
}

void UndoManager::debug()
{
#if 0
	_RPT0(0, "\n======UNDO=======\n");
	for (int i = 0 ; i < size() ; ++i)
	{
		_RPT1(0, "%c", i == undoPosition ? '>' : ' ');
		_RPTN(0, "%s\n", history[i].first.c_str());
	}
#endif
}

void UndoManager::initial(MpController* controller)
{
	history.clear();
	snapshot(controller, {});

	UpdateGui(controller);
}

void UndoManager::initialFromXml(MpController* controller, std::string xml)
{
	history.clear();
	push({}, std::move(xml));
}

bool UndoManager::canUndo()
{
	return undoPosition > 0 && undoPosition < size();
}

bool UndoManager::canRedo()
{
	return undoPosition >= 0 && undoPosition < size() - 1;
}

void UndoManager::UpdateGui(MpController* controller)
{
	*(controller->getHostParameter(HC_CAN_UNDO)) = canUndo();
	*(controller->getHostParameter(HC_CAN_REDO)) = canRedo();
}

void UndoManager::push(std::string description, const std::string& preset)
{
	if (undoPosition < size() - 1)
	{
		history.resize(undoPosition + 1);
	}

	undoPosition = size();
	history.push_back({ description, preset });
}

void UndoManager::snapshot(MpController* controller, std::string description)
{
	if (!enabled)
		return;

	push(description, controller->getPresetXml());

	UpdateGui(controller);
	debug();
}

void UndoManager::undo(MpController* controller)
{
	if (undoPosition <= 0 || undoPosition >= size())
		return;

	--undoPosition;
	controller->setPreset(history[undoPosition].second);

	// if wer're back to the original preset, set modified=false.
	if (!canUndo())
		controller->setModified(false);

	UpdateGui(controller);
	debug();
}

void UndoManager::redo(MpController* controller)
{
	const auto next = undoPosition + 1;
	if (next < 0 || next >= size())
		return;

	controller->setPreset(history[next].second);

	undoPosition = next;

	controller->setModified(true);

	UpdateGui(controller);
	debug();
}

void UndoManager::getA(MpController* controller)
{
	if (AB_is_A)
		return;

	AB_is_A = true;

	auto current = controller->getPresetXml();
	push("Choose A", current);

	controller->setPreset(AB_storage);
	AB_storage = current;
}

void UndoManager::getB(MpController* controller)
{
	if (!AB_is_A)
		return;

	AB_is_A = false;

	// first time clicking 'B' just assign current preset to 'B'
	if (AB_storage.empty())
	{
		AB_storage = controller->getPresetXml();
		return;
	}

	auto current = controller->getPresetXml();
	push("Choose B", current);

	controller->setPreset(AB_storage);
	AB_storage = current;
}

void UndoManager::copyAB(MpController* controller)
{
	if (AB_is_A)
	{
		AB_storage = controller->getPresetXml();
	}
	else
	{
		controller->setPreset(AB_storage);
	}
}

gmpi_gui::IMpGraphicsHost* MpController::getGraphicsHost()
{
#if !defined(SE_USE_JUCE_UI)
	for (auto g : m_guis2)
	{
		auto pa = dynamic_cast<GuiPatchAutomator3*>(g);
		if (pa)
		{
			auto gh = dynamic_cast<gmpi_gui::IMpGraphicsHost*>(pa->getHost());
			if (gh)
				return gh;
		}
	}
#endif

	return nullptr;
}

void MpController::OnSetHostControl(int hostControl, int32_t paramField, int32_t size, const void* data, int32_t voice)
{
	switch (hostControl)
	{
	case HC_PROGRAM:
		if (!inhibitProgramChangeParameter && paramField == gmpi::MP_FT_VALUE)
		{
			auto preset = RawToValue<int32_t>(data, size);

			MpParameter* programNameParam = nullptr;
			for (auto& p : parameters_)
			{
				if (p->getHostControl() == HC_PROGRAM_NAME)
				{
					programNameParam = p.get();
					break;
				}
			}

			if (preset >= 0 && preset < presets.size())
			{
				if (programNameParam)
				{
					const auto nameW = Utf8ToWstring(presets[preset].name);
					const auto raw2 = ToRaw4(nameW);
					const auto field = gmpi::MP_FT_VALUE;
					if(programNameParam->setParameterRaw(field, raw2.size(), raw2.data()))
					{
						programNameParam->updateProcessor(field, voice);
					}
				}

				if (presets[preset].isSession)
				{
					setPreset(session_preset_xml);
				}
				else if (presets[preset].isFactory)
				{
					loadFactoryPreset(preset, false);
				}
				else
				{
					std::string xml;
					if (presets[preset].filename.find(L".xmlpreset") != string::npos)
					{
						platform_string nativePath = toPlatformString(presets[preset].filename);
						FileToString(nativePath, xml);
					}
					else
					{
						xml = loadNativePreset(presets[preset].filename);
					}
					setPreset(xml);
				}

				undoManager.initial(this);
				setModified(false);
			}
		}
		break;


	case HC_PATCH_COMMANDS:
		if (paramField == gmpi::MP_FT_VALUE)
		{
			const auto patchCommand = *(int32_t*)data;

            if(patchCommand <= 0)
                break;
            
            // JUCE toolbar commands
			switch (patchCommand)
			{
			case (int) EPatchCommands::Undo:
				undoManager.undo(this);
				break;

			case (int)EPatchCommands::Redo:
				undoManager.redo(this);
				break;

			case (int)EPatchCommands::CompareGet_A:
				undoManager.getA(this);
				break;

			case (int)EPatchCommands::CompareGet_B:
				undoManager.getB(this);
				break;

			case (int)EPatchCommands::CompareGet_CopyAB:
				undoManager.copyAB(this);
				break;

			default:
				break;
			};

#if !defined(SE_USE_JUCE_UI)
            // L"Load Preset=2,Save Preset,Import Bank,Export Bank"
            if (patchCommand > 5)
                break;

			auto gh = getGraphicsHost();

			if (!gh)
                break;
            
            int dialogMode = (patchCommand == 2 || patchCommand == 4) ? 0 : 1; // load or save.
            nativeFileDialog = nullptr; // release any existing dialog.
            gh->createFileDialog(dialogMode, nativeFileDialog.GetAddressOf());

            if (nativeFileDialog.isNull())
                break;
            
            if (patchCommand > 3)
            {
                nativeFileDialog.AddExtension("xmlbank", "XML Bank");
                auto fullPath = WStringToUtf8(BundleInfo::instance()->getUserDocumentFolder());
                combinePathAndFile(fullPath.c_str(), "bank.xmlbank");
                nativeFileDialog.SetInitialFullPath(fullPath);
            }
            else
            {
                const auto presetFolder = BundleInfo::instance()->getPresetFolder();
                CreateFolderRecursive(presetFolder);

				// default extension is the first one.
				if (getNativePresetExtension() == L"vstpreset")
				{
					nativeFileDialog.AddExtension("vstpreset", "VST3 Preset");
					//#ifdef _WIN32
					//					nativeFileDialog.AddExtension("fxp", "VST2 Preset");
					//#endif
				}
				else
				{
					nativeFileDialog.AddExtension("aupreset", "Audio Unit Preset");
				}
                nativeFileDialog.AddExtension("xmlpreset", "XML Preset");

				// least-relevant option last
				if (getNativePresetExtension() == L"vstpreset")
				{
					nativeFileDialog.AddExtension("aupreset", "Audio Unit Preset");
				}
				else
				{
					nativeFileDialog.AddExtension("vstpreset", "VST3 Preset");
				}
                nativeFileDialog.AddExtension("*", "All Files");
                nativeFileDialog.SetInitialFullPath(WStringToUtf8(presetFolder));
            }

            nativeFileDialog.ShowAsync([this, patchCommand](int32_t result) -> void { this->OnFileDialogComplete(patchCommand, result); });
#endif
		}

		break;
	}
}

int32_t MpController::sendSdkMessageToAudio(int32_t handle, int32_t id, int32_t size, const void* messageData)
{
	auto queue = getQueueToDsp();

	// discard any too-big message.
	const auto totalMessageSize = 4 * static_cast<int>(sizeof(int)) + size;
	if(totalMessageSize > queue->freeSpace())
		return gmpi::MP_FAIL;

	my_msg_que_output_stream s(queue, (int32_t)handle, "sdk\0");
    
	s << (int32_t)(size + 2 * sizeof(int32_t)); // size of ID plus sizeof message.

	s << id;

	s << size;
	s.Write(messageData, size);

    s.Send();
    
	return gmpi::MP_OK;
}

#if 0
// these can't update processor with normal handle-based method becuase their handles are assigned at runtime, only in the controller.
void MpController::HostControlToDsp(MpParameter* param, int32_t voice)
{
	assert(param->getHostControl() >= 0);

	my_msg_que_output_stream s(getQueueToDsp(), UniqueSnowflake::APPLICATION, "hstc");

	SerialiseParameterValueToDsp(s, param);
}
#endif

void MpController::SerialiseParameterValueToDsp(my_msg_que_output_stream& stream, MpParameter* param)
{
	//---send a binary message
	const int32_t voice = 0;
	bool isVariableSize = param->datatype_ == DT_TEXT || param->datatype_ == DT_BLOB;

	auto raw = param->getValueRaw(gmpi::MP_FT_VALUE, voice);

	bool due_to_program_change = false;
	int32_t recievingMessageLength = (int)(sizeof(bool) + raw.size());
	if (isVariableSize)
	{
		recievingMessageLength += (int)sizeof(int32_t);
	}

	if (param->isPolyPhonic())
	{
		recievingMessageLength += (int)sizeof(int32_t);
	}

	stream << recievingMessageLength;
	stream << due_to_program_change;

	if (param->isPolyPhonic())
	{
		stream << voice;
	}

	if (isVariableSize)
	{
		stream << (int32_t)raw.size();
	}

	stream.Write(raw.data(), (unsigned int)raw.size());

	stream.Send();
}

void MpController::ParamToDsp(MpParameter* param, int32_t voice)
{
	assert(dynamic_cast<SeParameter_vst3_hostControl*>(param) == nullptr); // These have (not) "unique" handles that may map to totally random DSP parameters.

	my_msg_que_output_stream s(getQueueToDsp(), param->parameterHandle_, "ppc\0"); // "ppc"
	SerialiseParameterValueToDsp(s, param);
}

void MpController::UpdateProgramCategoriesHc(MpParameter* param)
{
	ListBuilder_base<char> l;
	for (auto& preset : presets)
	{
		if(param->getHostControl() == HC_PROGRAM_CATEGORIES_LIST)
			l.Add(preset.category);
		else
		{
			assert(param->getHostControl() == HC_PROGRAM_NAMES_LIST);
			l.Add(preset.name);
		}
	}
	std::wstring_convert<std::codecvt_utf8<wchar_t>> convert;

	auto enumList = convert.from_bytes(l.str());

	param->setParameterRaw(gmpi::FieldType::MP_FT_VALUE, RawView(enumList));
}

MpParameter* MpController::createHostParameter(int32_t hostControl)
{
	SeParameter_vst3_hostControl* p = {};

	switch (hostControl)
	{
	case HC_PATCH_COMMANDS:
		p = new SeParameter_vst3_hostControl(this, hostControl);
		p->enumList_ = L"Load Preset=2,Save Preset,Import Bank,Export Bank";
		if (undoManager.enabled)
		{
			p->enumList_ += L", Undo=17, Redo";
		}
		break;

	case HC_CAN_UNDO:
	case HC_CAN_REDO:
		p = new SeParameter_vst3_hostControl(this, hostControl);
		break;

	case HC_PROGRAM:
	{
		p = new SeParameter_vst3_hostControl(this, hostControl);
		p->datatype_ = DT_INT;
		p->maximum = (std::max)(0.0, static_cast<double>(presets.size() - 1));
		const int32_t initialVal = -1; // ensure patch-browser shows <NULL> at first.
		RawView raw(initialVal);
		p->setParameterRaw(gmpi::MP_FT_VALUE, (int32_t)raw.size(), raw.data());
	}
	break;

	case HC_PROGRAM_NAME:
		p = new SeParameter_vst3_hostControl(this, hostControl);
		{
			auto raw2 = ToRaw4(L"Factory");
			p->setParameterRaw(gmpi::MP_FT_VALUE, (int32_t)raw2.size(), raw2.data());
		}
		break;

	case HC_PROGRAM_NAMES_LIST:
	{
		auto param = new SeParameter_vst3_hostControl(this, hostControl);
		p = param;
		p->datatype_ = DT_TEXT;

		UpdateProgramCategoriesHc(param);
	}
	break;

	case HC_PROGRAM_CATEGORIES_LIST:
	{
		auto param = new SeParameter_vst3_hostControl(this, hostControl);
		p = param;
		p->datatype_ = DT_TEXT;

		UpdateProgramCategoriesHc(param);
	}
	break;

	case HC_PROGRAM_MODIFIED:
	{
		auto param = new SeParameter_vst3_hostControl(this, hostControl);
		p = param;
		p->datatype_ = DT_BOOL;
	}
	break;


	/* what would it do?
	case HC_MIDI_CHANNEL:
	break;
	*/
	}

	if (!p)
		return {};

	p->stateful_ = false;

	// clashes with valid handles on DSP, ensure NEVER sent to DSP!!

	// generate unique parameter handle, assume all other parameters already registered.
	p->parameterHandle_ = 0;
	if (!ParameterHandleIndex.empty())
		p->parameterHandle_ = ParameterHandleIndex.rbegin()->first + 1;

	ParameterHandleIndex.insert(std::make_pair(p->parameterHandle_, p));
	parameters_.push_back(std::unique_ptr<MpParameter>(p));

	return p;
}

MpParameter* MpController::getHostParameter(int32_t hostControl)
{
	const auto it = std::find_if(
		parameters_.begin()
		, parameters_.end()
		, [hostControl](std::unique_ptr<MpParameter>& p) {return p->getHostControl() == hostControl; }
	);
	
	if(it != parameters_.end())
		return (*it).get();

	return createHostParameter(hostControl);
}

int32_t MpController::getParameterHandle(int32_t moduleHandle, int32_t moduleParameterId)
{
	int hostControl = -1 - moduleParameterId;

	if (hostControl >= 0)
	{
		// why not just shove it in with negative handle? !!! A: becuase of potential attachment to container.
		for (auto& p : parameters_)
		{
			if (p->getHostControl() == hostControl && (moduleHandle == -1 || moduleHandle == p->ModuleHandle()))
			{
				return p->parameterHandle_;
				break;
			}
		}

		if (auto p = createHostParameter(hostControl); p)
		{
			return p->parameterHandle_;
		}
	}
	else
	{
		auto it = moduleParameterIndex.find(std::make_pair(moduleHandle, moduleParameterId));
		if (it != moduleParameterIndex.end())
			return (*it).second;
	}

	return -1;
}

void MpController::initializeGui(gmpi::IMpParameterObserver* gui, int32_t parameterHandle, gmpi::FieldType FieldId)
{
	auto it = ParameterHandleIndex.find(parameterHandle);

	if (it != ParameterHandleIndex.end())
	{
		auto p = (*it).second;

		for (int voice = 0; voice < p->getVoiceCount(); ++voice)
		{
			auto raw = p->getValueRaw(FieldId, voice);
			gui->setParameter(parameterHandle, FieldId, voice, raw.data(), (int32_t)raw.size());
		}
	}
}

bool MpController::onQueMessageReady(int recievingHandle, int recievingMessageId, class my_input_stream& p_stream)
{
	auto it = ParameterHandleIndex.find(recievingHandle);
	if (it != ParameterHandleIndex.end())
	{
		auto p = (*it).second;
		p->updateFromDsp(recievingMessageId, p_stream);
		return true;
	}
	else
	{
		switch(recievingMessageId)
		{
		case id_to_long2("sdk"):
		{
			struct DspMsgInfo2
			{
				int id;
				int size;
				void* data;
				int handle;
			};
			DspMsgInfo2 nfo;
			p_stream >> nfo.id;
			p_stream >> nfo.size;
			nfo.data = malloc(nfo.size);
			p_stream.Read(nfo.data, nfo.size);
			nfo.handle = recievingHandle;

			if (presenter_)
				presenter_->OnChildDspMessage(&nfo);

			free(nfo.data);

			return true;
		}
		break;

		case id_to_long2("ltnc"): // latency changed. VST3 or AU.
		{
			OnLatencyChanged();
		}
		break;
		}
	}
	return false;
}

bool MpController::OnTimer()
{
	message_que_dsp_to_ui.pollMessage(this);

	if (startupTimerCounter-- == 0)
	{
		OnStartupTimerExpired();
	}
	
	if (presetsFolderChanged)
	{
		presetsFolderChanged = false;
		ScanPresets();
		UpdatePresetBrowser();
	}

	return true;
}

void MpController::OnStartupTimerExpired()
{
	if (BundleInfo::instance()->getPluginInfo().emulateIgnorePC)
	{
		ignoreProgramChange = true;

		my_msg_que_output_stream s(getQueueToDsp(), UniqueSnowflake::APPLICATION, "EIPC"); // Emulate Ignore Program Change
		s << (uint32_t)0;
		s.Send();
	}
}

int32_t MpController::resolveFilename(const wchar_t* shortFilename, int32_t maxChars, wchar_t* returnFullFilename)
{
	// copied from CSynthEditAppBase.

	std::wstring l_filename(shortFilename);
	std::wstring file_ext;
	file_ext = GetExtension(l_filename);

    const bool isUrl = l_filename.find(L"://") != string::npos;
    
    // Is this a relative or absolute filename?
#ifdef _WIN32
    const bool has_root_path = l_filename.find(L':') != string::npos;
#else
    const bool has_root_path = l_filename.size() > 0 && l_filename[0] == L'/';
#endif
    
	if (!has_root_path && !isUrl)
	{
		auto default_path = BundleInfo::instance()->getImbeddedFileFolder();

		l_filename = combine_path_and_file(default_path, l_filename);
	}

	if (l_filename.size() >= static_cast<size_t> (maxChars))
	{
		// return empty string (if room).
		if (maxChars > 0)
			returnFullFilename[0] = 0;

		return gmpi::MP_FAIL;
	}

	WStringToWchars(l_filename, returnFullFilename, maxChars);

	return gmpi::MP_OK;
}

void MpController::OnFileDialogComplete(int patchCommand, int32_t result)
{
	if (result == gmpi::MP_OK)
	{
		auto fullpath = nativeFileDialog.GetSelectedFilename();
		auto filetype = GetExtension(fullpath);
		bool isXmlPreset = filetype == "xmlpreset";

		switch (patchCommand) // L"Load Preset=2,Save Preset,Import Bank,Export Bank"
		{
		case 2:	// Load Preset
			if (isXmlPreset)
				ImportPresetXml(fullpath.c_str());
			else
			{
				auto xml = loadNativePreset( Utf8ToWstring(fullpath) );
                setPresetFromDaw(xml, true); // will update preset browser if appropriate
			}
			break;

		case 3:	// Save Preset
			if (isXmlPreset)
				ExportPresetXml(fullpath.c_str());
			else
			{
				// Update preset name and category, so filename matches name in browser (else very confusing).
				std::wstring r_file, r_path, r_extension;
				decompose_filename(Utf8ToWstring(fullpath), r_file, r_path, r_extension);

				// Update program name and cateogry (as they are queried by getPresetXml() ).
				for (auto& p : parameters_)
				{
					if (p->getHostControl() == HC_PROGRAM_NAME)
					{
						p->setParameterRaw(gmpi::FieldType::MP_FT_VALUE, RawView(r_file));
						p->updateProcessor(gmpi::FieldType::MP_FT_VALUE, 0); // Important that processor has correct name when DAW saves the session.
						updateGuis(p.get(), gmpi::FieldType::MP_FT_VALUE);
					}

					// Presets saved by user go into "User" category.
					if (p->getHostControl() == HC_PROGRAM_CATEGORY)
					{
						std::wstring category{L"User"};
						p->setParameterRaw(gmpi::FieldType::MP_FT_VALUE, RawView(category));
						p->updateProcessor(gmpi::FieldType::MP_FT_VALUE, 0);
						updateGuis(p.get(), gmpi::FieldType::MP_FT_VALUE);
					}
				}

				saveNativePreset(fullpath.c_str(), WStringToUtf8(r_file), getPresetXml());

				ScanPresets();
				UpdatePresetBrowser();

				// Update current preset name in browser.
				for (auto& p : parameters_)
				{
					if (p->getHostControl() == HC_PROGRAM)
					{
						auto nameU = WStringToUtf8(r_file);

						for (int32_t i = 0; i < presets.size(); ++i)
						{
							if (presets[i].name == nameU && presets[i].category == "User")
							{
								p->setParameterRaw(gmpi::FieldType::MP_FT_VALUE, RawView(i));

								updateGuis(p.get(), gmpi::FieldType::MP_FT_VALUE);
								break;
							}
						}
					}
				}
			}
			break;

		case 4:
			ImportBankXml(fullpath.c_str());
			break;

		case 5:
			ExportBankXml(fullpath.c_str());
			break;
		}
	}

	nativeFileDialog.setNull(); // release it.
}

void MpController::ImportPresetXml(const char* filename, int presetIndex)
{
	TiXmlDocument doc;
	doc.LoadFile(filename);

	if (doc.Error())
	{
		assert(false);
		return;
	}

	TiXmlHandle hDoc(&doc);

	setPreset(hDoc.ToNode(), true, presetIndex);
}

std::string MpController::getPresetXml(std::string presetNameOverride)
{
	std::string presetName, presetCategory;

	// Sort for export consistancy.
	list<MpParameter*> sortedParameters;
	for (auto& p : parameters_)
	{
		if (p->getHostControl() == HC_PROGRAM_NAME)
		{
			presetName = WStringToUtf8( (std::wstring) p->getValueRaw(gmpi::FieldType::MP_FT_VALUE, 0));
			continue; // force non-save
		}
		if (p->getHostControl() == HC_PROGRAM_CATEGORY)
		{
			presetCategory = WStringToUtf8((std::wstring) p->getValueRaw(gmpi::FieldType::MP_FT_VALUE, 0));
			continue; // force non-save
		}

		if (p->stateful_)
		{
			sortedParameters.push_back(p.get());
		}
	}
	sortedParameters.sort([](MpParameter* a, MpParameter* b) -> bool
	{
		if (a->getNativeTag() != a->getNativeTag())
			return a->getNativeTag() < b->getNativeTag();

		return a->parameterHandle_ < b->parameterHandle_;
	});

	if (!presetNameOverride.empty())
	{
		presetName = presetNameOverride;
	}

	TiXmlDocument doc;
	TiXmlDeclaration* decl = new TiXmlDeclaration("1.0", "", "");
	doc.LinkEndChild(decl);

	auto element = new TiXmlElement("Preset");
	doc.LinkEndChild(element);

	{
		char buffer[20];
		sprintf(buffer, "%08x", BundleInfo::instance()->getPluginId());
		element->SetAttribute("pluginId", buffer);
	}

	if (!presetName.empty())
		element->SetAttribute("name", presetName);

	if (!presetCategory.empty())
		element->SetAttribute("category", presetCategory);

	for (auto parameter : sortedParameters)
	{
		auto paramElement = new TiXmlElement("Param");
		element->LinkEndChild(paramElement);
		paramElement->SetAttribute("id", parameter->parameterHandle_);

		const int voice = 0;
		auto raw = parameter->getValueRaw(gmpi::MP_FT_VALUE, voice);
		// nope turns blanks into double apostrophes:		string val = QuoteStringIfSpaces(RawToUtf8B(parameter->datatype_, raw.data(), raw.size()));
		string val = RawToUtf8B(parameter->datatype_, raw.data(), raw.size());

		paramElement->SetAttribute("val", val);

		// MIDI learn.
		if (parameter->MidiAutomation != -1)
		{
			paramElement->SetAttribute("MIDI", parameter->MidiAutomation);

			if (!parameter->MidiAutomationSysex.empty())
				paramElement->SetAttribute("MIDI_SYSEX", WStringToUtf8(parameter->MidiAutomationSysex));
		}
	}

	TiXmlPrinter printer;
	printer.SetIndent(" ");
	doc.Accept(&printer);

	return printer.CStr();
}

int32_t MpController::getParameterModuleAndParamId(int32_t parameterHandle, int32_t* returnModuleHandle, int32_t* returnModuleParameterId)
{
	auto it = ParameterHandleIndex.find(parameterHandle);
	if (it != ParameterHandleIndex.end())
	{
		auto seParameter = (*it).second;
		*returnModuleHandle = seParameter->moduleHandle_;
		*returnModuleParameterId = seParameter->moduleParamId_;
		return gmpi::MP_OK;
	}
	return gmpi::MP_FAIL;
}

RawView MpController::getParameterValue(int32_t parameterHandle, int32_t fieldId, int32_t voice)
{
	auto it = ParameterHandleIndex.find(parameterHandle);
	if (it != ParameterHandleIndex.end())
	{
		auto param = (*it).second;
		return param->getValueRaw((gmpi::FieldType) fieldId, 0);
	}

	return {};
}

void MpController::setPreset(TiXmlNode* parentXml, bool updateProcessor, int preset)
{
	// see also CPatchManager::ImportPresetXml()

	/* Possible formats.
		1) PatchManager/Parameters/Parameter/Preset/patch-list
		2)      Presets/Parameters/Parameter/Preset/patch-list
		3) Presets/Preset/Param.val
		4)     Parameters/Param.val
		5)         Preset/Param.val
	*/

	TiXmlNode* presetXml = nullptr;
	auto presetsXml = parentXml->FirstChild("Presets");
	if (presetsXml) // exported from SE has Presets/Preset
	{
		int presetIndex = 0;
		for (auto node = presetsXml->FirstChild("Preset"); node; node = node->NextSibling("Preset"))
		{
			presetXml = node;
			if (presetIndex++ >= preset)
				break;
		}
	}
	else
	{
		// Individual preset.
		presetXml = parentXml->FirstChildElement("Parameters"); // Format 4. "Parameters" in place of "Preset".
		if (!presetXml)
		{
			presetXml = parentXml->FirstChildElement("Preset"); // Format 5. (VST2 fxp preset).
		}
	}

	TiXmlNode* parametersE = nullptr;
	if (presetXml)
	{
		// Individual preset has Preset.Param.val or Parameters.Param.val
		parametersE = presetXml;
	}
	else
	{
		auto controllerE = parentXml->FirstChild("Controller");

		// should always have a valid root but handle gracefully if it does
		if (!controllerE)
			return;

		auto patchManagerE = controllerE->FirstChildElement();
		assert(strcmp(patchManagerE->Value(), "PatchManager") == 0);

		// Internal preset has parameters wrapping preset values. (PatchManager.Parameters.Parameter.patch-list)
		parametersE = patchManagerE->FirstChild("Parameters")->ToElement();

		// Could be either of two formats. Internal (PatchManager.Parameter.patch-list), or vstpreset V1.3 (Parameters.Param)
		auto ParamElement13 = parametersE->FirstChildElement("Param"); /// is this a V1.3 vstpreset?
		if (ParamElement13)
		{
			presetXml = parametersE;
		}
	}

	if (parametersE == nullptr)
		return;

	if(updateProcessor)
		OnStartPresetChange();

	if (presetXml)
	{
        
//        _RPT0(_CRT_WARN, "\n\n===============================\nPRESETS: Controller Set Preset\n");
/*
		// TODO reset non-imported params
		if(updateProcessor)
		{
			TiXmlPrinter printer;
			printer.SetIndent("");
			presetXml->Accept(&printer);

	       	my_msg_que_output_stream s(&queueToDsp_, UniqueSnowflake::APPLICATION, "prst"); // preset.
			const int32_t totalBytes = sizeof(int32_t) + printer.Str().size();
			s << totalBytes;
			s << printer.Str();
            s.Send();

			_RPT0(_CRT_WARN, "PRESETS: Sent XML to Processor\n");
		}
*/
		auto presetXmlElement = presetXml->ToElement();

		// Query plugin's 4-char code. Presence Indicates also that preset format supports MIDI learn.
		int32_t fourCC = -1;
		int formatVersion = 0;
		{
			std::string hexcode;
			if (TIXML_SUCCESS == presetXmlElement->QueryStringAttribute("pluginId", &hexcode))
			{
				formatVersion = 1;
				try
				{
					fourCC = std::stoul(hexcode.c_str(), nullptr, 16);
				}
				catch (...)
				{
					// who gives a f*ck
				}
			}
		}

		// !!! TODO: Check 4-char ID correct.
		const int voiceId = 0;

		std::string categoryName;
		presetXmlElement->QueryStringAttribute("category", &categoryName);
		{
			const std::wstring categoryNameW = Utf8ToWstring(categoryName);
			auto parameterHandle = getParameterHandle(-1, -1 - HC_PROGRAM_CATEGORY);
			auto it = ParameterHandleIndex.find(parameterHandle);
			if (it != ParameterHandleIndex.end())
			{
				auto p = (*it).second;
				p->setParameterRaw(gmpi::FieldType::MP_FT_VALUE, RawView(categoryNameW)); // don't check changed flag, if even originated from GUI, param is already changed. Still need top go to DSP.
				if(updateProcessor)
				{
					p->updateProcessor(gmpi::MP_FT_VALUE, voiceId);
				}
			}
		}

		std::string presetName;
		presetXmlElement->QueryStringAttribute("name", &presetName);
		{
			const std::wstring nameW = Utf8ToWstring(presetName);
			auto parameterHandle = getParameterHandle(-1, -1 - HC_PROGRAM_NAME);
			auto it = ParameterHandleIndex.find(parameterHandle);
			if (it != ParameterHandleIndex.end())
			{
				auto p = (*it).second;
				p->setParameterRaw(gmpi::FieldType::MP_FT_VALUE, RawView(nameW)); // don't check changed flag, if even originated from GUI, param is already changed. Still need top go to DSP.
				if(updateProcessor)
				{
					p->updateProcessor(gmpi::MP_FT_VALUE, voiceId);
				}
			}
		}

		// assuming we are passed "Preset.Parameters" node.
		for (auto node = presetXml->FirstChild("Param"); node; node = node->NextSibling("Param"))
		{
			// check for existing
			auto ParamElement = node->ToElement();

			int paramHandle = -1;
			ParamElement->QueryIntAttribute("id", &paramHandle);

			if (paramHandle == -1)
				continue;

			auto it = ParameterHandleIndex.find(paramHandle);
			if (it == ParameterHandleIndex.end())
				continue;

			auto& parameter = (*it).second;
			if (!parameter->stateful_) // For VST2 wrapper aeffect ptr. prevents it being inadvertantly zeroed.
				continue;

			if (parameter->ignorePc_ && ignoreProgramChange) // a short time period after plugin loads, ignore-PC parameters will no longer overwrite existing value.
				continue;

#if 0 // test for race-conditions def _DEBUG
                std::this_thread::sleep_for(30ms);
#endif
			const std::string v = ParamElement->Attribute("val");

			const auto raw = ParseToRaw(parameter->datatype_, v);

			// This block seems messy. Should updating a parameter be a single function call?
						// (would need to pass 'updateProcessor')
						{
							// calls controller_->updateGuis(this, voice)
						parameter->setParameterRaw(gmpi::MP_FT_VALUE, (int32_t)raw.size(), raw.data(), voiceId);

						// updated cached value.
						parameter->upDateImmediateValue();

						if(updateProcessor) // For non-private parameters, update DAW.
						{
							parameter->updateProcessor(gmpi::MP_FT_VALUE, voiceId);
							}
						}
                        
                        // MIDI learn.
                        if(updateProcessor && formatVersion > 0)
                        {
                            int32_t midiController = -1;
                            ParamElement->QueryIntAttribute("MIDI", &midiController);
                                {
                                    my_msg_que_output_stream s(getQueueToDsp(), parameter->parameterHandle_, "CCID");
                                    s << static_cast<uint32_t>(sizeof(midiController));
                                    s << midiController;
                                    s.Send();
                                }

                                std::string sysexU;
                                ParamElement->QueryStringAttribute("MIDI_SYSEX", &sysexU);
                                {
                                    const auto sysex = Utf8ToWstring(sysexU);
                                    
                                    my_msg_que_output_stream s(getQueueToDsp(), parameter->parameterHandle_, "CCSX");
                                    s << static_cast<uint32_t>(sizeof(int32_t) + sizeof(wchar_t) * sysex.size());
                                    s << sysex;
                                    s.Send();
                                }
                            }
		}
	}
	else // Old-style. 'Internal' Presets
	{
		// assuming we are passed "PatchManager.Parameters" node.
		for (auto ParamElement = parametersE->FirstChildElement("Parameter"); ParamElement; ParamElement = ParamElement->NextSiblingElement("Parameter"))
		{
			// VST3 "Controler" XML uses "Handle", VST3 presets use "id" for the same purpose.
			int paramHandle = -1;
			ParamElement->QueryIntAttribute("Handle", &paramHandle);

			auto it = ParameterHandleIndex.find(paramHandle);
			if (it != ParameterHandleIndex.end())
			{
				auto& parameter = (*it).second;

				// Preset values from patch list.
				ParseXmlPreset(
					ParamElement,
					[parameter](int voiceId, int preset, const char* xmlvalue)
					{
						const auto raw = ParseToRaw(parameter->datatype_, xmlvalue);
						if (parameter->setParameterRaw(gmpi::MP_FT_VALUE, (int32_t)raw.size(), raw.data(), voiceId))
						{
							// updated cached value.
							parameter->upDateImmediateValue();
							parameter->updateProcessor(gmpi::MP_FT_VALUE, voiceId);
						}
					}
				);
			}
		}
	}

	if (updateProcessor)
		OnEndPresetChange();
}

void MpController::OnEndPresetChange()
{
	// try to 'debounce' multiple preset changes at startup.
	if (startupTimerCounter > 0)
	{
		startupTimerCounter = startupTimerInit;
	}
}

// xml may contain multiple presets, or just one.
void MpController::setPreset(const std::string& xml, bool updateProcessor, int preset)
{
	TiXmlDocument doc;
	doc.Parse(xml.c_str());

	if (doc.Error())
	{
		assert(false);
		return;
	}

	setPreset(&doc, updateProcessor, preset);
}

void MpController::setPresetFromDaw(const std::string& xml, bool updateProcessor)
{
	setPreset(xml, updateProcessor);

	std::string presetName;
	{
		auto parameterHandle = getParameterHandle(-1, -1 - HC_PROGRAM_NAME);
		if (auto it = ParameterHandleIndex.find(parameterHandle) ; it != ParameterHandleIndex.end())
		{
			const auto raw = (*it).second->getValueRaw(gmpi::FieldType::MP_FT_VALUE, 0);
			auto presetNameW = RawToValue<std::wstring>(raw.data(), raw.size());
			presetName = WStringToUtf8(presetNameW);
		}
	}

	// When DAW loads preset XML, try to determine if it's a factory preset, and update browser to suit.
	int32_t presetIndex = -1; // exact match
	int32_t presetSameNameIndex = -1; // name matches, but not settings.

	/*
	XML will not match if any parameter was set outside the normalized range, because it will get clamped in the plugin.
	*/
  //  _RPT2(_CRT_WARN, "setPresetFromDaw: hash=%d\nXML:\n%s\n", (int) std::hash<std::string>{}(xml), xml.c_str());
	auto hash = std::hash<std::string>{}(xml);

	// If preset has no name, treat it as a (modified) "Default"
	if (presetName.empty())
		presetName = "Default";

	// Check if preset coincides with a factory preset, if so update browser to suit.
	int idx = 0;
	for (const auto& preset : presets)
	{
		if (preset.hash == hash)
		{
			presetIndex = idx;
			break;
		}
		if (preset.name == presetName && !preset.isSession)
		{
			presetSameNameIndex = idx;
			break;
		}
		
		++idx;
	}

	if (presetIndex == -1)
	{
		if (presetSameNameIndex != -1)
		{
			// assume it's the same preset, except it's been modified
			presetIndex = presetSameNameIndex;

			// put original as undo state
			std::string newXml;
			const platform_string nativePath = toPlatformString(presets[presetIndex].filename);
			FileToString(nativePath, newXml);
			undoManager.initialFromXml(this, newXml);
			undoManager.snapshot(this, "Load Session Preset");
			setModified(true);
		}
		else
		{
			// remove any existing "Session preset"
			presets.erase(
				std::remove_if(presets.begin(), presets.end(), [](presetInfo& preset) { return preset.isSession; })
				, presets.end()
			);
			session_preset_xml.clear();
			
			// preset not available and not the same name as any existing ones, add it to presets as 'session' preset.
			presetIndex = static_cast<int32_t>(presets.size());
			presets.push_back(
				parsePreset({}, xml)
			);
			presets.back().name = presetName;
			presets.back().isSession = true;
			session_preset_xml = xml;
		}
	}

	{
		auto parameterHandle = getParameterHandle(-1, -1 - HC_PROGRAM);
		auto it = ParameterHandleIndex.find(parameterHandle);
		if (it != ParameterHandleIndex.end())
		{
			auto p = (*it).second;
			inhibitProgramChangeParameter = true;
			if(p->setParameterRaw(gmpi::FieldType::MP_FT_VALUE, RawView(presetIndex)))
			{
// VST2 only I think				p->updateProcessor(gmpi::FieldType::MP_FT_VALUE, 0); // Unusual. Informs VST2 DAW of program number.
			}

			inhibitProgramChangeParameter = false;
		}
		parameterHandle = getParameterHandle(-1, -1 - HC_PROGRAM_NAME);
		it = ParameterHandleIndex.find(parameterHandle);
		if (it != ParameterHandleIndex.end())
		{
			auto p = (*it).second;

			std::wstring name;
			if(presetIndex == -1)
			{
				const auto raw = p->getValueRaw(gmpi::FieldType::MP_FT_VALUE, 0);
				name = RawToValue<std::wstring>(raw.data(), raw.size());
			}
			else
			{
				// Preset found.
				name = Utf8ToWstring(presets[presetIndex].name);
			}

			if(p->setParameterRaw(gmpi::FieldType::MP_FT_VALUE, RawView(name)) && updateProcessor)
			{
				p->updateProcessor(gmpi::FieldType::MP_FT_VALUE, 0);
			}
		}
	}
}

bool MpController::isPresetModified()
{
	return undoManager.canUndo();
}

void MpController::SavePreset(int32_t presetIndex)
{
	const auto presetFolderW = BundleInfo::instance()->getPresetFolder();
	CreateFolderRecursive(presetFolderW);

	auto preset = getPresetInfo(presetIndex);
	const auto presetFolder = WStringToUtf8(presetFolderW);
	const auto fullPath = combinePathAndFile(presetFolder, preset.name) + ".xmlpreset";

	ExportPresetXml(fullPath.c_str());

	setModified(false);
	undoManager.initial(this);

	ScanPresets();
	UpdatePresetBrowser();
}

void MpController::SavePresetAs(const std::string& presetName)
{
	const auto presetFolderW = BundleInfo::instance()->getPresetFolder();
	CreateFolderRecursive(presetFolderW);

	const auto presetFolder = WStringToUtf8(presetFolderW);
	const auto fullPath = combinePathAndFile(presetFolder, presetName ) + ".xmlpreset";

	ExportPresetXml(fullPath.c_str(), presetName);

	setModified(false);

	ScanPresets();

	// find the new preset
	for (int32_t presetIndex = 0; presetIndex < presets.size(); ++presetIndex)
	{
		if (presets[presetIndex].name == presetName)
		{
			auto parameterHandle = getParameterHandle(-1, -1 - HC_PROGRAM);
			auto it = ParameterHandleIndex.find(parameterHandle);
			if (it != ParameterHandleIndex.end())
			{
				auto p = (*it).second;
				p->setParameterRaw(gmpi::FieldType::MP_FT_VALUE, RawView(presetIndex));
			}
			break;
		}
	}

	UpdatePresetBrowser();
}

void MpController::DeletePreset(int presetIndex)
{
	assert(presetIndex >= 0 && presetIndex < presets.size());

	auto parameterHandle = getParameterHandle(-1, -1 - HC_PROGRAM);
	auto it = ParameterHandleIndex.find(parameterHandle);
	if (it != ParameterHandleIndex.end())
	{
		auto p = (*it).second;

		auto currentPreset = (int32_t) p->getValueRaw(gmpi::FieldType::MP_FT_VALUE, 0);

		// if we're delteing the current preset, switch back to preset 0
		if (currentPreset == presetIndex)
		{
			int32_t newCurrentPreset = 0;
			(*p) = newCurrentPreset;
		}
	}
#if defined(__cpp_lib_filesystem)
	std::filesystem::remove(presets[presetIndex].filename);
#else
    std::remove(WStringToUtf8(presets[presetIndex].filename).c_str());
#endif

	ScanPresets();
	UpdatePresetBrowser();
}

// Note: Don't handle polyphonic stateful parameters.
void MpController::ExportPresetXml(const char* filename, std::string presetNameOverride)
{
	ofstream myfile;
	myfile.open(filename);
	myfile << getPresetXml(presetNameOverride);
	myfile.close();
}

void MpController::ExportBankXml(const char* filename)
{
	// Create output XML document.
	tinyxml2::XMLDocument xml;
	xml.LinkEndChild(xml.NewDeclaration());

	auto presets_xml = xml.NewElement("Presets");
	xml.LinkEndChild(presets_xml);

	// Iterate native preset files, combine them into bank, and export.
	auto srcFolder = ToPlatformString(BundleInfo::instance()->getPresetFolder());
	auto searchString = srcFolder + _T("*.");
	searchString += ToPlatformString(getNativePresetExtension());
	for (FileFinder it = searchString.c_str(); !it.done(); ++it)
	{
		if (!(*it).isFolder)
		{
			auto sourceFilename = combine_path_and_file(srcFolder, (*it).filename);

			auto presetName = (*it).filename;
			{
				// chop off extension
				auto p = presetName.find_last_of(L'.');
				if (p != std::string::npos)
					presetName = presetName.substr(0, p);
			}

			auto chunk = loadNativePreset( ToWstring(sourceFilename) );

			{
				tinyxml2::XMLDocument presetDoc;

				presetDoc.Parse(chunk.c_str());

				if (!presetDoc.Error())
				{
					auto parameters = presetDoc.FirstChildElement("Preset");
					auto copyOfParameters = parameters->DeepClone(&xml)->ToElement();
					presets_xml->LinkEndChild(copyOfParameters);
					copyOfParameters->SetAttribute("name", ToUtf8String(presetName).c_str());
				}
			}
		}
	}

	// Save output XML document.
	xml.SaveFile(filename);
}

void MpController::ImportBankXml(const char* xmlfilename)
{
	auto presetFolder = BundleInfo::instance()->getPresetFolder();

	CreateFolderRecursive(presetFolder);

	TiXmlDocument doc; // Don't use tinyXML2. XML must match *exactly* the current format, including indent, declaration, everything. Else Preset Browser won't correctly match presets.
	doc.LoadFile(xmlfilename);

	if (doc.Error())
	{
		assert(false);
		return;
	}

	auto presetsE = doc.FirstChildElement("Presets");

	for (auto PresetE = presetsE->FirstChildElement("Preset"); PresetE; PresetE = PresetE->NextSiblingElement())
	{
		// Query plugin's 4-char code. Presence Indicates also that preset format supports MIDI learn.
		int32_t fourCC = -1; // -1 = not specified.
		int formatVersion = 0;
		{
			std::string hexcode;
			if (TIXML_SUCCESS == PresetE->QueryStringAttribute("pluginId", &hexcode))
			{
				formatVersion = 1;
				try
				{
					fourCC = std::stoul(hexcode, nullptr, 16);
				}
				catch (...)
				{
					// who gives a f*ck
				}
			}
		}

		// TODO !!! Check fourCC.

		std::string name;
		if (tinyxml2::XML_SUCCESS != PresetE->QueryStringAttribute("name", &name))
		{
			PresetE->QueryStringAttribute("Name", &name); // old format used to be capitalized.
		}
		auto filename = presetFolder + Utf8ToWstring(name) + L".";
		filename += getNativePresetExtension();

		// Create a new XML document, containing only one preset.
		TiXmlDocument doc2;
		doc2.LinkEndChild( new TiXmlDeclaration( "1.0", "", "" ) );
		doc2.LinkEndChild(PresetE->Clone());

		TiXmlPrinter printer;
		printer.SetIndent(" ");
		doc2.Accept(&printer);
		const std::string& presetXml = printer.Str();

		// dialog if file exists.
//		auto result = gmpi::MP_OK;

/* no mac support
		fs::path fn(filename);
		if (fs::exists(fn))
*/
#ifdef _WIN32
        auto file = _wfopen(filename.c_str(), L"r"); // fs::exists(filename)
#else
        auto file = fopen(WStringToUtf8(filename).c_str(), "r");
#endif
        if(file)
		{
			fclose(file);

			auto gh = getGraphicsHost();

			if (gh)
			{
				okCancelDialog.setNull(); // free previous.
				gh->createOkCancelDialog(0, okCancelDialog.GetAddressOf());

				if (okCancelDialog.isNull())
					return;

				std::ostringstream oss;
				oss << "Overwrite preset '" << name << "'?";
				okCancelDialog.SetText(oss.str().c_str());

				okCancelDialog.ShowAsync([this, name, presetXml, filename] (int32_t result) -> void
					{
						if( result == gmpi::MP_OK )
                            saveNativePreset(
                                 WStringToUtf8(filename).c_str(),
                                 name,
                                 presetXml
                            );
					}
				);
			}
		}
		else
		{
			saveNativePreset(WStringToUtf8(filename).c_str(), name, presetXml);
		}
	}

	ScanPresets();
	UpdatePresetBrowser();
}

void MpController::setModified(bool presetIsModified)
{
	(*getHostParameter(HC_PROGRAM_MODIFIED)) = presetIsModified;
}
