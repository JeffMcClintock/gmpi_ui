#include <cmath>
#include "grid.h"

namespace gmpi::ui::builder
{

void Grid::doLayout()
{
	const auto childCount = static_cast<float>(childViews.size());
	const bool autoFlowRows = spec.auto_flow == eAutoFlow::rows;

	// Resolve explicit track sizes (column_widths for columns, column_heights for rows).
	// Per-track values: positive = fixed px, negative = fractional units (-1 = 1fr, -2 = 2fr),
	// auto_size() (= +inf) = ask the child via getDesiredWidth/Height().
	const auto& trackSizes = autoFlowRows ? spec.column_heights : spec.column_widths;
	const bool hasExplicitTracks = !trackSizes.empty() || spec.default_track_size != -1.0f;

	auto rawTrackSize = [&](size_t i) -> float {
		return i < trackSizes.size() ? trackSizes[i] : spec.default_track_size;
	};
	auto resolveTrackSize = [&](size_t i) -> float {
		float ts = rawTrackSize(i);
		if (std::isinf(ts))
		{
			ts = autoFlowRows ? childViews[i]->getDesiredHeight() : childViews[i]->getDesiredWidth();
		}
		return ts;
	};

	std::vector<float> resolvedSizes;

	if (hasExplicitTracks)
	{
		const float totalExtent = autoFlowRows ? getHeight(bounds) : getWidth(bounds);
		const float totalGaps = spec.gap * (std::max(0.f, childCount - 1));

		// Sum fixed sizes (incl. resolved auto-content) and total fr units
		float fixedTotal = 0.0f;
		float frTotal = 0.0f;
		for (size_t i = 0; i < childViews.size(); ++i)
		{
			const float trackSize = resolveTrackSize(i);
			if (trackSize >= 0.0f)
				fixedTotal += trackSize;
			else
				frTotal += -trackSize;
		}

		const float frSpace = std::max(0.0f, totalExtent - fixedTotal - totalGaps);
		const float perFr = frTotal > 0.0f ? frSpace / frTotal : 0.0f;

		for (size_t i = 0; i < childViews.size(); ++i)
		{
			const float trackSize = resolveTrackSize(i);
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
	// Fast out: nothing below us needs attention.
	if (!dirty && !childDirty)
		return false;

	const auto iwasdirty = dirty;

	if (dirty)
	{
		View::RenderIfDirty(env, parent_visual, mouseParent);

		// Grid itself has no visuals; render children into the parent visual on the same pass,
		// otherwise the next frame's childDirty fast-out would skip them.
		for (auto& view : childViews)
			view->RenderIfDirty(env, parent_visual, mouseParent);
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

	childDirty = false;
	return iwasdirty;
}

// Helper: desired extent along the flow (main) axis.
// Sums explicit/auto-content track sizes plus gaps; fr tracks contribute 0 (they only stretch
// inside a known container). Falls back to auto_rows/auto_columns or constructor bounds.
static float computeMainAxisDesired(
	const ViewParent::Initializer& spec,
	const std::vector<std::unique_ptr<View>>& childViews,
	const gmpi::drawing::Rect& bounds
)
{
	const bool autoFlowRows = spec.auto_flow == ViewParent::eAutoFlow::rows;
	const auto count = childViews.size();
	if (count == 0)
		return 0.0f;

	const float gapTotal = (count > 1) ? (count - 1) * spec.gap : 0.0f;

	const auto& trackSizes = autoFlowRows ? spec.column_heights : spec.column_widths;
	const bool hasExplicitTracks = !trackSizes.empty() || spec.default_track_size != -1.0f;

	if (hasExplicitTracks)
	{
		float total = 0.0f;
		for (size_t i = 0; i < count; ++i)
		{
			float ts = i < trackSizes.size() ? trackSizes[i] : spec.default_track_size;
			if (std::isinf(ts))
				ts = autoFlowRows ? childViews[i]->getDesiredHeight() : childViews[i]->getDesiredWidth();
			else if (ts < 0.0f)
				ts = 0.0f; // fr tracks contribute 0 to natural extent
			total += ts;
		}
		return total + gapTotal;
	}

	const float autoExt = autoFlowRows ? spec.auto_rows : spec.auto_columns;
	if (autoExt > 0.0f)
		return count * autoExt + gapTotal;

	return autoFlowRows ? getHeight(bounds) : getWidth(bounds);
}

float Grid::getDesiredWidth() const
{
	if (spec.auto_flow == eAutoFlow::columns)
		return computeMainAxisDesired(spec, childViews, bounds);

	// row-flow: width is the cross axis
	if (spec.auto_columns > 0.0f)
		return spec.auto_columns;

	float maxW = 0.0f;
	for (auto& c : childViews)
		maxW = std::max(maxW, c->getDesiredWidth());
	return std::max(maxW, getWidth(bounds));
}

float Grid::getDesiredHeight() const
{
	if (spec.auto_flow == eAutoFlow::rows)
		return computeMainAxisDesired(spec, childViews, bounds);

	// column-flow: height is the cross axis
	if (spec.auto_rows > 0.0f)
		return spec.auto_rows;

	float maxH = 0.0f;
	for (auto& c : childViews)
		maxH = std::max(maxH, c->getDesiredHeight());
	return std::max(maxH, getHeight(bounds));
}

} // namespace gmpi::ui::builder
