/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "SE2JUCE_Editor.h"
#include "SE2JUCE_Processor.h"

#ifdef _WIN32
#include <Windowsx.h>
#else

// without including objective-C headers, we need to create an NSView.
// forward declare function here to return the view, using void* as return type.
void* createNativeView(class IGuiHost2* controller, int width, int height);
void onCloseNativeView(void* ptr);

#endif

#include "../se_sdk3_hosting/JsonDocPresenter.h"
#include "../se_sdk3_hosting/ContainerView.h"

//==============================================================================
SynthEditEditor::SynthEditEditor (SE2JUCE_Processor& p, SeJuceController& pcontroller)
    : AudioProcessorEditor (&p)
    , controller(pcontroller)
{
    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.
    addAndMakeVisible(drawingframe);

	Json::Value document_json;
	{
		Json::Reader reader;
		reader.parse(BundleInfo::instance()->getResource("gui.se.json"), document_json);
	}

	auto& gui_json = document_json["gui"];

	const int width = gui_json["width"].asInt();
	const int height = gui_json["height"].asInt();
	setSize(width, height);
}

void SynthEditEditor::parentHierarchyChanged()
{
	// this is an oportunity to detect when parent window is available for the first time.
	const auto r = getBounds();

#ifdef _WIN32
	auto hwnd = (HWND)getWindowHandle();
	if (hwnd && !drawingframe.getHWND())
	{
		HDC hdc = ::GetDC(hwnd);
		const int dpi = GetDeviceCaps(hdc, LOGPIXELSX);
		::ReleaseDC(hwnd, hdc);

		auto presenter = new JsonDocPresenter(dynamic_cast<IGuiHost2*>(&controller));

		{
			auto cv =
				new SynthEdit2::ContainerView(
					GmpiDrawing::Size(static_cast<float>(drawingframe.viewDimensions), static_cast<float>(drawingframe.viewDimensions))
				);

			drawingframe.AddView(cv);

			cv->release();

			cv->setDocument(presenter, CF_PANEL_VIEW);
		}

		presenter->RefreshView();

		drawingframe.open(
			hwnd,
			(r.getWidth() * dpi) / 96,
			(r.getHeight() * dpi) / 96
		);
	}
#else
	if (!drawingframe.getView())
	{
		drawingframe.open(&controller, r.getWidth(), r.getHeight());
	}
#endif
}

//==============================================================================
void SynthEditEditor::paint (juce::Graphics& g)
{
}

void SynthEditEditor::resized()
{
	drawingframe.setBounds(getLocalBounds());
}

#ifdef _WIN32

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
	RECT r{0, 0, width ,height };
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
