#pragma once

/*
#include "backends/DrawingFrameWin.h"
*/

#if defined(_WIN32) // skip compilation on macOS

#include <vector>
#include <string>
#include <memory>
#include <chrono>
#include <dxgi1_6.h>
#include <d3d11_1.h>
#include <dwmapi.h>
#include "DirectXGfx.h"
#include "helpers/Timer.h"
#include "DrawingFrameCommon.h"

namespace gmpi
{

namespace hosting
{
	// utility functions
	std::wstring RegisterWindowsClass(HINSTANCE dllHandle, WNDPROC windowProc);
	HWND CreateHostingWindow(HMODULE dllHandle, const std::wstring& windowClass, HWND parentWnd, RECT r, LONG_PTR userData);

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
		for (auto& path : pathInfo)
		{
			const int idx = path.sourceInfo.modeInfoIdx;

			if (idx == DISPLAYCONFIG_PATH_MODE_IDX_INVALID)
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
			if (intersectArea <= bestIntersectArea)
				continue;

			// Get monitor handle for this path's target
			targetName.header.adapterId = path.targetInfo.adapterId;
			white_level.header.adapterId = path.targetInfo.adapterId;
			targetName.header.id = path.targetInfo.id;
			white_level.header.id = path.targetInfo.id;

			if (DisplayConfigGetDeviceInfo(&targetName.header) != ERROR_SUCCESS)
				continue;

			if (DisplayConfigGetDeviceInfo(&white_level.header) != ERROR_SUCCESS)
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
	struct tempSharedD2DBase : public gmpi::api::IDrawingHost
	{
		enum { FallBack_8bit, Fallback_Software };

		// https://learn.microsoft.com/en-us/windows/win32/direct3darticles/high-dynamic-range
		const DXGI_FORMAT bestFormat = DXGI_FORMAT_R16G16B16A16_FLOAT; // Proper gamma-correct blending.
		const DXGI_FORMAT fallbackFormat = DXGI_FORMAT_B8G8R8A8_UNORM; // shitty linear blending.

		gmpi::drawing::Matrix3x2 viewTransform;
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
		inline static int m_fallbackStrategy = Fallback_Software;
		bool reentrant = false;
		bool lowDpiMode = {};
		bool firstPresent = false;
		float windowWhiteLevel{};
		bool monitorChanged = false;
		std::function<void()> clientInvalidated; // called from Paint if monitor white-level changed since last check.

		virtual float calcWhiteLevel() = 0;

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

		void recreateSwapChainAndClientAsync();
		void setWhiteLevel(float newWhiteLevel);
		void CreateSwapPanel(ID2D1Factory1* d2dFactory);
		void CreateDeviceSwapChainBitmap();
		void OnSize(UINT width, UINT height);
		virtual void OnSwapChainCreated() = 0;
		virtual void sizeClientDips(float width, float height) = 0;

		// to help re-create device when lost.
		void ReleaseDevice()
		{
			if (hdrWhiteScaleEffect)
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
	};

	// Base class for JuceDrawingFrameBase, DrawingFrame (VST3 Plugins) and MyFrameWndDirectX (SynthEdit 1.4+ Panel View).
	class DxDrawingFrameBase :
		public tempSharedD2DBase,
		public DrawingFrameCommon,
//		public gmpi::api::IDrawingHost,
		public gmpi::api::IInputHost,
		public gmpi::api::IDialogHost,
		public gmpi::TimerClient
	{
		std::chrono::time_point<std::chrono::steady_clock> frameCountTime;
		UpdateRegionWinGdi updateRegion_native;

	protected:
		gmpi::shared_ptr<gmpi::api::IGraphicsRedrawClient> frameUpdateClient;

		// Paint() uses Direct-2d which block on vsync. Therefore all invalid rects should be applied in one "hit", else windows message queue chokes calling WM_PAINT repeately and blocking on every rect.
		std::vector<gmpi::drawing::RectL> backBufferDirtyRects;
		int toolTiptimer = 0;
		bool toolTipShown;
		HWND tooltipWindow;
		static const int toolTiptimerInit = 40; // x/60 Hz
		std::wstring toolTipText;
		bool isTrackingMouse = false;
		gmpi::drawing::Point cubaseBugPreviousMouseMove = { -1,-1 };

	public:
		static const int viewDimensions = 7968; // DIPs (divisible by grids 60x60 + 2 24 pixel borders)
		gmpi::directx::Factory DrawingFactory;

		DxDrawingFrameBase() :
			 toolTipShown(false)
			, tooltipWindow(0)
		{
		}

		virtual ~DxDrawingFrameBase()
		{
			stopTimer();

			ReleaseDevice();
		}

		// provids a default message handler. Note that some clients provide their own. e.g. MyFrameWndDirectX
		LRESULT WindowProc(
			HWND hwnd,
			UINT message,
			WPARAM wParam,
			LPARAM lParam);

		HRESULT createNativeSwapChain
		(
			IDXGIFactory2* factory,
			ID3D11Device* d3dDevice,
			DXGI_SWAP_CHAIN_DESC1* desc,
			IDXGISwapChain1** returnSwapChain
		) override;

		void OnSwapChainCreated() override;

		void setFallbackHost(gmpi::api::IUnknown* paramHost)
		{
			parameterHost = paramHost;
		}

		void attachClient(gmpi::api::IUnknown* gfx);
		void detachAndRecreate();

		void OnPaint();

		// IMpGraphicsHost
		void invalidateRect(const gmpi::drawing::Rect * invalidRect) override;
		float getRasterizationScale() override; // DPI scaling
		gmpi::ReturnCode getDrawingFactory(gmpi::api::IUnknown** returnFactory) override
		{
			*returnFactory = &DrawingFactory;
			return gmpi::ReturnCode::Ok;
		}

		// IInputHost
		gmpi::ReturnCode setCapture() override;
		gmpi::ReturnCode getCapture(bool& returnValue) override;
		gmpi::ReturnCode releaseCapture() override;

		gmpi::ReturnCode getFocus() override;
		gmpi::ReturnCode releaseFocus() override;

		// IDialogHost
		gmpi::ReturnCode createTextEdit(const gmpi::drawing::Rect* r, gmpi::api::IUnknown** returnTextEdit) override;
		gmpi::ReturnCode createPopupMenu(const gmpi::drawing::Rect* r, gmpi::api::IUnknown** returnMenu) override;
		gmpi::ReturnCode createKeyListener(const gmpi::drawing::Rect* r, gmpi::api::IUnknown** returnKeyListener) override;
		gmpi::ReturnCode createFileDialog(int32_t dialogType, gmpi::api::IUnknown** returnMenu) override;
		gmpi::ReturnCode createStockDialog(int32_t dialogType, gmpi::api::IUnknown** returnDialog) override;

		// IUnknown methods
		gmpi::ReturnCode queryInterface(const gmpi::api::Guid* iid, void** returnInterface) override
		{
			*returnInterface = {};

			GMPI_QUERYINTERFACE(IDrawingHost);
			GMPI_QUERYINTERFACE(IInputHost);
			GMPI_QUERYINTERFACE(IDialogHost);
			GMPI_QUERYINTERFACE(IInputHost);

			if (parameterHost)
			{
				return parameterHost->queryInterface(iid, returnInterface);
			}

			return gmpi::ReturnCode::NoSupport;
		}

		GMPI_REFCOUNT_NO_DELETE;

		void initTooltip();
		void TooltipOnMouseActivity();
		void ShowToolTip();
		void HideToolTip();
		void sizeClientDips(float width, float height) override;
		bool onTimer() override;
		virtual void autoScrollStart() {}
		virtual void autoScrollStop() {}
	};

	// This is used in GMPI VST3. Native HWND window frame.
	class DrawingFrame : public DxDrawingFrameBase
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

		// tempSharedD2DBase
		float calcWhiteLevel() override;
	};


namespace win32
{
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
