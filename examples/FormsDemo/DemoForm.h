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
//
//   * An IMMUTABLE model with undo/redo. Every committed edit produces a new
//     Model value and appends it to an immer::vector - a persistent structure,
//     so each version is a cheap, independent snapshot. A cursor into that
//     vector says which version is current: undo and redo just move it, and a
//     commit made after an undo drops the versions beyond the cursor (the redo
//     tail), as every editor does. No command objects, no inverse operations:
//     the history IS the undo stack.

#include <cstddef>
#include <string>

#include <immer/vector.hpp>

#include "GmpiUiDrawing.h"
#include "experimental/forms.h"
#include "experimental/theme.h"

// What the page is FOR. A plain value: edits make a new one.
struct Model
{
    double      amount  = 1.5;
    std::string name    = "Untitled";
    bool        enabled = true;
};

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
    // Every version of the model, oldest first; never empty. history_[cursor_]
    // is the current one; anything after it is what redo would restore.
    immer::vector<Model> history_{ Model{} };
    std::size_t          cursor_ = 0;

    const Model& model() const { return history_[cursor_]; }

    bool canUndo() const { return cursor_ > 0; }
    bool canRedo() const { return cursor_ + 1 < history_.size(); }
    void undo();
    void redo();

    // The widget-facing States (model -> UI). Re-seeded from the model by
    // refreshStates(); never written by the widgets themselves.
    gmpi_forms::State<std::string> amountText_;
    gmpi_forms::State<std::string> nameText_;
    gmpi_forms::State<bool>        enabledState_;

    void refreshStates();

    // Commit a new version: drop any redo tail, append it, rebuild the page.
    void commit(Model next);

    // Move the cursor (undo/redo both land here) and rebuild the page.
    void showVersion(std::size_t index);

    gmpi::drawing::Rect bounds{};
    bool formIsDirty_ = true;
    gmpi::ui::ThemeMode lastRenderedTheme_ = gmpi::ui::ThemeMode::Dark;
};
