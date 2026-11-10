//
//  NSString_CustomTruncation.m
//  Notation
//
//  Created by Zachary Schneirov on 1/12/11.

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


#import "NSString_CustomTruncation.h"
#import "NSString_NV.h"
#import "GlobalPrefs.h"

@implementation NSString (CustomTruncation)

static NSMutableParagraphStyle *LineBreakingStyle();
static NSDictionary *GrayTextAttributes();
static NSDictionary *LineTruncAttributes();
static size_t EstimatedCharCountForWidth(float upToWidth);



static NSMutableDictionary *titleTruncAttrs = nil;

void ResetFontRelatedTableAttributes() {
	[titleTruncAttrs release];
	titleTruncAttrs = nil;
}

static NSMutableParagraphStyle *LineBreakingStyle() {
	static NSMutableParagraphStyle *lineBreaksStyle = nil;
	if (!lineBreaksStyle) {
		lineBreaksStyle = [[NSMutableParagraphStyle alloc] init];
		[lineBreaksStyle setLineBreakMode:NSLineBreakByTruncatingTail];
		[lineBreaksStyle setTighteningFactorForTruncation:0.0];
	}
	return lineBreaksStyle;
}

static NSDictionary *GrayTextAttributes() {
	static NSDictionary *grayTextAttributes = nil;
	if (!grayTextAttributes) grayTextAttributes = [[NSDictionary dictionaryWithObjectsAndKeys:[NSColor grayColor], NSForegroundColorAttributeName, nil] retain];
	return grayTextAttributes;
}

static NSDictionary *LineTruncAttributes() {
	static NSDictionary *lineTruncAttributes = nil;
	if (!lineTruncAttributes) lineTruncAttributes = [[NSDictionary dictionaryWithObjectsAndKeys:LineBreakingStyle(), NSParagraphStyleAttributeName, nil] retain];
	return lineTruncAttributes;
}

#pragma mark fix truncation here
NSDictionary *LineTruncAttributesForTitle() {
	if (!titleTruncAttrs) {
		GlobalPrefs *prefs = [GlobalPrefs defaultPrefs];
		unsigned int bitmap = [prefs tableColumnsBitmap];
		float fontSize = [prefs tableFontSize];
		BOOL usesBold = ColumnIsSet(NoteLabelsColumn, bitmap) || ColumnIsSet(NoteDateCreatedColumn, bitmap) ||
		ColumnIsSet(NoteDateModifiedColumn, bitmap) || [prefs tableColumnsShowPreview];
		NSFont *titleFont = [prefs tableFont];
		if (usesBold)
			titleFont = [[NSFontManager sharedFontManager] convertFont:titleFont toHaveTrait:NSBoldFontMask];
		
		titleTruncAttrs = [[NSMutableDictionary dictionaryWithObjectsAndKeys:[[LineBreakingStyle() mutableCopy] autorelease], NSParagraphStyleAttributeName,
							titleFont, NSFontAttributeName, nil] retain];
		
		if (ColumnIsSet(NoteDateCreatedColumn, bitmap) || ColumnIsSet(NoteDateModifiedColumn, bitmap)) {
			//account for right-"aligned" date string, which will be relatively constant, so this can be cached
            
            NSString *dateTest=[NSString relativeDateStringWithAbsoluteTime:CFDateGetAbsoluteTime((CFDateRef)[NSDate dateWithNaturalLanguageString:@"April 1, 2013"])];
            CGFloat multiplier=-4.7;
            if (dateTest&&(dateTest.length>8)) {
                multiplier=-6.1;
            }
			[[titleTruncAttrs objectForKey:NSParagraphStyleAttributeName] setTailIndent: fontSize * multiplier]; //avg of -55 for ~11-12 font size
		}
	}
	return titleTruncAttrs;
}

static size_t EstimatedCharCountForWidth(float upToWidth) {
	return (size_t)(upToWidth / ([[GlobalPrefs defaultPrefs] tableFontSize] / 2.5f));
}

//LineTruncAttributesForTags would be variable, depending on the note; each preview string will have its own copy of the nsdictionary

- (NSAttributedString*)attributedMultiLinePreviewFromBodyText:(NSAttributedString*)bodyText upToWidth:(float)upToWidth intrusionWidth:(float)intWidth {
	//first line is title, truncated to a shorter width to account for date/time, using a negative -[NSMutableParagraphStyle setTailIndent:] value
	//next "two" lines are wrapped body text, with a character-count estimation of essentially double that of a single-line preview
	//also with an independent tailindent to account for a separately-drawn tags-string, if tags exist
	//upToWidth will be used to manually truncate note-bodies only, and should be the full column width available
	//intWidth will typically be the width of the tags string or other representation
	
	size_t bodyCharCount = (EstimatedCharCountForWidth(upToWidth) * 2) - EstimatedCharCountForWidth(intWidth);
	bodyCharCount = MIN(bodyCharCount, [bodyText length]);
	
	NSMutableString *unattributedPreview = [[NSMutableString alloc] initWithCapacity:bodyCharCount + [self length] + 2];
	
	NSString *truncatedBodyString = [[bodyText string] truncatedPreviewStringOfLength:bodyCharCount];
	if (!truncatedBodyString){
        [unattributedPreview release];
        return nil;
    }
	
	[unattributedPreview appendString:self];
	[unattributedPreview appendString:@"\n"];
	[unattributedPreview appendString:truncatedBodyString];
	
	NSMutableAttributedString *attributedStringPreview = [[NSMutableAttributedString alloc] initWithString:unattributedPreview];
	
	//title is black (no added colors) and truncated with LineTruncAttributesForTitle()
	//body is gray and truncated with a variable tail indent, depending on intruding tags
	
	NSDictionary *bodyTruncDict = [NSDictionary dictionaryWithObjectsAndKeys:[[LineBreakingStyle() mutableCopy] autorelease], 
								   NSParagraphStyleAttributeName, [NSColor grayColor], NSForegroundColorAttributeName, nil];
	//set word-wrapping to let -[NSCell setTruncatesLastVisibleLine:] work
	[[bodyTruncDict objectForKey:NSParagraphStyleAttributeName] setLineBreakMode:NSLineBreakByWordWrapping];
	
	if (intWidth > 0.0) {
		//there are tags; add an appropriately-sized tail indent to the body
		[[bodyTruncDict objectForKey:NSParagraphStyleAttributeName] setTailIndent:-intWidth];
	}
	
	[attributedStringPreview addAttributes:LineTruncAttributesForTitle() range:NSMakeRange(0, [self length])];
	[attributedStringPreview addAttributes:bodyTruncDict range:NSMakeRange([self length] + 1, [unattributedPreview length] - ([self length] + 1))];
	
	[unattributedPreview release];
	
	return [attributedStringPreview autorelease];
}

- (NSAttributedString*)attributedSingleLineTitle {
	//show only a single line, with a tail indent large enough for both the date and tags (if there are any)
	//because this method displays the title only, manual truncation isn't really necessary
	//the highlighted version of this string should be bolded
	
	NSMutableAttributedString *titleStr = [[NSMutableAttributedString alloc] initWithString:self attributes:LineTruncAttributesForTitle()];

	return [titleStr autorelease];
}


- (NSAttributedString*)attributedSingleLinePreviewFromBodyText:(NSAttributedString*)bodyText upToWidth:(float)upToWidth {
	
	//compute the char count for this note based on the width of the title column and the length of the receiver
	size_t bodyCharCount = EstimatedCharCountForWidth(upToWidth) - [self length];
	bodyCharCount = MIN(bodyCharCount, [bodyText length]);
	
	NSString *truncatedBodyString = [[bodyText string] truncatedPreviewStringOfLength:bodyCharCount];
	if (!truncatedBodyString) return nil;
	
	NSMutableString *unattributedPreview = [self mutableCopy];
	NSString *delimiter = NSLocalizedString(@" option-shift-dash ", @"title/description delimiter");
	[unattributedPreview appendString:delimiter];
	[unattributedPreview appendString:truncatedBodyString];
	
	NSMutableAttributedString *attributedStringPreview = [[NSMutableAttributedString alloc] initWithString:unattributedPreview attributes:LineTruncAttributes()];
	[attributedStringPreview addAttributes:GrayTextAttributes() range:NSMakeRange([self length], [unattributedPreview length] - [self length])];
	
	[unattributedPreview release];
	
	return [attributedStringPreview autorelease];
}


@end
