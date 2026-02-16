#include "Drawables.h"
#include "Utils.h"
//#include "modules/shared/unicode_conversion.h"
//#include "ModuleBrowser.h"

using namespace gmpi;
using namespace gmpi::drawing;

namespace gmpi_forms
{
#if 0
	void TextEditWidget::renderVisuals(Form& environment)
	{
		environment.child.add(
			new EditBox(bounds, environment.state.filterText)
		);

		environment.child.add(
			new RectangleMouseTarget(bounds)
		);
#if 0
		environment.child.mouseTargets.back()->onPointerDown = [&environment](gmpi::drawing::Point p)
		{
			_RPT0(0, "Text Edit Clicked\n");
			environment.setKeyboardHandler(
				[&environment](std::string glyph)
				{
					auto& text = environment.state.filterText;

					if (glyph == "\b")
					{
						if (!text.empty())
							text = text.substr(0, text.size() - 1);
					}
					else
					{
						text += glyph;
					}
					//				environment.updateFilter(environment.state.editText);
					environment.renderVisuals();
				}
			);
		};
#endif
	}
#endif

	void EditBox::Draw(gmpi::drawing::Graphics& g /*, Style style */) const
	{
		auto fillBrush = g.createSolidColorBrush(Colors::White);
		auto outlineBrush = g.createSolidColorBrush(Colors::Black);
		auto textFormat = g.getFactory().createTextFormat();

		g.fillRectangle(bounds, fillBrush);
		g.drawRectangle(bounds, outlineBrush);

		g.drawTextU(text, textFormat, bounds, outlineBrush);
	}

#if 0
	void KnobWidget::renderVisuals(Form& environment)
	{
#if 0
		environment.views.push_back(
			std::make_unique<Knob>(bounds, environment.state.knbNormalized)
		);

		environment.mouseTargets.push_back(
			std::make_unique<RectangleMouseTarget>(bounds)
		);

		environment.mouseTargets.back()->onPointerDown = [&environment](gmpi::drawing::Point p)
		{
			environment.capturePointer(
				[&environment](gmpi::drawing::Size delta)
				{
					environment.state.knbNormalized = std::clamp(environment.state.knbNormalized - delta.height * 0.01f, 0.0f, 1.0f);
					environment.redraw();
				}
			);

			_RPT0(0, "Knob Clicked\n");
		};
#endif
	}
#endif

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

	void ScrollBar::Draw(gmpi::drawing::Graphics& g) const
	{
		style->Init(g);

		const float normalized = 0.0f; // TODO position.get() / scrollRangePixels;

		// total height of content is the visible area (getHeight(bounds)) plus the scroll range (scrollRangePixels)
		const float handleHeight = getHeight(bounds) * getHeight(bounds) / (getHeight(bounds) - scrollRangePixels);
		auto scrollRect = bounds;
		scrollRect.top = (getHeight(scrollRect) - handleHeight) * normalized;
		scrollRect.bottom = scrollRect.top + handleHeight;
		g.fillRectangle(scrollRect, style->fill);
	}
	
#if 0
	void ListView::Draw(gmpi::drawing::Graphics& g /*, Style style */) const
	{
		auto fillBrush = g.createSolidColorBrush(gmpi::drawing::colorFromHex(0x003E3E3Eu));
		auto outlineBrush = g.createSolidColorBrush(Colors::White);
		auto textFormat = g.getFactory().createTextFormat();
		textFormat.SetWordWrapping(WordWrapping::NoWrap);

		g.fillRectangle(bounds, fillBrush);
		//	g.drawRectangle(bounds, outlineBrush);

		auto textRect = bounds;
		textRect.left += 2;
		textRect.bottom = textRect.top + 16;

		for (auto& text : data)
		{
			g.drawTextU(text.second.c_str(), textFormat, textRect, outlineBrush, DrawTextOptions::Clip);

			textRect.top += 16;
			textRect.bottom += 16;
		}
	}
	bool ListView::HitTest(gmpi::drawing::Point p) const
	{
		return bounds.ContainsPoint(p);
	}
#if 1

	bool ListView::onPointerDown(gmpi::drawing::Point p) const
	{
		if (onItemSelected)
		{
			p.x -= bounds.left;
			p.y -= bounds.top;

			const auto itemIndex = static_cast<int>(p.y) / 16;

			onItemSelected(data[itemIndex]);
		}

		return true;
	}
#endif
#endif

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

			if(fixedLineSpacing)
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
		form->captureMouse(this);
	}


	///////////////////////////////////////////////////////////////////////

	ViewPort::ViewPort()
	{
		firstVisual.peerNext = &lastVisual;
		lastVisual.peerPrev = &firstVisual;
		firstMouseTarget.peerNext = &lastMouseTarget;
		lastMouseTarget.peerPrev = &firstMouseTarget;
	}

	ViewPort::~ViewPort()
	{
		clearChildren();
	}

	void ViewPort::clearChildren()
	{
		mouseOverObject = {};
//		mouseTargets.clear();
//		visuals.clear();

#if 0
		// delete all nodes between sentinels
		Child* obj = firstChild.peerNext;
		while (obj && obj != &lastChild)
		{
			Child* next = obj->peerNext;
			delete obj;
			obj = next;
		}

		firstChild.peerNext = &lastChild;
		lastChild.peerPrev = &firstChild;
#endif
	}

#if 0 // TODO
	void ViewPort::add(Child* child, const char* id)
	{
		child->peerUnlink();

		lastChild.peerPrev->peerNext = child;
		child->peerPrev = lastChild.peerPrev;
		child->peerNext = &lastChild;

//		children.push_back(std::unique_ptr<Child>(child));

		if (auto drawable = dynamic_cast<Visual*>(child); drawable)
		{
			drawable->parent = this;
			visuals.push_back(drawable);

			Invalidate(drawable->ClipRect());
		}

		if (auto mouseable = dynamic_cast<Interactor*>(child); mouseable)
		{
			mouseable->form = form;

			mouseTargets.push_back(mouseable);
		}

		child->onAddedToParent();
	}

	// Removes a contiguous range of children
	void ViewPort::remove(Child* first, Child* last)
	{
		if (!first || !last)
			return;

		gmpi::drawing::Rect invalidRect{}; // union of all removed children.

		auto nextAfterLast = last->peerNext;
		auto beforeFirst = first->peerPrev;

		// Detach the range from the intrusive list now so traversal of remaining children
		// can't accidentally step into nodes that are being deleted.
		beforeFirst->peerNext = nextAfterLast;
		nextAfterLast->peerPrev = beforeFirst;

		// Isolate removed nodes to avoid leaving dangling links while deleting.
		first->peerPrev = nullptr;
		last->peerNext = nullptr;
		for (auto* obj = first; obj; )
		{
			auto* next = obj->peerNext;

			// is it a visual?
			if (auto itx = std::find(visuals.begin(), visuals.end(), dynamic_cast<Visual*>(obj)); itx != visuals.end())
			{
				if(isNull(invalidRect))
					invalidRect = (*itx)->ClipRect();
				else
					invalidRect = unionRect(invalidRect, (*itx)->ClipRect());
				
				visuals.erase(itx);
			}

			// is it a mouse-taret?
			auto obj_interactor = dynamic_cast<Interactor*>(obj);
			if (auto itx = std::find(mouseTargets.begin(), mouseTargets.end(), obj_interactor); itx != mouseTargets.end())
			{
				mouseTargets.erase(itx);
			}

			if (obj_interactor == mouseOverObject)
				mouseOverObject = {};

			delete obj;
			obj = next;
		}

		Invalidate(invalidRect);
	}
#endif

	void ViewPort::clear()
	{
		clearChildren();
	}

	void ViewPort::Draw(gmpi::drawing::Graphics& g) const
	{
		const auto originalTransform = g.getTransform();
		const auto adjustedTransform = transform * originalTransform;
		g.setTransform(adjustedTransform);

		const auto clipRect = g.getAxisAlignedClip();

		for (auto v = firstVisual.peerNext ; v != &lastVisual ; v = v->peerNext)
		{
			const auto childRect = v->ClipRect();
			if (!empty(intersectRect(clipRect, childRect)))
			{
				v->Draw(g);
			}
		}

		g.setTransform(originalTransform);
	}

	bool ViewPort::HitTest(gmpi::drawing::Point p) const
	{
		const auto point = transformPoint(reverseTransform, p);

		for (auto t = firstMouseTarget.peerNext; t != &lastMouseTarget; t = t->peerNext)
		{
			if (t->HitTest(point))
			{
				return true;
				break;
			}
		}

		return false;
	}

	bool ViewPort::onPointerDown(PointerEvent* e) const
//	bool ViewPort::onPointerDown(gmpi::drawing::Point p) const
	{
		auto e2 = *e;
		e2.position = transformPoint(reverseTransform, e->position);
//		const auto point = transformPoint(reverseTransform, e->position);

		// iterate mouseTargets in reverse (to respect Z-order)
		for (auto t = firstMouseTarget.peerNext; t != &lastMouseTarget; t = t->peerNext)
		{
			if (t->HitTest(e2.position) && t->onPointerDown(&e2))
				return true;
		}

		return false;
	}

	bool ViewPort::onPointerUp(gmpi::drawing::Point p) const
	{
		const auto point = transformPoint(reverseTransform, p);

		// iterate mouseTargets in reverse (to respect Z-order)
		for (auto t = firstMouseTarget.peerNext; t != &lastMouseTarget; t = t->peerNext)
		{
			if (t->HitTest(point) && t->onPointerUp(point))
				return true;
		}

		return false;
	}

	bool ViewPort::onPointerMove(gmpi::drawing::Point p) const
	{
//		_RPTN(0, "---------------ViewPort::onPointerMove( %f, %f)\n", p.x, p.y);

		mousePoint = transformPoint(reverseTransform, p);
		const auto prevMouseOverObject = mouseOverObject;

		mouseOverObject = {};

		// iterate mouseTargets in reverse (to respect Z-order)
		for (auto t = firstMouseTarget.peerNext; t != &lastMouseTarget; t = t->peerNext)
		{
			if (t->HitTest(mousePoint) && t->wantsClicks())
			{
				mouseOverObject = t;
				break;
			}
		}

		if (prevMouseOverObject != mouseOverObject)
		{
			const char* prevName = prevMouseOverObject ? typeid(*prevMouseOverObject).name() : "<none>";
			const char* nextName = mouseOverObject ? typeid(*mouseOverObject).name() : "<none>";

			_RPTN(0, "%x MouseOverObject %s [%x] => %s [%x]\n", this, prevName, prevMouseOverObject, nextName, mouseOverObject);
		}

		if (prevMouseOverObject != mouseOverObject)
		{
			if (prevMouseOverObject)
			{
				prevMouseOverObject->setHover(false);
			}

			if (mouseOverObject)
			{
//				_RPTN(0, "SetHover(true) :%s\n", typeid (*mouseOverObject).name());
				mouseOverObject->setHover(true);
			}
		}

		if (mouseOverObject)
		{
			mouseOverObject->onPointerMove(mousePoint);
		}

		return mouseOverObject != nullptr;
	}

	bool ViewPort::setHover(bool isMouseOverMe) const
	{
		isHovered = isMouseOverMe;
		if (!isMouseOverMe && mouseOverObject)
		{
			mouseOverObject->setHover(false);
			mouseOverObject = {};
		}

		return true;
	}

	bool ViewPort::onMouseWheel(int32_t flags, int32_t delta, gmpi::drawing::Point point) const
	{
		for (auto t = firstMouseTarget.peerNext; t != &lastMouseTarget; t = t->peerNext)
		{
			if (t->HitTest(point) && t->onMouseWheel(flags, delta, point))
				return true;
		}

		return false;
	}


#if 0
	bool ViewPort::onHover(gmpi::drawing::Point point, bool isHovered) const
	{
		for (auto& t : mouseTargets)
		{
			if (t->HitTest(point) && t->onHover())
			{
				return true;
			}
		}

		return false;
	}
#endif

	void ViewPort::Invalidate(gmpi::drawing::Rect r) const
	{
		if (!parent) // still under construction
			return;

		auto r2 = transformRect(reverseTransform, r);
		parent->Invalidate(r2);
	}

	///////////////////////////////////////////////////////////////////////


	Portal::Portal()
	{
		firstVisual.peerNext = &lastVisual;
		lastVisual.peerPrev = &firstVisual;
	}

	Portal::~Portal()
	{
		clearChildren();
	}

	void Portal::clearChildren()
	{
		mouseOverObject = {};

#if 0
		// delete all nodes between sentinels
		Child* obj = firstChild.peerNext;
		while (obj && obj != &lastChild)
		{
			Child* next = obj->peerNext;
			delete obj;
			obj = next;
		}

		firstChild.peerNext = &lastChild;
		lastChild.peerPrev = &firstChild;
#endif
	}


	void Portal::clear()
	{
		clearChildren();
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
			if (!empty(intersectRect(clipRect, childRect)))
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

#if 0
	bool Portal::HitTest(gmpi::drawing::Point p) const
	{
		const auto point = transformPoint(reverseTransform, p);

		for (auto t = firstMouseTarget.peerNext; t != &lastMouseTarget; t = t->peerNext)
		{
			if (t->HitTest(point))
			{
				return true;
				break;
			}
		}

		return false;
	}

	bool Portal::onPointerDown(PointerEvent* e) const
		//	bool Portal::onPointerDown(gmpi::drawing::Point p) const
	{
		auto e2 = *e;
		e2.position = transformPoint(reverseTransform, e->position);
		//		const auto point = transformPoint(reverseTransform, e->position);

				// iterate mouseTargets in reverse (to respect Z-order)
		for (auto t = firstMouseTarget.peerNext; t != &lastMouseTarget; t = t->peerNext)
		{
			if (t->HitTest(e2.position) && t->onPointerDown(&e2))
				return true;
		}

		return false;
	}

	bool Portal::onPointerUp(gmpi::drawing::Point p) const
	{
		const auto point = transformPoint(reverseTransform, p);

		// iterate mouseTargets in reverse (to respect Z-order)
		for (auto t = firstMouseTarget.peerNext; t != &lastMouseTarget; t = t->peerNext)
		{
			if (t->HitTest(point) && t->onPointerUp(point))
				return true;
		}

		return false;
	}

	bool Portal::onPointerMove(gmpi::drawing::Point p) const
	{
		//		_RPTN(0, "---------------Portal::onPointerMove( %f, %f)\n", p.x, p.y);

		mousePoint = transformPoint(reverseTransform, p);
		const auto prevMouseOverObject = mouseOverObject;

		mouseOverObject = {};

		// iterate mouseTargets in reverse (to respect Z-order)
		for (auto t = firstMouseTarget.peerNext; t != &lastMouseTarget; t = t->peerNext)
		{
			if (t->HitTest(mousePoint) && t->wantsClicks())
			{
				mouseOverObject = t;
				break;
			}
		}

		if (prevMouseOverObject != mouseOverObject)
		{
			const char* prevName = prevMouseOverObject ? typeid(*prevMouseOverObject).name() : "<none>";
			const char* nextName = mouseOverObject ? typeid(*mouseOverObject).name() : "<none>";

			_RPTN(0, "%x MouseOverObject %s [%x] => %s [%x]\n", this, prevName, prevMouseOverObject, nextName, mouseOverObject);
		}

		if (prevMouseOverObject != mouseOverObject)
		{
			if (prevMouseOverObject)
			{
				prevMouseOverObject->setHover(false);
			}

			if (mouseOverObject)
			{
				//				_RPTN(0, "SetHover(true) :%s\n", typeid (*mouseOverObject).name());
				mouseOverObject->setHover(true);
			}
		}

		if (mouseOverObject)
		{
			mouseOverObject->onPointerMove(mousePoint);
		}

		return mouseOverObject != nullptr;
	}

	bool Portal::setHover(bool isMouseOverMe) const
	{
		isHovered = isMouseOverMe;
		if (!isMouseOverMe && mouseOverObject)
		{
			mouseOverObject->setHover(false);
			mouseOverObject = {};
		}

		return true;
	}

	bool Portal::onMouseWheel(int32_t flags, int32_t delta, gmpi::drawing::Point point) const
	{
		for (auto t = firstMouseTarget.peerNext; t != &lastMouseTarget; t = t->peerNext)
		{
			if (t->HitTest(point) && t->onMouseWheel(flags, delta, point))
				return true;
		}

		return false;
	}
#endif
	///////////////////////////////////////////////////////////////////////

	void Visual::Invalidate(gmpi::drawing::Rect r) const
	{
		assert(parent);
		parent->Invalidate(r);
	}

	void TopView::Invalidate(gmpi::drawing::Rect r) const
	{
		form->invalidate(&r);
//		assert(form);

//		form->redraw(); // TODO, use rect
	}

	ScrollView::ScrollView(std::string ppath, gmpi::drawing::Rect pbounds)
		: bounds(pbounds),
		path(ppath)
	{
		_RPTN(0, "new ScrollView %x\n", this);
		//		e->reg(&scroll, path + "scroll");
		
		scroll.add([this]()
			{
				UpdateTransform();
				// avoid my own invalidate, since it assumes the rect needs to be un-scrolled
				parent->Invalidate(bounds);
			}
		);
	}

	ScrollView::~ScrollView()
	{
//		_RPTN(0, "~ScrollView %x\n", this);
	}

	void ScrollView::onAddedToParent()
	{
		// todo:: getPath() from parent, so don't need member variable crap.
		getEnvironment()->reg(&scroll, path + "scroll");
		UpdateTransform();
	}

	void ScrollView::UpdateTransform()
	{
		transform = makeTranslation({ bounds.left, bounds.top + scroll.get() });
		reverseTransform = invert(transform);
	}

	Environment* TopView::getEnvironment()
	{
		return form->getEnvironment();
	}

} // namespace

