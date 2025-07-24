#if defined(_WIN32) // skip compilation on macOS

#include <d2d1_2.h>
#include <d3d11_1.h>
#include <dxgi1_6.h>
#include <wrl.h> // Comptr
#include <Windowsx.h>
#include <commctrl.h>
#include "./DrawingFrameWin.h"

using namespace std;
using namespace gmpi;
using namespace gmpi::drawing;

//using namespace Microsoft::WRL;
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
		const auto scale = getRasterizationScale();

		sizeClientDips(
			static_cast<float>(r.right - r.left) * scale,
			static_cast<float>(r.bottom - r.top) * scale);
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
	if (tooltipWindow == nullptr && getWindowHandle())
	{
		auto instanceHandle = getDllHandle();
		{
			TOOLINFO ti{};

			// Create the ToolTip control.
			HWND hwndTT = CreateWindow(TOOLTIPS_CLASS, TEXT(""),
				WS_POPUP,
				CW_USEDEFAULT, CW_USEDEFAULT,
				CW_USEDEFAULT, CW_USEDEFAULT,
				NULL, (HMENU)NULL, instanceHandle,
				NULL);

			// Prepare TOOLINFO structure for use as tracking ToolTip.
			ti.cbSize = TTTOOLINFO_V1_SIZE; // win 7 compatible. sizeof(TOOLINFO);
			ti.uFlags = TTF_SUBCLASS;
			ti.hwnd = (HWND)getWindowHandle();
			ti.uId = (UINT)0;
			ti.hinst = instanceHandle;
			ti.lpszText = const_cast<TCHAR*> (TEXT("This is a tooltip"));
			ti.rect.left = ti.rect.top = ti.rect.bottom = ti.rect.right = 0;

			// Add the tool to the control
			if (!SendMessage(hwndTT, TTM_ADDTOOL, 0, (LPARAM)&ti))
			{
				DestroyWindow(hwndTT);
				return;
			}

			tooltipWindow = hwndTT;
		}
	}
}

void DxDrawingFrameBase::TooltipOnMouseActivity()
{
	if(toolTipShown)
	{
		if (toolTiptimer < -20) // ignore spurious MouseMove when Tooltip shows
		{
			HideToolTip();
			toolTiptimer = toolTiptimerInit;
		}
	}
	else
		toolTiptimer = toolTiptimerInit;
}

void DxDrawingFrameBase::ShowToolTip()
{
//	_RPT0(_CRT_WARN, "YEAH!\n");

	//UTF8StringHelper tooltipText(tooltip);
	//if (platformObject)
	{
		auto platformObject = tooltipWindow;

		RECT rc;
		rc.left = (LONG)0;
		rc.top = (LONG)0;
		rc.right = (LONG)100000;
		rc.bottom = (LONG)100000;
		TOOLINFO ti = { 0 };
		ti.cbSize = TTTOOLINFO_V1_SIZE; // win 7 compatible. sizeof(TOOLINFO);
		ti.hwnd = (HWND)getWindowHandle(); // frame->getSystemWindow();
		ti.uId = 0;
		ti.rect = rc;
		ti.lpszText = (TCHAR*)(const TCHAR*)toolTipText.c_str();
		SendMessage((HWND)platformObject, TTM_UPDATETIPTEXT, 0, (LPARAM)&ti);
		SendMessage((HWND)platformObject, TTM_NEWTOOLRECT, 0, (LPARAM)&ti);
		SendMessage((HWND)platformObject, TTM_POPUP, 0, 0);
	}

	toolTipShown = true;
}

void DxDrawingFrameBase::HideToolTip()
{
	toolTipShown = false;
//	_RPT0(_CRT_WARN, "NUH!\n");

	if (tooltipWindow)
	{
		TOOLINFO ti = { 0 };
		ti.cbSize = TTTOOLINFO_V1_SIZE; // win 7 compatible. sizeof(TOOLINFO);
		ti.hwnd = (HWND)getWindowHandle(); // frame->getSystemWindow();
		ti.uId = 0;
		ti.lpszText = 0;
		SendMessage((HWND)tooltipWindow, TTM_UPDATETIPTEXT, 0, (LPARAM)&ti);
		SendMessage((HWND)tooltipWindow, TTM_POP, 0, 0);
	}
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
				cubaseBugPreviousMouseMove = Point{ -1, -1 };
			}

			TooltipOnMouseActivity();

			int32_t flags = gmpi::api::GG_POINTER_FLAG_INCONTACT | gmpi::api::GG_POINTER_FLAG_PRIMARY | gmpi::api::GG_POINTER_FLAG_CONFIDENCE;

			switch (message)
			{
				case WM_MBUTTONDOWN:
				case WM_LBUTTONDOWN:
				case WM_RBUTTONDOWN:
					flags |= gmpi::api::GG_POINTER_FLAG_NEW;
					break;
			}

			switch (message)
			{
			case WM_LBUTTONUP:
			case WM_LBUTTONDOWN:
				flags |= gmpi::api::GG_POINTER_FLAG_FIRSTBUTTON;
				break;
			case WM_RBUTTONDOWN:
			case WM_RBUTTONUP:
				flags |= gmpi::api::GG_POINTER_FLAG_SECONDBUTTON;
				break;
			case WM_MBUTTONDOWN:
			case WM_MBUTTONUP:
				flags |= gmpi::api::GG_POINTER_FLAG_THIRDBUTTON;
				break;
			}

			if (GetKeyState(VK_SHIFT) < 0)
			{
				flags |= gmpi::api::GG_POINTER_KEY_SHIFT;
			}
			if (GetKeyState(VK_CONTROL) < 0)
			{
				flags |= gmpi::api::GG_POINTER_KEY_CONTROL;
			}
			if (GetKeyState(VK_MENU) < 0)
			{
				flags |= gmpi::api::GG_POINTER_KEY_ALT;
			}

			gmpi::ReturnCode r;
			switch (message)
			{
			case WM_MOUSEMOVE:
				{
					r = inputClient->onPointerMove(p, flags);

					// get notified when mouse leaves window
					if (!isTrackingMouse)
					{
						TRACKMOUSEEVENT tme{};
						tme.cbSize = sizeof(TRACKMOUSEEVENT);
						tme.dwFlags = TME_LEAVE;
						tme.hwndTrack = hwnd;

						if (::TrackMouseEvent(&tme))
						{
							isTrackingMouse = true;
						}
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
			// supplied point is relative to *screen* not window.
			POINT pos = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
			MapWindowPoints(NULL, getWindowHandle(), &pos, 1); // !!! ::ScreenToClient() might be more correct. ref MyFrameWndDirectX::OnMouseWheel

			Point p{ static_cast<float>(pos.x), static_cast<float>(pos.y) };
			p = transformPoint(WindowToDips, p);

            //The wheel rotation will be a multiple of WHEEL_DELTA, which is set at 120. This is the threshold for action to be taken, and one such action (for example, scrolling one increment) should occur for each delta.
			const auto zDelta = GET_WHEEL_DELTA_WPARAM(wParam);

			int32_t flags = gmpi::api::GG_POINTER_FLAG_PRIMARY | gmpi::api::GG_POINTER_FLAG_CONFIDENCE;

			if (WM_MOUSEHWHEEL == message)
				flags |= gmpi::api::GG_POINTER_SCROLL_HORIZ;

			const auto fwKeys = GET_KEYSTATE_WPARAM(wParam);
			if (MK_SHIFT & fwKeys)
			{
				flags |= gmpi::api::GG_POINTER_KEY_SHIFT;
			}
			if (MK_CONTROL & fwKeys)
			{
				flags |= gmpi::api::GG_POINTER_KEY_CONTROL;
			}
			//if (GetKeyState(VK_MENU) < 0)
			//{
			//	flags |= gmpi::api::GG_POINTER_KEY_ALT;
			//}

			/*auto r =*/ inputClient->onMouseWheel(p, flags, zDelta);
		}
		break;

	case WM_CHAR:
		if(inputClient)
			inputClient->OnKeyPress((wchar_t) wParam);
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
				inputClient->OnKeyPress((wchar_t)wParam);
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
		frameUpdateClient->PreGraphicsRedraw();
	}

	// Queue pending drawing updates to backbuffer.
	const BOOL bErase = FALSE;

	for (auto& invalidRect : backBufferDirtyRects)
	{
		::InvalidateRect(hwnd, reinterpret_cast<RECT*>(&invalidRect), bErase);
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
		drawingClient->open(static_cast<gmpi::api::IDrawingHost*>(this));
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

void DxDrawingFrameBase::OnPaint()
{
	// First clear update region (else windows will pound on this repeatedly).
	updateRegion_native.copyDirtyRects(getWindowHandle(), swapChainSize);
	ValidateRect(getWindowHandle(), NULL); // Clear invalid region for next frame.

	auto& dirtyRects = updateRegion_native.getUpdateRects();

	// prevent infinite assert dialog boxes when assert happens during painting.
	if (monitorChanged || reentrant || !drawingClient || dirtyRects.empty())
	{
		return;
	}
	reentrant = true;

	//	_RPT1(_CRT_WARN, "OnPaint(); %d dirtyRects\n", dirtyRects.size() );
#if 0
	if (!d2dDeviceContext) // not quite right, also need to re-create any resources (brushes etc) else most object draw blank. Could refresh the view in this case.
	{
		CreateSwapPanel(DrawingFactory.getD2dFactory());
	}
#endif

	gmpi::directx::ComPtr <ID2D1DeviceContext> deviceContext;

	gmpi::directx::ComPtr<ID2D1Bitmap> pSourceBitmap;
	if (hdrRenderTarget) // draw onto intermediate buffer, then pass that through an effect to scale white.
	{
		d2dDeviceContext->BeginDraw();
		deviceContext = hdrRenderTargetDC; // hdrRenderTarget.as<ID2D1DeviceContext>();
	}
	else // draw directly on the swapchain bitmap.
	{
		deviceContext = d2dDeviceContext;
	}

	{
		gmpi::directx::GraphicsContext context1(deviceContext.get(), &DrawingFactory);
		Graphics graphics(&context1);

		graphics.beginDraw();
		graphics.setTransform(viewTransform);

		// clip and draw each rect individually (causes some objects to redraw several times)
		for (auto& r : dirtyRects)
		{
			auto r2 = transformRect(WindowToDips, drawing::Rect{ static_cast<float>(r.left), static_cast<float>(r.top), static_cast<float>(r.right), static_cast<float>(r.bottom) });

			// Snap to whole DIPs.
			drawing::Rect temp;
			temp.left = floorf(r2.left);
			temp.top = floorf(r2.top);
			temp.right = ceilf(r2.right);
			temp.bottom = ceilf(r2.bottom);

			graphics.pushAxisAlignedClip(temp);

			drawingClient->render(&context1);

			graphics.popAxisAlignedClip();
		}

		// Print OS Version.

		// Print Frame Rate
//		const bool displayFrameRate = true;
		const bool displayFrameRate = false;
		//		static int presentTimeMs = 0;
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

				//				sprintf(frameCountString, "%3.1f FPS. %dms PT", frameRate, presentTimeMs);
				sprintf_s(frameCountString, sizeof(frameCountString), "%3.1f FPS", frameRate);
				frameCountTime = timenow;
				frameCount = 0;

				auto brush = graphics.createSolidColorBrush(Colors::Black);
				auto fpsRect = drawing::Rect{ 0, 0, 50, 18 };
				graphics.fillRectangle(fpsRect, brush);
				brush.setColor(Colors::White);
				graphics.drawTextU(frameCountString, graphics.getFactory().createTextFormat(12), fpsRect, brush);

				dirtyRects.push_back({ 0, 0, 100, 36 });
			}
		}

		graphics.endDraw();
	}

	// draw the filtered intermediate buffer onto the swapchain.
	if (hdrRenderTarget)
	{
		assert(hdrWhiteScaleEffect);

		// Draw the effect output to the screen
		const D2D1_RECT_F destRect(0, 0, static_cast<float>(swapChainSize.width), static_cast<float>(swapChainSize.height));
		d2dDeviceContext->DrawImage(
			  hdrWhiteScaleEffect.get()
			, D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR
			, D2D1_COMPOSITE_MODE_SOURCE_COPY
		);

		d2dDeviceContext->EndDraw();
	}

	// Present the backbuffer (if it has some new content)
	if (firstPresent)
	{
		firstPresent = false;
		const auto hr = swapChain->Present(1, 0);
		if (S_OK != hr && DXGI_STATUS_OCCLUDED != hr)
		{
			// DXGI_ERROR_INVALID_CALL 0x887A0001L
			ReleaseDevice();
		}
	}
	else
	{
		HRESULT hr = S_OK;
		{
			assert(!dirtyRects.empty());
			DXGI_PRESENT_PARAMETERS presetParameters{ (UINT)dirtyRects.size(), reinterpret_cast<RECT*>(dirtyRects.data()), nullptr, nullptr, };
/*
			presetParameters.pScrollRect = nullptr;
			presetParameters.pScrollOffset = nullptr;
			presetParameters.DirtyRectsCount = (UINT) dirtyRects.size();
			presetParameters.pDirtyRects = reinterpret_cast<RECT*>(dirtyRects.data()); // should be exact same layout.
*/
			// checkout DXGI_PRESENT_DO_NOT_WAIT
//				hr = m_swapChain->Present1(1, DXGI_PRESENT_TEST, &presetParameters);
//				_RPT1(_CRT_WARN, "Present1() test = %x\n", hr);
/* NEVER returns DXGI_ERROR_WAS_STILL_DRAWING
//			_RPT1(_CRT_WARN, "Present1() DirtyRectsCount = %d\n", presetParameters.DirtyRectsCount);
			hr = m_swapChain->Present1(1, DXGI_PRESENT_DO_NOT_WAIT, &presetParameters);
			if (hr == DXGI_ERROR_WAS_STILL_DRAWING)
			{
				_RPT1(_CRT_WARN, "Present1() Blocked\n", hr);
*/
				// Present(0... improves framerate only from 60 -> 64 FPS, so must be blocking a little with "1".
//				auto timeA = std::chrono::steady_clock::now();
			hr = swapChain->Present1(1, 0, &presetParameters);
			//auto elapsed = std::chrono::steady_clock::now() - timeA;
			//presentTimeMs = (float)std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
//				}
/* could put this in timer to reduce blocking, agregating dirty rects until call successful.
*/			}

		if (S_OK != hr && DXGI_STATUS_OCCLUDED != hr)
		{
			// DXGI_ERROR_INVALID_CALL 0x887A0001L
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
	const bool useDeepColor = DX_support_sRGB || useSoftwareRenderer;

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
			,D2D1_ALPHA_MODE_UNKNOWN
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

		const auto scale = getRasterizationScale();

		sizeClientDips(
			static_cast<float>(width) * scale,
			static_cast<float>(height) * scale
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

// see also DrawingFrameHwndBase::calcWhiteLevel()
float DrawingFrame::calcWhiteLevel()
{
	return gmpi::hosting::calcWhiteLevelForHwnd(getWindowHandle());
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
	drawing::RectL r;
	if (invalidRect)
	{
		//_RPT4(_CRT_WARN, "invalidateRect r[ %d %d %d %d]\n", (int)invalidRect->left, (int)invalidRect->top, (int)invalidRect->right, (int)invalidRect->bottom);
		r = RectToIntegerLarger( transformRect(DipsToWindow , *invalidRect) );
	}
	else
	{
		GetClientRect(getWindowHandle(), reinterpret_cast<RECT*>( &r ));
	}

	auto area1 = getWidth(r) * getHeight(r);

	for (auto& dirtyRect : backBufferDirtyRects )
	{
		auto area2 = getWidth(dirtyRect) * getHeight(dirtyRect);

		drawing::RectL unionrect(dirtyRect);

		unionrect.top = (std::min)(unionrect.top, r.top);
		unionrect.bottom = (std::max)(unionrect.bottom, r.bottom);
		unionrect.left = (std::min)(unionrect.left, r.left);
		unionrect.right = (std::max)(unionrect.right, r.right);

		auto unionarea = getWidth(unionrect) * getHeight(unionrect);

		if (unionarea <= area1 + area2)
		{
			// replace existing rect with combined rect
			dirtyRect = unionrect;
			return;
			break;
		}
	}

	// no optimisation found, add new rect.
	backBufferDirtyRects.push_back(r);
}

gmpi::ReturnCode DxDrawingFrameBase::setCapture()
{
	::SetCapture(getWindowHandle());
	return gmpi::ReturnCode::Ok;
}

gmpi::ReturnCode DxDrawingFrameBase::getCapture(bool& returnValue)
{
	returnValue = ::GetCapture() == getWindowHandle();
	return gmpi::ReturnCode::Ok;
}

gmpi::ReturnCode DxDrawingFrameBase::releaseCapture()
{
	::ReleaseCapture();
	return gmpi::ReturnCode::Ok;
}

gmpi::ReturnCode DxDrawingFrameBase::getFocus()
{
	::SetFocus(getWindowHandle());
	return gmpi::ReturnCode::Ok;
}

gmpi::ReturnCode DxDrawingFrameBase::releaseFocus()
{
	return gmpi::ReturnCode::Ok;
}

// IDialogHost
gmpi::ReturnCode DxDrawingFrameBase::createStockDialog(int32_t dialogType, gmpi::api::IUnknown** returnDialog)
{
	return gmpi::ReturnCode::NoSupport;
}
gmpi::ReturnCode DxDrawingFrameBase::createTextEdit(const gmpi::drawing::Rect* r, gmpi::api::IUnknown** returnTextEdit)
{
	return gmpi::ReturnCode::NoSupport;
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
		style, 0, 0, bounds.right - bounds.left, bounds.bottom - bounds.top,
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
			flags |= gmpi::api::GG_POINTER_KEY_SHIFT;
		}
		if (GetKeyState(VK_CONTROL) < 0)
		{
			flags |= gmpi::api::GG_POINTER_KEY_CONTROL;
		}
		if (GetKeyState(VK_MENU) < 0) // don't work.
		{
			flags |= gmpi::api::GG_POINTER_KEY_ALT;
		}

		callback2->onKeyDown(key[0], flags);
		break;
	}
	break;
	}

	return DefWindowProc(hwnd, message, wParam, lParam);
}
} // namespace win32

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

	gmpi::ReturnCode addItem(const char* text, int32_t id, int32_t flags) override
	{
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
			AppendMenu(hmenus.back(), nativeFlags, menuIds.size(), privateStuff::Utf8ToWstring(text).c_str());
		}

		return gmpi::ReturnCode::Ok;
	}

	gmpi::ReturnCode showAsync(gmpi::api::IUnknown* returnCallback) override
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

		gmpi::shared_ptr<gmpi::api::IUnknown> unknown(returnCallback);
		if(auto callback = unknown.as<gmpi::api::IPopupMenuCallback>(); callback)
		{
			callback->onComplete(index >= 0 ? gmpi::ReturnCode::Ok : gmpi::ReturnCode::Cancel, selectedId);
		}
#if 0 // not required
		else if (auto callback = unknown.as<gmpi::api::ILegacyCompletionCallback>(); callback)
		{
			callback->OnComplete(index >= 0 ? gmpi::ReturnCode::Ok : gmpi::ReturnCode::Cancel);
		}
#endif
		return gmpi::ReturnCode::Ok;
	}

	gmpi::ReturnCode setAlignment(int32_t alignment) override
	{
#if 0 // TODO
		switch (alignment)
		{
		case GmpiDrawing_API::MP1_TEXT_ALIGNMENT_LEADING:
			align = TPM_LEFTALIGN;
			break;
		case GmpiDrawing_API::MP1_TEXT_ALIGNMENT_CENTER:
			align = TPM_CENTERALIGN;
			break;
		case GmpiDrawing_API::MP1_TEXT_ALIGNMENT_TRAILING:
		default:
			align = TPM_RIGHTALIGN;
			break;
		}
#endif
		return gmpi::ReturnCode::Ok;
	}

	GMPI_QUERYINTERFACE_METHOD(gmpi::api::IPopupMenu);
	GMPI_REFCOUNT;
};

gmpi::ReturnCode DxDrawingFrameBase::createPopupMenu(const gmpi::drawing::Rect* r, gmpi::api::IUnknown** returnMenu)
{
	contextMenu.attach(new GMPI_WIN_PopupMenu(getWindowHandle(), r, DipsToWindow._22));
	contextMenu->addRef(); // add an extra refcount so i can own it after caller releases.

	*returnMenu = contextMenu.get();
	return gmpi::ReturnCode::Ok;
}

gmpi::ReturnCode DxDrawingFrameBase::createKeyListener(const gmpi::drawing::Rect* r, gmpi::api::IUnknown** returnKeyListener)
{
	*returnKeyListener = new gmpi::hosting::win32::PlatformKeyListener(getWindowHandle(), r);
	return gmpi::ReturnCode::Ok;
}

gmpi::ReturnCode DxDrawingFrameBase::createFileDialog(int32_t dialogType, gmpi::api::IUnknown** returnDialog)
{
	return gmpi::ReturnCode::NoSupport;
}
} //namespace
} //namespace

#endif // skip compilation on macOS


/*
*  child window DPI awareness, needs to be TOP level window I think, not the frame.
*     // Store the current thread's DPI-awareness context. Windows 10 only
	// DPI_AWARENESS_CONTEXT previousDpiContext = SetThreadDpiAwarenessContext(context);
	//HINSTANCE h_kernal = GetModuleHandle(_T("Kernel32.DLL"));
	HMODULE hUser32 = GetModuleHandleW(L"user32");
	typedef int32_t DPI_AWARENESS_CONTEXT;
	typedef int32_t DPI_HOSTING_BEHAVIOR;
	typedef DPI_AWARENESS_CONTEXT ( WINAPI * PFN_SETTHREADDPIAWARENESS)(DPI_AWARENESS_CONTEXT param);
	PFN_SETTHREADDPIAWARENESS p_SetThreadDpiAwarenessContext = (PFN_SETTHREADDPIAWARENESS)GetProcAddress(hUser32, "SetThreadDpiAwarenessContext");
	PFN_SETTHREADDPIAWARENESS p_SetThreadDpiHostingBehavior = (PFN_SETTHREADDPIAWARENESS)GetProcAddress(hUser32, "SetThreadDpiHostingBehavior");
	DPI_AWARENESS_CONTEXT previousDpiContext = {};
	if(p_SetThreadDpiAwarenessContext)
	{
		// DPI_AWARENESS_CONTEXT_SYSTEM_AWARE - System DPI aware. This window does not scale for DPI changes. It will query for the DPI once and use that value for the lifetime of the process.
		const auto DPI_AWARENESS_CONTEXT_SYSTEM_AWARE = (DPI_AWARENESS_CONTEXT) -1;
		previousDpiContext = p_SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_SYSTEM_AWARE);
	}
	struct CreateParams
	{
		BOOL bEnableNonClientDpiScaling;
		BOOL bChildWindowDpiIsolation;
	};
	CreateParams createParams{TRUE, TRUE};

	// Windows 10 (1803) supports child-HWND DPI-mode isolation. This enables
	// child HWNDs to run in DPI-scaling modes that are isolated from that of
	// their parent (or host) HWND. Without child-HWND DPI isolation, all HWNDs
	// in an HWND tree must have the same DPI-scaling mode.
	DPI_HOSTING_BEHAVIOR previousDpiHostingBehavior = {};
	if (p_SetThreadDpiAwarenessContext) // &&bChildWindowDpiIsolation)
	{
		const int32_t DPI_HOSTING_BEHAVIOR_MIXED = 2;
		previousDpiHostingBehavior = p_SetThreadDpiHostingBehavior(DPI_HOSTING_BEHAVIOR_MIXED);
	}

	windowHandle = CreateWindowEx(extended_style, gClassName, L"",
		style, 0, 0, r.right - r.left, r.bottom - r.top,
		parentWnd, NULL, getDllHandle(), &createParams);// NULL);

	if (windowHandle)
	{
		SetWindowLongPtr(windowHandle, GWLP_USERDATA, (__int3264)(LONG_PTR)this);
		//		RegisterDragDrop(windowHandle, new CDropTarget(this));

		// Restore the current thread's DPI awareness context
		if(p_SetThreadDpiAwarenessContext)
		{
			p_SetThreadDpiAwarenessContext(previousDpiContext);

			// Restore the current thread DPI hosting behavior, if we changed it.
			//if(bChildWindowDpiIsolation)
			{
				p_SetThreadDpiHostingBehavior(previousDpiHostingBehavior);
			}
		}

		CreateRenderTarget();

		initTooltip();
	}
*/
