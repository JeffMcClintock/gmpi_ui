#include "grid.h"

namespace gmpi::ui::builder
{

void Grid::doLayout()
{
	const auto childCount = static_cast<float>(childViews.size());
	const bool autoFlowRows = spec.auto_flow == eAutoFlow::rows;

	// Resolve explicit track sizes (column_widths for columns, column_heights for rows).
	// Positive values = fixed px, negative values = fractional units (-1 = 1fr, -2 = 2fr).
	const auto& trackSizes = autoFlowRows ? spec.column_heights : spec.column_widths;
	const bool hasExplicitTracks = !trackSizes.empty();

	std::vector<float> resolvedSizes;

	if (hasExplicitTracks)
	{
		const float totalExtent = autoFlowRows ? getHeight(bounds) : getWidth(bounds);
		const float totalGaps = spec.gap * (std::max(0.f, childCount - 1));

		// Sum fixed sizes and total fr units
		float fixedTotal = 0.0f;
		float frTotal = 0.0f;
		for (size_t i = 0; i < childViews.size(); ++i)
		{
			const float trackSize = i < trackSizes.size() ? trackSizes[i] : -1.0f; // default 1fr
			if (trackSize >= 0.0f)
				fixedTotal += trackSize;
			else
				frTotal += -trackSize;
		}

		const float frSpace = std::max(0.0f, totalExtent - fixedTotal - totalGaps);
		const float perFr = frTotal > 0.0f ? frSpace / frTotal : 0.0f;

		for (size_t i = 0; i < childViews.size(); ++i)
		{
			const float trackSize = i < trackSizes.size() ? trackSizes[i] : -1.0f;
			resolvedSizes.push_back(trackSize >= 0.0f ? trackSize : (-trackSize * perFr));
		}
	}

	const bool size_to_children = !hasExplicitTracks && (autoFlowRows ? spec.auto_rows > 0.0f : spec.auto_columns > 0.0f);

	const auto uniformExtent = hasExplicitTracks ? 0.0f :
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

	for (size_t i = 0; i < childViews.size(); ++i)
	{
		const float itemExtent = hasExplicitTracks ? resolvedSizes[i] : uniformExtent;

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

		childViews[i]->setBounds(childRect);
		childViews[i]->doLayout();

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
