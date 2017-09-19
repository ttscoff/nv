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
    return NO;
}

+ (NSScrollerStyle)preferredScrollerStyle{
//    if ([[NSScroller class]preferredScrollerStyle]==NSScrollerStyleOverlay) {
//        
//    }
    return NSScrollerStyleLegacy;
}


#endif

- (void)dealloc{
    [knobTop release];
    [knobVerticalFill release];
    [knobBottom release];
    [slotTop release];
    [slotBottom release];
    [slotVerticalFill release];
    [super dealloc];
}

#if MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_7
+ (CGFloat)scrollerWidthForControlSize:(NSControlSize)controlSize scrollerStyle:(NSScrollerStyle)scrollerStyle{
    return 15.0;
}

#else
+ (CGFloat)scrollerWidth
{
	return 15.0;//slotVerticalFill.size.width + verticalPaddingLeft + verticalPaddingRight;
}
+ (CGFloat)scrollerWidthForControlSize:(NSControlSize)controlSize 
{
	return 15.0;//slotVerticalFill.size.width + verticalPaddingLeft + verticalPaddingRight;
}
#endif

- (void)setFillBackground:(BOOL)fillIt{
    fillBackground=fillIt;
}

- (void)drawRect:(NSRect)aRect;
{       
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
   
    NSDrawThreePartImage(slotRect, slotTop, slotVerticalFill, slotBottom, YES, NSCompositeSourceOver, slotAlpha, NO);
   
}

- (void)drawKnob;
{
	NSRect knobRect = [self rectForPart:NSScrollerKnob];

	NSDrawThreePartImage(knobRect, knobTop, knobVerticalFill, knobBottom, YES, NSCompositeSourceOver, knobAlpha, NO);
   
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

- (NSRect)rectForPart:(NSScrollerPart)aPart{    
	switch (aPart)
	{
		case NSScrollerNoPart:
        {
			return [self bounds];
			break;
		}
        case NSScrollerKnob:
		{
			NSRect knobRect=[super rectForPart:NSScrollerKnob];
//			NSRect slotRect = [self rectForPart:NSScrollerKnobSlot];
//			float knobHeight = roundf(slotRect.size.height * [self knobProportion]);
//			if (knobHeight < minKnobHeight){
//                if (minKnobHeight>slotRect.size.height) {
//                    knobHeight=knobRect.size.height;
//                }else{
//                    knobHeight = minKnobHeight;
//                }
//            }
            CGFloat slotY=roundf(verticalPaddingTop);
            CGFloat knobY=knobRect.origin.y;
            CGFloat slotHt=roundf([self bounds].size.height-(verticalPaddingTop+verticalPaddingBottom));
            if (knobY<slotY) {
                knobY=slotY;
            }
            else if ((knobY+knobRect.size.height)>(slotHt+slotY)){
                knobY=slotHt+slotY-knobRect.size.height;

            }
//			knobRect = NSMakeRect(verticalPaddingLeft, knobY, slotRect.size.width, knobHeight);
            knobRect.origin.x=roundf(verticalPaddingLeft);
            knobRect.origin.y=roundf(knobY);
            knobRect.size.width=roundf([self bounds].size.width - verticalPaddingLeft - verticalPaddingRight);
			
			return knobRect;
		}
			break;	
		case NSScrollerKnobSlot:
		{
			NSRect slotRect=[self bounds];
			slotRect.origin.x=roundf(verticalPaddingLeft);
            slotRect.size.width=roundf(slotRect.size.width - verticalPaddingLeft - verticalPaddingRight);
            slotRect.origin.y=roundf(verticalPaddingTop);
            slotRect.size.height=roundf(slotRect.size.height-(verticalPaddingTop+verticalPaddingBottom));
			return slotRect;
		}
			break;
		case NSScrollerIncrementLine:
			return NSZeroRect;
			break;
		case NSScrollerDecrementLine:
			return NSZeroRect;
			break;
		case NSScrollerIncrementPage:
		{
			NSRect incrementPageRect;
			NSRect knobRect = [self rectForPart:NSScrollerKnob];
//			NSRect slotRect = [self rectForPart:NSScrollerKnobSlot];
//			NSRect decPageRect = [self rectForPart:NSScrollerDecrementPage];
            CGFloat slotHt=roundf([self bounds].size.height-(verticalPaddingTop+verticalPaddingBottom));
			CGFloat knobY = roundf(knobRect.origin.y + knobRect.size.height);
            CGFloat knobHt=roundf(slotHt - knobRect.size.height - knobRect.origin.y - verticalPaddingTop);
			incrementPageRect = NSMakeRect(roundf(verticalPaddingLeft), knobY, knobRect.size.width, knobHt);
            
			return incrementPageRect;
		}
			break;
		case NSScrollerDecrementPage:
		{
			NSRect decrementPageRect;
			NSRect knobRect = [self rectForPart:NSScrollerKnob];
			
            
			decrementPageRect = NSMakeRect(roundf(verticalPaddingLeft), roundf(verticalPaddingTop), knobRect.size.width, roundf(knobRect.origin.y - verticalPaddingTop));
            
			return decrementPageRect;
		}
			break;
		default:
			break;
	}
	
	return NSZeroRect;
}

@end
