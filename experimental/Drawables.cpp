#include "Drawables.h"
#include "Utils.h"
#include "modules/shared/unicode_conversion.h"
#include "ModuleBrowser.h"

using namespace gmpi;
using namespace GmpiDrawing;
using namespace GmpiDrawing_API;

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
		environment.child.mouseTargets.back()->onPointerDown = [&environment](GmpiDrawing::Point p)
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

	void EditBox::Draw(GmpiDrawing::Graphics& g /*, Style style */) const
	{
		auto fillBrush = g.CreateSolidColorBrush(Color::White);
		auto outlineBrush = g.CreateSolidColorBrush(Color::Black);
		auto textFormat = g.GetFactory().CreateTextFormat2();

		g.FillRectangle(bounds, fillBrush);
		g.DrawRectangle(bounds, outlineBrush);

		g.DrawTextU(text, textFormat, bounds, outlineBrush);
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

		environment.mouseTargets.back()->onPointerDown = [&environment](GmpiDrawing::Point p)
		{
			environment.capturePointer(
				[&environment](GmpiDrawing::Size delta)
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

	void Rectangle::Draw(GmpiDrawing::Graphics& g) const
	{
		style->Init(g);

		g.FillRectangle(bounds, style->fill);
		g.DrawRectangle(bounds, style->stroke);

		// Color the rectangle a little different on each redraw to get a visual indication of what is being invalidated.
#if 0 //def _DEBUG
		static uint32_t colors[] =
		{
			0xff0000,
			0x00ff00,
			0x0000ff,
			0xff00ff,
		};
		auto brush = g.CreateSolidColorBrush(Color(colors[redrawCount & 3], 0.1f));
		g.FillRectangle(bounds, brush);
		++redrawCount;
#endif
	}

	void ScrollBar::Draw(GmpiDrawing::Graphics& g) const
	{
		style->Init(g);

		const float normalized = position.get() / scrollRangePixels;

		// total height of content is the visible area (bounds.getHeight()) plus the scroll range (scrollRangePixels)
		const float handleHeight = bounds.getHeight() * bounds.getHeight() / (bounds.getHeight() - scrollRangePixels);
		auto scrollRect = bounds;
		scrollRect.top = (scrollRect.getHeight() - handleHeight) * normalized;
		scrollRect.bottom = scrollRect.top + handleHeight;
		g.FillRectangle(scrollRect, style->fill);
	}
	
#if 0
	void ListView::Draw(GmpiDrawing::Graphics& g /*, Style style */) const
	{
		auto fillBrush = g.CreateSolidColorBrush(Color(0x003E3E3Eu));
		auto outlineBrush = g.CreateSolidColorBrush(Color::White);
		auto textFormat = g.GetFactory().CreateTextFormat2();
		textFormat.SetWordWrapping(WordWrapping::NoWrap);

		g.FillRectangle(bounds, fillBrush);
		//	g.DrawRectangle(bounds, outlineBrush);

		auto textRect = bounds;
		textRect.left += 2;
		textRect.bottom = textRect.top + 16;

		for (auto& text : data)
		{
			g.DrawTextU(text.second.c_str(), textFormat, textRect, outlineBrush, DrawTextOptions::Clip);

			textRect.top += 16;
			textRect.bottom += 16;
		}
	}
	bool ListView::HitTest(GmpiDrawing::Point p) const
	{
		return bounds.ContainsPoint(p);
	}
#if 1

	bool ListView::onPointerDown(GmpiDrawing::Point p) const
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

	void ShapeStyle::Init(GmpiDrawing::Graphics& g) const
	{
		if (!fill.Get())
		{
			fill = g.CreateSolidColorBrush(fillColor);
			stroke = g.CreateSolidColorBrush(strokeColor);
		}
	}

	void TextBoxStyle::setLineSpacing(float pfixedLineSpacing)
	{
		fixedLineSpacing = pfixedLineSpacing;
	}
	
	void TextBoxStyle::Draw(GmpiDrawing::Graphics& g, const TextBox& t) const
	{
		if (!foreGround.Get())
		{
			backGround = g.CreateSolidColorBrush(backgroundColor);
			foreGround = g.CreateSolidColorBrush(foregroundColor);
			textFormat = g.GetFactory().CreateTextFormat2(bodyHeight);
			textFormat.SetWordWrapping(WordWrapping::NoWrap);

			if(fixedLineSpacing)
				textFormat.SetLineSpacing(fixedLineSpacing, 0.8f * fixedLineSpacing);
		}

		g.FillRectangle(t.bounds, backGround);
		//	g.DrawRectangle(bounds, outlineBrush);

		//auto textRect = bounds;
		//textRect.left += 2;
		//textRect.bottom = textRect.top + 16;

		g.DrawTextU(t.text, textFormat, t.bounds, foreGround, DrawTextOptions::Clip);
	}

	void TextBox::Draw(GmpiDrawing::Graphics& g) const
	{
		style->Draw(g, *this);
	}

	void Shape::Draw(GmpiDrawing::Graphics& g) const
	{
		const auto originalTransform = g.GetTransform();
		const auto adjustedTransform = originalTransform * Matrix3x2::Translation({ bounds.left, bounds.top });
		g.SetTransform(adjustedTransform);

		style->Draw(g, *this, *path);

		g.SetTransform(originalTransform);
	}

	void ShapeStyle::Draw(GmpiDrawing::Graphics& g, const Shape& t, const PathHolder& path) const
	{
		Init(g);
		if (!path.path)
		{
			path.path = g.GetFactory().CreatePathGeometry();
			path.InitPath(path.path);
		}
		if (path.path)
		{
			g.FillGeometry(path.path, fill);
			g.DrawGeometry(path.path, stroke, 1.f);
		}
	}

	void Circle::Draw(GmpiDrawing::Graphics& g /*, Style style */) const
	{
		auto fillBrush = g.CreateSolidColorBrush(Color::White);
		auto outlineBrush = g.CreateSolidColorBrush(Color::Black);

		g.FillCircle(gmpi_sdk::Center(bounds), bounds.getWidth() * 0.5f, fillBrush);
		g.DrawCircle(gmpi_sdk::Center(bounds), bounds.getWidth() * 0.5f, outlineBrush);
	}

	void Knob::Draw(GmpiDrawing::Graphics& g /*, Style style */) const
	{
		auto fillBrush = g.CreateSolidColorBrush(Color::White);
		auto outlineBrush = g.CreateSolidColorBrush(Color::Black);

		const auto centerPoint = gmpi_sdk::Center(bounds);

		g.FillCircle(centerPoint, bounds.getWidth() * 0.5f, fillBrush);
		g.DrawCircle(centerPoint, bounds.getWidth() * 0.5f, outlineBrush);

		// 0.0 12 o'clock
		const auto turn = -0.3f + 0.6f * normalized;
		const auto radius = bounds.getWidth() * 0.3f;
		Point tickPoint(centerPoint.x + radius * gmpi_sdk::tsin(turn), centerPoint.y - radius * gmpi_sdk::tcos(turn));

		g.FillCircle(tickPoint, bounds.getWidth() * 0.1f, outlineBrush);
	}

	bool RectangleMouseTarget::HitTest(GmpiDrawing::Point p) const
	{
		return bounds.ContainsPoint(p);
	}

	void RectangleMouseTarget::captureMouse(bool capture)
	{
		// how to capture mouse? (currently dragging scrollbar works only so long as mouse is over narrow strip)
// need to get to form/environment whatever.		getEnvironment()->captureMouse(this, capture);

		// need to get topview to capture mouse on behalf of my identity, so even if this object is deleted, mouse capture is still valid, and topview will route it 
		// to my replacement.
		form->captureMouse(this);
	}

	void ViewPort::add(Child* child, const char* id)
	{
		children.push_back({ id ? std::string(id) : std::string(), std::unique_ptr<Child>(child)});

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
		auto it1 = children.begin();
		while (it1 != children.end() && (*it1).child.get() != first)
			++it1;

		auto it2 = it1;
		while (it2 != children.end() && (*it2).child.get() != last)
			++it2;

		if (it1 == children.end() || it2 == children.end())
			return;

		it2++;

		GmpiDrawing::Rect invalidRect{}; // union of all removed children. (faster than many individual calls to invalidate)
		
		for (auto it = it1; it < it2; ++it)
		{
			auto obj = (*it).child.get();

			if (auto itx = std::find(visuals.begin(), visuals.end(), dynamic_cast<Visual*>(obj)); itx != visuals.end())
			{
				invalidRect = UnionIgnoringNull(invalidRect, (*itx)->ClipRect());
				
				visuals.erase(itx);
			}

			auto obj_interactor = dynamic_cast<Interactor*>(obj);
			if (auto itx = std::find(mouseTargets.begin(), mouseTargets.end(), obj_interactor); itx != mouseTargets.end())
			{
				mouseTargets.erase(itx);
			}

			if (obj_interactor == mouseOverObject)
				mouseOverObject = {};
		}

		children.erase(it1, it2);

		Invalidate(invalidRect);
	}

	void ViewPort::clear()
	{
		mouseOverObject = {};
		mouseTargets.clear();
		visuals.clear();
		children.clear();
	}

	void ViewPort::Draw(GmpiDrawing::Graphics& g) const
	{
		const auto originalTransform = g.GetTransform();
		const auto adjustedTransform = transform * originalTransform;
		g.SetTransform(adjustedTransform);

		const auto clipRect = g.GetAxisAlignedClip();

		for (auto& v : visuals)
		{
			const auto childRect = v->ClipRect();
			if (isOverlapped(clipRect, childRect))
			{
				v->Draw(g);
			}
		}

		g.SetTransform(originalTransform);
	}

	bool ViewPort::HitTest(GmpiDrawing::Point p) const
	{
		const auto point = reverseTransform.TransformPoint(p);

		for (auto& v : mouseTargets)
		{
			if (v->HitTest(point))
			{
				return true;
				break;
			}
		}

		return false;
	}

	bool ViewPort::onPointerDown(PointerEvent* e) const
//	bool ViewPort::onPointerDown(GmpiDrawing::Point p) const
	{
		auto e2 = *e;
		e2.position = reverseTransform.TransformPoint(e->position);
//		const auto point = reverseTransform.TransformPoint(e->position);

		// iterate mouseTargets in reverse (to respect Z-order)
		for (auto it = mouseTargets.rbegin(); it != mouseTargets.rend(); ++it)
		{
			auto t = *it;
			if (t->HitTest(e2.position) && t->onPointerDown(&e2))
			{
				return true;
			}
		}

		return false;
	}

	bool ViewPort::onPointerUp(GmpiDrawing::Point p) const
	{
		const auto point = reverseTransform.TransformPoint(p);

		// iterate mouseTargets in reverse (to respect Z-order)
		for (auto it = mouseTargets.rbegin(); it != mouseTargets.rend(); ++it)
		{
			auto t = *it;
			if (t->HitTest(point) && t->onPointerUp(point))
			{
				return true;
			}
		}

		return false;
	}

	bool ViewPort::onPointerMove(GmpiDrawing::Point p) const
	{
		mousePoint = reverseTransform.TransformPoint(p);
		const auto prevMouseOverObject = mouseOverObject;

		mouseOverObject = {};

		// iterate mouseTargets in reverse (to respect Z-order)
		for (auto it = mouseTargets.rbegin(); it != mouseTargets.rend(); ++it)
		{
			auto t = *it;
			if (t->HitTest(mousePoint) && t->wantsClicks())
			{
				mouseOverObject = t;
				t->onPointerMove(mousePoint);
				break;
			}
		}

		if (prevMouseOverObject != mouseOverObject)
		{
			if (prevMouseOverObject)
				prevMouseOverObject->setHover(false);

			if (mouseOverObject)
			{
//				_RPTN(0, "SetHover(true) :%s\n", typeid (*mouseOverObject).name());

				mouseOverObject->setHover(true);
			}
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

	bool ViewPort::onMouseWheel(int32_t flags, int32_t delta, GmpiDrawing::Point point) const
	{
		for (auto& t : mouseTargets)
		{
			if (t->HitTest(point) && t->onMouseWheel(flags, delta, point))
			{
				return true;
			}
		}

		return false;
	}


#if 0
	bool ViewPort::onHover(GmpiDrawing::Point point, bool isHovered) const
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

	void Visual::Invalidate(GmpiDrawing::Rect r) const
	{
		assert(parent);
		parent->Invalidate(r);
	}

	void ViewPort::Invalidate(GmpiDrawing::Rect r) const
	{
		assert(parent);

		auto r2 = reverseTransform.TransformRect(r);
		parent->Invalidate(r2);
	}

	void TopView::Invalidate(GmpiDrawing::Rect r) const
	{
		form->invalidate(&r);
//		assert(form);

//		form->redraw(); // TODO, use rect
	}

	ScrollView::ScrollView(std::string ppath, GmpiDrawing::Rect pbounds)
		: bounds(pbounds),
		path(ppath)
	{
//		e->reg(&scroll, path + "scroll");
		
		scroll.add([this]()
			{
				UpdateTransform();
				// avoid my own invalidate, since it assumes the rect needs to be un-scrolled
				parent->Invalidate(bounds);
			}
		);
	}

	void ScrollView::onAddedToParent()
	{
		// todo:: getPath() from parent, so don't need member variable crap.
		getEnvironment()->reg(&scroll, path + "scroll");
		UpdateTransform();
	}

	void ScrollView::UpdateTransform()
	{
		transform = Matrix3x2::Translation({ bounds.left, bounds.top + scroll.get() });

		reverseTransform = transform;
		reverseTransform.Invert(); // should be functional reverseTransform = Invert(transform);
	}

	Environment* TopView::getEnvironment()
	{
		return form->getEnvironment();
	}

} // namespace

