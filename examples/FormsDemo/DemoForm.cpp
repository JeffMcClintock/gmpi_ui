#include "DemoForm.h"

#include <format>

#include "experimental/builders.h"
#include "TextHelpers.h"

using namespace gmpi::ui;
using gmpi::ui::builder::fr;
using eAutoFlow = gmpi::ui::builder::ViewParent::eAutoFlow;

namespace
{

// The page's metrics, in DIPs.
const PageStyle kPageStyle{
    .margin = 12.0f, .rowHeight = 22.0f, .headingHeight = 16.0f, .gap = 4.0f, .labelFraction = 0.35f, .scrolls = false };

} // namespace

DemoForm::DemoForm()
{
    // The frame delivers WM_CHAR, so a control chord arrives as its ASCII control code.
    setKeyboardHandler([this](std::string glyph)
    {
        if (glyph == "\x1A") // ctrl+Z
            undo();
        else if (glyph == "\x19") // ctrl+Y
            redo();
    });
}

DemoForm::~DemoForm()
{
    // Form's destructor asserts the display and mouse lists are empty.
    clear();
}

void DemoForm::Body()
{
    const auto& m = model();

    Page page(bounds, kPageStyle);

    Heading heading("PROPERTIES");

    // Commits only text that is a number AND a different value: "abc", and "1.50" over "1.5", are ignored.
    {
        LabeledRow row("Amount");
        TextEdit editor(niceDoubleToString(m.amount), [this](const std::string& text)
        {
            if (const auto v = parseDouble(text); v && *v != model().amount)
                edit([&](Model& next) { next.amount = *v; });
        });
    }

    {
        LabeledRow row("Name");
        TextEdit editor(m.name, [this](const std::string& text)
        {
            if (const auto name = trimmed(text); name != model().name)
                edit([&](Model& next) { next.name = name; });
        });
    }

    {
        LabeledRow row("Enabled");
        TickBox tickBox(m.enabled, [this](bool on)
        {
            edit([on](Model& next) { next.enabled = on; });
        });
    }

    // Undo / redo: two fixed-width buttons and a filler. The widget set has no
    // disabled look, so a button with nothing to do says so in its caption.
    {
        constexpr float kButtonW = 90.0f;

        Grid row(
              { .gap = kPageStyle.gap, .auto_rows = kPageStyle.rowHeight, .auto_flow = eAutoFlow::columns, .column_widths = { kButtonW, kButtonW, fr(1.0f) } }
            , {}
        );

        Button undo(history_.canUndo() ? "Undo" : "(no undo)");
        undo.view->onClick = [this] { this->undo(); };

        Button redo(history_.canRedo() ? "Redo" : "(no redo)");
        redo.view->onClick = [this] { this->redo(); };

        Label filler("");
    }

    // Summary: what the model holds, at heading size so the line fits.
    Spacer spacer({ 0, 0, 0, kPageStyle.headingHeight });
    Label summary(
          std::format("amount = {}   name = \"{}\"   enabled = {}   (version {} of {})",
                      niceDoubleToString(m.amount), m.name, m.enabled ? "true" : "false",
                      history_.version(), history_.size())
        , { 0, 0, 0, kPageStyle.headingHeight }
    );
}
