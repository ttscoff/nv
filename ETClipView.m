//
//  ETClipView.m
//  nvALT
//
//  Created by elasticthreads on 8/15/11.
//

#import "ETClipView.h"
#import "GlobalPrefs.h"
#import "AppController.h"
#import "LinkingEditor.h"
#import "ETScrollView.h"
#import "NVTheme.h"


@implementation ETClipView

- (id)initWithFrame:(NSRect)frameRect{
    self = [super initWithFrame:frameRect];
    if (self) {
        managesTextWidth=[[GlobalPrefs defaultPrefs] managesTextWidthInWindow];
        [[GlobalPrefs defaultPrefs] registerForSettingChange:@selector(setMaxNoteBodyWidth:sender:) withTarget:self];
        [[GlobalPrefs defaultPrefs] registerForSettingChange:@selector(setManagesTextWidthInWindow:sender:) withTarget:self];
    }
    return self;
}

//
-(void)setFrame:(NSRect)frameRect{
    if (managesTextWidth||([[NSApp delegate]isInFullScreen])) {
        if (frameRect.size.width!=[self frame].size.width) {
            NSRect clipRect;
            NSRect docRect = [[self documentView] frame];
            if ([self clipRect:&clipRect forFrameRect:frameRect]) {
                frameRect=clipRect;
                //                docRect.origin.x=0.0;
                docRect.size.width=frameRect.size.width;
                //                [[self documentView] setFrame:docRect];
                [[self documentView] setConstrainedFrameSize:docRect.size];
            }else if (docRect.size.width>[self frame].size.width){
                //                NSLog(@"problem");
                docRect.size.width=[self frame].size.width;
                [[self documentView] setConstrainedFrameSize:docRect.size];
            }
        }
    }
    [super setFrame:frameRect];
    NSScrollView *scrollView = [self enclosingScrollView];
    if ([scrollView respondsToSelector:@selector(nv_showWindowChromeMargins)])
        [(ETScrollView *)scrollView nv_showWindowChromeMargins];
}

- (BOOL)clipRect:(NSRect *)clipRect forFrameRect:(NSRect)frameRect{
    CGFloat theMax=[[GlobalPrefs defaultPrefs] maxNoteBodyWidth]+(kTextMargins*2);
    if (frameRect.size.width>=(theMax+(kTextMargins/5))){
        CGFloat diff = fabs(frameRect.size.width-theMax);
        diff=round(diff/2);
        frameRect.origin.x=diff;
        frameRect.size.width=theMax;
        *clipRect=frameRect;
        return YES;
    }
    return NO;
}

- (BOOL)clipWidthSettingChanged:(NSRect)frameRect{
    NSRect clipRect;
    NSRect docRect = [[self documentView] frame];
    if ([self clipRect:&clipRect forFrameRect:frameRect]) {
        frameRect=clipRect;
        docRect.size.width=frameRect.size.width;
        [[self documentView] setConstrainedFrameSize:docRect.size];
        [super setFrame:frameRect];
        NSScrollView *scrollView = [self enclosingScrollView];
        if ([scrollView respondsToSelector:@selector(nv_showWindowChromeMargins)])
            [(ETScrollView *)scrollView nv_showWindowChromeMargins];
        return YES;
    }else{
        NSScrollView *scrollV=[[self documentView] enclosingScrollView];
        CGFloat numba=scrollV.frame.size.width;
        if (![[scrollV verticalScroller] isHidden]) {
            numba-=[[scrollV verticalScroller]frame].size.width;
        }
        if (numba>frameRect.size.width) {
            frameRect.size.width=numba;
            [self setFrame:frameRect];
            return YES;
        }   
    }
    return NO;
}

- (void)settingChangedForSelectorString:(NSString*)selectorString{
    if (([selectorString isEqualToString:SEL_STR(setMaxNoteBodyWidth:sender:)])||([selectorString isEqualToString:SEL_STR(setManagesTextWidthInWindow:sender:)])){
        if ([selectorString isEqualToString:SEL_STR(setManagesTextWidthInWindow:sender:)]) {
            managesTextWidth=[[GlobalPrefs defaultPrefs] managesTextWidthInWindow];
            [[self documentView] setManagesTextWidth:managesTextWidth];
        }
        if (!managesTextWidth){
            [[self documentView]resetInset];
        }else if(![self clipWidthSettingChanged:[self frame]]) {
            [[self documentView]updateInsetAndForceLayout:YES];
        }
    }
}

@end
