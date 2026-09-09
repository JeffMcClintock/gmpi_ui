// FormsDemo on Windows: an overlapped HWND whose client area is filled by
// gmpi_ui's Direct2D DrawingFrame, with a DemoForm attached as the frame's
// client.
//
// The frame is the same one the GMPI VST3 wrapper embeds in a DAW. It carries
// the swap chain, the render loop, the mouse and keyboard dispatch, the tooltip
// and the native popup/text-edit/file-dialog implementations - everything
// except a window of its own, because open() creates a WS_CHILD inside a
// parent. This file supplies that parent and nothing else. A trimmed-down
// GMPI_Wrappers/wrapper/Standalone/windows/ToplevelWindow.cpp, essentially.
//
// Sizes crossing the API are DIPs, which is what the Form measures itself in;
// the conversion to pixels happens here against the monitor the window is on.

#include <windows.h>

#include "backends/DrawingFrameWin.h"

#include "DemoForm.h"

namespace
{

constexpr DWORD   kWindowStyle = WS_OVERLAPPEDWINDOW;
constexpr wchar_t kWindowClassName[] = L"GmpiFormsDemoToplevel";

// The window's initial client size, in DIPs.
constexpr int kClientWidthDips  = 460;
constexpr int kClientHeightDips = 260;

class App
{
public:
    // DPI awareness and the COM apartment, before any window exists and
    // before anything asks for a DPI. STA, because this thread owns windows.
    App()
    {
        ::SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        comInit_ = ::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    }

    ~App()
    {
        close();

        if (SUCCEEDED(comInit_))
            ::CoUninitialize();
    }

    bool create()
    {
        const HINSTANCE instance = ::GetModuleHandleW(nullptr);

        WNDCLASSEXW wc{};
        wc.cbSize        = sizeof(wc);
        wc.lpfnWndProc   = &App::windowProc;
        wc.hInstance     = instance;
        wc.hCursor       = ::LoadCursor(nullptr, IDC_ARROW);
        wc.hIcon         = ::LoadIcon(nullptr, IDI_APPLICATION);
        wc.lpszClassName = kWindowClassName;
        wc.hbrBackground = nullptr; // the frame paints every pixel of the client area

        if (!::RegisterClassExW(&wc))
            return false;

        // Created at the system DPI, then corrected once it is known which
        // monitor the window actually opened on.
        const UINT guessedDpi = ::GetDpiForSystem();
        RECT r = clientRectForDpi(guessedDpi);

        hwnd_ = ::CreateWindowExW(
            0, kWindowClassName, L"gmpi_ui Forms Demo", kWindowStyle,
            CW_USEDEFAULT, CW_USEDEFAULT, r.right - r.left, r.bottom - r.top,
            nullptr, nullptr, instance,
            this); // -> WM_NCCREATE installs the back-pointer

        if (!hwnd_)
            return false;

        if (const UINT actualDpi = ::GetDpiForWindow(hwnd_); actualDpi != guessedDpi)
        {
            r = clientRectForDpi(actualDpi);
            ::SetWindowPos(hwnd_, nullptr, 0, 0, r.right - r.left, r.bottom - r.top,
                           SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        }

        // The frame sizes itself to the parent's client rect and starts its
        // own redraw timer.
        frame_.open(hwnd_);
        if (!frame_.getWindowHandle())
            return false;

        // The frame holds its own references to the form (attachClient queries
        // three interfaces); detachClient in close() drops them before the
        // form's last reference goes with this object (member order below).
        frame_.attachClient(static_cast<gmpi::api::IEditor*>(form_.get()));
        return true;
    }

    int run()
    {
        ::ShowWindow(hwnd_, SW_SHOW);
        ::UpdateWindow(hwnd_);

        MSG msg{};
        while (::GetMessageW(&msg, nullptr, 0, 0) > 0)
        {
            ::TranslateMessage(&msg);
            ::DispatchMessageW(&msg);
        }
        return static_cast<int>(msg.wParam);
    }

private:
    static RECT clientRectForDpi(UINT dpi)
    {
        RECT r{ 0, 0,
                ::MulDiv(kClientWidthDips,  static_cast<int>(dpi), 96),
                ::MulDiv(kClientHeightDips, static_cast<int>(dpi), 96) };
        ::AdjustWindowRectExForDpi(&r, kWindowStyle, FALSE, 0, dpi);
        return r;
    }

    // Frame first: DestroyWindow on the parent would destroy the child too,
    // and the frame must be the one to do that, so it can clear its window-proc
    // back-pointer and stop its timer before the HWND goes.
    void close()
    {
        frame_.detachClient();
        frame_.close();

        if (hwnd_)
            ::DestroyWindow(hwnd_);
    }

    static LRESULT CALLBACK windowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        if (message == WM_NCCREATE)
        {
            auto* const create = reinterpret_cast<CREATESTRUCTW*>(lParam);
            ::SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
        }

        auto* const self = reinterpret_cast<App*>(::GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (!self)
            return ::DefWindowProcW(hwnd, message, wParam, lParam);

        if (message == WM_NCDESTROY)
        {
            ::SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
            self->hwnd_ = {};
            return ::DefWindowProcW(hwnd, message, wParam, lParam);
        }

        // WM_CREATE and the first WM_SIZE arrive before CreateWindowEx returns.
        if (!self->hwnd_)
            self->hwnd_ = hwnd;

        return self->onMessage(hwnd, message, wParam, lParam);
    }

    LRESULT onMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        switch (message)
        {
        case WM_SIZE:
            // A minimised window reports 0x0, which no swap chain can satisfy;
            // restoring sends another WM_SIZE with the real size.
            if (wParam != SIZE_MINIMIZED && frame_.getWindowHandle())
            {
                RECT client{};
                ::GetClientRect(hwnd, &client);
                frame_.reSize(0, 0, client.right, client.bottom);
            }
            return 0;

        case WM_DPICHANGED:
        {
            // Take the rectangle Windows worked out for the new monitor; the
            // WM_SIZE that follows resizes the frame, which re-reads its DPI.
            const auto* const suggested = reinterpret_cast<const RECT*>(lParam);
            ::SetWindowPos(hwnd, nullptr,
                           suggested->left, suggested->top,
                           suggested->right - suggested->left,
                           suggested->bottom - suggested->top,
                           SWP_NOZORDER | SWP_NOACTIVATE);
            return 0;
        }

        case WM_SETFOCUS:
            // Keyboard input belongs to the frame: it turns WM_CHAR into
            // IInputClient::onKeyPress.
            if (const HWND child = frame_.getWindowHandle())
                ::SetFocus(child);
            return 0;

        case WM_ERASEBKGND:
            return 1; // the frame covers every pixel

        case WM_CLOSE:
            // Hide and end the loop; the teardown runs in ~App, in order.
            ::ShowWindow(hwnd, SW_HIDE);
            ::PostQuitMessage(0);
            return 0;

        case WM_DESTROY:
            hwnd_ = {};
            ::PostQuitMessage(0);
            return 0;
        }

        return ::DefWindowProcW(hwnd, message, wParam, lParam);
    }

    // MEMBER ORDER IS THE CONTRACT: the form is released after the frame that
    // references it.
    //
    // Heap-allocated, never a plain member: gmpi::ui::Form is reference
    // counted and deletes itself when its count reaches zero, so it has to
    // have come from `new`. The shared_ptr adopts the count of 1 a fresh
    // object carries.
    HRESULT                     comInit_{};
    gmpi::shared_ptr<DemoForm>  form_{ new DemoForm() };
    gmpi::hosting::DrawingFrame frame_;
    HWND                        hwnd_{};
};

} // namespace

int APIENTRY wWinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPWSTR, _In_ int)
{
    App app;
    if (!app.create())
    {
        ::MessageBoxW(nullptr, L"Could not create the window.", L"gmpi_ui Forms Demo", MB_OK | MB_ICONERROR);
        return 1;
    }
    return app.run();
}
