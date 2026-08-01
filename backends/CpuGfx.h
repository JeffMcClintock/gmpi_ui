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
// Solid color brush. GMPI Color is linear, non-premultiplied.
// ---------------------------------------------------------------------------
class SolidColorBrush final : public drawing::api::ISolidColorBrush
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
// PathGeometry: flattened polygons in local space.
// Curves never reach the rasterizer: the Gfx_base sink reduces arcs to cubics
// and cubics to line segments (AGG curve4_div); this sink only records points.
// ---------------------------------------------------------------------------
struct Figure
{
    std::vector<drawing::Point> points;
    bool filled{ true }; // FigureBegin::Filled; hollow figures are never filled (D2D semantics)
};

class PathGeometry final : public drawing::api::IPathGeometry
{
public:
    drawing::api::IFactory* factory{};
    std::vector<Figure> figures;
    drawing::FillMode fillMode{ drawing::FillMode::Alternate }; // matches the D2D sink default

    PathGeometry(drawing::api::IFactory* pfactory) : factory(pfactory) {}

    ReturnCode open(drawing::api::IGeometrySink** returnGeometrySink) override;

    ReturnCode strokeContainsPoint(drawing::Point, float, drawing::api::IStrokeStyle*, const drawing::Matrix3x2*, bool* returnContains) override
    {
        *returnContains = false;
        return ReturnCode::NoSupport; // milestone 3 (stroker)
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

    ReturnCode getWidenedBounds(float, drawing::api::IStrokeStyle*, const drawing::Matrix3x2*, drawing::Rect*) override
    {
        return ReturnCode::NoSupport; // milestone 3 (stroker)
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
    std::vector<drawing::Point> devicePoints;
    std::vector<char> figureValid;

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

    ReturnCode createGradientstopCollection(const drawing::Gradientstop*, uint32_t, drawing::ExtendMode, drawing::api::IGradientstopCollection** returnCollection) override
    {
        *returnCollection = {};
        return ReturnCode::NoSupport; // milestone 5 (gradients)
    }

    ReturnCode createLinearGradientBrush(const drawing::LinearGradientBrushProperties*, const drawing::BrushProperties*, drawing::api::IGradientstopCollection*, drawing::api::ILinearGradientBrush** returnBrush) override
    {
        *returnBrush = {};
        return ReturnCode::NoSupport; // milestone 5 (gradients)
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
        auto* solid = dynamic_cast<SolidColorBrush*>(brush);
        if (!path || !solid)
            return ReturnCode::NoSupport; // other brush types: milestones 5/6
        if (opacityBrush)
            return ReturnCode::NoSupport; // masked fills: fail loudly rather than render wrong

        // A non-finite color would poison even zero-coverage pixels through
        // the blend (NaN * 0 = NaN); render nothing instead.
        if (!(std::isfinite(solid->color.r) && std::isfinite(solid->color.g) &&
              std::isfinite(solid->color.b) && std::isfinite(solid->color.a) &&
              std::isfinite(solid->opacity)))
            return ReturnCode::Ok;

        if (path->figures.empty())
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
            for (const auto& fig : path->figures)
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
        for (const auto& fig : path->figures)
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

        // 4. Per row: prefix-sum to coverage, then blend.
        //    Premultiplied source; over with coverage:
        //    dst = src*cov + dst*(1 - srcA*cov)
        const float sa = solid->color.a * solid->opacity;
        const float sr = solid->color.r * sa;
        const float sg = solid->color.g * sa;
        const float sb = solid->color.b * sa;
        const bool nonzero = (path->fillMode == drawing::FillMode::Winding);

        // Chunk-aligned blend span; row padding absorbs the rounding (cov = 0 there).
        const int ax0 = ix0 & ~3;
        const int ax1 = (std::min)(s.stridePixels, (ix1 + 3) & ~3);
        const int spanPx = ax1 - ax0;

        covBuf.assign(size_t(s.stridePixels), 0.0f);
        rowBuf.resize(size_t(spanPx) * 4);

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

            float* d = rowBuf.data();
            const float* cv = cov + ax0;
            for (int i = 0; i < spanPx; ++i, d += 4)
            {
                const float c = cv[i];
                const float k = 1.0f - sa * c;
                d[0] = sr * c + d[0] * k;
                d[1] = sg * c + d[1] * k;
                d[2] = sb * c + d[2] * k;
                d[3] = sa * c + d[3] * k;
            }

            storeSpan(rowBuf.data(), p, spanPx * 4);

            // reset the coverage cells we wrote
            std::memset(cov + ix0, 0, size_t(w) * sizeof(float));
        }

        return ReturnCode::Ok;
    }

    ReturnCode drawGeometry(drawing::api::IPathGeometry*, drawing::api::IBrush*, float, drawing::api::IStrokeStyle*) override
    {
        return ReturnCode::NoSupport; // milestone 3 (stroker)
    }

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
