#import <Cocoa/Cocoa.h>
#include "helpers/GraphicsRedrawClient.h"
#include "GmpiSdkCommon.h"
#import "CocoaGfx.h"
// hmm part of GMPI plugins not GMPI Drawing. how to keep no dependancy?
#include "GmpiApiEditor.h"

namespace gmpi
{
namespace interaction
{
    // TODO niceify, centralize
    enum GG_POINTER_FLAGS {
        GG_POINTER_FLAG_NONE = 0,
        GG_POINTER_FLAG_NEW = 0x01,                    // Indicates the arrival of a new pointer.
        GG_POINTER_FLAG_INCONTACT = 0x04,
        GG_POINTER_FLAG_FIRSTBUTTON = 0x10,
        GG_POINTER_FLAG_SECONDBUTTON = 0x20,
        GG_POINTER_FLAG_THIRDBUTTON = 0x40,
        GG_POINTER_FLAG_FOURTHBUTTON = 0x80,
        GG_POINTER_FLAG_CONFIDENCE = 0x00000400,    // Confidence is a suggestion from the source device about whether the pointer represents an intended or accidental interaction.
        GG_POINTER_FLAG_PRIMARY = 0x00002000,    // First pointer to contact surface. Mouse is usually Primary.

        GG_POINTER_SCROLL_HORIZ = 0x00008000,    // Mouse Wheel is scrolling horizontal.

        GG_POINTER_KEY_SHIFT = 0x00010000,    // Modifer key - <SHIFT>.
        GG_POINTER_KEY_CONTROL = 0x00020000,    // Modifer key - <CTRL> or <Command>.
        GG_POINTER_KEY_ALT = 0x00040000,    // Modifer key - <ALT> or <Option>.
    };
}
}

class DrawingFrameCocoa :
#ifdef GMPI_HOST_POINTER_SUPPORT
public gmpi_gui::IMpGraphicsHost,
public GmpiGuiHosting::PlatformTextEntryObserver,
#endif
public gmpi::api::IDrawingHost,
public gmpi::api::IInputHost
{
public:
    gmpi::shared_ptr<gmpi::api::IDrawingClient> drawingClient;
    gmpi::shared_ptr<gmpi::api::IInputClient> inputClient;
    gmpi::api::IUnknown* parameterHost{};
    
    int32_t mouseCaptured = 0;

#ifdef GMPI_HOST_POINTER_SUPPORT
    gmpi::shared_ptr<gmpi_gui_api::IMpGraphics3> client;
    GmpiGuiHosting::PlatformTextEntry* currentTextEdit = nullptr;
#endif
    
    gmpi::cocoa::DrawingFactory drawingFactory;
    NSView* view;
    NSBitmapImageRep* backBuffer{}; // backing buffer with linear colorspace for correct blending.
    
    void Init(gmpi::api::IUnknown* paramHost, gmpi::api::IUnknown* pclient)
    {
        parameterHost = paramHost;
        
        pclient->queryInterface(&gmpi::api::IDrawingClient::guid, drawingClient.put_void());
        pclient->queryInterface(&gmpi::api::IInputClient::guid, inputClient.put_void());
        gmpi::shared_ptr<gmpi::api::IEditor> editor;
        pclient->queryInterface(&gmpi::api::IEditor::guid, editor.put_void());

        if(editor)
            editor->setHost(static_cast<gmpi::api::IDrawingHost*>(this));
    }
    
    void open()
    {
        if(drawingClient)
        {
            drawingClient->open(static_cast<gmpi::api::IDrawingHost*>(this));
            
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
         drawingClient = {};
         inputClient = {};
     }

    ~DrawingFrameCocoa()
    {
    }
    // look into: https://blog.rectorsquid.com/getting-gpu-acceleration-with-nsgraphicscontext/
    void onRender(NSView* frame, gmpi::drawing::Rect* dirtyRect)
    {
#if USE_BACKING_BUFFER
        if(!backBuffer)
            initBackingBitmap();
        
        // draw onto linear back buffer.
        [NSGraphicsContext saveGraphicsState];
        [backBuffer retain];
        NSGraphicsContext *g = [NSGraphicsContext graphicsContextWithBitmapImageRep:backBuffer];
        [NSGraphicsContext setCurrentContext:g];

        auto flipper = [NSAffineTransform transform];
        [flipper scaleXBy:1 yBy:-1];
        [flipper translateXBy:0.0 yBy:-[frame bounds].size.height];
        [flipper concat];
#endif
        // context must be disposed before restoring state, because it's destructor also restores state
        {
            gmpi::cocoa::GraphicsContext context(frame, &drawingFactory);
            
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
        
        [NSGraphicsContext restoreGraphicsState];
        
        // blit back buffer onto screen.
        [backBuffer drawInRect:[view bounds]]; // copes with DPI
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
    void invalidateRect(const gmpi::drawing::Rect* invalidRect) override
    {
        if(invalidRect)
        {
            [view setNeedsDisplayInRect:NSMakeRect (invalidRect->left, invalidRect->top, invalidRect->right - invalidRect->left, invalidRect->bottom - invalidRect->top)];
        }
        else
        {
            [view setNeedsDisplay:YES];
        }
    }
    
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
    
    gmpi::ReturnCode getFocus() override
    {
        return gmpi::ReturnCode::NoSupport;
    }
    gmpi::ReturnCode releaseFocus() override
    {
        return gmpi::ReturnCode::NoSupport;
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
        NSSize logicalsize = view.frame.size;
        NSSize pysicalsize = [view convertRectToBacking:[view bounds]].size;

        // kCGColorSpaceGenericRGBLinear - middle gray is darker, blend seems correct.
        // kCGColorSpaceExtendedLinearSRGB, kCGColorSpaceLinearDisplayP3 - same
        // kCGColorSpaceGenericRGB - actually sRGB
        // kCGColorSpaceExtendedLinearSRGB caused image to turn dark after intial draw, like OS was compressing non-extended colors

        CGColorSpaceRef colorSpace = CGColorSpaceCreateWithName(kCGColorSpaceLinearSRGB);        
        NSColorSpace *linearRGBColorSpace = [[NSColorSpace alloc] initWithCGColorSpace:colorSpace];

        NSBitmapImageRep* imagerep1 = [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:NULL
           pixelsWide:pysicalsize.width
           pixelsHigh:pysicalsize.height
           bitsPerSample:16 //8     // 1, 2, 4, 8, 12, or 16.
           samplesPerPixel:3
           hasAlpha:NO
           isPlanar:NO
           colorSpaceName: NSCalibratedRGBColorSpace // makes no difference if we retag it later anyhow.
           bitmapFormat: NSBitmapFormatFloatingPointSamples //NSAlphaFirstBitmapFormat
           bytesPerRow:0    // 0 = don't care  800 * 4
           bitsPerPixel:64 ];
        
        backBuffer = [imagerep1 bitmapImageRepByRetaggingWithColorSpace:linearRGBColorSpace];
        [backBuffer setSize: logicalsize]; // Communicates DPI

        // Release the resources
        CGColorSpaceRelease(colorSpace);
    }
    
    GMPI_REFCOUNT_NO_DELETE;
};

gmpi::drawing::Point mouseToGmpi(NSView* view, NSEvent* theEvent)
{
    NSPoint localPoint = [view convertPoint: [theEvent locationInWindow] fromView: nil];
    
#if USE_BACKING_BUFFER
    localPoint.y = view.bounds.origin.y + view.bounds.size.height - localPoint.y;
#endif
    
    gmpi::drawing::Point p{(float)localPoint.x, (float)localPoint.y};
    return p;
}

// Objective-C can't handle loading the same class into different plugins, give each iteration of this class a unique name
#define GMPI_VIEW_CLASS GMPI_VIEW_VERSION_01

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
        drawingFrame.drawingFactory.setBestColorSpace(window);
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
    gmpi::drawing::Rect r{
        static_cast<float>(dirtyRect.origin.x),
        static_cast<float>(dirtyRect.origin.y),
        static_cast<float>(dirtyRect.origin.x + dirtyRect.size.width),
        static_cast<float>(dirtyRect.origin.y + dirtyRect.size.height)
    };
    
    drawingFrame.onRender(self, &r);
 }

//--------------------------------------------------------------------------------------------------------------
- (void) setFrame: (NSRect) newSize
{
    [super setFrame: newSize];
}

//--------------------------------------------------------------------------------------------------------------
#if !USE_BACKING_BUFFER
- (BOOL)isFlipped { return YES; }
#endif

- (BOOL)acceptsFirstMouse:(NSEvent *)event { return YES; }

//-------------------------------------------------------------------------------------------------------------
#if 1
void ApplyKeyModifiers(int32_t& flags, NSEvent* theEvent)
{
    // <Shift> key?
    if(([theEvent modifierFlags ] & (NSEventModifierFlagShift | NSEventModifierFlagCapsLock)) != 0)
    {
        flags |= gmpi::interaction::GG_POINTER_KEY_SHIFT;
    }
    
    if(([theEvent modifierFlags ] & NSEventModifierFlagControl /* was NSEventModifierFlagCommand*/ ) != 0)
    {
        flags |= gmpi::interaction::GG_POINTER_KEY_CONTROL;
    }
    
    if(([theEvent modifierFlags ] & NSEventModifierFlagOption) != 0) // <Option> key.
    {
        flags |= gmpi::interaction::GG_POINTER_KEY_ALT;
    }
}


- (void)mouseDown:(NSEvent *)theEvent
{
    drawingFrame.removeTextEdit();
    
    [[self window] makeFirstResponder:self]; // take focus off any text-edit. Works but does not dimiss it.
 

    int32_t flags = gmpi::interaction::GG_POINTER_FLAG_INCONTACT | gmpi::interaction::GG_POINTER_FLAG_PRIMARY | gmpi::interaction::GG_POINTER_FLAG_CONFIDENCE;
    flags |= gmpi::interaction::GG_POINTER_FLAG_NEW;
	flags |= gmpi::interaction::GG_POINTER_FLAG_FIRSTBUTTON;
    
    ApplyKeyModifiers(flags, theEvent);
    
    if(drawingFrame.inputClient)
        drawingFrame.inputClient->onPointerDown(mouseToGmpi(self, theEvent), flags);

 // no help to edit box   [super mouseDown:theEvent];
}

- (void)rightMouseDown:(NSEvent *)theEvent
{
    drawingFrame.removeTextEdit();

    int32_t flags = gmpi::interaction::GG_POINTER_FLAG_INCONTACT | gmpi::interaction::GG_POINTER_FLAG_PRIMARY | gmpi::interaction::GG_POINTER_FLAG_CONFIDENCE;
    flags |= gmpi::interaction::GG_POINTER_FLAG_NEW;
    flags |= gmpi::interaction::GG_POINTER_FLAG_SECONDBUTTON;
    
    ApplyKeyModifiers(flags, theEvent);
    
    // TODO     drawingFrame.getView()->onPointerDown(flags, p);
    if(drawingFrame.inputClient)
        drawingFrame.inputClient->onPointerDown(mouseToGmpi(self, theEvent), flags);
}

- (void)rightMouseUp:(NSEvent *)theEvent
{
    int32_t flags = gmpi::interaction::GG_POINTER_FLAG_INCONTACT | gmpi::interaction::GG_POINTER_FLAG_PRIMARY | gmpi::interaction::GG_POINTER_FLAG_CONFIDENCE;
    flags |= gmpi::interaction::GG_POINTER_FLAG_NEW;
    flags |= gmpi::interaction::GG_POINTER_FLAG_SECONDBUTTON;
    
    ApplyKeyModifiers(flags, theEvent);
    
    if(drawingFrame.inputClient)
        drawingFrame.inputClient->onPointerUp(mouseToGmpi(self, theEvent), flags);
}

- (void)mouseUp:(NSEvent *)theEvent {
    int32_t flags = gmpi::interaction::GG_POINTER_FLAG_INCONTACT | gmpi::interaction::GG_POINTER_FLAG_PRIMARY | gmpi::interaction::GG_POINTER_FLAG_CONFIDENCE;
    flags |= gmpi::interaction::GG_POINTER_FLAG_FIRSTBUTTON;
    
    ApplyKeyModifiers(flags, theEvent);
    
    if(drawingFrame.inputClient)
        drawingFrame.inputClient->onPointerUp(mouseToGmpi(self, theEvent), flags);
}

- (void)mouseDragged:(NSEvent *)theEvent {

    int32_t flags = gmpi::interaction::GG_POINTER_FLAG_INCONTACT | gmpi::interaction::GG_POINTER_FLAG_PRIMARY | gmpi::interaction::GG_POINTER_FLAG_CONFIDENCE;
    flags |= gmpi::interaction::GG_POINTER_FLAG_FIRSTBUTTON;
    
    ApplyKeyModifiers(flags, theEvent);
    
    if(drawingFrame.inputClient)
        drawingFrame.inputClient->onPointerMove(mouseToGmpi(self, theEvent), flags);
}

- (void)scrollWheel:(NSEvent *)theEvent {
    // Get the scroll wheel delta
    auto deltaX = theEvent.deltaX;
    auto deltaY = theEvent.deltaY;
 
    int32_t flags = gmpi::interaction::GG_POINTER_FLAG_PRIMARY | gmpi::interaction::GG_POINTER_FLAG_CONFIDENCE;
    ApplyKeyModifiers(flags, theEvent);

    constexpr float wheelConversion = 120.0f; // on windows the wheel scrolls 120 per knotch
    if(deltaY)
    {
        // TODO         drawingFrame.getView()->onMouseWheel(flags, wheelConversion * deltaY, mousePos);
    }
    if(deltaX)
    {
        flags |= gmpi::interaction::GG_POINTER_SCROLL_HORIZ;
        // TODO         drawingFrame.getView()->onMouseWheel(flags, wheelConversion * deltaX, mousePos);
        //if(drawingFrame.inputClient)
        //    drawingFrame.inputClient->onMouseWheel(mouseToGmpi(self, theEvent), flags);
    }
}

- (void)mouseEntered:(NSEvent *)theEvent {
    [[self window] setAcceptsMouseMovedEvents:YES];
    [[self window] makeFirstResponder:self];
}

- (void)mouseMoved:(NSEvent *)theEvent {
   
    [self ToolTipOnMouseActivity];
    
    int32_t flags = gmpi::interaction::GG_POINTER_FLAG_INCONTACT | gmpi::interaction::GG_POINTER_FLAG_PRIMARY | gmpi::interaction::GG_POINTER_FLAG_CONFIDENCE;
    flags |= gmpi::interaction::GG_POINTER_FLAG_FIRSTBUTTON;
    
    ApplyKeyModifiers(flags, theEvent);
    
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
#endif

@end

// without including objective-C headers, we need to create an NSView from C++.
// here is the function here to return the view, using void* as return type.
void* createNativeView(void* parent, class IUnknown* paramHost, class IUnknown* client, int width, int height)
{
    NSSize inPreferredSize{(CGFloat)width, (CGFloat)height};
    
    NSView* native = [[GMPI_VIEW_CLASS alloc] initWithClient:client parameterHost:paramHost preferredSize:inPreferredSize];
    
    if(parent) // JUCE creates the view then *later* adds it to the parent. GMPI adds it here.
    {
        NSView* parentView = (NSView*) parent;
        [parentView addSubview:native];
    }
    
    return (void*) native;
}

void onCloseNativeView(void* ptr)
{
    auto view = (GMPI_VIEW_CLASS*) ptr;
    [view onClose];
}
