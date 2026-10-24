//
//  NotesTableHeaderCell.m
//  Notation
//
//  Created by David Halter on 6/12/13.
//  Copyright (c) 2013 David Halter. All rights reserved.
//

#import "NotesTableHeaderCell.h"

@interface NotesTableHeaderCell (Private)

- (void)_drawBackgroundInFrame:(NSRect)cellFrame pressed:(BOOL)pressed;
- (void)_drawTitleInFrame:(NSRect)cellFrame;
- (void)_drawSeparatorInFrame:(NSRect)cellFrame;
- (void)_drawChevronInRect:(NSRect)rect ascending:(BOOL)ascending;

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
        [self setFont:[NSFont systemFontOfSize:[NSFont systemFontSize] weight:NSFontWeightMedium]];
        [self setAlignment:NSTextAlignmentLeft];
        [self setLineBreakMode:NSLineBreakByTruncatingTail];
    }
    return self;
}


- (BOOL)isOpaque{
    return YES;
}

- (NSRect)drawingRectForBounds:(NSRect)theRect {
	NSRect rect = theRect;
	rect.origin.x += 8.0;
	rect.size.width -= 22.0;
	if (rect.size.width < 4.0)
		rect.size.width = 4.0;
	return rect;
}

- (NSRect)sortIndicatorRectForBounds:(NSRect)theRect{
	NSRect rect = theRect;
	rect.origin.x = NSMaxX(theRect) - 16.0;
	rect.size.width = 12.0;
	return rect;
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
    [self _drawBackgroundInFrame:inFrame pressed:NO];
    [self _drawTitleInFrame:inFrame];
    [self _drawSeparatorInFrame:inFrame];
}

#define kSelectedCellEmphasisLevel 0.24f
#define kSelectedCellTextEmphasisLevel 0.3f

- (void)highlight:(BOOL)hBool withFrame:(NSRect)inFrame inView:(NSView *)controlView{
	/* AppKit calls this for the sorted column as well as a click. Only the click should tint the background. */
	NSEvent *event = [NSApp currentEvent];
	BOOL pressed = hBool && event && ([event type] == NSLeftMouseDown || [event type] == NSLeftMouseDragged);
    [self _drawBackgroundInFrame:inFrame pressed:pressed];
    [self _drawTitleInFrame:inFrame];
    [self _drawSeparatorInFrame:inFrame];
}

- (void)drawSortIndicatorWithFrame:(NSRect)cellFrame inView:(NSView *)controlView ascending:(BOOL)ascending priority:(NSInteger)priority {
	if (priority != 0)
		return;
	[self _drawChevronInRect:[self sortIndicatorRectForBounds:cellFrame] ascending:ascending];
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

- (NSColor *)_lineColor {
	NSColor *ink = tColor ? tColor : [NSColor labelColor];
	return [ink colorWithAlphaComponent:0.18];
}

- (void)_drawBackgroundInFrame:(NSRect)cellFrame pressed:(BOOL)pressed {
	NSColor *fill = bColor ? bColor : [NSColor windowBackgroundColor];
	if (pressed) {
		NSColor *ink = tColor ? tColor : [NSColor labelColor];
		fill = [fill blendedColorWithFraction:0.08 ofColor:ink];
	}
	[fill setFill];
	NSRectFill(cellFrame);
}

- (void)_drawTitleInFrame:(NSRect)cellFrame {
	NSString *title = [self stringValue];
	if (![title length])
		return;

	NSFont *font = [self font] ? [self font] : [NSFont systemFontOfSize:[NSFont systemFontSize] weight:NSFontWeightMedium];
	NSColor *color = tColor ? tColor : [NSColor secondaryLabelColor];
	color = [color colorWithAlphaComponent:0.72];
	NSMutableParagraphStyle *style = [[NSMutableParagraphStyle alloc] init];
	[style setAlignment:NSTextAlignmentLeft];
	[style setLineBreakMode:NSLineBreakByTruncatingTail];
	NSDictionary *attrs = [[NSDictionary alloc] initWithObjectsAndKeys:
						   font, NSFontAttributeName,
						   color, NSForegroundColorAttributeName,
						   style, NSParagraphStyleAttributeName,
						   nil];
	NSRect textRect = [self drawingRectForBounds:cellFrame];
	NSSize size = [title sizeWithAttributes:attrs];
	textRect.origin.y = floor(NSMidY(cellFrame) - (size.height / 2.0));
	textRect.size.height = ceil(size.height);
	[title drawInRect:textRect withAttributes:attrs];
	[attrs release];
	[style release];
}

- (void)_drawSeparatorInFrame:(NSRect)cellFrame {
	[[self _lineColor] setStroke];

	NSBezierPath *bottom = [NSBezierPath bezierPath];
	CGFloat y = NSMaxY(cellFrame) - 0.5;
	[bottom moveToPoint:NSMakePoint(NSMinX(cellFrame), y)];
	[bottom lineToPoint:NSMakePoint(NSMaxX(cellFrame), y)];
	[bottom setLineWidth:1.0];
	[bottom stroke];

	if (NSMinX(cellFrame) > 1.0) {
		NSBezierPath *divider = [NSBezierPath bezierPath];
		CGFloat x = NSMinX(cellFrame) + 0.5;
		CGFloat inset = 7.0;
		[divider moveToPoint:NSMakePoint(x, NSMinY(cellFrame) + inset)];
		[divider lineToPoint:NSMakePoint(x, NSMaxY(cellFrame) - inset)];
		[divider setLineWidth:1.0];
		[divider stroke];
	}
}

- (void)_drawChevronInRect:(NSRect)rect ascending:(BOOL)ascending {
	NSBezierPath *path = [NSBezierPath bezierPath];
	CGFloat midX = NSMidX(rect);
	CGFloat midY = NSMidY(rect);
	CGFloat w = 3.5;
	CGFloat h = 2.0;
	if (ascending) {
		[path moveToPoint:NSMakePoint(midX - w, midY + h * 0.35)];
		[path lineToPoint:NSMakePoint(midX, midY - h)];
		[path lineToPoint:NSMakePoint(midX + w, midY + h * 0.35)];
	} else {
		[path moveToPoint:NSMakePoint(midX - w, midY - h * 0.35)];
		[path lineToPoint:NSMakePoint(midX, midY + h)];
		[path lineToPoint:NSMakePoint(midX + w, midY - h * 0.35)];
	}
	[path setLineWidth:1.35];
	[path setLineCapStyle:NSRoundLineCapStyle];
	[path setLineJoinStyle:NSRoundLineJoinStyle];
	NSColor *ink = tColor ? tColor : [NSColor labelColor];
	[[ink colorWithAlphaComponent:0.55] setStroke];
	[path stroke];
}

@end
