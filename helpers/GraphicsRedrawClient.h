//
//  GraphicsRedrawClient.h
//  Switches
//
//  Created by Jeff McClintock on 4/10/22.
//

#ifndef GraphicsRedrawClient2_h
#define GraphicsRedrawClient2_h

#include "GmpiApiDrawing.h"

namespace gmpi
{
namespace api
{

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
	virtual ReturnCode open(gmpi::api::IUnknown* host) = 0;

	// First pass of layout update. Return minimum size required for given available size
	virtual ReturnCode measure(const gmpi::drawing::Size* availableSize, gmpi::drawing::Size* returnDesiredSize) = 0;

	// Second pass of layout.
	virtual ReturnCode arrange(const gmpi::drawing::Rect* finalRect) = 0;

	// TODO: getClipRect() ?

	virtual ReturnCode render(gmpi::drawing::api::IDeviceContext* drawingContext) = 0;

	virtual ReturnCode getClipArea(drawing::Rect* returnRect) = 0;

	// {E922D16F-447B-4E82-B0B1-FD995CA4210E}
	inline static const gmpi::api::Guid guid =
	{ 0xe922d16f, 0x447b, 0x4e82, { 0xb0, 0xb1, 0xfd, 0x99, 0x5c, 0xa4, 0x21, 0xe } };
};

// TODO GMPIfy
enum GG_POINTER_FLAGS {
	GG_POINTER_FLAG_NONE = 0,
	GG_POINTER_FLAG_NEW = 0x01,					// Indicates the arrival of a new pointer.
	GG_POINTER_FLAG_INCONTACT = 0x04,
	GG_POINTER_FLAG_FIRSTBUTTON = 0x10,
	GG_POINTER_FLAG_SECONDBUTTON = 0x20,
	GG_POINTER_FLAG_THIRDBUTTON = 0x40,
	GG_POINTER_FLAG_FOURTHBUTTON = 0x80,
	GG_POINTER_FLAG_CONFIDENCE = 0x00000400,	// Confidence is a suggestion from the source device about whether the pointer represents an intended or accidental interaction.
	GG_POINTER_FLAG_PRIMARY = 0x00002000,	// First pointer to contact surface. Mouse is usually Primary.

	GG_POINTER_SCROLL_HORIZ = 0x00008000,	// Mouse Wheel is scrolling horizontal.

	GG_POINTER_KEY_SHIFT = 0x00010000,	// Modifer key - <SHIFT>.
	GG_POINTER_KEY_CONTROL = 0x00020000,	// Modifer key - <CTRL> or <Command>.
	GG_POINTER_KEY_ALT = 0x00040000,	// Modifer key - <ALT> or <Option>.
};

// TODO incorporate IMpKeyClient?
class DECLSPEC_NOVTABLE IInputClient : public gmpi::api::IUnknown
{
public:
	// Mouse events.
	virtual ReturnCode hitTest(gmpi::drawing::Point point, int32_t flags) = 0;

	virtual ReturnCode onPointerDown(gmpi::drawing::Point point, int32_t flags) = 0;
	virtual ReturnCode onPointerMove(gmpi::drawing::Point point, int32_t flags) = 0;
	virtual ReturnCode onPointerUp(gmpi::drawing::Point point, int32_t flags) = 0;
	virtual ReturnCode onMouseWheel(gmpi::drawing::Point point, int32_t flags, int32_t delta) = 0;
	virtual ReturnCode setHover(bool isMouseOverMe) = 0;

	// keyboard events.
	virtual ReturnCode OnKeyPress(wchar_t c) = 0;

	// {D2D020D1-BCEE-49F9-A173-97BC6460A727}
	inline static const gmpi::api::Guid guid =
	{ 0xd2d020d1, 0xbcee, 0x49f9, { 0xa1, 0x73, 0x97, 0xbc, 0x64, 0x60, 0xa7, 0x27 } };
};

class IDrawingHost : public gmpi::api::IUnknown
{
public:
	virtual ReturnCode getDrawingFactory(gmpi::api::IUnknown** returnFactory) = 0;

	// TODO: sort out method name case.
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

class IInputHost : public gmpi::api::IUnknown
{
public:
	// mouse
	virtual ReturnCode setCapture() = 0;
	virtual ReturnCode getCapture(bool& returnValue) = 0;
	virtual ReturnCode releaseCapture() = 0;

	// keyboard
	virtual ReturnCode getFocus() = 0;
	virtual ReturnCode releaseFocus() = 0;
	
	// {B5109952-2608-48B3-9685-788D36EBA7AF}
	inline static const gmpi::api::Guid guid =
	{ 0xb5109952, 0x2608, 0x48b3, { 0x96, 0x85, 0x78, 0x8d, 0x36, 0xeb, 0xa7, 0xaf } };
};
} // namespace api
} // namespace gmpi
#endif /* GraphicsRedrawClient_h */
