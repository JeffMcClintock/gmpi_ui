#pragma once
#include <vector>
#include "../se_sdk3/mp_sdk_stdint.h"
#include "RawView.h"

namespace SynthEdit2
{
	// kind of a patch-cable-list deserialiser.
	class PatchCables
	{
		struct Cable
		{
			int32_t fromUgHandle;
			int32_t toUgHandle;
			int32_t fromUgPin;
			int32_t toUgPin;
			int32_t colorIndex;
		};

	public:
		std::vector< Cable > cables;

		PatchCables() {}
		PatchCables(RawView raw);
		RawData Serialise();

		void insert(int32_t fromModule, int fromPin, int32_t toModule, int toPin, int color)
		{
			cables.insert(cables.begin(), {fromModule, toModule, fromPin, toPin, color});
		}

		void push_back(int32_t fromModule, int fromPin, int32_t toModule, int toPin, int color)
		{
			cables.push_back({fromModule, toModule, fromPin, toPin, color});
		}

		void push_back(Cable& c)
		{
			cables.push_back(c);
		}
	};
}