#ifndef GMPI_MAC_EVENT_HELPERS_H
#define GMPI_MAC_EVENT_HELPERS_H

// Shared event-conversion helpers used by macOS NSView subclasses that
// route Cocoa input into gmpi::api::IInputClient.

#import <Cocoa/Cocoa.h>
#include "GmpiSdkCommon.h"
#include "helpers/NativeUi.h"

inline gmpi::drawing::Point mouseToGmpi(NSView* view, NSEvent* theEvent)
{
    NSPoint localPoint = [view convertPoint:[theEvent locationInWindow] fromView:nil];
#if USE_BACKING_BUFFER
    localPoint.y = view.bounds.origin.y + view.bounds.size.height - localPoint.y;
#endif
    return {(float)localPoint.x, (float)localPoint.y};
}

// Maps NSEvent modifier flags onto gmpi::api::PointerFlags. Cmd → KeyControl
// follows the macOS convention that Cmd is the equivalent of Win Ctrl for
// shortcut purposes (matching cross-platform user expectations).
inline void applyKeyModifiers(int32_t& flags, NSEvent* theEvent)
{
    const auto mod = [theEvent modifierFlags];
    if ((mod & NSEventModifierFlagShift) != 0)
        flags |= static_cast<int32_t>(gmpi::api::PointerFlags::KeyShift);
    if ((mod & NSEventModifierFlagCommand) != 0)
        flags |= static_cast<int32_t>(gmpi::api::PointerFlags::KeyControl);
    if ((mod & NSEventModifierFlagOption) != 0)
        flags |= static_cast<int32_t>(gmpi::api::PointerFlags::KeyAlt);
}

#endif // GMPI_MAC_EVENT_HELPERS_H
