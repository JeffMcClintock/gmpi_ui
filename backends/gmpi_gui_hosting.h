#pragma once

/*
#include "modules/se_sdk3_hosting/gmpi_gui_hosting.h"
using namespace GmpiGuiHosting;
*/

#include <vector>
#include <string>

#include "../se_sdk3/mp_sdk_gui2.h"
#include "../se_sdk3/mp_gui.h"
#include "../shared/unicode_conversion.h"

namespace GmpiGuiHosting
{
	class FastGamma
	{
		static float toFloat[256];
		static float toSRGB[256];

	public:
		inline static float sRGB_to_float(unsigned char pixel)
		{
			return toFloat[pixel];
		}

		inline static int float_to_sRGB(float intensity)
		{
			int i;

			if (intensity > toSRGB[128])
				i = 128;
			else
				i = 0;

			if (intensity > toSRGB[i + 64])
				i += 64;

			if (intensity > toSRGB[i + 32])
				i += 32;

			if (intensity > toSRGB[i + 16])
				i += 16;

			if (intensity > toSRGB[i + 8])
				i += 8;

			if (intensity > toSRGB[i + 4])
				i += 4;

			if (intensity > toSRGB[i + 2])
				i += 2;

			if (intensity > toSRGB[i + 1])
				i += 1;

			return i;
		}
	};

	/*
	class UpdateRegionDefault : public GmpiDrawing_API::IUpdateRegion
	{
		GmpiDrawing::Rect everything;
		GmpiDrawing_API::MP1_RECT* rects[2];

	public:
		UpdateRegionDefault() :
			everything(0.0f,0.f,30000.f,30000.f)
		{
			rects[0] = &everything;
			rects[1] = nullptr;
		}

		bool isVisible(GmpiDrawing_API::MP1_RECT* rect) override
		{
			return true;
		}
		int32_t MP_STDCALL getUpdateRects(GmpiDrawing_API::MP1_RECT*** rect) override
		{
			*rect = &(rects[0]);
			return gmpi::MP_OK;
		}

		GMPI_QUERYINTERFACE1(GmpiDrawing_API::SE_IID_UPDATE_REGION_MPGUI, GmpiDrawing_API::IUpdateRegion);
		GMPI_REFCOUNT_NO_DELETE;
	};
	*/

	/*
	class GgFactory : public GmpiDrawing_API::IMpFactory
	{
	public:
		static GgFactory* GetInstance();

		int32_t MP_STDCALL CreatePathGeometry(GmpiDrawing_API::IMpPathGeometry **pathGeometry) override;
		int32_t MP_STDCALL CreateTextFormat(const char* fontFamilyName, void* unused / * fontCollection * /, GmpiDrawing_API::MP1_FONT_WEIGHT fontWeight, GmpiDrawing_API::MP1_FONT_STYLE fontStyle, GmpiDrawing_API::MP1_FONT_STRETCH fontStretch, float fontSize, void* unused2 / * localeName * /, GmpiDrawing_API::IMpTextFormat** textFormat) override
		{
			assert(false);
			return gmpi::MP_FAIL;
		}
		int32_t MP_STDCALL CreateImage(int32_t width, int32_t height, GmpiDrawing_API::IMpBitmap** returnDiBitmap) override
		{
			assert(false);
			return gmpi::MP_FAIL;
		}
		int32_t MP_STDCALL LoadImageU(const char* utf8Uri, GmpiDrawing_API::IMpBitmap** returnDiBitmap) override
		{
			assert(false);
			return gmpi::MP_FAIL;
		}
		


		GMPI_QUERYINTERFACE1(GmpiDrawing_API::SE_IID_FACTORY_MPGUI, GmpiDrawing_API::IMpFactory);
		GMPI_REFCOUNT_NO_DELETE;
	};
	*/

	// Context Menus.
	struct menuInfo
	{
		std::string menuText;
		int32_t index;
		int32_t flags;

		menuInfo(const char* pMenuText, int32_t pIndex, int32_t pFlags) :
			menuText(pMenuText)
			, index(pIndex)
			, flags(pFlags)
		{}
	};

	class ContextItemsSink : public gmpi::IMpContextItemSink
	{
	public:
		std::vector<menuInfo> menuInfoList;

		int32_t MP_STDCALL AddItem(const char* text, int32_t id, int32_t flags = 0) override
		{
			menuInfoList.push_back(menuInfo(text, id, flags));
			return gmpi::MP_OK;
		}
		void Clear()
		{
			menuInfoList.clear();
		}
		GMPI_QUERYINTERFACE1(gmpi::MP_IID_CONTEXT_ITEMS_SINK, gmpi::IMpContextItemSink);
		GMPI_REFCOUNT_NO_DELETE;
	};
    
	class ContextItemsSink2 : public gmpi::IMpContextItemSink
	{
		struct menuInfo2
		{
			std::string menuText;
			int32_t index;
			int32_t flags;
			std::function<void(int32_t, GmpiDrawing_API::MP1_POINT)> callback;

			menuInfo2(const char* pMenuText, int32_t pIndex, int32_t pFlags, std::function<void(int32_t, GmpiDrawing_API::MP1_POINT)> pcallback) :
				menuText(pMenuText)
				, index(pIndex)
				, flags(pFlags)
				, callback(pcallback)
			{}
		};

		gmpi_gui::MpCompletionHandler onPopupMenuCompleteEvent;
		GmpiGui::PopupMenu nativeMenu;
		GmpiDrawing_API::MP1_POINT menuPosition;

	public:
		std::vector<menuInfo2> menuInfoList;
		std::function<int32_t(int32_t, GmpiDrawing_API::MP1_POINT)> currentCallback;

		ContextItemsSink2() :
			onPopupMenuCompleteEvent([this](int32_t result) -> void { this->OnPopupmenuComplete(result, menuPosition); })
		{
		}

		void Clear()
		{
			menuInfoList.clear();
		}

		void BeginSubMenu(const char* text)
		{
			menuInfoList.push_back(menuInfo2(text, 0, gmpi_gui::MP_PLATFORM_SUB_MENU_BEGIN, nullptr));
		}
		void EndSubMenu()
		{
			menuInfoList.push_back(menuInfo2("", 0, gmpi_gui::MP_PLATFORM_SUB_MENU_END, nullptr));
		}

		// Add separator, avoiding duplicates.
		void AddSeparator()
		{
			menuInfoList.push_back(menuInfo2("", 0, gmpi_gui::MP_PLATFORM_MENU_SEPARATOR, nullptr));
		}

		int32_t MP_STDCALL AddItem(const char* text, int32_t id, int32_t flags = 0) override
		{
			assert(currentCallback != nullptr); // got to set this first.

			menuInfoList.push_back(menuInfo2(text, id, flags, currentCallback));
			return gmpi::MP_OK;
		}

		int32_t AddItem(const char* text, std::function<void(int32_t, GmpiDrawing_API::MP1_POINT)> customCallback, int32_t flags = 0)
		{
			int id = menuInfoList.empty() ? 0 : menuInfoList.back().index + 1;

			menuInfoList.push_back(menuInfo2(text, id, flags, customCallback));

			return gmpi::MP_OK;
		}

		int32_t Populate(gmpi::IMpUserInterface2* object, GmpiDrawing_API::MP1_POINT point)
		{
			if (object) // some objects don't inherit IMpUserInterface2.
			{
				currentCallback = [object](int32_t idx, GmpiDrawing_API::MP1_POINT point) { return object->onContextMenu(idx); };
				return object->populateContextMenu(point.x, point.y, this);
			}
			return gmpi::MP_UNHANDLED;
		}

		void ActionResult(int32_t result, int32_t index)
		{
			if (result == gmpi::MP_OK && index >= 0 && index < static_cast<int32_t>(menuInfoList.size()))
			{
				menuInfoList[index].callback(menuInfoList[index].index, menuPosition);
			}
		}

		void ShowMenuAsync(gmpi_gui::IMpGraphicsHost* guihost, GmpiDrawing_API::MP1_POINT point)
		{
			//GmpiGui::GraphicsHost host(guihost); // can't create this as may get destructed by action on context menu.
			//auto menu = host.createPlatformMenu(point);

			GmpiDrawing::Rect rect(point.x, point.y, point.x + 120, point.y + 20);
			GmpiGui::PopupMenu menu;
			guihost->createPlatformMenu(&rect, menu.GetAddressOf());

			ShowMenuAsync2(menu, point);
		}

		void ShowMenuAsync2(GmpiGui::PopupMenu& menu, GmpiDrawing_API::MP1_POINT point)
		{
			nativeMenu = menu.Get();
			menuPosition = point;

			// remove trailing separators.
			while (!menuInfoList.empty() && (menuInfoList.back().flags & gmpi_gui::MP_PLATFORM_MENU_SEPARATOR) != 0)
			{
				menuInfoList.pop_back();
			}

			int idx = 0;
			bool previousIsSeparator = true; // suppress separator at start
			for (auto& m : menuInfoList)
			{
				bool isSeparator = (m.flags & gmpi_gui::MP_PLATFORM_MENU_SEPARATOR) != 0;

				// Suppress doubled-up separators.
				if(!isSeparator || !previousIsSeparator )
					menu.AddItem(m.menuText.c_str(), idx, m.flags );

				previousIsSeparator = isSeparator;
				++idx;
			}
			menu.ShowAsync(&onPopupMenuCompleteEvent);
		}

		void OnPopupmenuComplete(int32_t result, GmpiDrawing_API::MP1_POINT point)
		{
			ActionResult(result, nativeMenu.GetSelectedId());

			nativeMenu.setNull(); // release it.
		}
		
		GmpiDrawing_API::MP1_POINT getMenuPosition()
		{
			return menuPosition;
		}
		GMPI_QUERYINTERFACE1(gmpi::MP_IID_CONTEXT_ITEMS_SINK, gmpi::IMpContextItemSink);
		GMPI_REFCOUNT_NO_DELETE;
	};


#ifdef _WIN32

	// This code is for Win32 desktop apps

	class UpdateRegionWinGdi
	{
		HRGN hRegion = 0;
		std::string regionDataBuffer;
		std::vector<GmpiDrawing::RectL> rects;
		GmpiDrawing::RectL bounds;

	public:
		UpdateRegionWinGdi();
		~UpdateRegionWinGdi();

		void copyDirtyRects(HWND window, GmpiDrawing::SizeL swapChainSize);
		void optimizeRects();

		inline std::vector<GmpiDrawing::RectL>& getUpdateRects()
		{
			return rects;
		}
		inline GmpiDrawing::RectL& getBoundingRect()
		{
			return bounds;
		}
	};
/*
	// generic GMPI update region
	class UpdateRegion : public GmpiDrawing_API::IUpdateRegion
	{
		std::vector<GmpiDrawing::Rect> rects;
		GmpiDrawing::Rect overallRect;

	public:
		// Construct from Win32 rects, apply DPI and convert to float.
		UpdateRegion(const std::vector<GmpiDrawing::RectL>& rects_native, const GmpiDrawing::Matrix3x2& WindowToDips)
		{
			for (auto& r : rects_native)
			{
				auto r2 = WindowToDips.TransformRect(GmpiDrawing::Rect(static_cast<float>(r.left), static_cast<float>(r.top), static_cast<float>(r.right), static_cast<float>(r.bottom)));

				rects.push_back(
					GmpiDrawing::Rect(
						static_cast<float>(FastRealToIntTruncateTowardZero(r2.left)),
						static_cast<float>(FastRealToIntTruncateTowardZero(r2.top)),
						static_cast<float>(FastRealToIntTruncateTowardZero(r2.right) + 1),
						static_cast<float>(FastRealToIntTruncateTowardZero(r2.bottom) + 1)
				)
				);
			}

			assert(!rects.empty());
			overallRect = rects[0];
			for (int i = 1; i < rects.size(); ++i)
			{
				overallRect.Union(rects[i]);
			}
		}

		// Construct from parent region, but apply transform to child.
		UpdateRegion(const UpdateRegion* parent, const GmpiDrawing::Matrix3x2& transform)
		{
			for (auto& r : parent->rects)
			{
				rects.push_back( transform.TransformRect(r) );
			}

			overallRect = transform.TransformRect(parent->overallRect);
		}

		const GmpiDrawing::Rect& getOverallRect()
		{
			return overallRect;
		}

		int32_t MP_STDCALL getUpdateRects(const GmpiDrawing_API::MP1_RECT** returnRects) override
		{
			*returnRects = rects.data();
			return static_cast<int32_t>(rects.size());
		}

		bool MP_STDCALL isVisible(const GmpiDrawing_API::MP1_RECT* rect) override
		{
			for (auto& r : rects)
			{
				auto objectRect = *reinterpret_cast<const GmpiDrawing::Rect*>(rect);
				if (!GmpiDrawing::Intersect(r, objectRect).empty())
					return true;
			}

			return false;
		}

		GMPI_QUERYINTERFACE1(GmpiDrawing_API::SE_IID_UPDATE_REGION_MPGUI, GmpiDrawing_API::IUpdateRegion);
		GMPI_REFCOUNT;
	};
*/
	class PGCC_PlatformTextEntry : public gmpi_gui::IMpPlatformText
	{
		HWND hWndEdit;
		HWND parentWnd;
		int align;
		float dpiScale;
		float textHeight;
		GmpiDrawing::Rect editrect_s;

	public:
		std::string text_;
		bool multiline_ = false;

		PGCC_PlatformTextEntry(void* pParentWnd, GmpiDrawing_API::MP1_RECT* editrect, float dpi) : hWndEdit(0)
			, parentWnd( (HWND) pParentWnd )
			, align(TPM_LEFTALIGN)
			, dpiScale(dpi)
			, editrect_s(*editrect)
			,textHeight(12)
		{
		}

		int32_t MP_STDCALL SetText(const char* text) override
		{
			text_ = text;
			if( hWndEdit )
			{
				::SetWindowTextW(hWndEdit, JmUnicodeConversions::Utf8ToWstring(text_).c_str());
			}
			return gmpi::MP_OK;
		}

		int32_t MP_STDCALL GetText(IMpUnknown* returnString) override
		{
			gmpi::IString* returnValue = 0;

			if (gmpi::MP_OK != returnString->queryInterface(gmpi::MP_IID_RETURNSTRING, reinterpret_cast<void**>( &returnValue)))
			{
				return gmpi::MP_NOSUPPORT;
			}

			returnValue->setData(text_.data(), (int32_t) text_.size());
			return gmpi::MP_OK;
		}

		int32_t MP_STDCALL ShowAsync(gmpi_gui::ICompletionCallback* returnCompletionHandler) override;

		virtual int32_t MP_STDCALL SetAlignment(int32_t alignment)
		{
			align = (alignment & 0x03);
			multiline_ = (alignment >> 16) == 1;
			return gmpi::MP_OK;
		}

		int32_t getAlignment()
		{
			return align;
		}

		virtual int32_t MP_STDCALL SetTextSize(float height)
		{
			textHeight = height;
			return gmpi::MP_OK;
		}

		GMPI_QUERYINTERFACE1(gmpi_gui::SE_IID_GRAPHICS_PLATFORM_TEXT, gmpi_gui::IMpPlatformText);
		GMPI_REFCOUNT;
	};

	class PGCC_PlatformMenu : public gmpi_gui::IMpPlatformMenu
	{
		HMENU hmenu;
		std::vector<HMENU> hmenus;
		HWND parentWnd;
		int align;
		float dpiScale;
		GmpiDrawing::Rect editrect_s;
		int32_t selectedId;
		std::vector<int32_t> menuIds;

	public:
		// Might need to apply DPI to Text size, like text-entry does.
		PGCC_PlatformMenu(HWND pParentWnd, GmpiDrawing_API::MP1_RECT* editrect, float dpi = 1.0f) : hmenu(0)
			, parentWnd(pParentWnd)
			, align(TPM_LEFTALIGN)
			, dpiScale(dpi)
			, editrect_s(*editrect)
			, selectedId(-1)
		{
			hmenu = CreatePopupMenu();
			hmenus.push_back(hmenu);
		}

		~PGCC_PlatformMenu()
		{
			DestroyMenu(hmenu);
		}

		virtual int32_t MP_STDCALL AddItem(const char* text, int32_t id, int32_t flags)
		{
			UINT nativeFlags = MF_STRING;
			if ((flags & gmpi_gui::MP_PLATFORM_MENU_TICKED) != 0)
			{
				nativeFlags |= MF_CHECKED;
			}
			if ((flags & gmpi_gui::MP_PLATFORM_MENU_GRAYED) != 0)
			{
				nativeFlags |= MF_GRAYED;
			}
			if ((flags & gmpi_gui::MP_PLATFORM_MENU_SEPARATOR) != 0)
			{
				nativeFlags |= MF_SEPARATOR;
			}
			if ((flags & gmpi_gui::MP_PLATFORM_MENU_BREAK) != 0)
			{
				nativeFlags |= MF_MENUBREAK;
			}

			if ((flags & (gmpi_gui::MP_PLATFORM_SUB_MENU_BEGIN | gmpi_gui::MP_PLATFORM_SUB_MENU_END)) != 0)
			{
				if ((flags & gmpi_gui::MP_PLATFORM_SUB_MENU_BEGIN) != 0)
				{
					auto submenu = CreatePopupMenu();
					AppendMenu(hmenus.back(), nativeFlags | MF_POPUP, (UINT_PTR) submenu, JmUnicodeConversions::Utf8ToWstring(text).c_str());
					hmenus.push_back(submenu);
				}
				if ((flags & gmpi_gui::MP_PLATFORM_SUB_MENU_END) != 0)
				{
					hmenus.pop_back();
				}
			}
			else
			{
				menuIds.push_back(id);
				AppendMenu(hmenus.back(), nativeFlags, menuIds.size(), JmUnicodeConversions::Utf8ToWstring(text).c_str());
			}

			return gmpi::MP_OK;
		}

		int32_t MP_STDCALL ShowAsync(gmpi_gui::ICompletionCallback* returnCompletionHandler) override
		{
			POINT nativePoint;
			nativePoint.x = (int)editrect_s.left;
			nativePoint.y = (int)editrect_s.top;
			ClientToScreen(parentWnd, &nativePoint);

			int flags = align | TPM_LEFTBUTTON | TPM_NONOTIFY | TPM_RETURNCMD;

			auto index = TrackPopupMenu(hmenu, flags, nativePoint.x, nativePoint.y, 0, parentWnd, 0) - 1;

			if (index >= 0)
			{
				selectedId = menuIds[index];
			}
			else
			{
				selectedId = 0; // N/A
			}

			returnCompletionHandler->OnComplete(index >= 0 ? gmpi::MP_OK : gmpi::MP_CANCEL);

			return gmpi::MP_OK;
		}

		virtual int32_t MP_STDCALL SetAlignment(int32_t alignment)
		{
			switch (alignment)
			{
			case GmpiDrawing_API::MP1_TEXT_ALIGNMENT_LEADING:
				align = TPM_LEFTALIGN;
				break;
			case GmpiDrawing_API::MP1_TEXT_ALIGNMENT_CENTER:
				align = TPM_CENTERALIGN;
				break;
			case GmpiDrawing_API::MP1_TEXT_ALIGNMENT_TRAILING:
			default:
				align = TPM_RIGHTALIGN;
				break;
			}
			return gmpi::MP_OK;
		}

		int32_t MP_STDCALL GetSelectedId() override
		{
			return selectedId;
		}

		GMPI_QUERYINTERFACE1(gmpi_gui::SE_IID_GRAPHICS_PLATFORM_MENU, gmpi_gui::IMpPlatformMenu);
		GMPI_REFCOUNT;
	};

	class Gmpi_Win_FileDialog : public gmpi_gui::IMpFileDialog
	{
		HWND parentWnd;
		int32_t mode_;
		std::wstring initial_filename;
		std::wstring initial_folder;
		std::string selectedFilename;

	public:
		std::vector< std::pair< std::string, std::string> > extensions;

		Gmpi_Win_FileDialog( int32_t mode, HWND pParentWnd) :
			parentWnd(pParentWnd)
			, mode_(mode)
		{
		}

		int32_t MP_STDCALL AddExtension(const char* extension, const char* description ) override
		{
			std::string ext(extension);
			std::string desc(description);
			if( desc.empty() )
			{
				if( ext == "*" )
					desc = "All";
				else
					desc = ext;
				desc += " Files";
			}
			extensions.push_back(std::pair<std::string, std::string>(extension, desc));
			return gmpi::MP_OK;
		}
		int32_t MP_STDCALL SetInitialFilename(const char* text) override
		{
			initial_filename = JmUnicodeConversions::Utf8ToWstring( text );
			return gmpi::MP_OK;
		}
		int32_t MP_STDCALL setInitialDirectory(const char* text) override
		{
			initial_folder = JmUnicodeConversions::Utf8ToWstring(text);
			return gmpi::MP_OK;
		}

//		int32_t MP_STDCALL Show(IMpUnknown* returnString) override;
		int32_t MP_STDCALL ShowAsync(gmpi_gui::ICompletionCallback* returnCompletionHandler) override;
		int32_t MP_STDCALL GetSelectedFilename(IMpUnknown* returnString) override;

		GMPI_QUERYINTERFACE1(gmpi_gui::SE_IID_GRAPHICS_PLATFORM_FILE_DIALOG, gmpi_gui::IMpFileDialog);
		GMPI_REFCOUNT;
	};


	
	class Gmpi_Win_OkCancelDialog : public gmpi_gui::IMpOkCancelDialog
	{
		HWND parentWnd;
		int32_t mode_;
		std::wstring title;
		std::wstring text;

	public:
		Gmpi_Win_OkCancelDialog(int32_t mode, HWND pParentWnd) :
			parentWnd(pParentWnd)
			, mode_(mode)
		{
		}

		int32_t MP_STDCALL SetTitle(const char* ptext) override
		{
			title = JmUnicodeConversions::Utf8ToWstring(ptext);
			return gmpi::MP_OK;
		}
		int32_t MP_STDCALL SetText(const char* ptext) override
		{
			text = JmUnicodeConversions::Utf8ToWstring(ptext);
			return gmpi::MP_OK;
		}

		int32_t MP_STDCALL ShowAsync(gmpi_gui::ICompletionCallback* returnCompletionHandler) override;

		GMPI_QUERYINTERFACE1(gmpi_gui::SE_IID_GRAPHICS_OK_CANCEL_DIALOG, gmpi_gui::IMpOkCancelDialog);
		GMPI_REFCOUNT;
	};


#else

class PGCC_PlatformTextEntry : public gmpi_gui::IMpPlatformText
{
	void* parentWnd;
//	int align;
	float dpiScale;
	int offsetX;
	int offsetY;
	float textHeight;

public:
	std::string text_;

	PGCC_PlatformTextEntry(void* pParentWnd, int poffsetX, int poffsetY, float dpi) :
		 parentWnd( pParentWnd)
		, dpiScale(dpi)
		, offsetX(poffsetX)
		, offsetY(poffsetY)
		, textHeight(12)
	{
	}

	int32_t MP_STDCALL SetText(const char* text) override
	{
		text_ = text;
		//if( hWndEdit )
		//{
		//	::SetWindowTextW(hWndEdit, Utf8ToWstring(text_).c_str());
		//}
		return gmpi::MP_OK;
	}
    
    int32_t MP_STDCALL GetText(IMpUnknown* returnString) override
    {
        gmpi::IString* returnValue = 0;
        
        if (gmpi::MP_OK != returnString->queryInterface(gmpi::MP_IID_RETURNSTRING, reinterpret_cast<void**>( &returnValue)) )
        {
            return gmpi::MP_NOSUPPORT;
        }
        
        returnValue->setData(text_.data(), (int32_t) text_.size());
        return gmpi::MP_OK;
    }
    
//	virtual int32_t MP_STDCALL Show(float x, float y, float w, float h, IMpUnknown* returnString);
    int32_t MP_STDCALL ShowAsync(gmpi_gui::ICompletionCallback* returnCompletionHandler) override;

	void OnEditReturn()
	{
		//		::DestroyWindow(hWndEdit);
	}

	int32_t MP_STDCALL SetAlignment(int32_t alignment) override
	{/*
		switch( alignment )
		{
		case gmpi_gui::PopupMenu::HorizontalAlignment::A_Left:
			align = TPM_LEFTALIGN;
			break;
		case gmpi_gui::PopupMenu::HorizontalAlignment::A_Center:
			align = TPM_CENTERALIGN;
			break;
		case gmpi_gui::PopupMenu::HorizontalAlignment::A_Right:
		default:
			align = TPM_RIGHTALIGN;
			break;
		}
      */
		return gmpi::MP_OK;
	}

	int32_t MP_STDCALL SetTextSize(float height) override
	{
		textHeight = height;
		return gmpi::MP_OK;
	}

	GMPI_QUERYINTERFACE1(gmpi_gui::SE_IID_GRAPHICS_PLATFORM_TEXT, gmpi_gui::IMpPlatformText);
	GMPI_REFCOUNT;
};
#endif

} // namespace.
