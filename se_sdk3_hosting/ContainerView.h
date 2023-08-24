#pragma once
#include <vector>
#include <memory>
#include "ViewBase.h"
#include "./DrawingFrame_win32.h"
#include "../se_sdk3_hosting/GraphicsRedrawClient.h"

class IGuiHost2;
namespace SynthEdit2
{
	class IPresenter;
}

namespace SynthEdit2
{
	// The one top-level view.
	class ContainerView : public ViewBase, public IGraphicsRedrawClient, public gmpi_gui_api::IMpKeyClient
	{
		std::string skinName_;

	public:
		ContainerView(GmpiDrawing::Size size);
		~ContainerView();

		void setDocument(SynthEdit2::IPresenter* presenter, int pviewType);

		int GetViewType()
		{
			return viewType;
		}

		IViewChild* Find(GmpiDrawing::Point& p);
		void Unload();
		virtual std::string getSkinName() override
		{
			if( viewType == CF_PANEL_VIEW )
				return skinName_;
			else
				return "default2";
		}

		virtual void Refresh(Json::Value* context, std::map<int, SynthEdit2::ModuleView*>& guiObjectMap_) override;

		// Inherited via IMpGraphics
		virtual int32_t MP_STDCALL OnRender(GmpiDrawing_API::IMpDeviceContext* drawingContext) override;
		virtual int32_t MP_STDCALL onPointerDown(int32_t flags, GmpiDrawing_API::MP1_POINT point) override;

		GmpiDrawing::Point MapPointToView(ViewBase* parentView, GmpiDrawing::Point p) override
		{
			return p;
		}

		bool isShown() override
		{
			return true;
		}
		
		void OnPatchCablesVisibilityUpdate() override;

		void PreGraphicsRedraw() override;

		int32_t MP_STDCALL OnKeyPress(wchar_t c) override;

		int32_t MP_STDCALL queryInterface(const gmpi::MpGuid& iid, void** returnInterface) override
		{
			*returnInterface = nullptr;

			if (iid == IGraphicsRedrawClient::guid)
			{
				*returnInterface = static_cast<IGraphicsRedrawClient*>(this);
				addRef();
				return gmpi::MP_OK;
			}

			if (iid == gmpi_gui_api::IMpKeyClient::guid)
			{
				*returnInterface = static_cast<gmpi_gui_api::IMpKeyClient*>(this);
				addRef();
				return gmpi::MP_OK;
			}

			return ViewBase::queryInterface(iid, returnInterface);
		}
		GMPI_REFCOUNT;
	};
}
