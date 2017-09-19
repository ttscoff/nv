/*Copyright (c) 2010, Zachary Schneirov. All rights reserved.
  Redistribution and use in source and binary forms, with or without modification, are permitted 
  provided that the following conditions are met:
   - Redistributions of source code must retain the above copyright notice, this list of conditions 
     and the following disclaimer.
   - Redistributions in binary form must reproduce the above copyright notice, this list of 
	 conditions and the following disclaimer in the documentation and/or other materials provided with
     the distribution.
   - Neither the name of Notational Velocity nor the names of its contributors may be used to endorse 
     or promote products derived from this software without specific prior written permission. */


#import "LinearDividerShader.h"


static void ColorBlendFunction(void *info, const CGFloat *in, CGFloat *out);

@implementation LinearDividerShader

- (id)initWithStartColor:(NSColor*)start endColor:(NSColor*)end {
    
    if ((self = [super init])) {
		
		colorSpaceRef = CGColorSpaceCreateDeviceRGB();
		
		[[start colorUsingColorSpaceName:NSDeviceRGBColorSpace] getRed: &colors.firstColor.redComp green:&colors.firstColor.greenComp
																  blue:&colors.firstColor.blueComp alpha:&colors.firstColor.alphaComp];
		
		[[end colorUsingColorSpaceName:NSDeviceRGBColorSpace] getRed: &colors.secondColor.redComp green:&colors.secondColor.greenComp
																blue:&colors.secondColor.blueComp alpha:&colors.secondColor.alphaComp];
		
		static const CGFloat validIntervals[8] = { 0.0, 1.0, 0.0, 1.0, 0.0, 1.0, 0.0, 1.0 };
		static const CGFunctionCallbacks cgFunctionCallbacks = { 0, &ColorBlendFunction, nil };
		
		axialShadingFunction = CGFunctionCreate(&colors, 1, validIntervals, 4, validIntervals, &cgFunctionCallbacks);
		
		//dimpleImage = [[NSImage alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForImageResource:@"SplitViewDimple.tif"]];
    }

    return self;
}

- (id)initWithBaseColors:(id)sender {
	if ((self = [super init])) {
		NSColor *endColor = [NSColor colorWithCalibratedWhite:0.875 alpha:1.0f];
		NSColor *oneColor = [NSColor colorWithCalibratedWhite:0.988 alpha:1.0f];
		
		colorSpaceRef = CGColorSpaceCreateDeviceRGB();
		
		[[oneColor colorUsingColorSpaceName:NSDeviceRGBColorSpace] getRed: &colors.firstColor.redComp green:&colors.firstColor.greenComp
																  blue:&colors.firstColor.blueComp alpha:&colors.firstColor.alphaComp];
		
		[[endColor colorUsingColorSpaceName:NSDeviceRGBColorSpace] getRed: &colors.secondColor.redComp green:&colors.secondColor.greenComp
																blue:&colors.secondColor.blueComp alpha:&colors.secondColor.alphaComp];
		
		static const CGFloat validIntervals[8] = { 0.0, 1.0, 0.0, 1.0, 0.0, 1.0, 0.0, 1.0 };
		static const CGFunctionCallbacks cgFunctionCallbacks = { 0, &ColorBlendFunction, nil };
		
		axialShadingFunction = CGFunctionCreate(&colors, 1, validIntervals, 4, validIntervals, &cgFunctionCallbacks);
		
		//dimpleImage = [[NSImage alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForImageResource:@"SplitViewDimple.tif"]];
	}
	return self;
}

- (void)dealloc {

	CGFunctionRelease(axialShadingFunction);
	CGColorSpaceRelease(colorSpaceRef);
//	[dimpleImage release];
	[borderCol release];
	[backCol release];
	[super dealloc];
}


- (void)updateColorsWithBackgroundColor:(NSColor*)backgrndColor andForegroundColor:(NSColor *)fColor{
	[backCol release];
	[borderCol release];
	backCol = [backgrndColor retain];
	borderCol = [backgrndColor colorUsingColorSpaceName:NSCalibratedWhiteColorSpace];
	CGFloat fWhite;
	fWhite = [borderCol whiteComponent];
    
	NSColor *endColor;
//	CGFloat fWhite;
//	fWhite = [[startColor colorUsingColorSpaceName:NSCalibratedWhiteColorSpace] whiteComponent];
//    NSLog(@"hia:%f",fWhite);
	if (fWhite < 0.75f) {
		if (fWhite<0.15f) {
//			fWhite += 0.2f;
//			endColor = [NSColor colorWithCalibratedWhite:0.1f alpha:1.0f];
//			backgrndColor = [NSColor colorWithCalibratedWhite:0.2f alpha:1.0f];
            endColor = [NSColor colorWithCalibratedWhite:0.06f alpha:1.0f];
			backgrndColor = [NSColor colorWithCalibratedWhite:0.2f alpha:1.0f];
		}else if (fWhite < 0.33) {
			fWhite += 0.2f;
			endColor = [NSColor colorWithCalibratedWhite:(fWhite - 0.12f) alpha:1.0f];
			backgrndColor = [NSColor colorWithCalibratedWhite:(fWhite + 0.07f) alpha:1.0f];
		}else if (fWhite < 0.52) {
			fWhite += 0.16f;
			endColor = [NSColor colorWithCalibratedWhite:(fWhite - 0.15f) alpha:1.0f];
			backgrndColor = [NSColor colorWithCalibratedWhite:(fWhite + 0.05f) alpha:1.0f];
		}else {
			fWhite += 0.16f;
			endColor = [NSColor colorWithCalibratedWhite:(fWhite - 0.2f) alpha:1.0f];
			backgrndColor = [NSColor colorWithCalibratedWhite:(fWhite + 0.02f) alpha:1.0f];
		}
	}else {
		backgrndColor = [NSColor colorWithCalibratedWhite:0.988 alpha:1.0f];
		endColor = [NSColor colorWithCalibratedWhite:0.875 alpha:1.0f];
//		fWhite -= 0.25f;
		//dimpleImage = [[NSImage alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForImageResource:@"SplitViewDimple.tif"]];
	}
    CGFloat fract=0.45f;
//   backgrndColor= [backCol highlightWithLevel:fract];
    backgrndColor=[[backCol blendedColorWithFraction:fract ofColor:backgrndColor] highlightWithLevel:0.04f];
//    endColor=[backCol shadowWithLevel:fract*1.9f];
    endColor=[[backCol blendedColorWithFraction:fract ofColor:endColor] shadowWithLevel:0.13f];
	borderCol = [fColor retain];// [[backCol blendedColorWithFraction:0.35f ofColor:fColor] retain];
//    borderCol = [[NSColor colorWithCalibratedWhite:fWhite alpha:1.0f] retain];
    
    
	colorSpaceRef = CGColorSpaceCreateDeviceRGB();
	
	[[backgrndColor colorUsingColorSpaceName:NSDeviceRGBColorSpace] getRed: &colors.firstColor.redComp green:&colors.firstColor.greenComp
                                                                   blue:&colors.firstColor.blueComp alpha:&colors.firstColor.alphaComp];
	
	[[endColor colorUsingColorSpaceName:NSDeviceRGBColorSpace] getRed: &colors.secondColor.redComp green:&colors.secondColor.greenComp
																 blue:&colors.secondColor.blueComp alpha:&colors.secondColor.alphaComp];
	
	static const CGFloat validIntervals[8] = { 0.0, 1.0, 0.0, 1.0, 0.0, 1.0, 0.0, 1.0 };
	static const CGFunctionCallbacks cgFunctionCallbacks = { 0, &ColorBlendFunction, nil };
	
	axialShadingFunction = CGFunctionCreate(&colors, 1, validIntervals, 4, validIntervals, &cgFunctionCallbacks);
}


- (void)drawDividerInRect:(NSRect)aRect withDimpleRect:(NSRect)dimpleRect blendVertically:(BOOL)v {
	if (!borderCol) {
		borderCol =[[NSColor grayColor] retain];
	}
	if (!v||((aRect.origin.x==0.0f)&&(aRect.origin.y==0.0f))) {
		if (!backCol) {
			backCol = [[NSColor lightGrayColor] retain];
		}        
//		[backCol setFill];
//		NSRectFill(aRect);
        NSPoint pt=aRect.origin;
        NSPoint endPt;
        CGFloat strokeWt=0.28f;
        if (!v&&(aRect.origin.x>0.0f)) { //draw single line for hor. layout
            strokeWt=0.5f;
            pt.x+=0.5f;
            endPt=pt;
            endPt.y=ceilf(aRect.size.height);
        }else if (aRect.size.width < aRect.size.height) {
            pt.x=floorf(aRect.size.width)-0.5f;
            endPt=pt;
            endPt.y=ceilf(aRect.size.height);
		}else {
            pt.y=(floorf(aRect.size.height)-0.5f);
            endPt=pt;
            endPt.x=ceilf(aRect.size.width);
		}
        
        NSBezierPath *path=[NSBezierPath bezierPath];
		[path moveToPoint:pt];
        [path lineToPoint:endPt];
        [path setLineWidth:1.0f];
        [[backCol blendedColorWithFraction:strokeWt ofColor:borderCol] setStroke];
        [path stroke];
		
	}else {
		CGShadingRef cgShading = CGShadingCreateAxial(colorSpaceRef, CGPointMake(aRect.origin.x, aRect.origin.y), 
													  CGPointMake(v ? NSMinX(aRect) : NSMaxX(aRect), v ? NSMaxY(aRect) : NSMinY(aRect)), 
													  axialShadingFunction, NO, NO);	
		CGContextDrawShading((CGContextRef)[[NSGraphicsContext currentContext] graphicsPort], cgShading);
		CGShadingRelease(cgShading);
        
        
        NSPoint pt=aRect.origin;
        pt.y+=0.5f;
        NSPoint endPt=pt;
        endPt.x=ceilf(aRect.size.width);
        NSBezierPath *path=[NSBezierPath bezierPath];
		[path moveToPoint:pt];
        [path lineToPoint:endPt];
        pt.y+=(aRect.size.height-1.0f);
        endPt.y=pt.y;
		[path moveToPoint:pt];
        [path lineToPoint:endPt];
        [path setLineWidth:1.0f];
        [[backCol blendedColorWithFraction:0.27f ofColor:borderCol] setStroke];
        [path stroke];
		/*
		if (!NSIsEmptyRect(dimpleRect)&&(dimpleImage)) {
			[dimpleImage drawCenteredInRect:dimpleRect];
		}*/
	}

	
}


@end

NSRect centeredRectInRect(NSRect rect, NSSize size) {
	NSRect centerRect;
	centerRect.size = size;
	centerRect.origin = NSMakePoint((rect.size.width - size.width) / 2.0,
									(rect.size.height - size.height) / 2.0);
	centerRect.origin = NSMakePoint(rect.origin.x + centerRect.origin.x, rect.origin.y + centerRect.origin.y);
	return centerRect;
}


void ColorBlendFunction(void *info, const CGFloat *in, CGFloat *out) {
	ColorSet* colors = (ColorSet *)info;
	
	float inVal = in[0];
	
	unsigned int i;
	for (i=0; i<4; i++) out[i] = (1.0 - inVal) * colors->firstColor.channels[i] + inVal * colors->secondColor.channels[i];
}


@implementation NSImage (CenteredDrawing)

- (void)drawCenteredInRect:(NSRect)aRect {
	[self drawCenteredInRect:aRect fraction:1.0];
}
- (void)drawCenteredInRect:(NSRect)aRect fraction:(float)aFraction {
	NSRect cent = centeredRectInRect(aRect, [self size]);
	cent = [[NSView focusView] centerScanRect:cent];
//	[self compositeToPoint:NSMakePoint(cent.origin.x, cent.origin.y + cent.size.height) operation:NSCompositeSourceOver fraction:aFraction];
    
[self drawInRect:cent fromRect:NSZeroRect operation:NSCompositeSourceOver fraction:aFraction respectFlipped:YES hints:nil];
}

@end
