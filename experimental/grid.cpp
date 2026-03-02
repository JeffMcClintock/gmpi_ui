#include "grid.h"

namespace gmpi::ui::builder
{

void Grid::doLayout()
{
	const auto childCount = static_cast<float>(childViews.size());
	const bool autoFlowRows = spec.auto_flow == eAutoFlow::rows;
	const bool size_to_children = autoFlowRows ? spec.auto_rows > 0.0f : spec.auto_columns > 0.0f;

	const auto itemExtent =
		autoFlowRows ?
		(
			spec.auto_rows > 0.0f ? spec.auto_rows :
			childCount > 0 ? (getHeight(bounds) - spec.gap * (childCount - 1)) / childCount : 0.0f
		) :
		(
			spec.auto_columns > 0.0f ? spec.auto_columns :
			childCount > 0 ? (getWidth(bounds) - spec.gap * (childCount - 1)) / childCount : 0.0f
		);

	auto childRect = bounds;

	for (auto& child : childViews)
	{
		if (autoFlowRows)
		{
			if (spec.auto_columns > 0.0f)
				childRect.right = childRect.left + spec.auto_columns;

			childRect.bottom = childRect.top + itemExtent;
		}
		else
		{
			if (spec.auto_rows > 0.0f)
				childRect.bottom = childRect.top + spec.auto_rows;

			childRect.right = childRect.left + itemExtent;
		}

		child->setBounds(childRect);
		child->doLayout();

		if (autoFlowRows)
			childRect.top = childRect.bottom + spec.gap;
		else
			childRect.left = childRect.right + spec.gap;
	}

	// CSS-like behavior: if this grid is placed by a parent item box, do not
	// auto-resize the grid container from child content.
	if (size_to_children && !boundsSetByParent)
	{
		if (autoFlowRows)
			bounds.bottom = childRect.top - spec.gap;
		else
			bounds.right = childRect.left - spec.gap;
	}
}

bool Grid::RenderIfDirty(
	gmpi_forms::Environment* env,
	gmpi::forms::primitive::IVisualParent& parent_visual,
	gmpi::forms::primitive::IMouseParent& mouseParent
) const
{
	const auto iwasdirty = dirty;

	if (dirty)
	{
		View::RenderIfDirty(env, parent_visual, mouseParent);
	}
	else
	{
		auto mouseportal = dynamic_cast<gmpi::forms::primitive::MousePortal*>(&mouseParent);
		const auto mouseState = mouseportal->saveMouseState();

		bool childWasDirty = false;
		for (auto& view : childViews)
			childWasDirty |= view->RenderIfDirty(env, parent_visual, mouseParent);

		if (childWasDirty)
			mouseportal->restoreMouseState(mouseState);
	}

	return iwasdirty;
}

} // namespace gmpi::ui::builder
