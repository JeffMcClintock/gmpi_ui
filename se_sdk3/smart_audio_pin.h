// Copyright 2008 Jeff McClintock

#ifndef SMART_AUDIO_PIN_H_INCLUDED
#define SMART_AUDIO_PIN_H_INCLUDED

#include "mp_sdk_audio.h" 
#include <limits>

// This class supports audio-rate control signal outputs with smoothing.
/*
#include "..\se_sdk3\smart_audio_pin.h"
*/

class SmartAudioPin : public AudioOutPin
{
public:
	SmartAudioPin();

	// Pointer to sound processing member function.
	typedef void (SmartAudioPin::* SapSubProcessMethodPointer)( int bufferOffset, int sampleFrames, bool& canSleep );

	float operator=(const float value)
	{
		setValue(value);
		return value;
	}
	// glide to new value.
	void setValue( float targetValue, int blockPosition = -1 );
	// jump to new value.
	void setValueInstant( float targetValue, int blockPosition = -1 );
	void pulse( float pulseHeight, int blockPosition = -1 );
	float getInstantValue();
	void setTransitionTime( float transitionTime ); // in samples. -1 = auto.
	void subProcess( int bufferOffset, int sampleFrames, bool& canSleep )
	{
		(this->*(curSubProcess_))( bufferOffset, sampleFrames, canSleep );
	}
	void setCurveType( int curveMode );
	enum CurveType{ Linear, Curved, LinearAdaptive };

private:
	void subProcessFirstSample( int bufferOffset, int sampleFrames, bool& canSleep );
	void subProcessStatic( int bufferOffset, int sampleFrames, bool& canSleep );
	void subProcessRamp( int bufferOffset, int sampleFrames, bool& canSleep );
	void subProcessCurve( int bufferOffset, int sampleFrames, bool& canSleep );
	void subProcessPulse( int bufferOffset, int sampleFrames, bool& canSleep );

	SapSubProcessMethodPointer curSubProcess_;
	double transitionTime_ = {};
	double inverseTransitionTime_ = {};
	double currentValue_ = {};
	double targetValue_ = {};
	int mode_ = {};

	// Curve variables.
	double dv = {};	//difference v (velocity)
	double ddv = {};//difference dv
	double c = {};	//constant added to ddv

	int count = {};
	double adaptiveHi_ = {};
	double adaptiveLo_ = {};
};

class RampGenerator
{
public:
	RampGenerator();
	void setTransitionTime( float transitionSamples );
	void setTarget( float targetValue );
	void setValueInstant( float targetValue );
    void jumpToTarget();
	float getInstantValue();
	float getTargetValue();
	float getNext();
	bool isDone()
	{
		return dv == 0.0;
	}

private:
	double dv = {};
	double currentValue_ = {};
	double targetValue_ = {};
	double inverseTransitionTime_ = {};
	double transitionTime_ = {};
};

class RampGeneratorAdaptive
{
	double adaptiveHi_ = {};
	double adaptiveLo_ = {};

public:
	RampGeneratorAdaptive() :
		currentValue_((std::numeric_limits<double>::max)())
		, dv(0.0)
	{}

	void Init( float sampleRate)
	{
		inverseTransitionTime_ = 1.0 / ( sampleRate * 0.015 ); // 15ms default.

		adaptiveLo_ = 1.0 / ( sampleRate * 0.050 ); // 50ms max.
		adaptiveHi_ = 1.0 / ( sampleRate * 0.001 ); // 1ms min.
	}
	void setTarget(float targetValue)
	{
		if( currentValue_ == ( std::numeric_limits<double>::max )( ) )
		{
			currentValue_ = targetValue_ = targetValue;
			dv = 0.0;
			return;
		}

		if( currentValue_ == targetValue_ )
		{
			if( inverseTransitionTime_ < adaptiveHi_ )
			{
				inverseTransitionTime_ *= 1.05; // slower 'decay', kind of peak follower.
			}
		}
		else
		{
			if( inverseTransitionTime_ > adaptiveLo_ )
			{
				inverseTransitionTime_ *= 0.9;
			}
		}

		targetValue_ = targetValue;
		dv = ( targetValue_ - currentValue_ ) * inverseTransitionTime_;
	}

	void setValueInstant(float targetValue)
	{
		currentValue_ = targetValue_ = targetValue;
		dv = 0.0;
	}

	float getInstantValue() const
	{
		return static_cast<float>(currentValue_);
	}

	float getTargetValue() const
	{
		return static_cast<float>(targetValue_);
	}

	inline float getNext()
	{
		currentValue_ += dv;

		if( dv > 0.0 )
		{
			if( currentValue_ >= targetValue_ )
			{
				currentValue_ = targetValue_;
				dv = 0.0;
			}
		}
		else
		{
			if( currentValue_ <= targetValue_ )
			{
				currentValue_ = targetValue_;
				dv = 0.0;
			}
		}

		return static_cast<float>(currentValue_);
	}

	inline bool isDone()
	{
		return dv == 0.0;
	}

	void jumpToTarget()
	{
		setValueInstant(static_cast<float>(targetValue_));
	}
private:
	double dv = {};
	double currentValue_ = {};
	double targetValue_ = {};
	double inverseTransitionTime_ = {};
};

#endif // .H INCLUDED
