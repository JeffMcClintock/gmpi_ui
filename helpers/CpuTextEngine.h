#pragma once

/*
#include "helpers/CpuTextEngine.h"
*/

// Text for the software backend, on HarfBuzz. This is the ONLY file that
// includes hb.h — backends/CpuGfx.h takes an ICpuTextEngine and knows nothing
// about shaping, fonts, or platforms.
//
// Glyphs are paths. HarfBuzz's draw callbacks emit move/line/quadratic/cubic,
// which map one-to-one onto the drawing API's geometry sink, so text is filled
// by the ordinary path pipeline and inherits its antialiasing, transforms,
// clipping and brushes. Nothing here touches a pixel.
//
// Usage:
//     gmpi::cpugfx::Factory factory;
//     gmpi::drawing::CpuTextEngine textEngine(gmpi::drawing::findFont);
//     factory.textEngine = &textEngine;
//
// Stage A: shaping, metrics, word wrap, alignment. Left-to-right only, one
// font per format (no fallback yet), monochrome glyphs.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include <hb.h>
#include <hb-ot.h>

#include "../Drawing.h"
#include "../backends/CpuGfx.h"
#include "../backends/MarkdownParser.h" // shared with the platform backends
#include "DecodedImage.h"
#include "FontFile.h"
#include "TextSegmentation.h"

namespace gmpi { namespace drawing {

namespace detail {

// RAII for HarfBuzz's refcounted handles.
template <class T, void (*Destroy)(T*)>
struct HbHandle
{
    T* ptr{};
    HbHandle() = default;
    explicit HbHandle(T* p) : ptr(p) {}
    ~HbHandle() { if (ptr) Destroy(ptr); }
    HbHandle(const HbHandle&) = delete;
    HbHandle& operator=(const HbHandle&) = delete;
    HbHandle(HbHandle&& other) noexcept : ptr(other.ptr) { other.ptr = nullptr; }
    HbHandle& operator=(HbHandle&& other) noexcept
    {
        if (this != &other) { if (ptr) Destroy(ptr); ptr = other.ptr; other.ptr = nullptr; }
        return *this;
    }
    T* get() const { return ptr; }
    explicit operator bool() const { return ptr != nullptr; }
};

using HbBlob = HbHandle<hb_blob_t, hb_blob_destroy>;
using HbFace = HbHandle<hb_face_t, hb_face_destroy>;
using HbFont = HbHandle<hb_font_t, hb_font_destroy>;
using HbBuffer = HbHandle<hb_buffer_t, hb_buffer_destroy>;

// A loaded font face, shared by every text format that uses it.
struct FontFace
{
    std::vector<uint8_t> bytes; // must outlive the blob
    HbBlob blob;
    HbFace face;
    HbFont unscaledFont;        // font at units-per-em, for metrics and outlines
    unsigned unitsPerEm{ 1000 };

    bool hasColorPng{};   // CBDT/sbix: Apple Color Emoji, older Noto Color Emoji
    bool hasColorPaint{}; // COLRv1: Segoe UI Emoji, current Noto Color Emoji
    std::string name;     // what the provider resolved to; for diagnostics

    bool init(FontData&& data)
    {
        name = data.resolvedName;
        bytes = std::move(data.bytes);
        blob = HbBlob(hb_blob_create(reinterpret_cast<const char*>(bytes.data()),
                                     unsigned(bytes.size()), HB_MEMORY_MODE_READONLY, nullptr, nullptr));
        if (!blob || blob.get() == hb_blob_get_empty())
            return false;

        face = HbFace(hb_face_create(blob.get(), data.faceIndex));
        if (!face || face.get() == hb_face_get_empty())
            return false;

        unitsPerEm = hb_face_get_upem(face.get());
        if (unitsPerEm == 0)
            unitsPerEm = 1000;

        unscaledFont = HbFont(hb_font_create(face.get()));
        if (!unscaledFont)
            return false;
        hb_font_set_scale(unscaledFont.get(), int(unitsPerEm), int(unitsPerEm));

        hasColorPng = hb_ot_color_has_png(face.get()) != 0;
        hasColorPaint = hb_ot_color_has_paint(face.get()) != 0;
        return true;
    }
};

// Font metrics in em units (divide by unitsPerEm, multiply by point size).
struct EmMetrics
{
    float ascent{}, descent{}, lineGap{};
    float capHeight{}, xHeight{};
    float underlinePosition{}, underlineThickness{};
    float strikethroughPosition{}, strikethroughThickness{};
};

inline float metricOr(hb_font_t* font, hb_ot_metrics_tag_t tag, float fallback)
{
    hb_position_t value{};
    if (hb_ot_metrics_get_position(font, tag, &value))
        return float(value);
    return fallback;
}

// Big-endian table readers. Font tables are always big-endian, whatever the
// host is; the lengths are checked so a truncated or absent table falls back
// rather than reading past the blob.
inline int16_t  beI16(const uint8_t* p) { return int16_t((uint16_t(p[0]) << 8) | p[1]); }
inline uint16_t beU16(const uint8_t* p) { return uint16_t((uint16_t(p[0]) << 8) | p[1]); }

// Borrow a font table, returning its bytes only if it is long enough to hold
// the field we are about to read. A short or absent table yields nullptr, so
// callers fall back rather than reading past the blob.
inline const uint8_t* tableBytes(const HbBlob& blob, unsigned needed)
{
    unsigned length{};
    const char* data = hb_blob_get_data(blob.get(), &length);
    return (data && length >= needed) ? reinterpret_cast<const uint8_t*>(data) : nullptr;
}

// Vertical metrics, read the way Direct2D reads them.
//
// This does NOT go through hb_font_get_h_extents, which prefers hhea. That is a
// legitimate choice, just a different one from the platform backends, and the
// difference is not academic: for Calibri and Consolas hhea ascent+descent is
// exactly one em while OS/2 usWin says 1.2207 em and 1.1709 em, so the two
// backends disagreed by 22% and 17% on the very quantity body-height font
// scaling divides by. Arial happens to have hhea == usWin, which is why the
// original cross-backend test agreed and proved nothing.
//
// The rule below is not from documentation - it was measured against
// DirectWrite on five fonts covering both branches (CpuVsD2D
// .TextMetricsAgreeWithDirectWrite), and reproduces it to the digit:
//
//   USE_TYPO_METRICS set  ->  sTypoAscender / -sTypoDescender / sTypoLineGap
//   otherwise             ->  usWinAscent / usWinDescent, and the GDI
//                             "external leading" line gap:
//                               max(0, hhea.lineGap - (winSum - hheaSum))
//
// Verified: Arial/Verdana (hhea == usWin), Calibri/Consolas (they differ),
// Bahnschrift (sets the bit; typo 1.0000 em vs usWin 1.2002 em, and Direct2D
// follows typo).
inline bool readVerticalMetrics(hb_face_t* face, EmMetrics& m)
{
    constexpr unsigned kOs2Min = 78;  // through usWinDescent
    constexpr unsigned kHheaMin = 10; // through lineGap

    const HbBlob os2(hb_face_reference_table(face, HB_TAG('O', 'S', '/', '2')));
    const uint8_t* o = tableBytes(os2, kOs2Min);
    if (!o)
        return false;

    const uint16_t fsSelection = beU16(o + 62);
    constexpr uint16_t kUseTypoMetrics = 1u << 7;

    if (fsSelection & kUseTypoMetrics)
    {
        m.ascent  =  float(beI16(o + 68)); // sTypoAscender
        m.descent = -float(beI16(o + 70)); // sTypoDescender (negative in the file)
        m.lineGap =  float(beI16(o + 72)); // sTypoLineGap
        return m.ascent > 0.0f;
    }

    const float winAscent  = float(beU16(o + 74));
    const float winDescent = float(beU16(o + 76));
    m.ascent = winAscent;
    m.descent = winDescent;
    m.lineGap = 0.0f;

    const HbBlob hhea(hb_face_reference_table(face, HB_TAG('h', 'h', 'e', 'a')));
    if (const uint8_t* h = tableBytes(hhea, kHheaMin))
    {
        const float hheaAscender  = float(beI16(h + 4));
        const float hheaDescender = float(beI16(h + 6));
        const float hheaLineGap   = float(beI16(h + 8));
        // Leading already covered by the (usually larger) win box is not added
        // again - otherwise Calibri would gain a phantom 452-unit gap that
        // Direct2D does not report.
        const float excess = (winAscent + winDescent) - (hheaAscender - hheaDescender);
        m.lineGap = (std::max)(0.0f, hheaLineGap - excess);
    }

    return m.ascent > 0.0f;
}

inline EmMetrics readMetrics(const FontFace& face)
{
    hb_font_t* font = face.unscaledFont.get();
    const float upem = float(face.unitsPerEm);

    EmMetrics m;
    if (!readVerticalMetrics(face.face.get(), m))
    {
        // No usable OS/2 (some bitmap-only and CFF faces). HarfBuzz's own
        // extents are the next best thing.
        hb_font_extents_t extents{};
        hb_font_get_h_extents(font, &extents);
        // HarfBuzz reports ascender up-positive and descender down-negative; the
        // drawing API wants both as positive distances from the baseline.
        m.ascent = float(extents.ascender);
        m.descent = -float(extents.descender);
        m.lineGap = float(extents.line_gap);
    }

    if (m.ascent <= 0.0f) m.ascent = 0.8f * upem;
    if (m.descent <= 0.0f) m.descent = 0.2f * upem;
    if (m.lineGap < 0.0f) m.lineGap = 0.0f;

    m.capHeight = metricOr(font, HB_OT_METRICS_TAG_CAP_HEIGHT, 0.7f * upem);
    m.xHeight = metricOr(font, HB_OT_METRICS_TAG_X_HEIGHT, 0.5f * upem);
    m.underlinePosition = metricOr(font, HB_OT_METRICS_TAG_UNDERLINE_OFFSET, -0.1f * upem);
    m.underlineThickness = metricOr(font, HB_OT_METRICS_TAG_UNDERLINE_SIZE, 0.05f * upem);
    m.strikethroughPosition = metricOr(font, HB_OT_METRICS_TAG_STRIKEOUT_OFFSET, 0.25f * upem);
    m.strikethroughThickness = metricOr(font, HB_OT_METRICS_TAG_STRIKEOUT_SIZE, 0.05f * upem);

    // Some fonts omit these; a zero would silently break layouts built on them.
    if (m.capHeight <= 0.0f) m.capHeight = 0.7f * upem;
    if (m.xHeight <= 0.0f) m.xHeight = 0.5f * upem;
    return m;
}

// Feeds HarfBuzz's glyph outline callbacks straight into the drawing API's
// geometry sink. This is the whole of "glyph rasterization" here: none.
struct GlyphOutlineSink
{
    api::IGeometrySink* sink{};
    // Separate axes because font space is y-up and this API is y-down. Text
    // sets scaleY negative to flip here; COLRv1 painting sets both to 1 and
    // lets the context transform do the flip, because HarfBuzz's paint
    // transforms operate in font space and must compose BEFORE it.
    float scaleX{}, scaleY{};
    float originX{}, originY{};
    // Synthetic italic: the tangent of the lean, applied about the baseline.
    // Selawik ships no italic face, and DirectWrite slants the regular one
    // rather than declining - the reference images show exactly that.
    float shearX{};
    bool figureOpen{};

    Point map(float x, float y) const
    {
        // dy is negative above the baseline (device y grows downward), so
        // subtracting shears the top of the glyph to the right.
        const float dy = y * scaleY;
        return { originX + x * scaleX - shearX * dy, originY + dy };
    }
};

inline void glyphMoveTo(hb_draw_funcs_t*, void* userData, hb_draw_state_t*,
                        float toX, float toY, void*)
{
    auto* s = static_cast<GlyphOutlineSink*>(userData);
    if (s->figureOpen)
        s->sink->endFigure(FigureEnd::Closed);
    s->sink->beginFigure(s->map(toX, toY), FigureBegin::Filled);
    s->figureOpen = true;
}

inline void glyphLineTo(hb_draw_funcs_t*, void* userData, hb_draw_state_t*,
                        float toX, float toY, void*)
{
    auto* s = static_cast<GlyphOutlineSink*>(userData);
    s->sink->addLine(s->map(toX, toY));
}

inline void glyphQuadraticTo(hb_draw_funcs_t*, void* userData, hb_draw_state_t*,
                             float controlX, float controlY, float toX, float toY, void*)
{
    auto* s = static_cast<GlyphOutlineSink*>(userData);
    const QuadraticBezierSegment segment{ s->map(controlX, controlY), s->map(toX, toY) };
    s->sink->addQuadraticBezier(&segment);
}

inline void glyphCubicTo(hb_draw_funcs_t*, void* userData, hb_draw_state_t*,
                         float control1X, float control1Y, float control2X, float control2Y,
                         float toX, float toY, void*)
{
    auto* s = static_cast<GlyphOutlineSink*>(userData);
    const BezierSegment segment{ s->map(control1X, control1Y), s->map(control2X, control2Y), s->map(toX, toY) };
    s->sink->addBezier(&segment);
}

inline void glyphClosePath(hb_draw_funcs_t*, void* userData, hb_draw_state_t*, void*)
{
    auto* s = static_cast<GlyphOutlineSink*>(userData);
    if (s->figureOpen)
    {
        s->sink->endFigure(FigureEnd::Closed);
        s->figureOpen = false;
    }
}

inline hb_draw_funcs_t* glyphDrawFuncs(); // defined below; used by the clip painter

// True when the glyph produces at least one outline segment (every outline
// starts with a move_to). A face can map a codepoint in its cmap yet draw
// NOTHING — macOS's Reserved UI fonts carry cmap and metrics but keep their
// outlines in a private format, so text shaped with them gets real advances
// and renders as blank space.
inline bool glyphHasOutline(hb_font_t* font, hb_codepoint_t glyph)
{
    static hb_draw_funcs_t* funcs = [] {
        hb_draw_funcs_t* f = hb_draw_funcs_create();
        hb_draw_funcs_set_move_to_func(f,
            [](hb_draw_funcs_t*, void* any, hb_draw_state_t*, float, float, void*) {
                *static_cast<bool*>(any) = true;
            }, nullptr, nullptr);
        hb_draw_funcs_make_immutable(f);
        return f;
    }();
    bool any{};
    hb_font_draw_glyph(font, glyph, funcs, &any);
    return any;
}

// Text-weight model. This renderer blends EVERYTHING in linear premultiplied
// space (cpugfx::blendPixel); perceived glyph weight is matched to Direct2D
// by warping the coverage values once per draw call — the same place
// DirectWrite applies its own gamma/contrast correction — so no transfer
// function survives in any per-pixel blend.
//
// The dark anchor is DERIVED, not tuned. Direct2D grayscale text contains
// EXACTLY seventeen grey levels (coverage quantised to k/16 by the
// rasterizer), and black-on-white renders to the measured bytes
// {255,238,222,208,194,180,167,154,142,130,118,105,92,77,62,41,0}
// (gimpi_ui_tests reference images, byte-exact). The alpha a LINEAR blend of
// black over white needs to reproduce those bytes is
// 1 - srgbToLinear(byte/255) — the values below. A plain power curve misfits
// this by up to 4.5%, so the table is kept as data.
inline constexpr float kTextWarpDark[17] = {
    0.0f,       0.1450074f, 0.2695393f, 0.3692429f, 0.4605205f, 0.5435890f,
    0.6135706f, 0.6768568f, 0.7295022f, 0.7767720f, 0.8188358f, 0.8587367f,
    0.8929769f, 0.9257864f, 0.9518282f, 0.9778261f, 1.0f,
};

// How strongly kTextWarpDark applies, in [0,1]: 1 = the full curve, 0 =
// identity (pure linear coverage). Two factors, both measured:
//   * Foreground luminance. The correction is for DARK glyphs only: measured
//     at 10-12px against D2D, black-on-light needs the full curve (linear
//     coverage reads 27% too light) while white-on-dark is already at
//     reference weight — slightly past it — with NO correction. So strength
//     scales with darkness, reaching zero for white text.
//   * Glyph size, in device pixels. Past ~2px stems the antialiased fringe
//     is a small fraction of the glyph, weight no longer reads differently,
//     and unwarped linear coverage draws curves and diagonals most
//     faithfully — so the warp fades to identity across a band rather than
//     switching at a threshold (the editor zooms continuously, and a hard
//     cut would make text weight pop mid-zoom).
inline float textWarpStrength(float foregroundLuminance, float emPixels)
{
    constexpr float kWarpFadeStartPx = 16.0f;
    constexpr float kWarpFadeEndPx = 32.0f;

    const float darkness = 1.0f - std::clamp(foregroundLuminance, 0.0f, 1.0f);
    const float sizeFade = std::clamp(
        (kWarpFadeEndPx - emPixels) / (kWarpFadeEndPx - kWarpFadeStartPx), 0.0f, 1.0f);
    return darkness * sizeFade;
}

// ---------------------------------------------------------------------------
// COLRv1 painting.
//
// HarfBuzz does not hand back an image for a colour glyph; it drives a
// callback interface, and those callbacks are very nearly a description of
// primitives this renderer already has. Measured against Segoe UI Emoji, the
// operations actually used are: push/pop transform, push_clip_glyph,
// push_clip_rectangle, pop_clip, color, linear_gradient and radial_gradient.
// Sweep gradients, groups and blend modes are never emitted by that font, so
// they are declined rather than approximated — a font that needs them will
// simply not paint, which is visible, instead of painting something wrong.
//
// Everything below draws through the PUBLIC drawing API.
// ---------------------------------------------------------------------------
struct PaintState
{
    api::IDeviceContext* context{};
    api::IFactory* factory{};
    Matrix3x2 baseTransform;             // font units -> device, at the pen
    std::vector<Matrix3x2> transformStack;
    int clipDepth{};
    bool failed{};

    Matrix3x2 current() const { return transformStack.empty() ? baseTransform : transformStack.back(); }

    void apply() const
    {
        const auto m = current();
        context->setTransform(&m);
    }

    // Fill everything the current clip allows. COLRv1's paint operations mean
    // "cover the current clip with this paint", so the shape comes from the
    // clip and the geometry is just a big enough rectangle.
    void fillClip(api::IBrush* brush) const
    {
        Rect clip{};
        if (context->getAxisAlignedClip(&clip) != ReturnCode::Ok)
            return;
        context->fillRectangle(&clip, brush);
    }
};

inline Color fromHbColor(hb_color_t c)
{
    // HarfBuzz colours are 8-bit sRGB, unpremultiplied; ours are linear.
    return { SRGBPixelToLinear(hb_color_get_red(c)),
             SRGBPixelToLinear(hb_color_get_green(c)),
             SRGBPixelToLinear(hb_color_get_blue(c)),
             hb_color_get_alpha(c) * (1.0f / 255.0f) };
}

// Turn a HarfBuzz colour line into a stop collection.
inline gmpi::shared_ptr<api::IGradientstopCollection> makeStops(api::IDeviceContext* context, hb_color_line_t* colorLine)
{
    gmpi::shared_ptr<api::IGradientstopCollection> collection;
    if (!colorLine)
        return collection;

    unsigned count = hb_color_line_get_color_stops(colorLine, 0, nullptr, nullptr);
    if (count == 0)
        return collection;
    std::vector<hb_color_stop_t> hbStops(count);
    hb_color_line_get_color_stops(colorLine, 0, &count, hbStops.data());

    std::vector<Gradientstop> stops;
    stops.reserve(count);
    for (const auto& s : hbStops)
        stops.push_back({ s.offset, fromHbColor(s.color) });

    ExtendMode extend = ExtendMode::Clamp;
    switch (hb_color_line_get_extend(colorLine))
    {
    case HB_PAINT_EXTEND_REPEAT:  extend = ExtendMode::Wrap;   break;
    case HB_PAINT_EXTEND_REFLECT: extend = ExtendMode::Mirror; break;
    default: break;
    }

    api::IGradientstopCollection* raw{};
    if (context->createGradientstopCollection(stops.data(), uint32_t(stops.size()), extend, &raw) == ReturnCode::Ok)
        collection.attach(raw);
    return collection;
}

inline void paintPushTransform(hb_paint_funcs_t*, void* data,
                               float xx, float yx, float xy, float yy, float dx, float dy, void*)
{
    auto* s = static_cast<PaintState*>(data);
    // HarfBuzz gives column-major xx,yx,xy,yy,dx,dy; this API is row-major.
    const Matrix3x2 m{ xx, yx, xy, yy, dx, dy };
    s->transformStack.push_back(m * s->current());
    s->apply();
}

inline void paintPopTransform(hb_paint_funcs_t*, void* data, void*)
{
    auto* s = static_cast<PaintState*>(data);
    if (!s->transformStack.empty())
        s->transformStack.pop_back();
    s->apply();
}

inline void paintPushClipRectangle(hb_paint_funcs_t*, void* data,
                                   float xmin, float ymin, float xmax, float ymax, void*)
{
    auto* s = static_cast<PaintState*>(data);
    // Font-space coordinates: the context transform flips y, and
    // pushAxisAlignedClip takes the bounds of all four transformed corners, so
    // a y-inverted rectangle is handled correctly.
    const Rect r{ xmin, ymin, xmax, ymax };
    s->context->pushAxisAlignedClip(&r);
    ++s->clipDepth;
}

inline void paintPushClipGlyph(hb_paint_funcs_t*, void* data, hb_codepoint_t glyph, hb_font_t* font, void*)
{
    auto* s = static_cast<PaintState*>(data);

    api::IPathGeometry* rawGeometry{};
    if (s->factory->createPathGeometry(&rawGeometry) != ReturnCode::Ok || !rawGeometry)
    {
        s->failed = true;
        // Keep push/pop balanced (see below): an empty clip is also the right
        // result when no geometry could be created.
        const Rect empty{ 0.0f, 0.0f, 0.0f, 0.0f };
        s->context->pushAxisAlignedClip(&empty);
        ++s->clipDepth;
        return;
    }
    gmpi::shared_ptr<api::IPathGeometry> geometry;
    geometry.attach(rawGeometry);

    api::IGeometrySink* rawSink{};
    if (geometry->open(&rawSink) != ReturnCode::Ok || !rawSink)
    {
        s->failed = true;
        // Push SOMETHING so the matching pop_clip stays balanced: an empty
        // clip is the correct answer anyway, since no shape could be built.
        const Rect empty{ 0.0f, 0.0f, 0.0f, 0.0f };
        s->context->pushAxisAlignedClip(&empty);
        ++s->clipDepth;
        return;
    }
    gmpi::shared_ptr<api::IGeometrySink> sink;
    sink.attach(rawSink);
    sink->setFillMode(FillMode::Winding);

    // The glyph outline is in font units, and the context transform already
    // maps those to device, so emit it unscaled.
    GlyphOutlineSink outline;
    outline.sink = sink.get();
    outline.scaleX = 1.0f;
    outline.scaleY = 1.0f; // the context transform carries the y-flip
    outline.originX = 0.0f;
    outline.originY = 0.0f;
    outline.figureOpen = false;
    hb_font_draw_glyph(font, glyph, glyphDrawFuncs(), &outline);
    if (outline.figureOpen)
        sink->endFigure(FigureEnd::Closed);
    sink->close();

    // Every path through this function pushes exactly one clip, so the font's
    // matching pop_clip always has something to pop. A backend that declines
    // geometry clipping falls back to an empty clip rather than desynchronising
    // the stack and leaking a clip into later drawing.
    if (s->context->pushClipGeometry(geometry.get()) != ReturnCode::Ok)
    {
        const Rect empty{ 0.0f, 0.0f, 0.0f, 0.0f };
        s->context->pushAxisAlignedClip(&empty);
    }
    ++s->clipDepth;
}

inline void paintPopClip(hb_paint_funcs_t*, void* data, void*)
{
    auto* s = static_cast<PaintState*>(data);
    if (s->clipDepth > 0)
    {
        s->context->popAxisAlignedClip();
        --s->clipDepth;
    }
}

inline void paintColor(hb_paint_funcs_t*, void* data, hb_bool_t, hb_color_t color, void*)
{
    auto* s = static_cast<PaintState*>(data);
    const auto c = fromHbColor(color);
    api::ISolidColorBrush* raw{};
    if (s->context->createSolidColorBrush(&c, nullptr, &raw) != ReturnCode::Ok || !raw)
        return;
    gmpi::shared_ptr<api::ISolidColorBrush> brush;
    brush.attach(raw);
    s->fillClip(brush.get());
}

inline void paintLinearGradient(hb_paint_funcs_t*, void* data, hb_color_line_t* colorLine,
                                float x0, float y0, float x1, float y1, float x2, float y2, void*)
{
    auto* s = static_cast<PaintState*>(data);
    auto stops = makeStops(s->context, colorLine);
    if (!stops)
        return;

    // COLRv1 gives p0, p1 and a rotation point p2. Project p1 onto the line
    // perpendicular to (p2 - p0) through p0, which is the equivalent simple
    // two-point gradient.
    const float dx = x2 - x0, dy = y2 - y0;
    float ex = x1 - x0, ey = y1 - y0;
    const float lenSq = dx * dx + dy * dy;
    if (lenSq > 0.0f)
    {
        const float k = (ex * dx + ey * dy) / lenSq;
        ex -= dx * k;
        ey -= dy * k;
    }

    LinearGradientBrushProperties props{};
    props.startPoint = { x0, y0 };                  // font space; the transform flips y
    props.endPoint = { x0 + ex, y0 + ey };
    const BrushProperties brushProps{};

    api::ILinearGradientBrush* raw{};
    if (s->context->createLinearGradientBrush(&props, &brushProps, stops.get(), &raw) != ReturnCode::Ok || !raw)
        return;
    gmpi::shared_ptr<api::ILinearGradientBrush> brush;
    brush.attach(raw);
    s->fillClip(brush.get());
}

inline void paintRadialGradient(hb_paint_funcs_t*, void* data, hb_color_line_t* colorLine,
                                float x0, float y0, float r0, float x1, float y1, float r1, void*)
{
    auto* s = static_cast<PaintState*>(data);
    auto stops = makeStops(s->context, colorLine);
    if (!stops)
        return;

    // COLRv1's radial gradient is the two-circle form. This API's radial brush
    // is the focal form: outer circle plus an origin offset, which matches when
    // the inner circle is small — the common case in emoji.
    RadialGradientBrushProperties props{};
    props.center = { x1, y1 };
    props.gradientOriginOffset = { x0 - x1, y0 - y1 };
    props.radiusX = r1 > 0.0f ? r1 : r0;
    props.radiusY = props.radiusX;
    if (!(props.radiusX > 0.0f))
        return;
    const BrushProperties brushProps{};

    api::IRadialGradientBrush* raw{};
    if (s->context->createRadialGradientBrush(&props, &brushProps, stops.get(), &raw) != ReturnCode::Ok || !raw)
        return;
    gmpi::shared_ptr<api::IRadialGradientBrush> brush;
    brush.attach(raw);
    s->fillClip(brush.get());
}

inline hb_paint_funcs_t* glyphPaintFuncs()
{
    static hb_paint_funcs_t* funcs = [] {
        hb_paint_funcs_t* f = hb_paint_funcs_create();
        hb_paint_funcs_set_push_transform_func(f, paintPushTransform, nullptr, nullptr);
        hb_paint_funcs_set_pop_transform_func(f, paintPopTransform, nullptr, nullptr);
        hb_paint_funcs_set_push_clip_glyph_func(f, paintPushClipGlyph, nullptr, nullptr);
        hb_paint_funcs_set_push_clip_rectangle_func(f, paintPushClipRectangle, nullptr, nullptr);
        hb_paint_funcs_set_pop_clip_func(f, paintPopClip, nullptr, nullptr);
        hb_paint_funcs_set_color_func(f, paintColor, nullptr, nullptr);
        hb_paint_funcs_set_linear_gradient_func(f, paintLinearGradient, nullptr, nullptr);
        hb_paint_funcs_set_radial_gradient_func(f, paintRadialGradient, nullptr, nullptr);
        // Deliberately unset: sweep_gradient, push_group/pop_group, image.
        // Segoe UI Emoji emits none of them; a font that needs them will paint
        // nothing rather than something wrong.
        hb_paint_funcs_make_immutable(f);
        return f;
    }();
    return funcs;
}

inline hb_draw_funcs_t* glyphDrawFuncs()
{
    static hb_draw_funcs_t* funcs = [] {
        hb_draw_funcs_t* f = hb_draw_funcs_create();
        hb_draw_funcs_set_move_to_func(f, glyphMoveTo, nullptr, nullptr);
        hb_draw_funcs_set_line_to_func(f, glyphLineTo, nullptr, nullptr);
        hb_draw_funcs_set_quadratic_to_func(f, glyphQuadraticTo, nullptr, nullptr);
        hb_draw_funcs_set_cubic_to_func(f, glyphCubicTo, nullptr, nullptr);
        hb_draw_funcs_set_close_path_func(f, glyphClosePath, nullptr, nullptr);
        hb_draw_funcs_make_immutable(f);
        return f;
    }();
    return funcs;
}

} // namespace detail

// ---------------------------------------------------------------------------
// TextFormat
// ---------------------------------------------------------------------------
class CpuTextFormat final : public api::ITextFormat
{
public:
    std::shared_ptr<detail::FontFace> face;
    detail::EmMetrics em;
    float fontSize{};          // device units per em, after the FontFlags rescale
    TextAlignment textAlignment{ TextAlignment::Leading };
    ParagraphAlignment paragraphAlignment{ ParagraphAlignment::Near };
    WordWrapping wordWrapping{ WordWrapping::Wrap };
    float lineSpacing{ -1.0f };
    float baseline{};

    // One run of glyphs from a single font. Mixed-script text needs several,
    // because no font covers Latin, CJK and emoji.
    struct GlyphRun
    {
        std::shared_ptr<detail::FontFace> face;
        std::vector<uint32_t> glyphs;
        std::vector<float> advances;   // device units, per glyph
        std::vector<float> offsetsX;
        std::vector<float> offsetsY;
        std::vector<uint32_t> clusters; // byte offset in the source, per glyph
        float scale{};                 // this face's units -> device
        float width{};

        // Rich text only; plain text leaves these at their defaults. A run is
        // the unit of styling as well as of font, because a change of either
        // ends it.
        float emSize{};                // device units per em, for decorations
        float obliqueShear{};          // tangent of the synthetic-italic angle
        bool strikethrough{};
    };

    struct Line
    {
        std::vector<GlyphRun> runs;
        float width{};

        // Zero means "ask the format" — plain text has one font throughout, so
        // every line is the same height. Rich text sets these per line, since a
        // heading is taller than the paragraph under it.
        float ascent{};
        float descent{};
        float advance{};
    };

    // A stretch of source text that one font covers.
    struct FontRunSpan
    {
        std::shared_ptr<detail::FontFace> face;
        uint32_t beginByte{};
        uint32_t endByte{};
    };

    // Called back to find a font covering a codepoint the primary font lacks.
    std::function<std::shared_ptr<detail::FontFace>(uint32_t)> fallbackFor;

    float scale() const { return fontSize / float(face->unitsPerEm); }
    float ascent() const { return em.ascent * scale(); }
    float descent() const { return em.descent * scale(); }

    // Baseline-to-baseline distance. It includes lineGap, matching DirectWrite:
    // measured on Arial at 20, each additional line advances by
    // ascent+descent+lineGap (22.998), not ascent+descent (22.344).
    // Metrics come from the PRIMARY font, so a line does not change height just
    // because one fallback glyph appeared in it.
    float lineAdvance() const
    {
        return lineSpacing >= 0.0f ? lineSpacing : (ascent() + descent() + em.lineGap * scale());
    }

    // Total height of n lines. There is no trailing line gap — DirectWrite's
    // height for n lines is (n-1) advances plus one body height.
    float blockHeight(size_t lines) const
    {
        if (lines == 0)
            return 0.0f;
        if (lineSpacing >= 0.0f)
            return float(lines) * lineSpacing;
        return float(lines - 1) * lineAdvance() + ascent() + descent();
    }

    float baselineOffset() const { return lineSpacing >= 0.0f ? baseline : ascent(); }

    static bool covers(const detail::FontFace& f, uint32_t codepoint)
    {
        hb_codepoint_t glyph{};
        return hb_font_get_nominal_glyph(f.unscaledFont.get(), codepoint, &glyph) != 0;
    }

    // Split the text into stretches each covered by one font. Cluster
    // boundaries are respected, so a ZWJ emoji sequence or a combining mark
    // never lands in a different run from its base.
    std::vector<FontRunSpan> itemize(std::string_view utf8) const
    {
        std::vector<FontRunSpan> spans;
        const auto cps = text::decodeUtf8(utf8);
        if (cps.empty())
            return spans;
        const auto clusterStarts = text::graphemeBoundaries(cps);

        std::shared_ptr<detail::FontFace> current;
        uint32_t spanBegin{};

        for (size_t i = 0; i < cps.size(); ++i)
        {
            if (!clusterStarts[i])
                continue; // mid-cluster: keep whatever font the cluster started with

            const uint32_t c = cps[i].value;
            const bool neutral = (c == ' ' || c == '\t' || text::isGraphemeExtender(c));

            // The PRIMARY font always wins where it can, which is what
            // DirectWrite's MapCharacters does: it is handed the base family at
            // every unmapped position and returns the base font whenever the
            // base covers the text. Preferring whichever font the previous
            // character used instead makes fallback sticky — and because
            // fallback faces cover ASCII (Segoe UI Emoji carries 0-9 and # for
            // keycaps, CJK fonts carry half-width Latin), one leading symbol
            // drags the entire rest of the string into the wrong font. Measured:
            // "<emoji> Hello 2024" itemised as a SINGLE Segoe UI Emoji run.
            std::shared_ptr<detail::FontFace> wanted;
            if (neutral)
            {
                wanted = current ? current : face; // spaces and marks join the run they follow
            }
            else if (covers(*face, c))
            {
                wanted = face;
            }
            else if (current && current != face && covers(*current, c))
            {
                wanted = current; // keep a fallback run together for its own script
            }
            else if (fallbackFor)
            {
                wanted = fallbackFor(c);
                if (!wanted)
                    wanted = face; // nothing covers it; the primary draws .notdef
            }
            else
            {
                wanted = face;
            }

            if (!current)
            {
                current = wanted;
                spanBegin = cps[i].byteOffset;
            }
            else if (wanted != current)
            {
                spans.push_back({ current, spanBegin, cps[i].byteOffset });
                current = wanted;
                spanBegin = cps[i].byteOffset;
            }
        }

        if (current)
            spans.push_back({ current, spanBegin, uint32_t(utf8.size()) });
        return spans;
    }

    // emSizeOverride <= 0 means "this format's own size", which is every plain
    // text call. Rich text passes the run's size, since a heading and the
    // paragraph under it are laid out by the same format.
    GlyphRun shapeSpan(const std::shared_ptr<detail::FontFace>& runFace,
                       std::string_view whole, uint32_t beginByte, uint32_t endByte,
                       float emSizeOverride = 0.0f) const
    {
        const float emSize = emSizeOverride > 0.0f ? emSizeOverride : fontSize;

        GlyphRun run;
        run.face = runFace;
        run.scale = emSize / float(runFace->unitsPerEm);
        run.emSize = emSize;
        if (endByte <= beginByte)
            return run;

        detail::HbFont font(hb_font_create(runFace->face.get()));
        if (!font)
            return run;
        hb_font_set_scale(font.get(), int(runFace->unitsPerEm), int(runFace->unitsPerEm));

        detail::HbBuffer buffer(hb_buffer_create());
        // Give HarfBuzz the whole string with an item range, so shaping sees
        // the surrounding context rather than a truncated fragment.
        hb_buffer_add_utf8(buffer.get(), whole.data(), int(whole.size()),
                           int(beginByte), int(endByte - beginByte));
        hb_buffer_set_direction(buffer.get(), HB_DIRECTION_LTR);
        hb_buffer_guess_segment_properties(buffer.get());
        hb_shape(font.get(), buffer.get(), nullptr, 0);

        unsigned count{};
        const hb_glyph_info_t* info = hb_buffer_get_glyph_infos(buffer.get(), &count);
        const hb_glyph_position_t* pos = hb_buffer_get_glyph_positions(buffer.get(), &count);

        run.glyphs.reserve(count);
        for (unsigned i = 0; i < count; ++i)
        {
            run.glyphs.push_back(info[i].codepoint);
            run.clusters.push_back(info[i].cluster);
            run.advances.push_back(float(pos[i].x_advance) * run.scale);
            run.offsetsX.push_back(float(pos[i].x_offset) * run.scale);
            run.offsetsY.push_back(float(pos[i].y_offset) * run.scale);
            run.width += float(pos[i].x_advance) * run.scale;
        }
        return run;
    }

    // Shape a paragraph into runs, then break it into lines at the allowed
    // opportunities. Shaping happens ONCE; breaking walks the accumulated
    // advances, rather than re-shaping a candidate string per word.
    void layoutParagraph(std::string_view paragraph, float maxWidth, std::vector<Line>& returnLines) const
    {
        const bool wrap = wordWrapping == WordWrapping::Wrap && maxWidth > 0.0f;

        std::vector<GlyphRun> runs;
        for (const auto& span : itemize(paragraph))
            runs.push_back(shapeSpan(span.face, paragraph, span.beginByte, span.endByte));

        if (runs.empty())
        {
            returnLines.push_back({});
            return;
        }

        if (!wrap)
        {
            Line line;
            for (auto& run : runs)
            {
                line.width += run.width;
                line.runs.push_back(std::move(run));
            }
            returnLines.push_back(std::move(line));
            return;
        }

        // Byte offsets where a break is permitted.
        const auto cps = text::decodeUtf8(paragraph);
        const auto opportunities = text::lineBreakOpportunities(cps);
        std::vector<uint8_t> canBreakAtByte(paragraph.size() + 1, 0);
        for (size_t i = 0; i < cps.size(); ++i)
            if (opportunities[i])
                canBreakAtByte[cps[i].byteOffset] = 1;

        // Flatten to a glyph sequence so breaking can walk it linearly.
        struct FlatGlyph { size_t runIndex; size_t glyphIndex; float advance; uint32_t cluster; };
        std::vector<FlatGlyph> flat;
        for (size_t r = 0; r < runs.size(); ++r)
            for (size_t g = 0; g < runs[r].glyphs.size(); ++g)
                flat.push_back({ r, g, runs[r].advances[g], runs[r].clusters[g] });

        size_t lineStart = 0;
        while (lineStart < flat.size())
        {
            float width = 0.0f;
            size_t lastBreak = 0;      // exclusive end of the best candidate
            size_t i = lineStart;
            for (; i < flat.size(); ++i)
            {
                // Only the FIRST glyph of a cluster may start a line. One
                // cluster can shape to several glyphs (a base plus a mark, or a
                // decomposed emoji), and they all carry the same source byte —
                // so testing the byte alone would happily break between them,
                // splitting a grapheme.
                const bool clusterStart = (i == 0) || flat[i].cluster != flat[i - 1].cluster;
                const bool isBreak = clusterStart && flat[i].cluster < canBreakAtByte.size()
                                  && canBreakAtByte[flat[i].cluster];
                if (isBreak && i > lineStart)
                {
                    if (width > maxWidth)
                        break;
                    lastBreak = i;
                }
                width += flat[i].advance;
            }

            size_t lineEnd = flat.size();
            if (i < flat.size() || width > maxWidth)
                lineEnd = (lastBreak > lineStart) ? lastBreak : (std::max)(lineStart + 1, i);
            if (lineEnd <= lineStart)
                lineEnd = lineStart + 1; // always make progress

            returnLines.push_back(buildLine(runs, flat, lineStart, lineEnd));
            lineStart = lineEnd;
        }
    }

    template <class FlatGlyphT>
    static Line buildLine(const std::vector<GlyphRun>& runs, const std::vector<FlatGlyphT>& flat,
                          size_t begin, size_t end)
    {
        Line line;
        for (size_t i = begin; i < end; )
        {
            const size_t runIndex = flat[i].runIndex;
            GlyphRun piece;
            piece.face = runs[runIndex].face;
            piece.scale = runs[runIndex].scale;
            // A piece is part of one run, so it inherits the whole style.
            piece.emSize = runs[runIndex].emSize;
            piece.obliqueShear = runs[runIndex].obliqueShear;
            piece.strikethrough = runs[runIndex].strikethrough;
            while (i < end && flat[i].runIndex == runIndex)
            {
                const size_t g = flat[i].glyphIndex;
                piece.glyphs.push_back(runs[runIndex].glyphs[g]);
                piece.clusters.push_back(runs[runIndex].clusters[g]);
                piece.advances.push_back(runs[runIndex].advances[g]);
                piece.offsetsX.push_back(runs[runIndex].offsetsX[g]);
                piece.offsetsY.push_back(runs[runIndex].offsetsY[g]);
                piece.width += runs[runIndex].advances[g];
                ++i;
            }
            line.width += piece.width;
            line.runs.push_back(std::move(piece));
        }
        return line;
    }

    // Split on newlines, then lay out each paragraph.
    void layout(const char* utf8, int32_t length, float maxWidth, std::vector<Line>& returnLines) const
    {
        returnLines.clear();
        if (!utf8)
            return;
        const std::string_view text(utf8, length < 0 ? std::char_traits<char>::length(utf8) : size_t(length));

        size_t lineStart = 0;
        while (lineStart <= text.size())
        {
            size_t lineEnd = text.find('\n', lineStart);
            auto paragraph = text.substr(lineStart, lineEnd == std::string_view::npos ? std::string_view::npos
                                                                                      : lineEnd - lineStart);
            if (!paragraph.empty() && paragraph.back() == '\r')
                paragraph.remove_suffix(1);

            layoutParagraph(paragraph, maxWidth, returnLines);

            if (lineEnd == std::string_view::npos)
                break;
            lineStart = lineEnd + 1;
        }
    }

    ReturnCode setTextAlignment(TextAlignment value) override { textAlignment = value; return ReturnCode::Ok; }
    ReturnCode setParagraphAlignment(ParagraphAlignment value) override { paragraphAlignment = value; return ReturnCode::Ok; }
    ReturnCode setWordWrapping(WordWrapping value) override { wordWrapping = value; return ReturnCode::Ok; }

    ReturnCode setLineSpacing(float pLineSpacing, float pBaseline) override
    {
        lineSpacing = pLineSpacing;
        baseline = pBaseline;
        return ReturnCode::Ok;
    }

    ReturnCode getTextExtentU(const char* utf8String, int32_t stringLength, float maxWidth, Size* returnSize) override
    {
        std::vector<Line> lines;
        layout(utf8String, stringLength, maxWidth, lines);

        float width{};
        for (const auto& line : lines)
            width = (std::max)(width, line.width);

        *returnSize = { width, blockHeight(lines.size()) };
        return ReturnCode::Ok;
    }

    ReturnCode getFontMetrics(FontMetrics* returnFontMetrics) override
    {
        const float s = scale();
        FontMetrics m{};
        m.ascent = em.ascent * s;
        m.descent = em.descent * s;
        m.lineGap = em.lineGap * s;
        m.capHeight = em.capHeight * s;
        m.xHeight = em.xHeight * s;
        m.underlinePosition = em.underlinePosition * s;
        m.underlineThickness = em.underlineThickness * s;
        m.strikethroughPosition = em.strikethroughPosition * s;
        m.strikethroughThickness = em.strikethroughThickness * s;
        *returnFontMetrics = m;
        return ReturnCode::Ok;
    }

    GMPI_QUERYINTERFACE_METHOD(api::ITextFormat);
    GMPI_REFCOUNT;
};

// ---------------------------------------------------------------------------
// RichTextFormat
//
// Markdown, laid out once at creation. The parsing is NOT done here: backends
// share helpers/../backends/MarkdownParser.h, so every backend agrees on what
// "**bold**" means and only the rendering differs.
//
// A "style" is one resolved combination of face, size and decoration. Runs of
// the same style are shaped together, and a line's height is the tallest style
// on it - which is what makes a heading push the paragraph below it down.
// ---------------------------------------------------------------------------
class CpuRichTextFormat final : public api::IRichTextFormat
{
public:
    struct Style
    {
        std::shared_ptr<detail::FontFace> face;
        detail::EmMetrics em;
        float emSize{};
        float shear{};          // synthetic italic
        bool strikethrough{};
        std::function<std::shared_ptr<detail::FontFace>(uint32_t)> fallbackFor;

        float scale() const { return emSize / float(face->unitsPerEm); }
        float ascent() const { return em.ascent * scale(); }
        float descent() const { return em.descent * scale(); }
        float lineGap() const { return em.lineGap * scale(); }
    };

    std::string plainText;
    std::vector<Style> styles;
    std::vector<uint16_t> styleOfByte; // one entry per byte of plainText
    // The base format owns alignment, wrapping and line spacing, and is also
    // what the engine measures its first baseline from - the same base format
    // the Direct2D backend builds its layout on.
    std::unique_ptr<CpuTextFormat> base;

    using Line = CpuTextFormat::Line;
    using GlyphRun = CpuTextFormat::GlyphRun;

    uint16_t styleAt(size_t byteOffset) const
    {
        return byteOffset < styleOfByte.size() ? styleOfByte[byteOffset] : uint16_t(0);
    }

    // Font-coverage itemisation for ONE style, mirroring CpuTextFormat::itemize
    // but with that style's face as the primary.
    std::vector<std::pair<std::shared_ptr<detail::FontFace>, uint32_t>>
    itemizeStyled(std::string_view text, const Style& style) const
    {
        // (face, beginByte) pairs; the next entry's begin ends the previous.
        std::vector<std::pair<std::shared_ptr<detail::FontFace>, uint32_t>> spans;
        const auto cps = text::decodeUtf8(text);
        if (cps.empty())
            return spans;
        const auto clusterStarts = text::graphemeBoundaries(cps);

        std::shared_ptr<detail::FontFace> current;
        for (size_t i = 0; i < cps.size(); ++i)
        {
            if (!clusterStarts[i])
                continue;

            const uint32_t c = cps[i].value;
            const bool neutral = (c == ' ' || c == '\t' || text::isGraphemeExtender(c));

            std::shared_ptr<detail::FontFace> wanted;
            if (neutral)
                wanted = current ? current : style.face;
            else if (CpuTextFormat::covers(*style.face, c))
                wanted = style.face;
            else if (current && current != style.face && CpuTextFormat::covers(*current, c))
                wanted = current;
            else if (style.fallbackFor)
            {
                wanted = style.fallbackFor(c);
                if (!wanted)
                    wanted = style.face;
            }
            else
                wanted = style.face;

            if (!current || wanted != current)
            {
                spans.emplace_back(wanted, cps[i].byteOffset);
                current = wanted;
            }
        }
        return spans;
    }

    // Shape one paragraph into runs that change at every style OR font
    // boundary, then break them into lines.
    void layoutParagraph(std::string_view whole, uint32_t paraBegin, uint32_t paraEnd,
                         float maxWidth, std::vector<Line>& returnLines) const
    {
        const std::string_view paragraph = whole.substr(paraBegin, paraEnd - paraBegin);

        std::vector<GlyphRun> runs;
        std::vector<uint16_t> styleOfRun;

        // Walk style spans, and inside each, font-coverage spans.
        uint32_t spanBegin = paraBegin;
        while (spanBegin < paraEnd)
        {
            const uint16_t sIndex = styleAt(spanBegin);
            uint32_t spanEnd = spanBegin;
            while (spanEnd < paraEnd && styleAt(spanEnd) == sIndex)
                ++spanEnd;

            const auto& style = styles[(std::min)(size_t(sIndex), styles.size() - 1)];
            const std::string_view styled = whole.substr(spanBegin, spanEnd - spanBegin);

            auto fontSpans = itemizeStyled(styled, style);
            if (fontSpans.empty())
                fontSpans.emplace_back(style.face, 0u);

            for (size_t f = 0; f < fontSpans.size(); ++f)
            {
                const uint32_t runBegin = spanBegin + fontSpans[f].second;
                const uint32_t runEnd = (f + 1 < fontSpans.size())
                    ? spanBegin + fontSpans[f + 1].second
                    : spanEnd;
                if (runEnd <= runBegin)
                    continue;

                // Shaped against the WHOLE string so context is not lost at a
                // style boundary, exactly as plain text does at a font one.
                auto run = base->shapeSpan(fontSpans[f].first, whole, runBegin, runEnd, style.emSize);
                run.obliqueShear = style.shear;
                run.strikethrough = style.strikethrough;
                runs.push_back(std::move(run));
                styleOfRun.push_back(sIndex);
            }

            spanBegin = spanEnd;
        }

        if (runs.empty())
        {
            Line empty;
            const auto& style = styles.front();
            empty.ascent = style.ascent();
            empty.descent = style.descent();
            empty.advance = style.ascent() + style.descent() + style.lineGap();
            returnLines.push_back(std::move(empty));
            return;
        }

        // Metrics of a set of runs: the tallest wins, so a line containing a
        // heading is as tall as the heading.
        const auto metricsOf = [&](size_t firstRun, size_t lastRun, Line& line) {
            float a = 0.0f, d = 0.0f, adv = 0.0f;
            for (size_t r = firstRun; r <= lastRun && r < styleOfRun.size(); ++r)
            {
                const auto& st = styles[styleOfRun[r]];
                a = (std::max)(a, st.ascent());
                d = (std::max)(d, st.descent());
                adv = (std::max)(adv, st.ascent() + st.descent() + st.lineGap());
            }
            line.ascent = a;
            line.descent = d;
            line.advance = base->lineSpacing >= 0.0f ? base->lineSpacing : adv;
        };

        const bool wrap = base->wordWrapping == WordWrapping::Wrap && maxWidth > 0.0f;

        struct FlatGlyph { size_t runIndex; size_t glyphIndex; float advance; uint32_t cluster; };
        std::vector<FlatGlyph> flat;
        for (size_t r = 0; r < runs.size(); ++r)
            for (size_t g = 0; g < runs[r].glyphs.size(); ++g)
                flat.push_back({ r, g, runs[r].advances[g], runs[r].clusters[g] });

        if (flat.empty())
        {
            Line line;
            metricsOf(0, runs.size() - 1, line);
            returnLines.push_back(std::move(line));
            return;
        }

        std::vector<uint8_t> canBreakAtByte(whole.size() + 1, 0);
        if (wrap)
        {
            const auto cps = text::decodeUtf8(paragraph);
            const auto opportunities = text::lineBreakOpportunities(cps);
            for (size_t i = 0; i < cps.size(); ++i)
                if (opportunities[i])
                    canBreakAtByte[paraBegin + cps[i].byteOffset] = 1;
        }

        size_t lineStart = 0;
        while (lineStart < flat.size())
        {
            size_t lineEnd = flat.size();
            if (wrap)
            {
                float width = 0.0f;
                size_t lastBreak = 0;
                size_t i = lineStart;
                for (; i < flat.size(); ++i)
                {
                    const bool clusterStart = (i == 0) || flat[i].cluster != flat[i - 1].cluster;
                    const bool isBreak = clusterStart && flat[i].cluster < canBreakAtByte.size()
                                      && canBreakAtByte[flat[i].cluster];
                    if (isBreak && i > lineStart)
                    {
                        if (width > maxWidth)
                            break;
                        lastBreak = i;
                    }
                    width += flat[i].advance;
                }
                if (i < flat.size() || width > maxWidth)
                    lineEnd = (lastBreak > lineStart) ? lastBreak : (std::max)(lineStart + 1, i);
            }
            if (lineEnd <= lineStart)
                lineEnd = lineStart + 1;

            auto line = CpuTextFormat::buildLine(runs, flat, lineStart, lineEnd);
            metricsOf(flat[lineStart].runIndex, flat[lineEnd - 1].runIndex, line);
            returnLines.push_back(std::move(line));
            lineStart = lineEnd;
        }
    }

    void layout(float maxWidth, std::vector<Line>& returnLines) const
    {
        returnLines.clear();
        const std::string_view whole(plainText);

        size_t lineStart = 0;
        while (lineStart <= whole.size())
        {
            size_t nl = whole.find('\n', lineStart);
            size_t end = (nl == std::string_view::npos) ? whole.size() : nl;
            size_t trimmed = end;
            if (trimmed > lineStart && whole[trimmed - 1] == '\r')
                --trimmed;

            layoutParagraph(whole, uint32_t(lineStart), uint32_t(trimmed), maxWidth, returnLines);

            if (nl == std::string_view::npos)
                break;
            lineStart = nl + 1;
        }
    }

    Size measure(float maxWidth) const
    {
        std::vector<Line> lines;
        layout(maxWidth, lines);

        float width = 0.0f;
        for (const auto& l : lines)
            width = (std::max)(width, l.width);

        float height = 0.0f;
        for (size_t i = 0; i < lines.size(); ++i)
        {
            // No trailing line gap, matching DirectWrite: the last line
            // contributes its body height, the rest their full advance.
            height += (i + 1 == lines.size())
                ? lines[i].ascent + lines[i].descent
                : (lines[i].advance > 0.0f ? lines[i].advance : lines[i].ascent + lines[i].descent);
        }
        return { width, height };
    }

    ReturnCode getTextExtentU(Size* returnSize) override
    {
        // Unwrapped, like the Direct2D backend: it builds the layout at a
        // 100000-wide box and only narrows it when the text is finally drawn.
        //
        // Known bounded divergence: a lone h1 heading measures 53.2 tall here
        // (win ascent+descent of the doubled size) where the reference says
        // 46. Every other reference extent matches to the pixel — same-size
        // lines, lists, and the mixed-size MultiBlock — and 46/40 em maps to
        // no metric in Selawik's tables at all (it is 2355 font units, which
        // happens to be Arial's line height), so whatever DirectWrite did
        // there is not reconstructible from the font. The rendered glyphs
        // match; only the reported box is taller.
        *returnSize = measure(1.0e6f);
        return ReturnCode::Ok;
    }

    GMPI_QUERYINTERFACE_METHOD(api::IRichTextFormat);
    GMPI_REFCOUNT;
};

// ---------------------------------------------------------------------------
// The engine
// ---------------------------------------------------------------------------
class CpuTextEngine final : public cpugfx::ICpuTextEngine
{
    struct FallbackKey
    {
        uint32_t codepoint{};
        int weight{}, style{}, stretch{};
        std::string baseFamily;

        bool operator<(const FallbackKey& o) const
        {
            return std::tie(codepoint, weight, style, stretch, baseFamily)
                 < std::tie(o.codepoint, o.weight, o.style, o.stretch, o.baseFamily);
        }
    };

    std::function<bool(const FontRequest&, FontData&)> fontProvider;
    // One entry per distinct font FILE, shared by every format and every
    // fallback that resolves to it.
    std::map<std::string, std::shared_ptr<detail::FontFace>> faceCache;
    // Maps a (family, weight, style, stretch) request to its resolved face, so
    // repeat createTextFormat calls skip the system font database.
    std::map<std::string, std::shared_ptr<detail::FontFace>> requestCache;
    // Fallback lookups hit the system font database, which is far too slow to
    // repeat per character; cache the answer (null included, so a character
    // nothing covers is only looked up once). The faces themselves are shared
    // via faceCache, so this map holds pointers, not font files.
    std::map<FallbackKey, std::shared_ptr<detail::FontFace>> fallbackCache;
    // Decoded colour-glyph images, keyed by face and glyph.
    std::map<std::pair<const detail::FontFace*, uint32_t>, gmpi::shared_ptr<api::IBitmap>> colorGlyphCache;

    // --- glyph atlas --------------------------------------------------------
    //
    // Outlines are otherwise extracted, flattened and rasterized on every draw.
    // A cached coverage mask replaces all of that with a blend, and feeds the
    // renderer's ordinary mask path, so pixels are identical to the slow route.
    //
    // Horizontal sub-pixel position is quantised rather than rounded: rounding
    // glyph origins to whole pixels visibly changes spacing. Vertical too, so
    // that a fractional translation does not shift text.
    //
    // Quarter-pixel steps are not merely a cache-size compromise — they are
    // what DirectWrite itself does. Solved against the reference images with
    // an exact-geometry sampler over every candidate: pens rounded to 1/4 px
    // mismatch 49 of 9492 text-pixel bytes; true un-quantised pens 273; 1/16
    // pens 292; 26.6 (1/64) outline snapping 337. Nearest-quarter, true
    // outlines, nothing else.
    static constexpr int kSubPixelSteps = 4;

    struct GlyphKey
    {
        const detail::FontFace* face{};
        uint32_t glyph{};
        float scaleX{}, scaleY{};
        int subX{}, subY{};
        float shearX{}; // a slanted glyph is a different mask, not a moved one

        bool operator<(const GlyphKey& o) const
        {
            return std::tie(face, glyph, scaleX, scaleY, subX, subY, shearX)
                 < std::tie(o.face, o.glyph, o.scaleX, o.scaleY, o.subX, o.subY, o.shearX);
        }
    };

    struct GlyphMask
    {
        std::shared_ptr<detail::FontFace> face; // keeps the key's pointer valid
        drawing::RectL bounds{};                // relative to the pen, device pixels
        std::vector<float> alpha;
    };

    std::map<GlyphKey, std::shared_ptr<const GlyphMask>> glyphAtlas;
    size_t glyphAtlasBytes{};

    // Masks are small but unbounded in variety (every size, every glyph). Cap
    // the total and start over rather than growing without limit; text redraws
    // constantly, so the cache refills immediately and the cliff is invisible.
    static constexpr size_t kGlyphAtlasBudget = 8u * 1024u * 1024u;

    // Rasterize one glyph the way DirectWrite's grayscale rasterizer does:
    // flatten the outline at 4x in both axes, then reduce each pixel to the
    // count of its sixteen sub-cells the outline covers. Point-sampling on
    // that grid (sample centres at (2i+1)/8) is not an approximation of the
    // Direct2D reference — it IS its measured behaviour: reference text
    // contains exactly the seventeen grey levels this produces, and 99% of
    // clean glyph-edge pixels reproduce the reference byte exactly. A sub-cell
    // counts as covered when at least half its area is, which for the straight
    // edges glyphs have at 1/16-pixel scale is the same thing as its centre
    // being inside. See kTextWarpDark for the matching output curve.

    std::shared_ptr<GlyphMask> buildGlyphMask(api::IFactory* factory,
                                              const std::shared_ptr<detail::FontFace>& face,
                                              uint32_t glyph, float scaleX, float scaleY,
                                              int subX, int subY, float shearX = 0.0f)
    {
        auto mask = std::make_shared<GlyphMask>();
        mask->face = face;

        // Flatten the outline through the ordinary geometry sink at 4x,
        // positioned by the sub-pixel offset (which is already in quarter
        // pixels, i.e. whole 4x pixels).
        api::IPathGeometry* rawGeometry{};
        if (factory->createPathGeometry(&rawGeometry) != ReturnCode::Ok || !rawGeometry)
            return {};
        gmpi::shared_ptr<api::IPathGeometry> geometry;
        geometry.attach(rawGeometry);

        api::IGeometrySink* rawSink{};
        if (geometry->open(&rawSink) != ReturnCode::Ok || !rawSink)
            return {};
        gmpi::shared_ptr<api::IGeometrySink> sink;
        sink.attach(rawSink);
        sink->setFillMode(FillMode::Winding);

        // Flatten curves ~8x finer than the AGG default. Point-sampling reads
        // the outline position directly, so flattening error is not averaged
        // away the way area antialiasing averages it; the exact-geometry
        // solver puts the achievable floor at 1/128 px, and default 4x-space
        // flattening measurably misses it.
        if (auto* baseSink = dynamic_cast<se::generic_graphics::GeometrySink*>(sink.get()))
            baseSink->curveApproximationScale = 8.0f;

        detail::GlyphOutlineSink outline;
        outline.sink = sink.get();
        outline.scaleX = scaleX * 4.0f;
        outline.scaleY = -scaleY * 4.0f; // font space is y-up
        // The sub-pixel phase is in pixels/kSubPixelSteps; this space is 4x.
        outline.originX = float(subX) * (4.0f / float(kSubPixelSteps));
        outline.originY = float(subY) * (4.0f / float(kSubPixelSteps));
        outline.shearX = shearX;
        outline.figureOpen = false;
        hb_font_draw_glyph(face->unscaledFont.get(), glyph, detail::glyphDrawFuncs(), &outline);
        if (outline.figureOpen)
            sink->endFigure(FigureEnd::Closed);
        sink->close();

        auto* path = dynamic_cast<cpugfx::PathGeometry*>(geometry.get());
        if (!path)
            return {};

        // The epsilon breaks exact edge-through-sample-centre ties the way a
        // scanline fill rule does — interior to the right/below wins — using
        // geometry, since a coverage threshold cannot see which side the
        // interior is on. Such exact ties are common because the control
        // points were snapped to the 1/64 grid and sample centres sit on
        // eighths; measured: a stem edge landing exactly on 24.625 renders as
        // covered in the reference. 1/256 px is far below the snap grid, far
        // above float noise.
        constexpr float fillRuleEpsilon = 4.0f / 256.0f;
        float minX = 1e30f, minY = 1e30f, maxX = -1e30f, maxY = -1e30f;
        for (auto& fig : path->figures)
            for (auto& p : fig.points)
            {
                if (!std::isfinite(p.x) || !std::isfinite(p.y))
                    return {};
                p.x -= fillRuleEpsilon;
                p.y -= fillRuleEpsilon;
                minX = (std::min)(minX, p.x); maxX = (std::max)(maxX, p.x);
                minY = (std::min)(minY, p.y); maxY = (std::max)(maxY, p.y);
            }

        if (minX <= maxX)
        {
            // Bounds in whole pixels (the flattened points are at 4x), with one
            // pixel of margin so edge sub-cells are not clipped away.
            mask->bounds = { int(std::floor(minX * 0.25f)) - 1, int(std::floor(minY * 0.25f)) - 1,
                             int(std::ceil(maxX * 0.25f)) + 1, int(std::ceil(maxY * 0.25f)) + 1 };
            const drawing::RectL bounds4{ mask->bounds.left * 4, mask->bounds.top * 4,
                                          mask->bounds.right * 4, mask->bounds.bottom * 4 };
            std::vector<float> alpha4;
            cpugfx::raster::rasterizeCoverage(path->figures, FillMode::Winding, bounds4, alpha4);

            const int w = mask->bounds.right - mask->bounds.left;
            const int h = mask->bounds.bottom - mask->bounds.top;
            mask->alpha.assign(size_t(w) * size_t(h), 0.0f);
            for (int y = 0; y < h; ++y)
                for (int x = 0; x < w; ++x)
                {
                    int covered = 0;
                    for (int sy = 0; sy < 4; ++sy)
                    {
                        const float* row = alpha4.data() + size_t(y * 4 + sy) * size_t(w * 4) + size_t(x * 4);
                        for (int sx = 0; sx < 4; ++sx)
                            covered += row[sx] >= 0.5f;
                    }
                    mask->alpha[size_t(y) * w + x] = float(covered) * (1.0f / 16.0f);
                }
        }
        // A blank glyph (space) yields an empty mask, which is still worth
        // caching so it is not re-flattened every draw.
        return mask;
    }

    std::shared_ptr<const GlyphMask> glyphMask(api::IFactory* factory,
                                               const std::shared_ptr<detail::FontFace>& face,
                                               uint32_t glyph, float scaleX, float scaleY,
                                               int subX, int subY, float shearX = 0.0f)
    {
        const GlyphKey key{ face.get(), glyph, scaleX, scaleY, subX, subY, shearX };
        if (auto it = glyphAtlas.find(key); it != glyphAtlas.end())
            return it->second;

        auto mask = buildGlyphMask(factory, face, glyph, scaleX, scaleY, subX, subY, shearX);
        if (!mask)
            return {};

        if (glyphAtlasBytes > kGlyphAtlasBudget)
        {
            glyphAtlas.clear();
            glyphAtlasBytes = 0;
        }
        glyphAtlasBytes += mask->alpha.size() * sizeof(float);
        glyphAtlas[key] = mask;
        return mask;
    }

    static std::string cacheKey(const char* family, FontWeight weight, FontStyle style, FontStretch stretch)
    {
        return std::string(family ? family : "") + "/" + std::to_string(int(weight)) + "/"
             + std::to_string(int(style)) + "/" + std::to_string(int(stretch));
    }

    std::shared_ptr<detail::FontFace> loadFace(const char* family, FontWeight weight, FontStyle style, FontStretch stretch)
    {
        const auto requestKey = cacheKey(family, weight, style, stretch);
        if (auto it = requestCache.find(requestKey); it != requestCache.end())
            return it->second;

        if (!fontProvider)
            return {};

        FontRequest request;
        request.familyName = family ? family : "system-ui";
        request.weight = weight;
        request.style = style;
        request.stretch = stretch;

        FontData data;
        if (!fontProvider(request, data) || !data)
            return {};

        auto face = adoptFace(std::move(data), 0);
        if (face)
            requestCache[requestKey] = face;
        return face;
    }

    std::shared_ptr<detail::FontFace> findFallback(const std::string& baseFamily, FontWeight weight,
                                                   FontStyle style, FontStretch stretch, uint32_t codepoint)
    {
        // Keyed on the style too: a bold run and a regular run must not share
        // whichever face happened to be requested first.
        const FallbackKey key{ codepoint, int(weight), int(style), int(stretch), baseFamily };
        if (auto it = fallbackCache.find(key); it != fallbackCache.end())
            return it->second;

        std::shared_ptr<detail::FontFace> result;
        if (fontProvider)
        {
            FontRequest request;
            request.familyName = baseFamily;
            request.weight = weight;
            request.style = style;
            request.stretch = stretch;
            request.mustCoverCodepoint = codepoint;

            FontData data;
            if (fontProvider(request, data) && data)
                result = adoptFace(std::move(data), codepoint);
        }

        fallbackCache[key] = result;
        return result;
    }

    // Share one FontFace per distinct font file. Fallback is resolved per
    // CODEPOINT, so a page of CJK asks for a font hundreds of times and gets
    // the same file back every time; without this, each answer kept its own
    // private copy of a multi-megabyte font. Deduplication is on what the
    // provider actually resolved to, plus the file's size and face index.
    std::shared_ptr<detail::FontFace> adoptFace(FontData&& data, uint32_t mustCover)
    {
        const std::string key = data.resolvedName + "|" + std::to_string(data.faceIndex)
                              + "|" + std::to_string(data.bytes.size());
        if (auto it = faceCache.find(key); it != faceCache.end())
        {
            const auto& existing = it->second;
            if (mustCover == 0)
                return existing;
            hb_codepoint_t glyph{};
            if (existing && hb_font_get_nominal_glyph(existing->unscaledFont.get(), mustCover, &glyph))
                return existing;
            return {};
        }

        auto candidate = std::make_shared<detail::FontFace>();
        if (!candidate->init(std::move(data)))
            return {};

        if (mustCover != 0)
        {
            // Only accept a font that really has the character; a provider that
            // ignored mustCoverCodepoint would otherwise hand back the primary
            // font and quietly produce .notdef boxes.
            hb_codepoint_t glyph{};
            if (!hb_font_get_nominal_glyph(candidate->unscaledFont.get(), mustCover, &glyph))
                return {};

            // And a mapping is still not a rendering: require a drawable
            // outline too, unless the face is a colour font (sbix/CBDT/COLR),
            // which legitimately has no outlines. See glyphHasOutline.
            if (!candidate->hasColorPng && !candidate->hasColorPaint
                && !detail::glyphHasOutline(candidate->unscaledFont.get(), glyph))
                return {};
        }

        faceCache[key] = candidate;
        return candidate;
    }

    // --- colour glyphs (CBDT/sbix) -----------------------------------------

    static bool hasColorImage(const detail::FontFace& face, uint32_t glyph)
    {
        detail::HbBlob png(hb_ot_color_glyph_reference_png(face.unscaledFont.get(), glyph));
        return png && hb_blob_get_length(png.get()) > 0;
    }

    // Decode a colour glyph's PNG into a bitmap, once. Decoding per frame would
    // be far too slow, and these images are large (often 128px square).
    gmpi::shared_ptr<api::IBitmap> colorGlyphBitmap(api::IFactory* factory,
                                                    const std::shared_ptr<detail::FontFace>& face,
                                                    uint32_t glyph)
    {
        const auto key = std::make_pair(face.get(), glyph);
        if (auto it = colorGlyphCache.find(key); it != colorGlyphCache.end())
            return it->second;

        gmpi::shared_ptr<api::IBitmap> bitmap;
        detail::HbBlob png(hb_ot_color_glyph_reference_png(face->unscaledFont.get(), glyph));
        unsigned length{};
        const char* data = png ? hb_blob_get_data(png.get(), &length) : nullptr;

        DecodedImage decoded;
        if (data && length > 0 && imageDecoder &&
            imageDecoder(reinterpret_cast<const uint8_t*>(data), size_t(length), decoded) && decoded)
        {
            api::IBitmap* raw{};
            if (factory->createImage(int32_t(decoded.width), int32_t(decoded.height), 0, &raw) == ReturnCode::Ok && raw)
            {
                bitmap.attach(raw);
                writeDecodedImage(*bitmap.get(), decoded);
            }
        }

        colorGlyphCache[key] = bitmap;
        return bitmap;
    }

    // 8-bit sRGB straight -> the surface's own premultiplied linear format,
    // through the public pixel-locking API.
    static void writeDecodedImage(api::IBitmap& bitmap, const DecodedImage& decoded)
    {
        api::IBitmapPixels* rawPixels{};
        if (bitmap.lockPixels(&rawPixels, int32_t(BitmapLockFlags::Write)) != ReturnCode::Ok || !rawPixels)
            return;
        gmpi::shared_ptr<api::IBitmapPixels> pixels;
        pixels.attach(rawPixels);

        uint8_t* address{};
        int32_t bytesPerRow{};
        int32_t pixelFormat{};
        pixels->getAddress(&address);
        pixels->getBytesPerRow(&bytesPerRow);
        pixels->getPixelFormat(&pixelFormat);
        if (!address || pixelFormat != api::IBitmapPixels::RGBA_16f)
            return; // only the software backend's format is written here

        for (uint32_t y = 0; y < decoded.height; ++y)
        {
            const uint8_t* src = decoded.pixels.data() + size_t(y) * decoded.width * 4;
            uint16_t* dst = reinterpret_cast<uint16_t*>(address + size_t(y) * bytesPerRow);
            for (uint32_t x = 0; x < decoded.width; ++x, src += 4, dst += 4)
            {
                const float a = src[3] * (1.0f / 255.0f);
                dst[0] = detail::floatToHalf(SRGBPixelToLinear(src[0]) * a);
                dst[1] = detail::floatToHalf(SRGBPixelToLinear(src[1]) * a);
                dst[2] = detail::floatToHalf(SRGBPixelToLinear(src[2]) * a);
                dst[3] = detail::floatToHalf(a);
            }
        }
    }

public:
    explicit CpuTextEngine(std::function<bool(const FontRequest&, FontData&)> provider)
        : fontProvider(std::move(provider))
    {
    }

    // Needed for colour emoji, whose glyphs are PNG blobs inside the font.
    // helpers/DecodeImage.h supplies one: engine.imageDecoder = decodeImageMemory;
    std::function<bool(const uint8_t*, size_t, DecodedImage&)> imageDecoder;

    // Cache rasterized glyph coverage instead of re-extracting outlines every
    // draw. Off rebuilds every mask from its outline on every draw — same
    // pixels, no cache — which is how the two are compared in tests; there is
    // no other reason to disable it.
    bool useGlyphAtlas{ true };

    // Number of masks currently held, for tests and diagnostics.
    size_t glyphAtlasSize() const { return glyphAtlas.size(); }

    // Synthetic italic. Selawik and most UI families ship no italic face, and
    // DirectWrite slants the upright one rather than declining - the reference
    // images show exactly that. The tangent was chosen by sweeping it against
    // those references; see the note where it is used.
    static constexpr float kSyntheticObliqueTangent = 1.0f / 3.0f;

    ReturnCode createTextFormat(const char* fontFamilyName, FontWeight fontWeight, FontStyle fontStyle,
                                FontStretch fontStretch, float fontHeight, int32_t fontFlags,
                                api::ITextFormat** returnTextFormat) override
    {
        *returnTextFormat = {};

        auto face = loadFace(fontFamilyName, fontWeight, fontStyle, fontStretch);
        if (!face)
            return ReturnCode::Fail;

        auto format = std::make_unique<CpuTextFormat>();
        format->face = face;
        format->em = detail::readMetrics(*face);

        // FontFlags decides what the requested height MEANS, and this
        // deliberately mirrors the Direct2D backend's test EXACTLY — including
        // the part that does not do what its name suggests.
        //
        // FontFlags::BodyHeight is 0, so DirectXGfx.cpp's
        // "(fontFlags & FontFlags::BodyHeight) != 0" is never true: the D2D
        // backend computes a body-height scale factor and then never applies
        // it, leaving the default meaning "em size = requested height". Scaling
        // here instead would render every string about 10% smaller than on
        // Windows for the same request, which for a plugin UI is worse than
        // matching a questionable behaviour. Measured against DirectWrite:
        // Arial at 20 gives ascent 18.11 unscaled versus 16.21 scaled.
        //
        // If the D2D branch is ever repaired, this is the matching place to
        // change.
        const float upem = float(face->unitsPerEm);
        float emSize = fontHeight;
        if ((fontFlags & int32_t(FontFlags::CapHeight)) != 0)
            emSize = fontHeight * upem / format->em.capHeight;

        format->fontSize = emSize;

        const std::string baseFamily = fontFamilyName ? fontFamilyName : "system-ui";
        format->fallbackFor = [this, baseFamily, fontWeight, fontStyle, fontStretch](uint32_t codepoint) {
            return findFallback(baseFamily, fontWeight, fontStyle, fontStretch, codepoint);
        };

        *returnTextFormat = format.release();
        return ReturnCode::Ok;
    }

    // Everything past the layout stage is shared by plain and rich text: the
    // two differ in how lines are built and styled, not in how glyphs reach
    // pixels. Lines carry their own metrics only when they need to (a heading
    // is taller than the paragraph under it); zero means "use the default",
    // which is every plain-text line.
    struct LaidOut
    {
        std::vector<CpuTextFormat::Line> lines;
        TextAlignment textAlignment{ TextAlignment::Leading };
        ParagraphAlignment paragraphAlignment{ ParagraphAlignment::Near };
        float defaultAscent{};
        float defaultDescent{};
        float defaultAdvance{};
        float baselineOffset{}; // first baseline, from the block top
    };

    // A retained text layout (api::ITextLayout): the shaped, line-broken text,
    // kept so a draw goes straight to rasterisation.
    //
    // This is the whole point of the interface on this backend. drawText
    // re-runs itemisation, line breaking and HarfBuzz shaping on every call,
    // and glyph rasters are already cached, so shaping is what a repeated draw
    // was paying for. Holding the LaidOut removes it.
    //
    // Parity with drawText is structural rather than delicate: drawText is
    // "lay out, then renderLaidOut", so a retained layout is the same laying
    // out done once, handed to the same renderLaidOut with the same rect.
    class RetainedLayout final : public api::ITextLayout
    {
    public:
        LaidOut laid;
        Size extent{};
        float boxWidth{};
        float boxHeight{};

        ReturnCode getTextExtentU(Size* returnSize) override
        {
            *returnSize = extent;
            return ReturnCode::Ok;
        }

        GMPI_QUERYINTERFACE_METHOD(api::ITextLayout);
        GMPI_REFCOUNT;
    };

    ReturnCode createRichTextFormat(const char* markdownText, float fontHeight, const char* fontFamilyName,
                                    int32_t fontFlags, TextAlignment textAlignment,
                                    ParagraphAlignment paragraphAlignment, WordWrapping wordWrapping,
                                    float lineSpacing, float baseline,
                                    api::IRichTextFormat** returnRichTextFormat) override
    {
        *returnRichTextFormat = {};
        if (!markdownText)
            return ReturnCode::Fail;

        // The base format carries the paragraph-level settings and is what the
        // first baseline is measured from, exactly as on Direct2D.
        api::ITextFormat* rawBase{};
        if (createTextFormat(fontFamilyName, FontWeight::Regular, FontStyle::Normal, FontStretch::Normal,
                             fontHeight, fontFlags, &rawBase) != ReturnCode::Ok || !rawBase)
            return ReturnCode::Fail;

        auto rich = std::make_unique<CpuRichTextFormat>();
        rich->base.reset(dynamic_cast<CpuTextFormat*>(rawBase));
        if (!rich->base)
        {
            rawBase->release();
            return ReturnCode::Fail;
        }
        rich->base->textAlignment = textAlignment;
        rich->base->paragraphAlignment = paragraphAlignment;
        rich->base->wordWrapping = wordWrapping;
        rich->base->setLineSpacing(lineSpacing, baseline);

        const auto parsed = parseMarkdown(markdownText);
        rich->plainText = parsed.plainText;
        rich->styleOfByte.assign(rich->plainText.size(), 0);

        const std::string family = fontFamilyName ? fontFamilyName : "system-ui";
        const float baseEmSize = rich->base->fontSize;

        // Style 0 is the unformatted base, and every run that needs nothing
        // special shares it.
        const auto makeStyle = [&](bool bold, bool italic, bool monospace, float sizeScale)
        {
            CpuRichTextFormat::Style style;
            const float emSize = sizeScale > 0.0f ? baseEmSize * sizeScale : baseEmSize;
            const auto weight = bold ? FontWeight::Bold : FontWeight::Regular;

            // Monospace asks for a different FAMILY, and only takes it if one
            // resolves - which mirrors Direct2D, where the family name is
            // looked up in the layout's own font collection and silently
            // ignored when it holds only the bundled face. The reference
            // images show inline code in the base font for exactly that
            // reason, so declining here is matching, not giving up.
            std::shared_ptr<detail::FontFace> face;
            if (monospace)
                face = loadFace("Consolas", weight, FontStyle::Normal, FontStretch::Normal);
            if (!face)
                face = loadFace(family.c_str(), weight, FontStyle::Normal, FontStretch::Normal);
            if (!face)
                face = rich->base->face;

            style.face = face;
            style.em = detail::readMetrics(*face);
            style.emSize = emSize;
            style.shear = italic ? kSyntheticObliqueTangent : 0.0f;
            style.fallbackFor = [this, family, weight](uint32_t codepoint) {
                return findFallback(family, weight, FontStyle::Normal, FontStretch::Normal, codepoint);
            };
            return style;
        };

        rich->styles.push_back(makeStyle(false, false, false, 0.0f));

        for (const auto& run : parsed.runs)
        {
            if (run.startPosition >= rich->plainText.size())
                continue;

            // One style per distinct combination, so repeated bold spans share
            // a face rather than reloading it.
            uint16_t index = 0;
            bool found = false;
            for (uint16_t i = 0; i < uint16_t(rich->styles.size()); ++i)
            {
                const auto& s = rich->styles[i];
                const float wantSize = run.fontSizeScale > 0.0f ? baseEmSize * run.fontSizeScale : baseEmSize;
                const bool wantBold = run.bold;
                const bool isBold = s.face && s.face->name.find("Bold") != std::string::npos;
                if (s.emSize == wantSize && s.shear == (run.italic ? kSyntheticObliqueTangent : 0.0f)
                    && s.strikethrough == run.strikethrough && isBold == wantBold)
                {
                    index = i;
                    found = true;
                    break;
                }
            }
            if (!found)
            {
                auto style = makeStyle(run.bold, run.italic, run.monospace, run.fontSizeScale);
                style.strikethrough = run.strikethrough;
                rich->styles.push_back(std::move(style));
                index = uint16_t(rich->styles.size() - 1);
            }

            const size_t end = (std::min)(size_t(run.startPosition) + run.length, rich->plainText.size());
            for (size_t b = run.startPosition; b < end; ++b)
                rich->styleOfByte[b] = index;
        }

        *returnRichTextFormat = rich.release();
        return ReturnCode::Ok;
    }

    ReturnCode drawRichText(api::IDeviceContext* context, api::IRichTextFormat* richTextFormat,
                            const Rect& layoutRect, api::IBrush* brush, int32_t options) override
    {
        auto* rich = dynamic_cast<CpuRichTextFormat*>(richTextFormat);
        if (!rich || !context || rich->styles.empty())
            return ReturnCode::NoSupport;

        api::IFactory* factory{};
        if (context->getFactory(&factory) != ReturnCode::Ok || !factory)
            return ReturnCode::Fail;

        LaidOut laid;
        rich->layout(layoutRect.right - layoutRect.left, laid.lines);
        if (laid.lines.empty())
            return ReturnCode::Ok;

        laid.textAlignment = rich->base->textAlignment;
        laid.paragraphAlignment = rich->base->paragraphAlignment;
        // The BASE format's ascent, not the first line's: Direct2D snaps the
        // block using the format it built the layout from, whatever the first
        // line happens to contain.
        laid.defaultAscent = rich->base->ascent();
        laid.defaultDescent = rich->base->descent();
        laid.defaultAdvance = rich->base->lineAdvance();
        laid.baselineOffset = rich->base->baselineOffset();

        return renderLaidOut(context, factory, laid, layoutRect, brush, options);
    }

    // Lay the text out now and keep the result.
    //
    // Styling runs are declined for the moment: the engine can already vary
    // font, weight, size and decorations per run (that is how markdown is
    // drawn), but not colour - GlyphRun carries no colour and renderLaidOut
    // batches a whole block into one geometry fill and one coverage blend
    // against a single brush, which per-run colour would have to be split up.
    // Declining costs a caller nothing today: the fallback, drawText, cannot
    // style text at all.
    ReturnCode createTextLayout(const char* utf8, int32_t length, api::ITextFormat* baseFormat,
                                float maxWidth, float maxHeight,
                                const TextStyleRun* /*runs*/, int32_t runCount,
                                const char* const* /*runFamilies*/, int32_t /*runFamilyCount*/,
                                api::ITextLayout** returnTextLayout) override
    {
        *returnTextLayout = {};

        auto* format = dynamic_cast<CpuTextFormat*>(baseFormat);
        if (!format || !utf8 || length < 0)
            return ReturnCode::NoSupport;

        if (runCount > 0)
            return ReturnCode::NoSupport; // see above

        auto layout = std::make_unique<RetainedLayout>();

        // Exactly what drawText does per call - once.
        format->layout(utf8, length, maxWidth, layout->laid.lines);
        layout->laid.textAlignment = format->textAlignment;
        layout->laid.paragraphAlignment = format->paragraphAlignment;
        layout->laid.defaultAscent = format->ascent();
        layout->laid.defaultDescent = format->descent();
        layout->laid.defaultAdvance = format->lineAdvance();
        layout->laid.baselineOffset = format->baselineOffset();

        layout->boxWidth = maxWidth;
        layout->boxHeight = maxHeight;

        // Measured through the format, so the answer matches what the format
        // itself would report for this text at this width.
        format->getTextExtentU(utf8, length, maxWidth, &layout->extent);

        *returnTextLayout = layout.release();
        return ReturnCode::Ok;
    }

    ReturnCode drawTextLayout(api::IDeviceContext* context, api::ITextLayout* textLayout,
                              Point point, api::IBrush* brush, int32_t options) override
    {
        auto* layout = dynamic_cast<RetainedLayout*>(textLayout);
        if (!layout || !context)
            return ReturnCode::NoSupport;

        if (layout->laid.lines.empty())
            return ReturnCode::Ok;

        api::IFactory* factory{};
        if (context->getFactory(&factory) != ReturnCode::Ok || !factory)
            return ReturnCode::Fail;

        // The rect drawText would have been given for the same box.
        const Rect layoutRect{
            point.x, point.y,
            point.x + layout->boxWidth, point.y + layout->boxHeight };

        return renderLaidOut(context, factory, layout->laid, layoutRect, brush, options);
    }

    ReturnCode drawText(api::IDeviceContext* context, api::ITextFormat* textFormat,
                        const char* utf8, int32_t length, const Rect& layoutRect,
                        api::IBrush* brush, int32_t options) override
    {
        auto* format = dynamic_cast<CpuTextFormat*>(textFormat);
        if (!format || !context)
            return ReturnCode::NoSupport;

        api::IFactory* factory{};
        if (context->getFactory(&factory) != ReturnCode::Ok || !factory)
            return ReturnCode::Fail;

        LaidOut laid;
        format->layout(utf8, length, layoutRect.right - layoutRect.left, laid.lines);
        if (laid.lines.empty())
            return ReturnCode::Ok;

        laid.textAlignment = format->textAlignment;
        laid.paragraphAlignment = format->paragraphAlignment;
        laid.defaultAscent = format->ascent();
        laid.defaultDescent = format->descent();
        laid.defaultAdvance = format->lineAdvance();
        laid.baselineOffset = format->baselineOffset();

        return renderLaidOut(context, factory, laid, layoutRect, brush, options);
    }

    ReturnCode renderLaidOut(api::IDeviceContext* context, api::IFactory* factory,
                             const LaidOut& laid, const Rect& layoutRect,
                             api::IBrush* brush, int32_t options)
    {
        const auto& lines = laid.lines;

        // DrawTextOptions::Clip confines the text to its layout rectangle,
        // which matters for descenders and for text that overflows.
        const bool clipToLayout = (options & DrawTextOptions::Clip) != 0;
        if (clipToLayout)
            context->pushAxisAlignedClip(&layoutRect);
        struct ClipGuard
        {
            api::IDeviceContext* ctx;
            bool active;
            ~ClipGuard() { if (active) ctx->popAxisAlignedClip(); }
        } clipGuard{ context, clipToLayout };

        api::IPathGeometry* rawGeometry{};
        if (factory->createPathGeometry(&rawGeometry) != ReturnCode::Ok || !rawGeometry)
            return ReturnCode::Fail;
        gmpi::shared_ptr<api::IPathGeometry> geometry;
        geometry.attach(rawGeometry);

        api::IGeometrySink* rawSink{};
        if (geometry->open(&rawSink) != ReturnCode::Ok || !rawSink)
            return ReturnCode::Fail;
        gmpi::shared_ptr<api::IGeometrySink> sink;
        sink.attach(rawSink);

        // Glyph outlines wind the opposite way to this API's default fill rule,
        // so ask for nonzero: counters (the hole in an 'o') come out right.
        sink->setFillMode(FillMode::Winding);

        // Colour glyphs cannot go in the outline path, so they are collected
        // and drawn afterwards. Outlines and emoji do not overlap in ordinary
        // text, so the two passes are not visually distinguishable from
        // strictly interleaved drawing.
        struct ColorGlyphDraw { std::shared_ptr<detail::FontFace> face; uint32_t glyph; Rect dest; };
        std::vector<ColorGlyphDraw> colorGlyphs;

        // COLRv1 glyphs are painted, not decoded — HarfBuzz drives fills,
        // gradients and clips that map onto this API's own primitives.
        struct PaintGlyphDraw { std::shared_ptr<detail::FontFace> face; uint32_t glyph; float penX, penY, scale; };
        std::vector<PaintGlyphDraw> paintGlyphs;

        // Atlas glyphs are COLLECTED, merged into one coverage buffer, and
        // blended once - not blended one glyph at a time. Where two glyph
        // boxes overlap (kerned pairs, italics), per-glyph blending applies
        // the brush twice and the seam goes dark; the geometry path unions
        // everything through one winding fill, so the two paths disagreed by
        // up to 0.28 in the comparison test. Merging with max() approximates
        // the union the way Direct2D's glyph-run rasterisation does.
        struct AtlasGlyphDraw { std::shared_ptr<const GlyphMask> mask; int x, y; };
        std::vector<AtlasGlyphDraw> atlasGlyphs;

        // Per line, because rich text mixes sizes. For plain text every line
        // returns the same number and this is the old n * lineHeight.
        const auto lineAdvanceOf = [&laid](const CpuTextFormat::Line& l) {
            return l.advance > 0.0f ? l.advance : laid.defaultAdvance;
        };
        float blockHeight = 0.0f;
        for (const auto& l : lines)
            blockHeight += lineAdvanceOf(l);

        float y = layoutRect.top;
        switch (laid.paragraphAlignment)
        {
        case ParagraphAlignment::Far:    y = layoutRect.bottom - blockHeight; break;
        case ParagraphAlignment::Center: y = 0.5f * (layoutRect.top + layoutRect.bottom - blockHeight); break;
        default: break;
        }

        const bool snap = (options & DrawTextOptions::NoSnap) == 0;

        // The Direct2D backend does not hand DrawText the caller's rectangle:
        // it first shifts it so the TOP-ALIGNED first baseline lands on the
        // half-pixel grid (DirectXGfx.cpp drawTextU, "snap to pixel to match
        // Mac": floor((baseline - 0.25) / 0.5) * 0.5 + 0.5), and Direct2D then
        // snaps each line's baseline to a whole pixel. The shift is at most
        // ±0.25, but it decides which pixel wins when a baseline lands near
        // x.5 — measured: 30px Selawik centred in a 256px rect put the middle
        // line at 140.42; unshifted rounding gives 140, the reference says
        // 141. Same rule here, or every such tie renders one pixel off.
        const float winBaseline = layoutRect.top + laid.defaultAscent;
        const float baselineAdjust =
            std::floor((winBaseline - 0.25f) / 0.5f) * 0.5f + 0.5f - winBaseline;

        // The mask path needs the software backend, a CPU brush, and a
        // transform without rotation or skew — a rotated glyph would need a
        // different mask per angle. It is where the DirectWrite grayscale
        // emulation lives (4x4 sampling + coverage warp), so it is taken
        // whenever possible, with or without the atlas cache; useGlyphAtlas
        // off merely rebuilds each mask instead of caching it, so tests can
        // compare the two. Rotated or exotically-brushed text falls through
        // to the continuous-coverage geometry path below — as Direct2D itself
        // stops grid-processing text under such transforms.
        auto* cpuTarget = dynamic_cast<cpugfx::RenderTarget*>(context);
        auto* cpuBrush = dynamic_cast<cpugfx::CpuBrush*>(brush);
        Matrix3x2 contextTransform;
        context->getTransform(&contextTransform);
        const bool axisAligned = contextTransform._12 == 0.0f && contextTransform._21 == 0.0f
                              && contextTransform._11 > 0.0f && contextTransform._22 > 0.0f;

        // Translucent text gets NONE of the grayscale treatment. Decoded from
        // the reference images: 60%-white text over Tomato reproduces, to the
        // byte, a plain linear-space blend of CONTINUOUS coverage times brush
        // alpha — no 1/16 quantisation, no contrast table, no gamma-space
        // blending. Direct2D evidently routes translucent text through its
        // ordinary per-primitive antialiasing, so it must fall through to the
        // geometry path here.
        bool opaqueBrush = true;
        float brushLuminance = 0.5f; // neutral middle for non-solid brushes
        if (auto* solid = dynamic_cast<cpugfx::SolidColorBrush*>(brush))
        {
            opaqueBrush = solid->color.a * solid->opacity >= 1.0f;
            brushLuminance = 0.2126f * solid->color.r + 0.7152f * solid->color.g
                           + 0.0722f * solid->color.b;
        }

        const bool maskPath = cpuTarget && cpuBrush && axisAligned && opaqueBrush;

        detail::GlyphOutlineSink outline;
        outline.sink = sink.get();
        outline.scaleX = 1.0f;
        outline.scaleY = -1.0f; // font space is y-up; overwritten per run

        // Strikethrough is geometry, not glyphs: collected per run and filled
        // once at the end, so it composites like the text rather than blending
        // separately over each glyph it crosses.
        std::vector<Rect> strikeRects;

        // Largest em size actually masked, in device pixels — feeds the
        // coverage warp's size fade (mixed-size rich text warps by its
        // biggest run, erring toward lighter).
        float maxEmPx = 0.0f;

        for (const auto& line : lines)
        {
            float x = layoutRect.left;
            switch (laid.textAlignment)
            {
            case TextAlignment::Trailing: x = layoutRect.right - line.width; break;
            case TextAlignment::Center:   x = 0.5f * (layoutRect.left + layoutRect.right - line.width); break;
            default: break;
            }

            float baselineY = y + (line.ascent > 0.0f ? line.ascent : laid.baselineOffset) + baselineAdjust;
            if (snap)
                baselineY = std::floor(baselineY + 0.5f); // whole-pixel baselines read sharper

            // Each run carries its own face, since a line may mix fonts.
            for (const auto& run : line.runs)
            {
                outline.scaleX = run.scale;
                outline.scaleY = -run.scale;
                outline.shearX = run.obliqueShear;
                const float runStartX = x;
                for (size_t i = 0; i < run.glyphs.size(); ++i)
                {
                    const float penX = x + run.offsetsX[i];
                    const float penY = baselineY - run.offsetsY[i];

                    if (run.face->hasColorPaint &&
                        hb_ot_color_glyph_has_paint(run.face->face.get(), run.glyphs[i]))
                    {
                        paintGlyphs.push_back({ run.face, run.glyphs[i], penX, penY, run.scale });
                        x += run.advances[i];
                        continue;
                    }

                    if (run.face->hasColorPng && hasColorImage(*run.face, run.glyphs[i]))
                    {
                        // A bitmap colour glyph. Its extents give the box to
                        // place the image in, relative to the pen.
                        hb_glyph_extents_t extents{};
                        if (hb_font_get_glyph_extents(run.face->unscaledFont.get(), run.glyphs[i], &extents))
                        {
                            const Rect dest{
                                penX + float(extents.x_bearing) * run.scale,
                                penY - float(extents.y_bearing) * run.scale,
                                penX + float(extents.x_bearing + extents.width) * run.scale,
                                penY - float(extents.y_bearing + extents.height) * run.scale };
                            colorGlyphs.push_back({ run.face, run.glyphs[i], dest });
                        }
                        x += run.advances[i];
                        continue;
                    }

                    if (maskPath)
                    {
                        // Position in device space, split into a whole-pixel
                        // destination and a quantised sub-pixel offset that
                        // selects which cached mask to use.
                        const auto device = drawing::transformPoint(contextTransform, { penX, penY });

                        // Round the position to the sub-pixel grid FIRST, then
                        // split it. Taking the fraction before rounding loses
                        // the carry: a fraction of 0.9 rounds to the next whole
                        // pixel, and bucketing it as 0 without incrementing the
                        // integer part shifts the glyph a whole pixel left.
                        const float gx = std::floor(device.x * kSubPixelSteps + 0.5f);
                        const float gy = std::floor(device.y * kSubPixelSteps + 0.5f);
                        const int ix = int(std::floor(gx / kSubPixelSteps));
                        const int iy = int(std::floor(gy / kSubPixelSteps));
                        const int subX = int(gx) - ix * kSubPixelSteps;
                        const int subY = int(gy) - iy * kSubPixelSteps;

                        const float maskScaleX = run.scale * contextTransform._11;
                        const float maskScaleY = run.scale * contextTransform._22;
                        if (auto mask = useGlyphAtlas
                                ? glyphMask(factory, run.face, run.glyphs[i], maskScaleX, maskScaleY, subX, subY, run.obliqueShear)
                                : std::shared_ptr<const GlyphMask>(
                                      buildGlyphMask(factory, run.face, run.glyphs[i], maskScaleX, maskScaleY, subX, subY, run.obliqueShear)))
                        {
                            if (!mask->alpha.empty())
                            {
                                const int mx = ix + mask->bounds.left;
                                const int my = iy + mask->bounds.top;
                                atlasGlyphs.push_back({ std::move(mask), mx, my });
                                maxEmPx = (std::max)(maxEmPx, run.emSize * contextTransform._22);
                            }
                            x += run.advances[i];
                            continue;
                        }
                    }

                    outline.originX = penX;
                    outline.originY = penY;
                    outline.figureOpen = false;
                    hb_font_draw_glyph(run.face->unscaledFont.get(), run.glyphs[i],
                                       detail::glyphDrawFuncs(), &outline);
                    if (outline.figureOpen)
                        sink->endFigure(FigureEnd::Closed);
                    x += run.advances[i];
                }

                if (run.strikethrough && x > runStartX)
                {
                    // Position and thickness come from the run's own face at
                    // the run's own size, so a struck heading gets a heavier
                    // line than struck body text.
                    const auto em = detail::readMetrics(*run.face);
                    const float toDevice = run.emSize / float(run.face->unitsPerEm);
                    const float centre = baselineY - em.strikethroughPosition * toDevice;
                    const float thickness = (std::max)(1.0f, em.strikethroughThickness * toDevice);
                    strikeRects.push_back({ runStartX, centre - 0.5f * thickness,
                                            x, centre + 0.5f * thickness });
                }
            }
            y += lineAdvanceOf(line);
        }

        sink->close();

        if (!atlasGlyphs.empty())
        {
            // Merge every collected mask into one coverage buffer and blend
            // once, with a SATURATING SUM. Where a pixel straddles two
            // glyphs, each mask holds the fraction that glyph covers; the
            // regions are disjoint within the pixel for ordinary text (one
            // shape ends, the next begins), so the union's coverage is their
            // sum - which is exactly what the geometry path computes when its
            // winding fill crosses both outlines in one scan. max() was tried
            // first and measured worse (0.22 vs 0.13 against the geometry
            // path): it under-counts every straddled pixel, not just the rare
            // genuinely-overlapping ones that a sum over-counts.
            drawing::RectL box{ INT32_MAX, INT32_MAX, INT32_MIN, INT32_MIN };
            for (const auto& g : atlasGlyphs)
            {
                box.left   = (std::min)(box.left,   g.x);
                box.top    = (std::min)(box.top,    g.y);
                box.right  = (std::max)(box.right,  g.x + (g.mask->bounds.right - g.mask->bounds.left));
                box.bottom = (std::max)(box.bottom, g.y + (g.mask->bounds.bottom - g.mask->bounds.top));
            }

            const int bw = box.right - box.left;
            const int bh = box.bottom - box.top;

            std::vector<float> merged(size_t(bw) * size_t(bh), 0.0f);
            for (const auto& g : atlasGlyphs)
            {
                const int w = g.mask->bounds.right - g.mask->bounds.left;
                const int h = g.mask->bounds.bottom - g.mask->bounds.top;
                for (int row = 0; row < h; ++row)
                {
                    const float* src = g.mask->alpha.data() + size_t(row) * size_t(w);
                    float* dst = merged.data()
                        + size_t(g.y - box.top + row) * size_t(bw) + size_t(g.x - box.left);
                    for (int col = 0; col < w; ++col)
                        dst[col] = (std::min)(1.0f, dst[col] + src[col]);
                }
            }

            // The union coverage is still on the 1/16 grid (masks hold k/16
            // and the sum saturates). The Direct2D weight match happens HERE,
            // on the coverage itself (see kTextWarpDark); the blend below is
            // ordinary linear source-over like every other fill.
            const float strength = detail::textWarpStrength(brushLuminance, maxEmPx);
            if (strength > 0.0f)
            {
                float warp[17];
                for (int i = 0; i <= 16; ++i)
                {
                    const float identity = float(i) / 16.0f;
                    warp[i] = identity + strength * (detail::kTextWarpDark[i] - identity);
                }
                for (auto& coverage : merged)
                    coverage = warp[std::clamp(int(std::lround(coverage * 16.0f)), 0, 16)];
            }
            cpuTarget->blendCoverageMask(merged.data(), bw, bh, box.left, box.top, *cpuBrush);
        }

        if (brush)
        {
            // Rotated or exotically-brushed glyphs and all translucent text:
            // continuous coverage through the ordinary linear fill. For
            // translucent text that is the measured Direct2D behaviour (see
            // above). The rare rotated-opaque case renders a shade lighter
            // than masked text at small sizes — the coverage warp is a
            // mask-path concept — much as Direct2D itself stops
            // grid-processing text under such transforms.
            context->fillGeometry(geometry.get(), brush, nullptr);
        }

        if (brush && !strikeRects.empty())
        {
            for (const auto& r : strikeRects)
                context->fillRectangle(&r, brush);
        }

        for (const auto& item : colorGlyphs)
        {
            if (auto bitmap = colorGlyphBitmap(factory, item.face, item.glyph))
            {
                SizeU size{};
                bitmap->getSizeU(&size);
                const Rect source{ 0.0f, 0.0f, float(size.width), float(size.height) };
                context->drawBitmap(bitmap.get(), &item.dest, 1.0f,
                                    BitmapInterpolationMode::Linear, &source);
            }
        }

        if (!paintGlyphs.empty())
        {
            // COLRv1 fonts can reference the "foreground" palette entry, which
            // means whatever colour the text is being drawn in.
            hb_color_t foreground = HB_COLOR(0, 0, 0, 255);
            if (auto* solid = dynamic_cast<cpugfx::SolidColorBrush*>(brush))
            {
                const auto& c = solid->color;
                foreground = HB_COLOR(linearPixelToSRGB(c.b), linearPixelToSRGB(c.g),
                                      linearPixelToSRGB(c.r),
                                      uint8_t(std::clamp(c.a * solid->opacity * 255.0f + 0.5f, 0.0f, 255.0f)));
            }

            Matrix3x2 saved{};
            context->getTransform(&saved);

            for (const auto& item : paintGlyphs)
            {
                // Map font units (y-up) to device at this glyph's pen position,
                // on top of whatever transform the caller had active.
                // Negative y scale: font space is y-up. Doing the flip HERE
                // rather than at the leaves is what makes HarfBuzz's paint
                // transforms compose in the right order.
                const Matrix3x2 glyphToLocal{ item.scale, 0.0f, 0.0f, -item.scale, item.penX, item.penY };

                detail::PaintState state;
                state.context = context;
                state.factory = factory;
                state.baseTransform = glyphToLocal * saved;
                state.apply();

                // Foreground-palette layers take the text brush's colour, the
                // way a monochrome glyph would; painting them black regardless
                // would make those parts of an emoji ignore the brush.
                hb_font_paint_glyph(item.face->unscaledFont.get(), item.glyph,
                                    detail::glyphPaintFuncs(), &state, 0, foreground);

                // Unwind anything the font left open, so a malformed glyph
                // cannot leak a clip into subsequent drawing.
                while (state.clipDepth > 0)
                {
                    context->popAxisAlignedClip();
                    --state.clipDepth;
                }
            }
            context->setTransform(&saved);
        }
        return ReturnCode::Ok;
    }
};

}} // namespace gmpi::drawing
