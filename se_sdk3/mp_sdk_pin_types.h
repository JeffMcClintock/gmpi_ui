#ifndef MP_SDK_PIN_TYPES_H_INCLUDED
#define MP_SDK_PIN_TYPES_H_INCLUDED

// BLOB - Binary datatype.
struct MpBlob
{
	MpBlob();
	// copy constructor. supports use in standard containers.
	MpBlob( const MpBlob& other );
	~MpBlob();

	void setValueRaw( int size, const void* data );
	const MpBlob &operator=( const MpBlob& other );
	bool operator==( const MpBlob& other ) const;
	bool compare( char* data, int size );
	bool operator!=( const MpBlob& other );
	int32_t getSize() const;
	char* getData() const;

private:
	int32_t size_;
	char* data_;
};

template <typename T>
class MpTypeTraits
{
private:
	// convert type to int representing datatype. N is dummy to satisfy partial specialization rules enforced by GCC.
	template <class U, int N> struct PinDataTypeTraits
	{
	};
	template<int N> struct PinDataTypeTraits<int,N>
	{
		enum { result = gmpi::MP_INT32 };
	};
	template<int N> struct PinDataTypeTraits<bool,N>
	{
		enum { result = gmpi::MP_BOOL };
	};
	template<int N> struct PinDataTypeTraits<float,N>
	{
		enum { result = gmpi::MP_FLOAT32 };
	};
	template<int N> struct PinDataTypeTraits<std::wstring,N>
	{
		enum { result = gmpi::MP_STRING };
	};
	template<int N> struct PinDataTypeTraits<MpBlob,N>
	{
		enum { result = gmpi::MP_BLOB };
	};

public:
	enum{ PinDataType = PinDataTypeTraits<T,0>::result };
};


// Get size of variable's data.
template <typename T>
inline int variableRawSize( const T &value )
{
	return sizeof(T);
};

template<>
inline int variableRawSize<std::wstring>( const std::wstring& value )
{
	return (int) sizeof(wchar_t) * (int) value.length();
}

template<>
inline int variableRawSize<MpBlob>( const MpBlob& value )
{
	return value.getSize();
}

// Serialize variable's value as bytes.
template <typename T>
inline void* variableRawData( const T &value )
{
	return (void*) &value;
};

template<>
inline void* variableRawData<std::wstring>( const std::wstring& value )
{
	return (void*) value.data();
};

template<>
inline void* variableRawData<MpBlob>( const MpBlob& value )
{
	return (void*) value.getData();
};


// Compare two instances of a type.
template <typename T>
inline bool variablesAreEqual( const T& a, const T& b )
{
	return a == b;
};

template <>
inline bool variablesAreEqual<std::wstring>( const std::wstring& a, const std::wstring& b )
{
	return a.compare(b) == 0;
};

template <typename T>
inline void setVariableToDefault( T& value )
{
	value = (T) 0;
};

template <>
inline void setVariableToDefault<std::wstring>( std::wstring& value )
{
	value = L"";
};

template <>
inline void setVariableToDefault<struct MpBlob>( struct MpBlob& value )
{
	value.setValueRaw( 0, 0 );
};

// De-serialize type.
template <typename T>
inline void VariableFromRaw( int size, void* data, T& returnValue )
{
	assert( size == sizeof(T) );
	memcpy( &returnValue, data, size );
};

template <>
inline void VariableFromRaw<struct MpBlob>( int size, void* data, struct MpBlob& returnValue )
{
	returnValue.setValueRaw( size, data );
}

template <>
inline void VariableFromRaw<bool>( int size, void* data, bool& returnValue )
{
	// bool is pased as int.
	if( size == 4 ) // DSP sends bool events as int.
	{
		returnValue = *((int*) data) != 0;
	}
	else
	{
		assert( size == 1 );
		returnValue = *((bool*) data);
	}
}
template <>
inline void VariableFromRaw<std::wstring>( int size, void* data, std::wstring& returnValue )
{
	returnValue.assign( (wchar_t* ) data, size / sizeof(wchar_t) );
};

// Specializations of above for various types.

#endif	// MP_SDK_PIN_TYPES_H_INCLUDED

//===== MpGraphicsExtensionWinGdi.h =====
