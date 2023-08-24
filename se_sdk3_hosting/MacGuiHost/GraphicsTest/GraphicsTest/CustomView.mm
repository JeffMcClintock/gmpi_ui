#import <Foundation/Foundation.h>

#import "CustomView.h"
#import "../../../../se_sdk3_hosting/Cocoa_Gfx.h"
#import "../../../../se_sdk3_hosting/CocoaGuiHost.h"
//#import "../../../../../../Shared/ContainerView.h"
//#include "../../Shared/JsonDocPresenter.h"
//#include "BundleInfo.h"
//#include "CocoaNamespaceMacros.h"
//#include "AUInstrumentBase.h"
#include "GraphicsClientCodeTest.h"

class DrawingFrameCocoa : public gmpi_gui::IMpGraphicsHost, public gmpi::IMpUserInterfaceHost2
    , public GmpiGuiHosting::PlatformTextEntryObserver
{
    gmpi::cocoa::DrawingFactory DrawingFactory;
//    IGuiHost2* controller;
    int32_t mouseCaptured = 0;
    GmpiGuiHosting::PlatformTextEntry* currentTextEdit = nullptr;
    
public:
    NSView* view = nullptr;
    gmpi_gui_api::IMpGraphics *client = nullptr;
    
    DrawingFrameCocoa()
    {
  
    }
    void Init()
    {
 //       controller = hostPatchManager;
        
//        const int topViewBounds = 8000; // simply a large enough size.
     }
    
    void OnRender(NSView* frame, GmpiDrawing_API::MP1_RECT* dirtyRect)
    {
        gmpi::cocoa::DrawingFactory cocoafactory;
        gmpi::cocoa::GraphicsContext context(frame, &cocoafactory);
        
        context.PushAxisAlignedClip(dirtyRect);
        
//        auto c = GmpiDrawing::Color::FromRgb(0xff00ff);
 //       context.Clear(&c);
        
        client->OnRender(&context);
        
        context.PopAxisAlignedClip();
    }
    
    // Inherited via IMpUserInterfaceHost2
    virtual int32_t MP_STDCALL pinTransmit(int32_t pinId, int32_t size, const void * data, int32_t voice = 0) override
    {
//TODO         assert(false); // not implemented.
        return gmpi::MP_FAIL;
    }
    virtual int32_t MP_STDCALL createPinIterator(gmpi::IMpPinIterator** returnIterator) override
    {
//TODO         assert(false); // not implemented.
        return gmpi::MP_FAIL;
    }
    virtual int32_t MP_STDCALL getHandle(int32_t & returnValue) override
    {
 //TODO        assert(false); // not implemented.
        return gmpi::MP_FAIL;
    }
    virtual int32_t MP_STDCALL sendMessageToAudio(int32_t id, int32_t size, const void * messageData) override
    {
 //TODO        assert(false); // not implemented.
        return gmpi::MP_FAIL;
    }
    virtual int32_t MP_STDCALL ClearResourceUris() override
    {
//TODO         assert(false); // not implemented.
        return gmpi::MP_FAIL;
    }
    virtual int32_t MP_STDCALL RegisterResourceUri(const char * resourceName, const char * resourceType, gmpi::IString* returnString) override
    {
//TODO         assert(false); // not implemented.
        return gmpi::MP_FAIL;
    }
    virtual int32_t MP_STDCALL OpenUri(const char * fullUri, gmpi::IProtectedFile2** returnStream) override
    {
//TODO         assert(false); // not implemented.
        return gmpi::MP_FAIL;
    }
    virtual int32_t MP_STDCALL FindResourceU(const char * resourceName, const char * resourceType, gmpi::IString* returnString) override
    {
//TODO         assert(false); // not implemented.
        return gmpi::MP_FAIL;
    }
    
    virtual int32_t MP_STDCALL LoadPresetFile_DEPRECATED(const char* presetFilePath) override
    {
        //TODO         assert(false); // not implemented.
        return gmpi::MP_FAIL;
    }

    // IMpGraphicsHost
    virtual void MP_STDCALL invalidateRect(const GmpiDrawing_API::MP1_RECT* invalidRect) override
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
    virtual void MP_STDCALL invalidateMeasure() override
    {
//TODO        assert(false); // not implemented.
    }
    virtual int32_t MP_STDCALL setCapture(void) override
    {
        mouseCaptured = 1;
        return gmpi::MP_OK;
    }
    virtual int32_t MP_STDCALL getCapture(int32_t & returnValue) override
    {
        returnValue = mouseCaptured;
        return gmpi::MP_OK;
    }
    virtual int32_t MP_STDCALL releaseCapture(void) override
    {
        mouseCaptured = 0;
        return gmpi::MP_OK;
    }
    virtual int32_t MP_STDCALL GetDrawingFactory(GmpiDrawing_API::IMpFactory ** returnFactory) override
    {
        *returnFactory = &DrawingFactory;
        return gmpi::MP_OK;
    }
    
    virtual int32_t MP_STDCALL createPlatformMenu(GmpiDrawing_API::MP1_RECT* rect, gmpi_gui::IMpPlatformMenu** returnMenu) override
    {
        *returnMenu = new GmpiGuiHosting::PlatformMenu(view, rect);
        return gmpi::MP_OK;
    }
    virtual int32_t MP_STDCALL createPlatformTextEdit(GmpiDrawing_API::MP1_RECT* rect, gmpi_gui::IMpPlatformText** returnTextEdit) override
    {
        currentTextEdit = new GmpiGuiHosting::PlatformTextEntry(this, view, rect);
        *returnTextEdit = currentTextEdit;
        return gmpi::MP_OK;
    }
    virtual int32_t MP_STDCALL createFileDialog(int32_t dialogType, gmpi_gui::IMpFileDialog** returnFileDialog) override
    {
        *returnFileDialog = new GmpiGuiHosting::PlatformFileDialog(dialogType, view);
        return gmpi::MP_OK;
    }
    virtual int32_t MP_STDCALL createOkCancelDialog(int32_t dialogType, gmpi_gui::IMpOkCancelDialog** returnDialog) override
    {
        *returnDialog = new GmpiGuiHosting::PlatformOkCancelDialog(dialogType, view);
        return gmpi::MP_OK;
    }
    
    // IUnknown methods
    virtual int32_t MP_STDCALL queryInterface(const gmpi::MpGuid& iid, void** returnInterface) override
    {
        if (iid == gmpi::MP_IID_UI_HOST2)
        {
            // important to cast to correct vtable (ug_plugin3 has 2 vtables) before reinterpret cast
            *returnInterface = reinterpret_cast<void*>(static_cast<IMpUserInterfaceHost2*>(this));
            addRef();
            return gmpi::MP_OK;
        }
        
        if (iid == gmpi_gui::SE_IID_GRAPHICS_HOST || iid == gmpi_gui::SE_IID_GRAPHICS_HOST_BASE || iid == gmpi::MP_IID_UNKNOWN)
        {
            // important to cast to correct vtable (ug_plugin3 has 2 vtables) before reinterpret cast
            *returnInterface = reinterpret_cast<void*>(static_cast<IMpGraphicsHost*>(this));
            addRef();
            return gmpi::MP_OK;
        }
        
        *returnInterface = 0;
        return gmpi::MP_NOSUPPORT;
    }

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

    GMPI_REFCOUNT_NO_DELETE;
};


//--------------------------------------------------------------------------------------------------------------
@interface CustomView : NSView {
    //--------------------------------------------------------------------------------------------------------------
    DrawingFrameCocoa drawingFrame;
    NSTrackingArea* trackingArea;
    TestClient client;
}

- (id) initWithFrame:(NSRect)frameRect;
- (id) initWithCoder:(NSCoder*)coder;
//- (id) initWithEditController: /*(Vst::IEditController*)*/ (class AUBase*) editController audioUnit: (AudioUnit) au preferredSize: (NSSize) size;

- (void)drawRect:(NSRect)dirtyRect;
@end

//--------------------------------------------------------------------------------------------------------------
// SMTG_AU_PLUGIN_NAMESPACE (SMTGAUPluginCocoaView)
//--------------------------------------------------------------------------------------------------------------
/*
//--------------------------------------------------------------------------------------------------------------
@implementation CustomAuView


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
/ *
//--------------------------------------------------------------------------------------------------------------
- (NSView *)uiViewForAudioUnit:(AudioUnit)inAU withSize:(NSSize)inPreferredSize
{
//    Vst::IEditController* editController = 0;
    class AUBase* editController = 0;
    UInt32 size = sizeof (void*);
    if (AudioUnitGetProperty (inAU, 64000, kAudioUnitScope_Global, 0, &editController, &size) != noErr)
        return nil;
    return [[[SYNTHEDIT_PLUGIN_COCOA_NSVIEW_WRAPPER_CLASSNAME alloc] initWithEditController:editController audioUnit:inAU preferredSize:inPreferredSize] autorelease];
}
* /
@end
*/
//--------------------------------------------------------------------------------------------------------------
@implementation CustomView
/*
//--------------------------------------------------------------------------------------------------------------
- (id) initWithEditController: / *(class AUBase*) _editController audioUnit: (AudioUnit) au* / preferredSize: (NSSize) size
{
//    Json::Value document_json;
//    Json::Reader r;
 //   r.parse(BundleInfo::instance()->getResource("gui.se.json"), document_json);
//    auto& gui_json = document_json["gui"];
    int width = 100;//gui_json["width"].asInt();
    int height = 100;//gui_json["height"].asInt();

    self = [super initWithFrame: NSMakeRect (0, 0, width, height)];
    if (self)
    {
        self->trackingArea = [NSTrackingArea alloc];
        [self->trackingArea initWithRect:NSZeroRect options:(NSTrackingMouseEnteredAndExited | NSTrackingInVisibleRect | NSTrackingMouseMoved| NSTrackingActiveAlways) owner:self userInfo:nil];
        [self addTrackingArea:self->trackingArea ];
        //[self colorSpace:];
        //    [self addTrackingArea: [[NSTrackingArea alloc] initWithRect:NSZeroRect options:(NSTrackingMouseEnteredAndExited | NSTrackingInVisibleRect | NSTrackingMouseMoved| NSTrackingActiveAlways) owner:self userInfo:nil]];
        
        drawingFrame.view = self;
//        auto presenter = new JsonDocPresenter(&gui_json, dynamic_cast<IGuiHost2*>(_editController));
 //       drawingFrame.Init(presenter, dynamic_cast<IGuiHost2*>(_editController), CF_PANEL_VIEW);
 //       presenter->RefreshView();
        

    }
    return self;
}
*/
- (id) initWithFrame:(NSRect)frame; // not called.
{
    self = [super initWithFrame:frame];
    
    drawingFrame.view = self;
    drawingFrame.client = &client;
    client.setHost(static_cast<gmpi_gui::IMpGraphicsHost*>(&drawingFrame));
    return self;
}

- (id) initWithCoder:(NSCoder*)coder
{
    self = [super initWithCoder:coder];
    
    drawingFrame.view = self;
    drawingFrame.client = &client;
    client.setHost(static_cast<gmpi_gui::IMpGraphicsHost*>(&drawingFrame));

    return self;
}

- (void)drawRect:(NSRect)dirtyRect
{
    
 //   NSRect bounds = [self bounds];
 //   [[NSColor greenColor] set];
 //   [NSBezierPath fillRect:bounds];
    
    GmpiDrawing::Rect r(dirtyRect.origin.x, dirtyRect.origin.y, dirtyRect.origin.x + dirtyRect.size.width, dirtyRect.origin.y + dirtyRect.size.height);

    drawingFrame.OnRender(self, &r);
 }

//--------------------------------------------------------------------------------------------------------------
- (void) setFrame: (NSRect) newSize
{
    [super setFrame: newSize];
    /*
    ViewRect viewRect (0, 0, newSize.size.width, newSize.size.height);
    
    if (plugView)
        plugView->onSize (&viewRect);
     */
}


//--------------------------------------------------------------------------------------------------------------
- (BOOL)isFlipped { return YES; }

- (BOOL)acceptsFirstMouse:(NSEvent *)event { return YES; }

//--------------------------------------------------------------------------------------------------------------
    /*- (void)viewDidMoveToSuperview
{

    if (plugView)
    {
        if ([self superview])
        {
            if (!isAttached)
            {
                isAttached = plugView->attached (self, kPlatformTypeNSView) == kResultTrue;
            }
        }
        else
        {
            if (isAttached)
            {
                plugView->removed ();
                isAttached = NO;
            }
        }
    }

}
     */
//--------------------------------------------------------------------------------------------------------------
- (void) dealloc
{
    if( trackingArea )
        [self removeTrackingArea:trackingArea];

    [super dealloc];
}

void ApplyKeyModifiers(int32_t& flags, NSEvent* theEvent)
{
    // <Shift> key?
    if(([theEvent modifierFlags ] & (NSShiftKeyMask | NSAlphaShiftKeyMask)) != 0)
    {
        flags |= gmpi_gui_api::GG_POINTER_KEY_SHIFT;
    }
    
    if(([theEvent modifierFlags ] & NSCommandKeyMask) != 0)
    {
        flags |= gmpi_gui_api::GG_POINTER_KEY_CONTROL;
    }
    
    if(([theEvent modifierFlags ] & NSAlternateKeyMask) != 0) // <Option> key.
    {
        flags |= gmpi_gui_api::GG_POINTER_KEY_ALT;
    }
}

- (void)mouseDown:(NSEvent *)theEvent
{
    // Try to close text edit
        /*
    if( [[[self window] firstResponder]isKindOfClass:[NSTextView class]])
    {
        auto textview = (NSTextView*) [[self window] firstResponder];
 //       [[textview delegate] textView:<#(nonnull NSTextView *)#> doCommandBySelector:<#(nonnull SEL)#>];
    }

        
        drawingFrame.removeTextEdit();
/ *
        auto textview = (NSTextView*) [[self window] firstResponder];
        auto del = textview.delegate;
        auto notification = [NSNotification alloc];// notification;
        [del textDidEndEditing:notification];
  //      [textview.delegate textDidEndEditing:textview];
//        [[(NSTextView*)[[self window] firstResponder] delegate] textDidEndEditing:<#(nonnull NSNotification *)#>];
//[[self window] performSelector:@selector(resignFirstResponder:) withObject:myTextField afterDelay:0.0];
 * /
    }
 */
    [[self window] makeFirstResponder:self]; // take focus off any text-edit. Works but does not dimiss it.
 
    NSPoint localPoint = [self convertPoint: [theEvent locationInWindow] fromView: nil];
//    GmpiDrawing::Point p([theEvent locationInWindow].x, self.frame.size.height - [theEvent locationInWindow].y);
    GmpiDrawing::Point p(localPoint.x, localPoint.y);

    int32_t flags = gmpi_gui_api::GG_POINTER_FLAG_INCONTACT | gmpi_gui_api::GG_POINTER_FLAG_PRIMARY | gmpi_gui_api::GG_POINTER_FLAG_CONFIDENCE;
    flags |= gmpi_gui_api::GG_POINTER_FLAG_NEW;
	flags |= gmpi_gui_api::GG_POINTER_FLAG_FIRSTBUTTON;
    
    ApplyKeyModifiers(flags, theEvent);
    
// pass to ContainerView    drawingFrame.getView()->onPointerDown(flags, p);
    client.onPointerDown(flags, p);
 // no help to edit box   [super mouseDown:theEvent];
}

- (void)rightMouseDown:(NSEvent *)theEvent
{
    NSPoint localPoint = [self convertPoint: [theEvent locationInWindow] fromView: nil];
    GmpiDrawing::Point p(localPoint.x, localPoint.y);
    
    int32_t flags = gmpi_gui_api::GG_POINTER_FLAG_INCONTACT | gmpi_gui_api::GG_POINTER_FLAG_PRIMARY | gmpi_gui_api::GG_POINTER_FLAG_CONFIDENCE;
    flags |= gmpi_gui_api::GG_POINTER_FLAG_NEW;
    flags |= gmpi_gui_api::GG_POINTER_FLAG_SECONDBUTTON;
    
    ApplyKeyModifiers(flags, theEvent);
    
// pass to ContainerView     drawingFrame.getView()->onPointerDown(flags, p);
    client.onPointerDown(flags, p);
}

- (void)rightMouseUp:(NSEvent *)theEvent
{
    NSPoint localPoint = [self convertPoint: [theEvent locationInWindow] fromView: nil];
    GmpiDrawing::Point p(localPoint.x, localPoint.y);
    
    int32_t flags = gmpi_gui_api::GG_POINTER_FLAG_INCONTACT | gmpi_gui_api::GG_POINTER_FLAG_PRIMARY | gmpi_gui_api::GG_POINTER_FLAG_CONFIDENCE;
    flags |= gmpi_gui_api::GG_POINTER_FLAG_NEW;
    flags |= gmpi_gui_api::GG_POINTER_FLAG_SECONDBUTTON;
    
    ApplyKeyModifiers(flags, theEvent);
    
 // pass to ContainerView    drawingFrame.getView()->onPointerUp(flags, p);
    client.onPointerUp(flags, p);
}

- (void)mouseUp:(NSEvent *)theEvent {
    NSPoint localPoint = [self convertPoint: [theEvent locationInWindow] fromView: nil];
    GmpiDrawing::Point p(localPoint.x, localPoint.y);

    int32_t flags = gmpi_gui_api::GG_POINTER_FLAG_INCONTACT | gmpi_gui_api::GG_POINTER_FLAG_PRIMARY | gmpi_gui_api::GG_POINTER_FLAG_CONFIDENCE;
    flags |= gmpi_gui_api::GG_POINTER_FLAG_FIRSTBUTTON;
    
    ApplyKeyModifiers(flags, theEvent);
    
 // pass to ContainerView    drawingFrame.getView()->onPointerUp(flags, p);
    client.onPointerUp(flags, p);
}

- (void)mouseDragged:(NSEvent *)theEvent {
    
    NSPoint localPoint = [self convertPoint: [theEvent locationInWindow] fromView: nil];
    //    GmpiDrawing::Point p([theEvent locationInWindow].x, self.frame.size.height - [theEvent locationInWindow].y);
    GmpiDrawing::Point p(localPoint.x, localPoint.y);

    int32_t flags = gmpi_gui_api::GG_POINTER_FLAG_INCONTACT | gmpi_gui_api::GG_POINTER_FLAG_PRIMARY | gmpi_gui_api::GG_POINTER_FLAG_CONFIDENCE;
    flags |= gmpi_gui_api::GG_POINTER_FLAG_FIRSTBUTTON;
    
    ApplyKeyModifiers(flags, theEvent);
    
 // pass to ContainerView    drawingFrame.getView()->onPointerMove(flags, p);
    client.onPointerMove(flags, p);
}

// Opt-in to notification that mouse entered window.
- (void)viewDidMoveToWindow {

//    [self addTrackingArea: [[NSTrackingArea alloc] initWithRect:NSZeroRect options:(NSTrackingMouseEnteredAndExited | NSTrackingInVisibleRect | NSTrackingMouseMoved| NSTrackingActiveAlways) owner:self userInfo:nil]];

    // trackingRect is an NSTrackingRectTag instance variable
    // eyeBox is a region of the view (instance variable)
 //   trackingRect = [self addTrackingRect:eyeBox owner:self userData:NULL assumeInside:NO];
}

- (void)mouseEntered:(NSEvent *)theEvent {
    [[self window] setAcceptsMouseMovedEvents:YES];
    [[self window] makeFirstResponder:self];
    
 //test   [[self window] setColorSpace:[NSColorSpace sRGBColorSpace] ];
}

- (void)mouseMoved:(NSEvent *)theEvent {
    NSPoint localPoint = [self convertPoint: [theEvent locationInWindow] fromView: nil];

    GmpiDrawing::Point p(localPoint.x, localPoint.y);
    
    int32_t flags = gmpi_gui_api::GG_POINTER_FLAG_INCONTACT | gmpi_gui_api::GG_POINTER_FLAG_PRIMARY | gmpi_gui_api::GG_POINTER_FLAG_CONFIDENCE;
    flags |= gmpi_gui_api::GG_POINTER_FLAG_FIRSTBUTTON;
    
    ApplyKeyModifiers(flags, theEvent);
    
 // pass to ContainerView    drawingFrame.getView()->onPointerMove(flags, p);
    client.onPointerMove(flags, p);
}

- (void)mouseExited:(NSEvent *)theEvent {
    [[self window] setAcceptsMouseMovedEvents:NO];
}

@end
