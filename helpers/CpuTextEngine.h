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
#include <vector>

#include <hb.h>
#include <hb-ot.h>

#include "../Drawing.h"
#include "../backends/CpuGfx.h"
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

    bool init(FontData&& data)
    {
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

inline EmMetrics readMetrics(const FontFace& face)
{
    hb_font_t* font = face.unscaledFont.get();
    const float upem = float(face.unitsPerEm);

    hb_font_extents_t extents{};
    hb_font_get_h_extents(font, &extents);

    EmMetrics m;
    // HarfBuzz reports ascender up-positive and descender down-negative; the
    // drawing API wants both as positive distances from the baseline.
    m.ascent = float(extents.ascender);
    m.descent = -float(extents.descender);
    m.lineGap = float(extents.line_gap);

    if (m.ascent <= 0.0f) m.ascent = 0.8f * upem;
    if (m.descent <= 0.0f) m.descent = 0.2f * upem;

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
    float scale{};        // font units -> device
    float originX{}, originY{};
    bool figureOpen{};

    Point map(float x, float y) const
    {
        // Font space is y-up; the drawing API is y-down.
        return { originX + x * scale, originY - y * scale };
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
    };

    struct Line
    {
        std::vector<GlyphRun> runs;
        float width{};
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
    // Line height comes from the PRIMARY font, so a line does not change height
    // just because one fallback glyph appeared in it.
    float lineHeight() const { return lineSpacing >= 0.0f ? lineSpacing : (ascent() + descent()); }
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

            // Characters with no visible glyph must not drag in a fallback font.
            std::shared_ptr<detail::FontFace> wanted = current ? current : face;
            const bool neutral = (c == ' ' || c == '\t' || text::isGraphemeExtender(c));
            if (!neutral && !(wanted && covers(*wanted, c)))
            {
                if (covers(*face, c))
                    wanted = face;
                else if (fallbackFor)
                {
                    if (auto fallback = fallbackFor(c))
                        wanted = fallback;
                    else
                        wanted = face; // nothing covers it; the primary draws .notdef
                }
                else
                {
                    wanted = face;
                }
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

    GlyphRun shapeSpan(const std::shared_ptr<detail::FontFace>& runFace,
                       std::string_view whole, uint32_t beginByte, uint32_t endByte) const
    {
        GlyphRun run;
        run.face = runFace;
        run.scale = fontSize / float(runFace->unitsPerEm);
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
                const bool isBreak = flat[i].cluster < canBreakAtByte.size() && canBreakAtByte[flat[i].cluster];
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

        *returnSize = { width, float(lines.size()) * lineHeight() };
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
// The engine
// ---------------------------------------------------------------------------
class CpuTextEngine final : public cpugfx::ICpuTextEngine
{
    std::function<bool(const FontRequest&, FontData&)> fontProvider;
    std::map<std::string, std::shared_ptr<detail::FontFace>> faceCache;
    // Fallback lookups hit the system font database, which is far too slow to
    // repeat per character; cache the answer per codepoint (null included, so a
    // character nothing covers is only looked up once).
    std::map<uint32_t, std::shared_ptr<detail::FontFace>> fallbackCache;

    static std::string cacheKey(const char* family, FontWeight weight, FontStyle style, FontStretch stretch)
    {
        return std::string(family ? family : "") + "/" + std::to_string(int(weight)) + "/"
             + std::to_string(int(style)) + "/" + std::to_string(int(stretch));
    }

    std::shared_ptr<detail::FontFace> loadFace(const char* family, FontWeight weight, FontStyle style, FontStretch stretch)
    {
        const auto key = cacheKey(family, weight, style, stretch);
        if (auto it = faceCache.find(key); it != faceCache.end())
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

        auto face = std::make_shared<detail::FontFace>();
        if (!face->init(std::move(data)))
            return {};

        faceCache[key] = face;
        return face;
    }

    std::shared_ptr<detail::FontFace> findFallback(const std::string& baseFamily, FontWeight weight,
                                                   FontStyle style, FontStretch stretch, uint32_t codepoint)
    {
        if (auto it = fallbackCache.find(codepoint); it != fallbackCache.end())
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
            {
                auto candidate = std::make_shared<detail::FontFace>();
                if (candidate->init(std::move(data)))
                {
                    hb_codepoint_t glyph{};
                    // Only accept a font that really has the character; a
                    // provider that ignores mustCoverCodepoint would otherwise
                    // hand back the primary font and produce .notdef boxes.
                    if (hb_font_get_nominal_glyph(candidate->unscaledFont.get(), codepoint, &glyph))
                        result = candidate;
                }
            }
        }

        fallbackCache[codepoint] = result;
        return result;
    }

public:
    explicit CpuTextEngine(std::function<bool(const FontRequest&, FontData&)> provider)
        : fontProvider(std::move(provider))
    {
    }

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

        // FontFlags decides what the requested height MEANS. BodyHeight (the
        // wrapper's default) makes it ascent+descent, CapHeight makes it the
        // cap height, and SystemHeight passes it through as the em size. The
        // Direct2D backend rescales the same way; without this every extent
        // and layout would disagree between backends.
        const float upem = float(face->unitsPerEm);
        float emSize = fontHeight;
        if ((fontFlags & int32_t(FontFlags::CapHeight)) != 0)
            emSize = fontHeight * upem / format->em.capHeight;
        else if ((fontFlags & int32_t(FontFlags::SystemHeight)) != 0)
            emSize = fontHeight;
        else // BodyHeight == 0, the default
            emSize = fontHeight * upem / (format->em.ascent + format->em.descent);

        format->fontSize = emSize;

        const std::string baseFamily = fontFamilyName ? fontFamilyName : "system-ui";
        format->fallbackFor = [this, baseFamily, fontWeight, fontStyle, fontStretch](uint32_t codepoint) {
            return findFallback(baseFamily, fontWeight, fontStyle, fontStretch, codepoint);
        };

        *returnTextFormat = format.release();
        return ReturnCode::Ok;
    }

    ReturnCode getTextGeometry(api::ITextFormat* textFormat, const char* utf8, int32_t length,
                               const Rect& layoutRect, int32_t options,
                               api::IFactory* factory, api::IPathGeometry** returnGeometry) override
    {
        *returnGeometry = {};
        auto* format = dynamic_cast<CpuTextFormat*>(textFormat);
        if (!format || !factory)
            return ReturnCode::NoSupport;

        std::vector<CpuTextFormat::Line> lines;
        format->layout(utf8, length, layoutRect.right - layoutRect.left, lines);
        if (lines.empty())
            return ReturnCode::Fail;

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

        const float lineHeight = format->lineHeight();
        const float blockHeight = float(lines.size()) * lineHeight;

        float y = layoutRect.top;
        switch (format->paragraphAlignment)
        {
        case ParagraphAlignment::Far:    y = layoutRect.bottom - blockHeight; break;
        case ParagraphAlignment::Center: y = 0.5f * (layoutRect.top + layoutRect.bottom - blockHeight); break;
        default: break;
        }

        const bool snap = (options & DrawTextOptions::NoSnap) == 0;
        detail::GlyphOutlineSink outline;
        outline.sink = sink.get();
        outline.scale = format->scale();

        for (const auto& line : lines)
        {
            float x = layoutRect.left;
            switch (format->textAlignment)
            {
            case TextAlignment::Trailing: x = layoutRect.right - line.width; break;
            case TextAlignment::Center:   x = 0.5f * (layoutRect.left + layoutRect.right - line.width); break;
            default: break;
            }

            float baselineY = y + format->baselineOffset();
            if (snap)
                baselineY = std::floor(baselineY + 0.5f); // whole-pixel baselines read sharper

            // Each run carries its own face, since a line may mix fonts.
            for (const auto& run : line.runs)
            {
                outline.scale = run.scale;
                for (size_t i = 0; i < run.glyphs.size(); ++i)
                {
                    outline.originX = x + run.offsetsX[i];
                    outline.originY = baselineY - run.offsetsY[i];
                    outline.figureOpen = false;
                    hb_font_draw_glyph(run.face->unscaledFont.get(), run.glyphs[i],
                                       detail::glyphDrawFuncs(), &outline);
                    if (outline.figureOpen)
                        sink->endFigure(FigureEnd::Closed);
                    x += run.advances[i];
                }
            }
            y += lineHeight;
        }

        sink->close();
        *returnGeometry = geometry.get();
        geometry.get()->addRef();
        return ReturnCode::Ok;
    }
};

}} // namespace gmpi::drawing
