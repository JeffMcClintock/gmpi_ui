#pragma once
#include "./IViewChild.h"
#include "modules/se_sdk3/Drawing_API.h"
#include "modules/se_sdk3/Drawing.h"

namespace SynthEdit2
{
	/*
	class DragLine : public IViewChild
	{
	public:
		GmpiDrawing::Point startPos;
		GmpiDrawing::Point mousePos;
		class ViewBase* parent;
		int32_t fromModule;
		int fromPin;

		DragLine(class ViewBase* pparent, int32_t pfromModule, int pFromPin, GmpiDrawing_API::MP1_POINT fromPoint);

		// IViewChild
		//virtual int32_t MP_STDCALL populateContextMenu(GmpiDrawing_API::MP1_POINT point, GmpiGuiHosting::ContextItemsSink2* contextMenuItemsSink) override { return 0; };

		// IMpGraphics ----------------------------------
		virtual int32_t measure(GmpiDrawing::Size availableSize, GmpiDrawing::Size* returnDesiredSize) override { return 0; }
		virtual int32_t arrange(GmpiDrawing::Rect finalRect) override { return 0; }
		virtual void OnRender(GmpiDrawing::Graphics& g) override
		{
			auto brush = g.CreateSolidColorBrush(GmpiDrawing::Color::Orange);
			g.DrawLine(startPos, mousePos, brush, 1);
		}
		virtual bool hitTest(GmpiDrawing_API::MP1_POINT point) override
		{
			return false; // N/A
		}
		virtual int32_t onPointerDown(int32_t flags, gmpi_gui_api::MP1_POINT point) override { return 0; }
		virtual int32_t onPointerMove(int32_t flags, gmpi_gui_api::MP1_POINT point) override;
		virtual int32_t onPointerUp(int32_t flags, gmpi_gui_api::MP1_POINT point) override;
		virtual int32_t populateContextMenu(GmpiDrawing_API::MP1_POINT point, GmpiGuiHosting::ContextItemsSink2* contextMenuItemsSink) override { return 0; }
		virtual int32_t onContextMenu(int32_t idx) override
		{
			return gmpi::MP_UNHANDLED;
		}
		virtual GmpiDrawing::Rect getLayoutRect() override
		{
			return GmpiDrawing::Rect(0,0,100,100);
		}
		virtual int32_t getModuleHandle() override
		{
			return -1;
		}
		virtual void OnMoved(GmpiDrawing::Rect& newRect) {}
		virtual bool getSelected() override
		{
			return false;
		}
	};
	*/
}