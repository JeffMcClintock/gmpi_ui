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

	auto prevVisual = &child.firstVisual;
	auto prevMouseTarget = &child.firstMouseTarget;

	for (auto& view : body_)
	{
		if (!view->dirty)
			continue;

		wasDirty = true;
		view->dirty = false;

		// remove old render from linked-list of drawables
		auto& visuals = view->children.visuals;
		if (!visuals.empty())
		{
			prevVisual = visuals.front()->peerPrev;
			auto next = visuals.back()->peerNext;
			// paper over the gap
			prevVisual->peerNext = next;
			next->peerPrev = prevVisual;

			visuals.clear();
		}

		// remove old mouse-targets from linked-list
		auto& mousetargets = view->children.mouseTargets;
		if (!mousetargets.empty())
		{
			prevMouseTarget = mousetargets.front()->peerPrev;
			auto next = mousetargets.back()->peerNext;
			// paper over the gap
			prevMouseTarget->peerNext = next;
			next->peerPrev = prevMouseTarget;

			mousetargets.clear();
		}


		// render fresh content
		view->Render(&env, view->children); // TODO env should be accessable from any parent, and should chain upward

		// store pointer to views range of children, if any.
		if (!visuals.empty())
		{
			auto next = prevVisual->peerNext;

			for (auto& c : visuals)
			{
				prevVisual->peerNext = c.get();
				c->peerPrev = prevVisual;
				prevVisual = c.get();

				c->parent = &child;
				child.Invalidate(c->ClipRect());
			}

			prevVisual->peerNext = next;
			next->peerPrev = prevVisual;

			view->firstVisual = visuals.front().get();
			view->lastVisual = visuals.back().get();

#if 0 // todo
			if (auto mouseable = dynamic_cast<Interactor*>(c); mouseable)
			{
				mouseable->form = this;

				child.mouseTargets.push_back(mouseable);
			}
#endif
		}
		else
		{
			view->firstVisual = view->lastVisual = {};
		}

		// store pointer to views range of children, if any.
		if (!mousetargets.empty())
		{
			auto next = prevMouseTarget->peerNext;

			for (auto& c : mousetargets)
			{
				prevMouseTarget->peerNext = c.get();
				c->peerPrev = prevMouseTarget;
				prevMouseTarget = c.get();

				c->form = this;
			}

			prevMouseTarget->peerNext = next;
			next->peerPrev = prevMouseTarget;

			view->firstMouseTarget = mousetargets.front().get();
			view->lastMouseTarget = mousetargets.back().get();
		}
		else
		{
			view->firstMouseTarget = view->lastMouseTarget = {};
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

namespace gmpi_form_builder
{
void PortalStart_internal::Render(gmpi_forms::Environment* env, gmpi_forms::Canvas& canvas) const
{
	last_rendered = new gmpi_forms::PortalStart();
	last_rendered->bounds = bounds;
	canvas.add(last_rendered);
}
void PortalEnd_internal::Render(gmpi_forms::Environment* env, gmpi_forms::Canvas& canvas) const
{
	auto pe = new gmpi_forms::PortalEnd();
	pe->buddy = buddy->last_rendered;
	canvas.add(pe);
}

Portal::Portal(gmpi::drawing::Rect pbounds)
{
	saveParent = gmpi_forms::ThreadLocalCurrentBuilder;
	auto& result = *gmpi_forms::ThreadLocalCurrentBuilder;
	bounds = pbounds;

	auto portal = std::make_unique<gmpi_form_builder::Portal_internal>(bounds);

	gmpi_forms::ThreadLocalCurrentBuilder = &(portal->result);

	result.push_back(std::move(portal));
}

Portal::~Portal()
{
	gmpi_forms::ThreadLocalCurrentBuilder = saveParent;
	/*
	auto& result = *gmpi_forms::ThreadLocalCurrentBuilder;

	auto pend = std::make_unique<gmpi_form_builder::PortalEnd_internal>(bounds);
	pend->buddy = portal_start;
	result.push_back(std::move(pend));
	*/
}

void Portal_internal::Render(gmpi_forms::Environment* env, gmpi_forms::Canvas& canvas) const
{
//	auto& result = *gmpi_forms::ThreadLocalCurrentBuilder;

	portal = new gmpi_forms::Portal();
	canvas.add(portal);

	DoUpdates(env);
}

void Portal_internal::DoUpdates(gmpi_forms::Environment* env) const
{
	bool wasDirty = false;

	auto prevVisual = &firstVisual;
//	auto prevMouseTarget = &firstMouseTarget;

	for (auto& view : result)
	{
		if (!view->dirty)
			continue;

		wasDirty = true;
		view->dirty = false;

		// remove old render from linked-list of drawables
		auto& visuals = view->children.visuals;
		if (!visuals.empty())
		{
			prevVisual = visuals.front()->peerPrev;
			auto next = visuals.back()->peerNext;
			// paper over the gap
			prevVisual->peerNext = next;
			next->peerPrev = prevVisual;

			visuals.clear();
		}

#if 0 // TODO mouse-portal
		// remove old mouse-targets from linked-list
		auto& mousetargets = view->children.mouseTargets;
		if (!mousetargets.empty())
		{
			prevMouseTarget = mousetargets.front()->peerPrev;
			auto next = mousetargets.back()->peerNext;
			// paper over the gap
			prevMouseTarget->peerNext = next;
			next->peerPrev = prevMouseTarget;

			mousetargets.clear();
		}
#endif

		// render fresh content
		view->Render(env, view->children); // TODO env should be accessable from any parent, and should chain upward

		// store pointer to views range of children, if any.
		if (!visuals.empty())
		{
			auto next = prevVisual->peerNext;

			for (auto& c : visuals)
			{
				prevVisual->peerNext = c.get();
				c->peerPrev = prevVisual;
				prevVisual = c.get();

				c->parent = portal;
				portal->Invalidate(c->ClipRect());
			}

			prevVisual->peerNext = next;
			next->peerPrev = prevVisual;

			view->firstVisual = visuals.front().get();
			view->lastVisual = visuals.back().get();

#if 0 // todo
			if (auto mouseable = dynamic_cast<Interactor*>(c); mouseable)
			{
				mouseable->form = this;

				child.mouseTargets.push_back(mouseable);
			}
#endif
		}
		else
		{
			view->firstVisual = view->lastVisual = {};
		}
#if 0
		// store pointer to views range of children, if any.
		if (!mousetargets.empty())
		{
			auto next = prevMouseTarget->peerNext;

			for (auto& c : mousetargets)
			{
				prevMouseTarget->peerNext = c.get();
				c->peerPrev = prevMouseTarget;
				prevMouseTarget = c.get();

				c->form = this;
			}

			prevMouseTarget->peerNext = next;
			next->peerPrev = prevMouseTarget;

			view->firstMouseTarget = mousetargets.front().get();
			view->lastMouseTarget = mousetargets.back().get();
		}
		else
		{
			view->firstMouseTarget = view->lastMouseTarget = {};
		}
#endif
	}

	if (wasDirty)
	{
// TODO		child.postRender();
	}
}




} // namespace gmpi_form_builder