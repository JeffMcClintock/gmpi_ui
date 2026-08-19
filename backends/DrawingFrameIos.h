#ifndef DRAWINGFRAME_IOS_H
#define DRAWINGFRAME_IOS_H

#include "GmpiSdkCommon.h"
#include "helpers/NativeUi.h"

// The same three-function C boundary as DrawingFrameMac.h, so wrapper code that
// hosts an editor is written once per platform pair rather than per function:
// the pointer is a UIView* here where the Mac header returns an NSView*.
void* createNativeView(void* parent, class IUnknown* parameterHost, class IUnknown* controller, int width, int height);
void  resizeNativeView(void* view, int width, int height);
void  gmpi_onCloseNativeView(void* view);

#endif // DRAWINGFRAME_IOS_H
