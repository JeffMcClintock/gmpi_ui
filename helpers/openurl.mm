#import <AppKit/AppKit.h>

// Called from gmpi::open_url() declared in helpers/openurl.h
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
