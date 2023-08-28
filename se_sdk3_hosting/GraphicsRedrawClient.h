//
//  GraphicsRedrawClient.h
//  Switches
//
//  Created by Jeff McClintock on 4/10/22.
//

#ifndef GraphicsRedrawClient_h
#define GraphicsRedrawClient_h

#include "../se_sdk3/Drawing_API.h"

// notify a plugin when it's time to display a new frame.
// Supports the client optimizing how often it checks the DSP queue
class DECLSPEC_NOVTABLE IGraphicsRedrawClient : public gmpi::IMpUnknown
{
public:
	virtual void PreGraphicsRedraw() = 0;

	// {4CCF9E3A-05AE-46C8-AEBB-1FFC5E950494}
	inline static const gmpi::MpGuid guid =
	{ 0x4ccf9e3a, 0x5ae, 0x46c8, { 0xae, 0xbb, 0x1f, 0xfc, 0x5e, 0x95, 0x4, 0x94 } };
};

//TODO COMbine?

// TODO: all rects be passed as pointers (for speed and consistency w D2D and C ABI compatibility). !!!
class IMpDrawingClient : public gmpi::IMpUnknown
{
public:
	virtual int32_t open(gmpi::IMpUnknown* host) = 0;

	// First pass of layout update. Return minimum size required for given available size
	virtual int32_t MP_STDCALL measure(const GmpiDrawing_API::MP1_SIZE* availableSize, GmpiDrawing_API::MP1_SIZE* returnDesiredSize) = 0;

	// Second pass of layout.
	virtual int32_t MP_STDCALL arrange(const GmpiDrawing_API::MP1_RECT* finalRect) = 0;

	virtual int32_t MP_STDCALL OnRender(GmpiDrawing_API::IMpDeviceContext* drawingContext) = 0;

	// {E922D16F-447B-4E82-B0B1-FD995CA4210E}
	inline static const gmpi::MpGuid guid =
	{ 0xe922d16f, 0x447b, 0x4e82, { 0xb0, 0xb1, 0xfd, 0x99, 0x5c, 0xa4, 0x21, 0xe } };
};


class IDrawingHost : public gmpi::IMpUnknown
{
public:
	virtual int32_t MP_STDCALL getDrawingFactory(gmpi::IMpUnknown** returnFactory) = 0;

	// TODO: sort out methd name case.
	// Get host's current skin's font information.
	virtual void MP_STDCALL invalidateRect(const GmpiDrawing_API::MP1_RECT* invalidRect) = 0;
#if 0
	virtual void MP_STDCALL invalidateMeasure() = 0;

	virtual int32_t MP_STDCALL setCapture() = 0;
	virtual int32_t MP_STDCALL getCapture(int32_t& returnValue) = 0;
	virtual int32_t MP_STDCALL releaseCapture() = 0;

	virtual int32_t MP_STDCALL createPlatformMenu(/* shouldbe const */ GmpiDrawing_API::MP1_RECT* rect, gmpi_gui::IMpPlatformMenu** returnMenu) = 0;
	virtual int32_t MP_STDCALL createPlatformTextEdit(/* shouldbe const */ GmpiDrawing_API::MP1_RECT* rect, gmpi_gui::IMpPlatformText** returnTextEdit) = 0;
	// Ideally this would be in IMpGraphicsHostBase, but doing so would break ABI for existing modules.
	virtual int32_t MP_STDCALL createOkCancelDialog(int32_t dialogType, gmpi_gui::IMpOkCancelDialog** returnDialog) = 0;

	static gmpi::MpGuid IID() { return SE_IID_GRAPHICS_HOST; };
#endif
	// {4E7EEF02-1F0B-4E10-AA44-DD54C0B1CBB0}
	inline static const gmpi::MpGuid guid =
	{ 0x4e7eef02, 0x1f0b, 0x4e10, { 0xaa, 0x44, 0xdd, 0x54, 0xc0, 0xb1, 0xcb, 0xb0 } };
};

#endif /* GraphicsRedrawClient_h */
