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

	const float margin = size.height / 20.0f;

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


	// bezier curves
	x1 += side * 1.0f;
	side = 70.f;
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

	// shapes and fills
	{
		x1 = margin;
		float y1 = 90.0f;
		float y2 = 150.0f;
		float width = 80.0f;

		// create some common resources
		auto solidColorBrush = g.createSolidColorBrush(gmpi::drawing::Colors::LightSeaGreen);

		const gmpi::drawing::Gradientstop gradientStopsLinear[] = {
			{ 0.0f, gmpi::drawing::Colors::White},
			{ 1.0f, gmpi::drawing::Colors::Peru}
		};

		const gmpi::drawing::Gradientstop gradientStopsRadial[] = {
			{ 0.0f, gmpi::drawing::Colors::White},
			{ 1.0f, gmpi::drawing::Colors::LightSlateGray}
		};

		auto gradientStopCollectionLinear = g.createGradientstopCollection(gradientStopsLinear);
		auto gradientStopCollectionRadial = g.createGradientstopCollection(gradientStopsRadial);

		// create a pattern brush
		auto tartan = [](int x, int y) -> uint32_t
		{
			uint32_t col{ gmpi::drawing::rgBytesToPixel(0x42,0x73,0x9e) }; //  0xFF42739Eu }; // Blue

			int index = (x & 1) ^ (y & 1) ? x : y;

			if ((index >> 3) % 2 == 0)
			{
				col = gmpi::drawing::rgBytesToPixel(0, 0, 0); // 0xFF000000u; // Black
			}
			else
			{
				if ((index >> 1) % 4 == 2)
				{
					col = gmpi::drawing::rgBytesToPixel(0xff, 0xff, 0xff); // 0xFFffffffu; // White
				}
			}

			return col;
		};

		const uint32_t sz = 128;
		auto bitmap = g.getFactory().createImage({ sz, sz });
		{
			auto pixels = bitmap.lockPixels((int32_t)gmpi::drawing::BitmapLockFlags::Write); // TODO no cast
			for (uint32_t py = 0; py < sz; ++py)
			{
				for (uint32_t px = 0; px < sz; ++px)
				{
					pixels.setPixel(px, py, tartan(px, py)); // ARGB
				}
			}
		}
		auto bitmapBrush = g.createBitmapBrush(bitmap);

		// solid fill
		{
			g.fillRectangle({ x1, y1, x1 + width, y2 }, solidColorBrush);
		}

		x1 += width + margin;

		// linear gradient fill
        {
            gmpi::drawing::Point grad1{ 0.f, y1 };
            gmpi::drawing::Point grad2{ 0.f, y2 };
            
            gmpi::drawing::LinearGradientBrushProperties lgbp1{grad1, grad2};
			auto gradientBrush = g.createLinearGradientBrush(lgbp1, {}, gradientStopCollectionLinear);

			g.fillRoundedRectangle({ { x1, y1, x1 + width, y2 }, margin, margin }, gradientBrush);
		}

		x1 += width + margin;

		// radial gradient fill
		{
			gmpi::drawing::Point gradientCenter{ x1 + width * 0.25f, y1 + (y2 - y1) * 0.25f };

			auto gradientBrush = g.createRadialGradientBrush(gradientStopCollectionRadial, gradientCenter, width);
			g.fillRoundedRectangle({ { x1, y1, x1 + width, y2 }, margin * 2.0f, margin * 2.0f }, gradientBrush);
		}

		x1 += width + margin;
       
		// pattern fill
		{
			const auto radius = (y2 - y1) * 0.5f;
			const gmpi::drawing::Point center{ x1 + radius, y1 + radius };
			g.fillCircle(center, radius, bitmapBrush);
		}

		x1 = margin;
		y1 = 170.0f;
		y2 = 230.0f;

		// outlines

		// solid outline
		{
			const float strokeWidth = 4.0f;
			g.drawRectangle({ x1, y1, x1 + width, y2 }, solidColorBrush, strokeWidth);
		}
		x1 += width + margin;

		// linear gradient fill
        {
            gmpi::drawing::Point grad1{ 0.f, y1 };
            gmpi::drawing::Point grad2{ 0.f, y2 };
            
            gmpi::drawing::LinearGradientBrushProperties lgbp1{grad1, grad2};
			auto gradientBrush = g.createLinearGradientBrush(lgbp1, {}, gradientStopCollectionLinear);

			const float strokeWidth = 6.0f;
			g.drawRoundedRectangle({ { x1, y1, x1 + width, y2 }, margin, margin }, gradientBrush, strokeWidth);
		}

		x1 += width + margin;

		// radial gradient fill
		{
			gmpi::drawing::Point gradientCenter{ x1 + width * 0.25f, y1 + (y2 - y1) * 0.25f };

			auto gradientBrush = g.createRadialGradientBrush(gradientStopCollectionRadial, gradientCenter, width);

			const float strokeWidth = 8.0f;
			g.drawRoundedRectangle({ { x1, y1, x1 + width, y2 }, margin * 2.0f, margin * 2.0f }, gradientBrush, strokeWidth);
		}

		x1 += width + margin;

		// tiled image fill
		{
			const auto radius = (y2 - y1) * 0.5f;
			const gmpi::drawing::Point center{ x1 + radius, y1 + radius };
			
			const float strokeWidth = 10.0f;
			g.drawCircle(center, radius, bitmapBrush, strokeWidth);
		}

		x1 = margin;
		y1 = 236.0f;
		y2 = 260.0f;

		// Text fills
		{
			gmpi::drawing::Factory::FontStack stack{ std::vector<std::string>{"Segoe"} };
			auto textFormat = g.getFactory().createTextFormat2(y2 - y1, stack);
			textFormat.setTextAlignment(gmpi::drawing::TextAlignment::Center);

			// Solid
			{
				gmpi::drawing::Rect textRect{ x1, y1, x1 + width, y2 };

				g.drawTextU("Solid", textFormat, textRect, solidColorBrush);
			}

			x1 += width + margin;

			// Gradient (linear)
			{
				gmpi::drawing::Rect textRect{ x1, y1, x1 + width, y2 };

				gmpi::drawing::Point grad1{ 0.f, textRect.top };
				gmpi::drawing::Point grad2{ 0.f, textRect.bottom };

				gmpi::drawing::LinearGradientBrushProperties lgbp1{ grad1, grad2 };
				auto gradientBrush = g.createLinearGradientBrush(lgbp1, {}, gradientStopCollectionLinear);

				g.drawTextU("Linear", textFormat, textRect, gradientBrush);
			}

			x1 += width + margin;

			// Gradient (radial)
			{
				gmpi::drawing::Rect textRect{ x1, y1, x1 + width, y2 };

				gmpi::drawing::Point gradientCenter{ x1 + width * 0.25f, y1 + (y2 - y1) * 0.25f };

				auto gradientBrush = g.createRadialGradientBrush(gradientStopCollectionRadial, gradientCenter, width);

				g.drawTextU("Radial", textFormat, textRect, gradientBrush);
			}

			x1 += width + margin;

			// Pattern
			{
				gmpi::drawing::Rect textRect{ x1, y1, x1 + width, y2 };

				g.drawTextU("Pattern", textFormat, textRect, bitmapBrush);
			}
		}

		// alpha composting
		{
			const auto radius = size.height * 0.04f;
			gmpi::drawing::Point p{ size.width * 0.5f, size.height - radius - margin };
#if 0
			//chekerboard
			auto whiteBrush = g.createSolidColorBrush(gmpi::drawing::Colors::White);
			auto blackBrush = g.createSolidColorBrush(gmpi::drawing::Colors::Black);

			bool fill{};
			float squareSize = 12.0f;
			for(float x = p.x - margin * 3.0f; x < p.x + margin * 3.0f; x += squareSize)
			{
				for (float y = p.y - margin * 3.0f; y < size.height - margin; y += squareSize)
				{
					g.fillRectangle({ x, y, x + margin, y + margin }, fill ? whiteBrush : blackBrush);
					fill = !fill;
				}
//				fill = !fill;
			}
#endif
			auto blackBrush = g.createSolidColorBrush(gmpi::drawing::Colors::Black);
			g.fillCircle(gmpi::drawing::Point{ p.x, p.y }, radius * 1.8f, blackBrush);

			auto fillBrushRed   = g.createSolidColorBrush(gmpi::drawing::Color{ 1.0f, 0.0f, 0.0f, 0.5f });
			auto fillBrushGreen = g.createSolidColorBrush(gmpi::drawing::Color{ 0.0f, 1.0f, 0.0f, 0.5f });
			auto fillBrushBlue  = g.createSolidColorBrush(gmpi::drawing::Color{ 0.0f, 0.0f, 1.0f, 0.5f });

			g.fillCircle(gmpi::drawing::Point{ p.x - radius * 0.433f, p.y + radius * 0.25f }, radius, fillBrushRed);
			g.fillCircle(gmpi::drawing::Point{ p.x                , p.y - radius * 0.5f }, radius, fillBrushGreen);
			g.fillCircle(gmpi::drawing::Point{ p.x + radius * 0.433f, p.y + radius * 0.25f }, radius, fillBrushBlue);
		}
	}
}
