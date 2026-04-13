#pragma once
#include "builders.h"
#include "grid.h"

namespace gmpi::ui
{

class Grid
{
	gmpi::ui::builder::ViewParent* saveParent = {};

public:
	Grid(gmpi::ui::builder::ViewParent::Initializer init);
	Grid(gmpi::ui::builder::ViewParent::Initializer init, gmpi::drawing::Rect pbounds);
	~Grid();
};

struct Rectangle
{
	gmpi::ui::builder::RectangleView* view = {};

	Rectangle(
		gmpi::drawing::Rect bounds,
		gmpi::drawing::Color fillColor = gmpi::drawing::Colors::Black,
		gmpi::drawing::Color strokeColor = gmpi::drawing::Colors::White
	);
};

} // namespace gmpi::ui
