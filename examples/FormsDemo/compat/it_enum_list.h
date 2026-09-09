#pragma once

// A shim, not a component of this example.
//
// gmpi_ui/experimental/builders.cpp - the widget set this demo is built from -
// does `#include "it_enum_list.h"` and uses three of its functions to turn a
// combo box's comma-separated option string into entries. That header lives in
// the SynthEdit SDK (SynthEditLib/modules/shared/it_enum_list.h), so building
// those widgets normally requires SynthEditLib on the include path.
//
// This example depends on GMPI and gmpi_ui and nothing else, so the three
// functions builders.cpp actually calls are reproduced here, byte-compatible
// with the original (and identical to the copy the GMPI standalone wrapper
// carries in GMPI_Wrappers/wrapper/Standalone/compat/). The option-string
// syntax (`a,b,c`, explicit `name=7` ids, and `range lo,hi`) is a format
// plugins write, so the parsers must agree exactly.
//
// Only this example's translation units see this directory (it is a PRIVATE
// include path in CMakeLists.txt), so a project that has the real header keeps
// getting it.

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

struct enum_entry2
{
    int32_t index;
    int32_t id;
    std::string text;
};

inline std::vector<enum_entry2> it_enum_list2(const std::string_view enum_list)
{
    std::vector<enum_entry2> res;

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

inline enum_entry2 enum_list_lookup_id(const std::string_view enum_list, int32_t enum_value)
{
    for (auto& [index, id, text] : it_enum_list2(enum_list))
    {
        if (id == enum_value)
            return { index, id, text };
    }
    return {};
}

inline enum_entry2 enum_list_lookup_index(const std::string_view enum_list, int32_t enum_index)
{
    const auto enums = it_enum_list2(enum_list);

    if (enum_index >= 0 && static_cast<size_t>(enum_index) < enums.size())
        return enums[enum_index];

    return {};
}
