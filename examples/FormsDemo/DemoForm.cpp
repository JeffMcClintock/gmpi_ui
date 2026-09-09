#include "DemoForm.h"

#include <cstdlib>
#include <format>
#include <memory>
#include <string_view>

#include "experimental/builders.h"

using namespace gmpi::ui;
using gmpi::ui::builder::fr;
using gmpi::ui::builder::auto_size;
using eAutoFlow = gmpi::ui::builder::ViewParent::eAutoFlow;

namespace
{

// Layout metrics, in DIPs. The same trio PropertiesBrowser uses: a row height
// for editors, a line gap between rows, and a margin around the whole page.
constexpr float kRowHeight     = 22.0f;
constexpr float kLineSpacing   = 4.0f;
constexpr float kOuterMargin   = 12.0f;
constexpr float kHeadingHeight = 16.0f;

// The label column as a fraction of the text area, like PropertiesBrowser's
// draggable divider - only here it does not move.
constexpr float kLabelColumnFraction = 0.35f;

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
    refreshStates();
}

DemoForm::~DemoForm()
{
    // Form's destructor asserts the display and mouse lists are empty: the
    // visuals point at the State members above, which are destroyed first.
    clear();
}

void DemoForm::refreshStates()
{
    const auto& m = model();
    amountText_   = niceDoubleToString(m.amount);
    nameText_     = m.name;
    enabledState_ = m.enabled;
}

void DemoForm::commit(Model next)
{
    // push_back returns a NEW vector sharing structure with the old one; the
    // previous versions are untouched and still addressable.
    history_ = history_.push_back(std::move(next));

    refreshStates();

    // Rebuild rather than patch: the summary line below the controls is
    // re-created with fresh text on the next render.
    formIsDirty_ = true;
    redraw();
}

gmpi::ReturnCode DemoForm::arrange(const gmpi::drawing::Rect* finalRect)
{
    bounds = *finalRect;
    formIsDirty_ = true;
    Invalidate({});
    return gmpi::ReturnCode::Ok;
}

gmpi::ReturnCode DemoForm::render(gmpi::drawing::api::IDeviceContext* dc)
{
    const auto mode = gmpi::ui::themeModeStorage();
    if (mode != lastRenderedTheme_)
        formIsDirty_ = true;

    if (formIsDirty_)
    {
        formIsDirty_ = false;
        lastRenderedTheme_ = mode;
        renderVisuals(); // runs Body() and lays the result out
    }

    gmpi::drawing::Graphics g(dc);
    g.clear(gmpi::ui::currentTheme().panelBackground);

    return Form::render(dc);
}

void DemoForm::Body()
{
    gmpi::drawing::Rect textArea = bounds;
    textArea.left   += kOuterMargin;
    textArea.right  -= kOuterMargin;
    textArea.top    += kOuterMargin;
    textArea.bottom -= kOuterMargin;

    const float labelColumnWidth = kLabelColumnFraction * (textArea.right - textArea.left);

    // The page: a stack of rows, each as tall as its content asks to be.
    Grid outer(
          { .gap = kLineSpacing, .auto_flow = eAutoFlow::rows, .default_track_size = auto_size() }
        , textArea
    );

    Label heading("PROPERTIES", { 0, 0, 0, kHeadingHeight });

    // One label/editor row. The nested grid maps one column track per child:
    // the label takes the fixed left column and the editor the remaining 1fr.
    auto beginRow = [&](std::string_view labelText)
    {
        auto row = std::make_unique<Grid>(
              gmpi::ui::builder::ViewParent::Initializer{
                    .gap = kLineSpacing
                  , .auto_rows = kRowHeight
                  , .auto_flow = eAutoFlow::columns
                  , .column_widths = { labelColumnWidth, fr(1.0f) } }
            , gmpi::drawing::Rect{}
        );
        Label label(labelText);
        return row; // the row stays "open" (current builder) until this is destroyed
    };

    // --- number entry --------------------------------------------------------
    // Shows the model formatted nicely; commits only text that parses as a
    // number AND differs in value, so retyping "1.50" over "1.5" is a no-op and
    // "abc" is thrown away with the old value restored.
    {
        auto row = beginRow("Amount");

        TextEdit editor(
              amountText_
            , [this](const std::string& val)
            {
                double v{};
                if (!parseDouble(val, v))
                {
                    refreshStates(); // put the previous value back on screen
                    return;
                }

                if (v != model().amount)
                {
                    auto next = model();
                    next.amount = v;
                    commit(std::move(next));
                }
                else
                {
                    refreshStates(); // same value, maybe different spelling: re-show the canonical one
                }
            }
        );
    }

    // --- text entry ----------------------------------------------------------
    {
        auto row = beginRow("Name");

        TextEdit editor(
              nameText_
            , [this](const std::string& val)
            {
                const auto newName = trimmed(val);
                if (newName != model().name)
                {
                    auto next = model();
                    next.name = newName;
                    commit(std::move(next));
                }
                else
                {
                    refreshStates();
                }
            }
        );
    }

    // --- tick box ------------------------------------------------------------
    // Left-aligned at the value column's edge: a nested grid holds the square
    // cell plus an empty filler that absorbs the rest of the column. (The grid
    // maps one column track per child, so the filler is a real child.)
    {
        auto row = beginRow("Enabled");

        auto tickBox = std::make_unique<gmpi::ui::builder::TickBox>(enabledState_);
        tickBox->validateAndSave = [this](bool newValue)
        {
            if (newValue != model().enabled)
            {
                auto next = model();
                next.enabled = newValue;
                commit(std::move(next));
            }
        };

        Grid boolCell(
              { .gap = kLineSpacing, .auto_rows = kRowHeight, .auto_flow = eAutoFlow::columns, .column_widths = { kRowHeight, fr(1.0f) } }
            , {}
        );
        gmpi::ui::builder::ThreadLocalCurrentBuilder->push_back(std::move(tickBox));
        Label filler("");
    }

    // --- summary -------------------------------------------------------------
    // What the model currently holds, so an edit's effect is visible even when
    // the editor re-shows exactly what was typed. Heading-sized: a label's
    // text scales with its row, and at editor height this line would not fit.
    Spacer spacer({ 0, 0, 0, kHeadingHeight });
    const auto& m = model();
    Label summary(
          std::format("amount = {}   name = \"{}\"   enabled = {}   ({} edits kept)",
                      niceDoubleToString(m.amount), m.name, m.enabled ? "true" : "false",
                      history_.size() - 1)
        , { 0, 0, 0, kHeadingHeight }
    );
}
