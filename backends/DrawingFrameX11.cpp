#include "backends/DrawingFrameX11.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <cstdio>
#include <string>
#include <vector>

// EVERY gmpi header before Xlib. Xlib #defines None, Status, Bool and Success;
// gmpi::colorpicker::Model has a `Region::None` and gmpi::api::PinDatatype has
// a `Bool`, and getting this order wrong turns them into integer literals with
// an error twenty headers from the cause. X11Clipboard.h is the exception - it
// is ours but it needs Xlib - so it comes after.
#include "backends/CpuEncode.h"
#include "backends/PortalFileDialog.h"
#include "backends/TextEditModel.h"
#include "backends/ColorPickerModel.h"
#include "backends/TooltipModel.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/extensions/XShm.h>
#include <X11/extensions/shape.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include "backends/X11Clipboard.h"

// Xlib's macros, disposed of now that its headers are in. `None` and `Status`
// collide with ordinary C++ names - Region::None below is one - and nothing
// here wants either: a null X resource is spelled 0.
#undef Status
#undef None

namespace gmpi::hosting
{
namespace
{

constexpr int kDoubleClickMs = 400;   // GNOME's default
constexpr int kDoubleClickSlopPx = 5;

// Menu metrics, deliberately the same as the Wayland backend's so a plugin
// looks the same whichever way the host embeds it.
constexpr int kMenuItemHeight      = 24;
constexpr int kMenuSeparatorHeight = 9;
constexpr int kMenuTickGutter      = 26;
constexpr int kMenuArrowGutter     = 22;
constexpr int kMenuPadV            = 4;
constexpr int kMenuMinWidth        = 120;

// Stock-dialog metrics and palette, again matching the Wayland backend.
constexpr int kDialogMargin    = 20;
constexpr int kDialogButtonW   = 92;
constexpr int kDialogButtonH   = 30;
constexpr int kDialogButtonGap = 10;
constexpr int kDialogTextWidth = 380;

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

// XSetInputFocus on a window that is not yet viewable raises BadMatch, and
// Xlib's default error handler calls exit() - inside a DAW that is the host
// going down, not just us. Ask first; a window that is not viewable cannot
// usefully hold focus anyway.
void focusIfViewable(Display* display, Window window)
{
    if (!display || !window)
        return;

    XWindowAttributes attr{};
    if (XGetWindowAttributes(display, window, &attr) && attr.map_state == IsViewable)
        XSetInputFocus(display, window, RevertToParent, CurrentTime);
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

    // Menus are children of the ROOT window, not of ours: an override-redirect
    // window inside the plugin's window would be clipped to it, and a menu
    // routinely extends past the plugin's edge. They share this Display and so
    // this connection fd, which is what lets the host's single registered
    // handler service them - see routeToMenus in processEvents.
    std::vector<class X11PopupMenu*> menus;

    // Dialogs are toplevels, not children of ours, but they still live on this
    // Display and so are serviced from this frame's event pump.
    std::vector<class X11StockDialog*> dialogs;
    std::vector<class X11ColorDialog*> colorDialogs;

    gmpi::drawing::api::ITextFormat* menuFont{};

    // The desktop portal answers on the session bus. Connected on first use, so
    // a plugin that never opens a file chooser never talks to D-Bus at all.
    gmpi::portal::PortalBus portalBus;

    X11Clipboard clipboard;

    // Hover timing lives in the model so X11, Wayland and Windows all wait the
    // same length of time; the window is below.
    gmpi::tooltip::Model tooltip;
    class X11Tooltip*    tooltipWindow{};
    int lastMoveRootX = 0, lastMoveRootY = 0;
    gmpi::drawing::Point lastMovePoint{};

    // The in-place editor and the raw-key sink, at most one of each. Both are
    // drawn on / fed by THIS window rather than getting one of their own: an
    // edit is an overlay on the client, and a key listener is invisible.
    // Non-owning - each keeps itself alive until it finishes, exactly as the
    // dialogs do.
    class X11TextEdit*     activeEdit{};
    class X11KeyListener*  keySink{};

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
// X11PopupMenu - IPopupMenu on an override-redirect window
// ---------------------------------------------------------------------------
// X11 gives us a window that the window manager will not touch, and a pointer
// grab. There is no menu widget, so the items are drawn here with the same CPU
// renderer and the same metrics as the Wayland backend - a plugin should look
// the same whichever way its host embeds it.
//
// Three things shape this:
//
//  * NO EVENT LOOP. The frame owns the connection, and the host owns the frame.
//    Menu windows live on the same Display, so their events arrive on the same
//    fd and X11DrawingFrame::processEvents routes them here by window id. A
//    modal "run the menu until it closes" loop would deadlock the host.
//
//  * The menu is a child of the ROOT window. Inside the plugin's window it
//    would be clipped to it, and menus routinely extend past the edge.
//
//  * The grab belongs to the ROOT menu. A submenu does not take its own -
//    nested grabs on one pointer are how you end up with an X server nobody can
//    click, which on a single-seat machine means a reboot.
class X11PopupMenu : public gmpi::api::IPopupMenu
{
public:
    using Item = gmpi::popupmenu::Item;

    X11PopupMenu(X11DrawingFrame::Impl& frame, gmpi::cpugfx::Factory& factory,
                 gmpi::drawing::api::ITextFormat* font, gmpi::drawing::Rect anchor)
        : frame_(frame), factory_(factory), font_(font), anchor_(anchor)
    {
        frame_.menus.push_back(this);
    }

    ~X11PopupMenu()
    {
        destroyWindow();
        std::erase(frame_.menus, this);
    }

    // --- IContextItemSink / IPopupMenu ---
    gmpi::ReturnCode addItem(const char* text, int32_t id, int32_t flags,
                             gmpi::api::IUnknown* itemCallback) override
    {
        return builder_.addItem(text, id, flags, itemCallback);
    }
    gmpi::ReturnCode setAlignment(int32_t) override { return gmpi::ReturnCode::Ok; }
    gmpi::ReturnCode showAsync() override;

    gmpi::ReturnCode queryInterface(const gmpi::api::Guid* iid, void** returnInterface) override
    {
        *returnInterface = {};
        GMPI_QUERYINTERFACE(gmpi::api::IPopupMenu);
        GMPI_QUERYINTERFACE(gmpi::api::IContextItemSink);
        return gmpi::ReturnCode::NoSupport;
    }
    GMPI_REFCOUNT;

    const std::vector<Item>& items() const { return builder_.items(); }
    Window window() const { return window_; }
    bool isDismissed() const { return dismissed_; }

    // Called by the frame for any event naming one of our windows. Returns true
    // if it was ours.
    bool handleEvent(const XEvent& e);

    // See selfOwned_. Called by showContextMenu after it has addRef'd.
    void takeSelfOwnership() { selfOwned_ = true; }

private:
    int  measuredWidth() const;
    int  measuredHeight() const;
    int  itemTop(int index) const;
    int  itemAt(int y) const;
    void present();
    void destroyWindow();
    void openChildFor(int index);
    void closeChild();
    void choose(const Item& item);
    void dismiss();
    X11PopupMenu* rootMenu();

    X11DrawingFrame::Impl& frame_;
    gmpi::cpugfx::Factory& factory_;
    gmpi::drawing::api::ITextFormat* font_{};
    gmpi::drawing::Rect anchor_{};
    gmpi::popupmenu::Builder builder_;

    Window window_{};
    XImage* image_{};
    GC      gc_{};
    int     width_ = 0, height_ = 0;

    int  hovered_ = -1;
    bool dismissed_ = false;
    bool grabbed_ = false;

    // A context menu outlives the call that created it: showContextMenu hands
    // the reference over rather than destroying the menu on the way out. The
    // menu drops it when dismissed. Submenus are owned by their parent instead.
    bool selfOwned_ = false;

    X11PopupMenu* child_{};
    X11PopupMenu* parentMenu_{};
};

// ---------------------------------------------------------------------------
// X11TextEdit - ITextEdit as an overlay on the plugin's own window
// ---------------------------------------------------------------------------
// No window of its own. A rename box is a few pixels over the client, and
// giving it an override-redirect window would put it on a different visual
// with its own clipping and its own focus. Instead the frame draws it after
// the client (see Impl::present) and routes keys to it while it is up.
//
// The buffer, caret, selection and keystroke table are gmpi::textedit::Model,
// shared with the Wayland edit and covered by the headless tests.
class X11TextEdit : public gmpi::api::ITextEdit
{
public:
    X11TextEdit(X11DrawingFrame::Impl& frame, gmpi::drawing::api::ITextFormat* font,
                gmpi::drawing::Rect rect)
        : frame_(frame), font_(font), rect_(rect)
    {
        model_.setClipboard = [this](const std::string& s) { frame_.clipboard.setText(s); };
        model_.getClipboard = [this] { return frame_.clipboard.getText(); };
        model_.onChanged    = [this] { notifyChanged(); invalidate(); };
        model_.onFinish     = [this](gmpi::ReturnCode r) { finish(r); };
    }

    // --- ITextEdit ---
    gmpi::ReturnCode setText(const char* text) override
    {
        model_.setText(text ? text : "");
        model_.selectAll();   // showing an edit selects its contents
        return gmpi::ReturnCode::Ok;
    }
    gmpi::ReturnCode setAlignment(int32_t) override { return gmpi::ReturnCode::Ok; }
    gmpi::ReturnCode setTextSize(float) override    { return gmpi::ReturnCode::Ok; }

    gmpi::ReturnCode showAsync(gmpi::api::IUnknown* callback) override
    {
        callback_ = callback;

        // One edit at a time: a second one commits the first, which is what
        // clicking from one rename box into another should do.
        if (frame_.activeEdit && frame_.activeEdit != this)
            frame_.activeEdit->commit();

        frame_.activeEdit = this;
        addRef();             // alive until committed or abandoned
        invalidate();
        return gmpi::ReturnCode::Ok;
    }

    gmpi::ReturnCode queryInterface(const gmpi::api::Guid* iid, void** returnInterface) override
    {
        *returnInterface = {};
        GMPI_QUERYINTERFACE(gmpi::api::ITextEdit);
        return gmpi::ReturnCode::NoSupport;
    }
    GMPI_REFCOUNT;

    bool active() const { return !finished_; }
    void commit() { finish(gmpi::ReturnCode::Ok); }
    const gmpi::drawing::Rect& rect() const { return rect_; }

    void handleKey(uint32_t keysym, uint32_t utf32, int32_t flags, bool down)
    {
        if (finished_)
            return;
        model_.handleKey(keysym, utf32, flags, down);
        invalidate();
    }

    // A click inside places the caret; the frame commits us on a click outside.
    void onPointer(double x, double y, bool pressed);
    void render(gmpi::cpugfx::RenderTarget* rt);

private:
    void invalidate() { frame_.dirtyAll = true; }

    void notifyChanged()
    {
        if (auto cb = callback_.as<gmpi::api::ITextEditCallback>(); cb)
            cb->onChanged(model_.text().c_str());
    }

    void finish(gmpi::ReturnCode result)
    {
        if (finished_)
            return;
        finished_ = true;

        if (frame_.activeEdit == this)
            frame_.activeEdit = nullptr;

        if (auto cb = callback_.as<gmpi::api::ITextEditCallback>(); cb)
            cb->onComplete(result);
        callback_ = {};

        frame_.dirtyAll = true;
        release();   // balances showAsync
    }

    X11DrawingFrame::Impl& frame_;
    gmpi::drawing::api::ITextFormat* font_{};
    gmpi::drawing::Rect rect_{};
    gmpi::textedit::Model model_;
    float scrollX_ = 0.f;
    bool  finished_ = false;
    gmpi::shared_ptr<gmpi::api::IUnknown> callback_;
};

// ---------------------------------------------------------------------------
// X11KeyListener - IKeyListener, an invisible sink for raw keys
// ---------------------------------------------------------------------------
class X11KeyListener : public gmpi::api::IKeyListener
{
public:
    explicit X11KeyListener(X11DrawingFrame::Impl& frame) : frame_(frame) {}

    ~X11KeyListener()
    {
        // Detach only. stop() releases, and by the time a destructor runs the
        // count is already zero - decrementing again turns teardown into a
        // double free.
        if (frame_.keySink == this)
            frame_.keySink = nullptr;
    }

    gmpi::ReturnCode showAsync(gmpi::api::IUnknown* callback) override
    {
        callback_ = callback;

        if (frame_.keySink && frame_.keySink != this)
            frame_.keySink->stop(gmpi::ReturnCode::Cancel);

        frame_.keySink = this;
        addRef();             // alive until focus is lost
        return gmpi::ReturnCode::Ok;
    }

    gmpi::ReturnCode queryInterface(const gmpi::api::Guid* iid, void** returnInterface) override
    {
        *returnInterface = {};
        GMPI_QUERYINTERFACE(gmpi::api::IKeyListener);
        return gmpi::ReturnCode::NoSupport;
    }
    GMPI_REFCOUNT;

    void handleKey(uint32_t keysym, uint32_t utf32, int32_t flags, bool down);
    void stop(gmpi::ReturnCode result);

private:
    static bool isClipboardKey(uint32_t keysym)
    {
        switch (keysym)
        {
        case 'c': case 'C': case 'x': case 'X': case 'v': case 'V': return true;
        default: return false;
        }
    }

    X11DrawingFrame::Impl& frame_;
    gmpi::shared_ptr<gmpi::api::IUnknown> callback_;
    bool stopped_ = false;
};

// ---------------------------------------------------------------------------
// X11Tooltip - a small override-redirect window that never takes input
// ---------------------------------------------------------------------------
// Owned by the frame, not refcounted and not handed to the client: a tooltip is
// something the frame decides to show, not an object the plugin holds.
//
// The event mask is ExposureMask ALONE. A tooltip that accepts button or motion
// events is one the user cannot click through, and it sits right under the
// pointer - so it must be invisible to input, not merely ignore it.
class X11Tooltip
{
public:
    void show(X11DrawingFrame::Impl& frame, gmpi::cpugfx::Factory& factory,
              gmpi::drawing::api::ITextFormat* font,
              const std::string& text, int rootX, int rootY);
    void hide(X11DrawingFrame::Impl& frame);

    bool handleEvent(X11DrawingFrame::Impl& frame, gmpi::cpugfx::Factory& factory, const XEvent& e);

private:
    void present(X11DrawingFrame::Impl& frame, gmpi::cpugfx::Factory& factory);

    Window  window_{};
    XImage* image_{};
    GC      gc_{};
    int     w_ = 0, h_ = 0;
    std::string text_;
    gmpi::drawing::api::ITextFormat* font_{};
};

// ---------------------------------------------------------------------------
// X11ColorDialog - IColorDialog as a transient-for toplevel
// ---------------------------------------------------------------------------
// Same window arrangement as X11StockDialog: an ordinary toplevel the window
// manager decorates, transient for the plugin's window so it stays above the
// DAW. The picker itself - conversions, layout, hit-testing and drawing - is
// gmpi::colorpicker, shared with the Wayland one.
class X11ColorDialog : public gmpi::api::IColorDialog
{
public:
    X11ColorDialog(X11DrawingFrame::Impl& frame, gmpi::cpugfx::Factory& factory,
                   gmpi::drawing::api::ITextFormat* font, gmpi::drawing::Color initialColor)
        : frame_(frame), factory_(factory), font_(font)
    {
        model_.setFromLinear(initialColor);
        frame_.colorDialogs.push_back(this);
    }

    ~X11ColorDialog() { destroyWindow(); }

    gmpi::ReturnCode showAsync(gmpi::api::IUnknown* callback) override;

    gmpi::ReturnCode queryInterface(const gmpi::api::Guid* iid, void** returnInterface) override
    {
        *returnInterface = {};
        GMPI_QUERYINTERFACE(gmpi::api::IColorDialog);
        return gmpi::ReturnCode::NoSupport;
    }
    GMPI_REFCOUNT;

    bool handleEvent(const XEvent& e);

private:
    using Region = gmpi::colorpicker::Model::Region;

    void present();
    void destroyWindow();
    void complete(gmpi::ReturnCode result);

    X11DrawingFrame::Impl& frame_;
    gmpi::cpugfx::Factory& factory_;
    gmpi::drawing::api::ITextFormat* font_{};
    gmpi::colorpicker::Model model_;

    gmpi::shared_ptr<gmpi::api::IUnknown> callback_;

    Window  window_{};
    XImage* image_{};
    GC      gc_{};
    Atom    wmDelete_{};
    int     hoveredButton_ = -1;
    Region  dragging_ = Region::None;
    bool    completed_ = false;
};

// ---------------------------------------------------------------------------
// X11StockDialog - IStockDialog as a transient-for toplevel
// ---------------------------------------------------------------------------
// Much simpler than the Wayland equivalent: X11 has a window manager, so the
// dialog is an ordinary toplevel and the WM decorates it. All we add is
// WM_TRANSIENT_FOR, so it stays above the host's window rather than becoming a
// separate task-bar entry that can fall behind the DAW - which, for a modal
// question, the user experiences as a hang.
//
// Not modal in the X sense. Nothing here grabs: a grab would freeze the DAW's
// own UI too, and a plugin has no business doing that. The dialog simply stays
// on top and answers when clicked.
class X11StockDialog : public gmpi::api::IStockDialog
{
public:
    X11StockDialog(X11DrawingFrame::Impl& frame, gmpi::cpugfx::Factory& factory,
                   gmpi::drawing::api::ITextFormat* font,
                   int32_t dialogType, std::string title, std::string text);

    ~X11StockDialog() { destroyWindow(); }

    gmpi::ReturnCode showAsync(gmpi::api::IUnknown* callback) override;

    gmpi::ReturnCode queryInterface(const gmpi::api::Guid* iid, void** returnInterface) override
    {
        *returnInterface = {};
        GMPI_QUERYINTERFACE(gmpi::api::IStockDialog);
        return gmpi::ReturnCode::NoSupport;
    }
    GMPI_REFCOUNT;

    Window window() const { return window_; }
    bool handleEvent(const XEvent& e);

private:
    struct Button
    {
        std::string label;
        gmpi::api::StockDialogButton id{};
        gmpi::drawing::Rect rect{};
    };

    void layout();
    void present();
    void destroyWindow();
    int  buttonAt(int x, int y) const;
    void finish(gmpi::api::StockDialogButton button);

    X11DrawingFrame::Impl& frame_;
    gmpi::cpugfx::Factory& factory_;
    gmpi::drawing::api::ITextFormat* font_{};
    std::string title_, text_;
    std::vector<Button> buttons_;
    gmpi::api::StockDialogButton escape_{ gmpi::api::StockDialogButton::Cancel };

    gmpi::shared_ptr<gmpi::api::IUnknown> callback_;

    Window  window_{};
    XImage* image_{};
    GC      gc_{};
    Atom    wmDelete_{};
    int     w_ = 0, h_ = 0;
    int     hovered_ = -1;
    bool    finished_ = false;
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

    // The clipboard is a selection owned by a window, so it needs ours.
    d.clipboard.init(d.display, d.window);

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

    if (d.tooltipWindow)
    {
        d.tooltipWindow->hide(d);
        delete d.tooltipWindow;
        d.tooltipWindow = nullptr;
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

        // Menus first, and by window id. They share this connection, so their
        // events arrive here; a menu that has dismissed itself is dropped from
        // the list by its destructor, so an event naming a dead window matches
        // nothing and falls through harmlessly.
        {
            bool consumed = false;
            // Copies: handling can dismiss a menu or close a dialog, which
            // mutates the lists we are walking.
            const auto menus = d.menus;
            const auto dialogs = d.dialogs;
            const auto colorDialogs = d.colorDialogs;
            for (auto* m : menus)
            {
                if (m->handleEvent(e))
                {
                    consumed = true;
                    break;
                }
            }
            for (auto* d2 : dialogs)
            {
                if (!consumed && d2->handleEvent(e))
                {
                    consumed = true;
                    break;
                }
            }
            for (auto* cd : colorDialogs)
            {
                if (!consumed && cd->handleEvent(e))
                {
                    consumed = true;
                    break;
                }
            }
            // Selection traffic is the clipboard's, not the client's.
            if (!consumed && d.clipboard.handleEvent(e))
                consumed = true;

            if (!consumed && d.tooltipWindow && d.tooltipWindow->handleEvent(d, factory_, e))
                consumed = true;

            if (consumed)
                continue;
        }

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
            d.tooltip.onActivity(nowMs());
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
            d.tooltip.suppress();
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

            const gmpi::drawing::Point mp{ static_cast<float>(latest.xmotion.x) / d.scale,
                                           static_cast<float>(latest.xmotion.y) / d.scale };

            // Any movement restarts the wait and takes down whatever is up.
            // Recorded in ROOT coordinates too, because the tooltip is a
            // top-level window and has to be placed in them.
            d.lastMovePoint = mp;
            d.lastMoveRootX = latest.xmotion.x_root;
            d.lastMoveRootY = latest.xmotion.y_root;
            d.tooltip.onActivity(nowMs());

            if (d.inputClient)
                d.inputClient->onPointerMove(mp, basePointerFlags(latest.xmotion.state));
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

            // A click means the user is doing something, not reading.
            d.tooltip.suppress();

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
            focusIfViewable(d.display, d.window);

            // A click inside an active edit places the caret; a click outside
            // commits it, which is what every in-place rename box does. Either
            // way the client does not see this press.
            if (d.activeEdit)
            {
                const gmpi::drawing::Point ep{ static_cast<float>(b.x) / d.scale,
                                               static_cast<float>(b.y) / d.scale };
                const auto& er = d.activeEdit->rect();
                if (ep.x >= er.left && ep.x < er.right && ep.y >= er.top && ep.y < er.bottom)
                    d.activeEdit->onPointer(ep.x, ep.y, true);
                else
                    d.activeEdit->commit();
                break;
            }

            if (d.inputClient)
            {
                const gmpi::drawing::Point pt{ static_cast<float>(b.x) / d.scale,
                                               static_cast<float>(b.y) / d.scale };
                const auto handled = d.inputClient->onPointerDown(pt, flags);

                // Right-click that the client did not claim: ask it what belongs
                // on a context menu, and show one if it says anything. Gated on
                // Unhandled so a control with its own right-drag keeps it, and
                // an empty menu is the client saying "not here" - put nothing on
                // screen rather than an empty box. Mirrors the Wayland frame.
                if (b.button == Button3 && handled == gmpi::ReturnCode::Unhandled)
                    showContextMenu(pt);
            }
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
        case KeyRelease:
        {
            char buf[32]{};
            KeySym keysym{};
            const int n = XLookupString(&e.xkey, buf, sizeof(buf) - 1, &keysym, nullptr);
            const bool down = (e.type == KeyPress);
            const uint32_t utf32 = (n > 0) ? uint32_t(uint8_t(buf[0])) : 0u;
            const int32_t keyFlags = modifierFlags(e.xkey.state);

            // An in-place edit or a key listener owns the keyboard while it is
            // up, or typing into a rename box would also drive the client's
            // shortcuts.
            if (d.activeEdit)
            {
                d.activeEdit->handleKey(uint32_t(keysym), utf32, keyFlags, down);
                break;
            }
            if (d.keySink)
            {
                d.keySink->handleKey(uint32_t(keysym), utf32, keyFlags, down);
                break;
            }

            if (!down)
                break;   // the client's onKeyPress has no release half

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

    // Tooltips. The client is asked ONLY when the pointer has been still long
    // enough - a hit-test of its whole tree at pointer rate is exactly what the
    // delay exists to avoid. Suppressed while captured: mid-drag the user is
    // not reading, and the pointer is not where the tooltip would describe.
    if (d.tooltip.takeHideRequest() && d.tooltipWindow)
        d.tooltipWindow->hide(d);

    if (d.inputClient && !d.captured && d.pointerInside && d.tooltip.ready(nowMs()))
    {
        gmpi::ReturnString text;
        if (d.inputClient->getToolTip(d.lastMovePoint, &text) == gmpi::ReturnCode::Ok
            && text.getSize() > 0)
        {
            if (!d.tooltipWindow)
                d.tooltipWindow = new X11Tooltip;

            d.tooltipWindow->show(d, factory_, d.menuFont,
                                  std::string(text.getData(), size_t(text.getSize())),
                                  d.lastMoveRootX, d.lastMoveRootY);
            d.tooltip.markShown();
        }
    }

    // The portal replies on the session bus, not the X connection, so it needs
    // pumping too. Doing it here rather than requiring the host to register a
    // second descriptor: pump() is non-blocking and a no-op until a file dialog
    // has connected the bus, and a file chooser does not care about 16ms.
    d.portalBus.pump();
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

    // An in-place edit sits ON the client, so it is drawn after - same order as
    // the Wayland frame's drawActiveTextEdit.
    if (d.activeEdit)
        d.activeEdit->render(rt);

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

// ---------------------------------------------------------------------------
// X11PopupMenu
// ---------------------------------------------------------------------------

int X11PopupMenu::itemTop(int index) const
{
    int top = kMenuPadV;
    for (int i = 0; i < index && i < (int)items().size(); ++i)
        top += items()[i].separator ? kMenuSeparatorHeight : kMenuItemHeight;
    return top;
}

int X11PopupMenu::measuredHeight() const
{
    return itemTop(int(items().size())) + kMenuPadV;
}

int X11PopupMenu::measuredWidth() const
{
    // Without a text engine there is nothing to measure against, so fall back to
    // a width that at least shows the highlight and the structure.
    float widest = 0.0f;
    if (font_)
    {
        for (const auto& item : items())
        {
            if (item.separator)
                continue;
            gmpi::drawing::Size sz{};
            font_->getTextExtentU(item.label.c_str(), int32_t(item.label.size()), 10000.0f, &sz);
            widest = (std::max)(widest, sz.width);
        }
    }

    return (std::max)(kMenuMinWidth,
                      int(widest) + kMenuTickGutter + kMenuArrowGutter + 8);
}

int X11PopupMenu::itemAt(int y) const
{
    int top = kMenuPadV;
    for (size_t i = 0; i < items().size(); ++i)
    {
        const int h = items()[i].separator ? kMenuSeparatorHeight : kMenuItemHeight;
        if (y >= top && y < top + h)
            return items()[i].separator ? -1 : int(i);
        top += h;
    }
    return -1;
}

gmpi::ReturnCode X11PopupMenu::showAsync()
{
    auto& d = frame_;
    if (!d.display || items().empty())
        return gmpi::ReturnCode::Fail;

    width_  = measuredWidth();
    height_ = measuredHeight();

    // Anchor is in the FRAME's coordinates; a root-window child needs screen
    // coordinates, so translate through the frame's window.
    int rootX = 0, rootY = 0;
    Window ignored{};
    XTranslateCoordinates(d.display, d.window, DefaultRootWindow(d.display),
                          int(anchor_.left * d.scale), int(anchor_.bottom * d.scale),
                          &rootX, &rootY, &ignored);

    // Keep it on screen. A menu that opens past the bottom edge is a menu whose
    // last items cannot be reached.
    const int screenW = DisplayWidth(d.display, d.screen);
    const int screenH = DisplayHeight(d.display, d.screen);
    if (rootX + width_ > screenW)  rootX = (std::max)(0, screenW - width_);
    if (rootY + height_ > screenH) rootY = (std::max)(0, rootY - height_ - int(anchor_.bottom - anchor_.top));

    XSetWindowAttributes attr{};
    attr.override_redirect = True;   // the window manager must not decorate or move it
    attr.background_pixel = 0;
    attr.border_pixel = 0;
    attr.colormap = XCreateColormap(d.display, DefaultRootWindow(d.display), d.visual, AllocNone);
    attr.event_mask = ExposureMask | ButtonPressMask | ButtonReleaseMask
                    | PointerMotionMask | KeyPressMask | LeaveWindowMask;

    window_ = XCreateWindow(d.display, DefaultRootWindow(d.display),
                            rootX, rootY, unsigned(width_), unsigned(height_),
                            0, d.depth, InputOutput, d.visual,
                            CWOverrideRedirect | CWBorderPixel | CWColormap | CWEventMask,
                            &attr);
    if (!window_)
        return gmpi::ReturnCode::Fail;

    gc_ = XCreateGC(d.display, window_, 0, nullptr);
    XMapRaised(d.display, window_);

    // Only the root menu grabs. GrabModeAsync on both so the server keeps
    // delivering to us rather than freezing the device, and owner_events True so
    // our own windows still get their events normally.
    if (!parentMenu_)
    {
        XGrabPointer(d.display, window_, True,
                     ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
                     GrabModeAsync, GrabModeAsync, 0, 0, CurrentTime);
        XGrabKeyboard(d.display, window_, True, GrabModeAsync, GrabModeAsync, CurrentTime);
        grabbed_ = true;
    }

    present();
    XFlush(d.display);
    return gmpi::ReturnCode::Ok;
}

void X11PopupMenu::present()
{
    auto& d = frame_;
    if (!d.display || !window_ || width_ <= 0 || height_ <= 0)
        return;

    gmpi::drawing::api::IBitmapRenderTarget* rtRaw{};
    factory_.createCpuRenderTarget({ uint32_t(width_), uint32_t(height_) }, 0, &rtRaw, 96.0f);
    auto* rt = dynamic_cast<gmpi::cpugfx::RenderTarget*>(rtRaw);
    if (!rt)
    {
        if (rtRaw) rtRaw->release();
        return;
    }

    rt->beginDraw();

    auto brush = [&](gmpi::drawing::Color c)
    {
        gmpi::drawing::api::ISolidColorBrush* b{};
        rt->createSolidColorBrush(&c, nullptr, &b);
        return b;
    };

    // Same palette as the Wayland menu, deliberately.
    const gmpi::drawing::Color bg{ 0.16f, 0.17f, 0.19f, 1.0f };
    rt->clear(&bg);

    auto* ink    = brush({ 0.92f, 0.93f, 0.95f, 1.0f });
    auto* grey   = brush({ 0.45f, 0.46f, 0.50f, 1.0f });
    auto* hilite = brush({ 0.16f, 0.42f, 0.75f, 1.0f });
    auto* rule   = brush({ 0.28f, 0.29f, 0.32f, 1.0f });

    for (size_t i = 0; i < items().size(); ++i)
    {
        const auto& item = items()[i];
        const float top = float(itemTop(int(i)));

        if (item.separator)
        {
            if (rule)
            {
                const gmpi::drawing::Rect r{ 6.f, top + kMenuSeparatorHeight * 0.5f,
                                             float(width_) - 6.f,
                                             top + kMenuSeparatorHeight * 0.5f + 1.f };
                rt->fillRectangle(&r, rule);
            }
            continue;
        }

        if (int(i) == hovered_ && !item.grayed && hilite)
        {
            const gmpi::drawing::Rect r{ 3.f, top, float(width_) - 3.f, top + kMenuItemHeight };
            rt->fillRectangle(&r, hilite);
        }

        auto* fg = item.grayed ? grey : ink;
        if (!fg || !font_)
            continue;

        if (item.ticked)
        {
            const char* tick = "\xe2\x9c\x93";
            const gmpi::drawing::Rect r{ 8.f, top + 3.f, 26.f, top + kMenuItemHeight };
            rt->drawTextU(tick, 3, font_, &r, fg, 0);
        }

        const gmpi::drawing::Rect r{ float(kMenuTickGutter) + 2.f, top + 3.f,
                                     float(width_) - kMenuArrowGutter, top + kMenuItemHeight };
        rt->drawTextU(item.label.c_str(), uint32_t(item.label.size()), font_, &r, fg, 0);

        if (!item.submenu.empty())
        {
            const char* arrow = "\xe2\x80\xba";
            const gmpi::drawing::Rect a{ float(width_) - 20.f, top + 3.f,
                                         float(width_) - 6.f, top + kMenuItemHeight };
            rt->drawTextU(arrow, 3, font_, &a, fg, 0);
        }
    }

    if (ink)    ink->release();
    if (grey)   grey->release();
    if (hilite) hilite->release();
    if (rule)   rule->release();

    rt->endDraw();

    gmpi::drawing::api::IBitmap* bmRaw{};
    rt->getBitmap(&bmRaw);
    if (auto* bm = dynamic_cast<gmpi::cpugfx::Bitmap*>(bmRaw))
    {
        if (!image_)
        {
            auto* data = static_cast<char*>(std::calloc(size_t(width_) * height_, 4));
            if (data)
                image_ = XCreateImage(d.display, d.visual, d.depth, ZPixmap, 0,
                                      data, width_, height_, 32, width_ * 4);
            if (!image_ && data)
                std::free(data);
        }

        if (image_)
        {
            const auto& s = bm->surface;
            const gmpi::cpugfx::SourceSurface src{ s.pixels, s.stridePixels, s.width, s.height };
            const gmpi::cpugfx::DestSurface   dst{ reinterpret_cast<uint8_t*>(image_->data),
                                                   image_->bytes_per_line, width_, height_,
                                                   gmpi::cpugfx::PixelEncoding::Bgra8888 };
            gmpi::cpugfx::encodeDirtyRect(src, dst, gmpi::drawing::RectL{ 0, 0, width_, height_ });

            XPutImage(d.display, window_, gc_, image_, 0, 0, 0, 0,
                      unsigned(width_), unsigned(height_));
        }
    }

    if (bmRaw) bmRaw->release();
    rtRaw->release();

    XFlush(d.display);
}

void X11PopupMenu::destroyWindow()
{
    auto& d = frame_;
    closeChild();

    if (!d.display)
        return;

    if (grabbed_)
    {
        XUngrabPointer(d.display, CurrentTime);
        XUngrabKeyboard(d.display, CurrentTime);
        grabbed_ = false;
    }

    if (image_)  { XDestroyImage(image_); image_ = {}; }
    if (gc_)     { XFreeGC(d.display, gc_); gc_ = {}; }
    if (window_) { XDestroyWindow(d.display, window_); window_ = {}; }

    XFlush(d.display);
}

X11PopupMenu* X11PopupMenu::rootMenu()
{
    auto* m = this;
    while (m->parentMenu_)
        m = m->parentMenu_;
    return m;
}

void X11PopupMenu::closeChild()
{
    if (!child_)
        return;

    auto* c = child_;
    child_ = nullptr;
    c->dismissed_ = true;
    c->destroyWindow();
    c->release();
}

void X11PopupMenu::openChildFor(int index)
{
    closeChild();

    if (index < 0 || index >= (int)items().size() || items()[index].submenu.empty())
        return;

    // Anchor in the PARENT's coordinates: the right edge of the hovered item.
    // showAsync translates through the frame's window, so express it there by
    // going via the root - the submenu's parent is this menu, not the frame.
    auto& d = frame_;
    int rootX = 0, rootY = 0, frameX = 0, frameY = 0;
    Window ignored{};
    XTranslateCoordinates(d.display, window_, DefaultRootWindow(d.display),
                          width_ - 6, itemTop(index), &rootX, &rootY, &ignored);
    XTranslateCoordinates(d.display, DefaultRootWindow(d.display), d.window,
                          rootX, rootY, &frameX, &frameY, &ignored);

    const gmpi::drawing::Rect itemRect{ float(frameX) / d.scale, float(frameY) / d.scale,
                                        float(frameX) / d.scale + 1.f,
                                        float(frameY) / d.scale };

    auto* c = new X11PopupMenu(frame_, factory_, font_, itemRect);
    c->builder_.adopt(items()[index].submenu);
    c->parentMenu_ = this;

    child_ = c;
    c->showAsync();
}

void X11PopupMenu::choose(const Item& item)
{
    auto callback = item.callback;   // copy: as<> is non-const
    if (auto cb = callback.as<gmpi::api::IPopupMenuCallback>(); cb)
        cb->onComplete(gmpi::ReturnCode::Ok, item.id);

    // Picking from a submenu closes the whole menu, not just that level.
    // `this` may be gone after it, so touch nothing below.
    rootMenu()->dismiss();
}

void X11PopupMenu::dismiss()
{
    if (dismissed_)
        return;
    dismissed_ = true;
    destroyWindow();

    // LAST: this may delete us, so nothing may touch a member afterwards.
    if (selfOwned_)
    {
        selfOwned_ = false;
        release();
    }
}

bool X11PopupMenu::handleEvent(const XEvent& e)
{
    if (!window_ || e.xany.window != window_)
        return false;

    switch (e.type)
    {
    case Expose:
        present();
        return true;

    case MotionNotify:
    {
        const int index = itemAt(e.xmotion.y);
        if (index != hovered_)
        {
            hovered_ = index;
            present();

            // Hovering a plain item closes any open submenu; hovering a header
            // opens its own. Both are what every desktop menu does.
            if (index >= 0 && !items()[index].submenu.empty())
                openChildFor(index);
            else
                closeChild();
        }
        return true;
    }

    case ButtonPress:
        return true;   // act on release, like every other menu

    case ButtonRelease:
    {
        const int index = itemAt(e.xbutton.y);

        // Outside the menu: dismiss. With owner_events True the grab still
        // reports coordinates relative to this window, so a click elsewhere
        // lands outside its bounds.
        if (e.xbutton.x < 0 || e.xbutton.y < 0 ||
            e.xbutton.x >= width_ || e.xbutton.y >= height_)
        {
            rootMenu()->dismiss();
            return true;
        }

        if (index >= 0 && !items()[index].grayed)
        {
            // A submenu header is not a choice - clicking it must not dismiss.
            if (items()[index].submenu.empty())
                choose(items()[index]);
            else
                openChildFor(index);
        }
        return true;
    }

    case KeyPress:
    {
        KeySym keysym{};
        char buf[8]{};
        XLookupString(const_cast<XKeyEvent*>(&e.xkey), buf, sizeof(buf) - 1, &keysym, nullptr);

        if (keysym == XK_Escape)
        {
            rootMenu()->dismiss();
            return true;
        }

        auto step = [&](int dir)
        {
            const int n = int(items().size());
            for (int k = 1; k <= n; ++k)
            {
                const int i = ((hovered_ < 0 ? (dir > 0 ? -1 : 0) : hovered_) + dir * k + 2 * n) % n;
                if (!items()[i].separator && !items()[i].grayed)
                {
                    hovered_ = i;
                    present();
                    return;
                }
            }
        };

        if (keysym == XK_Down)  { step(+1); return true; }
        if (keysym == XK_Up)    { step(-1); return true; }

        if ((keysym == XK_Return || keysym == XK_KP_Enter) &&
            hovered_ >= 0 && !items()[hovered_].grayed)
        {
            if (items()[hovered_].submenu.empty())
                choose(items()[hovered_]);
            else
                openChildFor(hovered_);
            return true;
        }
        return true;
    }

    default:
        return false;
    }
}

// ---------------------------------------------------------------------------
// X11TextEdit / X11KeyListener
// ---------------------------------------------------------------------------

void X11TextEdit::onPointer(double x, double y, bool pressed)
{
    if (!pressed || !font_ || finished_)
        return;

    (void)y;

    // Place the caret at the character boundary nearest the click, by measuring
    // each prefix. Linear, but a text edit holds a name, not a document.
    const double target = x - rect_.left - 3.0 + scrollX_;
    const std::string& text = model_.text();

    int32_t best = 0;
    double bestDist = 1e30;
    for (int32_t i = 0; i <= int32_t(text.size()); i = model_.nextCodepoint(i))
    {
        gmpi::drawing::Size sz{};
        if (i > 0)
            font_->getTextExtentU(text.c_str(), i, 100000.f, &sz);

        const double dist = std::fabs(double(sz.width) - target);
        if (dist < bestDist)
        {
            bestDist = dist;
            best = i;
        }
        if (i == int32_t(text.size()))
            break;
    }

    model_.placeCaret(best);
    invalidate();
}

void X11TextEdit::render(gmpi::cpugfx::RenderTarget* rt)
{
    if (finished_ || !rt)
        return;

    auto brush = [&](gmpi::drawing::Color c)
    {
        gmpi::drawing::api::ISolidColorBrush* b{};
        rt->createSolidColorBrush(&c, nullptr, &b);
        return b;
    };

    // Same palette as the Wayland edit.
    auto* back = brush({ 0.10f, 0.11f, 0.13f, 1.0f });
    auto* edge = brush({ 0.38f, 0.55f, 0.85f, 1.0f });
    auto* ink  = brush({ 0.94f, 0.95f, 0.97f, 1.0f });
    auto* sel  = brush({ 0.16f, 0.42f, 0.75f, 1.0f });

    if (back) rt->fillRectangle(&rect_, back);
    if (edge) rt->drawRectangle(&rect_, edge, 1.f, nullptr);

    const std::string& text = model_.text();

    auto widthTo = [&](int32_t i) -> float
    {
        if (i <= 0 || !font_) return 0.f;
        gmpi::drawing::Size sz{};
        font_->getTextExtentU(text.c_str(), i, 100000.f, &sz);
        return sz.width;
    };

    const float span = (rect_.right - 2.f) - (rect_.left + 3.f);
    scrollX_ = gmpi::textedit::Model::scrollFor(widthTo(model_.caret()),
                                                widthTo(int32_t(text.size())), span, scrollX_);

    const float textLeft = rect_.left + 3.f - scrollX_;
    const float textTop  = rect_.top + 2.f;

    rt->pushAxisAlignedClip(&rect_);

    if (sel && model_.hasSelection())
    {
        const auto [a, b] = model_.selection();
        const gmpi::drawing::Rect r{ textLeft + widthTo(a), rect_.top + 1.f,
                                     textLeft + widthTo(b), rect_.bottom - 1.f };
        rt->fillRectangle(&r, sel);
    }

    if (font_ && ink && !text.empty())
    {
        // The clip limits the right edge, not the rect, so a scrolled-off tail
        // cannot spill past the box.
        const gmpi::drawing::Rect tr{ textLeft, textTop, textLeft + 100000.f, rect_.bottom };
        rt->drawTextU(text.c_str(), uint32_t(text.size()), font_, &tr, ink, 0);
    }

    // Solid rather than blinking: a blink needs a timer of its own, and a steady
    // caret is not the thing anyone notices about a rename box.
    if (ink)
    {
        const float cx = textLeft + widthTo(model_.caret());
        const gmpi::drawing::Rect c{ cx, rect_.top + 2.f, cx + 1.5f, rect_.bottom - 2.f };
        rt->fillRectangle(&c, ink);
    }

    rt->popAxisAlignedClip();

    if (back) back->release();
    if (edge) edge->release();
    if (ink)  ink->release();
    if (sel)  sel->release();
}

void X11KeyListener::stop(gmpi::ReturnCode result)
{
    if (stopped_)
        return;
    stopped_ = true;

    if (frame_.keySink == this)
        frame_.keySink = nullptr;

    if (auto cb = callback_.as<gmpi::api::IKeyListenerCallback>(); cb)
        cb->onLostFocus(result);
    callback_ = {};

    release();   // balances showAsync
}

void X11KeyListener::handleKey(uint32_t keysym, uint32_t utf32, int32_t flags, bool down)
{
    auto cb = callback_.as<gmpi::api::IKeyListenerCallback>();
    if (!cb)
        return;

    const bool ctrl = (flags & int32_t(gmpi::api::PointerFlags::KeyControl)) != 0;

    // Clipboard first: the callback owns the text, we own the selection.
    //
    // Both edges are swallowed, but only the press acts. Acting on the release
    // as well would copy twice; delivering the release as an ordinary keystroke
    // would hand the caller an onKeyUp with no matching onKeyDown.
    if (ctrl && isClipboardKey(keysym))
    {
        if (!down)
            return;

        switch (keysym)
        {
        case 'c': case 'C':
        case 'x': case 'X':
        {
            gmpi::ReturnString text;
            if (keysym == 'x' || keysym == 'X')
                cb->cut(&text);
            else
                cb->copy(&text);

            frame_.clipboard.setText(std::string(text.getData(), size_t(text.getSize())));
            return;
        }
        case 'v': case 'V':
        {
            const std::string text = frame_.clipboard.getText();
            if (!text.empty())
                cb->paste(text.c_str(), text.size());
            return;
        }
        default: break;
        }
    }

    // Escape gives the focus back rather than being delivered as a keystroke,
    // which is what every caller of this expects to end an edit.
    if (down && keysym == XK_Escape)
    {
        stop(gmpi::ReturnCode::Cancel);
        return;
    }

    const int32_t key = utf32 ? int32_t(utf32) : int32_t(keysym);
    if (down)
        cb->onKeyDown(key, flags);
    else
        cb->onKeyUp(key, flags);
}

// ---------------------------------------------------------------------------
// X11Tooltip
// ---------------------------------------------------------------------------

void X11Tooltip::show(X11DrawingFrame::Impl& frame, gmpi::cpugfx::Factory& factory,
                      gmpi::drawing::api::ITextFormat* font,
                      const std::string& text, int rootX, int rootY)
{
    hide(frame);

    auto& d = frame;
    if (!d.display || text.empty())
        return;

    text_ = text;
    font_ = font;

    const auto box = gmpi::tooltip::Model::layout(
        text_, font_, float(rootX), float(rootY),
        float(DisplayWidth(d.display, d.screen)), float(DisplayHeight(d.display, d.screen)));

    w_ = int(box.right - box.left);
    h_ = int(box.bottom - box.top);
    if (w_ <= 0 || h_ <= 0)
        return;

    XSetWindowAttributes attr{};
    attr.override_redirect = True;   // the window manager must not decorate it
    attr.border_pixel = 0;
    attr.colormap = XCreateColormap(d.display, DefaultRootWindow(d.display), d.visual, AllocNone);
    attr.event_mask = ExposureMask;  // deliberately nothing else - see the class comment

    window_ = XCreateWindow(d.display, DefaultRootWindow(d.display),
                            int(box.left), int(box.top), unsigned(w_), unsigned(h_),
                            0, d.depth, InputOutput, d.visual,
                            CWOverrideRedirect | CWBorderPixel | CWColormap | CWEventMask,
                            &attr);
    if (!window_)
        return;

    // Belt and braces: an empty input region means the server routes pointer
    // events straight through to whatever is underneath, even if a future edit
    // adds a mask by accident.
    // Both out-params are written unconditionally - passing nullptr segfaults
    // inside libXext rather than returning False.
    int shapeEventBase = 0, shapeErrorBase = 0;
    if (XShapeQueryExtension(d.display, &shapeEventBase, &shapeErrorBase))
    {
        const XRectangle empty{ 0, 0, 0, 0 };
        XShapeCombineRectangles(d.display, window_, ShapeInput, 0, 0,
                                const_cast<XRectangle*>(&empty), 1, ShapeSet, Unsorted);
    }

    gc_ = XCreateGC(d.display, window_, 0, nullptr);
    XMapRaised(d.display, window_);
    present(frame, factory);
    XFlush(d.display);
}

void X11Tooltip::hide(X11DrawingFrame::Impl& frame)
{
    auto& d = frame;
    if (!d.display)
        return;

    if (image_)  { XDestroyImage(image_); image_ = {}; }
    if (gc_)     { XFreeGC(d.display, gc_); gc_ = {}; }
    if (window_) { XDestroyWindow(d.display, window_); window_ = {}; XFlush(d.display); }
}

bool X11Tooltip::handleEvent(X11DrawingFrame::Impl& frame, gmpi::cpugfx::Factory& factory,
                             const XEvent& e)
{
    if (!window_ || e.xany.window != window_)
        return false;

    if (e.type == Expose)
    {
        present(frame, factory);
        return true;
    }
    return false;
}

void X11Tooltip::present(X11DrawingFrame::Impl& frame, gmpi::cpugfx::Factory& factory)
{
    auto& d = frame;
    if (!d.display || !window_)
        return;

    gmpi::drawing::api::IBitmapRenderTarget* rtRaw{};
    factory.createCpuRenderTarget({ uint32_t(w_), uint32_t(h_) }, 0, &rtRaw, 96.0f);
    auto* rt = dynamic_cast<gmpi::cpugfx::RenderTarget*>(rtRaw);
    if (!rt)
    {
        if (rtRaw) rtRaw->release();
        return;
    }

    rt->beginDraw();
    gmpi::tooltip::Model::render(rt, text_, font_, float(w_), float(h_));
    rt->endDraw();

    gmpi::drawing::api::IBitmap* bmRaw{};
    rt->getBitmap(&bmRaw);
    if (auto* bm = dynamic_cast<gmpi::cpugfx::Bitmap*>(bmRaw))
    {
        if (!image_)
        {
            auto* data = static_cast<char*>(std::calloc(size_t(w_) * h_, 4));
            if (data)
                image_ = XCreateImage(d.display, d.visual, d.depth, ZPixmap, 0,
                                      data, w_, h_, 32, w_ * 4);
            if (!image_ && data)
                std::free(data);
        }

        if (image_)
        {
            const auto& s = bm->surface;
            const gmpi::cpugfx::SourceSurface src{ s.pixels, s.stridePixels, s.width, s.height };
            const gmpi::cpugfx::DestSurface   dst{ reinterpret_cast<uint8_t*>(image_->data),
                                                   image_->bytes_per_line, w_, h_,
                                                   gmpi::cpugfx::PixelEncoding::Bgra8888 };
            gmpi::cpugfx::encodeDirtyRect(src, dst, gmpi::drawing::RectL{ 0, 0, w_, h_ });
            XPutImage(d.display, window_, gc_, image_, 0, 0, 0, 0, unsigned(w_), unsigned(h_));
        }
    }

    if (bmRaw) bmRaw->release();
    rtRaw->release();
    XFlush(d.display);
}

// ---------------------------------------------------------------------------
// X11ColorDialog
// ---------------------------------------------------------------------------

gmpi::ReturnCode X11ColorDialog::showAsync(gmpi::api::IUnknown* callback)
{
    auto& d = frame_;
    if (!d.display)
        return gmpi::ReturnCode::Fail;

    const int w = model_.contentWidth();
    const int h = model_.contentHeight();

    // Centred on the host's window, then clamped - the picker is wider than
    // many plugin views, so the raw result is often off-screen.
    int rootX = 0, rootY = 0;
    Window ignored{};
    XTranslateCoordinates(d.display, d.window, DefaultRootWindow(d.display),
                          (d.width - w) / 2, (d.height - h) / 2, &rootX, &rootY, &ignored);

    const int screenW = DisplayWidth(d.display, d.screen);
    const int screenH = DisplayHeight(d.display, d.screen);
    rootX = (std::clamp)(rootX, 0, (std::max)(0, screenW - w));
    rootY = (std::clamp)(rootY, 0, (std::max)(0, screenH - h));

    XSetWindowAttributes attr{};
    attr.border_pixel = 0;
    attr.colormap = XCreateColormap(d.display, DefaultRootWindow(d.display), d.visual, AllocNone);
    attr.event_mask = ExposureMask | ButtonPressMask | ButtonReleaseMask
                    | PointerMotionMask | KeyPressMask | StructureNotifyMask;

    window_ = XCreateWindow(d.display, DefaultRootWindow(d.display),
                            rootX, rootY, unsigned(w), unsigned(h),
                            0, d.depth, InputOutput, d.visual,
                            CWBorderPixel | CWColormap | CWEventMask, &attr);
    if (!window_)
        return gmpi::ReturnCode::Fail;

    XSetTransientForHint(d.display, window_, d.window);
    XStoreName(d.display, window_, "Colour");

    wmDelete_ = XInternAtom(d.display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(d.display, window_, &wmDelete_, 1);

    gc_ = XCreateGC(d.display, window_, 0, nullptr);
    XMapRaised(d.display, window_);

    // Same reasoning as the stock dialog: without focus, Escape and Return
    // never arrive, and nothing guarantees a window manager is running.
    XSync(d.display, False);
    focusIfViewable(d.display, window_);

    callback_ = callback;
    addRef();   // alive until the user answers

    present();
    XFlush(d.display);
    return gmpi::ReturnCode::Ok;
}

void X11ColorDialog::present()
{
    auto& d = frame_;
    if (!d.display || !window_)
        return;

    const int w = model_.contentWidth();
    const int h = model_.contentHeight();

    gmpi::drawing::api::IBitmapRenderTarget* rtRaw{};
    factory_.createCpuRenderTarget({ uint32_t(w), uint32_t(h) }, 0, &rtRaw, 96.0f);
    auto* rt = dynamic_cast<gmpi::cpugfx::RenderTarget*>(rtRaw);
    if (!rt)
    {
        if (rtRaw) rtRaw->release();
        return;
    }

    rt->beginDraw();
    gmpi::colorpicker::render(rt, model_, font_, hoveredButton_);
    rt->endDraw();

    gmpi::drawing::api::IBitmap* bmRaw{};
    rt->getBitmap(&bmRaw);
    if (auto* bm = dynamic_cast<gmpi::cpugfx::Bitmap*>(bmRaw))
    {
        if (!image_)
        {
            auto* data = static_cast<char*>(std::calloc(size_t(w) * h, 4));
            if (data)
                image_ = XCreateImage(d.display, d.visual, d.depth, ZPixmap, 0,
                                      data, w, h, 32, w * 4);
            if (!image_ && data)
                std::free(data);
        }

        if (image_)
        {
            const auto& s = bm->surface;
            const gmpi::cpugfx::SourceSurface src{ s.pixels, s.stridePixels, s.width, s.height };
            const gmpi::cpugfx::DestSurface   dst{ reinterpret_cast<uint8_t*>(image_->data),
                                                   image_->bytes_per_line, w, h,
                                                   gmpi::cpugfx::PixelEncoding::Bgra8888 };
            gmpi::cpugfx::encodeDirtyRect(src, dst, gmpi::drawing::RectL{ 0, 0, w, h });
            XPutImage(d.display, window_, gc_, image_, 0, 0, 0, 0, unsigned(w), unsigned(h));
        }
    }

    if (bmRaw) bmRaw->release();
    rtRaw->release();

    XFlush(d.display);
}

void X11ColorDialog::destroyWindow()
{
    auto& d = frame_;
    std::erase(d.colorDialogs, this);

    if (!d.display)
        return;

    if (image_)  { XDestroyImage(image_); image_ = {}; }
    if (gc_)     { XFreeGC(d.display, gc_); gc_ = {}; }
    if (window_) { XDestroyWindow(d.display, window_); window_ = {}; }

    XFlush(d.display);
}

void X11ColorDialog::complete(gmpi::ReturnCode result)
{
    if (completed_)
        return;
    completed_ = true;

    auto cb = callback_;
    const auto picked = model_.color();
    callback_ = {};

    destroyWindow();

    if (auto sink = cb.as<gmpi::api::IColorDialogCallback>(); sink)
        sink->onComplete(result, picked);

    // LAST: drops the reference showAsync took, which may delete us.
    release();
}

bool X11ColorDialog::handleEvent(const XEvent& e)
{
    if (!window_ || e.xany.window != window_)
        return false;

    switch (e.type)
    {
    case Expose:
        present();
        return true;

    case MotionNotify:
    {
        // A drag keeps tracking outside the control: releasing the pointer is
        // what ends it, not wandering off the edge of the square.
        if (dragging_ != Region::None)
        {
            if (model_.applyDrag(dragging_, e.xmotion.x, e.xmotion.y))
                present();
            return true;
        }

        const int hit = model_.buttonAt(e.xmotion.x, e.xmotion.y);
        if (hit != hoveredButton_)
        {
            hoveredButton_ = hit;
            present();
        }
        return true;
    }

    case ButtonPress:
    {
        const Region r = model_.regionAt(e.xbutton.x, e.xbutton.y);
        if (r == Region::SatVal || r == Region::Hue || r == Region::Alpha)
        {
            dragging_ = r;
            if (model_.applyDrag(r, e.xbutton.x, e.xbutton.y))
                present();
        }
        return true;
    }

    case ButtonRelease:
    {
        if (dragging_ != Region::None)
        {
            dragging_ = Region::None;
            return true;
        }

        // Act on release, so a press that slid off a button does not count.
        switch (model_.regionAt(e.xbutton.x, e.xbutton.y))
        {
        case Region::Ok:     complete(gmpi::ReturnCode::Ok); break;
        case Region::Cancel: complete(gmpi::ReturnCode::Cancel); break;
        default: break;
        }
        return true;
    }

    case KeyPress:
    {
        KeySym keysym{};
        char buf[8]{};
        XLookupString(const_cast<XKeyEvent*>(&e.xkey), buf, sizeof(buf) - 1, &keysym, nullptr);

        if (keysym == XK_Escape)
            complete(gmpi::ReturnCode::Cancel);
        else if (keysym == XK_Return || keysym == XK_KP_Enter)
            complete(gmpi::ReturnCode::Ok);
        return true;
    }

    case ClientMessage:
        if (static_cast<Atom>(e.xclient.data.l[0]) == wmDelete_)
            complete(gmpi::ReturnCode::Cancel);
        return true;

    default:
        return false;
    }
}

// ---------------------------------------------------------------------------
// X11StockDialog
// ---------------------------------------------------------------------------

X11StockDialog::X11StockDialog(X11DrawingFrame::Impl& frame, gmpi::cpugfx::Factory& factory,
                               gmpi::drawing::api::ITextFormat* font,
                               int32_t dialogType, std::string title, std::string text)
    : frame_(frame), factory_(factory), font_(font),
      title_(std::move(title)), text_(std::move(text))
{
    using T = gmpi::api::StockDialogType;
    using B = gmpi::api::StockDialogButton;

    // Same button sets and same escape mapping as the Wayland dialog.
    switch (static_cast<T>(dialogType))
    {
    case T::OkCancel:    buttons_ = { { "OK", B::Ok }, { "Cancel", B::Cancel } };
                         escape_ = B::Cancel; break;
    case T::YesNo:       buttons_ = { { "Yes", B::Yes }, { "No", B::No } };
                         escape_ = B::No; break;
    case T::YesNoCancel: buttons_ = { { "Yes", B::Yes }, { "No", B::No }, { "Cancel", B::Cancel } };
                         escape_ = B::Cancel; break;
    default:             buttons_ = { { "OK", B::Ok } };
                         escape_ = B::Ok; break;
    }

    frame_.dialogs.push_back(this);
}

void X11StockDialog::layout()
{
    // Measure the message so a long one gets a taller box rather than a clipped
    // one. Without a font we cannot measure, so fall back to a fixed height.
    float textH = 40.0f;
    if (font_)
    {
        gmpi::drawing::Size sz{};
        if (font_->getTextExtentU(text_.c_str(), int32_t(text_.size()),
                                  float(kDialogTextWidth), &sz) == gmpi::ReturnCode::Ok)
            textH = (std::max)(20.0f, sz.height);
    }

    w_ = kDialogTextWidth + 2 * kDialogMargin;
    h_ = kDialogMargin + int(textH) + 24 + kDialogButtonH + kDialogMargin;

    // buttons right-aligned along the bottom, first one rightmost so the default
    // sits where the eye lands
    int x = w_ - kDialogMargin;
    const int y = h_ - kDialogMargin - kDialogButtonH;
    for (auto& b : buttons_)
    {
        x -= kDialogButtonW;
        b.rect = { float(x), float(y), float(x + kDialogButtonW), float(y + kDialogButtonH) };
        x -= kDialogButtonGap;
    }
}

int X11StockDialog::buttonAt(int x, int y) const
{
    for (size_t i = 0; i < buttons_.size(); ++i)
    {
        const auto& r = buttons_[i].rect;
        if (x >= r.left && x < r.right && y >= r.top && y < r.bottom)
            return int(i);
    }
    return -1;
}

gmpi::ReturnCode X11StockDialog::showAsync(gmpi::api::IUnknown* callback)
{
    auto& d = frame_;
    if (!d.display)
        return gmpi::ReturnCode::Fail;

    layout();

    // Centred on the host's window, which is where the user is looking - then
    // clamped to the screen. A dialog is routinely WIDER than the plugin view it
    // is centred on, so the raw result is often negative, and there is no
    // guarantee a window manager will move it back: under a bare X server
    // nothing does, and the message loses its left edge.
    int rootX = 0, rootY = 0;
    Window ignored{};
    XTranslateCoordinates(d.display, d.window, DefaultRootWindow(d.display),
                          (d.width - w_) / 2, (d.height - h_) / 2, &rootX, &rootY, &ignored);

    const int screenW = DisplayWidth(d.display, d.screen);
    const int screenH = DisplayHeight(d.display, d.screen);
    rootX = (std::clamp)(rootX, 0, (std::max)(0, screenW - w_));
    rootY = (std::clamp)(rootY, 0, (std::max)(0, screenH - h_));

    XSetWindowAttributes attr{};
    attr.border_pixel = 0;
    attr.colormap = XCreateColormap(d.display, DefaultRootWindow(d.display), d.visual, AllocNone);
    attr.event_mask = ExposureMask | ButtonPressMask | ButtonReleaseMask
                    | PointerMotionMask | KeyPressMask | StructureNotifyMask;

    window_ = XCreateWindow(d.display, DefaultRootWindow(d.display),
                            rootX, rootY, unsigned(w_), unsigned(h_),
                            0, d.depth, InputOutput, d.visual,
                            CWBorderPixel | CWColormap | CWEventMask, &attr);
    if (!window_)
        return gmpi::ReturnCode::Fail;

    // The whole point of a dialog rather than a bare window: transient-for keeps
    // it above the host and off the task bar. Without it a modal question can
    // end up behind the DAW, which reads to the user as a hang.
    XSetTransientForHint(d.display, window_, d.window);
    XStoreName(d.display, window_, title_.c_str());

    // Let the WM's close button answer as Escape would, rather than killing the
    // connection - which would take the plugin down with it.
    wmDelete_ = XInternAtom(d.display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(d.display, window_, &wmDelete_, 1);

    gc_ = XCreateGC(d.display, window_, 0, nullptr);
    XMapRaised(d.display, window_);

    // Take the keyboard ourselves rather than trusting something else to hand
    // it over. A window manager would focus a transient-for dialog, but nothing
    // guarantees one is running - under a bare X server nothing is - and
    // without focus Escape and Return never arrive, so the only way out of the
    // dialog is the mouse. XSync first: the window must be mapped before it can
    // take focus, or the request is silently dropped with a BadMatch.
    XSync(d.display, False);
    focusIfViewable(d.display, window_);

    callback_ = callback;

    // Stay alive until the user answers, independent of the caller's reference -
    // the same contract as the Wayland dialog.
    addRef();

    present();
    XFlush(d.display);
    return gmpi::ReturnCode::Ok;
}

void X11StockDialog::present()
{
    auto& d = frame_;
    if (!d.display || !window_ || w_ <= 0 || h_ <= 0)
        return;

    gmpi::drawing::api::IBitmapRenderTarget* rtRaw{};
    factory_.createCpuRenderTarget({ uint32_t(w_), uint32_t(h_) }, 0, &rtRaw, 96.0f);
    auto* rt = dynamic_cast<gmpi::cpugfx::RenderTarget*>(rtRaw);
    if (!rt)
    {
        if (rtRaw) rtRaw->release();
        return;
    }

    rt->beginDraw();

    auto brush = [&](gmpi::drawing::Color c)
    {
        gmpi::drawing::api::ISolidColorBrush* b{};
        rt->createSolidColorBrush(&c, nullptr, &b);
        return b;
    };

    const gmpi::drawing::Color bg{ 0.16f, 0.17f, 0.19f, 1.0f };
    rt->clear(&bg);

    auto* ink     = brush({ 0.92f, 0.93f, 0.95f, 1.0f });
    auto* face    = brush({ 0.24f, 0.25f, 0.28f, 1.0f });
    auto* faceHot = brush({ 0.30f, 0.44f, 0.68f, 1.0f });
    auto* edge    = brush({ 0.38f, 0.39f, 0.43f, 1.0f });

    if (font_ && ink)
    {
        const gmpi::drawing::Rect tr{ float(kDialogMargin), float(kDialogMargin),
                                      float(kDialogMargin + kDialogTextWidth),
                                      float(h_ - kDialogMargin - kDialogButtonH - 12) };
        rt->drawTextU(text_.c_str(), uint32_t(text_.size()), font_, &tr, ink, 0);
    }

    for (size_t i = 0; i < buttons_.size(); ++i)
    {
        const auto& b = buttons_[i];
        if (auto* fill = (int(i) == hovered_) ? faceHot : face; fill)
            rt->fillRectangle(&b.rect, fill);
        if (edge)
            rt->drawRectangle(&b.rect, edge, 1.0f, nullptr);

        if (font_ && ink)
        {
            // nudged down so the glyphs sit optically centred in the button
            const gmpi::drawing::Rect lr{ b.rect.left, b.rect.top + 6.f,
                                          b.rect.right, b.rect.bottom };
            rt->drawTextU(b.label.c_str(), uint32_t(b.label.size()), font_, &lr, ink, 0);
        }
    }

    if (ink)     ink->release();
    if (face)    face->release();
    if (faceHot) faceHot->release();
    if (edge)    edge->release();

    rt->endDraw();

    gmpi::drawing::api::IBitmap* bmRaw{};
    rt->getBitmap(&bmRaw);
    if (auto* bm = dynamic_cast<gmpi::cpugfx::Bitmap*>(bmRaw))
    {
        if (!image_)
        {
            auto* data = static_cast<char*>(std::calloc(size_t(w_) * h_, 4));
            if (data)
                image_ = XCreateImage(d.display, d.visual, d.depth, ZPixmap, 0,
                                      data, w_, h_, 32, w_ * 4);
            if (!image_ && data)
                std::free(data);
        }

        if (image_)
        {
            const auto& s = bm->surface;
            const gmpi::cpugfx::SourceSurface src{ s.pixels, s.stridePixels, s.width, s.height };
            const gmpi::cpugfx::DestSurface   dst{ reinterpret_cast<uint8_t*>(image_->data),
                                                   image_->bytes_per_line, w_, h_,
                                                   gmpi::cpugfx::PixelEncoding::Bgra8888 };
            gmpi::cpugfx::encodeDirtyRect(src, dst, gmpi::drawing::RectL{ 0, 0, w_, h_ });
            XPutImage(d.display, window_, gc_, image_, 0, 0, 0, 0, unsigned(w_), unsigned(h_));
        }
    }

    if (bmRaw) bmRaw->release();
    rtRaw->release();

    XFlush(d.display);
}

void X11StockDialog::destroyWindow()
{
    auto& d = frame_;
    std::erase(d.dialogs, this);

    if (!d.display)
        return;

    if (image_)  { XDestroyImage(image_); image_ = {}; }
    if (gc_)     { XFreeGC(d.display, gc_); gc_ = {}; }
    if (window_) { XDestroyWindow(d.display, window_); window_ = {}; }

    XFlush(d.display);
}

void X11StockDialog::finish(gmpi::api::StockDialogButton button)
{
    if (finished_)
        return;
    finished_ = true;

    auto cb = callback_;
    callback_ = {};

    destroyWindow();

    if (auto sink = cb.as<gmpi::api::IStockDialogCallback>(); sink)
        sink->onComplete(button);

    // LAST: drops the reference showAsync took, which may delete us.
    release();
}

bool X11StockDialog::handleEvent(const XEvent& e)
{
    if (!window_ || e.xany.window != window_)
        return false;

    switch (e.type)
    {
    case Expose:
        present();
        return true;

    case MotionNotify:
    {
        const int index = buttonAt(e.xmotion.x, e.xmotion.y);
        if (index != hovered_)
        {
            hovered_ = index;
            present();
        }
        return true;
    }

    case ButtonRelease:
    {
        const int index = buttonAt(e.xbutton.x, e.xbutton.y);
        if (index >= 0)
            finish(buttons_[index].id);
        return true;
    }

    case KeyPress:
    {
        KeySym keysym{};
        char buf[8]{};
        XLookupString(const_cast<XKeyEvent*>(&e.xkey), buf, sizeof(buf) - 1, &keysym, nullptr);

        if (keysym == XK_Escape)
            finish(escape_);
        else if ((keysym == XK_Return || keysym == XK_KP_Enter) && !buttons_.empty())
            finish(buttons_.front().id);   // the first button is the default
        return true;
    }

    case ClientMessage:
        // The WM's close button. Answer as Escape would.
        if (static_cast<Atom>(e.xclient.data.l[0]) == wmDelete_)
            finish(escape_);
        return true;

    default:
        return false;
    }
}

// ---------------------------------------------------------------------------
// IDialogHost on the frame
// ---------------------------------------------------------------------------

void X11DrawingFrame::setMenuFont(gmpi::drawing::api::ITextFormat* font)
{
    impl_->menuFont = font;
}

void X11DrawingFrame::showContextMenu(gmpi::drawing::Point pt)
{
    auto& d = *impl_;
    if (!d.inputClient)
        return;

    gmpi::api::IUnknown* raw{};
    const gmpi::drawing::Rect anchor{ pt.x, pt.y, pt.x + 1.f, pt.y + 1.f };
    if (createPopupMenu(&anchor, &raw) != gmpi::ReturnCode::Ok || !raw)
        return;

    gmpi::shared_ptr<gmpi::api::IUnknown> owner;
    owner.attach(raw);   // createPopupMenu returned a fresh reference

    auto popup = owner.as<gmpi::api::IPopupMenu>();
    if (!popup)
        return;

    if (d.inputClient->populateContextMenu(pt, popup.get()) != gmpi::ReturnCode::Ok)
        return;

    // The menu outlives this call. Hand our reference over rather than letting
    // the shared_ptr destroy it on the way out; dismiss() drops it.
    if (popup->showAsync() != gmpi::ReturnCode::Ok)
        return;

    popup->addRef();
    static_cast<X11PopupMenu*>(popup.get())->takeSelfOwnership();
}

int X11DrawingFrame::portalFd() const
{
    return impl_->portalBus.fd();
}

gmpi::ReturnCode X11DrawingFrame::createColorDialog(gmpi::drawing::Color initialColor,
                                                    gmpi::api::IUnknown** returnDialog)
{
    *returnDialog = {};

    if (!impl_->display)
        return gmpi::ReturnCode::Fail;

    auto* dlg = new X11ColorDialog(*impl_, factory_, impl_->menuFont, initialColor);
    *returnDialog = static_cast<gmpi::api::IColorDialog*>(dlg);
    return gmpi::ReturnCode::Ok;
}

gmpi::ReturnCode X11DrawingFrame::createTextEdit(const gmpi::drawing::Rect* r,
                                                 gmpi::api::IUnknown** returnTextEdit)
{
    *returnTextEdit = {};

    if (!impl_->display)
        return gmpi::ReturnCode::Fail;

    auto* edit = new X11TextEdit(*impl_, impl_->menuFont, r ? *r : gmpi::drawing::Rect{});
    *returnTextEdit = static_cast<gmpi::api::ITextEdit*>(edit);
    return gmpi::ReturnCode::Ok;
}

gmpi::ReturnCode X11DrawingFrame::createKeyListener(const gmpi::drawing::Rect*,
                                                    gmpi::api::IUnknown** returnKeyListener)
{
    *returnKeyListener = {};

    if (!impl_->display)
        return gmpi::ReturnCode::Fail;

    auto* listener = new X11KeyListener(*impl_);
    *returnKeyListener = static_cast<gmpi::api::IKeyListener*>(listener);
    return gmpi::ReturnCode::Ok;
}

gmpi::ReturnCode X11DrawingFrame::createFileDialog(int32_t dialogType,
                                                   gmpi::api::IUnknown** returnDialog)
{
    *returnDialog = {};

    auto& d = *impl_;
    if (!d.display || !d.window)
        return gmpi::ReturnCode::Fail;

    // No session bus means no portal - a bare X session, or a container without
    // one. Say so rather than handing back a dialog that will never answer.
    if (!d.portalBus.connect())
        return gmpi::ReturnCode::NoSupport;

    // The portal's parent_window format for X11. Lower-case hex, no 0x, as
    // written by GTK and Qt; without it the chooser floats unparented and is
    // not modal to the host.
    char parent[32];
    std::snprintf(parent, sizeof parent, "x11:%lx", static_cast<unsigned long>(d.window));

    auto* dlg = new gmpi::portal::PortalFileDialog(d.portalBus, dialogType, parent);
    *returnDialog = static_cast<gmpi::api::IFileDialog*>(dlg);
    return gmpi::ReturnCode::Ok;
}

gmpi::ReturnCode X11DrawingFrame::createStockDialog(int32_t dialogType, const char* title,
                                                    const char* text,
                                                    gmpi::api::IUnknown** returnDialog)
{
    *returnDialog = {};

    if (!impl_->display)
        return gmpi::ReturnCode::Fail;

    auto* dlg = new X11StockDialog(*impl_, factory_, impl_->menuFont, dialogType,
                                   title ? title : "", text ? text : "");
    *returnDialog = static_cast<gmpi::api::IStockDialog*>(dlg);
    return gmpi::ReturnCode::Ok;
}

gmpi::ReturnCode X11DrawingFrame::createPopupMenu(const gmpi::drawing::Rect* r,
                                                  gmpi::api::IUnknown** returnPopupMenu)
{
    *returnPopupMenu = {};

    if (!impl_->display)
        return gmpi::ReturnCode::Fail;

    auto* menu = new X11PopupMenu(*impl_, factory_, impl_->menuFont,
                                  r ? *r : gmpi::drawing::Rect{});
    *returnPopupMenu = static_cast<gmpi::api::IPopupMenu*>(menu);
    return gmpi::ReturnCode::Ok;
}

} // namespace gmpi::hosting
