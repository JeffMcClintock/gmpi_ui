#ifndef GMPI_MAC_COLORDIALOG_H
#define GMPI_MAC_COLORDIALOG_H

// Single-header (stb-style) gmpi::api::IColorDialog implementation for macOS,
// backed by the process-wide shared NSColorPanel.
//
// Define GMPI_MAC_COLORDIALOG_IMPLEMENTATION in exactly one .mm per binary
// before including, to emit the Obj-C @implementation block (mirrors the
// MacTextEdit.h / MacKeyListener.h house convention). The @interface + the C++
// class are always declared; only the @implementation is gated, so the header
// can never register the ObjC helper class twice if it is ever pulled into a
// second translation unit in the same binary.
//
// -------------------------------------------------------------------------
// CROSS-PLATFORM BEHAVIOUR NOTE (read before touching this or the Win class)
// -------------------------------------------------------------------------
// Windows (DrawingFrameWin.cpp, GMPI_WIN_ColorDialog) uses comdlg32
// ChooseColorW, which is MODAL and fires IColorDialogCallback::onComplete
// EXACTLY ONCE: Ok(pickedColor) when the user presses OK, or
// Cancel(initialColor) when the dialog is dismissed. The pin only changes on
// commit.
//
// macOS NSColorPanel is a process-wide MODELESS singleton
// ([NSColorPanel sharedColorPanel]) with NO OK/Cancel buttons and no
// sheet-with-completion-handler equivalent. The reliable native signals are:
//   (i)  changeColor:  (target/action, fired continuously as the user drags), and
//   (ii) NSWindowWillCloseNotification (the panel was dismissed).
// This implementation DEFERS notification until dismissal, to match the modal
// Windows behaviour: the app is told once, on commit, not live on every drag.
// Each USER-driven changeColor: only RECORDS the current colour; the single
// onComplete is delivered when the session ends (in -finish), which happens
// when the panel closes OR the picker is handed off to another swatch:
//   * if the user changed the colour -> onComplete(Ok,     finalColour)
//   * otherwise                      -> onComplete(Cancel, initialColour)
// That mirrors the Windows OK/Cancel outcomes on a panel that has no OK/Cancel
// buttons, and means the pin/undo history sees exactly ONE write per session
// (no live-drag churn) — and no onComplete can re-enter the caller mid-drag.
// sdk::ColorDialogCallback (NativeUi.h) runs onSuccess on the Ok and drops the
// Cancel.
//
// -------------------------------------------------------------------------
// MEMORY MODEL: MRR (manual retain/release), NOT ARC. This target compiles
// with no -fobjc-arc (proven by the explicit [trackingArea release] in
// GMPI_VIEW_CLASS::onClose). Hence explicit alloc/init/retain/release/
// [super dealloc]; no __bridge; the gmpi callback is kept alive by an explicit
// addRef()/release() pair, not by ARC block capture.
//
// LIFETIME: NSColorPanel does NOT retain its target (classic AppKit weak/assign
// target). The modeless panel can outlive the C++ GMPI_MAC_ColorDialog (the
// caller typically holds the IColorDialog in a shared_ptr member that is
// released on its very next interaction). Therefore:
//   * The ObjC helper (GMPI_MAC_ColorPanelTarget_03) owns a ref on the gmpi
//     callback (addRef in init, release in dealloc).
//   * The ObjC helper self-anchors with an extra [self retain] in init so it
//     stays alive as long as it is the panel's (unretained) target, regardless
//     of whether the C++ dialog still exists. That self-anchor is dropped in
//     -finish (invoked on panel close, or on hand-off to a newer dialog).
//   * The C++ dialog also [retain]s the helper (an independent ref) and
//     [release]s it in its destructor; releasing the dialog early just drops
//     that extra ref, never the self-anchor.
//
// SHARED-SINGLETON HAND-OFF: opening a second colour dialog re-targets the one
// shared panel. showAsync eagerly tears the PREVIOUS helper down (it can no
// longer fire live events once re-targeted) so at most one live helper exists
// at a time. Belt-and-braces, every teardown is ALSO guarded by an identity
// check (panel.target == self) so a stale helper can never clobber a newer
// dialog's wiring, and each helper always removes its own notification
// observer.
//
// CAVEAT (documented, not fixed here): the modeless panel can also outlive the
// host DrawingFrameCocoa VIEW (editor close). Nothing in this file de-targets
// on view teardown; the helper stays alive via its self-anchor, still targets
// the process-wide panel and still retains the callback until the user closes
// the panel. The consumer's callback must therefore not dereference document
// state that dies at editor-close. A deterministic fix would close/orphan the
// panel from DrawingFrameCocoa::DeInit — out of scope for this dialog file.

#import <Cocoa/Cocoa.h>
#include <cmath>
#include "GmpiSdkCommon.h"
#include "GmpiUiDrawing.h"        // gmpi::drawing::Color, colorFromArgb, linearPixelToSRGB
#include "helpers/NativeUi.h"

// Objective-C can't load the same class into two plugins in one process; give
// this helper a per-iteration unique name, matching the _03 house convention
// (cf. GMPI_EVENT_HELPER_CLASSNAME_03, GMPI_VIEW_VERSION_03).
#define GMPI_MAC_COLORPANEL_TARGET_CLASS GMPI_MAC_ColorPanelTarget_03

// ---------------------------------------------------------------------------
// ObjC helper: the shared NSColorPanel's target/action + close observer.
// Owns a ref on the gmpi callback and self-anchors for the panel session.
// ---------------------------------------------------------------------------
@interface GMPI_MAC_COLORPANEL_TARGET_CLASS : NSObject
{
    gmpi::api::IColorDialogCallback* _callback; // owned: addRef in init, release in dealloc
    gmpi::drawing::Color _initialColor;         // seed, returned on Cancel if untouched
    gmpi::drawing::Color _lastColor;            // last recorded colour, returned on Ok
    BOOL _completed;                            // one-shot guard so the terminal notify fires once
    BOOL _changed;                              // did the user actually pick a new colour?
    BOOL _seeding;                              // suppresses the programmatic seed changeColor:
}
- (instancetype)initWithCallback:(gmpi::api::IColorDialogCallback*)cb
                    initialColor:(gmpi::drawing::Color)initialColor;
- (void)changeColor:(id)sender;
- (void)beginSeeding;
- (void)endSeeding;
- (void)tearDownNow;   // eager hand-off teardown when a newer dialog re-targets the panel
@end

#ifdef GMPI_MAC_COLORDIALOG_IMPLEMENTATION

@implementation GMPI_MAC_COLORPANEL_TARGET_CLASS

- (instancetype)initWithCallback:(gmpi::api::IColorDialogCallback*)cb
                    initialColor:(gmpi::drawing::Color)initialColor
{
    if ((self = [super init]))
    {
        _callback = cb;
        if (_callback)
            _callback->addRef();

        _initialColor = initialColor;
        _lastColor = initialColor;
        _completed = NO;
        _changed = NO;
        _seeding = NO;

        // The panel does not retain its target, and NSNotificationCenter does
        // not retain observers. Self-anchor so we survive for the whole panel
        // session even if the C++ dialog is released first. Balanced by the
        // [self autorelease] in -finish.
        [self retain];

        [[NSNotificationCenter defaultCenter]
            addObserver:self
               selector:@selector(panelWillClose:)
                   name:NSWindowWillCloseNotification
                 object:[NSColorPanel sharedColorPanel]];
    }
    return self;
}

- (void)beginSeeding
{
    _seeding = YES;
}

- (void)endSeeding
{
    _seeding = NO;
}

// The panel's target/action. AppKit sends this on every colour change. In the
// DEFERRED model it only RECORDS the current colour — the app is notified once,
// later, in -finish. No onComplete fires here, so a live drag can neither
// re-enter the caller nor churn its undo history.
- (void)changeColor:(id)sender
{
    // Suppress the programmatic changeColor: that -[NSColorPanel setColor:]
    // synthesises while we seed the panel — that is not a user commit.
    if (_seeding)
        return;

    NSColorPanel* panel = [NSColorPanel sharedColorPanel];

    // panel.color may be in any colour space (calibrated / device / named);
    // getRed:green:blue:alpha: on a non-RGB colour throws or yields garbage.
    // Convert to sRGB first, and null-check (nil if conversion is impossible).
    NSColor* c = [[panel color] colorUsingColorSpace:[NSColorSpace sRGBColorSpace]];
    if (!c)
        return; // non-convertible colour: ignore this event

    CGFloat r = 0, g = 0, b = 0, a = 1;
    [c getRed:&r green:&g blue:&b alpha:&a]; // 0..1 GAMMA-ENCODED sRGB

    // sRGB float -> sRGB byte (round-to-nearest; NSColor gives continuous
    // floats, truncation would drift up to 1 LSB vs Windows) -> LINEAR gmpi
    // Color via the SAME SRGBPixelToLinear LUT ChooseColorW's result uses,
    // so a colour originally sourced from 8-bit round-trips identically on both
    // platforms. Alpha forced to 1.0f to match Windows (comdlg32 has no alpha).
    auto to255 = [](CGFloat v) -> uint8_t
    {
        if (v < 0.0) v = 0.0; else if (v > 1.0) v = 1.0;
        return static_cast<uint8_t>(std::lround(v * 255.0));
    };
    const gmpi::drawing::Color picked =
        gmpi::drawing::colorFromArgb(to255(r), to255(g), to255(b), 1.0f);

    _lastColor = picked;   // record only; the app is notified once, on dismissal
    _changed = YES;
}

// End the picker session exactly once: deliver the single deferred
// notification, then de-target, stop observing, and drop the self-anchor.
// One-shot on _completed (guards BOTH the notify AND the teardown), so a
// second call — e.g. panelWillClose: arriving after a hand-off tearDownNow —
// is a harmless no-op and the self-anchor is released exactly once.
- (void)finish
{
    if (_completed)
        return;
    _completed = YES;

    // Deferred delivery: the app hears about the pick now, on dismissal, once.
    // Ok(final) if the user changed the colour, else Cancel(initial) — the
    // closest mapping to the Windows OK/Cancel outcomes on a button-less panel.
    // NOTE: onComplete may synchronously rebuild the caller's view tree (a pin
    // write can tear down the ColorSwatchView and hence the C++ dialog, which
    // [release]s us). Safe: our session self-anchor keeps us alive across the
    // call, and the _completed guard makes any re-entrant -finish a no-op.
    if (_callback)
    {
        if (_changed)
            _callback->onComplete(gmpi::ReturnCode::Ok, _lastColor);
        else
            _callback->onComplete(gmpi::ReturnCode::Cancel, _initialColor);
    }

    // Only de-target if WE are still the current target: a newer colour dialog
    // may have re-targeted the shared panel since we installed ourselves.
    NSColorPanel* panel = [NSColorPanel sharedColorPanel];
    if ([panel target] == self)
        [panel setTarget:nil];

    [[NSNotificationCenter defaultCenter] removeObserver:self];

    // Drop the session self-anchor from -init. Deferred (autorelease) so we do
    // not deallocate while still unwinding a notification dispatch.
    [self autorelease];
}

// The panel was dismissed — end the session.
- (void)panelWillClose:(NSNotification*)note
{
    [self finish];
}

// Eager teardown when a newer dialog takes over the shared panel: end this
// session now (it can no longer fire once re-targeted) so the orphaned helper
// does not linger for the app's lifetime.
- (void)tearDownNow
{
    [self finish];
}

- (void)dealloc
{
    // Defensive: never leave the shared panel targeting a freed object, and
    // never observe after we're gone. Guarded so we don't clobber a newer
    // dialog's wiring. (Normally -panelWillClose:/-tearDownNow already did both.)
    NSColorPanel* panel = [NSColorPanel sharedColorPanel];
    if ([panel target] == self)
        [panel setTarget:nil];

    [[NSNotificationCenter defaultCenter] removeObserver:self];

    if (_callback)
    {
        _callback->release();
        _callback = nullptr;
    }
    [super dealloc];
}

@end

#endif // GMPI_MAC_COLORDIALOG_IMPLEMENTATION

// ---------------------------------------------------------------------------
// C++ IColorDialog: immutable (initial colour supplied at construction), seeds
// + installs the ObjC helper, then may be destroyed at any time. GMPI refcount.
// ---------------------------------------------------------------------------
class GMPI_MAC_ColorDialog : public gmpi::api::IColorDialog
{
    NSView* view;                             // parent view (reserved for [view window] / main-thread context)
    const gmpi::drawing::Color initialColor;  // immutable linear-light seed (mirrors the Win const)
    GMPI_MAC_COLORPANEL_TARGET_CLASS* target_ = nil; // retained; released in dtor

public:
    GMPI_MAC_ColorDialog(NSView* pview, gmpi::drawing::Color pInitialColor)
        : view(pview)
        , initialColor(pInitialColor)
    {
    }

    ~GMPI_MAC_ColorDialog()
    {
        // Drop our independent retain on the helper. The helper's own session
        // self-anchor keeps it alive until the panel closes (or a newer dialog
        // tears it down on hand-off).
        if (target_)
        {
            [target_ release];
            target_ = nil;
        }
    }

    gmpi::ReturnCode showAsync(gmpi::api::IUnknown* callback) override
    {
        gmpi::shared_ptr<gmpi::api::IUnknown> unknown;
        unknown = callback;
        auto colorCallback = unknown.as<gmpi::api::IColorDialogCallback>();
        if (!colorCallback)
            return gmpi::ReturnCode::Fail;

        NSColorPanel* panel = [NSColorPanel sharedColorPanel];

        // Match Windows (comdlg32 has no alpha). Turning this on would be a
        // deliberate cross-platform divergence.
        [panel setShowsAlpha:NO];

        // SHARED-SINGLETON HAND-OFF: if a previous colour dialog's helper is
        // still installed on the one shared panel, eagerly tear it down now.
        // It can no longer fire live events once we re-target, and this
        // collapses the helper population to at most one live helper.
        {
            id existing = [panel target];
            if ([existing isKindOfClass:[GMPI_MAC_COLORPANEL_TARGET_CLASS class]])
                [(GMPI_MAC_COLORPANEL_TARGET_CLASS*)existing tearDownNow];
        }

        // Create the helper and install it as the panel's target/action +
        // close observer BEFORE seeding. The helper takes its own addRef on the
        // callback and self-anchors, so it survives independently of this
        // (possibly short-lived) C++ object.
        //
        // ORDER MATTERS: setTarget:/setAction: MUST precede setColor:.
        // -[NSColorPanel setColor:] synthesises a changeColor: to the CURRENT
        // target; installing ourselves first guarantees that seed event lands
        // in THIS dialog's helper (and it is suppressed via begin/endSeeding),
        // never in a stale previous helper -> no spurious write into the old pin.
        target_ = [[GMPI_MAC_COLORPANEL_TARGET_CLASS alloc]
                      initWithCallback:colorCallback.get()
                          initialColor:initialColor];
        [panel setTarget:target_];
        [panel setAction:@selector(changeColor:)];

        // SEED: linear gmpi Color -> 8-bit sRGB (linearPixelToSRGB) -> 0..1
        // sRGB -> sRGB NSColor. Mirrors the Win RGB(linearPixelToSRGB(...))
        // seed exactly. Do NOT feed the raw linear floats into
        // colorWithSRGBRed: — that API expects sRGB-encoded components and
        // would double-apply gamma (washed-out swatch). Wrapped in
        // begin/endSeeding so the resulting programmatic changeColor: does not
        // fire a bogus onComplete(Ok, seed).
        const CGFloat sr = gmpi::drawing::linearPixelToSRGB(initialColor.r) / 255.0;
        const CGFloat sg = gmpi::drawing::linearPixelToSRGB(initialColor.g) / 255.0;
        const CGFloat sb = gmpi::drawing::linearPixelToSRGB(initialColor.b) / 255.0;
        NSColor* seed = [NSColor colorWithSRGBRed:sr green:sg blue:sb alpha:1.0];

        [target_ beginSeeding];
        [panel setColor:seed];
        [target_ endSeeding];

        // orderFront: (not makeKeyAndOrderFront:) so we don't steal key focus
        // from the editor.
        [panel orderFront:nil];

        return gmpi::ReturnCode::Ok;
    }

    gmpi::ReturnCode queryInterface(const gmpi::api::Guid* iid, void** returnInterface) override
    {
        *returnInterface = {};
        GMPI_QUERYINTERFACE(gmpi::api::IColorDialog);
        return gmpi::ReturnCode::NoSupport;
    }
    GMPI_REFCOUNT;
};

#endif // GMPI_MAC_COLORDIALOG_H
