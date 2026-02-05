#include "forms.h"
#include "modules/shared/unicode_conversion.h"

namespace gmpi_forms
{
Form::Form()
{
	child.form = this;

	startTimer(16);
}

void Form::invalidate(gmpi::drawing::Rect* r)
{
	if(host)
		host->invalidateRect(r);
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
	bool wasDirty = false;

	for (auto& view : body_)
	{
		if (view->dirty)
		{
			wasDirty = true;
			view->dirty = false;

			// remove old render from list of drawables
			if (view->firstVisual)
			{
				assert(view->lastVisual);
				child.remove(view->firstVisual, view->lastVisual);
				view->firstVisual = view->lastVisual = {};
			}

			const auto beforeChildCount = child.count();
			// render fresh content

			view->Render(&env, child); // TODO env should be accessable from any parent, and should chain upward

			// store pointer to childs range of drawables.
			const auto afterChildCount = child.count();
			if (beforeChildCount < afterChildCount)
			{
				view->firstVisual = child.get(beforeChildCount);
				view->lastVisual = child.get(afterChildCount - 1);
			}
		}
		else
		{
			// needs to check ALL Views, not only immediate children.
//				view->OnTimer();
		}
	}

	if (wasDirty)
	{
		child.postRender();
	}

}

#if 0
void Form::add(Widget& widget)
{
	//	children.push_back(&widget);
}
#endif

// map current state => drawables
void Form::renderVisuals()
{
	child.clear(); // or maybe need a fresh copy to diff with the old?

	{
		Builder builder(body_);
		Body(); // creates factories
	}

	DoUpdates();
}

#if 0
void Form::capturePointer(std::function<void(gmpi::drawing::Size)> pOnMouseMove)
{
	prevPoint = currentPoint;
	onMouseMove = pOnMouseMove;
	host->setCapture();
}
#endif

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
} // namespace gmpi_forms
