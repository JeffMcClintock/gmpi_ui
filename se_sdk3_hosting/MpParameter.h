#pragma once
#include <vector>
#include <string>
#include <iostream>
#include "../se_sdk3/TimerManager.h"
#include "../shared/RawView.h"
#include "../shared/xplatform.h"
#include "../se_sdk3/mp_sdk_common.h"

class MpParameter : public TimerClient
{
	bool m_grabbed = false;
	int m_grabbed_by_MIDI_timer = 0;

public:
	class MpController* controller_ = nullptr;

	int parameterHandle_ = -1;
	int datatype_ = -1;
	int moduleHandle_ = -1;
	int moduleParamId_ = -1;
	int stateful_ = false;
	bool ignorePc_ = {};
	std::wstring name_;
	std::wstring enumList_;
	int32_t MidiAutomation = -1;
	std::wstring MidiAutomationSysex;
	std::string tempReturnValue; // getValueRaw() needs temp data to stick arround after method exists.

	MpParameter(MpController* controller) : controller_(controller)
	{
	}

	virtual ~MpParameter() {}

	inline int ModuleHandle() const
	{
		return moduleHandle_;
	}
	inline int ModuleParameterId() const
	{
		return moduleParamId_;
	}

	bool isGrabbed()
	{
		return m_grabbed;
	}

	virtual RawView getValueRaw(gmpi::FieldType paramField, int32_t voice);
	virtual bool setParameterRaw(gmpi::FieldType paramField, int32_t size, const void * data, int32_t voice = 0);
	template<typename T>
	const T& operator=(const T& value)
	{
		RawView temp{ value };
		setParameterRaw(gmpi::MP_FT_VALUE, temp.size(), temp.data());

		return value;
	}
	bool setParameterRaw(gmpi::FieldType paramField, size_t size, const void * data, int32_t voice = 0) // convinience overload for size_t
	{
		return setParameterRaw(paramField, static_cast<int32_t>(size), data, voice);
	}
	bool setParameterRaw(gmpi::FieldType paramField, RawView raw, int32_t voice = 0) // convenience overload for size_t
	{
		return setParameterRaw(paramField, static_cast<int32_t>(raw.size()), raw.data(), voice);
	}
	virtual void updateFromDsp(int recievingMessageId, class my_input_stream& strm) = 0;
	virtual int getNativeTag() = 0; // -1 = not exported to DAW.
	virtual bool isPolyPhonic() = 0;
	virtual int getHostControl() = 0;

	virtual float getNormalized() const = 0;
	virtual double getValueReal() const = 0;
	virtual std::wstring normalisedToString(double normalized) const = 0;
	virtual double stringToNormalised(const std::wstring& string) const = 0;
	virtual double normalisedToReal(double normalized) const = 0;
    virtual double RealToNormalized(double normalized) const = 0;

	virtual void updateProcessor(gmpi::FieldType fieldId, int32_t voice) = 0;
    
	// VST2 requires an unsafe cache of most recent parameter value from DAW.
	// Override this method to allow the framework to update the cached value from the plugin side.
    virtual void upDateImmediateValue()= 0;

	void emulateMouseDown();
	
	// TimerClient
	virtual bool OnTimer() override;
    
    const bool isEnum() const
    {
        return (datatype_ == gmpi::MP_INT32 || datatype_ == gmpi::MP_INT64) && !enumList_.empty();
    }

	virtual int getVoiceCount() = 0;
};

class MpParameter_base : public MpParameter
{
public:
	std::vector< std::string > rawValues_; // rawValues_[voice] where voice is 0 - 127
	double minimum = 0.0;
	double maximum = 1.0;
	int hostControl_ = -1;
	bool isPolyphonic_ = false;

	MpParameter_base(MpController* controller) : MpParameter(controller)
	{
	}

	virtual RawView getValueRaw(gmpi::FieldType paramField, int32_t voice) override;
	bool setParameterRaw(gmpi::FieldType paramField, int32_t size, const void * data, int32_t voice = 0) override;
	void updateFromDsp(int recievingMessageId, class my_input_stream& strm) override;
	int getNativeTag() override;
	virtual bool isPolyPhonic() override {
		return isPolyphonic_;
	}
	virtual int getHostControl() override {
		return hostControl_;
	}

	float getNormalized() const override
	{
		return static_cast<float>(RealToNormalized(getValueReal()));
	}

	virtual double getValueReal() const override;
	virtual std::wstring normalisedToString(double normalized) const override;
	virtual double stringToNormalised(const std::wstring& string) const override;
	virtual double normalisedToReal(double normalized) const override;
    virtual double RealToNormalized(double normalized) const override;

	int getVoiceCount() override
	{
		return static_cast<int>(rawValues_.size());
	}
};

class MpParameter_private : public MpParameter_base
{
public:
	MpParameter_private(MpController* controller) : MpParameter_base(controller)
	{
	}

	void updateProcessor(gmpi::FieldType fieldId, int32_t voice) override;
	void upDateImmediateValue() override {}
};
    
class MpParameter_native : public MpParameter_base
{
public:
	int hostTag = -1;	// index as set in SE, not nesc sequential.
	int hostIndex = -1;	// strict sequential index, no gaps.

	MpParameter_native(MpController* controller) : MpParameter_base(controller)
	{}

	int getNativeTag() override { return hostTag; } // -1 = not exported to DAW.
	int getNativeIndex() { return hostIndex; }

	bool isPolyPhonic() override {
		return false;
	}
};

// Host controls which are not stateful and are not available on the Processor
// Exception is Program-Name
class SeParameter_vst3_hostControl : public MpParameter_private
{
public:
	SeParameter_vst3_hostControl(MpController* controller, int hostControl);

	bool setParameterRaw(gmpi::FieldType paramField, int32_t size, const void * data, int32_t voice = 0) override;
 
	void updateProcessor(gmpi::FieldType fieldId, int32_t voice) override;
};
