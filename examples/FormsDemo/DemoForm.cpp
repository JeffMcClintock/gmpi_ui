#include "DemoForm.h"

#include <cstdlib>
#include <format>
#include <memory>
#include <string_view>

#include "experimental/builders.h"

using namespace gmpi::ui;
using gmpi::ui::builder::fr;
using eAutoFlow = gmpi::ui::builder::ViewParent::eAutoFlow;

namespace
{

// The page's metrics, in DIPs.
const PageStyle kPageStyle{
    .margin = 12.0f, .rowHeight = 22.0f, .headingHeight = 16.0f, .gap = 4.0f, .labelFraction = 0.35f, .scrolls = false };

// "1.5", "100", "0.001" - the shortest text that reads back as the same
// double. What a number entry should SHOW, as opposed to what the user typed.
std::string niceDoubleToString(double v)
{
    return std::format("{}", v);
}

// Parse the whole string as a number, allowing surrounding whitespace and
// nothing else. "12abc" is rejected rather than read as 12.
bool parseDouble(const std::string& text, double& out)
{
    const char* begin = text.c_str();
    char* end{};
    const double v = std::strtod(begin, &end);

    if (end == begin)
        return false;

    while (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n')
        ++end;

    if (*end != '\0')
        return false;

    out = v;
    return true;
}

std::string trimmed(std::string s)
{
    const auto isSpace = [](char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; };
    size_t b = 0, e = s.size();
    while (b < e && isSpace(s[b])) ++b;
    while (e > b && isSpace(s[e - 1])) --e;
    return s.substr(b, e - b);
}

} // namespace

DemoForm::DemoForm()
{
    // Keyboard shortcuts. The frame delivers WM_CHAR, so a control chord
    // arrives as the ASCII control code: ctrl+Z is 0x1A, ctrl+Y is 0x19.
    setKeyboardHandler([this](std::string glyph)
    {
        if (glyph == std::string(1, char(0x1A)))
            undo();
        else if (glyph == std::string(1, char(0x19)))
            redo();
    });
}

DemoForm::~DemoForm()
{
    // Form's destructor asserts the display and mouse lists are empty.
    clear();
}

void DemoForm::commit(Model next)
{
    // take() and push_back() each return a NEW vector sharing structure with
    // the old one; the versions they keep are untouched and still addressable.
    // Truncating first is what discards the redo tail after an undo.
    history_ = history_.take(cursor_ + 1).push_back(std::move(next));
    showVersion(history_.size() - 1);
}

void DemoForm::undo()
{
    if (canUndo())
        showVersion(cursor_ - 1);
}

void DemoForm::redo()
{
    if (canRedo())
        showVersion(cursor_ + 1);
}

void DemoForm::showVersion(std::size_t index)
{
    cursor_ = index;
    invalidateView();
}

void DemoForm::Body()
{
    Page page(bounds, kPageStyle);

    Heading heading("PROPERTIES");

    // A number entry commits only text that parses as a number AND differs in
    // value, so retyping "1.50" over "1.5" is a no-op and "abc" is ignored.
    {
        LabeledRow row("Amount");
        TextEdit editor(niceDoubleToString(model().amount), [this](const std::string& val)
        {
            double v{};
            if (parseDouble(val, v) && v != model().amount)
            {
                auto next = model();
                next.amount = v;
                commit(std::move(next));
            }
        });
    }

    {
        LabeledRow row("Name");
        TextEdit editor(model().name, [this](const std::string& val)
        {
            if (const auto newName = trimmed(val); newName != model().name)
            {
                auto next = model();
                next.name = newName;
                commit(std::move(next));
            }
        });
    }

    {
        LabeledRow row("Enabled");
        TickBox tickBox(model().enabled, [this](bool newValue)
        {
            auto next = model();
            next.enabled = newValue;
            commit(std::move(next));
        });
    }

    // --- undo / redo ---------------------------------------------------------
    // A row of two fixed-width buttons and a filler that absorbs the rest. A
    // button with nothing to do is still drawn (the widget set has no disabled
    // look) but says so in its caption, and its click is a no-op.
    {
        constexpr float kButtonW = 90.0f;

        Grid row(
              { .gap = kPageStyle.gap, .auto_rows = kPageStyle.rowHeight, .auto_flow = eAutoFlow::columns, .column_widths = { kButtonW, kButtonW, fr(1.0f) } }
            , {}
        );

        Button undo(canUndo() ? "Undo" : "(no undo)");
        undo.view->onClick = [this] { this->undo(); };

        Button redo(canRedo() ? "Redo" : "(no redo)");
        redo.view->onClick = [this] { this->redo(); };

        Label filler("");
    }

    // --- summary -------------------------------------------------------------
    // What the model currently holds, so an edit's effect is visible even when
    // the editor re-shows exactly what was typed. Heading-sized: a label's
    // text scales with its row, and at editor height this line would not fit.
    Spacer spacer({ 0, 0, 0, kPageStyle.headingHeight });
    const auto& m = model();
    Label summary(
          std::format("amount = {}   name = \"{}\"   enabled = {}   (version {} of {})",
                      niceDoubleToString(m.amount), m.name, m.enabled ? "true" : "false",
                      cursor_ + 1, history_.size())
        , { 0, 0, 0, kPageStyle.headingHeight }
    );
}
