
/* Copyright (c) 2014 Jeff F McClintock
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*     * Redistributions of source code must retain the above copyright
*       notice, this list of conditions and the following disclaimer.
*     * Redistributions in binary form must reproduce the above copyright
*       notice, this list of conditions and the following disclaimer in the
*       documentation and/or other materials provided with the distribution.
*     * Neither the name SEM, nor 'Music Plugin Interface' nor the
*       names of its contributors may be used to endorse or promote products
*       derived from this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY Jeff F McClintock ``AS IS'' AND ANY
* EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL Jeff F McClintock BE LIABLE FOR ANY
* DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
* LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
* ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
* SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/


//===== MpGui_sdk.h =====
#ifndef SE_VSTGUI_SDK_H_INCLUDED
#define SE_VSTGUI_SDK_H_INCLUDED

#include "mp_sdk_gui2.h"

namespace gmpi
{
	// Cross-platform VST-GUI4 graphics.
	class ISeGraphics : public gmpi::IMpUnknown
	{
	public:
		// First pass of layout update. Return minimum size required for given available size
		virtual int32_t MP_STDCALL measure(MpSize availableSize, MpSize& returnDesiredSize) = 0;

		// Second pass of layout. 
		virtual int32_t MP_STDCALL arrange(MpRect finalRect) = 0;

		virtual int32_t MP_STDCALL OnRender(GmpiDrawing_API::IMpDeviceContext* drawingContext) = 0;

		/*
		virtual int32_t MP_STDCALL openWindow( void ) = 0;

		virtual int32_t MP_STDCALL closeWindow( void ) = 0;

		virtual int32_t MP_STDCALL paint( HDC hDC ) = 0;

		virtual int32_t MP_STDCALL onLButtonDown( UINT flags, POINT point ) = 0;

		virtual int32_t MP_STDCALL onMouseMove( UINT flags, POINT point ) = 0;

		virtual int32_t MP_STDCALL onLButtonUp( UINT flags, POINT point ) = 0;
		*/
	};

	// GUID for ISeGraphics
	// {01FA4268-FE38-4767-81CE-E4CE4FF2D6BC}
	static const gmpi::MpGuid SE_IID_GRAPHICS_VSTGUI =
	{ 0x1fa4268, 0xfe38, 0x4767, { 0x81, 0xce, 0xe4, 0xce, 0x4f, 0xf2, 0xd6, 0xbc } };


	// SynthEdit's graphics API Host-side. New style.
	class ISeGraphicsHostVstGui : public gmpi::IMpUnknown
	{
	public:
		// Get host's current skin's font information.
		//		virtual int32_t MP_STDCALL getFontInfo( wchar_t* style, MpFontInfo& fontInfo, HFONT& returnFontInformation ) = 0;
	};

	// GUID for IMpGraphicsHostWinGdiSynthEdit
	// {B1EFF8C9-9ADC-450b-BCD6-D359CFDC8275}
	static const gmpi::MpGuid SE_IID_GRAPHICS_HOST_VSTGUI =
	{ 0x2c672479, 0x722f, 0x4e87, { 0xaa, 0x92, 0xca, 0xc, 0x4e, 0x74, 0xdc, 0x0 } };
}


/**********************************************************************************
SeGuiVstGuiBase
This links directly to VST GUI graphics API. Only temporary.
**********************************************************************************/
// Internal native VST-GUI graphics.
// graphicsApi="VSTGUI"
class SeGuiVstGuiBase :
	public MpGuiBase2, public gmpi::ISeGraphics
{
public:
	SeGuiVstGuiBase() :
	refCount_(1)
	{
	}
	// IUnknown methods
	virtual int32_t MP_STDCALL queryInterface(const gmpi::MpGuid& iid, void** returnInterface)
	{
		*returnInterface = 0;

		if ( iid == gmpi::SE_IID_GRAPHICS_VSTGUI)
		{
			*returnInterface = static_cast<ISeGraphics*>(this);
			addRef();
			return gmpi::MP_OK;
		}

		if (iid == gmpi::MP_IID_GUI_PLUGIN2 || iid == gmpi::MP_IID_UNKNOWN)
		{
			*returnInterface = static_cast<IMpUserInterface2*>(this);
			addRef();
			return gmpi::MP_OK;
		}

		return gmpi::MP_NOSUPPORT;
	}
	virtual int32_t MP_STDCALL addRef() = 0;
	virtual int32_t MP_STDCALL release() = 0;

	// IMpUserInterface2 methods
	virtual int32_t MP_STDCALL setHost(gmpi::IMpUnknown* host)
	{
		host->queryInterface(gmpi::SE_IID_GRAPHICS_HOST_VSTGUI, reinterpret_cast<void**>(&guiHost_));

		if (guiHost_ == 0)
		{
			return gmpi::MP_NOSUPPORT; //  throw "host Interfaces not supported";
		}

		return MpGuiBase2::setHost(host);
	}

	// ISeGraphics methods
	// First pass of layout update. Return minimum size required for given available size
	virtual int32_t MP_STDCALL measure(MpSize availableSize, MpSize& returnDesiredSize) = 0;

	// Second pass of layout. 
	virtual int32_t MP_STDCALL arrange(MpRect finalRect) = 0;

	virtual int32_t MP_STDCALL OnRender(GmpiDrawing_API::IMpDeviceContext* drawingContext){ return gmpi::MP_OK; };
	/*
	virtual int32_t MP_STDCALL openWindow( void ) = 0;

	virtual int32_t MP_STDCALL closeWindow( void ) = 0;

	virtual int32_t MP_STDCALL paint( HDC hDC ) = 0;

	virtual int32_t MP_STDCALL onLButtonDown( UINT flags, POINT point ) = 0;

	virtual int32_t MP_STDCALL onMouseMove( UINT flags, POINT point ) = 0;

	virtual int32_t MP_STDCALL onLButtonUp( UINT flags, POINT point ) = 0;
	*/

	ISeGraphics* getGuiHost(){ return guiHost_; };

//protected:
	int32_t refCount_;

private:
	ISeGraphics* guiHost_;
	MpRect rect_;
};


#endif // include guard.
