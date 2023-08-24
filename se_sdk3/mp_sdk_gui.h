// Copyright 2006 Jeff McClintock

#ifndef MP_SDK_GUI_H_INCLUDED
#define MP_SDK_GUI_H_INCLUDED

#include <map>
#include <assert.h>
#include "mp_sdk_common.h"

#define REGISTER_GUI_PLUGIN( className, pluginId ) namespace{ gmpi::IMpUnknown* PASTE_FUNC(create, className)(){ return static_cast<gmpi::IMpUserInterface*> (new className(0)); }; int32_t PASTE_FUNC(r, className) = RegisterPlugin( gmpi::MP_SUB_TYPE_GUI, pluginId, &PASTE_FUNC(create, className) );}

#define GMPI_REGISTER_GUI( family, className, pluginId ) namespace{ gmpi::IMpUnknown* PASTE_FUNC(create, className)(){ return static_cast<class MpGuiBase2*>(new className()); }; int32_t PASTE_FUNC(r, className) = RegisterPlugin( family, pluginId, &PASTE_FUNC(create, className) );}
//#define GMPI_REGISTER_CONTROLLER( className, pluginId ) namespace{ gmpi::IMpUnknown* PASTE_FUNC(create, className)(){ return static_cast<class gmpi::IMpController*>(new className()); }; int32_t PASTE_FUNC(r, className) = RegisterPlugin( gmpi::MP_SUB_TYPE_CONTROLLER, pluginId, &PASTE_FUNC(create, className) );}

/* best non-macro solution (allows use of templated classnames, which fail in macro due to commas in template defintion).
template< class moduleClass >
class Register
{
public:
static gmpi::IMpUnknown* Gui(const wchar_t* moduleIdentifier)
{
return RegisterPlugin(gmpi::MP_SUB_TYPE_GUI, moduleIdentifier,
[]() -> gmpi::IMpUnknown* { return static_cast<gmpi::IMpUserInterface*> (new moduleClass(nullptr)); }
}
};

namespace
{
auto r1 = Register< SimpleGuiConverter<int, bool> >::Gui(L"SE IntToBool GUI");
auto r2 = Register< BlobToText >::Gui(L"SE BlobToText GUI");
}

// This could be done with templated function returning pointer to moduleInfo (or surrogate).
auto r = Register(BlobToText)->withId(L"SE BlobToText GUI");
*/


class GuiPinOwner;
typedef	std::map<int, class MpGuiPinBase*> GuiPins_t;

class IGuiPinCallbackImpl
{
public:
	virtual ~IGuiPinCallbackImpl() {}
	virtual void operator()( int voice ) = 0;
};

template<class T>
class IGuiPinCallback : public IGuiPinCallbackImpl
{
	T* object_;
	void ( T::*member_ )( void );
	virtual void operator()( int ignored )
	{
		assert( ignored == 0 && "Not a polyphonic pin" );

		( ( object_ )->*member_ )( );
	}

public:
	IGuiPinCallback( T* object, void ( T::* member )( void ) ) : object_( object ), member_( member )
	{}
};

template<class T>
class IGuiPinCallbackIndexed : public IGuiPinCallbackImpl // IGuiPinCallbackIndexedImpl
{
	T* object_;
	void ( T::*member_ )( int );
	virtual void operator()( int voice = 0 )
	{
		( ( object_ )->*member_ )( voice );
	}

public:
	IGuiPinCallbackIndexed( T* object, void ( T::* member )( int ) ) : object_( object ), member_( member )
	{}
};

typedef void ( gmpi::IMpUserInterface::* MpGuiBaseMemberPtr )( void );
typedef void ( gmpi::IMpUserInterface::* MpGuiBaseMemberIndexedPtr )( int );

class GuiPinOwner
{
public:
	GuiPinOwner(){}
	virtual ~GuiPinOwner(){}
	void addPin( int pinId, MpGuiPinBase& pin );
	MpGuiPinBase* getPin( int pinId );

	int32_t setPin( int32_t pinId, int32_t voice, int32_t size, const void* data );
	int32_t setPin2( int32_t pinId, int32_t voice, int32_t size, const void* data );
	int32_t notifyPin( int32_t pinId, int32_t voice );
	virtual int32_t pinTransmit(int32_t pinId, int32_t size, const void* data, int32_t voice = 0) = 0;

protected:
	GuiPins_t pins_;
};

class UiHost
{
	gmpi::IMpUserInterfaceHost* host_ = {};
	gmpi::IMpUserInterfaceHost2* host2_ = {};
	
public:
	~UiHost()
	{
		if (host_)
			host_->release();
		
		if (host2_)
			host2_->release();
	}
	
	int32_t Init(gmpi::IMpUnknown* phost)
	{
		phost->queryInterface(gmpi::MP_IID_UI_HOST, (void**)&host_);
		return phost->queryInterface(gmpi::MP_IID_UI_HOST2, (void**)&host2_);
	}

#if 0
	// Plugin UI updates a parameter.
	virtual int32_t MP_STDCALL pinTransmit(int32_t pinId, int32_t size, /*const*/ void* data, int32_t voice = 0) = 0;

	// Backdoor to Audio class. Not recommended. Use Parameters instead to support proper automation.
	virtual int32_t MP_STDCALL sendMessageToAudio(int32_t id, int32_t size, /*const*/ void* messageData) = 0;

	// Start/Stop regular idle timer calls to plugin UI. Deprecated. Don't use.
	virtual int32_t MP_STDCALL setIdleTimer(int32_t active) = 0;

	// Each plugin instance has unique handle shared by UI and Audio class.
	virtual int32_t MP_STDCALL getHandle(int32_t& returnValue) = 0;

	// Identify Host. e.g. "SynthEdit"
	virtual int32_t MP_STDCALL getHostId(int32_t maxChars, wchar_t* returnString) = 0;

	// Query Host version number. e.g. returnValue of 400000 is Version 4.0
	virtual int32_t MP_STDCALL getHostVersion(int32_t& returnValue) = 0;

	// Get list of UI's pins.
	virtual int32_t MP_STDCALL createPinIterator(gmpi::IMpPinIterator** returnIterator) = 0;
	// Backward compatibility,
	inline int32_t createPinIterator(void** returnIterator)
	{
		return createPinIterator((gmpi::IMpPinIterator**)returnIterator);
	}
	
	// SynthEdit-specific.  Add item to context (right-click) menu in reponse to onCreateContextMenu().
	// index must be between 0 - 1000.
	virtual int32_t MP_STDCALL addContextMenuItem( /*const*/ wchar_t* menuText, int32_t index, int32_t flags = MI_NONE) = 0;

	// SynthEdit-specific.  Get count of UI's pins. Usefull with autoduplicate pins. UPDATE: Not suitable for autoduplicating pins becuase it's not avail in constructor when we need to init pins. open() is too late.
	virtual int32_t MP_STDCALL getPinCount(int32_t& returnCount) = 0;

#endif
	
	// SynthEdit-specific.  Determine file's location depending on host application's conventions. // e.g. "bell.wav" -> "C:/My Documents/bell.wav"
	// IMpUserInterfaceHost
	std::wstring resolveFilename(std::wstring filename)
	{
		wchar_t fullFilename[500] = L"";
		host_->resolveFilename(filename.c_str(), static_cast<int32_t>(std::size(fullFilename)), fullFilename);
		return fullFilename;
	}
	
	// SynthEdit-specific.  Determine file's location depending on host application's conventions. // e.g. "bell.wav" -> "C:/My Documents/bell.wav"
	// DEPRECATED: GmpiSdk::UriFile openProtectedFile(const wchar_t* shortFilename);
	
	// SynthEdit-specific.  Locate resources and make SynthEdit imbedd them during save-as-vst.
	// IMpUserInterfaceHost2
	void ClearResourceUris()
	{
		if (host2_)
			host2_->ClearResourceUris();
	}
	
	std::string RegisterResourceUri(const char* resourceName, const char* resourceType);
	GmpiSdk::UriFile OpenUri(std::string uri);

	// Locate resources without imbedding them during save-as-vst.
	std::string FindResourceU(const char* resourceName, const char* resourceType);
};

// Abstract base class for several derived classes.
class MpGuiBase_base :
	public gmpi::IMpUserInterface, public IoldSchoolInitialisation, public gmpi::IMpLegacyInitialization, public GuiPinOwner
{
public:
	MpGuiBase_base(IMpUnknown* host);
	virtual ~MpGuiBase_base() {} // need virtual destructor to support deletion via pointer-to-base.

	void initializePin( int pinId, MpGuiPinBase& pin, MpGuiBaseMemberPtr handler );
	void initializePin( int pinId, MpGuiPinBase& pin, MpGuiBaseMemberIndexedPtr handler );
	void initializePin( int pinId, MpGuiPinBase& pin );

	// Same but assume id's consecutive.
	void initializePin(MpGuiPinBase& pin, MpGuiBaseMemberPtr handler = 0)
	{
		int id;
		if( pins_.empty() )
		{
			id = 0;
		}
		else
		{
			id = pins_.rbegin()->first + 1;
		}
		initializePin(id, pin, handler);
	}

	// IMpUserInterface methods
	virtual int32_t initialize() override {return gmpi::MP_OK;}
	int32_t setPin( int32_t pinId, int32_t voice, int32_t size, void* data ) override
	{
		return GuiPinOwner::setPin( pinId, voice, size, data );
	}
	int32_t notifyPin( int32_t pinId, int32_t voice ) override
	{
		return GuiPinOwner::notifyPin( pinId, voice );
	}
	int32_t receiveMessageFromAudio( int32_t /*id*/, int32_t /*size*/, void* /*messageData*/ ) override { return gmpi::MP_OK; }
	int32_t onCreateContextMenu() override;
	int32_t onContextMenu( int32_t selection ) override;

	// IMpLegacyInitialization interface.
	virtual int32_t setHost(IMpUnknown* host) override;
	gmpi::IMpUserInterfaceHost* getHost(){ return patchMemoryHost_; }

	int32_t pinTransmit(int32_t pinId, int32_t size, const void* data, int32_t voice = 0) override
	{
		return getHost()->pinTransmit(pinId, size, (void*) data, voice);
	}

//	GMPI_QUERYINTERFACE1(gmpi::MP_IID_GUI_PLUGIN, gmpi::IMpUserInterface)
	GMPI_REFCOUNT;
	int32_t queryInterface( const gmpi::MpGuid& iid, void** returnInterface ) override
	{
		*returnInterface = 0;

		if( iid == gmpi::MP_IID_GUI_PLUGIN || iid == gmpi::MP_IID_UNKNOWN )
		{
			*returnInterface = static_cast<gmpi::IMpUserInterface*>(this);
			addRef();
			return gmpi::MP_OK;
		}

		if( iid == gmpi::MP_IID_LEGACY_INITIALIZATION )
		{
			*returnInterface = static_cast<gmpi::IMpLegacyInitialization*>(this);
			addRef();
			return gmpi::MP_OK;
		}

		return gmpi::MP_NOSUPPORT;
	}

protected:
	gmpi::IMpUserInterfaceHost* patchMemoryHost_;
	UiHost uiHost;
};

class MpGuiBase :
	public MpGuiBase_base
{
public:
	MpGuiBase(IMpUnknown* host) : MpGuiBase_base(host){}
	GMPI_REFCOUNT
};


typedef void ( gmpi::IMpUserInterface2::* MpGuiBaseMemberPtr2 )( void );
typedef void ( gmpi::IMpUserInterface2::* MpGuiBaseMemberIndexedPtr2 )( int );

class MpGuiBase2 :
	public gmpi::IMpUserInterface2B, public GuiPinOwner
{
	gmpi::IMpUserInterfaceHost2* patchMemoryHost_;

protected:
#ifdef _DEBUG
	bool debugInitializeCheck_;
#endif

public:
	MpGuiBase2();
	virtual ~MpGuiBase2(); // need virtual destructor to support deletion via pointer-to-base from release ().

	void initializePin( int pinIndex, MpGuiPinBase& pin, MpGuiBaseMemberPtr2 handler = 0 );
	void initializePin( int pinIndex, MpGuiPinBase& pin, MpGuiBaseMemberIndexedPtr2 handler );

	void initializePin(MpGuiPinBase& pin)
	{
		//	initializePin((int) pins_.size(), pin, handler);
		int id;
		if (pins_.empty())
		{
			id = 0;
		}
		else
		{
			id = pins_.rbegin()->first + 1;
		}
		initializePin(id, pin);
	}

	// Same but assume indexes consecutive.
	template<typename PointerToMember>
	void initializePin(MpGuiPinBase& pin, PointerToMember handler)
	{
		int id;
		if (pins_.empty())
		{
			id = 0;
		}
		else
		{
			id = pins_.rbegin()->first + 1;
		}
		initializePin(id, pin, static_cast<MpGuiBaseMemberPtr2>(handler)); // Arrays pins need initilize with pin index first.
	}

	// IMpUserInterface2 methods
	virtual int32_t setHost( gmpi::IMpUnknown* host ) override;
	virtual int32_t initialize() override;

	gmpi::IMpUserInterfaceHost2* getHost(){ return patchMemoryHost_; }

	// Pin updates come before initialize as each pin gets connected to whatever.
	// Should we suport 'quiet' updates via bool flag or seperate method?
	// mayby not, in a system that allows feedback, we can't guarentee all inputs will be set before initialise. It HAS to be ad-hoc. So getting premature notifications during setup don't matter much.
	int32_t setPin( int32_t pinId, int32_t voice, int32_t size, const void* data ) override
	{
//		return GuiPinOwner::setPin2(pinId, voice, size, data); // faulty.
		return GuiPinOwner::setPin(pinId, voice, size, data);
	}

	int32_t notifyPin(int32_t pinId, int32_t voice) override
	{
		return GuiPinOwner::notifyPin(pinId, voice);
	}

	int32_t receiveMessageFromAudio(int32_t /*id*/, int32_t /*size*/, const void* /*messageData*/) override
    { return gmpi::MP_UNHANDLED; }

	int32_t pinTransmit(int32_t pinId, int32_t size, const void* data, int32_t voice = 0) override
	{
		return getHost()->pinTransmit(pinId, size, data, voice);
	}

	// TODO pass actual interface.
	int32_t populateContextMenu(float /*x*/, float /*y*/, gmpi::IMpUnknown* /*contextMenuItemsSink*/) override
	{
		return gmpi::MP_UNHANDLED;
	}
	int32_t onContextMenu(int32_t /*selection*/) override
	{
		return gmpi::MP_UNHANDLED;
	}
	// TODO pass drawing POINT, pass actual returnstring interface.
	int32_t getToolTip(float /*x*/, float /*y*/, gmpi::IMpUnknown* /*returnToolTipString*/) override
	{
		return gmpi::MP_UNHANDLED;
	}

//	GMPI_QUERYINTERFACE1(gmpi::MP_IID_GUI_PLUGIN2, gmpi::IMpUserInterface2)
	int32_t queryInterface(const gmpi::MpGuid& iid, void** returnInterface) override
	{
		*returnInterface = 0;

		if (iid == gmpi::MP_IID_GUI_PLUGIN2B || iid == gmpi::MP_IID_GUI_PLUGIN2 || iid == gmpi::MP_IID_UNKNOWN)
		{
			*returnInterface = static_cast<gmpi::IMpUserInterface2B*>(this);
			addRef();
			return gmpi::MP_OK;
		}

		return gmpi::MP_NOSUPPORT;
	}
	
	UiHost uiHost;
};

class SeGuiInvisibleBase :
	public MpGuiBase2
{
	GMPI_REFCOUNT
};

#if defined(_WIN32) && defined(_INC_WINDOWS) && ( !defined(WINAPI_FAMILY) || WINAPI_FAMILY != WINAPI_FAMILY_APP )

// SynthEdit double-buffered drawing with transparency support.
// graphicsApi="composited"
class SeGuiCompositedGfxBase :
	public MpGuiBase, public ISeGraphicsComposited
{
public:
	SeGuiCompositedGfxBase(IMpUnknown* host);

	// IMpGraphicsSynthEdit methods
	virtual int32_t paint( HDC /*hDC*/ ) override{return gmpi::MP_OK;};
	virtual int32_t measure( MpSize availableSize, MpSize& returnDesiredSize ) override;
	virtual int32_t arrange( MpRect finalRect ) override;
	virtual int32_t onLButtonDown( UINT flags, POINT point ) override;
	virtual int32_t onMouseMove( UINT flags, POINT point ) override;
	virtual int32_t onLButtonUp( UINT flags, POINT point ) override;
	virtual int32_t openWindow( void ) override;
	virtual int32_t closeWindow( void ) override;

	void setCapture();
	void releaseCapture();
	bool getCapture();
	MpRect getRect(){return rect_;}
	void invalidateRect( RECT* invalidRect = 0, bool eraseBackground = true );
	ISeGraphicsHostComposited* getGuiHost(){return guiHost_;};

	// IMpLegacyInitialization interface.
	virtual int32_t setHost(IMpUnknown* host) override;

	GMPI_QUERYINTERFACE2(SE_IID_GRAPHICS_COMPOSITED, ISeGraphicsComposited, MpGuiBase)
	GMPI_REFCOUNT

private:
	ISeGraphicsHostComposited* guiHost_;
	MpRect rect_;
};

// graphicsApi="HWND"
class MpGuiWindowsGfxBase :
	public MpGuiBase, public IMpGraphicsWinGdi
{
public:
	MpGuiWindowsGfxBase( IMpUnknown* unused );
	virtual ~MpGuiWindowsGfxBase() {} // must be virtual so release () can delete cleanly.

	// IMpGraphicsWinGdi methods.
	virtual int32_t measure( MpSize availableSize, MpSize& returnDesiredSize ) override;
	virtual int32_t arrange( MpRect finalRect ) override;
	virtual int32_t openWindow( HWND parentWindow, HWND& returnWindowHandle ) override;
	virtual int32_t closeWindow( void ) override;

	// This handles the Windows message loop.
	virtual LRESULT MsgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam );

	virtual int32_t PreCreateWindow( WNDCLASSEX& wcx );
	virtual void getWindowStyles( DWORD& style, DWORD& extendedStyle );
	virtual int32_t RegisterWindowClass();
	virtual int32_t UnRegisterWindowClass();

	// Handle common windows messages. Uses only to ease switching user code to/from SeGuiCompositedGfxBase.
	virtual int32_t paint(HDC hDC);
	virtual int32_t onLButtonDown( UINT flags, POINT point );
	virtual int32_t onLButtonUp( UINT flags, POINT point );
	virtual int32_t onMouseMove( UINT flags, POINT point );
	virtual int32_t onTimer();

	void invalidateRect( RECT* invalidRect = 0, bool eraseBackground = true );
	bool getCapture();
	void setCapture();
	void releaseCapture();
	HWND getWindowHandle(){return window_;};
	MpRect getRect(){return rect_;};
	IMpGraphicsWinGdi* getGuiHost(){return guiHost_;};

	// IMpLegacyInitialization interface.
	virtual int32_t setHost(IMpUnknown* host) override;

	GMPI_QUERYINTERFACE2(MP_IID_GRAPHICS_WIN_GDI, IMpGraphicsWinGdi, MpGuiBase)
	GMPI_REFCOUNT

protected:
	HWND window_;

private:
	IMpGraphicsWinGdi* guiHost_;
	MpRect rect_;
	static int windowCount_;
};

// Extended for SynthEdit
class SeGuiWindowsGfxBase :
	public MpGuiWindowsGfxBase
{
public:
	SeGuiWindowsGfxBase( IMpUnknown* unused );

	ISeGraphicsHostWinGdi* getGuiHost(){return seGuiHost_;};

	// IMpLegacyInitialization interface.
	virtual int32_t setHost(gmpi::IMpUnknown* host) override;

//	GMPI_QUERYINTERFACE2(MP_IID_GRAPHICS_WIN_GDI, IMpGraphicsWinGdi, MpGuiWindowsGfxBase)
	GMPI_REFCOUNT

private:
	ISeGraphicsHostWinGdi* seGuiHost_;
};

#endif // _WIN32

class MpGuiPinBase
{
public:
	MpGuiPinBase() :
		id_( -1 ),
		plugin_( 0 ),
		eventHandler2_(0)
	{
	}
	virtual ~MpGuiPinBase()
	{
		delete eventHandler2_;
	} // ensure virtual destructor.

	void initialize( GuiPinOwner* plugin, int p_id, IGuiPinCallbackImpl* handler = 0 )
	{
		id_ = p_id;
		plugin_ = plugin;

		eventHandler2_ = handler;
		plugin->addPin( p_id, *this );
	}

	// return true if value_ changed
	virtual bool set( int32_t voice, int size, const void* data ) = 0;
	void doNotify( int32_t voice )
	{
		if( eventHandler2_ )
			( *eventHandler2_ )( voice );
	}

	int getId( void )
	{
		return id_;
	}
	virtual int getDatatype() const = 0;

protected:
	int id_;
	class GuiPinOwner* plugin_;
	IGuiPinCallbackImpl* eventHandler2_;
};

// TODO: Need a way to set a large BLOB without unnesc copying, like supporting blob = std::move(blob(ptr, size));

template
<typename T> class MpGuiPin : public MpGuiPinBase
{
	friend class GuiPinOwner;

public:
	MpGuiPin()
	{
	}

	void initialize( MpGuiBase* plugin, int p_id )
	{
		MpGuiPinBase::initialize( static_cast<GuiPinOwner*>( plugin ), p_id );
	}

	void initialize( MpGuiBase* plugin, int p_id, MpGuiBaseMemberPtr handler )
	{
		MpGuiPinBase::initialize( static_cast<GuiPinOwner*>( plugin ), p_id, new IGuiPinCallback<gmpi::IMpUserInterface>( static_cast<gmpi::IMpUserInterface*>( plugin ), handler ) );
	}

	void initialize( MpGuiBase* plugin, int pinId, MpGuiBaseMemberIndexedPtr handler )
	{
		assert( false && "pin not polyphonic. wrong type of handler" );
	}
	void sendPinUpdate()
	{
		assert( plugin_ != 0 && "Don't forget initializePin() on each pin in your constructor.");
		plugin_->pinTransmit(getId(), rawSize(), rawData());
	}
	const T& getValue() const
	{
		assert( plugin_ != 0 &&"Don't forget initializePin() on each pin in your constructor.");
		return value_;
	}
	operator T()
	{
		assert( plugin_ != 0 &&"Don't forget initializePin() on each pin in your constructor.");
		return value_;
	}
	bool operator==(T other)
	{
		return other == value_;
	}
	// todo: specialise for value_ vs ref types !!!
	const T& operator=(MpGuiPin<T> const & other)
	{
		return operator=(other.getValue());
	}
	// Send new value out of module.
	const T& operator=(const T& value)
	{
		if( ! variablesAreEqual<T>(value, value_) )
		{
			value_ = value;
			sendPinUpdate();
		}
		return value_;
	}

	virtual int rawSize()
	{
		return variableRawSize<T>(value_);
	}
	virtual void* rawData()
	{
		return variableRawData<T>(value_);
	}
	virtual int getDatatype() const
	{
		return MpTypeTraits<T>::PinDataType;
	}
	virtual MpGuiBaseMemberPtr getDefaultEventHandler()
	{
		return 0;
	}

	// Only applies to Blobs and strings.
	void resize( int size )
	{
		value_.resize( size );
	}

protected:
	// These members intended only for internal use by SDK.
	// accept new value into module. Return true if value changed.
	virtual bool set( int32_t voice, int size, const void* data )
	{
		assert(voice == 0 && "Not a polyphonic pin");

		// new. Don't create temporary variable (wasteful for large blobs).
		if( size == variableRawSize(value_) )
		{
			if( memcmp( variableRawData(value_), data, static_cast<size_t>(size)) == 0 )
			{
				return false;
			}
		}

		VariableFromRaw<T>( size, data, value_ );

		return true;
	}

	T value_ = {};
};


typedef MpGuiPin<int> IntGuiPin;
typedef MpGuiPin<bool> BoolGuiPin;
typedef MpGuiPin<float> FloatGuiPin;
typedef MpGuiPin<MpBlob> BlobGuiPin;

class StringGuiPin : public MpGuiPin<std::wstring>
{
public:
	// UTF8 compatibility..
	// Send new value out of module (UTF8 version).
	const std::string& operator=(const std::string& valueUtf8);
	operator std::string();
	const std::wstring& operator=(const std::wstring& value);
};

// Pin with array of values, for polyphonic parameters.
// Array-style access.
template
<typename T, int ARRAY_SIZE> class MpGuiArrayPin : public MpGuiPinBase
{
public:
	MpGuiArrayPin() {}

	void initialize( MpGuiBase* plugin, int p_id )
	{
		MpGuiPinBase::initialize( static_cast<GuiPinOwner*>( plugin ), p_id );
	}

	void initialize( MpGuiBase* plugin, int pinId, MpGuiBaseMemberPtr handler )
	{
		assert( false && "wrong type of handler" );
	}

	void initialize( MpGuiBase* plugin, int p_id, MpGuiBaseMemberIndexedPtr handler )
	{
		MpGuiPinBase::initialize(plugin, p_id, new IGuiPinCallbackIndexed<gmpi::IMpUserInterface>(plugin, handler));
	}

	void sendPinUpdate( int index )
	{
		assert( plugin_ != 0 &&"Don't forget initializePin() on each pin in your constructor.");
//		plugin_->getHost()->pinTransmit(getId(), rawSize(index), rawData(index), index);
		plugin_->pinTransmit(getId(), rawSize(index), rawData(index), index);
	}
	T getValue(int index) const
	{
		assert( plugin_ != 0 &&"Don't forget initializePin() on each pin in your constructor.");
		return value_[index];
	}
	void setValue(int index, const T& value)
	{
		if( ! variablesAreEqual<T>(value, value_[index]) )
		{
			value_[index] = value;
			sendPinUpdate(index);
		}
	}

	int size() const
	{
		return ARRAY_SIZE;
	}

	// Array-style access.
	class Helper
	{
	public:
		Helper(MpGuiArrayPin<T, ARRAY_SIZE> *pArray, int _index)
		: m_self(pArray), m_index(_index)
		{}

		void operator = (T value)
		{
			m_self->setValue(m_index, value);
		}

		operator T () const
		{
			return m_self->getValue(m_index);
		}

	private:
		int m_index;
		MpGuiArrayPin<T, ARRAY_SIZE>* m_self;
	};

	// Overloaded subscript operator for non-const Arrays
    Helper operator[]( int _index )
    {
        return Helper(this, _index);
    }
/* NA
    // Overloaded subscript operator for const Arrays
    const int &Array::operator[]( int _index ) const
    {
        // check for subscript out of range error
        assert( 0 <= _index && _index < static_cast<int>(m_sizeArray) );
        return m_ptrArray[ _index ]; // reference return
    }
*/

/*
	operator T()
	{
		return value_;
	};
	bool operator==(T other)
	{
		return other == value_;
	};
	// todo: specialise for value_ vs ref types !!!
	const T &operator=(MpGuiPin<T>& other)
	{
		return operator=(other.getValue());
	};
	const T& operator=(const T& value)
	{
		if( ! variablesAreEqual<T>(value, value_) )
		{
			value_ = value;
			sendPinUpdate();
		}
		return value;
	}
*/
	virtual bool set( int32_t voice, int size, const void* data )
	{
		return setValueRaw(voice, size, data);
	}
	/*
	virtual void doNotify( int32_t voice )
	{
		if( eventHandler_ != 0 )
		{
			// (plugin_->*eventHandler_)( voice );
			( ( plugin_->getCallbackObject() )->*eventHandler_ )( voice );
		}
	}
	*/

	virtual int getDatatype() const
	{
		return MpTypeTraits<T>::PinDataType;
	}
//	virtual int getDirection() const{return DIRECTION;};
	virtual MpGuiBaseMemberPtr getDefaultEventHandler()
	{
		return 0;
	}
protected:
	T value_[ARRAY_SIZE] = {};

public:
	virtual bool setValueRaw(int index, int size, const void* data)
	{
		// new: Don't create temporary variable (wasteful for large blobs).
		if( size == variableRawSize(value_[index]) )
		{
			if( memcmp( variableRawData(value_[index]), data, size ) == 0 )
			{
				return false;
			}
		}

		VariableFromRaw<T>( size, data, value_[index] );

		return true;
	}
	virtual int rawSize(int index)
	{
		return variableRawSize<T>(value_[index]);
	}
	virtual void* rawData(int index)
	{
		return variableRawData<T>(value_[index]);
	}
};

#define MP_VOICE_COUNT 128
typedef MpGuiArrayPin<float, MP_VOICE_COUNT> FloatArrayGuiPin;
typedef MpGuiArrayPin<int, MP_VOICE_COUNT> IntArrayGuiPin;
typedef MpGuiArrayPin<MpBlob, MP_VOICE_COUNT> BlobArrayGuiPin;
typedef MpGuiArrayPin<std::wstring, MP_VOICE_COUNT> StringArrayGuiPin;

#endif // .H INCLUDED
