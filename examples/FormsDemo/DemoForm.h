#pragma once

// A small properties page, drawn with gmpi_ui's widget set.
//
// Modelled on SynthEdit's PropertiesBrowser (SynthEditLib/EditorLib/
// PropertiesBrowser.cpp), which is where these conventions come from:
//
//   * A two-column grid of rows - label on the left, editor on the right - laid
//     out by gmpi::ui::Grid rather than by hand-placed rectangles.
//
//   * ONE-WAY bindings. Each widget reads a gmpi_forms::State that the form
//     owns (model -> UI). Edits do NOT write that State back; they arrive
//     through the widget's `validateAndSave` back-channel, where the form
//     validates them, updates the real model, and only then refreshes the
//     State. That is what lets a number entry reject "abc" without the junk
//     sticking, and lets "0" -> "0.0" count as no change at all.
//
//   * A rebuild-on-dirty render: Body() is re-run whenever the model, the
//     bounds or the theme change, so the summary line at the bottom is simply
//     re-created with fresh text rather than kept in sync piecemeal.

#include <string>

#include "GmpiUiDrawing.h"
#include "experimental/forms.h"
#include "experimental/theme.h"

class DemoForm : public gmpi::ui::Form
{
public:
    DemoForm();
    ~DemoForm();

    void Body() override;

    // Form's base arrange/render neither lay out nor clear - a Form subclass
    // owns that. Without these the page has zero bounds and paints black.
    gmpi::ReturnCode measure(const gmpi::drawing::Size*, gmpi::drawing::Size*) override
    { return gmpi::ReturnCode::Ok; }
    gmpi::ReturnCode arrange(const gmpi::drawing::Rect* finalRect) override;
    gmpi::ReturnCode render(gmpi::drawing::api::IDeviceContext* dc) override;
    gmpi::ReturnCode getClipArea(gmpi::drawing::Rect* returnRect) override
    {
        *returnRect = bounds;
        return gmpi::ReturnCode::Ok;
    }

private:
    // The model: what the page is FOR. Plain values, nothing observable.
    double      amount_  = 1.5;
    std::string name_    = "Untitled";
    bool        enabled_ = true;

    // The widget-facing States (model -> UI). Re-seeded from the model by
    // refreshStates(); never written by the widgets themselves.
    gmpi_forms::State<std::string> amountText_;
    gmpi_forms::State<std::string> nameText_;
    gmpi_forms::State<bool>        enabledState_;

    void refreshStates();
    void modelChanged();

    gmpi::drawing::Rect bounds{};
    bool formIsDirty_ = true;
    gmpi::ui::ThemeMode lastRenderedTheme_ = gmpi::ui::ThemeMode::Dark;
};
