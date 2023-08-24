// Copyright 2006 Jeff McClintock



#include <map>

#include <string>

#include <memory.h>

#include "mp_sdk_gui.h" 



using namespace gmpi;



// Define the entry point for the DLL

#ifdef _MANAGED

#pragma managed(push, off)

#endif



/* Jeff for Waves

#ifndef COMPILE_HOST_SUPPORT



// temp. testing with MFC included.

#ifndef __AFXWIN_H__



// store dll instance handle, needed when creating windows and window classes.

HMODULE dllInstanceHandle;



extern "C"

__declspec (dllexport)

BOOL APIENTRY DllMain( HMODULE hModule,

					   DWORD  ul_reason_for_call,

					   LPVOID lpReserved )

{

	dllInstanceHandle = hModule;

	return TRUE;

}

#endif

#endif

*/



#ifdef _MANAGED

#pragma managed(pop)

#endif



//--------------GUID ---------------------

int MpGuidCompare( const MpGuid* a, const MpGuid* b )

{

	if (a->data1 != b->data1)

		return (a->data1 - b->data1);

	if (a->data2 != b->data2)

		return (a->data2 - b->data2);

	if (a->data3 != b->data3)

		return (a->data3 - b->data3);

	return memcmp(a->data4, b->data4, sizeof(a->data4));

}



int

MpGuidEqual( const gmpi::MpGuid* a, const gmpi::MpGuid* b )

{

	return ( MpGuidCompare(a, b) == 0 );

}



#if 0

//ndef COMPILE_HOST_SUPPORT

//---------------FACTORY --------------------



// MpFactory - a singleton object.  The plugin registers it's ID with the factory.



struct PluginInfo

{

	std::wstring id;

	MP_CreateFunc create_dsp;

	MP_CreateFunc create_gui;



	PluginInfo(const wchar_t* uniqueId) : id(uniqueId), create_dsp(0), create_gui(0){};

	void Register(int32_t subType, MP_CreateFunc create)

	{

		if( subType == MP_SUB_TYPE_AUDIO )

		{

			create_dsp = create;

		}

		else

		{

			assert( subType == MP_SUB_TYPE_GUI );

			create_gui = static_cast<MP_CreateFunc>(create);

		}

	};

};



typedef std::pair<std::wstring, PluginInfo*> MpPluginInfoPair;

typedef std::map<std::wstring, PluginInfo*>	MpPluginInfoMap;

typedef std::map<std::wstring, PluginInfo*>::iterator MpPluginInfoIterator;



class MpFactory: public IMpFactory

{

public:

	MpFactory(void);

	virtual ~MpFactory(void);



	/* IMpUnknown methods */

	virtual int32_t MP_STDCALL queryInterface( const MpGuid& iid, void** returnInterface );

	virtual int32_t MP_STDCALL addRef(void);

	virtual int32_t MP_STDCALL release(void);



	/* IMpFactory methods */

	virtual int32_t MP_STDCALL createInstance(

			const wchar_t* uniqueId,

			int32_t subType,

			IMpUnknown* host,

			IMpUnknown** returnInterface );



	virtual int32_t MP_STDCALL getSdkInformation( int32_t& returnSdkVersion, int32_t maxChars, wchar_t* returnCompilerInformation );



	// Registration

	int32_t RegisterPlugin( const wchar_t* uniqueId, int32_t subType, MP_CreateFunc create );



private:

	PluginInfo* FindOrCreatePluginInfo( const wchar_t* p_unique_id );



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



//  Factory lives untill plugin dll is unloaded.

//  this helper destroys the factory automatically.

class factory_deleter_helper

{

public:

	~factory_deleter_helper()

	{

		delete Factory();

	}

} grim_reaper;







// This is the DLL's main entry point.  It returns the factory.

extern "C"

__declspec (dllexport)

int32_t MP_STDCALL

MP_GetFactory( gmpi::IMpUnknown** returnInterface )

{

	// call queryInterface() to keep refcounting in sync

	return Factory()->queryInterface( MP_IID_UNKNOWN, returnInterface );

}



// register a DSP plugin with the factory

int32_t

RegisterPlugin( const wchar_t* uniqueId, MP_PluginCreateFunc create )

{

	return Factory()->RegisterPlugin(uniqueId, MP_SUB_TYPE_AUDIO, reinterpret_cast<MP_CreateFunc>(create));

}



// register a GUI plugin with the factory

int32_t

RegisterPlugin( const wchar_t* uniqueId, MP_GuiPluginCreateFunc create )

{

	return Factory()->RegisterPlugin( uniqueId, MP_SUB_TYPE_GUI, reinterpret_cast<MP_CreateFunc>(create) );

}



// Factory methods



int32_t MpFactory::RegisterPlugin( const wchar_t* uniqueId, int32_t subType, MP_CreateFunc create )

{

	Factory()->FindOrCreatePluginInfo(uniqueId)->Register(subType,create);

	return gmpi::MP_OK;

}



PluginInfo *MpFactory::FindOrCreatePluginInfo( const wchar_t* p_unique_id )

{

	MpPluginInfoIterator it = m_pluginMap.find( p_unique_id );

	

	if( it == m_pluginMap.end() )

	{

		PluginInfo * mi = new PluginInfo( p_unique_id );



		std::pair< MpPluginInfoIterator, bool > res = m_pluginMap.insert( MpPluginInfoPair( std::wstring( p_unique_id ), mi ));

		assert( res.second );

		return mi;

	}

	else

	{

		return (*it).second;

	}

}



int32_t

MpFactory::queryInterface( const MpGuid& iid, void** returnInterface )

{

//char debugMessage[200];

//sprintf(debugMessage, "queryInterface iid={%x,%x,%x}, returnInterface=%x  ",  iid.data1, iid.data2, iid.data3,    returnInterface); 

//MessageBoxA(0,debugMessage, "debug msg", MB_OK );



	if (iid == MP_IID_UNKNOWN || iid == MP_IID_FACTORY)

	{

		*returnInterface = this;

		addRef();

		return gmpi::MP_OK;

	}



	*returnInterface = 0;

	return MP_NOSUPPORT;

}



int32_t

MpFactory::addRef(void)

{

	// factory lives as long as dll is loaded.

	return sizeof(MpGuid); //1

}



int32_t

MpFactory::release(void)

{

	// factory lives as long as dll is loaded.

	return 1;

}



int32_t

MpFactory::createInstance( const wchar_t* uniqueId, int32_t subType,

		gmpi::IMpUnknown* host,

		gmpi::IMpUnknown** returnInterface )

{

	*returnInterface = 0; // if we fail for any reason, default return-val to NULL



	/* search m_pluginMap for the requested IID */

	MpPluginInfoMap::iterator it;

	it = m_pluginMap.find( uniqueId );

	if( it == m_pluginMap.end() )

	{

		return MP_NOSUPPORT;

	}



	MP_CreateFunc create = 0;



	if( subType == MP_SUB_TYPE_AUDIO )

	{

		create = (*it).second->create_dsp;

	}

	else

	{

		if( subType == MP_SUB_TYPE_GUI )

		{

			create = (*it).second->create_gui;

		}

	}



	if( create == 0 )

	{

		return MP_NOSUPPORT;

	}



	try

	{

		*returnInterface = create(host);

		(*returnInterface)->addRef();

	}



	// the new function will throw a std::bad_alloc exception if the memory allocation fails.

	// the constructor will throw a char* if host don't support required interfaces.

	catch(...)

	{

		return MP_FAIL;

	}



	return gmpi::MP_OK;

}



int32_t MpFactory::getSdkInformation( int32_t& returnSdkVersion, int32_t maxChars, wchar_t* returnCompilerInformation )

{

	returnSdkVersion = 30000; // 3.0



	// use safe string printf if possible.

	#if defined(_MSC_VER ) && _MSC_VER >= 1400

		#if defined( _DEBUG )

			_snwprintf_s( returnCompilerInformation, maxChars, _TRUNCATE, L"MS Compiler V%d (DEBUG)", (int) _MSC_VER );

		#else

			_snwprintf_s( returnCompilerInformation, maxChars, _TRUNCATE, L"MS Compiler V%d", (int) _MSC_VER );

		#endif

	#else

		#if defined( __GXX_ABI_VERSION )

			swprintf( returnCompilerInformation, L"GCC Compiler V%d", (int) __GXX_ABI_VERSION );

		#else

			wcscpy( returnCompilerInformation, L"Unknown Compiler" );

		#endif

	#endif



	return gmpi::MP_OK;

}



MpFactory::MpFactory(void)

{

}



MpFactory::~MpFactory(void)

{

	for( MpPluginInfoMap::iterator it = m_pluginMap.begin() ; it != m_pluginMap.end() ; ++it )

	{

		delete (*it).second;

	}

}

#endif	// COMPILE_HOST_SUPPORT



// BLOB datatype



MpBlob::MpBlob() :

	size_(0),

	data_(0)

{

}



void MpBlob::setValueRaw(size_t size, const void* data )
{
	if( size_ != size )
	{
		delete [] data_;
		size_ = size;
		data_ = new char[size_];
	}

	if( size_ > 0 )
	{
		memcpy( data_, data, size_ );
	}
}
/*


MpBlob::MpBlob( const MpBlob& other )

{

	size_ = other.size_;

	if( size_ > 0 )

	{

		data_ = new char[size_];

		memcpy( data_, other.data_, size_ );

	}

	else

	{

		data_ = 0;

	}

}



MpBlob::MpBlob( int size, const void* data )

{

	size_ = size;

	if( size_ > 0 )

	{

		data_ = new char[size_];

		memcpy( data_, data, size_ );

	}

	else

	{

		data_ = 0;

	}

}

*/


int32_t MpBlob::getSize() const

{

	return size_;

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



	for( int i = 0 ; i < size_ ; ++i )

	{

		if( data_[i] != other.data_[i] )

			return false;

	}

	return true;

}



bool MpBlob::compare( char* data, int size )

{

	if( size_ != size )

		return false;



	for( int i = 0 ; i < size_ ; ++i )

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

