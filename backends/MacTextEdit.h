#ifndef GMPI_MAC_TEXTEDIT_H
#define GMPI_MAC_TEXTEDIT_H

// Single-header (stb-style) gmpi::api::ITextEdit implementation for macOS.
// Define GMPI_MAC_TEXTEDIT_IMPLEMENTATION in exactly one .mm per binary
// before including, to emit the Obj-C @implementation blocks.

#import <Cocoa/Cocoa.h>
#include <string>
#include "GmpiSdkCommon.h"
#include "helpers/NativeUi.h"

struct EventHelperClient
{
    virtual void CallbackFromCocoa(NSObject* sender) = 0;
    virtual void CancelFromCocoa() {}
};

@interface GMPI_EVENT_HELPER_CLASSNAME_03 : NSObject <NSTextFieldDelegate> {
    EventHelperClient* client;
}
- (void)initWithClient:(EventHelperClient*)client;
- (void)menuItemSelected:(id)sender;
- (void)endEditing:(id)sender;
- (void)cancelEditing:(id)sender;
- (void)textDidChange:(NSNotification*)notification;
@end

// NSTextField subclass that forwards Escape to the event helper.
@interface GMPI_EscapableTextField : NSTextField
{
    BOOL multilineMode;
}
@property (assign) BOOL multilineMode;
@end

inline NSRect gmpiRectToViewRect(NSRect viewbounds, gmpi::drawing::Rect const* rect)
{
#if USE_BACKING_BUFFER
    return NSMakeRect(
        rect->left,
        viewbounds.origin.y + viewbounds.size.height - rect->bottom,
        rect->right - rect->left,
        rect->bottom - rect->top
    );
#else
    return NSMakeRect(rect->left, rect->top, rect->right - rect->left, rect->bottom - rect->top);
#endif
}

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
        multiline = (palignment & (int32_t)gmpi::api::TextMultilineFlag::MultiLine) != 0;
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

        // Self-extend lifetime: dismissTextField runs inside a Cocoa callback, and a
        // caller may release its ref from inside onComplete. Without this we'd unwind
        // through dismissTextField on a destroyed object.
        addRef();

        return showNativeTextField();
    }

private:
    void releaseMyselfAsync()
    {
        dispatch_async(dispatch_get_main_queue(), ^{
            this->release();
        });
    }

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
        ((GMPI_EscapableTextField*)textField).multilineMode = multiline;

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
        [textField setDelegate:eventHelper];

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

        if (!callback)
            return;

        // Push final text before completion. Cocoa's textDidChange already pushes per
        // keystroke, but endEditing fires without a preceding textDidChange so the
        // last keystroke can be lost. Explicit onChanged here closes that gap.
        if (result == gmpi::ReturnCode::Ok)
            callback->onChanged(text.c_str());

        callback->onComplete(result);
        callback = {};
        releaseMyselfAsync();
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

#ifdef GMPI_MAC_TEXTEDIT_IMPLEMENTATION

@implementation GMPI_EVENT_HELPER_CLASSNAME_03

- (void)initWithClient:(EventHelperClient*)pclient
{
    client = pclient;
}
- (void)menuItemSelected:(id)sender
{
    client->CallbackFromCocoa(sender);
}
- (void)endEditing:(id)sender
{
    client->CallbackFromCocoa(sender);
}
- (void)cancelEditing:(id)sender
{
    client->CancelFromCocoa();
}
- (void)textDidChange:(NSNotification*)notification
{
    client->CallbackFromCocoa(nil); // nil sender signals a live change, not completion
}
- (void)onMenuAction:(id)sender
{
    client->CallbackFromCocoa(sender);
}
// NSTextFieldDelegate: intercept Return so multiline fields insert a newline
// instead of firing the action (which would dismiss the editor).
- (BOOL)control:(NSControl*)control textView:(NSTextView*)textView doCommandBySelector:(SEL)commandSelector
{
    if (commandSelector == @selector(insertNewline:)
        && [control isKindOfClass:[GMPI_EscapableTextField class]]
        && ((GMPI_EscapableTextField*)control).multilineMode)
    {
        [textView insertNewlineIgnoringFieldEditor:self];
        return YES;
    }
    return NO;
}
@end

@implementation GMPI_EscapableTextField

@synthesize multilineMode;

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

#endif // GMPI_MAC_TEXTEDIT_IMPLEMENTATION

#endif // GMPI_MAC_TEXTEDIT_H
