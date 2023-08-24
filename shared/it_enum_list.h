#pragma once
#include <string>
#include <assert.h>

enum class enum_entry_type {Normal, Separator, Break, SubMenu, SubMenuEnd};

struct enum_entry
{
	int index;
	int value;
	std::wstring text;

	enum_entry_type getType() const
	{
		// Special commands (sub-menus)?
		if(text.size() < 4)
		{
			return enum_entry_type::Normal;
		}

		for(int i = 1; i < 4; ++i)
		{
			if(text[0] != text[i])
			{
				return enum_entry_type::Normal;
			}
		}

		switch (text[0])
		{
		case L'-':
			return enum_entry_type::Separator;
			break;

		case L'|':
			return enum_entry_type::Break;
			break;

		case L'>':
			return enum_entry_type::SubMenu;
			break;

		case L'<':
			return enum_entry_type::SubMenuEnd;
			break;
		}

		return enum_entry_type::Normal;
	}
};

// TODO: make more like STL, defer extracting text unless CurrentItem() called. hold pointer-to string, not copy-of.
class it_enum_list
{
public:
	it_enum_list(const std::wstring &p_enum_list);
	enum_entry * CurrentItem(){	assert(!IsDone() );	return &m_current;}
	enum_entry * operator*(){	assert(!IsDone() );	return &m_current;}
	bool IsDone(){return m_current.index == -1;}
	void Next();
	it_enum_list &operator++(){Next();return *this;} //Prefix increment
	void First();
	int size();
	bool FindValue( int p_value );
	bool FindIndex( int p_index );
	static bool IsValidValue( const std::wstring  &p_enum_list, int p_value );
	static int ForceValidValue( const std::wstring  &p_enum_list, int p_value );
	bool IsRange(){return m_range_mode;};
	int RangeHi(){return range_hi;}
	int RangeLo(){return range_lo;}

private:
	int StringToInt(const std::wstring  &string, int p_base = 10);
	std::wstring  m_enum_list;
	enum_entry m_current;
	bool m_range_mode;
	int range_lo; // also used as current position within string
	int range_hi;
};


