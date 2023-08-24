#pragma once

#include "Drawing.h"

struct lineStyle
{
	gmpi::drawing::CapStyle capStyle;
	gmpi::drawing::Color color;
	gmpi::drawing::DashStyle dashStyle;
};

struct triangleStyle
{
	gmpi::drawing::Color color;
	gmpi::drawing::LineJoin lineJoin;
};

void drawLinesDemo(gmpi::drawing::Graphics& g, gmpi::drawing::SizeL size)
{
	g.clear(gmpi::drawing::colorFromHex(0x323E44u));

	const float margin = size.height / 24.0f;

	// lines
	const lineStyle lineStyles[] = {
		{gmpi::drawing::CapStyle::Flat  , gmpi::drawing::Colors::Salmon, gmpi::drawing::DashStyle::Solid},
		{gmpi::drawing::CapStyle::Round , gmpi::drawing::Colors::DarkOrange, gmpi::drawing::DashStyle::Solid},
		{gmpi::drawing::CapStyle::Square, gmpi::drawing::Colors::Gold, gmpi::drawing::DashStyle::Solid},
		{gmpi::drawing::CapStyle::Square, gmpi::drawing::Colors::LightSeaGreen, gmpi::drawing::DashStyle::Dash},
		{gmpi::drawing::CapStyle::Round , gmpi::drawing::Colors::DodgerBlue, gmpi::drawing::DashStyle::Dot}
	};

	float y = margin;

	auto brush1 = g.createSolidColorBrush(gmpi::drawing::Colors::Green);
	auto brush2 = g.createSolidColorBrush(gmpi::drawing::Colors::White);

	float x1 = margin;
	float x2 = 98.0f;

	for (const auto& style : lineStyles)
	{
		gmpi::drawing::StrokeStyleProperties strokeStyleProperties{};
		strokeStyleProperties.lineCap = style.capStyle;
		//strokeStyleProperties.lineJoin = gmpi::drawing::LineJoin::Miter;
		//strokeStyleProperties.miterLimit = 10.0f;
		strokeStyleProperties.dashStyle = style.dashStyle;

		auto strokeStyle = g.getFactory().createStrokeStyle(strokeStyleProperties);
		brush1.setColor(style.color);

		g.drawLine({ x1, y }, { x2, y }, brush1, 6.0f, strokeStyle);
//		g.drawLine({ x1, y }, { x2, y }, brush2, 1.0f, strokeStyle);

		y += margin;
	}

	// triangles (line joins)
	const triangleStyle linejoins[] = {
		{gmpi::drawing::Colors::Firebrick    , gmpi::drawing::LineJoin::Bevel},
		{gmpi::drawing::Colors::Coral        , gmpi::drawing::LineJoin::Miter},
		{gmpi::drawing::Colors::Aquamarine   , gmpi::drawing::LineJoin::Round}
	};

	x1 = 120.0f;
	y = 50.f;
	float side = 30.f;

	for (const auto& style : linejoins)
	{
		gmpi::drawing::StrokeStyleProperties strokeStyleProperties{};
		strokeStyleProperties.lineJoin = style.lineJoin;

		auto strokeStyle = g.getFactory().createStrokeStyle(strokeStyleProperties);

		auto geometry = g.getFactory().createPathGeometry();
		auto sink = geometry.open();
		sink.beginFigure({ x1, y });
		sink.addLine({ x1 + side, y });
		sink.addLine({ x1 + side * 0.5f, y - side * 0.866f });
		sink.endFigure(gmpi::drawing::FigureEnd::Closed);
		sink.close();

		brush1.setColor(style.color);
		g.drawGeometry(geometry, brush1, 6.0f, strokeStyle);

		x1 += side * 1.5f;
	}


	// curves
	x1 += side * 1.0f;
	side = 80.f;
	{
		brush1.setColor(gmpi::drawing::Colors::GreenYellow);
		for (float dx = -side; dx < side; dx += 10.f)
		{
			auto geometry = g.getFactory().createPathGeometry();
			auto sink = geometry.open();
			sink.beginFigure({ x1, y });
			sink.addQuadraticBezier({ { x1 + dx, y - side }, { x1 + side, y } });
			sink.addQuadraticBezier({ { x1 - dx, y + side }, { x1, y } });
			sink.endFigure(gmpi::drawing::FigureEnd::Closed);
			sink.close();

			g.drawGeometry(geometry, brush1, 1.0f);
		}
	}
}