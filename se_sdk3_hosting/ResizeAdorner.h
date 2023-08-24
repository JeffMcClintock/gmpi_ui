#pragma once

#include "./IViewChild.h"
#include "Drawing.h"
#include "ModuleView.h"

namespace SynthEdit2
{
	class ModuleView;

	class ResizeAdorner : public IViewChild
	{
	protected:
		static const int SelectionFrameOffset = 1;
		static const int ResizeHandleRadius = 4;
		static const int DragAreaheight = 4;
		ViewBase* parent;
		ModuleView* module;
		GmpiDrawing::Point pointPrev;
		int dragNodeX;
		int dragNodeY;
		GmpiDrawing::Color::Enum color;
		bool isResizableX;
		bool isResizableY;
		GmpiDrawing::Rect bounds;
		bool mouseHover{};

		struct node
		{
			int xIndex;
			int yIndex;
			GmpiDrawing::Point location;
		};

		std::vector<node> getNodes() const
		{
			GmpiDrawing::Rect r = GetOutlineRect();
			r -= GmpiDrawing::Size(bounds.left, bounds.top);

			int startX, endX;
			if (isResizableX)
			{
				startX = 0;
				endX = 2;
			}
			else
			{
				startX = endX = 1;
			}

			int startY, endY;
			if (isResizableY)
			{
				startY = parent->getViewType() == CF_PANEL_VIEW ? 0 : 1;
				endY = 2;
			}
			else
			{
				startY = endY = 1;
			}

			std::vector<node> nodes;
			for (int x = startX; x <= endX; ++x)
			{
				const auto pointx = r.left + r.getWidth() * (float)x * 0.5f;
				for (int y = startY; y <= endY; ++y)
				{
					const auto pointy = r.top + r.getHeight() * (float)y * 0.5f;
					if (x != 1 || y != 1)
					{
						nodes.push_back({ x, y, {pointx, pointy} });
					}
				}
			}

			return nodes;
		}

	public:
		bool hasGripper = true; // and top handles. i.e. is Panel view.

		ResizeAdorner(ViewBase* pParent, ModuleView* pModule) :
			parent(pParent)
			, module(pModule)
			,color(GmpiDrawing::Color::DodgerBlue)
			, hasGripper(true)
		{
			if (pModule->ignoreMouse)
			{
				hasGripper = false;
				color = GmpiDrawing::Color::Gray;
			}

			bounds = clientBoundsToAdorner(module->getLayoutRect());
		}

		virtual int32_t measure(GmpiDrawing::Size availableSize, GmpiDrawing::Size* returnDesiredSize) override
		{
			// determin resizeability.
			{
				GmpiDrawing::Size desiredMax(0, 0);
				GmpiDrawing::Size desiredMin(0, 0);
				module->measure(GmpiDrawing::Size(0, 0), &desiredMin);
				module->measure(GmpiDrawing::Size(10000, 10000), &desiredMax);

				isResizableX = desiredMin.width != desiredMax.width;
				isResizableY = desiredMin.height != desiredMax.height;
			}

			*returnDesiredSize = availableSize;

			return gmpi::MP_OK;
		}
		virtual int32_t arrange(GmpiDrawing::Rect finalRect) override
		{
			bounds = clientBoundsToAdorner(finalRect);
			return gmpi::MP_OK;
		}

		inline GmpiDrawing_API::MP1_RECT clientBoundsToAdorner(GmpiDrawing_API::MP1_RECT r)
		{
			const int penThickness = 1;
			GmpiDrawing::Rect r2(r);
			r2.Inflate(ResizeHandleRadius + SelectionFrameOffset + penThickness);

			if(hasGripper)
				r2.top -= DragAreaheight;

			return r2;
		}

		 // this is NOT outline of entire module, only lower half containing plugin gfx
		virtual GmpiDrawing_API::MP1_RECT GetOutlineRect() const
		{
			GmpiDrawing::Rect r(module->bounds_);
			r.Inflate((float)SelectionFrameOffset);

			if (hasGripper)
				r.top -= DragAreaheight;

			return r;
		}

		virtual GmpiDrawing::Rect getLayoutRect() override
		{
			return bounds; // clientBoundsToAdorner(module->getBounds());
		}

		virtual GmpiDrawing::Rect GetClipRect() override
		{
			GmpiDrawing::Rect r(GetOutlineRect());
			r.Inflate(ResizeHandleRadius + 1.0f);
			return r; // clientBoundsToAdorner(module->bounds_);
		}

		virtual void OnMoved(GmpiDrawing::Rect& r) override
		{
			auto invalidRect = bounds;
			bounds = clientBoundsToAdorner(r);
			invalidRect.Union(bounds);
			invalidRect.Inflate((float)ResizeHandleRadius);

			parent->getGuiHost()->invalidateRect(&invalidRect);
		}
		void OnNodesMoved(std::vector<GmpiDrawing::Point>& newNodes) override {}

		// rendered only on panel
		void OnRender(GmpiDrawing::Graphics& g) override
		{
			GmpiDrawing::Rect r = GetOutlineRect();
			r -= GmpiDrawing::Size(bounds.left, bounds.top);
			auto brush = g.CreateSolidColorBrush(color);

			if (mouseHover)
			{
				brush.SetColor(GmpiDrawing::Color::DeepSkyBlue);
			}

			g.DrawRectangle(r, brush);

			if (hasGripper)
			{
				GmpiDrawing::Rect dragArea(r);
				dragArea.bottom = dragArea.top + DragAreaheight;
				g.FillRectangle(dragArea, brush);

				// grip indication
				auto bg = g.CreateSolidColorBrush(GmpiDrawing::Color::FromArgb(0x20000000));
				for( int x = 1 ; x < r.getWidth() - 1; x += 3)
				{
					for (int y = 1; y < DragAreaheight - 1; y += 2)
					{
						if( ((x/3) ^ (y/2)) & 0x1 )
						{
							GmpiDrawing::Rect rg(r.left + x, r.top + y, r.left + x + 2, r.top + y + 1);
							g.FillRectangle(rg, bg);
						}
					}
				}
			}

			// draw Resize handles.
			GmpiDrawing::Ellipse circle(GmpiDrawing::Point(0., 0.), (float)ResizeHandleRadius);
			auto FillBrush = g.CreateSolidColorBrush(GmpiDrawing::Color::White);

			for(auto& n : getNodes())
			{
				circle.point = n.location;

				g.FillEllipse(circle, FillBrush);
				g.DrawEllipse(circle, brush);
			}
		}

		bool hitTest(int32_t flags, GmpiDrawing_API::MP1_POINT point) override
		{
			GmpiDrawing::Rect r(GetOutlineRect()); // on structure this is NOT outline of entire module, only lower half containing plugin gfx

			// Outer Rect.
			GmpiDrawing::Rect outerRect(r);
			const float outerThickness = ResizeHandleRadius; // hits count slightly outside line.
			outerRect.Inflate(outerThickness);
			if (!outerRect.ContainsPoint(point))
			{
				return false;
			}

			// Gripper
			if (hasGripper && point.y >= 0 && point.y < r.top + DragAreaheight)
			{
				return true;
			}

			const auto pointLocal = GmpiDrawing::Point(point) - GmpiDrawing::Size(bounds.left, bounds.top);

			// Resize handles.
			int hitNodeX, hitNodeY;
			hitTestNodes(pointLocal, hitNodeX, hitNodeY);
			if (hitNodeX >= 0 || hitNodeY >= 0)
			{
				return true;
			}

			GmpiDrawing::Rect innerRect(r);
			innerRect.Deflate(1.5f);
			return !innerRect.ContainsPoint(point);
		}

		bool hitTest(int32_t flags, GmpiDrawing_API::MP1_RECT selectionRect) override
		{
			return isOverlapped(GmpiDrawing::Rect(selectionRect), GmpiDrawing::Rect(GetOutlineRect()));
		}

		virtual std::string getToolTip(GmpiDrawing_API::MP1_POINT point) override
		{
			return std::string();
		}

		virtual void receiveMessageFromAudio(void*) override
		{
		}

		void hitTestNodes(GmpiDrawing_API::MP1_POINT point, int& hitNodeX, int& hitNodeY)
		{
			for(auto& n : getNodes())
			{
				const float dx = n.location.x - point.x;
				const float dy = n.location.y - point.y;

				if((dx * dx + dy * dy) <= (float)((1 + ResizeHandleRadius) * (1 + ResizeHandleRadius)))
				{
					hitNodeX = n.xIndex;
					hitNodeY = n.yIndex;
					return;
				}
			}

			hitNodeX = hitNodeY = -1;
		}

		void setHover(bool mouseIsOverMe) override
		{
			mouseHover = mouseIsOverMe;
//			auto redrawRect = GetClipRect();
			parent->getGuiHost()->invalidateRect(&bounds);
		}

		int32_t onPointerDown(int32_t flags, GmpiDrawing_API::MP1_POINT point) override
		{
			const auto pointLocal = GmpiDrawing::Point(point) - GmpiDrawing::Size(bounds.left, bounds.top);
			hitTestNodes(pointLocal, dragNodeX, dragNodeY);

			pointPrev = point;
			parent->setCapture(this);
			parent->autoScrollStart();

			return gmpi::MP_OK;
		}

		virtual int32_t onPointerMove(int32_t flags, GmpiDrawing_API::MP1_POINT point) override
		{
			if (parent->getCapture())
			{
				auto snapGridSize = parent->Presenter()->GetSnapSize();
				GmpiDrawing::Size delta(point.x - pointPrev.x, point.y - pointPrev.y);

				if (delta.width == 0.0f && delta.height == 0.0f) // avoid false snap on selection
					return gmpi::MP_OK;

				GmpiDrawing::Point snapReference(module->getLayoutRect().left, module->getLayoutRect().top);

				switch (dragNodeX)
				{
				case -1: // border.
					{
						// Snap-to-grid logic.
						GmpiDrawing::Point newPoint = snapReference + delta;
						newPoint.x = floorf((snapGridSize / 2 + newPoint.x) / snapGridSize) * snapGridSize;
						newPoint.y = floorf((snapGridSize / 2 + newPoint.y) / snapGridSize) * snapGridSize;
						GmpiDrawing::Size snapDelta = newPoint - snapReference;

						pointPrev += snapDelta;

						if (snapDelta.width != 0.0 || snapDelta.height != 0.0)
							parent->Presenter()->DragSelection(snapDelta);
					}
					return gmpi::MP_OK;
					break;

				/* already the default
				case 0:
					snapReference.x = module->getBounds().left;
					break;
				*/

				case 1: // h center
					delta.width = 0;
					break;

				case 2: // Right.
					snapReference.x = module->getLayoutRect().right;
					break;
				}

				switch (dragNodeY)
				{
				/* already the default
				case 0: // top
					snapReference.y = module->getBounds().top;
					break;
				*/

				case 1: // vert center
					delta.height = 0;
					break;

				case 2: // bottom
					snapReference.y = module->getLayoutRect().bottom;
					break;
				}

				// Snap-to-grid logic.
				GmpiDrawing::Point newPoint = snapReference + delta;
				newPoint.x = floorf((snapGridSize / 2 + newPoint.x) / snapGridSize) * snapGridSize;
				newPoint.y = floorf((snapGridSize / 2 + newPoint.y) / snapGridSize) * snapGridSize;
				GmpiDrawing::Size snapDelta = newPoint - snapReference;

				pointPrev += snapDelta;

				if (snapDelta.width != 0.0 || snapDelta.height != 0.0)
					parent->Presenter()->ResizeModule(getModuleHandle(), dragNodeX, dragNodeY, snapDelta);
			}

			return gmpi::MP_OK;
		}

		virtual int32_t onPointerUp(int32_t flags, GmpiDrawing_API::MP1_POINT point) override
		{
			if (parent->getCapture())
			{
				parent->releaseCapture();
				parent->autoScrollStop();
			}

			return gmpi::MP_OK;
		}
		int32_t onMouseWheel(int32_t flags, int32_t delta, GmpiDrawing_API::MP1_POINT point) override
		{
			return gmpi::MP_UNHANDLED;
		}
		virtual int32_t populateContextMenu(GmpiDrawing_API::MP1_POINT point, GmpiGuiHosting::ContextItemsSink2* contextMenuItemsSink) override
		{
			return gmpi::MP_OK;
		}
		virtual int32_t onContextMenu(int32_t idx) override
		{
			return gmpi::MP_OK;
		}

		virtual int32_t getModuleHandle() override
		{
			return module->getModuleHandle();
		}
		virtual bool getSelected() override
		{
			return false;
		}

		void setSelected(bool selected) override
		{
		}

		virtual GmpiDrawing::Point getConnectionPoint(CableType cableType, int pinIndex) override
		{
			return GmpiDrawing::Point();
		}
	};

	class ResizeAdornerStructure : public ResizeAdorner
	{
	public:
		ResizeAdornerStructure(ViewBase* pParent, ModuleView* pModule) : ResizeAdorner(pParent, pModule)
		{
			hasGripper = false;
		}

		GmpiDrawing_API::MP1_RECT GetOutlineRect() const override
		{
			GmpiDrawing::Rect r(module->bounds_);

			r.Deflate(6.5f, 0.5f);
			r.top = r.bottom - module->pluginGraphicsPos.getHeight() - 1.5f;

			return r;
		}

		bool hitTest(int32_t flags, GmpiDrawing_API::MP1_POINT point) override
		{
			GmpiDrawing::Rect outerRect(GetOutlineRect()); // on structure, this is NOT outline of entire module, only lower half containing plugin gfx
			const float outerThickness = ResizeHandleRadius; // hits count slightly outside line.
			outerRect.Inflate(outerThickness);
			if(!outerRect.ContainsPoint(point))
			{
				return false;
			}

			const auto pointLocal = GmpiDrawing::Point(point) - GmpiDrawing::Size(bounds.left, bounds.top);

			// Resize handles.
			int hitNodeX, hitNodeY;
			hitTestNodes(pointLocal, hitNodeX, hitNodeY);
			return hitNodeX >= 0 || hitNodeY >= 0;
		}

		bool hitTest(int32_t flags, GmpiDrawing_API::MP1_RECT selectionRect) override
		{
			return isOverlapped(GmpiDrawing::Rect(selectionRect), GmpiDrawing::Rect(GetOutlineRect()));
		}

		void OnRender(GmpiDrawing::Graphics& g) override
		{
			const auto nodes = getNodes();
			if(nodes.empty())
			{
				return;
			}

			// draw Resize handles only. Outline is handled by module.
			GmpiDrawing::Ellipse circle(GmpiDrawing::Point(0., 0.), (float)ResizeHandleRadius);
			auto FillBrush = g.CreateSolidColorBrush(GmpiDrawing::Color::White);
			auto OutlineBrush = g.CreateSolidColorBrush(color);

			for(auto& n : nodes)
			{
				circle.point = n.location;

				g.FillEllipse(circle, FillBrush);
				g.DrawEllipse(circle, OutlineBrush);
			}
		}
	};
}
