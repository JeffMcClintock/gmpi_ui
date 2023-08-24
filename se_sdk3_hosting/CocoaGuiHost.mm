//
//  CocoaGuiHost.m
//  Standalone
//
//  Created by Jenkins on 22/09/17.
//  Copyright © 2017 Jenkins. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "EventHelper.h"

@implementation SYNTHEDIT_EVENT_HELPER_CLASSNAME

- (void)initWithClient:(EventHelperClient*)pclient
{
    client = pclient;
}

- (void)menuItemSelected: (id) sender
{
 //   drawingFrame.onMenuItemSelected(sender);
    client->CallbackFromCocoa(sender);
}

- (void)endEditing: (id) sender
{

    client->CallbackFromCocoa(sender);
}
/*
- (void)resignFirstResponder: (id) sender
{
    client->CallbackFromCocoa(sender);
}
*/
- (void)onMenuAction: (id) sender
{
   client->CallbackFromCocoa(sender);
}

/* called only on <ret>, seems to PREVENT closing somehow
- (BOOL) textShouldEndEditing:(id) sender
{
    return YES;
}
*/
/* called only on <ret> like 'endEditing'
- (void)controlTextDidEndEditing: (id) sender
{
    client->CallbackFromCocoa(sender);
}
 */
 
@end
