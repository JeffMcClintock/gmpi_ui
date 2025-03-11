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

	// stuff we can share between GMPI-UI DxDrawingFrameBase and SynthEditlib DrawingFrameBase2
	struct tempSharedD2DBase
	{
		enum { FallBack_8bit, Fallback_Software };

		gmpi::drawing::Matrix3x2 viewTransform;
		gmpi::drawing::Matrix3x2 DipsToWindow;
		gmpi::drawing::Matrix3x2 WindowToDips;

		// HDR support. Above swapChain so these get released first.
		gmpi::directx::ComPtr<ID2D1Effect> hdrWhiteScaleEffect;
		gmpi::directx::ComPtr<ID2D1BitmapRenderTarget> hdrRenderTarget;
		gmpi::directx::ComPtr<ID2D1Bitmap> hdrBitmap;

		directx::ComPtr<::IDXGISwapChain2> swapChain;
		directx::ComPtr<::ID2D1DeviceContext> d2dDeviceContext;

		gmpi::drawing::SizeL swapChainSize = {};
		inline static bool m_disable_gpu = false;
		inline static int m_fallbackStrategy = Fallback_Software;
		bool reentrant = false;
		bool lowDpiMode = {};
		bool firstPresent = false;

		// override these please.
		virtual HWND getWindowHandle() = 0;
		virtual float getRasterizationScale() = 0; // DPI scaling
		virtual HRESULT createNativeSwapChain
		(
			IDXGIFactory2* factory,
			ID3D11Device* d3dDevice,
			DXGI_SWAP_CHAIN_DESC1* desc,
			IDXGISwapChain1** returnSwapChain
		) = 0;

		void CreateSwapPanel(ID2D1Factory1* d2dFactory);
		void CreateDeviceSwapChainBitmap();
		virtual void OnSwapChainCreated(bool useDeepColor) = 0;

		// to help re-create device when lost.
		void ReleaseDevice()
		{
			if (hdrWhiteScaleEffect)
			{
				hdrWhiteScaleEffect->SetInput(0, nullptr);
				hdrWhiteScaleEffect = nullptr;
			}
			hdrBitmap = nullptr;
			hdrRenderTarget = nullptr;
			d2dDeviceContext = nullptr;
			swapChain = nullptr;
		}
		HMODULE getDllHandle();
	};

	// Base class for DrawingFrame (VST3 Plugins) and MyFrameWndDirectX (SynthEdit 1.4+ Panel View).
	class DxDrawingFrameBase :
		public tempSharedD2DBase,
		public DrawingFrameCommon,
		public gmpi::api::IDrawingHost,
		public gmpi::api::IInputHost,
		public gmpi::api::IDialogHost,
		public gmpi::TimerClient
	{
		std::chrono::time_point<std::chrono::steady_clock> frameCountTime;
		UpdateRegionWinGdi updateRegion_native;
//		std::unique_ptr<gmpi::directx::GraphicsContext_base> context;

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

		void OnSwapChainCreated(bool useDeepColor) override;

		void ResizeSwapChainBitmap()
		{
			d2dDeviceContext->SetTarget(nullptr);
			if (S_OK == swapChain->ResizeBuffers(0,
				0, 0,
				DXGI_FORMAT_UNKNOWN,
				0))
			{
				CreateDeviceSwapChainBitmap();
			}
			else
			{
				ReleaseDevice();
			}
		}

		void setFallbackHost(gmpi::api::IUnknown* paramHost)
		{
			parameterHost = paramHost;
		}

		void attachClient(gmpi::api::IUnknown* gfx)
		{
			gfx->queryInterface(&gmpi::api::IDrawingClient::guid, drawingClient.put_void());
			gfx->queryInterface(&gmpi::api::IInputClient::guid, inputClient.put_void());
			
			// legacy
			gfx->queryInterface(&gmpi::api::IGraphicsRedrawClient::guid, frameUpdateClient.put_void());
			if (drawingClient)
			{
				drawingClient->open(static_cast<gmpi::api::IDrawingHost*>(this));
			}
		}

		void OnPaint();

		// IMpGraphicsHost
		virtual void invalidateRect(const gmpi::drawing::Rect * invalidRect) override;
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
		gmpi::ReturnCode createTextEdit(gmpi::api::IUnknown** returnTextEdit) override;
		gmpi::ReturnCode createKeyListener(gmpi::api::IUnknown** returnKeyListener) override;
		gmpi::ReturnCode createPopupMenu(gmpi::api::IUnknown** returnMenu) override;
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
		void OnSize(UINT width, UINT height);
		bool onTimer() override;
		virtual void autoScrollStart() {}
		virtual void autoScrollStop() {}
		float getRasterizationScale() override; // DPI scaling
	};

	// This is used in VST3. Native HWND window frame.
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
	};
} // namespace.
} // namespace.

#endif // skip compilation on macOS
