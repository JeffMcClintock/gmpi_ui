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
		directx::ComPtr<::IDXGISwapChain2> swapChain;
		directx::ComPtr<::ID2D1DeviceContext> d2dDeviceContext;
		gmpi::drawing::SizeL swapChainSize = {};
		inline static bool m_disable_gpu = false;
		bool reentrant = false;
		bool lowDpiMode = {};
		bool firstPresent = false;

		// override these please.
		virtual HWND getWindowHandle() = 0;

		// to help re-create device when lost.
		void ReleaseDevice()
		{
			d2dDeviceContext = nullptr;
			swapChain = nullptr;
		}
		HMODULE getDllHandle();
	};

	// Base class for DrawingFrame (VST3 Plugins) and MyFrameWndDirectX (SynthEdit 1.4+ Panel View).
	class DxDrawingFrameBase :
		public DrawingFrameCommon,
		public tempSharedD2DBase,
		public gmpi::api::IDrawingHost,
		public gmpi::api::IInputHost,
		public gmpi::api::IDialogHost,
		public gmpi::TimerClient
	{
		std::chrono::time_point<std::chrono::steady_clock> frameCountTime;
		UpdateRegionWinGdi updateRegion_native;
		std::unique_ptr<gmpi::directx::GraphicsContext_base> context;

	protected:
		gmpi::shared_ptr<gmpi::api::IGraphicsRedrawClient> frameUpdateClient;

#ifdef GMPI_HOST_POINTER_SUPPORT
		gmpi::shared_ptr<gmpi_gui_api::IMpGraphics3> gmpi_gui_client; // usually a ContainerView at the topmost level
		gmpi::shared_ptr<gmpi_gui_api::IMpKeyClient> gmpi_key_client;
#endif

		// Paint() uses Direct-2d which block on vsync. Therefore all invalid rects should be applied in one "hit", else windows message queue chokes calling WM_PAINT repeately and blocking on every rect.
		std::vector<gmpi::drawing::RectL> backBufferDirtyRects;
		gmpi::drawing::Matrix3x2 viewTransform;
		gmpi::drawing::Matrix3x2 DipsToWindow;
		gmpi::drawing::Matrix3x2 WindowToDips;
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
			, DrawingFactory(nullptr)
		{
		}

		virtual ~DxDrawingFrameBase()
		{
			stopTimer();

#ifdef GMPI_HOST_POINTER_SUPPORT
			// Free GUI objects first so they can release fonts etc before releasing factorys.
			gmpi_gui_client = nullptr;
#endif

			ReleaseDevice();
		}

		// provids a default message handler. Note that some clients provide their own. e.g. MyFrameWndDirectX
		LRESULT WindowProc(
			HWND hwnd,
			UINT message,
			WPARAM wParam,
			LPARAM lParam);

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

		void CreateSwapPanel();
		void CreateDeviceSwapChainBitmap();

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
#ifdef GMPI_HOST_POINTER_SUPPORT
			pcontainerView->queryInterface(&gmpi_gui_api::IMpGraphics3::guid, gmpi_gui_client.put());
			pcontainerView->queryInterface(&gmpi_gui_api::IMpKeyClient::guid, gmpi_key_client.put());
#endif

#if 0
			gmpi::shared_ptr<gmpi::api::IUserInterface2> pinHost;
			gmpi_gui_client->queryInterface(&gmpi::MP_IID_GUI_PLUGIN2, pinHost.put());

			if(pinHost)
				pinHost->setHost(static_cast<gmpi_gui::IMpGraphicsHost*>(this));
#endif
			if (drawingClient)
			{
				drawingClient->open(static_cast<gmpi::api::IDrawingHost*>(this));
			}
		}

		void OnPaint();
#if 0
		// Inherited via IMpUserInterfaceHost2
		int32_t pinTransmit(int32_t pinId, int32_t size, const void * data, int32_t voice = 0) override;
		int32_t createPinIterator(gmpi::api::IPinIterator** returnIterator) override;
		int32_t getHandle(int32_t & returnValue) override;
		int32_t sendMessageToAudio(int32_t id, int32_t size, const void * messageData) override;
		int32_t ClearResourceUris() override;
		int32_t RegisterResourceUri(const char * resourceName, const char * resourceType, gmpi::IString* returnString) override;
		int32_t OpenUri(const char * fullUri, gmpi::IProtectedFile2** returnStream) override;
		int32_t FindResourceU(const char * resourceName, const char * resourceType, gmpi::IString* returnString) override;
		int32_t LoadPresetFile_DEPRECATED(const char* presetFilePath) override
		{
//			Presenter()->LoadPresetFile(presetFilePath);
			return gmpi::MP_FAIL;
		}
#endif
		// IMpGraphicsHost
		virtual void invalidateRect(const gmpi::drawing::Rect * invalidRect) override;
#ifdef GMPI_HOST_POINTER_SUPPORT
		virtual void invalidateMeasure() override;
		int32_t setCapture() override;
		int32_t getCapture(int32_t & returnValue) override;
		int32_t releaseCapture() override;
		gmpi::ReturnCode GetDrawingFactory(gmpi::drawing::IFactory** returnFactory) override
		{
			*returnFactory = &DrawingFactory;
			return gmpi::ReturnCode::Ok;
		}
#endif
		gmpi::ReturnCode getDrawingFactory(gmpi::api::IUnknown** returnFactory) override
		{
			*returnFactory = &DrawingFactory;
			return gmpi::ReturnCode::Ok;
		}
#ifdef GMPI_HOST_POINTER_SUPPORT

		int32_t createPlatformMenu(gmpi::drawing::Rect* rect, gmpi_gui::IMpPlatformMenu** returnMenu) override;
		int32_t createPlatformTextEdit(gmpi::drawing::Rect* rect, gmpi_gui::IMpPlatformText** returnTextEdit) override;
		int32_t createFileDialog(int32_t dialogType, gmpi_gui::IMpFileDialog** returnFileDialog) override;
		int32_t createOkCancelDialog(int32_t dialogType, gmpi_gui::IMpOkCancelDialog** returnDialog) override;
#endif
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

#if 1//def GMPI_HOST_POINTER_SUPPORT
		// IUnknown methods
		gmpi::ReturnCode queryInterface(const gmpi::api::Guid* iid, void** returnInterface) override
		{
			*returnInterface = {};

			if (*iid == IDrawingHost::guid)
			{
				// important to cast to correct vtable (ug_plugin3 has 2 vtables) before reinterpret cast
				*returnInterface = reinterpret_cast<void*>(static_cast<IDrawingHost*>(this));
				addRef();
				return gmpi::ReturnCode::Ok;
			}
			if (*iid == IInputHost::guid)
			{
				// important to cast to correct vtable (ug_plugin3 has 2 vtables) before reinterpret cast
				*returnInterface = reinterpret_cast<void*>(static_cast<IInputHost*>(this));
				addRef();
				return gmpi::ReturnCode::Ok;
			}
			if (*iid == IDialogHost::guid)
			{
				*returnInterface = reinterpret_cast<void*>(static_cast<IDialogHost*>(this));
				addRef();
				return gmpi::ReturnCode::Ok;
			}
			if (*iid == gmpi::api::IUnknown::guid)
			{
				*returnInterface = this;
				addRef();
				return gmpi::ReturnCode::Ok;
			}

			if (parameterHost)
			{
				return parameterHost->queryInterface(iid, returnInterface);
			}

			return gmpi::ReturnCode::NoSupport;
		}

		GMPI_REFCOUNT_NO_DELETE;

#ifdef GMPI_HOST_POINTER_SUPPORT
		auto getClient() {
			return gmpi_gui_client;
		}
#endif
#endif

		void initTooltip();
		void TooltipOnMouseActivity();
		void ShowToolTip();
		void HideToolTip();
		void OnSize(UINT width, UINT height);
		bool onTimer() override;
		virtual void autoScrollStart() {}
		virtual void autoScrollStop() {}
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
