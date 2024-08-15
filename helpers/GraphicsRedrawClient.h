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
struct DECLSPEC_NOVTABLE IGraphicsRedrawClient : gmpi::api::IUnknown
{
	virtual void PreGraphicsRedraw() = 0;

	// {4CCF9E3A-05AE-46C8-AEBB-1FFC5E950494}
	inline static const gmpi::api::Guid guid =
	{ 0x4ccf9e3a, 0x5ae, 0x46c8, { 0xae, 0xbb, 0x1f, 0xfc, 0x5e, 0x95, 0x4, 0x94 } };
};

//TODO COMbine?

// TODO: all rects be passed as pointers (for speed and consistency w D2D and C ABI compatibility). !!!
struct IDrawingClient : gmpi::api::IUnknown
{
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

struct DECLSPEC_NOVTABLE IInputClient : gmpi::api::IUnknown
{
	// Mouse events.
	virtual ReturnCode setHover(bool isMouseOverMe) = 0;
	virtual ReturnCode hitTest(gmpi::drawing::Point point, int32_t flags) = 0;

	virtual ReturnCode onPointerDown(gmpi::drawing::Point point, int32_t flags) = 0;
	virtual ReturnCode onPointerMove(gmpi::drawing::Point point, int32_t flags) = 0;
	virtual ReturnCode onPointerUp(gmpi::drawing::Point point, int32_t flags) = 0;
	virtual ReturnCode onMouseWheel(gmpi::drawing::Point point, int32_t flags, int32_t delta) = 0;

	// right-click menu
	virtual ReturnCode populateContextMenu(gmpi::drawing::Point point, gmpi::api::IUnknown* contextMenuItemsSink) = 0;
	virtual ReturnCode onContextMenu(int32_t idx) = 0;

	// keyboard events.
	virtual ReturnCode OnKeyPress(wchar_t c) = 0;

	// {D2D020D1-BCEE-49F9-A173-97BC6460A727}
	inline static const gmpi::api::Guid guid =
	{ 0xd2d020d1, 0xbcee, 0x49f9, { 0xa1, 0x73, 0x97, 0xbc, 0x64, 0x60, 0xa7, 0x27 } };
};

struct DECLSPEC_NOVTABLE IContextItemSink : gmpi::api::IUnknown
{
	// WARNING: USING SAME GUID AS IMpContextItemSink. If u change the interface, change the GUID!
	virtual ReturnCode addItem(const char* text, int32_t id, int32_t flags = 0) = 0;

	// {BC152E7E-7FB8-4921-84EE-BED7CFD9A897}
	inline static const gmpi::api::Guid guid =
	{ 0xbc152e7e, 0x7fb8, 0x4921, { 0x84, 0xee, 0xbe, 0xd7, 0xcf, 0xd9, 0xa8, 0x97 } };
};

struct DECLSPEC_NOVTABLE IDrawingHost : gmpi::api::IUnknown
{
	virtual ReturnCode getDrawingFactory(gmpi::api::IUnknown** returnFactory) = 0;

	// TODO: sort out method name case.
	// Get host's current skin's font information.
	virtual void invalidateRect(const gmpi::drawing::Rect* invalidRect) = 0;

#if 0
	virtual void invalidateMeasure() = 0;
#endif

	// {4E7EEF02-1F0B-4E10-AA44-DD54C0B1CBB0}
	inline static const gmpi::api::Guid guid =
	{ 0x4e7eef02, 0x1f0b, 0x4e10, { 0xaa, 0x44, 0xdd, 0x54, 0xc0, 0xb1, 0xcb, 0xb0 } };
};

struct DECLSPEC_NOVTABLE IInputHost : gmpi::api::IUnknown
{
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

struct DECLSPEC_NOVTABLE IDialogHost : gmpi::api::IUnknown
{
	virtual ReturnCode createTextEdit(gmpi::api::IUnknown** returnTextEdit) = 0;
	virtual ReturnCode createPopupMenu(gmpi::api::IUnknown** returnPopupMenu) = 0;
	virtual ReturnCode createFileDialog(int32_t dialogType, gmpi::api::IUnknown** returnDialog) = 0;
	virtual ReturnCode createStockDialog(int32_t dialogType, gmpi::api::IUnknown** returnDialog) = 0;

	// {7BB86E70-88CB-44B5-8059-7D3D1CBE9F56}
	inline static const gmpi::api::Guid guid =
	{ 0x7bb86e70, 0x88cb, 0x44b5, { 0x80, 0x59, 0x7d, 0x3d, 0x1c, 0xbe, 0x9f, 0x56 } };
};

// move out of api?
enum class PopupMenuFlags : int32_t
{
	Grayed = 1,
	Break = 2,	// Windows only
	Ticked = 4,
	Separator = 8,
	SubMenuBegin = 16,
	SubMenuEnd = 32,
};

struct DECLSPEC_NOVTABLE IPopupMenu : gmpi::api::IUnknown
{
	virtual ReturnCode addItem(const char* text, int32_t id, int32_t flags) = 0;
	virtual ReturnCode setAlignment(int32_t alignment) = 0;
	virtual ReturnCode showAsync(const gmpi::drawing::Rect* rect, gmpi::api::IUnknown* returnCallback) = 0;
// not async	virtual ReturnCode GetSelectedId() = 0;

	// {7BB86E70-88CB-44B5-8059-7D3D1CBE9F56}
	inline static const gmpi::api::Guid guid =
	{ 0x7bb86e70, 0x88cb, 0x44b5, { 0x80, 0x59, 0x7d, 0x3d, 0x1c, 0xbe, 0x9f, 0x56 } };
};

struct DECLSPEC_NOVTABLE IPopupMenuCallback : gmpi::api::IUnknown
{
public:
	virtual void onComplete(ReturnCode result, int32_t selectedID) = 0;

	// {E88E02C8-61B1-415B-9379-11AB7368B903}
	inline static const gmpi::api::Guid guid =
	{ 0xe88e02c8, 0x61b1, 0x415b, { 0x93, 0x79, 0x11, 0xab, 0x73, 0x68, 0xb9, 0x3 } };
};

#if 0 // not required atm. SynthEdit SDK3 support
struct DECLSPEC_NOVTABLE ILegacyCompletionCallback : gmpi::api::IUnknown
{
public:
	virtual void OnComplete(ReturnCode result) = 0;

	// {709582BA-AF65-43E6-A24C-AB05F8D6980B}
	inline static const gmpi::api::Guid guid =
	{ 0x709582ba, 0xaf65, 0x43e6,{ 0xa2, 0x4c, 0xab, 0x5, 0xf8, 0xd6, 0x98, 0xb } };
};
#endif

struct DECLSPEC_NOVTABLE IFileDialog : gmpi::api::IUnknown
{
	// save or open?
	virtual ReturnCode addExtension(const char* extension, const char* description = "") = 0;
	virtual ReturnCode setInitialFilename(const char* text) = 0;
	virtual ReturnCode setInitialDirectory(const char* text) = 0;
	virtual ReturnCode showAsync(const gmpi::drawing::Rect* rect, gmpi::api::IUnknown* returnCallback) = 0;
// not async	virtual ReturnCode GetSelectedFilename(gmpi::api::IUnknown* returnString) = 0;

// {5D44F94E-26DB-4A22-934B-FC07BFDD6096}
	inline static const gmpi::api::Guid guid =
	{ 0x5d44f94e, 0x26db, 0x4a22, { 0x93, 0x4b, 0xfc, 0x7, 0xbf, 0xdd, 0x60, 0x96 } };
};

struct DECLSPEC_NOVTABLE IStockDialog : gmpi::api::IUnknown
{
	virtual ReturnCode setTitle(const char* text) = 0;
	virtual ReturnCode setText(const char* text) = 0;
	virtual ReturnCode showAsync(const gmpi::drawing::Rect* rect, gmpi::api::IUnknown* returnCallback) = 0;

	// {A4F2DFEC-97B6-44CB-BE2F-44F0A7F90BC3}
	inline static const gmpi::api::Guid guid =
	{ 0xa4f2dfec, 0x97b6, 0x44cb, { 0xbe, 0x2f, 0x44, 0xf0, 0xa7, 0xf9, 0xb, 0xc3 } };
};

} // namespace api
} // namespace gmpi
#endif /* GraphicsRedrawClient_h */
