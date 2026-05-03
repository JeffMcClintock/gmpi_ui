#import <AudioUnit/AudioUnit.h>
#import <AudioUnit/AUCocoaUIView.h>
#import <Cocoa/Cocoa.h>
#include "GmpiSdkCommon.h"
#include "GmpiApiEditor.h"
#import "CocoaGfx.h"
#include "DrawingFrameCommon.h"
#include "DrawingFrameMac.h"
#import "helpers/IController.h"
#include <algorithm>
#include <array>
#include <string>
#include <string_view>

struct EventHelperClient
{
    virtual void CallbackFromCocoa(NSObject* sender) = 0;
    virtual void CancelFromCocoa() {}
};

@interface GMPI_EVENT_HELPER_CLASSNAME_03 : NSObject {
    EventHelperClient* client;
}
- (void)initWithClient:(EventHelperClient*)client;
- (void)menuItemSelected: (id) sender;
- (void)endEditing: (id) sender;
- (void)cancelEditing: (id) sender;
- (void)textDidChange:(NSNotification*)notification;
@end

@implementation GMPI_EVENT_HELPER_CLASSNAME_03

- (void)initWithClient:(EventHelperClient*)pclient
{
    client = pclient;
}
- (void)menuItemSelected: (id) sender
{
    client->CallbackFromCocoa(sender);
}
- (void)endEditing: (id) sender
{
    client->CallbackFromCocoa(sender);
}
- (void)cancelEditing: (id) sender
{
    client->CancelFromCocoa();
}
- (void)textDidChange:(NSNotification*)notification
{
    client->CallbackFromCocoa(nil); // nil sender signals a live change, not completion
}
- (void)onMenuAction: (id) sender
{
   client->CallbackFromCocoa(sender);
}
@end

// NSTextField subclass that forwards Escape to the event helper.
@interface GMPI_EscapableTextField : NSTextField
@end

@implementation GMPI_EscapableTextField

- (void)cancelOperation:(id)sender
{
    if (self.target && [self.target respondsToSelector:@selector(cancelEditing:)])
        [self.target performSelector:@selector(cancelEditing:) withObject:self];
}

// Intercept editing keys (Delete, arrows, etc.) so the menu key-equivalent
// system doesn't claim them (e.g. Edit > Delete deleting document objects).
- (BOOL)performKeyEquivalent:(NSEvent*)event
{
    if (self.currentEditor)
    {
        [self.currentEditor interpretKeyEvents:@[event]];
        return YES;
    }
    return [super performKeyEquivalent:event];
}

@end

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

inline NSRect gmpiRectToViewRect(NSRect viewbounds, gmpi::drawing::Rect const* rect)
{
    #if USE_BACKING_BUFFER
        // flip co-ords
        return NSMakeRect(
          rect->left,
          viewbounds.origin.y + viewbounds.size.height - rect->bottom,
          rect->right - rect->left,
          rect->bottom - rect->top
          );
    #else
        return NSMakeRect(rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top);
    #endif
}

// ─── GMPI_MAC_TextEdit ─────────────────────────────────────────────────────
// Implements gmpi::api::ITextEdit by wrapping an NSTextField.

class GMPI_MAC_TextEdit : public gmpi::api::ITextEdit, public EventHelperClient
{
    NSView* parentView;
    NSTextField* textField = nil;
    GMPI_EVENT_HELPER_CLASSNAME_03* eventHelper = nil;
    gmpi::drawing::Rect editRect;
    std::string text;
    float textHeight = 12.0f;
    int32_t alignment = 0;
    bool multiline = false;
    gmpi::shared_ptr<gmpi::api::ITextEditCallback> callback;

public:
    GMPI_MAC_TextEdit(NSView* pview, gmpi::drawing::Rect rect)
        : parentView(pview)
        , editRect(rect)
    {
        eventHelper = [GMPI_EVENT_HELPER_CLASSNAME_03 alloc];
        [eventHelper initWithClient:this];
    }

    ~GMPI_MAC_TextEdit()
    {
        if (textField)
        {
            [textField removeFromSuperview];
            textField = nil;
        }
    }

    gmpi::ReturnCode setText(const char* ptext) override
    {
        text = ptext;
        return gmpi::ReturnCode::Ok;
    }

    gmpi::ReturnCode setAlignment(int32_t palignment) override
    {
        alignment = palignment & 0x03;
        multiline = (palignment >> 16) == 1;
        return gmpi::ReturnCode::Ok;
    }

    gmpi::ReturnCode setTextSize(float height) override
    {
        textHeight = height;
        return gmpi::ReturnCode::Ok;
    }

    gmpi::ReturnCode showAsync(gmpi::api::IUnknown* pcallback) override
    {
        pcallback->queryInterface(&gmpi::api::ITextEditCallback::guid, callback.put_void());
        return showNativeTextField();
    }

private:
    gmpi::ReturnCode showNativeTextField()
    {
        if (textField)
        {
            [textField removeFromSuperview];
            textField = nil;
        }

        textField = [[GMPI_EscapableTextField alloc] initWithFrame:gmpiRectToViewRect(parentView.bounds, &editRect)];
        [textField setFont:[NSFont systemFontOfSize:textHeight]];

        NSString* nsstr = [NSString stringWithCString:text.c_str() encoding:NSUTF8StringEncoding];
        [textField setStringValue:nsstr];

        textField.bezeled = false;
        textField.drawsBackground = true;
        [textField setBackgroundColor:[NSColor textBackgroundColor]];
        textField.usesSingleLineMode = !multiline;

        switch (alignment)
        {
        case 1: // center
            textField.alignment = NSTextAlignmentCenter;
            break;
        case 2: // trailing
            textField.alignment = NSTextAlignmentRight;
            break;
        default: // leading
            break;
        }

        [textField setTarget:eventHelper];
        [textField setAction:@selector(endEditing:)];

        [[NSNotificationCenter defaultCenter]
            addObserver:eventHelper
               selector:@selector(textDidChange:)
                   name:NSControlTextDidChangeNotification
                 object:textField];

        [[NSNotificationCenter defaultCenter]
            addObserver:eventHelper
               selector:@selector(endEditing:)
                   name:NSControlTextDidEndEditingNotification
                 object:textField];

        [parentView addSubview:textField];
        [[parentView window] makeFirstResponder:textField];

        return gmpi::ReturnCode::Ok;
    }

public:
    void CallbackFromCocoa(NSObject* sender) override
    {
        if (!textField)
            return;

        text = [[textField stringValue] UTF8String];

        if (!sender)
        {
            if (callback)
                callback->onChanged(text.c_str());
            return;
        }

        dismissTextField(gmpi::ReturnCode::Ok);
    }

    void CancelFromCocoa() override
    {
        if (!textField)
            return;

        dismissTextField(gmpi::ReturnCode::Cancel);
    }

private:
    void dismissTextField(gmpi::ReturnCode result)
    {
        [[NSNotificationCenter defaultCenter] removeObserver:eventHelper
                                                        name:NSControlTextDidChangeNotification
                                                      object:textField];
        [[NSNotificationCenter defaultCenter] removeObserver:eventHelper
                                                        name:NSControlTextDidEndEditingNotification
                                                      object:textField];
        [textField removeFromSuperview];
        textField = nil;

        if (callback)
            callback->onComplete(result);
    }

public:
    gmpi::ReturnCode queryInterface(const gmpi::api::Guid* iid, void** returnInterface) override
    {
        *returnInterface = {};
        GMPI_QUERYINTERFACE(gmpi::api::ITextEdit);
        return gmpi::ReturnCode::NoSupport;
    }
    GMPI_REFCOUNT;
};

// ─── GMPI_MAC_PopupMenu ──────────────────────────────────────────────────

class GMPI_MAC_PopupMenu : public gmpi::api::IPopupMenu, public EventHelperClient
{
    struct MenuCallback
    {
        int32_t localId{};
        gmpi::shared_ptr<gmpi::api::IUnknown> callback;
    };

    int32_t selectedId;
    NSView* view;
    std::vector<MenuCallback> callbacks;
    GMPI_EVENT_HELPER_CLASSNAME_03* eventhelper{};
    NSPopUpButton* button;
    
    std::vector<NSMenu*> menuStack;
    gmpi::drawing::Rect rect;
    
public:

    GMPI_MAC_PopupMenu(NSView* pview, gmpi::drawing::Rect prect) :
    view(pview)
    ,rect(prect)
    {
        eventhelper = [GMPI_EVENT_HELPER_CLASSNAME_03 alloc];
        [eventhelper initWithClient : this];
 
        button = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(10,1000,30,30)];
        menuStack.push_back([button menu]);
    }

    ~GMPI_MAC_PopupMenu()
    {
        if (button != nil)
        {
            [button removeFromSuperview];
            button = nil;
        }
    }

    void CallbackFromCocoa(NSObject* sender) override
    {
        int index = static_cast<int>([((NSMenuItem*) sender) tag]) - 1;
        const bool validIndex = index >= 0 && index < static_cast<int>(callbacks.size());
        if (validIndex)
        {
            selectedId = callbacks[index].localId;
        }

        [button removeFromSuperview];

        if (validIndex && callbacks[index].callback)
        {
            if(auto callback = callbacks[index].callback.as<gmpi::api::IPopupMenuCallback>(); callback)
            {
                callback->onComplete(gmpi::ReturnCode::Ok, selectedId);
            }
        }
    }

    gmpi::ReturnCode addItem(const char* text, int32_t id, int32_t flags, gmpi::api::IUnknown* callback) override
    {
        if ((flags & static_cast<int32_t>(gmpi::api::PopupMenuFlags::Separator)) != 0)
        {
            [menuStack.back() addItem:[NSMenuItem separatorItem] ];
        }
        else
        {
            std::string stripped(text);
            stripped.erase(std::remove(stripped.begin(), stripped.end(), '&'), stripped.end());
            NSString* nsstr = [NSString stringWithCString : stripped.c_str() encoding : NSUTF8StringEncoding];

            const bool isSubMenuStart = (flags & static_cast<int32_t>(gmpi::api::PopupMenuFlags::SubMenuBegin)) != 0;
            const bool isSubMenuEnd = (flags & static_cast<int32_t>(gmpi::api::PopupMenuFlags::SubMenuEnd)) != 0;
            if (isSubMenuStart || isSubMenuEnd)
            {
                if (isSubMenuStart)
                {
                    auto menuItem = [menuStack.back() addItemWithTitle:nsstr action : nil keyEquivalent:@""];
                    NSMenu* subMenu = [[NSMenu alloc] init];
                    [menuItem setSubmenu:subMenu];
                    menuStack.push_back(subMenu);
                }
                else if (isSubMenuEnd)
                {
                    menuStack.pop_back();
                }
            }
            else
            {
                gmpi::shared_ptr<gmpi::api::IUnknown> itemcallback(callback);
                callbacks.push_back({ id, itemcallback });

                NSMenuItem* menuItem;
                if ((flags & static_cast<int32_t>(gmpi::api::PopupMenuFlags::Grayed)) != 0)
                {
                    menuItem = [menuStack.back() addItemWithTitle:nsstr action : nil keyEquivalent:@""];
                }
                else
                {
                    menuItem = [menuStack.back() addItemWithTitle:nsstr action : @selector(menuItemSelected : ) keyEquivalent:@""];
                }
                
                [menuItem setTarget : eventhelper];
                [menuItem setTag: callbacks.size()]; // successive tags, starting at 1
                
                if ((flags & static_cast<int32_t>(gmpi::api::PopupMenuFlags::Ticked)) != 0)
                {
                    [menuItem setState:NSControlStateValueOn];
                }
            }
        }
        
        return gmpi::ReturnCode::Ok;
    }

    gmpi::ReturnCode showAsync() override
    {
        [[button cell] setAltersStateOfSelectedItem:NO];
        [[button cell] attachPopUpWithFrame:NSMakeRect(0,0,1,1) inView:view];
        [[button cell] performClickWithFrame:gmpiRectToViewRect(view.bounds, &rect) inView:view];

        return gmpi::ReturnCode::Ok;
    }

    gmpi::ReturnCode setAlignment(int32_t alignment) override
    {
        /*
        switch (alignment)
        {
            case GmpiDrawing_API::MP1_TEXT_ALIGNMENT_LEADING:
                align = TPM_LEFTALIGN;
                break;
            case GmpiDrawing_API::MP1_TEXT_ALIGNMENT_CENTER:
                align = TPM_CENTERALIGN;
                break;
            case GmpiDrawing_API::MP1_TEXT_ALIGNMENT_TRAILING:
            default:
                align = TPM_RIGHTALIGN;
                break;
        }
         */
        return gmpi::ReturnCode::Ok;
    }

    GMPI_QUERYINTERFACE_METHOD(gmpi::api::IPopupMenu);
    GMPI_REFCOUNT;
};

// GMPI_MAC_FileDialog

class GMPI_MAC_FileDialog : public gmpi::api::IFileDialog
{
    NSView* view;
    gmpi::api::FileDialogType dialogType;
    std::vector<std::pair<std::string, std::string>> extensions; // extension, description
    std::string initialFilename;
    std::string initialDirectory;

public:
    GMPI_MAC_FileDialog(NSView* pview, gmpi::api::FileDialogType type) :
        view(pview)
        , dialogType(type)
    {}

    gmpi::ReturnCode addExtension(const char* extension, const char* description) override
    {
        extensions.push_back({ extension ? extension : "", description ? description : "" });
        return gmpi::ReturnCode::Ok;
    }

    gmpi::ReturnCode setInitialFilename(const char* text) override
    {
        initialFilename = text ? text : "";
        return gmpi::ReturnCode::Ok;
    }

    gmpi::ReturnCode setInitialDirectory(const char* text) override
    {
        initialDirectory = text ? text : "";
        return gmpi::ReturnCode::Ok;
    }

    gmpi::ReturnCode showAsync(const gmpi::drawing::Rect* rect, gmpi::api::IUnknown* callback) override
    {
        gmpi::shared_ptr<gmpi::api::IUnknown> unknown(callback);

        auto fileCallback = unknown.as<gmpi::api::IFileDialogCallback>();
        if (!fileCallback)
            return gmpi::ReturnCode::Fail;

        if (dialogType == gmpi::api::FileDialogType::Folder)
        {
            return showFolderDialog(fileCallback.get());
        }

        const bool isSave = (dialogType == gmpi::api::FileDialogType::Save);

        if (isSave)
        {
            NSSavePanel* panel = [NSSavePanel savePanel];

            if (!initialDirectory.empty())
            {
                NSString* dir = [NSString stringWithUTF8String:initialDirectory.c_str()];
                [panel setDirectoryURL:[NSURL fileURLWithPath:dir]];
            }

            if (!initialFilename.empty())
            {
                NSString* filename = [NSString stringWithUTF8String:initialFilename.c_str()];
                [panel setNameFieldStringValue:filename];
            }

            if (!extensions.empty())
            {
                NSMutableArray* types = [NSMutableArray array];
                for (const auto& [ext, desc] : extensions)
                {
                    if (ext != "*")
                        [types addObject:[NSString stringWithUTF8String:ext.c_str()]];
                }
                if ([types count] > 0)
                    [panel setAllowedFileTypes:types];
            }

            // prevent the callback getting destroyed until after the completion handler runs
            auto prevent_release = fileCallback;
            [panel beginSheetModalForWindow:[view window] completionHandler:^(NSModalResponse result) {
                if (result == NSModalResponseOK)
                {
                    const char* path = [[panel URL] fileSystemRepresentation];
                    prevent_release->onComplete(gmpi::ReturnCode::Ok, path);
                }
                else
                {
                    prevent_release->onComplete(gmpi::ReturnCode::Cancel, "");
                }
            }];
        }
        else
        {
            NSOpenPanel* panel = [NSOpenPanel openPanel];
            [panel setCanChooseFiles:YES];
            [panel setCanChooseDirectories:NO];
            [panel setAllowsMultipleSelection:NO];

            if (!initialDirectory.empty())
            {
                NSString* dir = [NSString stringWithUTF8String:initialDirectory.c_str()];
                [panel setDirectoryURL:[NSURL fileURLWithPath:dir]];
            }

            if (!extensions.empty())
            {
                NSMutableArray* types = [NSMutableArray array];
                for (const auto& [ext, desc] : extensions)
                {
                    if (ext != "*")
                        [types addObject:[NSString stringWithUTF8String:ext.c_str()]];
                }
                if ([types count] > 0)
                    [panel setAllowedFileTypes:types];
            }

            auto prevent_release = fileCallback;
            [panel beginSheetModalForWindow:[view window] completionHandler:^(NSModalResponse result) {
                if (result == NSModalResponseOK)
                {
                    const char* path = [[[panel URLs] firstObject] fileSystemRepresentation];
                    prevent_release->onComplete(gmpi::ReturnCode::Ok, path);
                }
                else
                {
                    prevent_release->onComplete(gmpi::ReturnCode::Cancel, "");
                }
            }];
        }

        return gmpi::ReturnCode::Ok;
    }

    gmpi::ReturnCode queryInterface(const gmpi::api::Guid* iid, void** returnInterface) override
    {
        *returnInterface = {};
        GMPI_QUERYINTERFACE(gmpi::api::IFileDialog);
        return gmpi::ReturnCode::NoSupport;
    }
    GMPI_REFCOUNT;

private:
    gmpi::ReturnCode showFolderDialog(gmpi::api::IFileDialogCallback* fileCallback)
    {
        NSOpenPanel* panel = [NSOpenPanel openPanel];
        [panel setCanChooseFiles:NO];
        [panel setCanChooseDirectories:YES];
        [panel setAllowsMultipleSelection:NO];

        if (!initialDirectory.empty())
        {
            NSString* dir = [NSString stringWithUTF8String:initialDirectory.c_str()];
            [panel setDirectoryURL:[NSURL fileURLWithPath:dir]];
        }

        // prevent the callback getting destroyed until after the completion handler runs
        gmpi::shared_ptr<gmpi::api::IFileDialogCallback> prevent_release(fileCallback);
        [panel beginSheetModalForWindow:[view window] completionHandler:^(NSModalResponse result) {
            if (result == NSModalResponseOK)
            {
                const char* path = [[[panel URLs] firstObject] fileSystemRepresentation];
                prevent_release->onComplete(gmpi::ReturnCode::Ok, path);
            }
            else
            {
                prevent_release->onComplete(gmpi::ReturnCode::Cancel, "");
            }
        }];

        return gmpi::ReturnCode::Ok;
    }
};

// GMPI_MAC_StockDialog

class GMPI_MAC_StockDialog : public gmpi::api::IStockDialog
{
    NSView* view;
    gmpi::api::StockDialogType dialogType;
    std::string title;
    std::string text;

public:
    GMPI_MAC_StockDialog(NSView* pview, gmpi::api::StockDialogType type, const char* ptitle, const char* ptext) :
        view(pview)
        , dialogType(type)
        , title(ptitle ? ptitle : "")
        , text(ptext ? ptext : "")
    {}

    gmpi::ReturnCode showAsync(gmpi::api::IUnknown* callback) override
    {
        gmpi::shared_ptr<gmpi::api::IUnknown> unknown(callback);

        auto dialogCallback = unknown.as<gmpi::api::IStockDialogCallback>();

        if (!dialogCallback)
            return gmpi::ReturnCode::Fail;

        NSAlert* alert = [[NSAlert alloc] init];
        [alert setMessageText:[NSString stringWithUTF8String:title.c_str()]];
        [alert setInformativeText:[NSString stringWithUTF8String:text.c_str()]];

        switch (dialogType)
        {
        case gmpi::api::StockDialogType::Ok:
            [alert addButtonWithTitle:@"OK"];
            break;
        case gmpi::api::StockDialogType::OkCancel:
            [alert addButtonWithTitle:@"OK"];
            [alert addButtonWithTitle:@"Cancel"];
            break;
        case gmpi::api::StockDialogType::YesNo:
            [alert addButtonWithTitle:@"Yes"];
            [alert addButtonWithTitle:@"No"];
            break;
        case gmpi::api::StockDialogType::YesNoCancel:
            [alert addButtonWithTitle:@"Yes"];
            [alert addButtonWithTitle:@"No"];
            [alert addButtonWithTitle:@"Cancel"];
            break;
        }

        auto prevent_release = dialogCallback;
        auto type = dialogType;
        [alert beginSheetModalForWindow:[view window] completionHandler:^(NSModalResponse returnCode) {
            gmpi::api::StockDialogButton button{};
            switch (type)
            {
            case gmpi::api::StockDialogType::Ok:
                button = gmpi::api::StockDialogButton::Ok;
                break;
            case gmpi::api::StockDialogType::OkCancel:
                button = (returnCode == NSAlertFirstButtonReturn)
                    ? gmpi::api::StockDialogButton::Ok
                    : gmpi::api::StockDialogButton::Cancel;
                break;
            case gmpi::api::StockDialogType::YesNo:
                button = (returnCode == NSAlertFirstButtonReturn)
                    ? gmpi::api::StockDialogButton::Yes
                    : gmpi::api::StockDialogButton::No;
                break;
            case gmpi::api::StockDialogType::YesNoCancel:
                if (returnCode == NSAlertFirstButtonReturn)
                    button = gmpi::api::StockDialogButton::Yes;
                else if (returnCode == NSAlertSecondButtonReturn)
                    button = gmpi::api::StockDialogButton::No;
                else
                    button = gmpi::api::StockDialogButton::Cancel;
                break;
            }
            prevent_release->onComplete(button);
        }];

        return gmpi::ReturnCode::Ok;
    }

    gmpi::ReturnCode queryInterface(const gmpi::api::Guid* iid, void** returnInterface) override
    {
        *returnInterface = {};
        GMPI_QUERYINTERFACE(gmpi::api::IStockDialog);
        return gmpi::ReturnCode::NoSupport;
    }
    GMPI_REFCOUNT;
};

// GMPI_MAC_KeyListener

@interface KeyListenerView_03 : NSView
{
    gmpi::api::IKeyListenerCallback* callback;
}

- (id)initWithParent:(NSView*)parent callback:(gmpi::api::IKeyListenerCallback*)listener;
- (void)keyDown:(NSEvent*)event;

@end
/////////////////////////////////////////////
@implementation KeyListenerView_03

- (id)initWithParent:(NSView*)parent callback:(gmpi::api::IKeyListenerCallback*)pcallback
{
    self = [super initWithFrame:NSMakeRect(0, 0, 40, 40)];
    if (self)
    {
        callback = pcallback;
        if (parent)
        {
            [parent addSubview:self];
        }
    }
    return self;
}

- (void)keyDown:(NSEvent*)event
{
    if (callback)
    {
        // Convert NSEvent to a format suitable for gmpi::api::IKeyListener
        // For simplicity, we'll just forward the key code and characters
        NSString* characters = [event characters];
        const char* utf8String = [characters UTF8String];
        int32_t keyCode = [event keyCode];

        // Call the keyListener's method
        callback->onKeyDown(keyCode, 0);
    }
    else
    {
        [super keyDown:event];
    }
}

@end
//////////////////////////////////////////////////////////
GMPI_MAC_KeyListener::GMPI_MAC_KeyListener(NSView* pview, const gmpi::drawing::Rect* r) :
      parentView(pview)
    , bounds(*r)
{}
    
GMPI_MAC_KeyListener::~GMPI_MAC_KeyListener()
{
}

gmpi::ReturnCode GMPI_MAC_KeyListener::showAsync(gmpi::api::IUnknown* callback)
{
    callback->queryInterface(&gmpi::api::IKeyListenerCallback::guid, (void**)&callback2);

    keyListenerView = [[KeyListenerView_03 alloc] initWithParent:parentView callback:callback2];

    return gmpi::ReturnCode::Ok;
}


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
            drawingClient->open(static_cast<gmpi::api::IDrawingHost*>(this));
    }
    
    void open() // called from viewDidMoveToWindow <= createNativeView()
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

//-------------------------------------------------------------------------------------------------------------
#if 1
void gmpi_ApplyKeyModifiers(int32_t& flags, NSEvent* theEvent)
{
    // <Shift> key?
    if(([theEvent modifierFlags ] & (NSEventModifierFlagShift | NSEventModifierFlagCapsLock)) != 0)
    {
        flags |= gmpi::interaction::GG_POINTER_KEY_SHIFT;
    }
    
    if(([theEvent modifierFlags ] & NSEventModifierFlagControl) != 0)
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
    
    gmpi_ApplyKeyModifiers(flags, theEvent);
    const auto p = mouseToGmpi(self, theEvent);

    if(drawingFrame.inputClient)
        drawingFrame.inputClient->onPointerDown(p, flags);

 // no help to edit box   [super mouseDown:theEvent];
}

- (void)rightMouseDown:(NSEvent *)theEvent
{
    drawingFrame.removeTextEdit();

    int32_t flags = gmpi::interaction::GG_POINTER_FLAG_INCONTACT | gmpi::interaction::GG_POINTER_FLAG_PRIMARY | gmpi::interaction::GG_POINTER_FLAG_CONFIDENCE;
    flags |= gmpi::interaction::GG_POINTER_FLAG_NEW;
    flags |= gmpi::interaction::GG_POINTER_FLAG_SECONDBUTTON;
    
    gmpi_ApplyKeyModifiers(flags, theEvent);
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
    int32_t flags = gmpi::interaction::GG_POINTER_FLAG_INCONTACT | gmpi::interaction::GG_POINTER_FLAG_PRIMARY | gmpi::interaction::GG_POINTER_FLAG_CONFIDENCE;
    flags |= gmpi::interaction::GG_POINTER_FLAG_NEW;
    flags |= gmpi::interaction::GG_POINTER_FLAG_SECONDBUTTON;
    
    gmpi_ApplyKeyModifiers(flags, theEvent);
    
    if(drawingFrame.inputClient)
        drawingFrame.inputClient->onPointerUp(mouseToGmpi(self, theEvent), flags);
}

- (void)mouseUp:(NSEvent *)theEvent {
    int32_t flags = gmpi::interaction::GG_POINTER_FLAG_INCONTACT | gmpi::interaction::GG_POINTER_FLAG_PRIMARY | gmpi::interaction::GG_POINTER_FLAG_CONFIDENCE;
    flags |= gmpi::interaction::GG_POINTER_FLAG_FIRSTBUTTON;
    
    gmpi_ApplyKeyModifiers(flags, theEvent);
    
    if(drawingFrame.inputClient)
        drawingFrame.inputClient->onPointerUp(mouseToGmpi(self, theEvent), flags);
}

- (void)mouseDragged:(NSEvent *)theEvent {

    int32_t flags = gmpi::interaction::GG_POINTER_FLAG_INCONTACT | gmpi::interaction::GG_POINTER_FLAG_PRIMARY | gmpi::interaction::GG_POINTER_FLAG_CONFIDENCE;
    flags |= gmpi::interaction::GG_POINTER_FLAG_FIRSTBUTTON;
    
    gmpi_ApplyKeyModifiers(flags, theEvent);
    
    if(drawingFrame.inputClient)
        drawingFrame.inputClient->onPointerMove(mouseToGmpi(self, theEvent), flags);
}

- (void)scrollWheel:(NSEvent *)theEvent {
    // Get the scroll wheel delta
    auto deltaX = theEvent.deltaX;
    auto deltaY = theEvent.deltaY;
 
    int32_t flags = gmpi::interaction::GG_POINTER_FLAG_PRIMARY | gmpi::interaction::GG_POINTER_FLAG_CONFIDENCE;
    gmpi_ApplyKeyModifiers(flags, theEvent);

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
    
    int32_t flags = gmpi::interaction::GG_POINTER_FLAG_INCONTACT | gmpi::interaction::GG_POINTER_FLAG_PRIMARY | gmpi::interaction::GG_POINTER_FLAG_CONFIDENCE;
    flags |= gmpi::interaction::GG_POINTER_FLAG_FIRSTBUTTON;
    
    gmpi_ApplyKeyModifiers(flags, theEvent);
    
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
