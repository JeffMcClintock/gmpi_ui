#import <AudioUnit/AudioUnit.h>
#import <AudioUnit/AUCocoaUIView.h>
#import <Cocoa/Cocoa.h>
#include "GmpiSdkCommon.h"
#include "GmpiApiEditor.h"
#import "CocoaGfx.h"
#include "DrawingFrameCommon.h"
#include "DrawingFrameMac.h"
#define GMPI_MAC_TEXTEDIT_IMPLEMENTATION
#include "MacTextEdit.h"
#include "MacPopupMenu.h"
#include "MacStockDialog.h"
#include "MacFileDialog.h"
#define GMPI_MAC_KEYLISTENER_IMPLEMENTATION
#include "MacKeyListener.h"
#include "MacEventHelpers.h"
#import "helpers/IController.h"
#include <algorithm>
#include <array>
#include <string>
#include <string_view>


class DrawingFrameCocoa :
    public DrawingFrameCommon,
    public gmpi::api::IDrawingHost,
    public gmpi::api::IInputHost,
    public gmpi::api::IDialogHost

//#ifdef GMPI_HOST_POINTER_SUPPORT
//    public gmpi_gui::IMpGraphicsHost,
//    public GmpiGuiHosting::PlatformTextEntryObserver,
//#endif
{
public:
    int32_t mouseCaptured = 0;

#ifdef GMPI_HOST_POINTER_SUPPORT
    gmpi::shared_ptr<gmpi_gui_api::IMpGraphics3> client;
    GmpiGuiHosting::PlatformTextEntry* currentTextEdit = nullptr;
#endif
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

#if 0
    virtual gmpi::ReturnCode  GetDrawingFactory(GmpiDrawing_API::IMpFactory ** returnFactory) override
    {
        *returnFactory = &drawingFactory;
        return gmpi::ReturnCode::Ok;
    }

    virtual gmpi::ReturnCode  createPlatformMenu(gmpi::drawing::Rect* rect, gmpi_gui::IMpPlatformMenu** returnMenu) override
    {
        *returnMenu = new GmpiGuiHosting::PlatformMenu(view, rect);
        return gmpi::ReturnCode::Ok;
    }
    virtual gmpi::ReturnCode  createPlatformTextEdit(gmpi::drawing::Rect* rect, gmpi_gui::IMpPlatformText** returnTextEdit) override
    {
        currentTextEdit = new GmpiGuiHosting::PlatformTextEntry(this, view, rect);
        *returnTextEdit = currentTextEdit;
        return gmpi::ReturnCode::Ok;
    }
    virtual gmpi::ReturnCode  createFileDialog(int32_t dialogType, gmpi_gui::IMpFileDialog** returnFileDialog) override
    {
        *returnFileDialog = new GmpiGuiHosting::PlatformFileDialog(dialogType, view);
        return gmpi::ReturnCode::Ok;
    }
    virtual gmpi::ReturnCode  createOkCancelDialog(int32_t dialogType, gmpi_gui::IMpOkCancelDialog** returnDialog) override
    {
        *returnDialog = new GmpiGuiHosting::PlatformOkCancelDialog(dialogType, view);
        return gmpi::ReturnCode::Ok;
    }
#endif
    
    // IUnknown methods
    gmpi::ReturnCode queryInterface(const gmpi::api::Guid* iid, void** returnInterface) override
    {
        *returnInterface = {};

        GMPI_QUERYINTERFACE(gmpi::api::IDrawingHost);
        GMPI_QUERYINTERFACE(gmpi::api::IInputHost);

        if(parameterHost)
            return parameterHost->queryInterface(iid, returnInterface);

        return gmpi::ReturnCode::NoSupport;
#if 0
        if (*iid == IDrawingHost::guid)
        {
            *returnInterface = reinterpret_cast<void*>(static_cast<IDrawingHost*>(this));
            addRef();
            return gmpi::ReturnCode::Ok;
        }
        if (*iid == IInputHost::guid)
        {
            // important to cast to correct vtable (ug_plugin3 has 2 vtables) before reinterpret cast
            *returnInterface = reinterpret_cast<void*>(static_cast<IInputHost*>(this));
            addRef();
            return gmpi::ReturnCode::Ok;
        }
        if (*iid == gmpi::api::IUnknown::guid)
        {
            *returnInterface = this;
            addRef();
            return gmpi::ReturnCode::Ok;
        }
#endif
#if 0
        if (iid == gmpi::MP_IID_UI_HOST2)
        {
            // important to cast to correct vtable (ug_plugin3 has 2 vtables) before reinterpret cast
            *returnInterface = reinterpret_cast<void*>(static_cast<IMpUserInterfaceHost2*>(this));
            addRef();
            return gmpi::ReturnCode::Ok;
        }

        if (iid == gmpi_gui::SE_IID_GRAPHICS_HOST || iid == gmpi_gui::SE_IID_GRAPHICS_HOST_BASE || iid == gmpi::MP_IID_UNKNOWN)
        {
            // important to cast to correct vtable (ug_plugin3 has 2 vtables) before reinterpret cast
            *returnInterface = reinterpret_cast<void*>(static_cast<IMpGraphicsHost*>(this));
            addRef();
            return gmpi::ReturnCode::Ok;
        }
#endif
        return gmpi::ReturnCode::NoSupport;
    }

    void removeTextEdit()
    {
#if 0 // TODO!!!
        if(!currentTextEdit)
            return;
        
        currentTextEdit->CallbackFromCocoa(nil);
#endif
    }
    
#if 0 // TODO!!!
    void onTextEditRemoved() override
    {
        currentTextEdit = nullptr;
    }
#endif
    
    void initBackingBitmap()
    {
        NSSize physicalsize = [view convertRectToBacking:[view bounds]].size;

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
    if(deltaY)
    {
        // TODO         drawingFrame.getView()->onMouseWheel(flags, wheelConversion * deltaY, mousePos);
    }
    if(deltaX)
    {
        flags |= static_cast<int32_t>(gmpi::api::PointerFlags::ScrollHoriz);
        // TODO         drawingFrame.getView()->onMouseWheel(flags, wheelConversion * deltaX, mousePos);
        //if(drawingFrame.inputClient)
        //    drawingFrame.inputClient->onMouseWheel(mouseToGmpi(self, theEvent), flags);
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
    auto r = [view frame];
    r.size.width = width;
    r.size.height = height;
    [view setFrame:r];
}
