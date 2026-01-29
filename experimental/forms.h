#pragma once
#include <string>
#include <memory>
#include <functional>
#include <vector>
#include <algorithm>
#include <cassert>
#include "GmpiUiDrawing.h"
#include "experimental/Drawables.h"
#include "helpers/Timer.h"

#define editor_padding 4.f;

namespace gmpi_form_builder
{
	struct View : public gmpi_forms::IObserver
	{
		mutable bool dirty = true;
		mutable gmpi_forms::Child* firstVisual = {};
		mutable gmpi_forms::Child* lastVisual = {};

		virtual ~View() {}
		virtual void Render(gmpi_forms::Environment* env, gmpi_forms::ViewPort& parent) const {}
		void setDirty();
//		virtual void RenderDirtyViews();
		void OnModelWillChange() override;
	};

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

		ValueObserverLiteral(T value, gmpi_form_builder::View* pview) : value(value)
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
	template<typename T>
	struct ValueObserverLambda : public ValueObserver<T>
	{
		std::function<T(void)> getfunc;
		std::function<void (T)> setfunc;

		ValueObserverLambda(
			gmpi_form_builder::View* pview,
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

	struct TextLabelView : public View
	{
		gmpi::drawing::Rect bounds;
		bool rightAlign = false;
		std::unique_ptr< ValueObserver<std::string> > text2;

		TextLabelView() = default;
		TextLabelView(std::string_view staticText);

		void Render(gmpi_forms::Environment* env, gmpi_forms::ViewPort& parent) const override;
		gmpi::drawing::Rect getBounds() const
		{
			return bounds;
		}
	};

	struct Seperator : public View
	{
		gmpi::drawing::Rect bounds;

		Seperator() = default;

		void Render(gmpi_forms::Environment* env, gmpi_forms::ViewPort& parent) const override;
		gmpi::drawing::Rect getBounds() const
		{
			return bounds;
		}
	};

	struct TextEditView : public View
	{
		gmpi::drawing::Rect bounds;
		std::unique_ptr< ValueObserver<std::string> > text2;
		bool rightAlign = false;

		mutable gmpi::shared_ptr<gmpi::api::ITextEdit> textEdit;

		TextEditView(gmpi_forms::Environment* env, std::string path);
		~TextEditView();

		void Render(gmpi_forms::Environment* env, gmpi_forms::ViewPort& parent) const override;
		gmpi::drawing::Rect getBounds() const
		{
			return bounds;
		}
	};
	
	struct ComboBoxView : public View
	{
		gmpi::drawing::Rect bounds;
		std::unique_ptr< ValueObserver<std::string> > enum_list;
		std::unique_ptr< ValueObserver<int32_t> > enum_value;

		mutable gmpi::shared_ptr<gmpi::api::IPopupMenu> combo;

		ComboBoxView() = default;

		void Render(gmpi_forms::Environment* env, gmpi_forms::ViewPort& parent) const override;
		gmpi::drawing::Rect getBounds() const
		{
			return bounds;
		}
	};

	struct TickBox : public View
	{
		gmpi::drawing::Rect bounds;
		std::unique_ptr< ValueObserver<bool> > value;

		TickBox() = default;

		void Render(gmpi_forms::Environment* env, gmpi_forms::ViewPort& parent) const override;
		gmpi::drawing::Rect getBounds() const
		{
			return bounds;
		}
	};

	struct FileBrowseButtonView : public View
	{
		gmpi::drawing::Rect bounds;
		std::unique_ptr< ValueObserver<std::string> > value;
		mutable gmpi::shared_ptr<gmpi::api::IFileDialog> fileDialog; // needs to be kept alive, so it can be called async.
//		mutable gmpi::sdk::FileDialogCallback dialog_callback;

		FileBrowseButtonView(gmpi::drawing::Rect bounds) : bounds(bounds){}

		void Render(gmpi_forms::Environment* env, gmpi_forms::ViewPort& parent) const override;
		gmpi::drawing::Rect getBounds() const
		{
			return bounds;
		}
	};

	inline TextLabelView::TextLabelView(std::string_view staticText)
	{
		std::string txt(staticText);

		text2 = std::make_unique< gmpi_form_builder::ValueObserverLambda<std::string> >(
			this,
			[txt]() ->std::string
			{
				return txt;
			}
		);
	}

	inline void TextLabelView::Render(gmpi_forms::Environment* env, gmpi_forms::ViewPort& parent) const
	{
		// Add a text box.
		const float itemHeight = 13;
		//	const float itemMargin = 2;
		gmpi::drawing::Rect textBoxArea = getBounds();
		textBoxArea.bottom = textBoxArea.top + itemHeight;

		// default text style
		auto style = new gmpi_forms::TextBoxStyle(gmpi::drawing::colorFromHex(0xBFBDBFu), gmpi::drawing::Colors::TransparentBlack);
		style->textAlignment = (int)(rightAlign ? gmpi::drawing::TextAlignment::Trailing : gmpi::drawing::TextAlignment::Leading);
		parent.add(style);

		// Draw the text
		auto tbox = new gmpi_forms::TextBox(style, textBoxArea, text2->get());

		parent.add(
			tbox
		);
	}

	inline TextEditView::TextEditView(gmpi_forms::Environment* env, std::string path)
	{
		//	env->reg(&text, path + "/textEdit");
		//	model = std::make_shared<TextLabelModel>(patch);
	//	_RPT0(0, "TextEditView\n");
	}
	inline TextEditView::~TextEditView()
	{
		//	_RPT0(0, "~TextEditView\n");
	}

	inline void TextEditView::Render(gmpi_forms::Environment* env, gmpi_forms::ViewPort& parent) const
	{
		// Add a text box.
	//	const float itemHeight = 13;

		gmpi::drawing::Rect textBoxArea = getBounds();
		//	textBoxArea.bottom = textBoxArea.top + itemHeight;
		textBoxArea.left += editor_padding;
		textBoxArea.right -= editor_padding;

		// default text style
		auto style = new gmpi_forms::TextBoxStyle(
			gmpi::drawing::Colors::White					// text color
			, gmpi::drawing::Colors::TransparentBlack		// background color
		);
		style->textAlignment = (int)(rightAlign ? gmpi::drawing::TextAlignment::Trailing : gmpi::drawing::TextAlignment::Leading);

		parent.add(style);

		// background rectangle
		{
			auto style2 = new gmpi_forms::ShapeStyle();
			style2->strokeColor = gmpi::drawing::Colors::TransparentBlack;
			style2->fillColor = gmpi::drawing::colorFromHex(0x383838u);
			constexpr float radius = 4.f;

			parent.add(style2);

			auto backGroundRect = new gmpi_forms::RoundedRectangle(style2, getBounds(), radius, radius);
			parent.add(backGroundRect);
		}

		// Draw the text
		auto tbox = new gmpi_forms::TextBox(style, textBoxArea, text2->get());

		parent.add(
			tbox
		);

		// Clicking over the textLabel brings up a native text-entry box
		auto clickDetector = new gmpi_forms::RectangleMouseTarget(textBoxArea);
		{
			parent.add(clickDetector);

			const auto textRect = clickDetector->bounds;

			clickDetector->onPointerDown_callback = [this, tbox, textRect](gmpi_forms::PointerEvent*)
				{
					// get dialog host
					auto topview = dynamic_cast<gmpi_forms::TopView*>(tbox->parent);
					gmpi::shared_ptr<gmpi::api::IDialogHost> dialogHost;
					topview->form->guiHost()->queryInterface(&gmpi::api::IDialogHost::guid, dialogHost.put_void());

					// creat text-editor
					gmpi::shared_ptr<gmpi::api::IUnknown> unknown;
					dialogHost->createTextEdit(&bounds, unknown.put());
					textEdit = unknown.as<gmpi::api::ITextEdit>();

					if (!textEdit)
						return;

					// TODO: !!! set font (at least height).
					textEdit->setAlignment((int32_t)gmpi::drawing::TextAlignment::Leading);
					textEdit->setText(tbox->text.c_str());

					textEdit->showAsync(
						new gmpi::sdk::TextEditCallback
						(
							[this](const std::string& text)
							{
								text2->set(text);
							}
						)
					);
				};
		}
	}

	inline void Seperator::Render(gmpi_forms::Environment* env, gmpi_forms::ViewPort& parent) const
	{
		auto style2 = new gmpi_forms::ShapeStyle();
		style2->strokeColor = gmpi::drawing::Colors::TransparentBlack;
		style2->fillColor = gmpi::drawing::colorFromHex(0x00444444u);

		parent.add(style2);

		auto thinLine = new gmpi_forms::Rectangle(style2, getBounds());
		parent.add(thinLine);
	}

	inline void ComboBoxView::Render(gmpi_forms::Environment* env, gmpi_forms::ViewPort& parent) const
	{
		// Add a text box.
	//	const float itemHeight = 13;
		//	const float itemMargin = 2;
		gmpi::drawing::Rect comboBoxArea = getBounds();
		//	comboBoxArea.bottom = comboBoxArea.top + itemHeight;
		comboBoxArea.left += editor_padding;
		comboBoxArea.right -= editor_padding;

		// default text style
		auto style = new gmpi_forms::TextBoxStyle(
			gmpi::drawing::colorFromHex(0xEEEEEEu) // text color
			, gmpi::drawing::Colors::TransparentBlack     // background color
		);
		parent.add(style);

		// background rectangle
		{
			auto style2 = new gmpi_forms::ShapeStyle();
			style2->strokeColor = gmpi::drawing::Colors::TransparentBlack;
			style2->fillColor = gmpi::drawing::colorFromHex(0x003E3E3Eu);
			constexpr float radius = 4.f;

			parent.add(style2);

			auto backGroundRect = new gmpi_forms::RoundedRectangle(style2, getBounds(), radius, radius);
			parent.add(backGroundRect);
		}

		// Draw the text
		const auto enum_str = WStringToUtf8(enum_get_substring(Utf8ToWstring(enum_list->get()), enum_value->get()));

		auto textArea = comboBoxArea;
		const auto symbol_width = 0.7f * getHeight(comboBoxArea);
		textArea.right -= symbol_width;

		auto tbox = new gmpi_forms::TextBox(style, textArea, enum_str);

		parent.add(
			tbox
		);

		// draw a drop-down symbol
		{
			auto symbolArea = comboBoxArea;
			symbolArea.left = symbolArea.right - symbol_width;
			symbolArea.top -= 4; // else too low

			auto tbox2 = new gmpi_forms::TextBox(style, symbolArea, "\xE2\x8C\x84"); // unicode 'down arrowhead'

			parent.add(
				tbox2
			);
		}

		// Clicking over the textLabel brings up a native combo box
		auto clickDetector = new gmpi_forms::RectangleMouseTarget(comboBoxArea);
		{
			parent.add(clickDetector);

			const auto textRect = clickDetector->bounds;

			clickDetector->onPointerDown_callback = [this, tbox, textRect](gmpi_forms::PointerEvent*)
				{
					// get dialog host
					auto topview = dynamic_cast<gmpi_forms::TopView*>(tbox->parent);
					gmpi::shared_ptr<gmpi::api::IDialogHost> dialogHost;
					topview->form->guiHost()->queryInterface(&gmpi::api::IDialogHost::guid, dialogHost.put_void());

					// create combo-box
					gmpi::shared_ptr<gmpi::api::IUnknown> unknown;
					dialogHost->createPopupMenu(&bounds, unknown.put());
					combo = unknown.as<gmpi::api::IPopupMenu>();

					if (!combo)
						return;

					// TODO: !!! set font (at least height).
					combo->setAlignment((int32_t)gmpi::drawing::TextAlignment::Leading);

					// populate combo
					constexpr int32_t flags{};
					for (auto& [index, id, text] : it_enum_list2(enum_list->get()))
					{
						combo->addItem(text.c_str(), index, flags);
					}

					combo->showAsync(
						new gmpi::sdk::PopupMenuCallback
						(
							[this](int32_t selectedId)
							{
								it_enum_list it(Utf8ToWstring(enum_list->get()));
								it.FindIndex(selectedId);
								if (!it.IsDone())
									enum_value->set(it.CurrentItem()->value);
							}
						)
					);
				};
		}
	}

	inline void TickBox::Render(gmpi_forms::Environment* env, gmpi_forms::ViewPort& parent) const
	{
		// Add a text box.
		const float itemHeight = 13;
		gmpi::drawing::Rect textBoxArea = getBounds();
		textBoxArea.bottom = textBoxArea.top + itemHeight;

		// default text style
		auto style = new gmpi_forms::TextBoxStyle(
			gmpi::drawing::Colors::White		// text color
			, gmpi::drawing::Colors::TransparentBlack	// background color
		);
		style->textAlignment = GmpiDrawing_API::MP1_TEXT_ALIGNMENT::MP1_TEXT_ALIGNMENT_CENTER;

		parent.add(style);

		// background rectangle
		{
			auto style2 = new gmpi_forms::ShapeStyle();
			style2->strokeColor = gmpi::drawing::Colors::TransparentBlack;
			style2->fillColor = gmpi::drawing::colorFromHex(0x006e6e6eu);
			constexpr float radius = 4.f;

			parent.add(style2);

			auto backGroundRect = new gmpi_forms::RoundedRectangle(style2, getBounds(), radius, radius);
			parent.add(backGroundRect);
		}

		// Draw the text
	//	const char check[] = { (char)0xE2, (char)0x9C, (char)0x93, (char)0 };
		const char* text = value->get() ? "\xE2\x9C\x93" : " ";

		auto textbox = new gmpi_forms::TextBox(style, textBoxArea, text);

		parent.add(textbox);

		// Clicking toggles the value
		auto clickDetector = new gmpi_forms::RectangleMouseTarget(textBoxArea);
		{
			parent.add(clickDetector);

			clickDetector->onPointerDown_callback = [this, clickDetector](gmpi_forms::PointerEvent*)
				{
					value->set(!value->get());
				};

			// don't work?
			//clickDetector->onPointerUp_callback = [this, clickDetector](gmpi::drawing::Point)
			//	{
			//		value->set(!value->get());
			//	};
		}
	}

	inline void FileBrowseButtonView::Render(gmpi_forms::Environment* env, gmpi_forms::ViewPort& parent) const
	{
		// Add a text box.
		const float itemHeight = 13;
		gmpi::drawing::Rect textBoxArea = getBounds();
		textBoxArea.bottom = textBoxArea.top + itemHeight;

		// default text style
		auto style = new gmpi_forms::TextBoxStyle(
			gmpi::drawing::colorFromHex(0xEEEEEEu) // text color
			, gmpi::drawing::Colors::TransparentBlack     // background color
		);
		style->textAlignment = GmpiDrawing_API::MP1_TEXT_ALIGNMENT::MP1_TEXT_ALIGNMENT_CENTER;
		parent.add(style);

		// background rectangle
		{
			auto style2 = new gmpi_forms::ShapeStyle();
			style2->strokeColor = gmpi::drawing::Colors::TransparentBlack;
			style2->fillColor = gmpi::drawing::colorFromHex(0x006e6e6eu);
			constexpr float radius = 4.f;

			parent.add(style2);

			auto backGroundRect = new gmpi_forms::RoundedRectangle(style2, getBounds(), radius, radius);
			parent.add(backGroundRect);
		}

		// Draw the text on the button
		parent.add(
			new gmpi_forms::TextBox(style, textBoxArea, "...")
		);

		// Clicking over the textLabel brings up a native combo box
		auto clickDetector = new gmpi_forms::RectangleMouseTarget(textBoxArea);
		{
			parent.add(clickDetector);

			const auto textRect = clickDetector->bounds;

			clickDetector->onPointerDown_callback = [this, clickDetector](gmpi_forms::PointerEvent*)
				{
					// get dialog host
					gmpi::shared_ptr<gmpi::api::IDialogHost> dialogHost;
					clickDetector->form->guiHost()->queryInterface(&gmpi::api::IDialogHost::guid, dialogHost.put_void());

					// creat file-browser
					gmpi::shared_ptr<gmpi::api::IUnknown> unknown;
					dialogHost->createFileDialog((int32_t)gmpi::api::FileDialogType::Save, unknown.put());
					fileDialog = unknown.as<gmpi::api::IFileDialog>();

					if (!fileDialog)
						return;

					fileDialog->setInitialFilename(value->get().c_str());

					// TODO bind to extensions list
					//nativeFileDialog2->addExtension("xmlpreset");
					//nativeFileDialog2->addExtension("aupreset");
					//nativeFileDialog2->addExtension("vstpreset");
					fileDialog->addExtension("*");

					fileDialog->showAsync(
						nullptr,
						new gmpi::sdk::FileDialogCallback(
							[this](const std::string& selectedPath) -> void
							{
								value->set(selectedPath);
							})
					);
				};
		}
	}
} // namespace gmpi_form_builder

namespace gmpi_forms
{
inline thread_local std::vector< std::unique_ptr<gmpi_form_builder::View> >* ThreadLocalCurrentBuilder = {};

class Builder
{
	std::vector< std::unique_ptr<gmpi_form_builder::View> >& result;
public:

	Builder(std::vector< std::unique_ptr<gmpi_form_builder::View> >& presult) : result(presult)
	{
		gmpi_forms::ThreadLocalCurrentBuilder = &result;
	}
	~Builder()
	{
		gmpi_forms::ThreadLocalCurrentBuilder = {};
	}

	/* weird compiler error
	std::vector< std::unique_ptr<gmpi_form_builder::View> >& getResult()
	{
		return result;
	}
	*/
};

class Form :
	public IForm
	, public gmpi::api::IDrawingClient
	, public gmpi::api::IInputClient
	, public gmpi::api::IEditor
	, public gmpi::TimerClient
	, public gmpi_forms::IPointerBoss
{
protected:
	gmpi::api::IDrawingHost* host = {};
	gmpi::api::IInputHost* inputhost = {};
	// can just query	gmpi::api::IDialogHost* dialogHost = {};

	std::function<void(std::string)> onKey;
	// layout
	std::vector< std::unique_ptr<gmpi_form_builder::View> > body_;
	gmpi::drawing::Point prevPoint = {};

	bool onTimer() override;

	void DoUpdates();

public:
	Environment env;
	TopView child;

	Form();
	~Form()
	{
		child.clear(); // release anything pointing to states before releasing states (else crash).
		body_.clear();
	}

	virtual void Body() = 0;

	// IForm
	gmpi::api::IDrawingHost* guiHost() const override
	{
		return host;
	}
	Environment* getEnvironment() override
	{
		return &env;
	}
	void captureMouse(class Interactor*) override
	{
		// need to capture mouse and record the interactors ID, so that mouse events can be routed correcty.
		// perhaps interactors are stored as std::pair<ID, Interactor> in a map.
	}

	// IPointerBoss
	void captureMouse(std::function<void(gmpi::drawing::Size)> callback) override
	{
		//		guiHost()->setCapture();
	}

	// just redraw, without any changes to visuals
	void invalidate(gmpi::drawing::Rect* r) override;
	void redraw();
	virtual void renderVisuals();
	void setKeyboardHandler(std::function<void(std::string glyph)> pOnKey);

	// IDrawingClient
	gmpi::ReturnCode open(gmpi::api::IUnknown* phost) override;
	gmpi::ReturnCode measure(const gmpi::drawing::Size* availableSize, gmpi::drawing::Size* returnDesiredSize) override { return gmpi::ReturnCode::NoSupport; }
	gmpi::ReturnCode arrange(const gmpi::drawing::Rect* finalRect) override { return gmpi::ReturnCode::NoSupport; };
	gmpi::ReturnCode render(gmpi::drawing::api::IDeviceContext* drawingContext) override { return gmpi::ReturnCode::NoSupport; }
	gmpi::ReturnCode getClipArea(gmpi::drawing::Rect* returnRect) override { return gmpi::ReturnCode::NoSupport; }	// IInputClient

	// IInputClient
	gmpi::ReturnCode setHover(bool isMouseOverMe) override { return gmpi::ReturnCode::NoSupport; }
	gmpi::ReturnCode hitTest(gmpi::drawing::Point, int32_t flags) override { return gmpi::ReturnCode::NoSupport; }
	gmpi::ReturnCode onPointerDown(gmpi::drawing::Point, int32_t flags) override { return gmpi::ReturnCode::NoSupport; }
	gmpi::ReturnCode onPointerMove(gmpi::drawing::Point, int32_t flags) override { return gmpi::ReturnCode::NoSupport; }
	gmpi::ReturnCode onPointerUp(gmpi::drawing::Point, int32_t flags) override { return gmpi::ReturnCode::NoSupport; }
	gmpi::ReturnCode onMouseWheel(gmpi::drawing::Point, int32_t flags, int32_t delta) override { return gmpi::ReturnCode::NoSupport; }

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
}