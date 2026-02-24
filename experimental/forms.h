#pragma once
#include <string>
#include <memory>
#include <functional>
#include <vector>
#include <algorithm>
#include <cassert>
#include "GmpiUiDrawing.h"
#include "Core/GmpiApiEditor.h"
#include "experimental/Drawables.h"
#include "helpers/Timer.h"
#include "ComponentBuilders.h"

#define editor_padding 4.f;

namespace gmpi::ui
{
class Form :
	  public gmpi::api::IDrawingClient
	, public gmpi::api::IInputClient
	, public gmpi::api::IEditor
	, public gmpi::TimerClient

	, public gmpi::forms::primitive::IPointerBoss
	, public gmpi::forms::primitive::displayList
	, public gmpi::forms::primitive::mouseList
	, public gmpi::ui::builder::ViewParent
	, public gmpi::forms::primitive::IVisualParent
	, public gmpi::forms::primitive::IMouseParent
{
protected:
	gmpi::api::IDrawingHost* host = {};
	gmpi::api::IInputHost* inputhost = {};
	// can just query	gmpi::api::IDialogHost* dialogHost = {};

	std::function<void(std::string)> onKey;
	std::function<void(gmpi::drawing::Size)> onMouseMove;

	bool onTimer() override;

	void DoUpdates();
	void restoreMouseState(gmpi::forms::primitive::Interactor* prevMouseOverObject);

public:
	gmpi_forms::Environment env;

	Form();
	~Form()
	{
		// derived class to call clear() in destructor please, to ensure that any visuals/mousetargets are released before the states that they point to are released (else crash).
		assert(
			gmpi::forms::primitive::displayList::empty() &&
			gmpi::forms::primitive::mouseList::empty() &&
			childViews.empty()
		);
	}

	void clear()
	{
		// release anything pointing to states before releasing states (else crash).
		gmpi::forms::primitive::displayList::clear();
		gmpi::forms::primitive::mouseList::clear();
		childViews.clear();
	}

	virtual void Body() = 0;

	// IMouseParent
	struct mouseList& getMouseTargetList() override
	{
		return *this;
	}
	void captureMouse(gmpi::forms::primitive::Interactor*) override
	{
		// need to capture mouse and record the interactors ID, so that mouse events can be routed correcty.
		// perhaps interactors are stored as std::pair<ID, Interactor> in a map.
	}

	void captureMouse(std::function<void(gmpi::drawing::Size)> callback) override
	{
		onMouseMove = callback;
		inputhost->setCapture();
	}

	// just redraw, without any changes to visuals
	void redraw();
	virtual void renderVisuals();
	void setKeyboardHandler(std::function<void(std::string glyph)> pOnKey);

	// IVisualParent
	void Invalidate(gmpi::drawing::Rect r) const override;
	gmpi_forms::Environment* getEnvironment() override { return &env; }
	gmpi::forms::primitive::displayList& getDisplayList() override { return *this; }
	gmpi::drawing::Matrix3x2 getTransform() const override
	{
		return {};
	}

	// IDrawingClient
	gmpi::ReturnCode open(gmpi::api::IUnknown* phost) override;
	gmpi::ReturnCode measure(const gmpi::drawing::Size* availableSize, gmpi::drawing::Size* returnDesiredSize) override { return gmpi::ReturnCode::NoSupport; }
	gmpi::ReturnCode arrange(const gmpi::drawing::Rect* finalRect) override { return gmpi::ReturnCode::NoSupport; };
	gmpi::ReturnCode render(gmpi::drawing::api::IDeviceContext* drawingContext) override; // { return gmpi::ReturnCode::NoSupport; }
	gmpi::ReturnCode getClipArea(gmpi::drawing::Rect* returnRect) override { return gmpi::ReturnCode::NoSupport; }	// IInputClient

	// IInputClient
	gmpi::ReturnCode setHover(bool isMouseOverMe) override;// { return gmpi::ReturnCode::NoSupport; }
	gmpi::ReturnCode hitTest(gmpi::drawing::Point, int32_t flags) override { return gmpi::ReturnCode::Ok; }
	gmpi::ReturnCode onPointerDown(gmpi::drawing::Point, int32_t flags) override;// { return gmpi::ReturnCode::NoSupport; }
	gmpi::ReturnCode onPointerMove(gmpi::drawing::Point, int32_t flags) override;// { return gmpi::ReturnCode::NoSupport; }
	gmpi::ReturnCode onPointerUp(gmpi::drawing::Point, int32_t flags) override;// { return gmpi::ReturnCode::NoSupport; }
	gmpi::ReturnCode onMouseWheel(gmpi::drawing::Point, int32_t flags, int32_t delta) override;// { return gmpi::ReturnCode::NoSupport; }

	// right-click menu
	gmpi::ReturnCode populateContextMenu(gmpi::drawing::Point, gmpi::api::IUnknown* contextMenuItemsSink) override { return gmpi::ReturnCode::NoSupport; }
	gmpi::ReturnCode onContextMenu(int32_t idx) override { return gmpi::ReturnCode::NoSupport; }

	// keyboard events.
	gmpi::ReturnCode onKeyPress(wchar_t c) override;

	// IEditor
	gmpi::ReturnCode setHost(IUnknown* phost);
	gmpi::ReturnCode initialize() override { return gmpi::ReturnCode::NoSupport; }
	gmpi::ReturnCode setPin(int32_t PinIndex, int32_t voice, int32_t size, const uint8_t* data) override { return gmpi::ReturnCode::NoSupport; }
	gmpi::ReturnCode notifyPin(int32_t PinIndex, int32_t voice) override { return gmpi::ReturnCode::NoSupport; }

	gmpi::ReturnCode queryInterface(const gmpi::api::Guid* iid, void** returnInterface) override
	{
		GMPI_QUERYINTERFACE(gmpi::api::IEditor);
		//		GMPI_QUERYINTERFACE(gmpi::api::IEditor2);
		GMPI_QUERYINTERFACE(gmpi::api::IInputClient);
		GMPI_QUERYINTERFACE(gmpi::api::IDrawingClient);
		return gmpi::ReturnCode::NoSupport;
	}

	GMPI_REFCOUNT;
};

// temporary helper object to create the portal builder
// it uses RIAA to replace and later restore the parent builder-list
class ScrollPortal
{
	std::vector< std::unique_ptr<gmpi::ui::builder::View> >* saveParent = {};
	gmpi::drawing::Rect bounds{};
	
public:
	ScrollPortal(gmpi::drawing::Rect pbounds);
	~ScrollPortal();
};

struct Spacer
{
	gmpi::ui::builder::Seperator* view = {};

	Spacer(gmpi::drawing::Rect bounds = {});
	void setBounds(gmpi::drawing::Rect newBounds);
};

class ToggleSwitch_old
{
public:
	ToggleSwitch_old(
		std::string_view labelText,
		gmpi::drawing::Rect bounds,
		std::function<bool()> getValue,
		std::function<void(bool)> setValue
	);
};

struct ToggleSwitch
{
	gmpi::ui::builder::ToggleSwitch* view = {};

	ToggleSwitch(
		std::string_view labelText,
		gmpi_forms::State<bool>& value
	);
};

class Label
{
public:
	gmpi::ui::builder::TextLabelView* view = {};

	Label(std::string_view text, gmpi::drawing::Rect bounds = {});
	Label(gmpi::drawing::Rect bounds);
};

class ComboBox
{
public:
	gmpi::ui::builder::ComboBoxView* view = {};

	ComboBox(gmpi::drawing::Rect bounds);
};

struct TextEdit
{
	gmpi::ui::builder::TextEditView* view = {};
	gmpi_forms::StateRef<std::string> name;

	TextEdit(gmpi_forms::State<std::string>& pname);
};

class Grid
{
	std::vector< std::unique_ptr<gmpi::ui::builder::View> >* saveParent = {};
	std::vector< std::unique_ptr<gmpi::ui::builder::View> > childViews;
	gmpi::drawing::Rect bounds{};
	float gap = 0.0f;

public:
	Grid(gmpi::drawing::Rect pbounds = {}, float pspacing = 0.0f);
	~Grid();
};

} // namespace gmpi::ui