#include "forms.h"
#include "builders.h"
#include "helpers/unicode_conversion.h"

using namespace gmpi::forms;

namespace gmpi::ui
{

Form::Form()
{
	startTimer(16);
}

void Form::Invalidate(gmpi::drawing::Rect r) const
{
	if (host)
		host->invalidateRect(&r);
}

void Form::redraw()
{
	if (host)
		host->invalidateRect(nullptr);
}

bool Form::onTimer()
{
	DoUpdates();
	return true;
}

void Form::restoreMouseState(gmpi::forms::primitive::Interactor* prevMouseOverObject)
{
	mouseList::restoreMouseState(prevMouseOverObject);
}

// map current state => drawables
void Form::renderVisuals()
{
	gmpi::forms::primitive::displayList::clear();
	gmpi::forms::primitive::mouseList::clear();
	gmpi::ui::builder::ViewParent::clear();

	{
		gmpi::ui::builder::ThreadLocalCurrentBuilder = this;

		Body(); // creates factories

		gmpi::ui::builder::ThreadLocalCurrentBuilder = {};

		doChildLayout();
	}

	DoUpdates();
}

void Form::DoUpdates()
{
	// why dialog host part of env, perhaps move it.

	bool childWasDirty = false;
	for (auto& view : childViews)
		childWasDirty |= view->RenderIfDirty(&env, *this, *this);

	restoreMouseState(mouseOverObject);
}

gmpi::ReturnCode Form::render(gmpi::drawing::api::IDeviceContext* drawingContext)
{
	//const auto originalTransform = g.getTransform();
	//const auto adjustedTransform = transform * originalTransform;
	//g.setTransform(adjustedTransform);
	gmpi::drawing::Graphics g(drawingContext);

	const auto clipRect = g.getAxisAlignedClip();

	for (auto v = firstVisual.peerNext; v != &lastVisual; v = v->peerNext)
	{
		const auto childRect = v->ClipRect();
		if (!gmpi::drawing::empty(intersectRect(clipRect, childRect)))
		{
			v->Draw(g);
		}
	}

//	g.setTransform(originalTransform);

	return gmpi::ReturnCode::Ok;
}

gmpi::ReturnCode Form::onPointerDown(gmpi::drawing::Point point, int32_t flags)
{
	mousePoint = point;
	gmpi::forms::primitive::PointerEvent e{ point, flags, this };
	mouseList::onPointerDown(&e);
	return gmpi::ReturnCode::Ok;
}

gmpi::ReturnCode Form::onPointerMove(gmpi::drawing::Point p, int32_t flags)
{
	const auto delta = gmpi::drawing::Size{ p.x - mousePoint.x, p.y - mousePoint.y };
	mousePoint = p;
	if (onMouseMove)
	{
		onMouseMove(delta);
	}
	else
		mouseList::onPointerMove(p);

	return gmpi::ReturnCode::Ok;
}

gmpi::ReturnCode Form::onPointerUp(gmpi::drawing::Point point, int32_t flags)
{
	// release mouse capture if we have it, and route event to mouse target if we don't.
	if (onMouseMove)
		onMouseMove = {};
	else
		mouseList::onPointerUp(point);

	return gmpi::ReturnCode::Ok;
}

gmpi::ReturnCode Form::onMouseWheel(gmpi::drawing::Point point, int32_t flags, int32_t delta)
{
	mouseList::onMouseWheel(flags, delta, point);

	return gmpi::ReturnCode::Ok;
}

gmpi::ReturnCode Form::setHover(bool isMouseOverMe)
{
	mouseList::setHover(isMouseOverMe);
	return gmpi::ReturnCode::Ok;
}

void Form::setKeyboardHandler(std::function<void(std::string glyph)> pOnKey)
{
	onKey = pOnKey;
}

gmpi::ReturnCode Form::setHost(IUnknown* phost)
{
	if (!phost)
	{
		host = {};
		inputhost = {};
		return gmpi::ReturnCode::Ok;
	}

	phost->queryInterface(&gmpi::api::IDrawingHost::guid, reinterpret_cast<void**>(&host));
	phost->queryInterface(&gmpi::api::IInputHost::guid, reinterpret_cast<void**>(&inputhost));
	return gmpi::ReturnCode::Ok;
}

gmpi::ReturnCode Form::open(gmpi::api::IUnknown* phost)
{
	phost->queryInterface(&gmpi::api::IDrawingHost::guid, reinterpret_cast<void**>(&host));
	phost->queryInterface(&gmpi::api::IInputHost::guid, reinterpret_cast<void**>(&inputhost));
	host->queryInterface(&gmpi::api::IDialogHost::guid, env.dialogHost.put_void());

	return gmpi::ReturnCode::Ok;
}

gmpi::ReturnCode Form::onKeyPress(wchar_t c)
{
	if (!onKey)
		return gmpi::ReturnCode::Ok;

	wchar_t temp[2] = { c, 0 };

	const auto u8string = gmpi::unicode::to_utf8(temp);

	onKey(u8string);

	return gmpi::ReturnCode::Ok;
}

ScrollPortal::ScrollPortal(gmpi::drawing::Rect pbounds)
{
	saveParent = gmpi::ui::builder::ThreadLocalCurrentBuilder;
	auto& result = *gmpi::ui::builder::ThreadLocalCurrentBuilder;
	bounds = pbounds;

	auto portal = std::make_unique<builder::ScrollPortal>(bounds);

	gmpi::ui::builder::ThreadLocalCurrentBuilder = portal.get(); // &(portal->childViews);

	result.push_back(std::move(portal));
}

ScrollPortal::~ScrollPortal()
{
	gmpi::ui::builder::ThreadLocalCurrentBuilder = saveParent;
}

Spacer::Spacer(gmpi::drawing::Rect bounds)
{
	auto& result = *gmpi::ui::builder::ThreadLocalCurrentBuilder;
	view = new gmpi::ui::builder::Seperator(bounds);
	result.push_back(std::unique_ptr<gmpi::ui::builder::Seperator>(view));
}

ToggleSwitch::ToggleSwitch(
	std::string_view labelText,
	gmpi_forms::State<bool>& value
)
{
	auto& result = *gmpi::ui::builder::ThreadLocalCurrentBuilder;

	auto temp = std::make_unique<gmpi::ui::builder::ToggleSwitch>(labelText, value);
	view = temp.get();

	result.push_back(std::move(temp));
}

Label::Label(std::string_view text, gmpi::drawing::Rect bounds)
{
	auto& result = *gmpi::ui::builder::ThreadLocalCurrentBuilder;

	auto label = std::make_unique<gmpi::ui::builder::TextLabelView>();
	
	// own the state myself. Todo constructor that accepts state directly.
	auto state = std::make_unique< gmpi_forms::State<std::string> >();
	label->text2.setSource(state.get());
	state->set((std::string)text);
	label->selfOwnedStates.push_back(std::move(state));

	label->bounds = bounds;
	view = label.get();
	result.push_back(std::move(label));
}

Label::Label(gmpi::drawing::Rect bounds)
{
	auto& result = *gmpi::ui::builder::ThreadLocalCurrentBuilder;
	auto label = std::make_unique<gmpi::ui::builder::TextLabelView>();
	label->bounds = bounds;
	view = label.get();
	result.push_back(std::move(label));
}

ComboBox::ComboBox(gmpi::drawing::Rect bounds)
{
	auto& result = *gmpi::ui::builder::ThreadLocalCurrentBuilder;
	auto combo = std::make_unique<gmpi::ui::builder::ComboBoxView>();
	combo->bounds = bounds;
	view = combo.get();
	result.push_back(std::move(combo));
}

TextEdit::TextEdit(gmpi_forms::State<std::string>& pname)
{
	name.addObserver([this]()
		{
			view->setDirty();
		});

	auto editor = std::make_unique<gmpi::ui::builder::TextEditView>();
	view = editor.get();
	view->text.setSource(&pname);

	auto& result = *gmpi::ui::builder::ThreadLocalCurrentBuilder;
	result.push_back(std::move(editor));
}

Grid::Grid(
	  gmpi::ui::builder::ViewParent::Initializer init
	, gmpi::drawing::Rect pbounds
)
{
	saveParent = gmpi::ui::builder::ThreadLocalCurrentBuilder;

	auto portal = std::make_unique<builder::Grid>(init, pbounds);

	gmpi::ui::builder::ThreadLocalCurrentBuilder = portal.get();

	saveParent->push_back(std::move(portal));
}

Grid::Grid(
	gmpi::ui::builder::ViewParent::Initializer init
)
{
	saveParent = gmpi::ui::builder::ThreadLocalCurrentBuilder;

	auto portal = std::make_unique<builder::Grid>(init, gmpi::drawing::Rect{});

	gmpi::ui::builder::ThreadLocalCurrentBuilder = portal.get();

	saveParent->push_back(std::move(portal));
}



//Grid::Grid()
//{
//    saveParent = gmpi::ui::builder::ThreadLocalCurrentBuilder;
//    gmpi::ui::builder::ThreadLocalCurrentBuilder = &childViews;
//}

Grid::~Grid()
{
	gmpi::ui::builder::ThreadLocalCurrentBuilder = saveParent;
//	if (!saveParent)
//		return;

//	doLayout();

	//	childViews.clear();
}

#if 0
void Grid::doLayout()
{
	if (layoutDone)
		return;

	auto& result = *saveParent;

	const auto childCount = static_cast<float>(childViews.size());
	const bool autoFlowRows = spec.auto_flow == eAutoFlow::rows;
	const bool size_to_children = autoFlowRows ? spec.auto_rows > 0.0f : spec.auto_columns > 0.0f;

	const auto itemExtent =
		autoFlowRows ?
		(
			spec.auto_rows > 0.0f ? spec.auto_rows :
			childCount > 0 ? (getHeight(bounds) - spec.gap * (childCount - 1)) / childCount : 0.0f
		) :
		(
			spec.auto_columns > 0.0f ? spec.auto_columns :
			childCount > 0 ? (getWidth(bounds) - spec.gap * (childCount - 1)) / childCount : 0.0f
		);

	auto childRect = bounds;

	// stack children in the selected auto-flow direction.
	int index = 0;
	for (auto& child : childViews)
	{
		if (autoFlowRows)
		{
			if (spec.auto_columns > 0.0f)
			{
				childRect.right = childRect.left + spec.auto_columns;
			}

			childRect.bottom = childRect.top + itemExtent;
		}
		else
		{
			if (spec.auto_rows > 0.0f)
			{
				childRect.bottom = childRect.top + spec.auto_rows;
			}

			childRect.right = childRect.left + itemExtent;
		}

		child->setBounds(childRect);

		if (autoFlowRows)
		{
			childRect.top = childRect.bottom + spec.gap;
		}
		else
		{
			childRect.left = childRect.right + spec.gap;
		}

		_RPTN(0, "child %2d : (LTRB) %f,%f,%f,%f\n", index, childRect.left, childRect.top, childRect.right, childRect.bottom);

		result.push_back(std::move(child));
		++index;
	}

	if(size_to_children)
	{
		if (autoFlowRows)
			bounds.bottom = childRect.top - spec.gap; // size to fit children, removing the last gap.
		else
			bounds.right = childRect.left - spec.gap; // size to fit children, removing the last gap.
	}

	childViews.clear(); // they have been moved away.

	layoutDone = true;
}
#endif

} // namespace gmpi_form_builder

