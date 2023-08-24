#include "it_enum_list.h"
#include <stdlib.h>
#include <wctype.h>

using namespace std;

int it_enum_list::StringToInt(const std::wstring  &string, int p_base)
{
//	assert( string.GetLength() < 9 || string.Find(_T(",")) != 0 ); // else too many digits overflows sizeof int
	wchar_t * temp;
	return static_cast<int>(wcstol( string.c_str(), &temp, p_base ));
}


it_enum_list::it_enum_list(const std::wstring  &p_enum_list) :
m_enum_list(p_enum_list)
{
	// two formats are allowed, a list of values or a range
	if( m_enum_list.substr(0,5) == std::wstring ( L"range" ) ) // how about upper/mixed case?
	{
		m_range_mode = true;
		std::wstring  list = m_enum_list;

		list = list.substr( 6, list.size() - 1);

		std::string::size_type p = list.find(L",");
		assert( p != string::npos );	// no range specified

		range_lo = StringToInt( list );
		list = list.substr( p + 1, list.size() - 1);
		range_hi = StringToInt( list );
	}
	else
	{
		m_range_mode = false;
	}
}

void it_enum_list::First()
{
	// two formats are allowed, a list of values or a range
	if( m_range_mode )
	{
		m_current.value = range_lo - 1;
	}
	else
	{
		range_lo = 0;
		m_current.value = -1;
	}
	m_current.index = -1; // anticipate initial next()
	Next();
}

void it_enum_list::Next()
{
	m_current.index++;
	m_current.value++;

	if(m_range_mode)
	{
		wchar_t temp[16];
		swprintf( temp, 16, L"%d", m_current.value);
		m_current.text = temp;
		if(m_current.value > range_hi)
		{
			m_current.index = -1;
			return;
		}
	}
	else
	{
		// find next comma
		std::string::size_type p = m_enum_list.find(L",", range_lo);
		if( p == string::npos ) // none left
		{
			p = m_enum_list.size();

			if( range_lo >= (int) p ) // then we are done
			{
				m_current.index = -1;
				return;
			}
		}

		size_t sub_string_length = p - range_lo;
		m_current.text = m_enum_list.substr( range_lo, sub_string_length);

		std::string::size_type p_eq = m_current.text.find(L"=");
		if( p_eq != string::npos )
		{
			std::wstring  id_str = m_current.text.substr(p_eq + 1, m_current.text.size() - 1 );
			m_current.text = m_current.text.substr( 0, p_eq );
			m_current.value = StringToInt(id_str);

			wchar_t* endptr;
			int numericPart = static_cast<int>( wcstol( id_str.c_str(), &endptr, 10 ) );
			// ignore '=' followed by non-numeric characters. Avoids weirdness when user puts '=' in patch name.
			if( endptr != id_str.c_str() )
			{
				m_current.value = numericPart;
			}
		}

		range_lo = (int)p + 1;

		// Trim spaces from start and end of text
		size_t st = 0;
		size_t en = m_current.text.size();
		while(st < en && iswspace( m_current.text[st] ) )
		{
			++st;
		}
		while(en > st && iswspace( m_current.text[en - 1] ) )
		{
			--en;
		}

		if( st > 0 || en < m_current.text.size() )
		{
			m_current.text = m_current.text.substr(st,en-st);
		}
	}
}

int it_enum_list::size()
{
	if(m_range_mode)
	{
		return 1 + abs(range_hi - range_lo);
	}
	else
	{
		if (m_enum_list.size() < 1)
			return 0;

		// count number of commas.
		int sz = 1;
		for( size_t i = m_enum_list.size() - 1 ; i > 0 ; i-- )
		{
			if( m_enum_list[i] == ',' )
				sz++;
		}
		return sz;
	}
}

bool it_enum_list::FindValue( int p_value )
{
	// could be specialied for ranges

	for( First(); !IsDone(); Next() )
	{
		if(CurrentItem()->value == p_value && CurrentItem()->getType() == enum_entry_type::Normal)
			return true;
	}
	return false;
}

bool it_enum_list::FindIndex( int p_index )
{
	for( First(); !IsDone(); Next() )
	{
		if(CurrentItem()->index == p_index && CurrentItem()->getType() == enum_entry_type::Normal)
			return true;
	}
	return false;
}

bool it_enum_list::IsValidValue( const std::wstring  &p_enum_list, int p_value )
{
	it_enum_list itr(p_enum_list);
	return itr.FindValue(p_value);
}

// ensure a value is one of the valid choices, if not return first item, if no items avail return 0
int it_enum_list::ForceValidValue( const std::wstring  &p_enum_list, int p_value )
{
	it_enum_list itr(p_enum_list);
	if( itr.FindValue(p_value) )
		return p_value;

	itr.First();
	if( !itr.IsDone() )
		return itr.CurrentItem()->value;

	return 0;
}
