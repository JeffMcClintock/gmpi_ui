// UIKit view host for GMPI editors - the iOS sibling of DrawingFrameMac.mm.
//
// Drawing reuses the identical CoreGraphics backend (CocoaGfx.h - already
// AppKit-free when TARGET_OS_OSX is not set) and the identical linear-colorspace
// backing-bitmap pipeline, so a plugin renders pixel-for-pixel the same as on
// macOS. The differences live at the edges:
//
//  * input is touches, delivered as GMPI pointer events (first touch = left
//    button drag, matching how plugin editors already interpret a mouse);
//  * UIKit's CGContext is already top-down, so the blit needs one extra flip
//    where AppKit's non-flipped view needed none;
//  * dialogs are UIAlertController where macOS uses NSPopUpButton/NSAlert.
//    File, colour and key-listener dialogs are declined (NoSupport) for now -
//    they have no sensible presentation inside an AUv3 extension's sandbox.

#import <UIKit/UIKit.h>
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>
#include "GmpiSdkCommon.h"
#include "GmpiApiEditor.h"
#import "CocoaGfx.h"
#include "DrawingFrameCommon.h"
#include "DrawingFrameIos.h"
#import "helpers/IController.h"

// Same product decision as DrawingFrameMac.mm, same numbers: bound what an
// editor may reserve, preserving aspect ratio when the byte budget bites.
// iPads top out at 1366 logical points, so the 8192 axis is generous.
namespace {
constexpr double maxEditorDimensionPoints = 8192.0;
constexpr size_t maxBackingBitmapBytes = 384u * 1024u * 1024u;
constexpr size_t backingBitmapBytesPerPixel = 8; // 16-bit x 4 components
constexpr double assumedBackingScale = 2.0;

void clampEditorExtent(double& width, double& height, double backingScale, double maxAxis)
{
    width  = std::max(1.0, std::min(width,  maxAxis));
    height = std::max(1.0, std::min(height, maxAxis));

    const double maxPixels = static_cast<double>(maxBackingBitmapBytes) / backingBitmapBytesPerPixel;
    const double pixels = (width * backingScale) * (height * backingScale);

    if (pixels > maxPixels)
    {
        const double shrink = std::sqrt(maxPixels / pixels);
        width  = std::max(1.0, std::floor(width  * shrink));
        height = std::max(1.0, std::floor(height * shrink));
    }
}

// The view controller nearest the view, walked via the responder chain.
// UIAlertController can only be SHOWN from a view controller, and inside an
// AUv3 extension that is the AUViewController hosting this view.
UIViewController* nearestViewController(UIView* view)
{
    for (UIResponder* r = view; r; r = r.nextResponder)
    {
        if (auto vc = [r isKindOfClass:[UIViewController class]] ? (UIViewController*)r : nil; vc)
            return vc;
    }
    return nil;
}
} // namespace

// ---------------------------------------------------------------------------
// UIAlertController-backed dialogs.
// All deferred-notify (onComplete once, on dismissal), matching the Mac and
// Windows implementations, and all presented from the hosting view controller.
// ---------------------------------------------------------------------------

class GMPI_IOS_PopupMenu : public gmpi::api::IPopupMenu
{
    struct MenuItem
    {
        std::string text;
        int32_t id{};
        int32_t flags{};
        gmpi::shared_ptr<gmpi::api::IUnknown> callback;
    };

    UIView* view;
    gmpi::drawing::Rect rect;
    std::vector<MenuItem> items;

public:
    GMPI_IOS_PopupMenu(UIView* pview, gmpi::drawing::Rect prect) : view(pview), rect(prect) {}

    gmpi::ReturnCode addItem(const char* text, int32_t id, int32_t flags, gmpi::api::IUnknown* itemCallback) override
    {
        // Sub-menus flatten: an action sheet has no nesting, so the begin/end
        // markers are dropped and their children appear inline. Separators have
        // no UIKit equivalent and are dropped likewise.
        constexpr int32_t structural =
              static_cast<int32_t>(gmpi::api::PopupMenuFlags::SubMenuBegin)
            | static_cast<int32_t>(gmpi::api::PopupMenuFlags::SubMenuEnd)
            | static_cast<int32_t>(gmpi::api::PopupMenuFlags::Separator)
            | static_cast<int32_t>(gmpi::api::PopupMenuFlags::Break);
        if (flags & structural)
            return gmpi::ReturnCode::Ok;

        MenuItem item;
        item.text = text ? text : "";
        item.id = id;
        item.flags = flags;
        item.callback = itemCallback;
        items.push_back(std::move(item));
        return gmpi::ReturnCode::Ok;
    }

    gmpi::ReturnCode setAlignment(int32_t alignment) override { return gmpi::ReturnCode::Ok; }

    gmpi::ReturnCode showAsync() override
    {
        UIViewController* host = nearestViewController(view);
        if (!host)
            return gmpi::ReturnCode::Fail;

        UIAlertController* sheet = [UIAlertController
            alertControllerWithTitle:nil
                             message:nil
                      preferredStyle:UIAlertControllerStyleActionSheet];

        for (auto& item : items)
        {
            // Block copies of the POD fields plus a retained interface pointer;
            // `this` may be released before the sheet is dismissed.
            auto cb = item.callback;
            const auto localId = item.id;

            // A tick is shown by decorating the title: UIAlertAction has no
            // public checkmark, and KVC-poking its private one is an App Store
            // rejection waiting to happen.
            std::string title = item.text;
            if (item.flags & static_cast<int32_t>(gmpi::api::PopupMenuFlags::Ticked))
                title = "\xE2\x9C\x93 " + title; // "✓ "

            UIAlertAction* action = [UIAlertAction
                actionWithTitle:[NSString stringWithUTF8String:title.c_str()]
                          style:UIAlertActionStyleDefault
                        handler:^(UIAlertAction*) {
                            auto cb2 = cb; // block captures are const; as() is not
                            if (auto typed = cb2.as<gmpi::api::IPopupMenuCallback>(); typed)
                                typed->onComplete(gmpi::ReturnCode::Ok, localId);
                        }];

            if (item.flags & static_cast<int32_t>(gmpi::api::PopupMenuFlags::Grayed))
                action.enabled = NO;

            [sheet addAction:action];
        }

        [sheet addAction:[UIAlertAction actionWithTitle:@"Cancel"
                                                  style:UIAlertActionStyleCancel
                                                handler:nil]];

        // iPad presents action sheets as popovers and requires an anchor.
        if (auto pop = sheet.popoverPresentationController; pop)
        {
            pop.sourceView = view;
            pop.sourceRect = CGRectMake(rect.left, rect.top,
                std::max(1.0f, rect.right - rect.left), std::max(1.0f, rect.bottom - rect.top));
        }

        [host presentViewController:sheet animated:YES completion:nil];
        return gmpi::ReturnCode::Ok;
    }

    gmpi::ReturnCode queryInterface(const gmpi::api::Guid* iid, void** returnInterface) override
    {
        *returnInterface = {};
        GMPI_QUERYINTERFACE(gmpi::api::IPopupMenu);
        GMPI_QUERYINTERFACE(gmpi::api::IContextItemSink);
        return gmpi::ReturnCode::NoSupport;
    }
    GMPI_REFCOUNT;
};

class GMPI_IOS_TextEdit : public gmpi::api::ITextEdit
{
    UIView* view;
    gmpi::drawing::Rect rect;
    std::string text;

public:
    GMPI_IOS_TextEdit(UIView* pview, gmpi::drawing::Rect prect) : view(pview), rect(prect) {}

    gmpi::ReturnCode setText(const char* ptext) override
    {
        text = ptext ? ptext : "";
        return gmpi::ReturnCode::Ok;
    }
    gmpi::ReturnCode setAlignment(int32_t) override { return gmpi::ReturnCode::Ok; }
    gmpi::ReturnCode setTextSize(float) override { return gmpi::ReturnCode::Ok; }

    gmpi::ReturnCode showAsync(gmpi::api::IUnknown* callback) override
    {
        UIViewController* host = nearestViewController(view);
        if (!host)
            return gmpi::ReturnCode::Fail;

        gmpi::shared_ptr<gmpi::api::IUnknown> cb(callback);

        UIAlertController* alert = [UIAlertController
            alertControllerWithTitle:nil
                             message:nil
                      preferredStyle:UIAlertControllerStyleAlert];

        NSString* initial = [NSString stringWithUTF8String:text.c_str()];
        [alert addTextFieldWithConfigurationHandler:^(UITextField* field) {
            field.text = initial;
            field.clearButtonMode = UITextFieldViewModeWhileEditing;
        }];

        [alert addAction:[UIAlertAction actionWithTitle:@"OK"
                                                  style:UIAlertActionStyleDefault
                                                handler:^(UIAlertAction*) {
            UITextField* field = alert.textFields.firstObject;
            auto cb2 = cb; // block captures are const; as() is not
            if (auto typed = cb2.as<gmpi::api::ITextEditCallback>(); typed)
            {
                typed->onChanged([field.text UTF8String]);
                typed->onComplete(gmpi::ReturnCode::Ok);
            }
        }]];

        [alert addAction:[UIAlertAction actionWithTitle:@"Cancel"
                                                  style:UIAlertActionStyleCancel
                                                handler:^(UIAlertAction*) {
            auto cb2 = cb; // block captures are const; as() is not
            if (auto typed = cb2.as<gmpi::api::ITextEditCallback>(); typed)
                typed->onComplete(gmpi::ReturnCode::Cancel);
        }]];

        [host presentViewController:alert animated:YES completion:nil];
        return gmpi::ReturnCode::Ok;
    }

    GMPI_QUERYINTERFACE_METHOD(gmpi::api::ITextEdit);
    GMPI_REFCOUNT;
};

class GMPI_IOS_StockDialog : public gmpi::api::IStockDialog
{
    UIView* view;
    gmpi::api::StockDialogType dialogType;
    std::string title;
    std::string text;

public:
    GMPI_IOS_StockDialog(UIView* pview, gmpi::api::StockDialogType type, const char* ptitle, const char* ptext)
        : view(pview), dialogType(type), title(ptitle ? ptitle : ""), text(ptext ? ptext : "") {}

    gmpi::ReturnCode showAsync(gmpi::api::IUnknown* callback) override
    {
        UIViewController* host = nearestViewController(view);
        if (!host)
            return gmpi::ReturnCode::Fail;

        gmpi::shared_ptr<gmpi::api::IUnknown> cb(callback);

        UIAlertController* alert = [UIAlertController
            alertControllerWithTitle:[NSString stringWithUTF8String:title.c_str()]
                             message:[NSString stringWithUTF8String:text.c_str()]
                      preferredStyle:UIAlertControllerStyleAlert];

        auto addButton = [&](NSString* label, UIAlertActionStyle style, gmpi::api::StockDialogButton result) {
            [alert addAction:[UIAlertAction actionWithTitle:label
                                                      style:style
                                                    handler:^(UIAlertAction*) {
                auto cb2 = cb; // block captures are const; as() is not
                if (auto typed = cb2.as<gmpi::api::IStockDialogCallback>(); typed)
                    typed->onComplete(result);
            }]];
        };

        using gmpi::api::StockDialogButton;
        using gmpi::api::StockDialogType;
        switch (dialogType)
        {
        case StockDialogType::Ok:
            addButton(@"OK", UIAlertActionStyleDefault, StockDialogButton::Ok);
            break;
        case StockDialogType::OkCancel:
            addButton(@"OK", UIAlertActionStyleDefault, StockDialogButton::Ok);
            addButton(@"Cancel", UIAlertActionStyleCancel, StockDialogButton::Cancel);
            break;
        case StockDialogType::YesNo:
            addButton(@"Yes", UIAlertActionStyleDefault, StockDialogButton::Yes);
            addButton(@"No", UIAlertActionStyleDefault, StockDialogButton::No);
            break;
        case StockDialogType::YesNoCancel:
            addButton(@"Yes", UIAlertActionStyleDefault, StockDialogButton::Yes);
            addButton(@"No", UIAlertActionStyleDefault, StockDialogButton::No);
            addButton(@"Cancel", UIAlertActionStyleCancel, StockDialogButton::Cancel);
            break;
        }

        [host presentViewController:alert animated:YES completion:nil];
        return gmpi::ReturnCode::Ok;
    }

    GMPI_QUERYINTERFACE_METHOD(gmpi::api::IStockDialog);
    GMPI_REFCOUNT;
};

// ---------------------------------------------------------------------------
// The drawing frame: IDrawingHost / IInputHost / IDialogHost for one UIView.
// ---------------------------------------------------------------------------

class DrawingFrameIos :
    public DrawingFrameCommon,
    public gmpi::api::IDrawingHost,
    public gmpi::api::IInputHost,
    public gmpi::api::IDialogHost
{
public:
    int32_t mouseCaptured = 0;

    gmpi::shared_ptr<gmpi::api::IEditor> pluginParameters_GMPI;

    gmpi::cocoa::Factory drawingFactory;
    UIView* view{};
    CGContextRef backBuffer{}; // linear colorspace for correct blending, like the Mac frame.
    CGFloat backBufferHeight{};

    void Init(gmpi::api::IUnknown* paramHost, gmpi::api::IUnknown* pclient)
    {
        parameterHost = paramHost;

        pclient->queryInterface(&gmpi::api::IDrawingClient::guid, drawingClient.put_void());
        pclient->queryInterface(&gmpi::api::IInputClient::guid, inputClient.put_void());
        pclient->queryInterface(&gmpi::api::IEditor::guid, pluginParameters_GMPI.put_void());

        if (pluginParameters_GMPI)
        {
            pluginParameters_GMPI->setHost(static_cast<gmpi::api::IDrawingHost*>(this));
            pluginParameters_GMPI->initialize();
        }

        if (drawingClient)
            drawingClient->setHost(static_cast<gmpi::api::IDrawingHost*>(this));
    }

    void open() // called from didMoveToWindow <= createNativeView()
    {
        if (drawingClient)
        {
            drawingClient->setHost(static_cast<gmpi::api::IDrawingHost*>(this));

            const auto r = [view bounds];

            const gmpi::drawing::Size available{
                static_cast<float>(r.size.width),
                static_cast<float>(r.size.height)
            };

            gmpi::drawing::Size desired{};
            drawingClient->measure(&available, &desired);
            gmpi::drawing::Rect finalRect{ 0, 0, available.width, available.height };
            drawingClient->arrange(&finalRect);
        }
    }

    void DeInit()
    {
        if (pluginParameters_GMPI)
        {
            auto controller = dynamic_cast<gmpi::hosting::IController*>(parameterHost);
            if (controller)
                controller->unRegisterGui(pluginParameters_GMPI.get());
        }

        drawingClient = {};
        inputClient = {};
        pluginParameters_GMPI = {};
    }

    void onRender(UIView* frame, gmpi::drawing::Rect* dirtyRect)
    {
        if (!backBuffer)
        {
            initBackingBitmap();

            const CGSize logicalsize = view.bounds.size;
            gmpi::drawing::Rect finalRect{ 0, 0, (float)logicalsize.width, (float)logicalsize.height };
            if (drawingClient)
                drawingClient->arrange(&finalRect);
        }

        if (!backBuffer)
            return; // bitmap creation failed, nothing to draw

        CGContextSaveGState(backBuffer);

        // Flip the bitmap context to match Direct2D (top-down), exactly as the
        // Mac frame does - the plugin draws in the same space on every platform.
        CGContextTranslateCTM(backBuffer, 0, backBufferHeight);
        CGContextScaleCTM(backBuffer, 1, -1);

        // Scale physical (pixel) to logical (point) coordinates.
        const CGFloat dpiScale = view.contentScaleFactor > 0 ? view.contentScaleFactor : assumedBackingScale;
        if (dpiScale != 1.0)
        {
            CGContextScaleCTM(backBuffer, dpiScale, dpiScale);
        }

        // context must be disposed before restoring state, because its destructor also restores state
        {
            gmpi::cocoa::GraphicsContext context(&drawingFactory);
            context.setCGContext(backBuffer);

            const auto r = [frame bounds];
            const gmpi::drawing::Rect bounds{
                (float)r.origin.x,
                (float)r.origin.y,
                (float)(r.origin.x + r.size.width),
                (float)(r.origin.y + r.size.height)
            };

            const gmpi::drawing::Rect dirtyClipped = intersectRect(bounds, *dirtyRect);

            context.pushAxisAlignedClip(&dirtyClipped);

            if (drawingClient)
                drawingClient->render(static_cast<gmpi::drawing::api::IDeviceContext*>(&context));

            // render() is re-entrant: a client that resizes its own view from inside
            // it reaches onResize(), which released the bitmap this context still
            // points at. Same guard, same reasoning, as DrawingFrameMac.mm.
            if (!backBuffer)
                return;

            context.popAxisAlignedClip();
        }

        CGContextRestoreGState(backBuffer);

        // Blit the back buffer onto the screen. UIKit's context is top-down and
        // CGContextDrawImage speaks bottom-up, so flip around the view height -
        // the one transform AppKit's non-flipped NSView did not need.
        CGImageRef backImage = CGBitmapContextCreateImage(backBuffer);
        if (backImage)
        {
            CGContextRef screenCtx = UIGraphicsGetCurrentContext();
            const auto b = [frame bounds];
            CGContextSaveGState(screenCtx);
            CGContextTranslateCTM(screenCtx, 0, b.size.height);
            CGContextScaleCTM(screenCtx, 1, -1);
            CGContextDrawImage(screenCtx, b, backImage);
            CGContextRestoreGState(screenCtx);
            CGImageRelease(backImage);
        }
    }

    // IDrawingHost
    void invalidateRect(const gmpi::drawing::Rect* rect) override
    {
        // UIKit is already top-down: the GMPI rect needs no flip.
        if (rect)
        {
            [view setNeedsDisplayInRect:
                CGRectMake(rect->left, rect->top, rect->right - rect->left, rect->bottom - rect->top)];
        }
        else
        {
            [view setNeedsDisplay];
        }
    }
    void invalidateMeasure() override {}

    gmpi::ReturnCode getDrawingFactory(gmpi::api::IUnknown** returnFactory) override
    {
        *returnFactory = &drawingFactory;
        return gmpi::ReturnCode::Ok;
    }

    float getRasterizationScale() override
    {
        return view.contentScaleFactor > 0 ? view.contentScaleFactor : assumedBackingScale;
    }

    // IInputHost
    gmpi::ReturnCode setCapture() override
    {
        // Touch delivery is implicitly captured by UIKit for the life of the
        // touch, which is the only gesture there is; record the state so
        // getCapture answers honestly.
        mouseCaptured = 1;
        return gmpi::ReturnCode::Ok;
    }
    gmpi::ReturnCode getCapture(bool& returnValue) override
    {
        returnValue = mouseCaptured;
        return gmpi::ReturnCode::Ok;
    }
    gmpi::ReturnCode releaseCapture() override
    {
        mouseCaptured = 0;
        return gmpi::ReturnCode::Ok;
    }

    // IDialogHost
    gmpi::ReturnCode createTextEdit(const gmpi::drawing::Rect* r, gmpi::api::IUnknown** returnTextEdit) override
    {
        auto textEdit = new GMPI_IOS_TextEdit(view, *r);
        *returnTextEdit = static_cast<gmpi::api::ITextEdit*>(textEdit);
        return gmpi::ReturnCode::Ok;
    }
    gmpi::ReturnCode createPopupMenu(const gmpi::drawing::Rect* r, gmpi::api::IUnknown** returnMenu) override
    {
        contextMenu.attach(new GMPI_IOS_PopupMenu(view, *r));
        contextMenu->addRef();
        *returnMenu = contextMenu.get();
        return gmpi::ReturnCode::Ok;
    }
    gmpi::ReturnCode createKeyListener(const gmpi::drawing::Rect* r, gmpi::api::IUnknown** returnKeyListener) override
    {
        // No hardware-keyboard focus model to hang this on inside an extension.
        *returnKeyListener = {};
        return gmpi::ReturnCode::NoSupport;
    }
    gmpi::ReturnCode createFileDialog(int32_t dialogType, gmpi::api::IUnknown** returnDialog) override
    {
        // UIDocumentPickerViewController needs entitlements and app-group
        // thinking that belong to the containing app, not this frame. Declined
        // rather than half-done.
        *returnDialog = {};
        return gmpi::ReturnCode::NoSupport;
    }
    gmpi::ReturnCode createStockDialog(int32_t dialogType, const char* title, const char* text, gmpi::api::IUnknown** returnDialog) override
    {
        *returnDialog = static_cast<gmpi::api::IStockDialog*>(
            new GMPI_IOS_StockDialog(view, static_cast<gmpi::api::StockDialogType>(dialogType), title, text));
        return gmpi::ReturnCode::Ok;
    }
    gmpi::ReturnCode createColorDialog(gmpi::drawing::Color initialColor, gmpi::api::IUnknown** returnDialog) override
    {
        // UIColorPickerViewController exists, but its deferred-dismissal flow
        // needs a retained delegate object; not written yet.
        *returnDialog = {};
        return gmpi::ReturnCode::NoSupport;
    }

    // IUnknown
    gmpi::ReturnCode queryInterface(const gmpi::api::Guid* iid, void** returnInterface) override
    {
        *returnInterface = {};

        GMPI_QUERYINTERFACE(gmpi::api::IDrawingHost);
        GMPI_QUERYINTERFACE(gmpi::api::IInputHost);
        GMPI_QUERYINTERFACE(gmpi::api::IDialogHost);

        if (parameterHost)
            return parameterHost->queryInterface(iid, returnInterface);

        return gmpi::ReturnCode::NoSupport;
    }

    void initBackingBitmap()
    {
        const CGFloat scale = view.contentScaleFactor > 0 ? view.contentScaleFactor : assumedBackingScale;
        CGSize physicalsize = view.bounds.size;
        physicalsize.width *= scale;
        physicalsize.height *= scale;

        {
            double pw = physicalsize.width;
            double ph = physicalsize.height;

            clampEditorExtent(pw, ph, 1.0, maxEditorDimensionPoints * std::max<double>(1.0, scale));

            physicalsize.width = pw;
            physicalsize.height = ph;
        }

        if (physicalsize.width < 1.0 || physicalsize.height < 1.0)
            return; // zero-sized view; onRender copes with a missing bitmap.

        CGColorSpaceRef colorSpace = CGColorSpaceCreateWithName(kCGColorSpaceLinearSRGB);

        // 16-bit integer per component in linear space, or 32-bit float fallback -
        // the same two attempts, in the same order, as the Mac frame.
        backBuffer = CGBitmapContextCreate(NULL,
            (size_t)physicalsize.width, (size_t)physicalsize.height,
            16, 0, colorSpace,
            (CGBitmapInfo)kCGImageAlphaPremultipliedLast | (CGBitmapInfo)kCGBitmapByteOrder16Big);

        if (!backBuffer)
        {
            backBuffer = CGBitmapContextCreate(NULL,
                (size_t)physicalsize.width, (size_t)physicalsize.height,
                32, 0, colorSpace,
                (CGBitmapInfo)kCGImageAlphaPremultipliedLast | (CGBitmapInfo)kCGBitmapFloatComponents | (CGBitmapInfo)kCGBitmapByteOrder32Host);
        }

        backBufferHeight = physicalsize.height;

        CGColorSpaceRelease(colorSpace);
    }

    void onResize()
    {
        if (backBuffer)
            CGContextRelease(backBuffer);
        backBuffer = nullptr;
    }

    GMPI_REFCOUNT_NO_DELETE;
};

// Objective-C can't handle loading the same class into different plugins; give
// each iteration of this class a unique name (same convention as the Mac view).
#define GMPI_IOS_VIEW_CLASS GMPI_IOS_VIEW_VERSION_01

@interface GMPI_IOS_VIEW_CLASS : UIView {
    DrawingFrameIos drawingFrame;
}

- (id)initWithClient:(class IUnknown*)_client parameterHost:(class IUnknown*)paramHost preferredSize:(CGSize)size;
- (void)onClose;

@end

@implementation GMPI_IOS_VIEW_CLASS

- (id)initWithClient:(class IUnknown*)_client parameterHost:(class IUnknown*)paramHost preferredSize:(CGSize)size
{
    self = [super initWithFrame:CGRectMake(0, 0, size.width, size.height)];
    if (self)
    {
        // Redraw whole-view on resize; the backing bitmap is reallocated anyway.
        self.contentMode = UIViewContentModeRedraw;
        self.multipleTouchEnabled = NO; // one pointer, like a mouse
        self.opaque = YES;

        drawingFrame.view = self;
        drawingFrame.Init((gmpi::api::IUnknown*)paramHost, (gmpi::api::IUnknown*)_client);
    }
    return self;
}

- (void)didMoveToWindow
{
    [super didMoveToWindow];

    if (self.window)
    {
        // The window's screen finally pins down the real backing scale.
        self.contentScaleFactor = self.window.screen.scale;
        drawingFrame.open();
    }
}

- (void)removeFromSuperview
{
    [super removeFromSuperview];

    // Editor is closing.
    [self onClose];
}

- (void)onClose
{
    drawingFrame.DeInit();
    drawingFrame.view = nil;
}

- (void)drawRect:(CGRect)dirtyRect
{
    // UIKit is top-down already: the dirty rect maps straight through.
    gmpi::drawing::Rect r{
        static_cast<float>(dirtyRect.origin.x),
        static_cast<float>(dirtyRect.origin.y),
        static_cast<float>(dirtyRect.origin.x + dirtyRect.size.width),
        static_cast<float>(dirtyRect.origin.y + dirtyRect.size.height)
    };
    drawingFrame.onRender(self, &r);
}

- (void)setFrame:(CGRect)newSize
{
    [super setFrame:newSize];

    drawingFrame.onResize();
}

// Touches: the first (only) touch is delivered as the primary "button", which
// is how every GMPI editor already interprets a mouse drag.
- (void)touchesBegan:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event
{
    UITouch* touch = [touches anyObject];
    const CGPoint p = [touch locationInView:self];

    int32_t flags = static_cast<int32_t>(gmpi::api::PointerFlags::InContact)
        | static_cast<int32_t>(gmpi::api::PointerFlags::Primary)
        | static_cast<int32_t>(gmpi::api::PointerFlags::Confidence)
        | static_cast<int32_t>(gmpi::api::PointerFlags::New)
        | static_cast<int32_t>(gmpi::api::PointerFlags::FirstButton);

    if (touch.tapCount == 2)
        flags |= static_cast<int32_t>(gmpi::api::PointerFlags::Double);

    if (drawingFrame.inputClient)
        drawingFrame.inputClient->onPointerDown({ (float)p.x, (float)p.y }, flags);
}

- (void)touchesMoved:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event
{
    UITouch* touch = [touches anyObject];
    const CGPoint p = [touch locationInView:self];

    int32_t flags = static_cast<int32_t>(gmpi::api::PointerFlags::InContact)
        | static_cast<int32_t>(gmpi::api::PointerFlags::Primary)
        | static_cast<int32_t>(gmpi::api::PointerFlags::Confidence)
        | static_cast<int32_t>(gmpi::api::PointerFlags::FirstButton);

    if (drawingFrame.inputClient)
        drawingFrame.inputClient->onPointerMove({ (float)p.x, (float)p.y }, flags);
}

- (void)touchesEnded:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event
{
    UITouch* touch = [touches anyObject];
    const CGPoint p = [touch locationInView:self];

    int32_t flags = static_cast<int32_t>(gmpi::api::PointerFlags::InContact)
        | static_cast<int32_t>(gmpi::api::PointerFlags::Primary)
        | static_cast<int32_t>(gmpi::api::PointerFlags::Confidence)
        | static_cast<int32_t>(gmpi::api::PointerFlags::FirstButton);

    if (drawingFrame.inputClient)
        drawingFrame.inputClient->onPointerUp({ (float)p.x, (float)p.y }, flags);
}

- (void)touchesCancelled:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event
{
    // Deliver as an ordinary up: GMPI has no cancel, and a stuck "button" is
    // the failure mode to avoid.
    [self touchesEnded:touches withEvent:event];
}

@end

// The same C boundary as the Mac file, so per-platform wrapper code differs
// only in what it casts the void* to.
void* createNativeView(void* parent, class IUnknown* paramHost, class IUnknown* client, int width, int height)
{
    const CGSize inPreferredSize{ (CGFloat)width, (CGFloat)height };

    GMPI_IOS_VIEW_CLASS* native = [[GMPI_IOS_VIEW_CLASS alloc] initWithClient:client parameterHost:paramHost preferredSize:inPreferredSize];

    if (parent)
    {
        UIView* parentView = (UIView*)parent;
        [parentView addSubview:native];
    }

    return (void*)native;
}

void gmpi_onCloseNativeView(void* ptr)
{
    auto view = (GMPI_IOS_VIEW_CLASS*)ptr;
    [view onClose];
}

void resizeNativeView(void* ptr, int width, int height)
{
    auto view = (UIView*)ptr;

    double w = width;
    double h = height;
    const double scale = view.window ? (double)view.window.screen.scale : assumedBackingScale;

    clampEditorExtent(w, h, std::max(1.0, scale), maxEditorDimensionPoints);

    auto r = view.frame;
    r.size.width = w;
    r.size.height = h;
    view.frame = r;
}
