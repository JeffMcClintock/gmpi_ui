# CPU backend plan (software renderer)

A pure-software GMPI-UI backend: no GPU, no platform graphics API. Primary target is
Linux (which may lack GPU support); it also serves as the high-quality software
fallback on Windows/macOS (replacing the 8-bit "shitty linear blending" path).
Guiding principle throughout: **simplicity foremost** — one rendering path, float
math, no specialised per-shape code.

## Pixel format (decided, researched Aug 2026)

Working format: **premultiplied, linear-light scRGB** — sRGB/709 primaries, values
may exceed [0,1] (wide gamut and HDR ride on out-of-range values at no precision
cost). Storage is **RGBA half-float** (`PixelFormat::RGBA_16f`, 8 bytes/px — the
same `64bppPRGBAHalf` format `createCpuRenderTarget` already produces on Windows);
arithmetic is **always fp32 in registers**. fp16 never appears in kernels — only in
the load/store span codec.

Why: correct linear compositing with none of fixed-point's normalize/overflow tax;
identical semantics to the Windows FP16 scRGB swap chain and macOS EDR, so software
and hardware backends are directly comparable; migration to native fp16 SIMD
(AVX-512 FP16/AVX10, aarch64 NEON) or GPU shaders changes kernels, not formats.
Key numbers: fp16's worst step in [0,1] is 1/2048 near white = 0.05% Weber (20×
below visibility); round-trips 8-bit sRGB with 19× margin, 10-bit with ~5×.
Rules: never *accumulate* in fp16 (sums/filter taps stay fp32); one blue-noise or
ordered dither at the final 8-bit output, never mid-pipeline. bf16 is unsuitable
(8-bit mantissa bands worse than 8-bit sRGB).

Colors: GMPI `Color` is **linear, non-premultiplied** float RGBA (the D2D backend
converts linear→sRGB for legacy 8-bit targets, not the reverse). Brushes
premultiply once at use: `src = (r·a, g·a, b·a, a) · opacity`.

## Architecture: one rendering path, three stages

Everything drawable funnels through **path → flatten → coverage → blend**.
No other code touches pixels.

```
geometry (scalar, cold)      rasterize (scalar, warm)       blend (hot, auto-vectorized)
─────────────────────────    ───────────────────────────    ─────────────────────────────
sink flattens curves→lines   signed-coverage accumulation   per row: load fp16→fp32,
stroke = widen to polygon    (font-rs style float buffer)   src·cov + dst·(1−srcA·cov),
transform at fill time       prefix-sum → coverage rows     store fp32→fp16
```

- **Flattening**: `Gfx_base.h`'s `GeometrySink` already reduces arcs→cubics
  (canvg port) and cubics→lines (AGG `curve4_div`); the CPU sink only implements
  `addLine`. The rest of the renderer sees only polygons with straight segments.
  Rectangles/ellipses/lines already route through geometry via
  `se::generic_graphics::GraphicsContext` — no specialised shape code, by design.
  Known limitation: curves flatten at geometry-build time in local space; a large
  zoom transform magnifies flattening error. Fix later by re-flattening on
  transform change (D2D-style "realizations" cached on the PathGeometry).
- **Rasterizer**: font-rs-style signed-area accumulation — walk each edge once,
  accumulate fractional signed coverage deltas into a float scratch buffer
  (bbox-sized), then prefix-sum each row to get analytic antialiased coverage.
  ~100 lines, no active-edge table, no sorting, no fixed-point. Nonzero fill:
  `min(1,|acc|)`; even-odd: fold `acc mod 2`. References: font-rs (Raph Levien),
  FreeType `smooth`, stb_truetype.
- **Stroking (milestone 3)**: stroke = geometry, not raster. Widen the polyline
  (per-segment quads + join wedges + caps; round joins are flattened arcs), then
  nonzero-fill the union. One rasterizer serves fill and stroke; because the whole
  widened outline lands in one coverage buffer before a single blend, translucent
  strokes don't double-darken at self-overlaps. `strokeContainsPoint` /
  `getWidenedBounds` reuse the same widener.
- **Brushes**: solid = splat. Linear gradient = per-row ramp `t = t0 + i·dt` into a
  premultiplied-linear LUT. Radial adds a sqrt per pixel. Bitmap brush = bilinear
  sample. All evaluated inside the same blend loop shape.

## Auto-vectorization contract

The hot loops are plain C++ over `float* __restrict`, structured so compilers
vectorize them without intrinsics:

1. **Stride rule**: surface stride in *pixels* is a multiple of 8 → rows are 64-byte
   multiples (8 px × 4 ch × 2 B); allocations 64-byte aligned.
2. **No tails**: coverage 0 makes the blend an identity (`dst = 0 + dst·1`), so
   spans round outward to chunk boundaries and inner loops run a whole number of
   chunks; row padding absorbs the edges.
3. **Chunk = 4 pixels (16 floats)** per iteration — 4×SSE2/NEON or 2×AVX2 ops;
   compilers unroll further on their own.
4. **fp16 in exactly one place**: the span codec (`loadSpan`/`storeSpan`). Scalar
   table/bit-twiddle version first (`detail::floatToHalf`/`halfToFloat` in
   BitmapMask.h); F16C / NEON intrinsic versions slot in behind the same call
   signature when profiling justifies them. `-march=x86-64-v3` lets GCC/Clang
   auto-vectorize `_Float16` conversion loops too.

Verify codegen once on GCC and Clang (`-O2`, and `-march=x86-64-v3`); resist
intrinsics until a profile demands them.

## Display output (milestone 7)

`endDraw` on a windowed target converts the dirty rect fp16-linear-scRGB → 8-bit
sRGB with a single ordered/blue-noise dither (1 LSB of the target), directly into
an XShm / `wl_shm` buffer. A 10-bit path is the same code with a different encode.
Offscreen (`createCpuRenderTarget`) targets skip this — tests and callers read the
fp16 pixels via `lockPixels`.

## Milestones

1. **Walking skeleton** — DONE (Aug 2026): `backends/CpuGfx.h` — fp16 surface,
   factory (`createPathGeometry`, `createStrokeStyle`, `createCpuRenderTarget`),
   solid brush, `clear`, `fillGeometry` via the coverage rasterizer.
   `fillRectangle`/`fillEllipse` working through the `Gfx_base` geometry routing
   proves the no-special-paths rule. PNG output via `helpers/SavePng.h`; gtest
   pixel probes.
2. **Rasterizer hardening** — DONE (Aug 2026): cross-backend battery
   (`tests/cpu_vs_d2d_tests.cpp`) renders identical scenes through D2D and the
   CPU backend and diffs them — fractional/aliased clips, nested clips, and
   off-surface scenes match D2D *exactly*; AA edges within a few code values.
   Implemented: exact segment clipping (left portions project onto x=0
   winding-exact, double-precision splits), D2D ALIASED pixel-centre clip rule,
   non-finite/degenerate-figure guards, hollow figures not filled, unbalanced
   clip-pop safety, RTNE fp16 stores. Found & fixed along the way: an old
   x/y radius swap in Gfx_base's arc flattener, a heap-underflow (`acc[-1]`)
   when interpolated x rounds an ulp below 0 (caught by adversarial review),
   several float→int UB cases at extreme coordinates.
3. **Stroker** — DONE (Aug 2026): flat/square/round caps, miter/bevel/round
   joins with miter limit, open and closed figures, strokes under transform
   (widened in local space, so a non-uniform scale gives an elliptical pen like
   D2D), dashes (all built-in styles, custom arrays and dash offset — the
   polyline is split into "on" runs before widening), plus
   `strokeContainsPoint`/`getWidenedBounds` off the same widener.

   The outline is traced as a **contour**, not stamped as overlapping quads,
   wedges and discs. That matters specifically because this is a coverage
   rasterizer: it accumulates *area* per pixel, so overlapping pieces
   double-count inside a partially covered pixel — a round cap over its own
   segment quad measured ~95% covered where the true union is 50%. Tracing one
   boundary has no overlap to double-count. Closed figures emit an annulus
   (outer ring plus an oppositely wound inner ring); open figures emit a single
   contour, up one side, around the end cap, back the other side.

   Three D2D behaviours worth remembering, each established by rendering the
   same scene through both backends rather than from the docs:
   * `LineJoin::Miter` past the miter limit does **not** bevel — it keeps the
     spike and cuts it off flat at the limit distance. `MiterOrBevel` is the
     one that bevels.
   * A dashed stroke's phase starts at the figure's first point, so the
     rectangle helper must walk the same way D2D's native rectangle does (top
     edge first, then clockwise) or every dash lands on the wrong edge.
   * There is no winding fix-up on the annulus, deliberately: +normal is always
     the same rotation of the travel direction, so the outer ring is always
     negatively wound whichever way the source figure is wound. Canonicalising
     on "the first ring" instead flips the outer ring for half of all inputs,
     and overlapping stroke bands then cancel to holes under nonzero fill.

   Known gap: when the pen is wider than the figure, the inward offset folds
   through itself. A full fix is offset-curve self-intersection removal; for
   now the inner ring is dropped when the pen exceeds half the figure's
   smaller bounding dimension, which covers the common "fat pen on a small
   shape" case. Partial folds on non-convex figures can still leave artefacts.
4. **Curve accuracy**: device-space flattening tolerance (re-flatten under zoom).
5. **Gradients** — DONE (Aug 2026): linear and radial (including elliptical
   radii and a focal origin offset), all three extend modes, brush opacity and
   brush transform, usable for both fills and strokes.

   Brushes implement an internal `CpuBrush` interface that fills a span of
   premultiplied linear RGBA, so the blend loop never branches on brush type —
   a solid colour is just a constant span. Stops interpolate in **straight
   (non-premultiplied) linear** colour: D2D uses
   `D2D1_COLOR_INTERPOLATION_MODE_STRAIGHT`, and an alpha-varying gradient
   matches it at zero pixel difference. Linear gradients walk `t` incrementally
   along the span (the per-row ramp); radial solves the standard focal
   quadratic per pixel.

   Fixed in the DirectX backend along the way: `createGradientstopCollection`
   hard-coded `D2D1_EXTEND_MODE_CLAMP`, so Wrap and Mirror silently rendered as
   Clamp on Windows. With that passed through, all three modes match the CPU
   backend exactly. `Drawing.h` also gained a `createGradientstopCollection`
   overload taking an ExtendMode — the wrapper previously had no way to ask for
   one.

   Deliberate divergence: a zero-length linear axis is undefined, and D2D's
   answer is not worth copying (it paints a fixed neutral grey that does not
   depend on the stops at all). We paint the last stop.

   Robustness, from an adversarial review that measured D2D as ground truth:
   `Drawing.h`'s `invert()` has **no zero-determinant guard**, so any collapsed
   or near-collapsed transform (a scale animating through zero, or one small
   enough that the determinant underflows) inverts to an infinite matrix. That
   made the gradient parameter non-finite, and a non-finite `t` used to sail
   through every comparison in `colorAt` — reading one past the end of a
   single-stop collection, and painting NaN over the whole chunk-aligned span
   including zero-coverage pixels. NaN is permanent in the surface too, because
   `NaN * 0` is still NaN, so nothing but a `clear()` repairs it. `colorAt` now
   pins a non-finite `t` to 0 and its index is clamped, which makes it total.
   The radial focus is clamped by vector **length**, not per-axis: a diagonal
   offset like (0.9, 0.9) survives a per-component clamp with |f| = 1.27, the
   focal discriminant goes negative, and a wedge of the plane collapses to flat
   last-stop colour.

   `invert()` itself was fixed separately (it is shared API used by every
   backend): it now returns identity when there is no usable inverse, and
   `tryInvert()` reports failure for callers that care. The test is on the
   finiteness of the *result*, not `det != 0`, because a determinant small
   enough that `1/det` overflows is equally unusable.
6. **Bitmaps & offscreens** — DONE (Aug 2026): `drawBitmap` (both interpolation
   modes, source-rect cropping, opacity), bitmap brushes (wrap + nearest,
   matching what the D2D backend hard-codes), `createCompatibleRenderTarget`
   (unblocks `CachedBlur`), and `loadImageU`.

   **One internal format, still.** Everything is fp16 premultiplied linear
   RGBA; decoded files convert once on load. One sampler serves both
   `drawBitmap` and bitmap brushes — they differ only in how local space maps
   to source pixels and in the edge rule (clamp-to-source-rect vs wrap).
   `drawBitmap` builds a rect geometry and goes through the ordinary fill path,
   so transform, clipping and antialiasing all come for free and it adds no
   pixel-touching code of its own.

   **Platform code is isolated to one file.** `backends/CpuGfx.h` contains
   none: it takes a decoder callback, and `helpers/DecodeImage.h` supplies one
   (WIC on Windows, ImageIO on macOS, JUCE elsewhere — the same arrangement
   `SavePng.h` uses). They meet at `helpers/DecodedImage.h`, which is just the
   interchange struct: 8-bit sRGB RGBA, straight alpha. A host can substitute
   its own decoder for an asset pipeline or a format we don't cover.

   **`Mask` and `SRGBPixels` render targets** (added Aug 2026, when the Linux
   build proved `CachedBlur` needs them — drop shadows, glows and neumorphic
   bumps all render through an 8-bit mask). Rendering stays fp16 in every case;
   the flags change only what `lockPixels` *presents*: `Alpha_8i` (one coverage
   byte, tightly packed — no stride padding, because callers index it as
   width*height) or `BGRA_sRGB_8i`. `endDraw` then rounds the surface through
   that format, because those targets really are 8-bit buffers on the DirectX
   and JUCE backends and 8-bit **linear** storage is coarse where it hurts: one
   step near black is ~20 sRGB levels once displayed, so skipping this makes the
   CPU backend quietly better than the others rather than the same.

   And a gamma trap worth stating plainly. An `SRGBPixels` bitmap holds LINEAR
   bytes — that is what the renderer writes and what `lockPixels` returns —
   but *sampling* one runs those bytes through the sRGB curve, because that is
   what D2D does with a 32bppPBGRA buffer (the "wrong gamma but consistent with
   SE 1.5" note in `DirectXGfx.cpp`). Measured, not assumed: 50% blue over red
   composites to linear 0.5, which is sRGB 188, and D2D's reference image has
   127 — exactly 0.5 re-decoded through the curve. The CPU sampler reproduces
   it, deliberately: a SynthEdit panel must not change appearance on Linux.

   Two fixes fell out of testing this. Inner stroke joins now use the true
   corner (where the two offset lines meet) rather than pushing both offset
   points: the crossover loop that made closed the notch, so an inner corner
   came out fully covered where D2D measures 0.75. And `SavePng.h` was writing
   premultiplied bytes into PNG, which has no premultiplied form — every
   translucent pixel was saved too dark (50%-alpha white round-tripped as 50%
   grey).
7. **Present path**: dithered sRGB encode + X11/Wayland blit.
8. **Text** ← current. Scope includes **CJK and colour emoji**; right-to-left
   (bidi, cursive shaping) is explicitly excluded.

   **HarfBuzz**, vendored, exactly as JUCE does it — one amalgamation source
   file (`harfbuzz.cc`), no HarfBuzz build system involved. It is the only
   dependency, and it covers shaping, glyph outlines, and colour-glyph access
   in one library. (JUCE reached the same conclusion: `Typeface::
   getOutlineForGlyph` pulls outlines from HarfBuzz and hands them to JUCE's
   own `EdgeTable` rasterizer. Skia does the opposite — it delegates glyph
   masks to the platform via `SkScalerContext` — which is the wrong trade for
   us, since the whole point of this backend is not depending on the platform.)

   **Glyphs stay paths.** `hb_font_draw_glyph` emits move/line/quadratic/cubic,
   which maps one-to-one onto the existing geometry sink, so text goes through
   the ordinary fill path and adds no pixel-touching code. Text is the last
   milestone and it does not break the "one path to pixels" property.

   **Colour emoji largely reduce to work already done.** `hb_font_paint_glyph`
   drives a callback interface whose operations are nearly a description of
   milestones 3-6: `color` is a solid fill, `linear_gradient` and
   `radial_gradient` are milestone 5 (its radial is the focal two-circle form
   already implemented), `image` is milestone 6 plus the decoder seam, and
   `push_transform`/`push_clip_rectangle` already exist. Bitmap emoji
   (CBDT/sbix, i.e. Apple Color Emoji and older Noto) arrive as PNG blobs from
   `hb_ot_color_glyph_reference_png` and feed the existing decoder.

   Genuinely new work, in cost order: font fallback and run itemisation (no
   font covers Latin + CJK + emoji, so text must be split into runs per
   covering font); line breaking (UAX #14 — CJK breaks between characters, not
   at spaces) plus grapheme clustering (UAX #29, so ZWJ sequences, skin-tone
   modifiers and flag pairs are not split); **geometry clipping**, since
   `pushClipGeometry` is still `NoSupport` here and COLRv1's `push_clip_glyph`
   needs it; a sweep (conic) gradient brush, which COLRv1 has and we do not;
   and `push_group`/`pop_group`, which needs an offscreen (have it) plus
   non-`over` compositing (do not). SVG-in-OpenType is out of scope.

   Font *discovery* is the platform-specific part, and gets the same treatment
   as image decoding: a callback on the factory, platform implementations in
   `helpers/FontProvider.h` (DirectWrite, CoreText, fontconfig), meeting at a
   platform-free interchange struct. `backends/CpuGfx.h` stays free of both
   platform code and HarfBuzz — it takes an `ICpuTextEngine` seam, and
   `helpers/CpuTextEngine.h` is the only file that includes `hb.h`.

   Parity note, and a trap. `FontFlags::BodyHeight` is documented as "the
   requested height is ascent+descent" and is the wrapper's default — but
   **its value is 0**, so the Direct2D backend's test
   `(fontFlags & FontFlags::BodyHeight) != 0` can never be true. DirectXGfx
   computes a body-height scale factor and then never applies it, so in
   practice the default means "em size = requested height". The CPU engine
   mirrors that exactly, including the dead branch, because rendering every
   string ~10% smaller than Windows would be far worse than matching a
   questionable behaviour. `CapHeight` (value 1) works and is applied.

   With that matched, CPU text metrics agree with DirectWrite **exactly** —
   ascent, descent, lineGap, capHeight, xHeight, advance width and multi-line
   heights all to the digit (`CpuVsD2D.TextMetricsAgreeWithDirectWrite`). Line
   advance includes lineGap; total height for n lines is (n-1) advances plus
   one body height, with no trailing gap. Both facts were measured, not assumed.

   Open decision for the owner: repairing the D2D branch would make
   `BodyHeight` behave as documented, but would resize text in every existing
   UI built against the current behaviour. If it is ever repaired, the matching
   place to change is `createTextFormat` in `helpers/CpuTextEngine.h`.

   Stages: **A** shaping + font discovery + monochrome glyphs (this alone gets
   Latin *and* CJK) — **DONE (Aug 2026)**; **B** line breaking + grapheme
   clusters + font fallback — **DONE (Aug 2026)**; **C** bitmap emoji —
   **DONE (Aug 2026), but see the platform note below**;
   **D** COLRv1 emoji — **DONE (Aug 2026)**; and a **glyph atlas — DONE
   (Aug 2026)**: cached coverage masks feed the same blender, since it already
   consumes a coverage array and does not care where it came from.

   The atlas caches rasterized glyph coverage keyed by face, glyph, device
   scale and sub-pixel position, replacing per-draw outline extraction,
   flattening and rasterization with a blend. Measured 1.63x faster on a line
   of Latin text in a Debug build. `raster::rasterizeCoverage` is now shared by
   the atlas and by geometry clipping, so neither can drift from filling on
   what a fill mode means.

   It is an optimisation, not a rendering change, and the test proves that by
   rendering the same scene both ways and comparing pixels — the engine has a
   `useGlyphAtlas` switch purely so the two paths can be compared. Agreement is
   close but deliberately not bit-exact: origins are quantised to quarter
   pixels (as FreeType and Skia do), so they can differ by up to an eighth of a
   pixel. Measured worst 0.12, mean 0.05 over the ~9% of pixels that are glyph
   edge.

   The fast path needs the software backend, a solid brush and a transform
   without rotation or skew; everything else falls back to the geometry path,
   which is why rotated text still draws. The cache is capped at 8 MB and
   cleared wholesale when exceeded — text redraws constantly, so it refills at
   once.

   Trap worth remembering: quantise the position to the sub-pixel grid FIRST,
   then split into whole pixels and a bucket. Taking the fraction before
   rounding drops the carry, so a fraction of 0.9 buckets as 0 without
   advancing the integer part and the glyph lands a whole pixel left. That bug
   showed as worst 0.95 against the geometry path, versus 0.12 once fixed.

   Stage B added `helpers/TextSegmentation.h` (portable, no dependencies) and
   font fallback. Text is now itemised into runs by covering font, so
   "Hi 你好" renders from one string with Arial and a system CJK font side by
   side; fallback lookups are cached per codepoint because they hit the system
   font database. A line may mix fonts, so each run carries its own face and
   scale, while line height stays on the *primary* font — otherwise a line
   would change height just because one fallback glyph appeared in it.

   Wrapping was also rebuilt. It used to re-shape a candidate string per word,
   which is quadratic and cannot wrap CJK at all (no spaces). Now runs are
   shaped **once**, glyphs carry their source byte cluster, and breaking walks
   the accumulated advances choosing the last permitted opportunity that fits.

   Segmentation is a deliberate SUBSET of UAX #14 and #29, documented range by
   range: combining marks, ZWJ sequences, variation selectors, skin-tone
   modifiers and regional-indicator pairs all stay in one cluster, and breaks
   are offered between ideographs with kinsoku rules keeping closing
   punctuation off the start of a line. Not covered: Indic and other complex
   scripts, whose clusters need script-specific tables.

   Stage C draws bitmap colour glyphs (CBDT/sbix) — the PNG blobs a font hands
   back from `hb_ot_color_glyph_reference_png`. They are decoded once and
   cached per (face, glyph), then drawn through the ordinary public
   `drawBitmap`, so colour emoji still add no pixel-touching code. The text
   seam changed shape to allow it: the engine no longer returns geometry, it
   *draws* into the device context through public calls, which is what lets one
   string mix filled outlines and bitmaps. `helpers/DecodeImage.h` gained a
   memory decoder, since a font carries PNGs as blobs rather than files.

   **Platform note, measured rather than assumed.** Which stage lights up emoji
   depends on the font's colour table, and a probe test reports it:
   Windows' *Segoe UI Emoji* has **no PNG glyphs at all** — it is COLRv0 layers
   plus COLRv1 paint — so stage C renders nothing there and **emoji on Windows
   need stage D**. Stage C is what serves Apple Color Emoji (sbix) and older
   Noto Color Emoji (CBDT). The emoji drawing test skips with that reason
   rather than pretending to pass; the memory decoder it depends on is tested
   directly and unconditionally.

   Stage D draws COLRv1 vector colour glyphs, which is what actually renders
   emoji on Windows. A probe recorded the operations Segoe UI Emoji really
   emits across a spread of glyphs, and the answer decided the scope:
   `push_clip_glyph` (325), `push_transform` (791), `linear_gradient` (179),
   `radial_gradient` (109), `color` (37), `push_clip_rectangle` (7) — and
   **no** sweep gradients, groups, blend modes or images at all. Those are left
   unimplemented rather than approximated, so a font needing them paints
   nothing (visible) instead of something wrong (not).

   Everything else was already here: `color` is a solid fill, the gradients are
   milestone 5, and the transforms are `setTransform`. The one genuine
   prerequisite was **geometry clipping** — `pushClipGeometry` is now real:
   the geometry is rasterized to a coverage mask, intersected with the enclosing
   clip, and multiplied into coverage at fill time, so a shaped clip is
   antialiased like any other edge. Fill and clip share one fill-mode fold, so
   they cannot disagree about what nonzero means. It matches D2D (which uses
   PushLayer) within antialiasing jitter.

   `clear()` respects the shaped clip too. It is still a REPLACE rather than a
   blend — what gets interpolated is the clip's partial coverage, so the
   shape's antialiased edge survives instead of being squared off to its
   bounding box.

   Worth remembering: the y-flip from font space (y-up) belongs in the glyph's
   base transform, NOT at the leaves. Flipping in the outline sink and in the
   gradient coordinates puts it on the wrong side of HarfBuzz's own paint
   transforms, which operate in font space — the emoji still renders, but
   layers land in the wrong places and the result is mud.

   Stage A shipped as `helpers/CpuTextEngine.h` (the only file that includes
   `hb.h`) plus `helpers/FontProvider.h` / `helpers/FontFile.h` for discovery.
   Shaping, metrics, extents, explicit newlines, word wrap, both alignment
   axes, line-spacing override, and the `FontFlags` rescale all work, and CJK
   renders given a font that covers it. Glyph outlines are filled with
   **nonzero** winding — counters (the hole in an 'o') come out wrong
   otherwise, since glyphs wind them opposite to the exterior.

   Known gaps, all deliberate: outlines are re-extracted per draw rather than
   cached (the glyph atlas), and `IRichTextFormat` markdown is unimplemented.

   From the text-engine review, two findings worth remembering:
   * **Font selection must prefer the PRIMARY font**, re-entering it as soon as
     it covers the text, which is what DirectWrite's `MapCharacters` does.
     Preferring whichever font the previous character used makes fallback
     sticky, and because fallback faces cover ASCII (emoji fonts carry 0-9 and
     `#` for keycaps; CJK fonts carry half-width Latin), one leading symbol
     dragged the entire rest of the string into the wrong font — measured:
     `"<emoji> Hello 2024"` itemised as a single Segoe UI Emoji run.
   * **Fallback resolves per codepoint**, so a page of CJK asks for a font
     hundreds of times. Faces are now shared per distinct font *file*; without
     that, every answer kept its own private copy of a multi-megabyte font.

   A note on testing that: width is a **useless** detector for wrong-font bugs.
   Yu Gothic UI and Segoe UI Emoji both measure "Hello 2024" within about a
   pixel of Arial, so the first regression test passed happily with the bug
   still in. The test asserts font identity per run instead.

   Testing differs here, and the fixture already anticipates it: text is
   compared by correlation with per-platform reference images, because
   different rasterizers legitimately differ. Metrics and layout (extents, line
   counts, alignment) are asserted exactly against D2D; pixels are not.

Perf posture: optimize nothing before milestone 2's goldens pass. The architecture
(spans, chunks, codec seam) is the headroom — SIMD intrinsics and threading slot in
later without redesign.

## Testing

- `gimpi_ui_tests` drives everything through `createCpuRenderTarget` already; the
  CPU backend implements the same seam, so the existing drawing tests can
  eventually run against it with per-backend reference images (the fixture already
  supports platform-specific references).
- **Linux runs on this backend as of Aug 2026** — JUCE is gone from that build.
  `tests/DrawingTestContext_cpu.cpp` wires `cpugfx::Factory` + `CpuTextEngine`,
  and the only system dependencies are fontconfig (font discovery) and libpng
  (image codec), so it builds and runs headless with no GPU and no display
  server. 167 pass, 14 skip, 0 fail.

  The reference images tell the real story. The old JUCE-era `*_linux.png`
  overrides were **deleted, not regenerated**: with the CPU backend, Linux now
  matches the *shared Direct2D* references for every non-text test — geometry,
  gradients, bitmaps, clips, masks, blurs. That is a far stronger check than
  blessing our own output, since a bug can no longer hide inside a reference we
  generated ourselves. Only the 29 text tests keep a `_linux.png`, and for a
  reason that will not go away: a different rasterizer (HarfBuzz + our coverage
  rasterizer vs DirectWrite) and a different font (fontconfig substitutes
  Liberation Sans for Arial).

  Three real bugs surfaced only by building on Linux, none of which any Windows
  test could have caught:
  * `BitmapRenderTargetFlags::Mask` returned `NoSupport`, so `CachedBlur`
    dereferenced a null render target — a hard crash, not a failure.
  * Colour emoji fell back to a *monochrome* glyph. fontconfig ranks family
    similarity above coverage, so a fallback request for U+1F600 while the base
    family is Arial lands on DejaVu Sans, which does cover it, in outline form.
    The provider now asks fontconfig's `emoji` generic family first for
    emoji-presentation codepoints (what browsers and GTK do), and verifies real
    coverage before accepting a match.
  * The macOS branch of the test CMakeLists never linked HarfBuzz, so
    `cpu_text_tests.cpp` could not have built there at all.

  Skips are honest gaps, not noise: 12 rich-text tests (markdown parsing and
  mixed-format runs are unimplemented in this backend — the fixture asks the
  factory and skips rather than crashing) and 2 COLRv1 emoji tests (Noto Color
  Emoji on this box is CBDT/PNG, so the COLR path has no font to exercise).
- Cross-backend comparison on Windows: render the same scene via DirectX and CPU
  factories and diff (tolerance-based — FMA contraction and D2D's rasterizer will
  differ by an LSB or two on AA edges).
- Determinism: tolerance-based comparisons, not bit-exact across compilers.
