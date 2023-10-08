#pragma once

/*
#include "modules/se_sdk3_hosting/DrawingFrame_win32.h"
using namespace GmpiGuiHosting;
*/

#if defined(_WIN32) // skip compilation on macOS

#include <vector>
#include <string>
#include <memory>
#include <chrono>
#include <d3d11_1.h>
#include "DirectXGfx.h"
#include "../se_sdk3/TimerManager.h"
//#include "../se_sdk3/mp_sdk_gui2.h"
//#include "../se_sdk3_hosting/gmpi_gui_hosting.h"
#include "./gmpi_gui_hosting.h"
#include "../se_sdk3_hosting/GraphicsRedrawClient.h"

namespace SynthEdit2
{
	class IPresenter;
}

namespace GmpiGuiHosting
{
	// Base class for DrawingFrame (VST3 Plugins) and MyFrameWndDirectX (SynthEdit 1.4+ Panel View).
	class DrawingFrameBase :
#ifdef GMPI_HOST_POINTER_SUPPORT
		public gmpi_gui::IMpGraphicsHost,
#endif
		public IDrawingHost,
		/*public gmpi::api::IUserInterfaceHost2,*/
		public TimerClient
	{
		std::chrono::time_point<std::chrono::steady_clock> frameCountTime;
		bool firstPresent = false;
		UpdateRegionWinGdi updateRegion_native;
		std::unique_ptr<gmpi::directx::GraphicsContext_base> context;

	protected:
		gmpi::shared_ptr<IGraphicsRedrawClient> frameUpdateClient;
		gmpi::shared_ptr<IDrawingClient> drawingClient;
		gmpi::shared_ptr<IInputClient> inputClient;
		gmpi::drawing::SizeL swapChainSize = {};

		ID2D1DeviceContext* mpRenderTarget = {};
		IDXGISwapChain1* m_swapChain = {};

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
		bool reentrant;
		bool lowDpiMode = {};
		bool isTrackingMouse = false;
		gmpi::drawing::Point cubaseBugPreviousMouseMove = { -1,-1 };

	public:
		static const int viewDimensions = 7968; // DIPs (divisible by grids 60x60 + 2 24 pixel borders)
		gmpi::directx::Factory DrawingFactory;

		DrawingFrameBase() :
			mpRenderTarget(nullptr)
			,m_swapChain(nullptr)
			, toolTipShown(false)
			, tooltipWindow(0)
			, reentrant(false)
		{
			DrawingFactory.Init();
		}

		virtual ~DrawingFrameBase()
		{
			StopTimer();

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

		// to help re-create device when lost.
		void ReleaseDevice()
		{
			if (mpRenderTarget)
				mpRenderTarget->Release();
			if (m_swapChain)
				m_swapChain->Release();

			mpRenderTarget = nullptr;
			m_swapChain = nullptr;
		}

		void ResizeSwapChainBitmap()
		{
			mpRenderTarget->SetTarget(nullptr);
			if (S_OK == m_swapChain->ResizeBuffers(0,
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

		void CreateDevice();
		void CreateDeviceSwapChainBitmap();

		void AddView(gmpi::api::IUnknown* pcontainerView)
		{
			pcontainerView->queryInterface(/*gmpi_gui_api::*/&IDrawingClient::guid, drawingClient.asIMpUnknownPtr());
			if(drawingClient)
			{
				drawingClient->open(this);// static_cast<gmpi_gui::IMpGraphicsHost*>(this));
			}
			pcontainerView->queryInterface(&IInputClient::guid, inputClient.asIMpUnknownPtr());
			
			// legacy
			pcontainerView->queryInterface(&IGraphicsRedrawClient::guid, frameUpdateClient.asIMpUnknownPtr());
#ifdef GMPI_HOST_POINTER_SUPPORT
			pcontainerView->queryInterface(&gmpi_gui_api::IMpGraphics3::guid, gmpi_gui_client.asIMpUnknownPtr());
			pcontainerView->queryInterface(&gmpi_gui_api::IMpKeyClient::guid, gmpi_key_client.asIMpUnknownPtr());
#endif

#if 0
			gmpi::shared_ptr<gmpi::api::IUserInterface2> pinHost;
			gmpi_gui_client->queryInterface(&gmpi::MP_IID_GUI_PLUGIN2, pinHost.asIMpUnknownPtr());

			if(pinHost)
				pinHost->setHost(static_cast<gmpi_gui::IMpGraphicsHost*>(this));
#endif
		}

		void OnPaint();
		virtual HWND getWindowHandle() = 0;
		void CreateRenderTarget();
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
#if 0
			if (*iid == gmpi::MP_IID_UI_HOST2)
			{
				// important to cast to correct vtable (ug_plugin3 has 2 vtables) before reinterpret cast
				*returnInterface = reinterpret_cast<void*>(static_cast<IMpUserInterfaceHost2*>(this));
				addRef();
				return gmpi::ReturnCode::Ok;
			}
#endif
#ifdef GMPI_HOST_POINTER_SUPPORT
			if (*iid == gmpi_gui::SE_IID_GRAPHICS_HOST || iid == gmpi_gui::SE_IID_GRAPHICS_HOST_BASE || iid == gmpi::MP_IID_UNKNOWN)
			{
				// important to cast to correct vtable (ug_plugin3 has 2 vtables) before reinterpret cast
				*returnInterface = reinterpret_cast<void*>(static_cast<IMpGraphicsHost*>(this));
				addRef();
				return gmpi::ReturnCode::Ok;
			}
#else
			if (*iid == gmpi::api::IUnknown::guid)
			{
				*returnInterface = this;
				addRef();
				return gmpi::ReturnCode::Ok;
			}
#endif
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
		bool OnTimer() override;
		virtual void autoScrollStart() {}
		virtual void autoScrollStop() {}
	};

	// This is used in VST3. Native HWND window frame.
	class DrawingFrame : public DrawingFrameBase
	{
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

#endif // skip compilation on macOS
