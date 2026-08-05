#include "backends/DrawingFrameX11.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <string>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/extensions/XShm.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include "backends/CpuEncode.h"

// Xlib's macros, disposed of the moment we no longer need them. `None` and
// `Status` in particular collide with ordinary C++ names; the rest are here so
// that a later edit cannot reintroduce the problem by accident.
#undef Status

namespace gmpi::hosting
{
namespace
{

constexpr int kDoubleClickMs = 400;   // GNOME's default
constexpr int kDoubleClickSlopPx = 5;

// X11 delivers wheel notches as button 4/5 (vertical) and 6/7 (horizontal).
// One notch is 120, the same unit Windows uses and what the clients expect.
constexpr int kWheelUp = 4, kWheelDown = 5, kWheelLeft = 6, kWheelRight = 7;

bool isEmptyRect(const gmpi::drawing::Rect& r)
{
    return r.right <= r.left || r.bottom <= r.top;
}

int64_t nowMs()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

int32_t modifierFlags(unsigned int state)
{
    int32_t f = 0;
    if (state & ShiftMask)   f |= int32_t(gmpi::api::PointerFlags::KeyShift);
    if (state & ControlMask) f |= int32_t(gmpi::api::PointerFlags::KeyControl);
    if (state & Mod1Mask)    f |= int32_t(gmpi::api::PointerFlags::KeyAlt);
    return f;
}

// Mirrors makePointerFlags in DrawingFrameWin.h and basePointerFlags in the
// Wayland frame. Getting this wrong is not a visual bug: SynthEdit's drag
// handling keys off InContact and New, so a missing bit reads as "the mouse
// does not work" while everything else looks fine.
int32_t basePointerFlags(unsigned int state)
{
    return modifierFlags(state)
         | int32_t(gmpi::api::PointerFlags::InContact)
         | int32_t(gmpi::api::PointerFlags::Primary)
         | int32_t(gmpi::api::PointerFlags::Confidence);
}

int32_t buttonFlag(unsigned int button)
{
    switch (button)
    {
    case Button1: return int32_t(gmpi::api::PointerFlags::FirstButton);
    case Button2: return int32_t(gmpi::api::PointerFlags::ThirdButton); // X middle
    case Button3: return int32_t(gmpi::api::PointerFlags::SecondButton); // X right
    default:      return 0;
    }
}

} // anonymous namespace

// ---------------------------------------------------------------------------

struct X11DrawingFrame::Impl
{
    Display* display{};
    Window   window{};
    Window   parent{};
    GC       gc{};
    Visual*  visual{};
    int      depth{};
    int      screen{};

    // The blit target. MIT-SHM when the server is local (the normal case, and
    // markedly faster for a full-window repaint); a plain malloc'd XImage
    // otherwise, including over a network display where SHM cannot work.
    XImage*         image{};
    XShmSegmentInfo shmInfo{};
    bool            usingShm = false;
    int             imageWidth = 0, imageHeight = 0;

    gmpi::api::IDrawingClient* client{};
    gmpi::api::IInputClient*   inputClient{};

    gmpi::drawing::Rect dirty{};
    bool  dirtyAll = true;
    bool  needsMeasure = true;
    bool  captured = false;
    float scale = 1.0f;

    int width = 0, height = 0;   // physical pixels

    // Double-click is decided here because X11 does not decide it for us: the
    // server reports two independent presses and the toolkit is expected to
    // apply the desktop's interval and slop.
    int64_t lastClickMs = 0;
    int     lastClickX = -1000, lastClickY = -1000;

    bool pointerInside = false;

    // Render whatever is dirty and blit it. The factory is the frame's, passed
    // in rather than duplicated here.
    void present(gmpi::cpugfx::Factory& factory);

    void releaseImage()
    {
        if (!image)
            return;

        if (usingShm)
        {
            XShmDetach(display, &shmInfo);
            XDestroyImage(image);
            shmdt(shmInfo.shmaddr);
            // The segment was marked IPC_RMID at creation, so the kernel frees
            // it once the last attach goes away. Nothing leaks if we crash.
            shmInfo = {};
        }
        else
        {
            XDestroyImage(image); // frees image->data too
        }

        image = {};
        usingShm = false;
        imageWidth = imageHeight = 0;
    }

    bool ensureImage(int w, int h)
    {
        if (image && imageWidth == w && imageHeight == h)
            return true;

        releaseImage();

        if (w <= 0 || h <= 0)
            return false;

        if (XShmQueryExtension(display))
        {
            image = XShmCreateImage(display, visual, depth, ZPixmap, nullptr, &shmInfo, w, h);
            if (image)
            {
                shmInfo.shmid = shmget(IPC_PRIVATE,
                                       static_cast<size_t>(image->bytes_per_line) * h,
                                       IPC_CREAT | 0600);
                if (shmInfo.shmid != -1)
                {
                    shmInfo.shmaddr = image->data = static_cast<char*>(shmat(shmInfo.shmid, nullptr, 0));
                    // Mark for destruction NOW: the segment survives until the
                    // last process detaches, so a crashed plugin cannot strand
                    // shared memory on the machine.
                    shmctl(shmInfo.shmid, IPC_RMID, nullptr);
                    shmInfo.readOnly = False;

                    if (shmInfo.shmaddr != reinterpret_cast<char*>(-1) &&
                        XShmAttach(display, &shmInfo))
                    {
                        XSync(display, False);
                        usingShm = true;
                        imageWidth = w;
                        imageHeight = h;
                        return true;
                    }
                }

                XDestroyImage(image);
                image = {};
                shmInfo = {};
            }
        }

        // Plain XImage fallback.
        const int bytesPerPixel = 4;
        auto* data = static_cast<char*>(std::calloc(static_cast<size_t>(w) * h, bytesPerPixel));
        if (!data)
            return false;

        image = XCreateImage(display, visual, depth, ZPixmap, 0, data, w, h, 32, w * bytesPerPixel);
        if (!image)
        {
            std::free(data);
            return false;
        }

        usingShm = false;
        imageWidth = w;
        imageHeight = h;
        return true;
    }
};

// ---------------------------------------------------------------------------

X11DrawingFrame::X11DrawingFrame() : impl_(std::make_unique<Impl>()) {}

X11DrawingFrame::~X11DrawingFrame()
{
    close();
}

void X11DrawingFrame::attachClient(gmpi::api::IDrawingClient* client)
{
    detachClient();

    impl_->client = client;
    if (!client)
        return;

    client->setHost(static_cast<gmpi::api::IDrawingHost*>(this));

    // An IDrawingClient that also handles input says so by implementing
    // IInputClient; almost every plugin editor does.
    gmpi::api::IInputClient* in{};
    if (client->queryInterface(&gmpi::api::IInputClient::guid, reinterpret_cast<void**>(&in)) == gmpi::ReturnCode::Ok && in)
    {
        impl_->inputClient = in;
        in->release(); // queryInterface addref'd; the client outlives us
    }

    impl_->needsMeasure = true;
    impl_->dirtyAll = true;
}

void X11DrawingFrame::detachClient()
{
    if (impl_->client)
        impl_->client->setHost(nullptr);

    impl_->client = {};
    impl_->inputClient = {};
}

bool X11DrawingFrame::open(uintptr_t parentWindowId, int width, int height)
{
    close();

    auto& d = *impl_;

    d.display = XOpenDisplay(nullptr);
    if (!d.display)
        return false;   // headless, or Wayland with no XWayland

    d.parent = static_cast<Window>(parentWindowId);
    d.screen = DefaultScreen(d.display);

    // Inherit the PARENT's visual and depth rather than assuming the default.
    // A host running a 32-bit ARGB visual (compositing toolkits do) would give
    // a BadMatch on window creation otherwise, which presents as "the plugin
    // window never appears" with no other symptom.
    XWindowAttributes parentAttr{};
    if (d.parent && XGetWindowAttributes(d.display, d.parent, &parentAttr))
    {
        d.visual = parentAttr.visual;
        d.depth  = parentAttr.depth;
    }
    else
    {
        d.visual = DefaultVisual(d.display, d.screen);
        d.depth  = DefaultDepth(d.display, d.screen);
    }

    d.width  = (std::max)(1, width);
    d.height = (std::max)(1, height);

    XSetWindowAttributes attr{};
    attr.background_pixmap = ParentRelative;
    attr.border_pixel = 0;
    attr.colormap = XCreateColormap(d.display, d.parent ? d.parent : DefaultRootWindow(d.display),
                                    d.visual, AllocNone);
    attr.event_mask = ExposureMask | StructureNotifyMask
                    | ButtonPressMask | ButtonReleaseMask
                    | PointerMotionMask
                    | EnterWindowMask | LeaveWindowMask
                    | KeyPressMask | KeyReleaseMask
                    | FocusChangeMask;

    d.window = XCreateWindow(d.display,
                             d.parent ? d.parent : DefaultRootWindow(d.display),
                             0, 0,
                             static_cast<unsigned>(d.width), static_cast<unsigned>(d.height),
                             0, d.depth, InputOutput, d.visual,
                             CWBorderPixel | CWColormap | CWEventMask,
                             &attr);

    if (!d.window)
    {
        XCloseDisplay(d.display);
        d.display = {};
        return false;
    }

    d.gc = XCreateGC(d.display, d.window, 0, nullptr);

    XMapWindow(d.display, d.window);
    XFlush(d.display);

    d.dirtyAll = true;
    d.needsMeasure = true;
    return true;
}

void X11DrawingFrame::close()
{
    auto& d = *impl_;

    if (!d.display)
        return;

    detachClient();

    if (d.captured)
    {
        XUngrabPointer(d.display, CurrentTime);
        d.captured = false;
    }

    d.releaseImage();

    if (d.gc)     { XFreeGC(d.display, d.gc); d.gc = {}; }
    if (d.window) { XDestroyWindow(d.display, d.window); d.window = {}; }

    XCloseDisplay(d.display);
    d.display = {};
}

bool X11DrawingFrame::isOpen() const { return impl_->display != nullptr; }

int X11DrawingFrame::connectionFd() const
{
    return impl_->display ? ConnectionNumber(impl_->display) : -1;
}

int X11DrawingFrame::width() const  { return impl_->width; }
int X11DrawingFrame::height() const { return impl_->height; }

void X11DrawingFrame::invalidateRect(const gmpi::drawing::Rect* invalidRect)
{
    auto& d = *impl_;

    if (!invalidRect)
    {
        d.dirtyAll = true;
        return;
    }

    // One union rather than a list, same reasoning as the Wayland frame: the
    // encode is cheap and a second blit costs a round trip.
    if (isEmptyRect(d.dirty))
        d.dirty = *invalidRect;
    else
    {
        d.dirty.left   = (std::min)(d.dirty.left,   invalidRect->left);
        d.dirty.top    = (std::min)(d.dirty.top,    invalidRect->top);
        d.dirty.right  = (std::max)(d.dirty.right,  invalidRect->right);
        d.dirty.bottom = (std::max)(d.dirty.bottom, invalidRect->bottom);
    }
}

void X11DrawingFrame::invalidateMeasure() { impl_->needsMeasure = true; }

float X11DrawingFrame::getRasterizationScale() { return impl_->scale; }

gmpi::ReturnCode X11DrawingFrame::setCapture()
{
    auto& d = *impl_;
    if (!d.display || !d.window)
        return gmpi::ReturnCode::Fail;

    // A real grab, not a flag: this is what lets a knob drag continue after the
    // pointer leaves the plugin's window, which is the whole point of capture
    // and the thing Wayland cannot offer.
    XGrabPointer(d.display, d.window, True,
                 ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
                 GrabModeAsync, GrabModeAsync, /*confine*/ 0, /*cursor*/ 0, CurrentTime);
    d.captured = true;
    return gmpi::ReturnCode::Ok;
}

gmpi::ReturnCode X11DrawingFrame::getCapture(bool& returnValue)
{
    returnValue = impl_->captured;
    return gmpi::ReturnCode::Ok;
}

gmpi::ReturnCode X11DrawingFrame::releaseCapture()
{
    auto& d = *impl_;
    if (d.display && d.captured)
        XUngrabPointer(d.display, CurrentTime);
    d.captured = false;
    return gmpi::ReturnCode::Ok;
}

void X11DrawingFrame::reSize(int width, int height)
{
    auto& d = *impl_;

    width  = (std::max)(1, width);
    height = (std::max)(1, height);

    if (width == d.width && height == d.height)
        return;

    d.width = width;
    d.height = height;
    d.needsMeasure = true;
    d.dirtyAll = true;

    if (d.display && d.window)
        XResizeWindow(d.display, d.window, static_cast<unsigned>(width), static_cast<unsigned>(height));
}

// ---------------------------------------------------------------------------
// Paint
// ---------------------------------------------------------------------------

void X11DrawingFrame::processEvents()
{
    auto& d = *impl_;
    if (!d.display)
        return;

    while (XPending(d.display))
    {
        XEvent e{};
        XNextEvent(d.display, &e);

        switch (e.type)
        {
        case Expose:
            d.dirtyAll = true;
            break;

        case ConfigureNotify:
            if (e.xconfigure.width != d.width || e.xconfigure.height != d.height)
            {
                d.width  = e.xconfigure.width;
                d.height = e.xconfigure.height;
                d.needsMeasure = true;
                d.dirtyAll = true;
            }
            break;

        case EnterNotify:
            d.pointerInside = true;
            if (d.inputClient)
            {
                d.inputClient->setHover(true);
                // Deliver a move on entry: a client that highlights on hover has
                // no other way to learn where the pointer arrived.
                d.inputClient->onPointerMove(
                    { static_cast<float>(e.xcrossing.x) / d.scale,
                      static_cast<float>(e.xcrossing.y) / d.scale },
                    basePointerFlags(e.xcrossing.state));
            }
            break;

        case LeaveNotify:
            d.pointerInside = false;
            if (d.inputClient)
                d.inputClient->setHover(false);
            break;

        case MotionNotify:
        {
            // Coalesce: X can queue a long tail of motion during a fast drag,
            // and acting on every one of them just renders frames nobody sees.
            XEvent latest = e;
            while (XCheckTypedWindowEvent(d.display, d.window, MotionNotify, &latest))
                ; // keep the last

            if (d.inputClient)
                d.inputClient->onPointerMove(
                    { static_cast<float>(latest.xmotion.x) / d.scale,
                      static_cast<float>(latest.xmotion.y) / d.scale },
                    basePointerFlags(latest.xmotion.state));
            break;
        }

        case ButtonPress:
        {
            const auto& b = e.xbutton;

            if (b.button == kWheelUp || b.button == kWheelDown ||
                b.button == kWheelLeft || b.button == kWheelRight)
            {
                if (d.inputClient)
                {
                    const bool horiz = (b.button == kWheelLeft || b.button == kWheelRight);
                    const int32_t delta = (b.button == kWheelUp || b.button == kWheelRight) ? 120 : -120;
                    int32_t flags = basePointerFlags(b.state);
                    if (horiz)
                        flags |= int32_t(gmpi::api::PointerFlags::ScrollHoriz);

                    d.inputClient->onMouseWheel(
                        { static_cast<float>(b.x) / d.scale, static_cast<float>(b.y) / d.scale },
                        flags, delta);
                }
                break;
            }

            const int64_t t = nowMs();
            const bool isDouble = (t - d.lastClickMs) < kDoubleClickMs
                               && std::abs(b.x - d.lastClickX) <= kDoubleClickSlopPx
                               && std::abs(b.y - d.lastClickY) <= kDoubleClickSlopPx;
            d.lastClickMs = t;
            d.lastClickX = b.x;
            d.lastClickY = b.y;

            int32_t flags = basePointerFlags(b.state)
                          | buttonFlag(b.button)
                          | int32_t(gmpi::api::PointerFlags::New);
            if (isDouble)
                flags |= int32_t(gmpi::api::PointerFlags::Double);

            // Keys must reach us once the user has clicked in the view; the host
            // gives its embedded child no focus of its own.
            XSetInputFocus(d.display, d.window, RevertToParent, CurrentTime);

            if (d.inputClient)
                d.inputClient->onPointerDown(
                    { static_cast<float>(b.x) / d.scale, static_cast<float>(b.y) / d.scale }, flags);
            break;
        }

        case ButtonRelease:
        {
            const auto& b = e.xbutton;
            if (b.button == kWheelUp || b.button == kWheelDown ||
                b.button == kWheelLeft || b.button == kWheelRight)
                break;   // wheel "release" is not an event the client wants

            if (d.inputClient)
                d.inputClient->onPointerUp(
                    { static_cast<float>(b.x) / d.scale, static_cast<float>(b.y) / d.scale },
                    modifierFlags(b.state) | buttonFlag(b.button)
                        | int32_t(gmpi::api::PointerFlags::Primary)
                        | int32_t(gmpi::api::PointerFlags::Confidence));
            break;
        }

        case KeyPress:
        {
            char buf[32]{};
            KeySym keysym{};
            const int n = XLookupString(&e.xkey, buf, sizeof(buf) - 1, &keysym, nullptr);

            if (d.inputClient && n > 0)
            {
                // Latin-1 range only, which is what IInputClient::onKeyPress
                // takes. Anything wider needs an XIM input context - a text edit
                // in the plugin GUI will want one; a knob never does.
                for (int i = 0; i < n; ++i)
                    d.inputClient->onKeyPress(static_cast<wchar_t>(static_cast<unsigned char>(buf[i])));
            }
            break;
        }

        default:
            break;
        }
    }

    d.present(factory_);
}

void X11DrawingFrame::onTimer()
{
    auto& d = *impl_;
    if (!d.display)
        return;

    // A parameter moving under automation invalidates from outside any X event,
    // so the timer is what gets that on screen. Events themselves are handled by
    // processEvents when the run loop says the fd is ready.
    d.present(factory_);
}

void X11DrawingFrame::Impl::present(gmpi::cpugfx::Factory& factory)
{
    auto& d = *this;

    if (!d.display || !d.window || !d.client)
        return;

    if (!d.dirtyAll && isEmptyRect(d.dirty))
        return;

    const int pw = d.width;
    const int ph = d.height;
    if (pw <= 0 || ph <= 0)
        return;

    if (!d.ensureImage(pw, ph))
        return;

    // cpugfx has no resize, so a size change means a new target - same trade as
    // the Wayland frame: reallocating during an interactive resize measures fine
    // and is far simpler than pooling.
    gmpi::drawing::api::IBitmapRenderTarget* rtRaw{};
    factory.createCpuRenderTarget({ static_cast<uint32_t>(pw), static_cast<uint32_t>(ph) },
                                  0, &rtRaw, 96.0f);
    auto* rt = dynamic_cast<gmpi::cpugfx::RenderTarget*>(rtRaw);
    if (!rt)
    {
        if (rtRaw) rtRaw->release();
        return;
    }

    const float logicalW = pw / d.scale;
    const float logicalH = ph / d.scale;

    if (d.needsMeasure)
    {
        gmpi::drawing::Size avail{ logicalW, logicalH };
        gmpi::drawing::Size desired{};
        d.client->measure(&avail, &desired);

        const gmpi::drawing::Rect all{ 0, 0, avail.width, avail.height };
        d.client->arrange(&all);
        d.needsMeasure = false;
    }

    rt->beginDraw();
    if (d.scale != 1.0f)
    {
        const auto m = gmpi::drawing::makeScale(d.scale, d.scale);
        rt->setTransform(&m);
    }
    d.client->render(static_cast<gmpi::drawing::api::IDeviceContext*>(rt));
    rt->endDraw();

    gmpi::drawing::api::IBitmap* bmRaw{};
    rt->getBitmap(&bmRaw);
    if (auto* bm = dynamic_cast<gmpi::cpugfx::Bitmap*>(bmRaw))
    {
        const auto& s = bm->surface;

        const gmpi::cpugfx::SourceSurface src{ s.pixels, s.stridePixels, s.width, s.height };
        const gmpi::cpugfx::DestSurface   dst{ reinterpret_cast<uint8_t*>(d.image->data),
                                               d.image->bytes_per_line, pw, ph,
                                               gmpi::cpugfx::PixelEncoding::Bgra8888 };

        gmpi::drawing::RectL area{ 0, 0, pw, ph };
        if (!d.dirtyAll && !isEmptyRect(d.dirty))
        {
            area.left   = (std::max)(0,  static_cast<int32_t>(d.dirty.left   * d.scale));
            area.top    = (std::max)(0,  static_cast<int32_t>(d.dirty.top    * d.scale));
            area.right  = (std::min)(pw, static_cast<int32_t>(d.dirty.right  * d.scale + 1.0f));
            area.bottom = (std::min)(ph, static_cast<int32_t>(d.dirty.bottom * d.scale + 1.0f));
        }

        gmpi::cpugfx::encodeDirtyRect(src, dst, area);

        const int aw = area.right - area.left;
        const int ah = area.bottom - area.top;
        if (aw > 0 && ah > 0)
        {
            if (d.usingShm)
                XShmPutImage(d.display, d.window, d.gc, d.image,
                             area.left, area.top, area.left, area.top,
                             static_cast<unsigned>(aw), static_cast<unsigned>(ah), False);
            else
                XPutImage(d.display, d.window, d.gc, d.image,
                          area.left, area.top, area.left, area.top,
                          static_cast<unsigned>(aw), static_cast<unsigned>(ah));

            XFlush(d.display);
        }
    }

    if (bmRaw) bmRaw->release();
    rtRaw->release();

    d.dirtyAll = false;
    d.dirty = {};
}

} // namespace gmpi::hosting
