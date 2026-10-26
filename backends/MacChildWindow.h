#ifndef GMPI_MAC_CHILDWINDOW_H
#define GMPI_MAC_CHILDWINDOW_H

// Single-header gmpi::api::IChildWindow implementation for macOS
// (IDialogHost::createChildWindow): an NSWindow whose content is a GMPI view
// (createNativeView) hosting the caller's client.
//
// Modeless is an ordinary window, centred over the owner's. Modal is a sheet on
// the owner's window - macOS's own way to say "nothing else in this window until
// this is answered".
//
// Every close - close(), the title bar, the owner going - runs one path,
// finishClose(). close() reaches it asynchronously, so a client may call it from
// its own button without the view being torn down under its click.
//
// Manual retain/release, like the rest of the backend.

#import <Cocoa/Cocoa.h>
#include <algorithm>
#include <string>
#include <vector>
#include "GmpiSdkCommon.h"
#include "helpers/NativeUi.h"
#include "GmpiObjCNames.h"

// DrawingFrameMac.mm
void* createNativeView(void* parent, class IUnknown* parameterHost, class IUnknown* client, int width, int height);
void  resizeNativeView(void* view, int width, int height);
void  gmpi_onCloseNativeView(void* view);

class GMPI_MAC_ChildWindow;

#define GMPI_CHILD_WINDOW_DELEGATE GMPI_OBJC_NAME(GMPI_CHILD_WINDOW_DELEGATE_01)
@interface GMPI_CHILD_WINDOW_DELEGATE : NSObject <NSWindowDelegate>
{
@public
    GMPI_MAC_ChildWindow* owner;
}
@end

class GMPI_MAC_ChildWindow : public gmpi::api::IChildWindow
{
public:
    // The windows one frame has open, so it can close them when its view goes.
    using Registry = std::vector<GMPI_MAC_ChildWindow*>;

    GMPI_MAC_ChildWindow(NSView* ownerView, Registry& registry, const char* title,
                         int32_t width, int32_t height, int32_t flags, gmpi::api::IUnknown* client)
        : ownerView_(ownerView)
        , registry_(registry)
        , title_(title ? title : "")
        , width_(std::max(1, width))
        , height_(std::max(1, height))
        , flags_(flags)
    {
        client_ = client; // assignment addRefs; the raw-pointer constructor would adopt the caller's reference
    }

    gmpi::ReturnCode showAsync(gmpi::api::IUnknown* callback) override
    {
        if (window_ || shown_ || !client_)
            return gmpi::ReturnCode::Fail;

        window_ = [[NSWindow alloc]
            initWithContentRect:NSMakeRect(0, 0, width_, height_)
                      styleMask:(NSWindowStyleMaskTitled
                               | NSWindowStyleMaskClosable
                               | NSWindowStyleMaskMiniaturizable
                               | NSWindowStyleMaskResizable)
                        backing:NSBackingStoreBuffered
                          defer:NO];
        if (!window_)
            return gmpi::ReturnCode::Fail;

        [window_ setTitle:[NSString stringWithUTF8String:title_.c_str()]];
        [window_ setReleasedWhenClosed:NO]; // released by finishClose()

        auto* delegate = [[GMPI_CHILD_WINDOW_DELEGATE alloc] init];
        delegate->owner = this;
        delegate_ = delegate;
        [window_ setDelegate:delegate];

        view_ = (NSView*)createNativeView((void*)[window_ contentView], nullptr,
                                          (class IUnknown*)client_.get(), width_, height_);
        if (!view_)
        {
            releaseWindow();
            return gmpi::ReturnCode::Fail;
        }
        [view_ setAutoresizingMask:NSViewNotSizable]; // windowDidResize sizes it, as the standalone does

        shown_ = true;
        registry_.push_back(this);
        addRef(); // alive while open; finishClose() releases it

        if (callback)
            callback->queryInterface(&gmpi::api::IChildWindowCallback::guid, callback_.put_void());

        NSWindow* parent = ownerView_ ? [ownerView_ window] : nil;
        if (modal() && parent)
        {
            [parent beginSheet:window_ completionHandler:nil];
        }
        else
        {
            if (parent)
            {
                const NSRect owner = [parent frame];
                NSRect frame = [window_ frame];
                frame.origin.x = NSMidX(owner) - frame.size.width / 2;
                frame.origin.y = NSMidY(owner) - frame.size.height / 2;
                [window_ setFrameOrigin:frame.origin];
            }
            else
            {
                [window_ center];
            }
            [window_ makeKeyAndOrderFront:nil];
        }
        [window_ makeFirstResponder:view_];

        return gmpi::ReturnCode::Ok;
    }

    gmpi::ReturnCode close() override
    {
        if (!window_ || closing_)
            return gmpi::ReturnCode::Ok;
        closing_ = true;

        // The block holds a reference of its own: if the owner closes us first
        // (finishClose() from ownerClosing()), this still finds a live object
        // with nothing left to do.
        addRef();
        dispatch_async(dispatch_get_main_queue(), ^{
            finishClose();
            release();
        });
        return gmpi::ReturnCode::Ok;
    }

    gmpi::ReturnCode activate() override
    {
        if (!window_)
            return gmpi::ReturnCode::Fail;
        if ([window_ isMiniaturized])
            [window_ deminiaturize:nil];
        [window_ makeKeyAndOrderFront:nil];
        return gmpi::ReturnCode::Ok;
    }

    // The owner's view is going: close now, while it still exists.
    void ownerClosing() { finishClose(); }

    // From the delegate.
    void onResized()
    {
        if (!window_ || !view_)
            return;
        const NSSize size = [[window_ contentView] bounds].size;
        resizeNativeView((void*)view_, static_cast<int>(size.width), static_cast<int>(size.height));
        [view_ setNeedsDisplay:YES];
    }

    gmpi::ReturnCode queryInterface(const gmpi::api::Guid* iid, void** returnInterface) override
    {
        *returnInterface = {};
        GMPI_QUERYINTERFACE(gmpi::api::IChildWindow);
        return gmpi::ReturnCode::NoSupport;
    }
    GMPI_REFCOUNT;

private:
    bool modal() const { return (flags_ & static_cast<int32_t>(gmpi::api::ChildWindowFlags::Modal)) != 0; }

    void finishClose()
    {
        if (!window_)
            return;

        registry_.erase(std::remove(registry_.begin(), registry_.end(), this), registry_.end());

        if (view_)
        {
            // Stops the view's timer and lets go of the client while the view is
            // still in its window. Not released: see ToplevelWindowMac::close.
            gmpi_onCloseNativeView((void*)view_);
            view_ = nil;
        }

        if (NSWindow* sheetParent = [window_ sheetParent])
            [sheetParent endSheet:window_];

        releaseWindow();

        if (auto callback = std::move(callback_))
            callback->onClosed();

        release(); // the showAsync reference; may delete this
    }

    void releaseWindow()
    {
        if (window_)
        {
            [window_ setDelegate:nil]; // before the close, so it cannot re-enter
            [window_ orderOut:nil];
            [window_ close];
            [window_ release];
            window_ = nil;
        }
        if (delegate_)
        {
            [delegate_ release];
            delegate_ = nil;
        }
    }

    NSView* ownerView_{};
    Registry& registry_;
    std::string title_;
    int width_{};
    int height_{};
    int32_t flags_{};
    gmpi::shared_ptr<gmpi::api::IUnknown> client_;
    gmpi::shared_ptr<gmpi::api::IChildWindowCallback> callback_;
    NSWindow* window_{};
    id delegate_{};
    NSView* view_{};
    bool shown_{};
    bool closing_{};
};

@implementation GMPI_CHILD_WINDOW_DELEGATE

// The title bar's close goes the same way as close(); the window is closed by us.
- (BOOL)windowShouldClose:(NSWindow*)sender
{
    (void)sender;
    if (owner)
        owner->close();
    return NO;
}

- (void)windowDidResize:(NSNotification*)notification
{
    (void)notification;
    if (owner)
        owner->onResized();
}

- (void)windowDidChangeBackingProperties:(NSNotification*)notification
{
    (void)notification;
    if (owner)
        owner->onResized();
}

@end

#endif // GMPI_MAC_CHILDWINDOW_H
