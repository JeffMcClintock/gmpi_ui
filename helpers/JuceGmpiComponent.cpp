#include "JuceGmpiComponent.h"
#include "NativeUi.h"
#include "helpers/Timer.h"
#include "../../../RefCountMacros.h"

#if GMPI_UI_USE_JUCE_GRAPHICS
#include "backends/JuceGfx.h"
#elif defined(_WIN32)
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

	gmpi::ReturnCode setHost(gmpi::api::IUnknown* host) override
	{
		if (!host)
		{
			drawinghost = {};
			return gmpi::ReturnCode::Ok;
		}
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
	gmpi::ReturnCode setHover(bool isMouseOverMe) override { return gmpi::ReturnCode::Unhandled; }
	gmpi::ReturnCode onKeyPress(wchar_t c) override { return gmpi::ReturnCode::Unhandled; }
	gmpi::ReturnCode populateContextMenu(gmpi::drawing::Point point, gmpi::api::IUnknown* contextMenuItemsSink) override { return gmpi::ReturnCode::Unhandled; }
	gmpi::ReturnCode getToolTip(gmpi::drawing::Point point, gmpi::api::IString* returnString) override { return gmpi::ReturnCode::Unhandled; }

	// TODO GMPI_QUERYINTERFACE(IDrawingClient::guid, IDrawingClient);
	gmpi::ReturnCode queryInterface(const gmpi::api::Guid* iid, void** returnInterface) override
	{
		*returnInterface = nullptr;
		GMPI_QUERYINTERFACE(IDrawingClient);
		GMPI_QUERYINTERFACE(IInputClient);
		return gmpi::ReturnCode::NoSupport;
	}
	GMPI_REFCOUNT_NO_DELETE;
};


#if GMPI_UI_USE_JUCE_GRAPHICS

// JUCE-graphics backend: an ordinary juce::Component painted via backends/JuceGfx.h.
// No native window, no platform code - works on any platform JUCE supports (e.g. Linux).

struct GmpiComponent::Pimpl
{
	gmpi::jucegfx::Factory factory;
};

GmpiComponent::GmpiComponent() :
	internal(std::make_unique<GmpiComponent::Pimpl>())
{
}

GmpiComponent::~GmpiComponent() {}

void GmpiComponent::parentHierarchyChanged()
{
}

void GmpiComponent::invalidateRect()
{
	repaint();
}

void GmpiComponent::paint(juce::Graphics& g)
{
	gmpi::jucegfx::GraphicsContext context(g, &internal->factory);

	gmpi::drawing::Graphics dc(&context);
	onRender(dc);
}

namespace
{
int32_t toGmpiPointerFlags(const juce::MouseEvent& e, bool isContact)
{
	int32_t flags = static_cast<int32_t>(gmpi::api::PointerFlags::Primary) | static_cast<int32_t>(gmpi::api::PointerFlags::Confidence);

	if (isContact)
		flags |= static_cast<int32_t>(gmpi::api::PointerFlags::InContact);

	if (e.mods.isLeftButtonDown())
		flags |= static_cast<int32_t>(gmpi::api::PointerFlags::FirstButton);
	if (e.mods.isRightButtonDown())
		flags |= static_cast<int32_t>(gmpi::api::PointerFlags::SecondButton);
	if (e.mods.isMiddleButtonDown())
		flags |= static_cast<int32_t>(gmpi::api::PointerFlags::ThirdButton);

	if (e.mods.isShiftDown())
		flags |= static_cast<int32_t>(gmpi::api::PointerFlags::KeyShift);
	if (e.mods.isCtrlDown())
		flags |= static_cast<int32_t>(gmpi::api::PointerFlags::KeyControl);
	if (e.mods.isAltDown())
		flags |= static_cast<int32_t>(gmpi::api::PointerFlags::KeyAlt);

	return flags;
}
}

void GmpiComponent::mouseDown(const juce::MouseEvent& e)
{
	onPointerDown({ e.position.x, e.position.y }, toGmpiPointerFlags(e, true));
}

void GmpiComponent::mouseMove(const juce::MouseEvent& e)
{
	onPointerMove({ e.position.x, e.position.y }, toGmpiPointerFlags(e, false));
}

void GmpiComponent::mouseDrag(const juce::MouseEvent& e)
{
	onPointerMove({ e.position.x, e.position.y }, toGmpiPointerFlags(e, true));
}

void GmpiComponent::mouseUp(const juce::MouseEvent& e)
{
	// note: in juce::Component::mouseUp, e.mods still reports the button that was just released.
	onPointerUp({ e.position.x, e.position.y }, toGmpiPointerFlags(e, false));
}

#elif defined(_WIN32)

class JuceDrawingFrameBase : public gmpi::hosting::DxDrawingFrameHwnd
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
	auto drawingFrame = (gmpi::hosting::DxDrawingFrameHwnd*)(LONG_PTR)GetWindowLongPtr(hwnd, GWLP_USERDATA);
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
	const auto lwindowHandle = gmpi::hosting::CreateHostingWindow(getDllHandle(), windowClass, (HWND)pparentWnd, r, (LONG_PTR)static_cast<gmpi::hosting::DxDrawingFrameHwnd*>(this));

	if (!lwindowHandle)
		return;

	juceComponent.setHWND(lwindowHandle);

	CreateSwapPanel(DrawingFactory.getD2dFactory());

	initTooltip();

	if (drawingClient)
	{
		const auto scale = 1.0 / getRasterizationScale();

		sizeClientDips(
			static_cast<float>(width) * scale,
			static_cast<float>(height) * scale);
	}

	startTimer(15); // 16.66 = 60Hz. 16ms timer seems to miss v-sync. Faster timers offer no improvement to framerate.
}

struct GmpiComponent::Pimpl
{
	GmpiComponent* outer{};
	std::unique_ptr<JuceComponentProxy> client;
    JuceDrawingFrameBase JuceDrawingFrameBase;

	GmpiComponent::Pimpl(GmpiComponent* pouter) :
		outer(pouter)
		, JuceDrawingFrameBase(*pouter)
	{
	}

    void open(HWND hwnd, juce::Rectangle<int> r)
    {
		JuceDrawingFrameBase.clientInvalidated = [this]()
			{
				JuceDrawingFrameBase.detachAndRecreate();

				client = std::make_unique<JuceComponentProxy>(outer);

				JuceDrawingFrameBase.attachClient(static_cast<gmpi::api::IDrawingClient*>(client.get()));

			};

		HDC hdc = ::GetDC(hwnd);
		const int dpi = GetDeviceCaps(hdc, LOGPIXELSX);
		::ReleaseDC(hwnd, hdc);

		JuceDrawingFrameBase.open(
			hwnd,
			(r.getWidth() * dpi) / 96,
			(r.getHeight() * dpi) / 96
		);

		JuceDrawingFrameBase.clientInvalidated();
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
    internal(std::make_unique<GmpiComponent::Pimpl>(this))
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
