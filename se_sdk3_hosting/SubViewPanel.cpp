#include "pch.h"
#include "SubViewPanel.h"
#include <cmath>
#include "../shared/xplatform.h"
#include "../shared/xp_simd.h"
#include "ConnectorView.h"

using namespace gmpi;
using namespace std;
using namespace GmpiDrawing_API;
using namespace GmpiDrawing;

GMPI_REGISTER_GUI(MP_SUB_TYPE_GUI2, SubView, L"ContainerX");

namespace
{
	int32_t r = RegisterPluginXml(

R"XML(
<?xml version="1.0" encoding="utf-8" ?>
<PluginList>
  <Plugin id="ContainerX" name="Container" category="Debug" graphicsApi="GmpiGui">
    <GUI>
      <Pin name="Controls on Parent" datatype="bool" direction="out" isMinimised="true" />
      <Pin name="Controls on Module" datatype="bool" noAutomation="true" />
      <Pin name="Visible" datatype="bool" />
      <Pin name="Ignore Program Change" datatype="bool" noAutomation="true" />
    </GUI>
<!--
    <Audio>
      <Pin name="Spare" datatype="float" rate="audio" autoRename="true" autoDuplicate="true" isContainerIoPlug="true" />
	  -- obsolete --
      <Pin name="Polyphony" datatype="enum" default="6" private="true" isMinimised="true" metadata="range 1,128" />
      <Pin name="MIDI Automation" datatype="bool" private="true" isMinimised="true" />
      <Pin name="Show Controls" datatype="enum" private="true" isMinimised="true" metadata="Off,On Panel,On Module" />
    </Audio>
-->
  </Plugin>
</PluginList>
)XML");

}

int32_t SubView::StartCableDrag(SynthEdit2::IViewChild* fromModule, int fromPin, Point dragStartPoint, bool isHeldAlt, SynthEdit2::CableType type)
{
	auto moduleview = dynamic_cast<SynthEdit2::ModuleView*>(this->getGuiHost());
	dragStartPoint += offset_;
	dragStartPoint += moduleview->OffsetToClient();

	return moduleview->parent->StartCableDrag(fromModule, fromPin, dragStartPoint, isHeldAlt);
}

void SubView::OnCableDrag(SynthEdit2::ConnectorViewBase* dragline, GmpiDrawing::Point dragPoint, float& bestDistance, SynthEdit2::IViewChild*& bestModule, int& bestPinIndex)
{
	dragPoint -= offset_;

	for (auto it = children.rbegin(); it != children.rend(); ++it) // iterate in reverse for correct Z-Order.
	{
		(*it)->OnCableDrag(dragline, dragPoint, bestDistance, bestModule, bestPinIndex);
	}
}

SubView::SubView(int pparentViewType) :
	parentViewType(pparentViewType)
{
	if (parentViewType == CF_PANEL_VIEW)
	{
		initializePin(0, showControlsLegacy, static_cast<MpGuiBaseMemberPtr2>(&SubView::onValueChanged));
		// 1 : "Controls on Module" not needed.
		initializePin(2, showControls, static_cast<MpGuiBaseMemberPtr2>(&SubView::onValueChanged));
		// 3 : "Ignore Program Change" not needed.
	}
	else
	{
		// Hack - Reroute Show-Controls-On-Module to Show-Controls-On-Panel
		initializePin(1, showControls, static_cast<MpGuiBaseMemberPtr2>(&SubView::onValueChanged));
	}

	offset_.height = offset_.width = -99999; // un-initialized
}

void SubView::onValueChanged()
{
	// if we just blinked into existence, need to update mouse over object
	if (isShown())
	{
		auto moduleview = dynamic_cast<SynthEdit2::ModuleView*>(getGuiHost());
		moduleview->parent->onSubPanelMadeVisible();
	}

	OnPatchCablesVisibilityUpdate();

	invalidateRect();
}

void SubView::OnPatchCablesVisibilityUpdate()
{
	auto parent = dynamic_cast<SynthEdit2::ViewChild*> (getGuiHost());
	parent->parent->OnPatchCablesVisibilityUpdate();
}

int32_t SubView::setCapture(SynthEdit2::IViewChild* module)
{
	// Avoid situation where some module turns off panel then captures mouse (ensuring it never can un-capture it).
	if (isShown())
	{
		return ViewBase::setCapture(module);
	}

	return gmpi::MP_FAIL;
}


int32_t MP_STDCALL SubView::initialize()
{
	onValueChanged(); // nesc in case initial value is 0.

	int32_t x, y;
	Presenter()->GetViewScroll(x, y);

	offset_.width = static_cast<float>(x);
	offset_.height = static_cast<float>(y);

	if( x != -99999)
	{
		auto module = dynamic_cast<SynthEdit2::ViewChild*> (getGuiHost());
		offset_.width -= module->bounds_.left;
		offset_.height -= module->bounds_.top;
	}

	return SynthEdit2::ViewBase::initialize();
}

int32_t SubView::measure(GmpiDrawing_API::MP1_SIZE availableSize, GmpiDrawing_API::MP1_SIZE* returnDesiredSize)
{
	// calc my bounds.
	// Start with inverted rect (no area).
    viewBounds = GmpiDrawing::Rect(200000, 200000, -200000, -200000);

    const GmpiDrawing::Size veryLarge(100000, 100000);
	GmpiDrawing::Size notused;

	for (auto& m : children)
	{
		// Copied form ViewBase::arrange().
		if (m->isVisable() && dynamic_cast<SynthEdit2::ConnectorViewBase*>(m.get()) == nullptr)
		{
			GmpiDrawing::Size savedSize(m->getLayoutRect().getWidth(), m->getLayoutRect().getHeight());
			GmpiDrawing::Size desired;
			GmpiDrawing::Size actualSize;
			bool changedSize = false;
			/*
			if (debug)
			{
			_RPT4(_CRT_WARN, "savedSize r[ %f %f %f %f]\n", m->getBounds().left, m->getBounds().top, m->getBounds().left + m->getBounds().getWidth(), m->getBounds().top + m->getBounds().getHeight());
			}
			*/
			// Detect brand-new objects that haven't had size calculated yet.
			if (savedSize.width == 0 && savedSize.height == 0)
			{
				const int defaultDimensions = 100;
				GmpiDrawing::Size defaultSize(defaultDimensions, defaultDimensions);
				m->measure(defaultSize, &desired);
				actualSize = desired;
				// stick with integer sizes for compatibility.
				actualSize.height = ceilf(actualSize.height);
				actualSize.width = ceilf(actualSize.width);
				changedSize = true;
			}
			else
			{
#ifdef _DEBUG
				desired.width = std::numeric_limits<float>::quiet_NaN();
#endif

				m->measure(savedSize, &desired);

#ifdef _DEBUG
				assert(!std::isnan(desired.width)); // object does not implement measure()!
#endif
													/*
													if (debug)
													{
													_RPT2(_CRT_WARN, "desired s[ %f %f]\n", desired.width, desired.height);
													}
													*/
													// Font variations cause Slider to report different desired size.
													// However resizing it causes alignment errors on Panel. It shifts left or right.
													// Avoid resizing unless module clearly needs a different size. Structure view always sizes to fit (else plugs end up with wrapped text)
				float tolerence = viewType == CF_PANEL_VIEW ? 3.0f : 0.0f;
				if (isArranged || (fabsf(desired.width - savedSize.width) > tolerence || fabsf(desired.height - savedSize.height) > tolerence))
				{
					actualSize = desired;
					// stick with integer sizes for compatibility.
					actualSize.height = ceilf(actualSize.height);
					actualSize.width = ceilf(actualSize.width);
					changedSize = true;
				}
				else
				{
					// Used save size from project, even if it varies a little.
					actualSize = savedSize;
				}
			}

			// Note, due to font width differences, this may result in different size/layout than original GDI graphics. e..g knobs shifting.
			/*
			if (debug)
			{
			_RPT4(_CRT_WARN, "arrange r[ %f %f %f %f]\n", m->getBounds().left, m->getBounds().top, m->getBounds().left + actualSize.width, m->getBounds().top + actualSize.height);
			}
			*/
			GmpiDrawing::Rect moduleRect(m->getLayoutRect().left, m->getLayoutRect().top, m->getLayoutRect().left + actualSize.width, m->getLayoutRect().top + actualSize.height);
			//m->arrange(GmpiDrawing::Rect(m->getLayoutRect().left, m->getLayoutRect().top, m->getLayoutRect().left + actualSize.width, m->getLayoutRect().top + actualSize.height));

			// Typically only when new object inserted.
			viewBounds.left = (std::min)(viewBounds.left, moduleRect.left);
			viewBounds.right = (std::max)(viewBounds.right, moduleRect.right);
			viewBounds.top = (std::min)(viewBounds.top, moduleRect.top);
			viewBounds.bottom = (std::max)(viewBounds.bottom, moduleRect.bottom);
		}
	}

	if (viewBounds.right == -200000) // no children. Default to small rectangle.
	{
		viewBounds.left = viewBounds.top = 0;
		viewBounds.right = viewBounds.bottom = 10;
	}

	returnDesiredSize->width = (std::max)(0.0f, viewBounds.getWidth());
	returnDesiredSize->height = (std::max)(0.0f, viewBounds.getHeight());

	// On first open, need to calc offset relative to view.
	// ref control_group_auto_size::RecalcBounds()
	if( offset_.width == -99999.f )
	{
		offset_.width = static_cast<float>(-FastRealToIntFloor(viewBounds.left));
		offset_.height = static_cast<float>(-FastRealToIntFloor(viewBounds.top));

		// avoid 'show on module' structure view messing up panel view's offset.
		if (parentViewType == CF_PANEL_VIEW)
		{
			auto module = dynamic_cast<SynthEdit2::ViewChild*>(getGuiHost());
			Presenter()->SetViewScroll(
				static_cast<int32_t>(offset_.width + module->bounds_.left),
				static_cast<int32_t>(offset_.height + module->bounds_.top)
			);
		}
	}
	else
	{
		// if top-left coords have changed last opened.
		// then shift sub-panel to compensate (panel view only).
		int32_t parentAdjustX(FastRealToIntFloor(offset_.width + viewBounds.left));
		int32_t parentAdjustY(FastRealToIntFloor(offset_.height + viewBounds.top));

		if (parentAdjustX != 0 || parentAdjustY != 0)
		{
			offset_.width -= parentAdjustX;
			offset_.height -= parentAdjustY;

			// Adjust module top-left.
			auto parent = dynamic_cast<SynthEdit2::ViewChild*> (getGuiHost());

			if (parent->parent->getViewType() == CF_PANEL_VIEW)
				parent->Presenter()->ResizeModule(parent->handle, 0, 0, GmpiDrawing::Size((float)parentAdjustX, (float)parentAdjustY));
		}
	}
	return gmpi::MP_OK;
}

int32_t SubView::arrange(GmpiDrawing_API::MP1_RECT finalRect)
{
	return ViewBase::arrange(finalRect);
}

bool SubView::isShown()
{
	auto parent = dynamic_cast<SynthEdit2::ViewChild*> (getGuiHost());
	if (!parent->isShown())
		return false;

	if (parentViewType == CF_PANEL_VIEW)
		return showControls || showControlsLegacy;
	else
		return showControls;
}

int32_t SubView::OnRender(GmpiDrawing_API::IMpDeviceContext* drawingContext)
{
	if (isShown())
	{
		GmpiDrawing::Graphics g(drawingContext);

		// Transform to module-relative.
		const auto originalTransform = g.GetTransform();
		auto adjustedTransform = Matrix3x2::Translation(offset_.width, offset_.height) * originalTransform;
		g.SetTransform(adjustedTransform);

		// Render.
		auto res = SynthEdit2::ViewBase::OnRender(drawingContext);

		// Transform back.
		g.SetTransform(originalTransform);
		return res;
	}
	else
		return gmpi::MP_UNHANDLED;
}

int32_t SubView::getToolTip(MP1_POINT point, gmpi::IString* returnString)
{
	const auto localPoint = GmpiDrawing::Point(point.x - offset_.width, point.y - offset_.height);
	return ViewBase::getToolTip(localPoint, returnString);
}

bool SubView::hitTest(int32_t flags, GmpiDrawing_API::MP1_POINT* point)
{
	if (isShown())
	{
		const auto localPoint = GmpiDrawing::Point(point->x - offset_.width, point->y - offset_.height );
		for (auto it = children.rbegin(); it != children.rend(); ++it) // iterate in reverse for correct Z-Order.
		{
			auto m = (*it).get();
			if (m->hitTest(flags, localPoint))
			{
				return true;
			}
		}
	}
	
	return false;
}

int32_t SubView::hitTest(GmpiDrawing_API::MP1_POINT point)
{
	return hitTest(0, &point) ? gmpi::MP_OK : gmpi::MP_UNHANDLED;
}

int32_t SubView::onPointerDown(int32_t flags, MP1_POINT point)
{
	if (isShown())
	{
		point.x -= offset_.width;
		point.y -= offset_.height;
		return ViewBase::onPointerDown(flags, point);
	}
	else
		return gmpi::MP_UNHANDLED;
}

int32_t SubView::onPointerMove(int32_t flags, MP1_POINT point)
{
	if (isShown())
	{
		point.x -= offset_.width;
		point.y -= offset_.height;
		return ViewBase::onPointerMove(flags, point);
	}
	else
		return gmpi::MP_UNHANDLED;
}

int32_t SubView::onPointerUp(int32_t flags, MP1_POINT point)
{
	if (isShown() || mouseCaptureObject)  // attempt to fix it when object on panel captures mouse, then hides itself
	{
		point.x -= offset_.width;
		point.y -= offset_.height;
		return ViewBase::onPointerUp(flags, point);
	}
	else
	{
		return gmpi::MP_UNHANDLED;
	}
}

int32_t SubView::onMouseWheel(int32_t flags, int32_t delta, GmpiDrawing_API::MP1_POINT point)
{
	if (!isShown())
		return gmpi::MP_UNHANDLED;

	point.x -= offset_.width;
	point.y -= offset_.height;

	return ViewBase::onMouseWheel(flags, delta, point);
}

void SubView::OnChildMoved()
{
	// TODO : enhancement - also calc my cliprect on sum of child cliprects.

	auto parent = dynamic_cast<SynthEdit2::ViewChild*> (getGuiHost());

	GmpiDrawing::Rect viewBoundsNew;
	GmpiDrawing::Rect unused2;
	calcBounds(viewBoundsNew, unused2);

	if (viewBounds == viewBoundsNew)
		return;

	// Adjust module layout rect
	auto parentLayoutRect = parent->getLayoutRect();

	// shift top-left origin if nesc. (panel only)
	if (parent->parent->getViewType() == CF_PANEL_VIEW)
	{
		parentLayoutRect.left += viewBoundsNew.left - viewBounds.left;
		parentLayoutRect.top += viewBoundsNew.top - viewBounds.top;
	}

	// set size.
	parentLayoutRect.right = parentLayoutRect.left + viewBoundsNew.right - viewBoundsNew.left;
	parentLayoutRect.bottom = parentLayoutRect.top + viewBoundsNew.bottom - viewBoundsNew.top;

	viewBounds = viewBoundsNew;
	offset_.width = -viewBoundsNew.left;
	offset_.height = -viewBoundsNew.top;

	parent->parent->OnChangedChildPosition(parent->handle, parentLayoutRect);
}

void SubView::calcBounds(GmpiDrawing::Rect& returnLayoutRect, GmpiDrawing::Rect& returnClipRect)
{
	// calc my bounds.
	// Start with inverted rect (no area).
	returnLayoutRect = GmpiDrawing::Rect(200000, 200000, -200000, -200000);

	const GmpiDrawing::Size veryLarge(100000, 100000);
	GmpiDrawing::Size notused;

	for (auto& m : children)
	{
		// Copied form ViewBase::arrange().
		if (m->isVisable() && dynamic_cast<SynthEdit2::ConnectorViewBase*>(m.get()) == nullptr)
		{
			GmpiDrawing::Size savedSize(m->getLayoutRect().getWidth(), m->getLayoutRect().getHeight());
			GmpiDrawing::Size desired;
			GmpiDrawing::Size actualSize;
			bool changedSize = false;
			/*
			if (debug)
			{
			_RPT4(_CRT_WARN, "savedSize r[ %f %f %f %f]\n", m->getBounds().left, m->getBounds().top, m->getBounds().left + m->getBounds().getWidth(), m->getBounds().top + m->getBounds().getHeight());
			}
			*/
			// Detect brand-new objects that haven't had size calculated yet.
			if (savedSize.width == 0 && savedSize.height == 0)
			{
				const int defaultDimensions = 100;
				GmpiDrawing::Size defaultSize(defaultDimensions, defaultDimensions);
				m->measure(defaultSize, &desired);
				actualSize = desired;
				// stick with integer sizes for compatibility.
				actualSize.height = ceilf(actualSize.height);
				actualSize.width = ceilf(actualSize.width);
				changedSize = true;
			}
			else
			{
#ifdef _DEBUG
				desired.width = std::numeric_limits<float>::quiet_NaN();
#endif

				m->measure(savedSize, &desired);

#ifdef _DEBUG
				assert(!std::isnan(desired.width)); // object does not implement measure()!
#endif
				/*
				if (debug)
				{
				_RPT2(_CRT_WARN, "desired s[ %f %f]\n", desired.width, desired.height);
				}
				*/
				// Font variations cause Slider to report different desired size.
				// However resizing it causes alignment errors on Panel. It shifts left or right.
				// Avoid resizing unless module clearly needs a different size. Structure view always sizes to fit (else plugs end up with wrapped text)
				float tolerence = viewType == CF_PANEL_VIEW ? 3.0f : 0.0f;
				if (isArranged || (fabsf(desired.width - savedSize.width) > tolerence || fabsf(desired.height - savedSize.height) > tolerence))
				{
					actualSize = desired;
					// stick with integer sizes for compatibility.
					actualSize.height = ceilf(actualSize.height);
					actualSize.width = ceilf(actualSize.width);
					changedSize = true;
				}
				else
				{
					// Used save size from project, even if it varies a little.
					actualSize = savedSize;
				}
			}

			// Note, due to font width differences, this may result in different size/layout than original GDI graphics. e..g knobs shifting.
			/*
			if (debug)
			{
			_RPT4(_CRT_WARN, "arrange r[ %f %f %f %f]\n", m->getBounds().left, m->getBounds().top, m->getBounds().left + actualSize.width, m->getBounds().top + actualSize.height);
			}
			*/
			GmpiDrawing::Rect moduleRect(m->getLayoutRect().left, m->getLayoutRect().top, m->getLayoutRect().left + actualSize.width, m->getLayoutRect().top + actualSize.height);
			//m->arrange(GmpiDrawing::Rect(m->getLayoutRect().left, m->getLayoutRect().top, m->getLayoutRect().left + actualSize.width, m->getLayoutRect().top + actualSize.height));

			// Typically only when new object inserted.
			returnLayoutRect.left = (std::min)(returnLayoutRect.left, moduleRect.left);
			returnLayoutRect.right = (std::max)(returnLayoutRect.right, moduleRect.right);
			returnLayoutRect.top = (std::min)(returnLayoutRect.top, moduleRect.top);
			returnLayoutRect.bottom = (std::max)(returnLayoutRect.bottom, moduleRect.bottom);
		}
	}

	if (returnLayoutRect.right == -200000) // no children. Default to small rectangle.
	{
		returnLayoutRect.left = returnLayoutRect.top = 0;
		returnLayoutRect.right = returnLayoutRect.bottom = 10;
	}

#if 0
	//returnLayoutRect.width = (std::max)(0.0f, viewBounds.getWidth());
	//returnLayoutRect.height = (std::max)(0.0f, viewBounds.getHeight());

	// On first open, need to calc offset relative to view.
	// ref control_group_auto_size::RecalcBounds()
	if (offset_.width == -99999.f)
	{
		offset_.width = -FastRealToIntFloor(viewBounds.left);
		offset_.height = -FastRealToIntFloor(viewBounds.top);

		auto module = dynamic_cast<SynthEdit2::ViewChild*> (getGuiHost());

		Presenter()->SetViewScroll(offset_.width + module->bounds_.left, offset_.height + module->bounds_.top);
	}
	else
	{
		/* moved to OnChildMoved()
		// if top-left coords have changed last opened.
		// then shift sub-panel to compensate (panel view only).
		int32_t parentAdjustX(offset_.width + FastRealToIntFloor(viewBounds.left));
		int32_t parentAdjustY(offset_.height + FastRealToIntFloor(viewBounds.top));

		if (parentAdjustX != 0 || parentAdjustY != 0)
		{
			offset_.width -= parentAdjustX;
			offset_.height -= parentAdjustY;

			// Adjust module top-left.
			auto parent = dynamic_cast<SynthEdit2::ViewChild*> (getGuiHost());

			if (parent->parent->getViewType() == CF_PANEL_VIEW)
				parent->Presenter()->ResizeModule(parent->handle, 0, 0, GmpiDrawing::Size(parentAdjustX, parentAdjustY));
		}
		*/

		//		if(parent->get)
	}
	return gmpi::MP_OK;
#endif

	returnLayoutRect.left = floorf(returnLayoutRect.left);
	returnLayoutRect.top = floorf(returnLayoutRect.top);
	returnLayoutRect.right = ceilf(returnLayoutRect.right);
	returnLayoutRect.bottom = ceilf(returnLayoutRect.bottom);
}
