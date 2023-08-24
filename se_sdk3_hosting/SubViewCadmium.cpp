#include "pch.h"
#include "SubViewCadmium.h"
#include <cmath>
#include "../shared/xplatform.h"
#include "../shared/xp_simd.h"
#include "mfc_emulation.h"
#include "ConnectorView.h"
#include "iguihost2.h"

using namespace gmpi;
using namespace std;
using namespace GmpiDrawing_API;
using namespace GmpiDrawing;

GMPI_REGISTER_GUI(MP_SUB_TYPE_GUI2, SubViewCadmium, L"ContainerY");

namespace
{
	int32_t r = RegisterPluginXml(

		R"XML(
<?xml version="1.0" encoding="utf-8" ?>
<PluginList>
  <Plugin id="ContainerY" name="Container" category="Debug" graphicsApi="GmpiGui">
    <GUI>
      <Pin name="Controls on Parent" datatype="bool" direction="out" isMinimised="true" />
      <Pin name="Controls on Module" datatype="bool" noAutomation="true" />
      <Pin name="Visible" datatype="bool" />
      <Pin name="Ignore Program Change" datatype="bool" noAutomation="true" />
    </GUI>
  </Plugin>
</PluginList>
)XML");

}



// utils
struct moduleConnection
{
	int32_t toModuleHandle = {};
	int32_t fromModulePin = {};
	int32_t toModulePin = {};
};

struct moduleInfo
{
	int32_t sort = -1; // unsorted
//	int32_t moduleHandle = {};
	std::vector<moduleConnection> connections;
};

void CalcSortOrder2(std::map<int32_t, moduleInfo>& allModules, moduleInfo& m, int maxSortOrderGlobal)
{
	m.sort = -2; // prevent recursion back to this.
	int maxSort = -1; // reset.
	for (auto connector : m.connections)
	{
		assert(allModules.find(connector.toModuleHandle) != allModules.end());
		auto& to = allModules[connector.toModuleHandle];
		{
			{
				int order = to.sort;
				if (order == -2) // Found an feedback path, report it.
				{
					assert(false); // FEEDBACK!
					return;
					/*
											m.sort = -1; // Allow this to be re-sorted after feedback (potentially) compensated.
											auto e = new FeedbackTrace(SE_FEEDBACK_PATH);
											e->AddLine(p, to);
											return e;
					*/
				}

				if (order == -1) // Found an unsorted path, go down it.
				{
					/*auto e = */ CalcSortOrder2(allModules, to, maxSortOrderGlobal);
#if 0
					if (e) // Downstream module encountered feedback.
					{
						// Not all modules have valid moduleType, e.g. oversampler_in
						if (moduleType && (moduleType->GetFlags() & CF_IS_FEEDBACK) != 0 && p->DataType == DT_MIDI) // dummy pin
						{
							// User has inserted a feedback module, activate it by removing dummy connection.
							auto dummy = plugs.back();
							dummy->connections.front()->connections.clear();
							dummy->connections.clear();

							// Feedback fixed, remove feedback trace.
							delete e;
							e = nullptr;

							// Continue as if nothing happened.
							goto done;
						}
						else
						{
							SetSortOrder(-1); // Allow this to be re-sorted after feedback (potentially) compensated.

							// If downstream module has feedback, add trace information.
							e->AddLine(p, to);
							if (e->feedbackConnectors.front().second->UG == this) // only reconstruct feedback loop as far as nesc.
							{
#if defined( _DEBUG )
								e->DebugDump();
#endif
								throw e;
							}
							return e;
						}
					}
					else
#endif
					{
						order = to.sort;// ->UG->GetSortOrder(); // now sorted. take into account.
					}
				}

				maxSort = (std::max)(maxSort, order);
			}
		}
	}

	++maxSort;

	assert(maxSort > -1);

	m.sort = maxSort;
	maxSortOrderGlobal = (std::max)(maxSort, maxSortOrderGlobal);
}

// ref ug_container::SortOrderSetup()
void SortModules(std::map<int32_t, moduleInfo>& allModules)
{
	int maxSortOrderGlobal = -1;

	for (auto& it : allModules)
	{
		auto& m = it.second;

		if (m.sort >= 0) // Skip sorted modules.
		{
			continue;
		}

		CalcSortOrder2(allModules, m, maxSortOrderGlobal);
	}
}


///////////////////////////////////////////////////////////
SubViewCadmium::~SubViewCadmium()
{
	Presenter()->GetPatchManager()->UnRegisterGui2(this);
}

int32_t SubViewCadmium::StartCableDrag(SynthEdit2::IViewChild* fromModule, int fromPin, Point dragStartPoint, bool isHeldAlt, SynthEdit2::CableType type)
{
	auto moduleview = dynamic_cast<SynthEdit2::ModuleView*>(this->getGuiHost());
	dragStartPoint += offset_;
	dragStartPoint += moduleview->OffsetToClient();

	return moduleview->parent->StartCableDrag(fromModule, fromPin, dragStartPoint, isHeldAlt);
}

void SubViewCadmium::OnCableDrag(SynthEdit2::ConnectorViewBase* dragline, GmpiDrawing::Point dragPoint, float& bestDistance, SynthEdit2::IViewChild*& bestModule, int& bestPinIndex)
{
	dragPoint -= offset_;

	for (auto it = children.rbegin(); it != children.rend(); ++it) // iterate in reverse for correct Z-Order.
	{
		(*it)->OnCableDrag(dragline, dragPoint, bestDistance, bestModule, bestPinIndex);
	}
}

bool SubViewCadmium::OnTimer()
{
	if (functionalUI.nextFrame())
	{
		invalidateRect();
		return true;
	}

	return false;
}

SubViewCadmium::SubViewCadmium(int pparentViewType) :
	parentViewType(pparentViewType)
{
	if (parentViewType == CF_PANEL_VIEW)
	{
		initializePin(0, showControlsLegacy, static_cast<MpGuiBaseMemberPtr2>(&SubViewCadmium::onValueChanged));
		// 1 : "Controls on Module" not needed.
		initializePin(2, showControls, static_cast<MpGuiBaseMemberPtr2>(&SubViewCadmium::onValueChanged));
		// 3 : "Ignore Program Change" not needed.
	}
	else
	{
		// Hack - Reroute Show-Controls-On-Module to Show-Controls-On-Panel
		initializePin(1, showControls, static_cast<MpGuiBaseMemberPtr2>(&SubViewCadmium::onValueChanged));
	}

	offset_.height = offset_.width = 0; // -99999; // un-initialized
}

void SubViewCadmium::onValueChanged()
{
	OnPatchCablesVisibilityUpdate();

	invalidateRect();
}

void SubViewCadmium::OnPatchCablesVisibilityUpdate()
{
	auto parent = dynamic_cast<SynthEdit2::ViewChild*> (getGuiHost());
	parent->parent->OnPatchCablesVisibilityUpdate();
}

int32_t SubViewCadmium::setCapture(SynthEdit2::IViewChild* module)
{
	// Avoid situation where some module turns off panel then captures mouse (ensuring it never can un-capture it).
	if (isShown())
	{
		return ViewBase::setCapture(module);
	}

	return gmpi::MP_FAIL;
}


int32_t MP_STDCALL SubViewCadmium::initialize()
{
	onValueChanged(); // patch-cables. nesc in case initial value is 0.

	int32_t x, y;
	Presenter()->GetViewScroll(x, y);

	offset_.width = static_cast<float>(x);
	offset_.height = static_cast<float>(y);

	if (x != -99999)
	{
		auto module = dynamic_cast<SynthEdit2::ViewChild*> (getGuiHost());
		offset_.width -= module->bounds_.left;
		offset_.height -= module->bounds_.top;
	}

	Presenter()->GetPatchManager()->RegisterGui2(this);

	return SynthEdit2::ViewBase::initialize();
}

int32_t SubViewCadmium::measure(GmpiDrawing_API::MP1_SIZE availableSize, GmpiDrawing_API::MP1_SIZE* returnDesiredSize)
{
	const float minSize = 12;

	returnDesiredSize->width = (std::max)(minSize, availableSize.width);
	returnDesiredSize->height = (std::max)(minSize, availableSize.height);

	return gmpi::MP_OK;

#if 0
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
	if (offset_.width == -99999.f)
	{
		offset_.width = static_cast<float>(-FastRealToIntFloor(viewBounds.left));
		offset_.height = static_cast<float>(-FastRealToIntFloor(viewBounds.top));

		// avoid 'show on module' structure view messing up panel view's offset.
		if (parentViewType == CF_PANEL_VIEW)
		{
			auto module = dynamic_cast<SynthEdit2::ViewChild*>(getGuiHost());
			Presenter()->SetViewScroll(
				static_cast<float>(offset_.width + module->bounds_.left),
				static_cast<float>(offset_.height + module->bounds_.top)
			);
		}
	}
	else
	{
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
	}
	return gmpi::MP_OK;
#endif
}

int32_t SubViewCadmium::arrange(GmpiDrawing_API::MP1_RECT finalRect)
{
	return ViewBase::arrange(finalRect);
}

bool SubViewCadmium::isShown()
{
	auto parent = dynamic_cast<SynthEdit2::ViewChild*> (getGuiHost());
	if (!parent->isShown())
		return false;

	if (parentViewType == CF_PANEL_VIEW)
		return showControls || showControlsLegacy;
	else
		return showControls;
}

int32_t SubViewCadmium::OnRender(GmpiDrawing_API::IMpDeviceContext* drawingContext)
{
	if (!isShown())
		return gmpi::MP_UNHANDLED;

	auto parent = dynamic_cast<SynthEdit2::ViewChild*> (getGuiHost());
	auto parentLayoutRect = parent->getLayoutRect();


	GmpiDrawing::Graphics g(drawingContext);

	ClipDrawingToBounds x(g, drawingBounds);

	// Transform to module-relative.
	const auto originalTransform = g.GetTransform();
	auto originToCenter = Matrix3x2::Translation(0.5f * drawingBounds.getWidth(), 0.5f * drawingBounds.getHeight());
	auto adjustedTransform =
//		Matrix3x2::Translation(offset_.width, offset_.height) *
//		Matrix3x2::Translation(0,0) * //parentLayoutRect.left /2, parentLayoutRect.top/2) *
		originalTransform *	originToCenter;

	g.SetTransform(adjustedTransform);

	// Render.
	//auto r = SynthEdit2::ViewBase::OnRender(drawingContext);

	for (auto& rendernode : renderNodes2)
	{
		std::get<RendererX>(rendernode->result.value).function(g);
	}

	// 'CD Render Function'
	// not optimized. just look for any node that produced a drawing function
	for (auto& n : functionalUI.graph)
	{
		auto drawFunc = std::get_if<RendererX>(&(n.result.value));
		if (drawFunc)
		{
			drawFunc->function(g);
		}
	}

	// Transform back.
	g.SetTransform(originalTransform);
	return gmpi::MP_OK;
}

int32_t SubViewCadmium::getToolTip(MP1_POINT point, gmpi::IString* returnString)
{
	return ViewBase::getToolTip(point, returnString);
}

int32_t SubViewCadmium::hitTest(GmpiDrawing_API::MP1_POINT point)
{
	if (isShown())
	{
/*
 
		point.x -= offset_.width;
		point.y -= offset_.height;
		for (auto it = children.rbegin(); it != children.rend(); ++it) // iterate in reverse for correct Z-Order.
		{
			auto m = (*it).get();
			if (m->hitTest(point))
			{
				return gmpi::MP_OK;
			}
		}
*/

		return gmpi::MP_OK;
	}

	return gmpi::MP_UNHANDLED;
}

int32_t SubViewCadmium::onPointerDown(int32_t flags, MP1_POINT point)
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

int32_t SubViewCadmium::onPointerMove(int32_t flags, MP1_POINT point)
{
	if (isShown())
	{
		auto originToCenter = Matrix3x2::Translation(-0.5f * drawingBounds.getWidth(), -0.5f * drawingBounds.getHeight());
		auto adjustedTransform =
//			Matrix3x2::Translation(-offset_.width, -offset_.height) *
			originToCenter;

		const auto lpoint = adjustedTransform.TransformPoint(point);

		functionalUI.onPointerMove(flags, lpoint);
		StartTimer();

		return ViewBase::onPointerMove(flags, point);
	}
	else
		return gmpi::MP_UNHANDLED;
}

int32_t SubViewCadmium::onPointerUp(int32_t flags, MP1_POINT point)
{
	if (isShown())
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

void SubViewCadmium::OnChildMoved()
{
	// !!!! Seems never called? (it don't nesc have any normal module children)

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

void SubViewCadmium::calcBounds(GmpiDrawing::Rect& returnLayoutRect, GmpiDrawing::Rect& returnClipRect)
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

void SubViewCadmium::BuildView(Json::Value* context)
{
	//TODO?		functionalUI.states2.clear();
	//TODO just make 'states2' and 'nodes' a member of me? no real need for seperate 'functionalUI' object.

	// stuff we need to pass in when building nodes.
	functionalUI::builder builder
	{
		functionalUI,
		Presenter()->GetPatchManager(),
		{},
		{},
		{}
	};

	auto document_json = *context;

	// sort modules (actually a 'dumb' representitive of the module)
	std::map<int32_t, moduleInfo> moduleSort;
	for (auto& module_json : document_json["connections"])
	{
		const auto fromHandle = module_json["fMod"].asInt();
		const auto toHandle = module_json["tMod"].asInt();
		const auto fromPin = module_json["fPin"].asInt(); // not needed while nodes have only one output? (always output node).
		const auto toPin = module_json["tPin"].asInt();

		moduleSort[fromHandle].connections.push_back(
		{
			toHandle,
			fromPin,
			toPin
			}
		);

		[[maybe_unused]] auto unused = moduleSort[toHandle].sort; // ensure we have a record for all 'to' modules.
	}

	SortModules(moduleSort);

#if 0 //def _DEBUG
	// print out sort result
	for (auto& it : moduleSort)
	{
		auto& m = it.second;
		_RPTN(0, "M %d : sort %d\n", it.first, m.sort);
	}
#endif

	for (auto& module_json : document_json["modules"])
	{
		const auto typeName = module_json["type"].asString();
		const auto handle = module_json["handle"].asInt();

		auto it = functionalUI::factory.find(typeName);
		size_t currentgraphsize{};
		if (it != functionalUI::factory.end())
		{
			auto& createNode = it->second;

			createNode(handle, module_json, builder);

			if (functionalUI.graph.size() != currentgraphsize)
			{
				currentgraphsize = functionalUI.graph.size();
				functionalUI.graph.back().debug_name = typeName;
			}
		}
	}

	// sort actual nodes in line with 'moduleSort' structure.
	std::sort(
		functionalUI.graph.begin(),
		functionalUI.graph.end(),
		[&moduleSort](const node& n1, const node& n2)
	{
		return moduleSort[n1.handle].sort > moduleSort[n2.handle].sort;
	}
	);


	std::unordered_map<int32_t, size_t> handleToIndex;
	for (int index = 0; index < functionalUI.graph.size(); ++index)
	{
		handleToIndex.insert({ functionalUI.graph[index].handle, index });
	}

	for (auto handle : builder.renderNodeHandles)
	{
		const auto index = handleToIndex[handle];
		renderNodes2.push_back(&functionalUI.graph[index]);
	}

	auto patchManager = Presenter()->GetPatchManager();
	for (auto it : builder.parameterHandles)
	{
/* old, directed to a graph node, that merely forwarded it.
		const auto moduleHandle = it.second;
		const auto parameterHandle = it.first;
		const auto index = handleToIndex[moduleHandle];
		nodeParameters.insert({ parameterHandle, &functionalUI.graph[index] });
*/
		// new: parameter is directed directly into an input state.
		const auto parameterHandle = it.first;
		const auto stateIndex = it.second;

		nodeParameters.insert({ parameterHandle, functionalUI.states2[stateIndex].get() });

		// init it's value
		patchManager->initializeGui(this, parameterHandle, MP_FT_VALUE);
	}
	
	for (auto& module_json : document_json["connections"])
	{
		const auto fromHandle = module_json["fMod"].asInt();
		const auto toHandle = module_json["tMod"].asInt();
		const auto fromPin = module_json["fPin"].asInt(); // not needed while nodes have only one output? (always output node).
		const auto toPin = module_json["tPin"].asInt();

		size_t toNodeIndex = SIZE_MAX;
		// look up regular node.
		if (auto it = handleToIndex.find(toHandle); it != handleToIndex.end())
		{
			toNodeIndex = (*it).second;
		}
		else
		{
			// special case for PatchMem, look up substitute node via temporary handle
			for (auto& proxy : builder.connectionProxiesNode)
			{
				if (proxy.first == toHandle)
				{
					auto it2 = handleToIndex.find(proxy.second);
					if (it2 != handleToIndex.end())
					{
						toNodeIndex = (*it2).second;
					}
					break;
				}
			}
		}

		if(toNodeIndex != SIZE_MAX)
		{
			std::vector<state_t*>* destArguments = {};
			destArguments = &functionalUI.graph[toNodeIndex].arguments;
			/* might be needed, probably not
						else
			{
				destArguments = &functionalUI.renderNodes[0].arguments;
			}
			*/
			// pad any inputs that have not been connected yet.
			// bug when first pin is output, ends up with null first arg
			while (destArguments->size() <= toPin)
			{
				destArguments->push_back({});
			}

			if (auto it2 = handleToIndex.find(fromHandle); it2 != handleToIndex.end())
			{
				const auto fromNodeIndex = it2->second;
				(*destArguments)[toPin] = &functionalUI.graph[fromNodeIndex].result;

				functionalUI.graph[fromNodeIndex].result.downstreamNodes.push_back(&functionalUI.graph[toNodeIndex]);
			}
			else
			{
				// from-module might not exist, but a plain state might take it's place (e.g. mouse pointer)
				for (auto& proxy : builder.connectionProxies)
				{
					if (proxy.moduleHandle == fromHandle && proxy.pinId == fromPin)
					{
						(*destArguments)[toPin] = functionalUI.states2[proxy.stateIndex].get();

						functionalUI.states2[proxy.stateIndex]->downstreamNodes.push_back(&functionalUI.graph[toNodeIndex]);

						break;
					}
				}
			}
		}
	}

	functionalUI.step();
}

int32_t SubViewCadmium::setParameter(int32_t parameterHandle, int32_t fieldId, int32_t voice, const void* data, int32_t size)
{
	if(fieldId != MP_FT_VALUE || size != sizeof(float))
		return MP_OK;

	auto it = nodeParameters.find(parameterHandle);

	if (it == nodeParameters.end())
		return MP_OK;
/*
	auto& node = *it->second;

	*node.arguments[0] = *(float*)data;
*/
	auto& state = *it->second;

	// this combo messy, wrap it somehow.
	functionalUI.updateState(state, *(float*)data);
	StartTimer();

	/*
	state = *(float*)data;

	functionalUI.step();
	invalidateRect(); //TESTIN!!!!
	*/
	return MP_OK;
}
