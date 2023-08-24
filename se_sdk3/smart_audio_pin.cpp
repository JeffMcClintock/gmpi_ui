// Copyright 2008 Jeff McClintock

#include "smart_audio_pin.h" 
#include <algorithm>

SmartAudioPin::SmartAudioPin() :
currentValue_( 0.0 )
,targetValue_( 0.0 )
,transitionTime_( 40 )
,mode_( Linear )
{
	inverseTransitionTime_ = 1.0 / transitionTime_;
	curSubProcess_ = &SmartAudioPin::subProcessFirstSample;
}

void SmartAudioPin::setValueInstant( float targetValue, int blockPosition )
{
	curSubProcess_ = &SmartAudioPin::subProcessStatic;

	if( currentValue_ != targetValue || isStreaming() )
	{
		if( blockPosition == -1 )
		{
			blockPosition = plugin_->getBlockPosition();
		}

		setStreaming( false, blockPosition );
	}
	currentValue_ = targetValue_ = targetValue;
}

void SmartAudioPin::setValue( float targetValue, int blockPosition )
{
	if ( transitionTime_ == 0 || curSubProcess_ == &SmartAudioPin::subProcessFirstSample )
	{
		setValueInstant(targetValue, blockPosition);
		return;
	}

	if( blockPosition == -1 )
	{
		blockPosition = plugin_->getBlockPosition();
	}

	targetValue_ = targetValue;

	// already at target?, nothing to do except stop streaming (if doing so).
	if( currentValue_ == targetValue_ )
	{
		curSubProcess_ = &SmartAudioPin::subProcessStatic;
		if( isStreaming() )
		{
			setStreaming( false, blockPosition );
		}
		return;
	}

	assert(transitionTime_ != 0);

	switch (mode_)
	{
	case LinearAdaptive:
		{
			if (curSubProcess_ == &SmartAudioPin::subProcessRamp)
			{
				if (inverseTransitionTime_ < adaptiveHi_)
				{
					inverseTransitionTime_ *= 1.05; // slower 'decay', kind of peak follower.
				}
			}
			else
			{
				if (inverseTransitionTime_ > adaptiveLo_)
				{
					inverseTransitionTime_ *= 0.9;
				}
			}

			dv = (targetValue_ - currentValue_) * inverseTransitionTime_;
			curSubProcess_ = &SmartAudioPin::subProcessRamp;
//			_RPT1(_CRT_WARN, "ms %f\n", 1000.0f / (plugin_->getSampleRate()*inverseTransitionTime_));
		}
		break;

	case Linear:
		{
			dv = ( targetValue_ - currentValue_ ) * inverseTransitionTime_;
			curSubProcess_ = &SmartAudioPin::subProcessRamp;
		}
		break;

	default: // curve mode.
		{
			curSubProcess_ = &SmartAudioPin::subProcessCurve;

			double N = transitionTime_;
			double A = targetValue_ - currentValue_;   //amp

			//v   = currentValue_;
			double NNN = N*N*N;
			dv  = A*(3*N - 2)/(NNN);  //difference v
			ddv = 6*A*(N - 2)/(NNN);  //difference dv
			c   = -12.0 * A /(NNN);  //constant added to ddv

			count = (int)transitionTime_ - 1;
			//target = p_end;

			// take first step
			currentValue_ += dv;
			dv += ddv;
			ddv += c;
		}
	}

	setStreaming( true, blockPosition );
}

void SmartAudioPin::pulse( float pulseHeight, int blockPosition )
{
	if( blockPosition == -1 )
	{
		blockPosition = plugin_->getBlockPosition();
	}

	currentValue_ = targetValue_ = pulseHeight;
	curSubProcess_ = &SmartAudioPin::subProcessPulse;
	setStreaming( false, blockPosition );
	count = (int) transitionTime_;
}

void SmartAudioPin::subProcessFirstSample( int bufferOffset, int sampleFrames, bool& canSleep )
{
	// On first sample, set streaming off.
	setStreaming( false, bufferOffset );
	curSubProcess_ = &SmartAudioPin::subProcessStatic;
	subProcessStatic( bufferOffset, sampleFrames, canSleep );
}

void SmartAudioPin::subProcessStatic( int bufferOffset, int sampleFrames, bool& /*canSleep*/ )
{
	float* out = bufferOffset + getBuffer();

	for( int s = sampleFrames ; s > 0 ; s-- )
	{
		*out++ = static_cast<float>(targetValue_);
	}
}

void SmartAudioPin::subProcessPulse( int bufferOffset, int sampleFrames, bool& canSleep )
{
	if( count == 0 ) // because pulse ended on very last sample of previous block.
	{
		targetValue_ = 0.0;
		curSubProcess_ = &SmartAudioPin::subProcessStatic;
		setStreaming( false, bufferOffset );
	}
	else
	{
		assert( count > 0 );

		if( count < sampleFrames )
		{
			// output tail of HI pulse.
			bool temp;
			subProcessStatic( bufferOffset, count, temp );

			sampleFrames -= count;
			bufferOffset += count;

			count = 0;
			targetValue_ = 0.0;

			// send pin status update.
			setStreaming( false, bufferOffset );
		}
		else
		{
			// pulse remains HI, keep counting down.
			count -= sampleFrames;

			// Keep module from sleeping (otherwise module will assume it can sleep because pin is static).
			plugin_->resetSleepCounter();
		}
	}

	assert( sampleFrames > 0 );
	{
		subProcessStatic( bufferOffset, sampleFrames, canSleep );
	}
}

// could make faster with sub-process-ramp-up/ ramp-down
void SmartAudioPin::subProcessRamp( int bufferOffset, int sampleFrames, bool& canSleep )
{
	canSleep = false;
	float* out = bufferOffset + getBuffer();

	for( int s = sampleFrames ; s > 0 ; s-- )
	{
		currentValue_ += dv;
		if( (dv > 0.0 && currentValue_ >= targetValue_) || (dv <= 0.0 && currentValue_ <= targetValue_))
		{
			currentValue_ = targetValue_;
			curSubProcess_ = &SmartAudioPin::subProcessStatic;
			setStreaming( false, bufferOffset + sampleFrames - s );

			// fill remainder of block if nesc
//			s--;
			for( ; s > 0 ; s-- )
			{
				*out++ = static_cast<float>(currentValue_);
			}
			return;
		}
		*out++ = static_cast<float>(currentValue_);
	}
}

void SmartAudioPin::subProcessCurve( int bufferOffset, int sampleFrames, bool& canSleep )
{
	canSleep = false;
	float* out = bufferOffset + getBuffer();

	for( int s = sampleFrames ; s > 0 ; --s )
	{
		*out++ = static_cast<float>(currentValue_);

		if( --count < 0 ) // done?
		{
			currentValue_ = targetValue_;
			// done in loop below*out = currentValue_; // correct small numeric errors. ensure output *exact*.

			curSubProcess_ = &SmartAudioPin::subProcessStatic;
			setStreaming( false, bufferOffset + sampleFrames - s );

			// fill remainder of block if nesc
			//s--;
			--out; // back up one so target sample exact value (no numerical error).
			for( ; s > 0 ; --s )
			{
				*out++ = static_cast<float>(currentValue_);
			}
			return;
		}

		currentValue_ += dv;
		dv += ddv;
		ddv += c;
	}
}

void SmartAudioPin::setTransitionTime( float transitionTime )
{
	transitionTime_ = ( std::max )( 1.0f, transitionTime );
	inverseTransitionTime_ = 1.0 / transitionTime_;
}

float SmartAudioPin::getInstantValue()
{
	return static_cast<float>(currentValue_);
}

void SmartAudioPin::setCurveType( int curveMode )
{
	mode_ = curveMode;
	inverseTransitionTime_ = 1.0 / (plugin_->getSampleRate() * 0.015); // 15ms default.

	adaptiveLo_ = 1.0 / (plugin_->getSampleRate() * 0.050); // 50ms max.
	adaptiveHi_ = 1.0 / (plugin_->getSampleRate() * 0.001); // 1ms min.
}

// Ramp Generator class.
RampGenerator::RampGenerator() :
currentValue_(0.0)
,dv(0.0)
{
}

void RampGenerator::setTransitionTime( float transitionSamples )
{
	const auto safeSamples = (std::max)(1.0f, transitionSamples);
	if( transitionTime_ != safeSamples)
	{
		transitionTime_ = safeSamples;
		inverseTransitionTime_ = 1.0 / transitionTime_;
	}
}

void RampGenerator::setTarget( float targetValue )
{
	targetValue_ = targetValue;
	dv = ( targetValue_ - currentValue_ ) * inverseTransitionTime_;
}

void RampGenerator::setValueInstant( float targetValue )
{
	currentValue_ = targetValue_ = targetValue;
	dv = 0.0;
}

void RampGenerator::jumpToTarget()
{
	currentValue_ = targetValue_;
	dv = 0.0;
}

float RampGenerator::getNext()
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

float RampGenerator::getInstantValue()
{
	return static_cast<float>(currentValue_);
}

float RampGenerator::getTargetValue()
{
	return static_cast<float>(targetValue_);
}
