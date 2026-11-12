//
//  ETScrollView.m
//  Notation
//
//  Created by elasticthreads on 3/14/11.
//

#import "ETScrollView.h"
#import "NVThemeManager.h"

@interface NVChromeMarginView : NSView
@end

@implementation NVChromeMarginView

- (void)drawRect:(NSRect)dirtyRect {
    NSColor *chrome = NVWindowChromeColor();
    if (!chrome)
        return;
    NSScrollView *scrollView = (NSScrollView *)[self superview];
    NSView *clip = [scrollView isKindOfClass:[NSScrollView class]] ? [scrollView contentView] : nil;
    if (!clip)
        return;
    NSRect column = [clip frame];
    NSRect bounds = [self bounds];
    [chrome setFill];
    if (NSMinX(column) > NSMinX(bounds) + 0.5) {
        NSRect left = NSMakeRect(NSMinX(bounds), NSMinY(bounds), NSMinX(column) - NSMinX(bounds), NSHeight(bounds));
        NSRectFillUsingOperation(NSIntersectionRect(left, dirtyRect), NSCompositingOperationSourceOver);
    }
    if (NSMaxX(column) < NSMaxX(bounds) - 0.5) {
        NSRect right = NSMakeRect(NSMaxX(column), NSMinY(bounds), NSMaxX(bounds) - NSMaxX(column), NSHeight(bounds));
        /* The vertical scroller lives in this strip. Filling it paints the
           window color over the bar a moment after the bar appears. */
        NSScroller *scroller = [scrollView verticalScroller];
        if (scroller && ![scroller isHidden] && NSWidth([scroller frame]) > 1.0) {
            NSRect scrollerRect = [self convertRect:[scroller frame] fromView:scrollView];
            if (NSMinX(scrollerRect) > NSMinX(right) + 0.5)
                right.size.width = NSMinX(scrollerRect) - NSMinX(right);
            else
                right = NSZeroRect;
        }
        if (!NSIsEmptyRect(right))
            NSRectFillUsingOperation(NSIntersectionRect(right, dirtyRect), NSCompositingOperationSourceOver);
    }
}

- (BOOL)isOpaque {
    return NO;
}

- (NSView *)hitTest:(NSPoint)aPoint {
    return nil;
}

@end

@implementation ETScrollView

- (void)awakeFromNib {
    if (IsLionOrLater) {
        [self setHorizontalScrollElasticity:NSScrollElasticityNone];
        [self setVerticalScrollElasticity:NSScrollElasticityAllowed];
    }
    [self nv_updateScrollerChrome];
}

- (void)nv_updateScrollerChrome {
    if (updatingScrollerChrome)
        return;
    updatingScrollerChrome = YES;
    NSColor *pane = nil;
    if ([[self documentView] respondsToSelector:@selector(backgroundColor)])
        pane = [(id)[self documentView] backgroundColor];
    NSColor *chrome = NVWindowChromeColor();
    BOOL lightChrome = !chrome || NVThemeColorIsLight(chrome);
    // Style the scroll view, not the scroller. Setting the scroller's knob style or
    // appearance makes AppKit leave the previous knob painted in the track.
    NSScrollerKnobStyle knobStyle = lightChrome ? NSScrollerKnobStyleDark : NSScrollerKnobStyleLight;
    if ([self scrollerKnobStyle] != knobStyle)
        [self setScrollerKnobStyle:knobStyle];
    // The area around a width-limited note is the scroll view background. Keep that on the window color.
    if (chrome) {
        [self setBackgroundColor:chrome];
        [self setDrawsBackground:YES];
    }
    if (pane || chrome) {
        [[self contentView] setBackgroundColor:(pane ?: chrome)];
        [[self contentView] setDrawsBackground:YES];
    }
    [self nv_showWindowChromeMargins];
    updatingScrollerChrome = NO;
}

- (void)viewDidChangeEffectiveAppearance {
    [super viewDidChangeEffectiveAppearance];
    [self nv_updateScrollerChrome];
}

- (void)nv_showWindowChromeMargins {
    if (installingChromeMargin)
        return;
    if (![[self documentView] isKindOfClass:[NSTextView class]])
        return;
    installingChromeMargin = YES;
    if (!nvChromeMargin) {
        nvChromeMargin = [[NVChromeMarginView alloc] initWithFrame:[self bounds]];
        [nvChromeMargin setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    }
    [nvChromeMargin setFrame:[self bounds]];
    /* Below the clip view still ends up above the scroller, so the gutter
       fill covers the bar. Keep the margin behind the scroller. */
    NSView *below = [self verticalScroller] ?: [self contentView];
    [self addSubview:nvChromeMargin positioned:NSWindowBelow relativeTo:below];
    [nvChromeMargin setNeedsDisplay:YES];
    [[self verticalScroller] setNeedsDisplay:YES];
    installingChromeMargin = NO;
}

- (void)dealloc {
    [nvChromeMargin release];
    [super dealloc];
}

- (void)tile {
    [super tile];
    [self nv_showWindowChromeMargins];
}


@end
