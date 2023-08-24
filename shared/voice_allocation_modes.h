#pragma once
/*
#include "voice_allocation_modes.h"
*/

/* CS80
From Synthmuseum.com: The sustain selector switches between two types of sustain (which is not "sustain" as we know it,
but rather affects envelope release). The first setting (I) lets a note decay normally. What is very cool is the second (II) sustain mode .
In most synths, you can't have a long release on a polyphonic lead sound because you end up with a blur of notes. In sustain mode II hitting a new key,
after letting go of all keys, kills sustaining notes. This lets you play sustained chords and leads together,
an effect unknown to any other synth. Sustain can also be turned off and on via a foot-switch (much like a piano's sustain pedal).
*/

// bits 0-2 is voice allocation mode. (AND with 0x07)
enum VoiceAllocationModes { VA_POLY_SOFT, VA_POLY_HARD, VA_POLY_OVERLAP, VA_POLY_OVERLAP_GUYR, VA_MONO_DEPRECATED = 4, VA_MONO_RETRIGGER_DEPRECATED };

// bits 8-9 byte is note priority
enum NotePriorityModes { NP_NONE, NP_LOWEST, NP_HIGHEST, NP_LAST };

// bit 10 reserved for future expansion.

// bits 11-13 indicate mono-mode and retrigger or not. MM_NOT_SET means "look at legacy system" (VA_MONO_RETRIGGER_DEPRECATED).
enum MonoRetrigger { MM_NOT_SET = 0, MM_IN_USE = 1, MM_ON = 2, MM_RETRIGGER = 4 }; 

// bit 15 reserved for future expansion.

// bit 16 glide type
// bit 17 reserved
// bit 18 glide rate type
enum NoteGlideModes { GT_LEGATO, GT_ALWAYS, GT_CONST_RATE = 0, GT_CONST_TIME = 4 };

// bits 19 Voice Refresh Disable
enum VoiceRefresh { NR_VOICE_REFRESH_ON, NR_VOICE_REFRESH_DISABLED };
/*
	auto refresh = ( ( lVoiceAllocationMode >> 19 ) & 0x01 ) == NR_VOICE_REFRESH_ON;
 */

namespace voice_allocation
{
	namespace bits
	{
		constexpr int AllocationModes_startbit = 0;
		constexpr int AllocationModes_sizebits = 3;

		constexpr int NotePriority_startbit = 8;
		constexpr int NotePriority_sizebits = 2;

		constexpr int MonoModes_startbit = 11;
		constexpr int MonoModes_sizebits = 3;

		constexpr int GlideType_startbit = 16;
		constexpr int GlideType_sizebits = 1;

		constexpr int GlideRate_startbit = 18;
		constexpr int GlideRate_sizebits = 1;

		constexpr int VoiceRefresh_startbit = 19;
		constexpr int VoiceRefresh_sizebits = 1;
	}

	inline int32_t extractBits(int32_t bitfield, int lowBit, int countBits)
	{
		assert(countBits > 0);

		const int bitmask = (1 << countBits) - 1;

		return (bitfield >> lowBit) & bitmask;
	}

	inline int32_t insertBits(int32_t bitfield, int lowBit, int countBits, int value)
	{
		assert(countBits > 0);

		const int bitmask = (1 << countBits) - 1;

		int newBits = (value & bitmask) << lowBit;

		const auto bitmask2 = bitmask << lowBit;

		return (bitfield & ~bitmask2) | newBits;
	}

	inline bool isMonoMode(int32_t voiceAllocationMode)
	{
		const auto monoControl = extractBits(voiceAllocationMode, bits::MonoModes_startbit, bits::MonoModes_sizebits);
		if(monoControl == MM_NOT_SET)
		{
			const auto r = extractBits(voiceAllocationMode, 0, 3);
			return r == VA_MONO_RETRIGGER_DEPRECATED || r == VA_MONO_DEPRECATED;
		}
		else
		{
			return 0 != (monoControl & MM_ON);
		}
	}

	inline bool isMonoRetrigger(int32_t voiceAllocationMode)
	{
		const auto monoControl = extractBits(voiceAllocationMode, bits::MonoModes_startbit, bits::MonoModes_sizebits);
		if(monoControl == MM_NOT_SET)
		{
			const auto r = extractBits(voiceAllocationMode, 0, 3);
			return r == VA_MONO_RETRIGGER_DEPRECATED || r == VA_POLY_HARD;
		}
		else
		{
			return 0 != (monoControl & MM_RETRIGGER);
		}
	}
}