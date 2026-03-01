#pragma once
#include <map>
#include <memory>
#include <utility>
#include "GmpiUiDrawing.h"
#include "helpers/NativeUi.h"
#include "observable.h"

/*
create an interface IObservableObject

subclass it as a TreeViewModel or whatever

create a body() method that 'builds' the views and saves them in the environment (I guess)

whenever TreeViewModel is updated (though expansion/contraction) flag the view that built it as dirty,
 this should delete any visuals created previously by that view (and it's children), and rebuild them.

 this requires each view to keep a list of visuals that it created
*/

// Forward declare builder view type used by update helpers.
//namespace gmpi_form_builder { struct View; }
namespace gmpi::ui::builder { struct View; }
namespace gmpi::forms::primitive
{
	class Interactor;
	struct IMouseParent;
}

namespace gmpi_forms
{
// This is an object that is not part of the view, but of the model. It is only observed by a view.
// It can notify the view(or anything) when it has changed. This should trigger the rebuild of that view (and children).
// (currently it's manually hooked up to rebuild entire UI in Form::Form() )
struct IObserver
{
	virtual void OnModelWillChange() = 0;
};

struct IObservableObject
{
	std::vector<IObserver*> observers2;

	void ObjectWillChange() const
	{
		for (auto& o : observers2) o->OnModelWillChange();
	}
};

struct Environment
{
	//		Environment* parent = {};
	//		std::vector<std::unique_ptr<Environment>> children;
private:
	std::map<std::string, std::unique_ptr< thing > > states;

public:
	gmpi::shared_ptr<gmpi::api::IDialogHost> dialogHost; // bit odd here.

	template <typename T>
	void reg(StateRef<T>& state, std::string path)
	{
		State<T>* s = {};

		if (auto it = states.find(path); it != states.end())
		{
			s = dynamic_cast<State<T>*>((*it).second.get());
		}
		else
		{
			auto temp = std::make_unique< State<T> >();
			s = temp.get();
			states[path] = std::move(temp);
		}

		state.setSource(s);
	}


	thing* findState(std::string path)
	{
		if (auto it = states.find(path); it != states.end())
			return (*it).second.get();

		return nullptr;
	}
	//std::string idPath()
	//{
	//	if (parent)
	//	{
	//		return parent->idPath() + "." + id;
	//	}
	//	return id;
	//}
};

} // namespace gmpi_forms

// the retained-mode objects that are composed to perform the actual drawing.
namespace gmpi::forms::primitive
{
// Forward declare model/environment types that live in `gmpi_forms`.
using Environment = gmpi_forms::Environment;

// A child is anything that can exist in a view, including drawable, and non-drawable (like styles)
// NEW: now for styles only
struct Child
{
	Child* peerPrev = nullptr;
	Child* peerNext = nullptr;

	virtual ~Child() { /*peerUnlink();*/ }
	virtual void onAddedToParent() {}
};

template <typename T>
struct intrusiveListItem
{
	mutable T* peerPrev = nullptr;
	mutable T* peerNext = nullptr;
};

struct IVisualParent
{
	virtual void Invalidate(gmpi::drawing::Rect) const = 0;
	virtual Environment* getEnvironment() = 0;
	virtual struct displayList& getDisplayList() = 0;
	virtual gmpi::drawing::Matrix3x2 getTransform() const = 0;
};

// IView ? SwiftUI Views aren't drawn, they are a factory that *creates* the things that are drawn.
class Visual : public intrusiveListItem<Visual>
{
public:
	IVisualParent* parent = {};

	virtual ~Visual() {}
	virtual void Draw(gmpi::drawing::Graphics&) const {}
	virtual void Invalidate(gmpi::drawing::Rect) const;
	// the cliprect is the minimum area that needs to be painted to display the visual
	virtual gmpi::drawing::Rect LayoutRect() const { return{}; }
	virtual gmpi::drawing::Rect ClipRect() const { return{}; }

	virtual Environment* getEnvironment()
	{
		return parent->getEnvironment();
	}

	virtual gmpi::drawing::Matrix3x2 getTransform() const
	{
		return parent->getTransform();
	}
};

class RectangularVisual : public Visual
{
public:
	gmpi::drawing::Rect bounds;

	RectangularVisual(gmpi::drawing::Rect pbounds) : bounds(pbounds) {}

	gmpi::drawing::Rect getBounds() const { return bounds; }
	gmpi::drawing::Rect ClipRect() const override { return bounds; }
	gmpi::drawing::Rect LayoutRect() const override { return bounds; }

	gmpi::drawing::Matrix3x2 getTransform() const override
	{
		return parent->getTransform() * gmpi::drawing::makeTranslation(bounds.left, bounds.top);
	}

	void setBounds(gmpi::drawing::Rect pbounds)
	{
		auto invalidateArea = unionRect(bounds, pbounds);
		bounds = pbounds;
		if(parent)
			parent->Invalidate(invalidateArea);
	}
};

struct IPointerBoss
{
	virtual void captureMouse(std::function<void(gmpi::drawing::Size)> callback) = 0;
};

struct PointerEvent
{
	gmpi::drawing::Point position;
	int32_t flags;
	IPointerBoss* boss;
};

// could be called 'MouseTarget' ?
class Interactor : public intrusiveListItem<Interactor>
{
	friend class Form;

public:
//		gmpi_forms::IForm* form = {};
	gmpi::forms::primitive::IMouseParent* mouseParent = {};

	Interactor() = default;
	virtual ~Interactor() {}
	virtual bool HitTest(gmpi::drawing::Point) const { return false; }
	// we want to support overlapping mouse targets, where one wants scroll-wheels and the other wants clicks
	virtual bool wantsClicks() const { return true; }

	// return true if handled
	virtual bool onPointerDown(const PointerEvent*) const { return false; }
	virtual bool onPointerUp(gmpi::drawing::Point) const { return false; }
	virtual bool onPointerMove(gmpi::drawing::Point) const { return false; }
	virtual bool onMouseWheel(int32_t flags, int32_t delta, gmpi::drawing::Point) const { return false; }
	virtual bool setHover(bool isMouseOverMe) const { return false; }
};

class RectangularInteractor : public Interactor
{
public:
	gmpi::drawing::Rect bounds;

	RectangularInteractor(gmpi::drawing::Rect pbounds) : bounds(pbounds) {}
	gmpi::drawing::Rect getBounds() const { return bounds; }
};

// display list. An intrusive linked lists of all child primatives. with sentinel nodes
struct displayList
{
	Visual firstVisual;
	Visual lastVisual;

	displayList()
	{
		clear();
	}
	void clear()
	{
		firstVisual.peerNext = &lastVisual;
		lastVisual.peerPrev = &firstVisual;
	}
	bool empty()
	{
		return firstVisual.peerNext == &lastVisual;
	}
};

struct mouseList
{
	Interactor firstMouseTarget;
	Interactor lastMouseTarget;

	gmpi::drawing::Matrix3x2 transform;			// for mouse events
	gmpi::drawing::Matrix3x2 reverseTransform;	// for invalidation

	// areas where the mouse produces some side-effect
	mutable Interactor* mouseOverObject = {};
	mutable gmpi::drawing::Point mousePoint;
	mutable bool isHovered = {};

	mouseList()
	{
		clear();
	}
	void clear()
	{
		firstMouseTarget.peerNext = &lastMouseTarget;
		lastMouseTarget.peerPrev = &firstMouseTarget;
	}
	bool empty()
	{
		return firstMouseTarget.peerNext == &lastMouseTarget;
	}

	Interactor* saveMouseState() const { return mouseOverObject; }
	void restoreMouseState(Interactor* prevMouseOverObject) const;

	bool HitTest(gmpi::drawing::Point p) const;
	bool onPointerDown(const PointerEvent* e) const;
	bool onPointerUp(gmpi::drawing::Point p) const;
	bool onMouseWheel(int32_t flags, int32_t delta, gmpi::drawing::Point point) const;
	bool setHover(bool isMouseOverMe) const;

	void onPointerMove(gmpi::drawing::Point p) const;

};

struct IMouseParent
{
	virtual void captureMouse(Interactor*) = 0;
	virtual mouseList& getMouseTargetList() = 0;
};


// not really a canvas, just a list of stuff
struct Canvas
{
	friend class Form;

//	protected:
	std::vector<std::unique_ptr<Child>> styles;
	std::vector<std::unique_ptr<Visual>> visuals;
	std::vector<std::unique_ptr<Interactor>> mouseTargets;

public:
	void clear()
	{
		styles.clear();
		visuals.clear();
		mouseTargets.clear();
	}
	void add(Child* v)
	{
		styles.push_back(std::unique_ptr<Child>(v));
	}
	void add(Visual* v)
	{
		visuals.push_back(std::unique_ptr<Visual>(v));
	}
	void add(Interactor* i)
	{
		mouseTargets.push_back(std::unique_ptr<Interactor>(i));
	}
};

#if 0
struct TopView : public ViewPort
{
	~TopView()
	{
		clearChildren(); // ensure States are deleted before StateHolders
	}

	void Invalidate(gmpi::drawing::Rect) const override;

	Environment* getEnvironment() override;
};
#endif

struct EditBox : public RectangularVisual
{
	std::string& text;

	EditBox(gmpi::drawing::Rect pbounds, std::string& ptext) : RectangularVisual(pbounds), text(ptext) {}
	void Draw(gmpi::drawing::Graphics& g) const override;
};

struct Rectangle : public RectangularVisual
{
	struct ShapeStyle* style = {};

#ifdef _DEBUG
	static inline int redrawCount = 0;
#endif

	Rectangle(ShapeStyle* pstyle, gmpi::drawing::Rect pbounds) : RectangularVisual(pbounds), style(pstyle){}
	void Draw(gmpi::drawing::Graphics& g) const override;
};

struct RoundedRectangle : public Rectangle
{
	float radiusX = 0;
	float radiusY = 0;
	RoundedRectangle(ShapeStyle* pstyle, gmpi::drawing::Rect pbounds, float pradiusX, float pradiusY)
		: Rectangle(pstyle, pbounds)
		, radiusX(pradiusX)
		, radiusY(pradiusY)
	{}
	void Draw(gmpi::drawing::Graphics& g) const override;
};

// A simple box containing text, where the text height is determined by the bounds height.

struct TextBox : public RectangularVisual
{
	struct TextBoxStyle* style = {};
	std::string text;

	TextBox(TextBoxStyle* pstyle, gmpi::drawing::Rect pbounds, std::string txt) : style(pstyle), RectangularVisual(pbounds), text(txt){}
	void Draw(gmpi::drawing::Graphics& g) const override;
};

struct PathHolder : public Child
{
	mutable gmpi::drawing::PathGeometry path;
	std::function<void(gmpi::drawing::PathGeometry&)> InitPath;
};

struct ShapeStyle : public Child
{
	mutable gmpi::drawing::Brush fill;
	mutable gmpi::drawing::Brush stroke;

	gmpi::drawing::Color strokeColor = gmpi::drawing::Colors::White;
	gmpi::drawing::Color fillColor = gmpi::drawing::Colors::Black;

	void Init(gmpi::drawing::Graphics& g) const;
	void Draw(gmpi::drawing::Graphics& g, const struct Shape& t, const PathHolder& path) const;
};

struct Shape : public RectangularVisual
{
	ShapeStyle* style = {};
	PathHolder* path = {};

	struct Shape(PathHolder* ppath, ShapeStyle* pstyle, gmpi::drawing::Rect pbounds/*, std::string txt*/) : RectangularVisual(pbounds), style(pstyle), path(ppath) {}
	void Draw(gmpi::drawing::Graphics& g) const override;
};

struct TextBoxStyle : public Child
{
	mutable gmpi::drawing::Brush foreGround;
	mutable gmpi::drawing::Brush backGround;
	mutable gmpi::drawing::TextFormat textFormat;

	gmpi::drawing::Color foregroundColor;
	gmpi::drawing::Color backgroundColor;

	float bodyHeight = 12; // i.e. text size
	float fixedLineSpacing = 0.0f; // 0.0f = auto.
	int textAlignment = (int) gmpi::drawing::TextAlignment::Leading; //left

	TextBoxStyle(gmpi::drawing::Color foreground = gmpi::drawing::Colors::White, gmpi::drawing::Color background = gmpi::drawing::colorFromHex(0x003E3E3Eu)) : foregroundColor(foreground), backgroundColor(background) {}
	void setLineSpacing(float fixedLineSpacing);
	void Draw(gmpi::drawing::Graphics& g, const TextBox& t) const;
};

struct Circle : public RectangularVisual
{
	Circle(gmpi::drawing::Rect pbounds) : RectangularVisual(pbounds) {}
	void Draw(gmpi::drawing::Graphics& g) const override;
};

struct Knob : public RectangularVisual
{
	float& normalized;

	Knob(gmpi::drawing::Rect pbounds, float& pnormalized) : RectangularVisual(pbounds), normalized(pnormalized){}
	void Draw(gmpi::drawing::Graphics& g) const override;
};


struct RectangleMouseTarget : public RectangularInteractor
{
#ifdef _DEBUG
	std::string debugName;
#endif
	RectangleMouseTarget(gmpi::drawing::Rect pbounds) : RectangularInteractor(pbounds) {}

	bool HitTest(gmpi::drawing::Point p) const override;
	bool wantsClicks() const override
	{
		return onPointerDown_callback || onPointerUp_callback || onPointerMove_callback;
	}
	bool onPointerDown(const PointerEvent* e) const override
	{
		if (onPointerDown_callback)
		{
			onPointerDown_callback(e);
			return true;
		}
		return false;
	}
	bool onPointerUp(gmpi::drawing::Point p) const override
	{
		if (onPointerUp_callback)
		{
			onPointerUp_callback(p);
			return true;
		}
		return false;
	}
	bool onPointerMove(gmpi::drawing::Point p) const override
	{
		if (onPointerMove_callback)
		{
			onPointerMove_callback(p);
			return true;
		}
		return false;
	}
	bool onMouseWheel(int32_t flags, int32_t delta, gmpi::drawing::Point p) const override
	{
		if (onMouseWheel_callback)
		{
			onMouseWheel_callback(flags, delta, p);
			return true;
		}
		return false;
	}

	bool setHover(bool isMouseOverMe) const override
	{
		if (onHover_callback)
		{
			onHover_callback(isMouseOverMe);
			return true;
		}
		return false;
	}

	void captureMouse(bool capture = true);

	std::function<void(bool)> onHover_callback;
	std::function<void(const PointerEvent*)> onPointerDown_callback;
	std::function<void(gmpi::drawing::Point)> onPointerUp_callback;
	std::function<void(gmpi::drawing::Point)> onPointerMove_callback;
	std::function<void(int32_t flags, int32_t delta, gmpi::drawing::Point)> onMouseWheel_callback;
};


class Portal : public RectangularVisual, public displayList, public IVisualParent
{
	friend class Form;

public:
	mutable gmpi::drawing::Rect contentBounds;
	gmpi::drawing::Rect getContentRect() const;

private:
	gmpi::drawing::Matrix3x2 transform;			// for drawing
	gmpi::drawing::Matrix3x2 reverseTransform;	// for invalidation

public:

	Portal(gmpi::drawing::Rect pbounds);
	virtual ~Portal();

	void setScroll(float dx, float dy);

	void Draw(gmpi::drawing::Graphics& g) const override;

	// IVisualParent
	void Invalidate(gmpi::drawing::Rect r) const override;
	gmpi_forms::Environment* getEnvironment() override { return parent->getEnvironment(); }
	gmpi::forms::primitive::displayList& getDisplayList() override { return *this; }
	gmpi::drawing::Matrix3x2 getTransform() const override
	{
		return parent->getTransform() * transform;
	}
};

class MousePortal : public RectangularInteractor
	, public mouseList
	, public IMouseParent
{
	friend class Form;

public:
	MousePortal(gmpi::drawing::Rect pbounds);
	virtual ~MousePortal() = default;

	Interactor* saveMouseState() const;
	void restoreMouseState(Interactor* prevMouseOverObject);
	void setScroll(float dx, float dy);

	bool HitTest(gmpi::drawing::Point p) const override;
	bool onPointerDown(const PointerEvent*) const override;

	bool onPointerUp(gmpi::drawing::Point point) const override;
	bool onPointerMove(gmpi::drawing::Point point) const override;
	bool onMouseWheel(int32_t flags, int32_t delta, gmpi::drawing::Point point) const override;
	bool setHover(bool isMouseOverMe) const override;

	// IMouseParent
	struct mouseList& getMouseTargetList() override
	{
		return *this;
	}
	// IMouseHost
	void captureMouse(gmpi::forms::primitive::Interactor* i) override { mouseParent->captureMouse(i); }
};
}
