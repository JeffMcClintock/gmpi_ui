#pragma once
// A small, dependency-light SVG renderer.
//
// Scope: the "static artwork" subset of SVG 1.1 — the shapes, paths, paints
// and transforms that vector artwork exported from Inkscape or Illustrator
// actually uses. Enough to draw a synth panel; not a browser.
//
// Supported
//   shapes      rect (incl. rx/ry), circle, ellipse, line, polyline, polygon,
//               path, g, nested svg, a
//   path data   M m L l H h V v C c S s Q q T t A a Z z, implicit command
//               repetition, and the "draw command with no open subpath
//               implies a moveto" rule
//   paints      fill and stroke as none | #rgb | #rrggbb | rgb() | a named
//               colour | currentColor | url(#id)
//   gradients   linearGradient and radialGradient, in both userSpaceOnUse and
//               objectBoundingBox units, with spreadMethod, gradientTransform,
//               stop-opacity, and href/xlink:href inheritance
//   styling     presentation attributes AND the style="" attribute (which
//               wins), inherited down the tree; fill-rule, fill-opacity,
//               stroke-opacity, opacity, stroke-width, stroke-linecap,
//               stroke-linejoin, stroke-miterlimit, stroke-dasharray,
//               display, visibility, color
//   document    width/height/viewBox
//
// Not supported (silently skipped, so a document using them still draws the
// rest): text, use/symbol, clipPath, mask, filter, pattern, image,
// preserveAspectRatio (viewBox is fitted by non-uniform scale), and CSS
// selectors in <style> blocks.
//
// Two approximations worth knowing about:
//   * `opacity` on a group is applied by multiplying it into each descendant's
//     paint alpha, rather than by compositing the group through an offscreen
//     layer. The two agree unless the group's own contents overlap.
//   * A gradient in objectBoundingBox units on a non-square box is mapped by
//     scaling its endpoints. SVG shears the gradient's iso-lines in that case;
//     this keeps them perpendicular to the axis, as the drawing API does.

#include <algorithm>
#include <cmath>
#include <cstring>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "tinyXml2/tinyxml2.h"
#include "GmpiUiDrawing.h"

namespace SvgParser
{

// ============================================================
// Number and token scanning
// ============================================================
namespace detail
{

inline bool isSvgDigit(char c) { return c >= '0' && c <= '9'; }
inline bool isSvgAlpha(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
inline bool isSvgSpace(char c)
{
	return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

// A cursor over a run of SVG numbers.
//
// The number reader is hand-rolled rather than handed to strtod because
// strtod honours the C locale: under a locale whose decimal separator is a
// comma, strtod("1.5") returns 1, and every path in the document collapses.
// SVG number syntax is always '.', everywhere, regardless of locale.
struct Scanner
{
	const char* p{};
	const char* end{};

	Scanner() = default;
	Scanner(std::string_view s) : p(s.data()), end(s.data() + s.size()) {}

	void skipSeparators()
	{
		while (p < end && (isSvgSpace(*p) || *p == ','))
			++p;
	}

	bool atEnd()
	{
		skipSeparators();
		return p >= end;
	}

	bool peekIsCommand()
	{
		skipSeparators();
		return p < end && isSvgAlpha(*p);
	}

	bool readNumber(float& out)
	{
		skipSeparators();

		const char* const start = p;

		bool negative = false;
		if (p < end && (*p == '+' || *p == '-'))
		{
			negative = (*p == '-');
			++p;
		}

		double mantissa = 0.0;
		int    fractionDigits = 0;
		bool   anyDigits = false;

		while (p < end && isSvgDigit(*p))
		{
			mantissa = mantissa * 10.0 + (*p - '0');
			anyDigits = true;
			++p;
		}

		if (p < end && *p == '.')
		{
			++p;
			while (p < end && isSvgDigit(*p))
			{
				mantissa = mantissa * 10.0 + (*p - '0');
				++fractionDigits;
				anyDigits = true;
				++p;
			}
		}

		if (!anyDigits)
		{
			p = start;
			return false;
		}

		int exponent = 0;
		if (p < end && (*p == 'e' || *p == 'E'))
		{
			const char* const beforeExponent = p;
			++p;

			bool exponentNegative = false;
			if (p < end && (*p == '+' || *p == '-'))
			{
				exponentNegative = (*p == '-');
				++p;
			}

			if (p < end && isSvgDigit(*p))
			{
				while (p < end && isSvgDigit(*p))
				{
					exponent = exponent * 10 + (*p - '0');
					++p;
				}
				if (exponentNegative)
					exponent = -exponent;
			}
			else
			{
				p = beforeExponent; // a trailing 'e' that is not an exponent
			}
		}

		double value = mantissa;
		const int scale = exponent - fractionDigits;
		if (scale != 0)
			value *= std::pow(10.0, scale);

		out = static_cast<float>(negative ? -value : value);
		return true;
	}

	// Arc flags are single characters, and may be run together with what
	// follows: "a5,5 0 1150,0" is large-arc=1, sweep=1, then 50,0. Reading
	// them as ordinary numbers would swallow the lot.
	bool readFlag(bool& out)
	{
		skipSeparators();
		if (p < end && (*p == '0' || *p == '1'))
		{
			out = (*p == '1');
			++p;
			return true;
		}
		return false;
	}

	bool readPoint(gmpi::drawing::Point& out)
	{
		return readNumber(out.x) && readNumber(out.y);
	}

	char readCommand()
	{
		skipSeparators();
		if (p < end && isSvgAlpha(*p))
			return *p++;
		return 0;
	}
};

inline std::string_view trim(std::string_view s)
{
	while (!s.empty() && isSvgSpace(s.front()))
		s.remove_prefix(1);
	while (!s.empty() && isSvgSpace(s.back()))
		s.remove_suffix(1);
	return s;
}

// ============================================================
// Colours
// ============================================================

struct NamedColor
{
	std::string_view name;
	uint32_t rgb;
};

// The SVG/CSS colour keywords, matching gmpi::drawing::Colors.
inline constexpr NamedColor kNamedColors[] = {
	{ "aliceblue", 0xF0F8FFu },            { "antiquewhite", 0xFAEBD7u },
	{ "aqua", 0x00FFFFu },                 { "aquamarine", 0x7FFFD4u },
	{ "azure", 0xF0FFFFu },                { "beige", 0xF5F5DCu },
	{ "bisque", 0xFFE4C4u },               { "black", 0x000000u },
	{ "blanchedalmond", 0xFFEBCDu },       { "blue", 0x0000FFu },
	{ "blueviolet", 0x8A2BE2u },           { "brown", 0xA52A2Au },
	{ "burlywood", 0xDEB887u },            { "cadetblue", 0x5F9EA0u },
	{ "chartreuse", 0x7FFF00u },           { "chocolate", 0xD2691Eu },
	{ "coral", 0xFF7F50u },                { "cornflowerblue", 0x6495EDu },
	{ "cornsilk", 0xFFF8DCu },             { "crimson", 0xDC143Cu },
	{ "cyan", 0x00FFFFu },                 { "darkblue", 0x00008Bu },
	{ "darkcyan", 0x008B8Bu },             { "darkgoldenrod", 0xB8860Bu },
	{ "darkgray", 0xA9A9A9u },             { "darkgrey", 0xA9A9A9u },
	{ "darkgreen", 0x006400u },            { "darkkhaki", 0xBDB76Bu },
	{ "darkmagenta", 0x8B008Bu },          { "darkolivegreen", 0x556B2Fu },
	{ "darkorange", 0xFF8C00u },           { "darkorchid", 0x9932CCu },
	{ "darkred", 0x8B0000u },              { "darksalmon", 0xE9967Au },
	{ "darkseagreen", 0x8FBC8Fu },         { "darkslateblue", 0x483D8Bu },
	{ "darkslategray", 0x2F4F4Fu },        { "darkslategrey", 0x2F4F4Fu },
	{ "darkturquoise", 0x00CED1u },        { "darkviolet", 0x9400D3u },
	{ "deeppink", 0xFF1493u },             { "deepskyblue", 0x00BFFFu },
	{ "dimgray", 0x696969u },              { "dimgrey", 0x696969u },
	{ "dodgerblue", 0x1E90FFu },           { "firebrick", 0xB22222u },
	{ "floralwhite", 0xFFFAF0u },          { "forestgreen", 0x228B22u },
	{ "fuchsia", 0xFF00FFu },              { "gainsboro", 0xDCDCDCu },
	{ "ghostwhite", 0xF8F8FFu },           { "gold", 0xFFD700u },
	{ "goldenrod", 0xDAA520u },            { "gray", 0x808080u },
	{ "grey", 0x808080u },                 { "green", 0x008000u },
	{ "greenyellow", 0xADFF2Fu },          { "honeydew", 0xF0FFF0u },
	{ "hotpink", 0xFF69B4u },              { "indianred", 0xCD5C5Cu },
	{ "indigo", 0x4B0082u },               { "ivory", 0xFFFFF0u },
	{ "khaki", 0xF0E68Cu },                { "lavender", 0xE6E6FAu },
	{ "lavenderblush", 0xFFF0F5u },        { "lawngreen", 0x7CFC00u },
	{ "lemonchiffon", 0xFFFACDu },         { "lightblue", 0xADD8E6u },
	{ "lightcoral", 0xF08080u },           { "lightcyan", 0xE0FFFFu },
	{ "lightgoldenrodyellow", 0xFAFAD2u }, { "lightgray", 0xD3D3D3u },
	{ "lightgrey", 0xD3D3D3u },            { "lightgreen", 0x90EE90u },
	{ "lightpink", 0xFFB6C1u },            { "lightsalmon", 0xFFA07Au },
	{ "lightseagreen", 0x20B2AAu },        { "lightskyblue", 0x87CEFAu },
	{ "lightslategray", 0x778899u },       { "lightslategrey", 0x778899u },
	{ "lightsteelblue", 0xB0C4DEu },       { "lightyellow", 0xFFFFE0u },
	{ "lime", 0x00FF00u },                 { "limegreen", 0x32CD32u },
	{ "linen", 0xFAF0E6u },                { "magenta", 0xFF00FFu },
	{ "maroon", 0x800000u },               { "mediumaquamarine", 0x66CDAAu },
	{ "mediumblue", 0x0000CDu },           { "mediumorchid", 0xBA55D3u },
	{ "mediumpurple", 0x9370DBu },         { "mediumseagreen", 0x3CB371u },
	{ "mediumslateblue", 0x7B68EEu },      { "mediumspringgreen", 0x00FA9Au },
	{ "mediumturquoise", 0x48D1CCu },      { "mediumvioletred", 0xC71585u },
	{ "midnightblue", 0x191970u },         { "mintcream", 0xF5FFFAu },
	{ "mistyrose", 0xFFE4E1u },            { "moccasin", 0xFFE4B5u },
	{ "navajowhite", 0xFFDEADu },          { "navy", 0x000080u },
	{ "oldlace", 0xFDF5E6u },              { "olive", 0x808000u },
	{ "olivedrab", 0x6B8E23u },            { "orange", 0xFFA500u },
	{ "orangered", 0xFF4500u },            { "orchid", 0xDA70D6u },
	{ "palegoldenrod", 0xEEE8AAu },        { "palegreen", 0x98FB98u },
	{ "paleturquoise", 0xAFEEEEu },        { "palevioletred", 0xDB7093u },
	{ "papayawhip", 0xFFEFD5u },           { "peachpuff", 0xFFDAB9u },
	{ "peru", 0xCD853Fu },                 { "pink", 0xFFC0CBu },
	{ "plum", 0xDDA0DDu },                 { "powderblue", 0xB0E0E6u },
	{ "purple", 0x800080u },                { "red", 0xFF0000u },
	{ "rosybrown", 0xBC8F8Fu },            { "royalblue", 0x4169E1u },
	{ "saddlebrown", 0x8B4513u },          { "salmon", 0xFA8072u },
	{ "sandybrown", 0xF4A460u },           { "seagreen", 0x2E8B57u },
	{ "seashell", 0xFFF5EEu },             { "sienna", 0xA0522Du },
	{ "silver", 0xC0C0C0u },               { "skyblue", 0x87CEEBu },
	{ "slateblue", 0x6A5ACDu },            { "slategray", 0x708090u },
	{ "slategrey", 0x708090u },            { "snow", 0xFFFAFAu },
	{ "springgreen", 0x00FF7Fu },          { "steelblue", 0x4682B4u },
	{ "tan", 0xD2B48Cu },                  { "teal", 0x008080u },
	{ "thistle", 0xD8BFD8u },              { "tomato", 0xFF6347u },
	{ "turquoise", 0x40E0D0u },            { "violet", 0xEE82EEu },
	{ "wheat", 0xF5DEB3u },                { "white", 0xFFFFFFu },
	{ "whitesmoke", 0xF5F5F5u },           { "yellow", 0xFFFF00u },
	{ "yellowgreen", 0x9ACD32u },
};

inline bool lookupNamedColor(std::string_view name, gmpi::drawing::Color& out)
{
	std::string lower(name);
	std::transform(lower.begin(), lower.end(), lower.begin(),
		[](unsigned char c) { return static_cast<char>(std::tolower(c)); });

	for (const auto& entry : kNamedColors)
	{
		if (entry.name == lower)
		{
			out = gmpi::drawing::colorFromHex(entry.rgb);
			return true;
		}
	}
	return false;
}

inline int hexDigit(char c)
{
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'a' && c <= 'f') return c - 'a' + 10;
	if (c >= 'A' && c <= 'F') return c - 'A' + 10;
	return -1;
}

// #rgb, #rgba, #rrggbb or #rrggbbaa.
inline bool parseHexColor(std::string_view s, gmpi::drawing::Color& out)
{
	s.remove_prefix(1); // '#'

	int digits[8]{};
	if (s.size() != 3 && s.size() != 4 && s.size() != 6 && s.size() != 8)
		return false;

	for (size_t i = 0; i < s.size(); ++i)
	{
		digits[i] = hexDigit(s[i]);
		if (digits[i] < 0)
			return false;
	}

	uint8_t channels[4] = { 0, 0, 0, 255 };
	if (s.size() <= 4)
	{
		// Shorthand: each digit is doubled, so #48b means #4488bb.
		for (size_t i = 0; i < s.size(); ++i)
			channels[i] = static_cast<uint8_t>(digits[i] * 17);
	}
	else
	{
		for (size_t i = 0; i < s.size() / 2; ++i)
			channels[i] = static_cast<uint8_t>(digits[i * 2] * 16 + digits[i * 2 + 1]);
	}

	out = gmpi::drawing::colorFromArgb(channels[0], channels[1], channels[2],
	                                   channels[3] / 255.0f);
	return true;
}

// rgb(r,g,b) / rgba(r,g,b,a), with each channel as a number or a percentage.
inline bool parseRgbColor(std::string_view s, gmpi::drawing::Color& out)
{
	const auto open = s.find('(');
	const auto close = s.rfind(')');
	if (open == std::string_view::npos || close == std::string_view::npos || close < open)
		return false;

	Scanner sc(s.substr(open + 1, close - open - 1));

	float channels[4] = { 0.f, 0.f, 0.f, 1.f };
	for (int i = 0; i < 3; ++i)
	{
		if (!sc.readNumber(channels[i]))
			return false;

		sc.skipSeparators();
		if (sc.p < sc.end && *sc.p == '%')
		{
			++sc.p;
			channels[i] = channels[i] * 255.0f / 100.0f;
		}
	}
	sc.readNumber(channels[3]); // optional alpha, already defaulted to 1

	const auto toByte = [](float v) {
		return static_cast<uint8_t>(std::clamp(v, 0.0f, 255.0f) + 0.5f);
	};

	out = gmpi::drawing::colorFromArgb(toByte(channels[0]), toByte(channels[1]),
	                                   toByte(channels[2]), std::clamp(channels[3], 0.0f, 1.0f));
	return true;
}

inline bool parseColor(std::string_view s, gmpi::drawing::Color& out)
{
	s = trim(s);
	if (s.empty())
		return false;

	if (s.front() == '#')
		return parseHexColor(s, out);

	if (s.size() > 3 && (s.compare(0, 4, "rgb(") == 0 || s.compare(0, 5, "rgba(") == 0))
		return parseRgbColor(s, out);

	return lookupNamedColor(s, out);
}

// ============================================================
// Transforms
// ============================================================

// A full transform list: "translate(10,20) rotate(45) scale(2)". Functions
// apply left to right, so the leftmost is the outermost.
inline gmpi::drawing::Matrix3x2 parseTransform(std::string_view s)
{
	using namespace gmpi::drawing;

	Matrix3x2 result; // identity

	const char* p = s.data();
	const char* const end = s.data() + s.size();

	while (p < end)
	{
		while (p < end && (isSvgSpace(*p) || *p == ','))
			++p;

		const char* const nameStart = p;
		while (p < end && (isSvgAlpha(*p)))
			++p;
		const std::string_view name(nameStart, p - nameStart);

		while (p < end && isSvgSpace(*p))
			++p;

		if (name.empty() || p >= end || *p != '(')
			break;

		const char* const argsStart = ++p;
		while (p < end && *p != ')')
			++p;
		if (p >= end)
			break;

		Scanner sc(std::string_view(argsStart, p - argsStart));
		++p; // ')'

		float args[6]{};
		int count = 0;
		while (count < 6 && sc.readNumber(args[count]))
			++count;

		Matrix3x2 m; // identity
		if (name == "matrix" && count == 6)
		{
			m = Matrix3x2{ args[0], args[1], args[2], args[3], args[4], args[5] };
		}
		else if (name == "translate" && count >= 1)
		{
			m = makeTranslation(args[0], count >= 2 ? args[1] : 0.0f);
		}
		else if (name == "scale" && count >= 1)
		{
			m = makeScale(args[0], count >= 2 ? args[1] : args[0]);
		}
		else if (name == "rotate" && count >= 1)
		{
			// SVG states rotation in degrees; makeRotation takes radians.
			constexpr float kDegToRad = 3.14159265358979324f / 180.0f;
			const Point center = (count >= 3) ? Point{ args[1], args[2] } : Point{};
			m = makeRotation(args[0] * kDegToRad, center);
		}
		else if (name == "skewX" && count >= 1)
		{
			constexpr float kDegToRad = 3.14159265358979324f / 180.0f;
			m = makeSkew(args[0] * kDegToRad, 0.0f);
		}
		else if (name == "skewY" && count >= 1)
		{
			constexpr float kDegToRad = 3.14159265358979324f / 180.0f;
			m = makeSkew(0.0f, args[0] * kDegToRad);
		}
		else
		{
			continue; // unrecognised function: ignore it, keep the rest
		}

		// Points are row vectors here, so `a * b` means "apply a, then b".
		// Later functions in the list are nested inside earlier ones.
		result = m * result;
	}

	return result;
}

// ============================================================
// Paint and style
// ============================================================

struct Paint
{
	enum class Kind { None, Solid, Gradient };

	Kind kind = Kind::None;
	gmpi::drawing::Color color{};
	std::string gradientId;
};

struct Style
{
	Paint fill{ Paint::Kind::Solid, gmpi::drawing::Colors::Black, {} };
	Paint stroke{};

	float fillOpacity = 1.0f;
	float strokeOpacity = 1.0f;
	float groupOpacity = 1.0f;   // accumulated `opacity`, see header note

	float strokeWidth = 1.0f;
	float miterLimit = 4.0f;     // SVG's default, not the drawing API's 10
	gmpi::drawing::CapStyle  lineCap = gmpi::drawing::CapStyle::Flat;
	gmpi::drawing::LineJoin  lineJoin = gmpi::drawing::LineJoin::Miter;
	std::vector<float> dashes;

	gmpi::drawing::FillMode fillMode = gmpi::drawing::FillMode::Winding;
	gmpi::drawing::Color currentColor = gmpi::drawing::Colors::Black;

	bool display = true;
	bool visible = true;
};

inline void parsePaint(std::string_view value, const Style& style, Paint& out)
{
	value = trim(value);

	if (value.empty() || value == "none" || value == "transparent")
	{
		out = Paint{ Paint::Kind::None, {}, {} };
		return;
	}

	if (value == "currentColor")
	{
		out = Paint{ Paint::Kind::Solid, style.currentColor, {} };
		return;
	}

	if (value.compare(0, 4, "url(") == 0)
	{
		const auto hash = value.find('#');
		const auto close = value.find(')', hash == std::string_view::npos ? 0 : hash);
		if (hash != std::string_view::npos && close != std::string_view::npos)
		{
			out.kind = Paint::Kind::Gradient;
			out.gradientId.assign(value.substr(hash + 1, close - hash - 1));
			return;
		}
		// A url() we cannot resolve falls through to its fallback colour, if any.
		const auto close2 = value.find(')');
		if (close2 != std::string_view::npos)
		{
			const auto fallback = trim(value.substr(close2 + 1));
			if (!fallback.empty() && parseColor(fallback, out.color))
			{
				out.kind = Paint::Kind::Solid;
				return;
			}
		}
		out = Paint{ Paint::Kind::None, {}, {} };
		return;
	}

	gmpi::drawing::Color color;
	if (parseColor(value, color))
		out = Paint{ Paint::Kind::Solid, color, {} };
	else
		out = Paint{ Paint::Kind::None, {}, {} };
}

// A length in the "user units" sense. Percentages need a reference length,
// which callers supply; the absolute units are the CSS 96dpi ones.
inline float parseLength(std::string_view value, float percentBasis = 0.0f, float fallback = 0.0f)
{
	value = trim(value);
	if (value.empty())
		return fallback;

	Scanner sc(value);
	float n = 0.f;
	if (!sc.readNumber(n))
		return fallback;

	const auto unit = trim(std::string_view(sc.p, sc.end - sc.p));
	if (unit.empty())         return n;
	if (unit == "%")          return n * percentBasis / 100.0f;
	if (unit == "px")         return n;
	if (unit == "pt")         return n * 96.0f / 72.0f;
	if (unit == "pc")         return n * 16.0f;
	if (unit == "mm")         return n * 96.0f / 25.4f;
	if (unit == "cm")         return n * 96.0f / 2.54f;
	if (unit == "in")         return n * 96.0f;
	return n;
}

inline void parseDashArray(std::string_view value, std::vector<float>& out)
{
	out.clear();
	value = trim(value);
	if (value.empty() || value == "none")
		return;

	Scanner sc(value);
	float n = 0.f;
	while (sc.readNumber(n))
		out.push_back(n);

	// An odd count repeats to make it even, which is what the stroke API expects.
	if (out.size() % 2)
		out.insert(out.end(), out.begin(), out.begin() + out.size());

	// An all-zero pattern means "solid", and would otherwise divide by zero.
	if (std::all_of(out.begin(), out.end(), [](float v) { return v <= 0.f; }))
		out.clear();
}

inline void applyProperty(Style& style, std::string_view name, std::string_view value)
{
	name = trim(name);
	value = trim(value);

	if (value.empty() || value == "inherit")
		return;

	if (name == "fill")
	{
		parsePaint(value, style, style.fill);
	}
	else if (name == "stroke")
	{
		parsePaint(value, style, style.stroke);
	}
	else if (name == "color")
	{
		parseColor(value, style.currentColor);
	}
	else if (name == "fill-opacity")
	{
		style.fillOpacity = std::clamp(parseLength(value, 1.0f, 1.0f), 0.0f, 1.0f);
	}
	else if (name == "stroke-opacity")
	{
		style.strokeOpacity = std::clamp(parseLength(value, 1.0f, 1.0f), 0.0f, 1.0f);
	}
	else if (name == "opacity")
	{
		style.groupOpacity *= std::clamp(parseLength(value, 1.0f, 1.0f), 0.0f, 1.0f);
	}
	else if (name == "stroke-width")
	{
		style.strokeWidth = std::max(0.0f, parseLength(value, 0.0f, style.strokeWidth));
	}
	else if (name == "stroke-miterlimit")
	{
		style.miterLimit = std::max(1.0f, parseLength(value, 0.0f, style.miterLimit));
	}
	else if (name == "stroke-linecap")
	{
		using gmpi::drawing::CapStyle;
		if      (value == "round")  style.lineCap = CapStyle::Round;
		else if (value == "square") style.lineCap = CapStyle::Square;
		else                        style.lineCap = CapStyle::Flat;
	}
	else if (name == "stroke-linejoin")
	{
		using gmpi::drawing::LineJoin;
		if      (value == "round") style.lineJoin = LineJoin::Round;
		else if (value == "bevel") style.lineJoin = LineJoin::Bevel;
		else                       style.lineJoin = LineJoin::Miter;
	}
	else if (name == "stroke-dasharray")
	{
		parseDashArray(value, style.dashes);
	}
	else if (name == "fill-rule" || name == "clip-rule")
	{
		using gmpi::drawing::FillMode;
		style.fillMode = (value == "evenodd") ? FillMode::Alternate : FillMode::Winding;
	}
	else if (name == "display")
	{
		style.display = (value != "none");
	}
	else if (name == "visibility")
	{
		style.visible = (value != "hidden" && value != "collapse");
	}
}

// style="a:b; c:d" — declarations win over the equivalent presentation
// attributes, so this is applied second.
inline void applyStyleAttribute(Style& style, std::string_view css)
{
	size_t pos = 0;
	while (pos < css.size())
	{
		const auto semi = css.find(';', pos);
		const auto decl = css.substr(pos, semi == std::string_view::npos ? std::string_view::npos : semi - pos);

		const auto colon = decl.find(':');
		if (colon != std::string_view::npos)
			applyProperty(style, decl.substr(0, colon), decl.substr(colon + 1));

		if (semi == std::string_view::npos)
			break;
		pos = semi + 1;
	}
}

inline Style resolveStyle(const Style& parent, tinyxml2::XMLElement* e)
{
	Style style = parent;

	// `opacity` does not inherit; it is folded into the running multiplier at
	// the element that declares it, and that product is what descendants see.
	// Everything else here is an inherited property, so starting from the
	// parent's value is right.

	static constexpr const char* kProperties[] = {
		"fill", "stroke", "color", "fill-opacity", "stroke-opacity", "opacity",
		"stroke-width", "stroke-miterlimit", "stroke-linecap", "stroke-linejoin",
		"stroke-dasharray", "fill-rule", "clip-rule", "display", "visibility",
	};

	for (const char* name : kProperties)
	{
		if (const char* value = e->Attribute(name))
			applyProperty(style, name, value);
	}

	if (const char* css = e->Attribute("style"))
		applyStyleAttribute(style, css);

	return style;
}

// ============================================================
// Gradients
// ============================================================

struct GradientStop
{
	float offset{};
	gmpi::drawing::Color color{};
};

struct Gradient
{
	bool radial = false;
	bool userSpace = false;                 // gradientUnits="userSpaceOnUse"

	// Linear, in whichever unit system `userSpace` selects.
	float x1 = 0.0f, y1 = 0.0f, x2 = 1.0f, y2 = 0.0f;

	// Radial.
	float cx = 0.5f, cy = 0.5f, r = 0.5f;
	float fx = 0.5f, fy = 0.5f;
	bool  hasFocus = false;

	gmpi::drawing::Matrix3x2 transform;
	gmpi::drawing::ExtendMode extendMode = gmpi::drawing::ExtendMode::Clamp;

	std::vector<GradientStop> stops;
	std::string href;                       // xlink:href="#other"

	// Which attributes this element set itself, so href inheritance only
	// fills in the gaps.
	bool setUnits = false, setTransform = false, setSpread = false;
	bool setX1 = false, setY1 = false, setX2 = false, setY2 = false;
	bool setCx = false, setCy = false, setR = false;
};

using GradientMap = std::unordered_map<std::string, Gradient>;

inline void parseGradientStops(tinyxml2::XMLElement* e, std::vector<GradientStop>& out)
{
	for (auto* child = e->FirstChildElement("stop"); child;
	     child = child->NextSiblingElement("stop"))
	{
		GradientStop stop;
		stop.color = gmpi::drawing::Colors::Black;

		// A stop's colour and opacity may arrive as attributes or via style="".
		std::string_view colorText;
		std::string_view opacityText;

		if (const char* v = child->Attribute("stop-color"))   colorText = v;
		if (const char* v = child->Attribute("stop-opacity")) opacityText = v;

		if (const char* css = child->Attribute("style"))
		{
			std::string_view s(css);
			size_t pos = 0;
			while (pos < s.size())
			{
				const auto semi = s.find(';', pos);
				const auto decl = s.substr(pos, semi == std::string_view::npos ? std::string_view::npos : semi - pos);
				const auto colon = decl.find(':');
				if (colon != std::string_view::npos)
				{
					const auto name = trim(decl.substr(0, colon));
					const auto value = trim(decl.substr(colon + 1));
					if (name == "stop-color")   colorText = value;
					if (name == "stop-opacity") opacityText = value;
				}
				if (semi == std::string_view::npos)
					break;
				pos = semi + 1;
			}
		}

		if (!colorText.empty())
			parseColor(colorText, stop.color);

		stop.offset = std::clamp(parseLength(child->Attribute("offset") ? child->Attribute("offset") : "0", 1.0f, 0.0f), 0.0f, 1.0f);

		if (!opacityText.empty())
			stop.color.a *= std::clamp(parseLength(opacityText, 1.0f, 1.0f), 0.0f, 1.0f);

		out.push_back(stop);
	}
}

inline void parseGradientElement(tinyxml2::XMLElement* e, bool radial, Gradient& out)
{
	out.radial = radial;

	const auto readCoord = [&](const char* name, float& value, bool& wasSet) {
		if (const char* v = e->Attribute(name))
		{
			value = parseLength(v, 1.0f, value);
			wasSet = true;
		}
	};

	if (const char* units = e->Attribute("gradientUnits"))
	{
		out.userSpace = (std::string_view(units) == "userSpaceOnUse");
		out.setUnits = true;
	}

	if (const char* t = e->Attribute("gradientTransform"))
	{
		out.transform = parseTransform(t);
		out.setTransform = true;
	}

	if (const char* spread = e->Attribute("spreadMethod"))
	{
		using gmpi::drawing::ExtendMode;
		const std::string_view s(spread);
		out.extendMode = (s == "reflect") ? ExtendMode::Mirror
		               : (s == "repeat")  ? ExtendMode::Wrap
		                                  : ExtendMode::Clamp;
		out.setSpread = true;
	}

	if (radial)
	{
		readCoord("cx", out.cx, out.setCx);
		readCoord("cy", out.cy, out.setCy);
		readCoord("r",  out.r,  out.setR);

		bool dummy = false;
		out.fx = out.cx;
		out.fy = out.cy;
		if (e->Attribute("fx")) { readCoord("fx", out.fx, dummy); out.hasFocus = true; }
		if (e->Attribute("fy")) { readCoord("fy", out.fy, dummy); out.hasFocus = true; }
	}
	else
	{
		readCoord("x1", out.x1, out.setX1);
		readCoord("y1", out.y1, out.setY1);
		readCoord("x2", out.x2, out.setX2);
		readCoord("y2", out.y2, out.setY2);
	}

	if (const char* href = e->Attribute("xlink:href"))
	{
		if (href[0] == '#') out.href = href + 1;
	}
	else if (const char* href2 = e->Attribute("href"))
	{
		if (href2[0] == '#') out.href = href2 + 1;
	}

	parseGradientStops(e, out.stops);
}

// Gradients may live anywhere in the tree, not only inside <defs>.
inline void collectGradients(tinyxml2::XMLElement* e, GradientMap& out)
{
	for (auto* child = e->FirstChildElement(); child; child = child->NextSiblingElement())
	{
		const std::string_view name(child->Name());
		const bool radial = (name == "radialGradient");

		if (radial || name == "linearGradient")
		{
			if (const char* id = child->Attribute("id"))
			{
				Gradient g;
				parseGradientElement(child, radial, g);
				out.emplace(id, std::move(g));
			}
		}

		collectGradients(child, out);
	}
}

// Follow href to fill in whatever this gradient did not set itself.
inline Gradient resolveGradient(const GradientMap& map, const Gradient& start)
{
	Gradient result = start;

	std::string next = start.href;
	for (int depth = 0; depth < 8 && !next.empty(); ++depth)
	{
		const auto it = map.find(next);
		if (it == map.end())
			break;

		const Gradient& base = it->second;

		if (result.stops.empty())      result.stops = base.stops;
		if (!result.setUnits)        { result.userSpace = base.userSpace;   result.setUnits = base.setUnits; }
		if (!result.setTransform)    { result.transform = base.transform;   result.setTransform = base.setTransform; }
		if (!result.setSpread)       { result.extendMode = base.extendMode; result.setSpread = base.setSpread; }
		if (!result.setX1)           { result.x1 = base.x1; result.setX1 = base.setX1; }
		if (!result.setY1)           { result.y1 = base.y1; result.setY1 = base.setY1; }
		if (!result.setX2)           { result.x2 = base.x2; result.setX2 = base.setX2; }
		if (!result.setY2)           { result.y2 = base.y2; result.setY2 = base.setY2; }
		if (!result.setCx)           { result.cx = base.cx; result.setCx = base.setCx; }
		if (!result.setCy)           { result.cy = base.cy; result.setCy = base.setCy; }
		if (!result.setR)            { result.r  = base.r;  result.setR  = base.setR;  }
		if (!result.hasFocus && base.hasFocus)
		{
			result.fx = base.fx;
			result.fy = base.fy;
			result.hasFocus = true;
		}

		next = base.href;
	}

	return result;
}

// ============================================================
// Rendering
// ============================================================

struct Context
{
	gmpi::drawing::Graphics* graphics{};
	gmpi::drawing::Matrix3x2 baseTransform;
	GradientMap gradients;
};

// Accumulated as a path is built, for gradients in objectBoundingBox units.
// Curve control points are included, so a curved shape's box is an
// over-estimate — the same simplification most lightweight renderers make.
struct BoundsAccumulator
{
	gmpi::drawing::Rect rect{};
	bool valid = false;

	void add(gmpi::drawing::Point p)
	{
		if (!valid)
		{
			rect = { p.x, p.y, p.x, p.y };
			valid = true;
			return;
		}
		rect.left   = std::min(rect.left,   p.x);
		rect.top    = std::min(rect.top,    p.y);
		rect.right  = std::max(rect.right,  p.x);
		rect.bottom = std::max(rect.bottom, p.y);
	}
};

inline gmpi::drawing::Brush makeBrush(
	gmpi::drawing::Graphics& g,
	const Context& ctx,
	const Paint& paint,
	float opacity,
	gmpi::drawing::Rect bounds)
{
	using namespace gmpi::drawing;

	Brush brush;

	if (paint.kind == Paint::Kind::Solid)
	{
		Color color = paint.color;
		color.a *= opacity;
		auto solid = g.createSolidColorBrush(color);
		brush = solid;
		return brush;
	}

	if (paint.kind != Paint::Kind::Gradient)
		return brush;

	const auto it = ctx.gradients.find(paint.gradientId);
	if (it == ctx.gradients.end())
		return brush;

	const Gradient gradient = resolveGradient(ctx.gradients, it->second);
	if (gradient.stops.empty())
		return brush;

	std::vector<Gradientstop> stops;
	stops.reserve(gradient.stops.size());
	for (const auto& s : gradient.stops)
	{
		Color color = s.color;
		color.a *= opacity;
		stops.push_back(Gradientstop{ s.offset, color });
	}

	auto stopCollection = g.createGradientstopCollection(stops, gradient.extendMode);

	const float boundsW = bounds.right - bounds.left;
	const float boundsH = bounds.bottom - bounds.top;

	// objectBoundingBox coordinates are fractions of the shape's own box.
	const auto mapX = [&](float v) { return gradient.userSpace ? v : bounds.left + v * boundsW; };
	const auto mapY = [&](float v) { return gradient.userSpace ? v : bounds.top  + v * boundsH; };

	BrushProperties brushProperties;
	brushProperties.transform = gradient.transform;

	if (gradient.radial)
	{
		RadialGradientBrushProperties properties;
		properties.center = { mapX(gradient.cx), mapY(gradient.cy) };
		properties.radiusX = gradient.userSpace ? gradient.r : gradient.r * boundsW;
		properties.radiusY = gradient.userSpace ? gradient.r : gradient.r * boundsH;
		properties.gradientOriginOffset = gradient.hasFocus
			? Point{ mapX(gradient.fx) - properties.center.x,
			         mapY(gradient.fy) - properties.center.y }
			: Point{};

		auto radial = g.createRadialGradientBrush(properties, brushProperties, stopCollection);
		brush = radial;
	}
	else
	{
		LinearGradientBrushProperties properties;
		properties.startPoint = { mapX(gradient.x1), mapY(gradient.y1) };
		properties.endPoint   = { mapX(gradient.x2), mapY(gradient.y2) };

		auto linear = g.createLinearGradientBrush(properties, brushProperties, stopCollection);
		brush = linear;
	}

	return brush;
}

// ------------------------------------------------------------
// Path data
// ------------------------------------------------------------

struct PathBuilder
{
	gmpi::drawing::GeometrySink& sink;
	BoundsAccumulator& bounds;
	gmpi::drawing::FigureBegin figureBegin = gmpi::drawing::FigureBegin::Filled;

	gmpi::drawing::Point current{};
	gmpi::drawing::Point subpathStart{};
	gmpi::drawing::Point lastCubicControl{};
	gmpi::drawing::Point lastQuadControl{};
	bool inFigure = false;
	char previousCommand = 0;

	void moveTo(gmpi::drawing::Point p)
	{
		endFigure(gmpi::drawing::FigureEnd::Open);
		sink.beginFigure(p, figureBegin);
		bounds.add(p);
		inFigure = true;
		current = subpathStart = p;
	}

	// "If a drawing command follows a closepath, the next subpath starts at
	// the same point as the one just closed" — and a path may legally begin
	// with a drawing command, in which case it starts wherever we are.
	void ensureFigure()
	{
		if (inFigure)
			return;
		sink.beginFigure(current, figureBegin);
		bounds.add(current);
		inFigure = true;
		subpathStart = current;
	}

	void lineTo(gmpi::drawing::Point p)
	{
		ensureFigure();
		sink.addLine(p);
		bounds.add(p);
		current = p;
	}

	void cubicTo(gmpi::drawing::Point c1, gmpi::drawing::Point c2, gmpi::drawing::Point p)
	{
		ensureFigure();
		sink.addBezier({ c1, c2, p });
		bounds.add(c1);
		bounds.add(c2);
		bounds.add(p);
		current = p;
		lastCubicControl = c2;
	}

	void quadTo(gmpi::drawing::Point c, gmpi::drawing::Point p)
	{
		ensureFigure();
		sink.addQuadraticBezier({ c, p });
		bounds.add(c);
		bounds.add(p);
		current = p;
		lastQuadControl = c;
	}

	void arcTo(gmpi::drawing::Size radii, float rotationDegrees, bool largeArc, bool sweep,
	           gmpi::drawing::Point p)
	{
		using namespace gmpi::drawing;
		ensureFigure();

		// ArcSegment is the SVG parameterisation already: endpoint, radii,
		// x-axis rotation in degrees, and the two flags.
		sink.addArc({
			p,
			radii,
			rotationDegrees,
			sweep ? SweepDirection::Clockwise : SweepDirection::CounterClockwise,
			largeArc ? ArcSize::Large : ArcSize::Small });

		// The true arc extent needs the centre parameterisation; the endpoints
		// plus a radius margin bound it without that work.
		bounds.add({ std::min(current.x, p.x) - radii.width,  std::min(current.y, p.y) - radii.height });
		bounds.add({ std::max(current.x, p.x) + radii.width,  std::max(current.y, p.y) + radii.height });
		current = p;
	}

	void closeFigure()
	{
		if (inFigure)
		{
			sink.endFigure(gmpi::drawing::FigureEnd::Closed);
			inFigure = false;
		}
		current = subpathStart;
	}

	void endFigure(gmpi::drawing::FigureEnd how)
	{
		if (inFigure)
		{
			sink.endFigure(how);
			inFigure = false;
		}
	}

	// Reflect the previous control point through the current point, which is
	// what S and T mean. With no previous curve of the matching kind, the
	// reflection is the current point itself.
	gmpi::drawing::Point reflectedCubicControl() const
	{
		if (previousCommand == 'C' || previousCommand == 'c' ||
		    previousCommand == 'S' || previousCommand == 's')
		{
			return { 2.0f * current.x - lastCubicControl.x,
			         2.0f * current.y - lastCubicControl.y };
		}
		return current;
	}

	gmpi::drawing::Point reflectedQuadControl() const
	{
		if (previousCommand == 'Q' || previousCommand == 'q' ||
		    previousCommand == 'T' || previousCommand == 't')
		{
			return { 2.0f * current.x - lastQuadControl.x,
			         2.0f * current.y - lastQuadControl.y };
		}
		return current;
	}
};

inline void parsePathData(std::string_view d, PathBuilder& path)
{
	using namespace gmpi::drawing;

	Scanner sc(d);
	char command = 0;

	const auto relative = [&](Point p) {
		return Point{ path.current.x + p.x, path.current.y + p.y };
	};

	for (;;)
	{
		if (sc.atEnd())
			break;

		if (sc.peekIsCommand())
		{
			command = sc.readCommand();
		}
		else if (command == 0)
		{
			break; // leading garbage
		}
		else if (command == 'M')
		{
			command = 'L'; // repeated moveto arguments are implicit linetos
		}
		else if (command == 'm')
		{
			command = 'l';
		}

		Point p{}, c1{}, c2{};

		switch (command)
		{
		case 'M':
			if (!sc.readPoint(p)) return;
			path.moveTo(p);
			break;

		case 'm':
			if (!sc.readPoint(p)) return;
			path.moveTo(relative(p));
			break;

		case 'L':
			if (!sc.readPoint(p)) return;
			path.lineTo(p);
			break;

		case 'l':
			if (!sc.readPoint(p)) return;
			path.lineTo(relative(p));
			break;

		case 'H':
			if (!sc.readNumber(p.x)) return;
			path.lineTo({ p.x, path.current.y });
			break;

		case 'h':
			if (!sc.readNumber(p.x)) return;
			path.lineTo({ path.current.x + p.x, path.current.y });
			break;

		case 'V':
			if (!sc.readNumber(p.y)) return;
			path.lineTo({ path.current.x, p.y });
			break;

		case 'v':
			if (!sc.readNumber(p.y)) return;
			path.lineTo({ path.current.x, path.current.y + p.y });
			break;

		case 'C':
			if (!sc.readPoint(c1) || !sc.readPoint(c2) || !sc.readPoint(p)) return;
			path.cubicTo(c1, c2, p);
			break;

		case 'c':
			if (!sc.readPoint(c1) || !sc.readPoint(c2) || !sc.readPoint(p)) return;
			path.cubicTo(relative(c1), relative(c2), relative(p));
			break;

		case 'S':
			if (!sc.readPoint(c2) || !sc.readPoint(p)) return;
			path.cubicTo(path.reflectedCubicControl(), c2, p);
			break;

		case 's':
			if (!sc.readPoint(c2) || !sc.readPoint(p)) return;
			// Both are relative to the current point, which reflection must
			// be taken from BEFORE the curve moves it.
			c2 = relative(c2);
			p  = relative(p);
			path.cubicTo(path.reflectedCubicControl(), c2, p);
			break;

		case 'Q':
			if (!sc.readPoint(c1) || !sc.readPoint(p)) return;
			path.quadTo(c1, p);
			break;

		case 'q':
			if (!sc.readPoint(c1) || !sc.readPoint(p)) return;
			path.quadTo(relative(c1), relative(p));
			break;

		case 'T':
			if (!sc.readPoint(p)) return;
			path.quadTo(path.reflectedQuadControl(), p);
			break;

		case 't':
			if (!sc.readPoint(p)) return;
			p = relative(p);
			path.quadTo(path.reflectedQuadControl(), p);
			break;

		case 'A':
		case 'a':
		{
			Size radii{};
			float rotation = 0.f;
			bool largeArc = false, sweep = false;

			if (!sc.readNumber(radii.width) || !sc.readNumber(radii.height) ||
			    !sc.readNumber(rotation) ||
			    !sc.readFlag(largeArc) || !sc.readFlag(sweep) ||
			    !sc.readPoint(p))
				return;

			path.arcTo(radii, rotation, largeArc, sweep,
			           command == 'a' ? relative(p) : p);
			break;
		}

		case 'Z':
		case 'z':
			path.closeFigure();
			command = 0; // a following argument would be meaningless
			break;

		default:
			return; // unknown command; the rest of the string is not trustworthy
		}

		if (command != 0)
			path.previousCommand = command;
	}
}

// ------------------------------------------------------------
// Shape elements
// ------------------------------------------------------------

// Every shape is turned into a PathGeometry so that fill and stroke share one
// definition, and so an element with both gets exactly one outline.
inline bool buildShapeGeometry(
	gmpi::drawing::Graphics& g,
	tinyxml2::XMLElement* e,
	const Style& style,
	gmpi::drawing::PathGeometry& geometry,
	BoundsAccumulator& bounds,
	bool& fillable)
{
	using namespace gmpi::drawing;

	const std::string_view name(e->Name());
	fillable = true;

	const auto attr = [&](const char* n, float fallback = 0.0f) {
		const char* v = e->Attribute(n);
		return v ? parseLength(v, 0.0f, fallback) : fallback;
	};

	geometry = g.getFactory().createPathGeometry();
	auto sink = geometry.open();
	sink.setFillMode(style.fillMode);

	if (name == "path")
	{
		const char* d = e->Attribute("d");
		if (!d)
			return false;

		PathBuilder path{ sink, bounds };
		parsePathData(d, path);
		path.endFigure(FigureEnd::Open);
	}
	else if (name == "rect")
	{
		const float x = attr("x"), y = attr("y");
		const float w = attr("width"), h = attr("height");
		if (w <= 0.0f || h <= 0.0f)
			return false;

		// Either radius alone implies the other; both are clamped to half the
		// corresponding side.
		const bool hasRx = e->Attribute("rx") != nullptr;
		const bool hasRy = e->Attribute("ry") != nullptr;
		float rx = hasRx ? attr("rx") : (hasRy ? attr("ry") : 0.0f);
		float ry = hasRy ? attr("ry") : rx;
		rx = std::clamp(rx, 0.0f, w * 0.5f);
		ry = std::clamp(ry, 0.0f, h * 0.5f);

		const Rect r{ x, y, x + w, y + h };
		if (rx > 0.0f && ry > 0.0f)
			sink.addRoundedRect({ r, rx, ry }, FigureBegin::Filled);
		else
			sink.addRect(r, FigureBegin::Filled);

		bounds.add({ r.left, r.top });
		bounds.add({ r.right, r.bottom });
	}
	else if (name == "circle" || name == "ellipse")
	{
		const float cx = attr("cx"), cy = attr("cy");
		float rx, ry;
		if (name == "circle")
		{
			rx = ry = attr("r");
		}
		else
		{
			rx = attr("rx");
			ry = attr("ry");
		}
		if (rx <= 0.0f || ry <= 0.0f)
			return false;

		// Two half-turns, because a single arc from a point back to itself is
		// degenerate.
		sink.beginFigure({ cx - rx, cy }, FigureBegin::Filled);
		sink.addArc({ { cx + rx, cy }, { rx, ry }, 0.f, SweepDirection::Clockwise, ArcSize::Small });
		sink.addArc({ { cx - rx, cy }, { rx, ry }, 0.f, SweepDirection::Clockwise, ArcSize::Small });
		sink.endFigure(FigureEnd::Closed);

		bounds.add({ cx - rx, cy - ry });
		bounds.add({ cx + rx, cy + ry });
	}
	else if (name == "line")
	{
		const Point a{ attr("x1"), attr("y1") };
		const Point b{ attr("x2"), attr("y2") };

		sink.beginFigure(a, FigureBegin::Hollow);
		sink.addLine(b);
		sink.endFigure(FigureEnd::Open);

		bounds.add(a);
		bounds.add(b);
		fillable = false; // `fill` has no effect on a line
	}
	else if (name == "polyline" || name == "polygon")
	{
		const char* pointsText = e->Attribute("points");
		if (!pointsText)
			return false;

		std::vector<Point> points;
		Scanner sc(pointsText);
		Point p;
		while (sc.readPoint(p))
			points.push_back(p);

		if (points.size() < 2)
			return false;

		sink.beginFigure(points.front(), FigureBegin::Filled);
		sink.addLines(std::span<const Point>(points).subspan(1));
		sink.endFigure(name == "polygon" ? FigureEnd::Closed : FigureEnd::Open);

		for (auto pt : points)
			bounds.add(pt);
	}
	else
	{
		return false;
	}

	sink.close();
	return true;
}

inline void drawElement(Context& ctx, tinyxml2::XMLElement* e, const Style& parentStyle,
                        gmpi::drawing::Matrix3x2 parentTransform);

inline void drawChildren(Context& ctx, tinyxml2::XMLElement* e, const Style& style,
                         gmpi::drawing::Matrix3x2 transform)
{
	for (auto* child = e->FirstChildElement(); child; child = child->NextSiblingElement())
		drawElement(ctx, child, style, transform);
}

inline void drawElement(Context& ctx, tinyxml2::XMLElement* e, const Style& parentStyle,
                        gmpi::drawing::Matrix3x2 parentTransform)
{
	using namespace gmpi::drawing;

	const std::string_view name(e->Name());

	// Elements that define resources or metadata rather than artwork. Gradients
	// were harvested up front, so <defs> has nothing left to contribute.
	if (name == "defs" || name == "metadata" || name == "title" || name == "desc" ||
	    name == "style" || name == "linearGradient" || name == "radialGradient" ||
	    name == "clipPath" || name == "mask" || name == "filter" || name == "pattern" ||
	    name == "symbol" || name == "marker" || name == "namedview")
	{
		return;
	}

	const Style style = resolveStyle(parentStyle, e);
	if (!style.display)
		return;

	Matrix3x2 transform = parentTransform;
	if (const char* t = e->Attribute("transform"))
		transform = parseTransform(t) * transform;

	if (name == "g" || name == "a" || name == "svg")
	{
		drawChildren(ctx, e, style, transform);
		return;
	}

	if (!style.visible)
		return;

	auto& g = *ctx.graphics;

	PathGeometry geometry;
	BoundsAccumulator bounds;
	bool fillable = true;
	if (!buildShapeGeometry(g, e, style, geometry, bounds, fillable))
		return;

	const bool wantFill = fillable && style.fill.kind != Paint::Kind::None;
	const bool wantStroke = style.stroke.kind != Paint::Kind::None && style.strokeWidth > 0.0f;
	if (!wantFill && !wantStroke)
		return;

	g.setTransform(transform);

	if (wantFill)
	{
		auto brush = makeBrush(g, ctx, style.fill,
		                       style.fillOpacity * style.groupOpacity, bounds.rect);
		if (brush)
			g.fillGeometry(geometry, brush);
	}

	if (wantStroke)
	{
		auto brush = makeBrush(g, ctx, style.stroke,
		                       style.strokeOpacity * style.groupOpacity, bounds.rect);
		if (brush)
		{
			StrokeStyleProperties properties;
			properties.lineCap = style.lineCap;
			properties.lineJoin = style.lineJoin;
			properties.miterLimit = style.miterLimit;

			// The dash lengths are absolute user units in SVG, but multiples of
			// the stroke width in the drawing API.
			std::vector<float> dashes;
			if (!style.dashes.empty())
			{
				properties.dashStyle = DashStyle::Custom;
				dashes.reserve(style.dashes.size());
				for (float d : style.dashes)
					dashes.push_back(d / style.strokeWidth);
			}

			auto strokeStyle = g.getFactory().createStrokeStyle(properties, dashes);
			g.drawGeometry(geometry, brush, style.strokeWidth, strokeStyle);
		}
	}

	g.setTransform(ctx.baseTransform);
}

} // namespace detail

// ============================================================
// Public interface
// ============================================================

// Render an SVG document into `g`, returning its intrinsic size in user units
// (or {} if the document has no usable root).
//
// Drawing happens in the document's own coordinate system, composed with
// whatever transform `g` already has — so to place or scale the artwork, set a
// transform on `g` first.
inline gmpi::drawing::Size draw(gmpi::drawing::Graphics& g, tinyxml2::XMLElement* svgElement)
{
	using namespace gmpi::drawing;

	if (!svgElement)
		return {};

	detail::Context ctx;
	ctx.graphics = &g;
	ctx.baseTransform = g.getTransform();
	detail::collectGradients(svgElement, ctx.gradients);

	// width/height give the intrinsic size; viewBox gives the coordinate
	// system the artwork is drawn in. When they disagree, the artwork is
	// scaled to fit (preserveAspectRatio is not honoured).
	Size size{
		detail::parseLength(svgElement->Attribute("width")  ? svgElement->Attribute("width")  : "", 0.f, 0.f),
		detail::parseLength(svgElement->Attribute("height") ? svgElement->Attribute("height") : "", 0.f, 0.f)
	};

	Matrix3x2 transform = ctx.baseTransform;

	if (const char* viewBoxText = svgElement->Attribute("viewBox"))
	{
		detail::Scanner sc(viewBoxText);
		float x = 0.f, y = 0.f, w = 0.f, h = 0.f;
		if (sc.readNumber(x) && sc.readNumber(y) && sc.readNumber(w) && sc.readNumber(h) &&
		    w > 0.f && h > 0.f)
		{
			if (size.width <= 0.f)  size.width = w;
			if (size.height <= 0.f) size.height = h;

			const float scaleX = size.width / w;
			const float scaleY = size.height / h;
			if (scaleX != 1.0f || scaleY != 1.0f || x != 0.0f || y != 0.0f)
				transform = makeTranslation(-x, -y) * makeScale(scaleX, scaleY) * transform;
		}
	}

	detail::Style rootStyle;
	detail::drawChildren(ctx, svgElement, rootStyle, transform);

	g.setTransform(ctx.baseTransform);

	return size;
}

// Render an SVG file. Returns {} if it cannot be read or parsed.
inline gmpi::drawing::Size draw(gmpi::drawing::Graphics& g, const std::string& fullFilename)
{
	tinyxml2::XMLDocument doc;
	if (doc.LoadFile(fullFilename.c_str()) != tinyxml2::XML_SUCCESS)
		return {};

	return draw(g, doc.FirstChildElement("svg"));
}

// Render an SVG document held in memory.
inline gmpi::drawing::Size drawFromMemory(gmpi::drawing::Graphics& g, std::string_view xml)
{
	tinyxml2::XMLDocument doc;
	if (doc.Parse(xml.data(), xml.size()) != tinyxml2::XML_SUCCESS)
		return {};

	return draw(g, doc.FirstChildElement("svg"));
}

// ============================================================
// Geometry-only interface
// ============================================================
//
// Flattens every shape in the document into a single geometry sink, discarding
// paint and transform. Used where the caller wants one outline to stroke or
// fill itself (see SynthEditLib's SvgGeometry module) rather than a rendering
// of the document.

namespace detail
{

inline void appendToSink(tinyxml2::XMLElement* e, gmpi::drawing::GeometrySink& sink)
{
	using namespace gmpi::drawing;

	for (auto* child = e->FirstChildElement(); child; child = child->NextSiblingElement())
	{
		const std::string_view name(child->Name());

		if (name == "defs" || name == "metadata" || name == "title" || name == "desc" ||
		    name == "linearGradient" || name == "radialGradient" || name == "namedview")
		{
			continue;
		}

		// Paint is discarded here, but display:none is not paint — it says the
		// element is not part of the picture at all, so it stays out of the
		// outline too. (Inkscape marks hidden guide layers this way, and
		// Fade.svg's "components" layer is exactly that.)
		Style style;
		style = resolveStyle(style, child);
		if (!style.display)
			continue;

		if (name == "g" || name == "a" || name == "svg")
		{
			appendToSink(child, sink);
			continue;
		}

		if (name == "path")
		{
			if (const char* d = child->Attribute("d"))
			{
				BoundsAccumulator bounds;
				PathBuilder path{ sink, bounds };
				parsePathData(d, path);
				path.endFigure(FigureEnd::Open);
			}
			continue;
		}

		// The remaining shapes reuse buildShapeGeometry's parsing by writing
		// straight into this sink instead of a private geometry.
		const auto attr = [&](const char* n, float fallback = 0.0f) {
			const char* v = child->Attribute(n);
			return v ? parseLength(v, 0.0f, fallback) : fallback;
		};

		if (name == "rect")
		{
			const float x = attr("x"), y = attr("y");
			const float w = attr("width"), h = attr("height");
			if (w <= 0.0f || h <= 0.0f)
				continue;

			const bool hasRx = child->Attribute("rx") != nullptr;
			const bool hasRy = child->Attribute("ry") != nullptr;
			float rx = hasRx ? attr("rx") : (hasRy ? attr("ry") : 0.0f);
			float ry = hasRy ? attr("ry") : rx;
			rx = std::clamp(rx, 0.0f, w * 0.5f);
			ry = std::clamp(ry, 0.0f, h * 0.5f);

			const Rect r{ x, y, x + w, y + h };
			if (rx > 0.0f && ry > 0.0f)
				sink.addRoundedRect({ r, rx, ry }, FigureBegin::Filled);
			else
				sink.addRect(r, FigureBegin::Filled);
		}
		else if (name == "circle" || name == "ellipse")
		{
			const float cx = attr("cx"), cy = attr("cy");
			const float rx = (name == "circle") ? attr("r") : attr("rx");
			const float ry = (name == "circle") ? attr("r") : attr("ry");
			if (rx <= 0.0f || ry <= 0.0f)
				continue;

			sink.beginFigure({ cx - rx, cy }, FigureBegin::Filled);
			sink.addArc({ { cx + rx, cy }, { rx, ry }, 0.f, SweepDirection::Clockwise, ArcSize::Small });
			sink.addArc({ { cx - rx, cy }, { rx, ry }, 0.f, SweepDirection::Clockwise, ArcSize::Small });
			sink.endFigure(FigureEnd::Closed);
		}
		else if (name == "line")
		{
			sink.beginFigure({ attr("x1"), attr("y1") }, FigureBegin::Hollow);
			sink.addLine({ attr("x2"), attr("y2") });
			sink.endFigure(FigureEnd::Open);
		}
		else if (name == "polyline" || name == "polygon")
		{
			if (const char* pointsText = child->Attribute("points"))
			{
				std::vector<Point> points;
				Scanner sc(pointsText);
				Point p;
				while (sc.readPoint(p))
					points.push_back(p);

				if (points.size() >= 2)
				{
					sink.beginFigure(points.front(), FigureBegin::Filled);
					sink.addLines(std::span<const Point>(points).subspan(1));
					sink.endFigure(name == "polygon" ? FigureEnd::Closed : FigureEnd::Open);
				}
			}
		}
	}
}

} // namespace detail

inline gmpi::drawing::Size parseToGeometry(tinyxml2::XMLElement* svgE, gmpi::drawing::api::IGeometrySink* sink)
{
	if (!svgE || !sink)
		return {};

	gmpi::drawing::GeometrySink wrapper;
	*gmpi::drawing::AccessPtr::put(wrapper) = sink;
	sink->addRef();

	detail::appendToSink(svgE, wrapper);
	wrapper.close();

	return {
		 svgE->FloatAttribute("width")
		,svgE->FloatAttribute("height")
	};
}

inline gmpi::drawing::Size parseToGeometry(std::string fullFilename, gmpi::drawing::api::IGeometrySink* sink)
{
	tinyxml2::XMLDocument doc;
	if (doc.LoadFile(fullFilename.c_str()) != tinyxml2::XML_SUCCESS)
		return {};

	return parseToGeometry(doc.FirstChildElement("svg"), sink);
}

} // namespace SvgParser
