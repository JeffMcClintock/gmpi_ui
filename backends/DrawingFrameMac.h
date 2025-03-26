#ifndef DRAWINGFRAME_MAC_H
#define DRAWINGFRAME_MAC_H

#include <Cocoa/Cocoa.h>
#include "GmpiSdkCommon.h"
#include "DrawingFrameCommon.h"

class GMPI_MAC_KeyListener : public gmpi::api::IKeyListener
{
    gmpi::drawing::Rect bounds{};
    gmpi::api::IKeyListenerCallback* callback2{};
    NSView* view{};
    KeyListenerView* keyListenerView{};

public:
    GMPI_MAC_KeyListener(NSView* pview, const gmpi::drawing::Rect* r);
    ~GMPI_MAC_KeyListener();

    gmpi::ReturnCode showAsync(gmpi::api::IUnknown* callback) override;

    GMPI_QUERYINTERFACE_METHOD(gmpi::api::IKeyListener);
    GMPI_REFCOUNT;
};

#endif // DRAWINGFRAME_MAC_H
