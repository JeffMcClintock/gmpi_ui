#include <TargetConditionals.h>

// Called from gmpi::open_url() declared in helpers/openurl.h
//
// Both Apple platforms can open a URL; they just disagree about which
// framework does it. AppKit does not exist on iOS, so the split is here (see
// the companion comment in browseto.mm, TideSynth BACKLOG D3).
#if TARGET_OS_OSX

#import <AppKit/AppKit.h>

void open_url_impl(const char* utf8_url)
{
    @autoreleasepool {
        NSString* urlString = [NSString stringWithUTF8String:utf8_url];
        NSURL* url = [NSURL URLWithString:urlString];
        if (url) {
            [[NSWorkspace sharedWorkspace] openURL:url];
        }
    }
}

#else

#import <UIKit/UIKit.h>

void open_url_impl(const char* utf8_url)
{
    @autoreleasepool {
        NSString* urlString = [NSString stringWithUTF8String:utf8_url];
        NSURL* url = [NSURL URLWithString:urlString];
        if (url) {
            // Asynchronous and permission-checked on iOS; the completion
            // handler is deliberately nil — callers here are fire-and-forget
            // menu items (help, donation links).
            [[UIApplication sharedApplication] openURL:url options:@{} completionHandler:nil];
        }
    }
}

#endif
