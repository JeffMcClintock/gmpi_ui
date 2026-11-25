#pragma once

// Enum lists as plugins write them: "A,B,C", "A=5,B" (B is 6), or "range 0,127".
// Same parse as SynthEdit's it_enum_list2(), so the two always agree.

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

namespace gmpi::ui
{

struct EnumEntry
{
	int32_t index;
	int32_t id;
	std::string text;
};

inline std::vector<EnumEntry> parseEnumList(const std::string_view enum_list)
{
	std::vector<EnumEntry> res;

	if (enum_list.find("range") == 0) // e.g. "range 0,127"
	{
		auto p = enum_list.find(' ');
		if (p == std::string::npos)
			return res;

		auto p2 = enum_list.find(',');
		if (p2 == std::string::npos)
			return res;

		auto lo = static_cast<int32_t>(strtol(enum_list.data() + p + 1, nullptr, 10));
		auto hi = static_cast<int32_t>(strtol(enum_list.data() + p2 + 1, nullptr, 10));

		int index = 0;
		for (int32_t i = lo; i <= hi; ++i)
		{
			res.push_back({ index, i, std::to_string(i) });
			++index;
		}

		return res;
	}

	int32_t index = 0;
	int32_t id = 0;
	for (auto p = enum_list.data(); p < enum_list.data() + enum_list.size();)
	{
		auto p2 = (char*)memchr(p, ',', enum_list.data() + enum_list.size() - p);
		if (!p2)
			p2 = (char*)enum_list.data() + enum_list.size();

		auto len = p2 - p;

		// find '=' sign, extract integer after it.
		auto p3 = (char*)memchr(p, '=', p2 - p);
		if (p3)
		{
			char* endptr{};
			auto explicitId = static_cast<int32_t>(strtol(p3 + 1, &endptr, 10));

			// ignore '=' followed by non-numeric characters. Avoids weirdness
			// when a user puts '=' in a patch name.
			if (endptr != p3 + 1)
			{
				len = p3 - p;
				id = explicitId;
			}
		}

		res.push_back({ index, id, std::string(p, len) });

		p = p2 + 1;
		++id;
		++index;
	}

	return res;
}

inline EnumEntry enumListLookupId(const std::string_view enum_list, int32_t enum_value)
{
	for (auto& [index, id, text] : parseEnumList(enum_list))
	{
		if (id == enum_value)
			return { index, id, text };
	}
	return {};
}

inline EnumEntry enumListLookupIndex(const std::string_view enum_list, int32_t enum_index)
{
	const auto enums = parseEnumList(enum_list);

	if (enum_index >= 0 && static_cast<size_t>(enum_index) < enums.size())
		return enums[enum_index];

	return {};
}

} // namespace gmpi::ui
