#include "mp_sdk_gui.h"
#include <codecvt>
#include <locale>
#include "MpString.h"

using namespace gmpi;

const std::string& StringGuiPin::operator=(const std::string& valueUtf8)
{
#if defined(_WIN32)
	// std::wstring_convert does not handle emojis on Windows at least.
	std::wstring value;
	const size_t size = MultiByteToWideChar(
		CP_UTF8,
		0,
		valueUtf8.data(),
		static_cast<int>(valueUtf8.size()),
		0,
		0
	);

	value.resize(size);

	MultiByteToWideChar(
		CP_UTF8,
		0,
		valueUtf8.data(),
		static_cast<int>(valueUtf8.size()),
		const_cast<LPWSTR>(value.data()),
		static_cast<int>(size)
	);
#else
	static std::wstring_convert<std::codecvt_utf8<wchar_t> > convert;
	auto value = convert.from_bytes(valueUtf8);
#endif

	if (!variablesAreEqual<std::wstring>(value, value_))
	{
		value_ = value;
		sendPinUpdate();
	}
	return valueUtf8;
}

StringGuiPin::operator std::string()
{
	static std::wstring_convert<std::codecvt_utf8<wchar_t> > convert;
	return convert.to_bytes(getValue());
}

const std::wstring& StringGuiPin::operator=(const std::wstring& value)
{
	if (!variablesAreEqual<std::wstring>(value, value_))
	{
		value_ = value;
		sendPinUpdate();
	}
	return value;
}

#ifdef _WIN32

// copied from MP_GetDllHandle
HMODULE local_GetDllHandle()
{
	HMODULE hmodule = 0;
	GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)&local_GetDllHandle, &hmodule);
	return (HMODULE)hmodule;
}

#endif

/**********************************************************************************
MpGuiBase
**********************************************************************************/

MpGuiBase_base::MpGuiBase_base(IMpUnknown* /*unused*/) :
patchMemoryHost_(0)
{
	/*
	if( host ) // VST-GUI uses two stage construction, host passed later.
	{
		setHost(host);
//		host->query Interface(MP_IID_UI_HOST, reinterpret_cast<void **>(&patchMemoryHost_));

		if(patchMemoryHost_== 0)
		{
			throw "host Interfaces not supported";
		}
	}
	*/
}

int32_t MpGuiBase_base::setHost(gmpi::IMpUnknown* host)
{
	int32_t r = host->queryInterface(MP_IID_UI_HOST, reinterpret_cast<void**>( &patchMemoryHost_ ));

	if(patchMemoryHost_ == 0)
	{
		throw "host Interfaces not supported";
	}

	uiHost.Init(host);
	
	return r;
}

int32_t GuiPinOwner::setPin( int32_t pinId, int32_t voice, int32_t size, const void* data )
{
	auto it = pins_.find(pinId);
	if( it != pins_.end() )
	{
		(*it).second->set(voice, size, data); //todo consistant order size/data params everywhere
		return MP_OK;
	}

	return MP_FAIL;
}

int32_t GuiPinOwner::notifyPin( int32_t pinId, int32_t voice )
{
	auto it = pins_.find(pinId);
	if( it != pins_.end() )
	{
		(*it).second->doNotify( voice );
		return MP_OK;
	}

	return MP_FAIL;
}

// new 1-stage notification. FAULTY. deprecated.
int32_t GuiPinOwner::setPin2( int32_t pinId, int32_t voice, int32_t size, const void* data )
{
	auto it = pins_.find( pinId );
	if( it != pins_.end() )
	{
		if ((*it).second->set(voice, size, data)) //todo consistant order size/data params everywhere.
		{
			( *it ).second->doNotify( voice );
		}

		return MP_OK;
	}

	return MP_FAIL;
}

void GuiPinOwner::addPin( int pinId, MpGuiPinBase& pin )
{
#if defined(_DEBUG)
	auto res =
#endif

		pins_.insert({ pinId, &pin });

	#if defined(_DEBUG)
	assert( res.second && "initialize GUI pin failed - same ID used twice." );
//TODO!!		getHost()->verifyPinMatchesXml( id, 0 /*pin.GetDirection()*/, pin.getDatatype() );
	#endif
}

MpGuiPinBase* GuiPinOwner::getPin( int pinId )
{
	auto it = pins_.find( pinId );
	if( it != pins_.end() )
	{
		return (*it).second;
	}

	return 0;
}

int32_t MpGuiBase_base::onCreateContextMenu()
{
	return MP_OK;
}

int32_t MpGuiBase_base::onContextMenu( int32_t /*selection*/ )
{
	return MP_OK;
}


void MpGuiBase_base::initializePin( int pinId, MpGuiPinBase& pin, MpGuiBaseMemberPtr handler )
{
	pin.initialize( this, pinId, new IGuiPinCallback<gmpi::IMpUserInterface>( this, handler ) );
}

void MpGuiBase_base::initializePin( int pinId, MpGuiPinBase& pin, MpGuiBaseMemberIndexedPtr handler )
{
	pin.initialize( this, pinId, new IGuiPinCallbackIndexed<gmpi::IMpUserInterface>( this, handler ) );
}

void MpGuiBase_base::initializePin( int pinId, MpGuiPinBase& pin )
{
	pin.initialize( this, pinId );
}


/**********************************************************************************
MpGuiBase2
**********************************************************************************/

MpGuiBase2::MpGuiBase2() :
patchMemoryHost_(0)
#ifdef _DEBUG
	,debugInitializeCheck_(false)
#endif
{

}

MpGuiBase2::~MpGuiBase2()
{
#ifdef _DEBUG
	assert(debugInitializeCheck_); // Ensure you called base class ::initialize(); from your module's initialize() method.
#endif
}

int32_t MpGuiBase2::setHost( gmpi::IMpUnknown* host )
{
	host->queryInterface( MP_IID_UI_HOST2, reinterpret_cast<void **>( &patchMemoryHost_ ) );
	uiHost.Init(host);
	
	if( patchMemoryHost_ == 0 )
	{
		assert(false);
		return MP_NOSUPPORT; //  throw "host Interfaces not supported";
	}

	return MP_OK;
}

void MpGuiBase2::initializePin(int pinIndex, MpGuiPinBase& pin, MpGuiBaseMemberPtr2 handler)
{
	if( handler )
	{
		pin.initialize(this, pinIndex, new IGuiPinCallback<gmpi::IMpUserInterface2>(this, handler));
	}
	else
	{
		pin.initialize(this, pinIndex);
	}
}

void MpGuiBase2::initializePin(int pinIndex, MpGuiPinBase& pin, MpGuiBaseMemberIndexedPtr2 handler)
{
	pin.initialize(this, pinIndex, new IGuiPinCallbackIndexed<gmpi::IMpUserInterface2>(this, handler));
}

int32_t MpGuiBase2::initialize()
{
#ifdef _DEBUG
	assert(!debugInitializeCheck_);
	debugInitializeCheck_ = true;
#endif

	/* deprecated, faulty.
	// Fire an update on all input pins set to zero because they won't have fired when the host set them.
	// BUG !! does not differentiate input pins !!
	for (auto it = pins_.begin(); it != pins_.end(); ++it)
	{
		(*it).second->NotifyIfDefault((*it).first);
	}
	*/

	return gmpi::MP_OK;
}

std::string UiHost::RegisterResourceUri(const char* resourceName, const char* resourceType)
{
	if (!host2_)
		return {};

	gmpi_sdk::MpString fullUri;
	host2_->RegisterResourceUri(resourceName, resourceType, &fullUri);

	return fullUri.str();
}

//GmpiSdk::UriFile UiHost::openProtectedFile(const wchar_t* shortFilename)
//{
//	IProtectedFile* file = {};
//	host_->openProtectedFile(shortFilename, &file);
//	return GmpiSdk::UriFile(file);
//}

GmpiSdk::UriFile UiHost::OpenUri(std::string uri)
{
	if (!host2_)
		return {};

	gmpi_sdk::mp_shared_ptr<gmpi::IProtectedFile2> obj;
	host2_->OpenUri(uri.c_str(), &obj.get());

	return { obj };
}

std::string UiHost::FindResourceU(const char* resourceName, const char* resourceType)
{
	if (!host2_)
		return {};

	gmpi_sdk::MpString r;
	host2_->FindResourceU(resourceName, resourceType, &r);
	return r.str();
}

#if defined(_WIN32) && !defined(_WIN64)
/**********************************************************************************
SeGuiCompositedGfxBase
This uses the SynthEdit graphics API which supports transparancy.
**********************************************************************************/


SeGuiCompositedGfxBase::SeGuiCompositedGfxBase( gmpi::IMpUnknown* host ) : MpGuiBase( host )
{
}

int32_t SeGuiCompositedGfxBase::setHost( gmpi::IMpUnknown* host )
{
	MpGuiBase::setHost( host );

	int32_t r = host->queryInterface(MP_IID_GRAPHICS_HOST_SYNTHEDIT, reinterpret_cast<void **>( &guiHost_ ));

	if( guiHost_ == 0 )
	{
		throw "host Interfaces not supported";
	}
	return r;
}

int32_t SeGuiCompositedGfxBase::measure(MpSize availableSize, MpSize &returnDesiredSize)
{
	const int prefferedSize = 100;
	const int minSize = 15;

	returnDesiredSize.x = availableSize.x;
	returnDesiredSize.y = availableSize.y;
	if(returnDesiredSize.x > prefferedSize)
	{
		returnDesiredSize.x = prefferedSize;
	}
	else
	{
		if(returnDesiredSize.x < minSize)
		{
			returnDesiredSize.x = minSize;
		}
	}
	if(returnDesiredSize.y > prefferedSize)
	{
		returnDesiredSize.y = prefferedSize;
	}
	else
	{
		if(returnDesiredSize.y < minSize)
		{
			returnDesiredSize.y = minSize;
		}
	}

	return MP_OK;
}

int32_t SeGuiCompositedGfxBase::arrange(MpRect finalRect)
{
	rect_ = finalRect; return MP_OK;
}

void SeGuiCompositedGfxBase::setCapture()
{
	getGuiHost()->setCapture();
}

void SeGuiCompositedGfxBase::releaseCapture()
{
	getGuiHost()->releaseCapture();
}

bool SeGuiCompositedGfxBase::getCapture()
{
	int32_t ret;
	getGuiHost()->getCapture(ret);
	return ret != 0;
}

//void SeGuiCompositedGfxBase::invalidate()
void SeGuiCompositedGfxBase::invalidateRect( RECT* invalidRect, bool eraseBackground )
{
	getGuiHost()->invalidateRect( invalidRect );
}

int32_t SeGuiCompositedGfxBase::onLButtonDown(UINT flags, POINT point)
{
	return MP_OK;
}

int32_t SeGuiCompositedGfxBase::onMouseMove(UINT flags, POINT point)
{
	return MP_OK;
}

int32_t SeGuiCompositedGfxBase::onLButtonUp(UINT flags, POINT point)
{
	return MP_OK;
}

int32_t MP_STDCALL SeGuiCompositedGfxBase::openWindow( void )
{
	return MP_OK;
}

int32_t MP_STDCALL SeGuiCompositedGfxBase::closeWindow( void )
{
	return MP_OK;
}

/**********************************************************************************
MpGuiWindowsGfxBase
**********************************************************************************/

//extern HMODULE dllInstanceHandle;

// Windows message loop.
LRESULT CALLBACK MpWindowProc( HWND hwnd,
	UINT uMsg,
	WPARAM wParam,
	LPARAM lParam )
{
	// Recover the pointer to our class, don't forget to type cast it back.

#if defined(_MSC_VER)
	// Note: MS Compiler option /Wp64 - "Detect 64-bit Portability Issues" WRONGLY warns about the type cast.
	#pragma warning( push )
	#pragma warning(disable : 4312)
#endif

	MpGuiWindowsGfxBase* classptr = (MpGuiWindowsGfxBase*)GetWindowLongPtr( hwnd, GWLP_USERDATA );

#if defined(_MSC_VER)
	#pragma warning( pop )
#endif

	// Check if the pointer is NULL and call the Default WndProc.
	if( classptr == NULL )
	{
		return DefWindowProc( hwnd, uMsg, wParam, lParam );
	}
	else
	{
		// Call the Message Handler for my class (MsgProc in my case).
		return classptr->MsgProc( hwnd, uMsg, wParam, lParam );
	}
}

int MpGuiWindowsGfxBase::windowCount_ = 0;

MpGuiWindowsGfxBase::MpGuiWindowsGfxBase( IMpUnknown* unused ) : MpGuiBase( unused )
//,guiHost_(0)
,window_(0)
{
}

int32_t MpGuiWindowsGfxBase::setHost(gmpi::IMpUnknown* host)
{
	MpGuiBase::setHost( host );

	int32_t r = host->queryInterface(MP_IID_GRAPHICS_HOST_WIN_GDI, reinterpret_cast<void **>( &guiHost_ ));

	if( guiHost_ == 0 )
	{
		assert( false && "Wrong host interface supplied or 'graphicsApi' wrong in XML" );
		throw "host Interfaces not supported";
	}
	return r;
}

int32_t MpGuiWindowsGfxBase::PreCreateWindow( WNDCLASSEX& wcx )
{
	wcx.style = CS_HREDRAW | CS_VREDRAW;						// redraw if size changes
	wcx.lpfnWndProc = MpWindowProc;		// points to window procedure
 //   wcx.cbClsExtra = 0;				// no extra class memory
 //   wcx.cbWndExtra = 0;				// no extra window memory
	wcx.hInstance = local_GetDllHandle(); // dllInstanceHandle;	// handle to dll instance, very important to differentiate when same window class name used in 2 different dlls.
 //   wcx.hIcon = LoadIcon(NULL,
 //     IDI_APPLICATION);				// predefined app. icon
	wcx.hCursor = LoadCursor(NULL,
		IDC_ARROW);						// predefined arrow
//    wcx.hbrBackground = (HBRUSH) GetStockObject(
//        BLACK_BRUSH);					// white background brush
//    wcx.lpszMenuName =  "MainMenu";	// name of menu resource
	wcx.lpszClassName = L"DefaultMpWClass";  // name of window class
/*
	wcx.hIconSm = LoadImage(hinstance, // small class icon
		MAKEINTRESOURCE(5),
		IMAGE_ICON,
		GetSystemMetrics(SM_CXSMICON),
		GetSystemMetrics(SM_CYSMICON),
		LR_DEFAULTCOLOR);
*/
	return MP_OK;
}

int32_t MpGuiWindowsGfxBase::RegisterWindowClass()
{
	WNDCLASSEX wcx;
	memset( &wcx, 0 , sizeof(wcx) );
	wcx.cbSize = sizeof(wcx);

	PreCreateWindow(wcx);

	// Create Window class (describes type of window we're using.
	RegisterClassEx( &wcx );

	return MP_OK;
}


int32_t MpGuiWindowsGfxBase::UnRegisterWindowClass()
{
	UnregisterClass(L"DefaultMpWClass", local_GetDllHandle()); // dllInstanceHandle );

	return MP_OK;
}

int32_t MP_STDCALL MpGuiWindowsGfxBase::measure( MpSize availableSize, MpSize& returnDesiredSize )
{
	const int prefferedSize = 100;
	const int minSize = 15;

	returnDesiredSize.x = availableSize.x;
	returnDesiredSize.y = availableSize.y;
	if(returnDesiredSize.x > prefferedSize)
	{
		returnDesiredSize.x = prefferedSize;
	}
	else
	{
		if(returnDesiredSize.x < minSize)
		{
			returnDesiredSize.x = minSize;
		}
	}
	if(returnDesiredSize.y > prefferedSize)
	{
		returnDesiredSize.y = prefferedSize;
	}
	else
	{
		if(returnDesiredSize.y < minSize)
		{
			returnDesiredSize.y = minSize;
		}
	}

	return gmpi::MP_OK;
}

int32_t MP_STDCALL MpGuiWindowsGfxBase::arrange( MpRect finalRect )
{
	rect_ = finalRect;
	return gmpi::MP_OK;
}

void MpGuiWindowsGfxBase::getWindowStyles( DWORD& style, DWORD& extendedStyle )
{
	 style = WS_CHILD | WS_VISIBLE;
}

int32_t MP_STDCALL MpGuiWindowsGfxBase::openWindow( HWND parentWindow, HWND& returnWindowHandle )
{
	// register window class (if not already).
	if( windowCount_++ == 0 )
	{
		RegisterWindowClass();
	}

	returnWindowHandle = 0;

	MpRect r = getRect();

	DWORD style = 0;
	DWORD extendedStyle = 0;
	getWindowStyles( style, extendedStyle );

	returnWindowHandle = window_ = CreateWindowEx(
								extendedStyle,
							   L"DefaultMpWClass",
							   (LPCTSTR) NULL,
							   style,
							   r.left,r.top,r.right-r.left,r.bottom-r.top,
							   parentWindow,
							   (HMENU) 0,
								local_GetDllHandle(), //dllInstanceHandle,
							   NULL);

	// Adds a pointer to your current class to the WndClassEx structure.

#if defined(_MSC_VER)
	// Note: MS Compiler option /Wp64 - "Detect 64-bit Portability Issues" WRONGLY warns about the type cast.
	#pragma warning( push )
	#pragma warning(disable : 4244)
#endif

	SetWindowLongPtr( returnWindowHandle, GWLP_USERDATA, (LONG_PTR) this );

#if defined(_MSC_VER)
	#pragma warning( pop )
#endif

//?	ShowWindow( returnWindowHandle, SW_NORMAL );
//?	UpdateWindow( returnWindowHandle );

	return gmpi::MP_OK;
}

int32_t MP_STDCALL MpGuiWindowsGfxBase::closeWindow( void )
{
	if( window_ != 0 )
	{
		DestroyWindow( window_ );
		window_ = 0;

		// unregister window class when last window closes.
		if( --windowCount_ == 0 )
		{
			UnRegisterWindowClass();
		}
	}

	return gmpi::MP_OK;
}

int32_t MpGuiWindowsGfxBase::paint(HDC hDC)
{
	return gmpi::MP_OK;
}

int32_t MpGuiWindowsGfxBase::onLButtonDown( UINT flags, POINT point )
{
	return gmpi::MP_OK;
}

int32_t MpGuiWindowsGfxBase::onLButtonUp( UINT flags, POINT point )
{
	return gmpi::MP_OK;
}

int32_t MpGuiWindowsGfxBase::onMouseMove( UINT flags, POINT point )
{
	return gmpi::MP_OK;
}


LRESULT MpGuiWindowsGfxBase::MsgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
	switch (message)
	{
		case (WM_CLOSE):
			DestroyWindow(hwnd);
			return 1;
			break;
		case (WM_DESTROY):
			if (!GetParent(hwnd))
			{
				PostQuitMessage(0);
			}
			return 1;
			break;
		case (WM_PAINT):
			{
				PAINTSTRUCT ps;
				HDC hdc = BeginPaint( hwnd, &ps );
				paint( hdc );
				EndPaint( hwnd, &ps );
			}
			return 1;
			break;
		case (WM_LBUTTONDOWN):
			{
				POINT p;
				p.x = MAKEPOINTS(lParam).x;
				p.y = MAKEPOINTS(lParam).y;

				onLButtonDown( (UINT) wParam, p );
			}
			return 1;
			break;
		case (WM_LBUTTONUP):
			{
				POINT p;
				p.x = MAKEPOINTS(lParam).x;
				p.y = MAKEPOINTS(lParam).y;

				onLButtonUp( (UINT) wParam, p );
			}
			return 1;
			break;
		case (WM_MOUSEMOVE):
			{
				POINT p;
				p.x = MAKEPOINTS(lParam).x;
				p.y = MAKEPOINTS(lParam).y;

				onMouseMove( (UINT) wParam, p );
			}
			return 1;
			break;
		case (WM_TIMER):
			onTimer();
			return 1;
			break;
	}

	return DefWindowProc(hwnd, message, wParam, lParam);
}

int32_t MpGuiWindowsGfxBase::onTimer()
{
	return gmpi::MP_OK;
}

void MpGuiWindowsGfxBase::invalidateRect( RECT* invalidRect, bool eraseBackground )
{
	if( getWindowHandle() != 0 )
	{
			InvalidateRect(
				getWindowHandle(),	// handle to window
				invalidRect,		// rectangle coordinates
				eraseBackground		// erase state
		);
	}
}

void MpGuiWindowsGfxBase::setCapture()
{
	SetCapture( getWindowHandle() );
}

void MpGuiWindowsGfxBase::releaseCapture()
{
	ReleaseCapture();
}

bool MpGuiWindowsGfxBase::getCapture()
{
	return GetCapture() == getWindowHandle();
}

/**********************************************************************************
MpGuiWindowsGfxBase
This is a regular MS Windows window.
**********************************************************************************/

SeGuiWindowsGfxBase::SeGuiWindowsGfxBase( IMpUnknown* unused ) : MpGuiWindowsGfxBase( unused )
{
}

int32_t SeGuiWindowsGfxBase::setHost(gmpi::IMpUnknown* host)
{
	MpGuiWindowsGfxBase::setHost( host );

	int32_t r = host->queryInterface(SE_IID_GRAPHICS_HOST_WIN_GDI, reinterpret_cast<void**>( &seGuiHost_ ));

	if( seGuiHost_ == 0 )
	{
		assert( false && "Wrong host interface supplied" );
		throw "host Interfaces not supported";
	}
	return r;
}

#endif // Windows
