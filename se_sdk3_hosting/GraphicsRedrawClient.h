//
//  GraphicsRedrawClient.h
//  Switches
//
//  Created by Jeff McClintock on 4/10/22.
//

#ifndef GraphicsRedrawClient_h
#define GraphicsRedrawClient_h

#include "Drawing_API.h"

// notify a plugin when it's time to display a new frame.
// Supports the client optimizing how often it checks the DSP queue
class DECLSPEC_NOVTABLE IGraphicsRedrawClient : public gmpi::api::IUnknown
{
public:
	virtual void PreGraphicsRedraw() = 0;

	// {4CCF9E3A-05AE-46C8-AEBB-1FFC5E950494}
	inline static const gmpi::api::Guid guid =
	{ 0x4ccf9e3a, 0x5ae, 0x46c8, { 0xae, 0xbb, 0x1f, 0xfc, 0x5e, 0x95, 0x4, 0x94 } };
};

//TODO COMbine?

// TODO: all rects be passed as pointers (for speed and consistency w D2D and C ABI compatibility). !!!
class IDrawingClient : public gmpi::api::IUnknown
{
public:
	virtual gmpi::ReturnCode open(gmpi::api::IUnknown* host) = 0;

	// First pass of layout update. Return minimum size required for given available size
	virtual gmpi::ReturnCode measure(const gmpi::drawing::Size* availableSize, gmpi::drawing::Size* returnDesiredSize) = 0;

	// Second pass of layout.
	virtual gmpi::ReturnCode arrange(const gmpi::drawing::Rect* finalRect) = 0;

	virtual gmpi::ReturnCode onRender(gmpi::drawing::api::IDeviceContext* drawingContext) = 0;

	// {E922D16F-447B-4E82-B0B1-FD995CA4210E}
	inline static const gmpi::api::Guid guid =
	{ 0xe922d16f, 0x447b, 0x4e82, { 0xb0, 0xb1, 0xfd, 0x99, 0x5c, 0xa4, 0x21, 0xe } };
};

// TODO incorporate IMpKeyClient?
class DECLSPEC_NOVTABLE IInputClient : public gmpi::api::IUnknown
{
public:
	virtual gmpi::ReturnCode onPointerDown(gmpi::drawing::Point point, int32_t flags) = 0;
	virtual gmpi::ReturnCode onPointerMove(gmpi::drawing::Point point, int32_t flags) = 0;
	virtual gmpi::ReturnCode onPointerUp(gmpi::drawing::Point point, int32_t flags) = 0;

	// {D2D020D1-BCEE-49F9-A173-97BC6460A727}
	inline static const gmpi::api::Guid guid =
	{ 0xd2d020d1, 0xbcee, 0x49f9, { 0xa1, 0x73, 0x97, 0xbc, 0x64, 0x60, 0xa7, 0x27 } };
};

class IDrawingHost : public gmpi::api::IUnknown
{
public:
	virtual gmpi::ReturnCode getDrawingFactory(gmpi::api::IUnknown** returnFactory) = 0;

	// TODO: sort out methd name case.
	// Get host's current skin's font information.
	virtual void invalidateRect(const gmpi::drawing::Rect* invalidRect) = 0;
#if 0
	virtual void invalidateMeasure() = 0;

	virtual int32_t setCapture() = 0;
	virtual int32_t getCapture(int32_t& returnValue) = 0;
	virtual int32_t releaseCapture() = 0;

	virtual int32_t createPlatformMenu(/* shouldbe const */ gmpi::drawing::Rect* rect, gmpi_gui::IPlatformMenu** returnMenu) = 0;
	virtual int32_t createPlatformTextEdit(/* shouldbe const */ gmpi::drawing::Rect* rect, gmpi_gui::IPlatformText** returnTextEdit) = 0;
	// Ideally this would be in IMpGraphicsHostBase, but doing so would break ABI for existing modules.
	virtual int32_t createOkCancelDialog(int32_t dialogType, gmpi_gui::IOkCancelDialog** returnDialog) = 0;

	static gmpi::api::Guid IID() { return SE_IID_GRAPHICS_HOST; };
#endif
	// {4E7EEF02-1F0B-4E10-AA44-DD54C0B1CBB0}
	inline static const gmpi::api::Guid guid =
	{ 0x4e7eef02, 0x1f0b, 0x4e10, { 0xaa, 0x44, 0xdd, 0x54, 0xc0, 0xb1, 0xcb, 0xb0 } };
};

#endif /* GraphicsRedrawClient_h */
