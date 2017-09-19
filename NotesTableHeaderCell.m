//
//  NotesTableHeaderCell.m
//  Notation
//
//  Created by David Halter on 6/12/13.
//  Copyright (c) 2013 David Halter. All rights reserved.
//

#import "NotesTableHeaderCell.h"

@interface NotesTableHeaderCell (Private)

- (void)_drawBorderWithFrame:(NSRect)cellFrame;
- (void)_drawGradientFromColor:(NSColor *)baseColor inRect:(NSRect)cellFrame;

@end


NSColor *bColor;
NSColor *tColor;

@implementation NotesTableHeaderCell

+ (void)initialize{
    if (!bColor) {
        bColor = [[[NSColor whiteColor] colorUsingColorSpaceName:NSCalibratedRGBColorSpace] retain];
    }
    if (!tColor) {
        tColor = [[[NSColor blackColor] colorUsingColorSpaceName:NSCalibratedRGBColorSpace] retain];
    }
}

- (id)initTextCell:(NSString *)text{
    if ((self = [super initTextCell:text])) {
        if (!text || (text.length==0)) {
            [self setTitle:@"Title"];
        }
    }
    return self;
}


- (BOOL)isOpaque{
    return YES;
}

- (NSRect)drawingRectForBounds:(NSRect)theRect {
	return NSIntegralRect(NSInsetRect(theRect, 6.0f, 1.0f));
}

- (NSRect)sortIndicatorRectForBounds:(NSRect)theRect{
    theRect=[super sortIndicatorRectForBounds:theRect];
    theRect.origin.y = floor(theRect.origin.y-0.5f);
    return NSIntegralRect(theRect);
}

//- (void)drawSortIndicatorWithFrame:(NSRect)cellFrame inView:(NSView *)controlView ascending:(BOOL)ascending priority:(NSInteger)priority{
//	NSLog(@"draw sort");
//}

//- (void)drawInteriorWithFrame:(NSRect)cellFrame inView:(NSView *)controlView{
////    cellFrame=NSInsetRect(cellFrame, 0.0f, 1.0f);
////    cellFrame.size.height-=1.0f;
//    [super drawInteriorWithFrame:cellFrame inView:controlView];
//}



- (void)drawWithFrame:(NSRect)inFrame inView:(NSView*)inView{
    [self setTextColor:tColor];
//    [super drawWithFrame:inFrame inView:inView];
    [self _drawGradientFromColor:bColor inRect:inFrame];
    [self drawInteriorWithFrame:inFrame inView:inView];
    [self _drawBorderWithFrame:inFrame];
}

#define kSelectedCellEmphasisLevel 0.24f
#define kSelectedCellTextEmphasisLevel 0.3f

- (void)highlight:(BOOL)hBool withFrame:(NSRect)inFrame inView:(NSView *)controlView{
    NSColor *theBack;
    if ([[bColor colorUsingColorSpaceName:NSCalibratedWhiteColorSpace] whiteComponent]<0.5f) {
        theBack=[bColor highlightWithLevel:kSelectedCellEmphasisLevel];
        [self setTextColor:[tColor highlightWithLevel:kSelectedCellTextEmphasisLevel]];
	}else {
        theBack=[bColor shadowWithLevel:kSelectedCellEmphasisLevel];
        [self setTextColor:[tColor shadowWithLevel:kSelectedCellTextEmphasisLevel]];
	}
    [self _drawGradientFromColor:theBack inRect:inFrame];
    [self drawInteriorWithFrame:inFrame inView:controlView];
    [self _drawBorderWithFrame:inFrame];
}



#pragma mark - nvALT additions

+ (void)setBColor:(NSColor *)inColor{
    if (bColor) {
        [bColor release];
    }
	bColor = [[inColor colorUsingColorSpaceName:NSCalibratedRGBColorSpace] retain];
}

+ (void)setTxtColor:(NSColor *)inColor{
    if (tColor) {
        [tColor release];
    }
    if ([[inColor colorUsingColorSpaceName:NSCalibratedWhiteColorSpace] whiteComponent]>0.5f) {
        inColor=[inColor highlightWithLevel:kSelectedCellEmphasisLevel];
    }else{
        inColor=[inColor shadowWithLevel:kSelectedCellEmphasisLevel];
    }
    inColor=[inColor colorUsingColorSpaceName:NSCalibratedRGBColorSpace];
	tColor = [inColor retain];
}

@end


@implementation NotesTableHeaderCell (Private)

- (void)_drawBorderWithFrame:(NSRect)cellFrame{
    NSBezierPath* thePath = [NSBezierPath new];
    [thePath removeAllPoints];
    NSPoint pt=NSMakePoint(cellFrame.origin.x, (cellFrame.origin.y +  cellFrame.size.height-0.5f));
    [thePath moveToPoint:NSMakePoint(NSMaxX(cellFrame),pt.y)];
    [thePath lineToPoint:pt];
    
    [[tColor blendedColorWithFraction:0.33f ofColor:bColor] setStroke];
    [thePath setLineWidth:1.0f];
    [thePath stroke];
    
    if (cellFrame.origin.x>5.0f) {
        [thePath removeAllPoints];
         [thePath moveToPoint:NSMakePoint(cellFrame.origin.x,(cellFrame.origin.y + cellFrame.size.height))];
        [thePath lineToPoint:cellFrame.origin];
        
//        [[tColor colorWithAlphaComponent:0.95f]setStroke];
        [thePath setLineWidth:1.0f];
        [thePath stroke];
    }
//    if ([self state]) {
//         NSLog(@"isHigh :>%d<  title :>%@<",[self isHighlighted],[self title]);
//    }
//    [thePath ]
    [thePath release];
}

- (void)_drawGradientFromColor:(NSColor *)baseColor inRect:(NSRect)cellFrame{
    
    baseColor = [baseColor colorUsingColorSpaceName:NSCalibratedRGBColorSpace];//[bColor    
    NSColor *startColor = [baseColor blendedColorWithFraction:0.25f ofColor:[[NSColor colorWithCalibratedWhite:0.9f alpha:1.0f] colorUsingColorSpaceName:NSCalibratedRGBColorSpace]];
    
    NSColor *endColor = [baseColor blendedColorWithFraction:0.4f ofColor:[[NSColor colorWithCalibratedWhite:0.1f alpha:1.0f] colorUsingColorSpaceName:NSCalibratedRGBColorSpace]];
 
    
    NSGradient *theGrad = [[NSGradient alloc] initWithColorsAndLocations: startColor, 0.14f,
                                endColor, 0.94f, nil];
    [theGrad drawInRect:cellFrame angle:90.0f];
    [theGrad release];
}


@end
