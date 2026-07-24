#pragma once

// SPDX-License-Identifier: ISC
// Copyright 2007-2026 Jeff McClintock.

/*
#include "JuceGfx.h"

A gmpi_ui drawing backend implemented on top of juce::Graphics.

Unlike the Direct2D (Windows) and CoreGraphics (macOS) backends, this backend has no
platform-specific code at all - it draws through JUCE's own graphics primitives.
This is what enables gmpi_ui on platforms without a native backend (e.g. Linux),
and it also allows gmpi_ui components to render into a regular juce::Component
without overlaying a native window on top of the JUCE window.

Usage: construct a gmpi::jucegfx::Factory (one per plugin/window is fine), then for
each paint callback wrap the juce::Graphics in a gmpi::jucegfx::GraphicsContext.
See helpers/JuceGmpiComponent.h for a ready-made component doing exactly that.

Known limitations (compared to the native backends):
 - gradients interpolate in sRGB space (JUCE) rather than linear color space.
 - radial gradients ignore gradientOriginOffset (JUCE has no focal-point support).
 - fillGeometry ignores the opacityBrush parameter.
 - createRichTextFormat (markdown text) is not implemented.
 - clear() alpha-blends like a fill; it cannot write transparency into the target.
*/

#include <memory>
#include <vector>
#include <algorithm>
#include <cmath>
#include <JuceHeader.h>
#include "../Drawing.h"
#include "../RefCountMacros.h"
#include "Gfx_base.h" // se::generic_graphics: StrokeStyle storage + arc-to-bezier fallbacks.

namespace gmpi
{
namespace jucegfx
{
// gmpi colors are linear-light floats; JUCE colours are 8-bit sRGB.
inline juce::Colour toJuce(const drawing::Color& c)
{
	const auto alpha = static_cast<juce::uint8>(0.5f + 255.0f * std::clamp(c.a, 0.0f, 1.0f));
	return juce::Colour::fromRGBA(
		drawing::linearPixelToSRGB(c.r),
		drawing::linearPixelToSRGB(c.g),
		drawing::linearPixelToSRGB(c.b),
		alpha);
}

inline juce::AffineTransform toJuce(const drawing::Matrix3x2& m)
{
	// gmpi (row vectors): x' = x*_11 + y*_21 + _31. JUCE: x' = x*mat00 + y*mat01 + mat02.
	return { m._11, m._21, m._31, m._12, m._22, m._32 };
}

inline juce::Rectangle<float> toJuce(const drawing::Rect& r)
{
	return juce::Rectangle<float>::leftTopRightBottom(r.left, r.top, r.right, r.bottom);
}

inline juce::Point<float> toJuce(drawing::Point p)
{
	return { p.x, p.y };
}

// this backend reuses the generic (backend-neutral) stroke-style, which simply stores the properties.
using StrokeStyle = se::generic_graphics::StrokeStyle;

inline juce::PathStrokeType toJuce(float strokeWidth, const drawing::api::IStrokeStyle* strokeStyle)
{
	auto joint = juce::PathStrokeType::mitered;
	auto cap = juce::PathStrokeType::butt;

	if (strokeStyle)
	{
		const auto& properties = static_cast<const StrokeStyle*>(strokeStyle)->strokeStyleProperties;

		switch (properties.lineJoin)
		{
		case drawing::LineJoin::Bevel: joint = juce::PathStrokeType::beveled; break;
		case drawing::LineJoin::Round: joint = juce::PathStrokeType::curved;  break;
		default:                       joint = juce::PathStrokeType::mitered; break;
		}

		switch (properties.lineCap)
		{
		case drawing::CapStyle::Square: cap = juce::PathStrokeType::square;  break;
		case drawing::CapStyle::Round:  cap = juce::PathStrokeType::rounded; break;
		default:                        cap = juce::PathStrokeType::butt;    break;
		}
	}

	return { strokeWidth, joint, cap };
}

// Direct2D standard dash patterns, in multiples of stroke width.
// Zero-length dashes (dots) are given a tiny length so JUCE renders them (as dots, with round/square caps).
inline std::vector<float> dashPattern(const drawing::api::IStrokeStyle* strokeStyle, float strokeWidth)
{
	std::vector<float> result;

	if (!strokeStyle)
		return result;

	auto style = static_cast<const StrokeStyle*>(strokeStyle);
	const auto w = (std::max)(strokeWidth, 0.01f);
	const auto dot = 0.01f * w;

	switch (style->strokeStyleProperties.dashStyle)
	{
	case drawing::DashStyle::Dash:       result = { 2 * w, 2 * w }; break;
	case drawing::DashStyle::Dot:        result = { dot, 2 * w }; break;
	case drawing::DashStyle::DashDot:    result = { 2 * w, 2 * w, dot, 2 * w }; break;
	case drawing::DashStyle::DashDotDot: result = { 2 * w, 2 * w, dot, 2 * w, dot, 2 * w }; break;

	case drawing::DashStyle::Custom:
		for (auto d : style->dashes)
			result.push_back((std::max)(d * w, dot));
		break;

	default: // Solid
		break;
	}

	return result;
}

class Factory;

// GEOMETRY

class PathGeometry final : public drawing::api::IPathGeometry
{
	friend class GraphicsContextBase;
	friend class GeometrySink;

	drawing::api::IFactory* factory{};
	juce::Path path;

public:
	PathGeometry(drawing::api::IFactory* pfactory) : factory(pfactory)
	{
		path.setUsingNonZeroWinding(false); // Direct2D default fill mode is 'Alternate' (even-odd).
	}

	ReturnCode open(drawing::api::IGeometrySink** returnGeometrySink) override;

	ReturnCode getFactory(drawing::api::IFactory** returnFactory) override
	{
		*returnFactory = factory;
		return ReturnCode::Ok;
	}

	ReturnCode strokeContainsPoint(drawing::Point point, float strokeWidth, drawing::api::IStrokeStyle* strokeStyle, const drawing::Matrix3x2* worldTransform, bool* returnContains) override
	{
		auto local = point;
		if (worldTransform)
			local = drawing::transformPoint(drawing::invert(*worldTransform), point);

		juce::Path stroked;
		toJuce(strokeWidth, strokeStyle).createStrokedPath(stroked, path);

		*returnContains = stroked.contains(toJuce(local));
		return ReturnCode::Ok;
	}

	ReturnCode fillContainsPoint(drawing::Point point, const drawing::Matrix3x2* worldTransform, bool* returnContains) override
	{
		auto local = point;
		if (worldTransform)
			local = drawing::transformPoint(drawing::invert(*worldTransform), point);

		*returnContains = path.contains(toJuce(local));
		return ReturnCode::Ok;
	}

	ReturnCode getWidenedBounds(float strokeWidth, drawing::api::IStrokeStyle* strokeStyle, const drawing::Matrix3x2* worldTransform, drawing::Rect* returnBounds) override
	{
		juce::Path stroked;
		toJuce(strokeWidth, strokeStyle).createStrokedPath(stroked, path);

		if (worldTransform)
			stroked.applyTransform(toJuce(*worldTransform));

		const auto bounds = stroked.getBounds();
		*returnBounds = { bounds.getX(), bounds.getY(), bounds.getRight(), bounds.getBottom() };
		return ReturnCode::Ok;
	}

	GMPI_QUERYINTERFACE_METHOD(drawing::api::IPathGeometry);
	GMPI_REFCOUNT
};

// Writes into the owning PathGeometry's juce::Path.
// Arcs (and bulk operations) fall back to the generic implementation, which
// decomposes them into the cubic beziers handled below.
class GeometrySink final : public se::generic_graphics::GeometrySink
{
	PathGeometry* geometry{};

	juce::Path& path()
	{
		return geometry->path;
	}

public:
	GeometrySink(PathGeometry* pgeometry) : geometry(pgeometry)
	{
		geometry->addRef();
	}

	~GeometrySink()
	{
		geometry->release();
	}

	void beginFigure(drawing::Point pStartPoint, drawing::FigureBegin figureBegin) override
	{
		se::generic_graphics::GeometrySink::beginFigure(pStartPoint, figureBegin);
		path().startNewSubPath(pStartPoint.x, pStartPoint.y);
	}

	void endFigure(drawing::FigureEnd figureEnd) override
	{
		if (figureEnd == drawing::FigureEnd::Closed)
		{
			path().closeSubPath();
			lastPoint = startPoint;
		}
	}

	void addLine(drawing::Point point) override
	{
		path().lineTo(point.x, point.y);
		lastPoint = point;
	}

	void addBezier(const drawing::BezierSegment* bezier) override
	{
		path().cubicTo(
			bezier->point1.x, bezier->point1.y,
			bezier->point2.x, bezier->point2.y,
			bezier->point3.x, bezier->point3.y);
		lastPoint = bezier->point3;
	}

	void addQuadraticBezier(const drawing::QuadraticBezierSegment* bezier) override
	{
		path().quadraticTo(bezier->point1.x, bezier->point1.y, bezier->point2.x, bezier->point2.y);
		lastPoint = bezier->point2;
	}

	void setFillMode(drawing::FillMode fillMode) override
	{
		path().setUsingNonZeroWinding(fillMode == drawing::FillMode::Winding);
	}
};

inline ReturnCode PathGeometry::open(drawing::api::IGeometrySink** returnGeometrySink)
{
	*returnGeometrySink = {};
	path.clear();
	path.setUsingNonZeroWinding(false);

	gmpi::shared_ptr<gmpi::api::IUnknown> sink;
	sink.attach(new GeometrySink(this));
	return sink->queryInterface(&drawing::api::IGeometrySink::guid, reinterpret_cast<void**>(returnGeometrySink));
}

// TEXT

// Measures the extent of a glyph above the baseline (e.g. 'H' => cap height, 'x' => x-height).
inline float measureGlyphTop(const juce::Font& font, const char* utf8Glyph)
{
	juce::GlyphArrangement glyphs;
	glyphs.addLineOfText(font, juce::String::fromUTF8(utf8Glyph), 0.0f, 0.0f);

	juce::Path outline;
	glyphs.createPath(outline);

	const auto bounds = outline.getBounds();
	return bounds.isEmpty() ? font.getAscent() : -bounds.getY();
}

// Width of a line of text, including trailing whitespace (matches DirectWrite's widthIncludingTrailingWhitespace).
inline float measureStringWidth(const juce::Font& font, const juce::String& text)
{
	if (text.isEmpty())
		return 0.0f;

	juce::GlyphArrangement glyphs;
	glyphs.addLineOfText(font, text, 0.0f, 0.0f);

	const auto glyphCount = glyphs.getNumGlyphs();
	return glyphCount > 0 ? glyphs.getGlyph(glyphCount - 1).getRight() : 0.0f;
}

class TextFormat final : public drawing::api::ITextFormat
{
public:
	juce::Font font;
	drawing::TextAlignment textAlignment = drawing::TextAlignment::Leading;
	drawing::ParagraphAlignment paragraphAlignment = drawing::ParagraphAlignment::Near;
	drawing::WordWrapping wordWrapping = drawing::WordWrapping::Wrap;
	float lineSpacing = -1.0f; // negative: natural spacing from the font.
	float baseline = 0.0f;     // used only with uniform (non-negative) lineSpacing.

	TextFormat(const juce::Font& pfont) : font(pfont) {}

	float getLineHeight() const
	{
		return lineSpacing >= 0.0f ? lineSpacing : font.getHeight();
	}

	float getBaselineOffset() const
	{
		return lineSpacing >= 0.0f ? baseline : font.getAscent();
	}

	struct Line
	{
		juce::String text;
		float width{};
	};

	void layoutText(const juce::String& text, float maxWidth, std::vector<Line>& returnLines) const
	{
		const bool wrap = wordWrapping == drawing::WordWrapping::Wrap && maxWidth > 0.0f;

		juce::StringArray paragraphs;
		paragraphs.addLines(text); // splits on any line-break style, preserving empty lines.

		for (const auto& paragraph : paragraphs)
		{
			if (!wrap || paragraph.isEmpty())
			{
				returnLines.push_back({ paragraph, measureStringWidth(font, paragraph) });
				continue;
			}

			// greedy word-wrap. break decisions ignore trailing whitespace, like DirectWrite.
			juce::String line;
			for (int pos = 0; pos < paragraph.length();)
			{
				int wordEnd = paragraph.indexOfChar(pos, ' ');
				wordEnd = wordEnd < 0 ? paragraph.length() : wordEnd + 1; // keep the delimiter with the word.

				const auto candidate = line + paragraph.substring(pos, wordEnd);

				if (line.isNotEmpty() && measureStringWidth(font, candidate.trimEnd()) > maxWidth)
				{
					returnLines.push_back({ line, measureStringWidth(font, line) });
					line.clear();
					continue; // retry the same word on a fresh line.
				}

				line = candidate;
				pos = wordEnd;

				// hard-break single words wider than the layout box.
				while (measureStringWidth(font, line.trimEnd()) > maxWidth && line.trimEnd().length() > 1)
				{
					auto fitChars = line.length() - 1;
					while (fitChars > 1 && measureStringWidth(font, line.substring(0, fitChars)) > maxWidth)
						--fitChars;

					returnLines.push_back({ line.substring(0, fitChars), measureStringWidth(font, line.substring(0, fitChars)) });
					line = line.substring(fitChars);
				}
			}

			returnLines.push_back({ line, measureStringWidth(font, line) });
		}
	}

	ReturnCode setTextAlignment(drawing::TextAlignment ptextAlignment) override
	{
		textAlignment = ptextAlignment;
		return ReturnCode::Ok;
	}

	ReturnCode setParagraphAlignment(drawing::ParagraphAlignment pparagraphAlignment) override
	{
		paragraphAlignment = pparagraphAlignment;
		return ReturnCode::Ok;
	}

	ReturnCode setWordWrapping(drawing::WordWrapping pwordWrapping) override
	{
		wordWrapping = pwordWrapping;
		return ReturnCode::Ok;
	}

	ReturnCode getTextExtentU(const char* utf8String, int32_t stringLength, float maxWidth, drawing::Size* returnSize) override
	{
		const auto text = juce::String::fromUTF8(utf8String, stringLength);

		std::vector<Line> lines;
		layoutText(text, maxWidth, lines);

		float width{};
		for (const auto& line : lines)
			width = (std::max)(width, line.width);

		*returnSize = { width, static_cast<float>(lines.size()) * getLineHeight() };
		return ReturnCode::Ok;
	}

	ReturnCode getFontMetrics(drawing::FontMetrics* returnFontMetrics) override
	{
		const auto emSize = font.getHeight() * font.getHeightToPointsFactor();

		drawing::FontMetrics metrics{};
		metrics.ascent = font.getAscent();
		metrics.descent = font.getDescent();
		metrics.lineGap = 0.0f; // JUCE folds any line gap into its font height.
		metrics.capHeight = measureGlyphTop(font, "H");
		metrics.xHeight = measureGlyphTop(font, "x");
		metrics.underlinePosition = -0.1f * emSize;
		metrics.underlineThickness = 0.05f * emSize;
		metrics.strikethroughPosition = 0.5f * metrics.xHeight;
		metrics.strikethroughThickness = metrics.underlineThickness;

		*returnFontMetrics = metrics;
		return ReturnCode::Ok;
	}

	ReturnCode setLineSpacing(float plineSpacing, float pbaseline) override
	{
		lineSpacing = plineSpacing;
		baseline = pbaseline;
		return ReturnCode::Ok;
	}

	GMPI_QUERYINTERFACE_METHOD(drawing::api::ITextFormat);
	GMPI_REFCOUNT
};

// BITMAPS

class BitmapPixels final : public drawing::api::IBitmapPixels
{
	std::unique_ptr<juce::Image::BitmapData> bitmapData;
	int32_t pixelFormat = BGRA_sRGB_8i;

public:
	BitmapPixels(juce::Image& image, int32_t flags)
	{
		const auto mode =
			flags == static_cast<int32_t>(drawing::BitmapLockFlags::Read) ? juce::Image::BitmapData::readOnly :
			flags == static_cast<int32_t>(drawing::BitmapLockFlags::Write) ? juce::Image::BitmapData::writeOnly :
			juce::Image::BitmapData::readWrite;

		bitmapData = std::make_unique<juce::Image::BitmapData>(image, mode);

		// JUCE software images are premultiplied BGRA (little-endian ARGB), same as the Direct2D backend exposes.
		if (image.getFormat() == juce::Image::SingleChannel)
			pixelFormat = Alpha_8i;
	}

	ReturnCode getAddress(uint8_t** returnAddress) override
	{
		*returnAddress = bitmapData->data;
		return ReturnCode::Ok;
	}

	ReturnCode getBytesPerRow(int32_t* returnBytesPerRow) override
	{
		*returnBytesPerRow = bitmapData->lineStride;
		return ReturnCode::Ok;
	}

	ReturnCode getPixelFormat(int32_t* returnPixelFormat) override
	{
		*returnPixelFormat = pixelFormat;
		return ReturnCode::Ok;
	}

	GMPI_QUERYINTERFACE_METHOD(drawing::api::IBitmapPixels);
	GMPI_REFCOUNT
};

class Bitmap final : public drawing::api::IBitmap
{
public:
	juce::Image image; // shared reference (juce::Image is reference-counted).
	drawing::api::IFactory* factory{};

	Bitmap(drawing::api::IFactory* pfactory, juce::Image pimage) : image(pimage), factory(pfactory) {}

	ReturnCode getSizeU(drawing::SizeU* returnSize) override
	{
		*returnSize = { static_cast<uint32_t>(image.getWidth()), static_cast<uint32_t>(image.getHeight()) };
		return ReturnCode::Ok;
	}

	ReturnCode lockPixels(drawing::api::IBitmapPixels** returnPixels, int32_t flags) override
	{
		*returnPixels = {};

		if (image.isNull())
			return ReturnCode::Fail;

		gmpi::shared_ptr<gmpi::api::IUnknown> pixels;
		pixels.attach(new BitmapPixels(image, flags));
		return pixels->queryInterface(&drawing::api::IBitmapPixels::guid, reinterpret_cast<void**>(returnPixels));
	}

	ReturnCode getFactory(drawing::api::IFactory** returnFactory) override
	{
		*returnFactory = factory;
		return ReturnCode::Ok;
	}

	GMPI_QUERYINTERFACE_METHOD(drawing::api::IBitmap);
	GMPI_REFCOUNT
};

// BRUSHES

// Common ancestor of all brushes: converts the brush to a juce::FillType at draw time.
class JuceBrushBase
{
public:
	drawing::api::IFactory* factory{};
	float opacity = 1.0f;
	juce::AffineTransform brushTransform;

	JuceBrushBase(drawing::api::IFactory* pfactory, const drawing::BrushProperties* brushProperties) : factory(pfactory)
	{
		if (brushProperties)
		{
			opacity = brushProperties->opacity;
			brushTransform = toJuce(brushProperties->transform);
		}
	}

	virtual ~JuceBrushBase() {}

	virtual juce::FillType getFillType() const = 0;
};

class SolidColorBrush final : public drawing::api::ISolidColorBrush, public JuceBrushBase
{
	drawing::Color color;

public:
	SolidColorBrush(drawing::api::IFactory* pfactory, const drawing::Color* pcolor, const drawing::BrushProperties* brushProperties) :
		JuceBrushBase(pfactory, brushProperties)
		, color(*pcolor)
	{}

	juce::FillType getFillType() const override
	{
		juce::FillType fill(toJuce(color));

		if (opacity != 1.0f)
			fill.setOpacity(opacity * fill.getOpacity());

		return fill;
	}

	void setColor(const drawing::Color* pcolor) override
	{
		color = *pcolor;
	}

	ReturnCode getFactory(drawing::api::IFactory** returnFactory) override
	{
		*returnFactory = factory;
		return ReturnCode::Ok;
	}

	GMPI_QUERYINTERFACE_METHOD(drawing::api::ISolidColorBrush);
	GMPI_REFCOUNT
};

class GradientstopCollection final : public drawing::api::IGradientstopCollection
{
public:
	std::vector<drawing::Gradientstop> gradientstops;
	drawing::api::IFactory* factory{};

	GradientstopCollection(drawing::api::IFactory* pfactory, const drawing::Gradientstop* pgradientstops, uint32_t gradientstopsCount) : factory(pfactory)
	{
		gradientstops.assign(pgradientstops, pgradientstops + gradientstopsCount);
	}

	ReturnCode getFactory(drawing::api::IFactory** returnFactory) override
	{
		*returnFactory = factory;
		return ReturnCode::Ok;
	}

	GMPI_QUERYINTERFACE_METHOD(drawing::api::IGradientstopCollection);
	GMPI_REFCOUNT
};

inline void addStopsToGradient(juce::ColourGradient& gradient, const std::vector<drawing::Gradientstop>& stops)
{
	for (const auto& stop : stops)
		gradient.addColour(std::clamp(stop.position, 0.0f, 1.0f), toJuce(stop.color));

	// juce::ColourGradient requires at least two stops.
	if (gradient.getNumColours() == 0)
		gradient.addColour(0.0, juce::Colours::transparentBlack);
	if (gradient.getNumColours() == 1)
		gradient.addColour(gradient.getColourPosition(0) < 1.0 ? 1.0 : 0.0, gradient.getColour(0));
}

class LinearGradientBrush final : public drawing::api::ILinearGradientBrush, public JuceBrushBase
{
	drawing::Point startPoint;
	drawing::Point endPoint;
	std::vector<drawing::Gradientstop> stops;

public:
	LinearGradientBrush(
		drawing::api::IFactory* pfactory,
		const drawing::LinearGradientBrushProperties* linearGradientBrushProperties,
		const drawing::BrushProperties* brushProperties,
		const drawing::api::IGradientstopCollection* gradientstopCollection
	) :
		JuceBrushBase(pfactory, brushProperties)
		, startPoint(linearGradientBrushProperties->startPoint)
		, endPoint(linearGradientBrushProperties->endPoint)
		, stops(static_cast<const GradientstopCollection*>(gradientstopCollection)->gradientstops)
	{}

	juce::FillType getFillType() const override
	{
		juce::ColourGradient gradient;
		gradient.point1 = toJuce(startPoint);
		gradient.point2 = toJuce(endPoint);
		gradient.isRadial = false;
		addStopsToGradient(gradient, stops);

		juce::FillType fill(gradient);
		fill.transform = brushTransform;
		fill.setOpacity(opacity);
		return fill;
	}

	void setStartPoint(drawing::Point pstartPoint) override { startPoint = pstartPoint; }
	void setEndPoint(drawing::Point pendPoint) override { endPoint = pendPoint; }

	ReturnCode getFactory(drawing::api::IFactory** returnFactory) override
	{
		*returnFactory = factory;
		return ReturnCode::Ok;
	}

	GMPI_QUERYINTERFACE_METHOD(drawing::api::ILinearGradientBrush);
	GMPI_REFCOUNT
};

class RadialGradientBrush final : public drawing::api::IRadialGradientBrush, public JuceBrushBase
{
	drawing::Point center;
	drawing::Point gradientOriginOffset; // not supported by JUCE gradients; ignored.
	float radiusX{};
	float radiusY{};
	std::vector<drawing::Gradientstop> stops;

public:
	RadialGradientBrush(
		drawing::api::IFactory* pfactory,
		const drawing::RadialGradientBrushProperties* radialGradientBrushProperties,
		const drawing::BrushProperties* brushProperties,
		const drawing::api::IGradientstopCollection* gradientstopCollection
	) :
		JuceBrushBase(pfactory, brushProperties)
		, center(radialGradientBrushProperties->center)
		, gradientOriginOffset(radialGradientBrushProperties->gradientOriginOffset)
		, radiusX(radialGradientBrushProperties->radiusX)
		, radiusY(radialGradientBrushProperties->radiusY)
		, stops(static_cast<const GradientstopCollection*>(gradientstopCollection)->gradientstops)
	{}

	juce::FillType getFillType() const override
	{
		juce::ColourGradient gradient;
		gradient.point1 = toJuce(center);
		gradient.point2 = { center.x + (std::max)(radiusX, 0.001f), center.y };
		gradient.isRadial = true;
		addStopsToGradient(gradient, stops);

		juce::FillType fill(gradient);
		fill.transform = brushTransform;

		// elliptical gradients: squash the circular gradient about its center.
		if (radiusX > 0.0f && radiusY > 0.0f && radiusX != radiusY)
			fill.transform = juce::AffineTransform::scale(1.0f, radiusY / radiusX, center.x, center.y).followedBy(fill.transform);

		fill.setOpacity(opacity);
		return fill;
	}

	void setCenter(drawing::Point pcenter) override { center = pcenter; }
	void setGradientOriginOffset(drawing::Point pgradientOriginOffset) override { gradientOriginOffset = pgradientOriginOffset; }
	void setRadiusX(float pradiusX) override { radiusX = pradiusX; }
	void setRadiusY(float pradiusY) override { radiusY = pradiusY; }

	ReturnCode getFactory(drawing::api::IFactory** returnFactory) override
	{
		*returnFactory = factory;
		return ReturnCode::Ok;
	}

	GMPI_QUERYINTERFACE_METHOD(drawing::api::IRadialGradientBrush);
	GMPI_REFCOUNT
};

class BitmapBrush final : public drawing::api::IBitmapBrush, public JuceBrushBase
{
	juce::Image image;

public:
	BitmapBrush(
		drawing::api::IFactory* pfactory,
		const drawing::api::IBitmap* bitmap,
		const drawing::BrushProperties* brushProperties
	) :
		JuceBrushBase(pfactory, brushProperties)
		, image(static_cast<const Bitmap*>(bitmap)->image)
	{}

	juce::FillType getFillType() const override
	{
		juce::FillType fill(image, brushTransform); // image fills tile, matching Direct2D EXTEND_MODE_WRAP.
		fill.setOpacity(opacity);
		return fill;
	}

	ReturnCode getFactory(drawing::api::IFactory** returnFactory) override
	{
		*returnFactory = factory;
		return ReturnCode::Ok;
	}

	GMPI_QUERYINTERFACE_METHOD(drawing::api::IBitmapBrush);
	GMPI_REFCOUNT
};

// DEVICE CONTEXT

// NOTE: this class must introduce no virtual functions beyond those of IDeviceContext,
// so that BitmapRenderTarget::getBitmap() lands in the vtable slot where
// IBitmapRenderTarget expects it (same emulation as the DirectX and Cocoa backends).
class GraphicsContextBase : public drawing::api::IDeviceContext
{
protected:
	juce::Graphics* g{};
	drawing::api::IFactory* factory{};
	drawing::Matrix3x2 currentTransform;

	struct ClipEntry
	{
		juce::Path devicePath;        // in the coordinate space the juce::Graphics arrived with.
		drawing::Rect deviceBounds;
	};
	std::vector<ClipEntry> clipStack;

	GraphicsContextBase(drawing::api::IFactory* pfactory) : factory(pfactory)
	{
		constexpr float defaultClipBounds = 100000.0f;
		clipStack.push_back({ {}, { -defaultClipBounds, -defaultClipBounds, defaultClipBounds, defaultClipBounds } });
	}

	// non-virtual. derived classes own their own cleanup.
	~GraphicsContextBase() {}

	void init(juce::Graphics* pg)
	{
		g = pg;
		g->saveState();
	}

	void deinit()
	{
		g->restoreState();
	}

	// re-establish the JUCE graphics state from scratch: base state, then clips (in device space), then the transform.
	// juce::Graphics has no absolute setTransform or clip-pop, so state changes rebuild from the saved base state.
	void rebuildState()
	{
		g->restoreState();
		g->saveState();

		for (size_t i = 1; i < clipStack.size(); ++i) // entry 0 is the implicit 'infinite' clip.
			g->reduceClipRegion(clipStack[i].devicePath);

		g->addTransform(toJuce(currentTransform));
	}

	const JuceBrushBase* toFill(drawing::api::IBrush* brush)
	{
		auto result = dynamic_cast<const JuceBrushBase*>(brush);

		if (result)
			g->setFillType(result->getFillType());

		return result;
	}

	void strokePathCommon(const juce::Path& path, drawing::api::IBrush* brush, float strokeWidth, drawing::api::IStrokeStyle* strokeStyle)
	{
		if (!toFill(brush))
			return;

		const auto strokeType = toJuce(strokeWidth, strokeStyle);
		const auto dashes = dashPattern(strokeStyle, strokeWidth);

		if (dashes.empty())
		{
			g->strokePath(path, strokeType);
		}
		else
		{
			juce::Path dashed;
			strokeType.createDashedStroke(dashed, path, dashes.data(), static_cast<int>(dashes.size()));
			g->fillPath(dashed);
		}
	}

public:
	ReturnCode getFactory(drawing::api::IFactory** returnFactory) override
	{
		*returnFactory = factory;
		return ReturnCode::Ok;
	}

	ReturnCode createBitmapBrush(drawing::api::IBitmap* bitmap, const drawing::BrushProperties* brushProperties, drawing::api::IBitmapBrush** returnBitmapBrush) override
	{
		*returnBitmapBrush = {};
		gmpi::shared_ptr<gmpi::api::IUnknown> brush;
		brush.attach(new BitmapBrush(factory, bitmap, brushProperties));
		return brush->queryInterface(&drawing::api::IBitmapBrush::guid, reinterpret_cast<void**>(returnBitmapBrush));
	}

	ReturnCode createSolidColorBrush(const drawing::Color* color, const drawing::BrushProperties* brushProperties, drawing::api::ISolidColorBrush** returnSolidColorBrush) override
	{
		*returnSolidColorBrush = {};
		gmpi::shared_ptr<gmpi::api::IUnknown> brush;
		brush.attach(new SolidColorBrush(factory, color, brushProperties));
		return brush->queryInterface(&drawing::api::ISolidColorBrush::guid, reinterpret_cast<void**>(returnSolidColorBrush));
	}

	ReturnCode createGradientstopCollection(const drawing::Gradientstop* gradientstops, uint32_t gradientstopsCount, drawing::ExtendMode extendMode, drawing::api::IGradientstopCollection** returnGradientstopCollection) override
	{
		*returnGradientstopCollection = {};
		gmpi::shared_ptr<gmpi::api::IUnknown> collection;
		collection.attach(new GradientstopCollection(factory, gradientstops, gradientstopsCount));
		return collection->queryInterface(&drawing::api::IGradientstopCollection::guid, reinterpret_cast<void**>(returnGradientstopCollection));
	}

	ReturnCode createLinearGradientBrush(const drawing::LinearGradientBrushProperties* linearGradientBrushProperties, const drawing::BrushProperties* brushProperties, drawing::api::IGradientstopCollection* gradientstopCollection, drawing::api::ILinearGradientBrush** returnLinearGradientBrush) override
	{
		*returnLinearGradientBrush = {};
		gmpi::shared_ptr<gmpi::api::IUnknown> brush;
		brush.attach(new LinearGradientBrush(factory, linearGradientBrushProperties, brushProperties, gradientstopCollection));
		return brush->queryInterface(&drawing::api::ILinearGradientBrush::guid, reinterpret_cast<void**>(returnLinearGradientBrush));
	}

	ReturnCode createRadialGradientBrush(const drawing::RadialGradientBrushProperties* radialGradientBrushProperties, const drawing::BrushProperties* brushProperties, drawing::api::IGradientstopCollection* gradientstopCollection, drawing::api::IRadialGradientBrush** returnRadialGradientBrush) override
	{
		*returnRadialGradientBrush = {};
		gmpi::shared_ptr<gmpi::api::IUnknown> brush;
		brush.attach(new RadialGradientBrush(factory, radialGradientBrushProperties, brushProperties, gradientstopCollection));
		return brush->queryInterface(&drawing::api::IRadialGradientBrush::guid, reinterpret_cast<void**>(returnRadialGradientBrush));
	}

	ReturnCode drawLine(drawing::Point point0, drawing::Point point1, drawing::api::IBrush* brush, float strokeWidth, drawing::api::IStrokeStyle* strokeStyle) override
	{
		juce::Path path;
		path.startNewSubPath(toJuce(point0));
		path.lineTo(toJuce(point1));
		strokePathCommon(path, brush, strokeWidth, strokeStyle);
		return ReturnCode::Ok;
	}

	ReturnCode drawRectangle(const drawing::Rect* rect, drawing::api::IBrush* brush, float strokeWidth, drawing::api::IStrokeStyle* strokeStyle) override
	{
		// note: juce::Graphics::drawRect strokes inside the rectangle; gmpi/Direct2D center the stroke on the boundary.
		juce::Path path;
		path.addRectangle(toJuce(*rect));
		strokePathCommon(path, brush, strokeWidth, strokeStyle);
		return ReturnCode::Ok;
	}

	ReturnCode fillRectangle(const drawing::Rect* rect, drawing::api::IBrush* brush) override
	{
		if (toFill(brush))
			g->fillRect(toJuce(*rect));

		return ReturnCode::Ok;
	}

	ReturnCode drawRoundedRectangle(const drawing::RoundedRect* roundedRect, drawing::api::IBrush* brush, float strokeWidth, drawing::api::IStrokeStyle* strokeStyle) override
	{
		strokePathCommon(makeRoundedRectPath(*roundedRect), brush, strokeWidth, strokeStyle);
		return ReturnCode::Ok;
	}

	ReturnCode fillRoundedRectangle(const drawing::RoundedRect* roundedRect, drawing::api::IBrush* brush) override
	{
		if (toFill(brush))
			g->fillPath(makeRoundedRectPath(*roundedRect));

		return ReturnCode::Ok;
	}

	ReturnCode drawEllipse(const drawing::Ellipse* ellipse, drawing::api::IBrush* brush, float strokeWidth, drawing::api::IStrokeStyle* strokeStyle) override
	{
		juce::Path path;
		path.addEllipse(makeEllipseRect(*ellipse));
		strokePathCommon(path, brush, strokeWidth, strokeStyle);
		return ReturnCode::Ok;
	}

	ReturnCode fillEllipse(const drawing::Ellipse* ellipse, drawing::api::IBrush* brush) override
	{
		if (toFill(brush))
			g->fillEllipse(makeEllipseRect(*ellipse));

		return ReturnCode::Ok;
	}

	ReturnCode drawGeometry(drawing::api::IPathGeometry* pathGeometry, drawing::api::IBrush* brush, float strokeWidth, drawing::api::IStrokeStyle* strokeStyle) override
	{
		if (auto geometry = dynamic_cast<PathGeometry*>(pathGeometry); geometry)
			strokePathCommon(geometry->path, brush, strokeWidth, strokeStyle);

		return ReturnCode::Ok;
	}

	ReturnCode fillGeometry(drawing::api::IPathGeometry* pathGeometry, drawing::api::IBrush* brush, drawing::api::IBrush* opacityBrush) override
	{
		// opacityBrush not supported by this backend.
		auto geometry = dynamic_cast<PathGeometry*>(pathGeometry);

		if (geometry && toFill(brush))
			g->fillPath(geometry->path);

		return ReturnCode::Ok;
	}

	ReturnCode drawBitmap(drawing::api::IBitmap* bitmap, const drawing::Rect* destinationRectangle, float opacity, drawing::BitmapInterpolationMode interpolationMode, const drawing::Rect* sourceRectangle) override
	{
		auto bm = dynamic_cast<Bitmap*>(bitmap);
		if (!bm || bm->image.isNull())
			return ReturnCode::Fail;

		const drawing::Rect source = sourceRectangle ? *sourceRectangle
			: drawing::Rect{ 0.0f, 0.0f, static_cast<float>(bm->image.getWidth()), static_cast<float>(bm->image.getHeight()) };

		const drawing::Rect dest = destinationRectangle ? *destinationRectangle
			: drawing::Rect{ 0.0f, 0.0f, source.right - source.left, source.bottom - source.top };

		const auto sourceWidth = source.right - source.left;
		const auto sourceHeight = source.bottom - source.top;

		if (sourceWidth <= 0.0f || sourceHeight <= 0.0f)
			return ReturnCode::Ok;

		juce::Graphics::ScopedSaveState save(*g);

		juce::Path clip;
		clip.addRectangle(toJuce(dest));
		g->reduceClipRegion(clip);

		g->setImageResamplingQuality(interpolationMode == drawing::BitmapInterpolationMode::NearestNeighbor
			? juce::Graphics::lowResamplingQuality
			: juce::Graphics::mediumResamplingQuality);

		g->setOpacity(opacity);

		const auto transform = juce::AffineTransform::translation(-source.left, -source.top)
			.scaled((dest.right - dest.left) / sourceWidth, (dest.bottom - dest.top) / sourceHeight)
			.translated(dest.left, dest.top);

		g->drawImageTransformed(bm->image, transform);
		return ReturnCode::Ok;
	}

	ReturnCode drawTextU(const char* string, uint32_t stringLength, drawing::api::ITextFormat* textFormat, const drawing::Rect* layoutRect, drawing::api::IBrush* defaultForegroundBrush, int32_t options) override
	{
		auto format = dynamic_cast<TextFormat*>(textFormat);
		if (!format || !toFill(defaultForegroundBrush))
			return ReturnCode::Fail;

		const auto text = juce::String::fromUTF8(string, static_cast<int>(stringLength));
		const auto layoutWidth = layoutRect->right - layoutRect->left;

		std::vector<TextFormat::Line> lines;
		format->layoutText(text, layoutWidth, lines);

		if (lines.empty())
			return ReturnCode::Ok;

		const auto lineHeight = format->getLineHeight();
		const auto totalHeight = static_cast<float>(lines.size()) * lineHeight;

		float top = layoutRect->top;
		switch (format->paragraphAlignment)
		{
		case drawing::ParagraphAlignment::Center:
			top += ((layoutRect->bottom - layoutRect->top) - totalHeight) * 0.5f;
			break;
		case drawing::ParagraphAlignment::Far:
			top = layoutRect->bottom - totalHeight;
			break;
		default: // Near
			break;
		}

		juce::GlyphArrangement glyphs;
		float baselineY = top + format->getBaselineOffset();

		for (const auto& line : lines)
		{
			float x = layoutRect->left;
			switch (format->textAlignment)
			{
			case drawing::TextAlignment::Center:
				x += (layoutWidth - line.width) * 0.5f;
				break;
			case drawing::TextAlignment::Trailing:
				x = layoutRect->right - line.width;
				break;
			default: // Leading
				break;
			}

			if (line.text.isNotEmpty())
				glyphs.addLineOfText(format->font, line.text, x, baselineY);

			baselineY += lineHeight;
		}

		if ((options & drawing::DrawTextOptions::Clip) != 0)
		{
			juce::Graphics::ScopedSaveState save(*g);
			juce::Path clip;
			clip.addRectangle(toJuce(*layoutRect));
			g->reduceClipRegion(clip);
			glyphs.draw(*g);
		}
		else
		{
			glyphs.draw(*g);
		}

		return ReturnCode::Ok;
	}

	ReturnCode drawRichTextU(drawing::api::IRichTextFormat* richTextFormat, const drawing::Rect* layoutRect, drawing::api::IBrush* defaultForegroundBrush, int32_t options) override
	{
		return ReturnCode::NoSupport;
	}

	ReturnCode setTransform(const drawing::Matrix3x2* transform) override
	{
		currentTransform = *transform;
		rebuildState();
		return ReturnCode::Ok;
	}

	ReturnCode getTransform(drawing::Matrix3x2* returnTransform) override
	{
		*returnTransform = currentTransform;
		return ReturnCode::Ok;
	}

	ReturnCode pushAxisAlignedClip(const drawing::Rect* clipRect) override
	{
		juce::Path userPath;
		userPath.addRectangle(toJuce(*clipRect));

		auto devicePath = userPath;
		devicePath.applyTransform(toJuce(currentTransform));

		clipStack.push_back({ devicePath, drawing::transformRect(currentTransform, *clipRect) });

		// the current state already carries the transform, so clip with the untransformed rect.
		g->reduceClipRegion(userPath);
		return ReturnCode::Ok;
	}

	ReturnCode pushClipGeometry(drawing::api::IPathGeometry* geometry) override
	{
		auto pathGeometry = dynamic_cast<PathGeometry*>(geometry);
		if (!pathGeometry)
			return ReturnCode::Fail;

		auto devicePath = pathGeometry->path;
		devicePath.applyTransform(toJuce(currentTransform));

		const auto bounds = devicePath.getBounds();
		clipStack.push_back({ devicePath, { bounds.getX(), bounds.getY(), bounds.getRight(), bounds.getBottom() } });

		g->reduceClipRegion(pathGeometry->path);
		return ReturnCode::Ok;
	}

	ReturnCode popAxisAlignedClip() override
	{
		if (clipStack.size() > 1)
			clipStack.pop_back();

		rebuildState();
		return ReturnCode::Ok;
	}

	ReturnCode getAxisAlignedClip(drawing::Rect* returnClipRect) override
	{
		*returnClipRect = drawing::transformRect(drawing::invert(currentTransform), clipStack.back().deviceBounds);
		return ReturnCode::Ok;
	}

	ReturnCode clear(const drawing::Color* clearColor) override
	{
		g->fillAll(toJuce(*clearColor));
		return ReturnCode::Ok;
	}

	ReturnCode beginDraw() override
	{
		return ReturnCode::Ok;
	}

	ReturnCode endDraw() override
	{
		return ReturnCode::Ok;
	}

	ReturnCode createCompatibleRenderTarget(drawing::Size desiredSize, int32_t flags, drawing::api::IBitmapRenderTarget** returnBitmapRenderTarget) override;

	GMPI_QUERYINTERFACE_METHOD(drawing::api::IDeviceContext);

private:
	static juce::Rectangle<float> makeEllipseRect(const drawing::Ellipse& ellipse)
	{
		return {
			ellipse.point.x - ellipse.radiusX,
			ellipse.point.y - ellipse.radiusY,
			ellipse.radiusX * 2.0f,
			ellipse.radiusY * 2.0f
		};
	}

	static juce::Path makeRoundedRectPath(const drawing::RoundedRect& roundedRect)
	{
		juce::Path path;
		const auto r = toJuce(roundedRect.rect);

		// juce::Path only supports a single corner-size; use the average when radii differ.
		const auto cornerSize = (std::min)(
			(roundedRect.radiusX + roundedRect.radiusY) * 0.5f,
			(std::min)(r.getWidth(), r.getHeight()) * 0.5f);

		path.addRoundedRectangle(r, cornerSize);
		return path;
	}
};

// Renders to the juce::Graphics passed in, e.g. from juce::Component::paint().
class GraphicsContext final : public GraphicsContextBase
{
public:
	GraphicsContext(juce::Graphics& pg, drawing::api::IFactory* pfactory) : GraphicsContextBase(pfactory)
	{
		init(&pg);
	}

	~GraphicsContext()
	{
		deinit();
	}

	GMPI_REFCOUNT_NO_DELETE;
};

// Renders to an owned bitmap.
class BitmapRenderTarget final : public GraphicsContextBase // emulated by careful layout: public IBitmapRenderTarget
{
	juce::Image image;
	std::unique_ptr<juce::Graphics> ownedGraphics;

public:
	BitmapRenderTarget(drawing::SizeU size, int32_t flags, drawing::api::IFactory* pfactory) : GraphicsContextBase(pfactory)
	{
		const bool isMask = (flags & static_cast<int32_t>(drawing::BitmapRenderTargetFlags::Mask)) != 0;

		image = juce::Image(
			isMask ? juce::Image::SingleChannel : juce::Image::ARGB,
			(std::max)(1, static_cast<int>(size.width)),
			(std::max)(1, static_cast<int>(size.height)),
			true,
			juce::SoftwareImageType{});

		ownedGraphics = std::make_unique<juce::Graphics>(image);
		init(ownedGraphics.get());
	}

	// HACK, to be ABI compatible with IBitmapRenderTarget this virtual function needs to be
	// in the vtable right after all virtual functions of GraphicsContextBase.
	virtual ReturnCode getBitmap(drawing::api::IBitmap** returnBitmap)
	{
		*returnBitmap = {};
		gmpi::shared_ptr<gmpi::api::IUnknown> bitmap;
		bitmap.attach(new Bitmap(factory, image));
		return bitmap->queryInterface(&drawing::api::IBitmap::guid, reinterpret_cast<void**>(returnBitmap));
	}

	ReturnCode queryInterface(const gmpi::api::Guid* iid, void** returnInterface) override
	{
		*returnInterface = {};
		if (*iid == drawing::api::IBitmapRenderTarget::guid)
		{
			// non-standard. Forcing this class (which has the correct vtable) to pretend it's the emulated interface.
			*returnInterface = reinterpret_cast<drawing::api::IBitmapRenderTarget*>(this);
			addRef();
			return ReturnCode::Ok;
		}

		GMPI_QUERYINTERFACE(drawing::api::IDeviceContext);
		return ReturnCode::NoSupport;
	}

	GMPI_REFCOUNT
};

inline ReturnCode GraphicsContextBase::createCompatibleRenderTarget(drawing::Size desiredSize, int32_t flags, drawing::api::IBitmapRenderTarget** returnBitmapRenderTarget)
{
	*returnBitmapRenderTarget = {};

	const drawing::SizeU size{
		static_cast<uint32_t>(std::ceil(desiredSize.width)),
		static_cast<uint32_t>(std::ceil(desiredSize.height))
	};

	gmpi::shared_ptr<gmpi::api::IUnknown> target;
	target.attach(new BitmapRenderTarget(size, flags, factory));
	return target->queryInterface(&drawing::api::IBitmapRenderTarget::guid, reinterpret_cast<void**>(returnBitmapRenderTarget));
}

// FACTORY

class Factory : public drawing::api::IFactory
{
	juce::StringArray fontFamilyNames;

	const juce::StringArray& getFontFamilyNames()
	{
		if (fontFamilyNames.isEmpty())
			fontFamilyNames = juce::Font::findAllTypefaceNames();

		return fontFamilyNames;
	}

public:
	ReturnCode createPathGeometry(drawing::api::IPathGeometry** returnPathGeometry) override
	{
		*returnPathGeometry = {};
		gmpi::shared_ptr<gmpi::api::IUnknown> geometry;
		geometry.attach(new PathGeometry(this));
		return geometry->queryInterface(&drawing::api::IPathGeometry::guid, reinterpret_cast<void**>(returnPathGeometry));
	}

	ReturnCode createTextFormat(const char* fontFamilyName, drawing::FontWeight fontWeight, drawing::FontStyle fontStyle, drawing::FontStretch fontStretch, float fontHeight, int32_t fontFlags, drawing::api::ITextFormat** returnTextFormat) override
	{
		*returnTextFormat = {};

		auto family = juce::String::fromUTF8(fontFamilyName);

		if (family.compareIgnoreCase("system-ui") == 0)
		{
			family = juce::Font::getDefaultSansSerifFontName();
		}
		else
		{
			// fall back to the default font when the family isn't installed (like the other backends).
			bool found = false;
			for (const auto& name : getFontFamilyNames())
			{
				if (name.compareIgnoreCase(family) == 0)
				{
					family = name;
					found = true;
					break;
				}
			}

			if (!found)
				family = juce::Font::getDefaultSansSerifFontName();
		}

		int styleFlags = juce::Font::plain;
		if (static_cast<int32_t>(fontWeight) >= static_cast<int32_t>(drawing::FontWeight::SemiBold))
			styleFlags |= juce::Font::bold;
		if (fontStyle == drawing::FontStyle::Italic || fontStyle == drawing::FontStyle::Oblique)
			styleFlags |= juce::Font::italic;

		constexpr float referenceHeight = 100.0f;
		juce::Font font(juce::FontOptions{ family, referenceHeight, styleFlags });

		float juceHeight = fontHeight;
		if ((fontFlags & static_cast<int32_t>(drawing::FontFlags::CapHeight)) != 0)
		{
			// fontHeight specifies the cap-height.
			const auto referenceCapHeight = measureGlyphTop(font, "H");
			if (referenceCapHeight > 0.0f)
				juceHeight = referenceHeight * fontHeight / referenceCapHeight;
		}
		else
		{
			// fontHeight specifies the em-size (DirectWrite semantics); JUCE's font height is ascent + descent.
			const auto pointsFactor = font.getHeightToPointsFactor();
			if (pointsFactor > 0.0f)
				juceHeight = fontHeight / pointsFactor;
		}

		font = font.withHeight(juceHeight);

		// JUCE has no font-stretch support; approximate it with horizontal scaling.
		if (fontStretch != drawing::FontStretch::Normal && fontStretch != drawing::FontStretch::Undefined)
		{
			constexpr float stretchScales[] = { 1.0f, 0.5f, 0.625f, 0.75f, 0.875f, 1.0f, 1.125f, 1.25f, 1.5f, 2.0f };
			const auto index = static_cast<size_t>(fontStretch);
			if (index < std::size(stretchScales))
				font = font.withHorizontalScale(stretchScales[index]);
		}

		gmpi::shared_ptr<gmpi::api::IUnknown> textFormat;
		textFormat.attach(new TextFormat(font));
		return textFormat->queryInterface(&drawing::api::ITextFormat::guid, reinterpret_cast<void**>(returnTextFormat));
	}

	ReturnCode createImage(int32_t width, int32_t height, int32_t flags, drawing::api::IBitmap** returnBitmap) override
	{
		*returnBitmap = {};

		const bool isMask = (flags & static_cast<int32_t>(drawing::BitmapRenderTargetFlags::Mask)) != 0;

		juce::Image image(
			isMask ? juce::Image::SingleChannel : juce::Image::ARGB,
			(std::max)(1, width),
			(std::max)(1, height),
			true,
			juce::SoftwareImageType{});

		gmpi::shared_ptr<gmpi::api::IUnknown> bitmap;
		bitmap.attach(new Bitmap(this, image));
		return bitmap->queryInterface(&drawing::api::IBitmap::guid, reinterpret_cast<void**>(returnBitmap));
	}

	ReturnCode loadImageU(const char* uri, drawing::api::IBitmap** returnBitmap) override
	{
		*returnBitmap = {};

		const auto path = juce::String::fromUTF8(uri);
		const auto file = juce::File::isAbsolutePath(path)
			? juce::File(path)
			: juce::File::getCurrentWorkingDirectory().getChildFile(path);

		auto image = juce::ImageFileFormat::loadFrom(file);
		if (image.isNull())
			return ReturnCode::Fail;

		if (image.getFormat() != juce::Image::ARGB)
			image = image.convertedToFormat(juce::Image::ARGB);

		gmpi::shared_ptr<gmpi::api::IUnknown> bitmap;
		bitmap.attach(new Bitmap(this, image));
		return bitmap->queryInterface(&drawing::api::IBitmap::guid, reinterpret_cast<void**>(returnBitmap));
	}

	ReturnCode createStrokeStyle(const drawing::StrokeStyleProperties* strokeStyleProperties, const float* dashes, int32_t dashesCount, drawing::api::IStrokeStyle** returnStrokeStyle) override
	{
		*returnStrokeStyle = {};
		gmpi::shared_ptr<gmpi::api::IUnknown> strokeStyle;
		strokeStyle.attach(new StrokeStyle(this, strokeStyleProperties, dashes, dashesCount));
		return strokeStyle->queryInterface(&drawing::api::IStrokeStyle::guid, reinterpret_cast<void**>(returnStrokeStyle));
	}

	ReturnCode getFontFamilyName(int32_t fontIndex, gmpi::api::IString* returnName) override
	{
		const auto& names = getFontFamilyNames();

		if (fontIndex < 0 || fontIndex >= names.size())
			return ReturnCode::Fail;

		const auto utf8 = names[fontIndex].toStdString();
		return returnName->setData(utf8.data(), static_cast<int32_t>(utf8.size()));
	}

	ReturnCode getPlatformPixelFormat(int32_t* returnPixelFormat) override
	{
		*returnPixelFormat = drawing::api::IBitmapPixels::BGRA_sRGB_8i;
		return ReturnCode::Ok;
	}

	ReturnCode createRichTextFormat(const char* markdownText, float fontHeight, const char* fontFamilyName, int32_t fontFlags, drawing::TextAlignment textAlignment, drawing::ParagraphAlignment paragraphAlignment, drawing::WordWrapping wordWrapping, float lineSpacing, float baseline, drawing::api::IRichTextFormat** returnRichTextFormat) override
	{
		*returnRichTextFormat = {};
		return ReturnCode::NoSupport;
	}

	ReturnCode createCpuRenderTarget(drawing::SizeU size, int32_t flags, drawing::api::IBitmapRenderTarget** returnBitmapRenderTarget, float dpi = 96.0f) override
	{
		*returnBitmapRenderTarget = {};

		gmpi::shared_ptr<gmpi::api::IUnknown> target;
		target.attach(new BitmapRenderTarget(size, flags, this));
		return target->queryInterface(&drawing::api::IBitmapRenderTarget::guid, reinterpret_cast<void**>(returnBitmapRenderTarget));
	}

	GMPI_QUERYINTERFACE_METHOD(drawing::api::IFactory);
	GMPI_REFCOUNT_NO_DELETE;
};

} // namespace jucegfx
} // namespace gmpi
