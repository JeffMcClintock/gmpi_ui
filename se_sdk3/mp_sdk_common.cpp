// Copyright 2006 Jeff McClintock

#include <memory>
#include "mp_sdk_gui.h"
#include <sstream>

using namespace gmpi;

// Only standalone plugins need entrypoint.
#if defined( SE_EDIT_SUPPORT ) || defined( SE_TARGET_PLUGIN )
#define COMPILE_HOST_SUPPORT
#endif

#ifndef COMPILE_HOST_SUPPORT

//---------------FACTORY --------------------

// MpFactory - a singleton object.  The plugin registers it's ID with the factory.

class MpFactory: public IMpFactory2
{
	typedef std::map< std::pair<int32_t, std::wstring>, MP_CreateFunc2> MpPluginInfoMap;
	typedef std::map< std::pair<int32_t, std::wstring>, MP_CreateFunc2 >::iterator MpPluginInfoIterator;

public:
	MpFactory( void ){};
	virtual ~MpFactory( void ){};

	/* IMpUnknown methods */
	virtual int32_t MP_STDCALL queryInterface( const MpGuid& iid, void** returnInterface ) override;
	GMPI_REFCOUNT_NO_DELETE

	/* IMpFactory methods */
	virtual int32_t MP_STDCALL createInstance(
		const wchar_t* uniqueId,
		int32_t subType,
		IMpUnknown* host,
		void** returnInterface ) override;

	virtual int32_t MP_STDCALL createInstance2(
		const wchar_t* uniqueId,
		int32_t subType,
		void** returnInterface ) override;

	virtual int32_t MP_STDCALL getSdkInformation( int32_t& returnSdkVersion, int32_t maxChars, wchar_t* returnCompilerInformation ) override;

	// Registration
	int32_t RegisterPlugin( const wchar_t* uniqueId, int32_t subType, MP_CreateFunc2 create );

private:
	// a map of all registered IIDs: public for factory access
	MpPluginInfoMap m_pluginMap;
};


MpFactory* Factory()
{
	static MpFactory* theFactory = 0;

	// Initialize on first use.  Ensures the factory is alive before any other object
	// including other global static objects (allows plugins to auto-register).
	if( theFactory == 0 )
	{
		theFactory = new MpFactory;
	}

	return theFactory;
}

//  Factory lives until plugin dll is unloaded.
//  this helper destroys the factory automatically.
class factory_deleter_helper
{
public:
	~factory_deleter_helper()
	{
		delete Factory();
	}
} grim_reaper;

#if !defined(_M_X64) && defined(_MSC_VER)
	// Export additional symbol without name decoration.
	#pragma comment( linker, "/export:MP_GetFactory=_MP_GetFactory@4" )
#endif

// This is the DLL's main entry point.  It returns the factory.
extern "C"

#ifdef _WIN32
	#define VST_EXPORT __declspec (dllexport)
#else

#if defined (__GNUC__)
	#define VST_EXPORT	__attribute__ ((visibility ("default")))
#else
	#define VST_EXPORT
#endif

#endif


VST_EXPORT
int32_t MP_STDCALL MP_GetFactory( void** returnInterface )
{
	// call queryInterface() to keep refcounting in sync
	return Factory()->queryInterface( MP_IID_UNKNOWN, returnInterface );
}

// register a DSP plugin with the factory
int32_t RegisterPlugin( int subType, const wchar_t* uniqueId, MP_CreateFunc2 create )
{
	return Factory( )->RegisterPlugin( uniqueId, subType, create );
}

// register plugin's XML with the factory. Deprecated.
int32_t RegisterPluginXml( const char* /*xmlFile*/ )
{
	// Not supported yet in external plugin. XML read directly from resource by host.
//	return Factory()->RegisterPluginXml( xmlFile );
	return gmpi::MP_OK;
}

// Factory methods

int32_t MpFactory::RegisterPlugin( const wchar_t* uniqueId, int32_t subType, MP_CreateFunc2 create )
{
	MpPluginInfoIterator it = m_pluginMap.find( std::pair<int32_t, std::wstring>( subType, uniqueId ) );

	if( it == m_pluginMap.end( ) )
	{
#if defined(_DEBUG)
		std::pair< MpPluginInfoIterator, bool > res =
#endif
        m_pluginMap.insert( std::pair< std::pair<int32_t, std::wstring>, MP_CreateFunc2 >( std::pair<int32_t, std::wstring>( subType, std::wstring( uniqueId ) ), create ) );

#if defined(_DEBUG)
		assert( res.second );
#endif
	}
	return gmpi::MP_OK;
}

int32_t
MpFactory::queryInterface( const MpGuid& iid, void** returnInterface )
{
	if ( iid == MP_IID_FACTORY2 || iid == MP_IID_FACTORY || iid == MP_IID_UNKNOWN )
	{
		*returnInterface = this;
		addRef();
		return gmpi::MP_OK;
	}

	*returnInterface = 0;
	return MP_NOSUPPORT;
}

int32_t MpFactory::createInstance( const wchar_t* uniqueId, int32_t subType,
	gmpi::IMpUnknown* host,
	void** returnInterface )
{
	*returnInterface = 0; // if we fail for any reason, default return-val to NULL

	/* search m_pluginMap for the requested IID */
	MpPluginInfoIterator it = m_pluginMap.find( std::pair<int32_t, std::wstring>( subType, uniqueId ) );
	if( it == m_pluginMap.end( ) )
	{
		return MP_NOSUPPORT;
	}

	MP_CreateFunc2 create = ( *it ).second;

	if( create == 0 )
	{
		return MP_NOSUPPORT;
	}

	try
	{
		auto m = create();
		*returnInterface = m;
#ifdef _DEBUG
		{
			m->addRef();
			int refcount = m->release();
			assert(refcount == 1);
		}
#endif

		IMpLegacyInitialization* p = dynamic_cast<IMpLegacyInitialization*>( m );
		if( p )
		{
			p->setHost( host );
		}
	}

	// the new function will throw a std::bad_alloc exception if the memory allocation fails.
	// the constructor will throw a char* if host don't support required interfaces.
	catch(...)
	{
		return MP_FAIL;
	}

	return gmpi::MP_OK;
}

int32_t MpFactory::createInstance2( const wchar_t* uniqueId, int32_t subType,
	void** returnInterface )
{
	*returnInterface = 0; // if we fail for any reason, default return-val to NULL

	/* search m_pluginMap for the requested IID */
	MpPluginInfoIterator it = m_pluginMap.find( std::pair<int32_t, std::wstring>( subType, uniqueId ) );
	if( it == m_pluginMap.end( ) )
	{
		return MP_NOSUPPORT;
	}

	MP_CreateFunc2 create = ( *it ).second;

	if( create == 0 )
	{
		return MP_NOSUPPORT;
	}

	try
	{
		auto m = create();
		*returnInterface = m;
#ifdef _DEBUG
		{
			m->addRef();
			int refcount = m->release();
			assert(refcount == 1);
		}
#endif
	}

	// the new function will throw a std::bad_alloc exception if the memory allocation fails.
	// the constructor will throw a char* if host don't support required interfaces.
	catch( ... )
	{
		return MP_FAIL;
	}

	return gmpi::MP_OK;
}

int32_t MpFactory::getSdkInformation( int32_t& returnSdkVersion, int32_t maxChars, wchar_t* returnCompilerInformation )
{
	returnSdkVersion = GMPI_SDK_REVISION;

	std::wostringstream oss;

	// use safe string printf if possible.
	#if defined(_MSC_VER ) && _MSC_VER >= 1400
//			_snwprintf_s( returnCompilerInformation, maxChars, _TRUNCATE, L"MS Compiler V%d (DEBUG)", (int) _MSC_VER );
		oss << L"MS Compiler V" << _MSC_VER;
	#else
		#if defined( __GXX_ABI_VERSION )
//			swprintf( returnCompilerInformation, maxChars, L"GCC Compiler V%d", (int) __GXX_ABI_VERSION );
			oss << L"GCC Compiler V" << __GXX_ABI_VERSION;
		#else
//			wcscpy( returnCompilerInformation, L"Unknown Compiler" );
			oss << L"Unknown Compiler";
		#endif
	#endif
	#if defined( _DEBUG )
		oss << L" (Debug)";
	#else
		oss << L" (Release)";
	#endif

#if defined(_MSC_VER )
	wcscpy_s( returnCompilerInformation, static_cast<rsize_t>(maxChars), oss.str().c_str() );
#else
	wcscpy(returnCompilerInformation, oss.str().c_str());
#endif

	return gmpi::MP_OK;
}

#endif	// COMPILE_HOST_SUPPORT

// BLOB datatype

MpBlob::MpBlob() :
	size_(0),
	data_(0)
{
}

void MpBlob::setValueRaw(size_t size, const void* data)
{
	if (size_ != size)
	{
		delete[] data_;
		size_ = size;
		data_ = new char[size_];
	}

	if (size_ > 0)
	{
		memcpy(data_, data, size_);
	}
}

void MpBlob::resize( int size )
{
	if( size_ < static_cast<size_t>(size) )
	{
		delete [] data_;
		if( size > 0 )
		{
			data_ = new char[static_cast<size_t>(size)];
		}
		else
		{
			data_ = 0;
		}
	}

	size_ = static_cast<size_t>(size);
}

int32_t MpBlob::getSize() const
{
	return (int32_t) size_;
}

char* MpBlob::getData() const
{
	return data_;
}

const MpBlob& MpBlob::operator=( const MpBlob &other )
{
	setValueRaw( other.size_, other.data_ );
	return other;
}

bool MpBlob::operator==( const MpBlob& other ) const
{
	if( size_ != other.size_ )
		return false;

	for( size_t i = 0 ; i < size_ ; ++i )
	{
		if( data_[i] != other.data_[i] )
			return false;
	}
	return true;
}

bool MpBlob::compare( char* data, int size )
{
	if( size_ != static_cast<size_t>(size) )
		return false;

	for( size_t i = 0 ; i < size_ ; ++i )
	{
		if( data_[i] != data[i] )
			return false;
	}
	return true;
}

MpBlob::~MpBlob()
{
	delete [] data_;
}

bool MpBlob::operator!=( const MpBlob& other )
{
	return !operator==(other);
}
