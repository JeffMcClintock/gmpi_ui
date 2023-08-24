#pragma once

#ifndef SynthEditCocoaView_h
#define SynthEditCocoaView_h

#import <Cocoa/Cocoa.h>

#if !GMPI_IS_PLATFORM_JUCE && !defined(SE_TARGET_VST3)
#import <AudioUnit/AUCocoaUIView.h>
#endif

#include "CocoaNamespaceMacros.h"

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

#if !GMPI_IS_PLATFORM_JUCE && !defined(SE_TARGET_VST3)

//-----------------------------------------------------------------------------
@interface SYNTHEDIT_PLUGIN_COCOA_VIEW_CLASSNAME : NSObject <AUCocoaUIBase>
{
}

//-----------------------------------------------------------------------------
@end
#endif

#endif /* SynthEditCocoaView_h */
