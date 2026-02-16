#include "Drawables.h"
#include "Utils.h"
#include "forms.h"

using namespace gmpi;
using namespace gmpi::drawing;

namespace gmpi::forms::primitive
{

void EditBox::Draw(gmpi::drawing::Graphics& g /*, Style style */) const
{
	auto fillBrush = g.createSolidColorBrush(Colors::White);
	auto outlineBrush = g.createSolidColorBrush(Colors::Black);
	auto textFormat = g.getFactory().createTextFormat();

	g.fillRectangle(bounds, fillBrush);
	g.drawRectangle(bounds, outlineBrush);

	g.drawTextU(text, textFormat, bounds, outlineBrush);
}

void Rectangle::Draw(gmpi::drawing::Graphics& g) const
{
	style->Init(g);

	g.fillRectangle(bounds, style->fill);
	g.drawRectangle(bounds, style->stroke);

	// Color the rectangle a little different on each redraw to get a visual indication of what is being invalidated.
#if 0 //def _DEBUG
	static uint32_t colors[] =
	{
		0xff0000,
		0x00ff00,
		0x0000ff,
		0xff00ff,
	};
	auto brush = g.createSolidColorBrush(gmpi::drawing::colorFromHex(0xcolors[redrawCount & 3], 0.1f));
	g.fillRectangle(bounds, brush);
	++redrawCount;
#endif
}

void RoundedRectangle::Draw(gmpi::drawing::Graphics& g) const
{
	style->Init(g);

	gmpi::drawing::RoundedRect roundedRect(bounds, radiusX, radiusY);

	g.fillRoundedRectangle(roundedRect, style->fill);
	g.drawRoundedRectangle(roundedRect, style->stroke);
}

void ShapeStyle::Init(gmpi::drawing::Graphics& g) const
{
	if (!fill.getBrush())
	{
		fill = g.createSolidColorBrush(fillColor);
		stroke = g.createSolidColorBrush(strokeColor);
	}
}

void TextBoxStyle::setLineSpacing(float pfixedLineSpacing)
{
	fixedLineSpacing = pfixedLineSpacing;
}

void TextBoxStyle::Draw(gmpi::drawing::Graphics& g, const TextBox& t) const
{
	if (!foreGround.getBrush())
	{
		backGround = g.createSolidColorBrush(backgroundColor);
		foreGround = g.createSolidColorBrush(foregroundColor);
		textFormat = g.getFactory().createTextFormat(bodyHeight);
		textFormat.setWordWrapping(WordWrapping::NoWrap);
		textFormat.setTextAlignment(static_cast<TextAlignment>(textAlignment));

		if (fixedLineSpacing)
			textFormat.setLineSpacing(fixedLineSpacing, 0.8f * fixedLineSpacing);
	}

	g.fillRectangle(t.bounds, backGround);
	g.drawTextU(t.text, textFormat, t.bounds, foreGround, DrawTextOptions::Clip);
}

void TextBox::Draw(gmpi::drawing::Graphics& g) const
{
	style->Draw(g, *this);
}

void Shape::Draw(gmpi::drawing::Graphics& g) const
{
	const auto originalTransform = g.getTransform();
	const auto adjustedTransform = originalTransform * makeTranslation({ bounds.left, bounds.top });
	g.setTransform(adjustedTransform);

	style->Draw(g, *this, *path);

	g.setTransform(originalTransform);
}

void ShapeStyle::Draw(gmpi::drawing::Graphics& g, const Shape& t, const PathHolder& path) const
{
	Init(g);
	if (!path.path)
	{
		path.path = g.getFactory().createPathGeometry();
		path.InitPath(path.path);
	}
	if (path.path)
	{
		g.fillGeometry(path.path, fill);
		g.drawGeometry(path.path, stroke, 1.f);
	}
}

void Circle::Draw(gmpi::drawing::Graphics& g /*, Style style */) const
{
	auto fillBrush = g.createSolidColorBrush(Colors::White);
	auto outlineBrush = g.createSolidColorBrush(Colors::Black);

	g.fillCircle(getCenter(bounds), getWidth(bounds) * 0.5f, fillBrush);
	g.drawCircle(getCenter(bounds), getWidth(bounds) * 0.5f, outlineBrush);
}

void Knob::Draw(gmpi::drawing::Graphics& g /*, Style style */) const
{
	auto fillBrush = g.createSolidColorBrush(Colors::White);
	auto outlineBrush = g.createSolidColorBrush(Colors::Black);

	const auto centerPoint = getCenter(bounds);

	g.fillCircle(centerPoint, getWidth(bounds) * 0.5f, fillBrush);
	g.drawCircle(centerPoint, getWidth(bounds) * 0.5f, outlineBrush);

	// 0.0 12 o'clock
	const auto turn = -0.3f + 0.6f * normalized;
	const auto radius = getWidth(bounds) * 0.3f;
	Point tickPoint(centerPoint.x + radius * gmpi_sdk::tsin(turn), centerPoint.y - radius * gmpi_sdk::tcos(turn));

	g.fillCircle(tickPoint, getWidth(bounds) * 0.1f, outlineBrush);
}

bool RectangleMouseTarget::HitTest(gmpi::drawing::Point p) const
{
	return pointInRect(p, bounds);
}

void RectangleMouseTarget::captureMouse(bool capture)
{
	// how to capture mouse? (currently dragging scrollbar works only so long as mouse is over narrow strip)
// need to get to form/environment whatever.		getEnvironment()->captureMouse(this, capture);

		// need to get topview to capture mouse on behalf of my identity, so even if this object is deleted, mouse capture is still valid, and topview will route it 
		// to my replacement.
	mouseParent->captureMouse(this);
}

void mouseList::onPointerMove(gmpi::drawing::Point p) const
{
	//		_RPTN(0, "---------------Portal::onPointerMove( %f, %f)\n", p.x, p.y);

	mousePoint = transformPoint(reverseTransform, p);
	const auto prevMouseOverObject = mouseOverObject;

	mouseOverObject = {};

	// iterate mouseTargets in reverse (to respect Z-order)
	for (auto t = lastMouseTarget.peerPrev; t != &firstMouseTarget; t = t->peerPrev)
	{
		if (t->HitTest(mousePoint) && t->wantsClicks())
		{
			mouseOverObject = t;
			break;
		}
	}

	if (prevMouseOverObject != mouseOverObject)
	{
#ifdef _DEBUG
		const char* prevName = prevMouseOverObject ? typeid(*prevMouseOverObject).name() : "<none>";
		const char* nextName = mouseOverObject ? typeid(*mouseOverObject).name() : "<none>";
		_RPTN(0, "%x MouseOverObject %s [%x] => %s [%x]\n", this, prevName, prevMouseOverObject, nextName, mouseOverObject);
#endif

		if (prevMouseOverObject)
			prevMouseOverObject->setHover(false);

		if (mouseOverObject)
			mouseOverObject->setHover(true);
	}

	if (mouseOverObject)
		mouseOverObject->onPointerMove(mousePoint);
}

void mouseList::restoreMouseState(Interactor* prevMouseOverObject) const
{
	mouseOverObject = {}; // might have been deleted

	for (auto t = lastMouseTarget.peerPrev; t != &firstMouseTarget; t = t->peerPrev)
	{
		if (t == prevMouseOverObject)
		{
			mouseOverObject = t; // no change
			break;
		}
	}

	// need to determine new mouse-over object
	onPointerMove(transformPoint(transform, mousePoint));
}

bool mouseList::HitTest(gmpi::drawing::Point p) const
{
	const auto point = transformPoint(reverseTransform, p);

	for (auto t = lastMouseTarget.peerPrev; t != &firstMouseTarget; t = t->peerPrev)
	{
		if (t->HitTest(point))
		{
			return true;
			break;
		}
	}

	return false;
}

bool mouseList::onPointerDown(const gmpi::forms::primitive::PointerEvent* e) const
{
	auto e2 = *e;
	e2.position = transformPoint(reverseTransform, e->position);

	// iterate mouseTargets in reverse (to respect Z-order)
	for (auto t = lastMouseTarget.peerPrev; t != &firstMouseTarget; t = t->peerPrev)
	{
		if (t->HitTest(e2.position) && t->onPointerDown(&e2))
			return true;
	}

	return false;
}

bool mouseList::onPointerUp(gmpi::drawing::Point p) const
{
	const auto point = transformPoint(reverseTransform, p);

	// iterate mouseTargets in reverse (to respect Z-order)
	for (auto t = lastMouseTarget.peerPrev; t != &firstMouseTarget; t = t->peerPrev)
	{
		if (t->HitTest(point) && t->onPointerUp(point))
			return true;
	}

	return false;
}

bool mouseList::setHover(bool isMouseOverMe) const
{
	isHovered = isMouseOverMe;
	if (!isMouseOverMe && mouseOverObject)
	{
		mouseOverObject->setHover(false);
		mouseOverObject = {};
	}

	return true;
}

bool mouseList::onMouseWheel(int32_t flags, int32_t delta, gmpi::drawing::Point point) const
{
	for (auto t = lastMouseTarget.peerPrev; t != &firstMouseTarget; t = t->peerPrev)
	{
		if (t->HitTest(point) && t->onMouseWheel(flags, delta, point))
			return true;
	}

	return false;
}

///////////////////////////////////////////////////////////////////////

} // namespace gmpi::forms::primitive

namespace gmpi::forms::primitive
{
Portal::Portal(gmpi::drawing::Rect pbounds) : RectangularVisual(pbounds)
{
	firstVisual.peerNext = &lastVisual;
	lastVisual.peerPrev = &firstVisual;
}

Portal::~Portal()
{
//	clearChildren();
}

gmpi::drawing::Rect Portal::getContentRect() const
{
	if (isNull(contentBounds))
	{
		for (auto v = firstVisual.peerNext; v != &lastVisual; v = v->peerNext)
		{
			if (isNull(contentBounds))
				contentBounds = v->LayoutRect();
			else
				contentBounds = unionRect(contentBounds, v->LayoutRect());
		}
	}

	return contentBounds;
}

void Portal::setScroll(float dx, float dy)
{
	transform = makeTranslation({ bounds.left + dx, bounds.top + dy });
	reverseTransform = makeTranslation({ -bounds.left - dx, -bounds.top - dy });
	Invalidate(bounds);
}

void Portal::Draw(gmpi::drawing::Graphics& g) const
{
	const auto originalTransform = g.getTransform();
	const auto adjustedTransform = transform * originalTransform;
	g.setTransform(adjustedTransform);

	const auto clipRect = g.getAxisAlignedClip();

	for (auto v = firstVisual.peerNext; v != &lastVisual; v = v->peerNext)
	{
		const auto childRect = v->ClipRect();
		if (!gmpi::drawing::empty(intersectRect(clipRect, childRect)))
		{
			v->Draw(g);
		}
	}

	g.setTransform(originalTransform);
}

void Portal::Invalidate(gmpi::drawing::Rect r) const
{
	if (!parent) // still under construction
		return;

	auto r2 = transformRect(reverseTransform, r);
	parent->Invalidate(r2);
}

/////////////////////////////////////////////////////////////////////////////////

MousePortal::MousePortal(gmpi::drawing::Rect pbounds) : RectangularInteractor(pbounds)
{
	firstMouseTarget.peerNext = &lastMouseTarget;
	lastMouseTarget.peerPrev = &firstMouseTarget;
}

Interactor* MousePortal::saveMouseState() const
{
	return mouseList::saveMouseState();
}

void MousePortal::restoreMouseState(Interactor* prevMouseOverObject)
{
	mouseList::restoreMouseState(prevMouseOverObject);
}

void MousePortal::setScroll(float dx, float dy)
{
	const auto originalMouse = transformPoint(transform, mousePoint);

	transform = makeTranslation({ bounds.left + dx, bounds.top + dy });
	reverseTransform = makeTranslation({ -bounds.left - dx, -bounds.top - dy });

	// recalulate hovered object.
	if(isHovered)
		onPointerMove(originalMouse);
}

bool MousePortal::HitTest(gmpi::drawing::Point p) const
{
	return mouseList::HitTest(p);
}

bool MousePortal::onPointerDown(const gmpi::forms::primitive::PointerEvent* e) const
{
	return mouseList::onPointerDown(e);
}

bool MousePortal::onPointerUp(gmpi::drawing::Point p) const
{
	return mouseList::onPointerUp(p);
}

bool MousePortal::onPointerMove(gmpi::drawing::Point p) const
{
	mouseList::onPointerMove(p);

	return mouseOverObject != nullptr;
}

bool MousePortal::setHover(bool isMouseOverMe) const
{
	return mouseList::setHover(isMouseOverMe);
}

bool MousePortal::onMouseWheel(int32_t flags, int32_t delta, gmpi::drawing::Point point) const
{
	return mouseList::onMouseWheel(flags, delta, point);
}

void Visual::Invalidate(gmpi::drawing::Rect r) const
{
	assert(parent);
	parent->Invalidate(r);
}

} // namespace gmpi::forms::primitive

