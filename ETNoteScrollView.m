//
//  ETNoteScrollView.m
//  Notation
//
//  Created by David Halter on 10/3/16.
//  Copyright © 2016 David Halter. All rights reserved.
//

#import "ETNoteScrollView.h"
#import "LinkingEditor.h"

@implementation ETNoteScrollView

//- (void)awakeFromNib{
//}


- (NSView *)hitTest:(NSPoint)aPoint{
    NSRect vsRect=[[self verticalScroller] frame];
    vsRect.origin.x-=4.0;
    vsRect.size.width+=4.0;

    if (NSPointInRect (aPoint,vsRect)) {
        return [self verticalScroller];
    }else if (IsLionOrLater){
        NSView *tView=[super hitTest:aPoint];
        BOOL tViewIsDoc=(tView==self.documentView);
        if (tViewIsDoc||[tView isKindOfClass:self.class]||[tView isKindOfClass:NSClassFromString(@"ETClipView")]){
            [self.documentView setMouseInside:YES];
            return self.documentView;
        }
        return tView;
    }

    return [super hitTest:aPoint];
}


@end
