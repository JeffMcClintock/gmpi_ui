#pragma once

/*
#include "GmpiUI.h"
*/

#include "../../../se_sdk3_hosting/GraphicsRedrawClient.h"
#include "../../../se_sdk3/TimerManager.h"
#include "../../../RefCountMacros.h"
#include "../../../Drawing.h"

#ifdef _WIN32
// Add the path to the gmpi_ui library in the Projucer setting 'Header Search Paths'.
#include "backends/DrawingFrame_win32.h"

class JuceDrawingFrameBase : public GmpiGuiHosting::DrawingFrameBase
{
	juce::HWNDComponent& juceComponent;

protected:

public:
	JuceDrawingFrameBase(juce::HWNDComponent& pJuceComponent) : juceComponent(pJuceComponent)
	{
	}

    void open(void* pParentWnd, int width, int height);

	HWND getWindowHandle() override
	{
		return (HWND)juceComponent.getHWND();
	}

	LRESULT WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
};


class GmpiViewComponent : public juce::HWNDComponent, IMpDrawingClient
{
    JuceDrawingFrameBase JuceDrawingFrameBase;
    GmpiDrawing::Point cubaseBugPreviousMouseMove = { -1,-1 };

protected:
    void parentHierarchyChanged() override;

public:
    GmpiViewComponent() : JuceDrawingFrameBase(*this)
	{
	}

	// override this in your derived class
	virtual void OnRender(GmpiDrawing::Graphics& g) {}


    // IMpDrawingClient interface
	int32_t open(gmpi::IMpUnknown* host) override { return gmpi::MP_OK; }

	// First pass of layout update. Return minimum size required for given available size
	int32_t MP_STDCALL measure(const GmpiDrawing_API::MP1_SIZE* availableSize, GmpiDrawing_API::MP1_SIZE* returnDesiredSize) override { return gmpi::MP_OK; }

	// Second pass of layout.
	int32_t MP_STDCALL arrange(const GmpiDrawing_API::MP1_RECT* finalRect) override { return gmpi::MP_OK; }

	int32_t OnRender(GmpiDrawing_API::IMpDeviceContext* drawingContext) override
	{
		GmpiDrawing::Graphics g(drawingContext);
		OnRender(g);

		return gmpi::MP_OK;
	}

    void invalidateRect()
    {
		JuceDrawingFrameBase.invalidateRect(nullptr);
    }

//	void repaint()
//	{
//		invalidateRect();
//	}

	// TODO GMPI_QUERYINTERFACE(IMpDrawingClient::guid, IMpDrawingClient);
	int32_t MP_STDCALL queryInterface(const gmpi::MpGuid& iid, void** returnInterface) override
	{
		*returnInterface = nullptr;

		if (iid == IMpDrawingClient::guid || iid == gmpi::MP_IID_UNKNOWN)
		{
			*returnInterface = static_cast<IMpDrawingClient*>(this);
			addRef();
			return gmpi::MP_OK;
		}

#ifdef GMPI_HOST_POINTER_SUPPORT
		if (iid == gmpi_gui_api::SE_IID_GRAPHICS_MPGUI3)
		{
			*returnInterface = static_cast<IMpGraphics3*>(this);
			addRef();
			return gmpi::MP_OK;
		}

		if (iid == gmpi_gui_api::SE_IID_GRAPHICS_MPGUI2)
		{
			*returnInterface = static_cast<IMpGraphics2*>(this);
			addRef();
			return gmpi::MP_OK;
		}

		if (iid == gmpi_gui_api::SE_IID_GRAPHICS_MPGUI)
		{
			*returnInterface = static_cast<IMpGraphics*>(this);
			addRef();
			return gmpi::MP_OK;
		}
#endif

		return gmpi::MP_NOSUPPORT;
	}
	GMPI_REFCOUNT_NO_DELETE;
};
#else

class JuceComponentProxy : public IMpDrawingClient
{
    class GmpiViewComponent* component = {};
    IDrawingHost* drawinghost = {};
public:
    
    JuceComponentProxy(class GmpiViewComponent* pcomponent) : component(pcomponent){}
    
    int32_t open(gmpi::IMpUnknown* host) override
    {
        return host->queryInterface(IDrawingHost::guid, (void**)&drawinghost);
    }

    void invalidateRect()
    {
        drawinghost->invalidateRect(nullptr);
    }
    
    // First pass of layout update. Return minimum size required for given available size
    int32_t MP_STDCALL measure(const GmpiDrawing_API::MP1_SIZE* availableSize, GmpiDrawing_API::MP1_SIZE* returnDesiredSize) override { return gmpi::MP_OK; }

    // Second pass of layout.
    int32_t MP_STDCALL arrange(const GmpiDrawing_API::MP1_RECT* finalRect) override { return gmpi::MP_OK; }

    int32_t OnRender(GmpiDrawing_API::IMpDeviceContext* drawingContext) override;
    
    // TODO GMPI_QUERYINTERFACE(IMpDrawingClient::guid, IMpDrawingClient);
    int32_t MP_STDCALL queryInterface(const gmpi::MpGuid& iid, void** returnInterface) override
    {
        *returnInterface = nullptr;

        if (iid == IMpDrawingClient::guid || iid == gmpi::MP_IID_UNKNOWN)
        {
            *returnInterface = static_cast<IMpDrawingClient*>(this);
            addRef();
            return gmpi::MP_OK;
        }

#ifdef GMPI_HOST_POINTER_SUPPORT
        if (iid == gmpi_gui_api::SE_IID_GRAPHICS_MPGUI3)
        {
            *returnInterface = static_cast<IMpGraphics3*>(this);
            addRef();
            return gmpi::MP_OK;
        }

        if (iid == gmpi_gui_api::SE_IID_GRAPHICS_MPGUI2)
        {
            *returnInterface = static_cast<IMpGraphics2*>(this);
            addRef();
            return gmpi::MP_OK;
        }

        if (iid == gmpi_gui_api::SE_IID_GRAPHICS_MPGUI)
        {
            *returnInterface = static_cast<IMpGraphics*>(this);
            addRef();
            return gmpi::MP_OK;
        }
#endif

        return gmpi::MP_NOSUPPORT;
    }
    GMPI_REFCOUNT_NO_DELETE;
};

class GmpiViewComponent : public juce::NSViewComponent
{
    JuceComponentProxy proxy;
    
public:
    GmpiViewComponent() : proxy(this){}
    ~GmpiViewComponent();
    void open(gmpi::IMpUnknown* client, int width, int height);
    void parentHierarchyChanged() override
    {
        if(!getView())
        {
            const auto r = getLocalBounds();
            open(&proxy, r.getWidth(), r.getHeight());
        }
    }
    
    void invalidateRect()
    {
        proxy.invalidateRect();
    }

    // override this in your derived class
    virtual void OnRender(GmpiDrawing::Graphics& g) {}
};

inline int32_t JuceComponentProxy::OnRender(GmpiDrawing_API::IMpDeviceContext* drawingContext)
{
    GmpiDrawing::Graphics g(drawingContext);
    component->OnRender(g);
    return gmpi::MP_OK;
}

#endif

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnon-virtual-dtor"
#endif

class GmpiCanvas :
    /*public gmpi_gui_api::IMpGraphics3,*/
    //public TimerClient,
    public /*gmpi_gui_api::*/IMpDrawingClient
{
    //    gmpi_gui::IMpGraphicsHost* drawinghost = {};
    IDrawingHost* drawinghost = {};
	IMpDrawingClient* juceComponent = {};

public:

    GmpiCanvas(IMpDrawingClient* pJuceComponent) :
		juceComponent(pJuceComponent)
    {
//        StartTimer(1000 / 60);
    }

    ~GmpiCanvas()
    {
//       StopTimer();
    }

    // IMpDrawingClient
    int32_t open(gmpi::IMpUnknown* host)  override
    {
#ifdef GMPI_HOST_POINTER_SUPPORT
        // hack, should use queryinterface
       // drawinghost = dynamic_cast<gmpi_gui::IMpGraphicsHost*>(host);

        return host->queryInterface(gmpi_gui::IMpGraphicsHost::IID(), (void**)&drawinghost);
#else
        return host->queryInterface(IDrawingHost::guid, (void**)&drawinghost);
        //        return gmpi::MP_OK;
#endif
    }

    void invalidateRect(const GmpiDrawing_API::MP1_RECT* invalidRect = {})
    {
        if (drawinghost)
        {
            drawinghost->invalidateRect(invalidRect);
        }
    }

    int32_t MP_STDCALL measure(const GmpiDrawing_API::MP1_SIZE* availableSize, GmpiDrawing_API::MP1_SIZE* returnDesiredSize) override { return gmpi::MP_OK; }
    int32_t MP_STDCALL arrange(const GmpiDrawing_API::MP1_RECT* finalRect) override { return gmpi::MP_OK; }

#ifdef GMPI_HOST_POINTER_SUPPORT
    // IMpGraphics
    int32_t MP_STDCALL measure(GmpiDrawing_API::MP1_SIZE availableSize, GmpiDrawing_API::MP1_SIZE* returnDesiredSize) override { return gmpi::MP_OK; }
    int32_t MP_STDCALL arrange(GmpiDrawing_API::MP1_RECT finalRect) override { return gmpi::MP_OK; } // TODO const, and reference maybe?
#endif
    int32_t MP_STDCALL OnRender(GmpiDrawing_API::IMpDeviceContext* drawingContext) override;
#ifdef GMPI_HOST_POINTER_SUPPORT
    int32_t MP_STDCALL onPointerDown(int32_t flags, GmpiDrawing_API::MP1_POINT point) override { return gmpi::MP_OK; }
    int32_t MP_STDCALL onPointerMove(int32_t flags, GmpiDrawing_API::MP1_POINT point) override { return gmpi::MP_OK; }
    int32_t MP_STDCALL onPointerUp(int32_t flags, GmpiDrawing_API::MP1_POINT point) override { return gmpi::MP_OK; }    // IMpGraphics2
    int32_t MP_STDCALL hitTest(GmpiDrawing_API::MP1_POINT point) override { return gmpi::MP_OK; } // TODO!!! include mouse flags (for Patch Cables)
    int32_t MP_STDCALL getToolTip(GmpiDrawing_API::MP1_POINT point, gmpi::IString* returnString) override { return gmpi::MP_OK; }

    // IMpGraphics3
    int32_t MP_STDCALL hitTest2(int32_t flags, GmpiDrawing_API::MP1_POINT point) override { return gmpi::MP_OK; }
    int32_t MP_STDCALL onMouseWheel(int32_t flags, int32_t delta, GmpiDrawing_API::MP1_POINT point) override { return gmpi::MP_OK; }
    int32_t MP_STDCALL setHover(bool isMouseOverMe) override { return gmpi::MP_OK; }
#endif

    // IMpUnknown
    int32_t MP_STDCALL queryInterface(const gmpi::MpGuid& iid, void** returnInterface) override
    {
        *returnInterface = nullptr;

        if (iid == IMpDrawingClient::guid || iid == gmpi::MP_IID_UNKNOWN)
        {
            *returnInterface = static_cast<IMpDrawingClient*>(this);
            addRef();
            return gmpi::MP_OK;
        }

#ifdef GMPI_HOST_POINTER_SUPPORT
        if (iid == gmpi_gui_api::SE_IID_GRAPHICS_MPGUI3)
        {
            *returnInterface = static_cast<IMpGraphics3*>(this);
            addRef();
            return gmpi::MP_OK;
        }

        if (iid == gmpi_gui_api::SE_IID_GRAPHICS_MPGUI2)
        {
            *returnInterface = static_cast<IMpGraphics2*>(this);
            addRef();
            return gmpi::MP_OK;
        }

        if (iid == gmpi_gui_api::SE_IID_GRAPHICS_MPGUI)
        {
            *returnInterface = static_cast<IMpGraphics*>(this);
            addRef();
            return gmpi::MP_OK;
        }
#endif

        return gmpi::MP_NOSUPPORT;
    }
    GMPI_REFCOUNT;
};

#ifdef _WIN32
inline void GmpiViewComponent::parentHierarchyChanged()
{
#ifdef _WIN32
    if (auto hwnd = (HWND)getWindowHandle(); hwnd && !getHWND())
    {
        const auto r = getBounds();

        HDC hdc = ::GetDC(hwnd);
        const int dpi = GetDeviceCaps(hdc, LOGPIXELSX);
        ::ReleaseDC(hwnd, hdc);

        //		auto presenter = new JsonDocPresenter(dynamic_cast<IGuiHost2*>(&controller));

        {
            //auto cv =
            //	new SynthEdit2::ContainerView(
            //		GmpiDrawing::Size(static_cast<float>(drawingframe.viewDimensions), static_cast<float>(drawingframe.viewDimensions))
            //	);

//               if (!client)
            {
                auto client = new GmpiCanvas(this);
                //	client->drawinghost = &drawingframe;
                JuceDrawingFrameBase.AddView(static_cast</*gmpi_gui_api::*/IMpDrawingClient*>(client));// cv);

                client->release();
            }

            //			cv->setDocument(presenter, CF_PANEL_VIEW);
        }

        //		presenter->RefreshView();

		JuceDrawingFrameBase.open(
            hwnd,
            (r.getWidth() * dpi) / 96,
            (r.getHeight() * dpi) / 96
        );

        //            StartTimer(15);
    }
#else
    if (!drawingframe.getView())
    {
        client = new GmpiCanvas();
        //	client->drawinghost = &drawingframe;
        const auto r = getBounds();
        open(client, r.getWidth(), r.getHeight());
        client->release();
    }
#endif
}
#endif

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

inline int32_t GmpiCanvas::OnRender(GmpiDrawing_API::IMpDeviceContext* drawingContext)
{
	// defer to client
	return juceComponent->OnRender(drawingContext);
}

// move this crap
#ifdef _WIN32
#include <Windowsx.h>
#else

// without including objective-C headers, we need to create an NSView.
// forward declare function here to return the view, using void* as return type.
void* createNativeView(class IMpUnknown* client, int width, int height);
void onCloseNativeView(void* ptr);

#endif

#ifdef _WIN32

////////////////////////////////////////////////////////////////////////////
// 
// 
// 
inline LRESULT JuceDrawingFrameBase::WindowProc(
	HWND hwnd,
	UINT message,
	WPARAM wParam,
	LPARAM lParam)
{
	switch (message)
	{
#ifdef GMPI_HOST_POINTER_SUPPORT
	case WM_MBUTTONDOWN:
	case WM_MBUTTONUP:
	case WM_MOUSEMOVE:
	case WM_LBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_RBUTTONDOWN:
	case WM_RBUTTONUP:
	{
		GmpiDrawing::Point p(static_cast<float>(GET_X_LPARAM(lParam)), static_cast<float>(GET_Y_LPARAM(lParam)));
		p = WindowToDips.TransformPoint(p);

		// Cubase sends spurious mouse move messages when transport running.
		// This prevents tooltips working.
		if (message == WM_MOUSEMOVE)
		{
			if (cubaseBugPreviousMouseMove == p)
			{
				return TRUE;
			}
			cubaseBugPreviousMouseMove = p;
		}
		else
		{
			cubaseBugPreviousMouseMove = GmpiDrawing::Point(-1, -1);
		}

		TooltipOnMouseActivity();

		int32_t eventFlags = gmpi_gui_api::GG_POINTER_FLAG_INCONTACT | gmpi_gui_api::GG_POINTER_FLAG_PRIMARY | gmpi_gui_api::GG_POINTER_FLAG_CONFIDENCE;

		switch (message)
		{
		case WM_MBUTTONDOWN:
		case WM_LBUTTONDOWN:
		case WM_RBUTTONDOWN:
			eventFlags |= gmpi_gui_api::GG_POINTER_FLAG_NEW;
			break;
		}

		switch (message)
		{
		case WM_LBUTTONUP:
		case WM_LBUTTONDOWN:
			eventFlags |= gmpi_gui_api::GG_POINTER_FLAG_FIRSTBUTTON;
			break;
		case WM_RBUTTONDOWN:
		case WM_RBUTTONUP:
			eventFlags |= gmpi_gui_api::GG_POINTER_FLAG_SECONDBUTTON;
			break;
		case WM_MBUTTONDOWN:
		case WM_MBUTTONUP:
			eventFlags |= gmpi_gui_api::GG_POINTER_FLAG_THIRDBUTTON;
			break;
		}

		if (GetKeyState(VK_SHIFT) < 0)
		{
			eventFlags |= gmpi_gui_api::GG_POINTER_KEY_SHIFT;
		}
		if (GetKeyState(VK_CONTROL) < 0)
		{
			eventFlags |= gmpi_gui_api::GG_POINTER_KEY_CONTROL;
		}
		if (GetKeyState(VK_MENU) < 0)
		{
			eventFlags |= gmpi_gui_api::GG_POINTER_KEY_ALT;
		}

		//		int32_t r;
		switch (message)
		{
		case WM_MOUSEMOVE:
		{
			//?			r = containerView->onPointerMove(eventFlags, p);
		}
		break;

		case WM_LBUTTONDOWN:
		case WM_RBUTTONDOWN:
		case WM_MBUTTONDOWN:
			//?			r = containerView->onPointerDown(eventFlags, p);
			break;

		case WM_MBUTTONUP:
		case WM_RBUTTONUP:
		case WM_LBUTTONUP:
			//? handled by drawingframe?			r = containerView->onPointerUp(eventFlags, p);
			break;
		}
	}
	break;
#endif

	case WM_NCACTIVATE:
		//if( wParam == FALSE ) // USER CLICKED AWAY
		//	goto we_re_done;
		break;
		/*
			case WM_WINDOWPOSCHANGING:
			{
				LPWINDOWPOS wp = (LPWINDOWPOS)lParam;
			}
			break;
		*/
	case WM_ACTIVATE:
	{
		/*
		//HFONT hFont = CreateFont(18, 0, 0, 0, FW_DONTCARE, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_OUTLINE_PRECIS,
		//	CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, VARIABLE_PITCH, TEXT("Courier New"));

		SendMessage(child,      // Handle of edit control
		WM_SETFONT,         // Message to change the font
		(WPARAM)dialogFont,      // handle of the font
		MAKELPARAM(TRUE, 0) // Redraw text
		);

		::SetWindowPos(hwndDlg, 0, dialogX, dialogY, dialogW, dialogH, SWP_NOZORDER);
		::SetWindowPos(child, 0, 0, 0, dialogW, dialogH, SWP_NOZORDER);
		::SetWindowText(child, dialogEditText);
		::SetFocus(child);
		dialogReturnValue = 1;
		// Select all.
		#ifdef WIN32
		SendMessage(child, EM_SETSEL, (WPARAM)0, (LPARAM)-1);
		#else
		SendMessage(child, EM_SETSEL, 0, MAKELONG(0, -1));
		#endif
		*/
	}
	break;

	/*
case WM_COMMAND:
	switch( LOWORD(wParam) )
	{
	case IDOK:
	goto we_re_done;
	break;

	case IDCANCEL:
	dialogReturnValue = 0;
	EndDialog(hwndDlg, dialogReturnValue); // seems to call back here and exit at "we_re_done"
	return TRUE;
	}
	break;
	*/

	case WM_PAINT:
	{
		OnPaint();
		//		return ::DefWindowProc(windowHandle, message, wParam, lParam); // clear update rect.
	}
	break;

	default:
		return DefWindowProc(hwnd, message, wParam, lParam);

		//we_re_done:
		//	if( !GetDlgItemText(hwndDlg, IDC_EDIT1, dialogEditText, sizeof(dialogEditText) / sizeof(dialogEditText[0])) )
		//		*dialogEditText = 0;
		//	EndDialog(hwndDlg, dialogReturnValue);

	}
	return TRUE;
}

inline LRESULT CALLBACK DrawingFrameWindowProc(HWND hwnd,
	UINT message,
	WPARAM wParam,
	LPARAM lParam)
{
	auto drawingFrame = (JuceDrawingFrameBase*)(LONG_PTR)GetWindowLongPtr(hwnd, GWLP_USERDATA);
	if (drawingFrame)
	{
		return drawingFrame->WindowProc(hwnd, message, wParam, lParam);
	}

	return DefWindowProc(hwnd, message, wParam, lParam);
}

// copied from MP_GetDllHandle
inline HMODULE local_GetDllHandle_randomshit()
{
	HMODULE hmodule = 0;
	GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)&local_GetDllHandle_randomshit, &hmodule);
	return (HMODULE)hmodule;
}

inline static bool registeredWindowClass = false;
inline static WNDCLASS windowClass;
inline static wchar_t gClassName[100];

inline void JuceDrawingFrameBase::open(void* parentWnd, int width, int height)
{
	RECT r{ 0, 0, width ,height };
	/* while constructing editor, JUCE main window is a small fixed size, so no point querying it. easier to just pass in required size.
	GetClientRect(parentWnd, &r);
	*/

	if (!registeredWindowClass)
	{
		registeredWindowClass = true;
		OleInitialize(0);

		swprintf(gClassName, sizeof(gClassName) / sizeof(gClassName[0]), L"GMPIGUI%p", local_GetDllHandle_randomshit());

		windowClass.style = CS_GLOBALCLASS;// | CS_DBLCLKS;//|CS_OWNDC; // add Private-DC constant 

		windowClass.lpfnWndProc = DrawingFrameWindowProc;
		windowClass.cbClsExtra = 0;
		windowClass.cbWndExtra = 0;
		windowClass.hInstance = local_GetDllHandle_randomshit();
		windowClass.hIcon = 0;

		windowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
#if DEBUG_DRAWING
		windowClass.hbrBackground = GetSysColorBrush(COLOR_BTNFACE);
#else
		windowClass.hbrBackground = 0;
#endif
		windowClass.lpszMenuName = 0;
		windowClass.lpszClassName = gClassName;
		RegisterClass(&windowClass);

		//		bSwapped_mouse_buttons = GetSystemMetrics(SM_SWAPBUTTON) > 0;
	}

	int style = WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS;// | WS_OVERLAPPEDWINDOW;
	int extended_style = 0;

	const auto windowHandle = CreateWindowEx(extended_style, gClassName, L"",
		style, 0, 0, r.right - r.left, r.bottom - r.top,
		(HWND)parentWnd, NULL, local_GetDllHandle_randomshit(), NULL);

	if (windowHandle)
	{
		juceComponent.setHWND(windowHandle);

		SetWindowLongPtr(windowHandle, GWLP_USERDATA, (__int3264)(LONG_PTR)this);
		//		RegisterDragDrop(windowHandle, new CDropTarget(this));

		CreateRenderTarget();

		initTooltip();

		StartTimer(15); // 16.66 = 60Hz. 16ms timer seems to miss v-sync. Faster timers offer no improvement to framerate.
	}
}
#else

// macOS
inline void GmpiViewComponent::open(gmpi::IMpUnknown* client, int width, int height)
{
	auto nsView = createNativeView((class IMpUnknown*)client, width, height); // !!!probly need to pass controller

	setView(nsView);
	//    auto y = CFGetRetainCount(nsView);
}

inline GmpiViewComponent::~GmpiViewComponent()
{
	onCloseNativeView(getView());
}
#endif

// CONVERSIONS
inline GmpiDrawing::RectL toGmpi(juce::Rectangle<int> r)
{
	return { r.getX(), r.getY(), r.getRight(), r.getBottom() };
}

inline GmpiDrawing::Rect toGmpi(juce::Rectangle<float> r)
{
	return { r.getX(), r.getY(), r.getRight(), r.getBottom() };
}

inline GmpiDrawing::Color toGmpi(juce::Colour r)
{
	GmpiDrawing::Color ret;
	ret.InitFromSrgba(r.getRed(), r.getGreen(), r.getBlue(), r.getFloatAlpha());
	return ret;
}
