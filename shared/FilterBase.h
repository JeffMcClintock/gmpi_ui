#pragma once

#include <algorithm>
#include <math.h>
#include "../se_sdk3/mp_sdk_audio.h"

/*
#include "../shared/FilterBase.h"
*/

/*
This class implements advanced 'sleep' behaviour for filters or any module that takes some time to settle after the input signal goes quiet.

The basic theory of 'power saving' in SynthEdit is:

1.	Wait until the input is silent (not streaming).
2.	Watch until the output also is silent (or at least very quiet). Now you are ready to sleep.
3.	Output some zeros until the output buffer is all zeros (sends perfect silence to the next module).
4.	Set the output pin status to ‘not-streaming’.

Once all input and output pins are silent, the SDK will automatically sleep your module.
 
FilterBase just helps with that stuff. It's pretty simple to use.

* For step 1, implement isFilterSettling() and just return ‘true’ if the input is silent.
* For step 2, implement getOutputPin() which tells the base class what output to watch for silence.
* Step 3 and 4 are automatic with this method.

This 'FilterBase' class monitors the output signal, only when the output has settled into a steady-state does the module go to sleep.

USEAGE:

Derive your module from class FilterBase

	class MyFilter : public FilterBase
	{
	// etc
	}

Implement these member functions

	// Support for FilterBase

	// This is called to determin when your filter is settling. Typically you need to check if all your input pins are quiet (not streaming).
	bool isFilterSettling() override
	{
		return !pinSignal.isStreaming(); // <-example code. Place your code here.
	}

	// This allows the base class to monitor the filter's output signal, provide an audio output pin.
	AudioOutPin& getOutputPin() override
	{
		return pinOutput; // <-example code. Place your code here.
	}

	Call 'initSettling()' as the last thing in your onSetPins() method. It will check if filter is settling and if so commence monitoring the output signal.

	void MyFilter::onSetPins() override
	{

		// ... usual stuff first.

		initSettling(); // must be last.
	}

Optional: Under heavy modulation, recursive Filters can sometimes become unstable and emit noise or other unpleasant sounds.
    If this is the case, the base class provides some help for mitigating this situation by checking periodically if
	the the filter is stable or not. To implement this:

In you processing member function, call doStabilityCheck() to implement the periodic StabilityCheck() function.
To avoid wasting CPU, it calls your StabilityCheck() member only one time in 50. This must be the first thing in the function.

	void subProcess(int sampleFrames)
	{
		doStabilityCheck(); // must be first

		// ...etc

Override member StabilityCheck(), it should check if the filter is stable, and if not reset it. e.g.
	
	void StabilityCheck() override
	{
		if (!isfinite(my_state_variable)) // check for numeric overflow.
		{
			my_state_variable = 0.0f; // reset filter be zeroing it's state.
		}
	}
*/

class FilterBase : public MpBase2
{
protected:
	SubProcess_ptr2 actualSubProcess;

	int32_t stabilityCheckCounter = 0;
	float static_output = {};

	static const int historyCount = 32;
	int historyIdx = 0;

public:
	FilterBase()
	{}

	int32_t MP_STDCALL open() override
	{
		// randomise stabilityCheckCounter to avoid CPU spikes.
		getHost()->getHandle(stabilityCheckCounter);
		stabilityCheckCounter &= 63;

		return MpBase2::open();
	}

	virtual bool isFilterSettling() = 0;
	virtual void OnFilterSettled()
	{
		setSubProcess(&FilterBase::subProcessStatic);

		const int blockPosition = getBlockPosition(); // assuming StabilityCheck() always called at start of block.
		getOutputPin().setStreaming(false, blockPosition);
	}
	virtual AudioOutPin& getOutputPin() = 0;

	// This provides a counter to check the filter periodically to see it it has 'crashed'.
	inline void doStabilityCheck()
	{
		if (--stabilityCheckCounter < 0)
		{
			StabilityCheck();
			stabilityCheckCounter = 50;
		}
	}

	// override this to check if your filter memory contains invalid data.
	virtual void StabilityCheck() {}

	void subProcessSettling(int sampleFrames)
	{
		if (historyIdx <= 0)
		{
			setSubProcess(actualSubProcess); // safely measure to prevent accidental recursion.
			OnFilterSettled();
			return (this->*(getSubProcess()))(sampleFrames); // call subProcessStatic().
		}

		(this->*(actualSubProcess))(sampleFrames);

		// retain the last output value
		static_output = *(getBuffer(getOutputPin()) + sampleFrames - 1);

		int todo = (std::min) (historyIdx, sampleFrames);
		auto o = getBuffer(getOutputPin()) + sampleFrames - todo;

		for (int i = 0; i < todo; ++i)
		{
			const float INSIGNIFICANT_SAMPLE = 0.000001f;
			float energy = fabs(static_output - *o);
			if (energy > INSIGNIFICANT_SAMPLE)
			{
				// filter still not settled.
				historyIdx = historyCount;
			}
			--historyIdx;
			++o;
		}
	}

	// This assumes your filter has only one output, override OnFilterSettled() to use your own member function if needed.
	void subProcessStatic(int sampleFrames)
	{
		auto output = getBuffer(getOutputPin());

		assert(fabs(static_output) < 2000.0f); // sanity check

		for (int s = sampleFrames; s > 0; s--)
		{
			*output++ = static_output;
		}
	}

	void initSettling()
	{
		if (isFilterSettling())
		{
			if (getSubProcess() != &FilterBase::subProcessSettling)
			{
				actualSubProcess = getSubProcess();
				setSubProcess(&FilterBase::subProcessSettling);
			}

			historyIdx = historyCount;
		}
	}
};


