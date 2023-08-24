#import <Foundation/Foundation.h>

#import "SynthEditCocoaView.h"
#include "CocoaNamespaceMacros.h"
#include "./CocoaGuiHost.h"
#include "../se_sdk3_hosting/GraphicsRedrawClient.h"

// In VST3 wrapper this object is a child window of SynthEditPluginCocoaView,
// It serves to provide a C++ to Objective-C adaptor to the gmpi Drawing framework.
// (actual parent is objective c).

class DrawingFrameCocoa :
#ifdef GMPI_HOST_POINTER_SUPPORT
public gmpi_gui::IMpGraphicsHost,
public GmpiGuiHosting::PlatformTextEntryObserver,
#endif
/*public gmpi::IMpUserInterfaceHost2,*/
public IDrawingHost
{
    gmpi::shared_ptr<IDrawingClient> drawingClient;
#ifdef GMPI_HOST_POINTER_SUPPORT
    gmpi::shared_ptr<gmpi_gui_api::IMpGraphics3> client;
    int32_t mouseCaptured = 0;
    GmpiGuiHosting::PlatformTextEntry* currentTextEdit = nullptr;
#endif

public:
    gmpi::cocoa::DrawingFactory drawingFactory;
    NSView* view;

#if 0
    void Init(SE2::IPresenter* presenter, class IGuiHost2* hostPatchManager, int pviewType)
    {
        controller = hostPatchManager;
        
        const int topViewBounds = 8000; // simply a large enough size.
        containerView.Attach(new SE2::ContainerView(GmpiDrawing::Size(topViewBounds,topViewBounds)));
        containerView->setHost(static_cast<gmpi_gui::IMpGraphicsHost*>(this));
        
        containerView->setDocument(presenter, pviewType);
        
#if defined(SE_TARGET_AU)
        dynamic_cast<SEInstrumentBase*>(controller)->callbackOnUnloadPlugin = [this]
        {
            containerView = nullptr; // free all objects early to avoid dangling pointers to AudioUnit.
            controller = nullptr; // ensure we don't reference in in destructor.
        };
#endif
     }
#endif
    
    void Init(gmpi::api::IUnknown* pclient)
    {
 //todo       client = pclient;
        
        pclient->queryInterface(&IDrawingClient::guid, drawingClient.asIMpUnknownPtr());
        
        if(drawingClient)
        {
            drawingClient->open(this);
        }
    }
     
     void DeInit()
     {
//         client = {};
         drawingClient = {};
     }

    ~DrawingFrameCocoa()
    {
#if defined(SE_TARGET_AU)
        auto audioUnit = dynamic_cast<SEInstrumentBase*>(controller);
        if(audioUnit)
        {
            audioUnit->callbackOnUnloadPlugin = nullptr;
        }
#endif
// alternative?
// controller->OnDrawingFrameDeleted(); // nulls it's 'OnUnloadPlugin' callback
    }
    
    void onRender(NSView* frame, gmpi::drawing::Rect* dirtyRect)
    {
        gmpi::cocoa::GraphicsContext context(frame, &drawingFactory);
        
        if(drawingClient)
            drawingClient->onRender(static_cast<gmpi::drawing::api::IDeviceContext*>(&context));

 //       context.popAxisAlignedClip();
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
    virtual gmpi::ReturnCode  setCapture(void) override
    {
        mouseCaptured = 1;
        return gmpi::ReturnCode::Ok;
    }
    virtual gmpi::ReturnCode  getCapture(int32_t & returnValue) override
    {
        returnValue = mouseCaptured;
        return gmpi::ReturnCode::Ok;
    }
    virtual gmpi::ReturnCode  releaseCapture(void) override
    {
        mouseCaptured = 0;
        return gmpi::ReturnCode::Ok;
    }
#endif
    gmpi::ReturnCode getDrawingFactory(gmpi::api::IUnknown ** returnFactory) override
    {
        *returnFactory = &drawingFactory;
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

        if (*iid == IDrawingHost::guid)
        {
            *returnInterface = reinterpret_cast<void*>(static_cast<IDrawingHost*>(this));
            addRef();
            return gmpi::ReturnCode::Ok;
        }

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
#if 0
    void removeTextEdit()
    {
        if(!currentTextEdit)
            return;
        
        currentTextEdit->CallbackFromCocoa(nil);
    }
    
    void onTextEditRemoved() override
    {
        currentTextEdit = nullptr;
    }
#endif
    GMPI_REFCOUNT_NO_DELETE;
};

class whatever
{
public:
    ~whatever()
    {
//        _RPT0(0, "~whatever()\n");
    }
};

#define SYNTHEDIT_PLUGIN_COCOA_NSVIEW_WRAPPER_CLASSNAME SE_MAKE_CLASSNAME(Cocoa_NSViewWrapperForAU)

//--------------------------------------------------------------------------------------------------------------
@interface SYNTHEDIT_PLUGIN_COCOA_NSVIEW_WRAPPER_CLASSNAME : NSView {
    //--------------------------------------------------------------------------------------------------------------
    DrawingFrameCocoa drawingFrame;
    NSTrackingArea* trackingArea;
    NSTimer* timer;
    int toolTipTimer;
    bool toolTipShown;
    gmpi::drawing::Point mousePos;
    
    whatever testIfItGetsDeleted;
}

- (id) initWithController: (class IGuiHost2*) _editController preferredSize: (NSSize) size;
- (id) initWithClient: (class IMpUnknown*) _client preferredSize: (NSSize) size;
- (void)drawRect:(NSRect)dirtyRect;
- (void)onTimer: (NSTimer*) t;

@end

@interface SeTestView : NSView {
     NSTrackingArea* trackingArea;
   /*
    DrawingFrameCocoa drawingFrame;
    NSTimer* timer;
    int toolTipTimer;
    bool toolTipShown;
    gmpi::drawing::Point mousePos;
    */
    
    whatever testIfItGetsDeleted;
}

@end

//--------------------------------------------------------------------------------------------------------------
// SMTG_AU_PLUGIN_NAMESPACE (SMTGAUPluginCocoaView)
//--------------------------------------------------------------------------------------------------------------

#if defined(SE_TARGET_AU)

//--------------------------------------------------------------------------------------------------------------
@implementation SYNTHEDIT_PLUGIN_COCOA_VIEW_CLASSNAME


//--------------------------------------------------------------------------------------------------------------
- (unsigned) interfaceVersion
{
    return 0;
}

//--------------------------------------------------------------------------------------------------------------
- (NSString *) description
{
    return @"Cocoa View";
}

//--------------------------------------------------------------------------------------------------------------

- (NSView *)uiViewForAudioUnit:(AudioUnit)inAU withSize:(NSSize)inPreferredSize
{
    class ausdk::AUBase* editController = {};
    UInt32 size = sizeof (editController);
    if (AudioUnitGetProperty (inAU, 64000, kAudioUnitScope_Global, 0, &editController, &size) != noErr)
        return nil;
    
    return [[[SYNTHEDIT_PLUGIN_COCOA_NSVIEW_WRAPPER_CLASSNAME alloc] initWithController:dynamic_cast<IGuiHost2*>(editController) preferredSize:inPreferredSize] autorelease];
}

@end

#endif

@implementation SeTestView

- (id) initWithController: (class IGuiHost2*) _editController preferredSize: (NSSize) size_unused
{
//    _RPT0(0, "SeTestView-initWithController()\n");
    self = [super initWithFrame: NSMakeRect (0, 0, 200, 200)];
    
    if (!self)
        return nil;
    
    return self;
}

// Opt-in to notification that mouse entered window.
- (void)viewDidMoveToWindow {
    [super viewDidMoveToWindow];

    auto window = [self window];
    if(window)
    {
//        _RPT0(0, "viewDidMoveToWindow. Adding trackingArea\n");
   
        // drawingFrame.drawingFactory.setBestColorSpace(window);
        trackingArea = [NSTrackingArea alloc];
        [trackingArea initWithRect:NSZeroRect options:(NSTrackingMouseEnteredAndExited | NSTrackingInVisibleRect | NSTrackingMouseMoved| NSTrackingActiveAlways) owner:self userInfo:nil];
        [self addTrackingArea:trackingArea ];
    }
}

- (void) removeFromSuperview
{
    [super removeFromSuperview];
    
    // Editor is closing
    if( trackingArea )
    {
//        _RPT0(0, "removeFromSuperview. Removing trackingArea\n");
        [trackingArea release];
        trackingArea = nil;
    }
}

- (void)drawRect:(NSRect)dirtyRect
{
    [[NSColor orangeColor] set];
    NSRect aRect = NSMakeRect(5.0, 5.0, 7.0, 8.0);
    NSRectFill(aRect);
}

@end


//--------------------------------------------------------------------------------------------------------------
@implementation SYNTHEDIT_PLUGIN_COCOA_NSVIEW_WRAPPER_CLASSNAME

//--------------------------------------------------------------------------------------------------------------
- (id) initWithController: (class IGuiHost2*) _editController preferredSize: (NSSize) size_unused
{
#if 0
    Json::Value document_json;
    Json::Reader r;
    r.parse(BundleInfo::instance()->getResource("gui.se.json"), document_json);
    auto& gui_json = document_json["gui"];
    int width = gui_json["width"].asInt();
    int height = gui_json["height"].asInt();

    self = [super initWithFrame: NSMakeRect (0, 0, width, height)];
    if (self)
    {
        self->trackingArea = [NSTrackingArea alloc];
        [self->trackingArea initWithRect:NSZeroRect options:(NSTrackingMouseEnteredAndExited | NSTrackingInVisibleRect | NSTrackingMouseMoved| NSTrackingActiveAlways) owner:self userInfo:nil];
        [self addTrackingArea:self->trackingArea ];
        
        drawingFrame.view = self;
        auto presenter = new JsonDocPresenter(_editController);
        drawingFrame.Init(presenter, _editController, CF_PANEL_VIEW);
        presenter->RefreshView();
        
        timer = [NSTimer scheduledTimerWithTimeInterval:0.1 target:self selector:@selector(onTimer:) userInfo:nil repeats:YES ];
    }
    return self;
#else
    return nil;
#endif
}

- (id) initWithClient: (class IMpUnknown*) _client preferredSize: (NSSize) size
{
    self = [super initWithFrame: NSMakeRect (0, 0, size.width, size.height)];
    if (self)
    {
        self->trackingArea = [NSTrackingArea alloc];
        [self->trackingArea initWithRect:NSZeroRect options:(NSTrackingMouseEnteredAndExited | NSTrackingInVisibleRect | NSTrackingMouseMoved| NSTrackingActiveAlways) owner:self userInfo:nil];
        [self addTrackingArea:self->trackingArea ];
        
        drawingFrame.view = self;
        drawingFrame.Init((gmpi::api::IUnknown*) _client);
        
        timer = [NSTimer scheduledTimerWithTimeInterval:0.1 target:self selector:@selector(onTimer:) userInfo:nil repeats:YES ];
    }
    return self;
}


// Opt-in to notification that mouse entered window.
- (void)viewDidMoveToWindow {
     [super viewDidMoveToWindow];

    auto window = [self window];
    if(window)
    {
        drawingFrame.drawingFactory.setBestColorSpace(window);
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
/* don't seem nesc

    if( trackingArea )
    {
        [self removeTrackingArea:trackingArea];
        trackingArea = nil;
    }
 */
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
    
#ifdef _DEBUG
    {
        // write colorspace to window in debug mode
        auto window = [self window];
        auto windowColorspace = [window colorSpace];
        auto desc = [windowColorspace debugDescription];
        
        [desc drawAtPoint:CGPointMake(110,0) withAttributes:nil];
        
        const auto bpp = NSBitsPerSampleFromDepth( [window depthLimit] );
        NSString *str = [@(bpp) stringValue];
        
        [str drawAtPoint:CGPointMake(0,0) withAttributes:nil];
        
        if([window canRepresentDisplayGamut:NSDisplayGamutP3])
        {
            [@"P3:YES" drawAtPoint:CGPointMake(320,0) withAttributes:nil];
        }
        else{
            [@"P3:NO" drawAtPoint:CGPointMake(320,0) withAttributes:nil];
        }
    }
#endif
 }

//--------------------------------------------------------------------------------------------------------------
- (void) setFrame: (NSRect) newSize
{
    [super setFrame: newSize];
}

//--------------------------------------------------------------------------------------------------------------
- (BOOL)isFlipped { return YES; }

- (BOOL)acceptsFirstMouse:(NSEvent *)event { return YES; }

//-------------------------------------------------------------------------------------------------------------
#if 0
void ApplyKeyModifiers(int32_t& flags, NSEvent* theEvent)
{
    // <Shift> key?
    if(([theEvent modifierFlags ] & (NSEventModifierFlagShift | NSEventModifierFlagCapsLock)) != 0)
    {
        flags |= gmpi_gui_api::GG_POINTER_KEY_SHIFT;
    }
    
    if(([theEvent modifierFlags ] & NSEventModifierFlagControl /* was NSEventModifierFlagCommand*/ ) != 0)
    {
        flags |= gmpi_gui_api::GG_POINTER_KEY_CONTROL;
    }
    
    if(([theEvent modifierFlags ] & NSEventModifierFlagOption) != 0) // <Option> key.
    {
        flags |= gmpi_gui_api::GG_POINTER_KEY_ALT;
    }
}


- (void)mouseDown:(NSEvent *)theEvent
{
    drawingFrame.removeTextEdit();
    
    [[self window] makeFirstResponder:self]; // take focus off any text-edit. Works but does not dimiss it.
 
    NSPoint localPoint = [self convertPoint: [theEvent locationInWindow] fromView: nil];

    gmpi::drawing::Point p(localPoint.x, localPoint.y);

    int32_t flags = gmpi_gui_api::GG_POINTER_FLAG_INCONTACT | gmpi_gui_api::GG_POINTER_FLAG_PRIMARY | gmpi_gui_api::GG_POINTER_FLAG_CONFIDENCE;
    flags |= gmpi_gui_api::GG_POINTER_FLAG_NEW;
	flags |= gmpi_gui_api::GG_POINTER_FLAG_FIRSTBUTTON;
    
    ApplyKeyModifiers(flags, theEvent);
    
 // TODO   drawingFrame.getView()->onPointerDown(flags, p);
    
 // no help to edit box   [super mouseDown:theEvent];
}

- (void)rightMouseDown:(NSEvent *)theEvent
{
    drawingFrame.removeTextEdit();
    
    NSPoint localPoint = [self convertPoint: [theEvent locationInWindow] fromView: nil];
    gmpi::drawing::Point p(localPoint.x, localPoint.y);
    
    int32_t flags = gmpi_gui_api::GG_POINTER_FLAG_INCONTACT | gmpi_gui_api::GG_POINTER_FLAG_PRIMARY | gmpi_gui_api::GG_POINTER_FLAG_CONFIDENCE;
    flags |= gmpi_gui_api::GG_POINTER_FLAG_NEW;
    flags |= gmpi_gui_api::GG_POINTER_FLAG_SECONDBUTTON;
    
    ApplyKeyModifiers(flags, theEvent);
    
    // TODO     drawingFrame.getView()->onPointerDown(flags, p);
}

- (void)rightMouseUp:(NSEvent *)theEvent
{
    NSPoint localPoint = [self convertPoint: [theEvent locationInWindow] fromView: nil];
    gmpi::drawing::Point p(localPoint.x, localPoint.y);
    
    int32_t flags = gmpi_gui_api::GG_POINTER_FLAG_INCONTACT | gmpi_gui_api::GG_POINTER_FLAG_PRIMARY | gmpi_gui_api::GG_POINTER_FLAG_CONFIDENCE;
    flags |= gmpi_gui_api::GG_POINTER_FLAG_NEW;
    flags |= gmpi_gui_api::GG_POINTER_FLAG_SECONDBUTTON;
    
    ApplyKeyModifiers(flags, theEvent);
    
    // TODO     drawingFrame.getView()->onPointerUp(flags, p);
}

- (void)mouseUp:(NSEvent *)theEvent {
    NSPoint localPoint = [self convertPoint: [theEvent locationInWindow] fromView: nil];
    gmpi::drawing::Point p(localPoint.x, localPoint.y);

    int32_t flags = gmpi_gui_api::GG_POINTER_FLAG_INCONTACT | gmpi_gui_api::GG_POINTER_FLAG_PRIMARY | gmpi_gui_api::GG_POINTER_FLAG_CONFIDENCE;
    flags |= gmpi_gui_api::GG_POINTER_FLAG_FIRSTBUTTON;
    
    ApplyKeyModifiers(flags, theEvent);
    
    // TODO     drawingFrame.getView()->onPointerUp(flags, p);
}

- (void)mouseDragged:(NSEvent *)theEvent {
    
    NSPoint localPoint = [self convertPoint: [theEvent locationInWindow] fromView: nil];

    mousePos.x = static_cast<float>(localPoint.x);
    mousePos.y = static_cast<float>(localPoint.y);

    int32_t flags = gmpi_gui_api::GG_POINTER_FLAG_INCONTACT | gmpi_gui_api::GG_POINTER_FLAG_PRIMARY | gmpi_gui_api::GG_POINTER_FLAG_CONFIDENCE;
    flags |= gmpi_gui_api::GG_POINTER_FLAG_FIRSTBUTTON;
    
    ApplyKeyModifiers(flags, theEvent);
    
    // TODO     drawingFrame.getView()->onPointerMove(flags, mousePos);
}

- (void)scrollWheel:(NSEvent *)theEvent {
    // Get the scroll wheel delta
    auto deltaX = theEvent.deltaX;
    auto deltaY = theEvent.deltaY;
 
    NSPoint localPoint = [self convertPoint: [theEvent locationInWindow] fromView: nil];

    mousePos.x = static_cast<float>(localPoint.x);
    mousePos.y = static_cast<float>(localPoint.y);

    int32_t flags = gmpi_gui_api::GG_POINTER_FLAG_PRIMARY | gmpi_gui_api::GG_POINTER_FLAG_CONFIDENCE;
    ApplyKeyModifiers(flags, theEvent);

    constexpr float wheelConversion = 120.0f; // on windows the wheel scrolls 120 per knotch
    if(deltaY)
    {
        // TODO         drawingFrame.getView()->onMouseWheel(flags, wheelConversion * deltaY, mousePos);
    }
    if(deltaX)
    {
        flags |= gmpi_gui_api::GG_POINTER_SCROLL_HORIZ;
        // TODO         drawingFrame.getView()->onMouseWheel(flags, wheelConversion * deltaX, mousePos);
    }
}

- (void)mouseEntered:(NSEvent *)theEvent {
    [[self window] setAcceptsMouseMovedEvents:YES];
    [[self window] makeFirstResponder:self];
}

- (void)mouseMoved:(NSEvent *)theEvent {
    NSPoint localPoint = [self convertPoint: [theEvent locationInWindow] fromView: nil];

    mousePos.x = static_cast<float>(localPoint.x);
    mousePos.y = static_cast<float>(localPoint.y);
    
    [self ToolTipOnMouseActivity];
    
    int32_t flags = gmpi_gui_api::GG_POINTER_FLAG_INCONTACT | gmpi_gui_api::GG_POINTER_FLAG_PRIMARY | gmpi_gui_api::GG_POINTER_FLAG_CONFIDENCE;
    flags |= gmpi_gui_api::GG_POINTER_FLAG_FIRSTBUTTON;
    
    ApplyKeyModifiers(flags, theEvent);
    
    // TODO     drawingFrame.getView()->onPointerMove(flags, mousePos);
}

- (void)mouseExited:(NSEvent *)theEvent {
    [[self window] setAcceptsMouseMovedEvents:NO];
}

- (void) onTimer: (NSTimer*) t {
    if(toolTipTimer-- == 0 && !toolTipShown)
    {
        gmpi_sdk::MpString text;
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
void* createNativeView(class IGuiHost2* controller, int width, int height)
{
    NSSize inPreferredSize{(CGFloat)width, (CGFloat)height};
    
    return (void*) [[SYNTHEDIT_PLUGIN_COCOA_NSVIEW_WRAPPER_CLASSNAME alloc] initWithController:controller preferredSize:inPreferredSize];
}

// VST3 version
void* createNativeView(void* parent, class IGuiHost2* controller, int width, int height)
{
    NSSize inPreferredSize{(CGFloat)width, (CGFloat)height};
    NSView* native =
    [[SYNTHEDIT_PLUGIN_COCOA_NSVIEW_WRAPPER_CLASSNAME alloc] initWithController:controller preferredSize:inPreferredSize];
    
    NSView* parentView = (NSView*) parent;
    [parentView addSubview:native];
    
    return (void*) native;
}

void* createNativeView(class IMpUnknown* client, int width, int height)
{
    NSSize inPreferredSize{(CGFloat)width, (CGFloat)height};
    
    return (void*) [[SYNTHEDIT_PLUGIN_COCOA_NSVIEW_WRAPPER_CLASSNAME alloc] initWithClient:client preferredSize:inPreferredSize];
}

void onCloseNativeView(void* ptr)
{
    auto view = (SYNTHEDIT_PLUGIN_COCOA_NSVIEW_WRAPPER_CLASSNAME*) ptr;
    [view onClose];
}
