#pragma once
#include <string>
#include <memory>
#include <functional>
#include <vector>
#include <algorithm>
#include <optional>
#include <string_view>
#include <cassert>
#include "GmpiUiDrawing.h"
#include "Core/GmpiApiEditor.h"
#include "./primatives.h"
#include "helpers/Timer.h"
#include "ui_facades.h"
#include "theme.h"

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
	// Ref-counted (not raw): queryInterface addRefs what it returns, and some
	// hosts hand out tear-offs whose lifetime rides on these refs.
	gmpi::shared_ptr<gmpi::api::IDrawingHost> host;
	gmpi::shared_ptr<gmpi::api::IInputHost> inputhost;
	// can just query	gmpi::api::IDialogHost* dialogHost = {};

	std::function<void(std::string)> onKey;
	std::function<void(gmpi::drawing::Size)> onMouseMove;

	uint32_t lastSeenThemeVersion_ = 0;

	bool onTimer() override;

	void DoUpdates();
	bool viewInvalid = false; // see invalidateView()
	ThemeMode builtTheme = ThemeMode::Dark; // the theme Body() last ran under
	void restoreMouseState(gmpi::forms::primitive::Interactor* prevMouseOverObject);

public:
	gmpi_forms::Environment env;
	inline static bool darkMode = true;

	// What render() fills the form's bounds with, as a colour of the current theme; nullptr paints nothing.
	gmpi::drawing::Color ColorTheme::* background = &ColorTheme::panelBackground;

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
	// Runs Body() again before the next paint. Call it when what Body() reads (the model, the bounds) changes.
	void invalidateView();
	// Scrolls a ScrollPortal back to the top, e.g. when the page shows a different object.
	void scrollToTop(std::string_view portalName = {});
	void setKeyboardHandler(std::function<void(std::string glyph)> pOnKey);

	// IVisualParent
	void Invalidate(gmpi::drawing::Rect r) const override;
	gmpi_forms::Environment* getEnvironment() override { return &env; }
	gmpi::forms::primitive::displayList& getDisplayList() override { return *this; }
	gmpi::drawing::Matrix3x2 getTransform() const override
	{
		return {};
	}

	// Paints behind the widgets: fills 'background'. Override to draw more behind them.
	virtual void renderBackground(gmpi::drawing::Graphics& g);

	// IDrawingClient (setHost is declared under IEditor below; one override serves all three client interfaces)
	// A form fills whatever room it is given, and lays itself out in 'bounds'.
	gmpi::ReturnCode measure(const gmpi::drawing::Size* availableSize, gmpi::drawing::Size* returnDesiredSize) override;
	gmpi::ReturnCode arrange(const gmpi::drawing::Rect* finalRect) override;
	// Runs Body() if the view is invalid or the theme changed, then paints the background and the widgets.
	gmpi::ReturnCode render(gmpi::drawing::api::IDeviceContext* drawingContext) override;
	gmpi::ReturnCode getClipArea(gmpi::drawing::Rect* returnRect) override;

	// IInputClient
	gmpi::ReturnCode setHover(bool isMouseOverMe) override;// { return gmpi::ReturnCode::NoSupport; }
	gmpi::ReturnCode hitTest(gmpi::drawing::Point, int32_t flags) override { return gmpi::ReturnCode::Ok; }
	gmpi::ReturnCode onPointerDown(gmpi::drawing::Point, int32_t flags) override;// { return gmpi::ReturnCode::NoSupport; }
	gmpi::ReturnCode onPointerMove(gmpi::drawing::Point, int32_t flags) override;// { return gmpi::ReturnCode::NoSupport; }
	gmpi::ReturnCode onPointerUp(gmpi::drawing::Point, int32_t flags) override;// { return gmpi::ReturnCode::NoSupport; }
	gmpi::ReturnCode onMouseWheel(gmpi::drawing::Point, int32_t flags, int32_t delta) override;// { return gmpi::ReturnCode::NoSupport; }

	// right-click menu
	gmpi::ReturnCode populateContextMenu(gmpi::drawing::Point, gmpi::api::IUnknown* contextMenuItemsSink) override { return gmpi::ReturnCode::NoSupport; }

	// tooltip
	gmpi::ReturnCode getToolTip(gmpi::drawing::Point, gmpi::api::IString* returnString) override { return gmpi::ReturnCode::Unhandled; }

	// keyboard events.
	gmpi::ReturnCode onKeyPress(wchar_t c) override;

	// IEditor
	gmpi::ReturnCode setHost(IUnknown* phost) override;
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
	gmpi::ui::builder::ViewParent* saveParent = {};
	gmpi::drawing::Rect bounds{};

public:
	// pname suffix lets multiple ScrollPortals in one form keep independent scroll state
	ScrollPortal(gmpi::drawing::Rect pbounds, std::string pname = {});
	~ScrollPortal();
};

// The metrics of a Page's rows.
struct PageStyle
{
	float margin = 8.0f;
	float rowHeight = 16.0f;
	float headingHeight = 14.0f;
	float gap = 3.0f;
	float labelFraction = 0.4f; // of the text width, for LabeledRow
	bool scrolls = true;
	std::string scrollName; // when a form has more than one scrolling page
};

// A page of rows, as a properties pane is: it scrolls, keeps a margin, and stacks what is declared
// inside it top to bottom. A widget declared with no bounds takes the page's row height.
//
//     Page page(area);
//     Heading _("OSCILLATOR");
//     { LabeledRow row("Pitch"); TextEdit _(pitchText, [this](auto& s) { setPitch(s); }); }
class Page
{
	const Page* outer;
	std::optional<ScrollPortal> portal;
	std::optional<Grid> rows;

public:
	const PageStyle style;
	float labelWidth{};

	Page(gmpi::drawing::Rect bounds, PageStyle style = {});
	~Page();

	// the innermost Page being built, if any
	static const Page* current();
};

// A Page row with a label column: the label, then whatever is declared inside it (an editor).
// With no text, the row's first child is the label column (e.g. an editable name).
class LabeledRow
{
	Grid grid;

public:
	LabeledRow();
	LabeledRow(std::string_view label);
};

// a section heading
struct Heading
{
	gmpi::ui::builder::TextLabelView* view = {};

	Heading(std::string_view text);
};

struct Spacer
{
	gmpi::ui::builder::Seperator* view = {};

	Spacer(gmpi::drawing::Rect bounds = {});
	void setBounds(gmpi::drawing::Rect newBounds);
};

//class ToggleSwitch_old
//{
//public:
//	ToggleSwitch_old(
//		std::string_view labelText,
//		gmpi::drawing::Rect bounds,
//		std::function<bool()> getValue,
//		std::function<void(bool)> setValue
//	);
//};

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

	struct Item
	{
		std::string text;
		int32_t id{};
	};

	ComboBox(gmpi::drawing::Rect bounds);
	// shows 'value' (an item's id), or "Multiple values" when there is none; 'commit' gets the id picked
	ComboBox(const std::vector<Item>& items, std::optional<int32_t> value, std::function<void(int32_t)> commit);
};

// A square box with a tick, at the left of its cell.
struct TickBox
{
	gmpi::ui::builder::TickBox* view = {};

	// shows 'value', or a dash when there is none; 'commit' gets the value the click asks for
	TickBox(std::optional<bool> value, std::function<void(bool)> commit);
};

// A push button. `onClick` is the whole interface:
//
//     Button b("Close", { l, t, r, b });
//     b.view->onClick = [this] { ... };
struct Button
{
	gmpi::ui::builder::ButtonView* view = {};

	Button(std::string_view caption, gmpi::drawing::Rect bounds = {});
};

// A determinate progress bar (see builder::ProgressBarView): bind view->fraction to a State<float> in 0..1.
struct ProgressBar
{
	gmpi::ui::builder::ProgressBarView* view = {};

	ProgressBar(gmpi::drawing::Rect bounds = {});
};

// A row of page titles (see builder::TabBarView):
//
//     TabBar tabs({ "Audio", "MIDI" }, { l, t, r, t + 40 });
//     tabs.view->selected.setSource(&page);
struct TabBar
{
	gmpi::ui::builder::TabBarView* view = {};

	TabBar(std::vector<std::string> titles, gmpi::drawing::Rect bounds = {});
};

struct TextEdit
{
	gmpi::ui::builder::TextEditView* view = {};
	gmpi_forms::StateRef<std::string> name;

	TextEdit(
		gmpi_forms::State<std::string>& pname
		// new. clean separated one-way back-channel.
		, std::function <void(const std::string&)> validateAndSave = {}
	);

	// Shows 'value', or "Multiple values" when there is none; 'commit' gets what the user typed. One-way:
	// the box keeps showing 'value' until Body() runs again, so a model that rejects the text needs do nothing.
	TextEdit(std::optional<std::string> value, std::function<void(const std::string&)> commit);
};

} // namespace gmpi::ui
