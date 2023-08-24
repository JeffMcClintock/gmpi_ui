#include "MpParameter.h"
#include <sstream>
#include <iomanip>
#include "../../conversion.h"
#include "../../RawConversions.h"
#include "../shared/xp_simd.h"
#include "Controller.h"

using namespace std;

#define MIDI_LEARN_MENU_ITEMS L"Learn=1, UnLearn"


bool MpParameter_base::setParameterRaw(gmpi::FieldType paramField, int32_t size, const void* data, int32_t voice)
{
	// Handles real value and normalised only.
	assert(paramField == gmpi::MP_FT_VALUE || paramField == gmpi::MP_FT_NORMALIZED || paramField == gmpi::MP_FT_GRAB);

	bool changed = false;

	switch (paramField)
	{
	case gmpi::MP_FT_VALUE:
		{
			while (rawValues_.size() <= static_cast<size_t>(voice))
			{
				rawValues_.push_back(std::string());			// preset 0
			}
			changed = rawValues_[voice].size() != static_cast<size_t>(size) || memcmp(data, rawValues_[voice].data(), size) != 0;

			// Store normalized locally.
			rawValues_[voice].resize(size);
			rawValues_[voice].assign((char*)data, size);
		}
		break;
/* safer to be read-only.
	case FT_ENUM_LIST:
		{
			auto newEnumList = RawToValue<std::wstring>(data, size);
			changed = newEnumList != enumList_;
			enumList_ = newEnumList;
		}
		break;
*/

	case gmpi::MP_FT_NORMALIZED:
	{
		double normalized = (double)*(float*)data;

		// Update real world normalized in 'rawValue_'.
		double realWorld = 0.0;

		// Private parameter.
		if (enumList_.empty()) // float, double, int.
		{
			realWorld = minimum + normalized * (maximum - minimum);
		}
		else // enum.
		{
			realWorld = (double)normalised_to_enum(enumList_, static_cast<float>(normalized));
		}

		string newRawValue;

		switch (datatype_)
		{
		case DT_FLOAT:
		{
			newRawValue = ToRaw4((float)realWorld);
			break;
		}
		case DT_DOUBLE:
		{
			newRawValue = ToRaw4(realWorld);
			break;
		}
		case DT_INT:
		{
// -ves fail			newRawValue = ToRaw4((int32_t)(0.5 + realWorld));
			newRawValue = ToRaw4((int32_t) FastRealToIntFloor(0.5 + realWorld));
			break;
		}
		case DT_INT64:
		{
			// -ves fail			newRawValue = ToRaw4((int64_t)(0.5 + realWorld));
			newRawValue = ToRaw4((int64_t)FastRealToIntFloor(0.5 + realWorld));
			break;
		}
		case DT_BOOL:
		{
			newRawValue = ToRaw4((bool)(normalized > 0.5));
			break;
		}

		default:
			assert(false);
			break;
		}

		return setParameterRaw(gmpi::MP_FT_VALUE, static_cast<int32_t>(newRawValue.size()), newRawValue.data(), voice);
	}
	break;

	case gmpi::MP_FT_GRAB:
		return MpParameter::setParameterRaw(paramField, size, data, voice);
		break;

	default:
	{
		assert(false); // to be handled by caller.
	}
	break;
	}

	if (changed)
	{
//		emulateMouseDown();
		controller_->updateGuis(this, voice);
	}

	return changed;
}

int MpParameter_base::getNativeTag()
{
	return -1; // -1 = not exported to DAW.
}

void MpParameter_base::updateFromDsp(int recievingMessageId, my_input_stream & strm)
{
	MpParameter::updateFromDsp(recievingMessageId, strm);

	switch (recievingMessageId)
	{

	case code_to_long('p', 'p', 'c', 0): // "ppc" Patch parameter change. Either Output parameter or MIDI automation
	{
		bool vst_inhibit_send_automation;
		strm >> vst_inhibit_send_automation;

		int voice;
		strm >> voice;

		int32_t size = getDataTypeSize(datatype_);
		bool resizeable = size == 0;
		while (voice != -1)
		{
			if (resizeable) // variable size.
			{
				strm >> size;
			}

			while (rawValues_.size() <= static_cast<size_t>(voice))
			{
				rawValues_.push_back(std::string());
			}

			assert(rawValues_.size() > voice);
			rawValues_[voice].resize(size);
			strm.Read(rawValues_[voice].data(), size);

			controller_->updateGuis(this, voice);
			
			emulateMouseDown();

			// next? (-1 signifies end)
			strm >> voice;
		}
	}
	break;
	}
}

void MpParameter::updateFromDsp(int recievingMessageId, my_input_stream & strm)
{
	switch (recievingMessageId)
	{
	case code_to_long('l', 'e', 'r', 'n'): // "lern" Controller ID change
	{
		strm >> MidiAutomation;
		controller_->updateGuis(this, gmpi::MP_FT_AUTOMATION);
	}
	break;
	}
}

RawView MpParameter::getValueRaw(gmpi::FieldType paramField, int32_t voice)
{
	static const int32_t zero = 0;

	switch (paramField)
	{
	case gmpi::MP_FT_ENUM_LIST:
	case gmpi::MP_FT_FILE_EXTENSION:
	{
		return RawView(enumList_);
	}
	break;

	case gmpi::MP_FT_SHORT_NAME:
	case gmpi::MP_FT_LONG_NAME:
	{
		return RawView(name_);
	}

	case gmpi::MP_FT_MENU_SELECTION:
		return RawView(zero);
		break;

	case gmpi::MP_FT_GRAB:
		return RawView(m_grabbed);
		break;

	case gmpi::MP_FT_MENU_ITEMS:
	{
		if (datatype_ == DT_BLOB || datatype_ == DT_TEXT)
		{
			return RawView();
		}
		else
		{
			tempReturnValue = literalToRaw(MIDI_LEARN_MENU_ITEMS);
			return RawView(tempReturnValue);
		}
	}
	break;

	case gmpi::MP_FT_NORMALIZED:
	{
		float normalized = getNormalized();
		tempReturnValue = ToRaw4(normalized);
		return RawView(tempReturnValue);
	}
	break;

	case gmpi::MP_FT_AUTOMATION:
	{
		return RawView(MidiAutomation);
	}
	break;

	case gmpi::MP_FT_AUTOMATION_SYSEX:
	{
		return RawView(MidiAutomationSysex);
	}
	break;

	default:
		break;
	}

	assert(false); // super class to handle.
	return RawView();
}

RawView MpParameter_base::getValueRaw(gmpi::FieldType paramField, int32_t voice)
{
	int expected_size = -1;

	switch (paramField)
	{
		case gmpi::MP_FT_VALUE:
		{
			expected_size = getDataTypeSize(datatype_);

			if (rawValues_.size() > voice && (expected_size == 0 || expected_size == (int)rawValues_[voice].size()))
			{
				return rawValues_[voice];
			}
			else
			{
				// return zero.
				tempReturnValue.assign(expected_size, '\0');
				return RawView(tempReturnValue);
			}
			break;

		case gmpi::MP_FT_RANGE_LO:
		{
			// compute normalised normalized.
			switch (datatype_)
			{
			case DT_FLOAT:
			{
				float v = (float)minimum;
				tempReturnValue = ToRaw4(v);
				return RawView(tempReturnValue);
			}
			break;

			case DT_DOUBLE:
			{
				return ToRaw4(minimum);
			}
			break;

			case DT_BOOL:
			{
				tempReturnValue = ToRaw4(false);
				return RawView(tempReturnValue);
			}
			break;

			default:
			{
//				assert(false); // TODO.
				float normalised = 0;
				tempReturnValue = ToRaw4(normalised);
				return RawView(tempReturnValue);
			}
			break;
			}
		}
		break;

		case gmpi::MP_FT_RANGE_HI:
		{
			// compute normalised normalized.
			switch (datatype_)
			{
			case DT_FLOAT:
			{
				float v = (float)maximum;
				tempReturnValue = ToRaw4(v);
				return RawView(tempReturnValue);
			}
			break;

			case DT_DOUBLE:
			{
				return ToRaw4(maximum);
			}
			break;

			case DT_BOOL:
			{
				tempReturnValue = ToRaw4(true);
				return RawView(tempReturnValue);
			}
			break;

			default:
			{
//				assert(false); // TODO.
				float normalised = 1;
				//			return ToRaw4(normalised);
				tempReturnValue = ToRaw4(normalised);
				return RawView(tempReturnValue);
			}
			break;
			}
		}
		break;

		default:
			return MpParameter::getValueRaw(paramField, voice);
			break;
		}
	}

	return RawView();
}

double MpParameter_base::getValueReal() const
{
	const int voiceId = 0;
	switch (datatype_)
	{
	case DT_FLOAT:
	{
		if (rawValues_[voiceId].size() == sizeof(float))
		{
			return static_cast<double>( RawToValue<float>(rawValues_[voiceId].data()) );
		}
	}
	break;

	case DT_DOUBLE:
	{
		if (rawValues_[voiceId].size() == sizeof(double))
		{
			return static_cast<double>(RawToValue<double>(rawValues_[voiceId].data()));
		}
	}
	break;

	case DT_BOOL:
	{
		if (rawValues_[voiceId].size() == sizeof(bool))
		{
			return static_cast<double>(RawToValue<bool>(rawValues_[voiceId].data()));
		}
	}
	break;

	case DT_INT:
	{
		if (rawValues_[voiceId].size() == sizeof(int32_t))
		{
            const auto v = RawToValue<int32_t>(rawValues_[voiceId].data());
            if (enumList_.empty())
            {
                return static_cast<double>(v);
            }
            else
            {
                it_enum_list it(enumList_);
                if (it.FindValue(v))
                {
                    return static_cast<double>(it.CurrentItem()->index);
                }
                
                return 0.0;
            }
		}
	}
	break;

	case DT_INT64:
	{
		if (rawValues_[voiceId].size() == sizeof(int64_t))
		{
			return static_cast<double>(RawToValue<int64_t>(rawValues_[voiceId].data()));
		}
	}
	break;

	case DT_ENUM:
	{
		assert(false); // TODO?
	}
	break;

	}

	return 0.0f;
}

std::wstring SliderFloatToString(float val, int p_decimal_places = -1) // better because it removes trailing zeros. -1 for auto decimal places
{
	bool auto_decimal = p_decimal_places < 0; // remember auto (as decimal places is modified)
	std::wostringstream oss;
	oss << setiosflags(ios_base::fixed);

	if (auto_decimal)
	{
		// attempt to see enough of number when smallish.
		int precision = 3;
		float absValue = fabsf(val);
		if (absValue > 0.00000001f)
		{
			absValue *= 100.0f;

			while (precision < 8 && absValue < 1.0f)
			{
				++precision;
				absValue *= 10.0f;
			}
		}

		oss << setprecision(precision);
	}
	else
	{
		oss << setprecision(p_decimal_places); // x significant digits AFTER point.
	}

	oss << val;

	std::wstring s = oss.str();

	// Replace -0.0 with 0.0 ( same for -0.00 and -0.000 etc).
	// deliberate 'feature' of printf is to round small negative numbers to -0.0
	if (s[0] == L'-' && val > -1.0f)
	{
		int i = (int)s.size() - 1; // Not using unsigned size_t else fails <0 test below.

		while (i > 0)
		{
			if (s[i] != L'0' && s[i] != L'.')
			{
				break;
			}

			--i;
		}

		if (i == 0) // nothing but zeros (or dot).
		{
			//			s = Right(s, s.size() - 1);
			s = s.substr(1);
		}
	}

	return s;
}

std::wstring MpParameter_base::normalisedToString(double normalized) const
{
	if( (datatype_ == DT_INT || datatype_ == DT_INT64) && !enumList_.empty())
	{
		it_enum_list it(enumList_);

		const int maxindex = it.size() - 1;
		int enumIndex = FastRealToIntFloor(0.5 + normalized * maxindex);
		enumIndex = (std::min)(enumIndex, maxindex);

		if (it.FindIndex(enumIndex))
		{
			return it.CurrentItem()->text;
		}
	}

	auto value = normalisedToReal(normalized);
	//const int maxSize = 128;
	//wchar_t temp[maxSize];
	//swprintf(temp, maxSize, L"%f", value);

	//wstring r(temp);

	return SliderFloatToString(value);
}

double MpParameter_base::stringToNormalised(const std::wstring& string) const
{
	wchar_t* endPtr;
	return RealToNormalized( wcstof(string.c_str(), &endPtr) );
}

void MpParameter_private::updateProcessor(gmpi::FieldType fieldId, int32_t voice)
{
	controller_->ParamToDsp(this, voice);
}

double MpParameter_base::normalisedToReal(double normalized) const
{
	switch (datatype_)
	{
	case DT_FLOAT:
	case DT_DOUBLE:
	{
		return minimum + normalized * (maximum - minimum);
	}
	break;

	case DT_BOOL:
	{
		return floor(0.5 + normalized);
	}
	break;

	case DT_INT:
	case DT_INT64:
	{
		if (enumList_.empty())
		{
            return floor(0.5 + minimum + normalized * (maximum - minimum));
		}
		else
		{
			it_enum_list it(enumList_);

			const int maxindex = it.size() - 1;
			int enumIndex = (int) (0.5 + normalized * maxindex);
			enumIndex = (std::min)(enumIndex, maxindex);

            return enumIndex;
/* au don't like
			if (it.FindIndex(enumIndex))
			{
				return it.CurrentItem()->value;
			}
 */
		}
	}
	break;

	case DT_ENUM:
	{
		assert(false); // should never happen (enums use INT)
	}
	break;

	}

	return 0.0f;
}

double MpParameter_base::RealToNormalized(double real) const
{
    switch (datatype_)
    {
		case DT_INT:
		case DT_INT64:
		{
			if (!enumList_.empty())
			{
				it_enum_list it(enumList_);
				const int i = FastRealToIntFloor(0.5 + real);
                const int maxindex = it.size() - 1;
                if(maxindex < 1)
                {
                    return 0.0;
                }
                return i / (double)maxindex;
/*
//				if (it.FindValue(enumValue))
                if (it.FindIndex(enumValue))
				{
					const int maxindex = it.size() - 1;
					return it.CurrentItem()->index / (double)maxindex;
				}
				else
				{
					return 0.0;
				}
 */
			}
		}
		// deliberate fall-thru.
	
		case DT_FLOAT:
        case DT_DOUBLE:
        {
			double normalised = (real - minimum) / (maximum - minimum);

			if (normalised > 1.f)
			{
				normalised = 1.f;
			}
			else
			{
				if (!(normalised >= 0.f)) // reverse logic catches NANs and overflows
					normalised = 0.f;
			}

			return normalised;
        }
        break;
            
        case DT_BOOL:
        {
            return real < 0.5 ? 0.0 : 1.0;
        }
        break;
            
        case DT_ENUM:
        {
            assert(false); // should never happen (enums use INT)
        }
        break;
    }
    
    return 0.0f;
}

SeParameter_vst3_hostControl::SeParameter_vst3_hostControl(MpController* controller, int hostControl) : MpParameter_private(controller)
{
	hostControl_ = hostControl;
	name_ = GetHostControlName((HostControls) hostControl_);
	datatype_ = GetHostControlDatatype((HostControls)hostControl_);
	moduleHandle_ = -1;
	moduleParamId_ = -1;
	stateful_ = false;
	minimum = 0;
	maximum = 1;
	isPolyphonic_ = false;

	// Initialize to zero or whatever
	const char* nothing = "\0\0\0\0\0\0\0\0";
	rawValues_.push_back({ nothing, static_cast<size_t>(getDataTypeSize(datatype_)) });
}

bool SeParameter_vst3_hostControl::setParameterRaw(gmpi::FieldType paramField, int32_t size, const void * data, int32_t voice)
{
	const auto r = MpParameter_private::setParameterRaw(paramField, size, data, voice);

	if(r)
	{
		controller_->OnSetHostControl(hostControl_, paramField, size, data, voice);
	}

	return r;
}

void SeParameter_vst3_hostControl::updateProcessor(gmpi::FieldType fieldId, int32_t voice)
{
	// These are never sent to DSP, and wouldn't work as thier handles are generated at runtime by the controller.
	// Exception is Program Name, which is built as a regular param (not SeParameter_vst3_hostControl)
//	controller_->HostControlToDsp(this, voice);
}

bool MpParameter::setParameterRaw(gmpi::FieldType paramField, int32_t size, const void * data, int32_t voice)
{
	bool changed = false;

	if (gmpi::MP_FT_GRAB == paramField)
	{
		const bool oldVal = m_grabbed;
		m_grabbed = RawToValue<bool>(data, size);
//			_RPT1(_CRT_WARN, "m_grabbed %d\n", (int)m_grabbed);

		changed = oldVal != m_grabbed;
		// May already be grabbed by MIDI automation. In which case no need to update GUI.
		if (changed)
			controller_->updateGuis(this, paramField, voice);

		// Mouse overrides MIDI "grab".
		m_grabbed_by_MIDI_timer = 0;
	};

	return changed;
}

void MpParameter::emulateMouseDown()
{
	// Emulate Mouse down when automated from MIDI/Processor
	const int timerResetValue = 4;
	if (!m_grabbed)
	{
		m_grabbed = true;
		m_grabbed_by_MIDI_timer = timerResetValue;
		StartTimer(50);
//		_RPT1(_CRT_WARN, "m_grabbed (MIDI) %d\n", (int)m_grabbed);
		controller_->updateGuis(this, gmpi::MP_FT_GRAB);
	}
	else
	{
		// Already grabbed and more MIDI arrives, refresh timer.
		if (m_grabbed_by_MIDI_timer > 0)
		{
			m_grabbed_by_MIDI_timer = timerResetValue;
		}
	}
}

bool MpParameter::OnTimer()
{
//	_RPT0(_CRT_WARN, "*\n");
	bool done = m_grabbed_by_MIDI_timer-- <= 0;
	if (done)
	{
		if (m_grabbed)
		{
			m_grabbed = false;
//			_RPT1(_CRT_WARN, "m_grabbed (MIDI) %d\n", (int)m_grabbed);
			controller_->updateGuis(this, gmpi::MP_FT_GRAB);
		}
	}

	return !done;
}
