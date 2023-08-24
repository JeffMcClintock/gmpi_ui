/* Copyright (c) 2016 Jeff F McClintock
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

#define	GMPI_GUI_SDK_REVISION 10000 // 1.00
/* Version History
	11/09/2014 - V1.00 : Official release.

   TODO
   * Choose Folder Dialog
*/

#ifdef _MSC_VER
#pragma warning(disable : 4100) // "unreferenced formal parameter"
#endif

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnon-virtual-dtor"
#endif

//===== mp_sdk_gui2.h =====
#ifndef GMPI_SDK_GUI2_H_INCLUDED
#define GMPI_SDK_GUI2_H_INCLUDED

#include <functional>
#include <codecvt>
//#include "mp_sdk_gui.h"
#include "MpString.h"
#include "Drawing.h"

// Classes required to build a GUI module.

// First the API.
namespace gmpi_gui_api
{
	using GmpiDrawing_API::MP1_POINT;
	using GmpiDrawing_API::MP1_RECT;
	using GmpiDrawing_API::MP1_SIZE;

	enum GG_POINTER_FLAGS {
		GG_POINTER_FLAG_NONE = 0,
		GG_POINTER_FLAG_NEW = 0x01,					// Indicates the arrival of a new pointer.
		GG_POINTER_FLAG_INCONTACT = 0x04,
		GG_POINTER_FLAG_FIRSTBUTTON = 0x10,
		GG_POINTER_FLAG_SECONDBUTTON = 0x20,
		GG_POINTER_FLAG_THIRDBUTTON = 0x40,
		GG_POINTER_FLAG_FOURTHBUTTON = 0x80,
		GG_POINTER_FLAG_CONFIDENCE	= 0x00000400,	// Confidence is a suggestion from the source device about whether the pointer represents an intended or accidental interaction.
		GG_POINTER_FLAG_PRIMARY		= 0x00002000,	// First pointer to contact surface. Mouse is usually Primary.

		GG_POINTER_SCROLL_HORIZ		= 0x00008000,	// Mouse Wheel is scrolling horizontal.

		GG_POINTER_KEY_SHIFT		= 0x00010000,	// Modifer key - <SHIFT>.
		GG_POINTER_KEY_CONTROL		= 0x00020000,	// Modifer key - <CTRL> or <Command>.
		GG_POINTER_KEY_ALT			= 0x00040000,	// Modifer key - <ALT> or <Option>.
	};


	// Interfaces
	class DECLSPEC_NOVTABLE IMpGraphics : public gmpi::IMpUnknown
	{
	public:
		// First pass of layout update. Return minimum size required for given available size
		virtual int32_t MP_STDCALL measure(MP1_SIZE availableSize, MP1_SIZE* returnDesiredSize) = 0;

		// Second pass of layout.
		// TODO: should all rects be passed as pointers (for speed and consistency w D2D). !!!
		virtual int32_t MP_STDCALL arrange(MP1_RECT finalRect) = 0; // TODO const, and reference maybe?

		virtual int32_t MP_STDCALL OnRender(GmpiDrawing_API::IMpDeviceContext* drawingContext) = 0;

		// Mouse input events return MP_OK or MP_UNHANDLED to indicate "hit" or not.
		virtual int32_t MP_STDCALL onPointerDown(int32_t flags, MP1_POINT point) = 0;

		virtual int32_t MP_STDCALL onPointerMove(int32_t flags, MP1_POINT point) = 0;

		virtual int32_t MP_STDCALL onPointerUp(int32_t flags, MP1_POINT point) = 0;

		/*
		virtual int32_t MP_STDCALL onPointerVwheel( UINT flags, MP1_POINT point ) = 0;

		virtual int32_t MP_STDCALL onPointerHwheel( UINT flags, MP1_POINT point ) = 0;

		virtual int32_t MP_STDCALL openWindow( void ) = 0;

		virtual int32_t MP_STDCALL closeWindow( void ) = 0;
		*/
	};

	// GUID for IMpGraphics
	// {4540C6EE-98AB-4C79-B857-66AE64249125}
	static const gmpi::MpGuid SE_IID_GRAPHICS_MPGUI =
	{ 0x4540c6ee, 0x98ab, 0x4c79,{ 0xb8, 0x57, 0x66, 0xae, 0x64, 0x24, 0x91, 0x25 }};

	class DECLSPEC_NOVTABLE IMpGraphics2 : public IMpGraphics
	{
	public:
		virtual int32_t MP_STDCALL hitTest(MP1_POINT point) = 0; // TODO!!! include mouse flags (for Patch Cables)
		virtual int32_t MP_STDCALL getToolTip(MP1_POINT point, gmpi::IString *returnString) = 0;
	};

	// GUID for IMpGraphics2
	// {6AEF51B9-C92A-472B-A53E-4A476DF85C85}
	static const gmpi::MpGuid SE_IID_GRAPHICS_MPGUI2 =
	{ 0x6aef51b9, 0xc92a, 0x472b,{ 0xa5, 0x3e, 0x4a, 0x47, 0x6d, 0xf8, 0x5c, 0x85 } };

	class DECLSPEC_NOVTABLE IMpGraphics3 : public IMpGraphics2
	{
	public:
		virtual int32_t MP_STDCALL hitTest2(int32_t flags, MP1_POINT point) = 0;
		virtual int32_t MP_STDCALL onMouseWheel(int32_t flags, int32_t delta, MP1_POINT point) = 0;
		virtual int32_t MP_STDCALL setHover(bool isMouseOverMe) = 0;

		// {CE4448E5-5DBC-426A-A963-E8CE0E2C2533}
		inline static const gmpi::MpGuid guid =
		{ 0xce4448e5, 0x5dbc, 0x426a, { 0xa9, 0x63, 0xe8, 0xce, 0xe, 0x2c, 0x25, 0x33 } };
	};

	// GUID for IMpGraphics3
	// {CE4448E5-5DBC-426A-A963-E8CE0E2C2533}
	static const gmpi::MpGuid SE_IID_GRAPHICS_MPGUI3 =
	{ 0xce4448e5, 0x5dbc, 0x426a, { 0xa9, 0x63, 0xe8, 0xce, 0xe, 0x2c, 0x25, 0x33 } };

	class IMpKeyClient : public gmpi::IMpUnknown
	{
	public:
		virtual int32_t MP_STDCALL OnKeyPress(wchar_t c) = 0;

		// {4A054EB8-6693-4B89-8C2B-408644483FFD}
		inline static const gmpi::MpGuid guid =
		{ 0x4a054eb8, 0x6693, 0x4b89, { 0x8c, 0x2b, 0x40, 0x86, 0x44, 0x48, 0x3f, 0xfd } };
	};

}

// SDK (implementation of the API).
namespace gmpi_gui
{
	using gmpi_gui_api::GG_POINTER_FLAGS;
	using gmpi_gui_api::GG_POINTER_FLAG_FIRSTBUTTON;
	using GmpiDrawing_API::MP1_POINT;
	using GmpiDrawing_API::MP1_RECT;
	using GmpiDrawing_API::MP1_SIZE;

	// Standard Platform UI.
	class DECLSPEC_NOVTABLE ICompletionCallback : public gmpi::IMpUnknown
	{
	public:
		virtual void MP_STDCALL OnComplete(int32_t result) = 0;
	};
	// GUID
	// {709582BA-AF65-43E6-A24C-AB05F8D6980B}
	static const gmpi::MpGuid SE_IID_COMPLETION_CALLBACK =
	{ 0x709582ba, 0xaf65, 0x43e6,{ 0xa2, 0x4c, 0xab, 0x5, 0xf8, 0xd6, 0x98, 0xb } };

	enum { MP_PLATFORM_MENU_GRAYED = 1,
		MP_PLATFORM_MENU_BREAK = 2,
		MP_PLATFORM_MENU_TICKED = 4,
		MP_PLATFORM_MENU_SEPARATOR = 8,
		MP_PLATFORM_SUB_MENU_BEGIN = 16,
		MP_PLATFORM_SUB_MENU_END = 32,
	};

	class DECLSPEC_NOVTABLE IMpPlatformMenu : public gmpi::IMpUnknown
	{
	public:
		virtual int32_t MP_STDCALL AddItem(const char* text, int32_t id, int32_t flags) = 0;
		virtual int32_t MP_STDCALL SetAlignment(int32_t alignment) = 0;
		virtual int32_t MP_STDCALL ShowAsync(gmpi_gui::ICompletionCallback* returnCompletionHandler) = 0;
		virtual int32_t MP_STDCALL GetSelectedId() = 0;
	};

	// GUID for IMpPlatformMenu
	// {19AE8501-F207-43AC-A139-D0F84E119E4C}
	static const gmpi::MpGuid SE_IID_GRAPHICS_PLATFORM_MENU =
	{ 0x19ae8501, 0xf207, 0x43ac,{ 0xa1, 0x39, 0xd0, 0xf8, 0x4e, 0x11, 0x9e, 0x4c } };

	enum {
		MP_FILE_DIALOG_TYPE_OPEN = 0,
		MP_FILE_DIALOG_TYPE_SAVE = 1,
	};

	// FILE OPEN/SAVE DIALOG.
	class DECLSPEC_NOVTABLE IMpFileDialog : public gmpi::IMpUnknown
	{
	public:
		virtual int32_t MP_STDCALL AddExtension(const char* extension, const char* description = "") = 0;
		virtual int32_t MP_STDCALL SetInitialFilename(const char* text) = 0;
		virtual int32_t MP_STDCALL setInitialDirectory(const char* text) = 0;
//		virtual int32_t MP_STDCALL Show(IMpUnknown* returnString) = 0;
		virtual int32_t MP_STDCALL ShowAsync(gmpi_gui::ICompletionCallback* returnCompletionHandler) = 0;
		virtual int32_t MP_STDCALL GetSelectedFilename(IMpUnknown* returnString) = 0;
	};

	// GUID for IMpFileDialog
	// {E795D844-4709-479E-9CDF-71D0516CF353}
	static const gmpi::MpGuid SE_IID_GRAPHICS_PLATFORM_FILE_DIALOG =
	{ 0xe795d844, 0x4709, 0x479e,{ 0x9c, 0xdf, 0x71, 0xd0, 0x51, 0x6c, 0xf3, 0x53 } };

	// OK/CANCEL DIALOG.
	class DECLSPEC_NOVTABLE IMpOkCancelDialog : public gmpi::IMpUnknown
	{
	public:
		virtual int32_t MP_STDCALL SetTitle(const char* text) = 0;
		virtual int32_t MP_STDCALL SetText(const char* text) = 0;
		virtual int32_t MP_STDCALL ShowAsync(gmpi_gui::ICompletionCallback* returnCompletionHandler) = 0;
	};

	// GUID for IMpOkCancelDialog
	// {4C0D0BFE-CF8E-4C8B-90CC-3025C269285A}
	static const gmpi::MpGuid SE_IID_GRAPHICS_OK_CANCEL_DIALOG =
	{ 0x4c0d0bfe, 0xcf8e, 0x4c8b,{ 0x90, 0xcc, 0x30, 0x25, 0xc2, 0x69, 0x28, 0x5a } };


	// TEXT ENTRY
	class DECLSPEC_NOVTABLE IMpPlatformText : public gmpi::IMpUnknown
	{
	public:
		virtual int32_t MP_STDCALL SetText(const char* text) = 0;
		virtual int32_t MP_STDCALL GetText(IMpUnknown* returnString) = 0;
		virtual int32_t MP_STDCALL ShowAsync(gmpi_gui::ICompletionCallback* returnCompletionHandler) = 0;
		virtual int32_t MP_STDCALL SetAlignment(int32_t alignment) = 0;
		virtual int32_t MP_STDCALL SetTextSize(float height) = 0;
	};

	// GUID for gmpi_gui::IMpPlatformText
	// {A37A41CB-D731-4557-A717-A1EAD5442BE8}
	static const gmpi::MpGuid SE_IID_GRAPHICS_PLATFORM_TEXT =
	{ 0xa37a41cb, 0xd731, 0x4557,{ 0xa7, 0x17, 0xa1, 0xea, 0xd5, 0x44, 0x2b, 0xe8 } };

	// Meant to be used by non-graphical GUI modules?
	class IMpGraphicsHostBase : public gmpi::IMpUnknown
	{
	public:
		virtual int32_t MP_STDCALL createFileDialog(int32_t dialogType, gmpi_gui::IMpFileDialog** returnFileDialog) = 0;
	};

	// GUID for IMpGraphicsHostBase
	// {E4717EA4-CA6D-4FAE-9D35-BA95EBAAEA2B}
	static const gmpi::MpGuid SE_IID_GRAPHICS_HOST_BASE =
	{ 0xe4717ea4, 0xca6d, 0x4fae,{ 0x9d, 0x35, 0xba, 0x95, 0xeb, 0xaa, 0xea, 0x2b } };


	// GMPI graphics API Host-side.
	// TODO: don't return IUnknown !!! return actual interface, versions of factory and products should be locked together like D2D.
    
    // GUID for IMpGraphicsHost
    // {E394AF51-C4D0-4885-84F1-963CA8182964}
    static const gmpi::MpGuid SE_IID_GRAPHICS_HOST =
    { 0xe394af51, 0xc4d0, 0x4885,{ 0x84, 0xf1, 0x96, 0x3c, 0xa8, 0x18, 0x29, 0x64 } };


	// TODO seperate drawing and mouse interaction.
	class IMpGraphicsHost : public IMpGraphicsHostBase
	{
	public:
		virtual int32_t MP_STDCALL GetDrawingFactory(GmpiDrawing_API::IMpFactory** returnFactory) = 0;

		// TODO: sort out methd name case.
		// Get host's current skin's font information.
		virtual void MP_STDCALL invalidateRect(const GmpiDrawing_API::MP1_RECT* invalidRect) = 0;
		virtual void MP_STDCALL invalidateMeasure() = 0;

		virtual int32_t MP_STDCALL setCapture() = 0;
		virtual int32_t MP_STDCALL getCapture(int32_t& returnValue) = 0;
		virtual int32_t MP_STDCALL releaseCapture() = 0;

		virtual int32_t MP_STDCALL createPlatformMenu(/* shouldbe const */ GmpiDrawing_API::MP1_RECT* rect, gmpi_gui::IMpPlatformMenu** returnMenu) = 0;
		virtual int32_t MP_STDCALL createPlatformTextEdit(/* shouldbe const */ GmpiDrawing_API::MP1_RECT* rect, gmpi_gui::IMpPlatformText** returnTextEdit) = 0;
		// Ideally this would be in IMpGraphicsHostBase, but doing so would break ABI for existing modules.
		virtual int32_t MP_STDCALL createOkCancelDialog(int32_t dialogType, gmpi_gui::IMpOkCancelDialog** returnDialog) = 0;

        static gmpi::MpGuid IID(){ return SE_IID_GRAPHICS_HOST; };
    };

#if 0
	class MpGuiInvisibleBase :
		public MpGuiBase2
	{
		GMPI_REFCOUNT;
	};


	class MpGuiGfxBase :
		public MpGuiBase2, public gmpi_gui_api::IMpGraphics3
	{
		using MpGuiBase2::getToolTip; // silence compiler warning. Allows user to call deprecated version of 'getToolTip'.

	public:
#ifdef _DEBUG
		MpGuiGfxBase() :
		 debug_IsMeasured(false)
		, debug_IsArranged(false)
		{}

		bool debug_IsMeasured;
		bool debug_IsArranged;
#endif

		// IMpGraphics methods
		int32_t measure(GmpiDrawing_API::MP1_SIZE availableSize, GmpiDrawing_API::MP1_SIZE* returnDesiredSize) override
		{
#ifdef _DEBUG
			debug_IsMeasured = true;
#endif
			const float minSize = 1;

			returnDesiredSize->width = (std::max)(minSize, availableSize.width);
			returnDesiredSize->height = (std::max)(minSize, availableSize.height);

			return gmpi::MP_OK;
		}

		int32_t arrange(GmpiDrawing_API::MP1_RECT finalRect) override
		{
			if (rect_ != finalRect)
			{
				rect_ = finalRect;
				invalidateRect();
			}
#ifdef _DEBUG
			debug_IsArranged = true;
#endif
			return gmpi::MP_OK;
		}

		int32_t OnRender(GmpiDrawing_API::IMpDeviceContext* drawingContext) override
		{
			return gmpi::MP_OK;
		}

		// Doubles as hit-test. MP_OK = hit, MP_UNHANDLED = miss.
		// Default to MP_OK to allow user to select by clicking.
		int32_t onPointerDown(int32_t flags, MP1_POINT point) override
		{
			return hitTest(point);
		}

		int32_t onPointerMove(int32_t flags, MP1_POINT point) override
		{
			return hitTest(point);
		}

		int32_t onPointerUp(int32_t flags, MP1_POINT point) override
		{
			return hitTest(point);
		}

		// MP_OK = hit, MP_UNHANDLED/MP_FAIL = miss.
		// Default to MP_OK to allow user to select by clicking.
		// point will always be within bounding rect.
		int32_t hitTest(MP1_POINT point) override
		{
			return gmpi::MP_OK;
		}
		
		int32_t getToolTip(MP1_POINT point, gmpi::IString* returnString) override
		{
			return gmpi::MP_UNHANDLED;
		}

		// IMpGraphics3 interface
		int32_t hitTest2(int32_t flags, MP1_POINT point) override
		{
			return hitTest(point);
		}

		int32_t onMouseWheel(int32_t flags, int32_t delta, MP1_POINT point) override
		{
			return gmpi::MP_UNHANDLED;
		}

		int32_t setHover(bool isMouseOverMe) override
		{
			return gmpi::MP_UNHANDLED;
		}

		void setCapture()
		{
			getGuiHost()->setCapture();
		}
		void releaseCapture()
		{
			getGuiHost()->releaseCapture();
		}
		bool getCapture()
		{
			int32_t c{};
			getGuiHost()->getCapture(c);
			return c != 0;
		}
		GmpiDrawing::Rect getRect() { return rect_; };

		// Simplified host access.
		void invalidateRect(const MP1_RECT* invalidRect = 0)
		{
			guiHost_->invalidateRect(invalidRect);
		}
		void invalidateMeasure()
		{
			guiHost_->invalidateMeasure();
		}
		int32_t setHost(gmpi::IMpUnknown* host) override
		{
			host->queryInterface(gmpi_gui::SE_IID_GRAPHICS_HOST, guiHost_.asIMpUnknownPtr());
			return MpGuiBase2::setHost(host);
		}

		gmpi_gui::IMpGraphicsHost* getGuiHost() { return guiHost_; };

		// !!! TODO: All host calls to have 'easy' wrapped versions !!!
		// calling host.
		std::string FindResourceU(const char* resourceNamePtr, const char* resourceTypePtr = "")
		{
			std::string resourceType(resourceTypePtr);

			if (resourceType.empty())
			{
				std::string resourceName(resourceNamePtr);

				auto p = resourceName.find_last_of('.');
				if (p != std::string::npos)
				{
					auto extension = resourceName.substr(p);
					if (extension == ".wav")
					{
						resourceType = "Audio";
					}
					else
					{
						if (extension == ".txt")
						{
							resourceType = "ImageMeta";
						}
						else
						{
							if (extension == ".mid")
							{
								resourceType = "MIDI";
							}
							else
							{
								resourceType = "Image";
							}
						}
					}
				}
			}

			gmpi_sdk::MpString returnString;
			/*auto r =*/ getHost()->FindResourceU(resourceNamePtr, resourceType.c_str(), &returnString);
			return returnString.getData();
		}

		inline std::string FindResourceU(std::string resourceName, std::string resourceType = "")
		{
			return FindResourceU(resourceName.c_str(), resourceType.c_str());
		}

		inline int32_t getHandle()
		{
			int32_t handle = -1;
			getHost()->getHandle(handle);
			return handle;
		}

		inline GmpiDrawing::Factory GetGraphicsFactory()
		{
			GmpiDrawing::Factory temp;
			getGuiHost()->GetDrawingFactory(temp.GetAddressOf());
			return temp;
		}

		int32_t queryInterface(const gmpi::MpGuid& iid, void** returnInterface) override
		{
			*returnInterface = nullptr;

			if (iid == gmpi_gui_api::SE_IID_GRAPHICS_MPGUI3)
			{
				*returnInterface = static_cast<IMpGraphics3*>(this);
				addRef();
				return gmpi::MP_OK;
			}

			if (iid == gmpi_gui_api::SE_IID_GRAPHICS_MPGUI2)
			{
				*returnInterface = static_cast<IMpGraphics2*>(this);
				addRef();
				return gmpi::MP_OK;
			}

			if (iid == gmpi_gui_api::SE_IID_GRAPHICS_MPGUI)
			{
				*returnInterface = static_cast<IMpGraphics*>(this);
				addRef();
				return gmpi::MP_OK;
			}

			return MpGuiBase2::queryInterface(iid, returnInterface);
		}
		GMPI_REFCOUNT;

	private:
		gmpi_sdk::mp_shared_ptr<gmpi_gui::IMpGraphicsHost> guiHost_;
		GmpiDrawing::Rect rect_;
	};
#endif

	class MpCompletionHandler : public gmpi_gui::ICompletionCallback
	{
		std::function<void(int32_t)> callback;

	public:
		MpCompletionHandler(std::function<void(int32_t)> pcallback = nullptr) :
			callback(pcallback)
		{
		}
        virtual ~MpCompletionHandler(){} // satisfy GCC.

		// ICompletionCallback
		void OnComplete(int32_t result) override
		{
			callback(result);
		}

		GMPI_QUERYINTERFACE1(gmpi_gui::SE_IID_COMPLETION_CALLBACK, gmpi_gui::ICompletionCallback);
		GMPI_REFCOUNT;
	};
} //namespace

#if 0
namespace GmpiSdk
{
	class Controller : public gmpi::IMpController
	{
		gmpi_sdk::mp_shared_ptr<gmpi::IMpControllerHost> host;
		int32_t handle = -1;

	public:
		virtual ~Controller(){}

		// Establish connection to host.
		int32_t setHost(gmpi::IMpUnknown* phost) override
		{
			phost->queryInterface(gmpi::MP_IID_CONTROLLER_HOST, host.asIMpUnknownPtr());

			if (host.isNull())
			{
				return gmpi::MP_NOSUPPORT;
			}

			host->getHandle(handle);

			return gmpi::MP_OK;
		}

		// Pins defaults.
		int32_t setPinDefault(int32_t pinType, int32_t pinId, const char* defaultValue) override
		{
			return gmpi::MP_OK;
		}

		// IMpParameterObserver
		int32_t setParameter(int32_t parameterHandle, int32_t fieldId, int32_t voice, const void* data, int32_t size) override
		{
			return gmpi::MP_OK;
		}

		int32_t setPin(int32_t pinId, int32_t voice, int64_t size, const void* data) override { return gmpi::MP_OK; }
		int32_t notifyPin(int32_t pinId, int32_t voice) override { return gmpi::MP_OK; }
		int32_t onDelete() override { return gmpi::MP_OK; }
		int32_t preSaveState() override { return gmpi::MP_OK; }
		int32_t open() override { return gmpi::MP_OK; }


		gmpi::IMpControllerHost* getHost()
		{
			return host.get();
		}

		GMPI_REFCOUNT;
		GMPI_QUERYINTERFACE1(gmpi::MP_IID_CONTROLLER, gmpi::IMpController);
	};

} //namespace
#endif

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif // MP_GFX_SE_H_INCLUDED INCLUDED
