#include "JuceGmpiComponent.h"
#include "helpers/GraphicsRedrawClient.h"
#include "helpers/Timer.h"
#include "../../../RefCountMacros.h"

#ifdef _WIN32
#include "windowsx.h"
// Add the path to the gmpi_ui library in the Projucer setting 'Header Search Paths'.
#include "backends/DrawingFrameWin.h"
#endif

using namespace gmpi;
using namespace gmpi::drawing;

namespace gmpi
{
namespace interaction
{
	// TODO niceify
	enum GG_POINTER_FLAGS {
		GG_POINTER_FLAG_NONE = 0,
		GG_POINTER_FLAG_NEW = 0x01,					// Indicates the arrival of a new pointer.
		GG_POINTER_FLAG_INCONTACT = 0x04,
		GG_POINTER_FLAG_FIRSTBUTTON = 0x10,
		GG_POINTER_FLAG_SECONDBUTTON = 0x20,
		GG_POINTER_FLAG_THIRDBUTTON = 0x40,
		GG_POINTER_FLAG_FOURTHBUTTON = 0x80,
		GG_POINTER_FLAG_CONFIDENCE = 0x00000400,	// Confidence is a suggestion from the source device about whether the pointer represents an intended or accidental interaction.
		GG_POINTER_FLAG_PRIMARY = 0x00002000,	// First pointer to contact surface. Mouse is usually Primary.

		GG_POINTER_SCROLL_HORIZ = 0x00008000,	// Mouse Wheel is scrolling horizontal.

		GG_POINTER_KEY_SHIFT = 0x00010000,	// Modifer key - <SHIFT>.
		GG_POINTER_KEY_CONTROL = 0x00020000,	// Modifer key - <CTRL> or <Command>.
		GG_POINTER_KEY_ALT = 0x00040000,	// Modifer key - <ALT> or <Option>.
	};
}
}

class JuceComponentProxy : public gmpi::api::IDrawingClient, public gmpi::api::IInputClient
{
	class GmpiComponent* component = {};
	gmpi::api::IDrawingHost* drawinghost = {};

public:
	JuceComponentProxy(class GmpiComponent* pcomponent) : component(pcomponent) {}

	gmpi::ReturnCode open(gmpi::api::IUnknown* host) override
	{
		return host->queryInterface(&gmpi::api::IDrawingHost::guid, (void**)&drawinghost);
	}

	void invalidateRect()
	{
		drawinghost->invalidateRect(nullptr);
	}

	// First pass of layout update. Return minimum size required for given available size
	gmpi::ReturnCode measure(const gmpi::drawing::Size* availableSize, gmpi::drawing::Size* returnDesiredSize) override { return gmpi::ReturnCode::NoSupport; }

	// Second pass of layout.
	gmpi::ReturnCode arrange(const gmpi::drawing::Rect* finalRect) override { return gmpi::ReturnCode::Ok; }

	gmpi::ReturnCode render(gmpi::drawing::api::IDeviceContext* drawingContext) override;

	gmpi::ReturnCode getClipArea(drawing::Rect* returnRect) override { return gmpi::ReturnCode::NoSupport; }

	// IInputClient
	gmpi::ReturnCode hitTest(gmpi::drawing::Point point, int32_t flags) override { return gmpi::ReturnCode::Ok; }
	gmpi::ReturnCode onPointerDown(gmpi::drawing::Point point, int32_t flags) override;
	gmpi::ReturnCode onPointerMove(gmpi::drawing::Point point, int32_t flags) override;
	gmpi::ReturnCode onPointerUp  (gmpi::drawing::Point point, int32_t flags) override;
	gmpi::ReturnCode onMouseWheel (gmpi::drawing::Point point, int32_t flags, int32_t delta) override { return gmpi::ReturnCode::Unhandled; }
	gmpi::ReturnCode setHover(bool isMouseOverMe) { return gmpi::ReturnCode::Unhandled; }
	gmpi::ReturnCode OnKeyPress(wchar_t c) override { return gmpi::ReturnCode::Unhandled; }
	gmpi::ReturnCode populateContextMenu(gmpi::drawing::Point point, gmpi::api::IUnknown* contextMenuItemsSink) override { return gmpi::ReturnCode::Unhandled; }
	gmpi::ReturnCode onContextMenu(int32_t idx) override { return gmpi::ReturnCode::Unhandled; }

	// TODO GMPI_QUERYINTERFACE(IDrawingClient::guid, IDrawingClient);
	gmpi::ReturnCode queryInterface(const gmpi::api::Guid* iid, void** returnInterface) override
	{
		*returnInterface = nullptr;

		if (*iid == IDrawingClient::guid || *iid == gmpi::api::IUnknown::guid)
		{
			*returnInterface = static_cast<IDrawingClient*>(this);
			addRef();
			return gmpi::ReturnCode::Ok;
		}

		if (iid == /*gmpi::interaction::*/&IInputClient::guid)
		{
			*returnInterface = static_cast<IInputClient*>(this);
			addRef();
			return gmpi::ReturnCode::Ok;
		}

#ifdef GMPI_HOST_POINTER_SUPPORT
		if (iid == gmpi::interaction::SE_IID_GRAPHICS_MPGUI3)
		{
			*returnInterface = static_cast<IMpGraphics3*>(this);
			addRef();
			return gmpi::ReturnCode::Ok;
		}

		if (iid == gmpi::interaction::SE_IID_GRAPHICS_MPGUI2)
		{
			*returnInterface = static_cast<IMpGraphics2*>(this);
			addRef();
			return gmpi::ReturnCode::Ok;
		}

		if (iid == gmpi::interaction::SE_IID_GRAPHICS_MPGUI)
		{
			*returnInterface = static_cast<IMpGraphics*>(this);
			addRef();
			return gmpi::ReturnCode::Ok;
		}
#endif

		return gmpi::ReturnCode::NoSupport;
	}
	GMPI_REFCOUNT_NO_DELETE;
};


#ifdef _WIN32

class JuceDrawingFrameBase : public gmpi::hosting::DrawingFrameBase
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
#if 1 //def GMPI_HOST_POINTER_SUPPORT
		case WM_MBUTTONDOWN:
		case WM_MBUTTONUP:
		case WM_MOUSEMOVE:
		case WM_LBUTTONDOWN:
		case WM_LBUTTONUP:
		case WM_RBUTTONDOWN:
		case WM_RBUTTONUP:
		{
			Point p{ static_cast<float>(GET_X_LPARAM(lParam)), static_cast<float>(GET_Y_LPARAM(lParam)) };
			p = transformPoint(WindowToDips, p);

			// Cubase sends spurious mouse move messages when transport running.
			// This prevents tooltips working.
			if (message == WM_MOUSEMOVE)
			{
				if (cubaseBugPreviousMouseMove.x == p.x && cubaseBugPreviousMouseMove.y == p.y)
				{
					return TRUE;
				}
				cubaseBugPreviousMouseMove = p;
			}
			else
			{
				cubaseBugPreviousMouseMove = Point(-1, -1);
			}

			TooltipOnMouseActivity();

			int32_t eventFlags = gmpi::interaction::GG_POINTER_FLAG_INCONTACT | gmpi::interaction::GG_POINTER_FLAG_PRIMARY | gmpi::interaction::GG_POINTER_FLAG_CONFIDENCE;

			switch (message)
			{
			case WM_MBUTTONDOWN:
			case WM_LBUTTONDOWN:
			case WM_RBUTTONDOWN:
				eventFlags |= gmpi::interaction::GG_POINTER_FLAG_NEW;
				break;
			}

			switch (message)
			{
			case WM_LBUTTONUP:
			case WM_LBUTTONDOWN:
				eventFlags |= gmpi::interaction::GG_POINTER_FLAG_FIRSTBUTTON;
				break;
			case WM_RBUTTONDOWN:
			case WM_RBUTTONUP:
				eventFlags |= gmpi::interaction::GG_POINTER_FLAG_SECONDBUTTON;
				break;
			case WM_MBUTTONDOWN:
			case WM_MBUTTONUP:
				eventFlags |= gmpi::interaction::GG_POINTER_FLAG_THIRDBUTTON;
				break;
			}

			if (GetKeyState(VK_SHIFT) < 0)
			{
				eventFlags |= gmpi::interaction::GG_POINTER_KEY_SHIFT;
			}
			if (GetKeyState(VK_CONTROL) < 0)
			{
				eventFlags |= gmpi::interaction::GG_POINTER_KEY_CONTROL;
			}
			if (GetKeyState(VK_MENU) < 0)
			{
				eventFlags |= gmpi::interaction::GG_POINTER_KEY_ALT;
			}

			gmpi::ReturnCode r{};
			if (inputClient)
			{
				switch (message)
				{
				case WM_MOUSEMOVE:
				{
					r = inputClient->onPointerMove(p, eventFlags);
				}
				break;

				case WM_LBUTTONDOWN:
				case WM_RBUTTONDOWN:
				case WM_MBUTTONDOWN:
					r = inputClient->onPointerDown(p, eventFlags);
					break;

				case WM_MBUTTONUP:
				case WM_RBUTTONUP:
				case WM_LBUTTONUP:
					r = inputClient->onPointerUp(p, eventFlags);
					break;
				}
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
	// while constructing editor, JUCE main window is a small fixed size, so no point querying it. easier to just pass in required size.
	RECT r{ 0, 0, width ,height };

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

		startTimer(15); // 16.66 = 60Hz. 16ms timer seems to miss v-sync. Faster timers offer no improvement to framerate.
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

        JuceDrawingFrameBase.AddView(static_cast<gmpi::api::IDrawingClient*>(&proxy));

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
void* createNativeView(void* parent, class IUnknown* paramHost, class IUnknown* client, int width, int height);
void onCloseNativeView(void* ptr);

struct GmpiComponent::Pimpl
{
	JuceComponentProxy proxy;

    Pimpl(GmpiComponent* component) : proxy(component){}
    
	void open(juce::NSViewComponent* nsview)
	{
		const auto r = nsview->getLocalBounds();
		auto nsView = createNativeView(nullptr, nullptr, (class IUnknown*)&proxy, r.getWidth(), r.getHeight());
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

gmpi::ReturnCode JuceComponentProxy::render(gmpi::drawing::api::IDeviceContext* drawingContext)
{
	gmpi::drawing::Graphics g(drawingContext);
	component->onRender(g);
	return gmpi::ReturnCode::Ok;
}

gmpi::ReturnCode JuceComponentProxy::onPointerDown(gmpi::drawing::Point point, int32_t flags)
{
	return component->onPointerDown(point, flags);
}

gmpi::ReturnCode JuceComponentProxy::onPointerMove(gmpi::drawing::Point point, int32_t flags)
{
	return component->onPointerMove(point, flags);
}

gmpi::ReturnCode JuceComponentProxy::onPointerUp(gmpi::drawing::Point point, int32_t flags)
{
	return component->onPointerUp(point, flags);
}
