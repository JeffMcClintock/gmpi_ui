#ifndef MP_GFX_WIN_GDI_H_INCLUDED
#define MP_GFX_WIN_GDI_H_INCLUDED

// Windows Graphics API, Plugin-side.
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

class IMpGraphicsWinGdi : public gmpi::IMpUnknown
{
public:
	// First pass of layout update. Return minimum size required for given available size
	virtual int32_t MP_STDCALL measure( MpSize availableSize, MpSize& returnDesiredSize ) = 0;

	// Second pass of layout. 
	virtual int32_t MP_STDCALL arrange( MpRect finalRect ) = 0;

	virtual int32_t MP_STDCALL openWindow( HWND parentWindow, HWND& returnWindowHandle ) = 0;

	virtual int32_t MP_STDCALL closeWindow( void ) = 0;
};

// GUID for IMpGraphicsWinGdi
// {76FAC807-0C89-465d-9FAF-49B12D964118}
static const gmpi::MpGuid MP_IID_GRAPHICS_WIN_GDI = 
{ 0x76fac807, 0xc89, 0x465d, { 0x9f, 0xaf, 0x49, 0xb1, 0x2d, 0x96, 0x41, 0x18 } };


// Windows Graphics API, Host-side.
class IMpGraphicsHostWinGdi : public gmpi::IMpUnknown
{
public:
};

// GUID for IMpGraphicsHostWinGdi
// {E52FAA41-42C3-40a6-A11A-E69DB335CE50}
static const gmpi::MpGuid MP_IID_GRAPHICS_HOST_WIN_GDI = 
{ 0xe52faa41, 0x42c3, 0x40a6, { 0xa1, 0x1a, 0xe6, 0x9d, 0xb3, 0x35, 0xce, 0x50 } };

#endif // MP_GFX_WIN_GDI_H_INCLUDED INCLUDED



//===== MpGraphicsExtensionSynthEdit.h =====
