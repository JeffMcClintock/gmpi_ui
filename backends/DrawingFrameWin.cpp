#if defined(_WIN32) // skip compilation on macOS

#include <d2d1_2.h>
#include <d3d11_1.h>
#include <wrl.h> // Comptr
#include <commctrl.h>
#include <shobjidl.h>
#include "./DrawingFrameWin.h"

using namespace std;
using namespace gmpi;
using namespace gmpi::drawing;
using namespace D2D1;

namespace gmpi
{
namespace hosting
{

std::wstring RegisterWindowsClass(HINSTANCE dllHandle, WNDPROC windowProc)
{
	static WNDCLASS windowClass{};
	static wchar_t gClassName[20] = {};

	if (!windowClass.lpfnWndProc)
	{
		::OleInitialize(0);

		swprintf(gClassName, std::size(gClassName), L"GMPIUI%p", dllHandle);

		windowClass.style = CS_GLOBALCLASS;
		windowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
		windowClass.hInstance = dllHandle;
		windowClass.lpfnWndProc = windowProc;
		windowClass.lpszClassName = gClassName;

		RegisterClass(&windowClass);
	}

	return gClassName;
}

HWND CreateHostingWindow(HMODULE dllHandle, const std::wstring& windowClass, HWND parentWnd, RECT r, LONG_PTR userData)
{
	int style = WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS;// | WS_OVERLAPPEDWINDOW;
	int extended_style = 0;

	const auto windowHandle = CreateWindowEx(extended_style, windowClass.c_str(), L"",
		style, 0, 0, r.right - r.left, r.bottom - r.top,
		parentWnd, NULL, dllHandle, NULL);

	if (windowHandle)
	{
		SetWindowLongPtr(windowHandle, GWLP_USERDATA, (__int3264) userData);
		// RegisterDragDrop(windowHandle, new CDropTarget(this));
	}

	return windowHandle;
}

void DxDrawingFrameBase::doContextMenu(gmpi::drawing::Point point, int32_t flags)
{
	auto r = inputClient->onPointerDown(point, flags);

	// Handle right-click on background. (right-click on objects is handled by object itself).
	if (r == gmpi::ReturnCode::Unhandled && (flags & static_cast<int32_t>(gmpi::api::PointerFlags::SecondButton)) != 0 && inputClient)
	{
		gmpi::drawing::Rect rect{ point.x, point.y, point.x + 120, point.y + 20 };

		// create menu popup
		gmpi::shared_ptr<gmpi::api::IUnknown> unknown;
		createPopupMenu(&rect, unknown.put());
		contextMenu = unknown.as<gmpi::api::IPopupMenu>();

		// populate menu
		r = inputClient->populateContextMenu(point, contextMenu.get());

		// show menu
		contextMenu->showAsync();
	}
}

void UpdateRegionWinGdi::copyDirtyRects(HWND window, gmpi::drawing::SizeL swapChainSize)
{
	rects.clear();

	auto regionType = GetUpdateRgn(
		window,
		hRegion,
		FALSE
	);

	assert(regionType != RGN_ERROR);

	if (regionType != NULLREGION)
	{
		int size = GetRegionData(hRegion, 0, NULL); // query size of region data.
		if (size)
		{
			regionDataBuffer.resize(size);
			RGNDATA* pRegion = (RGNDATA*)regionDataBuffer.data();

			GetRegionData(hRegion, size, pRegion);

			// Overall bounding rect
			{
				auto& r = pRegion->rdh.rcBound;
				bounds = { r.left, r.top, r.right, r.bottom };
			}

			const RECT* pRect = (const RECT*)&pRegion->Buffer;
			for (unsigned i = 0; i < pRegion->rdh.nCount; i++)
			{
				gmpi::drawing::RectL r{ pRect[i].left, pRect[i].top, pRect[i].right, pRect[i].bottom };

				// Direct 2D will fail if any rect outside swapchain bitmap area.
				const auto res = intersectRect(r, { 0, 0, swapChainSize.width, swapChainSize.height });

				if (!empty(res))
				{
					rects.push_back(res);
				}
			}
		}
		optimizeRects();
	}
}

void UpdateRegionWinGdi::optimizeRects()
{
	for (int i1 = 0; i1 < rects.size(); ++i1)
	{
		gmpi::drawing::RectL r1(rects[i1]);
		auto area1 = getWidth(r1) * getHeight(r1);

		for (int i2 = i1 + 1; i2 < rects.size(); )
		{
			gmpi::drawing::RectL r2(rects[i2]);
			auto area2 = getWidth(r2) * getHeight(r2);

			gmpi::drawing::RectL unionrect(rects[i1]);

			unionrect.top = (std::min)(unionrect.top, rects[i2].top);
			unionrect.bottom = (std::max)(unionrect.bottom, rects[i2].bottom);
			unionrect.left = (std::min)(unionrect.left, rects[i2].left);
			unionrect.right = (std::max)(unionrect.right, rects[i2].right);

			auto unionarea = getWidth(unionrect) * getHeight(unionrect);

			if (unionarea <= area1 + area2)
			{
				rects[i1] = unionrect;
				area1 = unionarea;
				rects.erase(rects.begin() + i2);
			}
			else
			{
				++i2;
			}
		}
	}
}

UpdateRegionWinGdi::UpdateRegionWinGdi()
{
	hRegion = ::CreateRectRgn(0, 0, 0, 0);
}

UpdateRegionWinGdi::~UpdateRegionWinGdi()
{
	if (hRegion)
		DeleteObject(hRegion);
}

void DirtyRectQueue::add(gmpi::drawing::RectL rect)
{
	const auto width = rect.right - rect.left;
	const auto height = rect.bottom - rect.top;
	if (width <= 0 || height <= 0)
		return;

	const auto area1 = width * height;

	for (auto& dirtyRect : rects)
	{
		const auto area2 = getWidth(dirtyRect) * getHeight(dirtyRect);

		gmpi::drawing::RectL unionrect(dirtyRect);
		unionrect.top = (std::min)(unionrect.top, rect.top);
		unionrect.bottom = (std::max)(unionrect.bottom, rect.bottom);
		unionrect.left = (std::min)(unionrect.left, rect.left);
		unionrect.right = (std::max)(unionrect.right, rect.right);

		const auto unionarea = getWidth(unionrect) * getHeight(unionrect);
		if (unionarea <= area1 + area2)
		{
			dirtyRect = unionrect;
			return;
		}
	}

	rects.push_back(rect);
}

void DirtyRectQueue::add(const gmpi::drawing::Rect* invalidRect, const gmpi::drawing::Matrix3x2& dipsToWindow)
{
	if (!invalidRect)
		return;

	const auto actualRect = *invalidRect * dipsToWindow;
	add({
		static_cast<int32_t>(floorf(actualRect.left)),
		static_cast<int32_t>(floorf(actualRect.top)),
		static_cast<int32_t>(ceilf(actualRect.right)),
		static_cast<int32_t>(ceilf(actualRect.bottom))
	});
}

void DirtyRectQueue::replace(gmpi::drawing::RectL rect)
{
	rects.clear();
	add(rect);
}

void ToolTipManager::init(HMODULE instanceHandle, HWND parentWindow)
{
	if (windowHandle != nullptr || !parentWindow)
		return;

	TOOLINFO ti{};
	HWND hwndTT = CreateWindow(TOOLTIPS_CLASS, TEXT(""),
		WS_POPUP,
		CW_USEDEFAULT, CW_USEDEFAULT,
		CW_USEDEFAULT, CW_USEDEFAULT,
		NULL, (HMENU)NULL, instanceHandle,
		NULL);

	ti.cbSize = TTTOOLINFO_V1_SIZE;
	ti.uFlags = TTF_SUBCLASS;
	ti.hwnd = parentWindow;
	ti.uId = 0;
	ti.hinst = instanceHandle;
	ti.lpszText = const_cast<TCHAR*>(TEXT("This is a tooltip"));
	ti.rect.left = ti.rect.top = ti.rect.bottom = ti.rect.right = 0;

	if (!SendMessage(hwndTT, TTM_ADDTOOL, 0, (LPARAM)&ti))
	{
		DestroyWindow(hwndTT);
		return;
	}

	windowHandle = hwndTT;
}

void ToolTipManager::onMouseActivity(HWND parentWindow)
{
	if (shown)
	{
		if (timer < -20)
		{
			hide(parentWindow);
			timer = timerInit;
		}
	}
	else
	{
		timer = timerInit;
	}
}

void ToolTipManager::show(HWND parentWindow)
{
	auto platformObject = windowHandle;
	if (!platformObject)
		return;

	RECT rc;
	rc.left = 0;
	rc.top = 0;
	rc.right = 100000;
	rc.bottom = 100000;
	TOOLINFO ti{};
	ti.cbSize = TTTOOLINFO_V1_SIZE;
	ti.hwnd = parentWindow;
	ti.uId = 0;
	ti.rect = rc;
	ti.lpszText = (TCHAR*)(const TCHAR*)text.c_str();
	SendMessage(platformObject, TTM_UPDATETIPTEXT, 0, (LPARAM)&ti);
	SendMessage(platformObject, TTM_NEWTOOLRECT, 0, (LPARAM)&ti);
	SendMessage(platformObject, TTM_POPUP, 0, 0);

	shown = true;
}

void ToolTipManager::hide(HWND parentWindow)
{
	shown = false;

	if (!windowHandle)
		return;

	TOOLINFO ti{};
	ti.cbSize = TTTOOLINFO_V1_SIZE;
	ti.hwnd = parentWindow;
	ti.uId = 0;
	ti.lpszText = 0;
	SendMessage(windowHandle, TTM_UPDATETIPTEXT, 0, (LPARAM)&ti);
	SendMessage(windowHandle, TTM_POP, 0, 0);
}

///////////////////////////////////////////////////////////////////////////////////////////

LRESULT CALLBACK DrawingFrameWindowProc(
	HWND hwnd,
	UINT message,
	WPARAM wParam,
	LPARAM lParam)
{
	auto drawingFrame = (DrawingFrame*)(LONG_PTR)GetWindowLongPtr(hwnd, GWLP_USERDATA);
	if (drawingFrame)
	{
		return drawingFrame->WindowProc(hwnd, message, wParam, lParam);
	}

	return DefWindowProc(hwnd, message, wParam, lParam);
}

// copied from MP_GetDllHandle
HMODULE local44_GetDllHandle_randomshit()
{
	HMODULE hmodule = 0;
	GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)&local44_GetDllHandle_randomshit, &hmodule);
	return (HMODULE)hmodule;
}
HMODULE tempSharedD2DBase::getDllHandle()
{
	return local44_GetDllHandle_randomshit();
}

void DrawingFrame::open(void* pParentWnd, const gmpi::drawing::SizeL* overrideSize)
{
	RECT r{};
	if (overrideSize)
	{
		// size to document
		r.right = overrideSize->width;
		r.bottom = overrideSize->height;
	}
	else
	{
		// auto size to parent
		GetClientRect(parentWnd, &r);
	}

	parentWnd = (HWND)pParentWnd;

	const auto windowClass = gmpi::hosting::RegisterWindowsClass(getDllHandle(), DrawingFrameWindowProc);
	windowHandle = gmpi::hosting::CreateHostingWindow(getDllHandle(), windowClass, parentWnd, r, (LONG_PTR) this);

	if (!windowHandle)
		return;

	CreateSwapPanel(DrawingFactory.getD2dFactory());

	initTooltip();

	if (drawingClient)
	{
		const auto scale = 1.0 / getRasterizationScale();

		sizeClientDips(
			static_cast<float>((r.right - r.left) * scale),
			static_cast<float>((r.bottom - r.top) * scale));
	}

	// starting Timer latest to avoid first event getting 'in-between' other init events.
	startTimer(15); // 16.66 = 60Hz. 16ms timer seems to miss v-sync. Faster timers offer no improvement to framerate.
}

float DxDrawingFrameBase::getRasterizationScale()
{
	return GetDpiForWindow(getWindowHandle()) / 96.f;
}

void DxDrawingFrameBase::initTooltip()
{
  tooltip.init(getDllHandle(), getWindowHandle());
}

void DxDrawingFrameBase::TooltipOnMouseActivity()
{
    tooltip.onMouseActivity(getWindowHandle());
}

void DxDrawingFrameBase::ShowToolTip()
{
    tooltip.show(getWindowHandle());
}

void DxDrawingFrameBase::HideToolTip()
{
   tooltip.hide(getWindowHandle());
}

LRESULT DxDrawingFrameBase::WindowProc(
	HWND hwnd, 
	UINT message,
	WPARAM wParam,
	LPARAM lParam)
{
	if(
		!drawingClient
		)
		return DefWindowProc(hwnd, message, wParam, lParam);

	switch (message)
	{
		case WM_MBUTTONDOWN:
		case WM_MBUTTONUP:
		case WM_MOUSEMOVE:
		case WM_LBUTTONDOWN:
		case WM_LBUTTONUP:
		case WM_RBUTTONDOWN:
		case WM_RBUTTONUP:
		{
          Point p = win32::pointFromLParam(lParam, WindowToDips);
			if (win32::isDuplicateMouseMove(message, p, cubaseBugPreviousMouseMove))
			{
				return TRUE;
			}

			TooltipOnMouseActivity();

          int32_t flags = win32::makePointerFlags(message);

			gmpi::ReturnCode r;
			switch (message)
			{
			case WM_MOUSEMOVE:
				{
					r = inputClient->onPointerMove(p, flags);

					// get notified when mouse leaves window
					if (!isTrackingMouse)
					{
                      win32::beginMouseTracking(hwnd, isTrackingMouse);
						inputClient->setHover(true);
					}
				}
				break;

			case WM_LBUTTONDOWN:
			case WM_MBUTTONDOWN:
				r = inputClient->onPointerDown(p, flags);
				::SetFocus(hwnd);
				break;

			case WM_RBUTTONDOWN:
				r = inputClient->onPointerDown(p, flags);
				::SetFocus(hwnd);
				if (r == gmpi::ReturnCode::Unhandled)
				{
					doContextMenu(p, flags);
				}
				break;

			case WM_MBUTTONUP:
			case WM_RBUTTONUP:
			case WM_LBUTTONUP:
				r = inputClient->onPointerUp(p, flags);
				break;
			}
		}
		break;

	case WM_MOUSELEAVE:
		isTrackingMouse = false;
//		inputClient->setHover(false);
		break;

	case WM_MOUSEWHEEL:
	case WM_MOUSEHWHEEL:
		{
           Point p = win32::pointFromScreenLParam(getWindowHandle(), lParam, WindowToDips);

            //The wheel rotation will be a multiple of WHEEL_DELTA, which is set at 120. This is the threshold for action to be taken, and one such action (for example, scrolling one increment) should occur for each delta.
			const auto zDelta = GET_WHEEL_DELTA_WPARAM(wParam);

         int32_t flags = win32::makeWheelFlags(message, wParam);

			/*auto r =*/ inputClient->onMouseWheel(p, flags, zDelta);
		}
		break;

	case WM_CHAR:
		if(inputClient)
			inputClient->onKeyPress((wchar_t) wParam);
		break;

	case WM_KEYDOWN:
		if (inputClient)
		{
			switch (wParam)
			{
			case VK_RIGHT:
			case VK_LEFT:
			case VK_UP:
			case VK_DOWN:
				inputClient->onKeyPress((wchar_t)wParam);
			}
		}
		break;

	case WM_PAINT:
	{
		OnPaint();
		//		return ::DefWindowProc(hwnd, message, wParam, lParam); // clear update rect.
	}
	break;

	case WM_SIZE:
	{
		UINT width = LOWORD(lParam);
		UINT height = HIWORD(lParam);

		OnSize(width, height);
		return ::DefWindowProc(hwnd, message, wParam, lParam); // clear update rect.
	}
	break;
	
	default:
		return DefWindowProc(hwnd, message, wParam, lParam);

	}
	return TRUE;
}

void DxDrawingFrameBase::sizeClientDips(float width, float height)
{
	const gmpi::drawing::Size available{ width, height };
	const gmpi::drawing::Rect finalRect{ 0, 0, available.width, available.height };
	gmpi::drawing::Size desired{};

	drawingClient->measure(&available, &desired);
	drawingClient->arrange(&finalRect);
}

// Ideally this is called at 60Hz so we can draw as fast as practical, but without blocking to wait for Vsync all the time (makes host unresponsive).
bool DxDrawingFrameBase::onTimer()
{
	auto hwnd = getWindowHandle();
	if (hwnd == nullptr
		)
		return true;

	if (frameUpdateClient)
	{
		frameUpdateClient->preGraphicsRedraw();
	}

	// Queue pending drawing updates to backbuffer.
	const BOOL bErase = FALSE;

  for (const auto& invalidRect : backBufferDirtyRects.get())
	{
      RECT rect{ invalidRect.left, invalidRect.top, invalidRect.right, invalidRect.bottom };
		::InvalidateRect(hwnd, &rect, bErase);
	}
	backBufferDirtyRects.clear();

	return true;
}

void DxDrawingFrameBase::attachClient(gmpi::api::IUnknown* gfx)
{
	gfx->queryInterface(&gmpi::api::IDrawingClient::guid, drawingClient.put_void());
	gfx->queryInterface(&gmpi::api::IInputClient::guid, inputClient.put_void());

	// legacy
	gfx->queryInterface(&gmpi::api::IGraphicsRedrawClient::guid, frameUpdateClient.put_void());

	if (drawingClient)
	{
		drawingClient->setHost(static_cast<gmpi::api::IDrawingHost*>(this));
	}
}

void DxDrawingFrameBase::detachAndRecreate()
{
	assert(!reentrant); // do this async please.

	// detachClient();
	frameUpdateClient = {};
	inputClient = {};
	drawingClient = {};

	CreateSwapPanel(DrawingFactory.getD2dFactory());
}

void RenderLog(ID2D1RenderTarget* context_, IDWriteFactory* writeFactory, ID2D1Factory1* factory)
{
} // RenderLog ====================================================

bool DxDrawingFrameBase::canPaint(std::span<gmpi::drawing::RectL> dirtyRects)
{
  return drawingClient != nullptr;
}

void DxDrawingFrameBase::renderFrame(ID2D1DeviceContext* deviceContext, std::span<gmpi::drawing::RectL> dirtyRects)
{
	gmpi::directx::GraphicsContext context1(deviceContext, &DrawingFactory);
	Graphics graphics(&context1);

	graphics.beginDraw();

	for (auto& r : dirtyRects)
	{
		auto r2 = transformRect(WindowToDips, drawing::Rect{ static_cast<float>(r.left), static_cast<float>(r.top), static_cast<float>(r.right), static_cast<float>(r.bottom) });

		drawing::Rect temp;
		temp.left = floorf(r2.left);
		temp.top = floorf(r2.top);
		temp.right = ceilf(r2.right);
		temp.bottom = ceilf(r2.bottom);

		graphics.pushAxisAlignedClip(temp);
		drawingClient->render(&context1);
		graphics.popAxisAlignedClip();
	}

	const bool displayFrameRate = false;
	if (displayFrameRate)
	{
		static int frameCount = 0;
		static char frameCountString[100] = "";
		if (++frameCount == 60)
		{
			auto timenow = std::chrono::steady_clock::now();
			auto elapsed = std::chrono::steady_clock::now() - frameCountTime;
			auto elapsedSeconds = 0.001f * (float)std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

			float frameRate = frameCount / elapsedSeconds;
			sprintf_s(frameCountString, sizeof(frameCountString), "%3.1f FPS", frameRate);
			frameCountTime = timenow;
			frameCount = 0;

			auto brush = graphics.createSolidColorBrush(Colors::Black);
			auto fpsRect = drawing::Rect{ 0, 0, 50, 18 };
			graphics.fillRectangle(fpsRect, brush);
			brush.setColor(Colors::White);
			graphics.drawTextU(frameCountString, graphics.getFactory().createTextFormat(12), fpsRect, brush);

			backBufferDirtyRects.add({ 0, 0, 100, 36 });
		}
	}

	if (graphics.endDraw() != gmpi::ReturnCode::Ok)
	{
		ReleaseDevice();
	}
}

void DxDrawingFrameBase::OnPaint()
{
	updateRegion_native.copyDirtyRects(getWindowHandle(), swapChainSize);
	ValidateRect(getWindowHandle(), NULL);

	auto dirtyRects = updateRegion_native.getUpdateRects();
	PaintFrame({ dirtyRects.data(), dirtyRects.size() });
}

void tempSharedD2DBase::PaintFrame(std::span<gmpi::drawing::RectL> dirtyRects)
{
	if (reentrant || dirtyRects.empty() || !canPaint(dirtyRects))
	{
		return;
	}

	if (!preparePaint(dirtyRects) || monitorChanged)
	{
		return;
	}

	reentrant = true;

	if (!d2dDeviceContext && recreateDeviceOnPaint())
	{
		if (auto* d2dFactory = getD2dFactory())
		{
			CreateSwapPanel(d2dFactory);
		}
	}

	if (!d2dDeviceContext)
	{
		reentrant = false;
		return;
	}

	gmpi::directx::ComPtr<ID2D1DeviceContext> deviceContext;
	if (hdrRenderTarget)
	{
		d2dDeviceContext->BeginDraw();
		deviceContext = hdrRenderTargetDC;
	}
	else
	{
		deviceContext = d2dDeviceContext;
	}

	renderFrame(deviceContext.get(), dirtyRects);

	if (hdrRenderTarget)
	{
		assert(hdrWhiteScaleEffect);
		d2dDeviceContext->DrawImage(
			hdrWhiteScaleEffect.get(),
			D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR,
			D2D1_COMPOSITE_MODE_SOURCE_COPY
		);
		d2dDeviceContext->EndDraw();
	}

	if (firstPresent)
	{
		firstPresent = false;
		const auto hr = swapChain->Present(1, 0);
		if (S_OK != hr && DXGI_STATUS_OCCLUDED != hr)
		{
			ReleaseDevice();
		}
	}
	else
	{
		DXGI_PRESENT_PARAMETERS presetParameters{ static_cast<UINT>(dirtyRects.size()), reinterpret_cast<RECT*>(dirtyRects.data()), nullptr, nullptr };
		const auto hr = swapChain->Present1(1, 0, &presetParameters);
		if (S_OK != hr && DXGI_STATUS_OCCLUDED != hr)
		{
			ReleaseDevice();
		}
	}

	reentrant = false;
}

void tempSharedD2DBase::CreateSwapPanel(ID2D1Factory1* d2dFactory)
{
	ReleaseDevice();

	// Create a Direct3D Device
	UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#ifdef _DEBUG
	// you must explicity install DX debug support for this to work.
	creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

	D3D_FEATURE_LEVEL featureLevels[] = 
	{
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_1,
		D3D_FEATURE_LEVEL_10_0,
	};

	D3D_FEATURE_LEVEL supportedFeatureLevel;
	gmpi::directx::ComPtr<ID3D11Device> d3dDevice;
	
	// Create Hardware device.
    HRESULT r = DXGI_ERROR_UNSUPPORTED;
	do {
		r = D3D11CreateDevice(nullptr,
			D3D_DRIVER_TYPE_HARDWARE,
        	0,
			creationFlags,
			featureLevels,
			std::size(featureLevels),
			D3D11_SDK_VERSION,
			d3dDevice.put(),
			&supportedFeatureLevel,
			nullptr);

			// Clear D3D11_CREATE_DEVICE_DEBUG
			((creationFlags) &= (0xffffffff ^ (D3D11_CREATE_DEVICE_DEBUG)));

	} while (r == 0x887a002d); // The application requested an operation that depends on an SDK component that is missing or mismatched. (no DEBUG LAYER).

    // Get the Direct3D device.
    auto dxgiDevice = d3dDevice.as<::IDXGIDevice>();

	// Get the DXGI adapter.
	gmpi::directx::ComPtr< ::IDXGIAdapter > dxgiAdapter;
	dxgiDevice->GetAdapter(dxgiAdapter.put());

	// Support for HDR displays.
	windowWhiteLevel = calcWhiteLevel(); // the native white level.
	float whiteMult = windowWhiteLevel; // the swapchain white-level, that might be overriden by an 8-bit swapchain.

	bool DX_support_sRGB{ true };
	// !!! this is shit, returns DEFAULT adaptor only. No good if you are using both shitty onboard GPU plus high-end PCI GPU. !!!
	// query adaptor memory. Assume small integrated graphics cards do not have the capacity for float pixels.
	// Software renderer has no device memory, yet does support float pixels anyhow.
	if (!m_disable_gpu)
	{
		DXGI_ADAPTER_DESC adapterDesc{};
		dxgiAdapter->GetDesc(&adapterDesc);

		const auto dedicatedRamMB = adapterDesc.DedicatedVideoMemory / 0x100000;

		// Intel HD Graphics on my Kogan has 128MB.
		DX_support_sRGB &= (dedicatedRamMB >= 512); // MB
	}

	{
		UINT driverSrgbSupport = 0;
		auto hr = d3dDevice->CheckFormatSupport(bestFormat, &driverSrgbSupport);

		const UINT srgbflags = D3D11_FORMAT_SUPPORT_DISPLAY | D3D11_FORMAT_SUPPORT_TEXTURE2D | D3D11_FORMAT_SUPPORT_BLENDABLE;

		if (SUCCEEDED(hr))
		{
			DX_support_sRGB &= ((driverSrgbSupport & srgbflags) == srgbflags);
		}

		DX_support_sRGB &= D3D_FEATURE_LEVEL_11_0 <= supportedFeatureLevel;
	}

	const bool useSoftwareRenderer = (!DX_support_sRGB && Fallback_Software == m_fallbackStrategy) || m_disable_gpu;
	const bool useDeepColor = !m_disable_deep_color && (DX_support_sRGB || useSoftwareRenderer);

	if (useSoftwareRenderer)
	{
		// release hardware device
		d3dDevice = nullptr;
		r = DXGI_ERROR_UNSUPPORTED;
	}

	// If hardware swapchain failed or we choose to, fallback to software rendering.
	if (DXGI_ERROR_UNSUPPORTED == r)
	{
		do {
		r = D3D11CreateDevice(nullptr,
			D3D_DRIVER_TYPE_WARP,
			nullptr,
			creationFlags,
			nullptr, 0,
			D3D11_SDK_VERSION,
			d3dDevice.put(),
			&supportedFeatureLevel,
			nullptr);

			// Clear D3D11_CREATE_DEVICE_DEBUG
			((creationFlags) &= (0xffffffff ^ (D3D11_CREATE_DEVICE_DEBUG)));

		} while (r == 0x887a002d); // The application requested an operation that depends on an SDK component that is missing or mismatched. (no DEBUG LAYER).

		dxgiDevice = d3dDevice.as<::IDXGIDevice>();
		_RPT0(0, "Using Software Renderer\n");
	}
	else
	{
		_RPT0(0, "Using Hardware Renderer\n");
	}

    // Get the DXGI factory.
    gmpi::directx::ComPtr<::IDXGIFactory2> dxgiFactory;
    dxgiAdapter->GetParent(__uuidof(dxgiFactory), dxgiFactory.put_void());

	DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
    swapChainDesc.Format = useDeepColor ? bestFormat : fallbackFormat;
	swapChainDesc.SampleDesc.Count = 1; // Don't use multi-sampling.
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.BufferCount = 2;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
	swapChainDesc.Scaling = DXGI_SCALING_NONE; // prevents annoying stretching effect when resizing window. HWNDs only.

	if (lowDpiMode)
	{
		RECT temprect;
		GetClientRect(getWindowHandle(), &temprect);

		swapChainDesc.Width = (temprect.right - temprect.left) / 2;
		swapChainDesc.Height = (temprect.bottom - temprect.top) / 2;
		swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
	}

	// customization point.
	gmpi::directx::ComPtr<::IDXGISwapChain1> swapChain1;
	auto swapchainresult = createNativeSwapChain(
		dxgiFactory.get(),
		d3dDevice.get(),
		&swapChainDesc,
		swapChain1.put()
	);

    if (FAILED(swapchainresult))
    {
        assert(false);

        // Handle the error appropriately
        if (swapchainresult == DXGI_ERROR_INVALID_CALL)
        {
            OutputDebugString(L"DXGI_ERROR_INVALID_CALL: The method call is invalid.\n");
        }
        else
        {
            // Handle other potential errors
            OutputDebugString(L"Failed to create swap chain.\n");
        }

        return;
    }

	swapChain1->QueryInterface(swapChain.getAddressOf());

	// Creating the Direct2D Device
    gmpi::directx::ComPtr<::ID2D1Device> d2dDevice;
    d2dFactory->CreateDevice(dxgiDevice.get(), d2dDevice.put());

	// and context.
    d2dDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_ENABLE_MULTITHREADED_OPTIMIZATIONS, d2dDeviceContext.put());

	// disable DPI for testing.
	const float dpiScale = lowDpiMode ? 1.0f : getRasterizationScale();
 	DipsToWindow = gmpi::drawing::makeScale(dpiScale, dpiScale);
	WindowToDips = gmpi::drawing::invert(DipsToWindow);

    d2dDeviceContext->SetDpi(dpiScale * 96.f, dpiScale * 96.f);

	CreateDeviceSwapChainBitmap();

	// customisation point.
	OnSwapChainCreated();
}

void tempSharedD2DBase::recreateSwapChainAndClientAsync()
{
	monitorChanged = true;

	// notify client that it's device-dependent resources have been invalidated.
	if (clientInvalidated)
		clientInvalidated();
}

void tempSharedD2DBase::setWhiteLevel(float whiteMult)
{
	if(!swapChain) // no need to do anything if app not open properly yet.
		return;

	const auto bestFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;

	// query actual swapchain capabilities.
	bool using8bitSwapChain{ false };
	{
		DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
		swapChain->GetDesc1(&swapChainDesc);
		using8bitSwapChain = (swapChainDesc.Format != bestFormat);
	}

	if (using8bitSwapChain)
	{
		_RPT0(0, "Using 8-bit Swap Chain\n");
	}
	else
	{
		_RPT0(0, "Using Deep Color Swap Chain\n");
	}

	// if we're reverting to 8-bit colour HDR white-mult is N/A.
	if (using8bitSwapChain)
		whiteMult = 1.0f;

	if (!using8bitSwapChain)
	{
		// create a filter to adjust the white level for HDR displays.
		if (whiteMult != 1.0f)
		{
			// create whitescale effect
			// White level scale is used to multiply the color values in the image; this allows the user
			// to adjust the brightness of the image on an HDR display.
			d2dDeviceContext->CreateEffect(CLSID_D2D1ColorMatrix, hdrWhiteScaleEffect.put());

			// SDR white level scaling is performing by multiplying RGB color values in linear gamma.
			// We implement this with a Direct2D matrix effect.
			D2D1_MATRIX_5X4_F matrix = D2D1::Matrix5x4F(
				whiteMult, 0, 0, 0,  // [R] Multiply each color channel
				0, whiteMult, 0, 0,  // [G] by the scale factor in 
				0, 0, whiteMult, 0,  // [B] linear gamma space.
				0, 0, 0, 1,		 // [A] Preserve alpha values.
				0, 0, 0, 0);	 //     No offset.

			hdrWhiteScaleEffect->SetValue(D2D1_COLORMATRIX_PROP_COLOR_MATRIX, matrix);

			// increase the bit-depth of the filter, else it does a shitty 8-bit conversion. Which results in serious degredation of the image.
			if (d2dDeviceContext->IsBufferPrecisionSupported(D2D1_BUFFER_PRECISION_16BPC_FLOAT))
			{
				auto hr = hdrWhiteScaleEffect->SetValue(D2D1_PROPERTY_PRECISION, D2D1_BUFFER_PRECISION_16BPC_FLOAT);
			}

			const D2D1_SIZE_F desiredSize = D2D1::SizeF(static_cast<float>(swapChainSize.width), static_cast<float>(swapChainSize.height));

			d2dDeviceContext->CreateCompatibleRenderTarget(desiredSize, hdrRenderTarget.put());
			hdrRenderTargetDC = hdrRenderTarget.as<ID2D1DeviceContext>();

			_RPT0(0, "Using HDR White Adjustment filter\n");
		}
		else
		{
			_RPT0(0, "No Color Adjustment filter\n");
			hdrRenderTargetDC = {};
			hdrRenderTarget = {};
			hdrWhiteScaleEffect = {};
		}
	}
	else
	{
		// new: render to 16-bit back-buffer, then copy to swapchain during present.

		// create color management effect to convert from linear to sRGB image
		d2dDeviceContext->CreateEffect(CLSID_D2D1ColorManagement, hdrWhiteScaleEffect.put());

		hdrWhiteScaleEffect->SetValue(
			D2D1_COLORMANAGEMENT_PROP_QUALITY,
			D2D1_COLORMANAGEMENT_QUALITY_BEST   // Required for floating point and DXGI color space support.
		);

		gmpi::directx::ComPtr<ID2D1ColorContext> srcColorContext, dstColorContext;

		// The destination color space is the render target's (swap chain's) color space. This app uses an
		// FP16 swap chain, which requires the colorspace to be scRGB.
		d2dDeviceContext->CreateColorContext(
			D2D1_COLOR_SPACE_SCRGB,
			nullptr,
			0,
			srcColorContext.put()
		);

		d2dDeviceContext->CreateColorContext(
			D2D1_COLOR_SPACE_SRGB,
			nullptr,
			0,
			dstColorContext.put()
		);

		hdrWhiteScaleEffect->SetValue(D2D1_COLORMANAGEMENT_PROP_SOURCE_COLOR_CONTEXT, srcColorContext.get());
		hdrWhiteScaleEffect->SetValue(D2D1_COLORMANAGEMENT_PROP_DESTINATION_COLOR_CONTEXT, dstColorContext.get());

		/* no help*/
		// increase the bit-depth of the filter, else it does a shitty 8-bit conversion. Which results in serious degredation of the image.
		if (d2dDeviceContext->IsBufferPrecisionSupported(D2D1_BUFFER_PRECISION_16BPC_FLOAT))
		{
			auto hr = hdrWhiteScaleEffect->SetValue(D2D1_PROPERTY_PRECISION, D2D1_BUFFER_PRECISION_16BPC_FLOAT);
		}

		const D2D1_SIZE_F desiredSize{ static_cast<float>(swapChainSize.width), static_cast<float>(swapChainSize.height) };
		D2D1_PIXEL_FORMAT desiredFormat
		{
			bestFormat
			,D2D1_ALPHA_MODE_IGNORE // Claude advice for dark screen bug. D2D1_ALPHA_MODE_UNKNOWN
		};

		d2dDeviceContext->CreateCompatibleRenderTarget(
			&desiredSize
			, nullptr
			, &desiredFormat
			, D2D1_COMPATIBLE_RENDER_TARGET_OPTIONS_NONE
			, hdrRenderTarget.put()
		);

		hdrRenderTargetDC = hdrRenderTarget.as<ID2D1DeviceContext>();

		_RPT0(0, "Using F16 to sRGB Adjustment filter\n");
	}

	if (hdrWhiteScaleEffect)
	{
		hdrRenderTarget->GetBitmap(hdrBitmap.put());
		hdrWhiteScaleEffect->SetInput(0, hdrBitmap.get());
	}

	invalidateRect(nullptr); // force redraw with new white level.
}

HRESULT DxDrawingFrameBase::createNativeSwapChain
(
	IDXGIFactory2* factory,
	ID3D11Device* d3dDevice,
	DXGI_SWAP_CHAIN_DESC1* desc,
	IDXGISwapChain1** returnSwapChain
)
{
	return factory->CreateSwapChainForHwnd(
		d3dDevice,
		getWindowHandle(),
		desc,
		nullptr,
		nullptr,
		returnSwapChain
	);
}

void DxDrawingFrameBase::OnSwapChainCreated()
{
}

void tempSharedD2DBase::CreateDeviceSwapChainBitmap()
{
//	_RPT0(_CRT_WARN, "\n\nCreateDeviceSwapChainBitmap()\n");

	gmpi::directx::ComPtr<IDXGISurface> surface;
	swapChain->GetBuffer(0, // buffer index
		__uuidof(surface),
		surface.put_void());

	// Get the swapchain pixel format.
	DXGI_SURFACE_DESC sufaceDesc;
	surface->GetDesc(&sufaceDesc);

	auto props2 = BitmapProperties1(
		D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
		PixelFormat(sufaceDesc.Format, D2D1_ALPHA_MODE_IGNORE)
		);

	gmpi::directx::ComPtr<ID2D1Bitmap1> bitmap;
	d2dDeviceContext->CreateBitmapFromDxgiSurface(surface.get(),
		props2,
		bitmap.put());

	const auto bitmapsize = bitmap->GetSize();
	swapChainSize.width = static_cast<int32_t>(bitmapsize.width);
	swapChainSize.height = static_cast<int32_t>(bitmapsize.height);

//_RPT2(_CRT_WARN, "%x B[%f,%f]\n", this, bitmapsize.width, bitmapsize.height);

	// Now attach Device Context to swapchain bitmap.
	d2dDeviceContext->SetTarget(bitmap.get());

	const bool using8bitSwapChain = (sufaceDesc.Format == fallbackFormat);

	// if we're reverting to 8-bit colour HDR white-mult is N/A.
	const float whiteMult = using8bitSwapChain ? 1.0f : windowWhiteLevel; // the native white level.

	setWhiteLevel(whiteMult);

	// Initial present() moved here in order to ensure it happens before first Timer() tries to draw anything.
	firstPresent = true;
	monitorChanged = false;

	InvalidateRect(getWindowHandle(), nullptr, false);
}

void tempSharedD2DBase::OnSize(UINT width, UINT height)
{
	assert(swapChain);
	assert(d2dDeviceContext);

	d2dDeviceContext->SetTarget(nullptr);

	if (S_OK == swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0))
	{
		CreateDeviceSwapChainBitmap();

		const auto scale = 1.0 / getRasterizationScale();

		sizeClientDips(
			static_cast<float>(width * scale),
			static_cast<float>(height * scale)
		);
	}
	else
	{
		ReleaseDevice();
	}
}

void DrawingFrame::reSize(int left, int top, int right, int bottom)
{
	const auto width = right - left;
	const auto height = bottom - top;

	if (d2dDeviceContext && (swapChainSize.width != width || swapChainSize.height != height))
	{
		SetWindowPos(
			windowHandle
			, NULL
			, 0
			, 0
			, width
			, height
			, SWP_NOZORDER
		);

		d2dDeviceContext->SetTarget(nullptr);
		if (S_OK == swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0))
		{
			CreateDeviceSwapChainBitmap();
		}
		else
		{
			ReleaseDevice();
		}
	}
}

// Convert to an integer rect, ensuring it surrounds all partial pixels.
inline drawing::RectL RectToIntegerLarger(gmpi::drawing::Rect f)
{
	return drawing::RectL{
	static_cast<int32_t>(f.left),
	static_cast<int32_t>(f.top),
	static_cast<int32_t>(f.right) + 1,
	static_cast<int32_t>(f.bottom) + 1 };
}

void DxDrawingFrameBase::invalidateRect(const drawing::Rect* invalidRect)
{
	if (invalidRect)
	{
       backBufferDirtyRects.add(invalidRect, DipsToWindow);
	}
	else
	{
        drawing::RectL r;
		GetClientRect(getWindowHandle(), reinterpret_cast<RECT*>( &r ));
       backBufferDirtyRects.add(r);
	}
}

// IInputHost (setCapture/getCapture/releaseCapture) is now inline on tempSharedD2DBase.

//gmpi::ReturnCode DxDrawingFrameBase::getFocus()
//{
//	::SetFocus(getWindowHandle());
//	return gmpi::ReturnCode::Ok;
//}
//
//gmpi::ReturnCode DxDrawingFrameBase::releaseFocus()
//{
//	return gmpi::ReturnCode::Ok;
//}

// IDialogHost
gmpi::ReturnCode DxDrawingFrameBase::createTextEdit(const gmpi::drawing::Rect* r, gmpi::api::IUnknown** returnTextEdit)
{
	// Delegate to the shared factory so SynthEditLib's DrawingFrameBase2 hits the
	// same code path. See win32::createPlatformTextEdit (DrawingFrameWin.h).
	return gmpi::hosting::win32::createPlatformTextEdit(getWindowHandle(), r, DipsToWindow._22, returnTextEdit);
}

namespace privateStuff
{
	inline std::wstring Utf8ToWstring(std::string_view str)
	{
		std::wstring res;
		const size_t size = MultiByteToWideChar(
			CP_UTF8,
			0,
			str.data(),
			static_cast<int>(str.size()),
			0,
			0
		);

		res.resize(size);

		MultiByteToWideChar(
			CP_UTF8,
			0,
			str.data(),
			static_cast<int>(str.size()),
			const_cast<LPWSTR>(res.data()),
			static_cast<int>(size)
		);

		return res;
	};
	inline std::string WStringToUtf8(const std::wstring& p_cstring)
	{
		std::string res;

		const size_t size = WideCharToMultiByte(
			CP_UTF8,
			0,
			p_cstring.data(),
			static_cast<int>(p_cstring.size()),
			0,
			0,
			NULL,
			NULL
		);

		res.resize(size);

		WideCharToMultiByte(
			CP_UTF8,
			0,
			p_cstring.data(),
			static_cast<int>(p_cstring.size()),
			const_cast<LPSTR>(res.data()),
			static_cast<int>(size),
			NULL,
			NULL
		);

		return res;
	}
}

namespace win32
{

LRESULT CALLBACK GenericWindowProc(
	HWND hwnd,
	UINT message,
	WPARAM wParam,
	LPARAM lParam)
{
	_RPTN(0, "GenericWindowProc(%d)\n", message);

	if (auto client = (hasWindowProc*)(LONG_PTR)GetWindowLongPtr(hwnd, GWLP_USERDATA); client)
	{
		return client->WindowProc(hwnd, message, wParam, lParam);
	}

	return DefWindowProc(hwnd, message, wParam, lParam);
}

PlatformKeyListener::~PlatformKeyListener()
{
	_RPT0(0, "~PlatformKeyListener()\n");

	::SendMessage(windowHandle, WM_CLOSE, 0, 0);
}

ReturnCode PlatformKeyListener::showAsync(gmpi::api::IUnknown* callback)
{
	callback->queryInterface(&gmpi::api::IKeyListenerCallback::guid, (void**)&callback2);

	// Create an HWND, capture keystrokes etc.
	static WNDCLASS windowClass{};
	static wchar_t gClassName[20] = {};

	const auto dllHandle = local44_GetDllHandle_randomshit();

	if (!windowClass.lpfnWndProc)
	{
		::OleInitialize(0);

		swprintf(gClassName, std::size(gClassName), L"GMPIKL%p", dllHandle);

		windowClass.style = CS_GLOBALCLASS;
		windowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
		windowClass.hInstance = dllHandle;
		windowClass.lpfnWndProc = GenericWindowProc;
		windowClass.lpszClassName = gClassName;

		RegisterClass(&windowClass);
	}

	int style = WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS;
	int extended_style = 0;

	windowHandle = CreateWindowEx(extended_style, gClassName, L"",
		style, 0, 0, static_cast<int>(bounds.right - bounds.left), static_cast<int>(bounds.bottom - bounds.top),
		parentWnd, NULL, dllHandle, NULL);

	if (!windowHandle)
		return gmpi::ReturnCode::Fail;

	SetWindowLongPtr(windowHandle, GWLP_USERDATA, (__int3264)static_cast<hasWindowProc*>(this));

	::ShowWindow(windowHandle, SW_SHOWNORMAL);
	::SetFocus(windowHandle);

	return gmpi::ReturnCode::Ok;
}

LRESULT PlatformKeyListener::WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_KILLFOCUS:
	{
		// Release the keyboard focus
		if (callback2)
		{
			callback2->onLostFocus(gmpi::ReturnCode::Ok);
		}
		break;
	}

	case WM_KEYDOWN:
	{
		if(!callback2)
			break;

		wchar_t key[2] = { 0 };
		BYTE keyboardState[256];
		GetKeyboardState(keyboardState);

		// Translate the virtual-key code to a Unicode character
		int result = ToUnicode(static_cast<UINT>(wParam), static_cast<UINT>((lParam >> 16) & 0xFF), keyboardState, key, 2, 0);

		switch (wParam)
		{
		case VK_LEFT:
			key[0] = 0x25;
			break;
		case VK_RIGHT:
			key[0] = 0x27;
			break;
		case VK_DELETE:
			key[0] = 0x7F;
			break;

		default:
			// Handle other keys
//			key = static_cast<wchar_t>(wParam);
			break;
		}

		int32_t flags{};

		if (GetKeyState(VK_SHIFT) < 0)
		{
			flags |= static_cast<int32_t>(gmpi::api::PointerFlags::KeyShift);
		}
		if (GetKeyState(VK_CONTROL) < 0)
		{
			flags |= static_cast<int32_t>(gmpi::api::PointerFlags::KeyControl);
		}
		if (GetKeyState(VK_MENU) < 0) // don't work.
		{
			flags |= static_cast<int32_t>(gmpi::api::PointerFlags::KeyAlt);
		}

		callback2->onKeyDown(key[0], flags);
		break;
	}
	break;
	}

	return DefWindowProc(hwnd, message, wParam, lParam);
}
} // namespace win32

class GMPI_WIN_StockDialog : public gmpi::api::IStockDialog
{
	HWND parentWnd;
	gmpi::api::StockDialogType dialogType;
	std::wstring title;
	std::wstring text;

public:
	GMPI_WIN_StockDialog(HWND pParentWnd, gmpi::api::StockDialogType type, const char* ptitle, const char* ptext) :
		parentWnd(pParentWnd)
		, dialogType(type)
		, title(privateStuff::Utf8ToWstring(ptitle))
		, text(privateStuff::Utf8ToWstring(ptext))
	{}

	gmpi::ReturnCode showAsync(gmpi::api::IUnknown* callback) override
	{
		gmpi::shared_ptr<gmpi::api::IUnknown> unknown;
		unknown = callback;
		auto dialogCallback = unknown.as<gmpi::api::IStockDialogCallback>();
		if (!dialogCallback)
			return gmpi::ReturnCode::Fail;

		UINT mbType = MB_ICONINFORMATION;
		switch (dialogType)
		{
		case gmpi::api::StockDialogType::Ok:
			mbType = MB_OK | MB_ICONINFORMATION;
			break;
		case gmpi::api::StockDialogType::OkCancel:
			mbType = MB_OKCANCEL | MB_ICONQUESTION;
			break;
		case gmpi::api::StockDialogType::YesNo:
			mbType = MB_YESNO | MB_ICONQUESTION;
			break;
		case gmpi::api::StockDialogType::YesNoCancel:
			mbType = MB_YESNOCANCEL | MB_ICONQUESTION;
			break;
		}

		const int result = MessageBoxW(parentWnd, text.c_str(), title.c_str(), mbType);

		gmpi::api::StockDialogButton button{};
		switch (result)
		{
		case IDOK:     button = gmpi::api::StockDialogButton::Ok;     break;
		case IDCANCEL: button = gmpi::api::StockDialogButton::Cancel; break;
		case IDYES:    button = gmpi::api::StockDialogButton::Yes;    break;
		case IDNO:     button = gmpi::api::StockDialogButton::No;     break;
		default:       button = gmpi::api::StockDialogButton::Cancel; break;
		}

		dialogCallback->onComplete(button);
		return gmpi::ReturnCode::Ok;
	}

	gmpi::ReturnCode queryInterface(const gmpi::api::Guid* iid, void** returnInterface) override
	{
		*returnInterface = {};
		GMPI_QUERYINTERFACE(gmpi::api::IStockDialog);
		return gmpi::ReturnCode::NoSupport;
	}
	GMPI_REFCOUNT
};

gmpi::ReturnCode DxDrawingFrameBase::createStockDialog(int32_t dialogType, const char* title, const char* text, gmpi::api::IUnknown** returnDialog)
{
	// Delegate to the shared factory so SynthEditLib's DrawingFrameBase2 hits the
	// same code path.
	return gmpi::hosting::win32::createPlatformStockDialog(getWindowHandle(), dialogType, title, text, returnDialog);
}

class GMPI_WIN_PopupMenu : public gmpi::api::IPopupMenu
{
	HMENU hmenu;
	std::vector<HMENU> hmenus;
	HWND parentWnd;
	int align;
	float dpiScale;
	gmpi::drawing::Rect editrect_s{};
	int32_t selectedId;
	std::vector<int32_t> menuIds;
	struct MenuCallback
	{
		int32_t localId{};
		gmpi::shared_ptr<gmpi::api::IUnknown> callback;
	};
	std::vector<MenuCallback> callbacks;

public:
	// Might need to apply DPI to Text size, like text-entry does.
	GMPI_WIN_PopupMenu(HWND pParentWnd, const gmpi::drawing::Rect* r, float dpi = 1.0f) : hmenu(0)
		, parentWnd(pParentWnd)
		, align(TPM_LEFTALIGN)
		, dpiScale(dpi)
		, selectedId(-1)
		, editrect_s(*r)
	{
		hmenu = CreatePopupMenu();
		hmenus.push_back(hmenu);
	}

	~GMPI_WIN_PopupMenu()
	{
		DestroyMenu(hmenu);
	}

	gmpi::ReturnCode addItem(const char* text, int32_t id, int32_t flags, gmpi::api::IUnknown* callback) override
	{
       gmpi::shared_ptr<gmpi::api::IUnknown> itemcallback;
		itemcallback = callback;

		UINT nativeFlags = MF_STRING;
		if ((flags & static_cast<int32_t>(gmpi::api::PopupMenuFlags::Ticked)) != 0)
		{
			nativeFlags |= MF_CHECKED;
		}
		if ((flags & static_cast<int32_t>(gmpi::api::PopupMenuFlags::Grayed)) != 0)
		{
			nativeFlags |= MF_GRAYED;
		}
		if ((flags & static_cast<int32_t>(gmpi::api::PopupMenuFlags::Separator)) != 0)
		{
			nativeFlags |= MF_SEPARATOR;
		}
		if ((flags & static_cast<int32_t>(gmpi::api::PopupMenuFlags::Break)) != 0)
		{
			nativeFlags |= MF_MENUBREAK;
		}

		const bool isSubMenuStart = (flags & static_cast<int32_t>(gmpi::api::PopupMenuFlags::SubMenuBegin)) != 0;
		const bool isSubMenuEnd = (flags & static_cast<int32_t>(gmpi::api::PopupMenuFlags::SubMenuEnd)) != 0;	

		if (isSubMenuStart || isSubMenuEnd)
		{
			if (isSubMenuStart)
			{
				auto submenu = CreatePopupMenu();
				AppendMenu(hmenus.back(), nativeFlags | MF_POPUP, (UINT_PTR)submenu, privateStuff::Utf8ToWstring(text).c_str());
				hmenus.push_back(submenu);
			}
			else if (isSubMenuEnd)
			{
				hmenus.pop_back();
			}
		}
		else
		{
			menuIds.push_back(id);
          callbacks.push_back({ id, itemcallback });
			AppendMenu(hmenus.back(), nativeFlags, menuIds.size(), privateStuff::Utf8ToWstring(text).c_str());
		}

		return gmpi::ReturnCode::Ok;
	}

	gmpi::ReturnCode showAsync() override
	{
		POINT nativePoint{(LONG)editrect_s.left, (LONG)editrect_s.top };
		ClientToScreen(parentWnd, &nativePoint);

		int flags = align | TPM_LEFTBUTTON | TPM_NONOTIFY | TPM_RETURNCMD;

		auto index = TrackPopupMenu(hmenu, flags, nativePoint.x, nativePoint.y, 0, parentWnd, 0) - 1;

		if (index >= 0)
		{
			selectedId = menuIds[index];
		}
		else
		{
			selectedId = 0; // N/A
		}

		if (index >= 0 && index < static_cast<int32_t>(callbacks.size()) && callbacks[index].callback)
		{
			gmpi::shared_ptr<gmpi::api::IPopupMenuCallback> itemcallback;
			callbacks[index].callback->queryInterface(&gmpi::api::IPopupMenuCallback::guid, itemcallback.put_void());
			if (itemcallback)
			{
				itemcallback->onComplete(gmpi::ReturnCode::Ok, callbacks[index].localId);
			}
		}

		return gmpi::ReturnCode::Ok;
	}

	gmpi::ReturnCode setAlignment(int32_t alignment) override
	{
		// gmpi::drawing::TextAlignment values: Leading=0, Trailing=1, Center=2.
		// Binary-compatible with the legacy MP1_TEXT_ALIGNMENT_* constants.
		switch (alignment)
		{
		case static_cast<int32_t>(gmpi::drawing::TextAlignment::Leading):
			align = TPM_LEFTALIGN;
			break;
		case static_cast<int32_t>(gmpi::drawing::TextAlignment::Center):
			align = TPM_CENTERALIGN;
			break;
		case static_cast<int32_t>(gmpi::drawing::TextAlignment::Trailing):
		default:
			align = TPM_RIGHTALIGN;
			break;
		}
		return gmpi::ReturnCode::Ok;
	}

	gmpi::ReturnCode queryInterface(const gmpi::api::Guid* iid, void** returnInterface) override
	{
		*returnInterface = {};
		GMPI_QUERYINTERFACE(gmpi::api::IPopupMenu);
		GMPI_QUERYINTERFACE(gmpi::api::IContextItemSink);
		return gmpi::ReturnCode::NoSupport;
	}
	GMPI_REFCOUNT
};

gmpi::ReturnCode DxDrawingFrameBase::createPopupMenu(const gmpi::drawing::Rect* r, gmpi::api::IUnknown** returnMenu)
{
	// Build via the shared factory, then stash in `contextMenu` so the right-click
	// path (doContextMenu) can keep the menu alive across the caller's release.
	gmpi::shared_ptr<gmpi::api::IUnknown> tmp;
	if (gmpi::ReturnCode::Ok != gmpi::hosting::win32::createPlatformPopupMenu(
			getWindowHandle(), r, DipsToWindow._22, tmp.put()))
		return gmpi::ReturnCode::Fail;

	contextMenu = tmp.as<gmpi::api::IPopupMenu>();
	if (!contextMenu)
		return gmpi::ReturnCode::Fail;

	contextMenu->addRef(); // extra refcount so we keep ownership after the caller releases.
	*returnMenu = contextMenu.get();
	return gmpi::ReturnCode::Ok;
}

// =====================================================================
// Win32 modal-dialog ITextEdit implementation. Adapted from the legacy
// GmpiGuiHosting::PGCC_PlatformTextEntry but exposing the new
// gmpi::api::ITextEdit interface. Single source of truth — both
// DxDrawingFrameBase and SynthEditLib's DrawingFrameBase2 instantiate
// this via gmpi::hosting::win32::createPlatformTextEdit.
//
// The control is a one-line/multi-line EDIT inside an in-memory
// DLGTEMPLATE. The dialog proc captures the final text before EndDialog
// and reports it via ITextEditCallback::onChanged followed by onComplete.
// =====================================================================
namespace
{
	// Per-dialog state. We can't use the dialog's user data (DialogBoxIndirect
	// doesn't accept a param), so we stash a pointer in TLS for the lifetime of
	// showAsync. A modal dialog blocks reentry on the same thread, so a single
	// thread_local pointer is sufficient.
	struct GMPI_WIN_TextEditState
	{
		std::string text;
		bool multiline = false;
		int dialogX = 0;
		int dialogY = 0;
		int dialogW = 0;
		int dialogH = 0;
		HFONT dialogFont = nullptr;
	};
	thread_local GMPI_WIN_TextEditState* currentTextEditState = nullptr;

	constexpr int GMPI_TE_EDIT_ID = 100;

	BOOL CALLBACK gmpiTextEditDlgProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam)
	{
		HWND child = ::GetWindow(hwndDlg, GW_CHILD);

		switch (message)
		{
		case WM_NCACTIVATE: // <return>, click away
		case WM_NEXTDLGCTL: // <tab> key
			if (wParam == FALSE) // user clicked away
			{
				PostMessage(hwndDlg, WM_COMMAND, (WPARAM)IDOK, (LPARAM)0);
				return TRUE; // suppress default beep
			}
			break;

		case WM_ACTIVATE:
			if (LOWORD(wParam) == WA_INACTIVE)
			{
				PostMessage(hwndDlg, WM_COMMAND, (WPARAM)IDOK, (LPARAM)0);
				return TRUE;
			}
			else if (currentTextEditState)
			{
				SendMessage(child, WM_SETFONT, (WPARAM)currentTextEditState->dialogFont, MAKELPARAM(TRUE, 0));
				::SetWindowPos(hwndDlg, 0,
					currentTextEditState->dialogX, currentTextEditState->dialogY,
					currentTextEditState->dialogW, currentTextEditState->dialogH, SWP_NOZORDER);
				::SetWindowPos(child, 0, 0, 0,
					currentTextEditState->dialogW, currentTextEditState->dialogH, SWP_NOZORDER);

				const auto ws = privateStuff::Utf8ToWstring(currentTextEditState->text);
				::SetWindowText(child, ws.c_str());
				::SetFocus(child);
				SendMessage(child, EM_SETSEL, (WPARAM)0, (LPARAM)-1); // select-all
			}
			break;

		case WM_COMMAND:
			switch (LOWORD(wParam))
			{
			case IDOK:
			{
				if (currentTextEditState)
				{
					std::wstring buf;
					const size_t len = 1 + GetWindowTextLength(child);
					buf.resize(len);
					GetDlgItemText(hwndDlg, GMPI_TE_EDIT_ID, (LPWSTR)buf.data(), static_cast<int32_t>(len));
					if (!buf.empty() && buf.back() == 0)
						buf.pop_back();
					currentTextEditState->text = privateStuff::WStringToUtf8(buf);
				}
				EndDialog(hwndDlg, IDOK);
				return TRUE;
			}
			case IDCANCEL:
				EndDialog(hwndDlg, IDCANCEL);
				return TRUE;
			}
			break;
		}
		return FALSE;
	}

	// Aligns a memory pointer to a DWORD boundary — required when building a
	// DLGTEMPLATE in-memory because DLGITEMTEMPLATEs must be DWORD-aligned.
	inline LPWORD lpwAlign(LPWORD lpIn)
	{
		auto ptr = ((uintptr_t)lpIn + 3) & ~(uintptr_t)0x03;
		return (LPWORD)ptr;
	}

	class GMPI_WIN_TextEdit : public gmpi::api::ITextEdit
	{
		HWND parentWnd{};
		float dpiScale = 1.0f;
		float textHeight = 12.0f;
		int32_t alignment = 0;
		bool multiline = false;
		gmpi::drawing::Rect editRect{};
		std::string text;
		gmpi::shared_ptr<gmpi::api::ITextEditCallback> callback;

	public:
		GMPI_WIN_TextEdit(HWND pParentWnd, const gmpi::drawing::Rect* rect, float dpi)
			: parentWnd(pParentWnd)
			, dpiScale(dpi)
			, editRect(*rect)
		{
		}

		gmpi::ReturnCode setText(const char* ptext) override
		{
			text = ptext ? ptext : "";
			return gmpi::ReturnCode::Ok;
		}

		gmpi::ReturnCode setAlignment(int32_t palignment) override
		{
			alignment = palignment & 0x03;
			multiline = (palignment & static_cast<int32_t>(gmpi::api::TextMultilineFlag::MultiLine)) != 0;
			return gmpi::ReturnCode::Ok;
		}

		gmpi::ReturnCode setTextSize(float height) override
		{
			textHeight = height;
			return gmpi::ReturnCode::Ok;
		}

		gmpi::ReturnCode showAsync(gmpi::api::IUnknown* pcallback) override
		{
			// Self-extend lifetime across the modal dialog and the callback chain.
			// oldCb->OnComplete (invoked through the bridge inside our callback->
			// onComplete) typically releases the LegacyTextEditAdapter that owns
			// us (e.g. EditWidget::OnTextEnteredComplete sets nativeEdit=nullptr).
			// Without this addRef we get deleted while still on the call stack
			// inside this very showAsync — and `callback = {}` then crashes.
			// SelfReleaseGuard balances on every return path, including early ones.
			struct SelfReleaseGuard { GMPI_WIN_TextEdit* self; ~SelfReleaseGuard() { self->release(); } };
			addRef();
			SelfReleaseGuard guard{ this };

			if (pcallback)
				pcallback->queryInterface(&gmpi::api::ITextEditCallback::guid, callback.put_void());

			// Convert DIP rect to native pixels for placement.
			const auto nativeLeft   = editRect.left   * dpiScale;
			const auto nativeTop    = editRect.top    * dpiScale;
			const auto nativeRight  = editRect.right  * dpiScale;
			const auto nativeBottom = editRect.bottom * dpiScale;

			POINT clientOffset{ 0, 0 };
			ClientToScreen(parentWnd, &clientOffset);

			GMPI_WIN_TextEditState state;
			state.text = text;
			state.multiline = multiline;
			state.dialogX = clientOffset.x + static_cast<int32_t>(0.5f + nativeLeft);
			state.dialogY = clientOffset.y + static_cast<int32_t>(0.5f + nativeTop);
			state.dialogW = static_cast<int32_t>(0.5f + (nativeRight - nativeLeft));
			state.dialogH = static_cast<int32_t>(0.5f + (nativeBottom - nativeTop));
			const int gdiFontSize = -static_cast<int32_t>(0.5f + dpiScale * textHeight); // negative = pixel height
			state.dialogFont = CreateFont(gdiFontSize, 0, 0, 0, FW_DONTCARE, FALSE, FALSE, FALSE,
				0, 0, 0, 0, 0, NULL);

			currentTextEditState = &state;

			// Build an in-memory DLGTEMPLATE containing a single EDIT control. Using
			// DialogBoxIndirect rather than a resource ID keeps gmpi_ui free of any
			// .rc dependency.
			HGLOBAL hgbl = GlobalAlloc(GMEM_ZEROINIT, 1024);
			if (!hgbl)
			{
				DeleteObject(state.dialogFont);
				currentTextEditState = nullptr;
				return gmpi::ReturnCode::Fail;
			}

			LPDLGTEMPLATE lpdt = (LPDLGTEMPLATE)GlobalLock(hgbl);
			lpdt->style = DS_FIXEDSYS | WS_POPUP;
			lpdt->cdit  = 1;
			lpdt->x = 10; lpdt->y = 10;
			lpdt->cx = 100; lpdt->cy = 100;

			LPWORD lpw = (LPWORD)(lpdt + 1);
			*lpw++ = 0; // no menu
			*lpw++ = 0; // default class
			*lpw++ = 0; // empty title (single null wchar)

			lpw = lpwAlign(lpw); // DLGITEMTEMPLATE must be DWORD-aligned
			LPDLGITEMTEMPLATE lpdit = (LPDLGITEMTEMPLATE)lpw;
			lpdit->x  = 10; lpdit->y  = 10;
			lpdit->cx = 40; lpdit->cy = 20;
			lpdit->id = GMPI_TE_EDIT_ID;
			lpdit->dwExtendedStyle = WS_EX_CLIENTEDGE;
			lpdit->style = WS_CHILD | WS_VISIBLE;

			if (multiline)
				lpdit->style |= (ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN);
			else
				lpdit->style |= ES_AUTOHSCROLL;

			// Map gmpi::drawing::TextAlignment (Leading/Center/Trailing) to ES_*.
			switch (alignment)
			{
			case static_cast<int32_t>(gmpi::drawing::TextAlignment::Trailing):
				lpdit->style |= ES_RIGHT;
				break;
			case static_cast<int32_t>(gmpi::drawing::TextAlignment::Center):
				lpdit->style |= ES_CENTER;
				break;
			case static_cast<int32_t>(gmpi::drawing::TextAlignment::Leading):
			default:
				lpdit->style |= ES_LEFT;
				break;
			}

			lpw = (LPWORD)(lpdit + 1);
			*lpw++ = 0xFFFF;   // pre-defined class signature
			*lpw++ = 0x0081;   // EDIT class
			*lpw++ = 0;        // no creation data

			GlobalUnlock(hgbl);

			HMODULE hmodule = nullptr;
			GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
				(LPCWSTR)&gmpiTextEditDlgProc, &hmodule);

			const auto dr = DialogBoxIndirect((HINSTANCE)hmodule,
				(LPDLGTEMPLATE)hgbl,
				parentWnd,
				(DLGPROC)gmpiTextEditDlgProc);

			GlobalFree(hgbl);
			DeleteObject(state.dialogFont);

			text = state.text;
			currentTextEditState = nullptr;

			if (callback)
			{
				// Push final text before completion. The dialog proc accumulates the
				// text into state.text on IDOK; for cancel we leave the original.
				if (dr == IDOK)
					callback->onChanged(text.c_str());

				callback->onComplete(dr == IDOK ? gmpi::ReturnCode::Ok : gmpi::ReturnCode::Cancel);
				callback = {};
			}

			return gmpi::ReturnCode::Ok;
		}

		gmpi::ReturnCode queryInterface(const gmpi::api::Guid* iid, void** returnInterface) override
		{
			*returnInterface = {};
			GMPI_QUERYINTERFACE(gmpi::api::ITextEdit);
			return gmpi::ReturnCode::NoSupport;
		}
		GMPI_REFCOUNT
	};
} // anonymous namespace

// We are already inside `namespace gmpi { namespace hosting {` (opened at the
// top of this TU), so just open the inner win32 namespace here.
namespace win32 {

gmpi::ReturnCode createPlatformTextEdit(
	HWND parentWnd,
	const gmpi::drawing::Rect* rect,
	float dpiScale,
	gmpi::api::IUnknown** returnTextEdit)
{
	if (!rect || !returnTextEdit)
		return gmpi::ReturnCode::Fail;

	// GMPI_REFCOUNT initializes refCount2_ = 1, so the freshly-`new`'d object
	// already holds the single refcount we hand to the caller. No extra addRef.
	auto* edit = new GMPI_WIN_TextEdit(parentWnd, rect, dpiScale);
	*returnTextEdit = static_cast<gmpi::api::ITextEdit*>(edit);
	return gmpi::ReturnCode::Ok;
}

} // namespace win32

gmpi::ReturnCode DxDrawingFrameBase::createKeyListener(const gmpi::drawing::Rect* r, gmpi::api::IUnknown** returnKeyListener)
{
	*returnKeyListener = new gmpi::hosting::win32::PlatformKeyListener(getWindowHandle(), r);
	return gmpi::ReturnCode::Ok;
}

class GMPI_WIN_FileDialog : public gmpi::api::IFileDialog
{
	HWND parentWnd;
	gmpi::api::FileDialogType dialogType;
	std::vector<std::pair<std::wstring, std::wstring>> extensions; // extension, description
	std::wstring initialFilename;
	std::wstring initialDirectory;

public:
	GMPI_WIN_FileDialog(HWND pParentWnd, gmpi::api::FileDialogType type) :
		parentWnd(pParentWnd)
		, dialogType(type)
	{}

	gmpi::ReturnCode addExtension(const char* extension, const char* description) override
	{
		const auto wExt = privateStuff::Utf8ToWstring(extension);
		const auto wDesc = privateStuff::Utf8ToWstring(description);
		extensions.push_back({ wExt, wDesc });
		return gmpi::ReturnCode::Ok;
	}

	gmpi::ReturnCode setInitialFilename(const char* text) override
	{
		initialFilename = privateStuff::Utf8ToWstring(text);
		return gmpi::ReturnCode::Ok;
	}

	gmpi::ReturnCode setInitialDirectory(const char* text) override
	{
		initialDirectory = privateStuff::Utf8ToWstring(text);
		return gmpi::ReturnCode::Ok;
	}

	gmpi::ReturnCode showAsync(const gmpi::drawing::Rect* rect, gmpi::api::IUnknown* callback) override
	{
		gmpi::shared_ptr<gmpi::api::IUnknown> unknown;
		unknown = callback;
		auto fileCallback = unknown.as<gmpi::api::IFileDialogCallback>();
		if (!fileCallback)
			return gmpi::ReturnCode::Fail;

		if (dialogType == gmpi::api::FileDialogType::Folder)
		{
			return showFolderDialog(fileCallback.get());
		}

		const bool isSave = (dialogType == gmpi::api::FileDialogType::Save);
		Microsoft::WRL::ComPtr<::IFileDialog> dialog;
		HRESULT hr;
		if (isSave)
			hr = CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog));
		else
			hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog));

		if (FAILED(hr))
		{
			fileCallback->onComplete(gmpi::ReturnCode::Fail, "");
			return gmpi::ReturnCode::Ok;
		}

		// Set file type filters
		if (!extensions.empty())
		{
			std::vector<COMDLG_FILTERSPEC> filters;
			std::vector<std::wstring> specs; // keep wstrings alive
			for (const auto& [ext, desc] : extensions)
			{
				std::wstring spec = L"*." + ext;
				if (ext == L"*")
					spec = L"*.*";
				specs.push_back(spec);
				const auto& descStr = desc.empty() ? ext : desc;
				filters.push_back({ descStr.c_str(), specs.back().c_str() });
			}
			dialog->SetFileTypes(static_cast<UINT>(filters.size()), filters.data());
		}

		if (!initialFilename.empty())
			dialog->SetFileName(initialFilename.c_str());

		if (!initialDirectory.empty())
		{
			Microsoft::WRL::ComPtr<::IShellItem> folder;
			if (SUCCEEDED(SHCreateItemFromParsingName(initialDirectory.c_str(), nullptr, IID_PPV_ARGS(&folder))))
				dialog->SetFolder(folder.Get());
		}

		hr = dialog->Show(parentWnd);
		if (FAILED(hr))
		{
			fileCallback->onComplete(gmpi::ReturnCode::Cancel, "");
			return gmpi::ReturnCode::Ok;
		}

		Microsoft::WRL::ComPtr<::IShellItem> result;
		hr = dialog->GetResult(&result);
		if (FAILED(hr))
		{
			fileCallback->onComplete(gmpi::ReturnCode::Fail, "");
			return gmpi::ReturnCode::Ok;
		}

		PWSTR filePath = nullptr;
		hr = result->GetDisplayName(SIGDN_FILESYSPATH, &filePath);
		if (SUCCEEDED(hr) && filePath)
		{
			auto utf8Path = privateStuff::WStringToUtf8(filePath);
			CoTaskMemFree(filePath);
			fileCallback->onComplete(gmpi::ReturnCode::Ok, utf8Path.c_str());
		}
		else
		{
			fileCallback->onComplete(gmpi::ReturnCode::Fail, "");
		}

		return gmpi::ReturnCode::Ok;
	}

	gmpi::ReturnCode queryInterface(const gmpi::api::Guid* iid, void** returnInterface) override
	{
		*returnInterface = {};
		GMPI_QUERYINTERFACE(gmpi::api::IFileDialog);
		return gmpi::ReturnCode::NoSupport;
	}
	GMPI_REFCOUNT

private:
	gmpi::ReturnCode showFolderDialog(gmpi::api::IFileDialogCallback* fileCallback)
	{
		Microsoft::WRL::ComPtr<::IFileOpenDialog> dialog;
		HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog));
		if (FAILED(hr))
		{
			fileCallback->onComplete(gmpi::ReturnCode::Fail, "");
			return gmpi::ReturnCode::Ok;
		}

		DWORD options = 0;
		dialog->GetOptions(&options);
		dialog->SetOptions(options | FOS_PICKFOLDERS);

		if (!initialDirectory.empty())
		{
			Microsoft::WRL::ComPtr<::IShellItem> folder;
			if (SUCCEEDED(SHCreateItemFromParsingName(initialDirectory.c_str(), nullptr, IID_PPV_ARGS(&folder))))
				dialog->SetFolder(folder.Get());
		}

		hr = dialog->Show(parentWnd);
		if (FAILED(hr))
		{
			fileCallback->onComplete(gmpi::ReturnCode::Cancel, "");
			return gmpi::ReturnCode::Ok;
		}

		Microsoft::WRL::ComPtr<::IShellItem> result;
		hr = dialog->GetResult(&result);
		if (FAILED(hr))
		{
			fileCallback->onComplete(gmpi::ReturnCode::Fail, "");
			return gmpi::ReturnCode::Ok;
		}

		PWSTR folderPath = nullptr;
		hr = result->GetDisplayName(SIGDN_FILESYSPATH, &folderPath);
		if (SUCCEEDED(hr) && folderPath)
		{
			auto utf8Path = privateStuff::WStringToUtf8(folderPath);
			CoTaskMemFree(folderPath);
			fileCallback->onComplete(gmpi::ReturnCode::Ok, utf8Path.c_str());
		}
		else
		{
			fileCallback->onComplete(gmpi::ReturnCode::Fail, "");
		}

		return gmpi::ReturnCode::Ok;
	}

};

gmpi::ReturnCode DxDrawingFrameBase::createFileDialog(int32_t dialogType, gmpi::api::IUnknown** returnDialog)
{
	// Delegate to the shared factory so SynthEditLib's DrawingFrameBase2 hits the
	// same code path.
	return gmpi::hosting::win32::createPlatformFileDialog(getWindowHandle(), dialogType, returnDialog);
}

// ---------------------------------------------------------------------
// Shared Win32 dialog factories. Counterparts to createPlatformTextEdit
// — both DxDrawingFrameBase (gmpi_ui) and DrawingFrameBase2 (SynthEditLib)
// route here so the GMPI_WIN_FileDialog / GMPI_WIN_StockDialog classes
// remain the single source of truth and stay file-scope private.
// ---------------------------------------------------------------------
namespace win32 {

gmpi::ReturnCode createPlatformFileDialog(
	HWND parentWnd,
	int32_t dialogType,
	gmpi::api::IUnknown** returnDialog)
{
	if (!returnDialog)
		return gmpi::ReturnCode::Fail;

	// GMPI_REFCOUNT initializes refCount2_ = 1, so the freshly-`new`'d object
	// already holds the single refcount we hand to the caller.
	*returnDialog = new GMPI_WIN_FileDialog(parentWnd, static_cast<gmpi::api::FileDialogType>(dialogType));
	return gmpi::ReturnCode::Ok;
}

gmpi::ReturnCode createPlatformStockDialog(
	HWND parentWnd,
	int32_t dialogType,
	const char* title,
	const char* text,
	gmpi::api::IUnknown** returnDialog)
{
	if (!returnDialog)
		return gmpi::ReturnCode::Fail;

	*returnDialog = new GMPI_WIN_StockDialog(parentWnd,
		static_cast<gmpi::api::StockDialogType>(dialogType),
		title ? title : "",
		text ? text : "");
	return gmpi::ReturnCode::Ok;
}

gmpi::ReturnCode createPlatformPopupMenu(
	HWND parentWnd,
	const gmpi::drawing::Rect* rect,
	float dpiScale,
	gmpi::api::IUnknown** returnMenu)
{
	if (!rect || !returnMenu)
		return gmpi::ReturnCode::Fail;

	// GMPI_REFCOUNT initializes refCount2_ = 1, so the freshly-`new`'d object
	// already holds the single refcount we hand to the caller.
	auto* menu = new GMPI_WIN_PopupMenu(parentWnd, rect, dpiScale);
	*returnMenu = static_cast<gmpi::api::IPopupMenu*>(menu);
	return gmpi::ReturnCode::Ok;
}

} // namespace win32
} //namespace
} //namespace

#endif // skip compilation on macOS

