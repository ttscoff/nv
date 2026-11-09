//
//  ETScrollView.m
//  Notation
//
//  Created by elasticthreads on 3/14/11.
//

#import <Foundation/Foundation.h>



@interface ETScrollView : NSScrollView {
    BOOL updatingScrollerChrome;
    BOOL installingChromeMargin;
    NSView *nvChromeMargin;
}

- (void)nv_updateScrollerChrome;
- (void)nv_showWindowChromeMargins;

@end
