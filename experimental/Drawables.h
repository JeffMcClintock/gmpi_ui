#pragma once
#include "mp_sdk_gui2.h"
#include "Drawing.h"
#include "notify.h"
#include "Utils.h"
#include "TimerManager.h"

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
		virtual void invalidate(GmpiDrawing::Rect* r) = 0;
		virtual Environment* getEnvironment() = 0;
		virtual gmpi_gui::IMpGraphicsHost* guiHost() const = 0;
		virtual void captureMouse(class Interactor*) = 0;
	};

	// A child is anything that can exist in a view, including drawable, and non-drawable (like styles)
	struct Child
	{
		virtual ~Child() {}
		virtual void onAddedToParent() {}
	};

	// IView ? SwiftUI Views aren't drawn, they are a factory that *creates* the things that are drawn.
	class Visual
	{
	public:
		Visual* parent = {};

		virtual ~Visual() {}
		virtual void Draw(GmpiDrawing::Graphics&) const {}
		virtual void Invalidate(GmpiDrawing::Rect) const;
		// the cliprect is the minimum area that needs to be painted to display the visual
		virtual GmpiDrawing::Rect ClipRect() const {return{};}

		virtual Environment* getEnvironment()
		{
			return parent->getEnvironment();
		}
	};

	struct IPointerBoss
	{
		virtual void captureMouse(std::function<void(GmpiDrawing::Size)> callback) = 0;
	};

	struct PointerEvent
	{
		GmpiDrawing::Point position;
		int32_t flags;
		IPointerBoss* boss;
	};

	// could be called 'MouseTarget' ?
	class Interactor
	{
	public:
		IForm* form = {};

		virtual ~Interactor() {}
		virtual bool HitTest(GmpiDrawing::Point) const = 0;
		// we want to support overlapping mouse targets, where one wants scroll-wheels and the other wants clicks
		virtual bool wantsClicks() const { return true; };

		// return true if handled
		virtual bool onPointerDown(PointerEvent*) const = 0;
		virtual bool onPointerUp(GmpiDrawing::Point) const = 0;
		virtual bool onPointerMove(GmpiDrawing::Point) const { return false; };
		virtual bool onMouseWheel(int32_t flags, int32_t delta, GmpiDrawing::Point) const = 0;
		virtual bool setHover(bool isMouseOverMe) const { return false; };
	};

	class ViewPort : public Visual, public Interactor
	{
	protected:
		struct childInfo
		{
			std::string id;
			std::unique_ptr<Child> child;
		};

		std::vector<childInfo> /*std::unique_ptr<Child >>*/ children; // can be Style, View, Interactor, or both

		// shapes that get drawn (to represent widgets)
		std::vector<Visual*> visuals;

		// areas where the mouse produces some effect
		std::vector<Interactor*> mouseTargets;
		mutable Interactor* mouseOverObject = {};
		mutable GmpiDrawing::Point mousePoint;
		mutable bool isHovered = {};

	public:
		GmpiDrawing::Matrix3x2 transform;			// for drawing
		GmpiDrawing::Matrix3x2 reverseTransform;	// for mouse

		void add(Child* child, const char* id = nullptr);
		void remove(Child* first, Child* last);
		int64_t count() const { return children.size(); }
		Child* get(int64_t index) { return children[index].child.get(); }
void clear();

		void Draw(GmpiDrawing::Graphics& g) const override;
		bool HitTest(GmpiDrawing::Point p) const override;
		//bool onPointerDown(GmpiDrawing::Point) const override;
		bool onPointerDown(PointerEvent*) const override;

		bool onPointerUp(GmpiDrawing::Point) const override;
		bool onPointerMove(GmpiDrawing::Point) const override;
		bool onMouseWheel(int32_t flags, int32_t delta, GmpiDrawing::Point) const override;
		bool setHover(bool isMouseOverMe) const override;

		virtual void Invalidate(GmpiDrawing::Rect) const;
	};


	struct ScrollView : public ViewPort, public Child
	{
		GmpiDrawing::Rect bounds = {};
		State<float> scroll;
		std::string path;

		ScrollView(std::string path, GmpiDrawing::Rect pbounds);
		void onAddedToParent() override;
		void UpdateTransform();
		GmpiDrawing::Rect ClipRect() const override { return bounds; }
	};

	struct TopView : public ViewPort, public Child
	{
//		Form* form = {};
		~TopView()
		{
			children.clear(); // ensure States are deleted before StateHoldsers
		}

//		void preRender()
//		{
////			mouseOverObject = {};
//		}
		void postRender()
		{
			// need to redraw and redetermine mouse-over object, but not get stuck in loop
//??			invalidate();
//?? how to			onPointerMove();
			// need to redetermine mouse-over object
			for (auto t : mouseTargets)
			{
				if (t->HitTest(mousePoint))
				{
					mouseOverObject = t;
					t->onPointerMove(mousePoint);
					break;
					//			return true;
				}
			}
		}

		void Invalidate(GmpiDrawing::Rect) const override;

		Environment* getEnvironment() override;
	};

	struct EditBox : public Visual, public Child
	{
		GmpiDrawing::Rect bounds = {};
		std::string& text;

		EditBox(GmpiDrawing::Rect pbounds, std::string& ptext) : bounds(pbounds), text(ptext) {}
		void Draw(GmpiDrawing::Graphics& g) const override;
		GmpiDrawing::Rect ClipRect() const override { return bounds; }
	};
#if 0

	// should be just stackpanel
	struct ListView : public Visual, public Interactor, public Child
	{
		GmpiDrawing::Rect bounds = {};
		std::vector< std::pair<std::string, std::string> >& data;

		std::function<void(const std::pair<std::string, std::string>&)> onItemSelected;

		ListView(GmpiDrawing::Rect pbounds, std::vector< std::pair<std::string, std::string> >& pdata) : bounds(pbounds), data(pdata) {}

		// drawing
		void Draw(GmpiDrawing::Graphics& g) const override;
		GmpiDrawing::Rect ClipRect() const override { return bounds; }

// interaction
bool HitTest(GmpiDrawing::Point p) const override;
bool onPointerDown(GmpiDrawing::Point) const override;

bool onMouseWheel(int32_t flags, int32_t delta, GmpiDrawing::Point) const override {
	return false;
}
	};
#endif
	struct Rectangle : public Visual, public Child
	{
		GmpiDrawing::Rect bounds = {};
		struct ShapeStyle* style = {};

#ifdef _DEBUG
		static inline int redrawCount = 0;
#endif

		Rectangle(ShapeStyle* pstyle, GmpiDrawing::Rect pbounds) : bounds(pbounds), style(pstyle){}
		void Draw(GmpiDrawing::Graphics& g) const override;
		GmpiDrawing::Rect ClipRect() const override { return bounds; }
	};

	struct ScrollBar : public Visual, public Child
	{
		GmpiDrawing::Rect bounds = {};
		struct ShapeStyle* style = {};
		State<float> position;
		std::string path;
		float scrollRangePixels = 1;

		ScrollBar(const char* ppath, ShapeStyle* pstyle, GmpiDrawing::Rect pbounds, float pscrollRangePixels) :
			path(ppath)
			, bounds(pbounds)
			, style(pstyle)
			, scrollRangePixels(pscrollRangePixels)
		{}

		void onAddedToParent() override
		{
			getEnvironment()->reg(&position, path + "scroll");
		}

		void Draw(GmpiDrawing::Graphics& g) const override;
		GmpiDrawing::Rect ClipRect() const override { return bounds; }
	};
	// A simple box containing text, where the text height is determined by the bounds height.

	struct TextBox : public Visual, public Child
	{
		struct TextBoxStyle* style = {};
		GmpiDrawing::Rect bounds = {};
		std::string text;

		TextBox(TextBoxStyle* pstyle, GmpiDrawing::Rect pbounds, std::string txt) : style(pstyle), bounds(pbounds), text(txt){}
		void Draw(GmpiDrawing::Graphics& g) const override;
		GmpiDrawing::Rect ClipRect() const override { return bounds; }
	};

	struct PathHolder : public Child
	{
		mutable GmpiDrawing::PathGeometry path;
		std::function<void(GmpiDrawing::PathGeometry&)> InitPath;
	};

	struct ShapeStyle : public Child
	{
		mutable GmpiDrawing::Brush fill;
		mutable GmpiDrawing::Brush stroke;

		GmpiDrawing::Color strokeColor = GmpiDrawing::Color::White;
		GmpiDrawing::Color fillColor = GmpiDrawing::Color::Black;

		void Init(GmpiDrawing::Graphics& g) const;
		void Draw(GmpiDrawing::Graphics& g, const struct Shape& t, const PathHolder& path) const;
	};

	struct Shape : public Visual, public Child
	{
		ShapeStyle* style = {};
		PathHolder* path = {};
		GmpiDrawing::Rect bounds = {};

		struct Shape(PathHolder* ppath, ShapeStyle* pstyle, GmpiDrawing::Rect pbounds/*, std::string txt*/) : style(pstyle), bounds(pbounds), path(ppath) {}
		void Draw(GmpiDrawing::Graphics& g) const override;
		GmpiDrawing::Rect ClipRect() const override { return bounds; }
	};

	struct TextBoxStyle : public Child
	{
		mutable GmpiDrawing::Brush foreGround;
		mutable GmpiDrawing::Brush backGround;
		mutable GmpiDrawing::TextFormat textFormat;

		GmpiDrawing::Color foregroundColor;
		GmpiDrawing::Color backgroundColor;

		float bodyHeight = 13; // i.e. text size
		float fixedLineSpacing = 0.0f; // 0.0f = auto.
		
		TextBoxStyle(GmpiDrawing::Color foreground = GmpiDrawing::Color::White, GmpiDrawing::Color background = GmpiDrawing::Color(0x003E3E3Eu)) : foregroundColor(foreground), backgroundColor(background) {}
		void setLineSpacing(float fixedLineSpacing);
		void Draw(GmpiDrawing::Graphics& g, const TextBox& t) const;
	};

	struct Circle : public Visual
	{
		GmpiDrawing::Rect bounds = {};

		Circle(GmpiDrawing::Rect pbounds) : bounds(pbounds) {}
		void Draw(GmpiDrawing::Graphics& g) const override;
		GmpiDrawing::Rect ClipRect() const override { return bounds; }
	};

	struct Knob : public Visual
	{
		GmpiDrawing::Rect bounds = {};
		float& normalized;

		Knob(GmpiDrawing::Rect pbounds, float& pnormalized) : bounds(pbounds), normalized(pnormalized){}
		void Draw(GmpiDrawing::Graphics& g) const override;
		GmpiDrawing::Rect ClipRect() const override { return bounds; }
	};


	struct RectangleMouseTarget : public Interactor, public Child
	{
		GmpiDrawing::Rect bounds = {};
		RectangleMouseTarget(GmpiDrawing::Rect pbounds) : bounds(pbounds) {}

		bool HitTest(GmpiDrawing::Point p) const override;
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
		bool onPointerUp(GmpiDrawing::Point p) const override
		{
			if (onPointerUp_callback)
			{
				onPointerUp_callback(p);
				return true;
			}
			return false;
		}
		bool onPointerMove(GmpiDrawing::Point p) const override
		{
			if (onPointerMove_callback)
			{
				onPointerMove_callback(p);
				return true;
			}
			return false;
		}
		bool onMouseWheel(int32_t flags, int32_t delta, GmpiDrawing::Point p) const override
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
		std::function<void(GmpiDrawing::Point)> onPointerUp_callback;
		std::function<void(GmpiDrawing::Point)> onPointerMove_callback;
		std::function<void(int32_t flags, int32_t delta, GmpiDrawing::Point)> onMouseWheel_callback;
	};
}

