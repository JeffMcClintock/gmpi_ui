#include "ui_facades.h"

namespace gmpi::ui
{

Grid::Grid(
	  gmpi::ui::builder::ViewParent::Initializer init
	, gmpi::drawing::Rect pbounds
)
{
	saveParent = gmpi::ui::builder::ThreadLocalCurrentBuilder;

	auto portal = std::make_unique<builder::Grid>(init, pbounds);

	gmpi::ui::builder::ThreadLocalCurrentBuilder = portal.get();

	saveParent->push_back(std::move(portal));
}

Grid::Grid(
	gmpi::ui::builder::ViewParent::Initializer init
)
{
	saveParent = gmpi::ui::builder::ThreadLocalCurrentBuilder;

	auto portal = std::make_unique<builder::Grid>(init, gmpi::drawing::Rect{});

	gmpi::ui::builder::ThreadLocalCurrentBuilder = portal.get();

	saveParent->push_back(std::move(portal));
}

Grid::~Grid()
{
	gmpi::ui::builder::ThreadLocalCurrentBuilder = saveParent;
}

Rectangle::Rectangle(
	gmpi::drawing::Rect bounds,
	gmpi::drawing::Color fillColor,
	gmpi::drawing::Color strokeColor
)
{
	auto& result = *gmpi::ui::builder::ThreadLocalCurrentBuilder;
	auto rectView = std::make_unique<builder::RectangleView>(bounds, fillColor, strokeColor);
	view = rectView.get();
	result.push_back(std::move(rectView));
}

} // namespace gmpi::ui
