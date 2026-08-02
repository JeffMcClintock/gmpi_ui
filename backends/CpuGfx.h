#pragma once

/*
#include "backends/CpuGfx.h"
*/

// Pure-software GMPI-UI backend. No GPU, no platform graphics API. See PLAN.md.
//
// Everything drawable funnels through one path:
//     path -> flatten (Gfx_base sink) -> coverage (signed-area rasterizer) -> blend
//
// Surfaces are premultiplied linear scRGB stored as RGBA half-float
// (PixelFormat::RGBA_16f, the same 64bppPRGBAHalf that createCpuRenderTarget
// produces on Windows). All arithmetic is fp32; fp16 appears only in the
// load/store span codec below.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

#include "../Drawing.h"
#include "../helpers/BitmapMask.h" // detail::floatToHalf / halfToFloat
#include "Gfx_base.h"

namespace gmpi
{
namespace cpugfx
{

// ---------------------------------------------------------------------------
// Span codec: the ONLY place fp16 appears. Plain loops so compilers can
// auto-vectorize; F16C/NEON intrinsic versions can slot in behind the same
// signatures later without touching any kernel.
// ---------------------------------------------------------------------------
inline void loadSpan(const uint16_t* src, float* dst, int floatCount)
{
    for (int i = 0; i < floatCount; ++i)
        dst[i] = drawing::detail::halfToFloat(src[i]);
}

inline void storeSpan(const float* src, uint16_t* dst, int floatCount)
{
    for (int i = 0; i < floatCount; ++i)
        dst[i] = drawing::detail::floatToHalf(src[i]);
}

// ---------------------------------------------------------------------------
// Axis-aligned bounds of a rect transformed by an affine matrix.
// (Drawing.h's transformRect maps only two corners, which is wrong under
// rotation or negative scale; clip math needs true bounds of all four.)
// ---------------------------------------------------------------------------
inline drawing::Rect transformBounds(const drawing::Matrix3x2& m, const drawing::Rect& r)
{
    const drawing::Point corners[4] = {
        drawing::transformPoint(m, {r.left, r.top}), drawing::transformPoint(m, {r.right, r.top}),
        drawing::transformPoint(m, {r.left, r.bottom}), drawing::transformPoint(m, {r.right, r.bottom}) };
    drawing::Rect out{ corners[0].x, corners[0].y, corners[0].x, corners[0].y };
    for (int i = 1; i < 4; ++i)
    {
        out.left = (std::min)(out.left, corners[i].x);
        out.top = (std::min)(out.top, corners[i].y);
        out.right = (std::max)(out.right, corners[i].x);
        out.bottom = (std::max)(out.bottom, corners[i].y);
    }
    return out;
}

// Integer pixel bound of an axis-aligned clip edge under Direct2D's ALIASED
// clip rule: a pixel belongs to the clip iff its centre is inside (top-left
// rule, so a centre exactly on the left/top edge counts as inside). The same
// formula serves as inclusive begin and exclusive end.
inline int aliasedCoord(float edge)
{
    return int(std::ceil(edge - 0.5f));
}

// ---------------------------------------------------------------------------
// Surface: owned fp16 RGBA pixel storage.
// Stride in pixels is a multiple of 8 so rows are 64-byte multiples, and the
// first pixel is 64-byte aligned: blend loops may process whole chunks that
// extend into the row padding (coverage there is zero -> identity blend).
// ---------------------------------------------------------------------------
struct Surface
{
    std::vector<uint16_t> storage;
    uint16_t* pixels{};
    int32_t width{}, height{}, stridePixels{};

    void init(int32_t w, int32_t h)
    {
        width = (std::max)(1, w);
        height = (std::max)(1, h);
        stridePixels = (width + 7) & ~7;
        storage.resize(size_t(stridePixels) * height * 4 + 32); // +32 uint16 = 64B alignment slack
        const auto addr = reinterpret_cast<uintptr_t>(storage.data());
        pixels = reinterpret_cast<uint16_t*>((addr + 63) & ~uintptr_t(63));
        std::memset(pixels, 0, size_t(stridePixels) * height * 8);
    }

    uint16_t* row(int32_t y) { return pixels + size_t(y) * stridePixels * 4; }
};

// ---------------------------------------------------------------------------
// Bitmap / BitmapPixels
// ---------------------------------------------------------------------------
class Bitmap final : public drawing::api::IBitmap
{
public:
    Surface surface;
    drawing::api::IFactory* factory{};

    Bitmap(drawing::api::IFactory* pfactory, int32_t w, int32_t h) : factory(pfactory)
    {
        surface.init(w, h);
    }

    ReturnCode getSizeU(drawing::SizeU* returnSize) override
    {
        *returnSize = { uint32_t(surface.width), uint32_t(surface.height) };
        return ReturnCode::Ok;
    }

    ReturnCode lockPixels(drawing::api::IBitmapPixels** returnPixels, int32_t flags) override;

    ReturnCode getFactory(drawing::api::IFactory** returnFactory) override
    {
        *returnFactory = factory;
        return ReturnCode::Ok;
    }

    ReturnCode queryInterface(const gmpi::api::Guid* iid, void** returnInterface) override
    {
        *returnInterface = {};
        GMPI_QUERYINTERFACE(drawing::api::IBitmap);
        GMPI_QUERYINTERFACE(drawing::api::IResource);
        return ReturnCode::NoSupport;
    }
    GMPI_REFCOUNT;
};

class BitmapPixels final : public drawing::api::IBitmapPixels
{
    Bitmap* bitmap; // keeps the surface alive while locked

public:
    BitmapPixels(Bitmap* pbitmap) : bitmap(pbitmap)
    {
        bitmap->addRef();
    }
    ~BitmapPixels()
    {
        bitmap->release();
    }

    ReturnCode getAddress(uint8_t** returnAddress) override
    {
        *returnAddress = reinterpret_cast<uint8_t*>(bitmap->surface.pixels);
        return ReturnCode::Ok;
    }
    ReturnCode getBytesPerRow(int32_t* returnBytesPerRow) override
    {
        *returnBytesPerRow = bitmap->surface.stridePixels * 8;
        return ReturnCode::Ok;
    }
    ReturnCode getPixelFormat(int32_t* returnPixelFormat) override
    {
        *returnPixelFormat = drawing::api::IBitmapPixels::RGBA_16f;
        return ReturnCode::Ok;
    }

    GMPI_QUERYINTERFACE_METHOD(drawing::api::IBitmapPixels);
    GMPI_REFCOUNT;
};

inline ReturnCode Bitmap::lockPixels(drawing::api::IBitmapPixels** returnPixels, int32_t /*flags*/)
{
    *returnPixels = new BitmapPixels(this);
    return ReturnCode::Ok;
}

// ---------------------------------------------------------------------------
// Internal brush interface: fills a span of premultiplied linear RGBA for the
// blender. Everything the renderer paints with implements this, so the blend
// loop never branches on brush type.
// ---------------------------------------------------------------------------
class CpuBrush
{
public:
    virtual ~CpuBrush() = default;

    // Premultiplied linear RGBA for device pixels (x0 .. x0+count-1) on row y.
    // deviceToLocal maps device space to the space the geometry was given in
    // (i.e. the inverse of the context transform at draw time).
    virtual void evalSpan(const drawing::Matrix3x2& deviceToLocal, int x0, int y, int count, float* dst) const = 0;

    // False if this brush would paint non-finite values, which would poison
    // even zero-coverage pixels through the blend (NaN * 0 = NaN).
    virtual bool isPaintable() const = 0;
};

// ---------------------------------------------------------------------------
// Solid color brush. GMPI Color is linear, non-premultiplied.
// ---------------------------------------------------------------------------
class SolidColorBrush final : public drawing::api::ISolidColorBrush, public CpuBrush
{
public:
    drawing::api::IFactory* factory{};
    drawing::Color color;
    float opacity{ 1.0f };

    SolidColorBrush(drawing::api::IFactory* pfactory, const drawing::Color* pcolor, const drawing::BrushProperties* properties)
        : factory(pfactory), color(*pcolor)
    {
        if (properties)
            opacity = properties->opacity;
    }

    bool isPaintable() const override
    {
        return std::isfinite(color.r) && std::isfinite(color.g) && std::isfinite(color.b)
            && std::isfinite(color.a) && std::isfinite(opacity);
    }

    void evalSpan(const drawing::Matrix3x2&, int, int, int count, float* dst) const override
    {
        const float a = color.a * opacity;
        const float r = color.r * a, g = color.g * a, b = color.b * a;
        for (int i = 0; i < count; ++i, dst += 4)
        {
            dst[0] = r; dst[1] = g; dst[2] = b; dst[3] = a;
        }
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

    ReturnCode queryInterface(const gmpi::api::Guid* iid, void** returnInterface) override
    {
        *returnInterface = {};
        GMPI_QUERYINTERFACE(drawing::api::ISolidColorBrush);
        GMPI_QUERYINTERFACE(drawing::api::IResource);
        return ReturnCode::NoSupport;
    }
    GMPI_REFCOUNT;
};

// ---------------------------------------------------------------------------
// Gradients
// ---------------------------------------------------------------------------
class GradientstopCollection final : public drawing::api::IGradientstopCollection
{
public:
    drawing::api::IFactory* factory{};
    std::vector<drawing::Gradientstop> stops; // sorted by position
    drawing::ExtendMode extendMode{ drawing::ExtendMode::Clamp };

    GradientstopCollection(drawing::api::IFactory* pfactory, const drawing::Gradientstop* pstops, uint32_t count, drawing::ExtendMode pextendMode)
        : factory(pfactory), stops(pstops, pstops + count), extendMode(pextendMode)
    {
        std::stable_sort(stops.begin(), stops.end(),
            [](const drawing::Gradientstop& a, const drawing::Gradientstop& b) { return a.position < b.position; });
    }

    bool isPaintable() const
    {
        if (stops.empty())
            return false;
        for (const auto& s : stops)
            if (!std::isfinite(s.position) || !std::isfinite(s.color.r) || !std::isfinite(s.color.g)
                || !std::isfinite(s.color.b) || !std::isfinite(s.color.a))
                return false;
        return true;
    }

    // Straight (non-premultiplied) linear colour at gradient parameter t.
    drawing::Color colorAt(float t) const
    {
        // A non-finite t makes every comparison below false, which would walk
        // the stop search off the end (out of bounds for a single-stop
        // collection). It is reachable: a near-singular transform can push a
        // device point to infinity, and Wrap then turns inf into NaN via
        // inf - floor(inf). Pin it to the start of the gradient instead.
        if (!std::isfinite(t))
            t = 0.0f;

        // Apply the extend mode first: it wraps the parameter, not the colours.
        switch (extendMode)
        {
        case drawing::ExtendMode::Wrap:
            t -= std::floor(t);
            break;
        case drawing::ExtendMode::Mirror:
        {
            const float f = std::fabs(t - 2.0f * std::floor(t * 0.5f)); // period 2
            t = f > 1.0f ? 2.0f - f : f;
            break;
        }
        default:
            t = std::clamp(t, 0.0f, 1.0f);
            break;
        }

        if (t <= stops.front().position)
            return stops.front().color;
        if (t >= stops.back().position)
            return stops.back().color;

        size_t i = 1;
        while (i < stops.size() && stops[i].position < t)
            ++i;
        if (i >= stops.size())
            return stops.back().color; // total function, whatever t is
        const auto& s0 = stops[i - 1];
        const auto& s1 = stops[i];
        const float span = s1.position - s0.position;
        const float k = span > 0.0f ? (t - s0.position) / span : 0.0f;
        return { s0.color.r + (s1.color.r - s0.color.r) * k,
                 s0.color.g + (s1.color.g - s0.color.g) * k,
                 s0.color.b + (s1.color.b - s0.color.b) * k,
                 s0.color.a + (s1.color.a - s0.color.a) * k };
    }

    ReturnCode getFactory(drawing::api::IFactory** returnFactory) override
    {
        *returnFactory = factory;
        return ReturnCode::Ok;
    }

    ReturnCode queryInterface(const gmpi::api::Guid* iid, void** returnInterface) override
    {
        *returnInterface = {};
        GMPI_QUERYINTERFACE(drawing::api::IGradientstopCollection);
        GMPI_QUERYINTERFACE(drawing::api::IResource);
        return ReturnCode::NoSupport;
    }
    GMPI_REFCOUNT;
};

// Shared plumbing for the two gradient brushes: stop lookup, brush transform,
// opacity, and turning a gradient parameter into a premultiplied span entry.
class GradientBrushBase : public CpuBrush
{
protected:
    drawing::api::IFactory* factory{};
    gmpi::shared_ptr<GradientstopCollection> stops;
    drawing::Matrix3x2 brushTransform;
    float opacity{ 1.0f };

    GradientBrushBase(drawing::api::IFactory* pfactory, const drawing::BrushProperties* properties, drawing::api::IGradientstopCollection* collection)
        : factory(pfactory)
    {
        if (auto* c = dynamic_cast<GradientstopCollection*>(collection))
        {
            c->addRef();
            stops.attach(c);
        }
        if (properties)
        {
            opacity = properties->opacity;
            brushTransform = properties->transform;
        }
    }

    // Device space -> the brush's own coordinate space.
    drawing::Matrix3x2 deviceToBrush(const drawing::Matrix3x2& deviceToLocal) const
    {
        return deviceToLocal * drawing::invert(brushTransform);
    }

    void writeStop(float t, float* dst) const
    {
        const auto c = stops->colorAt(t);
        const float a = c.a * opacity;
        dst[0] = c.r * a; dst[1] = c.g * a; dst[2] = c.b * a; dst[3] = a;
    }

public:
    bool isPaintable() const override
    {
        return stops.get() != nullptr && stops->isPaintable() && std::isfinite(opacity)
            && std::isfinite(brushTransform._11) && std::isfinite(brushTransform._12)
            && std::isfinite(brushTransform._21) && std::isfinite(brushTransform._22)
            && std::isfinite(brushTransform._31) && std::isfinite(brushTransform._32);
    }
};

class LinearGradientBrush final : public drawing::api::ILinearGradientBrush, public GradientBrushBase
{
    drawing::Point startPoint, endPoint;

public:
    LinearGradientBrush(drawing::api::IFactory* pfactory, const drawing::LinearGradientBrushProperties* props,
                        const drawing::BrushProperties* brushProperties, drawing::api::IGradientstopCollection* collection)
        : GradientBrushBase(pfactory, brushProperties, collection)
        , startPoint(props->startPoint), endPoint(props->endPoint)
    {
    }

    void setStartPoint(drawing::Point p) override { startPoint = p; }
    void setEndPoint(drawing::Point p) override { endPoint = p; }

    void evalSpan(const drawing::Matrix3x2& deviceToLocal, int x0, int y, int count, float* dst) const override
    {
        const auto m = deviceToBrush(deviceToLocal);
        const drawing::Point axis{ endPoint.x - startPoint.x, endPoint.y - startPoint.y };
        const float lenSq = axis.x * axis.x + axis.y * axis.y;

        if (!(lenSq > 0.0f))
        {
            // Degenerate axis: D2D paints the last stop's colour.
            for (int i = 0; i < count; ++i)
                writeStop(1.0f, dst + size_t(i) * 4);
            return;
        }

        // t is affine in device x, so walk it incrementally along the span
        // (this is the "per-row ramp" the plan calls for).
        const auto p0 = drawing::transformPoint(m, { float(x0) + 0.5f, float(y) + 0.5f });
        const auto p1 = drawing::transformPoint(m, { float(x0) + 1.5f, float(y) + 0.5f });
        float t = ((p0.x - startPoint.x) * axis.x + (p0.y - startPoint.y) * axis.y) / lenSq;
        const float dt = ((p1.x - p0.x) * axis.x + (p1.y - p0.y) * axis.y) / lenSq;

        for (int i = 0; i < count; ++i, dst += 4, t += dt)
            writeStop(t, dst);
    }

    ReturnCode getFactory(drawing::api::IFactory** returnFactory) override
    {
        *returnFactory = factory;
        return ReturnCode::Ok;
    }

    ReturnCode queryInterface(const gmpi::api::Guid* iid, void** returnInterface) override
    {
        *returnInterface = {};
        GMPI_QUERYINTERFACE(drawing::api::ILinearGradientBrush);
        GMPI_QUERYINTERFACE(drawing::api::IResource);
        return ReturnCode::NoSupport;
    }
    GMPI_REFCOUNT;
};

class RadialGradientBrush final : public drawing::api::IRadialGradientBrush, public GradientBrushBase
{
    drawing::Point center, originOffset;
    float radiusX{}, radiusY{};

public:
    RadialGradientBrush(drawing::api::IFactory* pfactory, const drawing::RadialGradientBrushProperties* props,
                        const drawing::BrushProperties* brushProperties, drawing::api::IGradientstopCollection* collection)
        : GradientBrushBase(pfactory, brushProperties, collection)
        , center(props->center), originOffset(props->gradientOriginOffset)
        , radiusX(props->radiusX), radiusY(props->radiusY)
    {
    }

    void setCenter(drawing::Point p) override { center = p; }
    void setGradientOriginOffset(drawing::Point p) override { originOffset = p; }
    void setRadiusX(float r) override { radiusX = r; }
    void setRadiusY(float r) override { radiusY = r; }

    void evalSpan(const drawing::Matrix3x2& deviceToLocal, int x0, int y, int count, float* dst) const override
    {
        const auto m = deviceToBrush(deviceToLocal);

        if (!(radiusX != 0.0f && radiusY != 0.0f) || !std::isfinite(radiusX) || !std::isfinite(radiusY))
        {
            for (int i = 0; i < count; ++i)
                writeStop(1.0f, dst + size_t(i) * 4);
            return;
        }

        // Work in a space where the gradient ellipse is the unit circle. The
        // focus is the gradient origin; t = 1 / k where k scales the ray from
        // the focus through the point out to the circle (the standard focal
        // radial gradient, as in SVG and Canvas).
        //
        // The solver needs |f| < 1. Clamp by LENGTH, not per-axis: a diagonal
        // offset such as (0.9, 0.9) survives a per-component clamp with
        // |f| = 1.27, which drives the discriminant negative and paints a wedge
        // of the plane a flat last-stop colour.
        float fx = originOffset.x / radiusX;
        float fy = originOffset.y / radiusY;
        const float fLen = std::sqrt(fx * fx + fy * fy);
        if (!(fLen < 0.999f))
        {
            const float k = std::isfinite(fLen) && fLen > 0.0f ? 0.999f / fLen : 0.0f;
            fx *= k;
            fy *= k;
        }
        const float fLenSq = fx * fx + fy * fy;

        const auto p0 = drawing::transformPoint(m, { float(x0) + 0.5f, float(y) + 0.5f });
        const auto p1 = drawing::transformPoint(m, { float(x0) + 1.5f, float(y) + 0.5f });
        const float stepX = (p1.x - p0.x) / radiusX;
        const float stepY = (p1.y - p0.y) / radiusY;
        float qx = (p0.x - center.x) / radiusX;
        float qy = (p0.y - center.y) / radiusY;

        for (int i = 0; i < count; ++i, dst += 4, qx += stepX, qy += stepY)
        {
            const float dx = qx - fx;
            const float dy = qy - fy;
            const float a = dx * dx + dy * dy;
            float t;
            if (a <= 0.0f)
            {
                t = 0.0f; // at the focus
            }
            else
            {
                const float b = fx * dx + fy * dy;              // half of the linear term
                const float disc = b * b + a * (1.0f - fLenSq); // always >= 0 for |f| < 1
                const float k = (-b + std::sqrt((std::max)(disc, 0.0f))) / a;
                t = (k > 0.0f) ? 1.0f / k : 1.0f;
            }
            writeStop(t, dst);
        }
    }

    ReturnCode getFactory(drawing::api::IFactory** returnFactory) override
    {
        *returnFactory = factory;
        return ReturnCode::Ok;
    }

    ReturnCode queryInterface(const gmpi::api::Guid* iid, void** returnInterface) override
    {
        *returnInterface = {};
        GMPI_QUERYINTERFACE(drawing::api::IRadialGradientBrush);
        GMPI_QUERYINTERFACE(drawing::api::IResource);
        return ReturnCode::NoSupport;
    }
    GMPI_REFCOUNT;
};

// ---------------------------------------------------------------------------
// PathGeometry: flattened polygons in local space.
// Curves never reach the rasterizer: the Gfx_base sink reduces arcs to cubics
// and cubics to line segments (AGG curve4_div); this sink only records points.
// ---------------------------------------------------------------------------
struct Figure
{
    std::vector<drawing::Point> points;
    bool filled{ true }; // FigureBegin::Filled; hollow figures are never filled (D2D semantics)
    bool closed{ false }; // FigureEnd::Closed; only strokes care (joins vs caps)
};

// ---------------------------------------------------------------------------
// Stroking: a stroke is geometry, not a raster operation. Each figure is
// widened into a set of convex pieces (segment quads, join wedges, caps) which
// are then nonzero-filled as a union. Every piece is emitted with the same
// winding direction, so overlaps reinforce instead of cancelling, and because
// the whole widened outline lands in one coverage buffer before a single
// blend, a translucent self-overlapping stroke does not double-darken.
//
// Widening happens in LOCAL space, before the transform, so a non-uniform
// scale produces an elliptical pen exactly like Direct2D.
// ---------------------------------------------------------------------------
namespace stroke
{

constexpr float kTwoPi = 6.28318530717959f;

inline drawing::Point sub(drawing::Point a, drawing::Point b) { return { a.x - b.x, a.y - b.y }; }
inline float length(drawing::Point d) { return std::sqrt(d.x * d.x + d.y * d.y); }

// Append a polygon, normalising its winding so that nonzero-filling the whole
// collection yields the union of the pieces (opposite windings would cancel).
inline void emitPolygon(std::vector<Figure>& out, std::vector<drawing::Point> pts)
{
    if (pts.size() < 3)
        return;

    float area2 = 0.0f;
    for (size_t i = 0; i < pts.size(); ++i)
    {
        const auto& a = pts[i];
        const auto& b = pts[(i + 1) % pts.size()];
        area2 += a.x * b.y - b.x * a.y;
    }
    if (area2 == 0.0f)
        return; // degenerate sliver contributes nothing
    if (area2 > 0.0f)
        std::reverse(pts.begin(), pts.end());

    out.emplace_back();
    out.back().points = std::move(pts);
    out.back().filled = true;
    out.back().closed = true;
}

// Segment count for a circle of radius r. The tolerance is tighter than the
// curve flattener's 0.25px because round caps/joins are compared directly
// against Direct2D's true circles, where 0.25px of flattening is visible.
inline int arcSegments(float r)
{
    constexpr float tol = 0.02f;
    if (r <= tol)
        return 4;
    const float step = 2.0f * std::acos(std::clamp(1.0f - tol / r, -1.0f, 1.0f));
    if (!(step > 0.0f))
        return 256;
    return std::clamp(int(std::ceil(kTwoPi / step)), 8, 256);
}

inline void emitDisc(std::vector<Figure>& out, drawing::Point c, float r)
{
    const int n = arcSegments(r);
    // Vertices sit ON the circle. Outsetting to centre the flattening error
    // (r / cos(pi/n)) is tempting but makes the disc bulge past the stroke
    // band at the tangent points, which reads worse than being a hair small.
    std::vector<drawing::Point> pts;
    pts.reserve(size_t(n));
    for (int i = 0; i < n; ++i)
    {
        const float a = kTwoPi * float(i) / float(n);
        pts.push_back({ c.x + r * std::cos(a), c.y + r * std::sin(a) });
    }
    emitPolygon(out, std::move(pts));
}

inline float signedArea2(const std::vector<drawing::Point>& pts)
{
    float area2 = 0.0f;
    for (size_t i = 0; i < pts.size(); ++i)
    {
        const auto& a = pts[i];
        const auto& b = pts[(i + 1) % pts.size()];
        area2 += a.x * b.y - b.x * a.y;
    }
    return area2;
}

// Append the polygon exactly as given (no winding normalisation). Used for
// contours whose relative orientation carries meaning — a closed figure's
// stroke is an annulus, and its inner ring must wind opposite the outer one to
// punch the hole under nonzero fill.
inline void emitContour(std::vector<Figure>& out, std::vector<drawing::Point> pts)
{
    if (pts.size() < 3)
        return;
    out.emplace_back();
    out.back().points = std::move(pts);
    out.back().filled = true;
    out.back().closed = true;
}

inline void appendArc(std::vector<drawing::Point>& contour, drawing::Point c, float r, float a0, float delta)
{
    const int full = arcSegments(r);
    const int steps = (std::max)(1, int(std::ceil(std::fabs(delta) / kTwoPi * float(full))));
    for (int i = 0; i <= steps; ++i)
    {
        const float a = a0 + delta * float(i) / float(steps);
        contour.push_back({ c.x + r * std::cos(a), c.y + r * std::sin(a) });
    }
}

// Append the join at vertex v (between incoming unit dir u0 and outgoing u1)
// to the contour being traced along the +normal side.
inline void appendJoin(std::vector<drawing::Point>& contour, drawing::Point v, drawing::Point u0, drawing::Point u1,
                       float hw, drawing::LineJoin join, float miterLimit)
{
    const drawing::Point n0{ -u0.y * hw, u0.x * hw };
    const drawing::Point n1{ -u1.y * hw, u1.x * hw };
    const drawing::Point a{ v.x + n0.x, v.y + n0.y };
    const drawing::Point b{ v.x + n1.x, v.y + n1.y };

    const float cross = u0.x * u1.y - u0.y * u1.x;
    const float dot = u0.x * u1.x + u0.y * u1.y;
    if (std::fabs(cross) < 1e-6f && dot > 0.0f)
    {
        contour.push_back(a); // collinear: the offset lines already meet
        return;
    }

    // This side is the outside of the turn when the path turns away from it.
    const bool outer = (cross < 0.0f);
    if (!outer)
    {
        // Inner side: both offset points. The little crossover loop this makes
        // is interior to the stroke, and nonzero filling absorbs it.
        contour.push_back(a);
        contour.push_back(b);
        return;
    }

    switch (join)
    {
    case drawing::LineJoin::Round:
    {
        float a0 = std::atan2(n0.y, n0.x);
        float a1 = std::atan2(n1.y, n1.x);
        float delta = a1 - a0;
        while (delta > 3.14159265358979f) delta -= kTwoPi;
        while (delta < -3.14159265358979f) delta += kTwoPi;
        appendArc(contour, v, hw, a0, delta);
        return;
    }
    case drawing::LineJoin::Miter:
    case drawing::LineJoin::MiterOrBevel:
    {
        const drawing::Point bis{ n0.x + n1.x, n0.y + n1.y };
        const float bisLen = length(bis);
        if (bisLen > 1e-6f)
        {
            // cos of half the turn angle, from the unit bisector and unit normal
            const float cosHalf = (bis.x * n0.x + bis.y * n0.y) / (bisLen * hw);
            if (cosHalf > 1e-4f)
            {
                const float miterDist = hw / cosHalf;
                const float limit = miterLimit * hw; // D2D/SVG ratio: miterLength / strokeWidth
                if (miterDist <= limit)
                {
                    const float k = miterDist / bisLen;
                    contour.push_back(a);
                    contour.push_back({ v.x + bis.x * k, v.y + bis.y * k });
                    contour.push_back(b);
                    return;
                }
                if (join == drawing::LineJoin::Miter)
                {
                    // Past the limit, D2D's plain MITER keeps the spike but
                    // cuts it off flat at the limit distance (MITER_OR_BEVEL
                    // is the one that falls back to a bevel).
                    const float k = miterDist / bisLen;
                    const drawing::Point tip{ v.x + bis.x * k, v.y + bis.y * k };
                    const float atBis = hw * cosHalf; // distance along the bisector at a and b
                    const float denom = miterDist - atBis;
                    if (denom > 1e-6f)
                    {
                        const float t = std::clamp((limit - atBis) / denom, 0.0f, 1.0f);
                        contour.push_back(a);
                        contour.push_back({ a.x + (tip.x - a.x) * t, a.y + (tip.y - a.y) * t });
                        contour.push_back({ b.x + (tip.x - b.x) * t, b.y + (tip.y - b.y) * t });
                        contour.push_back(b);
                        return;
                    }
                }
            }
        }
        break; // MiterOrBevel past the limit, or a 180-degree reversal: bevel
    }
    default:
        break;
    }

    contour.push_back(a); // Bevel
    contour.push_back(b);
}

// Append the cap that turns the contour around at endpoint p, where u is the
// outward direction of travel.
inline void appendCap(std::vector<drawing::Point>& contour, drawing::Point p, drawing::Point u,
                      float hw, drawing::CapStyle cap)
{
    const drawing::Point n{ -u.y * hw, u.x * hw };
    switch (cap)
    {
    case drawing::CapStyle::Square:
        contour.push_back({ p.x + n.x + u.x * hw, p.y + n.y + u.y * hw });
        contour.push_back({ p.x - n.x + u.x * hw, p.y - n.y + u.y * hw });
        break;
    case drawing::CapStyle::Round:
        // Half turn from +normal to -normal, bulging through +u.
        appendArc(contour, p, hw, std::atan2(n.y, n.x), -3.14159265358979f);
        break;
    default: // Flat: the contour closes straight across
        break;
    }
}

// Trace one offset side of an open polyline, inserting joins at the interior
// vertices.
inline void appendOpenSide(std::vector<drawing::Point>& contour, const std::vector<drawing::Point>& pts,
                           const std::vector<drawing::Point>& dirs, float hw,
                           drawing::LineJoin join, float miterLimit)
{
    const size_t n = pts.size();
    contour.push_back({ pts[0].x - dirs[0].y * hw, pts[0].y + dirs[0].x * hw });
    for (size_t i = 1; i + 1 < n; ++i)
        appendJoin(contour, pts[i], dirs[i - 1], dirs[i], hw, join, miterLimit);
    const auto& uLast = dirs[n - 2];
    contour.push_back({ pts[n - 1].x - uLast.y * hw, pts[n - 1].y + uLast.x * hw });
}

// Widen one flattened figure into stroke outline contours.
//
// The outline is traced as a contour rather than stamped as overlapping
// quads/wedges/discs. A coverage rasterizer accumulates AREA per pixel, so
// overlapping pieces double-count inside a partially covered pixel (a round
// cap overlapping its segment quad reads as ~95% covered where the true union
// is 50%). Tracing a single boundary has no overlap to double-count.
inline void widenFigure(const Figure& fig, float halfWidth, drawing::CapStyle cap,
                        drawing::LineJoin join, float miterLimit, std::vector<Figure>& out)
{
    // Drop consecutive duplicates (zero-length segments have no direction, and
    // a closed figure repeats its start point).
    std::vector<drawing::Point> pts;
    pts.reserve(fig.points.size());
    for (const auto& p : fig.points)
    {
        if (!std::isfinite(p.x) || !std::isfinite(p.y))
            return; // poisoned figure: skip entirely
        if (pts.empty() || length(sub(p, pts.back())) > 1e-6f)
            pts.push_back(p);
    }
    const bool closed = fig.closed;
    if (closed && pts.size() > 1 && length(sub(pts.back(), pts.front())) <= 1e-6f)
        pts.pop_back();

    if (pts.empty())
        return;

    if (pts.size() == 1)
    {
        // Degenerate figure: D2D still marks it with a round or square cap.
        if (cap == drawing::CapStyle::Round)
            emitDisc(out, pts[0], halfWidth);
        else if (cap == drawing::CapStyle::Square)
            emitPolygon(out, { { pts[0].x - halfWidth, pts[0].y - halfWidth },
                               { pts[0].x + halfWidth, pts[0].y - halfWidth },
                               { pts[0].x + halfWidth, pts[0].y + halfWidth },
                               { pts[0].x - halfWidth, pts[0].y + halfWidth } });
        return;
    }

    const size_t n = pts.size();
    const size_t segCount = closed ? n : n - 1;

    // Unit direction of each segment (duplicates were already dropped, so no
    // segment is zero-length). Computed straight from the point list so the
    // forward and reversed walks cannot disagree about indexing.
    const auto segmentDirs = [segCount](const std::vector<drawing::Point>& p, bool& ok) {
        std::vector<drawing::Point> d(segCount);
        for (size_t i = 0; i < segCount; ++i)
        {
            const auto v = sub(p[(i + 1) % p.size()], p[i]);
            const float len = length(v);
            if (!(len > 0.0f))
            {
                ok = false;
                return d;
            }
            d[i] = { v.x / len, v.y / len };
        }
        return d;
    };

    bool ok = true;
    const std::vector<drawing::Point> dirs = segmentDirs(pts, ok);
    std::vector<drawing::Point> reversedPts(pts.rbegin(), pts.rend());
    const std::vector<drawing::Point> reversedDirs = segmentDirs(reversedPts, ok);
    if (!ok)
        return; // shouldn't happen after dedupe; bail rather than divide by zero

    if (!closed)
    {
        // One contour: up one side, around the end cap, back the other side,
        // around the start cap.
        std::vector<drawing::Point> contour;
        appendOpenSide(contour, pts, dirs, halfWidth, join, miterLimit);
        appendCap(contour, pts.back(), dirs.back(), halfWidth, cap);
        appendOpenSide(contour, reversedPts, reversedDirs, halfWidth, join, miterLimit);
        appendCap(contour, pts.front(), reversedDirs.back(), halfWidth, cap);

        // Canonical winding, so strokes of separate figures reinforce rather
        // than cancel where they overlap under nonzero fill.
        if (signedArea2(contour) > 0.0f)
            std::reverse(contour.begin(), contour.end());
        emitContour(out, std::move(contour));
        return;
    }

    // Closed figure: an annulus — outer ring plus an oppositely wound inner
    // ring that punches the hole.
    std::vector<drawing::Point> ring0, ring1;
    for (size_t i = 0; i < n; ++i)
        appendJoin(ring0, pts[i], dirs[(i + segCount - 1) % segCount], dirs[i % segCount], halfWidth, join, miterLimit);
    for (size_t i = 0; i < n; ++i)
        appendJoin(ring1, reversedPts[i], reversedDirs[(i + segCount - 1) % segCount], reversedDirs[i % segCount], halfWidth, join, miterLimit);

    // When the pen is wider than the figure, the inward offset passes through
    // itself: the inner ring comes out inverted and punches a hole where the
    // stroke should read solid. Detect it geometrically — the ring's signed
    // area is not a usable signal, because the corner crossover loops subtract
    // enough to flip the sign of a perfectly valid small ring.
    {
        float minX = pts[0].x, maxX = pts[0].x, minY = pts[0].y, maxY = pts[0].y;
        for (const auto& p : pts)
        {
            minX = (std::min)(minX, p.x); maxX = (std::max)(maxX, p.x);
            minY = (std::min)(minY, p.y); maxY = (std::max)(maxY, p.y);
        }
        if (halfWidth >= 0.5f * (std::min)(maxX - minX, maxY - minY))
        {
            auto& inner = (signedArea2(pts) > 0.0f) ? ring0 : ring1;
            inner.clear();
        }
    }

    // No winding fix-up here, deliberately. Because +n is always the same
    // rotation of the travel direction, the OUTER ring always comes out
    // negatively wound whichever way the source figure is wound: for a
    // positively wound figure the forward walk offsets inward (ring0 = inner)
    // and the reversed walk outward (ring1 = outer, negative); for a
    // negatively wound one it is the other way round. That already matches the
    // canonical sign forced on open contours above, so every stroke band in a
    // geometry reinforces under nonzero fill. Canonicalising on ring0 instead
    // (as this once did) flips the outer ring positive for half of all input
    // windings, and overlapping bands then cancel to holes.
    emitContour(out, std::move(ring0));
    emitContour(out, std::move(ring1));
}

// Everything a stroke needs: the style properties plus any custom dash array.
struct Params
{
    drawing::StrokeStyleProperties props{};
    std::vector<float> dashes;
};

// Dash lengths in absolute units. The built-in patterns are Direct2D's,
// expressed in multiples of the stroke width. Empty result = solid.
inline std::vector<float> dashPattern(const Params& params, float strokeWidth)
{
    std::vector<float> p;
    switch (params.props.dashStyle)
    {
    case drawing::DashStyle::Dash:       p = { 2.0f, 2.0f }; break;
    case drawing::DashStyle::Dot:        p = { 0.0f, 2.0f }; break;
    case drawing::DashStyle::DashDot:    p = { 2.0f, 2.0f, 0.0f, 2.0f }; break;
    case drawing::DashStyle::DashDotDot: p = { 2.0f, 2.0f, 0.0f, 2.0f, 0.0f, 2.0f }; break;
    case drawing::DashStyle::Custom:     p = params.dashes; break;
    default:                             return {}; // Solid
    }

    float total = 0.0f;
    for (auto& v : p)
    {
        v = (v < 0.0f ? 0.0f : v) * strokeWidth;
        total += v;
    }
    if (!(total > 0.0f) || p.size() < 2)
        return {}; // degenerate pattern: draw solid rather than nothing at all
    return p;
}

// Split one figure into the "on" runs of the dash pattern, as open figures.
inline void dashFigure(const Figure& fig, const std::vector<float>& pattern, float offset,
                       std::vector<Figure>& out)
{
    std::vector<drawing::Point> pts;
    pts.reserve(fig.points.size() + 1);
    for (const auto& p : fig.points)
    {
        if (!std::isfinite(p.x) || !std::isfinite(p.y))
            return;
        if (pts.empty() || length(sub(p, pts.back())) > 1e-6f)
            pts.push_back(p);
    }
    if (fig.closed && pts.size() > 1 && length(sub(pts.back(), pts.front())) > 1e-6f)
        pts.push_back(pts.front()); // dashes run continuously around a closed figure
    if (pts.size() < 2)
        return;

    // Cumulative arc length at each vertex.
    std::vector<float> arc(pts.size(), 0.0f);
    for (size_t i = 1; i < pts.size(); ++i)
        arc[i] = arc[i - 1] + length(sub(pts[i], pts[i - 1]));
    const float totalLen = arc.back();
    if (!(totalLen > 0.0f))
        return;

    float cycle = 0.0f;
    for (float v : pattern)
        cycle += v;

    // Point at a given arc length along the polyline.
    const auto pointAtArc = [&](float d) {
        d = std::clamp(d, 0.0f, totalLen);
        size_t i = 1;
        while (i + 1 < pts.size() && arc[i] < d)
            ++i;
        const float segLen = arc[i] - arc[i - 1];
        const float t = segLen > 0.0f ? (d - arc[i - 1]) / segLen : 0.0f;
        return drawing::Point{ pts[i - 1].x + (pts[i].x - pts[i - 1].x) * t,
                               pts[i - 1].y + (pts[i].y - pts[i - 1].y) * t };
    };

    // Start phase within the pattern.
    float phase = std::fmod(offset, cycle);
    if (phase < 0.0f)
        phase += cycle;
    size_t idx = 0;
    while (phase >= pattern[idx] && pattern[idx] > 0.0f)
    {
        phase -= pattern[idx];
        idx = (idx + 1) % pattern.size();
    }

    // Walk the pattern along the polyline. Each cycle advances by `cycle` > 0,
    // so this always terminates even when some entries are zero-length.
    float pos = -phase;
    while (pos < totalLen)
    {
        const float len = pattern[idx];
        const float start = pos;
        const float end = pos + len;
        const bool on = (idx % 2) == 0;

        if (on && end >= 0.0f && start <= totalLen)
        {
            const float s = (std::max)(start, 0.0f);
            const float e = (std::min)(end, totalLen);
            Figure run;
            run.filled = true;
            run.closed = false;
            if (e <= s)
            {
                run.points.push_back(pointAtArc(s)); // zero-length dash: a dot
            }
            else
            {
                run.points.push_back(pointAtArc(s));
                for (size_t i = 0; i < pts.size(); ++i)
                    if (arc[i] > s && arc[i] < e)
                        run.points.push_back(pts[i]); // keep interior corners
                run.points.push_back(pointAtArc(e));
            }
            out.push_back(std::move(run));
        }

        pos = end;
        idx = (idx + 1) % pattern.size();
    }
}

inline void widen(const std::vector<Figure>& figures, float strokeWidth,
                  const Params& params, std::vector<Figure>& out)
{
    const float hw = strokeWidth * 0.5f;
    const float miterLimit = (std::max)(1.0f, params.props.miterLimit);
    const auto pattern = dashPattern(params, strokeWidth);

    if (pattern.empty())
    {
        for (const auto& fig : figures)
            widenFigure(fig, hw, params.props.lineCap, params.props.lineJoin, miterLimit, out);
        return;
    }

    std::vector<Figure> dashed;
    for (const auto& fig : figures)
        dashFigure(fig, pattern, params.props.dashOffset * strokeWidth, dashed);
    for (const auto& fig : dashed)
        widenFigure(fig, hw, params.props.lineCap, params.props.lineJoin, miterLimit, out);
}

} // namespace stroke

// Stroke parameters from an IStrokeStyle (null = Direct2D defaults).
inline stroke::Params strokeParams(drawing::api::IStrokeStyle* strokeStyle)
{
    stroke::Params params;
    if (auto* ss = dynamic_cast<se::generic_graphics::StrokeStyle*>(strokeStyle))
    {
        params.props = ss->strokeStyleProperties;
        params.dashes = ss->dashes;
    }
    return params;
}

class PathGeometry final : public drawing::api::IPathGeometry
{
public:
    drawing::api::IFactory* factory{};
    std::vector<Figure> figures;
    drawing::FillMode fillMode{ drawing::FillMode::Alternate }; // matches the D2D sink default

    PathGeometry(drawing::api::IFactory* pfactory) : factory(pfactory) {}

    ReturnCode open(drawing::api::IGeometrySink** returnGeometrySink) override;

    ReturnCode strokeContainsPoint(drawing::Point point, float strokeWidth, drawing::api::IStrokeStyle* strokeStyle, const drawing::Matrix3x2* worldTransform, bool* returnContains) override
    {
        *returnContains = false;
        if (!(strokeWidth > 0.0f) || !std::isfinite(strokeWidth))
            return ReturnCode::Ok;

        std::vector<Figure> widened;
        stroke::widen(figures, strokeWidth, strokeParams(strokeStyle), widened);

        // Sum the winding over ALL pieces, exactly as the nonzero fill does.
        // The pieces are not independent shapes: a closed figure's stroke is
        // an annulus whose inner ring must subtract, so testing each piece on
        // its own and OR-ing would report the shape's whole interior as being
        // on the stroke.
        const drawing::Matrix3x2 identity;
        const auto& m = worldTransform ? *worldTransform : identity;
        int winding = 0;
        for (const auto& piece : widened)
        {
            const size_t n = piece.points.size();
            for (size_t i = 0; i < n; ++i)
            {
                const auto a = drawing::transformPoint(m, piece.points[i]);
                const auto b = drawing::transformPoint(m, piece.points[(i + 1) % n]);
                if ((a.y <= point.y) != (b.y <= point.y))
                {
                    const float xCross = a.x + (point.y - a.y) * (b.x - a.x) / (b.y - a.y);
                    if (point.x < xCross)
                        winding += (b.y > a.y) ? 1 : -1;
                }
            }
        }
        *returnContains = (winding != 0);
        return ReturnCode::Ok;
    }

    ReturnCode fillContainsPoint(drawing::Point point, const drawing::Matrix3x2* worldTransform, bool* returnContains) override
    {
        const drawing::Matrix3x2 identity;
        const auto& m = worldTransform ? *worldTransform : identity;

        int winding = 0;
        for (const auto& fig : figures)
        {
            if (!fig.filled)
                continue;
            const size_t n = fig.points.size();
            for (size_t i = 0; i < n; ++i)
            {
                const auto a = drawing::transformPoint(m, fig.points[i]);
                const auto b = drawing::transformPoint(m, fig.points[(i + 1) % n]);
                if ((a.y <= point.y) != (b.y <= point.y))
                {
                    const float xCross = a.x + (point.y - a.y) * (b.x - a.x) / (b.y - a.y);
                    if (point.x < xCross)
                        winding += (b.y > a.y) ? 1 : -1;
                }
            }
        }
        *returnContains = (fillMode == drawing::FillMode::Winding) ? (winding != 0) : ((winding & 1) != 0);
        return ReturnCode::Ok;
    }

    ReturnCode getWidenedBounds(float strokeWidth, drawing::api::IStrokeStyle* strokeStyle, const drawing::Matrix3x2* worldTransform, drawing::Rect* returnBounds) override
    {
        *returnBounds = {};
        if (!(strokeWidth > 0.0f) || !std::isfinite(strokeWidth))
            return ReturnCode::Ok;

        std::vector<Figure> widened;
        stroke::widen(figures, strokeWidth, strokeParams(strokeStyle), widened);

        const drawing::Matrix3x2 identity;
        const auto& m = worldTransform ? *worldTransform : identity;
        bool any = false;
        drawing::Rect bounds{};
        for (const auto& piece : widened)
        {
            for (const auto& p : piece.points)
            {
                const auto tp = drawing::transformPoint(m, p);
                if (!any)
                {
                    bounds = { tp.x, tp.y, tp.x, tp.y };
                    any = true;
                }
                else
                {
                    bounds.left = (std::min)(bounds.left, tp.x);
                    bounds.top = (std::min)(bounds.top, tp.y);
                    bounds.right = (std::max)(bounds.right, tp.x);
                    bounds.bottom = (std::max)(bounds.bottom, tp.y);
                }
            }
        }
        *returnBounds = bounds;
        return ReturnCode::Ok;
    }

    ReturnCode getFactory(drawing::api::IFactory** returnFactory) override
    {
        *returnFactory = factory;
        return ReturnCode::Ok;
    }

    ReturnCode queryInterface(const gmpi::api::Guid* iid, void** returnInterface) override
    {
        *returnInterface = {};
        GMPI_QUERYINTERFACE(drawing::api::IPathGeometry);
        GMPI_QUERYINTERFACE(drawing::api::IResource);
        return ReturnCode::NoSupport;
    }
    GMPI_REFCOUNT;
};

class GeometrySink final : public se::generic_graphics::GeometrySink
{
    PathGeometry* path;

public:
    GeometrySink(PathGeometry* ppath) : path(ppath)
    {
        path->addRef();
    }
    ~GeometrySink()
    {
        path->release();
    }

    void beginFigure(drawing::Point pStartPoint, drawing::FigureBegin figureBegin) override
    {
        se::generic_graphics::GeometrySink::beginFigure(pStartPoint, figureBegin);
        path->figures.emplace_back();
        path->figures.back().points.push_back(pStartPoint);
        path->figures.back().filled = (figureBegin == drawing::FigureBegin::Filled);
    }

    void addLine(drawing::Point point) override
    {
        if (!path->figures.empty())
            path->figures.back().points.push_back(point);
        lastPoint = point;
    }

    void endFigure(drawing::FigureEnd figureEnd) override
    {
        if (!path->figures.empty())
            path->figures.back().closed = (figureEnd == drawing::FigureEnd::Closed);
        // The base class appends the closing segment; widenFigure drops the
        // duplicated start point.
        se::generic_graphics::GeometrySink::endFigure(figureEnd);
    }

    void setFillMode(drawing::FillMode fillMode) override
    {
        path->fillMode = fillMode;
    }
};

inline ReturnCode PathGeometry::open(drawing::api::IGeometrySink** returnGeometrySink)
{
    figures.clear();
    fillMode = drawing::FillMode::Alternate; // each sink starts from the D2D default
    *returnGeometrySink = new GeometrySink(this);
    return ReturnCode::Ok;
}

// ---------------------------------------------------------------------------
// Rasterizer: signed-area coverage accumulation (font-rs style).
// Walk each edge once, accumulating fractional signed coverage deltas into a
// float buffer; a per-row prefix sum then yields analytic antialiased
// coverage. Coordinates are relative to the raster-area origin; callers clamp
// x into [0, w]. Each row has (w + 2) cells: edges may spill one cell past the
// right boundary, and the spill cells never enter the prefix sum.
// ---------------------------------------------------------------------------
namespace raster
{

inline void accumulateEdge(float* acc, int w, int h, drawing::Point p0, drawing::Point p1)
{
    if (p0.y == p1.y)
        return;

    float dir = 1.0f;
    if (p0.y > p1.y)
    {
        std::swap(p0, p1);
        dir = -1.0f;
    }
    const float dxdy = (p1.x - p0.x) / (p1.y - p0.y);
    const int rowStride = w + 2;

    float x = p0.x;
    if (p0.y < 0.0f)
        x -= p0.y * dxdy; // advance x to the y=0 boundary

    // Clamp in float BEFORE the int conversions: float->int is undefined
    // behaviour when the value exceeds int range (|y| can be ~1e30 here).
    const int yFirst = int(std::floor(std::clamp(p0.y, 0.0f, float(h))));
    const int yLimit = int(std::ceil(std::clamp(p1.y, 0.0f, float(h))));

    for (int y = yFirst; y < yLimit; ++y)
    {
        float* row = acc + size_t(y) * rowStride;
        const float dy = (std::min)(float(y + 1), p1.y) - (std::max)(float(y), p0.y);
        const float xnext = x + dxdy * dy;
        const float d = dy * dir;

        float x0 = x, x1 = xnext;
        if (x0 > x1)
            std::swap(x0, x1);
        // The per-row interpolated x can round an ulp outside [0, w] even when
        // the segment endpoints were clamped; unclamped, floor() yields -1 and
        // row[-1] is a heap-underflow write on raster row 0 (found and
        // confirmed by the adversarial review with exact float32 repro).
        x0 = std::clamp(x0, 0.0f, float(w));
        x1 = std::clamp(x1, 0.0f, float(w));

        const float x0floor = std::floor(x0);
        const int x0i = int(x0floor);
        const float x1ceil = std::ceil(x1);
        const int x1i = int(x1ceil);

        if (x1i <= x0i + 1)
        {
            // Sub-edge fits within one pixel column: midpoint split.
            const float xmf = 0.5f * (x0 + x1) - x0floor;
            row[x0i] += d * (1.0f - xmf);
            row[x0i + 1] += d * xmf;
        }
        else
        {
            const float s = 1.0f / (x1 - x0);
            const float x0f = x0 - x0floor;
            const float a0 = 0.5f * s * (1.0f - x0f) * (1.0f - x0f);
            const float x1f = x1 - (x1ceil - 1.0f);
            const float am = 0.5f * s * x1f * x1f;

            row[x0i] += d * a0;
            if (x1i == x0i + 2)
            {
                row[x0i + 1] += d * (1.0f - a0 - am);
            }
            else
            {
                const float a1 = s * (1.5f - x0f);
                row[x0i + 1] += d * (a1 - a0);
                for (int xi = x0i + 2; xi < x1i - 1; ++xi)
                    row[xi] += d * s;
                const float a2 = a1 + float(x1i - x0i - 3) * s;
                row[x1i - 1] += d * (1.0f - a2 - am);
            }
            row[x1i] += d * am;
        }
        x = xnext;
    }
}

// Clip an edge to the raster area's horizontal range, preserving winding:
// portions left of x=0 are projected onto the x=0 boundary (their winding
// affects every visible cell on their rows); portions right of x=w cannot
// affect visible cells and are dropped. Vertical clipping is exact inside
// accumulateEdge's row loop. The segment is split at boundary crossings so
// slopes are never distorted (unlike endpoint clamping).
inline void accumulateEdgeClipped(float* acc, int w, int h, drawing::Point p0, drawing::Point p1)
{
    if (p0.y == p1.y)
        return;

    const float fw = float(w);

    // Fast path: the edge never leaves the horizontal range.
    if (p0.x >= 0.0f && p0.x <= fw && p1.x >= 0.0f && p1.x <= fw)
    {
        accumulateEdge(acc, w, h, p0, p1);
        return;
    }

    // Split at crossings of x=0 and x=w. Splits are computed in double and the
    // original endpoints are used verbatim at t=0/1: with far-off-surface
    // coordinates (|x|,|y| ~ 1e7) reconstructing an endpoint as p0 + t*(p1-p0)
    // in float32 can land a whole pixel away (ulp of 1e7 is 1.0), which
    // visibly moved fractional edges near the surface.
    const double x0d = p0.x, y0d = p0.y;
    const double dxd = double(p1.x) - x0d;
    const double dyd = double(p1.y) - y0d;

    double ts[4] = { 0.0, 1.0, 1.0, 1.0 };
    int n = 2;
    if (dxd != 0.0)
    {
        const double tLeft = (0.0 - x0d) / dxd;
        const double tRight = (double(fw) - x0d) / dxd;
        if (tLeft > 0.0 && tLeft < 1.0)
            ts[n++] = tLeft;
        if (tRight > 0.0 && tRight < 1.0)
            ts[n++] = tRight;
    }
    std::sort(ts, ts + n);

    const auto pointAt = [&](double t) -> drawing::Point {
        if (t <= 0.0) return p0;
        if (t >= 1.0) return p1;
        return { float(x0d + t * dxd), float(y0d + t * dyd) };
    };

    for (int i = 0; i + 1 < n; ++i)
    {
        const double ta = ts[i], tb = ts[i + 1];
        if (tb <= ta)
            continue;

        auto a = pointAt(ta);
        auto b = pointAt(tb);

        const double xMid = x0d + 0.5 * (ta + tb) * dxd; // region classifier
        if (xMid < 0.0)
        {
            a.x = b.x = 0.0f; // project onto the left boundary
        }
        else if (xMid > fw)
        {
            continue; // right of the raster area: invisible
        }
        else
        {
            // guard float jitter at the split points
            a.x = std::clamp(a.x, 0.0f, fw);
            b.x = std::clamp(b.x, 0.0f, fw);
        }
        accumulateEdge(acc, w, h, a, b);
    }
}

} // namespace raster

// ---------------------------------------------------------------------------
// RenderTarget: device context + offscreen bitmap in one.
// Standard inheritance of IBitmapRenderTarget via the templated Gfx_base
// context (no vtable-layout emulation). Rectangles, ellipses, rounded rects
// and lines all route through path geometry in the base class -- the only
// pixel-touching code is fillGeometry's coverage/blend below.
// ---------------------------------------------------------------------------
class RenderTarget final : public se::generic_graphics::GraphicsContextT<drawing::api::IBitmapRenderTarget>
{
    drawing::api::IFactory* factory{};
    Bitmap* bitmap{}; // owned (refcounted)
    drawing::Matrix3x2 transform_;

    // scratch buffers, reused across fills
    std::vector<float> accBuf;      // (w + 2) * h coverage deltas
    std::vector<float> covBuf;      // per-row coverage, indexed by absolute x
    std::vector<float> rowBuf;      // fp32 staging for one row span
    std::vector<float> srcBuf;      // premultiplied source colour for one row span
    std::vector<drawing::Point> devicePoints;
    std::vector<char> figureValid;
    std::vector<Figure> strokeFigures; // widened stroke outline, reused per call

public:
    RenderTarget(drawing::api::IFactory* pfactory, drawing::SizeU size) : factory(pfactory)
    {
        bitmap = new Bitmap(factory, int32_t(size.width), int32_t(size.height));

        // Clip stack is in device space; replace the base's huge default with the surface bounds.
        clipRectStack.clear();
        clipRectStack.push_back(surfaceBounds());
    }

    ~RenderTarget()
    {
        bitmap->release();
    }

    drawing::Rect surfaceBounds() const
    {
        return { 0.0f, 0.0f, float(bitmap->surface.width), float(bitmap->surface.height) };
    }

    // ---- resource creation -------------------------------------------------
    ReturnCode createSolidColorBrush(const drawing::Color* color, const drawing::BrushProperties* brushProperties, drawing::api::ISolidColorBrush** returnSolidColorBrush) override
    {
        *returnSolidColorBrush = new SolidColorBrush(factory, color, brushProperties);
        return ReturnCode::Ok;
    }

    ReturnCode createGradientstopCollection(const drawing::Gradientstop* gradientstops, uint32_t gradientstopsCount, drawing::ExtendMode extendMode, drawing::api::IGradientstopCollection** returnCollection) override
    {
        *returnCollection = {};
        if (!gradientstops || gradientstopsCount == 0)
            return ReturnCode::Fail;
        *returnCollection = new GradientstopCollection(factory, gradientstops, gradientstopsCount, extendMode);
        return ReturnCode::Ok;
    }

    ReturnCode createLinearGradientBrush(const drawing::LinearGradientBrushProperties* linearGradientBrushProperties, const drawing::BrushProperties* brushProperties, drawing::api::IGradientstopCollection* gradientstopCollection, drawing::api::ILinearGradientBrush** returnBrush) override
    {
        *returnBrush = {};
        if (!linearGradientBrushProperties || !dynamic_cast<GradientstopCollection*>(gradientstopCollection))
            return ReturnCode::Fail;
        *returnBrush = new LinearGradientBrush(factory, linearGradientBrushProperties, brushProperties, gradientstopCollection);
        return ReturnCode::Ok;
    }

    ReturnCode createRadialGradientBrush(const drawing::RadialGradientBrushProperties* radialGradientBrushProperties, const drawing::BrushProperties* brushProperties, drawing::api::IGradientstopCollection* gradientstopCollection, drawing::api::IRadialGradientBrush** returnBrush) override
    {
        *returnBrush = {};
        if (!radialGradientBrushProperties || !dynamic_cast<GradientstopCollection*>(gradientstopCollection))
            return ReturnCode::Fail;
        *returnBrush = new RadialGradientBrush(factory, radialGradientBrushProperties, brushProperties, gradientstopCollection);
        return ReturnCode::Ok;
    }

    // ---- state -------------------------------------------------------------
    ReturnCode setTransform(const drawing::Matrix3x2* ptransform) override
    {
        transform_ = *ptransform;
        return ReturnCode::Ok;
    }

    ReturnCode getTransform(drawing::Matrix3x2* returnTransform) override
    {
        *returnTransform = transform_;
        return ReturnCode::Ok;
    }

    ReturnCode pushAxisAlignedClip(const drawing::Rect* clipRect) override
    {
        // D2D semantics: the rect is in local space at push time; stack is device space.
        clipRectStack.push_back(drawing::intersectRect(clipRectStack.back(), transformBounds(transform_, *clipRect)));
        return ReturnCode::Ok;
    }

    ReturnCode getAxisAlignedClip(drawing::Rect* returnClipRect) override
    {
        *returnClipRect = transformBounds(drawing::invert(transform_), clipRectStack.back());
        return ReturnCode::Ok;
    }

    ReturnCode beginDraw() override { return ReturnCode::Ok; }
    ReturnCode endDraw() override { return ReturnCode::Ok; }

    // ---- drawing -----------------------------------------------------------
    ReturnCode clear(const drawing::Color* clearColor) override
    {
        if (!(std::isfinite(clearColor->r) && std::isfinite(clearColor->g) &&
              std::isfinite(clearColor->b) && std::isfinite(clearColor->a)))
            return ReturnCode::Ok;

        auto& s = bitmap->surface;
        const auto clip = drawing::intersectRect(clipRectStack.back(), surfaceBounds());
        const int x0 = aliasedCoord(clip.left);
        const int x1 = aliasedCoord(clip.right);
        const int y0 = aliasedCoord(clip.top);
        const int y1 = aliasedCoord(clip.bottom);
        if (x1 <= x0 || y1 <= y0)
            return ReturnCode::Ok;

        const float a = clearColor->a;
        const uint16_t pat[4] = {
            drawing::detail::floatToHalf(clearColor->r * a),
            drawing::detail::floatToHalf(clearColor->g * a),
            drawing::detail::floatToHalf(clearColor->b * a),
            drawing::detail::floatToHalf(a) };
        uint64_t pat64;
        std::memcpy(&pat64, pat, 8);

        for (int y = y0; y < y1; ++y)
        {
            uint16_t* row = s.row(y);
            for (int x = x0; x < x1; ++x)
                std::memcpy(row + size_t(x) * 4, &pat64, 8);
        }
        return ReturnCode::Ok;
    }

    ReturnCode fillGeometry(drawing::api::IPathGeometry* pathGeometry, drawing::api::IBrush* brush, drawing::api::IBrush* opacityBrush) override
    {
        auto* path = dynamic_cast<PathGeometry*>(pathGeometry);
        auto* cpuBrush = dynamic_cast<CpuBrush*>(brush);
        if (!path || !cpuBrush)
            return ReturnCode::NoSupport; // e.g. bitmap brushes: milestone 6
        if (opacityBrush)
            return ReturnCode::NoSupport; // masked fills: fail loudly rather than render wrong

        return fillFigures(path->figures, path->fillMode, *cpuBrush);
    }

    ReturnCode drawGeometry(drawing::api::IPathGeometry* pathGeometry, drawing::api::IBrush* brush, float strokeWidth, drawing::api::IStrokeStyle* strokeStyle) override
    {
        auto* path = dynamic_cast<PathGeometry*>(pathGeometry);
        auto* cpuBrush = dynamic_cast<CpuBrush*>(brush);
        if (!path || !cpuBrush)
            return ReturnCode::NoSupport;
        if (!(strokeWidth > 0.0f) || !std::isfinite(strokeWidth))
            return ReturnCode::Ok; // D2D draws nothing for a zero-width stroke

        // Widen in local space (so a non-uniform transform yields an elliptical
        // pen, like D2D), then fill the union of the pieces.
        strokeFigures.clear();
        stroke::widen(path->figures, strokeWidth, strokeParams(strokeStyle), strokeFigures);
        return fillFigures(strokeFigures, drawing::FillMode::Winding, *cpuBrush);
    }

private:
    // Rasterize + blend a set of already-flattened figures. The single entry
    // point to pixels: both fillGeometry and drawGeometry come through here.
    ReturnCode fillFigures(const std::vector<Figure>& figures, drawing::FillMode fillMode, const CpuBrush& brush)
    {
        // A non-finite colour would poison even zero-coverage pixels through
        // the blend (NaN * 0 = NaN); render nothing instead.
        if (!brush.isPaintable())
            return ReturnCode::Ok;

        if (figures.empty())
            return ReturnCode::Ok;

        auto& s = bitmap->surface;

        // 1. Transform to device space; gather the bounding box.
        //    Figures containing a non-finite point (NaN/Inf inputs, or overflow
        //    from extreme coordinates x transform) are skipped entirely: they
        //    would poison the bbox and the winding sums.
        devicePoints.clear();
        figureValid.clear();
        float minX = 1e30f, minY = 1e30f, maxX = -1e30f, maxY = -1e30f;
        {
            size_t figStart = 0;
            for (const auto& fig : figures)
            {
                bool valid = fig.filled && !fig.points.empty();
                for (const auto& p : fig.points)
                {
                    const auto dp = drawing::transformPoint(transform_, p);
                    devicePoints.push_back(dp);
                    valid = valid && std::isfinite(dp.x) && std::isfinite(dp.y);
                }
                figureValid.push_back(valid ? 1 : 0);
                if (valid)
                {
                    for (size_t i = figStart; i < devicePoints.size(); ++i)
                    {
                        minX = (std::min)(minX, devicePoints[i].x);
                        minY = (std::min)(minY, devicePoints[i].y);
                        maxX = (std::max)(maxX, devicePoints[i].x);
                        maxY = (std::max)(maxY, devicePoints[i].y);
                    }
                }
                figStart = devicePoints.size();
            }
        }
        if (minX > maxX)
            return ReturnCode::Ok; // no valid figures

        // 2. Integer raster rect = bbox ∩ clip ∩ surface.
        //    Geometry bounds use floor/ceil (must cover the full antialiased
        //    extent); the clip contributes D2D ALIASED pixel-centre bounds.
        const auto clip = drawing::intersectRect(clipRectStack.back(), surfaceBounds());
        // Disjoint nested clips produce an inverted (empty) intersection;
        // std::clamp with lo > hi below would be undefined behaviour.
        if (!(clip.left < clip.right) || !(clip.top < clip.bottom))
            return ReturnCode::Ok;
        // Clamp the (possibly astronomically large, e.g. 1e30) bbox into the
        // clip rect in float BEFORE converting to int — float-to-int of an
        // out-of-range value is undefined behaviour.
        minX = std::clamp(minX, clip.left, clip.right);
        maxX = std::clamp(maxX, clip.left, clip.right);
        minY = std::clamp(minY, clip.top, clip.bottom);
        maxY = std::clamp(maxY, clip.top, clip.bottom);
        const int ix0 = (std::max)(int(std::floor(minX)), aliasedCoord(clip.left));
        const int iy0 = (std::max)(int(std::floor(minY)), aliasedCoord(clip.top));
        const int ix1 = (std::min)(int(std::ceil(maxX)), aliasedCoord(clip.right));
        const int iy1 = (std::min)(int(std::ceil(maxY)), aliasedCoord(clip.bottom));
        const int w = ix1 - ix0;
        const int h = iy1 - iy0;
        if (w <= 0 || h <= 0)
            return ReturnCode::Ok;

        // 3. Accumulate all figures' edges into the coverage-delta buffer,
        //    clipping each segment exactly at the raster bounds.
        const int rowStride = w + 2;
        accBuf.assign(size_t(rowStride) * h, 0.0f);

        size_t base = 0;
        size_t figIndex = 0;
        for (const auto& fig : figures)
        {
            const size_t n = fig.points.size();
            if (!figureValid[figIndex++])
            {
                base += n;
                continue;
            }
            for (size_t i = 0; i < n; ++i)
            {
                auto a = devicePoints[base + i];
                auto b = devicePoints[base + (i + 1) % n]; // implicit close for open figures
                a.x -= ix0;
                a.y -= iy0;
                b.x -= ix0;
                b.y -= iy0;
                raster::accumulateEdgeClipped(accBuf.data(), w, h, a, b);
            }
            base += n;
        }

        // 4. Per row: prefix-sum to coverage, ask the brush for the source
        //    colours, then blend. Premultiplied source, over with coverage:
        //    dst = src*cov + dst*(1 - srcA*cov)
        const bool nonzero = (fillMode == drawing::FillMode::Winding);
        const auto deviceToLocal = drawing::invert(transform_);

        // Chunk-aligned blend span; row padding absorbs the rounding (cov = 0 there).
        const int ax0 = ix0 & ~3;
        const int ax1 = (std::min)(s.stridePixels, (ix1 + 3) & ~3);
        const int spanPx = ax1 - ax0;

        covBuf.assign(size_t(s.stridePixels), 0.0f);
        rowBuf.resize(size_t(spanPx) * 4);
        srcBuf.resize(size_t(spanPx) * 4);

        for (int y = 0; y < h; ++y)
        {
            const float* acc = accBuf.data() + size_t(y) * rowStride;
            float* cov = covBuf.data();

            float run = 0.0f;
            if (nonzero)
            {
                for (int x = 0; x < w; ++x)
                {
                    run += acc[x];
                    cov[ix0 + x] = (std::min)(1.0f, std::fabs(run));
                }
            }
            else
            {
                for (int x = 0; x < w; ++x)
                {
                    run += acc[x];
                    const float t = std::fmod(std::fabs(run), 2.0f);
                    cov[ix0 + x] = 1.0f - std::fabs(t - 1.0f);
                }
            }

            uint16_t* p = s.row(iy0 + y) + size_t(ax0) * 4;
            loadSpan(p, rowBuf.data(), spanPx * 4);
            brush.evalSpan(deviceToLocal, ax0, iy0 + y, spanPx, srcBuf.data());

            float* d = rowBuf.data();
            const float* sc = srcBuf.data();
            const float* cv = cov + ax0;
            for (int i = 0; i < spanPx; ++i, d += 4, sc += 4)
            {
                const float c = cv[i];
                const float k = 1.0f - sc[3] * c;
                d[0] = sc[0] * c + d[0] * k;
                d[1] = sc[1] * c + d[1] * k;
                d[2] = sc[2] * c + d[2] * k;
                d[3] = sc[3] * c + d[3] * k;
            }

            storeSpan(rowBuf.data(), p, spanPx * 4);

            // reset the coverage cells we wrote
            std::memset(cov + ix0, 0, size_t(w) * sizeof(float));
        }

        return ReturnCode::Ok;
    }

public:
    ReturnCode fillRoundedRectangle(const drawing::RoundedRect* roundedRect, drawing::api::IBrush* brush) override
    {
        auto geometry = createRoundedRectGeometry(roundedRect);
        this->fillGeometry(drawing::AccessPtr::get(geometry), brush, nullptr);
        return ReturnCode::Ok;
    }

    ReturnCode drawRoundedRectangle(const drawing::RoundedRect* roundedRect, drawing::api::IBrush* brush, float strokeWidth, drawing::api::IStrokeStyle* strokeStyle) override
    {
        auto geometry = createRoundedRectGeometry(roundedRect);
        return this->drawGeometry(drawing::AccessPtr::get(geometry), brush, strokeWidth, strokeStyle);
    }

    ReturnCode drawRichTextU(drawing::api::IRichTextFormat*, const drawing::Rect*, drawing::api::IBrush*, int32_t) override
    {
        return ReturnCode::NoSupport; // milestone 8 (text)
    }

    // ---- target ------------------------------------------------------------
    ReturnCode getBitmap(drawing::api::IBitmap** returnBitmap) override
    {
        *returnBitmap = bitmap;
        bitmap->addRef();
        return ReturnCode::Ok;
    }

    ReturnCode getFactory(drawing::api::IFactory** returnFactory) override
    {
        *returnFactory = factory;
        return ReturnCode::Ok;
    }

    ReturnCode queryInterface(const gmpi::api::Guid* iid, void** returnInterface) override
    {
        *returnInterface = {};
        GMPI_QUERYINTERFACE(drawing::api::IBitmapRenderTarget);
        GMPI_QUERYINTERFACE(drawing::api::IDeviceContext);
        GMPI_QUERYINTERFACE(drawing::api::IResource);
        return ReturnCode::NoSupport;
    }
    GMPI_REFCOUNT;

private:
    drawing::PathGeometry createRoundedRectGeometry(const drawing::RoundedRect* rr)
    {
        drawing::Factory factoryWrapper;
        this->getFactory(drawing::AccessPtr::put(factoryWrapper));

        const auto& r = rr->rect;
        const float rx = (std::min)(rr->radiusX, (r.right - r.left) * 0.5f);
        const float ry = (std::min)(rr->radiusY, (r.bottom - r.top) * 0.5f);
        const drawing::Size radius{ rx, ry };

        auto geometry = factoryWrapper.createPathGeometry();
        auto sink = geometry.open();

        sink.beginFigure({ r.left + rx, r.top }, drawing::FigureBegin::Filled);
        sink.addLine({ r.right - rx, r.top });
        sink.addArc({ { r.right, r.top + ry }, radius });
        sink.addLine({ r.right, r.bottom - ry });
        sink.addArc({ { r.right - rx, r.bottom }, radius });
        sink.addLine({ r.left + rx, r.bottom });
        sink.addArc({ { r.left, r.bottom - ry }, radius });
        sink.addLine({ r.left, r.top + ry });
        sink.addArc({ { r.left + rx, r.top }, radius });
        sink.endFigure(drawing::FigureEnd::Closed);
        sink.close();

        return geometry;
    }
};

// ---------------------------------------------------------------------------
// Factory
// ---------------------------------------------------------------------------
class Factory final : public drawing::api::IFactory
{
public:
    ReturnCode createPathGeometry(drawing::api::IPathGeometry** returnPathGeometry) override
    {
        *returnPathGeometry = new PathGeometry(this);
        return ReturnCode::Ok;
    }

    ReturnCode createTextFormat(const char*, drawing::FontWeight, drawing::FontStyle, drawing::FontStretch, float, int32_t, drawing::api::ITextFormat** returnTextFormat) override
    {
        *returnTextFormat = {};
        return ReturnCode::NoSupport; // milestone 8 (text)
    }

    ReturnCode createImage(int32_t width, int32_t height, int32_t /*flags*/, drawing::api::IBitmap** returnBitmap) override
    {
        *returnBitmap = {};
        try
        {
            *returnBitmap = new Bitmap(this, width, height);
        }
        catch (...)
        {
            return ReturnCode::Fail; // allocation failure must not cross the ABI
        }
        return ReturnCode::Ok;
    }

    ReturnCode loadImageU(const char*, drawing::api::IBitmap** returnBitmap) override
    {
        *returnBitmap = {};
        return ReturnCode::NoSupport; // milestone 6 (bitmaps)
    }

    ReturnCode createStrokeStyle(const drawing::StrokeStyleProperties* strokeStyleProperties, const float* dashes, int32_t dashesCount, drawing::api::IStrokeStyle** returnStrokeStyle) override
    {
        *returnStrokeStyle = new se::generic_graphics::StrokeStyle(this, strokeStyleProperties, dashes, dashesCount);
        return ReturnCode::Ok;
    }

    ReturnCode getFontFamilyName(int32_t, gmpi::api::IString*) override
    {
        return ReturnCode::NoSupport; // milestone 8 (text)
    }

    ReturnCode getPlatformPixelFormat(int32_t* returnPixelFormat) override
    {
        *returnPixelFormat = drawing::api::IBitmapPixels::RGBA_16f;
        return ReturnCode::Ok;
    }

    ReturnCode createRichTextFormat(const char*, float, const char*, int32_t, drawing::TextAlignment, drawing::ParagraphAlignment, drawing::WordWrapping, float, float, drawing::api::IRichTextFormat** returnRichTextFormat) override
    {
        *returnRichTextFormat = {};
        return ReturnCode::NoSupport; // milestone 8 (text)
    }

    ReturnCode createCpuRenderTarget(drawing::SizeU size, int32_t flags, drawing::api::IBitmapRenderTarget** returnBitmapRenderTarget, float /*dpi*/) override
    {
        *returnBitmapRenderTarget = {};

        // Only the default fp16 target for now (Mask / SRGBPixels: milestone 6).
        constexpr int32_t unsupported =
            int32_t(drawing::BitmapRenderTargetFlags::Mask) | int32_t(drawing::BitmapRenderTargetFlags::SRGBPixels);
        if (flags & unsupported)
            return ReturnCode::NoSupport;

        try
        {
            *returnBitmapRenderTarget = new RenderTarget(this, size); // refcount born at 1; caller owns it
        }
        catch (...)
        {
            return ReturnCode::Fail; // allocation failure must not cross the ABI
        }
        return ReturnCode::Ok;
    }

    GMPI_QUERYINTERFACE_METHOD(drawing::api::IFactory);
    GMPI_REFCOUNT_NO_DELETE; // typically owned on the stack by the host
};

} // namespace cpugfx
} // namespace gmpi
