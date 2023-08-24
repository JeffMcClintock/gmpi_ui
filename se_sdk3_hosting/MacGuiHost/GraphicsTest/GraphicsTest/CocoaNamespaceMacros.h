//#include "../version.h"

//If you want to stringify the result of expansion of a macro argument, you have to use two levels of macros.
#define SE_STRINGIFY(s) SE_STRINGIFY_LEVEL2(s)
#define SE_STRINGIFY_LEVEL2(s) #s
#define SE_PASTE_MACRO4_LEVEL2(a,b,c,d) a##b##c##d
#define SE_PASTE_MACRO4(a,b,c,d) SE_PASTE_MACRO4_LEVEL2(a,b,c,d)
#define SE_PASTE_MACRO2_LEVEL2(a,b) a##b
#define SE_PASTE_MACRO2(a,b) SE_PASTE_MACRO2_LEVEL2(a,b)

#define SE_MAKE_CLASSNAME(x) SE_PASTE_MACRO2(x, _GUID_GOES_HERE_PLUS_SOME_MORE_CHARTR)
#define SYNTHEDIT_PLUGIN_COCOA_VIEW_CLASSNAME SE_MAKE_CLASSNAME(SynthEditPluginCocoaView)


