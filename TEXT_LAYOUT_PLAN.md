# ITextLayout — a retained, immutable, styled text layout

Status: PROPOSAL v2.1 (2026-08-15), revised after a three-way adversarial code
review (ABI/conventions, backend implementability, consumer fit), then
simplified by an owner decision: the JUCE backend is on its way out (all three
main platforms render without it), so it stubs this API rather than
implementing it — see Resolved decisions #6. Companion
analysis: the SynthEdit repo's `tests/profile/OPTIMIZATIONS.md` (structure-view
text is the largest remaining render cost on both renderers) and the rich-text
recon that motivated this: IRichTextFormat is the right *mechanism* — a
retained layout — bolted to the wrong *semantics* for plain text (markdown
joins lines, restyles user strings, and only the D2D backend actually retains
its layout; the per-draw box is why Cocoa/JUCE rebuild every call).

## The one-sentence design

Everything is specified at creation; the object is immutable; therefore the
interface is one method, every backend can retain its fully-processed form
(shaped glyphs included), and the API cannot creep by accident.

## What it is / is not

| | ITextFormat (existing) | IRichTextFormat (existing) | **ITextLayout (new)** |
|---|---|---|---|
| holds | a reusable style sheet | markdown → layout | **one string + styling runs + a fixed box** |
| mutable | yes (setters) | box mutated per draw | **no — nothing after creation** |
| markdown | — | yes (the point) | **never — text is verbatim, newlines preserved** |
| retained work | none | D2D only | **D2D + CPU + Cocoa, including shaping (JUCE: stubs, falls back)** |

## API (GmpiApiDrawing.h)

```cpp
// Styling expressed as runs over the UTF-8 text, resolved at creation.
// EVERY override is opt-in via the flags bitmask: an all-zero run is a valid
// no-op that inherits the base format entirely. (FontWeight has no zero
// value, and gating weight/style also means a colour-only run over an italic
// base can't accidentally un-italicise it.)
namespace TextStyleFlags
{
    enum {
        Underline     = 1,
        Strikethrough = 2,
        HasColor      = 4,   // run.color overrides the default brush
        HasFontSize   = 8,   // run.fontSizeScale multiplies the base size
        HasFontFamily = 16,  // run.fontFamilyIndex selects from the families array
        HasFontWeight = 32,  // run.fontWeight overrides the base weight
        HasFontStyle  = 64,  // run.fontStyle overrides the base style
    };
}

// All-int32/float POD, hole-free under the header's pack(8): no bool members
// (bools don't project to the plain-C projection's all-int32_t convention).
// FROZEN AT v1: extensions arrive as new factory methods, never new fields —
// the struct crosses the C ABI boundary and has no size/version slot.
struct TextStyleRun
{
    int32_t begin;            // byte offset into the utf8 string; must land on
    int32_t length;           // a codepoint boundary (backends re-map to UTF-16
                              // or glyph clusters). Runs must be sorted,
                              // non-overlapping and in-bounds or creation fails.
    int32_t flags;            // TextStyleFlags bitmask
    FontWeight fontWeight;    // full enum, honoured when HasFontWeight
    FontStyle  fontStyle;     // honoured when HasFontStyle
    Color   color;            // honoured when HasColor
    float   fontSizeScale;    // multiplier of the base format's RESOLVED size,
                              // honoured when HasFontSize. A scale, not DIPs:
                              // FontFlags (BodyHeight/CapHeight) rescaling is
                              // applied by each backend exactly as it is to the
                              // base — the D2D rich path's hard-won lesson
                              // (DirectXGfx.cpp:600-608), and the only form a
                              // shared markdown translator can emit.
    int32_t fontFamilyIndex;  // into createTextLayout's families array,
                              // honoured when HasFontFamily
};

// INTERFACE 'ITextLayout'
// Immutable: text, styling, box, alignment and spacing are all fixed at
// creation. Single-threaded use, like every drawing object here.
struct DECLSPEC_NOVTABLE ITextLayout : gmpi::api::IUnknown
{
    // The same value ITextFormat::getTextExtentU would return for this text
    // at the layout's maxWidth (D2D keeps its topAdjustment subtraction):
    // tight bounds of the formatted text, alignment-independent. Computed
    // once at creation and cached — never re-measured.
    virtual gmpi::ReturnCode getTextExtentU(Size* returnSize) = 0;

    // {TBD-GUID}
    inline static const gmpi::api::Guid guid = { /* new */ };
};
```

Appended LAST to `IFactory` (vtable-append convention, cf. pushClipGeometry):

```cpp
// Base styling (family, size, weight, style, font flags, alignments, word
// wrapping, line spacing) is CAPTURED BY VALUE from `baseFormat` at creation —
// mutating the format afterwards does not affect the layout. runs/runCount may
// be null/0: a plain-text retained layout. runFamilies lists families runs may
// select via fontFamilyIndex (e.g. a monospace face); an entry that does not
// resolve renders those runs in the base family — creation NEVER fails for
// this reason (both existing rich backends already behave this way). May
// return null ⇒ caller falls back to drawTextU.
virtual gmpi::ReturnCode createTextLayout(
    const char* utf8, int32_t length,
    ITextFormat* baseFormat,
    float maxWidth, float maxHeight,
    const TextStyleRun* runs, int32_t runCount,
    const char* const* runFamilies, int32_t runFamilyCount,
    ITextLayout** returnTextLayout) = 0;
```

Appended LAST to `IDeviceContext`:

```cpp
// Point, not Rect: the layout owns its box. Text exceeding the box is still
// laid out against it and DRAWN (overflow is not dropped);
// DrawTextOptions::Clip behaves exactly as drawTextU's Clip does on that
// backend for the rect {point, point + box}. Runs without HasColor fill with
// defaultForegroundBrush — so a caller can tint a cached layout per draw
// (hover/selection) without rebuilding it.
virtual gmpi::ReturnCode drawTextLayout(Point point, ITextLayout* textLayout,
    IBrush* defaultForegroundBrush, int32_t options) = 0;
```

That is the entire surface: one frozen POD, one factory method, one context
method, one one-method interface. No setters anywhere — immutability is the
creep firewall. Future wants (hit-testing for text editors, per-line metrics)
are appended methods on ITextLayout later; no current consumer needs them
(TextEditModel does caret math on byte indices with native widgets).

## The parity contract (normative)

`drawTextLayout(p, plainLayout, brush, opts)` MUST be pixel-identical to
`drawTextU(text, baseFormat, Rect{p, p + box}, brush, opts)` for a RUN-FREE
layout on every SUPPORTING backend (D2D, CPU, Cocoa) — that is what makes it
a drop-in for existing call sites and keeps the byte-identical verification
bar. A backend may instead decline the API entirely by returning null from
createTextLayout (JUCE does); callers' drawTextU fallback keeps it rendering
exactly as today. Styled (run-bearing) layouts are best-effort per backend
and verified cpu-vs-d2d with a tolerance. Acceptance tests in gimpi_ui_tests:
run-free pixel equality against drawTextU on the three supporting backends,
plus a styled smoke test.

## Per-backend implementation notes (review-hardened)

- **DirectXGfx**: retain the `IDWriteTextLayout`, but match drawTextU's
  geometry exactly: create it with height `maxHeight + topAdjustment`
  (topAdjustment and design ascent captured as floats from the base
  TextFormat at creation), and draw at `(p.x, p.y − topAdjustment + adjust)`
  where `adjust` is the 0.5px baseline snap recomputed per draw from p.y —
  the snap is draw-point-dependent and cannot be baked. Colors: keep a
  (range, Color) list; per draw, SetDrawingEffect(solid brush created on the
  CURRENT drawing context) per colored range, then
  `SetDrawingEffect(nullptr, range)` after DrawTextLayout so the retained,
  factory-lifetime layout never holds device resources across draws (device-
  loss safety; brushes are never cached on the layout). This transiently
  mutates the native object — the API's immutability claim is about
  observable state. Zero extra work for run-free layouts. Extent measured
  once at creation. Fix the existing createRichTextFormat IDWriteTextLayout
  refcount leak in the same series.
- **CpuGfx / CpuTextEngine**: `CpuTextLayout` retains the fully shaped +
  line-broken `LaidOut` — layout() is const and deterministic; drawText
  already snapshots format state into LaidOut per call, the layout object
  just does it once. This skips UTF-8 decode, itemization, line breaking and
  the ~one hb_shape per paragraph per call. GlyphRun gains an
  effective-color/flags field (it already carries per-run emSize /
  obliqueShear / strikethrough); renderLaidOut's single-brush batching (one
  winding path fill, one merged coverage blend) is partitioned per color
  group, one pass each. Underline joins strikethrough's decoration-rect
  mechanism (underlinePosition/Thickness metrics are already loaded).
- **CocoaGfx**: retain the attributed string with
  kCTForegroundColorFromContextAttributeName on all non-colored text so the
  default brush tints via CGContextSetFillColorWithColor per draw — NO
  attributed-string copy or framesetter rebuild (today's rich path rebuilds
  both per draw). Retain the CTFramesetter (the shaping cost). CTFrame drops
  lines outside its path, so the effective (overflow-enlarged) box is
  precomputed at creation using drawTextU's own rules — possible because
  text, box and format are all creation-time data — with at most two cached
  CTFrame variants (clipped box / extended box), built lazily. Strikethrough
  is NOT native to CTFrameDraw (today's rich strikethrough is a silent
  no-op — fix with decoration rects in this series); underline is native.
- **JuceGfx**: STUBS ONLY — createTextLayout returns NoSupport (null layout),
  drawTextLayout returns NoSupport. JUCE is an obsolescent backend (the three
  main platforms render via D2D / Cocoa / CPU) and the review showed a real
  implementation would be the hardest of the four: juce::TextLayout cannot
  express uniform line spacing (would misalign pin labels), so parity would
  require retaining drawTextU's hand-rolled engine internals, plus
  decoration-rect underline/strikethrough. The callers' drawTextU fallback
  keeps SynthEditJuce rendering exactly as today, at today's speed.

## ABI rollout — exactly who must compile

Both new methods are pure virtual at the API level with a NoSupport default
only in `se::generic_graphics::GraphicsContextT` (Gfx_base.h) — the
pushClipGeometry precedent exactly. Pure-virtual is deliberate: a defaulted
method would let a forwarding bridge silently swallow drawTextLayout (the
preGraphicsRedraw-wrapper failure mode); pure-virtual makes every missing
implementation a compile error. Out-of-tree parties implement IDrawingClient,
not IDeviceContext/IFactory, so nothing external breaks.

The census (`grep pushClipGeometry` is the authoritative list):

`IDeviceContext`: gmpi::directx::GraphicsContext_base (covers GraphicsContext,
BitmapRenderTarget, Win7, and SynthEditLib's UniversalGraphicsContext
derivatives), gmpi::cocoa::GraphicsContext (covers SynthEditLib's Cocoa
universal contexts — .mm-only, invisible to a Windows build),
gmpi::jucegfx::GraphicsContextBase (NoSupport stub only), GraphicsContextT
(default; covers cpugfx::RenderTarget until the real one lands), and the
forwarding se::DeviceContextLegacyAdapter (SynthEditLib
GmpiCpuUniversalContext.h — the screenshot-CLI / software-renderer bridge;
one-line forward, mandatory).

`IFactory`: the backend factories (DirectXGfx Factory_base — also covers
SynthEditLib's Factory_SDK3 — cpugfx, Cocoa; JUCE stubs NoSupport) plus the
internal ICpuTextEngine seam and CpuTextEngine.

Hazard callout: the DirectX and JUCE BitmapRenderTargets are "emulated by
careful vtable layout" — an append is safe only while getBitmap stays the
first derived-class virtual (pushClipGeometry's append already proved it;
re-verify). Verification build must cover TIDE + EditorScreenshot + the Mac
.mm hosts, not just SynthEdit2 (the IDialogHost lesson).

## Relationship to IRichTextFormat (phase 2, optional)

Reimplement the supporting backends' rich text as `parseMarkdown →
TextStyleRun[] → createTextLayout` with one shared translator (headings emit
fontSizeScale — which is why the run field is a scale; code spans emit
HasFontFamily). JUCE keeps its existing rich-text implementation untouched.
Honestly scoped: this reproduces the CURRENT flat markdown output — bullets
are literal "  • " text with no hanging indent, links are display-text only,
one alignment per block. IRichTextFormat keeps its per-draw box semantics;
PostIt is unaffected. Not required for the SynthEdit win.

## First consumer (SynthEdit structure view)

Three cached ITextLayouts per ModuleViewStruct, built in **CreateModuleOutline
alongside the strings they render** (that is where lPlugNames/rPlugNames are
rebuilt; arrange()'s size-change geometry clear is already the trigger, and
this also rebuilds the Trailing-aligned right column on width-only resizes).
Header ordering fixed: measure the unwrapped header width ONCE at build time
with tf_header.getTextExtentU, cache the Size, then create the header layout
at the widened box — the per-frame measure dies, but the layout can't be what
answers it (its extent is wrapped inside its own box). The r.top −= 0.5 tweak
is absorbed by the draw Point; Leading/Trailing and lineSpacing(12,10) are
captured from the base formats; blank pin-alignment lines are preserved
verbatim (no markdown). Current drawTextU calls stay as the null-layout
fallback.

Honest accounting: the cached header extent recovers ~0.2 ms on d2d today
with NO new API (worth landing first as its own commit); the ITextLayout
stake is the ~2.7 ms of drawTextU calls on d2d and part of cpu's ~3 ms
(shaping + line breaking retained; glyph blits still run). Expect roughly
d2d 4.30 → ~2.3–2.7 ms, cpu 18.4 → ~16–17 ms.

Lifetime: per-view member layouts die with the view — deliberately NOT the
sharedGraphicResources/outlineCache pattern, which never evicts; a
(string, width)-keyed shared cache would leak at high cardinality (every
rename mints a key). Memory shape at 750 modules × 3 layouts is low tens of
MB worst case (CPU LaidOut holds glyph ids/positions; masks stay in the
shared atlas).

## Bookkeeping

- Drawing.h wrappers are load-bearing (the consumer uses them exclusively):
  RAII `TextLayout` class (operator bool + getTextExtentU, modeled on
  RichTextFormat), `Graphics::drawTextLayout`, and a `Factory::createTextLayout`
  helper taking the wrapper TextFormat + span of runs + span of family names —
  no assert on null, per the optional-capability convention. Note the native
  factory has NO family-fallback logic; the try-each-family loop lives only in
  the wrapper, which is why run-family fallback is specified at the API level
  above.
- The plain-C projection (projections/plain_c/gmpi_drawing.h) gains
  GMPI_ITextLayout + the two method slots + GMPI_TextStyleRun when regenerated.
- Stale comments to fix while here: Drawing.h:1578-1586 and PLAN.md's claim
  that the CPU backend lacks rich text (it has it since f3f89d6).

## Resolved decisions

1. Name: **ITextLayout** (IDWriteTextLayout/juce::TextLayout precedent;
   "ITextFormat2" would misdescribe an object that isn't a style sheet).
2. Per-run FontFamily: **in v1** — TextStyleRun is ABI-frozen, so dropping it
   is a one-way door that would kill phase 2 (code spans need monospace).
3. Underline + strikethrough: **in v1**, both flags-gated. Native on D2D/CPU
   (+ Cocoa underline); decoration rects only for Cocoa strikethrough (fixing
   the existing CTFrameDraw no-op in the same series).
4. Draw takes a **Point** — a Rect would reintroduce the per-draw box, which
   is the exact flaw being removed.
5. Run violations (unsorted/overlapping/mid-codepoint): **creation fails** —
   loud beats clamped for a programming error.
6. **JUCE omitted** (owner decision): the JUCE backend is obsolescent — all
   three main platforms render via D2D / Cocoa / CPU — and its implementation
   would have been the hardest of the four. It compiles NoSupport stubs (the
   pure-virtual append requires that much) and callers' drawTextU fallback
   preserves today's behavior. If JUCE ever needs the win, the review's
   retained-line-vector recipe is recorded in v2 history.
