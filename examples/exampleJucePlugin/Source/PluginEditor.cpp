/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
NewProjectAudioProcessorEditor::NewProjectAudioProcessorEditor (NewProjectAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.
	addAndMakeVisible(drawingframe);

    setSize (400, 300);

	startTimerHz(60);
}

NewProjectAudioProcessorEditor::~NewProjectAudioProcessorEditor()
{
}

void NewProjectAudioProcessorEditor::timerCallback()
{
	model.step();

#if USE_JUCE_RENDERER
	repaint();
#else
	drawingframe.invalidateRect(nullptr);
#endif
}


#if USE_JUCE_RENDERER
//==============================================================================
void NewProjectAudioProcessorEditor::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (juce::Colours::green);

	for (auto& r : model.rects)
	{
		g.setColour(r.colour);
		g.fillRect(r.pos);
	}

    //g.setColour (juce::Colours::white);
    //g.setFont (15.0f);
    //g.drawFittedText ("Hello World!", getLocalBounds(), juce::Justification::centred, 1);
}
#endif

int32_t GmpiCanvas::OnRender(GmpiDrawing_API::IMpDeviceContext* drawingContext)
{
	GmpiDrawing::Graphics g(drawingContext);
	g.Clear(GmpiDrawing::Color::Green);

	auto brush = g.CreateSolidColorBrush(GmpiDrawing::Color::Red);
	GmpiDrawing::Color c;

	for(auto& r : model.rects)
	{
		c.InitFromSrgba(r.colour.getRed(), r.colour.getGreen(), r.colour.getBlue(), r.colour.getFloatAlpha());
		brush.SetColor(c);
		g.FillRectangle(r.pos.getX(), r.pos.getY(), r.pos.getRight(), r.pos.getBottom(), brush);
	}

	return gmpi::MP_OK;
}


void NewProjectAudioProcessorEditor::resized()
{
    // This is generally where you'll want to lay out the positions of any
    // subcomponents in your editor..
	drawingframe.setBounds(getBounds());
}

void NewProjectAudioProcessorEditor::parentHierarchyChanged()
{
	// this is an oportunity to detect when parent window is available for the first time.
	if (USE_JUCE_RENDERER)
		return;

#ifdef _WIN32
	if (auto hwnd = (HWND)getWindowHandle(); hwnd && !drawingframe.getHWND())
	{
		const auto r = getBounds();

		HDC hdc = ::GetDC(hwnd);
		const int dpi = GetDeviceCaps(hdc, LOGPIXELSX);
		::ReleaseDC(hwnd, hdc);

//		auto presenter = new JsonDocPresenter(dynamic_cast<IGuiHost2*>(&controller));

		{
			//auto cv =
			//	new SynthEdit2::ContainerView(
			//		GmpiDrawing::Size(static_cast<float>(drawingframe.viewDimensions), static_cast<float>(drawingframe.viewDimensions))
			//	);

			auto client = new GmpiCanvas(model);

			drawingframe.AddView(client);// cv);

			client->release();

//			cv->setDocument(presenter, CF_PANEL_VIEW);
		}

//		presenter->RefreshView();

		drawingframe.open(
			hwnd,
			(r.getWidth() * dpi) / 96,
			(r.getHeight() * dpi) / 96
		);

		drawingframe.StartTimer(15);
	}
#else
	if (!drawingframe.getView())
	{
		const auto r = getBounds();
		drawingframe.open(&controller, r.getWidth(), r.getHeight());
	}
#endif
}

// move this crap
#ifdef _WIN32
#include <Windowsx.h>
#else

// without including objective-C headers, we need to create an NSView.
// forward declare function here to return the view, using void* as return type.
void* createNativeView(class IGuiHost2* controller, int width, int height);
void onCloseNativeView(void* ptr);

#endif

#ifdef _WIN32

////////////////////////////////////////////////////////////////////////////
// 
// 
// 
LRESULT JuceDrawingFrame::WindowProc(
	HWND hwnd,
	UINT message,
	WPARAM wParam,
	LPARAM lParam)
{
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
		GmpiDrawing::Point p(static_cast<float>(GET_X_LPARAM(lParam)), static_cast<float>(GET_Y_LPARAM(lParam)));
		p = WindowToDips.TransformPoint(p);

		// Cubase sends spurious mouse move messages when transport running.
		// This prevents tooltips working.
		if (message == WM_MOUSEMOVE)
		{
			if (cubaseBugPreviousMouseMove == p)
			{
				return TRUE;
			}
			cubaseBugPreviousMouseMove = p;
		}
		else
		{
			cubaseBugPreviousMouseMove = GmpiDrawing::Point(-1, -1);
		}

		TooltipOnMouseActivity();

		int32_t eventFlags = gmpi_gui_api::GG_POINTER_FLAG_INCONTACT | gmpi_gui_api::GG_POINTER_FLAG_PRIMARY | gmpi_gui_api::GG_POINTER_FLAG_CONFIDENCE;

		switch (message)
		{
		case WM_MBUTTONDOWN:
		case WM_LBUTTONDOWN:
		case WM_RBUTTONDOWN:
			eventFlags |= gmpi_gui_api::GG_POINTER_FLAG_NEW;
			break;
		}

		switch (message)
		{
		case WM_LBUTTONUP:
		case WM_LBUTTONDOWN:
			eventFlags |= gmpi_gui_api::GG_POINTER_FLAG_FIRSTBUTTON;
			break;
		case WM_RBUTTONDOWN:
		case WM_RBUTTONUP:
			eventFlags |= gmpi_gui_api::GG_POINTER_FLAG_SECONDBUTTON;
			break;
		case WM_MBUTTONDOWN:
		case WM_MBUTTONUP:
			eventFlags |= gmpi_gui_api::GG_POINTER_FLAG_THIRDBUTTON;
			break;
		}

		if (GetKeyState(VK_SHIFT) < 0)
		{
			eventFlags |= gmpi_gui_api::GG_POINTER_KEY_SHIFT;
		}
		if (GetKeyState(VK_CONTROL) < 0)
		{
			eventFlags |= gmpi_gui_api::GG_POINTER_KEY_CONTROL;
		}
		if (GetKeyState(VK_MENU) < 0)
		{
			eventFlags |= gmpi_gui_api::GG_POINTER_KEY_ALT;
		}

		//		int32_t r;
		switch (message)
		{
		case WM_MOUSEMOVE:
		{
			//?			r = containerView->onPointerMove(eventFlags, p);
		}
		break;

		case WM_LBUTTONDOWN:
		case WM_RBUTTONDOWN:
		case WM_MBUTTONDOWN:
			//?			r = containerView->onPointerDown(eventFlags, p);
			break;

		case WM_MBUTTONUP:
		case WM_RBUTTONUP:
		case WM_LBUTTONUP:
			//? handled by drawingframe?			r = containerView->onPointerUp(eventFlags, p);
			break;
		}
	}
	break;

	case WM_NCACTIVATE:
		//if( wParam == FALSE ) // USER CLICKED AWAY
		//	goto we_re_done;
		break;
		/*
			case WM_WINDOWPOSCHANGING:
			{
				LPWINDOWPOS wp = (LPWINDOWPOS)lParam;
			}
			break;
		*/
	case WM_ACTIVATE:
	{
		/*
		//HFONT hFont = CreateFont(18, 0, 0, 0, FW_DONTCARE, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_OUTLINE_PRECIS,
		//	CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, VARIABLE_PITCH, TEXT("Courier New"));

		SendMessage(child,      // Handle of edit control
		WM_SETFONT,         // Message to change the font
		(WPARAM)dialogFont,      // handle of the font
		MAKELPARAM(TRUE, 0) // Redraw text
		);

		::SetWindowPos(hwndDlg, 0, dialogX, dialogY, dialogW, dialogH, SWP_NOZORDER);
		::SetWindowPos(child, 0, 0, 0, dialogW, dialogH, SWP_NOZORDER);
		::SetWindowText(child, dialogEditText);
		::SetFocus(child);
		dialogReturnValue = 1;
		// Select all.
		#ifdef WIN32
		SendMessage(child, EM_SETSEL, (WPARAM)0, (LPARAM)-1);
		#else
		SendMessage(child, EM_SETSEL, 0, MAKELONG(0, -1));
		#endif
		*/
	}
	break;

	/*
case WM_COMMAND:
	switch( LOWORD(wParam) )
	{
	case IDOK:
	goto we_re_done;
	break;

	case IDCANCEL:
	dialogReturnValue = 0;
	EndDialog(hwndDlg, dialogReturnValue); // seems to call back here and exit at "we_re_done"
	return TRUE;
	}
	break;
	*/

	case WM_PAINT:
	{
		OnPaint();
		//		return ::DefWindowProc(windowHandle, message, wParam, lParam); // clear update rect.
	}
	break;

	default:
		return DefWindowProc(hwnd, message, wParam, lParam);

		//we_re_done:
		//	if( !GetDlgItemText(hwndDlg, IDC_EDIT1, dialogEditText, sizeof(dialogEditText) / sizeof(dialogEditText[0])) )
		//		*dialogEditText = 0;
		//	EndDialog(hwndDlg, dialogReturnValue);

	}
	return TRUE;
}

LRESULT CALLBACK DrawingFrameWindowProc(HWND hwnd,
	UINT message,
	WPARAM wParam,
	LPARAM lParam)
{
	auto drawingFrame = (JuceDrawingFrame*)(LONG_PTR)GetWindowLongPtr(hwnd, GWLP_USERDATA);
	if (drawingFrame)
	{
		return drawingFrame->WindowProc(hwnd, message, wParam, lParam);
	}

	return DefWindowProc(hwnd, message, wParam, lParam);
}

// copied from MP_GetDllHandle
HMODULE local_GetDllHandle_randomshit()
{
	HMODULE hmodule = 0;
	GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)&local_GetDllHandle_randomshit, &hmodule);
	return (HMODULE)hmodule;
}

bool registeredWindowClass = false;
WNDCLASS windowClass;
wchar_t gClassName[100];

void JuceDrawingFrame::open(void* parentWnd, int width, int height)
{
	RECT r{ 0, 0, width ,height };
	/* while constructing editor, JUCE main window is a small fixed size, so no point querying it. easier to just pass in required size.
	GetClientRect(parentWnd, &r);
	*/

	if (!registeredWindowClass)
	{
		registeredWindowClass = true;
		OleInitialize(0);

		swprintf(gClassName, sizeof(gClassName) / sizeof(gClassName[0]), L"GMPIGUI%p", local_GetDllHandle_randomshit());

		windowClass.style = CS_GLOBALCLASS;// | CS_DBLCLKS;//|CS_OWNDC; // add Private-DC constant 

		windowClass.lpfnWndProc = DrawingFrameWindowProc;
		windowClass.cbClsExtra = 0;
		windowClass.cbWndExtra = 0;
		windowClass.hInstance = local_GetDllHandle_randomshit();
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

	int style = WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS;// | WS_OVERLAPPEDWINDOW;
	int extended_style = 0;

	const auto windowHandle = CreateWindowEx(extended_style, gClassName, L"",
		style, 0, 0, r.right - r.left, r.bottom - r.top,
		(HWND)parentWnd, NULL, local_GetDllHandle_randomshit(), NULL);

	if (windowHandle)
	{
		setHWND(windowHandle);

		SetWindowLongPtr(windowHandle, GWLP_USERDATA, (__int3264)(LONG_PTR)this);
		//		RegisterDragDrop(windowHandle, new CDropTarget(this));

		CreateRenderTarget();

		initTooltip();
	}
}
#else

// macOS
void JuceDrawingFrame::open(class IGuiHost2* controller, int width, int height)
{
	auto nsView = createNativeView(controller, width, height); // !!!probly need to pass controller

	setView(nsView);
	//    auto y = CFGetRetainCount(nsView);
}

JuceDrawingFrame::~JuceDrawingFrame()
{
	onCloseNativeView(getView());
}
#endif
