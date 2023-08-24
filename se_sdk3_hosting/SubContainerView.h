#pragma once
#include "./IViewChild.h"
#include "ModuleView.h"
#include "modules/se_sdk3/Drawing.h"

namespace SynthEdit2
{
	/*
	// This class is a ModuleView, except for support of Sub-Panel, and applying mouse offset.
	class SubContainerView :
		public ModuleViewPanel
	{
		GmpiDrawing_API::MP1_SIZE offset_;

	public:
		SubContainerView(Json::Value* context, ViewBase* pParent);

//		void RecalcBounds();

//		virtual GmpiDrawing_API::MP1_SIZE getOffsetToView();

//		virtual void OnRender(GmpiDrawing::Graphics& g) override;
		//virtual int32_t measure(GmpiDrawing_API::MP1_SIZE availableSize, GmpiDrawing_API::MP1_SIZE* returnDesiredSize) override;
		//virtual int32_t arrange(GmpiDrawing_API::MP1_RECT finalRect) override;
		//virtual int32_t onPointerDown(int32_t flags, GmpiDrawing_API::MP1_POINT point) override;
		//virtual int32_t onPointerMove(int32_t flags, GmpiDrawing_API::MP1_POINT point) override;
		//virtual int32_t onPointerUp(int32_t flags, GmpiDrawing_API::MP1_POINT point) override;
//		virtual int32_t populateContextMenu(GmpiDrawing_API::MP1_POINT point, GmpiGuiHosting::ContextItemsSink2* contextMenuItemsSink) override;

//		virtual void OnMoved(GmpiDrawing::Rect& newRect) override;
//		virtual void MP_STDCALL invalidateRect(const GmpiDrawing_API::MP1_RECT* invalidRect) override;

	};
	*/

} // namespace

