#pragma once
#include "builders.h"

namespace gmpi::ui::builder
{

struct Grid : public View, public ViewParent
{
	gmpi::drawing::Rect bounds{};
	bool boundsSetByParent = false; // true when parent layout assigns item box

	Grid(
		gmpi::ui::builder::ViewParent::Initializer init
		, gmpi::drawing::Rect pbounds = {}
	) : ViewParent(init), bounds(pbounds)
	{
		asView = this;
	}

	void doLayout() override;

	bool RenderIfDirty(
		gmpi_forms::Environment* env,
		gmpi::forms::primitive::IVisualParent& owner,
		gmpi::forms::primitive::IMouseParent& mouseParent
	) const override;

	gmpi::drawing::Rect getBounds() const override
	{
		return bounds;
	}
	void setBounds(gmpi::drawing::Rect newBounds) override
	{
		bounds = newBounds;
		boundsSetByParent = true;
	}

	float getDesiredWidth() const override;
	float getDesiredHeight() const override;
};

} // namespace gmpi::ui::builder
