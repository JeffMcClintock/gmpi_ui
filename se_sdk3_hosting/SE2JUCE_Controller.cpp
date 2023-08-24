#include "BinaryData.h"
#include "SE2JUCE_Controller.h"
#include "SE2JUCE_Processor.h"
#include "tinyxml/tinyxml.h"

MpParameterJuce::MpParameterJuce(SeJuceController* controller, int ParameterIndex, bool isInverted) :
	MpParameter_native(controller)
	,juceController(controller)
	, isInverted_(isInverted)
{
	hostIndex = ParameterIndex;
//	isInverted_ = normalisedToReal(0.0) > normalisedToReal(1.0);
}

SeJuceController::SeJuceController() :
	queueToDsp_(SeAudioMaster::AUDIO_MESSAGE_QUE_SIZE)
{
}

std::vector< MpController::presetInfo > SeJuceController::scanFactoryPresets()
{
	const char* xmlPresetExt = ".xmlpreset";

	std::vector< MpController::presetInfo > returnValues;

	for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
	{
		const std::string filename{ BinaryData::originalFilenames[i] };
		if (filename.find(xmlPresetExt) != std::string::npos)
		{
			const std::wstring fullpath = L"BinaryData/" + ToWstring(filename);

			int dataSizeInBytes = {};
			const auto data = BinaryData::getNamedResource(BinaryData::namedResourceList[i], dataSizeInBytes);
			const std::string xml(data, dataSizeInBytes);

			returnValues.push_back(parsePreset(fullpath, xml));
		}
	}

	return returnValues;
}

void SeJuceController::loadFactoryPreset(int index, bool fromDaw)
{
	const char* xmlPresetExt = ".xmlpreset";
	int presetIndex = 0;
	for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
	{
		const std::string filename{ BinaryData::originalFilenames[i] };
		if (filename.find(xmlPresetExt) != std::string::npos && presetIndex++ == index)
		{
			int dataSizeInBytes = {};
			const auto data = BinaryData::getNamedResource(BinaryData::namedResourceList[i], dataSizeInBytes);
			const std::string xml(data, dataSizeInBytes);

			if (fromDaw)
			{
				setPresetFromDaw(xml, true);
			}
			else // from internal preset browser
			{
				TiXmlDocument doc;
				doc.Parse(xml.c_str());

				if (doc.Error())
				{
					assert(false);
					return;
				}

				TiXmlHandle hDoc(&doc);

				setPreset(hDoc.ToNode(), true, presetIndex);
			}
			return;
		}
	}
}

// Mode should remain on 'Master' unless explicity set by user.
void SeJuceController::OnStartupTimerExpired()
{
	// disable updates to 'Master/Analyse' via loading presets.
	MpController::OnStartupTimerExpired();

	undoManager.initial(this);
}

void MpParameterJuce::setNormalizedUnsafe(float daw_normalized)
{
	dawNormalizedUnsafe = daw_normalized;

	dirty.store(true, std::memory_order_release);
	juceController->setDirty();
}

// this is the DAW setting a parameter.
// it needs to be relayed to the Controller (and GUI) and to the Processor
void MpParameterJuce::updateFromImmediate()
{
	const auto pdirty = dirty.load(std::memory_order_relaxed);
	if (!pdirty)
	{
		return;
	}

	dirty.store(false, std::memory_order_release);

	const float se_normalized = adjust(dawNormalizedUnsafe.load(std::memory_order_relaxed));

	// this will update all GUIs
	if (setParameterRaw(gmpi::MP_FT_NORMALIZED, sizeof(se_normalized), &se_normalized))
	{
		//updateProcessor(gmpi::MP_FT_VALUE, 0);

		// this will update the processor only, not the DAW.
		juceController->ParamToDsp(this);
	}
}

void MpParameterJuce::updateProcessor(gmpi::FieldType fieldId, int32_t voice)
{
	juceController->ParamToProcessorAndHost(this, fieldId, voice);
}

void SeJuceController::ParamToProcessorAndHost(MpParameterJuce* param, gmpi::FieldType fieldId, int32_t voice)
{
	// JUCE does not provide for thread-safe notification to the processor, so handle this via the message queue.
	ParamToDsp(param, voice);

	// also need to notify JUCE and DAW.
	switch (fieldId)
	{
	case gmpi::MP_FT_GRAB:
		{
			auto juceParameter = processor->getParameters()[param->getNativeIndex()];
			if (juceParameter)
			{
				if (param->isGrabbed())
					juceParameter->beginChangeGesture();
				else
					juceParameter->endChangeGesture();
			}
		}
		break;

	case gmpi::MP_FT_VALUE:
//	case gmpi::MP_FT_NORMALIZED:
		{
			// update JUCE parameter
			auto juceParameter = processor->getParameters()[param->getNativeIndex()];
			if (juceParameter)
			{
				const bool handleGrabMyself = !param->isGrabbed();
				if (handleGrabMyself)
				{
					juceParameter->beginChangeGesture();
				}

				juceParameter->setValueNotifyingHost(param->getDawNormalized());

				if (handleGrabMyself)
				{
					juceParameter->endChangeGesture();
				}
			}
		}
		break;
	}
}

//void SeJuceController::legacySendProgramChangeToHost(float)
//{
//	processor->updateHostDisplay(juce::AudioProcessorListener::ChangeDetails().withProgramChanged(true));
//}

// ensure GUI reflects the value of all parameters
void SeJuceController::initGuiParameters()
{
	const int voice = 0;
	for (auto& p : parameters_)
	{
		updateGuis(p.get(), voice);
	}
}
