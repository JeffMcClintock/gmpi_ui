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

		GMPI_QUERYINTERFACE(IDrawingClient);
		GMPI_QUERYINTERFACE(IInputClient);

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

	const auto windowClass = gmpi::hosting::RegisterWindowsClass(getDllHandle(), DrawingFrameWindowProc);
	const auto lwindowHandle = gmpi::hosting::CreateHostingWindow(getDllHandle(), windowClass, (HWND)pparentWnd, r, (LONG_PTR)static_cast<gmpi::hosting::DxDrawingFrameBase*>(this));

	if (!lwindowHandle)
		return;

	juceComponent.setHWND(lwindowHandle);

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
