#import <AppKit/AppKit.h>

// Called from gmpi::browse_to() declared in helpers/browseto.h
void browse_to_impl(const char* utf8_path)
{
    @autoreleasepool {
        NSString* path = [NSString stringWithUTF8String:utf8_path];
        NSURL* url = [NSURL fileURLWithPath:path];
        [[NSWorkspace sharedWorkspace] activateFileViewerSelectingURLs:@[url]];
    }
}
