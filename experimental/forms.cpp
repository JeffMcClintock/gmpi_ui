#include "forms.h"
#include "ComponentBuilders.h"
#include "modules/shared/unicode_conversion.h"

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

void Form::DoUpdates()
{
	// todo make a base class for 'thing that has visual/mousetarget list) for Form and also for ScrollPortal 
	// why dialog host part of env, perhaps move it.

	bool childWasDirty = false;
	for (auto& view : childViews)
		childWasDirty |= view->RenderIfDirty(&env, *this, *this);

	restoreMouseState(mouseOverObject);
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
		gmpi::ui::builder::Builder builder(childViews);
		Body(); // creates factories
	}

	DoUpdates();
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
	assert(false); // is this redundant???
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

	const auto u8string = JmUnicodeConversions::WStringToUtf8(temp);

	onKey(u8string);

	return gmpi::ReturnCode::Ok;
}

ScrollPortal::ScrollPortal(gmpi::drawing::Rect pbounds)
{
	saveParent = gmpi::ui::builder::ThreadLocalCurrentBuilder;
	auto& result = *gmpi::ui::builder::ThreadLocalCurrentBuilder;
	bounds = pbounds;

	auto portal = std::make_unique<builder::ScrollPortal>(bounds);

	gmpi::ui::builder::ThreadLocalCurrentBuilder = &(portal->childViews);

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

ToggleSwitch_old::ToggleSwitch_old(
	std::string_view labelText,
	gmpi::drawing::Rect bounds,
	std::function<bool()> getValue,
	std::function<void(bool)> setValue
)
{
	auto& result = *gmpi::ui::builder::ThreadLocalCurrentBuilder;

	auto label = std::make_unique<gmpi::ui::builder::TextLabelView>(labelText);
	auto toggleSwitch = std::make_unique<gmpi::ui::builder::TickBox>();

	toggleSwitch->bounds = bounds; toggleSwitch->bounds.left = toggleSwitch->bounds.right - getHeight(bounds); // { right - colHeight, y, right, y + colHeight };
	label->bounds = bounds; label->bounds.right = toggleSwitch->bounds.left - 3; // { left, y, toggleSwitch->bounds.left - 3.0f, y + colHeight };

	toggleSwitch->value = std::make_unique< gmpi::ui::builder::ValueObserverLambda<bool> >(
		toggleSwitch.get(),
		std::move(getValue),
		[setValue = std::move(setValue), view = toggleSwitch.get()](bool newval) -> void
		{
			setValue(newval);
			view->setDirty();
		}
	);

	result.push_back(std::move(label));
	result.push_back(std::move(toggleSwitch));
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
	auto label = std::make_unique<gmpi::ui::builder::TextLabelView>(text);
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
	auto editor = std::make_unique<gmpi::ui::builder::TextEditView>();
	view = editor.get();
	view->text.setSource(&pname);

	auto& result = *gmpi::ui::builder::ThreadLocalCurrentBuilder;
	result.push_back(std::move(editor));
}

Grid::Grid(gmpi::drawing::Rect pbounds, float pspacing)
	: bounds(pbounds)
	, gap(pspacing)
{
	saveParent = gmpi::ui::builder::ThreadLocalCurrentBuilder;
	gmpi::ui::builder::ThreadLocalCurrentBuilder = &childViews;
}

Grid::~Grid()
{
	gmpi::ui::builder::ThreadLocalCurrentBuilder = saveParent;
	if (!saveParent)
		return;

	auto& result = *saveParent;

	const auto childCount = static_cast<float>(childViews.size());
	const auto rowHeight = childCount > 0 ? (getHeight(bounds) - gap * (childCount - 1)) / childCount : 0.0f;

	auto childRect = bounds;

	for (auto& child : childViews)
	{
		childRect.bottom = childRect.top + rowHeight;
		child->setBounds(childRect);

		childRect.top = childRect.bottom + gap;
		result.push_back(std::move(child));
	}

	childViews.clear();
}
} // namespace gmpi_form_builder

