#ifndef DRAWINGFRAME_MAC_H
#define DRAWINGFRAME_MAC_H

#include <Cocoa/Cocoa.h>
#include "GmpiSdkCommon.h"
#include "helpers/NativeUi.h"
#include "MacKeyListener.h"

// returns an NSView* (cast to void* for languages other than objective-C)
void* createNativeView(void* parent, class IUnknown* parameterHost, class IUnknown* controller, int width, int height);

#endif // DRAWINGFRAME_MAC_H
