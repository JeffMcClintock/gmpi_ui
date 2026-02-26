#pragma once
#include "./Drawables.h"

#include "modules/shared/unicode_conversion.h"
#include <type_traits>
namespace uc = JmUnicodeConversions;

// NOTE: This UI layer is a retained declarative builder. gmpi_form_builder views
// (e.g. TextLabelView) don't draw directly; they configure and assemble the
// actual drawable primitives that get rendered later.
// for the actual drawing, refer to the GMPI-UI framework (GmpiUiDrawing.h) and the Drawables in experimental/Drawables.h.
namespace gmpi::ui::builder
{
struct View;

template<typename T>
struct PointerSpan
{
	mutable T* first = {};
	mutable T* last = {};

	void clear() const
	{
		first = last = {};
	}

	bool empty() const
	{
		return first == nullptr;
	}

	explicit operator bool() const
	{
		return !empty();
	}

	void unlink() const
	{
		if (empty())
			return;

		auto* prev = first->peerPrev;
		prev->peerNext = last->peerNext;
		prev->peerNext->peerPrev = prev;
		clear();
	}

	void replace(
		T*& prev,
		const std::vector<std::unique_ptr<T>>& children
	) const
	{
		// Assumes any previous contents were already removed from the linked-list
		// by `unlink()`. Therefore when `children` is empty, we do nothing to the
		// linked-list (it should already be "papered over").
		assert(empty());
		if (children.empty())
		{
//			clear();
			return;
		}

		auto next = prev->peerNext;

		for (auto& c : children)
		{
			prev->peerNext = c.get();
			c->peerPrev = prev;
			prev = c.get();
		}

		prev->peerNext = next;
		next->peerPrev = prev;

		first = children.front().get();
		last = children.back().get();
	}
};

inline thread_local std::vector< std::unique_ptr<View> >* ThreadLocalCurrentBuilder = {};

class Builder
{
	std::vector< std::unique_ptr<View> >& result;
public:

	Builder(std::vector< std::unique_ptr<View> >& presult) : result(presult)
	{
		ThreadLocalCurrentBuilder = &result;
	}
	~Builder()
	{
		ThreadLocalCurrentBuilder = {};
	}

	/* weird compiler error
	std::vector< std::unique_ptr<View> >& getResult()
	{
		return result;
	}
	*/
};

struct View : public gmpi_forms::IObserver
{
	mutable bool dirty = true;
	mutable PointerSpan<gmpi::forms::primitive::Visual> visuals;
	mutable PointerSpan<gmpi::forms::primitive::Interactor> mouseTargets;

	// TODO, could remove it and have linked-list own the primitives
	mutable gmpi::forms::primitive::Canvas children;

	mutable std::vector< std::unique_ptr<gmpi_forms::thing> > selfOwnedStates;

	virtual ~View() {}
	void clear2();
	virtual void Render(gmpi_forms::Environment* env, gmpi::forms::primitive::Canvas& canvas) const {}
	virtual gmpi::drawing::Rect getBounds() const { return {}; }
	virtual void setBounds(gmpi::drawing::Rect) {}
	virtual bool RenderIfDirty(
		gmpi_forms::Environment* env,
		gmpi::forms::primitive::IVisualParent& parent,
		gmpi::forms::primitive::IMouseParent& mouseParent
	) const;
	void setDirty();
	void OnModelWillChange() override;
};


// replaced by State
#if 0

// abstract the act of using a value from somewhere on a Visual
template<typename T>
struct ValueObserver // : public gmpi_forms::IObserver
{
	View* view = {};

	virtual ~ValueObserver() {}
	void ObjectChanged()
	{
		view->setDirty();
	}

	virtual T get() = 0;
	virtual void set(T) = 0;
};

// just a simple value that never changes
template<typename T>
struct ValueObserverLiteral : public ValueObserver<T>
{
	T value;

	ValueObserverLiteral(T value, gmpi::ui::builder::View* pview) : value(value)
	{
		ValueObserver<T>::view = pview;
	}

	T get() override
	{
		return value;
	}
	void set(T) override
	{
		assert(false); // not settable
	}
};

// A value that is a member of a class that is observable
// deprecated, ref: gmpi_forms::StateRef
template<typename T>
struct ValueObserverLambda : public ValueObserver<T>
{
	std::function<T(void)> getfunc;
	std::function<void(T)> setfunc;

	ValueObserverLambda(
		gmpi::ui::builder::View* pview,
		std::function<T(void)> pget,
		std::function<void(T)> pset = {}
	) : getfunc(pget), setfunc(pset)
	{
		ValueObserver<T>::view = pview;
	}

	T get() override
	{
		return getfunc();
	}
	void set(T newval) override
	{
		assert(setfunc); // not settable?
		setfunc(newval);
	}
};

// A value that binds a State to anything else via the lambda.
// only supports notification from observer to state, not the other way around, so it's basically a one-way adapter.
struct StateBindBass
{
	virtual ~StateBindBass() = default;
};

template<typename T>
struct StateBinding : public StateBindBass
{
	gmpi_forms::State<T> value;
	std::function<void(T)> setfunc;

	StateBinding(
		T initialValue,
		std::function<void(T)> pset = {}
	) : setfunc(pset)
	{
		value.set(initialValue);

		value.addObserver([this]()
			{
				if (setfunc)
					setfunc(value.get());
			});
	}
};
#endif

struct Seperator : public View
{
	gmpi::drawing::Rect bounds;

	Seperator(gmpi::drawing::Rect pbounds) : bounds(pbounds)
	{
	}

	void Render(gmpi_forms::Environment* env, gmpi::forms::primitive::Canvas& canvas) const override;
	gmpi::drawing::Rect getBounds() const override
	{
		return bounds;
	}
	void setBounds(gmpi::drawing::Rect newBounds) override
	{
		bounds = newBounds;
	}
};


struct TextLabelView : public View
{
	gmpi::drawing::Rect bounds;
	bool rightAlign = false;
	mutable gmpi_forms::StateRef<std::string> text2;

	TextLabelView() = default;

	void Render(gmpi_forms::Environment* env, gmpi::forms::primitive::Canvas& canvas) const override;
	gmpi::drawing::Rect getBounds() const override
	{
		return bounds;
	}
	void setBounds(gmpi::drawing::Rect newBounds) override
	{
		bounds = newBounds;
	}
};

struct TextEditView : public View
{
	gmpi::drawing::Rect bounds;
	mutable gmpi_forms::StateRef<std::string> text;
	bool rightAlign = false;

	mutable gmpi::shared_ptr<gmpi::api::ITextEdit> textEdit;

	TextEditView(std::string path = {});

	void Render(gmpi_forms::Environment* env, gmpi::forms::primitive::Canvas& canvas) const override;
	gmpi::drawing::Rect getBounds() const override
	{
		return bounds;
	}
	void setBounds(gmpi::drawing::Rect newBounds) override
	{
		bounds = newBounds;
	}
};

struct ComboBoxView : public View
{
	gmpi::drawing::Rect bounds;
	mutable gmpi_forms::StateRef<std::string> enum_list;
	mutable gmpi_forms::StateRef<int32_t> enum_value;

	mutable gmpi::shared_ptr<gmpi::api::IPopupMenu> combo;

	ComboBoxView()
	{
		enum_value.addObserver([this]()
			{
				setDirty();
			});
		enum_list.addObserver([this]()
			{
				setDirty();
			});
	}

	void Render(gmpi_forms::Environment* env, gmpi::forms::primitive::Canvas& canvas) const override;
	gmpi::drawing::Rect getBounds() const override
	{
		return bounds;
	}
	void setBounds(gmpi::drawing::Rect newBounds) override
	{
		bounds = newBounds;
	}
};

struct TickBox : public View
{
	gmpi::drawing::Rect bounds;
	mutable gmpi_forms::StateRef<bool> value;

	TickBox()
	{
		value.addObserver([this]()
			{
				setDirty();
			});
	}

	TickBox(gmpi_forms::State<bool>& pvalue)
	{
		// hook up the 'ticked' state to the one provided.
		value.setSource(&pvalue);

		value.addObserver([this]()
			{
				setDirty();
			});
	}

	void Render(gmpi_forms::Environment* env, gmpi::forms::primitive::Canvas& canvas) const override;
	gmpi::drawing::Rect getBounds() const override
	{
		return bounds;
	}
	void setBounds(gmpi::drawing::Rect newBounds) override
	{
		bounds = newBounds;
	}
};

// labled tickbox
struct ToggleSwitch : public View
{
	gmpi::drawing::Rect bounds;
	mutable gmpi_forms::StateRef<bool> value;
	mutable gmpi_forms::StateRef<std::string> text;

	ToggleSwitch(
		std::string_view labelText,
		gmpi_forms::State<bool>& pvalue
	)
	{
		// manage the label-text state myself.
		auto label = std::make_unique<gmpi_forms::State<std::string>>();
		label->set(std::string(labelText));
		text.setSource(label.get());
		selfOwnedStates.push_back(std::move(label));

		// hook up the 'ticked' state to the one provided.
		value.setSource(&pvalue);

		value.addObserver([this]()
			{
				setDirty();
			});
	}

	void Render(gmpi_forms::Environment* env, gmpi::forms::primitive::Canvas& canvas) const override;
	gmpi::drawing::Rect getBounds() const override
	{
		return bounds;
	}
	void setBounds(gmpi::drawing::Rect newBounds) override
	{
		bounds = newBounds;
	}
};

struct FileBrowseButtonView : public View
{
	gmpi::drawing::Rect bounds;
	mutable gmpi_forms::StateRef<std::string> value;
	mutable gmpi::shared_ptr<gmpi::api::IFileDialog> fileDialog; // needs to be kept alive, so it can be called async.

	FileBrowseButtonView(gmpi::drawing::Rect bounds) : bounds(bounds) {}

	void Render(gmpi_forms::Environment* env, gmpi::forms::primitive::Canvas& canvas) const override;
	gmpi::drawing::Rect getBounds() const override
	{
		return bounds;
	}
	void setBounds(gmpi::drawing::Rect newBounds) override
	{
		bounds = newBounds;
	}
};

// A text label that shows a popup menu when clicked.
struct PopupMenuView : public View
{
	gmpi::drawing::Rect bounds;
	mutable gmpi_forms::StateRef<std::string> text2;
	mutable gmpi_forms::StateRef<std::string> menuItems; // comma-separated menu items, e.g. "Learn,Unlearn,Edit..."
	std::function<void(int32_t)> onItemSelected;
	mutable gmpi::shared_ptr<gmpi::api::IPopupMenu> popupMenu;

	PopupMenuView()
	{
		text2.addObserver([this]()
			{
				setDirty();
			});
	}

	void Render(gmpi_forms::Environment* env, gmpi::forms::primitive::Canvas& canvas) const override;
	gmpi::drawing::Rect getBounds() const override
	{
		return bounds;
	}
	void setBounds(gmpi::drawing::Rect newBounds) override
	{
		bounds = newBounds;
	}
};

struct ViewParent
{
	std::vector< std::unique_ptr<gmpi::ui::builder::View> > childViews;

	void clear()
	{
		childViews.clear();
	}
};

// scene-builder object that constructs widgets by composing primitive drawable objects into a scene graph.
struct ScrollPortal : public View, public ViewParent
{
	struct ScrollInfo
	{
		float visibleLength;
		float contentLength;
		float thumbLength;
		float thumbPosition;
	};

	mutable gmpi::forms::primitive::Portal* portal = {};
	mutable gmpi::forms::primitive::MousePortal* mouseportal = {};
	mutable gmpi::forms::primitive::RoundedRectangle* scrollThumb = {};
	mutable gmpi::forms::primitive::RectangleMouseTarget* scrollThumbMouseTarget = {};
	mutable gmpi_forms::StateRef<float> scroll_state;

	gmpi::drawing::Rect bounds;

	ScrollPortal(gmpi::drawing::Rect pbounds) : bounds(pbounds)
	{
	}

	ScrollInfo calcScrollBar() const;

	bool RenderIfDirty(
		gmpi_forms::Environment* env,
		gmpi::forms::primitive::IVisualParent& owner,
		gmpi::forms::primitive::IMouseParent& mouseParent
	) const override;

	void Render(gmpi_forms::Environment* env, gmpi::forms::primitive::Canvas& canvas) const override;
	gmpi::drawing::Rect getBounds() const override
	{
		return bounds;
	}
	void setBounds(gmpi::drawing::Rect newBounds) override
	{
		bounds = newBounds;
	}
};

} // namespace gmpi::ui::builder