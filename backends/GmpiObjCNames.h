#pragma once
//
// Per-plugin Objective-C class names.
//
// THE PROBLEM. The Objective-C runtime has ONE FLAT, PROCESS-WIDE CLASS
// NAMESPACE. Two plugins in one host that export the same class name do not
// get one each -- whichever loads first wins, and every later one silently
// gets the first one's implementation. macOS says so and then carries on:
//
//   objc[59374]: Class CocoaEventHelper_... is implemented in both
//   .../DistrActioN.component/... and .../PD303.component/... . This may
//   cause spurious casting failures and mysterious crashes. One of the
//   duplicates must be removed or renamed.
//
// The version suffixes already on these names (_01, _03, _04) fix collisions
// between VERSIONS of this SDK. They can never fix collisions between two
// PLUGINS built from the same version -- which is the common case, and the
// case a user hits by installing two GMPI plugins.
//
// THE REMEDY, AND WHY THIS SHAPE. SynthEdit's own AU plugins already solve
// this by suffixing every class with a per-plugin GUID, and it works: two
// plugins with distinct GUIDs load silently. But it is applied by patching
// the GUID into the built binary, over a placeholder string, and THAT FAILS
// OPEN -- when the patch step does not run the placeholder ships as-is. On
// the machine where this was written, 6 of 25 installed components carried
// the literal, unpatched placeholder GUID_GOES_HERE_PLUS_SOME_MORE_CHAR and
// therefore collided with each other exactly as if nothing had been done.
//
// So this does it at COMPILE time instead, where it cannot fail open: the
// suffix is pasted into the identifier by the preprocessor, and a build that
// does not set GMPI_OBJC_SUFFIX gets a name that is still obviously
// unsuffixed rather than one that looks fixed and is not.
//
// USE GMPI_OBJC_NAME FOR THE CLASS AND GMPI_OBJC_NAME_STR FOR ANY STRING
// NAMING IT. Both derive from the same macro, so they cannot drift. This
// matters: the AU2 wrapper hands the host its view class name as a C string
// literal, and a rename that missed it would leave the plugin advertising a
// class that no longer exists -- a GUI that silently fails to open, with no
// warning from the runtime at all.
//
#ifndef GMPI_OBJC_SUFFIX
    // No per-plugin suffix was configured. Deliberately NOT a no-op: a build
    // in this state still collides with every other build in this state, and
    // the name says so rather than looking like a name that was made unique.
    // gmpi_target() in gmpi_plugin.cmake sets this for every GMPI plugin.
    #define GMPI_OBJC_SUFFIX _NO_PLUGIN_SUFFIX_SET
#endif

#define GMPI_OBJC_PASTE2(a, b) a##b
#define GMPI_OBJC_PASTE(a, b)  GMPI_OBJC_PASTE2(a, b)

// The class name itself: GMPI_OBJC_NAME(Foo) -> Foo_MyPlugin
#define GMPI_OBJC_NAME(base)   GMPI_OBJC_PASTE(base, GMPI_OBJC_SUFFIX)

// Stringify needs the SAME two-level indirection as the paste: #x stringifies
// its argument UNEXPANDED, so a one-level version would yield the literal text
// "GMPI_OBJC_NAME(Foo)" rather than the pasted name. That is precisely the
// drift this header exists to prevent, so it is spelled out.
#define GMPI_OBJC_STR2(x) #x
#define GMPI_OBJC_STR(x)  GMPI_OBJC_STR2(x)

// A string of the SAME name, for the one API that takes it as text.
#define GMPI_OBJC_NAME_STR(base) GMPI_OBJC_STR(GMPI_OBJC_NAME(base))
