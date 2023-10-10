#pragma once

#include "Drawing.h"

struct fontOptions
{
	std::string_view name;
	gmpi::drawing::FontWeight weight = gmpi::drawing::FontWeight::Normal;
	gmpi::drawing::FontStyle style = gmpi::drawing::FontStyle::Normal;
	gmpi::drawing::FontStretch stretch = gmpi::drawing::FontStretch::Normal;
};

struct fontAlignments
{
	std::string_view name;
	gmpi::drawing::TextAlignment alignment = gmpi::drawing::TextAlignment::Leading;
	gmpi::drawing::ParagraphAlignment paragraphAlignment = gmpi::drawing::ParagraphAlignment::Near;
};

void drawTextDemo(gmpi::drawing::Graphics& g, gmpi::drawing::SizeL size)
{
	g.clear(gmpi::drawing::colorFromHex(0x323E44u));

	// draw some large text
	fontOptions options[] =
	{
		{"Regular"   , gmpi::drawing::FontWeight::Normal, gmpi::drawing::FontStyle::Normal, gmpi::drawing::FontStretch::Normal},
		{"Bold"      , gmpi::drawing::FontWeight::Bold  , gmpi::drawing::FontStyle::Normal, gmpi::drawing::FontStretch::Normal},
//		{"Light"     , gmpi::drawing::FontWeight::Thin , gmpi::drawing::FontStyle::Normal, gmpi::drawing::FontStretch::Normal},
		{"Italic"    , gmpi::drawing::FontWeight::Normal, gmpi::drawing::FontStyle::Italic, gmpi::drawing::FontStretch::Normal},
		{"Condensed" , gmpi::drawing::FontWeight::Normal, gmpi::drawing::FontStyle::Normal, gmpi::drawing::FontStretch::Condensed},
	};


	const float textheight = size.height / 12.0f;
	const float margin = textheight / 2.0f;

    gmpi::drawing::Rect textRect{margin, margin, margin, margin + textheight};

	auto brush = g.createSolidColorBrush(gmpi::drawing::Colors::WhiteSmoke);

	for (auto& option : options)
	{
		gmpi::drawing::Factory::FontStack stack{ std::vector<std::string>{"Segoe"} };

		auto font = g.getFactory().createTextFormat2(textheight, stack, option.weight, option.style, option.stretch);

		const auto textSize = font.getTextExtentU(option.name);
		textRect.right = textRect.left + textSize.width + 1;

		g.drawTextU(option.name, font, textRect, brush);

		textRect = offsetRect(textRect, { margin + getWidth(textRect), 0 });
	}

	// draw a paragraph of text
	{
		const auto text = "One spring morning at four o'clock the first cuckoo arrived in the valley of the Moomins. He perched on the blue roof of Momin house and cuckooed 8 times - rather hoarsely to be sure, for it was still a bit early in the spring.\n   Then he flew away to the east.\n   Moomintroll woke up and lay a long time looking at the ceiling before he realised where he was. He had slept a hundred nights and a hundred days, and his dreams still thronged about his head trying to coax him back to sleep.";

		auto font = g.getFactory().createTextFormat2(14);

		textRect.left = margin;
		textRect.right = size.width / 2.0f;
		textRect.top = textRect.bottom + margin;
		textRect.bottom = size.height - margin;

		g.drawTextU(text, font, textRect, brush);
	}

	// color emoji
	{
		const auto text = (const char*) u8"Color emoji: 絵文字 🦑 😀"; // note: on Windows you need to save your source code as "UTF8 with signature"

		auto font = g.getFactory().createTextFormat2(14);
		const auto textSize = font.getTextExtentU(text);

		textRect.left = textRect.right + margin;
		textRect.right = size.width - margin;
		g.drawTextU(text, font, textRect, brush);

		textRect.top = textRect.top + textSize.height + margin;
	}

	// Alignment
	{
		fontAlignments alignments[] = {
			{ "Left", gmpi::drawing::TextAlignment::Leading, gmpi::drawing::ParagraphAlignment::Center },
			{ "Center", gmpi::drawing::TextAlignment::Center, gmpi::drawing::ParagraphAlignment::Center },
			{ "Right", gmpi::drawing::TextAlignment::Trailing, gmpi::drawing::ParagraphAlignment::Center },
			{ "Top", gmpi::drawing::TextAlignment::Center, gmpi::drawing::ParagraphAlignment::Near },
			{ "Center", gmpi::drawing::TextAlignment::Center, gmpi::drawing::ParagraphAlignment::Center },
			{ "Bottom", gmpi::drawing::TextAlignment::Center, gmpi::drawing::ParagraphAlignment::Far },
			{ "Baseline", gmpi::drawing::TextAlignment::Center, gmpi::drawing::ParagraphAlignment::Far },
		};

		const float boxHeight = 28.0f;
		auto font = g.getFactory().createTextFormat2(14);

        gmpi::drawing::Rect alignmentRect{textRect.left, textRect.top, textRect.right, textRect.top + boxHeight};
		auto boxBrush = g.createSolidColorBrush(gmpi::drawing::Colors::Blue);

		for (auto& box : alignments)
		{
			g.drawRectangle(alignmentRect, boxBrush);

			font.setTextAlignment(box.alignment);
			font.setParagraphAlignment(box.paragraphAlignment);

			if (box.name == "Baseline")
			{
				const auto textSize = font.getTextExtentU(box.name);
				const auto metrics = font.getFontMetrics();

				auto r = alignmentRect;
				r.top = r.bottom - calcBodyHeight(metrics);
				r = offsetRect(r, { 0, metrics.descent });
				g.drawTextU(box.name, font, r, brush);
			}
			else
			{
				g.drawTextU(box.name, font, alignmentRect, brush);
			}

			alignmentRect = offsetRect(alignmentRect, { 0, boxHeight + 2.0f });
		}
	}
}
