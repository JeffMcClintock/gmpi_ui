#include <TargetConditionals.h>

// Called from gmpi::browse_to() declared in helpers/browseto.h
//
// "Reveal in the file browser" is a desktop idea. macOS has Finder and
// NSWorkspace; iOS has neither, and AppKit does not exist there at all — an
// unconditional #import <AppKit/AppKit.h> is why an iOS build of EditorLib
// failed to COMPILE before it could reach any sandbox question (TideSynth
// BACKLOG D3). The platform split lives here rather than in each consumer's
// CMake so the TU compiles everywhere and the call sites (SkinMgr::EditSkin,
// MfcDocPresenter's skin-folder command) keep linking on every Apple target.
#if TARGET_OS_OSX

#import <AppKit/AppKit.h>

void browse_to_impl(const char* utf8_path)
{
    @autoreleasepool {
        NSString* path = [NSString stringWithUTF8String:utf8_path];
        NSURL* url = [NSURL fileURLWithPath:path];
        [[NSWorkspace sharedWorkspace] activateFileViewerSelectingURLs:@[url]];
    }
}

#else

// iOS and the rest: no file browser to reveal anything in, so this is a
// deliberate no-op rather than a stub that pretends to work. It also keeps
// activateFileViewerSelectingURLs: — a sandbox-hostile API — out of non-macOS
// builds entirely, which is what BACKLOG D4 was ultimately after.
void browse_to_impl(const char*)
{
}

#endif
