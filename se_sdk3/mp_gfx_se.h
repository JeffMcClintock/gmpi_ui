#ifndef MP_GFX_SE_H_INCLUDED
#define MP_GFX_SE_H_INCLUDED

// MPI supports multiple graphics APIs through optional interfaces.

// SynthEdit Graphics API, plugin-side. Based on Windows GDI.


struct MpFontInfo
{
	int size;
	int color;
	int colorBackground;
	int flags;			  // alignment etc.
	int fontHeight;
	char future [100];
};


// new, Native Window type.
class ISeGraphics : public IMpGraphicsWinGdi
{
public:
};

// GUID for IMpGraphicsSynthEdit
// {0157347B-3FD3-4cee-947E-88F694349254}
static const gmpi::MpGuid SE_IID_GRAPHICS = 
{ 0x157347b, 0x3fd3, 0x4cee, { 0x94, 0x7e, 0x88, 0xf6, 0x94, 0x34, 0x92, 0x54 } };


// Old-style composited (draw-on-window) graphics.
class ISeGraphicsComposited : public gmpi::IMpUnknown
{
public:
	virtual int32_t MP_STDCALL paint( HDC hDC ) = 0;

	// First pass of layout update. Return minimum size required for given available size
	virtual int32_t MP_STDCALL measure( MpSize availableSize, MpSize& returnDesiredSize ) = 0;

	// Second pass of layout. 
	virtual int32_t MP_STDCALL arrange( MpRect finalRect ) = 0;

	virtual int32_t MP_STDCALL onLButtonDown( UINT flags, POINT point ) = 0;

	virtual int32_t MP_STDCALL onMouseMove( UINT flags, POINT point ) = 0;

	virtual int32_t MP_STDCALL onLButtonUp( UINT flags, POINT point ) = 0;
};

// GUID for IMpGraphicsSynthEdit
// {0157347B-3FD3-4cee-947E-88F694349254}
static const gmpi::MpGuid SE_IID_GRAPHICS_COMPOSITED = 
{ 0x157347b, 0x3fd3, 0x4cee, { 0x94, 0x7e, 0x88, 0xf6, 0x94, 0x34, 0x92, 0x54 } };


// SynthEdit's graphics API Host-side. New style.
class ISeGraphicsHostWinGdi : public IMpGraphicsHostWinGdi
{
public:
	// Get host's current skin's font information.
	virtual int32_t MP_STDCALL getFontInfo( wchar_t* style, MpFontInfo& fontInfo, HFONT& returnFontInformation ) = 0;
};

// GUID for IMpGraphicsHostWinGdiSynthEdit
// {B1EFF8C9-9ADC-450b-BCD6-D359CFDC8275}
static const gmpi::MpGuid SE_IID_GRAPHICS_HOST_WIN_GDI = 
{ 0xb1eff8c9, 0x9adc, 0x450b, { 0xbc, 0xd6, 0xd3, 0x59, 0xcf, 0xdc, 0x82, 0x75 } };

// SynthEdit's graphics API Host-side. Old style.
class ISeGraphicsHostComposited : public gmpi::IMpUnknown
{
public:
	virtual int32_t MP_STDCALL setCapture( void ) = 0;

	virtual int32_t MP_STDCALL getCapture( int32_t& returnValue ) = 0;

	virtual int32_t MP_STDCALL releaseCapture( void ) = 0;

	virtual int32_t MP_STDCALL invalidateRect( RECT* rect ) = 0;

	// Map a point on plugin gui to the system screen (absolute co-ords).
	virtual int32_t MP_STDCALL mapPointToScreen( POINT& point ) = 0;

	// Get host's current skin's font information.
	virtual int32_t MP_STDCALL getFontInfo( wchar_t* style, MpFontInfo& fontInfo, HFONT& returnFontInformation ) = 0;
};

// GUID for ISeGraphicsHostComposited
// {2A3F4D28-52C5-4a31-BF85-B79853265F65}
static const gmpi::MpGuid MP_IID_GRAPHICS_HOST_SYNTHEDIT = 
{ 0x2a3f4d28, 0x52c5, 0x4a31, { 0xbf, 0x85, 0xb7, 0x98, 0x53, 0x26, 0x5f, 0x65 } };

#endif // MP_GFX_SE_H_INCLUDED INCLUDED
