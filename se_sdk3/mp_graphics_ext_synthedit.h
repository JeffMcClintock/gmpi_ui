//===== MpGraphicsExtensionSynthEdit.h =====
#ifndef MP_GFX_SE_H_INCLUDED
#define MP_GFX_SE_H_INCLUDED

// MPI supports multiple graphics APIs through optional interfaces.

// SynthEdit Graphics API, plugin-side. Based on Windows GDI.

struct MpSize
{
	int32_t x;
	int32_t y;
};


struct MpRect
{
	int32_t top;
	int32_t bottom;
	int32_t left;
	int32_t right;
};

struct MpFontInfo
{
	int size;
	int color;
	int colorBackground;
	int flags;			  // alignment etc.
	int fontHeight;
	char future [100];
};

class IMpGraphicsSynthEdit : public IMpUnknown
{
public:
	virtual MpResult paint( HDC hDC ) = 0;

	// First pass of layout update. Return minimum size required for given available size
	virtual MpResult measure( MpSize availableSize, MpSize& returnDesiredSize ) = 0;

	// Second pass of layout. 
	virtual MpResult arrange( MpRect finalRect ) = 0;

	virtual MpResult onLButtonDown( UINT flags, POINT point ) = 0;

	virtual MpResult onMouseMove( UINT flags, POINT point ) = 0;

	virtual MpResult onLButtonUp( UINT flags, POINT point ) = 0;
};

// GUID for IMpGraphicsSynthEdit
// {0157347B-3FD3-4cee-947E-88F694349254}
static const MpGuid MP_IID_GRAPHICS_SYNTHEDIT = 
{ 0x157347b, 0x3fd3, 0x4cee, { 0x94, 0x7e, 0x88, 0xf6, 0x94, 0x34, 0x92, 0x54 } };


// SynthEdit's graphics API Host-side.
class IMpGraphicsHostSynthEdit : public IMpUnknown
{
public:
	virtual MpResult setCapture( void ) = 0;

	virtual MpResult getCapture( int32_t& returnValue ) = 0;

	virtual MpResult releaseCapture( void ) = 0;

	virtual MpResult invalidateRect( RECT& rect ) = 0;

	// Map a point on plugin gui to the system screen (absolute co-ords).
	virtual MpResult mapPointToScreen( POINT& point ) = 0;

	// Get host's current skin's font information.
	virtual MpResult getFontInfo( wchar_t* style, MpFontInfo& fontInfo, HFONT& returnFontInformation ) = 0;
};

// GUID for IMpGraphicsHostSynthEdit
// {2A3F4D28-52C5-4a31-BF85-B79853265F65}
static const MpGuid MP_IID_GRAPHICS_HOST_SYNTHEDIT = 
{ 0x2a3f4d28, 0x52c5, 0x4a31, { 0xbf, 0x85, 0xb7, 0x98, 0x53, 0x26, 0x5f, 0x65 } };

#endif // MP_GFX_SE_H_INCLUDED INCLUDED
