#ifndef DrawingFrameCocoa_h
#define DrawingFrameCocoa_h

#pragma once

#import <Cocoa/Cocoa.h>
#include "mp_sdk_gui2.h"
#include "CocoaGuiHost.h"
#include "ContainerView.h"
//#import <AudioUnit/AUCocoaUIView.h>
//#include "../version.h"
//#include "CocoaNamespaceMacros.h"

/*
//#ifndef SMTG_AU_NAMESPACE
//# error define SMTG_AU_NAMESPACE
#endif

//-----------------------------------------------------------------------------
#define SMTG_AU_PLUGIN_NAMESPACE0(x) x
#define SMTG_AU_PLUGIN_NAMESPACE1(a, b) a##_##b
#define SMTG_AU_PLUGIN_NAMESPACE2(a, b) SMTG_AU_PLUGIN_NAMESPACE1(a,b)
#define SMTG_AU_PLUGIN_NAMESPACE(name) SMTG_AU_PLUGIN_NAMESPACE2(SMTG_AU_PLUGIN_NAMESPACE0(name), SMTG_AU_PLUGIN_NAMESPACE0(SMTG_AU_NAMESPACE))
*/
//-----------------------------------------------------------------------------
// SMTG_AU_PLUGIN_NAMESPACE (SMTGAUPluginCocoaView)
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
//@interface SYNTHEDIT_PLUGIN_COCOA_VIEW_CLASSNAME : NSObject <AUCocoaUIBase>
//{
//}

//-----------------------------------------------------------------------------
//@end

class DrawingFrameCocoa : public gmpi_gui::IMpGraphicsHost, public gmpi::IMpUserInterfaceHost2, public GmpiGuiHosting::PlatformTextEntryObserver
{
    gmpi_sdk::mp_shared_ptr<SynthEdit2::ContainerView> containerView;
    IGuiHost2* controller;
    int32_t mouseCaptured = 0;
    GmpiGuiHosting::PlatformTextEntry* currentTextEdit = nullptr;
    
public:
    gmpi::cocoa::DrawingFactory drawingFactory;
    NSView* view;

    void Init(SynthEdit2::IPresenter* presenter, class IGuiHost2* hostPatchManager, int pviewType)
    {
        controller = hostPatchManager;
        
        const int topViewBounds = 8000; // simply a large enough size.
        containerView.Attach(new SynthEdit2::ContainerView(GmpiDrawing::Size(topViewBounds,topViewBounds)));
        containerView->setHost(static_cast<gmpi_gui::IMpGraphicsHost*>(this));
        
        containerView->setDocument(presenter, pviewType);
 /* TODO!!!

        dynamic_cast<SEInstrumentBase*>(controller)->callbackOnUnloadPlugin = [this]
        {
            containerView = nullptr; // free all objects early to avoid dangling pointers to AudioUnit.
            controller = nullptr; // ensure we don't reference in in destructor.
        };
  */
     }
    
    ~DrawingFrameCocoa()
    {
        /* TODO!!!
        auto audioUnit = dynamic_cast<SEInstrumentBase*>(controller);
        if(audioUnit)
        {
            audioUnit->callbackOnUnloadPlugin = nullptr;
        }
         */
    }
    
    inline SynthEdit2::ContainerView* getView()
    {
        return containerView.get();
    }
    
    void OnRender(NSView* frame, GmpiDrawing_API::MP1_RECT* dirtyRect)
    {
        gmpi::cocoa::GraphicsContext context(frame, &drawingFactory);
        
        context.PushAxisAlignedClip(dirtyRect);
 
#ifdef _DEBUG
        if(!containerView)
        {
            GmpiDrawing::Graphics g(&context);
            auto tf = g.GetFactory().CreateTextFormat();
            auto brush = g.CreateSolidColorBrush(GmpiDrawing::Color::Red);
            GmpiDrawing::Rect r(0, 0, 1000, 100);
            g.DrawTextU("DrawingFrameCocoa: 'containerView' is null", tf, r, brush);
            
            context.PopAxisAlignedClip();
            return;
        }
#endif
        
        containerView->OnRender(static_cast<GmpiDrawing_API::IMpDeviceContext*>(&context));
        
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
        *returnFactory = &drawingFactory;
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

#endif /* SynthEditCocoaView_h */
