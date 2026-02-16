#pragma once
#include <map>
#include <memory>
#include "GmpiUiDrawing.h"
#include "helpers/NativeUi.h"

/*
create an interface IObservableObject

subclass it as a TreeViewModel or whatever

create a body() method that 'builds' the views and saves them in the environment (I guess)

whenever TreeViewModel is updated (though expansion/contraction) flag the view that built it as dirty,
 this should delete any visuals created previously by that view (and it's children), and rebuild them.

 this would require each view (or someone) to keep a list of visuals that it created

*/

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

	class Form;
	class Visual;
	struct Environment;

	template <typename T>
	struct State;

	// holds the value references by a state
	struct StateHolderbase
	{
		virtual ~StateHolderbase() = default;
	};

	template <typename T>
	struct StateHolder : public StateHolderbase
	{
	private:
		T value = {};

	public:
		std::vector<State<T>*> watchers;

		void set(T v)
		{
			value = v;

			for (auto& c : watchers)
				c->onChanged();
		}

		const T& get() const
		{
			return value;
		}

		void release(State<T>* s)
		{
			watchers.erase(
				std::remove_if(watchers.begin(), watchers.end(),
					[s](const auto* o) { return o == s; }),
				watchers.end());
		}
	};

	// a smart pointer to a state
	template <typename T>
	struct State
	{
		friend struct gmpi_forms::Environment;

		State() = default;
		State(StateHolder<T>* pstateholder) : stateholder(pstateholder)	{}

		const T& get() const
		{
			return stateholder->get();
		}

		void set(T value)
		{
			stateholder->set(value); // TEMP!!!
			// notify update for next frame			
			// and redraw.
		}

		void add(std::function<void(void)> callback)
		{
			callbacks.push_back(callback);
		}

		void onChanged()
		{
			for (auto& c : callbacks)
				c();
		}

		~State()
		{
			if(stateholder)
				stateholder->release(this);
		}

		StateHolder<T>* getSource() {
			return stateholder;
		}

	protected:
		StateHolder<T>* stateholder = {};
		std::vector < std::function<void(void)> > callbacks;
	};

	struct Environment
	{
		//		Environment* parent = {};
		//		std::vector<std::unique_ptr<Environment>> children;
	private:
		std::map<std::string, std::unique_ptr< StateHolderbase > > states;

	public:
		//		std::string id;

		template <typename T>
		void reg(State<T>* state, std::string path)
		{
			StateHolder<T>* s = {};

			auto it = states.find(path);
			if (it != states.end())
			{
				s = dynamic_cast< StateHolder<T>* >((*it).second.get());
			}
			else
			{
				auto temp = std::make_unique< StateHolder<T> >();
				s = temp.get();
				states[path] = std::move(temp);
			}

			state->stateholder = s;
			s->watchers.push_back(state);
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

	// The top-level object, that manages the views
	struct IForm
	{
		virtual void invalidate(gmpi::drawing::Rect* r) = 0;
		virtual Environment* getEnvironment() = 0;
		virtual gmpi::api::IDrawingHost* guiHost() const = 0;
		virtual void captureMouse(class Interactor*) = 0;
	};

	// A child is anything that can exist in a view, including drawable, and non-drawable (like styles)
	struct Child
	{
		Child* peerPrev = nullptr;
		Child* peerNext = nullptr;

		virtual ~Child() { peerUnlink(); }
		virtual void onAddedToParent() {}

		bool peerLinked() const { return peerPrev || peerNext; }

		void peerUnlink()
		{
			if (peerPrev)
				peerPrev->peerNext = peerNext;
			if (peerNext)
				peerNext->peerPrev = peerPrev;
			peerPrev = nullptr;
			peerNext = nullptr;
		}

		void peerInsertAfter(Child* pos)
		{
			peerUnlink();
			if (!pos)
				return;
			peerPrev = pos;
			peerNext = pos->peerNext;
			pos->peerNext->peerPrev = this;
			pos->peerNext = this;
		}

		void peerInsertBefore(Child* pos)
		{
			peerUnlink();
			if (!pos)
				return;
			peerNext = pos;
			peerPrev = pos->peerPrev;
			if (pos->peerPrev)
				pos->peerPrev->peerNext = this;
			pos->peerPrev = this;
		}

		Child* append(Child* next)
		{
			next->peerNext = peerNext;
			next->peerPrev = this;
			peerNext->peerPrev = next;
			peerNext = next;

			return next;
		}
	};

	template <typename T>
	struct intrusiveListItem
	{
		mutable T* peerPrev = nullptr;
		mutable T* peerNext = nullptr;
	};

	// IView ? SwiftUI Views aren't drawn, they are a factory that *creates* the things that are drawn.
	class Visual : public intrusiveListItem<Visual>
	{
	public:
		Visual* parent = {};

		virtual ~Visual() {}
		virtual void Draw(gmpi::drawing::Graphics&) const {}
		virtual void Invalidate(gmpi::drawing::Rect) const;
		// the cliprect is the minimum area that needs to be painted to display the visual
		virtual gmpi::drawing::Rect ClipRect() const {return{};}

		virtual Environment* getEnvironment()
		{
			return parent->getEnvironment();
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
		IForm* form = {};

		Interactor() = default;
		virtual ~Interactor() {}
		virtual bool HitTest(gmpi::drawing::Point) const { return false; }
		// we want to support overlapping mouse targets, where one wants scroll-wheels and the other wants clicks
		virtual bool wantsClicks() const { return true; }

		// return true if handled
		virtual bool onPointerDown(PointerEvent*) const { return false; }
		virtual bool onPointerUp(gmpi::drawing::Point) const { return false; }
		virtual bool onPointerMove(gmpi::drawing::Point) const { return false; }
		virtual bool onMouseWheel(int32_t flags, int32_t delta, gmpi::drawing::Point) const { return false; }
		virtual bool setHover(bool isMouseOverMe) const { return false; }
	};

	struct Portalx : public Visual
	{
		gmpi::drawing::Rect bounds{};
		mutable gmpi::drawing::Matrix3x2 originalTransform{};

		gmpi::drawing::Rect ClipRect() const override { return bounds; }
		void Draw(gmpi::drawing::Graphics& g) const override
		{
			originalTransform = g.getTransform();
			auto shifted = originalTransform * gmpi::drawing::makeTranslation(bounds.left, bounds.top);
			g.setTransform(shifted);
		}
	};


	struct PortalStart : public Visual
	{
		gmpi::drawing::Rect bounds{};
		mutable gmpi::drawing::Matrix3x2 originalTransform{};

		gmpi::drawing::Rect ClipRect() const override { return bounds; }
		void Draw(gmpi::drawing::Graphics& g) const override
		{
			originalTransform = g.getTransform();
			auto shifted = originalTransform * gmpi::drawing::makeTranslation(bounds.left, bounds.top);
			g.setTransform(shifted);
		}
	};

	struct PortalEnd : public Visual
	{
		struct PortalStart* buddy = {};

		gmpi::drawing::Rect ClipRect() const override { return buddy->bounds; }
		void Draw(gmpi::drawing::Graphics& g) const override
		{
			g.setTransform(buddy->originalTransform);
		}
	};


	class ViewPort : public Visual, public Interactor
	{
		friend class Form;

	protected:
		// intrusive linked lists of all drawables with sentinel nodes
		Visual firstVisual;
		Visual lastVisual;
		Interactor firstMouseTarget;
		Interactor lastMouseTarget;

		void clearChildren();

		// areas where the mouse produces some effect
		mutable Interactor* mouseOverObject = {};
		mutable gmpi::drawing::Point mousePoint;
		mutable bool isHovered = {};

	public:
		gmpi::drawing::Matrix3x2 transform;			// for drawing
		gmpi::drawing::Matrix3x2 reverseTransform;	// for mouse

		ViewPort();
		virtual ~ViewPort();
void clear();

		void Draw(gmpi::drawing::Graphics& g) const override;
		bool HitTest(gmpi::drawing::Point p) const override;
		bool onPointerDown(PointerEvent*) const override;

		bool onPointerUp(gmpi::drawing::Point point) const override;
		bool onPointerMove(gmpi::drawing::Point point) const override;
		bool onMouseWheel(int32_t flags, int32_t delta, gmpi::drawing::Point point) const override;
		bool setHover(bool isMouseOverMe) const override;
		virtual void Invalidate(gmpi::drawing::Rect) const;
	};

	class Portal : public Visual // , public Interactor
	{
		friend class Form;

	public:
		// intrusive linked lists of all drawables with sentinel nodes
		Visual firstVisual;
		Visual lastVisual;

		void clearChildren();

		// areas where the mouse produces some effect
		mutable Interactor* mouseOverObject = {};
		mutable gmpi::drawing::Point mousePoint;
		mutable bool isHovered = {};
		gmpi::drawing::Rect bounds;

	public:
		gmpi::drawing::Matrix3x2 transform;			// for drawing
		gmpi::drawing::Matrix3x2 reverseTransform;	// for mouse

		Portal(gmpi::drawing::Rect pbounds);
		virtual ~Portal();
		void clear();

		virtual gmpi::drawing::Rect ClipRect() const { return bounds; }
		void Draw(gmpi::drawing::Graphics& g) const override;
#if 0
		bool HitTest(gmpi::drawing::Point p) const override;
		bool onPointerDown(PointerEvent*) const override;

		bool onPointerUp(gmpi::drawing::Point point) const override;
		bool onPointerMove(gmpi::drawing::Point point) const override;
		bool onMouseWheel(int32_t flags, int32_t delta, gmpi::drawing::Point point) const override;
		bool setHover(bool isMouseOverMe) const override;
#endif
		virtual void Invalidate(gmpi::drawing::Rect) const;
	};

	// not really a canvas, just a temporary list of stuff
	struct Canvas
	{
		friend class Form;

//	protected:
		std::vector<std::unique_ptr<Child>> styles;
		std::vector<std::unique_ptr<Visual>> visuals;
		std::vector<std::unique_ptr<Interactor>> mouseTargets;

	public:
		
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

		void add(struct ScrollView* i);
	};

	struct ScrollView : public ViewPort //, public Child
	{
		gmpi::drawing::Rect bounds = {};
		State<float> scroll;
		std::string path;
		Canvas canvas;

		ScrollView(std::string path, gmpi::drawing::Rect pbounds);
		~ScrollView();
		void onAddedToParent(); // override;
		void UpdateTransform();
		gmpi::drawing::Rect ClipRect() const override { return bounds; }
	};

	// moved below ScrollView definition.
	inline void Canvas::add(ScrollView* i)
	{
		visuals.push_back(std::unique_ptr<Visual>(i));
		// TODO : separate mouse targets from visuals, just have a seperate mousetarget over the scroller/viewport.
// no, double delete		mouseTargets.push_back(std::unique_ptr<Interactor>(i));
	}


	struct TopView : public ViewPort //, public Child
	{
		~TopView()
		{
			clearChildren(); // ensure States are deleted before StateHolders
		}

		void postRender()
		{
			// need to redraw and redetermine mouse-over object, but not get stuck in loop
//??			invalidate();
//?? how to			onPointerMove();

			// need to redetermine mouse-over object
			// iterate mouseTargets in reverse (to respect Z-order)
			for (auto t = firstMouseTarget.peerNext ; t != &lastMouseTarget; t = t->peerNext)
			{
				if (t->HitTest(mousePoint) && t->wantsClicks())
				{
					t->onPointerMove(mousePoint);
					break;
				}
			}
		}

		void Invalidate(gmpi::drawing::Rect) const override;

		Environment* getEnvironment() override;
	};

	struct EditBox : public Visual, public Child
	{
		gmpi::drawing::Rect bounds = {};
		std::string& text;

		EditBox(gmpi::drawing::Rect pbounds, std::string& ptext) : bounds(pbounds), text(ptext) {}
		void Draw(gmpi::drawing::Graphics& g) const override;
		gmpi::drawing::Rect ClipRect() const override { return bounds; }
	};

	struct Rectangle : public Visual //, public Child
	{
		gmpi::drawing::Rect bounds = {};
		struct ShapeStyle* style = {};

#ifdef _DEBUG
		static inline int redrawCount = 0;
#endif

		Rectangle(ShapeStyle* pstyle, gmpi::drawing::Rect pbounds) : bounds(pbounds), style(pstyle){}
		void Draw(gmpi::drawing::Graphics& g) const override;
		gmpi::drawing::Rect ClipRect() const override { return bounds; }
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

	struct ScrollBar : public Visual //, public Child
	{
		gmpi::drawing::Rect bounds = {};
		struct ShapeStyle* style = {};
		State<float> position;
		std::string path;
		float scrollRangePixels = 1;

		ScrollBar(const char* ppath, ShapeStyle* pstyle, gmpi::drawing::Rect pbounds, float pscrollRangePixels) :
			path(ppath)
			, bounds(pbounds)
			, style(pstyle)
			, scrollRangePixels(pscrollRangePixels)
		{}

		void onAddedToParent() //override
		{
			getEnvironment()->reg(&position, path + "scroll");
		}

		void Draw(gmpi::drawing::Graphics& g) const override;
		gmpi::drawing::Rect ClipRect() const override { return bounds; }
	};
	// A simple box containing text, where the text height is determined by the bounds height.

	struct TextBox : public Visual //, public Child
	{
		struct TextBoxStyle* style = {};
		gmpi::drawing::Rect bounds = {};
		std::string text;

		TextBox(TextBoxStyle* pstyle, gmpi::drawing::Rect pbounds, std::string txt) : style(pstyle), bounds(pbounds), text(txt){}
		void Draw(gmpi::drawing::Graphics& g) const override;
		gmpi::drawing::Rect ClipRect() const override { return bounds; }
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

	struct Shape : public Visual //, public Child
	{
		ShapeStyle* style = {};
		PathHolder* path = {};
		gmpi::drawing::Rect bounds = {};

		struct Shape(PathHolder* ppath, ShapeStyle* pstyle, gmpi::drawing::Rect pbounds/*, std::string txt*/) : style(pstyle), bounds(pbounds), path(ppath) {}
		void Draw(gmpi::drawing::Graphics& g) const override;
		gmpi::drawing::Rect ClipRect() const override { return bounds; }
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

	struct Circle : public Visual
	{
		gmpi::drawing::Rect bounds = {};

		Circle(gmpi::drawing::Rect pbounds) : bounds(pbounds) {}
		void Draw(gmpi::drawing::Graphics& g) const override;
		gmpi::drawing::Rect ClipRect() const override { return bounds; }
	};

	struct Knob : public Visual
	{
		gmpi::drawing::Rect bounds = {};
		float& normalized;

		Knob(gmpi::drawing::Rect pbounds, float& pnormalized) : bounds(pbounds), normalized(pnormalized){}
		void Draw(gmpi::drawing::Graphics& g) const override;
		gmpi::drawing::Rect ClipRect() const override { return bounds; }
	};


	struct RectangleMouseTarget : public Interactor //, public Child
	{
		gmpi::drawing::Rect bounds = {};
#ifdef _DEBUG
		std::string debugName;
#endif
		RectangleMouseTarget(gmpi::drawing::Rect pbounds) : bounds(pbounds) {}

		bool HitTest(gmpi::drawing::Point p) const override;
		bool wantsClicks() const override
		{
			return onPointerDown_callback || onPointerUp_callback || onPointerMove_callback;
		}
		bool onPointerDown(PointerEvent* e) const override
		{
			if (onPointerDown_callback)
			{
				onPointerDown_callback(e);// ->position);
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
		std::function<void(PointerEvent*)> onPointerDown_callback;
		std::function<void(gmpi::drawing::Point)> onPointerUp_callback;
		std::function<void(gmpi::drawing::Point)> onPointerMove_callback;
		std::function<void(int32_t flags, int32_t delta, gmpi::drawing::Point)> onMouseWheel_callback;
	};
}

