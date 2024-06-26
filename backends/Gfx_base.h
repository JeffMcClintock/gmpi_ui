#pragma once

#define _USE_MATH_DEFINES
#include <math.h>
#include "Bezier.h"

#pragma warning(disable : 4100)

namespace gmpi
{
namespace generic_graphics
{
class Resource
{
protected:
	gmpi::drawing::api::IFactory* factory;

public:
	Resource(gmpi::drawing::api::IFactory* pfactory) :factory(pfactory)
	{}
};

class StrokeStyle : public Resource, public gmpi::drawing::api::IStrokeStyle
{
public:
	gmpi::drawing::StrokeStyleProperties strokeStyleProperties;
	std::vector<float> dashes;

	StrokeStyle(gmpi::drawing::api::IFactory* pfactory, const gmpi::drawing::StrokeStyleProperties* pstrokeStyleProperties, const float* pdashes, int32_t dashesCount) :
		Resource(pfactory)
		, strokeStyleProperties(*pstrokeStyleProperties)
	{
		for (int i = 0; i < dashesCount; ++i)
		{
			dashes.push_back(pdashes[i]);
		}
	}

	// IMpResource
	gmpi::ReturnCode getFactory(gmpi::drawing::api::IFactory** pfactory) override
	{
		*pfactory = factory;
		return gmpi::ReturnCode::Ok;
	}

	GMPI_QUERYINTERFACE_METHOD(drawing::api::IStrokeStyle);
	GMPI_REFCOUNT;
};

// Provide fallback implementations of Arcs, Quadratic beziers, and bulk operations.
class GeometrySink : public gmpi::drawing::api::IGeometrySink
{
protected:
	gmpi::drawing::Point startPoint;
	gmpi::drawing::Point lastPoint;

public:
	virtual ~GeometrySink() {}

	void beginFigure(gmpi::drawing::Point pStartPoint, gmpi::drawing::FigureBegin figureBegin) override
	{
		startPoint = lastPoint = pStartPoint;
	}

	gmpi::ReturnCode close() override
	{
		return gmpi::ReturnCode::Ok;
	}

	void addLines(const gmpi::drawing::Point* points, uint32_t pointsCount) override
	{
		for (uint32_t i = 0; i < pointsCount; ++i)
			addLine(points[i]);
	}

	void addBeziers(const gmpi::drawing::BezierSegment* beziers, uint32_t beziersCount) override
	{
		for (uint32_t i = 0; i < beziersCount; ++i)
			addBezier(beziers + i);
	}

	void addQuadraticBezier(const gmpi::drawing::QuadraticBezierSegment* bezier) override
	{
		/*
		A cubic Bézier curve (yellow) can be made identical to a quadratic one (black) by
1. Copying the end points, and
2. Placing its 2 middle control points (yellow circles) 2/3 along line segments from the end points to the quadratic curve's middle control point (black rectangle)
		*/
		gmpi::drawing::BezierSegment cubicbezier;
		//				auto startPoint = pathGeometry_->getLastPoint();
		cubicbezier.point3 = bezier->point2; // end point.
		cubicbezier.point1.x = lastPoint.x + 0.66666f * (bezier->point1.x - lastPoint.x);
		cubicbezier.point1.y = lastPoint.y + 0.66666f * (bezier->point1.y - lastPoint.y);
		cubicbezier.point2.x = bezier->point2.x + 0.66666f * (bezier->point1.x - bezier->point2.x);
		cubicbezier.point2.y = bezier->point2.y + 0.66666f * (bezier->point1.y - bezier->point2.y);

		addBezier(&cubicbezier);
	}

	void addQuadraticBeziers(const gmpi::drawing::QuadraticBezierSegment* beziers, uint32_t beziersCount) override
	{
		for (uint32_t i = 0; i < beziersCount; ++i)
			addQuadraticBezier(beziers + i);
	}

	static void nsvg__xformPoint(float* dx, float* dy, float x, float y, float* t)
	{
		*dx = x * t[0] + y * t[2] + t[4];
		*dy = x * t[1] + y * t[3] + t[5];
	}
	static void nsvg__xformVec(float* dx, float* dy, float x, float y, float* t)
	{
		*dx = x * t[0] + y * t[2];
		*dy = x * t[1] + y * t[3];
	}
	static float nsvg__sqr(float x) { return x * x; }
	static float nsvg__vmag(float x, float y) { return sqrtf(x * x + y * y); }
	static float nsvg__vecrat(float ux, float uy, float vx, float vy)
	{
		return (ux * vx + uy * vy) / (nsvg__vmag(ux, uy) * nsvg__vmag(vx, vy));
	}
	static float nsvg__vecang(float ux, float uy, float vx, float vy)
	{
		float r = nsvg__vecrat(ux, uy, vx, vy);
		if (r < -1.0f) r = -1.0f;
		if (r > 1.0f) r = 1.0f;
		return ((ux * vy < uy * vx) ? -1.0f : 1.0f) * acosf(r);
	}

	void addArc(const gmpi::drawing::ArcSegment* arc) override
	{
		// Ported from canvg (https://code.google.com/p/canvg/)
		float rx, ry, rotx;
		float x1, y1, x2, y2, cx, cy, dx, dy, d;
		float x1p, y1p, cxp, cyp, s, sa, sb;
		float ux, uy, vx, vy, a1, da;
		float x, y, tanx, tany, a, px = 0, py = 0, ptanx = 0, ptany = 0, t[6];
		float sinrx, cosrx;
		int fa, fs;
		int i, ndivs;
		float hda, kappa;

		rx = fabsf(arc->size.height);// args[0]);			// y radius
		ry = fabsf(arc->size.width);//args[1]);				// x radius
		rotx = arc->rotationAngle /* args[2] */ / 180.0f * static_cast<float>(M_PI); // NSVG_PI;		// x rotation angle
		fa = (int)arc->arcSize;// fabsf(args[3]) > 1e-6 ? 1 : 0;	// Large arc
		fs = (int)arc->sweepDirection;// fabsf(args[4]) > 1e-6 ? 1 : 0;	// Sweep direction
		x1 = lastPoint.x;// pathGeometry_->getLastPoint().X;// *cpx;						// start point
		y1 = lastPoint.y; // pathGeometry_->getLastPoint().Y;//*cpy;
		/* not relative
		if (rel) {							// end point
			x2 = *cpx + args[5];
			y2 = *cpy + args[6];
		}
		else
		*/
		{
			x2 = arc->point.x;// args[5];
			y2 = arc->point.y;// args[6];
		}

		dx = x1 - x2;
		dy = y1 - y2;
		d = sqrtf(dx * dx + dy * dy);
		if (d < 1e-6f || rx < 1e-6f || ry < 1e-6f) {
			// The arc degenerates to a line
			//nsvg__lineTo(p, x2, y2);
            addLine({x2, y2});
			//				*cpx = x2;
			//				*cpy = y2;
			return;
		}

		sinrx = sinf(rotx);
		cosrx = cosf(rotx);

		// Convert to center point parameterization.
		// http://www.w3.org/TR/SVG11/implnote.html#ArcImplementationNotes
		// 1) Compute x1', y1'
		x1p = cosrx * dx / 2.0f + sinrx * dy / 2.0f;
		y1p = -sinrx * dx / 2.0f + cosrx * dy / 2.0f;
		d = nsvg__sqr(x1p) / nsvg__sqr(rx) + nsvg__sqr(y1p) / nsvg__sqr(ry);
		if (d > 1) {
			d = sqrtf(d);
			rx *= d;
			ry *= d;
		}
		// 2) Compute cx', cy'
		s = 0.0f;
		sa = nsvg__sqr(rx) * nsvg__sqr(ry) - nsvg__sqr(rx) * nsvg__sqr(y1p) - nsvg__sqr(ry) * nsvg__sqr(x1p);
		sb = nsvg__sqr(rx) * nsvg__sqr(y1p) + nsvg__sqr(ry) * nsvg__sqr(x1p);
		if (sa < 0.0f) sa = 0.0f;
		if (sb > 0.0f)
			s = sqrtf(sa / sb);
		if (fa == fs)
			s = -s;
		cxp = s * rx * y1p / ry;
		cyp = s * -ry * x1p / rx;

		// 3) Compute cx,cy from cx',cy'
		cx = (x1 + x2) / 2.0f + cosrx * cxp - sinrx * cyp;
		cy = (y1 + y2) / 2.0f + sinrx * cxp + cosrx * cyp;

		// 4) Calculate theta1, and delta theta.
		ux = (x1p - cxp) / rx;
		uy = (y1p - cyp) / ry;
		vx = (-x1p - cxp) / rx;
		vy = (-y1p - cyp) / ry;
		a1 = nsvg__vecang(1.0f, 0.0f, ux, uy);	// Initial angle
		da = nsvg__vecang(ux, uy, vx, vy);		// Delta angle

		//	if (vecrat(ux,uy,vx,vy) <= -1.0f) da = NSVG_PI;
		//	if (vecrat(ux,uy,vx,vy) >= 1.0f) da = 0;

		if (fs == 0 && da > 0)
			da -= 2 * static_cast<float>(M_PI); // NSVG_PI;
		else if (fs == 1 && da < 0)
			da += 2 * static_cast<float>(M_PI); // NSVG_PI;

		// Approximate the arc using cubic spline segments.
		t[0] = cosrx; t[1] = sinrx;
		t[2] = -sinrx; t[3] = cosrx;
		t[4] = cx; t[5] = cy;

		// Split arc into max 90 degree segments.
		// The loop assumes an iteration per end point (including start and end), this +1.
		ndivs = (int)(fabsf(da) / (static_cast<float>(M_PI) * 0.5f) + 1.0f);
		hda = (da / (float)ndivs) / 2.0f;
		kappa = fabsf(4.0f / 3.0f * (1.0f - cosf(hda)) / sinf(hda));
		if (da < 0.0f)
			kappa = -kappa;

		for (i = 0; i <= ndivs; i++) {
			a = a1 + da * ((float)i / (float)ndivs);
			dx = cosf(a);
			dy = sinf(a);
			nsvg__xformPoint(&x, &y, dx * rx, dy * ry, t); // position
			nsvg__xformVec(&tanx, &tany, -dy * rx * kappa, dx * ry * kappa, t); // tangent
			if (i > 0)
			{
				//nsvg__cubicBezTo(p, px + ptanx, py + ptany, x - tanx, y - tany, x, y);
                drawing::BezierSegment bs{{px + ptanx, py + ptany}, {x - tanx, y - tany}, {x, y}};
				addBezier(&bs);
			}
			px = x;
			py = y;
			ptanx = tanx;
			ptany = tany;
		}
	}

	void addBezier(const gmpi::drawing::BezierSegment* bezier) override
	{
		// Convert bezier to line segments.
		agg::curve4_div bezierToLines;
		bezierToLines.geometrySink = this;
		bezierToLines.init(lastPoint.x, lastPoint.y, bezier->point1.x, bezier->point1.y, bezier->point2.x, bezier->point2.y, bezier->point3.x, bezier->point3.y);
	}

	void endFigure(gmpi::drawing::FigureEnd figureEnd) override
	{
		if (figureEnd == gmpi::drawing::FigureEnd::Closed)
		{
			addLine(startPoint);
		}
	}

	GMPI_QUERYINTERFACE_METHOD(drawing::api::IGeometrySink);
	GMPI_REFCOUNT;
};

class GraphicsContext : public gmpi::drawing::api::IDeviceContext
{
protected:
	std::vector<drawing::Rect> clipRectStack;

public:
	GraphicsContext()
	{
		const float defaultClipBounds = 100000.0f;
		gmpi::drawing::Rect r;
		r.top = r.left = -defaultClipBounds;
		r.bottom = r.right = defaultClipBounds;
		clipRectStack.push_back(r);
	}

	virtual ~GraphicsContext()
	{
	}

    drawing::PathGeometry createRectangleGeometry(const gmpi::drawing::Rect* rect, bool filled = false)
	{
		// create geometry
        drawing::Factory factory;
        getFactory(factory.put());

        auto geometry = factory.createPathGeometry();
        auto sink = geometry.open();

        sink.beginFigure({rect->left, rect->top}, filled ? drawing::FigureBegin::Filled : drawing::FigureBegin::Hollow);
        sink.addLine({rect->left, rect->bottom});
        sink.addLine({rect->right, rect->bottom});
        sink.addLine({rect->right, rect->top});

		sink.endFigure(drawing::FigureEnd::Closed);
		sink.close();

		return geometry;
	}

    ReturnCode drawRectangle(const gmpi::drawing::Rect* rect, gmpi::drawing::api::IBrush* brush, float strokeWidth, gmpi::drawing::api::IStrokeStyle* strokeStyle) override
	{
		auto geometry = createRectangleGeometry(rect, false);
		drawGeometry(geometry.get(), brush, strokeWidth, strokeStyle);
        return ReturnCode::Ok;
	}

    ReturnCode fillRectangle(const gmpi::drawing::Rect* rect, gmpi::drawing::api::IBrush* brush) override
	{
		auto geometry = createRectangleGeometry(rect);
		fillGeometry(geometry.get(), brush, nullptr);
        return ReturnCode::Ok;
	}

	ReturnCode clear(const drawing::Color* clearColor) override
	{
		return ReturnCode::Fail;
	}
	
	ReturnCode drawLine(drawing::Point point0, drawing::Point point1, drawing::api::IBrush* brush, float strokeWidth, drawing::api::IStrokeStyle* strokeStyle) override
	{
		auto geometry = createLineGeometry(point0, point1);
		drawGeometry(geometry.get(), brush, strokeWidth, strokeStyle);
		return ReturnCode::Ok;
	}
	
    drawing::PathGeometry createLineGeometry(gmpi::drawing::Point point0, gmpi::drawing::Point point1)
	{
		// create geometry
        drawing::Factory factory;
		getFactory(factory.put());

		auto geometry = factory.createPathGeometry();
		auto sink = geometry.open();

		sink.beginFigure(point0, drawing::FigureBegin::Hollow);
		sink.addLine(point1);

		sink.endFigure(drawing::FigureEnd::Open);
		sink.close();

		return geometry;
	}

    drawing::PathGeometry createEllipseGeometry(const gmpi::drawing::Ellipse* ellipse)
	{
		// create geometry
        drawing::Factory factory;
        getFactory(factory.put());

        auto geometry = factory.createPathGeometry();
        auto sink = geometry.open();

		/* from lines
		const double step = 0.1;
		Point p;
		bool first = true;
		for (double a = 0.0; a < M_PI * 2.0; a += step)
		{
		p.x = ellipse->point.x + ellipse->radiusX * sin(a);
		p.y = ellipse->point.y + ellipse->radiusY * cos(a);

		if (first)
		{
		sink.BeginFigure(p, FigureBegin::Filled);
		first = false;
		}
		else
		{
         sink.addLine(p);
		}
		}
		*/
        drawing::Size size{ellipse->radiusX, ellipse->radiusY};

        drawing::Point topCenter{ellipse->point.x, ellipse->point.y - size.height};
        drawing::Point bottomCenter{ellipse->point.x, ellipse->point.y + size.height};

        sink.beginFigure(topCenter, drawing::FigureBegin::Filled);
        drawing::ArcSegment firstHalf{bottomCenter, size};
        drawing::ArcSegment secondHalf{topCenter, size};

		sink.addArc(firstHalf);
		sink.addArc(secondHalf);

		sink.endFigure(drawing::FigureEnd::Closed);
		sink.close();

		/*

					// From Single Arc. Not so accurate up close.
					Point p;
					p.x = ellipse->point.x;
					p.y = ellipse->point.y + ellipse->radiusY;
					sink.BeginFigure(p, FigureBegin::Filled);

					float a = 0.01f;
					p.x = ellipse->point.x + ellipse->radiusX * sinf(a);
					p.y = ellipse->point.y + ellipse->radiusY * cosf(a);

					sink.AddArc(ArcSegment(p, Size(ellipse->radiusX, ellipse->radiusY), static_cast<float>(M_PI) * 1.99f, SweepDirection::Clockwise, ArcSize::Large));

					sink.endFigure(FigureEnd::Closed);
					sink.close();
		*/
		return geometry;
	}

	ReturnCode drawTextU(const char* string, uint32_t stringLength, drawing::api::ITextFormat* textFormat, const drawing::Rect* layoutRect, drawing::api::IBrush* defaultForegroundBrush, int32_t options) override
	{
		return ReturnCode::Fail;
	}

	ReturnCode drawBitmap(drawing::api::IBitmap* bitmap, const drawing::Rect* destinationRectangle, float opacity, drawing::BitmapInterpolationMode interpolationMode, const drawing::Rect* sourceRectangle) override
	{
		return ReturnCode::Fail;
	}

	ReturnCode setTransform(const drawing::Matrix3x2* transform) override
	{
		return ReturnCode::Fail;
	}

	ReturnCode getTransform(drawing::Matrix3x2* transform) override
	{
		return ReturnCode::Fail;
	}

	//	gmpi::ReturnCode createBitmap(gmpi::drawing::api::MP1_SIZE_U size, const gmpi::drawing::api::MP1_BITMAP_PROPERTIES* bitmapProperties, gmpi::drawing::api::IBitmap** bitmap) override;

	ReturnCode createBitmapBrush(drawing::api::IBitmap* bitmap, const drawing::BrushProperties* brushProperties, drawing::api::IBitmapBrush** returnBitmapBrush) override
	{
		return ReturnCode::Fail;
	}
	ReturnCode createRadialGradientBrush(const drawing::RadialGradientBrushProperties* radialGradientBrushProperties, const drawing::BrushProperties* brushProperties, drawing::api::IGradientstopCollection* gradientstopCollection, drawing::api::IRadialGradientBrush** returnRadialGradientBrush) override
	{
		return ReturnCode::Fail;
	}

	ReturnCode createCompatibleRenderTarget(drawing::Size desiredSize, drawing::api::IBitmapRenderTarget** bitmapRenderTarget) override
	{
		return ReturnCode::Fail;
	}
	
	ReturnCode drawEllipse(const drawing::Ellipse* ellipse, drawing::api::IBrush* brush, float strokeWidth, drawing::api::IStrokeStyle* strokeStyle) override
	{
		auto geometry = createEllipseGeometry(ellipse);
		drawGeometry(geometry.get(), brush, strokeWidth, strokeStyle);
		return ReturnCode::Ok;
	}

	ReturnCode fillEllipse(const drawing::Ellipse* ellipse, drawing::api::IBrush* brush) override
	{
		auto geometry = createEllipseGeometry(ellipse);
		fillGeometry(geometry.get(), brush, nullptr);
		return ReturnCode::Ok;
	}

	ReturnCode pushAxisAlignedClip(const drawing::Rect* clipRect) override
	{
		return ReturnCode::Fail;
	}

	ReturnCode popAxisAlignedClip() override
	{
		clipRectStack.pop_back();
		return ReturnCode::Ok;
	}

	ReturnCode getAxisAlignedClip(drawing::Rect* returnClipRect) override
	{
		return ReturnCode::Fail;
	}

	ReturnCode beginDraw() override
	{
		return ReturnCode::NoSupport;
	}

	ReturnCode endDraw() override
	{
		return ReturnCode::NoSupport;
	}

	GMPI_QUERYINTERFACE_METHOD(drawing::api::IDeviceContext);
	GMPI_REFCOUNT_NO_DELETE;
};

}
}
