/* Copyright (c) 2016 SynthEdit Ltd
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*     * Redistributions of source code must retain the above copyright
*       notice, this list of conditions and the following disclaimer.
*     * Redistributions in binary form must reproduce the above copyright
*       notice, this list of conditions and the following disclaimer in the
*       documentation and/or other materials provided with the distribution.
*     * Neither the name SynthEdit, nor SEM, nor 'Music Plugin Interface' nor the
*       names of its contributors may be used to endorse or promote products
*       derived from this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY SYNTHEDIT LTD ``AS IS'' AND ANY
* EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL SYNTHEDIT LTD BE LIABLE FOR ANY
* DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
* LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
* ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
* SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/*
#include "mp_gui.h"
using namespace GmpiGui;
*/

#ifndef MP_GUI_H_INCLUDED
#define MP_GUI_H_INCLUDED

#include <codecvt>
#include "mp_sdk_gui2.h" // TODO rename mp_gui_api.h
#include "mp_interface_wrapper.h"
#include "it_enum_list.h"
#include "../shared/string_utilities.h"

namespace GmpiGui
{
	enum
	{
		Menu_Grayed = gmpi_gui::MP_PLATFORM_MENU_GRAYED,
		Menu_Break = gmpi_gui::MP_PLATFORM_MENU_BREAK,
		Menu_Ticked = gmpi_gui::MP_PLATFORM_MENU_TICKED,
		Menu_Separator = gmpi_gui::MP_PLATFORM_MENU_SEPARATOR,
		Menu_SubMenuBegin = gmpi_gui::MP_PLATFORM_SUB_MENU_BEGIN,
		Menu_SubMenuEnd = gmpi_gui::MP_PLATFORM_SUB_MENU_END
	};

	class PopupMenu : public GmpiSdk::Internal::Object
	{
		gmpi_gui::MpCompletionHandler onDialogCompleteEvent;

	public:
		GMPIGUISDK_DEFINE_CLASS(PopupMenu, GmpiSdk::Internal::Object, gmpi_gui::IMpPlatformMenu);

		inline PopupMenu(gmpi::IMpUnknown* guiHost)
		{
			if (gmpi::MP_NOSUPPORT == guiHost->queryInterface(gmpi_gui::SE_IID_GRAPHICS_HOST, asIMpUnknownPtr()))
			{
				// throw?				return MP_NOSUPPORT;
			}
		}

		// see enum class MenuFlags.
		inline void AddItem(const char* text, int32_t id, int32_t flags = 0)
		{
			Get()->AddItem(text, id, flags);
		}
		inline void AddItem(std::wstring text, int32_t id, int32_t flags = 0)
		{
			std::wstring_convert<std::codecvt_utf8<wchar_t>> convert;

			AddItem(convert.to_bytes(text).c_str(), id, flags);
		}
		inline void ShowAsync(gmpi_gui::ICompletionCallback* returnCompletionHandler)
		{
			Get()->ShowAsync(returnCompletionHandler);
		}
		inline void ShowAsync(std::function<void(int32_t)> callback)
		{
			onDialogCompleteEvent = callback;
			Get()->ShowAsync(&onDialogCompleteEvent);
		}
		inline void SetAlignment(int32_t alignment)
		{
			Get()->SetAlignment(alignment);
		}
		inline void SetAlignment(GmpiDrawing::TextAlignment alignment)
		{
			Get()->SetAlignment((int32_t) alignment);
		}
		inline int32_t GetSelectedId()
		{
			return Get()->GetSelectedId();
		}
	};

	class TextEdit : public GmpiSdk::Internal::Object
	{
		gmpi_gui::MpCompletionHandler onDialogCompleteEvent;

	public:
		GMPIGUISDK_DEFINE_CLASS(TextEdit, GmpiSdk::Internal::Object, gmpi_gui::IMpPlatformText);

		inline TextEdit(gmpi::IMpUnknown* guiHost)
		{
			if (gmpi::MP_NOSUPPORT == guiHost->queryInterface(gmpi_gui::SE_IID_GRAPHICS_HOST, asIMpUnknownPtr()))
			{
				// throw?				return MP_NOSUPPORT;
			}
		}

		inline void ShowAsync(gmpi_gui::ICompletionCallback* returnCompletionHandler)
		{
			Get()->ShowAsync(returnCompletionHandler);
		}
		inline void ShowAsync(std::function<void(int32_t)> callback)
		{
			onDialogCompleteEvent = callback;
			Get()->ShowAsync(&onDialogCompleteEvent);
		}
		inline void SetAlignment(int32_t alignment)
		{
			Get()->SetAlignment(alignment);
		}
		inline void SetAlignment(GmpiDrawing::TextAlignment alignment, GmpiDrawing::WordWrapping wordWrapping = GmpiDrawing::WordWrapping::NoWrap)
		{
			const int32_t inverseWordWrapping = ((~(int32_t)wordWrapping) & 1) << 16;
			Get()->SetAlignment((int32_t)alignment | inverseWordWrapping);
		}
		inline void SetText(std::string text)
		{
			Get()->SetText(text.c_str());
		}
		inline std::string GetText()
		{
			gmpi_sdk::MpString s;
			Get()->GetText(s.getUnknown());
			return s.str();
		}
		inline void SetTextSize(float height)
		{
			Get()->SetTextSize(height);
		}
	};

	class FileDialog : public GmpiSdk::Internal::Object
	{
		gmpi_gui::MpCompletionHandler onDialogCompleteEvent;

	public:
		GMPIGUISDK_DEFINE_CLASS(FileDialog, GmpiSdk::Internal::Object, gmpi_gui::IMpFileDialog);

		/*  WTF ???
		inline FileDialog(gmpi::IMpUnknown* guiHost)
		{
			if (gmpi::MP_NOSUPPORT == guiHost->queryInterface(gmpi_gui::SE_IID_GRAPHICS_HOST, asIMpUnknownPtr()))
			{
				// throw?				return MP_NOSUPPORT;
			}
		}
		*/

		inline void ShowAsync(std::function<void(int32_t)> callback)
		{
			onDialogCompleteEvent = callback;
			Get()->ShowAsync(&onDialogCompleteEvent);
		}
		inline void AddExtension(const char* extension, const char* description = "")
		{
			Get()->AddExtension(extension, description);
		}
		inline void AddExtension(std::string extension, std::string description = "")
		{
			Get()->AddExtension(extension.c_str(), description.c_str());
		}
		inline void AddExtensionList(std::wstring extensions)
		{
			std::wstring_convert<std::codecvt_utf8<wchar_t>> convert;
			it_enum_list it2(extensions);

			if (it2.size() == 0)
			{
				AddExtension("*");
			}
			else
			{
				for (it2.First(); !it2.IsDone(); ++it2)
				{
					AddExtension(convert.to_bytes((*it2)->text), "");
				}
			}
		}
		// set just the filename. e.g. moose.wav
		inline void SetInitialFilename(const char* text)
		{
			Get()->SetInitialFilename(text);
		}
		inline void SetInitialFilename(std::string text)
		{
			Get()->SetInitialFilename(text.c_str());
		}
		// set just the folder. e.g. C:\samples
		inline void setInitialDirectory(const char* text)
		{
			Get()->setInitialDirectory(text);
		}
		inline void setInitialDirectory(std::string text)
		{
			Get()->setInitialDirectory(text.c_str());
		}

		// set filename and folder. e.g. C:\samples\moose.wav
		inline void SetInitialFullPath(std::string fullPath)
		{
			const auto nativePath = ToNativeSlashes(fullPath);
			SetInitialFilename(StripPath(nativePath));
			setInitialDirectory(StripFilename(nativePath));
		}

		inline std::string GetSelectedFilename()
		{
			gmpi_sdk::MpString s;
			Get()->GetSelectedFilename(s.getUnknown());
			return s.str();
		}
	};

	class OkCancelDialog : public GmpiSdk::Internal::Object
	{
		gmpi_gui::MpCompletionHandler onDialogCompleteEvent;

	public:
		GMPIGUISDK_DEFINE_CLASS(OkCancelDialog, GmpiSdk::Internal::Object, gmpi_gui::IMpOkCancelDialog);

		/*  WTF ???
		inline OkCancelDialog(gmpi::IMpUnknown* guiHost)
		{
			if (gmpi::MP_NOSUPPORT == guiHost->queryInterface(gmpi_gui::SE_IID_GRAPHICS_HOST, asIMpUnknownPtr()))
			{
				// throw?				return MP_NOSUPPORT;
			}
		}
		*/

		inline void ShowAsync(std::function<void(int32_t)> callback)
		{
			onDialogCompleteEvent = callback;
			Get()->ShowAsync(&onDialogCompleteEvent);
		}
		// set just the filename. e.g. moose.wav
		inline void SetTitle(const char* text)
		{
			Get()->SetTitle(text);
		}
		inline void SetText(std::string text)
		{
			Get()->SetText(text.c_str());
		}
	};

    class GraphicsHost : public GmpiSdk::Internal::GmpiIWrapper<gmpi_gui::IMpGraphicsHost> // GmpiSdk::Internal::Object
	{
	public:
//		GMPIGUISDK_DEFINE_CLASS(GraphicsHost, GmpiSdk::Internal::Object, gmpi_gui::IMpGraphicsHost);

        inline GraphicsHost(gmpi::IMpUnknown* guiHost) : GmpiSdk::Internal::GmpiIWrapper<gmpi_gui::IMpGraphicsHost>(guiHost)
		{
			if (gmpi::MP_NOSUPPORT == guiHost->queryInterface(gmpi_gui::SE_IID_GRAPHICS_HOST, asIMpUnknownPtr()))
			{
				// throw?				return MP_NOSUPPORT;
			}
		}

		inline GmpiDrawing::Factory GetDrawingFactory()
		{
			GmpiDrawing::Factory temp;
			Get()->GetDrawingFactory(temp.GetAddressOf());
			return temp;
		}

		// TODO: sort out methd name case.
		// Get host's current skin's font information.
		inline void invalidateRect(const GmpiDrawing_API::MP1_RECT* invalidRect)
		{
			Get()->invalidateRect(invalidRect);
		}
		inline void invalidateMeasure()
		{
			Get()->invalidateMeasure();
		}
		inline void setCapture()
		{
			/* auto r = */ Get()->setCapture();
		}
		inline bool getCapture()
		{
			int32_t returnValue;
			/* auto r = */ Get()->getCapture(returnValue);
			return returnValue != 0;
		}
		inline void releaseCapture()
		{
			/* auto r = */ Get()->releaseCapture();
		}

		inline PopupMenu createPlatformMenu(GmpiDrawing_API::MP1_RECT* rect)
		{
			PopupMenu temp;
			Get()->createPlatformMenu(rect, temp.GetAddressOf());
			return temp;
		}

		PopupMenu createPlatformMenu(GmpiDrawing_API::MP1_POINT point)
		{
			GmpiDrawing::Rect rect(point.x, point.y, point.x + 120, point.y + 20);
			PopupMenu temp;
			Get()->createPlatformMenu(&rect, temp.GetAddressOf());
			return temp;
		}

		TextEdit createPlatformTextEdit(/* const */ GmpiDrawing_API::MP1_RECT* rect)
		{
			TextEdit temp;
			Get()->createPlatformTextEdit(rect, temp.GetAddressOf());
			return temp;
		}

		TextEdit createPlatformTextEdit(GmpiDrawing::Rect rect)
		{
			TextEdit temp;
			Get()->createPlatformTextEdit(&rect, temp.GetAddressOf());
			return temp;
		}

		FileDialog createFileDialog(int32_t dialogType = 0) // 0 - Open, 1 - Save.
		{
			FileDialog temp;
			Get()->createFileDialog(dialogType, temp.GetAddressOf());
			return temp;
		}
	};

} // namespace

#endif // include guard.
