//
//  ETScrollView.m
//  Notation
//
//  Created by elasticthreads on 3/14/11.
//

#import "ETScrollView.h"
#import "ETOverlayScroller.h"
#import "ETTransparentScroller.h"
#import "GlobalPrefs.h"
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


+ (BOOL)isCompatibleWithResponsiveScrolling{
    return NO;
}


- (void)awakeFromNib{
    needsOverlayTiling=NO;
    if ([self.documentView isKindOfClass:[NSTableView class]]) {
        scrollerClass=NSClassFromString(@"ETOverlayScroller");
        [self setAutohidesScrollers:YES];
        if (!IsLionOrLater) {
            needsOverlayTiling=YES;
        }
    }else{
        scrollerClass=NSClassFromString(@"ETTransparentScroller");
    }
#if MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_7
    if (IsLionOrLater) {
        [[GlobalPrefs defaultPrefs] registerForSettingChange:@selector(setUseETScrollbarsOnLion:sender:) withTarget:self];
        [self setHorizontalScrollElasticity:NSScrollElasticityNone];
        [self setVerticalScrollElasticity:NSScrollElasticityAllowed];
    }
#endif
    [self changeUseETScrollbarsOnLion];
}


#if MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_7
- (void)settingChangedForSelectorString:(NSString*)selectorString{
    if (IsLionOrLater&&([selectorString isEqualToString:SEL_STR(setUseETScrollbarsOnLion:sender:)])){
        [self changeUseETScrollbarsOnLion];
    }
}

- (void)changeUseETScrollbarsOnLion{
    id theScroller = [[scrollerClass alloc] init];
    if ([theScroller respondsToSelector:@selector(setFillBackground:)])
        [theScroller setFillBackground:!IsLionOrLater && (scrollerClass == [ETTransparentScroller class])];
    NSScrollerStyle style = 0;
    if (IsLionOrLater)
        style = [[theScroller class] preferredScrollerStyle];
    [self setVerticalScroller:theScroller];

    if (IsLionOrLater) {
        [theScroller setScrollerStyle:style];
        [self setScrollerStyle:style];
    }
    [theScroller release];
    [self tile];
    [self reflectScrolledClipView:[self contentView]];
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
    NSColor *knob = NVWindowChromeKnobColor();
    BOOL lightChrome = !chrome || NVThemeColorIsLight(chrome);
    NSAppearance *appearance = [NSAppearance appearanceNamed:(lightChrome ? NSAppearanceNameAqua : NSAppearanceNameDarkAqua)];
    NSScroller *scroller = [self verticalScroller];
    BOOL legacy = IsLionOrLater && [scroller scrollerStyle] == NSScrollerStyleLegacy;
    [scroller setKnobStyle:lightChrome ? NSScrollerKnobStyleDark : NSScrollerKnobStyleLight];
    if ([scroller appearance] != appearance && (!appearance || ![[scroller appearance] isEqual:appearance]))
        [scroller setAppearance:appearance];
    NSColor *track = legacy ? chrome : nil;
    if ([scroller respondsToSelector:@selector(nv_setExplicitKnobColor:trackColor:)])
        [(ETTransparentScroller *)scroller nv_setExplicitKnobColor:knob trackColor:track];
    // The area around a width-limited note is the scroll view background. Keep that on the window color.
    if (chrome) {
        [self setBackgroundColor:chrome];
        [self setDrawsBackground:YES];
    }
    if (pane || chrome) {
        [[self contentView] setBackgroundColor:(pane ?: chrome)];
        [[self contentView] setDrawsBackground:YES];
    }
    [scroller setNeedsDisplay:YES];
    [self nv_showWindowChromeMargins];
    updatingScrollerChrome = NO;
}

- (void)viewDidChangeEffectiveAppearance {
    [super viewDidChangeEffectiveAppearance];
    [self nv_updateScrollerChrome];
}
#endif

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
    [self addSubview:nvChromeMargin positioned:NSWindowBelow relativeTo:[self contentView]];
    [nvChromeMargin setNeedsDisplay:YES];
    installingChromeMargin = NO;
}

- (void)dealloc {
    [nvChromeMargin release];
    [super dealloc];
}

- (void)tile {
    [super tile];
    [self nv_showWindowChromeMargins];
    if (needsOverlayTiling) {
        if (![[self verticalScroller] isHidden]) {
            //            NSRect vsRect=[[self verticalScroller] frame];
            NSRect conRect = [[self contentView] frame];
            //            NSView *wdContent = [[self contentView] retain];
            conRect.size.width = conRect.size.width + [[self verticalScroller] frame].size.width;
            [[self contentView] setFrameSize:conRect.size];
            //            [wdContent setFrame:conRect];
            //            [wdContent release];
            //            [[self verticalScroller] setFrame:vsRect];
        }
    }
}


@end
