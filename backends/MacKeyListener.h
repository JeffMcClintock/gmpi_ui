#ifndef GMPI_MAC_KEYLISTENER_H
#define GMPI_MAC_KEYLISTENER_H

// Single-header gmpi::api::IKeyListener implementation for macOS.
// Define GMPI_MAC_KEYLISTENER_IMPLEMENTATION in exactly one .mm per binary
// before including, to emit the Obj-C @implementation block.

#import <Cocoa/Cocoa.h>
#include "GmpiSdkCommon.h"
#include "helpers/NativeUi.h"

@interface GMPI_KeyListenerView : NSView
{
    gmpi::api::IKeyListenerCallback* keyCallback;
}
- (id)initWithParent:(NSView*)parent callback:(gmpi::api::IKeyListenerCallback*)listener;
@end

class GMPI_MAC_KeyListener : public gmpi::api::IKeyListener
{
    NSView* parentView;
    GMPI_KeyListenerView* keyListenerView = nil;
    gmpi::api::IKeyListenerCallback* callback2 = nullptr;
    gmpi::drawing::Rect bounds;

public:
    GMPI_MAC_KeyListener(NSView* pview, const gmpi::drawing::Rect* r)
        : parentView(pview)
        , bounds(*r)
    {
    }

    ~GMPI_MAC_KeyListener()
    {
        if (keyListenerView)
        {
            [keyListenerView removeFromSuperview];
            keyListenerView = nil;
        }
    }

    gmpi::ReturnCode showAsync(gmpi::api::IUnknown* callback) override
    {
        callback->queryInterface(&gmpi::api::IKeyListenerCallback::guid, (void**)&callback2);

        keyListenerView = [[GMPI_KeyListenerView alloc] initWithParent:parentView callback:callback2];
        [[parentView window] makeFirstResponder:keyListenerView];

        return gmpi::ReturnCode::Ok;
    }

    GMPI_QUERYINTERFACE_METHOD(gmpi::api::IKeyListener);
    GMPI_REFCOUNT;
};

#ifdef GMPI_MAC_KEYLISTENER_IMPLEMENTATION

@implementation GMPI_KeyListenerView

- (id)initWithParent:(NSView*)parent callback:(gmpi::api::IKeyListenerCallback*)pcallback
{
    self = [super initWithFrame:NSMakeRect(0, 0, 0, 0)];
    if (self)
    {
        keyCallback = pcallback;
        if (parent)
            [parent addSubview:self];
    }
    return self;
}

- (BOOL)acceptsFirstResponder { return YES; }

- (void)keyDown:(NSEvent*)event
{
    if (!keyCallback)
        return;

    NSString* characters = [event characters];
    if (characters.length == 0)
        return;

    wchar_t c = [characters characterAtIndex:0];
    int32_t flags = 0;
    if (([event modifierFlags] & NSEventModifierFlagShift) != 0)
        flags |= static_cast<int32_t>(gmpi::api::PointerFlags::KeyShift);
    if (([event modifierFlags] & NSEventModifierFlagCommand) != 0)
        flags |= static_cast<int32_t>(gmpi::api::PointerFlags::KeyControl);
    if (([event modifierFlags] & NSEventModifierFlagOption) != 0)
        flags |= static_cast<int32_t>(gmpi::api::PointerFlags::KeyAlt);

    keyCallback->onKeyDown(static_cast<int32_t>(c), flags);
}

- (void)keyUp:(NSEvent*)event
{
    if (!keyCallback)
        return;

    NSString* characters = [event characters];
    if (characters.length == 0)
        return;

    wchar_t c = [characters characterAtIndex:0];
    int32_t flags = 0;
    if (([event modifierFlags] & NSEventModifierFlagShift) != 0)
        flags |= static_cast<int32_t>(gmpi::api::PointerFlags::KeyShift);
    if (([event modifierFlags] & NSEventModifierFlagCommand) != 0)
        flags |= static_cast<int32_t>(gmpi::api::PointerFlags::KeyControl);
    if (([event modifierFlags] & NSEventModifierFlagOption) != 0)
        flags |= static_cast<int32_t>(gmpi::api::PointerFlags::KeyAlt);

    keyCallback->onKeyUp(static_cast<int32_t>(c), flags);
}

- (BOOL)resignFirstResponder
{
    if (keyCallback)
        keyCallback->onLostFocus(gmpi::ReturnCode::Ok);
    return [super resignFirstResponder];
}

@end

#endif // GMPI_MAC_KEYLISTENER_IMPLEMENTATION

#endif // GMPI_MAC_KEYLISTENER_H
