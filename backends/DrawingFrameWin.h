#pragma once

/*
#include "backends/DrawingFrameWin.h"
*/

#if defined(_WIN32) // skip compilation on macOS

#include <vector>
#include <string>
#include <memory>
#include <chrono>
#include <span>
#include <dxgi1_6.h>
#include <d3d11_1.h>
#include <dwmapi.h>
#include <Windowsx.h>
#include "DirectXGfx.h"
#include "helpers/NativeUi.h"
#include "helpers/Timer.h"
#include "GmpiSdkCommon.h"

namespace gmpi
{

namespace hosting
{
// utility functions
std::wstring RegisterWindowsClass(HINSTANCE dllHandle, WNDPROC windowProc);
HWND CreateHostingWindow(HMODULE dllHandle, const std::wstring& windowClass, HWND parentWnd, RECT r, LONG_PTR userData);

namespace win32
{
	// Single shared Win32 implementation of gmpi::api::ITextEdit. Used by both the
	// gmpi_ui DxDrawingFrameBase and SynthEditLib's DrawingFrameBase2 so the modal
	// edit-box logic lives in exactly one place. Returns an addRef'd ITextEdit via
	// returnTextEdit; the caller takes ownership. `rect` is in DIPs; the factory
	// applies dpiScale internally.
	gmpi::ReturnCode createPlatformTextEdit(
		HWND parentWnd,
		const gmpi::drawing::Rect* rect,
		float dpiScale,
		gmpi::api::IUnknown** returnTextEdit);

	// Shared Win32 IPopupMenu factory. `rect` is in DIPs (only the top-left corner
	// is used — that's where TrackPopupMenu pops the menu from). DPI scaling for
	// menu position is currently a no-op (DPI=1 path), preserved from the legacy
	// behaviour of both pre-existing implementations.
	gmpi::ReturnCode createPlatformPopupMenu(
		HWND parentWnd,
		const gmpi::drawing::Rect* rect,
		float dpiScale,
		gmpi::api::IUnknown** returnMenu);

	// Shared Win32 IFileDialog factory. `dialogType` is gmpi::api::FileDialogType.
	// Returns an addRef'd IFileDialog via returnDialog; caller owns the ref.
	gmpi::ReturnCode createPlatformFileDialog(
		HWND parentWnd,
		int32_t dialogType,
		gmpi::api::IUnknown** returnDialog);

	// Shared Win32 IStockDialog factory (MessageBoxW-backed). `dialogType` is
	// gmpi::api::StockDialogType. Title/text are UTF-8.
	gmpi::ReturnCode createPlatformStockDialog(
		HWND parentWnd,
		int32_t dialogType,
		const char* title,
		const char* text,
		gmpi::api::IUnknown** returnDialog);
} // namespace win32

class UpdateRegionWinGdi
{
	HRGN hRegion = 0;
	std::string regionDataBuffer;
	std::vector<gmpi::drawing::RectL> rects;
	gmpi::drawing::RectL bounds;

public:
	UpdateRegionWinGdi();
	~UpdateRegionWinGdi();

	void copyDirtyRects(HWND window, gmpi::drawing::SizeL swapChainSize);
	void optimizeRects();

	inline std::vector<gmpi::drawing::RectL>& getUpdateRects()
	{
		return rects;
	}
	inline gmpi::drawing::RectL& getBoundingRect()
	{
		return bounds;
	}
};

class DirtyRectQueue
{
	std::vector<gmpi::drawing::RectL> rects;

public:
	void clear()
	{
		rects.clear();
	}

	bool empty() const
	{
		return rects.empty();
	}

	std::span<gmpi::drawing::RectL> get()
	{
		return { rects.data(), rects.size() };
	}

	std::span<const gmpi::drawing::RectL> get() const
	{
		return { rects.data(), rects.size() };
	}

	void add(gmpi::drawing::RectL rect);
	void add(const gmpi::drawing::Rect* invalidRect, const gmpi::drawing::Matrix3x2& dipsToWindow);
	void replace(gmpi::drawing::RectL rect);
};

class ToolTipManager
{
	bool shown = false;
	HWND windowHandle = {};
	int timer = 0;
	std::wstring text;
	static const int timerInit = 40; // x/60 Hz

public:
	void init(HMODULE instanceHandle, HWND parentWindow);
	void onMouseActivity(HWND parentWindow);
	void show(HWND parentWindow);
	void hide(HWND parentWindow);
	bool readyToShow()
	{
		return timer-- == 0 && !shown;
	}
	bool isShown() const
	{
		return shown;
	}
	std::wstring& getText()
	{
		return text;
	}
};

// helper
inline float calcWhiteLevelForHwnd(HWND windowHandle)
{
	// get bounds of plugin window
	auto parent = ::GetAncestor(windowHandle, GA_ROOT);
	RECT m_windowBoundsRoot;
	GetWindowRect(parent, &m_windowBoundsRoot);

	RECT m_windowBounds;
	DwmGetWindowAttribute(parent, DWMWA_EXTENDED_FRAME_BOUNDS, &m_windowBounds, sizeof(m_windowBounds));

	// not DPI aware		GetWindowRect(windowHandle, &m_windowBounds);
	gmpi::drawing::RectL appWindowRect{ m_windowBounds.left, m_windowBounds.top, m_windowBounds.right, m_windowBounds.bottom };

	// get all the monitor paths.
	uint32_t numPathArrayElements{};
	uint32_t numModeArrayElements{};

	GetDisplayConfigBufferSizes(
		QDC_ONLY_ACTIVE_PATHS,
		&numPathArrayElements,
		&numModeArrayElements
	);

	std::vector<DISPLAYCONFIG_PATH_INFO> pathInfo;
	std::vector<DISPLAYCONFIG_MODE_INFO> modeInfo;

	pathInfo.resize(numPathArrayElements);
	modeInfo.resize(numModeArrayElements);

	QueryDisplayConfig(
		QDC_ONLY_ACTIVE_PATHS,
		&numPathArrayElements,
		pathInfo.data(),
		&numModeArrayElements,
		modeInfo.data(),
		nullptr
	);

	DISPLAYCONFIG_TARGET_DEVICE_NAME targetName{};
	targetName.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_NAME;
	targetName.header.size = sizeof(targetName);

	DISPLAYCONFIG_SDR_WHITE_LEVEL white_level{};
	white_level.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SDR_WHITE_LEVEL;
	white_level.header.size = sizeof(white_level);

	// check against each path (monitor)
	float whiteMult{ 1.0f };
	int bestIntersectArea = -1;
	for(auto& path : pathInfo)
	{
		const int idx = path.sourceInfo.modeInfoIdx;

		if(idx == DISPLAYCONFIG_PATH_MODE_IDX_INVALID)
			continue;

		const auto& sourceMode = modeInfo[idx].sourceMode;

		const gmpi::drawing::RectL outputRect
		{
			  sourceMode.position.x
			, sourceMode.position.y
			, static_cast<int32_t>(sourceMode.position.x + sourceMode.width)
			, static_cast<int32_t>(sourceMode.position.y + sourceMode.height)
		};

		const auto intersectRect = gmpi::drawing::intersectRect(appWindowRect, outputRect);
		const int intersectArea = getWidth(intersectRect) * getHeight(intersectRect);
		if(intersectArea <= bestIntersectArea)
			continue;

		// Get monitor handle for this path's target
		targetName.header.adapterId = path.targetInfo.adapterId;
		white_level.header.adapterId = path.targetInfo.adapterId;
		targetName.header.id = path.targetInfo.id;
		white_level.header.id = path.targetInfo.id;

		if(DisplayConfigGetDeviceInfo(&targetName.header) != ERROR_SUCCESS)
			continue;

		if(DisplayConfigGetDeviceInfo(&white_level.header) != ERROR_SUCCESS)
			continue;

		// divide by 1000 to get a factor
		const auto lwhiteMult = white_level.SDRWhiteLevel / 1000.f;

		//			_RPTWN(0, L"Monitor %s [%d, %d] white level %f\n", targetName.monitorFriendlyDeviceName, outputRect.left, outputRect.top, lwhiteMult);

		bestIntersectArea = intersectArea;
		whiteMult = lwhiteMult;
	}
	//		_RPTWN(0, L"Best white level %f\n", whiteMult);

	return whiteMult;
}


// Swapchain Manager: stuff we can share between DxDrawingFrameBase (GMPI-UI) and SynthEditlib DrawingFrameBase2 (SDK3)
//
// Also implements IInputHost — the three pointer-capture methods are byte-identical
// HWND wrappers in every Win32 frame (HostedView in WinUI3 overrides them), so
// hoisting them here removes the duplication on each parallel hierarchy.
struct tempSharedD2DBase : public gmpi::api::IDrawingHost, public gmpi::api::IInputHost
{
	enum { FallBack_8bit, Fallback_Software };

	// https://learn.microsoft.com/en-us/windows/win32/direct3darticles/high-dynamic-range
	const DXGI_FORMAT bestFormat = DXGI_FORMAT_R16G16B16A16_FLOAT; // Proper gamma-correct blending.
	const DXGI_FORMAT fallbackFormat = DXGI_FORMAT_B8G8R8A8_UNORM; // shitty linear blending.

	gmpi::drawing::Matrix3x2 DipsToWindow;
	gmpi::drawing::Matrix3x2 WindowToDips;

	// HDR support. Above swapChain so these get released first.
	gmpi::directx::ComPtr<ID2D1Effect> hdrWhiteScaleEffect;
	gmpi::directx::ComPtr<ID2D1BitmapRenderTarget> hdrRenderTarget;
	directx::ComPtr<::ID2D1DeviceContext> hdrRenderTargetDC; // we need to keep this, else if we requery it, it's address will change, crashing Bitmap::GetNativeBitmap() when it's checks for a consistant device context.
	gmpi::directx::ComPtr<ID2D1Bitmap> hdrBitmap;

	directx::ComPtr<::IDXGISwapChain2> swapChain;
	directx::ComPtr<::ID2D1DeviceContext> d2dDeviceContext;

	gmpi::drawing::SizeL swapChainSize = {};
	inline static bool m_disable_gpu = false;
	inline static bool m_disable_deep_color = false;
	inline static int m_fallbackStrategy = Fallback_Software;
	bool reentrant = false;
	bool lowDpiMode = {};
	bool firstPresent = false;
	float windowWhiteLevel{};
	bool monitorChanged = false;
	std::function<void()> clientInvalidated; // called from Paint if monitor white-level changed since last check.

	virtual float calcWhiteLevel()
	{
		return calcWhiteLevelForHwnd(getWindowHandle());
	}

#ifdef _DEBUG
	std::string debugName;
#endif

	// override these please.
	virtual HWND getWindowHandle() = 0;
	virtual HRESULT createNativeSwapChain
	(
		IDXGIFactory2* factory,
		ID3D11Device* d3dDevice,
		DXGI_SWAP_CHAIN_DESC1* desc,
		IDXGISwapChain1** returnSwapChain
	) = 0;
	// "Invalidate everything" extent — the surface bounds of the frame in pixel
	// coordinates. HWND hosts use GetClientRect; SwapChainPanel hosts use the
	// panel size. Used by invalidateRect(nullptr) and similar full-redraw paths.
	virtual gmpi::drawing::RectL getFullDirtyRect() = 0;

	void recreateSwapChainAndClientAsync();
	void setWhiteLevel(float newWhiteLevel);
	void CreateSwapPanel(ID2D1Factory1* d2dFactory);
	void CreateDeviceSwapChainBitmap();
	void OnSize(UINT width, UINT height);
	void PaintFrame(std::span<gmpi::drawing::RectL> dirtyRects);

	// Shared dirty-rect render loop. Caller is responsible for beginDraw/endDraw —
	// this just transforms each pixel-space dirty rect into a snap-out DIP-space
	// clip and dispatches the client's render() inside it. Null-client falls back
	// to clearing the clip to opaque black (matches SE's defensive behaviour).
	// Used by both gmpi_ui's DxDrawingFrameBase and SynthEditLib's DrawingFrameBase2 —
	// they only differ in which IDeviceContext concrete type they construct.
	void paintLoop(
		gmpi::drawing::api::IDeviceContext* deviceContext,
		std::span<gmpi::drawing::RectL> dirtyRects,
		gmpi::api::IDrawingClient* client);
	virtual ID2D1Factory1* getD2dFactory() = 0;
	virtual bool canPaint(std::span<gmpi::drawing::RectL> dirtyRects) = 0;
	// Default impl checks DXGI factory currency — if the GPU adapter changed
	// (e.g. laptop docking, switch to external GPU) the cached factory becomes
	// non-current and we must recreate the swap chain before painting. Returning
	// false here gates PaintFrame and triggers an async recreation.
	virtual bool preparePaint(std::span<gmpi::drawing::RectL> dirtyRects);
	virtual bool recreateDeviceOnPaint() const
	{
		return false;
	}
	virtual void renderFrame(ID2D1DeviceContext* deviceContext, std::span<gmpi::drawing::RectL> dirtyRects) = 0;
	virtual void OnSwapChainCreated() = 0;
	virtual void sizeClientDips(float width, float height) = 0;

	// to help re-create device when lost.
	void ReleaseDevice()
	{
		if(swapChain)
		{
			_RPT0(0, "ReleaseDevice: releasing swap chain\n");
		}
		if(hdrWhiteScaleEffect)
		{
			hdrWhiteScaleEffect->SetInput(0, nullptr);
			hdrWhiteScaleEffect = nullptr;
		}
		hdrBitmap = nullptr;
		hdrRenderTargetDC = nullptr;
		hdrRenderTarget = nullptr;
		d2dDeviceContext = nullptr;
		swapChain = nullptr;
	}
	HMODULE getDllHandle();

	// IDrawingHost
	void invalidateMeasure() override {}

	// IInputHost — trivial HWND-pointer-capture wrappers; HostedView (WinUI3)
	// overrides these for SwapChainPanel-native pointer capture.
	gmpi::ReturnCode setCapture() override
	{
		::SetCapture(getWindowHandle());
		return gmpi::ReturnCode::Ok;
	}
	gmpi::ReturnCode getCapture(bool& returnValue) override
	{
		returnValue = ::GetCapture() == getWindowHandle();
		return gmpi::ReturnCode::Ok;
	}
	gmpi::ReturnCode releaseCapture() override
	{
		::ReleaseCapture();
		return gmpi::ReturnCode::Ok;
	}

	// Native Win32 tooltip — same forwarder shape in every Win32 frame, hoisted
	// here so the four 1-line indirections aren't duplicated. HostedView (WinUI3)
	// doesn't use this and pays only the small ToolTipManager footprint.
	ToolTipManager tooltip;
	void initTooltip()           { tooltip.init(getDllHandle(), getWindowHandle()); }
	void TooltipOnMouseActivity(){ tooltip.onMouseActivity(getWindowHandle()); }
	void ShowToolTip()           { tooltip.show(getWindowHandle()); }
	void HideToolTip()           { tooltip.hide(getWindowHandle()); }
};

// Editor-host base. Adds client management (drawingClient/inputClient/IEditor
// lifecycle), the render pipeline, and IDialogHost on top of tempSharedD2DBase's
// swapchain machinery.
//
// NO HWND coupling lives here. HostedView (WinUI3 SwapChainPanel host) inherits
// this layer directly, as does SynthEditLib's DrawingFrameBase2. HWND-specific
// stuff (WindowProc, GDI dirty region, mouse tracking, timer-driven invalidation)
// lives on DxDrawingFrameHwnd below.
//
// IInputHost is inherited via tempSharedD2DBase (single path).
class DxDrawingFrameBase :
	public tempSharedD2DBase,
	public gmpi::api::IDialogHost
{
	gmpi::shared_ptr<gmpi::api::IPopupMenu> contextMenu;

public:
	// Public so external code (e.g. SEVSTGUIEditorWin.cpp's swapchain-recreate
	// callback) can ask "is a client attached?" without going through a
	// helper. Matches the public visibility SynthEditLib's old graphics_gmpi /
	// editor_gmpi had before the Phase 4b collapse.
	gmpi::shared_ptr<gmpi::api::IDrawingClient> drawingClient;
	gmpi::shared_ptr<gmpi::api::IInputClient> inputClient;
	gmpi::shared_ptr<gmpi::api::IGraphicsRedrawClient> frameUpdateClient;

protected:
	gmpi::api::IUnknown* parameterHost{};

	// Paint() uses Direct-2d which blocks on vsync. Therefore all invalid rects
	// should be applied in one "hit", else windows message queue chokes calling
	// WM_PAINT repeatedly and blocking on every rect.
	DirtyRectQueue backBufferDirtyRects;

public:
	gmpi::directx::Factory DrawingFactory;

	virtual ~DxDrawingFrameBase()
	{
		ReleaseDevice();
	}

	ID2D1Factory1* getD2dFactory() override
	{
		return DrawingFactory.getD2dFactory();
	}
	bool canPaint(std::span<gmpi::drawing::RectL> dirtyRects) override;
	void renderFrame(ID2D1DeviceContext* deviceContext, std::span<gmpi::drawing::RectL> dirtyRects) override;

	// Render-context customisation point. Default constructs a
	// gmpi::directx::GraphicsContext, runs beginDraw/paintLoop/endDraw, and
	// calls ReleaseDevice on endDraw failure. SE's DrawingFrameBase2 overrides
	// this to construct UniversalGraphicsContext (which dispatches both GMPI
	// and SDK3 IIDs in queryInterface). The renderFrame body itself doesn't
	// vary — only the IDeviceContext type.
	virtual void renderInDeviceContext(ID2D1DeviceContext* deviceContext, std::span<gmpi::drawing::RectL> dirtyRects);

	void OnSwapChainCreated() override;

	void setFallbackHost(gmpi::api::IUnknown* paramHost)
	{
		parameterHost = paramHost;
	}

	void attachClient(gmpi::api::IUnknown* gfx);
	// Detach the current client cleanly: notifies via setHost(nullptr) before
	// dropping the smart pointers, so any client cache of the host (e.g. a view's
	// `host` member used to flush invalidations) is cleared rather than left
	// dangling. Called by attachClient (to swap clients) and detachAndRecreate.
	void detachClient();
	void detachAndRecreate();
	void doContextMenu(gmpi::drawing::Point point, int32_t flags);

	// IDrawingHost
	void invalidateRect(const gmpi::drawing::Rect* invalidRect) override;
	float getRasterizationScale() override; // DPI scaling
	gmpi::ReturnCode getDrawingFactory(gmpi::api::IUnknown** returnFactory) override
	{
		*returnFactory = &DrawingFactory;
		return gmpi::ReturnCode::Ok;
	}

	// IInputHost — implemented inline on tempSharedD2DBase (HWND-pointer-capture wrappers).

	// IDialogHost
	gmpi::ReturnCode createTextEdit(const gmpi::drawing::Rect* r, gmpi::api::IUnknown** returnTextEdit) override;
	gmpi::ReturnCode createPopupMenu(const gmpi::drawing::Rect* r, gmpi::api::IUnknown** returnMenu) override;
	gmpi::ReturnCode createKeyListener(const gmpi::drawing::Rect* r, gmpi::api::IUnknown** returnKeyListener) override;
	gmpi::ReturnCode createFileDialog(int32_t dialogType, gmpi::api::IUnknown** returnMenu) override;
	gmpi::ReturnCode createStockDialog(int32_t dialogType, const char* title, const char* text, gmpi::api::IUnknown** returnDialog) override;

	// IUnknown methods
	gmpi::ReturnCode queryInterface(const gmpi::api::Guid* iid, void** returnInterface) override
	{
		*returnInterface = {};

		GMPI_QUERYINTERFACE(IDrawingHost);
		GMPI_QUERYINTERFACE(IInputHost);
		GMPI_QUERYINTERFACE(IDialogHost);

		if(parameterHost)
		{
			return parameterHost->queryInterface(iid, returnInterface);
		}

		return gmpi::ReturnCode::NoSupport;
	}

	GMPI_REFCOUNT_NO_DELETE;

	void sizeClientDips(float width, float height) override;
	virtual void autoScrollStart() {}
	virtual void autoScrollStop() {}
};

// HWND-coupled editor frame. Adds Win32 message dispatch (WindowProc), GDI
// dirty-region accounting (UpdateRegionWinGdi), mouse tracking via
// TrackMouseEvent, and a TimerClient that drains backBufferDirtyRects via
// InvalidateRect. Most VST3-style hosts inherit this layer; HostedView (WinUI3)
// stays at DxDrawingFrameBase.
//
// Virtual inheritance of DxDrawingFrameBase: SynthEditLib's DrawingFrameHwndBase
// inherits BOTH DxDrawingFrameHwnd (for HWND machinery) AND DrawingFrameBase2
// (which also virtually inherits DxDrawingFrameBase, for SDK3 dispatch). The
// shared virtual base collapses the diamond into a single subobject. Cost is
// one vbase pointer per instance — negligible.
class DxDrawingFrameHwnd :
	public virtual DxDrawingFrameBase,
	public gmpi::TimerClient
{
protected:
	UpdateRegionWinGdi updateRegion_native;
	bool isTrackingMouse = false;
	gmpi::drawing::Point cubaseBugPreviousMouseMove = { -1,-1 };

public:
	virtual ~DxDrawingFrameHwnd()
	{
		stopTimer();
	}

	// Default message handler. Note that some clients provide their own
	// (e.g. MyFrameWndDirectX wraps it).
	LRESULT WindowProc(
		HWND hwnd,
		UINT message,
		WPARAM wParam,
		LPARAM lParam);

	// Swapchain creation for native HWND owners — uses CreateSwapChainForHwnd.
	// HostedView (and any future SwapChainPanel host) overrides this with
	// CreateSwapChainForComposition.
	HRESULT createNativeSwapChain
	(
		IDXGIFactory2* factory,
		ID3D11Device* d3dDevice,
		DXGI_SWAP_CHAIN_DESC1* desc,
		IDXGISwapChain1** returnSwapChain
	) override;

	void OnPaint();
	bool onTimer() override;

	// tempSharedD2DBase pure virtual — full surface bounds via GetClientRect.
	gmpi::drawing::RectL getFullDirtyRect() override;
};

// This is used in GMPI VST3. Native HWND window frame.
class DrawingFrame : public DxDrawingFrameHwnd
{
protected:
	HWND windowHandle = {};
	HWND parentWnd = {};

public:

	HWND getWindowHandle() override
	{
		return windowHandle;
	}

	void open(void* pParentWnd, const gmpi::drawing::SizeL* overrideSize = {});
	void reSize(int left, int top, int right, int bottom);
	virtual void doClose() {}

};


namespace win32
{
inline gmpi::drawing::Point pointFromLParam(LPARAM lParam, const gmpi::drawing::Matrix3x2& windowToDips)
{
	gmpi::drawing::Point point{ static_cast<float>(GET_X_LPARAM(lParam)), static_cast<float>(GET_Y_LPARAM(lParam)) };
	return transformPoint(windowToDips, point);
}

inline gmpi::drawing::Point pointFromScreenLParam(HWND hwnd, LPARAM lParam, const gmpi::drawing::Matrix3x2& windowToDips)
{
	POINT pos = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
	MapWindowPoints(NULL, hwnd, &pos, 1);
	return transformPoint(windowToDips, gmpi::drawing::Point{ static_cast<float>(pos.x), static_cast<float>(pos.y) });
}

inline bool isDuplicateMouseMove(UINT message, const gmpi::drawing::Point& point, gmpi::drawing::Point& previousPoint)
{
	if(message == WM_MOUSEMOVE)
	{
		if(previousPoint == point)
			return true;

		previousPoint = point;
	}
	else
	{
		previousPoint = { -1, -1 };
	}

	return false;
}

inline void beginMouseTracking(HWND hwnd, bool& isTrackingMouse)
{
	if(isTrackingMouse)
		return;

	TRACKMOUSEEVENT tme{};
	tme.cbSize = sizeof(TRACKMOUSEEVENT);
	tme.dwFlags = TME_LEAVE;
	tme.hwndTrack = hwnd;

	if(::TrackMouseEvent(&tme))
	{
		isTrackingMouse = true;
	}
}

inline int32_t makePointerFlags(UINT message)
{
	int32_t flags = static_cast<int32_t>(gmpi::api::PointerFlags::InContact)
	              | static_cast<int32_t>(gmpi::api::PointerFlags::Primary)
	              | static_cast<int32_t>(gmpi::api::PointerFlags::Confidence);

	switch(message)
	{
	case WM_MBUTTONDOWN:
	case WM_LBUTTONDOWN:
	case WM_RBUTTONDOWN:
		flags |= static_cast<int32_t>(gmpi::api::PointerFlags::New);
		break;
	}

	switch(message)
	{
	case WM_LBUTTONUP:
	case WM_LBUTTONDOWN:
		flags |= static_cast<int32_t>(gmpi::api::PointerFlags::FirstButton);
		break;
	case WM_RBUTTONDOWN:
	case WM_RBUTTONUP:
		flags |= static_cast<int32_t>(gmpi::api::PointerFlags::SecondButton);
		break;
	case WM_MBUTTONDOWN:
	case WM_MBUTTONUP:
		flags |= static_cast<int32_t>(gmpi::api::PointerFlags::ThirdButton);
		break;
	}

	if(GetKeyState(VK_SHIFT) < 0)
	{
		flags |= static_cast<int32_t>(gmpi::api::PointerFlags::KeyShift);
	}
	if(GetKeyState(VK_CONTROL) < 0)
	{
		flags |= static_cast<int32_t>(gmpi::api::PointerFlags::KeyControl);
	}
	if(GetKeyState(VK_MENU) < 0)
	{
		flags |= static_cast<int32_t>(gmpi::api::PointerFlags::KeyAlt);
	}

	return flags;
}

inline int32_t makeWheelFlags(UINT message, WPARAM wParam)
{
	int32_t flags = static_cast<int32_t>(gmpi::api::PointerFlags::Primary)
	              | static_cast<int32_t>(gmpi::api::PointerFlags::Confidence);

	if(WM_MOUSEHWHEEL == message)
		flags |= static_cast<int32_t>(gmpi::api::PointerFlags::ScrollHoriz);

	const auto fwKeys = GET_KEYSTATE_WPARAM(wParam);
	if(MK_SHIFT & fwKeys)
	{
		flags |= static_cast<int32_t>(gmpi::api::PointerFlags::KeyShift);
	}
	if(MK_CONTROL & fwKeys)
	{
		flags |= static_cast<int32_t>(gmpi::api::PointerFlags::KeyControl);
	}

	return flags;
}

struct hasWindowProc
{
	virtual LRESULT WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) = 0;
};

class PlatformKeyListener : public gmpi::api::IKeyListener, hasWindowProc
{
	HWND parentWnd{};
	HWND windowHandle{};
	gmpi::drawing::Rect bounds{};
	gmpi::api::IKeyListenerCallback* callback2{};

	LRESULT WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) override;

public:
	PlatformKeyListener(HWND pParentWnd, const gmpi::drawing::Rect* r) :
		parentWnd(pParentWnd)
		, bounds(*r)
	{}
	~PlatformKeyListener();

	ReturnCode showAsync(gmpi::api::IUnknown* callback) override;

	GMPI_QUERYINTERFACE_METHOD(gmpi::api::IKeyListener);
	GMPI_REFCOUNT
};
} // namespace win32
} // namespace.
} // namespace.

#endif // skip compilation on macOS
