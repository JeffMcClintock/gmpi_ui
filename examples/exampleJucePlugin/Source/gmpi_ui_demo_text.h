#pragma once

#include "Drawing.h"


void drawTextDemo(gmpi::drawing::Graphics& g, gmpi::drawing::SizeL size)
{

	g.clear(gmpi::drawing::Colors::Bisque);

	auto font = g.getFactory().createTextFormat2();

	auto brush = g.createSolidColorBrush(gmpi::drawing::Colors::White);

	g.drawTextU("Hello World", font, gmpi::drawing::Rect(0, 0, size.width, size.height), brush);
}