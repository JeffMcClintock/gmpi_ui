#pragma once
#include <span>
#include "./tinyXml2/tinyxml2.h"
#include "GmpiUiDrawing.h"

namespace SvgParser
{
struct pathState
{
	gmpi::drawing::GeometrySink sink;
	gmpi::drawing::Point first;
	gmpi::drawing::Point last;
	bool inFigure{};
	bool filled{};
};

void path_moveTo(pathState& state, gmpi::drawing::Point p)
{
	if (state.inFigure)
	{
		state.sink.endFigure(state.filled ? gmpi::drawing::FigureEnd::Closed : gmpi::drawing::FigureEnd::Open);

		//if (state.strokeSink)
		//	state.sink.EndFigure(FigureEnd::Open);

		//if (state.fillSink)
		//	state.fillSink.EndFigure(FigureEnd::Closed);
	}

	//if (state.strokeSink)
	//	state.sink.BeginFigure(p, FigureBegin::Hollow);

	//if (state.fillSink)
	//	state.fillSink.BeginFigure(p, FigureBegin::Filled);
	state.sink.beginFigure(p, state.filled ? gmpi::drawing::FigureBegin::Filled : gmpi::drawing::FigureBegin::Hollow);

	state.first = state.last = p;
	state.inFigure = true;
}

void path_lineTo(pathState& state, gmpi::drawing::Point p)
{
	state.sink.addLine(p);
	state.last = p;
}

void path_hLine(pathState& state, float x)
{
	state.last.x = x;
	state.sink.addLine(state.last);
}

void path_vLine(pathState& state, float y)
{
	state.last.y = y;
	state.sink.addLine(state.last);
}

void path_quadCurve(pathState& state, std::span<float const> coords)
{
	assert(coords.size() == 4); // (x1 y1 x y)

	const gmpi::drawing::Point pt1{ coords[0], coords[1] };
	const gmpi::drawing::Point pt2{ coords[2], coords[3] };

	state.sink.addQuadraticBezier({ pt1, pt2 });

	state.last = pt2;
}

void path_cubicCurve(pathState& state, std::span<float const> coords)
{
	assert(coords.size() == 6);

	const gmpi::drawing::Point pt1{ coords[0], coords[1] };
	const gmpi::drawing::Point pt2{ coords[2], coords[3] };
	const gmpi::drawing::Point pt3{ coords[4], coords[5] };

	state.sink.addBezier({ pt1, pt2, pt3 });

	state.last = pt3;
}

gmpi::drawing::Matrix3x2 parseTransform(const char* transformS)
{
	// transform="matrix(4.16667,0,0,4.16667,0,0)"

	auto openBracket = strchr(transformS, '(');
	auto closeBracket = strrchr(transformS, ')');

	if (!openBracket || !closeBracket || openBracket > closeBracket)
		return {}; // identity matrix

	std::vector<float> args;

#if 0 // Apple CLANG 14 is not up to date with std::views nor std::from_chars.
	for (const auto& word : std::views::split(std::string_view(openBracket + 1, closeBracket - openBracket - 1), ','))
	{
		float val;
		std::from_chars(word.data(), word.data() + word.size(), val);
		args.push_back(val);
	}
#else
	{
		auto start = openBracket;

		while (start != closeBracket) {
			const auto finish = std::find(++start, closeBracket, ',');
			args.push_back((float)strtod(start, nullptr));

			start = finish;
		}
	}
#endif

	if (strncmp(transformS, "matrix", 6) == 0)
	{
		if (args.size() == 6)
			return gmpi::drawing::Matrix3x2{ args[0], args[1], args[2], args[3], args[4], args[5] };
	}
	else if (strncmp(transformS, "translate", 9) == 0)
	{
		if (args.size() == 2)
			return gmpi::drawing::makeTranslation(args[0], args[1]);
	}
	else if (strncmp(transformS, "scale", 5) == 0)
	{
		if (args.size() == 2)
			return gmpi::drawing::makeScale(args[0], args[1]);
	}
	else if (strncmp(transformS, "rotate", 6) == 0)
	{
		if (args.size() == 1)
			return gmpi::drawing::makeRotation(args[0]);
	}
	else if (strncmp(transformS, "skewX", 5) == 0)
	{
		if (args.size() == 1)
			return gmpi::drawing::makeSkew(args[0], 0);
	}
	else if (strncmp(transformS, "skewY", 5) == 0)
	{
		if (args.size() == 1)
			return gmpi::drawing::makeSkew(0, args[0]);
	}

	return {}; // identity matrix
}

void parseGroup(gmpi::drawing::api::IGeometrySink * sink, tinyxml2::XMLElement* groupE, gmpi::drawing::Matrix3x2 transform)
{
	//auto fillBrush = g.CreateSolidColorBrush(Color::Black);
	//auto strokeBrush = g.CreateSolidColorBrush(Color::Black);

	const auto transformS = groupE->Attribute("transform");
//	Matrix3x2 originalTransform;
	if (transformS)
	{
//		originalTransform = g.GetTransform();
		transform = parseTransform(transformS) * transform;
//		g.SetTransform(transform);
	}

	for (auto node = groupE->FirstChildElement(); node; node = node->NextSiblingElement())
	{
		auto c = node->ToElement();

		if (!c)
			continue;

		auto fillMode = gmpi::drawing::FillMode::Winding;
		auto fillRule(c->Attribute("fill-rule"));
		if (fillRule && strcmp(fillRule, "evenodd") == 0)
		{
			fillMode = gmpi::drawing::FillMode::Alternate;
		}

#if 0
		Color fillColor;
		Color strokeColor;
		fillColor.a = strokeColor.a = 0.0f; // default to not drawn

		auto fill(c->Attribute("fill"));
		if (fill && fill[0] == '#')
		{
			std::string fillstr(fill + 1);
			fillColor = colorFromHexString(fillstr);
		}
		else
		{
			// has a fill, but can't be bothered looking up the color name.
			fillColor = gmpi::drawing::Colors::Black;
		}

		auto stroke(c->Attribute("stroke"));
		if (stroke && stroke[0] == '#')
		{
			std::string strokestr(stroke + 1);
			strokeColor = colorFromHexString(strokestr);
		}
		else
		{
			// has a fill, but can't be bothered looking up the color name.
			fillColor = gmpi::drawing::Colors::Black;
		}

		if (pinFillOverride.rawSize() == sizeof(Color))
		{
			memcpy(&fillColor, pinFillOverride.rawData(), sizeof(Color)); // endian matter?
			fill = ""; // just to indicate that it's not null
		}

		if (pinStrokeOverride.rawSize() == sizeof(Color))
		{
			memcpy(&strokeColor, pinStrokeOverride.rawData(), sizeof(Color)); // endian matter?
			stroke = ""; // just to indicate that it's not null
		}

		if (!stroke && !fill)
		{
			fillColor = gmpi::drawing::Colors::Black;
		}
#endif

		//fillBrush.SetColor(fillColor);
		//strokeBrush.SetColor(strokeColor);

		if (strcmp(c->Name(), "g") == 0) // group
		{
			parseGroup(sink, c, transform);
		}

		if (strcmp(c->Name(), "rect") == 0)
		{
			gmpi::drawing::RoundedRect r{};

			r.rect.left = c->FloatAttribute("x");
			r.rect.top = c->FloatAttribute("y");
			r.rect.right = r.rect.left + c->FloatAttribute("width");
			r.rect.bottom = r.rect.top + c->FloatAttribute("height");

			r.radiusX = c->FloatAttribute("rx");
			r.radiusY = c->FloatAttribute("ry", r.radiusX);

#if 0 // TODO!!!
			if (fillColor.a > 0)
			{
				g.FillRoundedRectangle(r, fillBrush);
			}

			if (strokeColor.a > 0)
			{
				g.DrawRoundedRectangle(r, strokeBrush);
			}
#endif
		}
		else if (strcmp(c->Name(), "path") == 0)
		{
			// https://www.w3.org/TR/SVGTiny12/paths.html#PathDataCurveCommands
			auto d = c->Attribute("d");
			if (!d)
				break;

			// split the string at spaces and non-numbers into strokes
			struct strokeToken
			{
				char strokeType;
				std::vector<float> args;
			};

			std::vector<strokeToken> tokens;
			{
				for (auto p = d; *p; ++p)
				{
					if (isalpha(*p))
					{
						tokens.push_back({ *p });
					}
					else if (isdigit(*p) || *p == '-' || *p == '.')
					{
						if (!tokens.empty())
						{
							auto& token = tokens.back();
							char* pEnd{};
							token.args.push_back(strtod(p, &pEnd));
							p = pEnd - 1;
						}
					}
				}
			}

			// attach the sink to pathState::sink.
			pathState state;
			*gmpi::drawing::AccessPtr::put(state.sink) = sink;
			sink->addRef();

			// render the strokes

#if 0 // TODO!!!
			PathGeometry fillPath;
			if (fillColor.a > 0)
			{
				fillPath = g.GetFactory().CreatePathGeometry();
				state.fillSink = fillPath.Open();
				state.fillSink.SetFillMode(fillMode);
			}
			PathGeometry strokePath;
			if (strokeColor.a > 0)
			{
				strokePath = g.GetFactory().CreatePathGeometry();
				state.strokeSink = strokePath.Open();
			}
#endif
#if 0
			_RPT0(0, "\nd=\"");
			for (auto& t : tokens)
			{
				_RPTN(0, "%c ", t.strokeType);
				for (size_t i = 0; i < t.args.size(); ++i)
				{
					if ((i & 1) == 0 && i > 0)
						_RPTN(0, "%g,", t.args[i]);
					else
						_RPTN(0, "%g ", t.args[i]);
				}
			}
			_RPT0(0, "\n");
#endif
			for (auto& t : tokens)
			{
				switch (t.strokeType)
				{
				case 'M': // Move-to
					if (t.args.size() >= 2)
						path_moveTo(state, { t.args[0], t.args[1] });

					for (size_t i = 1; i < t.args.size() / 2; i++)
						path_lineTo(state, { t.args[2 * i], t.args[2 * i + 1] });
					break;

				case 'm': // move (relative)
					if (t.args.size() >= 2)
						path_moveTo(state, { state.last.x + t.args[0], state.last.y + t.args[1] });

					for (size_t i = 1; i < t.args.size() / 2; i++)
						path_lineTo(state, { state.last.x + t.args[2 * i], state.last.y + t.args[2 * i + 1] });
					break;

				case 'L': // Line-to
					for (size_t i = 0; i < t.args.size() / 2; i++)
						path_lineTo(state, { t.args[2 * i], t.args[2 * i + 1] });
					break;

				case 'l': // Line-to (relative)
					for (size_t i = 0; i < t.args.size() / 2; i++)
						path_lineTo(state, { state.last.x + t.args[2 * i], state.last.y + t.args[2 * i + 1] });
					break;

				case 'H': // Horizontal Line
					for (auto x : t.args)
						path_hLine(state, x);
					break;

				case 'h': // Horizontal Line
					for (auto x : t.args)
						path_hLine(state, state.last.x + x);
					break;

				case 'V': // Vertical Line
					for (auto y : t.args)
						path_vLine(state, y);
					break;

				case 'v': // Vertical Line
					for (auto y : t.args)
						path_vLine(state, state.last.y + y);
					//if(!t.args.empty())
					//	path_vLine(state, state.last.y + t.args.back());
					break;

				case 'Q': // Quadratic Bezier
					for (size_t i = 0; i < t.args.size() / 4; i++)
						path_quadCurve(state, std::span{ t.args }.subspan(4 * i, 4));
					break;

				case 'q': // Quadratic Bezier (relative)
					for (size_t i = 0; i < t.args.size() / 4; i++)
					{
						const float relativeArgs[4]
						{
							state.last.x + t.args[4 * i], state.last.y + t.args[4 * i + 1],
							state.last.x + t.args[4 * i + 2], state.last.y + t.args[4 * i + 3]
						};

						path_quadCurve(state, std::span{ relativeArgs });
					}
					break;

				case 'C': // Cubic Bezier
					for (size_t i = 0; i < t.args.size() / 6; i++)
						path_cubicCurve(state, std::span{ t.args }.subspan(6 * i, 6));
					break;

				case 'c': // Cubic Bezier (relative)
				{
					for (size_t i = 0; i < t.args.size() / 6; i++)
					{
						const float relativeArgs[6]
						{
							state.last.x + t.args[6 * i], state.last.y + t.args[6 * i + 1],
							state.last.x + t.args[6 * i + 2], state.last.y + t.args[6 * i + 3],
							state.last.x + t.args[6 * i + 4], state.last.y + t.args[6 * i + 5]
						};

						path_cubicCurve(state, std::span{ relativeArgs });
					}
				}
				break;


				/* todo
			case 'A': // Arc
			{
				if(t.args.size() != 7)
					break;

				Point pt1{t.args[0], t.args[1]};
				Point pt2{t.args[2], t.args[3]};
				float radius = t.args[4];
				bool largeArc = t.args[5] != 0;
				bool sweep = t.args[6] != 0;

				sink.AddArc({ pt1, pt2 }, radius, largeArc, sweep);
			}
				break;
			*/


				case 'Z': // Close
				case 'z':
				{
					state.sink.endFigure();

					state.inFigure = false;
					state.last = state.first;
				}
				break;

				default:
					assert(false); // TODO
					break;
				}
			}

			{
				if (state.inFigure)
					state.sink.endFigure();

				state.sink.close();

//				g.DrawGeometry(strokePath, strokeBrush);
			}
		}
		else if (strcmp(c->Name(), "circle") == 0)
		{
			gmpi::drawing::Point center{};
			center.x = c->FloatAttribute("cx");
			center.y = c->FloatAttribute("cy");

			float radius = c->FloatAttribute("r");

#if 0 // TODO!!!
			if (fillColor.a > 0)
			{
				g.FillCircle(center, radius, fillBrush);
			}

			if (strokeColor.a > 0)
			{
				g.DrawCircle(center, radius, strokeBrush);
			}
#endif
		}
		else if (strcmp(c->Name(), "ellipse") == 0)
		{
			gmpi::drawing::Point center{};
			center.x = c->FloatAttribute("cx");
			center.y = c->FloatAttribute("cy");

			gmpi::drawing::Size radius{};
			radius.width = c->FloatAttribute("rx");
			radius.height = c->FloatAttribute("ry");

#if 0 // TODO!!!
			if (fillColor.a > 0)
			{
				g.FillEllipse({ center, radius.width, radius.height }, fillBrush);
			}

			if (strokeColor.a > 0)
			{
				g.DrawEllipse({ center, radius.width, radius.height }, strokeBrush);
			}
#endif
		}
		else if (strcmp(c->Name(), "line") == 0)
		{
			gmpi::drawing::Point start{};
			start.x = c->FloatAttribute("x1");
			start.y = c->FloatAttribute("y1");

			gmpi::drawing::Point end{};
			end.x = c->FloatAttribute("x2");
			end.y = c->FloatAttribute("y2");

#if 0 // TODO!!!
			g.DrawLine(start, end, strokeBrush);
#endif
		}
		else if (strcmp(c->Name(), "polyline") == 0)
		{
			std::string points(c->Attribute("points"));
			if (!points.empty())
			{
				std::vector<gmpi::drawing::Point> points2;
				auto p = points.c_str();
				while (*p)
				{
					gmpi::drawing::Point pt{};
					pt.x = atof(p);
					while (*p && *p != ',')
						++p;
					if (*p)
						++p;
					pt.y = atof(p);
					while (*p && *p != ' ')
						++p;
					if (*p)
						++p;
					points2.push_back(pt);
				}

#if 0 // TODO!!!
				g.DrawLines(points2.data(), points.size(), strokeBrush);
#endif
			}
		}
	}

	//if (transformS)
	//{
	//	g.SetTransform(originalTransform);
	//}
}

gmpi::drawing::Size parseToGeometry(tinyxml2::XMLElement* svgE, gmpi::drawing::api::IGeometrySink* sink)
{
	gmpi::drawing::Matrix3x2 transform;

	parseGroup(sink, svgE, transform);

	return
	{
		 svgE->FloatAttribute("width")
		,svgE->FloatAttribute("height")
	};
}

gmpi::drawing::Size parseToGeometry(std::string fullFilename, gmpi::drawing::api::IGeometrySink* sink)
{
	tinyxml2::XMLDocument doc;
	doc.LoadFile(fullFilename.c_str());

	if (doc.Error())
		return {};

	auto svgE = doc.FirstChildElement("svg");

	if (!svgE)
		return {};

	return parseToGeometry(svgE, sink);
}
}