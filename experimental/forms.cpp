#include "forms.h"
#include "builders.h"
#include "helpers/unicode_conversion.h"
#include "theme.h"

using namespace gmpi::forms;

namespace gmpi::ui
{

Form::Form()
{
	startTimer(16);
}

void Form::Invalidate(gmpi::drawing::Rect r) const
{
	if (!host.isNull())
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

void Form::renderBackground(gmpi::drawing::Graphics& g)
{
	if (!background)
		return;

	auto brush = g.createSolidColorBrush(currentTheme().*background);
	g.fillRectangle(gmpi::drawing::empty(bounds) ? g.getAxisAlignedClip() : bounds, brush);
}

gmpi::ReturnCode Form::measure(const gmpi::drawing::Size* availableSize, gmpi::drawing::Size* returnDesiredSize)
{
	*returnDesiredSize = *availableSize;
	return gmpi::ReturnCode::Ok;
}

gmpi::ReturnCode Form::arrange(const gmpi::drawing::Rect* finalRect)
{
	if (*finalRect != bounds)
	{
		bounds = *finalRect;
		invalidateView();
	}
	return gmpi::ReturnCode::Ok;
}

gmpi::ReturnCode Form::getClipArea(gmpi::drawing::Rect* returnRect)
{
	*returnRect = bounds;
	return gmpi::ReturnCode::Ok;
}

void Form::invalidateView()
{
	viewInvalid = true;
	redraw();
}

void Form::scrollToTop(std::string_view portalName)
{
	const auto path = portalName.empty() ? std::string("ScrollPortal.scroll") : "ScrollPortal.scroll." + std::string(portalName);
	if (auto scroll = dynamic_cast<gmpi_forms::State<float>*>(env.findState(path)))
		scroll->set(0.0f);
}

// map current state => drawables
void Form::renderVisuals()
{
	viewInvalid = false;
	builtTheme = themeModeStorage();
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
	// Check if theme changed and mark all views dirty if so
	if (gmpi::ui::consumeThemeChanged(lastSeenThemeVersion_))
	{
		for (auto& view : childViews)
			view->setDirty();
	}

	// why dialog host part of env, perhaps move it.

	// Fast out: nothing in the tree is dirty — skip the whole traversal.
	if (!childDirty)
		return;

	bool childWasDirty = false;
	for (auto& view : childViews)
		childWasDirty |= view->RenderIfDirty(&env, *this, *this);

	childDirty = false;
	restoreMouseState(mouseOverObject);
}

gmpi::ReturnCode Form::render(gmpi::drawing::api::IDeviceContext* drawingContext)
{
	//const auto originalTransform = g.getTransform();
	//const auto adjustedTransform = transform * originalTransform;
	//g.setTransform(adjustedTransform);
	// A theme change recolours everything, so it rebuilds like a model change. It is noticed only here:
	// setThemeMode() repaints nothing, so an idle form keeps its colours until something invalidates it.
	if (viewInvalid || themeModeStorage() != builtTheme)
		renderVisuals();

	gmpi::drawing::Graphics g(drawingContext);

	std::optional<gmpi::drawing::ClipDrawingToBounds> clip;
	if (!gmpi::drawing::empty(bounds))
		clip.emplace(g, bounds);

	renderBackground(g);

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
		env.dialogHost = {};
		return gmpi::ReturnCode::Ok;
	}

	phost->queryInterface(&gmpi::api::IDrawingHost::guid, host.put_void());
	phost->queryInterface(&gmpi::api::IInputHost::guid, inputhost.put_void());
	phost->queryInterface(&gmpi::api::IDialogHost::guid, env.dialogHost.put_void());
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

ScrollPortal::ScrollPortal(gmpi::drawing::Rect pbounds, std::string pname)
{
	saveParent = gmpi::ui::builder::ThreadLocalCurrentBuilder;
	auto& result = *gmpi::ui::builder::ThreadLocalCurrentBuilder;
	bounds = pbounds;

	auto portal = std::make_unique<builder::ScrollPortal>(bounds, std::move(pname));

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

namespace
{
thread_local const Page* currentPage{};

// a widget declared with no bounds is one row of the Page it is in
gmpi::drawing::Rect rowBounds(gmpi::drawing::Rect bounds = {})
{
	if (bounds == gmpi::drawing::Rect{} && currentPage)
		bounds.bottom = currentPage->style.rowHeight;
	return bounds;
}

builder::ViewParent::Initializer labeledRowSpec()
{
	const PageStyle style = currentPage ? currentPage->style : PageStyle{};
	const float labelWidth = currentPage ? currentPage->labelWidth : 100.0f;
	return { .gap = style.gap, .auto_rows = style.rowHeight, .auto_flow = builder::ViewParent::eAutoFlow::columns, .column_widths = { labelWidth, builder::fr(1.0f) } };
}

template<typename V>
V* add(std::unique_ptr<V> view)
{
	auto* raw = view.get();
	builder::ThreadLocalCurrentBuilder->push_back(std::move(view));
	return raw;
}
} // namespace

Page::Page(gmpi::drawing::Rect bounds, PageStyle pstyle) : outer(currentPage), style(std::move(pstyle))
{
	constexpr float scrollBarWidth = 12.0f; // builder::ScrollPortal's

	auto text = bounds;
	text.left += style.margin;
	text.top += style.margin;
	text.right -= style.margin;
	text.bottom -= style.margin;

	if (style.scrolls)
	{
		portal.emplace(bounds, style.scrollName);
		text.right -= scrollBarWidth;
	}

	labelWidth = style.labelFraction * getWidth(text);
	rows.emplace(builder::ViewParent::Initializer{ .gap = style.gap, .auto_flow = builder::ViewParent::eAutoFlow::rows, .default_track_size = builder::auto_size() }, text);

	currentPage = this;
}

Page::~Page()
{
	currentPage = outer;
}

const Page* Page::current()
{
	return currentPage;
}

LabeledRow::LabeledRow() : grid(labeledRowSpec(), {})
{
}

LabeledRow::LabeledRow(std::string_view label) : LabeledRow()
{
	Label _(label);
}

Heading::Heading(std::string_view text)
{
	Label label(text, { 0.0f, 0.0f, 0.0f, currentPage ? currentPage->style.headingHeight : 14.0f });
	view = label.view;
}

Label::Label(std::string_view text, gmpi::drawing::Rect bounds)
{
	auto label = std::make_unique<gmpi::ui::builder::TextLabelView>();
	label->text2.own(std::string(text));
	label->bounds = rowBounds(bounds);
	view = add(std::move(label));
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

ComboBox::ComboBox(const std::vector<Item>& items, std::optional<int32_t> value, std::function<void(int32_t)> commit)
{
	std::string enumList; // "text=id,..."
	for (const auto& item : items)
		enumList += (enumList.empty() ? "" : ",") + item.text + "=" + std::to_string(item.id);

	auto combo = std::make_unique<gmpi::ui::builder::ComboBoxView>();
	combo->enum_list.own(std::move(enumList));
	combo->enum_value.own(value.value_or(0));
	combo->mixed = !value;
	combo->validateAndSave = std::move(commit);
	combo->bounds = rowBounds();
	view = add(std::move(combo));
}

TickBox::TickBox(std::optional<bool> value, std::function<void(bool)> commit)
{
	auto tick = std::make_unique<gmpi::ui::builder::TickBox>();
	tick->value.own(value.value_or(false));
	tick->mixed = !value;
	tick->validateAndSave = std::move(commit);
	tick->bounds = rowBounds();
	view = add(std::move(tick));
}

Button::Button(std::string_view caption, gmpi::drawing::Rect bounds)
{
	auto& result = *gmpi::ui::builder::ThreadLocalCurrentBuilder;
	auto button = std::make_unique<gmpi::ui::builder::ButtonView>(caption, bounds);
	view = button.get();
	result.push_back(std::move(button));
}

ProgressBar::ProgressBar(gmpi::drawing::Rect bounds)
{
	auto& result = *gmpi::ui::builder::ThreadLocalCurrentBuilder;
	auto bar = std::make_unique<gmpi::ui::builder::ProgressBarView>(bounds);
	view = bar.get();
	result.push_back(std::move(bar));
}

TabBar::TabBar(std::vector<std::string> titles, gmpi::drawing::Rect bounds)
{
	auto& result = *gmpi::ui::builder::ThreadLocalCurrentBuilder;
	auto tabs = std::make_unique<gmpi::ui::builder::TabBarView>(std::move(titles), bounds);
	view = tabs.get();
	result.push_back(std::move(tabs));
}

TextEdit::TextEdit(
	gmpi_forms::State<std::string>& pname
	, std::function <void(const std::string&)> validateAndSave
	)
{
	name.addObserver([this]()
		{
			view->setDirty();
		});

	auto editor = std::make_unique<gmpi::ui::builder::TextEditView>();
	view = editor.get();
	view->text.setSource(&pname);
	view->validateAndSave = validateAndSave;

	auto& result = *gmpi::ui::builder::ThreadLocalCurrentBuilder;
	result.push_back(std::move(editor));
}

TextEdit::TextEdit(std::optional<std::string> value, std::function<void(const std::string&)> commit)
{
	auto editor = std::make_unique<gmpi::ui::builder::TextEditView>();
	editor->mixed = !value;
	editor->text.own(value ? std::move(*value) : std::string{});
	editor->validateAndSave = std::move(commit);
	editor->bounds = rowBounds();
	view = add(std::move(editor));
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

