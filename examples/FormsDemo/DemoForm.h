#pragma once

// A small properties page, drawn with gmpi_ui's widget set.
//
// Modelled on SynthEdit's PropertiesBrowser (SynthEditLib/EditorLib/
// PropertiesBrowser.cpp), which is where these conventions come from:
//
//   * A Page of rows - label on the left, editor on the right - rather than
//     hand-placed rectangles.
//
//   * ONE-WAY editors. Each is given the model's value and a function that
//     receives the user's edit; the form validates it and commits a new model.
//     The editor keeps showing the model's value, which is what lets a number
//     entry reject "abc" without the junk sticking, and lets "0" -> "0.0"
//     count as no change at all.
//
//   * Rebuild on change: Body() re-runs whenever the model, the bounds or the
//     theme change, so the summary line at the bottom is
//     simply re-created with fresh text rather than kept in sync piecemeal.
//
//   * An IMMUTABLE model with undo/redo. Every committed edit produces a new
//     Model value and appends it to an immer::vector - a persistent structure,
//     so each version is a cheap, independent snapshot. A cursor into that
//     vector says which version is current: undo and redo just move it, and a
//     commit made after an undo drops the versions beyond the cursor (the redo
//     tail), as every editor does. No command objects, no inverse operations:
//     the history IS the undo stack.

#include <string>
#include <utility>

#include "GmpiUiDrawing.h"
#include "experimental/forms.h"
#include "experimental/theme.h"
#include "History.h"

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

private:
    History<Model> history_;

    const Model& model() const { return history_.current(); }

    // Commit a copy of the model with 'change' made to it, and rebuild the page.
    template <class Change>
    void edit(Change change)
    {
        auto next = model();
        change(next);
        history_.commit(std::move(next));
        invalidateView();
    }

    void undo() { if (history_.undo()) invalidateView(); }
    void redo() { if (history_.redo()) invalidateView(); }
};
