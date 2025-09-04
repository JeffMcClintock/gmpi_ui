#ifndef DRAWINGFRAME_MAC_H
#define DRAWINGFRAME_MAC_H

#include <Cocoa/Cocoa.h>
#include "GmpiSdkCommon.h"
#include "DrawingFrameCommon.h"

// returns an NSView* (cast to void* for languages other than objective-C)
void* createNativeView(void* parent, class IUnknown* parameterHost, class IUnknown* controller, int width, int height);

class GMPI_MAC_KeyListener : public gmpi::api::IKeyListener
{
    gmpi::drawing::Rect bounds{};
    gmpi::api::IKeyListenerCallback* callback2{};
    NSView* parentView{};
    NSView* keyListenerView{};

public:
    GMPI_MAC_KeyListener(NSView* pview, const gmpi::drawing::Rect* r);
    ~GMPI_MAC_KeyListener();

    gmpi::ReturnCode showAsync(gmpi::api::IUnknown* callback) override;

    GMPI_QUERYINTERFACE_METHOD(gmpi::api::IKeyListener);
    GMPI_REFCOUNT;
};

#endif // DRAWINGFRAME_MAC_H
