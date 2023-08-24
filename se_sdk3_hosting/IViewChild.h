#pragma once

#include "IModelBase.h"
#include "modules/shared/xplatform.h"
#include "modules/se_sdk3/Drawing_API.h"
#include "modules/se_sdk3/Drawing.h"
#include "modules/se_sdk3_hosting/gmpi_gui_hosting.h"

namespace SynthEdit2
{
	enum class CableType { PatchCable, StructureCable };

	// Children of a view inherit from this class. They may forward these calls to an SDK module.
	// This class needs to handle to translation of coordinates for mouse and drawing to/from the SDK module.
	class IViewChild
	{
	public:
		virtual ~IViewChild() {}

		virtual void setHover(bool)
		{}

		// Similar to IMpGraphics for convenience. But don't confuse that with compatible or interchangeable.
		virtual int32_t measure(GmpiDrawing::Size availableSize, GmpiDrawing::Size* returnDesiredSize) = 0;
		virtual int32_t arrange(GmpiDrawing::Rect finalRect) = 0;

		virtual GmpiDrawing::Rect getLayoutRect() = 0;
		virtual GmpiDrawing::Rect GetClipRect() = 0;
		virtual void OnRender(GmpiDrawing::Graphics& g) = 0;
		virtual bool hitTest(int32_t flags, GmpiDrawing_API::MP1_POINT point) = 0;
		virtual bool hitTest(int32_t flags, GmpiDrawing_API::MP1_RECT rect) = 0;
		virtual int32_t onPointerDown(int32_t flags, GmpiDrawing_API::MP1_POINT point) = 0;
		virtual int32_t onPointerMove(int32_t flags, GmpiDrawing_API::MP1_POINT point) = 0;
		virtual int32_t onPointerUp(int32_t flags, GmpiDrawing_API::MP1_POINT point) = 0;
		virtual int32_t onMouseWheel(int32_t flags, int32_t delta, GmpiDrawing_API::MP1_POINT point) = 0;
		virtual int32_t populateContextMenu(GmpiDrawing_API::MP1_POINT point, GmpiGuiHosting::ContextItemsSink2* contextMenuItemsSink) = 0;
		virtual int32_t onContextMenu(int32_t idx) = 0;
		virtual std::string getToolTip(GmpiDrawing_API::MP1_POINT point) = 0;
		virtual void receiveMessageFromAudio(void*) = 0;

		// Additions
		virtual int32_t getModuleHandle() = 0;
		virtual bool getSelected() = 0;
		virtual void setSelected(bool selected) = 0;
		virtual void OnMoved(GmpiDrawing::Rect& newRect) = 0;
		virtual void OnNodesMoved(std::vector<GmpiDrawing::Point>& newNodes) = 0;
		virtual bool isVisable() // indicates if module has a visible graphical component.
		{
			return true;
		}
		virtual bool isShown() // Indicates if module should be drawn or not (because of 'Show on Parent' state).
		{
			return true;
		}
		virtual bool isDraggable(bool editEnabled)
		{
			// default is that anything can be dragged in the editor.
			return editEnabled;
		}
		virtual void OnCableDrag(class ConnectorViewBase* dragline) // i.e. during cable being dragged around.
		{
		}
		virtual bool EndCableDrag(GmpiDrawing_API::MP1_POINT point, class ConnectorViewBase* dragline)
		{
			return false;
		}
		virtual void OnCableDrag(ConnectorViewBase* dragline, GmpiDrawing::Point dragPoint, float& bestDistance, IViewChild*& bestModule, int& bestPinIndex)
		{
		}

		virtual GmpiDrawing::Point getConnectionPoint(CableType cableType, int pinIndex) = 0;
	};


	class ViewChild : public IViewChild
	{
		bool selected = {};
	public:
		GmpiDrawing::Rect bounds_; // should be RectL since only integer co-ords are valid.
		Json::Value* datacontext = {};
		int handle = -1;
		class ViewBase* parent = {};
		bool isOpen = {};

		ViewChild(class IModelBase* model, class ViewBase* pParent);
		ViewChild(Json::Value* pContext, ViewBase* pParent);

		ViewChild(ViewBase* pParent, int pHandle) :
			handle(pHandle)
			, datacontext(nullptr)
			, parent(pParent)
		{
		}

		// IViewChild
		virtual int32_t measure(GmpiDrawing::Size availableSize, GmpiDrawing::Size* returnDesiredSize) override { return gmpi::MP_OK; };
		int32_t arrange(GmpiDrawing::Rect finalRect) override
		{
			bounds_ = finalRect;
			return gmpi::MP_OK;
		}

		// bounds for the purpose of layout. May not include all drawn pixels.
		GmpiDrawing::Rect getLayoutRect() override
		{
			return bounds_;
		}

		// bounds for the purpose of invalidating every single drawn pixel, Including those outside layout boundary.
		GmpiDrawing::Rect GetClipRect() override
		{
			return bounds_;
		}

		int32_t getModuleHandle() override
		{
			return handle;
		}
		// IViewChild //////////////////////////////////////

		bool editEnabled();
		bool getSelected() override
		{
			return selected;
		}

		// Can't use pure passive view for selection because module may have captured mouse,
		// so we can't refresh by destroying and recreating entire view.
		void setSelected(bool pselected) override
		{
			selected = pselected;
		}

		bool hitTest(int32_t flags, GmpiDrawing_API::MP1_POINT point) override
		{
			return getLayoutRect().ContainsPoint(point);
		}
		bool hitTest(int32_t flags, GmpiDrawing_API::MP1_RECT selectionRect) override
		{
			return isOverlapped(GmpiDrawing::Rect(selectionRect), getLayoutRect());
		}

		std::string getToolTip(GmpiDrawing_API::MP1_POINT point) override
		{
			return std::string();
		}
		void receiveMessageFromAudio(void*) override
		{
		}

		class IPresenter* Presenter();
		virtual GmpiDrawing::Point getConnectionPoint(CableType cableType, int pinIndex) override
		{
			return GmpiDrawing::Point();
		}
	};

}
