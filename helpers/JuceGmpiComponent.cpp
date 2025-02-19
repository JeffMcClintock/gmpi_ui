#include "JuceGmpiComponent.h"
#include "NativeUi.h"
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

class JuceDrawingFrameBase : public gmpi::hosting::DxDrawingFrameBase
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
};

static bool registeredWindowClass = false;
static WNDCLASS windowClass;
static wchar_t gClassName[100];

LRESULT CALLBACK DrawingFrameWindowProc(
	HWND hwnd,
	UINT message,
	WPARAM wParam,
	LPARAM lParam)
{
	auto drawingFrame = (gmpi::hosting::DxDrawingFrameBase*)(LONG_PTR)GetWindowLongPtr(hwnd, GWLP_USERDATA);
	if (drawingFrame)
	{
		return drawingFrame->WindowProc(hwnd, message, wParam, lParam);
	}

	return DefWindowProc(hwnd, message, wParam, lParam);
}

void JuceDrawingFrameBase::open(void* pparentWnd, int width, int height)
{
	// while constructing editor, JUCE main window is a small fixed size, so no point querying it. easier to just pass in required size.
	RECT r{ 0, 0, width ,height };

	if (!registeredWindowClass)
	{
		registeredWindowClass = true;
		OleInitialize(0);

		swprintf(gClassName, sizeof(gClassName) / sizeof(gClassName[0]), L"GMPIGUI%p", getDllHandle());

		windowClass.style = CS_GLOBALCLASS;// | CS_DBLCLKS;//|CS_OWNDC; // add Private-DC constant 

		windowClass.lpfnWndProc = DrawingFrameWindowProc;
		windowClass.cbClsExtra = 0;
		windowClass.cbWndExtra = 0;
		windowClass.hInstance = getDllHandle();
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

	const auto lwindowHandle = CreateWindowEx(extended_style, gClassName, L"",
		style, 0, 0, r.right - r.left, r.bottom - r.top,
		(HWND)pparentWnd, NULL, getDllHandle(), NULL);

	if (!lwindowHandle)
		return;

	juceComponent.setHWND(lwindowHandle);

	SetWindowLongPtr(lwindowHandle, GWLP_USERDATA, (__int3264)(LONG_PTR)static_cast<gmpi::hosting::DxDrawingFrameBase*>(this));
	//		RegisterDragDrop(windowHandle, new CDropTarget(this));

	CreateSwapPanel();

	initTooltip();

	if (drawingClient)
	{
		const auto scale = getRasterizationScale();

		const drawing::Size available{
			static_cast<float>((r.right - r.left) * scale),
			static_cast<float>((r.bottom - r.top) * scale)
		};

		drawing::Size desired{};
#ifdef GMPI_HOST_POINTER_SUPPORT
		gmpi_gui_client->measure(available, &desired);
		gmpi_gui_client->arrange({ 0, 0, available.width, available.height });
#endif
		drawingClient->measure(&available, &desired);
		const drawing::Rect finalRect{ 0, 0, available.width, available.height };
		drawingClient->arrange(&finalRect);
	}

	startTimer(15); // 16.66 = 60Hz. 16ms timer seems to miss v-sync. Faster timers offer no improvement to framerate.
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

        JuceDrawingFrameBase.attachClient(static_cast<gmpi::api::IDrawingClient*>(&proxy));

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
void gmpi_onCloseNativeView(void* ptr);

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
    gmpi_onCloseNativeView(getView());
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
