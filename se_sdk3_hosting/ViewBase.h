#pragma once
#include <vector>
#include <memory>
#include "IViewChild.h"
#include "modules/shared/xplatform.h"
#include "modules/se_sdk3/Drawing.h"
#include "../SE_DSP_CORE/UgDatabase.h"
#include "../SE_DSP_CORE/modules/se_sdk3/mp_gui.h"
#include "../SE_DSP_CORE/modules/se_sdk3_hosting/Presenter.h"

namespace GmpiGuiHosting
{
	class DrawingFrameBase;
	class ConnectorViewBase;
}

class IGuiHost2;

namespace SynthEdit2
{
	class DragLine;

	// Base of any view that displays modules. Itself behaving as a standard graphics module.
	class ViewBase : public gmpi_gui::MpGuiGfxBase
	{
		GmpiDrawing::Point pointPrev;
		GmpiGuiHosting::ContextItemsSink2 contextMenu;
		GmpiDrawing_API::MP1_POINT lastMovePoint = { -1, -1 };

	protected:
bool isIteratingChildren = false;
		bool isArranged = false;
		int viewType;
		std::vector< std::unique_ptr<IViewChild> > children;
		std::unique_ptr<IPresenter> presenter;

		GmpiDrawing::Rect drawingBounds;
		IViewChild* mouseCaptureObject;
		IViewChild* elementBeingDragged;
		IViewChild* mouseOverObject = {};

#ifdef _WIN32
		GmpiGuiHosting::DrawingFrameBase* frameWindow = {};
#endif
		class ModuleViewPanel* patchAutomatorWrapper_;

		void ConnectModules(const Json::Value& element, std::map<int, class ModuleView*>& guiObjectMap);// , ModuleView* patchAutomatorWrapper);
		class ModuleViewPanel* getPatchAutomator(std::map<int, class ModuleView*>& guiObjectMap);

	public:
		ViewBase();
		virtual ~ViewBase() {}

		virtual int32_t MP_STDCALL setHost(gmpi::IMpUnknown* host) override;

		void Init(class IPresenter* ppresentor);
		void BuildPatchCableNotifier(std::map<int, class ModuleView*>& guiObjectMap);
		void BuildModules(Json::Value* context, std::map<int, class ModuleView*>& guiObjectMap); // , ModuleView* patchAutomatorWrapper);

		virtual void Refresh(Json::Value* context, std::map<int, SynthEdit2::ModuleView*>& guiObjectMap_) = 0;
		void Unload();

		inline int getViewType()
		{
			return viewType;
		}

		void OnChildResize(IViewChild* child);
		void RemoveChild(IViewChild* child);

		int32_t MP_STDCALL measure(GmpiDrawing_API::MP1_SIZE availableSize, GmpiDrawing_API::MP1_SIZE * returnDesiredSize) override;
		int32_t MP_STDCALL arrange(GmpiDrawing_API::MP1_RECT finalRect) override;

		int32_t MP_STDCALL onPointerDown(int32_t flags, GmpiDrawing_API::MP1_POINT point) override;
		int32_t MP_STDCALL onPointerMove(int32_t flags, GmpiDrawing_API::MP1_POINT point) override;
		int32_t MP_STDCALL onPointerUp(int32_t flags, GmpiDrawing_API::MP1_POINT point) override;
		int32_t onMouseWheel(int32_t flags, int32_t delta, GmpiDrawing_API::MP1_POINT point) override;
		int32_t MP_STDCALL OnRender(GmpiDrawing_API::IMpDeviceContext* drawingContext) override;
		int32_t MP_STDCALL setHover(bool isMouseOverMe) override;

		void calcMouseOverObject(int32_t flags);
		void OnChildDeleted(IViewChild* childObject);
		void onSubPanelMadeVisible();
		
		int32_t onContextMenu(int32_t idx) override
		{
			return gmpi::MP_UNHANDLED;
		}

		GmpiDrawing_API::IMpFactory* GetDrawingFactory()
		{
			GmpiDrawing_API::IMpFactory* temp = nullptr;
			getGuiHost()->GetDrawingFactory(&temp);
			return temp;
		}

//		std::string getToolTip(GmpiDrawing_API::MP1_POINT point);
//		int32_t MP_STDCALL getToolTip(float x, float y, gmpi::IMpUnknown* returnToolTipString) override;
		int32_t MP_STDCALL getToolTip(GmpiDrawing_API::MP1_POINT point, gmpi::IString* returnString) override;

		virtual std::string getSkinName() = 0;

		virtual int32_t setCapture(IViewChild* module);
		int32_t releaseCapture();
		bool isMouseCaptured()
		{
			return mouseCaptureObject != nullptr;
		}

		virtual int32_t StartCableDrag(IViewChild* fromModule, int fromPin, GmpiDrawing::Point dragStartPoint, bool isHeldAlt, CableType type = CableType::PatchCable);
		void OnCableMove(ConnectorViewBase * dragline);
		int32_t EndCableDrag(GmpiDrawing_API::MP1_POINT point, ConnectorViewBase* dragline);
		void OnPatchCablesUpdate(RawView patchCablesRaw);
		void UpdateCablesBounds();
		virtual void OnPatchCablesVisibilityUpdate() = 0;
		void RemoveCables(ConnectorViewBase* cable);

		void OnChangedChildHighlight(int phandle, int flags);

		void OnChildDspMessage(void * msg);

		void MoveToFront(IViewChild* child);
		void MoveToBack(IViewChild* child);

		SynthEdit2::IPresenter* Presenter()
		{
			return presenter.get();
		}

		void OnChangedChildSelected(int handle, bool selected);
		void OnChangedChildPosition(int phandle, GmpiDrawing::Rect& newRect);
		void OnChangedChildNodes(int phandle, std::vector<GmpiDrawing::Point>& nodes);

		void OnDragSelectionBox(int32_t flags, GmpiDrawing::Rect selectionRect);

		// not to be confused with MpGuiGfxBase::invalidateRect
		virtual void ChildInvalidateRect(const GmpiDrawing_API::MP1_RECT& invalidRect)
		{
			getGuiHost()->invalidateRect(&invalidRect);
		}
		virtual void OnChildMoved() {}
		virtual int32_t ChildCreatePlatformTextEdit(const GmpiDrawing_API::MP1_RECT* rect, gmpi_gui::IMpPlatformText** returnTextEdit)
		{
			return getGuiHost()->createPlatformTextEdit(const_cast<GmpiDrawing_API::MP1_RECT*>(rect), returnTextEdit);
		}
		virtual int32_t ChildCreatePlatformMenu(const GmpiDrawing_API::MP1_RECT* rect, gmpi_gui::IMpPlatformMenu** returnMenu)
		{
			return getGuiHost()->createPlatformMenu(const_cast<GmpiDrawing_API::MP1_RECT*>(rect), returnMenu);
		}

		virtual bool isShown() = 0; // Indicates if view should be drawn or not (because of 'Show on Parent' state).

		void autoScrollStart();
		void autoScrollStop();
		void DoClose();

		virtual GmpiDrawing::Point MapPointToView(ViewBase* parentView, GmpiDrawing::Point p) = 0;
	};

	class SelectionDragBox : public ViewChild
	{
		GmpiDrawing::Point startPoint;

	public:
		SelectionDragBox(ViewBase* pParent, GmpiDrawing_API::MP1_POINT point) :
			ViewChild(pParent, -1)
		{
			startPoint = point;
			arrange(GmpiDrawing::Rect(point.x, point.y, point.x, point.y));
			parent->setCapture(this);
			parent->invalidateRect(&bounds_);
		}

		static const int lineWidth_ = 3;

		// IViewChild
		virtual int32_t onPointerDown(int32_t flags, GmpiDrawing_API::MP1_POINT point) override
		{
			/*
			bounds_.left = point.x - 1;
			bounds_.top = point.y - 1;
			bounds_.right = point.x;
			bounds_.bottom = point.y;

			parent->invalidateRect(&bounds_);
			*/
			return gmpi::MP_OK;
		}
		virtual int32_t onPointerMove(int32_t flags, GmpiDrawing_API::MP1_POINT point) override
		{
			GmpiDrawing::Rect invalidRect(bounds_);

			bounds_.right = (std::max)(startPoint.x, point.x);
			bounds_.left = (std::min)(startPoint.x, point.x);
			bounds_.bottom = (std::max)(startPoint.y, point.y);
			bounds_.top = (std::min)(startPoint.y, point.y);

			invalidRect.Union(bounds_);
			invalidRect.Inflate(2);

			parent->invalidateRect(&invalidRect);

			return gmpi::MP_OK;
		}

		int32_t onPointerUp(int32_t flags, GmpiDrawing_API::MP1_POINT point) override
		{
			parent->releaseCapture();
			parent->autoScrollStop();

			GmpiDrawing::Rect invalidRect(bounds_);
			invalidRect.Inflate(2);
			parent->invalidateRect(&invalidRect);

			// creat local copy of these to use after 'this' is deleted.
			auto localParent = parent;
			auto localBounds = bounds_;

			localParent->RemoveChild(this);
			// 'this' now deleted!!!

			// carefull not to access 'this'
			const float smallDragSuppression = 2.f;
			if (localBounds.getWidth() > smallDragSuppression && localBounds.getHeight() > smallDragSuppression)
			{
				localParent->OnDragSelectionBox(flags, localBounds);
			}

			return gmpi::MP_OK;
		}

		int32_t onMouseWheel(int32_t flags, int32_t delta, GmpiDrawing_API::MP1_POINT point) override
		{
			return gmpi::MP_UNHANDLED;
		}

		void OnRender(GmpiDrawing::Graphics& g) override
		{
			GmpiDrawing::Color col{ GmpiDrawing::Color::SkyBlue };
			col.a = 0.3f;
			auto brush = g.CreateSolidColorBrush(col);
			auto r = bounds_ - GmpiDrawing::Size(bounds_.left, bounds_.top);
			g.FillRectangle(r, brush);
			brush.SetColor(GmpiDrawing::Color::White);
			g.DrawRectangle(r, brush);
		}

		virtual int32_t populateContextMenu(GmpiDrawing_API::MP1_POINT point, GmpiGuiHosting::ContextItemsSink2* contextMenuItemsSink) override
		{
			return 0;
		}
		virtual int32_t onContextMenu(int32_t idx) override
		{
			return 0;
		}
		void OnMoved(GmpiDrawing::Rect& newRect) override {}
		void OnNodesMoved(std::vector<GmpiDrawing::Point>& newNodes) override {}
		virtual GmpiDrawing::Point getConnectionPoint(CableType cableType, int pinIndex) override
		{
			return GmpiDrawing::Point();
		}
	};
} //namespace SynthEdit2

