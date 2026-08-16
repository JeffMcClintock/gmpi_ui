#import <AudioUnit/AudioUnit.h>
#import <AudioUnit/AUCocoaUIView.h>
#import <Cocoa/Cocoa.h>
#include "GmpiSdkCommon.h"
#include "GmpiApiEditor.h"
#import "CocoaGfx.h"
#include "DrawingFrameCommon.h"
// IMPLEMENTATION macros must be defined before the first include of each
// single-header backend, including transitive includes via DrawingFrameMac.h.
#define GMPI_MAC_TEXTEDIT_IMPLEMENTATION
#define GMPI_MAC_KEYLISTENER_IMPLEMENTATION
#define GMPI_MAC_COLORDIALOG_IMPLEMENTATION
#include "DrawingFrameMac.h"
#include "MacTextEdit.h"
#include "MacPopupMenu.h"
#include "MacStockDialog.h"
#include "MacFileDialog.h"
#include "MacColorDialog.h"
#include "MacKeyListener.h"
#include "MacEventHelpers.h"
#import "helpers/IController.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <string_view>


// Bounds on the editor extent. Nothing on this path used to have one -- not onSize,
// not resizeNativeView, not initBackingBitmap -- and hosts do pass nonsense: REAPER
// was seen offering 2178 x 32672 on Windows.
//
// Do not copy the Windows number. 16384 there is the hard Direct3D 11 texture limit,
// so an over-limit extent fails loudly. CoreGraphics has no comparable wall: measured,
// CGBitmapContextCreate accepts 2178 x 32672, 16384 x 16384 and 65536 x 600, with a
// square limit of 131071 and a single axis of 4194303, because it reserves lazily. So
// there is no NULL to fall back on and onRender's `if(!backBuffer) return;` only ever
// fires at 0 x 0. The cost here is memory instead: one measured paint at 16385 x 600
// points cost +253 MiB.
//
// These numbers are therefore a product decision about how much an editor may reserve,
// not a technical ceiling. Reasoning, and the measurements behind them, in TideSynth
// docs/p7-resize-audit-mac-x11.md (BACKLOG P7a).
//
//  - maxEditorDimensionPoints is set from displays, not from any graphics API: the
//    widest single Mac display in logical points is a 6K Pro Display XDR in "more
//    space" at 3840, so 8192 is a bit over twice the largest real case.
//  - maxBackingBitmapBytes is what actually bounds memory. The backing bitmap is 8
//    bytes per pixel (16-bit x 4 components) at *backing* resolution, so a full-screen
//    editor on that same display at 2x reserves ~265 MiB. 384 MiB clears the largest
//    legitimate case with headroom while still biting on every rect the P7 audit
//    exercised.
constexpr double maxEditorDimensionPoints = 8192.0;
constexpr size_t maxBackingBitmapBytes = 384u * 1024u * 1024u;
constexpr size_t backingBitmapBytesPerPixel = 8; // 16-bit x 4 components, per initBackingBitmap
constexpr double assumedBackingScale = 2.0;      // Retina; the pessimistic guess when the real one is unknown

// Reduce an extent so neither axis exceeds maxAxis and the bitmap it implies fits the
// byte budget. Aspect ratio is preserved when the area budget bites, so a clamped
// editor is a smaller version of what was asked for rather than a differently-shaped
// one. Never returns a degenerate extent -- 0 x 0 is the one size CoreGraphics does
// refuse, and onRender copes with it, but there is no reason to hand it one.
static void clampEditorExtent(double& width, double& height, double backingScale, double maxAxis)
{
    width  = std::max(1.0, std::min(width,  maxAxis));
    height = std::max(1.0, std::min(height, maxAxis));

    const double maxPixels = static_cast<double>(maxBackingBitmapBytes) / backingBitmapBytesPerPixel;
    const double pixels = (width * backingScale) * (height * backingScale);

    if (pixels > maxPixels)
    {
        const double shrink = std::sqrt(maxPixels / pixels);
        width  = std::max(1.0, std::floor(width  * shrink));
        height = std::max(1.0, std::floor(height * shrink));
    }
}

// Exported for the VST3 wrapper's checkSizeConstraint, which must answer with a size
// this file will actually honour -- see GMPI_Wrappers wrapper/VST3/SEVSTGUIEditorMac.cpp.
// Forward-declared there rather than shared through a header, matching how
// createNativeView and resizeNativeView already cross that boundary.
//
// Works in logical points against assumedBackingScale: at checkSizeConstraint time the
// view may not be on a screen yet, so the real scale is not knowable and the pessimistic
// guess is the safe one. The result is therefore never larger than what resizeNativeView
// and initBackingBitmap will allow, which is the direction that matters.
void gmpi_clampEditorSize(int* width, int* height)
{
    double w = *width;
    double h = *height;

    clampEditorExtent(w, h, assumedBackingScale, maxEditorDimensionPoints);

    *width  = static_cast<int>(w);
    *height = static_cast<int>(h);
}


class DrawingFrameCocoa :
    public DrawingFrameCommon,
    public gmpi::api::IDrawingHost,
    public gmpi::api::IInputHost,
    public gmpi::api::IDialogHost
{
public:
    int32_t mouseCaptured = 0;

    gmpi::shared_ptr<gmpi::api::IEditor> pluginParameters_GMPI;

    gmpi::cocoa::Factory drawingFactory;
    NSView* view;
    CGContextRef backBuffer{}; // backing buffer with linear colorspace for correct blending.
    CGFloat backBufferHeight{}; // for coordinate flipping
    
    void Init(gmpi::api::IUnknown* paramHost, gmpi::api::IUnknown* pclient)
    {
        parameterHost = paramHost;
        
        pclient->queryInterface(&gmpi::api::IDrawingClient::guid, drawingClient.put_void());
        pclient->queryInterface(&gmpi::api::IInputClient::guid, inputClient.put_void());
        pclient->queryInterface(&gmpi::api::IEditor::guid, pluginParameters_GMPI.put_void());
        
        if(pluginParameters_GMPI)
        {
            pluginParameters_GMPI->setHost(static_cast<gmpi::api::IDrawingHost*>(this));
            pluginParameters_GMPI->initialize();
        }

        if(drawingClient)
            drawingClient->setHost(static_cast<gmpi::api::IDrawingHost*>(this));
    }

    void open() // called from viewDidMoveToWindow <= createNativeView()
    {
        if(drawingClient)
        {
            drawingClient->setHost(static_cast<gmpi::api::IDrawingHost*>(this));
            
            const auto r = [view bounds];

            const gmpi::drawing::Size available{
                static_cast<float>(r.size.width),
                static_cast<float>(r.size.height)
            };
            
            gmpi::drawing::Size desired{};
            drawingClient->measure(&available, &desired);
            gmpi::drawing::Rect finalRect{0,0, available.width, available.height};
            drawingClient->arrange(&finalRect);
        }
    }
    
     void DeInit()
     {
         if(pluginParameters_GMPI)
         {
             auto controller = dynamic_cast<gmpi::hosting::IController*>(parameterHost);
             if(controller)
                 controller->unRegisterGui(pluginParameters_GMPI.get());
         }
         
         drawingClient = {};
         inputClient = {};
         pluginParameters_GMPI = {};
     }

    ~DrawingFrameCocoa()
    {
    }
    // look into: https://blog.rectorsquid.com/getting-gpu-acceleration-with-nsgraphicscontext/
    void onRender(NSView* frame, gmpi::drawing::Rect* dirtyRect)
    {
#if USE_BACKING_BUFFER
        if(!backBuffer)
        {
            initBackingBitmap();

            NSSize logicalsize = view.frame.size;
            gmpi::drawing::Rect finalRect{0,0, (float) logicalsize.width, (float) logicalsize.height};
            if(drawingClient)
                drawingClient->arrange(&finalRect);
        }

        if(!backBuffer)
            return; // bitmap creation failed, nothing to draw

        // draw onto linear back buffer.
        CGContextSaveGState(backBuffer);

        // Flip coordinate system to match Direct2D (top-down).
        CGContextTranslateCTM(backBuffer, 0, backBufferHeight);
        CGContextScaleCTM(backBuffer, 1, -1);

        // Scale from physical to logical coordinates so plugin draws in logical (point) units.
        NSSize logicalsize = view.frame.size;
        NSSize physicalsize = [view convertRectToBacking:[view bounds]].size;
        const CGFloat dpiScale = (logicalsize.width > 0) ? physicalsize.width / logicalsize.width : 1.0;
        if (dpiScale != 1.0)
        {
            CGContextScaleCTM(backBuffer, dpiScale, dpiScale);
        }

        if(-1 == gmpi::cocoa::GraphicsContext::logicProFix)
        {
            gmpi::cocoa::GraphicsContext::logicProFix = 0;

            gmpi::cocoa::GraphicsContext context(frame, &drawingFactory);
            context.setCGContext(backBuffer);

            gmpi::drawing::Graphics g(&context);

            constexpr std::array<std::string_view, 1> fontnames{std::string_view{"Arial"}};
            auto tf = g.getFactory().createTextFormat(16, fontnames, gmpi::drawing::FontWeight::Normal);

            auto brush = g.createSolidColorBrush(gmpi::drawing::Colors::Black);
            g.fillRectangle(0,0,40,40, brush);
            brush.setColor(gmpi::drawing::Colors::White);
            g.drawTextU("_", tf, {0, 0, 40, 40}, brush);

            // Read pixels from the backing buffer to detect Logic Pro text baseline bug.
            // The back buffer may be 16-bit int or 32-bit float per component; we need
            // to read the green channel (component index 1) at the correct stride.
            const size_t bpc = CGBitmapContextGetBitsPerComponent(backBuffer);
            const auto stride = CGBitmapContextGetBytesPerRow(backBuffer);
            uint8_t const* rawPixels = (uint8_t const*)CGBitmapContextGetData(backBuffer);

            int bestBrightness = 0;
            int bestRow = 1;

            for(int y = 0 ; y < 40 ; ++y)
            {
                int brightness = 0;
                const uint8_t* rowBase = rawPixels + y * stride;
                if (bpc == 32) {
                    // 32-bit float: read green channel (component 1)
                    float fval;
                    memcpy(&fval, rowBase + sizeof(float), sizeof(float));
                    brightness = (int)(fval * 255.0f);
                } else if (bpc == 16) {
                    // 16-bit integer: read green channel (component 1)
                    uint16_t ival;
                    memcpy(&ival, rowBase + sizeof(uint16_t), sizeof(uint16_t));
                    brightness = ival >> 8; // scale to 0-255
                } else {
                    // 8-bit: read green channel
                    brightness = rowBase[1];
                }

                if(brightness > bestBrightness)
                {
                    bestBrightness = brightness;
                    bestRow = y;
                }
            }

            gmpi::cocoa::GraphicsContext::logicProFix = (int) (bestRow != 17 && bestRow != 33); // SD / HD (will be 18 / 35 for buggy situation)
        }
#endif
        // context must be disposed before restoring state, because it's destructor also restores state
        {
            gmpi::cocoa::GraphicsContext context(frame, &drawingFactory);
            context.setCGContext(backBuffer);

            // JUCE standalone tends to draw over window non-client area on macOS. clip drawing.
            const auto r = [frame bounds];
            const gmpi::drawing::Rect bounds{
                (float) r.origin.x,
                (float) r.origin.y,
                (float) (r.origin.x + r.size.width),
                (float) (r.origin.y + r.size.height)
            };

            const gmpi::drawing::Rect dirtyClipped = intersectRect(bounds, *dirtyRect);

            context.pushAxisAlignedClip(&dirtyClipped);

           if(drawingClient)
               drawingClient->render(static_cast<gmpi::drawing::api::IDeviceContext*>(&context));

            // render() is re-entrant: a client that resizes its own view from inside it
            // reaches setFrame: -> onResize(), which does CGContextRelease(backBuffer).
            // Everything below this line -- the clip pop, the state restore and the blit
            // -- then operates on a bitmap context that no longer exists. Time of check,
            // re-entrant call, use: the P4 Windows crash, on this path.
            //
            // The guard has to sit HERE rather than after the block closes, because the
            // line that actually faults is popAxisAlignedClip() below: GraphicsContext
            // kept its own copy of the pointer at setCGContext time and nothing nulls
            // that, so it restores state on freed memory. The two statements after the
            // block read the member, which onResize did null, so they merely hand
            // CoreGraphics a NULL.
            //
            // Reproduced under Guard Malloc before this guard existed -- SIGSEGV in
            // CGContextRestoreGState <- popAxisAlignedClip <- onRender. AddressSanitizer
            // does NOT catch it: the read happens inside CoreGraphics, which it does not
            // instrument. Test: gmpi_ui tests/mac_render_reentrant_resize.mm (BACKLOG P7b).
            //
            // Dropping this frame is safe: setFrame: has already invalidated the view, so
            // AppKit repaints, and the bitmap is reallocated lazily at the top of the next
            // onRender at the new size. Same rule this function already follows after
            // arrange() above.
            if(!backBuffer)
                return;

            context.popAxisAlignedClip();
        }

        CGContextRestoreGState(backBuffer);

        // blit back buffer onto screen.
        CGImageRef backImage = CGBitmapContextCreateImage(backBuffer);
        if (backImage)
        {
            CGContextRef screenCtx = [[NSGraphicsContext currentContext] CGContext];
            CGContextDrawImage(screenCtx, [view bounds], backImage);
            CGImageRelease(backImage);
        }
    }
#if 0
    // Inherited via IMpUserInterfaceHost2
    virtual gmpi::ReturnCode  pinTransmit(int32_t pinId, int32_t size, const void * data, int32_t voice = 0) override
    {
//TODO         assert(false); // not implemented.
        return gmpi::MP_FAIL;
    }
    virtual gmpi::ReturnCode  createPinIterator(gmpi::IMpPinIterator** returnIterator) override
    {
//TODO         assert(false); // not implemented.
        return gmpi::MP_FAIL;
    }
    virtual gmpi::ReturnCode  getHandle(int32_t & returnValue) override
    {
 //TODO        assert(false); // not implemented.
        return gmpi::MP_FAIL;
    }
    virtual gmpi::ReturnCode  sendMessageToAudio(int32_t id, int32_t size, const void * messageData) override
    {
 //TODO        assert(false); // not implemented.
        return gmpi::MP_FAIL;
    }
    virtual gmpi::ReturnCode  ClearResourceUris() override
    {
//TODO         assert(false); // not implemented.
        return gmpi::MP_FAIL;
    }
    virtual gmpi::ReturnCode  RegisterResourceUri(const char * resourceName, const char * resourceType, gmpi::IString* returnString) override
    {
//TODO         assert(false); // not implemented.
        return gmpi::MP_FAIL;
    }
    virtual gmpi::ReturnCode  OpenUri(const char * fullUri, gmpi::IProtectedFile2** returnStream) override
    {
//TODO         assert(false); // not implemented.
        return gmpi::MP_FAIL;
    }
    virtual gmpi::ReturnCode  FindResourceU(const char * resourceName, const char * resourceType, gmpi::IString* returnString) override
    {
//TODO         assert(false); // not implemented.
        return gmpi::MP_FAIL;
    }
    
    virtual gmpi::ReturnCode  LoadPresetFile_DEPRECATED(const char* presetFilePath) override
    {
        //TODO         assert(false); // not implemented.
        return gmpi::MP_FAIL;
    }
#endif
    
    // IMpGraphicsHost
    void invalidateRect(const gmpi::drawing::Rect* rect) override
    {
        if(rect)
        {
#if 0
            [view setNeedsDisplayInRect:
            NSMakeRect(            // flip co-ords
               rect->left,
               rect->top,
               rect->right - rect->left,
               rect->bottom - rect->top
               )
            ];
#else
            [view setNeedsDisplayInRect:
            NSMakeRect(            // flip co-ords
               rect->left,
               view.bounds.origin.y + view.bounds.size.height - rect->bottom,
               rect->right - rect->left,
               rect->bottom - rect->top
               )
            ];
#endif
        }
        else
        {
            [view setNeedsDisplay:YES];
        }
    }
    void invalidateMeasure() override {}

#if 0
    virtual void  invalidateMeasure() override
    {
//TODO        assert(false); // not implemented.
    }
#endif
    gmpi::ReturnCode setCapture(void) override
    {
        mouseCaptured = 1;
        return gmpi::ReturnCode::Ok;
    }
    gmpi::ReturnCode getCapture(bool & returnValue) override
    {
        returnValue = mouseCaptured;
        return gmpi::ReturnCode::Ok;
    }
    gmpi::ReturnCode releaseCapture() override
    {
        mouseCaptured = 0;
        return gmpi::ReturnCode::Ok;
    }

    gmpi::ReturnCode getDrawingFactory(gmpi::api::IUnknown ** returnFactory) override
    {
        *returnFactory = &drawingFactory;
        return gmpi::ReturnCode::Ok;
    }
    
    float getRasterizationScale() override
    {
        return [[view window] backingScaleFactor];
    }

    // IDialogHost
    gmpi::ReturnCode createTextEdit(const gmpi::drawing::Rect* r, gmpi::api::IUnknown** returnTextEdit) override
    {
        auto textEdit = new GMPI_MAC_TextEdit(view, *r);
        textEdit->addRef();
        *returnTextEdit = textEdit;
        return gmpi::ReturnCode::Ok;
    }
    gmpi::ReturnCode createPopupMenu(const gmpi::drawing::Rect* r, gmpi::api::IUnknown** returnMenu) override
    {
        contextMenu.attach(new GMPI_MAC_PopupMenu(view, *r));
        contextMenu->addRef();
        *returnMenu = contextMenu.get();
        return gmpi::ReturnCode::Ok;
    }
    gmpi::ReturnCode createKeyListener(const gmpi::drawing::Rect* r, gmpi::api::IUnknown** returnKeyListener) override
    {
        *returnKeyListener = new GMPI_MAC_KeyListener(view, r);
        return gmpi::ReturnCode::Ok;
    }
    gmpi::ReturnCode createFileDialog(int32_t dialogType, gmpi::api::IUnknown** returnMenu) override
    {
        *returnMenu = new GMPI_MAC_FileDialog(view, static_cast<gmpi::api::FileDialogType>(dialogType));
        return gmpi::ReturnCode::Ok;
    }
    gmpi::ReturnCode createStockDialog(int32_t dialogType, const char* title, const char* text, gmpi::api::IUnknown** returnDialog) override
    {
        *returnDialog = new GMPI_MAC_StockDialog(view, static_cast<gmpi::api::StockDialogType>(dialogType), title, text);
        return gmpi::ReturnCode::Ok;
    }
    gmpi::ReturnCode createColorDialog(gmpi::drawing::Color initialColor, gmpi::api::IUnknown** returnDialog) override
    {
        // NSColorPanel-backed colour picker. Deferred-notify model (fires
        // onComplete once, on dismissal), matching the modal Windows
        // ChooseColorW path — see MacColorDialog.h for the cross-platform note.
        // No extra addRef: GMPI_REFCOUNT starts refCount2_ at 1, and (unlike
        // createTextEdit/createPopupMenu, which also keep a member) the host
        // retains no second copy — matches createFileDialog/createStockDialog.
        *returnDialog = new GMPI_MAC_ColorDialog(view, initialColor);
        return gmpi::ReturnCode::Ok;
    }

    // Legacy SDK3 dialog overrides (gmpi_gui::IMpPlatform*) used to live here under
    // #ifdef GMPI_HOST_POINTER_SUPPORT — never defined anywhere in the gmpi_ui
    // build. Dead code referencing SDK3 types removed; modern dialog hosts live
    // on DrawingFrameCommon / Mac*Dialog single-headers.
    
    // IUnknown methods
    gmpi::ReturnCode queryInterface(const gmpi::api::Guid* iid, void** returnInterface) override
    {
        *returnInterface = {};

        GMPI_QUERYINTERFACE(gmpi::api::IDrawingHost);
        GMPI_QUERYINTERFACE(gmpi::api::IInputHost);
        GMPI_QUERYINTERFACE(gmpi::api::IDialogHost);

        if(parameterHost)
            return parameterHost->queryInterface(iid, returnInterface);

        return gmpi::ReturnCode::NoSupport;
    }

    // Stub kept for the click-to-dismiss call sites in mouseDown/rightMouseDown.
    // The legacy GmpiGuiHosting::PlatformTextEntry path it used to drive was
    // gated by GMPI_HOST_POINTER_SUPPORT (never defined) and has been removed.
    // GMPI_MAC_TextEdit dismisses itself when the user clicks elsewhere via the
    // NSTextField endEditing notification, so this stub is currently a no-op.
    void removeTextEdit() {}
    
    void initBackingBitmap()
    {
        NSSize physicalsize = [view convertRectToBacking:[view bounds]].size;

        // This line is where the memory is actually reserved, so bound it here as well
        // as in resizeNativeView. Not redundant: a host can set the view's frame
        // directly without going through the wrapper (JUCE does), and the backing scale
        // then multiplies whatever it set.
        //
        // Clamping in backing pixels, so the axis limit is scaled to match. If it bites,
        // the bitmap ends up smaller than the view and the blit at the end of onRender
        // stretches it -- a blurry editor at an absurd extent, which is the intended
        // trade against reserving gigabytes.
        {
            const double scale = ([view bounds].size.width > 0)
                ? physicalsize.width / [view bounds].size.width
                : assumedBackingScale;
            double pw = physicalsize.width;
            double ph = physicalsize.height;

            clampEditorExtent(pw, ph, 1.0, maxEditorDimensionPoints * std::max(1.0, scale));

            physicalsize.width  = pw;
            physicalsize.height = ph;
        }

        CGColorSpaceRef colorSpace = CGColorSpaceCreateWithName(kCGColorSpaceLinearSRGB);

        // CGBitmapContextCreate doesn't support 16-bit float.
        // Use 16-bit integer per component in linear space for correct blending with good precision,
        // or fall back to 32-bit float if that fails.
        backBuffer = CGBitmapContextCreate(NULL,
            (size_t)physicalsize.width, (size_t)physicalsize.height,
            16, 0, colorSpace,
            (CGBitmapInfo)kCGImageAlphaPremultipliedLast | (CGBitmapInfo)kCGBitmapByteOrder16Big);

        if (!backBuffer)
        {
            // Fallback to 32-bit float (always supported)
            backBuffer = CGBitmapContextCreate(NULL,
                (size_t)physicalsize.width, (size_t)physicalsize.height,
                32, 0, colorSpace,
                (CGBitmapInfo)kCGImageAlphaPremultipliedLast | (CGBitmapInfo)kCGBitmapFloatComponents | (CGBitmapInfo)kCGBitmapByteOrder32Host);
        }

        backBufferHeight = physicalsize.height;

        CGColorSpaceRelease(colorSpace);
    }
    
    void onResize()
    {
        if(backBuffer)
            CGContextRelease(backBuffer);
        backBuffer = nullptr;
     }
    
    GMPI_REFCOUNT_NO_DELETE;
};

// Objective-C can't handle loading the same class into different plugins, give each iteration of this class a unique name
#define GMPI_KEY_LISTENER_CLASS GMPI_KEY_LISTENER_VERSION_03

//--------------------------------------------------------------------------------------------------------------
@interface GMPI_KEY_LISTENER_CLASS : NSView {
}

- (id) initWithCallback: (int) x preferredSize: (NSSize) size;

@end

@implementation GMPI_KEY_LISTENER_CLASS

- (id) initWithCallback: (int) x preferredSize: (NSSize) size
{
    self = [super initWithFrame: NSMakeRect (0, 0, size.width, size.height)];
    if (self)
    {
    }
    return self;
}
@end // implementation: GMPI_KEY_LISTENER_CLASS

// without including objective-C headers, we need to create an key-listener NSView from C++.
// here is the function here to return the view, using void* as return type.
void* gmpi_ui_create_key_listener(void* parent, int width, int height)
{
    NSSize inPreferredSize{(CGFloat)width, (CGFloat)height};
    
    NSView* native = [[GMPI_KEY_LISTENER_CLASS alloc] initWithCallback:0 preferredSize:inPreferredSize];
    
    if(parent) // JUCE creates the view then *later* adds it to the parent. GMPI adds it here.
    {
        NSView* parentView = (NSView*) parent;
        [parentView addSubview:native];
    }
    
    return (void*) native;
}

// Objective-C can't handle loading the same class into different plugins, give each iteration of this class a unique name
#define GMPI_VIEW_CLASS GMPI_VIEW_VERSION_03

//--------------------------------------------------------------------------------------------------------------
@interface GMPI_VIEW_CLASS : NSView {
    //--------------------------------------------------------------------------------------------------------------
    DrawingFrameCocoa drawingFrame;
    NSTrackingArea* trackingArea;
    NSTimer* timer;
    int toolTipTimer;
    bool toolTipShown;
    gmpi::drawing::Point mousePos;
}

- (id) initWithClient: (class IUnknown*) _client parameterHost: (class IUnknown*) paramHost preferredSize: (NSSize) size;
- (void)drawRect:(NSRect)dirtyRect;
- (void)onTimer: (NSTimer*) t;
//- (NSView*) uiViewForAudioUnit:(AudioUnit)inAU withSize:(NSSize)inPreferredSize;

@end


//--------------------------------------------------------------------------------------------------------------
@implementation GMPI_VIEW_CLASS

- (id) initWithClient: (class IUnknown*) _client parameterHost: (class IUnknown*) paramHost preferredSize: (NSSize) size
{
    self = [super initWithFrame: NSMakeRect (0, 0, size.width, size.height)];
    if (self)
    {
        self->trackingArea = [NSTrackingArea alloc];
        [self->trackingArea initWithRect:NSZeroRect options:(NSTrackingMouseEnteredAndExited | NSTrackingInVisibleRect | NSTrackingMouseMoved| NSTrackingActiveAlways) owner:self userInfo:nil];
        [self addTrackingArea:self->trackingArea ];
        
        drawingFrame.view = self; // pass to init might be clearer
        drawingFrame.Init((gmpi::api::IUnknown*) paramHost, (gmpi::api::IUnknown*) _client);
        
        timer = [NSTimer scheduledTimerWithTimeInterval:0.1 target:self selector:@selector(onTimer:) userInfo:nil repeats:YES ];
    }
    return self;
}

// View shown for first time.
- (void)viewDidMoveToWindow {
     [super viewDidMoveToWindow];

    auto window = [self window];
    if(window)
    {
//        drawingFrame.drawingFactory.setBestColorSpace(window);
        drawingFrame.open();
    }
}

- (void) removeFromSuperview
{
    [super removeFromSuperview];

    // Editor is closing
    [self onClose];
}

-(void)onClose
{
    if( trackingArea )
    {
 //       _RPT0(0, "onClose. Removing trackingArea\n");
        [trackingArea release];
        trackingArea = nil;
    }
    
    // timer will retain NSView, so need to manually stop timer right before we release this view
    if( timer )
    {
        [self->timer invalidate];
        self->timer = nil;
    }
    
    drawingFrame.DeInit();
    drawingFrame.view = nil;
}

- (void)drawRect:(NSRect)dirtyRect
{
#if 0
    gmpi::drawing::Rect r{
        static_cast<float>(dirtyRect.origin.x),
        static_cast<float>(dirtyRect.origin.y),
        static_cast<float>(dirtyRect.origin.x + dirtyRect.size.width),
        static_cast<float>(dirtyRect.origin.y + dirtyRect.size.height)
    };
#else
    const auto bounds = [self bounds];
    gmpi::drawing::Rect r{
        static_cast<float>(dirtyRect.origin.x),
        static_cast<float>(bounds.origin.y + bounds.size.height - dirtyRect.origin.y - dirtyRect.size.height),
        static_cast<float>(dirtyRect.origin.x + dirtyRect.size.width),
        static_cast<float>(bounds.origin.y + bounds.size.height - dirtyRect.origin.y)
    };
#endif
    drawingFrame.onRender(self, &r);
 }

//--------------------------------------------------------------------------------------------------------------
- (void) setFrame: (NSRect) newSize
{
    [super setFrame: newSize];
    
    drawingFrame.onResize();
}

//--------------------------------------------------------------------------------------------------------------
#if !USE_BACKING_BUFFER
- (BOOL)isFlipped { return YES; }
#endif

- (BOOL)acceptsFirstMouse:(NSEvent *)event { return YES; }

- (void)mouseDown:(NSEvent *)theEvent
{
    drawingFrame.removeTextEdit();

    [[self window] makeFirstResponder:self]; // take focus off any text-edit. Works but does not dimiss it.

    int32_t flags = static_cast<int32_t>(gmpi::api::PointerFlags::InContact) | static_cast<int32_t>(gmpi::api::PointerFlags::Primary) | static_cast<int32_t>(gmpi::api::PointerFlags::Confidence);
    flags |= static_cast<int32_t>(gmpi::api::PointerFlags::New);
    flags |= static_cast<int32_t>(gmpi::api::PointerFlags::FirstButton);

    // NSEvent already coalesces fast clicks into clickCount, applying the
    // user's preferred timing/movement thresholds — clickCount == 2 means
    // this is the second click of a double-click pair.
    if (theEvent.clickCount == 2)
        flags |= static_cast<int32_t>(gmpi::api::PointerFlags::Double);

    applyKeyModifiers(flags, theEvent);
    const auto p = mouseToGmpi(self, theEvent);

    if(drawingFrame.inputClient)
        drawingFrame.inputClient->onPointerDown(p, flags);

 // no help to edit box   [super mouseDown:theEvent];
}

- (void)rightMouseDown:(NSEvent *)theEvent
{
    drawingFrame.removeTextEdit();

    int32_t flags = static_cast<int32_t>(gmpi::api::PointerFlags::InContact) | static_cast<int32_t>(gmpi::api::PointerFlags::Primary) | static_cast<int32_t>(gmpi::api::PointerFlags::Confidence);
    flags |= static_cast<int32_t>(gmpi::api::PointerFlags::New);
    flags |= static_cast<int32_t>(gmpi::api::PointerFlags::SecondButton);
    
    applyKeyModifiers(flags, theEvent);
    const auto p = mouseToGmpi(self, theEvent);

    gmpi::ReturnCode r = gmpi::ReturnCode::Unhandled;

    if(drawingFrame.inputClient)
        r = drawingFrame.inputClient->onPointerDown(p, flags);

    if (r == gmpi::ReturnCode::Unhandled)
    {
        drawingFrame.doContextMenu(p, flags);
    }
}

- (void)rightMouseUp:(NSEvent *)theEvent
{
    int32_t flags = static_cast<int32_t>(gmpi::api::PointerFlags::InContact) | static_cast<int32_t>(gmpi::api::PointerFlags::Primary) | static_cast<int32_t>(gmpi::api::PointerFlags::Confidence);
    flags |= static_cast<int32_t>(gmpi::api::PointerFlags::New);
    flags |= static_cast<int32_t>(gmpi::api::PointerFlags::SecondButton);
    
    applyKeyModifiers(flags, theEvent);
    
    if(drawingFrame.inputClient)
        drawingFrame.inputClient->onPointerUp(mouseToGmpi(self, theEvent), flags);
}

- (void)mouseUp:(NSEvent *)theEvent {
    int32_t flags = static_cast<int32_t>(gmpi::api::PointerFlags::InContact) | static_cast<int32_t>(gmpi::api::PointerFlags::Primary) | static_cast<int32_t>(gmpi::api::PointerFlags::Confidence);
    flags |= static_cast<int32_t>(gmpi::api::PointerFlags::FirstButton);
    
    applyKeyModifiers(flags, theEvent);
    
    if(drawingFrame.inputClient)
        drawingFrame.inputClient->onPointerUp(mouseToGmpi(self, theEvent), flags);
}

- (void)mouseDragged:(NSEvent *)theEvent {

    int32_t flags = static_cast<int32_t>(gmpi::api::PointerFlags::InContact) | static_cast<int32_t>(gmpi::api::PointerFlags::Primary) | static_cast<int32_t>(gmpi::api::PointerFlags::Confidence);
    flags |= static_cast<int32_t>(gmpi::api::PointerFlags::FirstButton);
    
    applyKeyModifiers(flags, theEvent);
    
    if(drawingFrame.inputClient)
        drawingFrame.inputClient->onPointerMove(mouseToGmpi(self, theEvent), flags);
}

- (void)scrollWheel:(NSEvent *)theEvent {
    // Get the scroll wheel delta
    auto deltaX = theEvent.deltaX;
    auto deltaY = theEvent.deltaY;
 
    int32_t flags = static_cast<int32_t>(gmpi::api::PointerFlags::Primary) | static_cast<int32_t>(gmpi::api::PointerFlags::Confidence);
    applyKeyModifiers(flags, theEvent);

    constexpr float wheelConversion = 120.0f; // on windows the wheel scrolls 120 per knotch
    const auto mousePos = mouseToGmpi(self, theEvent);
    if(deltaY && drawingFrame.inputClient)
    {
        drawingFrame.inputClient->onMouseWheel(mousePos, flags, static_cast<int32_t>(wheelConversion * deltaY));
    }
    if(deltaX && drawingFrame.inputClient)
    {
        flags |= static_cast<int32_t>(gmpi::api::PointerFlags::ScrollHoriz);
        drawingFrame.inputClient->onMouseWheel(mousePos, flags, static_cast<int32_t>(wheelConversion * deltaX));
    }
}

- (BOOL)hasActiveFieldEditor
{
    NSResponder* fr = [[self window] firstResponder];
    return [fr isKindOfClass:[NSTextView class]] && [(NSTextView*)fr isFieldEditor];
}

- (void)mouseEntered:(NSEvent *)theEvent {
    [[self window] setAcceptsMouseMovedEvents:YES];

    // Don't steal first responder from an active text-edit field editor
    if (![self hasActiveFieldEditor])
        [[self window] makeFirstResponder:self];
}

- (void)mouseMoved:(NSEvent *)theEvent {
   
    [self ToolTipOnMouseActivity];
    
    int32_t flags = static_cast<int32_t>(gmpi::api::PointerFlags::InContact) | static_cast<int32_t>(gmpi::api::PointerFlags::Primary) | static_cast<int32_t>(gmpi::api::PointerFlags::Confidence);
    flags |= static_cast<int32_t>(gmpi::api::PointerFlags::FirstButton);
    
    applyKeyModifiers(flags, theEvent);
    
    if(drawingFrame.inputClient)
        drawingFrame.inputClient->onPointerMove(mouseToGmpi(self, theEvent), flags);
}

- (void)mouseExited:(NSEvent *)theEvent {
    [[self window] setAcceptsMouseMovedEvents:NO];
}

- (void) onTimer: (NSTimer*) t {
    if(toolTipTimer-- == 0 && !toolTipShown)
    {
        gmpi::ReturnString text;
        // TODO         drawingFrame.getView()->getToolTip(mousePos, &text);
        
        if(text.str().empty())
        {
            [self setToolTip:nil];
        }
        else
        {
            NSString* nsstr = [NSString stringWithCString : text.c_str() encoding : NSUTF8StringEncoding];
            [self setToolTip:nsstr];
        }
        toolTipShown = true;
    }
}

- (void)ToolTipOnMouseActivity {
    if(toolTipShown)
    {
        [self setToolTip:nil];
        toolTipShown = false;
        toolTipTimer = 2;
    }
}

@end

// without including objective-C headers, we need to create an NSView from C++.
// here is the function here to return the view, using void* as return type.
void* createNativeView(void* parent, class IUnknown* paramHost, class IUnknown* client, int width, int height)
{
    NSSize inPreferredSize{(CGFloat)width, (CGFloat)height};
    
    NSView* native = [[GMPI_VIEW_CLASS alloc] initWithClient:client parameterHost:paramHost preferredSize:inPreferredSize];
    
    if(parent) // JUCE creates the view then *later* adds it to the parent. GMPI adds it here
    {
        NSView* parentView = (NSView*) parent;
        [parentView addSubview:native];
    }
    
    return (void*) native;
}

void gmpi_onCloseNativeView(void* ptr)
{
    auto view = (GMPI_VIEW_CLASS*) ptr;
    [view onClose];
}

void resizeNativeView(void* ptr, int width, int height)
{
    auto view = /*(GMPI_VIEW_CLASS*)*/ (NSView*) ptr;

    // Refuse to adopt an extent that would reserve more than the budget. This used to
    // forward the host's numbers verbatim, which is how 2178 x 32672 reaches a backing
    // bitmap. Here the real backing scale is usually knowable; fall back to the
    // pessimistic guess when the view is not on a screen yet.
    double w = width;
    double h = height;
    const double scale = ([view window] != nil) ? (double)[[view window] backingScaleFactor] : assumedBackingScale;

    clampEditorExtent(w, h, std::max(1.0, scale), maxEditorDimensionPoints);

    auto r = [view frame];
    r.size.width = w;
    r.size.height = h;
    [view setFrame:r];
}
