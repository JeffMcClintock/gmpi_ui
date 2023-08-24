/*
#include "../shared/voltage_conversions.h"

using namespace SynthEdit;
*/

#pragma once
#ifndef VOLTAGE_CONVERSIONS_H_INCLUDED
#define VOLTAGE_CONVERSIONS_H_INCLUDED

#include "math.h"

namespace SynthEdit
{

// Envelope time calculation. ADSR2.
inline float VoltsToSeconds(float v)
{
	return powf(10.0f, ( v * 0.4f ) - 3.0f);
}

inline float SecondsToVolts(float s)
{
	const float minimumTime = 0.0000001f;

	if( s < 0.0000001f ) // prevent overflow.
		s = minimumTime;

	return ( log10f(s) + 3.0f ) * 2.5f;
}

inline float MilliSecondsToVolts(float ms)
{
	return SecondsToVolts(ms * 0.001f);
}

//VCA Conversions
// VCA dB (new) mode.
inline float dBToVolts(float dB)
{
	if( dB > -35.0f )
	{
		return 10.0f + dB * 9.0f / 35.0f;
	}
	else
	{
		// Slope steepens below -35dB.
		return 49.393f * expf(dB * 0.114f);
	}
}

/*
Middle-A is 440Hz, MIDI Note 69, 5 Volts (0.5 normalized audio).
*/
static const int MIDDLE_A_MIDI = 69;
static const int MIDDLE_A_VOLTS = 5;
static const int VOLT_RANGE = 10;
static const int SEMITONE_PER_OCTAVE = 12;

inline float SemitoneToOctave(float pitchInSemitones)
{
	const float FLOAT_STEP_PER_NOTE = 1.f / (float) SEMITONE_PER_OCTAVE;
	return MIDDLE_A_VOLTS + (pitchInSemitones - MIDDLE_A_MIDI) * FLOAT_STEP_PER_NOTE;
}

inline float VoltsToAudio(float volts)
{
	return volts * 0.1f;
}

}

#endif