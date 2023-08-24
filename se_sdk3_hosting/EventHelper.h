//
//  EventHelper.h
//  Standalone
//
//  Created by Jenkins on 22/09/17.
//  Copyright © 2017 Jenkins. All rights reserved.
//

#ifndef EventHelper_h
#define EventHelper_h

#include "./CocoaNamespaceMacros.h"

//#define SYNTHEDIT_EVENT_HELPER_CLASSNAME SE_PASTE_MACRO4(EventHelper,SE_MAJOR_VERSION ,SE_MINOR_VERSION , SE_BUILD_NUMBER)
#define SYNTHEDIT_EVENT_HELPER_CLASSNAME SE_MAKE_CLASSNAME(CocoaEventHelper)


struct EventHelperClient
{
    virtual void CallbackFromCocoa(NSObject* sender) = 0;
};


@interface SYNTHEDIT_EVENT_HELPER_CLASSNAME : NSObject {
    EventHelperClient* client;
    
}

- (void)initWithClient:(EventHelperClient*)client;
- (void)menuItemSelected: (id) sender;
- (void)endEditing: (id) sender;

@end

#endif /* EventHelper_h */
