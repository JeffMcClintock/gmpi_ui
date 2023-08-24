#pragma once

/*
#include "ListBuilder.h"
*/
#include <string>

template <typename T>
class ListBuilder_base
{
	bool first = true;
	int index = 0;
	typedef std::basic_string< T, std::char_traits<T>, std::allocator<T> > my_string_type;

public:
	my_string_type list;

	void Add(my_string_type s)
	{
		if (first)
		{
			first = false;
		}
		else
		{
			list += (T) ',';
		}
		list += s; // todo quote if commas

		++index;
	}

	void Add(my_string_type s, int indexOverride)
	{
		if (first)
		{
			first = false;
		}
		else
		{
			list += L',';
		}
		list += s; // todo quote if commas

		if (indexOverride != index)
		{
			index = indexOverride;
			char buffer[20];
			sprintf(buffer, "=%d", index);
			for (char* c = buffer; *c != 0; ++c)
			{
				list += (T)*c;
			}
		}
		
		++index;
	}

	my_string_type& str()
	{
		return list;
	}
};

class ListBuilder : public ListBuilder_base<wchar_t>
{};