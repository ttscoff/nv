//
//  NSBezierPath_NV.m
//  Notation
//
//  Created by Zachary Schneirov on 1/14/11.

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

#import "NSBezierPath_NV.h"


@implementation NSBezierPath (NV)

+ (NSBezierPath *)bezierPathWithRoundRectInRect:(NSRect)aRect radius:(float)radius  {
	NSBezierPath* path = [NSBezierPath bezierPath];
	float smallestEdge = MIN(NSWidth(aRect), NSHeight(aRect));
	radius = MIN(radius, 0.5f * smallestEdge);
	NSRect rect = NSInsetRect(aRect, radius, radius);
	[path appendBezierPathWithArcWithCenter:NSMakePoint(NSMinX(rect), NSMinY(rect)) radius:radius startAngle:180.0 endAngle:270.0];
	[path appendBezierPathWithArcWithCenter:NSMakePoint(NSMaxX(rect), NSMinY(rect)) radius:radius startAngle:270.0 endAngle:360.0];
	[path appendBezierPathWithArcWithCenter:NSMakePoint(NSMaxX(rect), NSMaxY(rect)) radius:radius startAngle:  0.0 endAngle: 90.0];
	[path appendBezierPathWithArcWithCenter:NSMakePoint(NSMinX(rect), NSMaxY(rect)) radius:radius startAngle: 90.0 endAngle:180.0];
	[path closePath];
	return path;
}

+ (NSBezierPath *)bezierPathWithLayoutManager:(NSLayoutManager*)layoutManager characterRange:(NSRange)charRange atPoint:(NSPoint)point {
	NSRange range = [layoutManager glyphRangeForCharacterRange:charRange actualCharacterRange:NULL];
	NSGlyph *glyphs = (NSGlyph *)malloc(sizeof(NSGlyph) * range.length * 2);
	[layoutManager getGlyphs:glyphs range:range];
		
	NSBezierPath *path = [NSBezierPath bezierPath];
	[path moveToPoint:point];
	[path appendBezierPathWithGlyphs:glyphs count:range.length inFont:[[layoutManager textStorage] font]];
	
	free(glyphs);
	
	return path;
}


@end

@implementation NSImage (NV)

+ (NSImage*)smallIconForFSRef:(FSRef*)fsRef {
    OSStatus err = noErr;
    
    if (!fsRef)
    return nil;
    
    IconRef iconRef;
    if ((err = GetIconRefFromFileInfo(fsRef, 0, NULL, 0, NULL, kIconServicesNormalUsageFlag, &iconRef, NULL)) == noErr) {
    
    NSImage *image = [[[NSImage alloc] initWithSize:NSMakeSize(16.0f, 16.0f)] autorelease];
    NSRect frame = NSMakeRect(0.0f,0.0f,16.0f,16.0f);
    
    [image lockFocus];
    err = PlotIconRefInContext([[NSGraphicsContext currentContext] graphicsPort], (CGRect *)&frame, 0, 0, nil, 0, iconRef);
    [image unlockFocus];
    
    if (err == noErr)
      return image;
    }
    
    NVLogError(@"smallIconForFSRef error: %d", err);
    
    return nil;
}


@end

