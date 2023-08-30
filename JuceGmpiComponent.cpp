#include "JuceGmpiComponent.h"
#include "../../../se_sdk3_hosting/GraphicsRedrawClient.h"
#include "../../../se_sdk3/TimerManager.h"
#include "../../../RefCountMacros.h"
//#include <Windowsx.h>

class JuceComponentProxy : public IMpDrawingClient
{
	class GmpiComponent* component = {};
	IDrawingHost* drawinghost = {};
public:

	JuceComponentProxy(class GmpiComponent* pcomponent) : component(pcomponent) {}

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


#ifdef _WIN32

// Add the path to the gmpi_ui library in the Projucer setting 'Header Search Paths'.
#include "backends/DrawingFrame_win32.h"

class JuceDrawingFrameBase : public GmpiGuiHosting::DrawingFrameBase
{
	juce::HWNDComponent& juceComponent;

public:
	JuceDrawingFrameBase(juce::HWNDComponent& pJuceComponent) : juceComponent(pJuceComponent)
	{
	}

	void open(void* pParentWnd, int width, int height);

	HWND getWindowHandle() override
	{
		return (HWND)juceComponent.getHWND();
	}

	LRESULT WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
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
};

LRESULT CALLBACK DrawingFrameWindowProc(HWND hwnd,
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
HMODULE local_GetDllHandle_randomshit()
{
	HMODULE hmodule = 0;
	GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)&local_GetDllHandle_randomshit, &hmodule);
	return (HMODULE)hmodule;
}

static bool registeredWindowClass = false;
static WNDCLASS windowClass;
static wchar_t gClassName[100];

void JuceDrawingFrameBase::open(void* parentWnd, int width, int height)
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

struct GmpiComponent::Pimpl
{
	JuceComponentProxy proxy;
    JuceDrawingFrameBase JuceDrawingFrameBase;

    void open(HWND hwnd, juce::Rectangle<int> r)
    {
        HDC hdc = ::GetDC(hwnd);
        const int dpi = GetDeviceCaps(hdc, LOGPIXELSX);
        ::ReleaseDC(hwnd, hdc);

        JuceDrawingFrameBase.AddView(static_cast</*gmpi_gui_api::*/IMpDrawingClient*>(&proxy));

        JuceDrawingFrameBase.open(
            hwnd,
            (r.getWidth() * dpi) / 96,
            (r.getHeight() * dpi) / 96
        );
    }
};

void GmpiComponent::parentHierarchyChanged()
{
    if (auto hwnd = (HWND)getWindowHandle(); hwnd && !getHWND())
    {
        internal->open(hwnd, getBounds());
    }
}

void GmpiComponent::invalidateRect()
{
    internal->JuceDrawingFrameBase.invalidateRect(nullptr);
}

GmpiComponent::GmpiComponent() :
    internal(std::make_unique<GmpiComponent::Pimpl>(this, *this))
{
}

GmpiComponent::~GmpiComponent() {}

#else
// macOS
// without including objective-C headers, we need to create an NSView.
// forward declare function here to return the view, using void* as return type.
void* createNativeView(class IMpUnknown* client, int width, int height);
void onCloseNativeView(void* ptr);

struct GmpiComponent::Pimpl
{
	JuceComponentProxy proxy;

    Pimpl(GmpiComponent* component) : proxy(component){}
    
	void open(juce::NSViewComponent* nsview)
	{
		const auto r = nsview->getLocalBounds();
		auto nsView = createNativeView((class IMpUnknown*)&proxy, r.getWidth(), r.getHeight());
		nsview->setView(nsView);
	}
};

GmpiComponent::GmpiComponent() :
    internal(std::make_unique<GmpiComponent::Pimpl>(this))
{
}

GmpiComponent::~GmpiComponent()
{
    onCloseNativeView(getView());
}

void GmpiComponent::parentHierarchyChanged()
{
	if (!getView())
	{
		internal->open(this);
	}
}

void GmpiComponent::invalidateRect()
{
	internal->proxy.invalidateRect();
}
#endif

int32_t JuceComponentProxy::OnRender(GmpiDrawing_API::IMpDeviceContext* drawingContext)
{
	GmpiDrawing::Graphics g(drawingContext);
	component->OnRender(g);
	return gmpi::MP_OK;
}
