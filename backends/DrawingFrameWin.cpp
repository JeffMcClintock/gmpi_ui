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

using namespace Microsoft::WRL;
using namespace D2D1;

namespace gmpi
{
namespace hosting //GmpiGuiHosting
{
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

bool registeredWindowClass = false;
WNDCLASS windowClass;
wchar_t gClassName[100];

void DrawingFrame::open(void* pParentWnd, const gmpi::drawing::SizeL* overrideSize)
{
	parentWnd = (HWND)pParentWnd;

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

	int style = WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS;// | WS_OVERLAPPEDWINDOW;
	int extended_style = 0;

	windowHandle = CreateWindowEx(extended_style, gClassName, L"",
		style, 0, 0, r.right - r.left, r.bottom - r.top,
		parentWnd, NULL, getDllHandle(), NULL);

	if (windowHandle)
	{
		SetWindowLongPtr(windowHandle, GWLP_USERDATA, (__int3264)(LONG_PTR)this);
		//		RegisterDragDrop(windowHandle, new CDropTarget(this));

		CreateSwapPanel();

		initTooltip();

		int dpiX, dpiY;
		{
			HDC hdc = ::GetDC(windowHandle);
			dpiX = GetDeviceCaps(hdc, LOGPIXELSX);
			dpiY = GetDeviceCaps(hdc, LOGPIXELSY);
			::ReleaseDC(windowHandle, hdc);
		}

		const drawing::Size available{
			static_cast<float>(((r.right - r.left) * 96) / dpiX),
			static_cast<float>(((r.bottom - r.top) * 96) / dpiY)
		};

		drawing::Size desired{};
#ifdef GMPI_HOST_POINTER_SUPPORT
		gmpi_gui_client->measure(available, &desired);
		gmpi_gui_client->arrange({ 0, 0, available.width, available.height });
#endif
		if(drawingClient)
		{
			drawingClient->measure(&available, &desired);
			const drawing::Rect finalRect{ 0, 0, available.width, available.height };
			drawingClient->arrange(&finalRect);
		}
		// starting Timer latest to avoid first event getting 'in-between' other init events.
		startTimer(15); // 16.66 = 60Hz. 16ms timer seems to miss v-sync. Faster timers offer no improvement to framerate.
	}
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
#ifdef GMPI_HOST_POINTER_SUPPORT
		!gmpi_gui_client &&
#endif
		!drawingClient
		)
		return DefWindowProc(hwnd, message, wParam, lParam);

	switch (message)
	{
#if 1 //def GMPI_HOST_POINTER_SUPPORT
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
#endif

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

void DxDrawingFrameBase::OnSize(UINT width, UINT height)
{
	assert(swapChain);
	assert(d2dDeviceContext);

	d2dDeviceContext->SetTarget(nullptr);

	if (S_OK == swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0))
	{
		CreateDeviceSwapChainBitmap();
	}
	else
	{
		ReleaseDevice();
	}

	int dpiX, dpiY;
	{
		HDC hdc = ::GetDC(getWindowHandle());
		dpiX = GetDeviceCaps(hdc, LOGPIXELSX);
		dpiY = GetDeviceCaps(hdc, LOGPIXELSY);
		::ReleaseDC(getWindowHandle(), hdc);
	}

	const drawing::Size available{
		static_cast<float>(((width) * 96) / dpiX),
		static_cast<float>(((height) * 96) / dpiY)
	};

#ifdef GMPI_HOST_POINTER_SUPPORT
	gmpi_gui_client->arrange({0, 0, available.width, available.height });
#else
	if (drawingClient)
	{
		const gmpi::drawing::Rect r{ 0, 0, available.width, available.height };
		drawingClient->arrange(&r);
	}
#endif
}

// Ideally this is called at 60Hz so we can draw as fast as practical, but without blocking to wait for Vsync all the time (makes host unresponsive).
bool DxDrawingFrameBase::onTimer()
{
	auto hwnd = getWindowHandle();
	if (hwnd == nullptr
#ifdef GMPI_HOST_POINTER_SUPPORT
		|| gmpi_gui_client == nullptr
#endif
		)
		return true;

#ifdef GMPI_HOST_POINTER_SUPPORT

	// Tooltips
	if (toolTiptimer-- == 0 && !toolTipShown)
	{
		POINT P;
		GetCursorPos(&P);

		// Check mouse in window and not captured.
		if (WindowFromPoint(P) == hwnd && GetCapture() != hwnd)
		{
			ScreenToClient(hwnd, &P);

			const auto point = WindowToDips.TransformPoint(Point(static_cast<float>(P.x), static_cast<float>(P.y)));

			gmpi_sdk::MpString text;
			gmpi_gui_client->getToolTip(point, &text);
			if (!text.str().empty())
			{
				toolTipText = JmUnicodeConversions::Utf8ToWstring(text.str());
				ShowToolTip();
			}
		}
	}
#endif

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

void RenderLog(ID2D1RenderTarget* context_, IDWriteFactory* writeFactory, ID2D1Factory1* factory)
{
} // RenderLog ====================================================

void DxDrawingFrameBase::OnPaint()
{
	// First clear update region (else windows will pound on this repeatedly).
	updateRegion_native.copyDirtyRects(getWindowHandle(), swapChainSize);
	ValidateRect(getWindowHandle(), NULL); // Clear invalid region for next frame.

	// prevent infinite assert dialog boxes when assert happens during painting.
	if (reentrant)
	{
		return;
	}
	reentrant = true;

	auto& dirtyRects = updateRegion_native.getUpdateRects();
	if (
		(
#ifdef GMPI_HOST_POINTER_SUPPORT
		gmpi_gui_client ||
#endif
			drawingClient
		)
		&& !dirtyRects.empty())
	{
		//	_RPT1(_CRT_WARN, "OnPaint(); %d dirtyRects\n", dirtyRects.size() );

		if (!d2dDeviceContext) // not quite right, also need to re-create any resources (brushes etc) else most object draw blank. Could refresh the view in this case.
		{
			CreateSwapPanel();
		}

		{
			Graphics graphics(context.get());

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

#ifdef GMPI_HOST_POINTER_SUPPORT
				gmpi_gui_client->onRender(context.get());
#endif
				if (drawingClient)
					drawingClient->render(context.get());

				graphics.popAxisAlignedClip();
			}

			// Print OS Version.
#if 0 //def _DEBUG
			{
				OSVERSIONINFO osvi;
				memset(&osvi, 0, sizeof(osvi));

				osvi.dwOSVersionInfoSize = sizeof(osvi);
				GetVersionEx(&osvi);

				char versionString[100];
				sprintf(versionString, "OS Version %d.%d", (int)osvi.dwMajorVersion, (int)osvi.dwMinorVersion);

				auto brsh = graphics.CreateSolidColorBrush(Colors::Black);
				graphics.drawTextU(versionString, graphics.GetFactory().CreateTextFormat(12), drawing::Rect(2.0f, 2.0f, 200, 200), brsh);
			}
#endif

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

			/*const auto r =*/ graphics.endDraw();
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
	}

	reentrant = false;
}

void DxDrawingFrameBase::CreateSwapPanel()
{
	ReleaseDevice();

	// Create a Direct3D Device
	UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#ifdef _DEBUG
	// you must explicity install DX debug support for this to work.
	flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
	// Comment out first to test lower versions.
	D3D_FEATURE_LEVEL d3dLevels[] = 
	{
#if	ENABLE_HDR_SUPPORT
		D3D_FEATURE_LEVEL_11_1,
#endif
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_1,
		D3D_FEATURE_LEVEL_10_0,
		D3D_FEATURE_LEVEL_9_3,
		D3D_FEATURE_LEVEL_9_2, // flip SUPPORTED on my PC
		D3D_FEATURE_LEVEL_9_1, // NO flip
	};

	D3D_FEATURE_LEVEL currentDxFeatureLevel;
	ComPtr<ID3D11Device> D3D11Device;
	
	HRESULT r = DXGI_ERROR_UNSUPPORTED;

	// Create Hardware device.
	do {
		r = D3D11CreateDevice(nullptr,
			D3D_DRIVER_TYPE_HARDWARE,
			nullptr,
			flags,
			d3dLevels, sizeof(d3dLevels) / sizeof(d3dLevels[0]),
			D3D11_SDK_VERSION,
			D3D11Device.GetAddressOf(),
			&currentDxFeatureLevel,
			nullptr);

			// Clear D3D11_CREATE_DEVICE_DEBUG
			((flags) &= (0xffffffff ^ (D3D11_CREATE_DEVICE_DEBUG)));

		} while (r == 0x887a002d); // The application requested an operation that depends on an SDK component that is missing or mismatched. (no DEBUG LAYER).

	bool DX_support_sRGB;
	{
		/* !! only good for detecting Windows 7 !!
		Applications not manifested for Windows 8.1 or Windows 10 will return the Windows 8 OS version value (6.2).
		Once an application is manifested for a given operating system version,
		GetVersionEx will always return the version that the application is manifested for
		*/
		OSVERSIONINFO osvi;
		memset(&osvi, 0, sizeof(osvi));

		osvi.dwOSVersionInfoSize = sizeof(osvi);
		GetVersionEx(&osvi);

		DX_support_sRGB =
			((osvi.dwMajorVersion > 6) ||
				((osvi.dwMajorVersion == 6) && (osvi.dwMinorVersion > 1))); // Win7 = V6.1
	}

	// Support for HDR displays.
	float whiteMult{ 1.0f };
	{
		// query for the device object�s IDXGIDevice interface
		ComPtr<IDXGIDevice> dxdevice;
		D3D11Device.As(&dxdevice);

		// Retrieve the display adapter
		ComPtr<IDXGIAdapter> adapter;
		dxdevice->GetAdapter(adapter.GetAddressOf());

		UINT i = 0;
		ComPtr<IDXGIOutput> currentOutput;
		ComPtr<IDXGIOutput> bestOutput;
		int bestIntersectArea = -1;

		while (adapter->EnumOutputs(i, currentOutput.ReleaseAndGetAddressOf()) != DXGI_ERROR_NOT_FOUND)
		{
			getWindowHandle();

			// get bounds of window having handle: getWindowHandle()
			RECT m_windowBounds;
			GetWindowRect(getWindowHandle(), &m_windowBounds);

			// Get the retangle bounds of the app window
			gmpi::drawing::RectL appWindowRect = { m_windowBounds.left, m_windowBounds.top, m_windowBounds.right, m_windowBounds.bottom };

			// Get the rectangle bounds of current output
			DXGI_OUTPUT_DESC desc;
			/*auto hr =*/ currentOutput->GetDesc(&desc);
			RECT desktopRect = desc.DesktopCoordinates;
			gmpi::drawing::RectL outputRect = { desktopRect.left, desktopRect.top, desktopRect.right, desktopRect.bottom };

			// Compute the intersection
			const auto commonRect = intersectRect(appWindowRect, outputRect);
			const int intersectArea = getWidth(commonRect) * getHeight(commonRect);
			if (intersectArea > bestIntersectArea)
			{
				bestOutput = currentOutput;
				bestIntersectArea = intersectArea;
			}

			i++;
		}

		// Having determined the output (display) upon which the app is primarily being 
		// rendered, retrieve the HDR capabilities of that display by checking the color space.
		ComPtr<IDXGIOutput6> output6;
		auto hr = bestOutput.As(&output6);

		if (output6)
		{
			DXGI_OUTPUT_DESC1 desc1;
			hr = output6->GetDesc1(&desc1);

			if (desc1.ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709)
			{
				_RPT0(_CRT_WARN, "SDR Display\n");
			}
			else
			{
				_RPT0(_CRT_WARN, "HDR Display\n");
			}

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

			DISPLAYCONFIG_SDR_WHITE_LEVEL white_level = {};
			white_level.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SDR_WHITE_LEVEL;
			white_level.header.size = sizeof(white_level);
			for (uint32_t pathIdx = 0; pathIdx < numPathArrayElements; ++pathIdx)
			{
				white_level.header.adapterId = pathInfo[pathIdx].targetInfo.adapterId;
				white_level.header.id = pathInfo[pathIdx].targetInfo.id;

				if (DisplayConfigGetDeviceInfo(&white_level.header) == ERROR_SUCCESS)
				{
#if	ENABLE_HDR_SUPPORT // proper HDR rendering
					{
						// divide by 1000 to get nits, divide by reference nits (80) to get a factor
						whiteMult = white_level.SDRWhiteLevel / 1000.f;
//						DrawingFactory.whiteMult = whiteMult;
					}
#else // fall back to 8-bit rendering and ignore HDR
					{
						const auto whiteMultiplier = white_level.SDRWhiteLevel / 1000.f;
						DX_support_sRGB = DX_support_sRGB && whiteMultiplier == 1.0f; // workarround HDR issues by reverting to 8-bit colour
					}
#endif
				}
			}
		}
	}

	if (m_disable_gpu)
	{
		// release hardware device
		D3D11Device = nullptr;
		r = DXGI_ERROR_UNSUPPORTED;
	}

	// fallback to software rendering.
	if (DXGI_ERROR_UNSUPPORTED == r)
	{
		do {
		r = D3D11CreateDevice(nullptr,
			D3D_DRIVER_TYPE_WARP,
			nullptr,
			flags,
			nullptr, 0,
			D3D11_SDK_VERSION,
			D3D11Device.GetAddressOf(),
			&currentDxFeatureLevel,
			nullptr);

			// Clear D3D11_CREATE_DEVICE_DEBUG
			((flags) &= (0xffffffff ^ (D3D11_CREATE_DEVICE_DEBUG)));

		} while (r == 0x887a002d); // The application requested an operation that depends on an SDK component that is missing or mismatched. (no DEBUG LAYER).
	}

	// query for the device object�s IDXGIDevice interface
	ComPtr<IDXGIDevice> dxdevice;
	D3D11Device.As(&dxdevice);

	// Retrieve the display adapter
	ComPtr<IDXGIAdapter> adapter;
	dxdevice->GetAdapter(adapter.GetAddressOf());

	// adapter�s parent object is the DXGI factory
	ComPtr<IDXGIFactory2> factory; // Minimum supported client: Windows 8 and Platform Update for Windows 7 
	adapter->GetParent(__uuidof(factory), reinterpret_cast<void **>(factory.GetAddressOf()));

	// query adaptor memory. Assume small integrated graphics cards do not have the capacity for float pixels.
	// Software renderer has no device memory, yet does support float pixels anyhow.
	if (!m_disable_gpu)
	{
		DXGI_ADAPTER_DESC adapterDesc{};
		adapter->GetDesc(&adapterDesc);

		const auto dedicatedRamMB = adapterDesc.DedicatedVideoMemory / 0x100000;

		// Intel HD Graphics on my Kogan has 128MB.
		DX_support_sRGB &= (dedicatedRamMB >= 512); // MB
	}

	/* NOTES:
		DXGI_FORMAT_R10G10B10A2_UNORM - fails in CreateBitmapFromDxgiSurface() don't supprt direct2d says channel9.
		DXGI_FORMAT_R16G16B16A16_FLOAT - 'tends to take up far too much memoryand bandwidth at HD resolutions' - Stack Overlow.

		DXGI ERROR: IDXGIFactory::CreateSwapChain:
			Flip model swapchains (DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL and DXGI_SWAP_EFFECT_FLIP_DISCARD)
			only support the following Formats:
			(DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_R10G10B10A2_UNORM),
			assuming the underlying Device does as well.
	*/

	// https://learn.microsoft.com/en-us/windows/win32/direct3darticles/high-dynamic-range
	const DXGI_FORMAT bestFormat = DXGI_FORMAT_R16G16B16A16_FLOAT; // Proper gamma-correct blending.
	const DXGI_FORMAT fallbackFormat = DXGI_FORMAT_B8G8R8A8_UNORM; // shitty linear blending.

	{
		UINT driverSrgbSupport = 0;
		auto hr = D3D11Device->CheckFormatSupport(bestFormat, &driverSrgbSupport);

		const UINT srgbflags = D3D11_FORMAT_SUPPORT_DISPLAY | D3D11_FORMAT_SUPPORT_TEXTURE2D | D3D11_FORMAT_SUPPORT_BLENDABLE;

		if (SUCCEEDED(hr))
		{
			DX_support_sRGB &= ((driverSrgbSupport & srgbflags) == srgbflags);
		}
	}

	DX_support_sRGB &= D3D_FEATURE_LEVEL_11_0 <= currentDxFeatureLevel;

	DXGI_SWAP_CHAIN_DESC1 props {};
	props.SampleDesc.Count = 1;
	props.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	props.BufferCount = 2;
	props.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;	// Best. Efficient flip.
	props.Format = bestFormat;
	props.Scaling = DXGI_SCALING_NONE; // prevents annoying stretching effect when resizing window.

	if (lowDpiMode)
	{
		RECT temprect;
		GetClientRect(getWindowHandle(), &temprect);

		props.Width = (temprect.right - temprect.left) / 2;
		props.Height = (temprect.bottom - temprect.top) / 2;
		props.Scaling = DXGI_SCALING_STRETCH;
	}

	auto propsFallback = props;
	propsFallback.SwapEffect = DXGI_SWAP_EFFECT_SEQUENTIAL;	// Less efficient blit.
	propsFallback.Format = fallbackFormat;
	propsFallback.Scaling = DXGI_SCALING_STRETCH;

	for (int x = 0 ; x < 2 ;++x)
	{
		gmpi::directx::ComPtr<::IDXGISwapChain1> swapChain1;
		auto swapchainresult = factory->CreateSwapChainForHwnd(D3D11Device.Get(),
			getWindowHandle(),
			DX_support_sRGB ? &props : &propsFallback,
			nullptr,
			nullptr,
			swapChain1.getAddressOf());

		if (swapchainresult == S_OK)
		{
			swapChain1->QueryInterface(swapChain.getAddressOf()); // convert to IDXGISwapChain2
			break;
		}

		DX_support_sRGB = false;
	}


	// Creating the Direct2D Device
	ComPtr<ID2D1Device> device;
	DrawingFactory.getD2dFactory()->CreateDevice(dxdevice.Get(),
		device.GetAddressOf());

	// and context.
	device->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_ENABLE_MULTITHREADED_OPTIMIZATIONS, d2dDeviceContext.getAddressOf());

	float dpiX, dpiY;

	// disable DPI for testing.
	if (lowDpiMode)
	{
		dpiX = dpiY = 96.0f;
	}
	else
	{
		DrawingFactory.getD2dFactory()->GetDesktopDpi(&dpiX, &dpiY);
	}

	d2dDeviceContext->SetDpi(dpiX, dpiY);

	DrawingFactory.setSrgbSupport(DX_support_sRGB, whiteMult);
	DipsToWindow = makeScale(dpiX / 96.0f, dpiY / 96.0f); // was dpiScaleInverse
	WindowToDips = DipsToWindow;
	WindowToDips = invert(WindowToDips);

	// A little jagged on small fonts
	//	mpRenderTarget->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE); // "The quality of rendering grayscale text is comparable to ClearType but is much faster."}

	CreateDeviceSwapChainBitmap();

	drawing::api::IBitmapPixels::PixelFormat pixelFormat;
	DrawingFactory.getPlatformPixelFormat(&pixelFormat);

	if (pixelFormat == gmpi::drawing::api::IBitmapPixels::kBGRA_SRGB)
	{
		context.reset(new gmpi::directx::GraphicsContext(d2dDeviceContext, &DrawingFactory));
	}
	else
	{
		context.reset(new gmpi::directx::GraphicsContext_Win7(d2dDeviceContext, &DrawingFactory));
	}
}

void DxDrawingFrameBase::CreateDeviceSwapChainBitmap()
{
//	_RPT0(_CRT_WARN, "\n\nCreateDeviceSwapChainBitmap()\n");

	ComPtr<IDXGISurface> surface;
	swapChain->GetBuffer(0, // buffer index
		__uuidof(surface),
		reinterpret_cast<void **>(surface.GetAddressOf()));

	// Get the swapchain pixel format.
	DXGI_SURFACE_DESC sufaceDesc;
	surface->GetDesc(&sufaceDesc);

	auto props2 = BitmapProperties1(
		D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
		PixelFormat(sufaceDesc.Format, D2D1_ALPHA_MODE_IGNORE)
		);

	ComPtr<ID2D1Bitmap1> bitmap;
	d2dDeviceContext->CreateBitmapFromDxgiSurface(surface.Get(),
		props2,
		bitmap.GetAddressOf());

	const auto bitmapsize = bitmap->GetSize();
	swapChainSize.width = static_cast<int32_t>(bitmapsize.width);
	swapChainSize.height = static_cast<int32_t>(bitmapsize.height);

//_RPT2(_CRT_WARN, "%x B[%f,%f]\n", this, bitmapsize.width, bitmapsize.height);

	// Now attach Device Context to swapchain bitmap.
	d2dDeviceContext->SetTarget(bitmap.Get());

	// Initial present() moved here in order to ensure it happens before first Timer() tries to draw anything.
//	HRESULT hr = m_swapChain->Present(0, 0);
	firstPresent = true;

	InvalidateRect(getWindowHandle(), nullptr, false);
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

		// Note: This method can fail, but it's okay to ignore the
		// error here, because the error will be returned again
		// the next time EndDraw is called.
/*
		UINT Width = 0; // Auto size
		UINT Height = 0;

		if (lowDpiMode)
		{
			RECT r;
			GetClientRect(&r);

			Width = (r.right - r.left) / 2;
			Height = (r.bottom - r.top) / 2;
		}
*/
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
gmpi::ReturnCode DxDrawingFrameBase::createTextEdit(gmpi::api::IUnknown** returnTextEdit)
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
	GMPI_WIN_PopupMenu(HWND pParentWnd, float dpi = 1.0f) : hmenu(0)
		, parentWnd(pParentWnd)
		, align(TPM_LEFTALIGN)
		, dpiScale(dpi)
		, selectedId(-1)
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

	gmpi::ReturnCode showAsync(const gmpi::drawing::Rect* rect, gmpi::api::IUnknown* returnCallback) override
	{
		POINT nativePoint{(LONG)rect->left, (LONG)rect->top };
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

gmpi::ReturnCode DxDrawingFrameBase::createPopupMenu(gmpi::api::IUnknown** returnMenu)
{
	contextMenu.attach(new GMPI_WIN_PopupMenu(getWindowHandle(), DipsToWindow._22));
	contextMenu->addRef(); // add an extra refcount so i can own it after caller releases.

	*returnMenu = contextMenu.get();
	return gmpi::ReturnCode::Ok;
}

gmpi::ReturnCode DxDrawingFrameBase::createKeyListener(gmpi::api::IUnknown** returnKeyListener)
{
	return gmpi::ReturnCode::NoSupport;
}

gmpi::ReturnCode DxDrawingFrameBase::createFileDialog(int32_t dialogType, gmpi::api::IUnknown** returnDialog)
{
	return gmpi::ReturnCode::NoSupport;
}

#ifdef GMPI_HOST_POINTER_SUPPORT

void DrawingFrameBase::invalidateMeasure()
{
}

int32_t DrawingFrameBase::createPlatformMenu(gmpi::drawing::Rect* rect, gmpi_gui::IMpPlatformMenu** returnMenu)
{
	auto nativeRect = DipsToWindow.transformRect(*rect);
	*returnMenu = new GmpiGuiHosting::PGCC_PlatformMenu(getWindowHandle(), &nativeRect, DipsToWindow._22);
	return gmpi::ReturnCode::Ok;
}

int32_t DrawingFrameBase::createPlatformTextEdit(gmpi::drawing::Rect* rect, gmpi_gui::IMpPlatformText** returnTextEdit)
{
	auto nativeRect = DipsToWindow.transformRect(*rect);
	*returnTextEdit = new GmpiGuiHosting::PGCC_PlatformTextEntry(getWindowHandle(), &nativeRect, DipsToWindow._22);

	return gmpi::ReturnCode::Ok;
}

int32_t DrawingFrameBase::createFileDialog(int32_t dialogType, gmpi_gui::IMpFileDialog** returnFileDialog)
{
	*returnFileDialog = new Gmpi_Win_FileDialog(dialogType, getWindowHandle());
	return gmpi::ReturnCode::Ok;
}

int32_t DrawingFrameBase::createOkCancelDialog(int32_t dialogType, gmpi_gui::IMpOkCancelDialog** returnDialog)
{
	*returnDialog = new Gmpi_Win_OkCancelDialog(dialogType, getWindowHandle());
	return gmpi::ReturnCode::Ok;
}
#endif

#if 0
int32_t DrawingFrameBase::pinTransmit(int32_t pinId, int32_t size, const void * data, int32_t voice)
{
	return gmpi::ReturnCode::Ok;
}

int32_t DrawingFrameBase::createPinIterator(gmpi::api::IPinIterator** returnIterator)
{
	return gmpi::MP_FAIL;
}

int32_t DrawingFrameBase::getHandle(int32_t & returnValue)
{
	return gmpi::ReturnCode::Ok;
}

int32_t DrawingFrameBase::sendMessageToAudio(int32_t id, int32_t size, const void * messageData)
{
	return gmpi::ReturnCode::Ok;
}

int32_t DrawingFrameBase::ClearResourceUris()
{
	return gmpi::ReturnCode::Ok;
}

int32_t DrawingFrameBase::RegisterResourceUri(const char * resourceName, const char * resourceType, gmpi::IString* returnString)
{
	return gmpi::ReturnCode::Ok;
}

int32_t DrawingFrameBase::OpenUri(const char * fullUri, gmpi::IProtectedFile2** returnStream)
{
	return gmpi::ReturnCode::Ok;
}

int32_t DrawingFrameBase::FindResourceU(const char * resourceName, const char * resourceType, gmpi::IString* returnString)
{
	return gmpi::ReturnCode::Ok;
}
#endif

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
*
*
*/
