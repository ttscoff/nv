//
//  ETTransparentScroller.h
//
//  Created by Brandon Walkin (www.brandonwalkin.com)
//  All code is provided under the New BSD license.
//
//	Modified by elasticthreads 10/19/2010
//

#import "ETTransparentScroller.h"

//@interface NSScroller (NVTSPrivate)
//- (NSRect)_drawingRectForPart:(NSScrollerPart)aPart;
//@end

@implementation ETTransparentScroller

//+ (void)initialize
//{
//}

+ (BOOL)isCompatibleWithResponsiveScrolling{
    return NO;
}

- (id)initWithFrame:(NSRect)frameRect{
	if ((self=[super initWithFrame:frameRect])) {	
        fillBackground=NO;
        NSBundle *bundle = [NSBundle mainBundle];
        
        knobTop				= [[NSImage alloc] initWithContentsOfFile:[bundle pathForImageResource:@"TransparentScrollerKnobTop.tif"]];
        knobVerticalFill	= [[NSImage alloc] initWithContentsOfFile:[bundle pathForImageResource:@"TransparentScrollerKnobVerticalFill.tif"]];
        knobBottom			= [[NSImage alloc] initWithContentsOfFile:[bundle pathForImageResource:@"TransparentScrollerKnobBottom.tif"]];
        slotTop				= [[NSImage alloc] initWithContentsOfFile:[bundle pathForImageResource:@"TransparentScrollerSlotTop.tif"]];
        slotVerticalFill	= [[NSImage alloc] initWithContentsOfFile:[bundle pathForImageResource:@"TransparentScrollerSlotVerticalFill.tif"]];
        slotBottom			= [[NSImage alloc] initWithContentsOfFile:[bundle pathForImageResource:@"TransparentScrollerSlotBottom.tif"]];
       verticalPaddingLeft = 4.0f;
       verticalPaddingRight = 3.0f;
       verticalPaddingTop =3.75f;
       verticalPaddingBottom = 4.25f;
       minKnobHeight = knobTop.size.height + knobVerticalFill.size.height + knobBottom.size.height + 20.0;
        slotAlpha=0.45f;
        knobAlpha=0.45f;
		[self setArrowsPosition:NSScrollerArrowsNone];
        
//        isOverlay=NO;        
#if MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_7
        if (IsLionOrLater) {
//            isOverlay=[[self class]isCompatibleWithOverlayScrollers];
        }
#endif
	}
	return self;
}

#if MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_7
+ (BOOL)isCompatibleWithOverlayScrollers {
    return YES;
}

+ (NSScrollerStyle)preferredScrollerStyle{
    return [NSScroller preferredScrollerStyle];
}

- (BOOL)wantsUpdateLayer {
    return NO;
}


#endif

- (void)dealloc{
    [knobTop release];
    [knobVerticalFill release];
    [knobBottom release];
    [slotTop release];
    [slotBottom release];
    [slotVerticalFill release];
    [nvKnobColor release];
    [nvTrackColor release];
    [super dealloc];
}

- (void)nv_setExplicitKnobColor:(NSColor *)knob trackColor:(NSColor *)track {
    if (nvKnobColor != knob) {
        [nvKnobColor release];
        nvKnobColor = [knob retain];
    }
    if (nvTrackColor != track) {
        [nvTrackColor release];
        nvTrackColor = [track retain];
    }
}

#if MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_7
+ (CGFloat)scrollerWidthForControlSize:(NSControlSize)controlSize scrollerStyle:(NSScrollerStyle)scrollerStyle{
    return [NSScroller scrollerWidthForControlSize:controlSize scrollerStyle:scrollerStyle];
}

#else
+ (CGFloat)scrollerWidth
{
	return [NSScroller scrollerWidth];
}
+ (CGFloat)scrollerWidthForControlSize:(NSControlSize)controlSize 
{
	return [NSScroller scrollerWidthForControlSize:controlSize];
}
#endif

- (void)setFillBackground:(BOOL)fillIt{
    fillBackground=fillIt;
}

- (void)drawRect:(NSRect)aRect;
{
    if (nvKnobColor && [self scrollerStyle] == NSScrollerStyleOverlay) {
        if ([self usableParts] == NSAllScrollerParts)
            [self drawKnob];
        return;
    }
    if (nvTrackColor) {
        [nvTrackColor setFill];
        NSRectFill([self bounds]);
        if ([self usableParts] == NSAllScrollerParts)
            [self drawKnob];
        return;
    }
        // Only draw if the slot is larger than the knob
    if (IsLionOrLater) {
        [super drawRect:aRect];
    }else{
        if (([self bounds].size.height - verticalPaddingTop - verticalPaddingBottom + 1) > minKnobHeight){

            if (fillBackground) {                
                [[[[self window] contentView] backgroundColor] setFill];
                NSRectFill([self bounds]);
            }
            [self drawKnobSlotInRect:[self rectForPart:NSScrollerKnobSlot] highlight:NO];
            
            if ([self knobProportion] > 0.0)	
                [self drawKnob];
        }
    }
}

- (void)drawKnobSlotInRect:(NSRect)slotRect highlight:(BOOL)flag{
    if (!nvTrackColor) {
        [super drawKnobSlotInRect:slotRect highlight:flag];
        return;
    }
    [nvTrackColor setFill];
    NSRectFillUsingOperation(slotRect, NSCompositingOperationSourceOver);
}

- (void)drawKnob;
{
    if (!nvKnobColor) {
        [super drawKnob];
        return;
    }
    NSRect knobRect = [self rectForPart:NSScrollerKnob];
    knobRect = NSInsetRect(knobRect, 2.0, 2.0);
    if (knobRect.size.width < 4.0) {
        CGFloat extra = 4.0 - knobRect.size.width;
        knobRect.origin.x -= extra / 2.0;
        knobRect.size.width = 4.0;
    }
    if (knobRect.size.height < 4.0) {
        CGFloat extra = 4.0 - knobRect.size.height;
        knobRect.origin.y -= extra / 2.0;
        knobRect.size.height = 4.0;
    }
    if (knobRect.size.width < 1.0 || knobRect.size.height < 1.0)
        return;
    CGFloat radius = MIN(knobRect.size.width, knobRect.size.height) / 2.0;
    [nvKnobColor setFill];
    [[NSBezierPath bezierPathWithRoundedRect:knobRect xRadius:radius yRadius:radius] fill];
}
//
//- (NSRect)_drawingRectForPart:(NSScrollerPart)aPart;
//{
//	// Call super even though we're not using its value (has some side effects we need)
//	[super _drawingRectForPart:aPart];
//	
//	// Return our own rects rather than use the default behavior
//	return [self rectForPart:aPart];
//}



//- (void)trackKnob:(NSEvent *)theEvent{
//    NSPoint aPoint=[theEvent locationInWindow];
//    NSScrollerPart aPart=[super testPart:aPoint];
//    [super trackKnob:theEvent];
//}

@end
