#include "ComponentBuilders.h"
#include "forms.h"
#include "it_enum_list.h"
#include "conversion.h"

using namespace gmpi::forms;

namespace gmpi::ui::builder
{

void TextLabelView::Render(gmpi_forms::Environment* env, primitive::Canvas& canvas) const
{
	gmpi::drawing::Rect textBoxArea = getBounds();

	// default text style
	auto style = new primitive::TextBoxStyle(gmpi::drawing::colorFromHex(0xBFBDBFu), gmpi::drawing::Colors::TransparentBlack);
	style->bodyHeight = getHeight(textBoxArea) * 0.8f;
	style->textAlignment = (int)(rightAlign ? gmpi::drawing::TextAlignment::Trailing : gmpi::drawing::TextAlignment::Leading);
	canvas.add(style);

	// Draw the text
	auto tbox = new primitive::TextBox(style, textBoxArea, text2.get());

	canvas.add(
		tbox
	);
}

TextEditView::TextEditView(/*gmpi_forms::Environment* env,*/ std::string path)
{
	text.addObserver([this]()
		{
			setDirty();
		}
	);
	//	env->reg(&text, path + "/textEdit");
	//	model = std::make_shared<TextLabelModel>(patch);
//	_RPT0(0, "TextEditView\n");
}

void TextEditView::Render(gmpi_forms::Environment* env, primitive::Canvas& canvas) const
{
	// Add a text box.
	gmpi::drawing::Rect textBoxArea = getBounds();
	//	textBoxArea.bottom = textBoxArea.top + itemHeight;
	textBoxArea.left += editor_padding;
	textBoxArea.right -= editor_padding;

	// default text style
	auto style = new primitive::TextBoxStyle(
		gmpi::drawing::Colors::White					// text color
		, gmpi::drawing::Colors::TransparentBlack		// background color
	);
	style->textAlignment = (int)(rightAlign ? gmpi::drawing::TextAlignment::Trailing : gmpi::drawing::TextAlignment::Leading);

	canvas.add(style);

	// background rectangle
	{
		auto style2 = new primitive::ShapeStyle();
		style2->strokeColor = gmpi::drawing::Colors::TransparentBlack;
		style2->fillColor = gmpi::drawing::colorFromHex(0x383838u);
		constexpr float radius = 4.f;

		canvas.add(style2);

		auto backGroundRect = new primitive::RoundedRectangle(style2, getBounds(), radius, radius);
		canvas.add(backGroundRect);
	}

	// Draw the text
	auto tbox = new primitive::TextBox(style, textBoxArea, text.getSource() ? text.get() : "!!!");

	canvas.add(
		tbox
	);

	// Clicking over the textLabel brings up a native text-entry box
	{
		auto clickDetector = new primitive::RectangleMouseTarget(textBoxArea);
		canvas.add(clickDetector);

		const auto textRect = clickDetector->bounds;

		clickDetector->onPointerDown_callback = [this, env, tbox](const primitive::PointerEvent*)
			{
				// account for scrolling
				auto mtx = tbox->getTransform();
				const auto absoluteBounds = transformRect(mtx, gmpi::drawing::Rect{ 0.0f, 0.0f, getWidth(bounds), getHeight(bounds) });

				// creat text-editor
				gmpi::shared_ptr<gmpi::api::IUnknown> unknown;
				env->dialogHost->createTextEdit(&absoluteBounds, unknown.put());
				textEdit = unknown.as<gmpi::api::ITextEdit>();

				if (!textEdit)
					return;

				// TODO: !!! set font (at least height).
				textEdit->setAlignment((int32_t)gmpi::drawing::TextAlignment::Leading);
				textEdit->setText(tbox->text.c_str());

				textEdit->showAsync(
					new gmpi::sdk::TextEditCallback
					(
						[this](const std::string& newValue)
						{
							text = newValue;
						}
					)
				);
			};
	}
}

void Seperator::Render(gmpi_forms::Environment* env, primitive::Canvas& canvas) const
{
	auto style2 = new primitive::ShapeStyle();
	style2->strokeColor = gmpi::drawing::Colors::TransparentBlack;
	style2->fillColor = gmpi::drawing::colorFromHex(0x00444444u);

	canvas.add(style2);

	auto r = getBounds();
	r.top = 0.5f * (r.top + r.bottom);
	r.bottom = r.top + 1;

	auto thinLine = new primitive::Rectangle(style2, r);
	canvas.add(thinLine);
}

void ComboBoxView::Render(gmpi_forms::Environment* env, primitive::Canvas& canvas) const
{
	gmpi::drawing::Rect comboBoxArea = getBounds();
	comboBoxArea.left += editor_padding;
	comboBoxArea.right -= editor_padding;

	// default text style
	auto style = new primitive::TextBoxStyle(
		gmpi::drawing::colorFromHex(0xEEEEEEu) // text color
		, gmpi::drawing::Colors::TransparentBlack     // background color
	);
	canvas.add(style);

	// background rectangle
	{
		auto style2 = new primitive::ShapeStyle();
		style2->strokeColor = gmpi::drawing::Colors::TransparentBlack;
		style2->fillColor = gmpi::drawing::colorFromHex(0x003E3E3Eu);
		constexpr float radius = 4.f;

		canvas.add(style2);

		auto backGroundRect = new primitive::RoundedRectangle(style2, getBounds(), radius, radius);
		canvas.add(backGroundRect);
	}

	// Draw the text
	const auto enum_str = uc::WStringToUtf8(enum_get_substring(uc::Utf8ToWstring(enum_list.get()), enum_value.get()));

	auto textArea = comboBoxArea;
	const auto symbol_width = 0.7f * getHeight(comboBoxArea);
	textArea.right -= symbol_width;

	auto tbox = new primitive::TextBox(style, textArea, enum_str);

	canvas.add(
		tbox
	);

	// draw a drop-down symbol
	{
		auto symbolArea = comboBoxArea;
		symbolArea.left = symbolArea.right - symbol_width;
		symbolArea.top -= 4; // else too low

		auto tbox2 = new primitive::TextBox(style, symbolArea, "\xE2\x8C\x84"); // unicode 'down arrowhead'

		canvas.add(
			tbox2
		);
	}

	// Clicking over the textLabel brings up a native combo box
	auto clickDetector = new primitive::RectangleMouseTarget(comboBoxArea);
	{
		canvas.add(clickDetector);

		const auto textRect = clickDetector->bounds;

		clickDetector->onPointerDown_callback = [this, tbox, env](const primitive::PointerEvent*)
			{
				// account for scrolling
				auto mtx = tbox->getTransform();
				const auto absoluteBounds = transformRect(mtx, gmpi::drawing::Rect{ 0.0f, 0.0f, getWidth(bounds), getHeight(bounds) });

				// create combo-box
				gmpi::shared_ptr<gmpi::api::IUnknown> unknown;
				env->dialogHost->createPopupMenu(&absoluteBounds, unknown.put());
				combo = unknown.as<gmpi::api::IPopupMenu>();

				if (!combo)
					return;

				// TODO: !!! set font (at least height).
				combo->setAlignment((int32_t)gmpi::drawing::TextAlignment::Leading);

				// populate combo
				constexpr int32_t flags{};
				for (auto& [index, id, text] : it_enum_list2(enum_list.get()))
				{
					combo->addItem(text.c_str(), index, flags);
				}

				combo->showAsync(
					new gmpi::sdk::PopupMenuCallback
					(
						[this](int32_t selectedId)
						{
							it_enum_list it(Utf8ToWstring(enum_list.get()));
							it.FindIndex(selectedId);
							if (!it.IsDone())
								enum_value.set(it.CurrentItem()->value);
						}
					)
				);
			};
	}
}

void TickBox::Render(gmpi_forms::Environment* env, primitive::Canvas& canvas) const
{
	// Add a text box.
	const float itemHeight = 13;
	gmpi::drawing::Rect textBoxArea = getBounds();
	textBoxArea.bottom = textBoxArea.top + itemHeight;

	// default text style
	auto style = new primitive::TextBoxStyle(
		gmpi::drawing::Colors::White		// text color
		, gmpi::drawing::Colors::TransparentBlack	// background color
	);
	style->textAlignment = (int)gmpi::drawing::TextAlignment::Center;

	canvas.add(style);

	// background rectangle
	{
		auto style2 = new primitive::ShapeStyle();
		style2->strokeColor = gmpi::drawing::Colors::TransparentBlack;
		style2->fillColor = gmpi::drawing::colorFromHex(0x006e6e6eu);
		constexpr float radius = 4.f;

		canvas.add(style2);

		auto backGroundRect = new primitive::RoundedRectangle(style2, getBounds(), radius, radius);
		canvas.add(backGroundRect);
	}

	// Draw the text
	const char* text = value.get() ? "\xE2\x9C\x93" : " ";

	auto textbox = new primitive::TextBox(style, textBoxArea, text);

	canvas.add(textbox);

	// Clicking toggles the value
	auto clickDetector = new primitive::RectangleMouseTarget(textBoxArea);
	{
		canvas.add(clickDetector);

		clickDetector->onPointerDown_callback = [this, clickDetector](const primitive::PointerEvent*)
			{
				value.set(!value.get());
			};

		// don't work?
		//clickDetector->onPointerUp_callback = [this, clickDetector](gmpi::drawing::Point)
		//	{
		//		value.set()(!value.get());
		//	};
	}
}

void ToggleSwitch::Render(gmpi_forms::Environment* env, primitive::Canvas& canvas) const
{
	gmpi::drawing::Rect lableArea = getBounds();
	lableArea.right -= getHeight(lableArea) + 3.f;
	gmpi::drawing::Rect tickBoxArea = getBounds();
	tickBoxArea.left = tickBoxArea.right - getHeight(tickBoxArea);

	// add label

	// default label style
	auto style = new primitive::TextBoxStyle(gmpi::drawing::colorFromHex(0xBFBDBFu), gmpi::drawing::Colors::TransparentBlack);
	style->textAlignment = (int)gmpi::drawing::TextAlignment::Leading;

	canvas.add(style);

	// Draw the text
	auto tbox = new primitive::TextBox(style, lableArea, text.getSource() ? text.get() : "!!!");

	canvas.add(tbox);

	// Add the tickbox. (text)
	auto style3 = new primitive::TextBoxStyle(
		  gmpi::drawing::Colors::White				// text color
		, gmpi::drawing::Colors::TransparentBlack	// background color
	);
	style3->textAlignment = (int)gmpi::drawing::TextAlignment::Center;

	canvas.add(style3);

	// background rectangle
	{
		auto style2 = new primitive::ShapeStyle();
		style2->strokeColor = gmpi::drawing::Colors::TransparentBlack;
		style2->fillColor = gmpi::drawing::colorFromHex(0x006e6e6eu);
		constexpr float radius = 4.f;

		canvas.add(style2);

		auto backGroundRect = new primitive::RoundedRectangle(style2, tickBoxArea, radius, radius);
		canvas.add(backGroundRect);
	}

	// Draw the tcheckk mark
	std::string checktext = value.get() ? "\xE2\x9C\x93" : " ";

	auto tickbox = new primitive::TextBox(style3, tickBoxArea, checktext);

	canvas.add(tickbox);

	// Clicking toggles the value
	auto clickDetector = new primitive::RectangleMouseTarget(tickBoxArea);
	{
		canvas.add(clickDetector);

		clickDetector->onPointerDown_callback = [this, clickDetector](const primitive::PointerEvent*)
			{
				value = !value;
			};

		// don't work?
		//clickDetector->onPointerUp_callback = [this, clickDetector](gmpi::drawing::Point)
		//	{
		//		value.set()(!value.get());
		//	};
	}
}
void FileBrowseButtonView::Render(gmpi_forms::Environment* env, primitive::Canvas& canvas) const
{
	// Add a text box.
	const float itemHeight = 13;
	gmpi::drawing::Rect textBoxArea = getBounds();
	textBoxArea.bottom = textBoxArea.top + itemHeight;

	// default text style
	auto style = new primitive::TextBoxStyle(
		gmpi::drawing::colorFromHex(0xEEEEEEu) // text color
		, gmpi::drawing::Colors::TransparentBlack     // background color
	);
	style->textAlignment = (int)gmpi::drawing::TextAlignment::Center;
	canvas.add(style);

	// background rectangle
	{
		auto style2 = new primitive::ShapeStyle();
		style2->strokeColor = gmpi::drawing::Colors::TransparentBlack;
		style2->fillColor = gmpi::drawing::colorFromHex(0x006e6e6eu);
		constexpr float radius = 4.f;

		canvas.add(style2);

		auto backGroundRect = new primitive::RoundedRectangle(style2, getBounds(), radius, radius);
		canvas.add(backGroundRect);
	}

	// Draw the text on the button
	canvas.add(
		new primitive::TextBox(style, textBoxArea, "...")
	);

	// Clicking over the textLabel brings up a native combo box
	auto clickDetector = new primitive::RectangleMouseTarget(textBoxArea);
	{
		canvas.add(clickDetector);

		const auto textRect = clickDetector->bounds;

		clickDetector->onPointerDown_callback = [this, clickDetector, env](const primitive::PointerEvent*)
			{
				// get dialog host
				//gmpi::shared_ptr<gmpi::api::IDialogHost> dialogHost;
				//clickDetector->form->guiHost()->queryInterface(&gmpi::api::IDialogHost::guid, dialogHost.put_void());

				// creat file-browser
				gmpi::shared_ptr<gmpi::api::IUnknown> unknown;
				env->dialogHost->createFileDialog((int32_t)gmpi::api::FileDialogType::Save, unknown.put());
				fileDialog = unknown.as<gmpi::api::IFileDialog>();

				if (!fileDialog)
					return;

				fileDialog->setInitialFilename(value.get().c_str());

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
							value.set(selectedPath);
						})
				);
			};
	}
}

void PopupMenuView::Render(gmpi_forms::Environment* env, primitive::Canvas& canvas) const
{
	gmpi::drawing::Rect textBoxArea = getBounds();

	// default text style
	auto style = new primitive::TextBoxStyle(gmpi::drawing::colorFromHex(0xBFBDBFu), gmpi::drawing::Colors::TransparentBlack);
	style->bodyHeight = getHeight(textBoxArea) * 0.8f;
	style->textAlignment = (int)gmpi::drawing::TextAlignment::Leading;
	canvas.add(style);

	// Draw the text
	auto tbox = new primitive::TextBox(style, textBoxArea, text2.get());
	canvas.add(tbox);

	// Clicking the label brings up a popup menu
	auto clickDetector = new primitive::RectangleMouseTarget(textBoxArea);
	canvas.add(clickDetector);

	clickDetector->onPointerDown_callback = [this, tbox, env](const primitive::PointerEvent*)
		{
			// account for scrolling
			auto mtx = tbox->getTransform();
			const auto absoluteBounds = transformRect(mtx, gmpi::drawing::Rect{ 0.0f, 0.0f, getWidth(bounds), getHeight(bounds) });

			// create popup menu
			gmpi::shared_ptr<gmpi::api::IUnknown> unknown;
			env->dialogHost->createPopupMenu(&absoluteBounds, unknown.put());
			popupMenu = unknown.as<gmpi::api::IPopupMenu>();

			if (!popupMenu)
				return;

			popupMenu->setAlignment((int32_t)gmpi::drawing::TextAlignment::Leading);

			constexpr int32_t flags{};
			for (auto& [index, id, text] : it_enum_list2(menuItems.get()))
			{
				popupMenu->addItem(text.c_str(), id, flags);
			}

			popupMenu->showAsync(
				new gmpi::sdk::PopupMenuCallback(
					[this](int32_t selectedId)
					{
						if (onItemSelected && selectedId > 0)
						{
							onItemSelected(selectedId);
						}
					}
				)
			);
		};
}

bool ScrollPortal::RenderIfDirty(
	gmpi_forms::Environment* env,
	primitive::IVisualParent& parent_visual,
	primitive::IMouseParent& mouseParent
	) const
{
	// render myself
	const auto iwasdirty = dirty;

	if (dirty)
	{
		// recreate all children.
		View::RenderIfDirty(env, parent_visual, mouseParent);
	}
	else
	{
		// selectivly update children that are dirty.
		const auto mouseState = mouseportal->saveMouseState();

		bool childWasDirty = false;
		for (auto& view : childViews)
			childWasDirty |= view->RenderIfDirty(env, *portal, *mouseportal);

		if (childWasDirty)
			mouseportal->restoreMouseState(mouseState);
	}

	scroll_state.onChanged(); // update scroll position/size.

	return iwasdirty;
}

ScrollPortal::ScrollInfo ScrollPortal::calcScrollBar() const
{
	const auto visibleLength = getHeight(bounds);
	const auto contentLength = std::max(visibleLength, (getHeight(portal->getContentRect())));
	const auto thumbLength = (visibleLength / contentLength) * visibleLength;
	const auto thumbPosition = (-scroll_state.get() / std::max(contentLength - visibleLength, 0.01f)) * (visibleLength - thumbLength);

	return { visibleLength, contentLength, thumbLength, thumbPosition };
}

void ScrollPortal::Render(gmpi_forms::Environment* env, primitive::Canvas& canvas) const
{
	auto scrollBarBounds = bounds;
	scrollBarBounds.left = scrollBarBounds.right - 12;

	auto contentBounds = bounds;
	contentBounds.right = scrollBarBounds.left;

	portal = new primitive::Portal(contentBounds);
	canvas.add(portal);

	mouseportal = new primitive::MousePortal(contentBounds);
	canvas.add(mouseportal);

	// testing scrolling from mouse-wheel.
	auto mt = new primitive::RectangleMouseTarget(contentBounds);
	canvas.add(mt);

	// Scroll-bar
	{
		// drawing

		// background
		auto scrollBarBackgroundStyle = new primitive::ShapeStyle();
		scrollBarBackgroundStyle->strokeColor = gmpi::drawing::Colors::TransparentBlack;
		scrollBarBackgroundStyle->fillColor = gmpi::drawing::colorFromHex(0x505050);
		canvas.add(new primitive::Rectangle(scrollBarBackgroundStyle, scrollBarBounds));
		canvas.add(scrollBarBackgroundStyle);

		//thumb
		auto scrollBarStyle = new primitive::ShapeStyle();
		scrollBarStyle->strokeColor = gmpi::drawing::Colors::TransparentBlack;
		scrollBarStyle->fillColor = gmpi::drawing::colorFromHex(0xA0A0A0);
		auto r = scrollBarBounds;
		r.left += 4;
		r.right -= 4;
		scrollThumb = new primitive::RoundedRectangle(scrollBarStyle, r, 4.0f, 4.0f);
		canvas.add(scrollThumb);
		canvas.add(scrollBarStyle);

		// mouse
		scrollThumbMouseTarget = new primitive::RectangleMouseTarget(scrollBarBounds);
		canvas.add(scrollThumbMouseTarget);

		scrollThumbMouseTarget->onPointerDown_callback = [this](const primitive::PointerEvent* e)
			{
				// capture the scroller *state*, and manipulate it directly. let updates flow only from the state forward.
				e->boss->captureMouse(
					[this](gmpi::drawing::Size delta)
					{
						const auto info = calcScrollBar();
						const float scrollRatio = (std::max)(0.0f, info.contentLength) / (std::max)(1.0f, info.visibleLength);

						gmpi_forms::StateRef<float> scroll(scroll_state);

						const auto newScroll = std::clamp(scroll.get() - scrollRatio * delta.height, -(info.contentLength - info.visibleLength), 0.0f);
						scroll.set(newScroll);
					}
				);
			};
	}

	// the portal is an observer of the scroll state, so that it can update its transform when the scroll changes.
	scroll_state.addObserver([this, contentBounds]()
		{
			portal->setScroll(0, scroll_state.get());
			mouseportal->setScroll(0, scroll_state.get());

			// draw the scroll thumb
			const auto visibleLength = getHeight(contentBounds);
			const auto contentLength = std::max(visibleLength, (getHeight(portal->getContentRect())));
			const auto thumbLength = (visibleLength / contentLength) * visibleLength;
			const auto thumbPosition = (-scroll_state.get() / std::max(contentLength - visibleLength, 0.01f)) * (visibleLength - thumbLength);

			auto scrollThumbBounds = scrollThumb->bounds;
			scrollThumbBounds.top = contentBounds.top + thumbPosition;
			scrollThumbBounds.bottom = scrollThumbBounds.top + thumbLength;
			scrollThumb->setBounds(scrollThumbBounds);
			scrollThumbMouseTarget->bounds = scrollThumbBounds;

		});

	// create a persistent state to store the current vertical scroll position
	env->reg(scroll_state, "ScrollPortal.scroll");

	// the mouse target updates the model, not the portal directly.
	mt->onMouseWheel_callback = [this, contentBounds](int32_t flags, int32_t delta, gmpi::drawing::Point p)
		{
			const auto maxScroll = -(std::max)(0.0f, portal->getContentRect().bottom - getHeight(contentBounds));
			const auto newScroll = std::clamp(scroll_state.get() + delta * 0.2f, maxScroll, 0.0f);
			scroll_state.set(newScroll);
		};
}

void View::setDirty()
{
	dirty = true;
	// TODO!!! add to dirty list of top level, causing re-render on next frame.

}
void View::clear2()
{
	visuals.clear();
	mouseTargets.clear();

	children.visuals.clear();
	children.mouseTargets.clear();
	children.styles.clear();
}

void View::OnModelWillChange()
{
	setDirty();
}

bool View::RenderIfDirty(
	gmpi_forms::Environment* env,
	primitive::IVisualParent& parent,
	primitive::IMouseParent& mouseParent
) const
{
	if (!dirty)
		return false;

	dirty = false;

	// insert at current position in display-list if available, else at the end.
	auto prevVisual = visuals.first ? visuals.first->peerPrev : parent.getDisplayList().lastVisual.peerPrev;
	auto prevMouseTarget = mouseTargets.first ? mouseTargets.first->peerPrev : mouseParent.getMouseTargetList().lastMouseTarget.peerPrev;

	// remove old render and mouse-targets from linked-lists
	visuals.unlink();
	mouseTargets.unlink();

	// free objects
	children.clear();

	Render(env, children);

	visuals.replace(prevVisual, children.visuals);
	mouseTargets.replace(prevMouseTarget, children.mouseTargets);

	for (auto& c : children.visuals)
	{
		c->parent = &parent;
		parent.Invalidate(c->ClipRect());
	}

	for (auto& c : children.mouseTargets)
		c->mouseParent = &mouseParent;

	return true;
}
} // namespace gmpi::ui::builder