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


#import "LinkingEditor.h"
#import "NoteLayout.h"
#import "GlobalPrefs.h"
#import "SearchHighlight.h"
#import "AppController.h"
#import "AppController_Importing.h"
#import "NotesTableView.h"
#import "LinkingEditor_Indentation.h"
#import "NSCollection_utils.h"
#import "AttributedPlainText.h"
#import "NSString_NV.h"
#import "NVAutoPair.h"
#import "NVPasswordGenerator.h"
#import "NVThemeManager.h"
#import "MarkdownHighlight.h"
#import "CriticMarkup.h"
#import "TagSearch.h"
#import "LinkReferenceCompletion.h"
#import "ClickBelowText.h"
#import "SelectionRedraw.h"
#import "TaskList.h"
#import "ETScrollView.h"
#import "ETClipView.h"
//#import "NSTextFinder.h"
//#import "NVTextFinderAdditions.h"


#include <CoreServices/CoreServices.h>
#if MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_5
#include <Carbon/Carbon.h>
#endif

#define PASSWORD_SUGGESTIONS 0

#ifdef notyet
static long (*GetGetScriptManagerVariablePointer())(short);
#endif

#define kDefaultTextInsetWidth 8.0
#define kDefaultTextInsetHeight 16.0

@interface NSCursor (WhiteIBeamCursor)
+ (NSCursor*)whiteIBeamCursor;
@end

@implementation NSCursor (WhiteIBeamCursor)

+ (NSCursor*)whiteIBeamCursor {
	static NSCursor *invertedIBeamCursor = nil;
	if (!invertedIBeamCursor) {
		invertedIBeamCursor = [[NSCursor alloc] initWithImage:[NSImage imageNamed:@"IBeamInverted"] hotSpot:NSMakePoint(4,5)];
	}
	return invertedIBeamCursor;	
}

@end


@interface LinkingEditor (NVCompletionTheme)
- (NSColor *)nv_themeForegroundColor;
- (NSColor *)nv_linkBracketColor;
- (void)nv_applyCompletionSelectionColor;
- (void)nv_restoreSelectedTextAttributes;
- (void)nv_recolorCompletionBrackets;
- (void)nv_reapplyCompletionColors;
@end

static NSString * const NVSearchHighlightAttributeName = @"NVSearchHighlight";

@interface LinkingEditor (NVSearchHighlightOverlay)
- (void)nv_drawSearchHighlightsInOverlay:(NSView *)overlay dirtyRect:(NSRect)dirtyRect;
@end

/* The note draws search ink in a surface that sits above -drawRect:, so a chip
   painted there never appears behind the letters. This view is flipped like the
   text view and stays above that surface. */
@interface NVSearchHighlightOverlay : NSView
@end

@implementation NVSearchHighlightOverlay

- (BOOL)isFlipped {
	return YES;
}

- (BOOL)isOpaque {
	return NO;
}

- (NSView *)hitTest:(NSPoint)point {
	return nil;
}

- (void)drawRect:(NSRect)dirtyRect {
	NSView *parent = [self superview];
	if ([parent respondsToSelector:@selector(nv_drawSearchHighlightsInOverlay:dirtyRect:)])
		[(LinkingEditor *)parent nv_drawSearchHighlightsInOverlay:self dirtyRect:dirtyRect];
}

@end

@implementation LinkingEditor

@synthesize beforeString;
@synthesize afterString;
@synthesize activeParagraph;
@synthesize activeParagraphBeforeCursor;
@synthesize activeParagraphPastCursor;
@synthesize managesTextWidth;

CGFloat _perceptualDarkness(NSColor*a);

//+ (BOOL)stronglyReferencesTextStorage{
//    return NO;
//}

- (void)awakeFromNib {
	markdownHighlightDirty = NSMakeRange(NSNotFound, 0);
	markdownHighlightCoversAll = YES;
	markdownHighlightLimit = NSMakeRange(0, 0);
	plainForegroundDirty = NSMakeRange(NSNotFound, 0);

    prefsController = [GlobalPrefs defaultPrefs];

    [prefsController registerWithTarget:self forChangesInSettings:
	 @selector(setNoteBodyFont:sender:),
	 @selector(setMakeURLsClickable:sender:),
	 @selector(setSearchTermHighlightColor:sender:),
	 @selector(setShouldHighlightSearchTerms:sender:), nil];
	
	self.managesTextWidth=[prefsController managesTextWidthInWindow];
	/* The nib springs this view to the scroll view's height. A note is taller
	   than the window on its own; the scroll view must not pin it back. */
	[self setVerticallyResizable:YES];
	[self setAutoresizingMask:([self autoresizingMask] & ~NSViewHeightSizable)];
	[[self textContainer] setHeightTracksTextView:NO];
	[[self layoutManager] setBackgroundLayoutEnabled:NO];
	[self nv_matchVisibleMinHeight];
	[self setUsesRuler:NO];
	[self setUsesFontPanel:NO];
	[self setDrawsBackground:YES];
	[self updateTextColors];
    [self updateInsetAndForceLayout:YES];
    [self prepareTextFinder];
    
	[[self window] setAcceptsMouseMovedEvents:YES];
	if (IsLeopardOrLater) {
		defaultIBeamCursorIMP = method_getImplementation(class_getClassMethod([NSCursor class], @selector(IBeamCursor)));
		whiteIBeamCursorIMP = method_getImplementation(class_getClassMethod([NSCursor class], @selector(whiteIBeamCursor)));
	}
	
	didRenderFully = NO;
	automaticFirstVisibleCharIndex = NSNotFound;
	[[self layoutManager] setDelegate:self];
	[[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(noteTextStorageDidProcessEditing:) name:NSTextStorageDidProcessEditingNotification object:[self textStorage]];
	[[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(markdownCompletionPreferenceChanged:) name:NSUserDefaultsDidChangeNotification object:nil];
	[self scheduleMarkdownHighlight];


    [self bind:@"automaticQuoteSubstitutionEnabled" toObject:[NSUserDefaultsController sharedUserDefaultsController] withKeyPath:@"values.NSAutomaticQuoteSubstitutionEnabled" options:@{@"NSContinuouslyUpdatesValue":@YES}];
    [self bind:@"automaticDashSubstitutionEnabled" toObject:[NSUserDefaultsController sharedUserDefaultsController] withKeyPath:@"values.NSAutomaticDashSubstitutionEnabled" options:@{@"NSContinuouslyUpdatesValue":@YES}];
    [self bind:@"automaticTextReplacementEnabled" toObject:[NSUserDefaultsController sharedUserDefaultsController] withKeyPath:@"values.TextReplacementInNoteBody" options:@{@"NSContinuouslyUpdatesValue":@YES}];
    [self bind:@"continuousSpellCheckingEnabled" toObject:[NSUserDefaultsController sharedUserDefaultsController] withKeyPath:@"values.CheckSpellingInNoteBody" options:@{@"NSContinuouslyUpdatesValue":@YES}];
    [self bind:@"smartInsertDeleteEnabled" toObject:[NSUserDefaultsController sharedUserDefaultsController] withKeyPath:@"values.UseSmartInsertDelete" options:@{@"NSContinuouslyUpdatesValue":@YES}];


    //	NSNotificationCenter *center = [NSNotificationCenter defaultCenter];
//	[center addObserver:self selector:@selector(windowBecameOrResignedMain:) name:NSWindowDidBecomeMainNotification object:[self window]];
//	[center addObserver:self selector:@selector(windowBecameOrResignedMain:) name:NSWindowDidResignMainNotification object:[self window]];
    
	//[center addObserver:self selector:@selector(updateTextColors) name:NSSystemColorsDidChangeNotification object:nil]; // recreate gradient if needed
    //	NoMods = YES;
   
	outletObjectAwoke(self);
}

- (void)settingChangedForSelectorString:(NSString*)selectorString {
    
//    if ([selectorString isEqualToString:SEL_STR(setCheckSpellingAsYouType:sender:)]) {

//		[self setContinuousSpellCheckingEnabled:[prefsController checkSpellingAsYouType]];

//	} else if ([selectorString isEqualToString:SEL_STR(setUseTextReplacement:sender:)]) {

//		if (IsSnowLeopardOrLater) {
////			[self setAutomaticTextReplacementEnabled:[prefsController useTextReplacement]];
//		}
//    } else
if ([selectorString isEqualToString:SEL_STR(setNoteBodyFont:sender:)]) {

		[self setTypingAttributes:[prefsController noteBodyAttributes]];
		[self nv_applyThemeTypingColor];
		//[textView setFont:[prefsController noteBodyFont]];
	} else if ([selectorString isEqualToString:SEL_STR(setMakeURLsClickable:sender:)]) {
		
		[self setLinkTextAttributes:[self preferredLinkAttributes]];
    
	//} else if ([selectorString isEqualToString:SEL_STR(setBackgroundTextColor:sender:)]) {
		
		//link-color is derived both from foreground and background colors
		//[self updateTextColors];
		
	//} else if ([selectorString isEqualToString:SEL_STR(setForegroundTextColor:sender:)]) {
		
		//[self updateTextColors];
		//[self setTypingAttributes:[prefsController noteBodyAttributes]];
		
	} else if ([selectorString isEqualToString:SEL_STR(setSearchTermHighlightColor:sender:)] || 
			   [selectorString isEqualToString:SEL_STR(setShouldHighlightSearchTerms:sender:)]) {
		
		if (![prefsController highlightSearchTerms])
			[self removeHighlightedTerms];
		markdownHighlightCoversAll = YES;
		[self scheduleMarkdownHighlight];
	}
}

- (BOOL)becomeFirstResponder {
	[self discardSearchHighlights];
	[notesTableView setShouldUseSecondaryHighlightColor:YES];

	if ([[[self window] currentEvent] type] == NSKeyDown && [[[self window] currentEvent] firstCharacter] == '\t') {
		//"indicate" the current cursor/selection when moving focus to this field, but only if the user did not click here
		NSRange range = [self selectedRange];
		if (range.length) {
			range = NSMakeRange(MIN([[self string] length] - 1, range.location), range.length);
			[self performSelector:@selector(indicateRange:) withObject:[NSValue valueWithRange:range] afterDelay:0];
		}
	}
	[self setTypingAttributes:[prefsController noteBodyAttributes]];
	[self nv_applyThemeTypingColor];
//    NSLog(@"exist :>%@<",self.defaultParagraphStyle);
//    [self.textStorage setAttributes:@{NSParagraphStyleAttributeName:[NSParagraphStyle defaultParagraphStyle]} range:NSMakeRange(0, self.string.length)];
//    NSLog(@"exist :>%@<",self.defaultParagraphStyle);
	[self performSelector:@selector(_fixCursorForBackgroundUpdatingMouseInside:) withObject:[NSNumber numberWithBool:YES] afterDelay:0.0];
	
	return [super becomeFirstResponder];
}

- (void)indicateRange:(NSValue*)rangeValue {
	if (IsLeopardOrLater) {
		[self showFindIndicatorForRange:[rangeValue rangeValue]];
	}
}

- (BOOL)resignFirstResponder {
	[notesTableView setShouldUseSecondaryHighlightColor:NO];
	
	[self performSelector:@selector(_fixCursorForBackgroundUpdatingMouseInside:) withObject:[NSNumber numberWithBool:YES] afterDelay:0.0];
	
	return [super resignFirstResponder];
}

- (void)changeColor:(id)sender {
	//NSLog(@"You do not change the color.");
	return;
}

- (void)setBackgroundColor:(NSColor*)aColor {
	backgroundIsDark = (_perceptualDarkness([aColor colorUsingColorSpaceName:NSCalibratedRGBColorSpace]) > 0.5);
    [self fixCursorForBackgroundUpdatingMouseInside:YES];
	[super setBackgroundColor:aColor];
}

- (void)updateTextColors {
	NVEditorTheme *editorTheme = [[NVThemeManager sharedManager] currentEditorTheme];
	if (editorTheme) {
		NSColor *appFG = [(AppController *)[NSApp delegate] foregrndColor];
		NSColor *appBG = [(AppController *)[NSApp delegate] backgrndColor];
		NSColor *fgColor = [editorTheme editorForegroundColor] ?: appFG ?: [NSColor textColor];
		NSColor *bgColor = [editorTheme editorBackgroundColor] ?: appBG ?: [NSColor textBackgroundColor];
		[self setBackgroundColor:bgColor];
		[[self enclosingScrollView] setBackgroundColor:bgColor];
		[[[self enclosingScrollView] contentView] setBackgroundColor:bgColor];
		[[[self enclosingScrollView] contentView] setDrawsBackground:YES];
		if (IsLionOrLater && [self textFinderIsVisible])
			[[self window] invalidateCursorRectsForView:[[self enclosingScrollView] findBarView]];
		NSColor *caret = [editorTheme editorCaretColor] ?: [self _insertionPointColorForForegroundColor:fgColor backgroundColor:bgColor];
		NSColor *selection = [editorTheme editorSelectionColor] ?: [self _selectionColorForForegroundColor:fgColor backgroundColor:bgColor];
		[self setInsertionPointColor:caret];
		[self setTextColor:fgColor];
		[self setLinkTextAttributes:[self preferredLinkAttributes]];
		[self setSelectedTextAttributes:[NSDictionary dictionaryWithObject:selection forKey:NSBackgroundColorAttributeName]];
		if (completionSelectionColored)
			[self nv_applyCompletionSelectionColor];
		[self setTypingAttributes:[prefsController noteBodyAttributes]];
		[self nv_applyThemeTypingColor];
		[[self enclosingScrollView] setNeedsDisplay:YES];
		[self setNeedsDisplay:YES];
		markdownHighlightCoversAll = YES;
		[self highlightMarkdownSyntax];
		if ([[self enclosingScrollView] respondsToSelector:@selector(nv_updateScrollerChrome)])
			[(ETScrollView *)[self enclosingScrollView] nv_updateScrollerChrome];
		return;
	}
	NSColor *fgColor = [(AppController *)[NSApp delegate] foregrndColor];
	NSColor *bgColor = [self backgroundColor];
    if (bgColor!=[(AppController *)[NSApp delegate]backgrndColor]) {
        bgColor=[(AppController *)[NSApp delegate]backgrndColor];
        [self setBackgroundColor:bgColor];
    }
	[[self enclosingScrollView] setBackgroundColor:bgColor];
	[[[self enclosingScrollView] contentView] setBackgroundColor:bgColor];
	[[[self enclosingScrollView] contentView] setDrawsBackground:YES];
    if (IsLionOrLater&&[self textFinderIsVisible]) {
        [[self window]invalidateCursorRectsForView:[[self enclosingScrollView]findBarView]];
    }
    
	//[self setBackgroundColor:bgColor];
	//[nvTextScroller setBackgroundColor:bgColor];
	//[[self enclosingScrollView] setNeedsDisplay:YES];
    
	[self setInsertionPointColor:[self _insertionPointColorForForegroundColor:fgColor backgroundColor:bgColor]];
	if (fgColor)
		[self setTextColor:fgColor];
	[self setLinkTextAttributes:[self preferredLinkAttributes]];
	[self setSelectedTextAttributes:[NSDictionary dictionaryWithObject:[self _selectionColorForForegroundColor:fgColor backgroundColor:bgColor] 
																forKey:NSBackgroundColorAttributeName]];
	if (completionSelectionColored)
		[self nv_applyCompletionSelectionColor];
	[self setTypingAttributes:[prefsController noteBodyAttributes]];
	[self nv_applyThemeTypingColor];
    [[self enclosingScrollView]setNeedsDisplay:YES];
//    [[[self enclosingScrollView]contentView]setNeedsDisplay:YES];
    [self setNeedsDisplay:YES];
	markdownHighlightCoversAll = YES;
	[self scheduleMarkdownHighlight];
	if ([[self enclosingScrollView] respondsToSelector:@selector(nv_updateScrollerChrome)])
		[(ETScrollView *)[self enclosingScrollView] nv_updateScrollerChrome];
}

#define _CM(__ch) ((__ch) * 255.0)
CGFloat _perceptualDarkness(NSColor*a) {
	//0 to 1; the higher the darker
	
	CGFloat aRed, aGreen, aBlue;
	[a getRed:&aRed green:&aGreen blue:&aBlue alpha:NULL];

	return 1 - (0.299 * _CM(aRed) + 0.587 * _CM(aGreen) + 0.114 * _CM(aBlue))/255;
}
CGFloat _perceptualColorDifference(NSColor*a, NSColor*b) {
	//acceptable: 500
	CGFloat aRed, aGreen, aBlue, bRed, bGreen, bBlue;
	[a getRed:&aRed green:&aGreen blue:&aBlue alpha:NULL];
	[b getRed:&bRed green:&bGreen blue:&bBlue alpha:NULL];

	return (MAX(_CM(aRed), _CM(bRed)) - MIN(_CM(aRed), _CM(bRed))) + (MAX(_CM(aGreen), _CM(bGreen)) - MIN(_CM(aGreen), _CM(bGreen))) + 
	(MAX(_CM(aBlue), _CM(bBlue)) - MIN(_CM(aBlue), _CM(bBlue)));
}

- (NSColor*)_linkColorForForegroundColor:(NSColor*)fgColor backgroundColor:(NSColor*)bgColor {
	//if fgColor is black, choose blue; otherwise, rotate hue (keeping the same sat.) until color is different enough
	
	fgColor = [fgColor colorUsingColorSpaceName:NSCalibratedRGBColorSpace];
	bgColor = [bgColor colorUsingColorSpaceName:NSCalibratedRGBColorSpace];
	
	CGFloat hue, brightness, saturation, alpha, diffInc = 0.5;
	NSUInteger rotationsLeft = 25;
	[fgColor getHue:&hue saturation:&saturation brightness:&brightness alpha:&alpha];

	//if foreground color is too dark for hue changes to matter, then just use blue
	if (brightness <= 0.24)
		return [NSColor blueColor];
	
	brightness = _perceptualDarkness(bgColor) > 0.5 ? MAX(0.75, brightness) : MIN(0.35, brightness);
	
	saturation = MAX(0.5, saturation);
	
	//adjust hue until the perceptual differences between the proposed link
	//and current foreground and background colors are great enough
	NSColor *proposedLinkColor = nil;
	do {
		hue -= diffInc;
		if (hue < 0.0)
			hue += 1.0;
		
		proposedLinkColor = [NSColor colorWithCalibratedHue:hue saturation:saturation brightness:brightness alpha:alpha];
		
		diffInc = rotationsLeft > 15 ? 0.125 : 0.0625;
		
	} while ((_perceptualColorDifference(proposedLinkColor, bgColor) < 360.0 || 
			  _perceptualColorDifference(proposedLinkColor, fgColor) < 170.0) && --rotationsLeft > 0);
	return proposedLinkColor;
}

- (NSColor*)_selectionColorForForegroundColor:(NSColor*)fgColor backgroundColor:(NSColor*)bgColor {
	fgColor = [fgColor colorUsingColorSpaceName:NSCalibratedRGBColorSpace];
	bgColor = [bgColor colorUsingColorSpaceName:NSCalibratedRGBColorSpace];
    
	NSColor *proposedBlend = [fgColor blendedColorWithFraction:0.5 ofColor:bgColor];
	NSColor *defaultColor = [[NSColor selectedTextBackgroundColor] colorUsingColorSpaceName:NSCalibratedRGBColorSpace];
	
	CGFloat fgDiff = _perceptualColorDifference(proposedBlend, fgColor);
	CGFloat fgSelDiff = _perceptualColorDifference(defaultColor, fgColor);
	
	//selection color should be between foreground and background in terms of brightness
	//but the selection-color-difference from the foreground text needs to be great enough as well,
	//and the proposed-color-difference from the foreground can't be too poor
	//this heuristic chooses all the system-highlight colors in default fg/bg combinations and fg/bg blends in all others
    
	if ((_perceptualDarkness(fgColor) > _perceptualDarkness(defaultColor) &&
		 _perceptualDarkness(defaultColor) > (_perceptualDarkness(bgColor)+0.22) && fgSelDiff > 300.0) || fgDiff < 160.0){
		return defaultColor;
	}
	//amplify the background balance after testing
	return [fgColor blendedColorWithFraction:0.69 ofColor:bgColor];
}


- (NSColor*)_insertionPointColorForForegroundColor:(NSColor*)fgColor backgroundColor:(NSColor*)bgColor {
	fgColor = [fgColor colorUsingColorSpaceName:NSCalibratedRGBColorSpace];
	bgColor = [bgColor colorUsingColorSpaceName:NSCalibratedRGBColorSpace];

	CGFloat hue, brightness, saturation;
	[fgColor getHue:&hue saturation:&saturation brightness:&brightness alpha:NULL];
	
	//make the insertion point lighter than the foreground color if the background is dark and vise versa
	NSColor *brighter = [fgColor blendedColorWithFraction:0.4 ofColor:[NSColor whiteColor]];
	NSColor *darker = [fgColor blendedColorWithFraction:0.4 ofColor:[NSColor blackColor]];

	return _perceptualColorDifference(brighter, bgColor) > _perceptualColorDifference(darker, bgColor) ? brighter : darker;
}

- (NSDictionary*)preferredLinkAttributes {
	if (![prefsController URLsAreClickable])
		return [NSDictionary dictionary];
	
	NSColor *fgColor = [(AppController *)[NSApp delegate] foregrndColor];
	NSColor *bgColor = [(AppController *)[NSApp delegate] backgrndColor];
	NVEditorTheme *editorTheme = [[NVThemeManager sharedManager] currentEditorTheme];
	NSColor *linkColor = nil;
	if (editorTheme) {
		fgColor = [editorTheme editorForegroundColor] ?: fgColor;
		bgColor = [editorTheme editorBackgroundColor] ?: bgColor;
		linkColor = [[editorTheme styleForKey:@"linkParens"] foregroundColor];
	}
	if (!linkColor)
		linkColor = [self _linkColorForForegroundColor:fgColor backgroundColor:bgColor];
	return [NSDictionary dictionaryWithObjectsAndKeys:
			[NSCursor pointingHandCursor], NSCursorAttributeName,
			[NSNumber numberWithInt:NSUnderlineStyleSingle], NSUnderlineStyleAttributeName,
			linkColor,
			NSForegroundColorAttributeName, nil];
	
	/*
	 return [NSDictionary dictionaryWithObjectsAndKeys:
	 [NSCursor pointingHandCursor], NSCursorAttributeName,
	 [NSNumber numberWithInt:NSUnderlineStyleSingle], NSUnderlineStyleAttributeName,
	 [self _linkColorForForegroundColor:[prefsController foregroundTextColor] backgroundColor:[prefsController backgroundTextColor]],
	 NSForegroundColorAttributeName, nil];
	 */
}

/*
- (BOOL)acceptsFirstResponder {
	
    return ([[controlField stringValue] length] > 0);
}*/

//- (void)toggleAutomaticTextReplacement:(id)sender {
//	
//	[super toggleAutomaticTextReplacement:sender];
//	
//	[prefsController setUseTextReplacement:[self isAutomaticTextReplacementEnabled] sender:self];
//}
//
//- (void)toggleContinuousSpellChecking:(id)sender {
//
//	[super toggleContinuousSpellChecking:sender];
//	
//	[prefsController setCheckSpellingAsYouType:[self isContinuousSpellCheckingEnabled] sender:self];
//}

//- (BOOL)isContinuousSpellCheckingEnabled {
//	//optimization so that we don't spell-check while scrolling through notes that don't have focus
//
//    NSView *responder = (NSView*)[[self window] firstResponder];
//    return (responder == self && [super isContinuousSpellCheckingEnabled]);
//}

- (void)checkTextInRange:(NSRange)range types:(NSTextCheckingTypes)checkingTypes options:(NSDictionary *)options{
    //	//optimization so that we don't spell-check while scrolling through notes that don't have focus
    if ([[self window] firstResponder]!=self) {
        return;
    }

    [super checkTextInRange:range types:checkingTypes options:options];
}


- (BOOL)didRenderFully {
	return didRenderFully;
}

- (void)redisplayAfterNoteChange {
	/* AppKit invalidates glyphs that still exist. A shorter note, such as one
	   opened from a link, leaves the previous note's tail on screen. */
	markdownHighlightSuppressed = NO;
	markdownHighlightCoversAll = YES;
	markdownHighlightDirty = NSMakeRange(NSNotFound, 0);
	NSLayoutManager *layout = [self layoutManager];
	NSUInteger length = [[self string] length];
	if (layout && length)
		[layout invalidateDisplayForCharacterRange:NSMakeRange(0, length)];
	NSRect visible = [self visibleRect];
	if (NSIsEmptyRect(visible))
		visible = [self bounds];
	[self setNeedsDisplayInRect:visible avoidAdditionalLayout:NO];
	[[[self enclosingScrollView] contentView] setNeedsDisplay:YES];
}

- (void)layoutManager:(NSLayoutManager *)aLayoutManager didCompleteLayoutForTextContainer:(NSTextContainer *)aTextContainer atEnd:(BOOL)flag {
	/* A finished container is often only the window tall. Resizing here is
	   what made the scrollbar vanish again on the way back to a note. */
	(void)flag;
	didRenderFully = YES;
}

static NSColor *NVSearchHighlightInkColor(NSColor *highlight) {
	static NSColor *blackInk = nil;
	static NSColor *whiteInk = nil;
	CGFloat red = 0, green = 0, blue = 0, alpha = 0;
	NSColor *rgb = [highlight colorUsingColorSpace:[NSColorSpace sRGBColorSpace]];
	if (!rgb)
		return nil;
	[rgb getRed:&red green:&green blue:&blue alpha:&alpha];
	if (!blackInk) {
		blackInk = [[NSColor colorWithSRGBRed:0 green:0 blue:0 alpha:1] retain];
		whiteInk = [[NSColor colorWithSRGBRed:1 green:1 blue:1 alpha:1] retain];
	}
	return NVSearchHighlightLuminance(red, green, blue) <= 0.55 ? whiteInk : blackInk;
}

- (NSDictionary *)layoutManager:(NSLayoutManager *)layoutManager shouldUseTemporaryAttributes:(NSDictionary *)attrs forDrawingToScreen:(BOOL)toScreen atCharacterIndex:(NSUInteger)charIndex effectiveRange:(NSRangePointer)effectiveCharRange {
	if (!toScreen)
		return attrs;
	if (completionSelectionColored && completionColoredRange.length && NSLocationInRange(charIndex, completionColoredRange)) {
		NSColor *color = [self nv_themeForegroundColor];
		if (!color)
			return attrs;
		NSMutableDictionary *modified = attrs ? [[attrs mutableCopy] autorelease] : [NSMutableDictionary dictionary];
		[modified setObject:color forKey:NSForegroundColorAttributeName];
		if (effectiveCharRange)
			*effectiveCharRange = NSMakeRange(charIndex, 1);
		return modified;
	}
	/* Ink is chosen only while drawing. Storing it would replace the theme color,
	   and taking it away when the letters stop matching leaves the default black. */
	if ([attrs objectForKey:NVSearchHighlightAttributeName]) {
		NSColor *ink = NVSearchHighlightInkColor([attrs objectForKey:NSBackgroundColorAttributeName]);
		if (!ink)
			return attrs;
		NSMutableDictionary *modified = [[attrs mutableCopy] autorelease];
		[modified setObject:ink forKey:NSForegroundColorAttributeName];
		return modified;
	}
	return attrs;
}
- (void)layoutManagerDidInvalidateLayout:(NSLayoutManager *)aLayoutManager {
	didRenderFully = NO;	
}

- (BOOL)readSelectionFromPasteboard:(NSPasteboard *)pboard type:(NSString *)type {
	//NSLog(@"readSelectionFromPasteboard: %@ (total %@)", type, [[pboard types] description]);
	
	if ([type isEqualToString:NSFilenamesPboardType]) {
		//paste as a file:// URL, so that it can be linked
		NSString *allURLsString = [(AppController *)[NSApp delegate] stringWithNoteURLsOnPasteboard:pboard];
		
		if ([allURLsString length]) {
			NSRange selectedRange = [self rangeForUserTextChange];
			if ([self shouldChangeTextInRange:selectedRange replacementString:allURLsString]) {
				[self replaceCharactersInRange:selectedRange withString:allURLsString];
				[self didChangeText];
				
				return YES;
			}
		}
	}
	
	if ([type isEqualToString:NSRTFPboardType] || [type isEqualToString:NVPTFPboardType] || [type isEqualToString:NSHTMLPboardType]) {
		//strip formatting if RTF and stick it into a new pboard
		
		NSMutableAttributedString *newString = [[[NSMutableAttributedString alloc] performSelector:[type isEqualToString:NSHTMLPboardType] ? 
												 @selector(initWithHTML:documentAttributes:) : @selector(initWithRTF:documentAttributes:) 
																						withObject:[pboard dataForType:type] withObject:nil] autorelease];
		if ([newString length]) {
			if (![type isEqualToString:NVPTFPboardType]) {
							//remove the link attribute, because it will be re-added after we paste, and restyleText would preserve it otherwise
							//and we only want real URLs to be linked
							[newString removeAttribute:NSLinkAttributeName range:NSMakeRange(0, [newString length])];
							[newString indentTextLists];
							[newString restyleTextToFont:[prefsController noteBodyFont] usingBaseFont:nil];
						}

						NSRange selectedRange = [self rangeForUserTextChange];
						if ([self shouldChangeTextInRange:selectedRange replacementString:[newString string]]) {

							[self replaceCharactersInRange:selectedRange withRTF:[newString RTFFromRange:
																				  NSMakeRange(0, [newString length]) documentAttributes:nil]];

							//paragraph styles will ALWAYS be added _after_ replaceCharactersInRange, it seems
							//[[self textStorage] removeAttribute:NSParagraphStyleAttributeName range:NSMakeRange(0, [[self string] length])];
							[self didChangeText];

							return YES;
						}
		}
	}
	
	return [super readSelectionFromPasteboard:pboard type:type];
}

- (NSArray *)acceptableDragTypes {
	
	return [self readablePasteboardTypes];
}

- (NSArray *)readablePasteboardTypes {
	NSMutableArray *types = [NSMutableArray arrayWithObjects:NSFilenamesPboardType, NVPTFPboardType, NSStringPboardType, nil];
	
	if ([prefsController pastePreservesStyle]) {
		[types insertObject:NSRTFPboardType atIndex:2];
		[types insertObject:NSHTMLPboardType atIndex:3];
	}
	
	return types;
}

- (BOOL)writeSelectionToPasteboard:(NSPasteboard *)pboard type:(NSString *)type {
	
	if ([type isEqualToString:NVPTFPboardType] || [type isEqualToString:NSRTFPboardType]) {
		//always preserve RTF to allow pasting into ourselves; prejudice against external sources
		
		NSMutableAttributedString *newString = [[[self textStorage] attributedSubstringFromRange:[self selectedRange]] mutableCopy];
		
		if (![type isEqualToString:NVPTFPboardType])
			[newString removeAttribute:NSForegroundColorAttributeName range:NSMakeRange(0, [newString length])];

		NSData *rtfData = [newString RTFFromRange:NSMakeRange(0, [newString length]) documentAttributes:nil];;
		if (rtfData) [pboard setData:rtfData forType:type];
		[newString release];
		return YES;
	}
	
	return [super writeSelectionToPasteboard:pboard type:type];
}

#define COPY_PASTE_DEBUG 0

- (NSArray *)writablePasteboardTypes {
	NSMutableArray *types = [NSMutableArray arrayWithObjects:NVPTFPboardType, NSStringPboardType, nil];
	
	NSRange selectedRange = [self selectedRange];
	if (selectedRange.length) {
		
		NSRange firstAttributeRange;
		[[self textStorage] attributesAtIndex:selectedRange.location effectiveRange:&firstAttributeRange];
		if (firstAttributeRange.length < selectedRange.length) {
			//there are multiple styles across the selected text
			
			NSAttributedString *newString = [[self textStorage] attributedSubstringFromRange:selectedRange];
			NSRange effectiveRange = NSMakeRange(0,0);
			NSUInteger stringLength = [newString length];
			
			//iterate over all styles; if any are acceptable, copy as RTF
			while (NSMaxRange(effectiveRange) < stringLength) {
				// Get the attributes for the current range
				NSDictionary *attributes = [newString attributesAtIndex:NSMaxRange(effectiveRange) effectiveRange:&effectiveRange];
				
				if ([attributes attributesHaveFontTrait:NSBoldFontMask orAttribute:NSStrokeWidthAttributeName])
					goto copyRTFType;
				if ([attributes attributesHaveFontTrait:NSItalicFontMask orAttribute:NSObliquenessAttributeName])
					goto copyRTFType;
				if ([attributes attributesHaveFontTrait:0 orAttribute:NSStrikethroughStyleAttributeName])
					goto copyRTFType;
			}
#if COPY_PASTE_DEBUG
			NVLogInfo(@"false alarm: no real styles");
#endif
			
		} else {
#if COPY_PASTE_DEBUG
			NVLogInfo(@"homogeneous style");
#endif
		}
		
		if (0) {
copyRTFType:
			//we have more than a single styling segment within the selection--grudgingly allow regular RTF copying
#if COPY_PASTE_DEBUG
			NVLogInfo(@"copying RTF due to multiple attributes");
			[[self layoutManager] addTemporaryAttributes:[prefsController searchTermHighlightAttributes] forCharacterRange:effectiveRange];
#endif
			[types insertObject:NSRTFPboardType atIndex:1];
		}
	}
	
	return types;
}

//font panel is disabled for the note-body, so styles must be applied manually:

- (void)strikethroughNV:(id)sender {

	[self applyStyleOfTrait:0 alternateAttributeName:NSStrikethroughStyleAttributeName 
	alternateAttributeValue:[NSNumber numberWithInt:NSUnderlineStyleSingle]];
	
	[[self undoManager] setActionName:NSLocalizedString(@"Strikethrough",nil)];
}

#define STROKE_WIDTH_FOR_BOLD (-3.50f)
#define OBLIQUENESS_FOR_ITALIC (0.20f)



- (void)bold:(id)sender {	
    if ([[NSUserDefaults standardUserDefaults]boolForKey:@"UsesMarkdownCompletions"]) {
        [self changeMarkdownAttribute:[prefsController markdownBoldMarker]];
    }else{
        [self applyStyleOfTrait:NSBoldFontMask alternateAttributeName:NSStrokeWidthAttributeName 
        alternateAttributeValue:[NSNumber numberWithFloat:(float)STROKE_WIDTH_FOR_BOLD]];
        [[self undoManager] setActionName:NSLocalizedString(@"Bold",nil)];
	}
}

- (void)italic:(id)sender {
    if ([[NSUserDefaults standardUserDefaults]boolForKey:@"UsesMarkdownCompletions"]) {
        [self changeMarkdownAttribute:[prefsController markdownItalicMarker]];
    }else{
        [self applyStyleOfTrait:NSItalicFontMask alternateAttributeName:NSObliquenessAttributeName 
        alternateAttributeValue:[NSNumber numberWithFloat:(float)OBLIQUENESS_FOR_ITALIC]];
        
        [[self undoManager] setActionName:NSLocalizedString(@"Italic",nil)];
    }
}

- (void)applyStyleOfTrait:(NSFontTraitMask)trait alternateAttributeName:(NSString*)attrName alternateAttributeValue:(id)value {
	
	NSFont *font = nil;
	NSMutableDictionary *attributes = nil;
	BOOL hasTrait = NO;
	
	if ([self selectedRange].length) {
		NSRange limitRange, effectiveRange;
		NSTextStorage *text = [self textStorage];
		limitRange = [self selectedRange];
		
		if ([self shouldChangeTextInRange:limitRange replacementString:nil]) {
			
			NSDictionary *firstAttrs = [text attributesAtIndex:limitRange.location longestEffectiveRange:NULL inRange:limitRange];
			hasTrait = [firstAttrs attributesHaveFontTrait:trait orAttribute:attrName];
			
			[text beginEditing];
			while (limitRange.length > 0) {
				attributes = [[text attributesAtIndex:limitRange.location longestEffectiveRange:&effectiveRange 
											  inRange:limitRange] mutableCopyWithZone:nil];
				if (!attributes) attributes = [[prefsController noteBodyAttributes] mutableCopyWithZone:nil];
				font = [attributes objectForKey:NSFontAttributeName];
				
				[attributes applyStyleInverted:hasTrait trait:trait forFont:font alternateAttributeName:attrName alternateAttributeValue:value];
				[text setAttributes:attributes range:effectiveRange];
				[attributes release];
				
				limitRange = NSMakeRange( NSMaxRange( effectiveRange ), NSMaxRange( limitRange ) - NSMaxRange( effectiveRange ) );
			}
			[text endEditing];
			[self didChangeText];
		}
	} else {
		attributes = [[self typingAttributes] mutableCopyWithZone:nil];
		if (!attributes) attributes = [[prefsController noteBodyAttributes] mutableCopyWithZone:nil];
		font = [attributes objectForKey:NSFontAttributeName];
		
		hasTrait = [attributes attributesHaveFontTrait:trait orAttribute:attrName];
		[attributes applyStyleInverted:hasTrait trait:trait forFont:font alternateAttributeName:attrName alternateAttributeValue:value];
		[self setTypingAttributes:attributes];
		
		[attributes release];
	}
	
}

- (void)removeHighlightedTerms {
	[[self layoutManager] removeTemporaryAttribute:NSBackgroundColorAttributeName forCharacterRange:NSMakeRange(0, [[self string] length])];
}

static BOOL NVHighlightComponentsMatch(CGFloat a, CGFloat b) {
	CGFloat delta = a - b;
	if (delta < 0)
		delta = -delta;
	return delta < 0.02f;
}

static BOOL NVSearchHighlightColorMatches(NSColor *applied, NSColor *searchColor) {
	if (applied == searchColor)
		return YES;
	if (!applied || !searchColor)
		return NO;
	NSColor *appliedRGB = [applied colorUsingColorSpace:[NSColorSpace sRGBColorSpace]];
	NSColor *searchRGB = [searchColor colorUsingColorSpace:[NSColorSpace sRGBColorSpace]];
	if (!appliedRGB || !searchRGB)
		return [applied isEqual:searchColor];
	return NVHighlightComponentsMatch([appliedRGB redComponent], [searchRGB redComponent]) &&
		NVHighlightComponentsMatch([appliedRGB greenComponent], [searchRGB greenComponent]) &&
		NVHighlightComponentsMatch([appliedRGB blueComponent], [searchRGB blueComponent]) &&
		NVHighlightComponentsMatch([appliedRGB alphaComponent], [searchRGB alphaComponent]);
}

- (NSDictionary *)nv_searchHighlightAttributes {
	NSDictionary *base = [prefsController searchTermHighlightAttributes];
	if (!base)
		return nil;
	NSMutableDictionary *attributes = [[base mutableCopy] autorelease];
	[attributes removeObjectForKey:NSForegroundColorAttributeName];
	[attributes setObject:[NSNumber numberWithBool:YES] forKey:NVSearchHighlightAttributeName];
	return attributes;
}

- (NVSearchHighlightOverlay *)nv_searchHighlightOverlayCreating:(BOOL)create {
	NVSearchHighlightOverlay *overlay = nil;
	NSArray *subviews = [self subviews];
	NSUInteger i;
	for (i = 0; i < [subviews count]; i++) {
		NSView *view = [subviews objectAtIndex:i];
		if ([view isKindOfClass:[NVSearchHighlightOverlay class]])
			overlay = (NVSearchHighlightOverlay *)view;
	}
	if (!overlay && create) {
		overlay = [[NVSearchHighlightOverlay alloc] initWithFrame:[self bounds]];
		[overlay setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
		[self addSubview:overlay];
		[overlay release];
	}
	if (overlay && !NSEqualRects([overlay frame], [self bounds]))
		[overlay setFrame:[self bounds]];
	return overlay;
}

- (void)nv_displaySearchHighlights {
	NVSearchHighlightOverlay *overlay = [self nv_searchHighlightOverlayCreating:YES];
	[overlay setNeedsDisplay:YES];
}

- (void)didAddSubview:(NSView *)subview {
	[super didAddSubview:subview];
	if ([subview isKindOfClass:[NVSearchHighlightOverlay class]])
		return;
	NVSearchHighlightOverlay *overlay = [self nv_searchHighlightOverlayCreating:NO];
	if (overlay)
		[self addSubview:overlay];
}

- (void)setBoundsOrigin:(NSPoint)newOrigin {
	[super setBoundsOrigin:newOrigin];
	NVSearchHighlightOverlay *overlay = [self nv_searchHighlightOverlayCreating:NO];
	if (overlay)
		[overlay setFrame:[self bounds]];
}

- (void)setFrameSize:(NSSize)newSize {
	/* AppKit offers the window height while switching notes. Taking it hides
	   the scrollbar for a note that is already taller than the window. */
	newSize.height = NVNoteHeightForProposedHeight(self, newSize.height);
	[super setFrameSize:newSize];
	NVSearchHighlightOverlay *overlay = [self nv_searchHighlightOverlayCreating:NO];
	if (overlay)
		[overlay setFrame:[self bounds]];
}

- (void)setConstrainedFrameSize:(NSSize)size {
	size.height = NVNoteHeightForProposedHeight(self, size.height);
	[super setConstrainedFrameSize:size];
}

- (void)nv_fillSearchHighlightRange:(NSRange)slice color:(NSColor *)color inOverlay:(NSView *)overlay {
	NSLayoutManager *layout = [self layoutManager];
	NSTextContainer *container = [self textContainer];
	if (!layout || !container || !overlay || !slice.length || !color)
		return;
	NSRange glyphs = [layout glyphRangeForCharacterRange:slice actualCharacterRange:NULL];
	if (!glyphs.length)
		return;
	NSPoint containerOrigin = [self textContainerOrigin];
	NSPoint frameOrigin = [overlay frame].origin;
	NSPoint origin = NSMakePoint(containerOrigin.x - frameOrigin.x, containerOrigin.y - frameOrigin.y);
	NSRect rect = [layout boundingRectForGlyphRange:glyphs inTextContainer:container];
	rect.origin.x += origin.x;
	rect.origin.y += origin.y;
	[color setFill];
	NSRectFillUsingOperation(rect, NSCompositingOperationSourceOver);
	[layout drawGlyphsForGlyphRange:glyphs atPoint:origin];
}

- (void)nv_paintSearchHighlightSlice:(NSRange)slice avoidingRange:(NSRange)selected color:(NSColor *)color inOverlay:(NSView *)overlay {
	if (!slice.length)
		return;
	if (selected.length && selected.location != NSNotFound) {
		NSRange overlap = NSIntersectionRange(slice, selected);
		if (overlap.length) {
			if (overlap.location > slice.location)
				[self nv_fillSearchHighlightRange:NSMakeRange(slice.location, overlap.location - slice.location) color:color inOverlay:overlay];
			if (NSMaxRange(overlap) < NSMaxRange(slice))
				[self nv_fillSearchHighlightRange:NSMakeRange(NSMaxRange(overlap), NSMaxRange(slice) - NSMaxRange(overlap)) color:color inOverlay:overlay];
			return;
		}
	}
	[self nv_fillSearchHighlightRange:slice color:color inOverlay:overlay];
}

- (void)nv_drawSearchHighlightsInOverlay:(NSView *)overlay dirtyRect:(NSRect)dirtyRect {
	if (!prefsController || ![prefsController highlightSearchTerms])
		return;
	NSLayoutManager *layout = [self layoutManager];
	NSTextContainer *container = [self textContainer];
	if (!layout || !container)
		return;
	NSPoint frameOrigin = [overlay frame].origin;
	NSPoint containerOrigin = [self textContainerOrigin];
	NSRect containerRect = dirtyRect;
	containerRect.origin.x += frameOrigin.x - containerOrigin.x;
	containerRect.origin.y += frameOrigin.y - containerOrigin.y;
	containerRect = NSInsetRect(containerRect, -4.0, -4.0);
	NSRange glyphs = [layout glyphRangeForBoundingRect:containerRect inTextContainer:container];
	if (!glyphs.length)
		return;
	NSRange chars = [layout characterRangeForGlyphRange:glyphs actualGlyphRange:NULL];
	NSRange selected = [self selectedRange];
	NSUInteger index = chars.location;
	NSUInteger end = NSMaxRange(chars);
	while (index < end) {
		NSRange effective = NSMakeRange(0, 0);
		NSDictionary *attributes = [layout temporaryAttributesAtCharacterIndex:index effectiveRange:&effective];
		NSRange slice = NSIntersectionRange(effective, chars);
		NSColor *color = [attributes objectForKey:NSBackgroundColorAttributeName];
		if (slice.length && [attributes objectForKey:NVSearchHighlightAttributeName] && color)
			[self nv_paintSearchHighlightSlice:slice avoidingRange:selected color:color inOverlay:overlay];
		NSUInteger next = NSMaxRange(effective);
		if (next <= index)
			break;
		index = next;
	}
}

- (void)removeSearchTermHighlights {
	NSColor *searchColor = [[prefsController searchTermHighlightAttributes] objectForKey:NSBackgroundColorAttributeName];
	NSLayoutManager *layout = [self layoutManager];
	NSUInteger length = [[self string] length];
	if (!searchColor || !layout || !length)
		return;
	NSUInteger index = 0;
	while (index < length) {
		NSRange effective = NSMakeRange(0, 0);
		NSDictionary *attrs = [layout temporaryAttributesAtCharacterIndex:index effectiveRange:&effective];
		if (!effective.length)
			break;
		NSColor *background = [attrs objectForKey:NSBackgroundColorAttributeName];
		if ([attrs objectForKey:NVSearchHighlightAttributeName] || NVSearchHighlightColorMatches(background, searchColor)) {
			[layout removeTemporaryAttribute:NSBackgroundColorAttributeName forCharacterRange:effective];
			[layout removeTemporaryAttribute:NVSearchHighlightAttributeName forCharacterRange:effective];
		}
		NSUInteger next = NSMaxRange(effective);
		if (next <= index)
			break;
		index = next;
	}
	[self nv_displaySearchHighlights];
}

- (void)discardSearchHighlights {
	NSLayoutManager *layout = [self layoutManager];
	NSUInteger length = [[self string] length];
	if (layout && length) {
		NSRange all = NSMakeRange(0, length);
		[layout removeTemporaryAttribute:NSBackgroundColorAttributeName forCharacterRange:all];
		[layout removeTemporaryAttribute:NVSearchHighlightAttributeName forCharacterRange:all];
	}
	[self nv_displaySearchHighlights];
	/* Background colors for the whole note were just removed. */
	markdownHighlightCoversAll = YES;
	[self scheduleMarkdownHighlight];
}


//use with rangesOfWordsInString:(NSString*)findString earliestRange:(NSRange*)aRange inRange:
- (void)highlightRangesTemporarily:(CFArrayRef)ranges {
	CFIndex rangeIndex;
	long bodyLength = (long)[[self string] length];
	NSDictionary *highlightDict = [self nv_searchHighlightAttributes];
	
	for (rangeIndex = 0; rangeIndex < CFArrayGetCount(ranges); rangeIndex++) {
		CFRange *range = (CFRange *)CFArrayGetValueAtIndex(ranges, rangeIndex);
		
		if (range && range->length > 0 && range->location + range->length <= bodyLength) {
			[[self layoutManager] addTemporaryAttributes:highlightDict forCharacterRange:*(NSRange*)range];
		} else {
			NVLogWarning(@"highlightRangesTemporarily: Invalid range (%@)", range ? NSStringFromRange(*(NSRange*)range) : @"null");
		}
	}
	[self nv_displaySearchHighlights];
}

- (NSArray*)rangesOfSearchTerms:(NSString*)typedString {
	//every place a term of the search appears in the body, in order of position; quoted searches are one term
	NSMutableArray *result = [NSMutableArray array];
	CFStringRef quoteStr = CFSTR("\"");
	CFRange quoteRange = CFStringFind((CFStringRef)typedString, quoteStr, 0);
	CFArrayRef terms = CFStringCreateArrayBySeparatingStrings(NULL, (CFStringRef)typedString, 
															  quoteRange.location == kCFNotFound ? CFSTR(" ") : quoteStr);
	if (terms) {
		CFIndex termIndex, rangeIndex;
		CFStringRef bodyString = (CFStringRef)[self string];
		CFIndex bodyLength = CFStringGetLength(bodyString);
		
		for (termIndex = 0; termIndex < CFArrayGetCount(terms); termIndex++) {
			CFStringRef term = CFArrayGetValueAtIndex(terms, termIndex);
			NSString *trimmedTerm = NVStringByTrimmingHighlightTerm((NSString *)term);
			if ([trimmedTerm length] > 0) {
				CFArrayRef ranges = CFStringCreateArrayWithFindResults(NULL, bodyString, (CFStringRef)trimmedTerm, CFRangeMake(0, bodyLength),
																	   kCFCompareCaseInsensitive);
				if (!ranges)
					continue;
				for (rangeIndex = 0; rangeIndex < CFArrayGetCount(ranges); rangeIndex++) {
					CFRange *range = (CFRange *)CFArrayGetValueAtIndex(ranges, rangeIndex);
					if (range && range->length > 0 && range->location + range->length <= bodyLength)
						[result addObject:[NSValue valueWithRange:NSMakeRange((NSUInteger)range->location, (NSUInteger)range->length)]];
					else
						NVLogWarning(@"rangesOfSearchTerms: Invalid range (%@)", range ? NSStringFromRange(*(NSRange*)range) : @"?");
				}
				CFRelease(ranges);
			}
		}
		CFRelease(terms);
	}
	[result sortUsingComparator:^NSComparisonResult(NSValue *a, NSValue *b) {
		NSUInteger la = [a rangeValue].location, lb = [b rangeValue].location;
		return la < lb ? NSOrderedAscending : (la > lb ? NSOrderedDescending : NSOrderedSame);
	}];
	return result;
}

- (NSArray*)highlightTermsTemporarily:(NSString*)typedString avoidHighlight:(BOOL)noHighlight {
	NSArray *ranges = [self rangesOfSearchTerms:typedString];
	if (!noHighlight) {
		NSDictionary *highlightDict = [self nv_searchHighlightAttributes];
		NSValue *value;
		for (value in ranges)
			[[self layoutManager] addTemporaryAttributes:highlightDict forCharacterRange:[value rangeValue]];
	}
	[self nv_displaySearchHighlights];
	return ranges;
}

- (NSRange)highlightTermsTemporarilyReturningFirstRange:(NSString*)typedString avoidHighlight:(BOOL)noHighlight {
	NSArray *ranges = [self highlightTermsTemporarily:typedString avoidHighlight:noHighlight];
	return [ranges count] ? [[ranges objectAtIndex:0] rangeValue] : NSMakeRange(NSNotFound, 0);
}

- (NSRange)selectionRangeForProposedRange:(NSRange)proposedSelRange granularity:(NSSelectionGranularity)granularity {
    
   // [[NSApp delegate] updateWordCount:YES];
	if (granularity != NSSelectByWord || [[self string] length] == proposedSelRange.location) {
		// If it's not a double-click return unchanged
		return [super selectionRangeForProposedRange:proposedSelRange granularity:granularity];
	}
	
	unsigned int location = (unsigned int)[super selectionRangeForProposedRange:proposedSelRange granularity:NSSelectByCharacter].location;
	int originalLocation = location;
	
	NSString *completeString = [self string];
	unichar characterToCheck = [completeString characterAtIndex:location];
	unsigned short skipMatchingBrace = 0;
	unsigned int lengthOfString = (unsigned int)[completeString length];
	if (lengthOfString == proposedSelRange.location) { // To avoid crash if a double-click occurs after any text
		return [super selectionRangeForProposedRange:proposedSelRange granularity:granularity];
	}
	
	BOOL triedToMatchBrace = NO;
	
	char *rightGroupings = ")}]>";
	char *leftGroupings = "({[<";
	int groupingIndex = 0;
	
	char *rightChar = strchr(rightGroupings, (char)characterToCheck);
	if (rightChar) {
		groupingIndex = (int)(rightChar - rightGroupings);
		
		triedToMatchBrace = YES;
		while (location--) {
			characterToCheck = [completeString characterAtIndex:location];
			if (characterToCheck == leftGroupings[groupingIndex]) {
				if (!skipMatchingBrace) {
					return NSMakeRange(location, originalLocation - location + 1);
				} else {
					skipMatchingBrace--;
				}
			} else if (characterToCheck == *rightChar) {
				skipMatchingBrace++;
			}
		}
		//NSBeep();
	}
	
	char *leftChar = strchr(leftGroupings, (char)characterToCheck);
	if (leftChar) {
		groupingIndex = (int)(leftChar - leftGroupings);
		
		triedToMatchBrace = YES;
		while (++location < lengthOfString) {
			characterToCheck = [completeString characterAtIndex:location];
			if (characterToCheck == rightGroupings[groupingIndex]) {
				if (!skipMatchingBrace) {
					return NSMakeRange(originalLocation, location - originalLocation + 1);
				} else {
					skipMatchingBrace--;
				}
			} else if (characterToCheck == *leftChar) {
				skipMatchingBrace++;
			}
		}
		//NSBeep();
	}
    
    
	// If it has a found a "starting" brace but not found a match, a double-click should only select the "starting" brace and not what it usually would select at a double-click
	if (triedToMatchBrace) {
		return [super selectionRangeForProposedRange:NSMakeRange(proposedSelRange.location, 1) granularity:NSSelectByCharacter];
	} else {
		return [super selectionRangeForProposedRange:proposedSelRange granularity:granularity];
	}
}

- (NSRange)selectedRangeWasAutomatic:(BOOL*)automatic {
	NSRange myRange = [self selectedRange];
	if (automatic) {
		*automatic = !didRenderFully || NSEqualRanges(lastAutomaticallySelectedRange, myRange);
	}
	return myRange;
}

- (void)setAutomaticallySelectedRange:(NSRange)newRange {
	lastAutomaticallySelectedRange = newRange;
	didChangeIntoAutomaticRange = NO;
	[self setSelectedRange:newRange];
}

- (NSUInteger)firstVisibleCharIndexWasAutomatic:(BOOL*)automatic {
	//the top line on screen, and whether it is still where the note was put when shown rather than where the user scrolled
	NSUInteger charIndex = NVFirstVisibleCharIndex(self);
	if (automatic) {
		*automatic = charIndex == automaticFirstVisibleCharIndex;
	}
	return charIndex;
}

- (void)markScrollPositionAutomatic {
	automaticFirstVisibleCharIndex = NVFirstVisibleCharIndex(self);
}

static NSRegularExpression *NVMarkdownRegex(NSString *pattern) {
	NSError *error = nil;
	NSRegularExpression *regex = [NSRegularExpression regularExpressionWithPattern:pattern options:NSRegularExpressionAnchorsMatchLines error:&error];
	if (!regex)
		NVLogInfo(@"nvALT markdown highlight: %@", error);
	return regex;
}

static NSColor *NVBlendColors(NSColor *first, NSColor *second, CGFloat fraction, NSColor *fallback) {
	NSColor *a = [first colorUsingColorSpace:[NSColorSpace sRGBColorSpace]];
	NSColor *b = [second colorUsingColorSpace:[NSColorSpace sRGBColorSpace]];
	NSColor *blend = (a && b) ? [a blendedColorWithFraction:fraction ofColor:b] : nil;
	return blend ?: fallback;
}

- (BOOL)markdownCompletionsEnabled {
	return [[NSUserDefaults standardUserDefaults] boolForKey:@"UsesMarkdownCompletions"];
}

- (void)scheduleMarkdownHighlight {
	[NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(highlightMarkdownSyntax) object:nil];
	[self performSelector:@selector(highlightMarkdownSyntax) withObject:nil afterDelay:0.03];
}

- (void)scheduleFullMarkdownHighlight {
	markdownHighlightCoversAll = YES;
	[self scheduleMarkdownHighlight];
}

- (void)noteMarkdownHighlightEdit:(NSRange)edited change:(NSInteger)change {
	if (markdownHighlightCoversAll)
		return;
	NSUInteger length = [[self string] length];
	if (edited.location != NSNotFound && edited.location == 0 && edited.length == length && length > 4096) {
		markdownHighlightCoversAll = YES;
		markdownHighlightDirty = NSMakeRange(NSNotFound, 0);
		return;
	}
	markdownHighlightDirty = NVMarkdownHighlightUnionEdit(markdownHighlightDirty, edited, change, length);
}

- (void)noteTextStorageDidProcessEditing:(NSNotification *)note {
	if (![NSThread isMainThread]) {
		[self performSelectorOnMainThread:@selector(scheduleFullMarkdownHighlight) withObject:nil waitUntilDone:NO];
		return;
	}
	NSTextStorage *storage = [note object];
	/* The layout manager drops temporary attributes set inside this notification.
	   Apply the theme color on the next turn, after the edit has settled, and
	   do not schedule a highlight: that pass lays the whole note out. */
	if (!NVMarkdownSyntaxHighlightEnabledForLength([storage length])) {
		NSRange edited = [storage editedRange];
		markdownHighlightSuppressed = YES;
		if (edited.location == 0 && edited.length == [storage length])
			plainForegroundDirty = NSMakeRange(NSNotFound, 0);
		[self schedulePlainForegroundForRange:edited change:[storage changeInLength]];
		return;
	}
	[self noteMarkdownHighlightEdit:[storage editedRange] change:[storage changeInLength]];
	[self scheduleMarkdownHighlight];
}

- (void)markdownCompletionPreferenceChanged:(NSNotification *)note {
	static BOOL haveCached = NO;
	static BOOL cached = NO;
	BOOL enabled = [self markdownCompletionsEnabled];
	if (haveCached && cached == enabled)
		return;
	haveCached = YES;
	cached = enabled;
	markdownHighlightCoversAll = YES;
	[self scheduleMarkdownHighlight];
}

- (NSRange)activeMarkdownHighlightRange {
	NSUInteger length = [[self string] length];
	NSRange limit = markdownHighlightLimit;
	if (limit.location > length)
		return NSMakeRange(0, length);
	if (NSMaxRange(limit) > length)
		limit.length = length - limit.location;
	return limit;
}

- (void)paintMarkdownRange:(NSRange)range color:(NSColor *)color strikethrough:(BOOL)strikethrough {
	NSString *string = [self string];
	if (!color || range.length == 0 || range.location == NSNotFound || NSMaxRange(range) > [string length])
		return;
	NSTextStorage *storage = [self textStorage];
	NSLayoutManager *layout = [self layoutManager];
	NSUInteger index = range.location;
	NSUInteger end = NSMaxRange(range);
	NSDictionary *attrs = strikethrough ? @{ NSForegroundColorAttributeName: color, NSStrikethroughStyleAttributeName: @(NSUnderlineStyleSingle) } : @{ NSForegroundColorAttributeName: color };
	while (index < end) {
		NSRange effective = NSMakeRange(0, 0);
		id link = [storage attribute:NSLinkAttributeName atIndex:index longestEffectiveRange:&effective inRange:NSMakeRange(0, [string length])];
		NSRange slice = NSIntersectionRange(range, effective);
		if (slice.length && !link)
			[layout addTemporaryAttributes:attrs forCharacterRange:slice];
		if (NSMaxRange(effective) <= index)
			break;
		index = NSMaxRange(effective);
	}
}

- (void)paintMarkdownMatches:(NSRegularExpression *)regex string:(NSString *)string content:(NSColor *)content marker:(NSColor *)marker contentGroup:(NSInteger)contentGroup delimiterGroup:(NSInteger)delimiterGroup hasCloser:(BOOL)hasCloser strikethrough:(BOOL)strikethrough {
	if (!regex)
		return;
	[regex enumerateMatchesInString:string options:0 range:[self activeMarkdownHighlightRange] usingBlock:^(NSTextCheckingResult *result, NSMatchingFlags flags, BOOL *stop) {
		NSRange whole = [result range];
		[self paintMarkdownRange:whole color:marker strikethrough:NO];
		if (contentGroup > 0 && (NSInteger)[result numberOfRanges] > contentGroup) {
			NSRange inside = [result rangeAtIndex:(NSUInteger)contentGroup];
			if (inside.location != NSNotFound && inside.length)
				[self paintMarkdownRange:inside color:content strikethrough:strikethrough];
		}
		if (delimiterGroup > 0 && (NSInteger)[result numberOfRanges] > delimiterGroup) {
			NSRange opener = [result rangeAtIndex:(NSUInteger)delimiterGroup];
			if (opener.location != NSNotFound && opener.length) {
				[self paintMarkdownRange:opener color:marker strikethrough:NO];
				if (hasCloser && whole.length >= opener.length * 2) {
					NSRange closer = NSMakeRange(NSMaxRange(whole) - opener.length, opener.length);
					if (closer.location >= NSMaxRange(opener))
						[self paintMarkdownRange:closer color:marker strikethrough:NO];
				}
			}
		}
	}];
}

- (void)highlightFencedCodeInString:(NSString *)string code:(NSColor *)code marker:(NSColor *)marker {
	NSRange limit = [self activeMarkdownHighlightRange];
	NSUInteger length = NSMaxRange(limit);
	NSUInteger index = limit.location;
	while (index < length) {
		NSRange lineRange = [string lineRangeForRange:NSMakeRange(index, 0)];
		NSString *trimmed = [[string substringWithRange:lineRange] stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
		BOOL backtick = [trimmed hasPrefix:@"```"];
		BOOL tilde = [trimmed hasPrefix:@"~~~"];
		if (!backtick && !tilde) {
			index = NSMaxRange(lineRange);
			continue;
		}
		NSString *fence = backtick ? @"```" : @"~~~";
		NSRange openLine = lineRange;
		NSUInteger scan = NSMaxRange(lineRange);
		BOOL closed = NO;
		while (scan < length) {
			NSRange nextLine = [string lineRangeForRange:NSMakeRange(scan, 0)];
			NSString *nextTrimmed = [[string substringWithRange:nextLine] stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
			scan = NSMaxRange(nextLine);
			if ([nextTrimmed hasPrefix:fence]) {
				[self paintMarkdownRange:NSMakeRange(openLine.location, scan - openLine.location) color:code strikethrough:NO];
				[self paintMarkdownRange:openLine color:marker strikethrough:NO];
				[self paintMarkdownRange:nextLine color:marker strikethrough:NO];
				closed = YES;
				break;
			}
		}
		if (!closed) {
			[self paintMarkdownRange:NSMakeRange(openLine.location, length - openLine.location) color:code strikethrough:NO];
			[self paintMarkdownRange:openLine color:marker strikethrough:NO];
			return;
		}
		index = scan;
	}
}

- (void)reapplySearchHighlights {
	if (![prefsController highlightSearchTerms])
		return;
	NSString *typed = [(AppController *)[NSApp delegate] searchStringForHighlighting];
	[self removeSearchTermHighlights];
	if ([typed length])
		[self highlightTermsTemporarilyReturningFirstRange:typed avoidHighlight:NO];
}

- (void)cancelOperation:(id)sender {
	[[NSApp delegate] cancelOperation:sender];
}

- (void)paintThemeRange:(NSRange)range style:(NVThemeStyle *)style baseFont:(NSFont *)baseFont useBaseFont:(BOOL)useBaseFont forceStrikethrough:(BOOL)forceStrikethrough {
	[self paintThemeRange:range style:style baseFont:baseFont useBaseFont:useBaseFont forceStrikethrough:forceStrikethrough forceUnderline:NO];
}

- (void)paintThemeRange:(NSRange)range style:(NVThemeStyle *)style baseFont:(NSFont *)baseFont useBaseFont:(BOOL)useBaseFont forceStrikethrough:(BOOL)forceStrikethrough forceUnderline:(BOOL)forceUnderline {
	NSString *string = [self string];
	if (range.length == 0 || range.location == NSNotFound || NSMaxRange(range) > [string length])
		return;
	NSMutableDictionary *attrs = [NSMutableDictionary dictionary];
	NSColor *foreground = [style foregroundColor];
	NSColor *background = [style backgroundColor];
	if (foreground)
		[attrs setObject:foreground forKey:NSForegroundColorAttributeName];
	if (background)
		[attrs setObject:background forKey:NSBackgroundColorAttributeName];
	NSFont *font = [style fontBasedOnFont:baseFont];
	if (!font && useBaseFont && baseFont)
		font = baseFont;
	if (font)
		[attrs setObject:font forKey:NSFontAttributeName];
	if (forceStrikethrough || [style isStrikethrough])
		[attrs setObject:[NSNumber numberWithInteger:NSUnderlineStyleSingle] forKey:NSStrikethroughStyleAttributeName];
	if (forceUnderline)
		[attrs setObject:[NSNumber numberWithInteger:NSUnderlineStyleSingle] forKey:NSUnderlineStyleAttributeName];
	if (![attrs count])
		return;
	NSTextStorage *storage = [self textStorage];
	NSLayoutManager *layout = [self layoutManager];
	NSUInteger index = range.location;
	NSUInteger end = NSMaxRange(range);
	while (index < end) {
		NSRange effective = NSMakeRange(0, 0);
		id link = [storage attribute:NSLinkAttributeName atIndex:index longestEffectiveRange:&effective inRange:NSMakeRange(0, [string length])];
		NSRange slice = NSIntersectionRange(range, effective);
		if (slice.length && !link)
			[layout addTemporaryAttributes:attrs forCharacterRange:slice];
		if (NSMaxRange(effective) <= index)
			break;
		index = NSMaxRange(effective);
	}
}

- (void)paintThemeMatches:(NSRegularExpression *)regex string:(NSString *)string content:(NVThemeStyle *)content marker:(NVThemeStyle *)marker baseFont:(NSFont *)baseFont contentGroup:(NSInteger)contentGroup delimiterGroup:(NSInteger)delimiterGroup hasCloser:(BOOL)hasCloser forceStrikethrough:(BOOL)forceStrikethrough {
	if (!regex)
		return;
	[regex enumerateMatchesInString:string options:0 range:[self activeMarkdownHighlightRange] usingBlock:^(NSTextCheckingResult *result, NSMatchingFlags flags, BOOL *stop) {
		NSRange whole = [result range];
		[self paintThemeRange:whole style:content baseFont:baseFont useBaseFont:NO forceStrikethrough:forceStrikethrough];
		if (delimiterGroup > 0 && (NSInteger)[result numberOfRanges] > delimiterGroup) {
			NSRange opener = [result rangeAtIndex:(NSUInteger)delimiterGroup];
			if (opener.location != NSNotFound && opener.length) {
				[self paintThemeRange:opener style:marker baseFont:baseFont useBaseFont:YES forceStrikethrough:NO];
				if (hasCloser && whole.length >= opener.length * 2) {
					NSRange closer = NSMakeRange(NSMaxRange(whole) - opener.length, opener.length);
					if (closer.location >= NSMaxRange(opener))
						[self paintThemeRange:closer style:marker baseFont:baseFont useBaseFont:YES forceStrikethrough:NO];
				}
			}
		}
		if (contentGroup > 0 && (NSInteger)[result numberOfRanges] > contentGroup) {
			NSRange inside = [result rangeAtIndex:(NSUInteger)contentGroup];
			if (inside.location != NSNotFound && inside.length)
				[self paintThemeRange:inside style:content baseFont:baseFont useBaseFont:NO forceStrikethrough:forceStrikethrough];
		}
	}];
}

- (void)highlightThemedFencedCodeInString:(NSString *)string code:(NVThemeStyle *)code marker:(NVThemeStyle *)marker baseFont:(NSFont *)baseFont {
	NSRange limit = [self activeMarkdownHighlightRange];
	NSUInteger length = NSMaxRange(limit);
	NSUInteger index = limit.location;
	while (index < length) {
		NSRange lineRange = [string lineRangeForRange:NSMakeRange(index, 0)];
		NSString *trimmed = [[string substringWithRange:lineRange] stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
		BOOL backtick = [trimmed hasPrefix:@"```"];
		BOOL tilde = [trimmed hasPrefix:@"~~~"];
		if (!backtick && !tilde) {
			index = NSMaxRange(lineRange);
			continue;
		}
		NSString *fence = backtick ? @"```" : @"~~~";
		NSRange openLine = lineRange;
		NSUInteger scan = NSMaxRange(lineRange);
		BOOL closed = NO;
		while (scan < length) {
			NSRange nextLine = [string lineRangeForRange:NSMakeRange(scan, 0)];
			NSString *nextTrimmed = [[string substringWithRange:nextLine] stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
			scan = NSMaxRange(nextLine);
			if ([nextTrimmed hasPrefix:fence]) {
				[self paintThemeRange:NSMakeRange(openLine.location, scan - openLine.location) style:code baseFont:baseFont useBaseFont:NO forceStrikethrough:NO];
				[self paintThemeRange:openLine style:marker baseFont:baseFont useBaseFont:YES forceStrikethrough:NO];
				[self paintThemeRange:nextLine style:marker baseFont:baseFont useBaseFont:YES forceStrikethrough:NO];
				closed = YES;
				break;
			}
		}
		if (!closed) {
			[self paintThemeRange:NSMakeRange(openLine.location, length - openLine.location) style:code baseFont:baseFont useBaseFont:NO forceStrikethrough:NO];
			[self paintThemeRange:openLine style:marker baseFont:baseFont useBaseFont:YES forceStrikethrough:NO];
			return;
		}
		index = scan;
	}
}

- (void)highlightCompleteReferenceLinksInString:(NSString *)string style:(NVThemeStyle *)style baseFont:(NSFont *)baseFont {
	if (!style)
		return;
	NSArray *ranges = NVCompleteReferenceLinkRanges(string);
	NSUInteger i;
	for (i = 0; i < [ranges count]; i++)
		[self paintThemeRange:[[ranges objectAtIndex:i] rangeValue] style:style baseFont:baseFont useBaseFont:NO forceStrikethrough:NO];
}

- (void)highlightCompleteReferenceLinksInString:(NSString *)string color:(NSColor *)color {
	if (!color)
		return;
	NSArray *ranges = NVCompleteReferenceLinkRanges(string);
	NSUInteger i;
	for (i = 0; i < [ranges count]; i++)
		[self paintMarkdownRange:[[ranges objectAtIndex:i] rangeValue] color:color strikethrough:NO];
}

- (void)highlightMarkedSpansInString:(NSString *)string style:(NVThemeStyle *)style marker:(NVThemeStyle *)marker baseFont:(NSFont *)baseFont {
	NSArray *spans = NVMarkdownHighlightSpansInRange(string, [self activeMarkdownHighlightRange]);
	NSUInteger index;
	for (index = 0; index < [spans count]; index++) {
		NSDictionary *span = [spans objectAtIndex:index];
		[self paintThemeRange:[[span objectForKey:@"content"] rangeValue] style:style baseFont:baseFont useBaseFont:NO forceStrikethrough:NO];
		[self paintThemeRange:[[span objectForKey:@"opener"] rangeValue] style:marker baseFont:baseFont useBaseFont:YES forceStrikethrough:NO];
		[self paintThemeRange:[[span objectForKey:@"closer"] rangeValue] style:marker baseFont:baseFont useBaseFont:YES forceStrikethrough:NO];
	}
}

- (void)highlightCriticMarkupInString:(NSString *)string addition:(NVThemeStyle *)addition deletion:(NVThemeStyle *)deletion highlight:(NVThemeStyle *)highlight comment:(NVThemeStyle *)comment substitutionMarker:(NVThemeStyle *)substitutionMarker marker:(NVThemeStyle *)marker baseFont:(NSFont *)baseFont {
	NSArray *spans = NVCriticMarkupSpansInRange(string, [self activeMarkdownHighlightRange]);
	NSLayoutManager *layout = [self layoutManager];
	NSUInteger index;
	for (index = 0; index < [spans count]; index++) {
		NSDictionary *span = [spans objectAtIndex:index];
		NSString *type = [span objectForKey:@"type"];
		NSRange opener = [[span objectForKey:@"opener"] rangeValue];
		NSRange closer = [[span objectForKey:@"closer"] rangeValue];
		NSRange content = [[span objectForKey:@"content"] rangeValue];
		NSRange whole = NSMakeRange(opener.location, NSMaxRange(closer) - opener.location);
		if (layout && whole.length) {
			[layout removeTemporaryAttribute:NSStrikethroughStyleAttributeName forCharacterRange:whole];
			[layout removeTemporaryAttribute:NSUnderlineStyleAttributeName forCharacterRange:whole];
		}
		NSValue *oldValue = [span objectForKey:@"old"];
		NSValue *separatorValue = [span objectForKey:@"separator"];
		if ([type isEqualToString:@"substitution"] && oldValue) {
			[self paintThemeRange:[oldValue rangeValue] style:deletion baseFont:baseFont useBaseFont:NO forceStrikethrough:YES forceUnderline:NO];
			[self paintThemeRange:content style:addition baseFont:baseFont useBaseFont:NO forceStrikethrough:NO forceUnderline:YES];
			if (separatorValue)
				[self paintThemeRange:[separatorValue rangeValue] style:(substitutionMarker ?: marker) baseFont:baseFont useBaseFont:YES forceStrikethrough:NO forceUnderline:NO];
		} else {
			NVThemeStyle *style = addition;
			BOOL strike = NVCriticKindStrikethrough(type);
			BOOL underline = NVCriticKindUnderline(type);
			if ([type isEqualToString:@"deletion"])
				style = deletion;
			else if ([type isEqualToString:@"highlight"])
				style = highlight;
			else if ([type isEqualToString:@"comment"])
				style = comment;
			else if ([type isEqualToString:@"substitution"])
				underline = YES;
			[self paintThemeRange:content style:style baseFont:baseFont useBaseFont:NO forceStrikethrough:strike forceUnderline:underline];
		}
		NVThemeStyle *markStyle = ([type isEqualToString:@"substitution"] && substitutionMarker) ? substitutionMarker : marker;
		[self paintThemeRange:opener style:markStyle baseFont:baseFont useBaseFont:YES forceStrikethrough:NO forceUnderline:NO];
		[self paintThemeRange:closer style:markStyle baseFont:baseFont useBaseFont:YES forceStrikethrough:NO forceUnderline:NO];
	}
}

- (void)highlightMarkdownSyntaxWithTheme:(NVEditorTheme *)theme {
	NSLayoutManager *layout = [self layoutManager];
	NSString *string = [self string];
	NSRange fullRange = NSMakeRange(0, [string length]);
	NSRange limit = [self activeMarkdownHighlightRange];
	if (layout && limit.length) {
		[layout removeTemporaryAttribute:NSForegroundColorAttributeName forCharacterRange:limit];
		[layout removeTemporaryAttribute:NSBackgroundColorAttributeName forCharacterRange:limit];
		[layout removeTemporaryAttribute:NSStrikethroughStyleAttributeName forCharacterRange:limit];
		[layout removeTemporaryAttribute:NSUnderlineStyleAttributeName forCharacterRange:limit];
		[layout removeTemporaryAttribute:NSFontAttributeName forCharacterRange:limit];
	}
	if (!layout || !fullRange.length || !prefsController) {
		[self highlightWritingGremlins];
		[self reapplySearchHighlights];
		[self nv_reapplyCompletionColors];
		return;
	}
	NSFont *baseFont = [prefsController noteBodyFont] ?: [NSFont userFontOfSize:14.0];
	NSColor *foreground = [theme editorForegroundColor] ?: [NSColor textColor];
	NVThemeStyle *baseStyle = [NVThemeStyle styleWithForegroundColor:foreground];
	NVThemeStyle *markup = [theme styleForKey:@"markup"] ?: baseStyle;
	NVThemeStyle *link = [theme styleForKey:@"linkParens"] ?: [NVThemeStyle styleWithForegroundColor:[[self preferredLinkAttributes] objectForKey:NSForegroundColorAttributeName]];
	NVThemeStyle *code = [theme styleForKey:@"codeSpan"] ?: [theme styleForKey:@"codeBlock"] ?: baseStyle;
	NVThemeStyle *codeBlock = [theme styleForKey:@"codeBlock"] ?: code;
	NVThemeStyle *quote = [theme styleForKey:@"blockquote"] ?: baseStyle;
	NVThemeStyle *strong = [theme styleForKey:@"strong"] ?: baseStyle;
	NVThemeStyle *emphasis = [theme styleForKey:@"emphasis"] ?: baseStyle;
	NVThemeStyle *strike = [theme styleForKey:@"criticDeletion"] ?: baseStyle;
	NVThemeStyle *highlight = [NVThemeStyle highlightStyleFromThemeStyle:[theme styleForKey:@"highlight"]];

	[self paintThemeRange:limit style:baseStyle baseFont:baseFont useBaseFont:NO forceStrikethrough:NO];

	static NSRegularExpression *headingRegex;
	static NSRegularExpression *quoteRegex;
	static NSRegularExpression *listRegex;
	static NSRegularExpression *emphasisRegex;
	static NSRegularExpression *strikeRegex;
	static NSRegularExpression *linkRegex;
	static NSRegularExpression *wikiRegex;
	static NSRegularExpression *codeRegex;
	static NSRegularExpression *ruleRegex;
	static dispatch_once_t once;
	dispatch_once(&once, ^{
		headingRegex = [NVMarkdownRegex(@"^(#{1,6})[ \\t]+(.+)$") retain];
		quoteRegex = [NVMarkdownRegex(@"^([ \\t]{0,3}>)(.*)$") retain];
		listRegex = [NVMarkdownRegex(@"^[ \\t]*(?:[-+*]|\\d+[.)])(?=[ \\t])") retain];
		emphasisRegex = [NVMarkdownRegex(@"(?<![A-Za-z0-9])(\\*\\*|__|\\*|_)(?!\\s)([^\\n]+?)(?<!\\s)\\1(?![A-Za-z0-9])") retain];
		strikeRegex = [NVMarkdownRegex(@"~~(?!\\s)([^\\n]+?)(?<!\\s)~~") retain];
		linkRegex = [NVMarkdownRegex(@"!?\\[([^\\]\\n]*)\\]\\([^\\)\\n]*\\)") retain];
		wikiRegex = [NVMarkdownRegex(@"\\[\\[([^\\]\\n]+)\\]\\]") retain];
		codeRegex = [NVMarkdownRegex(@"(`+)([^`\\n]+?)\\1") retain];
		ruleRegex = [NVMarkdownRegex(@"^[ \\t]{0,3}(?:(?:\\*[ \\t]*){3,}|(?:-[ \\t]*){3,}|(?:_[ \\t]*){3,})$") retain];
	});

	[self paintThemeMatches:quoteRegex string:string content:quote marker:markup baseFont:baseFont contentGroup:2 delimiterGroup:1 hasCloser:NO forceStrikethrough:NO];
	[headingRegex enumerateMatchesInString:string options:0 range:limit usingBlock:^(NSTextCheckingResult *result, NSMatchingFlags flags, BOOL *stop) {
		NSRange hashes = [result rangeAtIndex:1];
		NSRange text = [result rangeAtIndex:2];
		NSUInteger level = hashes.length;
		NVThemeStyle *heading = [theme styleForHeadingLevel:level] ?: baseStyle;
		[self paintThemeRange:text style:heading baseFont:baseFont useBaseFont:NO forceStrikethrough:NO];
		[self paintThemeRange:hashes style:markup baseFont:baseFont useBaseFont:YES forceStrikethrough:NO];
	}];
	[emphasisRegex enumerateMatchesInString:string options:0 range:limit usingBlock:^(NSTextCheckingResult *result, NSMatchingFlags flags, BOOL *stop) {
		NSRange delimiter = [result rangeAtIndex:1];
		NSRange text = [result rangeAtIndex:2];
		NSString *markerText = delimiter.location == NSNotFound ? @"" : [string substringWithRange:delimiter];
		BOOL isStrong = [markerText length] > 1;
		[self paintThemeRange:text style:(isStrong ? strong : emphasis) baseFont:baseFont useBaseFont:NO forceStrikethrough:NO];
		[self paintThemeRange:delimiter style:markup baseFont:baseFont useBaseFont:YES forceStrikethrough:NO];
		if (delimiter.length && [result range].length >= delimiter.length * 2) {
			NSRange closer = NSMakeRange(NSMaxRange([result range]) - delimiter.length, delimiter.length);
			[self paintThemeRange:closer style:markup baseFont:baseFont useBaseFont:YES forceStrikethrough:NO];
		}
	}];
	[self paintThemeMatches:strikeRegex string:string content:strike marker:markup baseFont:baseFont contentGroup:1 delimiterGroup:0 hasCloser:NO forceStrikethrough:YES];
	[self paintThemeMatches:linkRegex string:string content:link marker:markup baseFont:baseFont contentGroup:1 delimiterGroup:0 hasCloser:NO forceStrikethrough:NO];
	[self paintThemeMatches:wikiRegex string:string content:link marker:markup baseFont:baseFont contentGroup:1 delimiterGroup:0 hasCloser:NO forceStrikethrough:NO];
	if ([string rangeOfString:@"[" options:0 range:limit].location != NSNotFound)
		[self highlightCompleteReferenceLinksInString:string style:link baseFont:baseFont];
	[self paintThemeMatches:listRegex string:string content:markup marker:markup baseFont:baseFont contentGroup:0 delimiterGroup:0 hasCloser:NO forceStrikethrough:NO];
	[self paintThemeMatches:ruleRegex string:string content:markup marker:markup baseFont:baseFont contentGroup:0 delimiterGroup:0 hasCloser:NO forceStrikethrough:NO];
	[self highlightMarkedSpansInString:string style:highlight marker:markup baseFont:baseFont];
	[self highlightCriticMarkupInString:string
		addition:[NVThemeStyle criticStyleForKind:@"addition" fromThemeStyle:[theme styleForKey:@"criticAddition"]]
		deletion:[NVThemeStyle criticStyleForKind:@"deletion" fromThemeStyle:[theme styleForKey:@"criticDeletion"]]
		highlight:[NVThemeStyle criticStyleForKind:@"highlight" fromThemeStyle:[theme styleForKey:@"criticHighlight"]]
		comment:[NVThemeStyle criticStyleForKind:@"comment" fromThemeStyle:[theme styleForKey:@"criticComment"]]
		substitutionMarker:[theme styleForKey:@"criticSubstitution"]
		marker:markup
		baseFont:baseFont];
	[self paintThemeMatches:codeRegex string:string content:code marker:markup baseFont:baseFont contentGroup:2 delimiterGroup:1 hasCloser:YES forceStrikethrough:NO];
	[self highlightThemedFencedCodeInString:string code:codeBlock marker:markup baseFont:baseFont];
	[self highlightWritingGremlins];
	[self reapplySearchHighlights];
	[self nv_reapplyCompletionColors];
}

- (void)clearMarkdownSyntaxHighlight {
	NSLayoutManager *layout = [self layoutManager];
	NSUInteger length = [[self string] length];
	if (!layout || !length)
		return;
	NSRange fullRange = NSMakeRange(0, length);
	[layout removeTemporaryAttribute:NSForegroundColorAttributeName forCharacterRange:fullRange];
	[layout removeTemporaryAttribute:NSBackgroundColorAttributeName forCharacterRange:fullRange];
	[layout removeTemporaryAttribute:NSStrikethroughStyleAttributeName forCharacterRange:fullRange];
	[layout removeTemporaryAttribute:NSUnderlineStyleAttributeName forCharacterRange:fullRange];
	[layout removeTemporaryAttribute:NSFontAttributeName forCharacterRange:fullRange];
}

- (NSColor *)plainEditorForegroundColor {
	NVEditorTheme *theme = [[NVThemeManager sharedManager] currentEditorTheme];
	NSColor *color = theme ? [theme editorForegroundColor] : nil;
	if (!color)
		color = [prefsController foregroundTextColor] ?: [(AppController *)[NSApp delegate] foregrndColor];
	return color ?: [NSColor textColor];
}

/* Notes store black when no color is written down. A theme paints over that, and a long note
   still needs the theme color after syntax coloring is skipped. One temporary run covers the
   range: walking every attribute run builds an element per slice, and freeing those hangs. */
- (void)applyPlainForegroundColorInRange:(NSRange)range {
	NSColor *color = [self plainEditorForegroundColor];
	NSString *string = [self string];
	NSLayoutManager *layout = [self layoutManager];
	NSTextStorage *storage = [self textStorage];
	NSUInteger length = [string length];
	if (!color || !layout || !storage || !length || !range.length || range.location == NSNotFound || range.location >= length)
		return;
	if (NSMaxRange(range) > length)
		range.length = length - range.location;
	if (range.length > 1024 || ![storage attribute:NSLinkAttributeName existsInRange:range]) {
		[layout addTemporaryAttribute:NSForegroundColorAttributeName value:color forCharacterRange:range];
		return;
	}
	NSUInteger index = range.location;
	NSUInteger end = NSMaxRange(range);
	while (index < end) {
		NSRange effective = NSMakeRange(0, 0);
		id link = [storage attribute:NSLinkAttributeName atIndex:index longestEffectiveRange:&effective inRange:NSMakeRange(0, length)];
		NSRange slice = NSIntersectionRange(range, effective);
		if (slice.length && !link)
			[layout addTemporaryAttribute:NSForegroundColorAttributeName value:color forCharacterRange:slice];
		if (NSMaxRange(effective) <= index)
			break;
		index = NSMaxRange(effective);
	}
}

- (void)schedulePlainForegroundForRange:(NSRange)edited change:(NSInteger)change {
	NSUInteger length = [[self string] length];
	plainForegroundDirty = NVMarkdownHighlightUnionEdit(plainForegroundDirty, edited, change, length);
	[NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(applyDeferredPlainForeground) object:nil];
	[self performSelector:@selector(applyDeferredPlainForeground) withObject:nil afterDelay:0];
}

- (void)applyDeferredPlainForeground {
	NSRange range = plainForegroundDirty;
	plainForegroundDirty = NSMakeRange(NSNotFound, 0);
	if (!NVMarkdownSyntaxHighlightEnabledForLength([[self string] length]))
		[self applyPlainForegroundColorInRange:range];
}

- (void)prepareMarkdownHighlightLimit {
	NSString *string = [self string];
	NSUInteger length = [string length];
	markdownHighlightLimit = NSMakeRange(0, length);
	if (!NVMarkdownSyntaxHighlightEnabledForLength(length))
		return;
	if (markdownHighlightSuppressed)
		markdownHighlightCoversAll = YES;
	BOOL full = markdownHighlightCoversAll || markdownHighlightDirty.location == NSNotFound;
	NSRange dirty = markdownHighlightDirty;
	markdownHighlightCoversAll = NO;
	markdownHighlightDirty = NSMakeRange(NSNotFound, 0);
	if (!full && length)
		markdownHighlightLimit = NVMarkdownHighlightRangeForEdit(string, dirty);
}

- (void)highlightMarkdownSyntax {
	[self prepareMarkdownHighlightLimit];
	NSUInteger length = [[self string] length];
	if (!NVMarkdownSyntaxHighlightEnabledForLength(length)) {
		/* A long note must not clear attributes, scan every character, or lay out
		   the whole file on a keystroke. That path builds a text-storage element
		   per run and freeing them is quadratic, which is the hang at the end of
		   the note. The theme color is one attribute run, applied again only to
		   the characters just typed. */
		BOOL refreshAll = !markdownHighlightSuppressed || markdownHighlightCoversAll;
		NSRange dirty = markdownHighlightDirty;
		markdownHighlightCoversAll = NO;
		markdownHighlightDirty = NSMakeRange(NSNotFound, 0);
		markdownHighlightSuppressed = YES;
		if (refreshAll)
			[self applyPlainForegroundColorInRange:NSMakeRange(0, length)];
		else if (dirty.location != NSNotFound)
			[self applyPlainForegroundColorInRange:dirty];
		return;
	}
	markdownHighlightSuppressed = NO;
	NVEditorTheme *editorTheme = [[NVThemeManager sharedManager] currentEditorTheme];
	if (editorTheme) {
		[self highlightMarkdownSyntaxWithTheme:editorTheme];
		NVLayOutEntireNote(self);
		return;
	}
	NSLayoutManager *layout = [self layoutManager];
	NSString *string = [self string];
	NSRange fullRange = NSMakeRange(0, [string length]);
	NSRange limit = [self activeMarkdownHighlightRange];
	if (layout && limit.length) {
		[layout removeTemporaryAttribute:NSForegroundColorAttributeName forCharacterRange:limit];
		[layout removeTemporaryAttribute:NSStrikethroughStyleAttributeName forCharacterRange:limit];
		[layout removeTemporaryAttribute:NSUnderlineStyleAttributeName forCharacterRange:limit];
		[layout removeTemporaryAttribute:NSFontAttributeName forCharacterRange:limit];
		[layout removeTemporaryAttribute:NSBackgroundColorAttributeName forCharacterRange:limit];
	}
	if (![self markdownCompletionsEnabled] || !layout || !fullRange.length || !prefsController) {
		[self highlightWritingGremlins];
		[self reapplySearchHighlights];
		[self nv_reapplyCompletionColors];
		NVLayOutEntireNote(self);
		return;
	}

	NSColor *foreground = [prefsController foregroundTextColor] ?: [NSColor textColor];
	NSColor *background = [self backgroundColor] ?: [NSColor textBackgroundColor];
	NSColor *marker = NVBlendColors(foreground, background, 0.55, [NSColor secondaryLabelColor]);
	NSColor *heading = NVBlendColors([NSColor systemBlueColor], foreground, 0.2, [NSColor systemBlueColor]);
	NSColor *code = NVBlendColors([NSColor systemRedColor], foreground, 0.35, [NSColor systemRedColor]);
	NSColor *quote = NVBlendColors(foreground, background, 0.28, foreground);
	NSColor *link = [[self preferredLinkAttributes] objectForKey:NSForegroundColorAttributeName] ?: heading;

	static NSRegularExpression *headingRegex;
	static NSRegularExpression *quoteRegex;
	static NSRegularExpression *listRegex;
	static NSRegularExpression *emphasisRegex;
	static NSRegularExpression *strikeRegex;
	static NSRegularExpression *linkRegex;
	static NSRegularExpression *wikiRegex;
	static NSRegularExpression *codeRegex;
	static NSRegularExpression *ruleRegex;
	static dispatch_once_t once;
	dispatch_once(&once, ^{
		headingRegex = [NVMarkdownRegex(@"^(#{1,6})[ \\t]+(.+)$") retain];
		quoteRegex = [NVMarkdownRegex(@"^([ \\t]{0,3}>)(.*)$") retain];
		listRegex = [NVMarkdownRegex(@"^[ \\t]*(?:[-+*]|\\d+[.)])(?=[ \\t])") retain];
		emphasisRegex = [NVMarkdownRegex(@"(?<![A-Za-z0-9])(\\*\\*|__|\\*|_)(?!\\s)([^\\n]+?)(?<!\\s)\\1(?![A-Za-z0-9])") retain];
		strikeRegex = [NVMarkdownRegex(@"~~(?!\\s)([^\\n]+?)(?<!\\s)~~") retain];
		linkRegex = [NVMarkdownRegex(@"!?\\[([^\\]\\n]*)\\]\\([^\\)\\n]*\\)") retain];
		wikiRegex = [NVMarkdownRegex(@"\\[\\[([^\\]\\n]+)\\]\\]") retain];
		codeRegex = [NVMarkdownRegex(@"(`+)([^`\\n]+?)\\1") retain];
		ruleRegex = [NVMarkdownRegex(@"^[ \\t]{0,3}(?:(?:\\*[ \\t]*){3,}|(?:-[ \\t]*){3,}|(?:_[ \\t]*){3,})$") retain];
	});

	[self paintMarkdownMatches:quoteRegex string:string content:quote marker:marker contentGroup:2 delimiterGroup:1 hasCloser:NO strikethrough:NO];
	[self paintMarkdownMatches:headingRegex string:string content:heading marker:marker contentGroup:2 delimiterGroup:1 hasCloser:NO strikethrough:NO];
	[self paintMarkdownMatches:emphasisRegex string:string content:foreground marker:marker contentGroup:2 delimiterGroup:1 hasCloser:YES strikethrough:NO];
	[self paintMarkdownMatches:strikeRegex string:string content:foreground marker:marker contentGroup:1 delimiterGroup:0 hasCloser:NO strikethrough:YES];
	[self paintMarkdownMatches:linkRegex string:string content:link marker:marker contentGroup:1 delimiterGroup:0 hasCloser:NO strikethrough:NO];
	[self paintMarkdownMatches:wikiRegex string:string content:link marker:marker contentGroup:1 delimiterGroup:0 hasCloser:NO strikethrough:NO];
	if ([string rangeOfString:@"[" options:0 range:limit].location != NSNotFound)
		[self highlightCompleteReferenceLinksInString:string color:link];
	[self paintMarkdownMatches:listRegex string:string content:marker marker:marker contentGroup:0 delimiterGroup:0 hasCloser:NO strikethrough:NO];
	[self paintMarkdownMatches:ruleRegex string:string content:marker marker:marker contentGroup:0 delimiterGroup:0 hasCloser:NO strikethrough:NO];
	NSFont *criticFont = [prefsController noteBodyFont] ?: [NSFont userFontOfSize:14.0];
	[self highlightMarkedSpansInString:string style:[NVThemeStyle highlightStyleFromThemeStyle:nil] marker:[NVThemeStyle styleWithForegroundColor:marker] baseFont:criticFont];
	[self highlightCriticMarkupInString:string
		addition:[NVThemeStyle criticStyleForKind:@"addition" fromThemeStyle:nil]
		deletion:[NVThemeStyle criticStyleForKind:@"deletion" fromThemeStyle:nil]
		highlight:[NVThemeStyle criticStyleForKind:@"highlight" fromThemeStyle:nil]
		comment:[NVThemeStyle criticStyleForKind:@"comment" fromThemeStyle:nil]
		substitutionMarker:nil
		marker:[NVThemeStyle styleWithForegroundColor:marker]
		baseFont:criticFont];
	[self paintMarkdownMatches:codeRegex string:string content:code marker:marker contentGroup:2 delimiterGroup:1 hasCloser:YES strikethrough:NO];
	[self highlightFencedCodeInString:string code:code marker:marker];
	[self highlightWritingGremlins];
	[self reapplySearchHighlights];
	[self nv_reapplyCompletionColors];
	NVLayOutEntireNote(self);
}

- (BOOL)performKeyEquivalent:(NSEvent *)anEvent {
//    [[NSApp delegate] resetModTimers];
    //    [[NSNotificationCenter defaultCenter] postNotificationName:@"ModTimersShouldReset" object:nil];
    NSUInteger modFlags=[anEvent modifierFlags];
    if((modFlags&NSControlKeyMask)||(modFlags&NSAlternateKeyMask)){
         [[NSNotificationCenter defaultCenter] postNotificationName:@"ModTimersShouldReset" object:nil];
    }
	if ([[self window] firstResponder] == self && [self handleMarkdownCommandShortcut:anEvent])
		return YES;
	if (((modFlags & NSCommandKeyMask)>0)&&([[self window]firstResponder]==self)) {
		
		unichar keyChar = [anEvent firstCharacterIgnoringModifiers];
		NSUInteger deviceIndependent = modFlags & NSDeviceIndependentModifierFlagsMask;
		BOOL commandOnly = (deviceIndependent & NSCommandKeyMask) &&
			!(deviceIndependent & (NSShiftKeyMask | NSAlternateKeyMask | NSControlKeyMask));
		BOOL commandOption = (deviceIndependent & NSCommandKeyMask) && (deviceIndependent & NSAlternateKeyMask) &&
			!(deviceIndependent & (NSShiftKeyMask | NSControlKeyMask));
		if (commandOption && (keyChar == 'l' || keyChar == 'L')) {
			[self cycleListMarkerOnCurrentLine];
			return YES;
		}
		if (commandOption && (keyChar == 't' || keyChar == 'T')) {
			[self toggleTaskMarker:nil];
			return YES;
		}
		if (commandOnly && (keyChar == 'b' || keyChar == 'B')) {
			[self bold:nil];
			return YES;
		}
		if (commandOnly && (keyChar == 'i' || keyChar == 'I')) {
			[self italic:nil];
			return YES;
		}
		if (keyChar == NSCarriageReturnCharacter || keyChar == NSNewlineCharacter || keyChar == NSEnterCharacter) {
		//	NSLog(@"insertion");
			NSUInteger charIndex = [self selectedRange].location;
			
			id aLink = [self highlightLinkAtIndex:charIndex];
			if ([aLink isKindOfClass:[NSURL class]]) {
				[self clickedOnLink:aLink atIndex:charIndex];
				return YES;
			}else if (!((modFlags&NSControlKeyMask)||(modFlags&NSAlternateKeyMask))){
                if (modFlags&NSShiftKeyMask) {
                    [self moveToBeginningOfParagraph:self]; 
                    [self moveBackward:self];       
                }else{            
                    [self moveToEndOfParagraph:self];
                }
                [self insertNewline:self];
//                [self insertNewlineIgnoringFieldEditor:self];  
                return YES;
            }
		} else if (keyChar == NSBackspaceCharacter || keyChar == NSDeleteCharacter)  {
			if ([[self string] length]) {
				[self doCommandBySelector:@selector(deleteToBeginningOfLine:)];
				return YES;
			}
		}else if ([[NSUserDefaults standardUserDefaults]boolForKey:@"UsesMarkdownCompletions"]){        
            NSString *firstChar=[NSString stringWithCharacters:&keyChar length:1]; 
            if ([firstChar isEqualToString:@"<"]) {
                [self removeStringAtStartOfSelectedParagraphs:@">"];
                return YES;
                //              NSLog(@"cmd-shift-<");   
            }else if ([firstChar isEqualToString:@">"]){ 
                [self insertStringAtStartOfSelectedParagraphs:@">"];
                return YES;
                //            NSLog(@"cmd-shift->");   
            }else if (([firstChar isEqualToString:@"+"])||([firstChar isEqualToString:@"="])){ 
                
                [self insertStringAtStartOfSelectedParagraphs:@"#"];
                return YES;
                //            NSLog(@"cmd-shift-+");   
            }else if (([firstChar isEqualToString:@"-"])||([firstChar isEqualToString:@"_"])){ 
                [self removeStringAtStartOfSelectedParagraphs:@"#"];
                return YES;
                //            NSLog(@"cmd-shift-MINUS");   
            } 
        }
	}
	
	return [super performKeyEquivalent:anEvent];
}

- (void)keyDown:(NSEvent*)anEvent {	
    //    [[NSNotificationCenter defaultCenter] postNotificationName:@"ModTimersShouldReset" object:nil];
	unichar keyChar = [anEvent firstCharacterIgnoringModifiers];
    
	if (keyChar == NSBackTabCharacter) {
		//apparently interpretKeyEvents: on 10.3 does not call insertBacktab
		//maybe it works on someone else's 10.3 Mac
		[self doCommandBySelector:@selector(insertBacktab:)];
		return;
	}
	if ([self handleMarkdownFunctionShortcut:anEvent])
		return;
    //[super interpretKeyEvents:[NSArray arrayWithObject:anEvent]];
	[super keyDown:anEvent];
    
}

- (BOOL)jumpToRenaming {
	NSEvent *event = [[self window] currentEvent];
	if ([event type] == NSKeyDown && ![event isARepeat] && NSEqualRanges([self selectedRange], NSMakeRange(0, 0))) {
		//command-left at the beginning of the note--jump to editing the title!
		[(AppController *)[NSApp delegate] renameNote:nil];
		NSText *editor = [notesTableView currentEditor];
		NSRange endRange = NSMakeRange([[editor string] length], 0);
		[editor setSelectedRange:endRange];
		[editor scrollRangeToVisible:endRange];
		return YES;
	}
	return NO;
}

- (void)moveToLeftEndOfLine:(id)sender {
	if (![self jumpToRenaming]) 
		[super moveToLeftEndOfLine:sender];
}

- (void)moveToBeginningOfLine:(id)sender {
	if (![self jumpToRenaming]) 
		[super moveToBeginningOfLine:sender];
}

- (BOOL)nv_cursorBelongsToTaskCheckbox {
	NSString *text = [self string];
	NSUInteger length = [text length];
	if (!length)
		return NO;
	NSUInteger location = MIN([self selectedRange].location, length);
	NSUInteger probe = location < length ? location : length - 1;
	NSRange lineRange = [text lineRangeForRange:NSMakeRange(probe, 0)];
	return NVTaskCursorBelongsToCheckbox([text substringWithRange:lineRange], location - lineRange.location);
}

- (void)insertTab:(id)sender {
	//check prefs for tab behavior
    if ([[NSUserDefaults standardUserDefaults]boolForKey:@"UsesMarkdownCompletions"] && ![self nv_cursorBelongsToTaskCheckbox]) {
        NSRange selectedRange=[self selectedRange];
        NSUInteger closer=[self cursorIsInsidePair:@"]"];   
        if ((closer!=NSNotFound)||([self cursorIsImmediatelyPastPair:@"]"])){ 
            NSUInteger insertPt=selectedRange.location;
            NSRange selRange;//=NSMakeRange(NSNotFound, 0);
            NSString *insertString;
            NSString *testString=self.activeParagraphPastCursor;
            if (closer!=NSNotFound) {
                closer+=1;
                if(testString.length>closer) {
                    testString=[testString substringFromIndex:closer];
                }
            }
            testString=[testString stringByTrimmingCharactersInSet:[NSCharacterSet characterSetWithCharactersInString:@"] "]];
            if ([self pairIsOnOwnParagraph:@"]"]) {
                insertString=@": http://";
                selRange=NSMakeRange((insertPt+2), 7);
            }else if ([testString hasPrefix:@"http://"]) {
                NSUInteger spaceDex=[testString rangeOfString:@" "].location;
                
                NSUInteger selDex;
                if (spaceDex!=NSNotFound){
                    selDex=spaceDex;
                }else{
                    selDex=testString.length;
                }           
                selDex+=insertPt;
                selDex+=3;
                selRange=NSMakeRange(selDex, 0);
                insertString=@": ";
            }else{					
                insertString=@"[]";
                selRange=NSMakeRange((insertPt+1), 0);
            }            
            if (selRange.location!=NSNotFound) {
                if (closer!=NSNotFound) {
                    insertPt+=closer;
                    selRange.location+=closer;  
                }
                [self insertText:insertString replacementRange:NSMakeRange(insertPt, 0)];
                [self setSelectedRange:selRange];
                return;
            } 
        }else if((selectedRange.length==7)&&([[[self string]substringWithRange:selectedRange] isEqualToString:@"http://"])&&(([self.activeParagraphBeforeCursor rangeOfString:@"]: "].location!=NSNotFound)||([self.activeParagraphBeforeCursor hasSuffix:@"]("]))){
            [self setSelectedRange:NSMakeRange(selectedRange.location+7, 0)];
            return;
        }      
    }
	if ([self indentListItemsInSelection:NO])
		return;
	if ([self adjustBlockquoteIndent:NO])
		return;
	BOOL wasAutomatic = NSEqualRanges(lastAutomaticallySelectedRange, [self selectedRange]);
	
	if ([prefsController tabKeyIndents] && (!wasAutomatic || ![[self string] length] || didChangeIntoAutomaticRange)) {
		[self insertTabIgnoringFieldEditor:sender];
	} else {
		[[self window] selectNextKeyView:self];
	}
}

- (void)insertBacktab:(id)sender {
	//check temporary NVHiddenBulletIndentAttributeName here first
    if ([[NSUserDefaults standardUserDefaults]boolForKey:@"UsesMarkdownCompletions"] && ![self nv_cursorBelongsToTaskCheckbox]) {
        NSRange selectedRange=[self selectedRange];
        NSUInteger closer=[self cursorIsInsidePair:@"]"];        
        if ((closer!=NSNotFound)||([self cursorIsImmediatelyPastPair:@"]"])){             
            NSUInteger insertPt=selectedRange.location;
            NSString *insertString=@"(http://)";
            if (closer!=NSNotFound) {
                closer+=1;
                insertPt+=closer;
            }
            NSRange selRange=NSMakeRange((insertPt+1), 7);
            [self insertText:insertString replacementRange:NSMakeRange(insertPt, 0)];
            [self setSelectedRange:selRange];
            return;
        }else if((selectedRange.length==7)&&([[[self string]substringWithRange:selectedRange] isEqualToString:@"http://"])&&([self.activeParagraphBeforeCursor hasSuffix:@"]("])){
            [self setSelectedRange:NSMakeRange(selectedRange.location+7, 0)];
            return;
        }
    }
	if ([self indentListItemsInSelection:YES])
		return;
	if ([self adjustBlockquoteIndent:YES])
		return;
	if ([prefsController autoFormatsListBullets] && [self _selectionAbutsBulletIndentRange]) {
		
		[self shiftLeftAction:nil];
	} else {
	
		[[self window] selectPreviousKeyView:self];
	}
}

- (void)insertTabIgnoringFieldEditor:(id)sender {
	
	NSRange range = [self selectedRange];
	if ((range.length > 0 && [[self string] rangeOfCharacterFromSet:[NSCharacterSet newlineCharacterSet] 
														   options:NSLiteralSearch range:range].location != NSNotFound) ||
		([prefsController autoFormatsListBullets] && [self _selectionAbutsBulletIndentRange])) {
		//tab shifts text only if there is more than one line selected (i.e., the selection contains at least one line break), or an indented bullet is near
		
		[self shiftRightAction:nil];
		
	} else if ([prefsController softTabs]) {
		
		NSUInteger numberOfSpacesPerTab = [prefsController numberOfSpacesInTab];

		NSUInteger locationOnLine = range.location - [[self string] lineRangeForRange:range].location;
		if (numberOfSpacesPerTab != 0) {
			NSUInteger numberOfSpacesLess = locationOnLine % numberOfSpacesPerTab;
			numberOfSpacesPerTab = numberOfSpacesPerTab - numberOfSpacesLess;
		}
		NSMutableString *spacesString = [[NSMutableString alloc] initWithCapacity:numberOfSpacesPerTab];
		while (numberOfSpacesPerTab--) {
			[spacesString appendString:@" "];
		}
		
		[self insertText:spacesString];
		[spacesString release];
	} else {
		[self insertText:@"\t"];
	}
}


- (void)deleteBackward:(id)sender {
	
	NSRange charRange = [self rangeForUserTextChange];
	if (charRange.location != NSNotFound) {
		if (charRange.length > 0) {
			// Non-zero selection.  Delete normally.
			[super deleteBackward:sender];
		} else {
			if (charRange.location == 0) {
				// At beginning of text.  Delete normally.
				[super deleteBackward:sender];
			}else {
            
				NSString *string = [self string];
				NSRange paraRange = [string lineRangeForRange:NSMakeRange(charRange.location - 1, 1)];
				if (paraRange.location == charRange.location) {
					// At beginning of line.  Delete normally.
					[super deleteBackward:sender];
				} else {
                    
					NSInteger tabWidth = [prefsController numberOfSpacesInTab];
					NSInteger indentWidth = tabWidth > 0 ? tabWidth : 4;
					BOOL usesTabs = ![prefsController softTabs];
					NSRange leadingSpaceRange = paraRange;
					NSInteger leadingSpaces = [string numberOfLeadingSpacesFromRange:&leadingSpaceRange tabWidth:tabWidth];
					
					if (charRange.location > NSMaxRange(leadingSpaceRange)) {
                        
                        if ([self deleteEmptyPairsBetweenRange:charRange inLineRange:paraRange]) {
                            return;
                        }
						// Not in leading whitespace.  Delete normally.
						[super deleteBackward:sender];
					} else {
						if ([string rangeOfString:@"\t" options:NSLiteralSearch range:leadingSpaceRange].location == NSNotFound) {
							//if this line was indented only with spaces, then keep the soft-tabbed-indentation
							usesTabs = NO;
						} else if ([string rangeOfString:@" " options:NSLiteralSearch range:leadingSpaceRange].location != NSNotFound && ![prefsController _bodyFontIsMonospace]) {
							//mixed tabs and spaces, and we have a proportional font -- what a mess! just revert to normal backward-deletes
							[super deleteBackward:sender];
							return;
						}
						
						NSTextStorage *text = [self textStorage];
						NSInteger leadingIndents = leadingSpaces / indentWidth;
						NSString *replaceString;
						
						// If we were indented to an fractional level just go back to the last even multiple of indentWidth, if we were exactly on, go back a full level.
						if (leadingSpaces % indentWidth == 0) {
							leadingIndents--;
						}
						leadingSpaces = leadingIndents * indentWidth;
						
						replaceString = ((leadingSpaces > 0) ? [NSString tabbifiedStringWithNumberOfSpaces:leadingSpaces tabWidth:tabWidth usesTabs:usesTabs] : @"");
						if ([self shouldChangeTextInRange:leadingSpaceRange replacementString:replaceString]) {
							NSDictionary *newTypingAttributes;
							if (charRange.location < [string length]) {
								newTypingAttributes = [[text attributesAtIndex:charRange.location effectiveRange:NULL] retain];
							} else {
								newTypingAttributes = [[text attributesAtIndex:(charRange.location - 1) effectiveRange:NULL] retain];
							}
							
							[text replaceCharactersInRange:leadingSpaceRange withString:replaceString];
							
							[self setTypingAttributes:newTypingAttributes];
							[newTypingAttributes release];
							
							[self didChangeText];
						}
					}
				}
			}
		}
	}
}

//maybe if we knew we would always have a mono-spaced font
/*- (void)insertNewline:(id)sender {
	NSString *lineEnding = @"\n";
	NSRange charRange = [self rangeForUserTextChange];
	if (charRange.location != NSNotFound) {
		NSString *insertString = (lineEnding ? lineEnding : @"");
		NSString *string = [self string];
		if (charRange.location > 0) {
			if (!lineEnding) {
				// the newline has already been inserted.  Back up by one char.
				charRange.location--;
			}
			if ((charRange.location > 0) && !IsHardLineBreakUnichar([string characterAtIndex:(charRange.location - 1)], string, charRange.location - 1)) {
				unsigned tabWidth = [prefsController numberOfSpacesInTab];
				NSRange paraRange = [string lineRangeForRange:NSMakeRange(charRange.location - 1, 1)];
				unsigned leadingSpaces = [string numberOfLeadingSpacesFromRange:&paraRange tabWidth:tabWidth];

				insertString = [insertString stringByAppendingString:[NSString tabbifiedStringWithNumberOfSpaces:leadingSpaces tabWidth:tabWidth 
																										usesTabs:![prefsController softTabs]]];
			}
		}
		[self insertText:insertString];
	}	
}
*/

- (void)mouseEntered:(NSEvent*)anEvent {
//	mouseInside = YES;
	[self fixCursorForBackgroundUpdatingMouseInside:YES];
    [super mouseEntered:anEvent];
}
- (void)mouseExited:(NSEvent*)anEvent {
	mouseInside = NO;
	[self fixCursorForBackgroundUpdatingMouseInside:NO];
    
    //fix for tooltip hanging around
    [super mouseExited:anEvent];
}

- (BOOL)nv_viewPointIsBelowText:(NSPoint)viewPoint {
	NSLayoutManager *layout = [self layoutManager];
	NSTextContainer *container = [self textContainer];
	if (!layout || !container || ![[self string] length])
		return NO;
	NSUInteger glyphs = [layout numberOfGlyphs];
	if (!glyphs)
		return NO;
	[layout ensureLayoutForGlyphRange:NSMakeRange(0, glyphs)];
	NSPoint origin = [self textContainerOrigin];
	NSRect lastLine = [layout lineFragmentRectForGlyphAtIndex:glyphs - 1 effectiveRange:NULL];
	NSRect extra = [layout extraLineFragmentRect];
	return NVClickIsPastLastLine(viewPoint.y - origin.y, NSMaxY(lastLine), !NSIsEmptyRect(extra), extra.origin.y);
}

- (void)nv_selectFromAnchor:(NSUInteger)anchor toIndex:(NSUInteger)index stillSelecting:(BOOL)stillSelecting {
	NSUInteger length = [[self string] length];
	if (anchor > length)
		anchor = length;
	if (index > length)
		index = length;
	NSUInteger start = MIN(anchor, index);
	NSSelectionAffinity affinity = index < anchor ? NSSelectionAffinityUpstream : NSSelectionAffinityDownstream;
	[self setSelectedRange:NSMakeRange(start, (anchor > index) ? anchor - index : index - anchor) affinity:affinity stillSelecting:stillSelecting];
}

- (void)nv_noteBodyClaimed {
	//a click or an edit in the body: the delegate treats the current view as where the user wants this note kept
	id delegate = [self delegate];
	if ([delegate respondsToSelector:@selector(noteBodyWasClaimed:)])
		[delegate noteBodyWasClaimed:self];
}

- (void)mouseDown:(NSEvent *)event {
	[self nv_noteBodyClaimed];
	if ([self nv_toggleTaskCheckboxForEvent:event])
		return;
	NSUInteger flags = [event modifierFlags] & NSDeviceIndependentModifierFlagsMask;
	NSPoint viewPoint = [self convertPoint:[event locationInWindow] fromView:nil];
	/* Control-click still brings up the menu. A double-click keeps the usual
	   word selection. A plain click past the last line goes to the end, including
	   a blank line that only holds spaces and a last line that holds text. */
	if ([event clickCount] != 1 || (flags & NSControlKeyMask) || ![self nv_viewPointIsBelowText:viewPoint]) {
		[super mouseDown:event];
		return;
	}

	NSWindow *window = [self window];
	BOOL extend = window && [window firstResponder] == self && (flags & NSShiftKeyMask);
	NSUInteger anchor = extend ? [self selectedRange].location : [[self string] length];
	[window makeFirstResponder:self];
	NSUInteger length = [[self string] length];
	if (!extend)
		anchor = length;
	[self nv_selectFromAnchor:anchor toIndex:length stillSelecting:NO];

	NSEvent *tracked = event;
	while (tracked && [tracked type] != NSLeftMouseUp) {
		tracked = [NSApp nextEventMatchingMask:NSLeftMouseDraggedMask | NSLeftMouseUpMask untilDate:[NSDate distantFuture] inMode:NSEventTrackingRunLoopMode dequeue:YES];
		if (!tracked || [tracked type] != NSLeftMouseDragged)
			continue;
		NSPoint draggedPoint = [self convertPoint:[tracked locationInWindow] fromView:nil];
		NSUInteger index = [self nv_viewPointIsBelowText:draggedPoint] ? length : [self characterIndexForInsertionAtPoint:draggedPoint];
		[self nv_selectFromAnchor:anchor toIndex:index stillSelecting:YES];
		[self autoscroll:tracked];
	}
	[self setSelectedRange:[self selectedRange] affinity:[self selectionAffinity] stillSelecting:NO];
}

- (void)_fixCursorForBackgroundUpdatingMouseInside:(NSNumber*)num {
	[self fixCursorForBackgroundUpdatingMouseInside:[num boolValue]];
}

- (void)fixCursorForBackgroundUpdatingMouseInside:(BOOL)checkMouseLoc {
	
	if (IsLeopardOrLater && whiteIBeamCursorIMP && defaultIBeamCursorIMP) {
        if (checkMouseLoc) {
            mouseInside=[self mouseIsHere];
        }
//          NSLog(@"mouseInside :>%d<",mouseInside);
		BOOL shouldBeWhite = mouseInside && backgroundIsDark && ![self isHidden];
      
		Class class = [NSCursor class];
		
		//set method implementation directly; whiteIBeamCursorIMP and defaultIBeamCursorIMP always point to the same respective blocks of code
		Method defaultIBeamCursorMethod = class_getClassMethod(class, @selector(IBeamCursor));
		method_setImplementation(defaultIBeamCursorMethod, shouldBeWhite ? whiteIBeamCursorIMP : defaultIBeamCursorIMP);
		
		NSCursor *currentCursor = [NSCursor currentCursor];
		NSCursor *whiteCursor = whiteIBeamCursorIMP;
		NSCursor *defaultCursor = defaultIBeamCursorIMP;
      
		//if the current cursor is set incorrectly, and and it's not a non-IBeam cursor, then update it (IBeamCursor points to our recently-set implementation)
		if ((currentCursor == whiteCursor) != shouldBeWhite && (currentCursor == whiteCursor || currentCursor == defaultCursor)) {
			[[NSCursor IBeamCursor] set];
		}
	}
}

//hiding or showing the view does not always produce mouseEntered/Exited events
- (void)viewDidUnhide {
	[self performSelector:@selector(_fixCursorForBackgroundUpdatingMouseInside:) withObject:[NSNumber numberWithBool:YES] afterDelay:0.0];

	[super viewDidUnhide];
}
- (void)viewDidHide {
	[self fixCursorForBackgroundUpdatingMouseInside:YES];
	[super viewDidHide];
}

- (void)windowBecameOrResignedMain:(NSNotification *)aNotification  {
	//changing the window ordering seems to occasionally trigger mouseExited events w/o a corresponding mouseEntered
//	[self fixCursorForBackgroundUpdatingMouseInside:YES];
}

- (BOOL)validateMenuItem:(NSMenuItem*)menuItem {
	//need to fix this for better style detection
	
	SEL action = [menuItem action];
	if (action == @selector(defaultStyle:) ||
		action == @selector(bold:) ||
		action == @selector(italic:) ||
		action == @selector(strikethroughNV:)) {
		
		NSRange effectiveRange = NSMakeRange(0,0), range = [self selectedRange];
		NSDictionary *attrs = nil;
		BOOL multipleAttributes = NO;
		if (range.length) {
			//we have something selected--find the attributes of the first thing in the range
			attrs = [[self textStorage] attributesAtIndex:range.location effectiveRange:&effectiveRange];
			if (effectiveRange.length < range.length) {
				//it's a multiple attribute range piece--don't want to bother
				multipleAttributes = YES;
			}
			//NSLog(@"sel attrs: %@", attrs);
		} else {
			//nothing selected--look at typing attrs
			attrs = [self typingAttributes];
		}
		
		BOOL menuItemState = NO;
		if (action == @selector(defaultStyle:)) {
			menuItemState = [attrs isEqualToDictionary:[prefsController noteBodyAttributes]];
		} else if (action == @selector(bold:)) {
			menuItemState = [attrs attributesHaveFontTrait:NSBoldFontMask orAttribute:NSStrokeWidthAttributeName];
		} else if (action == @selector(italic:)) {
			menuItemState = [attrs attributesHaveFontTrait:NSItalicFontMask orAttribute:NSObliquenessAttributeName];
		} else if (action == @selector(strikethroughNV:)) {
			menuItemState = [attrs attributesHaveFontTrait:0 orAttribute:NSStrikethroughStyleAttributeName];
		}
		
		if (menuItemState && multipleAttributes)
			menuItemState = NSMixedState;
		[menuItem setState:menuItemState];

		return YES;
	}else if (action==@selector(performFindPanelAction:)) {
        //for ElasticThreads Find... fix. Also make sure all Find menuItems point their targets to LinkingEditor instead of firstResponder
        
        //hide Find and Replace... on Pre-Lion machines
        if (!IsLionOrLater){
            if([menuItem tag]==12) {
            [menuItem setHidden:YES];
            return NO;
            }
        }else{
            if ([menuItem tag]==7) {
                if (![textFinder validateAction:[menuItem tag]]) {
                    return NO;
                }
            }
        }
        return YES;
    }else if (action==@selector(pasteMarkdownLink:)) {
        if (![self markdownCompletionsEnabled]) {
            [menuItem setHidden:YES];
            return NO;
        }
        [menuItem setHidden:NO];
        if ([self clipboardHasLink])
            return YES;
        return NO;
    }
	
	return [super validateMenuItem:menuItem];
}

/*
 > Manipulate the text storage directly.  Iterate over it by effective
 > ranges for NSFontAttributeName, making your changes.  Be sure to call
 > -[NSTextView shouldChangeTextInRange:replacementString:] first, then
 > -[NSTextStorage beginEditing], then make your changes, then call
 > -[NSTextStorage endEditing] and -[NSTextView didChangeText].
 */
- (void)defaultStyle:(id)sender {
	NSRange range = [self selectedRange];
	
	if (range.length > 0 && range.location != NSNotFound && 
		[self shouldChangeTextInRange:range replacementString:nil]) {
		
		NSTextStorage *textStorage = [self textStorage];
		[textStorage beginEditing];
		[textStorage setAttributes:[prefsController noteBodyAttributes] range:range];
		[textStorage endEditing];
		
		[self didChangeText];
	}
	
	[self setTypingAttributes:[prefsController noteBodyAttributes]];
	
	[[self undoManager] setActionName:NSLocalizedString(@"Plain Text Style",nil)];
}

- (id)highlightLinkAtIndex:(NSUInteger)givenIndex {
	NSUInteger totalLength = [[self string] length];
	NSUInteger charIndex = givenIndex;
	if (charIndex >= totalLength)
		charIndex = totalLength - 1;

	NSRange linkRange, maxRange = NSMakeRange(0, totalLength);
	id aLink = [[self textStorage] attribute:NSLinkAttributeName atIndex:charIndex longestEffectiveRange:&linkRange inRange:maxRange];
	
	if (aLink && linkRange.length && NSMaxRange(linkRange) <= maxRange.length)
		[self setAutomaticallySelectedRange:linkRange];
	return aLink;
}

- (void)clickedOnLink:(id)aLink atIndex:(NSUInteger)charIndex {
	NSEvent *currentEvent = [[self window] currentEvent];
//    NSLog(@"clicked:%@",[currentEvent description]);
	
	if (![prefsController URLsAreClickable] && [currentEvent modifierFlags] & NSCommandKeyMask) {
		
		[self highlightLinkAtIndex:charIndex];
		
	} else if (![prefsController URLsAreClickable]) {
		//pass normal mousedown?
		[self setSelectedRange:NSMakeRange(charIndex, 0)];
		return;
	}
	
	if ([aLink isKindOfClass:[NSURL class]] && [[aLink scheme] isEqualToString:@"nvalt"]) {
        NSUInteger flags=[currentEvent modifierFlags];
        if (((flags&NSDeviceIndependentModifierFlagsMask)==(flags&NSCommandKeyMask))&&((flags&NSDeviceIndependentModifierFlagsMask)>0)) {
            NSString *newURLString=[[aLink lastPathComponent]stringByAddingPercentEscapesUsingEncoding:NSUTF8StringEncoding];
            NSString *txtString=[[NSString stringWithFormat:@"[[%@]]",[aLink lastPathComponent]] stringByAddingPercentEscapesUsingEncoding:NSUTF8StringEncoding];
            newURLString=[NSString stringWithFormat:@"nvalt://make/?title=%@&txt=%@",newURLString,txtString];
//            NSLog(@"newurlstring:%@",newURLString);
            NSURL *newURL=[NSURL URLWithString:newURLString];
//            NSLog(@"interpret from cmd-keydown OLD URL:||%@||  AND NEW URL:|%@|",[aLink absoluteString],[newURL absoluteString]);
            aLink=newURL;
        }
		[(AppController *)[NSApp delegate] interpretNVURL:aLink];
	} else {
		[super clickedOnLink:aLink atIndex:charIndex];
	}
}

- (BOOL)linkReferenceCompletionIsOpen {
	NSRange selection = [self selectedRange];
	if (selection.length || selection.location > [[self string] length])
		return NO;
	NVLinkCompletionContext context = NVLinkCompletionContextAtLocation([self string], selection.location);
	return context.kind != NVLinkCompletionNone && context.range.location != NSNotFound && NSMaxRange(context.range) <= [[self string] length];
}

- (void)completeSuggestedLinks {
	if (isAutocompleting || wasDeleting)
		return;
	/* Completing a link copies the current line. A line that is the whole note is not worth scanning. */
	if (!NVMarkdownSyntaxHighlightEnabledForLength([[self string] length])) {
		NSRange selected = [self selectedRange];
		if (selected.location <= [[self string] length]) {
			NSRange line = [[self string] lineRangeForRange:NSMakeRange(selected.location == [[self string] length] && selected.location ? selected.location - 1 : selected.location, 0)];
			if (line.length > 8192)
				return;
		}
	}
	if ([[self undoManager] isUndoing] || [[self undoManager] isRedoing])
		return;
	if (![prefsController linksAutoSuggested] && ![self linkReferenceCompletionIsOpen])
		return;
	isAutocompleting = YES;
	[self complete:self];
	isAutocompleting = NO;
}

- (NSRange)rangeForUserCompletion {
	NSRange selection = [self selectedRange];
	NSString *document = [self string];
	if (selection.length == 0 && selection.location <= [document length]) {
		NVLinkCompletionContext context = NVLinkCompletionContextAtLocation(document, selection.location);
		if (context.kind != NVLinkCompletionNone && context.range.location != NSNotFound && NSMaxRange(context.range) <= [document length])
			return context.range;
	}

	NSRange completionRange = [super rangeForUserCompletion];
	//NSLog(@"completionRange: %@", [[self string] substringWithRange:completionRange]);
	
	
	//problem: changedRange.location was 201, but completionRange.location was 195
	NSRange beginLineRange = NSMakeRange(changedRange.location, completionRange.location - changedRange.location);
	if (beginLineRange.length > changedRange.length)
		goto cancelCompetion;
	
	NSRange backRange = [[self string] rangeOfString:@"[[" options:NSBackwardsSearch | NSLiteralSearch range:beginLineRange];
	if (backRange.location == NSNotFound)
		goto cancelCompetion;
	
	backRange.location += 2;
	backRange.length = completionRange.length + (completionRange.location - backRange.location);

	if ([[NSCharacterSet whitespaceAndNewlineCharacterSet] characterIsMember:[[self string] characterAtIndex:backRange.location]])
		goto cancelCompetion;
	
	if ([[self string] rangeOfString:@"]]" options:NSLiteralSearch range:backRange].location != NSNotFound)
		goto cancelCompetion;
		
	return backRange;
cancelCompetion:
	return NSMakeRange(NSNotFound, 0);
}

static BOOL NVIsLinkBracket(unichar character) {
	return character == '[' || character == ']' || character == '(' || character == ')';
}

- (NSColor *)nv_themeForegroundColor {
	NVEditorTheme *theme = [[NVThemeManager sharedManager] currentEditorTheme];
	NSColor *color = [theme editorForegroundColor];
	if (!color)
		color = [(AppController *)[NSApp delegate] foregrndColor];
	if (!color)
		color = [NSColor textColor];
	return color;
}

- (NSColor *)nv_linkBracketColor {
	NVEditorTheme *theme = [[NVThemeManager sharedManager] currentEditorTheme];
	NVThemeStyle *link = [theme styleForKey:@"linkParens"];
	NSColor *markup = [link markupForegroundColor];
	if (markup)
		return markup;
	NSColor *foreground = [link foregroundColor];
	if (foreground)
		return foreground;
	NVThemeStyle *generic = [theme styleForKey:@"markup"];
	return [generic foregroundColor] ?: [self nv_themeForegroundColor];
}

- (void)nv_applyCompletionSelectionColor {
	NSColor *color = [self nv_themeForegroundColor];
	if (!color)
		return;
	completionSelectionColored = YES;
	NSMutableDictionary *attributes = [[[self selectedTextAttributes] mutableCopy] autorelease];
	if (!attributes)
		attributes = [NSMutableDictionary dictionary];
	[attributes setObject:color forKey:NSForegroundColorAttributeName];
	[self setSelectedTextAttributes:attributes];
	NSUInteger length = [[self string] length];
	if (completionColoredRange.length && completionColoredRange.location < length) {
		NSRange word = completionColoredRange;
		if (NSMaxRange(word) > length)
			word.length = length - word.location;
		[[self layoutManager] addTemporaryAttribute:NSForegroundColorAttributeName value:color forCharacterRange:word];
	}
}

- (void)nv_restoreSelectedTextAttributes {
	NSColor *background = [[self selectedTextAttributes] objectForKey:NSBackgroundColorAttributeName];
	if (background)
		[self setSelectedTextAttributes:[NSDictionary dictionaryWithObject:background forKey:NSBackgroundColorAttributeName]];
}

- (void)nv_paintLinkBracketsFrom:(NSUInteger)index towardEnd:(BOOL)towardEnd {
	NSString *string = [self string];
	NSUInteger length = [string length];
	NSColor *color = [self nv_linkBracketColor];
	NSLayoutManager *layout = [self layoutManager];
	if (!color || !layout || index > length)
		return;
	if (!towardEnd) {
		while (index > 0) {
			unichar character = [string characterAtIndex:index - 1];
			if (character == '\n' || character == '\r')
				return;
			if (NVIsLinkBracket(character)) {
				NSUInteger end = index;
				do {
					index--;
				} while (index > 0 && NVIsLinkBracket([string characterAtIndex:index - 1]));
				[layout addTemporaryAttribute:NSForegroundColorAttributeName value:color forCharacterRange:NSMakeRange(index, end - index)];
				return;
			}
			index--;
		}
		return;
	}
	while (index < length) {
		unichar character = [string characterAtIndex:index];
		if (character == '\n' || character == '\r')
			return;
		if (NVIsLinkBracket(character)) {
			NSUInteger start = index;
			do {
				index++;
			} while (index < length && NVIsLinkBracket([string characterAtIndex:index]));
			[layout addTemporaryAttribute:NSForegroundColorAttributeName value:color forCharacterRange:NSMakeRange(start, index - start)];
			return;
		}
		index++;
	}
}

- (void)nv_recolorCompletionBrackets {
	if (!completionSelectionColored)
		return;
	NSRange selected = [self selectedRange];
	if (selected.location == NSNotFound || selected.location > [[self string] length])
		return;
	[self nv_paintLinkBracketsFrom:selected.location towardEnd:NO];
	NSUInteger after = NSMaxRange(selected);
	if (after > [[self string] length])
		after = [[self string] length];
	[self nv_paintLinkBracketsFrom:after towardEnd:YES];
}

- (void)nv_reapplyCompletionColors {
	if (!completionSelectionColored)
		return;
	[self nv_applyCompletionSelectionColor];
	[self nv_recolorCompletionBrackets];
}

- (void)insertCompletion:(NSString *)word forPartialWordRange:(NSRange)charRange movement:(NSInteger)movement isFinal:(BOOL)isFinal {
	NSString *str = [self string];
	BOOL finalizedCompletion = NO;
	BOOL referenceCompletion = NO;
	BOOL hasClosingBracket = NO;
	
	isFinal = isFinal && movement != NSRightTextMovement;

	if (charRange.location != NSNotFound && NSMaxRange(charRange) <= [str length]) {
		NVLinkCompletionContext context = NVLinkCompletionContextAtLocation(str, NSMaxRange(charRange));
		referenceCompletion = (context.kind != NVLinkCompletionNone && NSEqualRanges(context.range, charRange));
		if (referenceCompletion) {
			// The inline preview selects the characters added past the typed id, so an
			// existing ']' sits after that selection rather than after charRange.
			NSUInteger end = NSMaxRange(charRange);
			NSRange selected = [self selectedRange];
			if (selected.length && selected.location >= charRange.location && selected.location <= end && NSMaxRange(selected) <= [str length])
				end = MAX(end, NSMaxRange(selected));
			if (end < [str length] && [str characterAtIndex:end] == ']')
				hasClosingBracket = YES;
		}
	}
	
	if (referenceCompletion) {
		// Return, Tab, and a click all accept. The opening complete: call can also
		// report a final insertion while the list is still up; leave that as a preview.
		BOOL choose = (movement == NSReturnTextMovement || movement == NSTabTextMovement || movement == NSOtherTextMovement);
		if (isFinal && !isAutocompleting && [word length] && choose) {
			word = NVLinkCompletionInsert(word, hasClosingBracket);
			finalizedCompletion = YES;
		}
	} else if (isFinal && [word length] && (movement == NSReturnTextMovement || movement == NSTabTextMovement)) {
		
		//automatically add a trailing double-bracket if one does not already exist
		NSRange endRange = NSMakeRange(charRange.location + [word length], 2);
		if ([str length] < NSMaxRange(endRange) || ![[str substringWithRange:endRange] isEqualToString:@"]]"]) {
			word = [word stringByAppendingString:@"]]"];
		}
		finalizedCompletion = YES;
	}
	
	//preserve capitalization by transferring charRange substring into word
	if (!referenceCompletion && !finalizedCompletion && charRange.length <= [word length]) { 
		NSString *existingWord = [str substringWithRange:charRange];
		word = [existingWord stringByAppendingString:[word substringFromIndex:[existingWord length]]];
	}
	
	[super insertCompletion:word forPartialWordRange:charRange movement:movement isFinal:isFinal];

	[NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(nv_reapplyCompletionColors) object:nil];
	if (isFinal) {
		completionColoredRange = NSMakeRange(NSNotFound, 0);
		if (completionSelectionColored) {
			completionSelectionColored = NO;
			[self nv_restoreSelectedTextAttributes];
		}
	} else if (charRange.location != NSNotFound && [word length] && charRange.location <= [[self string] length]) {
		NSUInteger wordLength = MIN([word length], [[self string] length] - charRange.location);
		completionColoredRange = NSMakeRange(charRange.location, wordLength);
		[self nv_applyCompletionSelectionColor];
		[self nv_recolorCompletionBrackets];
		[self performSelector:@selector(nv_reapplyCompletionColors) withObject:nil afterDelay:0];
	}

	if (referenceCompletion && finalizedCompletion && hasClosingBracket) {
		NSUInteger caret = [self selectedRange].location;
		if (caret < [[self string] length] && [[self string] characterAtIndex:caret] == ']')
			[self setSelectedRange:NSMakeRange(caret + 1, 0)];
	}
}

- (void)didChangeText {
	
	//if the text storage was somehow shortened since changedRange was set in -shouldChangeText, avoid unsigned underflow and stale offsets
	NSUInteger stringLength = [[self string] length];
	if (changedRange.location > stringLength)
		changedRange = NSMakeRange(stringLength, 0);
	else
		changedRange = NSMakeRange(changedRange.location, MIN(changedRange.length, stringLength - changedRange.location));

	[self nv_noteBodyClaimed];


	/* A data detector over a multi-megabyte line is the return-key hang at the end of a long note. */
	if (NVMarkdownSyntaxHighlightEnabledForLength(stringLength) || changedRange.length <= 8192) {
		//-removeAttribute:range: seems slow for some reason; try checking with -attributesAtIndex:effectiveRange: first
		if ([[self textStorage] attribute:NSLinkAttributeName existsInRange:changedRange])
			[[self textStorage] removeAttribute:NSLinkAttributeName range:changedRange];
		[[self textStorage] addLinkAttributesForRange:changedRange];
		
		[[self textStorage] addStrikethroughNearDoneTagsForRange:changedRange];
	}
	
	[self completeSuggestedLinks];
	
	//[[self window] invalidateCursorRectsForView:self];
	
	[super didChangeText];
	if (!renumberingNumberedLists)
		[self scheduleNumberedListRenumber];
	
	//if the result of changing the text caused us to move into the automatic range, then temporarily ignore the automatic range
	//don't use -selectedRangeWasAutomatic: as it consults didRenderFully, which might not be true here
	if (NSEqualRanges(lastAutomaticallySelectedRange, [self selectedRange]))
		didChangeIntoAutomaticRange = YES;
}

- (BOOL)shouldChangeTextInRange:(NSRange)affectedCharRange replacementString:(NSString *)replacementString {
	wasDeleting = ![replacementString length];
	
	//it's not exactly proper to alter typing attributes when we don't yet know whether the text should actually be changed, but NV shouldn't cause that to happen, anyway
	[self fixTypingAttributesForSubstitutedFonts];
	
	NSString *string = [self string];
		
	NSCharacterSet *separatorCharacterSet = [NSCharacterSet newlineCharacterSet];
	//even when only seeking newlines, this manual line-finding method is less laggy than -[NSString lineRangeForRange:]
	NSUInteger begin = [string rangeOfCharacterFromSet:separatorCharacterSet options:NSBackwardsSearch range:NSMakeRange(0, affectedCharRange.location)].location;
	if (begin == NSNotFound) {
		begin = 0;
	}
	
	NSUInteger end = [string rangeOfCharacterFromSet:separatorCharacterSet options:0 range:NSMakeRange(affectedCharRange.location + affectedCharRange.length, 
																									   [string length] - (affectedCharRange.location + affectedCharRange.length))].location;
	if (end == NSNotFound) {
		end = [string length];
	}
	changedRange = NSMakeRange(begin, (end - begin) + [replacementString length]);
		
	if (affectedCharRange.length > 0 && replacementString != nil) { // Deleting something
		changedRange.length -= affectedCharRange.length;
	}
	
	return [super shouldChangeTextInRange:affectedCharRange replacementString:replacementString];
}

#ifdef notyet
static long (*GetGetScriptManagerVariablePointer())(short) {
	static long (*_GetScriptManagerVariablePointer)(short) = NULL;
	if (!_GetScriptManagerVariablePointer) {
		NVLogInfo(@"looking up");
		CFBundleRef csBundle = CFBundleCreate(NULL, CFURLCreateWithFileSystemPath(NULL, CFSTR("/System/Library/Frameworks/CoreServices.framework"), kCFURLPOSIXPathStyle, TRUE));
		if (csBundle) _GetScriptManagerVariablePointer = (long (*)(short))CFBundleGetDataPointerForName(csBundle, CFSTR("GetScriptManagerVariable"));
	}
	return _GetScriptManagerVariablePointer;
}
#endif

- (void)nv_applyThemeTypingColor {
	NSColor *color = [self plainEditorForegroundColor];
	if (!color)
		return;
	NSDictionary *typing = [self typingAttributes];
	NSColor *current = [typing objectForKey:NSForegroundColorAttributeName];
	if (current && ColorsEqualWith8BitChannels(current, color))
		return;
	NSMutableDictionary *updated = typing ? [typing mutableCopy] : [[NSMutableDictionary alloc] init];
	[updated setObject:color forKey:NSForegroundColorAttributeName];
	[self setTypingAttributes:updated];
	[updated release];
}

- (void)fixTypingAttributesForSubstitutedFonts {
	[self nv_applyThemeTypingColor];
	//fixes a problem with fonts substituted by non-system input languages that Apple should have fixed themselves
	
	//if the user has chosen a default font that does not support the current input script, and then changes back to a language input that _does_
	//then the font in the typing attributes will be changed back to match. the problem is that this change occurs only upon changing the input language
	//if the user starts typing in the middle of a block of font-substituted text, the typing attributes will change to that font
	//the result is that typing english in the middle of a block of japanese will use Hiragino Kaku Gothic instead of whatever else the user had chosen
	//this method detects these types of spurious font-changes and reverts to the default font, but only if the font would not be immediately switched back
	//as a result of continuing to type in the native script.
	
	//we'd ideally check smKeyScript against available scripts of current note body font:
	//call RevertTextEncodingToScriptInfo on ATSFontFamilyGetEncoding(ATSFontFamilyFindFromName(CFStringRef([bodyFont familyName]), kATSOptionFlagsDefault))
	//because someone on a japanese-localized system could see their font changing around a lot if they didn't set their note body font to something suitable for their language
	
	BOOL currentKeyboardInputIsSystemLanguage = NO;
	
#if MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_5
    TISInputSourceRef inputRef = TISCopyCurrentKeyboardInputSource();
    NSArray* inputLangs = [[(NSArray*)TISGetInputSourceProperty(inputRef, kTISPropertyInputSourceLanguages) retain] autorelease];
    CFRelease(inputRef);
    NSString *preferredLang = [[NSLocale autoupdatingCurrentLocale] objectForKey:NSLocaleLanguageCode];
    currentKeyboardInputIsSystemLanguage = nil != preferredLang && [inputLangs containsObject:preferredLang];
#else
	currentKeyboardInputIsSystemLanguage = GetScriptManagerVariable(smSysScript) == GetScriptManagerVariable(smKeyScript);
#endif
	
	if (currentKeyboardInputIsSystemLanguage) {
		//only attempt to restore fonts (with styles of course) if the current script is system default--that is, not using an input method that would change the font
		//this check helps prevent NSTextView from being repeatedly punched in the face when it can't help it
		
		NSFont *currentFont = [prefsController noteBodyFont];
		if (![[[[self typingAttributes] objectForKey:NSFontAttributeName] familyName] isEqualToString:[currentFont familyName]]) {
			//if someone managed to mangle the font--possibly with characters not present in it due to alt. text encoding--so mangle it back
			
			NSMutableDictionary *newTypingAttributes = [[self typingAttributes] mutableCopy];
			[newTypingAttributes setObject:currentFont forKey:NSFontAttributeName];
			//NSLog(@"mangling font 'back' to normal");
			
			if ([[self typingAttributes] attributesHaveFontTrait:NSBoldFontMask orAttribute:NSStrokeWidthAttributeName]) {
				[newTypingAttributes applyStyleInverted:NO trait:NSBoldFontMask forFont:currentFont 
								 alternateAttributeName:NSStrokeWidthAttributeName 
								alternateAttributeValue:[NSNumber numberWithFloat:STROKE_WIDTH_FOR_BOLD]];
				
				currentFont = [newTypingAttributes objectForKey:NSFontAttributeName];
			}
			
			if ([[self typingAttributes] attributesHaveFontTrait:NSItalicFontMask orAttribute:NSObliquenessAttributeName]) {
				[newTypingAttributes applyStyleInverted:NO trait:NSItalicFontMask forFont:currentFont 
								 alternateAttributeName:NSObliquenessAttributeName 
								alternateAttributeValue:[NSNumber numberWithFloat:OBLIQUENESS_FOR_ITALIC]];	
			}
			[self setTypingAttributes:newTypingAttributes];
            [newTypingAttributes release];
		}
	}
}

- (BOOL)_selectionAbutsBulletIndentRange {

	NSRange range = [self selectedRange];
	NSRange backBulletRange = NSMakeRange(range.location - 2, 2);
	NSRange frontBulletRange = NSMakeRange(range.location, 2);
	
	return ((backBulletRange.location > 0 && NSMaxRange(backBulletRange) < [[self string] length] && [self _rangeIsAutoIdentedBullet:backBulletRange]) || 
			(NSMaxRange(frontBulletRange) < [[self string] length] && [self _rangeIsAutoIdentedBullet:frontBulletRange]));
}

- (BOOL)_rangeIsAutoIdentedBullet:(NSRange)aRange {
	NSRange effectiveRange = NSMakeRange(aRange.location, 0);
	while (NSMaxRange(effectiveRange) < NSMaxRange(aRange)) {
		
		id bulletIndicator = nil;
		
		//sometimes the temporary attributes are split across juxtaposing characters for some reason, so longest-effective-range is necessary
		//unfortunately there is no such method on Tiger, and I'm not about to emulate its coalescing behavior here
		if (IsLeopardOrLater) {
			bulletIndicator = [[self layoutManager] temporaryAttribute:NVHiddenBulletIndentAttributeName atCharacterIndex:NSMaxRange(effectiveRange) 
												 longestEffectiveRange:&effectiveRange inRange:aRange];
		} else {
			NSDictionary *dict = [[self layoutManager] temporaryAttributesAtCharacterIndex:NSMaxRange(effectiveRange) effectiveRange:&effectiveRange];
			bulletIndicator = [dict objectForKey:NVHiddenBulletIndentAttributeName];
		}
		if (bulletIndicator && NSEqualRanges(effectiveRange, aRange)) {
			return YES;
		}
	}
	
	return NO;	
}

- (BOOL)replaceEmptyListMarkerWithLineBreak {
	if (![prefsController autoFormatsListBullets])
		return NO;
	NSRange selected = [self selectedRange];
	if (selected.length != 0 || selected.location == NSNotFound)
		return NO;
	NSString *string = [self string];
	if (selected.location > [string length])
		return NO;

	NSRange lineRange = [string lineRangeForRange:NSMakeRange(selected.location, 0)];
	if (lineRange.length == 0 || lineRange.length > 8192)
		return NO;
	NSString *line = [string substringWithRange:lineRange];
	NSUInteger contentLength = [line length];
	while (contentLength && [[NSCharacterSet newlineCharacterSet] characterIsMember:[line characterAtIndex:contentLength - 1]])
		contentLength--;
	if (contentLength == 0)
		return NO;

	NSCharacterSet *whitespace = [NSCharacterSet whitespaceCharacterSet];
	NSUInteger index = 0;
	while (index < contentLength && [whitespace characterIsMember:[line characterAtIndex:index]])
		index++;
	if (index >= contentLength)
		return NO;

	unichar marker = [line characterAtIndex:index];
	NSUInteger markerEnd = NSNotFound;
	if ([[NSCharacterSet listBulletsCharacterSet] characterIsMember:marker]) {
		markerEnd = index + 1;
	} else if ([[NSCharacterSet decimalDigitCharacterSet] characterIsMember:marker]) {
		NSUInteger digit = index;
		while (digit < contentLength && [[NSCharacterSet decimalDigitCharacterSet] characterIsMember:[line characterAtIndex:digit]])
			digit++;
		if (digit < contentLength && ([line characterAtIndex:digit] == '.' || [line characterAtIndex:digit] == ')'))
			markerEnd = digit + 1;
	}
	// A continued item always has whitespace after the marker. A bare "1." or "---" is left alone.
	if (markerEnd == NSNotFound || markerEnd >= contentLength)
		return NO;
	if ([line rangeOfCharacterFromSet:[whitespace invertedSet] options:0 range:NSMakeRange(markerEnd, contentLength - markerEnd)].location != NSNotFound && !NVTaskLineIsEmptyItem(line))
		return NO;

	if (![self shouldChangeTextInRange:lineRange replacementString:@"\n"])
		return NO;
	[self replaceCharactersInRange:lineRange withString:@"\n"];
	[self didChangeText];
	[self setSelectedRange:NSMakeRange(lineRange.location, 0)];
	[self scrollRangeToVisible:[self selectedRange]];
	return YES;
}

- (void)insertNewline:(id)sender {
	//reset custom styles after each line
	[self setTypingAttributes:[prefsController noteBodyAttributes]];
	[self nv_applyThemeTypingColor];

	if ([self replaceEmptyListMarkerWithLineBreak])
		return;
	if ([self insertMarkdownStructureNewline])
		return;
	
	[super insertNewline:sender];
	
	if ([prefsController autoIndentsNewLines]) {
		// If we should indent automatically, check the previous line and scan all the whitespace at the beginning of the line into a string and insert that string into the new line
		NSString *previousLineWhitespaceString = nil;
		NSUInteger caret = [self selectedRange].location;
		if (!caret)
			return;
		NSRange previousLineRange = [[self string] lineRangeForRange:NSMakeRange(caret - 1, 0)];
		/* Copying the whole note as the "previous line" is what stalls return at the end of a long paragraph. */
		if (previousLineRange.length > 8192)
			return;
		NSScanner *previousLineScanner = [[NSScanner alloc] initWithString:[[self string] substringWithRange:previousLineRange]];
		[previousLineScanner setCharactersToBeSkipped:nil];
		
		if (![previousLineScanner scanCharactersFromSet:[NSCharacterSet whitespaceCharacterSet] intoString:&previousLineWhitespaceString]) {
            previousLineWhitespaceString = @"";
        }
        //for propagating list-element, look for bullet-type-character + 1charWS + at-least-one-nonWSChar
        
        NSUInteger loc = [previousLineScanner scanLocation];
        NSString *str = [previousLineScanner string];
        unichar bulletChar, wsChar;
        NSRange realBulletRange = NSMakeRange(loc + previousLineRange.location, 2), carriedBulletRange = NSMakeRange(NSNotFound, 0);
        BOOL shouldDeleteLastBullet = NO;
        NSInteger listNumber=-1;
        bulletChar = [str characterAtIndex:loc];
        BOOL isNumberedList=[[NSCharacterSet decimalDigitCharacterSet] characterIsMember:bulletChar];
        if ([prefsController autoFormatsListBullets]) {
            
            NSUInteger keyLoc=loc;
            NSString *theNum=@"";
            if (isNumberedList&&([str length]>(keyLoc+1))) {
                NSRange numRange;
                theNum=[[self string] firstNumberFromStringWithinRange:previousLineRange isInRange:&numRange];
                if (numRange.location!=NSNotFound) {
                    keyLoc+=numRange.length;
                }else{
                    isNumberedList=NO;
                }
            }
            if (keyLoc + 2 < [str length] && ![previousLineScanner isAtEnd] &&
                ([[NSCharacterSet listBulletsCharacterSet] characterIsMember:bulletChar]||isNumberedList) &&
                [[NSCharacterSet whitespaceCharacterSet] characterIsMember:(wsChar = [str characterAtIndex:keyLoc + 1])] &&
                [[[NSCharacterSet whitespaceAndNewlineCharacterSet] invertedSet] characterIsMember:[str characterAtIndex:keyLoc + 2]]) {
                carriedBulletRange = NSMakeRange(NSMaxRange(previousLineRange) + [previousLineWhitespaceString length], 2);
                if (isNumberedList) {
                    
                    listNumber=[theNum integerValue];
                    if (listNumber==-1||((listNumber==0)&&(![theNum isEqualToString:@"0"]))) {
                        [previousLineScanner release];
                        return;
                    }
                    listNumber++;
                    previousLineWhitespaceString = [previousLineWhitespaceString stringByAppendingFormat:@"%ld.", listNumber];
                    
                }else{
                    previousLineWhitespaceString = [previousLineWhitespaceString stringByAppendingFormat:@"%@", [NSString stringWithCharacters:&bulletChar length:1]];
                }
                previousLineWhitespaceString=[previousLineWhitespaceString stringByAppendingFormat:@"%@",[NSString stringWithCharacters:&wsChar length:1]];
                NSString *taskSuffix = NVTaskContinuationSuffix([[self string] substringWithRange:previousLineRange]);
                if ([taskSuffix length])
                    previousLineWhitespaceString = [previousLineWhitespaceString stringByAppendingString:taskSuffix];
            } else if (NSMaxRange(realBulletRange) < [[self string] length] && [self _rangeIsAutoIdentedBullet:realBulletRange]) {
                //should not carry a bullet; also check if one is here that we should delete
                shouldDeleteLastBullet = YES;
            }
        }
        
        if (shouldDeleteLastBullet) {
            //we had carried a bullet, but now it is no carried no more
            //so instead of inserting the extra space, delete both that previously-carried-bullet and the newline added by -super up there
            if ([self shouldChangeTextInRange:NSMakeRange(realBulletRange.location, realBulletRange.length + 1) replacementString:@""]) { // Do it this way to mark it as an Undo
                [self replaceCharactersInRange:NSMakeRange(realBulletRange.location, realBulletRange.length + 1) withString:@""];
                [self didChangeText];
            }
        } else {
            previousLineRange=NSMakeRange(NSMaxRange(previousLineRange), 0);
            if ([self shouldChangeTextInRange:previousLineRange replacementString:previousLineWhitespaceString]) {
                [self replaceCharactersInRange:previousLineRange withString:previousLineWhitespaceString];
                if (carriedBulletRange.length>0) {
                    if (!isNumberedList) {
                        [[self layoutManager] addTemporaryAttributes:[NSDictionary dictionaryWithObject:[NSNull null] forKey:NVHiddenBulletIndentAttributeName]
                                                   forCharacterRange:carriedBulletRange];
                        //[[self layoutManager] addTemporaryAttributes:[prefsController searchTermHighlightAttributes] forCharacterRange:carriedBulletRange];
                    }else{
                        [[self layoutManager]invalidateDisplayForCharacterRange:previousLineRange];
                        [[self window] resetCursorRects];
                        [self updateNumberedListFromRange:[self rangeOfActiveParagraph] startingNum:listNumber];
                        [self setNeedsDisplay:YES];
                    }
                }
                [self didChangeText];
            }
        }
        
		[previousLineScanner release];
	}
}

- (void)setupFontMenu {
	NSMenu *theMenu = [[[NSMenu alloc] initWithTitle:@"NVFontMenu"] autorelease];
	NSMenuItem *theMenuItem;
	if(IsLeopardOrLater){
        
        theMenuItem = [[[NSMenuItem alloc] initWithTitle:NSLocalizedString(@"Enter Full Screen",@"menu item title for entering fullscreen") action:@selector(switchFullScreen:) keyEquivalent:@""] autorelease];
        [theMenuItem setTarget:[NSApp delegate]];
        [theMenu addItem:theMenuItem];         
	}
    theMenuItem = [[[NSMenuItem alloc] initWithTitle:NSLocalizedString(@"Insert Link",@"insert link menu item title") action:@selector(insertLink:) keyEquivalent:@""] autorelease];
	[theMenuItem setTarget:self];
	[theMenu addItem:theMenuItem];
	theMenuItem = [[[NSMenuItem alloc] initWithTitle:NSLocalizedString(@"Insert Markdown Link",@"markdown link menu item title") action:@selector(insertMarkdownLink:) keyEquivalent:@""] autorelease];
	[theMenuItem setTarget:self];
	[theMenu addItem:theMenuItem];
    theMenuItem = [[[NSMenuItem alloc] initWithTitle:NSLocalizedString(@"Use Selection for Find",@"find using selection menu item title") action:@selector(performFindPanelAction:) keyEquivalent:@""] autorelease];
    [theMenuItem setTag:7];
	[theMenuItem setTarget:self];
	[theMenu addItem:theMenuItem];
    [theMenu addItem:[NSMenuItem separatorItem]];
    
	theMenuItem = [[[NSMenuItem alloc] initWithTitle:NSLocalizedString(@"Cut",@"cut menu item title") action:@selector(cut:) keyEquivalent:@""] autorelease];
	[theMenuItem setTarget:self];
	[theMenu addItem:theMenuItem];
	
	theMenuItem = [[[NSMenuItem alloc] initWithTitle:NSLocalizedString(@"Copy",@"copy menu item title") action:@selector(copy:) keyEquivalent:@""] autorelease];
	[theMenuItem setTarget:self];
	[theMenu addItem:theMenuItem];
	
	theMenuItem = [[[NSMenuItem alloc] initWithTitle:NSLocalizedString(@"Paste",@"paste menu item title") action:@selector(paste:) keyEquivalent:@""] autorelease];
	[theMenuItem setTarget:self];
	[theMenu addItem:theMenuItem];
	[theMenu addItem:[NSMenuItem separatorItem]];
	
	NSMenu *formatMenu = [[[NSMenu alloc] initWithTitle:NSLocalizedString(@"Format", nil)] autorelease];
	
	theMenuItem = [[[NSMenuItem alloc] initWithTitle:NSLocalizedString(@"Plain Text Style",nil) 
											  action:@selector(defaultStyle:) keyEquivalent:@""] autorelease];
	[theMenuItem setTarget:self];
	[formatMenu addItem:theMenuItem];
	
	theMenuItem = [[[NSMenuItem alloc] initWithTitle:NSLocalizedString(@"Bold",nil) action:@selector(bold:) keyEquivalent:@""] autorelease];
	[theMenuItem setTarget:self];
	[formatMenu addItem:theMenuItem];
	
	theMenuItem = [[[NSMenuItem alloc] initWithTitle:NSLocalizedString(@"Italic",nil) action:@selector(italic:) keyEquivalent:@""] autorelease];
	[theMenuItem setTarget:self];
	[formatMenu addItem:theMenuItem];
	
	theMenuItem = [[[NSMenuItem alloc] initWithTitle:NSLocalizedString(@"Strikethrough",nil) action:@selector(strikethroughNV:) keyEquivalent:@""] autorelease];
	[theMenuItem setTarget:self];
	[formatMenu addItem:theMenuItem];

	theMenuItem = [[[NSMenuItem alloc] initWithTitle:NSLocalizedString(@"Task",nil) action:@selector(toggleTaskMarker:) keyEquivalent:@"t"] autorelease];
	[theMenuItem setKeyEquivalentModifierMask:NSCommandKeyMask | NSAlternateKeyMask];
	[theMenuItem setTarget:self];
	[formatMenu addItem:theMenuItem];
	
	theMenuItem = [[[NSMenuItem alloc] initWithTitle:NSLocalizedString(@"Format",@"format submenu title") action:NULL keyEquivalent:@""] autorelease];
	[theMenu addItem:theMenuItem];
	[theMenu setSubmenu:formatMenu forItem:theMenuItem];
	
	
	[self setMenu:theMenu];
    
	
    // Insert Password menus
    static BOOL additionalEditItems = YES;
    
    if (additionalEditItems) {
        additionalEditItems = NO;
		
        NSMenu *editMenu = [[NSApp mainMenu] numberOfItems] > 2 ? [[[NSApp mainMenu] itemAtIndex:2] submenu] : nil;
		
//		if (IsSnowLeopardOrLater) {
//            
//			theMenuItem = [[NSMenuItem alloc] initWithTitle:NSLocalizedString(@"Use Automatic Text Replacement", "use-text-replacement command in the edit menu")
//													 action:@selector(toggleAutomaticTextReplacement:) keyEquivalent:@""];
//			[theMenuItem setTarget:self];
//			[editMenu addItem:theMenuItem];
//			[theMenuItem release];
//		}
		theMenuItem = [[[NSMenuItem alloc] initWithTitle:NSLocalizedString(@"Insert Link",@"insert link menu item title") action:@selector(insertLink:) keyEquivalent:@"L"] autorelease];
        [theMenuItem setTarget:self];
        [editMenu addItem:theMenuItem];
		theMenuItem = [[[NSMenuItem alloc] initWithTitle:NSLocalizedString(@"Insert Markdown Link",@"markdown link menu item title") action:@selector(insertMarkdownLink:) keyEquivalent:@"k"] autorelease];
		[theMenuItem setKeyEquivalentModifierMask:NSCommandKeyMask | NSAlternateKeyMask];
		[theMenuItem setTarget:self];
		[editMenu addItem:theMenuItem];
        
		[editMenu addItem:[NSMenuItem separatorItem]];
        
#if PASSWORD_SUGGESTIONS
        theMenuItem = [[NSMenuItem alloc] initWithTitle:NSLocalizedString(@"New Password...", "new password command in the edit menu")
												 action:@selector(showGeneratedPasswords:) keyEquivalent:@"\\"];
        [theMenuItem setKeyEquivalentModifierMask:NSCommandKeyMask];
        [theMenuItem setTarget:nil]; // First Responder being the current Link Editor
        [editMenu addItem:theMenuItem];
        [theMenuItem release];
#endif
        
//        theMenuItem = [[NSMenuItem alloc] initWithTitle:NSLocalizedString(@"Insert New Password", "insert new password command in the edit menu")
//												 action:@selector(insertGeneratedPassword:) keyEquivalent:@"\\"];
//#if PASSWORD_SUGGESTIONS
//        [theMenuItem setAlternate:YES];
//#endif
//        [theMenuItem setKeyEquivalentModifierMask:NSCommandKeyMask|NSAlternateKeyMask];
//        [theMenuItem setTarget:nil]; // First Responder being the current Link Editor
//        [editMenu addItem:theMenuItem];
//        [theMenuItem release];
    }

}

- (void)insertPassword:(NSString*)password
{
    [self insertText:password];
    @try {
    NSPasteboard *pb = [NSPasteboard generalPasteboard];
    #if MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_6
    NSPasteboardItem *pbitem = [[[NSPasteboardItem alloc] init] autorelease];
    [pbitem setData:[password dataUsingEncoding:NSUTF8StringEncoding] forType:@"public.plain-text"];
    [pb writeObjects:[NSArray arrayWithObject:pbitem]];
    #else
    [pb declareTypes:[NSArray arrayWithObject:NSStringPboardType] owner:nil];
    [pb setString:password forType:NSStringPboardType];
    #endif
    } @catch (NSException *e) {}
}

- (void)insertGeneratedPassword:(id)sender {
    NSString *password = [NVPasswordGenerator strong];
    [self insertPassword:password];
}

- (void)showGeneratedPasswords:(id)sender {
    #ifdef notyet
    NSArray *suggestedPasswords = [NVPasswordGenerator suggestions];
    
    // display modal overlay, get user selection and insert it
    // Nice to have:
    // keep stats on the user's selection and then use the most frequent choice in [insertGeneratedPassword] (instead of just [strong])
    #else
    [self insertGeneratedPassword:nil];
    #endif
}

- (void)dealloc {
    [NSObject cancelPreviousPerformRequestsWithTarget:self];
    NVLogInfo(@"dealloc linkinged");
    [self unbind:@"automaticQuoteSubstitutionEnabled"];
    [self unbind:@"automaticDashSubstitutionEnabled"];
    [self unbind:@"automaticTextReplacementEnabled"];
    [self unbind:@"continuousSpellCheckingEnabled"];
    [self unbind:@"smartInsertDeleteEnabled"];
	[[NSNotificationCenter defaultCenter] removeObserver: self];
    if (IsLionOrLater) {
        [textFinder release];
    }
    [activeParagraphPastCursor release];
    [activeParagraph release];
    [activeParagraphBeforeCursor release];
    [beforeString release];
    [afterString release];
    [controlField release];
    [notesTableView release];
    [prefsController release];
    [lastImportedFindString release];
    [stringDuringFind release];
    [noteDuringFind release];
    
	[super dealloc];
}


#pragma mark - nvALT additions

- (void)setNeedsDisplayInRect:(NSRect)invalidRect {
	if (!expandingDisplayRect) {
		expandingDisplayRect = YES;
		invalidRect = NVTextRedrawRect(invalidRect);
		[super setNeedsDisplayInRect:invalidRect];
		expandingDisplayRect = NO;
		return;
	}
	[super setNeedsDisplayInRect:invalidRect];
}

- (void)setNeedsDisplayInRect:(NSRect)rect avoidAdditionalLayout:(BOOL)flag {
	if (!expandingDisplayRect) {
		expandingDisplayRect = YES;
		rect = NVTextRedrawRect(rect);
		[super setNeedsDisplayInRect:rect avoidAdditionalLayout:flag];
		expandingDisplayRect = NO;
		return;
	}
	[super setNeedsDisplayInRect:rect avoidAdditionalLayout:flag];
}

- (BOOL)isOpaque{
    return YES;
}

- (void)flagsChanged:(NSEvent *)theEvent{
	[(AppController *)[NSApp delegate] flagsChanged:theEvent];
}

- (BOOL)mouseIsHere{
    NSPoint mPt;
    NSRect vRect=[[self enclosingScrollView]visibleRect];
    if (IsLionOrLater) {
        NSRect aRect=NSZeroRect;
        aRect.origin=[NSEvent mouseLocation];
        mPt=[[self enclosingScrollView] convertPoint:[[self window] convertRectFromScreen:aRect].origin fromView:nil];
        if ([self textFinderIsVisible]) {
            NSView *fbView=[[self enclosingScrollView]findBarView];
            if (NSMouseInRect(mPt,[fbView frame],[fbView isFlipped])) {
                if (backgroundIsDark) {
                    [[self window]invalidateCursorRectsForView:fbView];
                }
                return NO;
            }
        }
    }else{
        mPt= [[self window]convertScreenToBase:[NSEvent mouseLocation]];
    }
    //    vRect.size.width=[[self enclosingScrollView]visibleRect].size.width;
    //     NSLog(@"mPt:%@     vRect :>%@<",NSStringFromPoint(mPt),NSStringFromRect(vRect));
    return NSMouseInRect(mPt,vRect,YES);
}


- (void)setMouseInside:(BOOL)inside{
    mouseInside=inside;
    [self fixCursorForBackgroundUpdatingMouseInside:NO];
}

- (void)selectRangeAndRegisterUndo:(NSRange)selRange{
    if (!NSEqualRanges([self selectedRange], selRange)) {
        [[[self undoManager] prepareWithInvocationTarget:self]
         selectRangeAndRegisterUndo:[self selectedRange]];
        [self setSelectedRange:selRange];
    }
}

- (void)insertText:(id)string {
    if([prefsController useAutoPairing]){
        NSString *oppositeAppend = nil;
        NSString *insertString;
        if([string isKindOfClass:[NSAttributedString class]]){
            insertString=[(NSAttributedString *)string string];
        }else{
            insertString=(NSString *)string;
        }
        NSInteger pairCode = [insertString isPairedCharacterWithMatchString:&oppositeAppend];
        if(pairCode>=0){
            NSString *appendString = insertString;
            NSRange selRange = [self selectedRange];
            NVAutoPairAction action = NVAutoPairActionForInsert(appendString, oppositeAppend, pairCode, self.activeParagraphBeforeCursor, self.activeParagraphPastCursor, selRange.length);
            if (action == NVAutoPairTypeOver) {
                selRange.location+=1;
                [self selectRangeAndRegisterUndo:selRange];
                return;
            }
            if (action == NVAutoPairWrapOrInsert) {
                if (selRange.length>0) {
                    NSString *prefix = appendString;
                    NSString *suffix = oppositeAppend;
                    NSString *affix = NVAutoPairSelectionAffix(appendString);
                    if (affix) {
                        prefix = affix;
                        suffix = affix;
                    }
                    [[[self undoManager] prepareWithInvocationTarget:self] setSelectedRange:selRange];
                    NSRange insRange=selRange;
                    insRange.length=0;
                    [super insertText:prefix replacementRange:insRange];
                    insRange.location += selRange.length + [prefix length];
                    [super insertText:suffix replacementRange:insRange];
                    insRange.location += [suffix length];
                    [self setSelectedRange:insRange];
                    return;
                }else {
                    [super insertText:[appendString stringByAppendingString:oppositeAppend]];
                    [self setSelectedRange:NSMakeRange(selRange.location+appendString.length, 0)];
                    // The pair is inserted with the caret after the closer, then moved between
                    // the brackets. Offer link ids once the caret is actually inside.
                    if ([appendString isEqualToString:@"["])
                        [self completeSuggestedLinks];
                    return;
                }
            }
        }
    }
    [super insertText:string];
}

- (IBAction)insertLink:(id)sender{
    if ([[self window] firstResponder]!=self) {
        [[self window] makeFirstResponder:self];
    }
    
    [self changeMarkdownAttribute:@"[["];
//    NSRange selRange = [self selectedRange];
//    if (selRange.length>0) {
//        NSString *selString = [[self string] substringWithRange:selRange];
//        selString = [NSString stringWithFormat:@"[[%@]]",selString];
//        [super insertText:selString];
//        
//    }else{
//        [super insertText:@"[[]]"];
//        [self setSelectedRange:NSMakeRange([self selectedRange].location-2, 0)];
//    }
    
} 

- (BOOL)changeMarkdownAttribute:(NSString *)syntaxBit{
    NSUInteger syntaxLength=syntaxBit.length;
    NSRange selRange=[self selectedRange];
    NSString *bifoString=[NSString stringWithString:self.activeParagraphBeforeCursor];
    NSString *aftaString=[NSString stringWithString:self.activeParagraphPastCursor];
    NSString *matchingPair=syntaxBit;
    if ([matchingPair isEqualToString:@"[["]) {
        matchingPair=@"]]";
    }
    if (selRange.length==0){
        if([[aftaString stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]]hasPrefix:matchingPair]){
            NSString *trimmedBefore=[bifoString stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];
            if ([trimmedBefore hasSuffix:syntaxBit]) {
                selRange.location-=syntaxLength;
                NSUInteger diff=syntaxLength*2;
                if ([bifoString hasSuffix:@" "]) {
                    diff=(bifoString.length-trimmedBefore.length);
                    if ([bifoString hasPrefix:@" "]) {
                        do {
                            bifoString = [bifoString substringFromIndex:1];
                            diff-=1;
                        } while ([bifoString hasPrefix:@" "]);
                    }
                    selRange.location-=diff;
                    diff+=(syntaxLength*2);
                }
                selRange.length=diff;
                [self insertText:@"" replacementRange:selRange];
            }else{
                selRange.location+=([aftaString rangeOfString:matchingPair].location+syntaxLength);
                selRange.length=0;
                [self setSelectedRange:selRange];
            }
            return YES;
        }
    }else{
        NSString *selString=[[self string]substringWithRange:selRange];
        aftaString=[aftaString substringFromIndex:selRange.length];
        
        NSRange beforeSyntax=NSMakeRange(NSNotFound, 0);
        NSRange afterSyntax=NSMakeRange(NSNotFound, 0);
        
        if ([aftaString hasPrefix:matchingPair]) {
            if([bifoString hasSuffix:syntaxBit]){
                beforeSyntax=selRange;
                beforeSyntax.length=syntaxLength;
                afterSyntax=beforeSyntax;
                beforeSyntax.location-=syntaxLength;
                afterSyntax.location+=selRange.length;
                selRange.location=NSNotFound;
            }else{
                afterSyntax.location=NSMaxRange(selRange);
                afterSyntax.length=syntaxLength;
                [super insertText:@"" replacementRange:afterSyntax];
                selRange.length=0;
                [super insertText:syntaxBit replacementRange:selRange];
                return YES;
            }
        }else if([bifoString hasSuffix:syntaxBit]){
            beforeSyntax=selRange;
            beforeSyntax.location-=syntaxLength;
            beforeSyntax.length=syntaxLength;
            [super insertText:@"" replacementRange:beforeSyntax];
            selRange.location=NSMaxRange(selRange);
            selRange.location-=syntaxLength;
            selRange.length=0;
            [super insertText:matchingPair replacementRange:selRange];
            return YES;
            
        } else if(([selString hasPrefix:syntaxBit])&&([selString hasSuffix:matchingPair])){
            afterSyntax=selRange;
            afterSyntax.location+=(selRange.length-syntaxLength);
            afterSyntax.length=syntaxLength;
            beforeSyntax=selRange;
            beforeSyntax.length=syntaxLength;
            selRange.length-=(syntaxLength*2);
        }else if(([selString rangeOfString:syntaxBit].location!=NSNotFound)||([selString rangeOfString:matchingPair].location!=NSNotFound)){
            NSUInteger syntCt=[[selString componentsSeparatedByString:syntaxBit] count];
            if ((syntCt % 2)==0) {//odd number of syntaxBits
                
                NSRange insertRange=selRange;
                insertRange.length=0;
                NSRange synRange=[selString rangeOfString:syntaxBit];
                if (synRange.location==0) {
                    insertRange.location+=(selRange.length-syntaxLength);
                }
                synRange.location+=selRange.location;
                [self insertText:@"" replacementRange:synRange];
                selRange.length-=syntaxLength;
                [self insertText:syntaxBit replacementRange:insertRange];
                [self setSelectedRange:selRange];
                return YES;
            }else{
                NVLogWarning(@"trying to add markdown syntax, but selection string contains an even# of the syntax. haven't dealt with this condition yet");
            }
        }
        
        if (beforeSyntax.location!=NSNotFound&&afterSyntax.location!=NSNotFound) {
            [super insertText:@"" replacementRange:afterSyntax];
            [super insertText:@"" replacementRange:beforeSyntax];
            if (selRange.location!=NSNotFound) {
                [self setSelectedRange:selRange];
            }
            return YES;
        }
    }
    if (selRange.length>0) {
        NSRange insertRange=selRange;
        insertRange.length=0;
        [super insertText:syntaxBit replacementRange:insertRange];
        insertRange.location+=(selRange.length+syntaxLength);
        [super insertText:matchingPair replacementRange:insertRange];
        insertRange.location+=syntaxLength;
        selRange.location+=syntaxLength;
        [self setSelectedRange:selRange];
        return YES;
    }else{
        NSString *doubleString=[syntaxBit stringByAppendingString:matchingPair];
        [super insertText:doubleString];
        selRange.location+=syntaxLength;
        [self setSelectedRange:selRange];
        return YES;
    }
    return NO;
}



static NSUInteger NVListIndentWidth(NSString *line, NSUInteger contentLength, NSUInteger tabWidth, NSUInteger *markerIndex) {
	NSUInteger width = 0;
	NSUInteger index = 0;
	while (index < contentLength) {
		unichar character = [line characterAtIndex:index];
		if (character == '\t')
			width += tabWidth;
		else if (character == ' ')
			width += 1;
		else
			break;
		index++;
	}
	if (markerIndex)
		*markerIndex = index;
	return width;
}

static BOOL NVLineIsFence(NSString *line, NSUInteger contentLength) {
	NSUInteger index = 0;
	while (index < contentLength && ([line characterAtIndex:index] == ' ' || [line characterAtIndex:index] == '\t'))
		index++;
	if (contentLength - index < 3)
		return NO;
	unichar fence = [line characterAtIndex:index];
	if (fence != '`' && fence != '~')
		return NO;
	NSUInteger start = index;
	while (index < contentLength && [line characterAtIndex:index] == fence)
		index++;
	if (index - start < 3)
		return NO;
	while (index < contentLength && ([line characterAtIndex:index] == ' ' || [line characterAtIndex:index] == '\t'))
		index++;
	while (index < contentLength && [[NSCharacterSet alphanumericCharacterSet] characterIsMember:[line characterAtIndex:index]])
		index++;
	return index == contentLength;
}

// Numbered markers are "1." or "1)" followed by whitespace. A run of digits that is not a marker returns NO.
static BOOL NVNumberedMarker(NSString *line, NSUInteger contentLength, NSUInteger markerIndex, NSRange *digits, NSInteger *number) {
	if (markerIndex >= contentLength || ![[NSCharacterSet decimalDigitCharacterSet] characterIsMember:[line characterAtIndex:markerIndex]])
		return NO;
	NSUInteger end = markerIndex;
	while (end < contentLength && [[NSCharacterSet decimalDigitCharacterSet] characterIsMember:[line characterAtIndex:end]])
		end++;
	if (end >= contentLength || ([line characterAtIndex:end] != '.' && [line characterAtIndex:end] != ')'))
		return NO;
	if (end + 1 >= contentLength || ![[NSCharacterSet whitespaceCharacterSet] characterIsMember:[line characterAtIndex:end + 1]])
		return NO;
	if (digits)
		*digits = NSMakeRange(markerIndex, end - markerIndex);
	if (number)
		*number = [[line substringWithRange:NSMakeRange(markerIndex, end - markerIndex)] integerValue];
	return YES;
}

static BOOL NVBulletMarker(NSString *line, NSUInteger contentLength, NSUInteger markerIndex) {
	if (markerIndex >= contentLength || ![[NSCharacterSet listBulletsCharacterSet] characterIsMember:[line characterAtIndex:markerIndex]])
		return NO;
	if (markerIndex + 1 < contentLength && [line characterAtIndex:markerIndex + 1] == [line characterAtIndex:markerIndex])
		return NO;
	return markerIndex + 1 < contentLength && [[NSCharacterSet whitespaceCharacterSet] characterIsMember:[line characterAtIndex:markerIndex + 1]];
}

- (NSUInteger)listTabWidth {
	NSInteger width = [prefsController numberOfSpacesInTab];
	return width > 0 ? (NSUInteger)width : 4;
}

- (void)scheduleNumberedListRenumber {
	if (renumberingNumberedLists || ![prefsController autoFormatsListBullets])
		return;
	if (!NVMarkdownSyntaxHighlightEnabledForLength([[self string] length]))
		return;
	if ([[self undoManager] isUndoing] || [[self undoManager] isRedoing])
		return;
	[NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(renumberNumberedLists) object:nil];
	[self performSelector:@selector(renumberNumberedLists) withObject:nil afterDelay:0.05];
}

- (void)renumberNumberedLists {
	if (renumberingNumberedLists || ![prefsController autoFormatsListBullets])
		return;
	if (!NVMarkdownSyntaxHighlightEnabledForLength([[self string] length]))
		return;
	if ([[self undoManager] isUndoing] || [[self undoManager] isRedoing])
		return;
	NSString *text = [self string];
	NSUInteger length = [text length];
	if (!length)
		return;
	NSUInteger tabWidth = [self listTabWidth];
	NSMutableDictionary *nextNumberAtWidth = [NSMutableDictionary dictionary];
	NSMutableArray *digitRanges = [NSMutableArray array];
	NSMutableArray *replacements = [NSMutableArray array];
	BOOL insideFence = NO;
	NSUInteger index = 0;
	while (index < length) {
		NSRange lineRange = [text lineRangeForRange:NSMakeRange(index, 0)];
		NSString *line = [text substringWithRange:lineRange];
		NSUInteger contentLength = [line length];
		while (contentLength && [[NSCharacterSet newlineCharacterSet] characterIsMember:[line characterAtIndex:contentLength - 1]])
			contentLength--;
		if (NVLineIsFence(line, contentLength))
			insideFence = !insideFence;
		else if (!insideFence && contentLength) {
			NSUInteger markerIndex = 0;
			NSUInteger width = NVListIndentWidth(line, contentLength, tabWidth, &markerIndex);
			NSRange digits = NSMakeRange(NSNotFound, 0);
			NSInteger number = 0;
			if (NVNumberedMarker(line, contentLength, markerIndex, &digits, &number)) {
				NSNumber *key = [NSNumber numberWithUnsignedInteger:width];
				NSInteger expected = [[nextNumberAtWidth objectForKey:key] integerValue];
				if (expected < 1)
					expected = 1;
				[nextNumberAtWidth setObject:[NSNumber numberWithInteger:expected + 1] forKey:key];
				NSArray *widths = [[nextNumberAtWidth allKeys] copy];
				NSUInteger widthIndex;
				for (widthIndex = 0; widthIndex < [widths count]; widthIndex++) {
					NSNumber *deeper = [widths objectAtIndex:widthIndex];
					if ([deeper unsignedIntegerValue] > width)
						[nextNumberAtWidth removeObjectForKey:deeper];
				}
				[widths release];
				if (number != expected) {
					[digitRanges addObject:[NSValue valueWithRange:NSMakeRange(lineRange.location + digits.location, digits.length)]];
					[replacements addObject:[NSString stringWithFormat:@"%ld", (long)expected]];
				}
			} else {
				NSUInteger resetAt = width;
				BOOL resetsThisLevel = YES;
				if (NVBulletMarker(line, contentLength, markerIndex))
					resetsThisLevel = YES;
				else if (markerIndex >= contentLength)
					resetsThisLevel = NO;
				if (resetsThisLevel || markerIndex < contentLength) {
					NSArray *widths = [[nextNumberAtWidth allKeys] copy];
					NSUInteger widthIndex;
					for (widthIndex = 0; widthIndex < [widths count]; widthIndex++) {
						NSNumber *existing = [widths objectAtIndex:widthIndex];
						NSUInteger existingWidth = [existing unsignedIntegerValue];
						if (existingWidth > width || (resetsThisLevel && existingWidth == resetAt))
							[nextNumberAtWidth removeObjectForKey:existing];
					}
					[widths release];
				}
			}
		}
		if (NSMaxRange(lineRange) <= index)
			break;
		index = NSMaxRange(lineRange);
	}
	if (![replacements count])
		return;

	renumberingNumberedLists = YES;
	NSRange selection = [self selectedRange];
	BOOL selectionInsideNumber = NO;
	NSInteger shiftBeforeCaret = 0;
	NSUInteger changeIndex;
	[[self undoManager] beginUndoGrouping];
	for (changeIndex = [replacements count]; changeIndex > 0; changeIndex--) {
		NSRange range = [[digitRanges objectAtIndex:changeIndex - 1] rangeValue];
		NSString *replacement = [replacements objectAtIndex:changeIndex - 1];
		if ([self shouldChangeTextInRange:range replacementString:replacement])
			[self replaceCharactersInRange:range withString:replacement];
	}
	for (changeIndex = 0; changeIndex < [replacements count]; changeIndex++) {
		NSRange range = [[digitRanges objectAtIndex:changeIndex] rangeValue];
		NSString *replacement = [replacements objectAtIndex:changeIndex];
		NSInteger delta = (NSInteger)[replacement length] - (NSInteger)range.length;
		if (NSMaxRange(range) <= selection.location)
			shiftBeforeCaret += delta;
		else if (range.location < selection.location && selection.location < NSMaxRange(range)) {
			selection.location = range.location + [replacement length];
			selection.length = 0;
			selectionInsideNumber = YES;
		}
	}
	if (!selectionInsideNumber)
		selection.location += shiftBeforeCaret;
	NSUInteger editedLength = [[self string] length];
	if (selection.location > editedLength)
		selection.location = editedLength;
	if (NSMaxRange(selection) > editedLength)
		selection.length = editedLength - selection.location;
	[self setSelectedRange:selection];
	[self didChangeText];
	[[self undoManager] setActionName:NSLocalizedString(@"Renumber List", nil)];
	[[self undoManager] endUndoGrouping];
	renumberingNumberedLists = NO;
}

- (BOOL)updateNumberedListFromRange:(NSRange)currentRange startingNum:(NSInteger)listNum {
	if (![prefsController autoFormatsListBullets])
		return NO;
	[self scheduleNumberedListRenumber];
	return YES;
}

- (BOOL)lineRangeIsListItem:(NSRange)lineRange inText:(NSString *)text tabWidth:(NSUInteger)tabWidth indentWidth:(NSUInteger *)outWidth {
	if (!lineRange.length || NSMaxRange(lineRange) > [text length])
		return NO;
	NSString *line = [text substringWithRange:lineRange];
	NSUInteger contentLength = [line length];
	while (contentLength && [[NSCharacterSet newlineCharacterSet] characterIsMember:[line characterAtIndex:contentLength - 1]])
		contentLength--;
	NSUInteger markerIndex = 0;
	NSUInteger width = NVListIndentWidth(line, contentLength, tabWidth, &markerIndex);
	if (outWidth)
		*outWidth = width;
	return NVNumberedMarker(line, contentLength, markerIndex, NULL, NULL) || NVBulletMarker(line, contentLength, markerIndex);
}

- (BOOL)indentListItemsInSelection:(BOOL)outdent {
	if (![prefsController autoFormatsListBullets])
		return NO;
	NSString *text = [self string];
	NSRange selection = [self selectedRange];
	if (![text length] || selection.location > [text length])
		return NO;
	NSUInteger endLocation = selection.length ? NSMaxRange(selection) - 1 : selection.location;
	if (endLocation >= [text length])
		endLocation = [text length] - 1;
	NSRange firstLine = [text lineRangeForRange:NSMakeRange(MIN(selection.location, [text length] - 1), 0)];
	NSRange lastLine = [text lineRangeForRange:NSMakeRange(endLocation, 0)];
	NSUInteger tabWidth = [self listTabWidth];
	NSMutableArray *lines = [NSMutableArray array];
	NSUInteger scan = firstLine.location;
	BOOL foundList = NO;
	while (scan <= lastLine.location && scan < [text length]) {
		NSRange lineRange = [text lineRangeForRange:NSMakeRange(scan, 0)];
		NSUInteger width = 0;
		BOOL isList = [self lineRangeIsListItem:lineRange inText:text tabWidth:tabWidth indentWidth:&width];
		if (isList)
			foundList = YES;
		[lines addObject:[NSValue valueWithRange:lineRange]];
		if (NSMaxRange(lineRange) <= scan)
			break;
		scan = NSMaxRange(lineRange);
	}
	if (!foundList)
		return NO;
	NSString *unit = [prefsController softTabs] ? [@"" stringByPaddingToLength:tabWidth withString:@" " startingAtIndex:0] : @"\t";
	[[self undoManager] beginUndoGrouping];
	NSInteger lineIndex;
	for (lineIndex = (NSInteger)[lines count] - 1; lineIndex >= 0; lineIndex--) {
		NSRange lineRange = [[lines objectAtIndex:(NSUInteger)lineIndex] rangeValue];
		text = [self string];
		if (NSMaxRange(lineRange) > [text length])
			continue;
		if (![self lineRangeIsListItem:lineRange inText:text tabWidth:tabWidth indentWidth:NULL])
			continue;
		NSString *line = [text substringWithRange:lineRange];
		NSString *core = line;
		if ([core length] && [[NSCharacterSet newlineCharacterSet] characterIsMember:[core characterAtIndex:[core length] - 1]])
			core = [core substringToIndex:[core length] - 1];
		NSRange edited = NSMakeRange(lineRange.location, 0);
		NSString *changeString = nil;
		if (outdent) {
			if ([core hasPrefix:unit])
				edited.length = [unit length];
			else if ([core hasPrefix:@"\t"])
				edited.length = 1;
			else {
				NSUInteger spaces = 0;
				while (spaces < [core length] && spaces < tabWidth && [core characterAtIndex:spaces] == ' ')
					spaces++;
				if (!spaces)
					continue;
				edited.length = spaces;
			}
			changeString = @"";
		} else {
			changeString = unit;
		}
		if (![self shouldChangeTextInRange:edited replacementString:changeString])
			continue;
		[self replaceCharactersInRange:edited withString:changeString];
		NSInteger delta = (NSInteger)[changeString length] - (NSInteger)edited.length;
		if (NSMaxRange(edited) <= selection.location) {
			selection.location += delta;
		} else 		if (edited.location < NSMaxRange(selection)) {
			if (edited.location <= selection.location)
				selection.location = edited.location + [changeString length];
			NSInteger newLength = (NSInteger)selection.length + delta;
			selection.length = newLength > 0 ? (NSUInteger)newLength : 0;
		}
	}
	NSUInteger editedLength = [[self string] length];
	if (selection.location > editedLength)
		selection.location = editedLength;
	if (NSMaxRange(selection) > editedLength)
		selection.length = editedLength - selection.location;
	[self setSelectedRange:selection];
	[self didChangeText];
	[[self undoManager] setActionName:outdent ? NSLocalizedString(@"Outdent", nil) : NSLocalizedString(@"Indent", nil)];
	[[self undoManager] endUndoGrouping];
	return YES;
}

- (NSArray *)nv_lineRangesForSelection:(NSRange)selection inText:(NSString *)text {
	NSMutableArray *lines = [NSMutableArray array];
	if (!text || selection.location > [text length])
		return lines;
	if (![text length]) {
		[lines addObject:[NSValue valueWithRange:NSMakeRange(0, 0)]];
		return lines;
	}
	NSUInteger start = MIN(selection.location, [text length]);
	NSUInteger endLocation = selection.length ? NSMaxRange(selection) - 1 : start;
	if (endLocation >= [text length])
		endLocation = [text length] - 1;
	NSRange firstLine = [text lineRangeForRange:NSMakeRange(start, 0)];
	NSRange lastLine = [text lineRangeForRange:NSMakeRange(endLocation, 0)];
	NSUInteger scan = firstLine.location;
	while (scan <= lastLine.location && scan <= [text length]) {
		NSRange lineRange = [text lineRangeForRange:NSMakeRange(MIN(scan, [text length]), 0)];
		[lines addObject:[NSValue valueWithRange:lineRange]];
		if (NSMaxRange(lineRange) <= scan)
			break;
		scan = NSMaxRange(lineRange);
	}
	return lines;
}

- (NSRange)nv_markerRangeInLine:(NSString *)line contentLength:(NSUInteger)contentLength markerIndex:(NSUInteger)markerIndex {
	NSRange digits = NSMakeRange(NSNotFound, 0);
	if (NVNumberedMarker(line, contentLength, markerIndex, &digits, NULL)) {
		NSUInteger markerEnd = digits.location + digits.length + 1;
		while (markerEnd < contentLength && [[NSCharacterSet whitespaceCharacterSet] characterIsMember:[line characterAtIndex:markerEnd]])
			markerEnd++;
		return NSMakeRange(digits.location, markerEnd - digits.location);
	}
	if (NVBulletMarker(line, contentLength, markerIndex)) {
		NSUInteger markerEnd = markerIndex + 1;
		while (markerEnd < contentLength && [[NSCharacterSet whitespaceCharacterSet] characterIsMember:[line characterAtIndex:markerEnd]])
			markerEnd++;
		return NSMakeRange(markerIndex, markerEnd - markerIndex);
	}
	return NSMakeRange(NSNotFound, 0);
}

- (NSRange)nv_rangeCoveringLineCount:(NSUInteger)count fromLocation:(NSUInteger)start {
	NSString *text = [self string];
	if (start > [text length])
		start = [text length];
	NSUInteger location = start;
	NSRange lastLine = NSMakeRange(start, 0);
	NSUInteger remaining = count;
	while (remaining && location <= [text length]) {
		lastLine = [text lineRangeForRange:NSMakeRange(MIN(location, [text length]), 0)];
		remaining--;
		if (!remaining || NSMaxRange(lastLine) <= location)
			break;
		location = NSMaxRange(lastLine);
	}
	NSRange covered = NSUnionRange(NSMakeRange(start, 0), lastLine);
	if (covered.location > [text length])
		covered.location = [text length];
	if (NSMaxRange(covered) > [text length])
		covered.length = [text length] - covered.location;
	return covered;
}

- (BOOL)nv_cycleListMarkersOnLineRanges:(NSArray *)lines {
	NSString *text = [self string];
	NSUInteger tabWidth = [self listTabWidth];
	NSUInteger plainCount = 0;
	NSUInteger bulletCount = 0;
	NSUInteger numberedCount = 0;
	for (NSValue *value in lines) {
		NSRange lineRange = [value rangeValue];
		if (NSMaxRange(lineRange) > [text length])
			continue;
		NSString *line = [text substringWithRange:lineRange];
		NSUInteger contentLength = [line length];
		while (contentLength && [[NSCharacterSet newlineCharacterSet] characterIsMember:[line characterAtIndex:contentLength - 1]])
			contentLength--;
		NSUInteger markerIndex = 0;
		NVListIndentWidth(line, contentLength, tabWidth, &markerIndex);
		if (markerIndex >= contentLength)
			continue;
		NSRange marker = [self nv_markerRangeInLine:line contentLength:contentLength markerIndex:markerIndex];
		if (marker.location == NSNotFound)
			plainCount++;
		else if (NVNumberedMarker(line, contentLength, markerIndex, NULL, NULL))
			numberedCount++;
		else
			bulletCount++;
	}
	if (!plainCount && !bulletCount && !numberedCount)
		return YES;
	/* Mixed lines clear back to plain text. A uniform bullet selection becomes
	   numbers, and a uniform numbered selection is the step that clears them. */
	BOOL remove = (plainCount && (bulletCount || numberedCount)) || (numberedCount && !bulletCount);
	BOOL number = !remove && bulletCount > 0;
	NSString *bullet = [[prefsController markdownListMarker] stringByAppendingString:@" "];
	NSUInteger firstLocation = [[lines objectAtIndex:0] rangeValue].location;
	[[self undoManager] beginUndoGrouping];
	BOOL changed = NO;
	NSInteger lineIndex;
	for (lineIndex = (NSInteger)[lines count] - 1; lineIndex >= 0; lineIndex--) {
		text = [self string];
		NSRange lineRange = [[lines objectAtIndex:(NSUInteger)lineIndex] rangeValue];
		if (NSMaxRange(lineRange) > [text length])
			continue;
		NSString *line = [text substringWithRange:lineRange];
		NSUInteger contentLength = [line length];
		while (contentLength && [[NSCharacterSet newlineCharacterSet] characterIsMember:[line characterAtIndex:contentLength - 1]])
			contentLength--;
		NSUInteger markerIndex = 0;
		NVListIndentWidth(line, contentLength, tabWidth, &markerIndex);
		NSRange marker = [self nv_markerRangeInLine:line contentLength:contentLength markerIndex:markerIndex];
		NSRange edited = NSMakeRange(NSNotFound, 0);
		NSString *replacement = nil;
		if (remove) {
			if (marker.location == NSNotFound)
				continue;
			edited = NSMakeRange(lineRange.location + marker.location, marker.length);
			replacement = @"";
		} else if (number) {
			if (marker.location == NSNotFound || NVNumberedMarker(line, contentLength, markerIndex, NULL, NULL))
				continue;
			edited = NSMakeRange(lineRange.location + marker.location, marker.length);
			replacement = @"1. ";
		} else {
			if (marker.location != NSNotFound || markerIndex >= contentLength)
				continue;
			edited = NSMakeRange(lineRange.location + markerIndex, 0);
			replacement = bullet;
		}
		if (![self shouldChangeTextInRange:edited replacementString:replacement])
			continue;
		[self replaceCharactersInRange:edited withString:replacement];
		changed = YES;
	}
	if (changed) {
		NSRange covered = [self nv_rangeCoveringLineCount:[lines count] fromLocation:firstLocation];
		[self setSelectedRange:covered];
		if (number) {
			renumberingNumberedLists = YES;
			[self didChangeText];
			renumberingNumberedLists = NO;
			[self renumberNumberedLists];
			NSUInteger renumberedStart = [self selectedRange].location;
			[self setSelectedRange:[self nv_rangeCoveringLineCount:[lines count] fromLocation:renumberedStart]];
		} else {
			[self didChangeText];
		}
		NSString *actionName = NSLocalizedString(@"Add List Markers", nil);
		if (remove)
			actionName = NSLocalizedString(@"Remove List Markers", nil);
		else if (number)
			actionName = NSLocalizedString(@"Number List", nil);
		[[self undoManager] setActionName:actionName];
	}
	[[self undoManager] endUndoGrouping];
	return YES;
}

- (BOOL)nv_toggleTaskCheckboxForEvent:(NSEvent *)event {
	if ([event clickCount] != 1)
		return NO;
	NSUInteger flags = [event modifierFlags] & NSDeviceIndependentModifierFlagsMask;
	if (flags & (NSControlKeyMask | NSShiftKeyMask | NSCommandKeyMask | NSAlternateKeyMask))
		return NO;
	NSPoint viewPoint = [self convertPoint:[event locationInWindow] fromView:nil];
	if ([self nv_viewPointIsBelowText:viewPoint])
		return NO;
	NSTextContainer *container = [self textContainer];
	NSLayoutManager *layout = [self layoutManager];
	if (!container || !layout || ![layout numberOfGlyphs])
		return NO;
	NSPoint containerPoint = viewPoint;
	NSPoint origin = [self textContainerOrigin];
	containerPoint.x -= origin.x;
	containerPoint.y -= origin.y;
	NSUInteger glyph = [layout glyphIndexForPoint:containerPoint inTextContainer:container fractionOfDistanceThroughGlyph:NULL];
	if (glyph >= [layout numberOfGlyphs])
		return NO;
	NSRect glyphRect = [layout boundingRectForGlyphRange:NSMakeRange(glyph, 1) inTextContainer:container];
	if (!NSMouseInRect(containerPoint, glyphRect, YES))
		return NO;
	NSUInteger charIndex = [layout characterIndexForGlyphAtIndex:glyph];
	NSString *text = [self string];
	if (charIndex >= [text length] || NVTaskIndexIsInsideFence(text, charIndex))
		return NO;
	NSRange lineRange = [text lineRangeForRange:NSMakeRange(charIndex, 0)];
	NSString *line = [text substringWithRange:lineRange];
	NVTaskEdit edit = NVTaskToggleEdit(line, charIndex - lineRange.location);
	if (edit.range.location == NSNotFound)
		return NO;
	NSRange documentRange = NSMakeRange(lineRange.location + edit.range.location, edit.range.length);
	if (![self shouldChangeTextInRange:documentRange replacementString:edit.replacement])
		return NO;
	[[self undoManager] beginUndoGrouping];
	[self replaceCharactersInRange:documentRange withString:edit.replacement];
	[self setSelectedRange:NSMakeRange(documentRange.location + [edit.replacement length], 0)];
	[self didChangeText];
	[[self undoManager] setActionName:NSLocalizedString(@"Toggle Task", nil)];
	[[self undoManager] endUndoGrouping];
	return YES;
}

- (BOOL)toggleTaskMarkerOnSelectedLines {
	NSString *text = [self string];
	NSRange selection = [self selectedRange];
	if (selection.location > [text length])
		return NO;
	NSArray *lines = nil;
	BOOL multi = NO;
	if ([text length] && selection.length) {
		lines = [self nv_lineRangesForSelection:selection inText:text];
		multi = [lines count] > 1;
	}
	if (!multi) {
		NSRange lineRange = [text lineRangeForRange:NSMakeRange(MIN(selection.location, [text length]), 0)];
		lines = [NSArray arrayWithObject:[NSValue valueWithRange:lineRange]];
	}
	NSUInteger firstLocation = [[lines objectAtIndex:0] rangeValue].location;
	BOOL grouping = NO;
	BOOL changed = NO;
	BOOL removed = NO;
	BOOL added = NO;
	NSInteger lineIndex;
	for (lineIndex = (NSInteger)[lines count] - 1; lineIndex >= 0; lineIndex--) {
		text = [self string];
		NSRange lineRange = [[lines objectAtIndex:(NSUInteger)lineIndex] rangeValue];
		if (lineRange.location > [text length])
			continue;
		if (NSMaxRange(lineRange) > [text length])
			lineRange.length = [text length] - lineRange.location;
		if (NVTaskIndexIsInsideFence(text, lineRange.location))
			continue;
		NSString *line = [text substringWithRange:lineRange];
		NVTaskEdit edit = NVTaskShortcutEdit(line);
		if (edit.range.location == NSNotFound)
			continue;
		NSRange documentRange = NSMakeRange(lineRange.location + edit.range.location, edit.range.length);
		if (NSMaxRange(documentRange) > [text length])
			continue;
		if (![self shouldChangeTextInRange:documentRange replacementString:edit.replacement])
			continue;
		if (!grouping) {
			[[self undoManager] beginUndoGrouping];
			grouping = YES;
		}
		if ([edit.replacement isEqualToString:@" "])
			removed = YES;
		else
			added = YES;
		[self replaceCharactersInRange:documentRange withString:edit.replacement];
		if (!multi) {
			NSInteger delta = (NSInteger)[edit.replacement length] - (NSInteger)documentRange.length;
			if (selection.location >= NSMaxRange(documentRange))
				selection.location += delta;
			else if (selection.location > documentRange.location)
				selection.location = documentRange.location + [edit.replacement length];
			if (selection.location > [[self string] length])
				selection.location = [[self string] length];
			selection.length = 0;
		}
		changed = YES;
	}
	if (changed) {
		if (multi)
			changedRange = [self nv_rangeCoveringLineCount:[lines count] fromLocation:firstLocation];
		[self setSelectedRange:multi ? changedRange : selection];
		[self didChangeText];
		NSString *actionName = (removed && !added) ? NSLocalizedString(@"Remove Task", nil) : NSLocalizedString(@"Add Task", nil);
		[[self undoManager] setActionName:actionName];
	}
	if (grouping)
		[[self undoManager] endUndoGrouping];
	return YES;
}

- (IBAction)toggleTaskMarker:(id)sender {
	(void)sender;
	[self toggleTaskMarkerOnSelectedLines];
}

- (BOOL)cycleListMarkerOnCurrentLine {
	if (![prefsController autoFormatsListBullets])
		return NO;
	NSString *text = [self string];
	NSRange selection = [self selectedRange];
	if (selection.location > [text length])
		return NO;
	if ([text length] && selection.length) {
		NSArray *lines = [self nv_lineRangesForSelection:selection inText:text];
		if ([lines count] > 1)
			return [self nv_cycleListMarkersOnLineRanges:lines];
	}
	NSRange lineRange = [text lineRangeForRange:NSMakeRange(MIN(selection.location, [text length]), 0)];
	NSString *line = [text substringWithRange:lineRange];
	NSUInteger contentLength = [line length];
	while (contentLength && [[NSCharacterSet newlineCharacterSet] characterIsMember:[line characterAtIndex:contentLength - 1]])
		contentLength--;
	NSUInteger tabWidth = [self listTabWidth];
	NSUInteger markerIndex = 0;
	NVListIndentWidth(line, contentLength, tabWidth, &markerIndex);
	if (markerIndex > contentLength)
		return NO;
	NSRange digits = NSMakeRange(NSNotFound, 0);
	NSString *replacement = nil;
	NSRange replaceRange = NSMakeRange(lineRange.location + markerIndex, 0);
	NSInteger cursorDelta = 0;
	if (NVNumberedMarker(line, contentLength, markerIndex, &digits, NULL)) {
		NSUInteger markerEnd = digits.location + digits.length + 1;
		while (markerEnd < contentLength && [[NSCharacterSet whitespaceCharacterSet] characterIsMember:[line characterAtIndex:markerEnd]])
			markerEnd++;
		replaceRange = NSMakeRange(lineRange.location + digits.location, markerEnd - digits.location);
		replacement = @"";
		cursorDelta = -(NSInteger)replaceRange.length;
	} else if (NVBulletMarker(line, contentLength, markerIndex)) {
		NSUInteger markerEnd = markerIndex + 1;
		while (markerEnd < contentLength && [[NSCharacterSet whitespaceCharacterSet] characterIsMember:[line characterAtIndex:markerEnd]])
			markerEnd++;
		replaceRange = NSMakeRange(lineRange.location + markerIndex, markerEnd - markerIndex);
		replacement = @"1. ";
		cursorDelta = (NSInteger)[replacement length] - (NSInteger)replaceRange.length;
	} else if (markerIndex < contentLength) {
		unichar character = [line characterAtIndex:markerIndex];
		BOOL bareMarker = [[NSCharacterSet listBulletsCharacterSet] characterIsMember:character] || [[NSCharacterSet decimalDigitCharacterSet] characterIsMember:character];
		if (bareMarker)
			return YES;
		replacement = [[prefsController markdownListMarker] stringByAppendingString:@" "];
		replaceRange = NSMakeRange(lineRange.location + markerIndex, 0);
		cursorDelta = (NSInteger)[replacement length];
	} else {
		replacement = [[prefsController markdownListMarker] stringByAppendingString:@" "];
		replaceRange = NSMakeRange(lineRange.location + markerIndex, 0);
		cursorDelta = (NSInteger)[replacement length];
	}
	if (![self shouldChangeTextInRange:replaceRange replacementString:replacement])
		return YES;
	[self replaceCharactersInRange:replaceRange withString:replacement];
	if (selection.location >= NSMaxRange(replaceRange))
		selection.location += cursorDelta;
	else if (selection.location > replaceRange.location)
		selection.location = replaceRange.location + [replacement length];
	NSUInteger editedLength = [[self string] length];
	if (selection.location > editedLength)
		selection.location = editedLength;
	[self setSelectedRange:selection];
	[self didChangeText];
	return YES;
}

#pragma mark managing text width

- (void)resetInset{
    NSSize inset = [self textContainerInset];
    if (inset.width != kDefaultTextInsetWidth || inset.height != kDefaultTextInsetHeight) {
        [self setTextContainerInset:NSMakeSize(kDefaultTextInsetWidth, kDefaultTextInsetHeight)];
    }//||didRenderFully
    //    else if (didRenderFully) {
    //        NSLog(@"resetting but not");
    //    }
}


- (void)updateInsetAndForceLayout:(BOOL)force{
    [self updateInsetForFrame:[self frame] andForceLayout:force];
}

- (void)updateInsetForFrame:(NSRect)frameRect andForceLayout:(BOOL)force{
    if (managesTextWidth||([(AppController *)[NSApp delegate]isInFullScreen])) {
        [self setInsetForFrame:frameRect alwaysSet:force];
    }else{
        [self resetInset];
    }
}

- (BOOL)setInsetForFrame:(NSRect)frameRect alwaysSet:(BOOL)always{
    CGFloat insX=kDefaultTextInsetWidth;
    CGFloat maxWidth=[prefsController maxNoteBodyWidth];
    if (frameRect.size.width>maxWidth) {
        insX=kTextMargins;
        CGFloat theMin=(maxWidth+(insX*1.9));
        if (frameRect.size.width<=theMin) {
            CGFloat diff=theMin-frameRect.size.width;
            diff=diff/2;
            insX=round(insX-diff);
            if (insX<kDefaultTextInsetWidth) {
                insX=kDefaultTextInsetWidth;
            }
        }
    }
    NSSize currentInset = [self textContainerInset];
    if (always || currentInset.width != insX || currentInset.height != kDefaultTextInsetHeight) {
        [self setTextContainerInset:NSMakeSize(insX, kDefaultTextInsetHeight)];
        return YES;
    }
    return NO;
}


- (void)nv_matchVisibleMinHeight {
	NSScrollView *scroll;
	CGFloat visible;
	NSSize min;
	if (matchingMinHeight)
		return;
	scroll = [self enclosingScrollView];
	if (!scroll)
		return;
	visible = [scroll contentSize].height;
	if (visible < 1.0)
		return;
	min = [self minSize];
	if (fabs(min.height - visible) < 0.5)
		return;
	matchingMinHeight = YES;
	[self setMinSize:NSMakeSize(min.width, visible)];
	matchingMinHeight = NO;
}

- (void)setFrame:(NSRect)frameRect{
    [self nv_matchVisibleMinHeight];
    [self updateInsetForFrame:frameRect andForceLayout:NO];
    frameRect.size.height = NVNoteHeightForProposedHeight(self, frameRect.size.height);
    [super setFrame:frameRect];
}


#pragma mark Pairing

- (BOOL)pairIsOnOwnParagraph:(NSString *)closingCharacter{
    if (![closingCharacter isEqualToString:@"]"]) 
        return NO;
    
    NSString *openingChar=@"[";
    NSString *thisPar=[[NSString stringWithString:self.activeParagraph] stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];
    if ((thisPar.length>0)&&([thisPar hasSuffix:closingCharacter])&&([thisPar hasPrefix:openingChar])) {
        return YES;
    }
    return NO;
}

- (BOOL)cursorIsImmediatelyPastPair:(NSString *)closingCharacter{
    if (![closingCharacter isEqualToString:@"]"])
        return NO;   
    
        //||([self isAlreadyNearMarkdownLink]))
    NSString *testString=[self.activeParagraphBeforeCursor stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];
    if ([testString hasSuffix:closingCharacter]) {
        testString=[testString stringByTrimmingCharactersInSet:[NSCharacterSet characterSetWithCharactersInString:closingCharacter]];
        NSString *openingCharacter=@"[";
       
        NSUInteger openingDex=[testString rangeOfString:openingCharacter options:NSBackwardsSearch].location;
        NSUInteger closingDex=[testString rangeOfString:closingCharacter options:NSBackwardsSearch].location;
        if ((openingDex!=NSNotFound)&&((closingDex==NSNotFound)||(openingDex>closingDex))) {
            NSString *aftaTest=[self.activeParagraphPastCursor stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];
            testString=[[testString substringToIndex:openingDex] stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];
            NSPredicate *aftaRefPred=[NSPredicate predicateWithFormat:@"SELF BEGINSWITH[cd] %@ OR SELF LIKE[cd] %@ OR SELF LIKE[cd] %@",@":",@"[*]*",@"(*)*"];
             NSPredicate *bifoRefPred=[NSPredicate predicateWithFormat:@"SELF LIKE[cd] %@",@"*[*]"];
            if ((![aftaRefPred evaluateWithObject:aftaTest])&&(![bifoRefPred evaluateWithObject:testString])) {
                
                return YES;
            }
        }
    }
    return NO;
}


- (NSUInteger)cursorIsInsidePair:(NSString *)closingCharacter{
    if (![closingCharacter isEqualToString:@"]"])
        return NSNotFound;
    
        //||([self isAlreadyNearMarkdownLink]))
    NSString *aftaString=[self.activeParagraphPastCursor stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
    NSString *testString=self.activeParagraphBeforeCursor;
    if(([aftaString isEqualToString:@""])||([[testString stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]] isEqualToString:@""]))
        return NSNotFound;   
    
    NSString *openingChar=@"[";
    NSUInteger openingIndex=[testString rangeOfString:openingChar options:NSBackwardsSearch].location;
    NSUInteger closingIndex=[testString rangeOfString:closingCharacter options:NSBackwardsSearch].location;
//     NSLog(@"openingIndex:%lu closingIndex:%lu",openingIndex,closingIndex);
    if ((openingIndex!=NSNotFound)&&((closingIndex==NSNotFound)||(openingIndex>closingIndex))) {
        testString=[[testString substringToIndex:openingIndex] stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];
        NSPredicate *bifoRefPred=[NSPredicate predicateWithFormat:@"SELF LIKE[cd] %@",@"*[*]"];
        closingIndex=[aftaString rangeOfString:closingCharacter].location;
        openingIndex=[aftaString rangeOfString:openingChar].location;
        if ((closingIndex!=NSNotFound)&&(![bifoRefPred evaluateWithObject:testString])&&((openingIndex==NSNotFound)||(closingIndex<openingIndex))) {  
            BOOL returnIt=YES;
            if (aftaString.length>(closingIndex+1)) {
                aftaString=[[aftaString substringFromIndex:(closingIndex+1)] stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];
                NSPredicate *aftaRefPred=[NSPredicate predicateWithFormat:@"SELF BEGINSWITH[cd] %@ OR SELF LIKE[cd] %@ OR SELF LIKE[cd] %@",@":",@"[*]*",@"(*)*"];
                if([aftaRefPred evaluateWithObject:aftaString]) {
                    returnIt=NO;
                }
            }
            if(returnIt) {
                return closingIndex;//+[self selectedRange].location;                
            }
        }
    }
    return NSNotFound;    
}

- (BOOL)deleteEmptyPairsBetweenRange:(NSRange)charRange inLineRange:(NSRange)lineRange{
    if (([prefsController useAutoPairing])&&([self cursorAtRange:charRange isBetweenEmptyPairsInLineRange:lineRange])) {
        charRange.location-=1;
        [self selectRangeAndRegisterUndo:charRange];
        charRange.length=2;
        [self insertText:@"" replacementRange:charRange];   
        return YES;
    }    
    return NO;
}

- (BOOL)cursorAtRange:(NSRange)charRange isBetweenEmptyPairsInLineRange:(NSRange)actRange{
    NSUInteger keyLoc=charRange.location;
    if ((keyLoc>actRange.location)&&(keyLoc<NSMaxRange(actRange))) {
        NSString *string = [self string];
        if (NVCharactersFormEmptyPair([string characterAtIndex:keyLoc - 1], [string characterAtIndex:keyLoc]))
            return YES;
    }
    return NO;
}


#pragma mark Useful properties

- (NSString *)activeParagraphTrimWS:(BOOL)shouldTrim{
    NSRange actRange=[self rangeOfActiveParagraph];
    if ((actRange.location!=NSNotFound)&&(actRange.length>0)) {
        NSString *actPar=[[self string]substringWithRange:actRange];
        if (shouldTrim) {
            actPar=[actPar stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
        }
        if (actPar&&(actPar.length>0)) {
            return actPar;
        }     
    }
    return @"";
}

- (NSString *)activeParagraph{
    return [self activeParagraphTrimWS:YES];
}

- (NSRange)rangeOfActiveParagraph{
    NSUInteger startDex;
    NSUInteger contentsEndDex;
    [[self string] getLineStart:&startDex end:NULL contentsEnd:&contentsEndDex forRange:[self selectedRange]];
   
    if (contentsEndDex>startDex) {
        return NSMakeRange(startDex,(contentsEndDex-startDex));
    }
    return NSMakeRange(NSNotFound, 0);
    
}

- (NSString *)activeParagraphPastCursor{
    NSRange actRange=[self rangeOfActiveParagraph];
    if ((actRange.location!=NSNotFound)&&(actRange.length>0)&&(actRange.location!=[self string].length)) {
        NSUInteger diff=[self selectedRange].location-actRange.location;
        if (diff!=NSNotFound) {
            
            return [[self string] substringWithRange:NSMakeRange([self selectedRange].location, actRange.length-diff)];
        }       
    }      
    return @"";     
}

- (NSString *)activeParagraphBeforeCursor{
    NSRange actRange=[self rangeOfActiveParagraph];
    if ((actRange.location!=NSNotFound)&&(actRange.length>0)) {
        NSUInteger diff=[self selectedRange].location-actRange.location;
        if (diff!=NSNotFound) {
            return [[self string]substringWithRange:NSMakeRange(actRange.location, diff)];  
        }       
    }      
    return @"";    
}

- (NSString *)afterString{
    
    NSRange selRange=[self selectedRange];
    if (selRange.location==0) {
        return [self string];
    }else if ((selRange.location+selRange.length)==[self string].length){
        return @"";
    }
    afterString=[[self string] substringFromIndex:[self selectedRange].location];
    if (!afterString) {
        afterString=@"";
    }
    return afterString;
}

- (NSString *)beforeString{
    NSRange selRange=[self selectedRange];
    if (selRange.location==[self string].length) {
        return [self string];
    }else if (selRange.location==0){
        return @"";
    }
    beforeString=[[self string] substringToIndex:selRange.location];
    if (!beforeString) {
        return @"";
    }
    return beforeString;
}

#pragma mark - ElasticThreads Lion Find... implementation
- (void)prepareTextFinder{        
#if MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_7
    if (IsLionOrLater) {
        
        
        [self setUsesFindBar:YES];
        
        [self setIncrementalSearchingEnabled:YES];
        textFinder=[[[NSTextFinder alloc]init]retain];
        [textFinder setClient:self];
        
        [textFinder setIncrementalSearchingEnabled:YES];
//        [textFinder setIncrementalSearchingShouldDimContentView:NO];
        NSNotificationCenter *dc=[NSNotificationCenter defaultCenter];
        [dc addObserver:self selector:@selector(textFinderShouldUpdateContext:) name:@"TextFindContextShouldUpdate" object:nil];
        [dc addObserver:self selector:@selector(textFinderShouldNoteChanges:) name:@"TextFindContextShouldNoteChanges" object:nil];
        [dc addObserver:self selector:@selector(textFinderShouldResetContext:) name:@"TextFindContextShouldReset" object:nil];
         [dc addObserver:self selector:@selector(hideTextFinderIfNecessary:) name:@"TextFinderShouldHide" object:nil];
        return;       
    }
#endif
    [self prepareTextFinderPreLion];
}

- (void)prepareTextFinderPreLion{
    [self setUsesFindPanel:YES];
    textFinder=[NSClassFromString(@"NSTextFinder")sharedTextFinder];
    [[textFinder findPanel:YES] setDelegate:self];
    NSArray *sViews = [[[textFinder findPanel:YES] contentView] subviews];
    for (id thing in sViews){
        if ([[thing className] isEqualToString:@"NSButton"]) {
            NSButton *aBut = thing;
            //            if (![aBut target]==nil) {
            [aBut setTarget:self];
            [aBut setAction:@selector(performFindPanelAction:)];
            //            }
        }
    }    
    [[textFinder findPanel:YES] update];
}


#if MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_7
- (void)textFinderShouldResetContext:(NSNotification *)aNotification{
    
    if (IsLionOrLater){
        [textFinder cancelFindIndicator];
        [textFinder noteClientStringWillChange];
    }
}

- (void)textFinderShouldNoteChanges:(NSNotification *)aNotification{
    if (IsLionOrLater){
        [textFinder noteClientStringWillChange];
    }
}

- (void)textFinderShouldUpdateContext:(NSNotification *)aNotification{
    
    if (IsLionOrLater){
        [textFinder setFindIndicatorNeedsUpdate:YES];
    }
}

- (void)hideTextFinderIfNecessary:(NSNotification *)aNotification{
    if (IsLionOrLater){        
        if([self textFinderIsVisible]){            
            [textFinder setFindIndicatorNeedsUpdate:YES];
            [textFinder cancelFindIndicator];
            [textFinder performAction:NSTextFinderActionHideFindInterface];
        }
    }
}
#endif

- (BOOL)textFinderIsVisible{
#if MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_7
    if ((IsLionOrLater)&&([[self enclosingScrollView]findBarView]!=nil)) {
        return [[[self enclosingScrollView] subviews]containsObject:[[self enclosingScrollView]findBarView]];
    }
#endif
    return NO;
}

- (IBAction)performFindPanelAction:(id)sender {
    id controller = [NSApp delegate];
    if(![controller setNoteIfNecessary])
        return;
    
    NSInteger findTag=[sender tag];
    
    [sender setTarget:self];
    if(!IsLionOrLater||([sender tag]!=7)){
        NSString *pbType;
        if (IsSnowLeopardOrLater) {
            pbType=NSPasteboardTypeString;
        }else{
            pbType=NSStringPboardType;
        }
        NSString *typedString = [controller typedString];
        if (!typedString) typedString = [controlField stringValue];
        if (!typedString||([typedString length]==0)) {
            typedString =[[NSPasteboard generalPasteboard]stringForType:pbType];
        }
         if (typedString&&([typedString length]>0)) {
             typedString = [typedString stringByReplacingOccurrencesOfString:@"\"" withString:@""];
             typedString=[typedString stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
             if ([typedString length] > 0 && ![lastImportedFindString isEqualToString:typedString]) {
                 
                 NSPasteboard *pasteboard = [NSPasteboard pasteboardWithName:NSFindPboard];
                 [pasteboard declareTypes:[NSArray arrayWithObject:pbType] owner:nil];
                 [pasteboard setString:typedString forType:pbType];
                 [lastImportedFindString release];
                 lastImportedFindString = [typedString retain];
             }
         }       
    
    }
    if ([[self window] firstResponder]!=self) {
        [[self window]makeFirstResponder:self];
    }
#if MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_7
    if (IsLionOrLater) {
        
        id newSender=[sender copy];
        if((findTag!=1)&&(findTag!=12)&&(findTag!=7)&&(![self textFinderIsVisible])){            
            [newSender setTag:NSTextFinderActionShowFindInterface];
            [super performTextFinderAction:newSender];
        } 
        if (findTag==1) {
            findTag=NSTextFinderActionShowFindInterface;
        }else if (findTag==2) {            
            findTag=NSTextFinderActionNextMatch;
        }else if (findTag==3) {
            findTag=NSTextFinderActionPreviousMatch;
        }else if (findTag==4) {
            findTag=NSTextFinderActionReplaceAll;
        }else if (findTag==5) {
            findTag=NSTextFinderActionReplace;
        }else if (findTag==6) {
            findTag=NSTextFinderActionReplaceAndFind;
        }else if (findTag==7) {
            findTag=(NSTextFinderActionSetSearchString);
        }else if (findTag==9) {
            findTag=NSTextFinderActionSelectAll;
        }else if (findTag==12) {
            findTag=NSTextFinderActionShowReplaceInterface;
        }//NSTextFinderActionSelectAll = 9,
        [newSender setTag:findTag];
        
        if ([textFinder validateAction:findTag]) {
            [super performTextFinderAction:newSender]; 
            if ((findTag==NSTextFinderActionSetSearchString)&&(![self textFinderIsVisible])) {
                [newSender setTag:NSTextFinderActionShowFindInterface];
                [super performTextFinderAction:newSender];
            }
            
//            [textFinder setFindIndicatorNeedsUpdate:YES];
        }else{
            NVLogError(@"find action was invalid");
        }
        [newSender release];
        return;
    }
#endif
    //not lion do it the old, hacky way
    if([sender tag]==1){
        if(lastImportedFindString&&(lastImportedFindString.length>0)&&([textFinder respondsToSelector:@selector(loadFindStringFromPasteboard)])){                
            if(![textFinder loadFindStringFromPasteboard]){
                [textFinder setFindString:lastImportedFindString writeToPasteboard:YES updateUI:YES];
            }
        }
//        else{
//            NSLog(@"Apple changed NSTextFinder (loadFindStringFromPasteboard)");
//        }	
    }
    [super performFindPanelAction:sender];    
}

- (IBAction)toggleLayoutOrientation:(id)sender {
  /*not ready yet. lots of display bugs. no horizontal scrollers... need to make SELF horizontally scrollable and switch to default scrollers or add ETTRANSPARENTHORIZONTAL BOYS. ALSO preference, binding, tag switching, etc.*/
    
//    [super changeLayoutOrientation:sender];
    
}

# pragma mark some markdown trickery methods

- (BOOL)clipboardHasLink{      
    NSPasteboard *pasteboard =  [NSPasteboard generalPasteboard]; 
    NSString *type = [pasteboard availableTypeFromArray: [NSArray arrayWithObjects: NSPasteboardTypeString,NSURLPboardType, nil]];
    if (type) {
        NSString *pString=[[pasteboard stringForType:type] stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];       
        NSURL *pUrl=[NSURL URLWithString:pString];
        if (pUrl) {
            NSString *urlString =[pUrl absoluteString];
            NSPredicate *urlMatch=[NSPredicate predicateWithFormat:@"SELF LIKE[cd] %@",@"http*://*.*"];
            if ([urlMatch evaluateWithObject:urlString]) {
                
                return YES;
            }
        }
    }
    return NO;
}

- (IBAction)pasteMarkdownLink:(id)sender{
    if (![self markdownCompletionsEnabled]) {
        [super paste:sender];
        return;
    }
    NSString *aftaString=[self.activeParagraphPastCursor stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];
    NSString *bifoString=[self.activeParagraphBeforeCursor stringByTrimmingCharactersInSet:[NSCharacterSet characterSetWithCharactersInString:@" https://"]];
    NSPredicate *bifoRefPred=[NSPredicate predicateWithFormat:@"SELF LIKE[cd] %@ OR SELF LIKE[cd] %@",@"*[*]",@"*[*]("];
    if((![bifoRefPred evaluateWithObject:bifoString])&&((![aftaString hasPrefix:@"]"])&&(![bifoString hasSuffix:@"["]))&&((![aftaString hasPrefix:@"\""])&&(![bifoString hasSuffix:@"\""]))&&((![aftaString hasPrefix:@">"])&&(![bifoString hasSuffix:@"<"]))&&((![aftaString hasPrefix:@"'"])&&(![bifoString hasSuffix:@"'"]))&&((![aftaString hasPrefix:@")"])&&(![bifoString hasSuffix:@"("]))){ 
        NSPasteboard *pasteboard =  [NSPasteboard generalPasteboard]; 
        NSString *type = [pasteboard availableTypeFromArray: [NSArray arrayWithObjects: NSPasteboardTypeString,NSURLPboardType, nil]];
        if (type) {
            NSString *pString=[[pasteboard stringForType:type] stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];       
            NSURL *pUrl=[NSURL URLWithString:pString];
            if (pUrl) {
                NSString *urlString =[pUrl absoluteString];
                // NSPredicate *urlMatch=[NSPredicate predicateWithFormat:@"SELF LIKE[cd] %@",@"http*://*.*"];
                //if ([urlMatch evaluateWithObject:urlString]) {
                NSString *selString=@"";
                NSRange selRange=[self selectedRange];
                if (selRange.length>0) {
                    selString=[[[self string]substringWithRange:selRange]stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];
                }
                NSString *paraString=[self.activeParagraph stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];
                if (([paraString isEqualToString:@""])||([paraString isEqualToString:selString])) {
                    urlString=[NSString stringWithFormat:@"[%@]: %@",selString,urlString];
                }else{
                    urlString=[NSString stringWithFormat:@"[%@](%@)",selString,urlString];            
                }
                [super insertText:urlString];
                selRange.location=[self selectedRange].location;
                selRange.location-=(urlString.length-1);
                [self setSelectedRange:selRange];
                return;
                // }
                //            else  if ([urlString hasPrefix:@"http"]) {
                //                NSLog(@"not match but has prefix:%@",urlString);
                //            }
            }
        }
    }
    [super paste:sender];
    
}


- (void)insertStringAtStartOfSelectedParagraphs:(NSString *)insertString{
    NSRange actRange=[self rangeOfActiveParagraph];
    NSRange selRange=[self selectedRange];
    if((actRange.location==NSNotFound)&&(selRange.length==0)){
        actRange=selRange;
    }    
    actRange.length=0;        
    if (actRange.location!=NSNotFound) {        
        NSString *actPar=[self activeParagraphTrimWS:NO];
        if (selRange.length==0) {
            if ((![actPar hasPrefix:insertString])&&(![actPar hasPrefix:@" "])) {
                insertString=[insertString stringByAppendingString:@" "];
            }
            if ([self shouldChangeTextInRange:actRange replacementString:insertString]) {
                [self replaceCharactersInRange:actRange withString:insertString];
                [self didChangeText]; 
            }
        }else{
            BOOL didIt=NO;
            NSArray *paragraphArray=[actPar componentsSeparatedByCharactersInSet:[NSCharacterSet newlineCharacterSet]];
            NSMutableCharacterSet *trimSet=[NSCharacterSet characterSetWithCharactersInString:insertString];            
            [trimSet formUnionWithCharacterSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
            NSString *replaceString;
            NSUInteger xtraLength=0;
            NSInteger i=0;
            NSUInteger charCt=0;
            for (NSString *thisPar in paragraphArray) {                
                if ([thisPar stringByTrimmingCharactersInSet:trimSet].length>0) {
                    replaceString=insertString;
                    if ((![thisPar hasPrefix:replaceString])&&(![thisPar hasPrefix:@" "])) {
                        replaceString=[replaceString stringByAppendingString:@" "];
                    }
                    NSRange thisRange=actRange;
                    if (i>0) {
                        thisRange.location+=charCt;
                    }else{
                        selRange.location+=replaceString.length;
                        selRange.length-=replaceString.length;
                    }
                    if ([self shouldChangeTextInRange:thisRange replacementString:replaceString]) {
                        didIt=YES;
                        [self replaceCharactersInRange:thisRange withString:replaceString];
                        charCt+=replaceString.length;
                        xtraLength+=replaceString.length;
                    }
                }                
                charCt++;
                charCt+=thisPar.length;
                i++;
            }
            if (didIt) {
                selRange.length+=xtraLength;
                [self setSelectedRange:selRange];
                [self didChangeText];    
            }
        }        
    }
}

- (void)removeStringAtStartOfSelectedParagraphs:(NSString *)removeString{
    NSRange actRange=[self rangeOfActiveParagraph];
    NSRange selRange=[self selectedRange];
    if((actRange.location==NSNotFound)&&(selRange.length==0)){
        actRange=selRange;
    }
    if (actRange.location!=NSNotFound){
        NSString *actPar=[self activeParagraphTrimWS:NO];
        if (selRange.length==0) {
            if (![actPar hasPrefix:removeString]) {
                removeString=@" ";
            }
            if ([actPar hasPrefix:removeString]) {
                if ([actPar hasPrefix:[removeString stringByAppendingString:@" "]]) {
                    actRange.length=2;
                }else{
                    actRange.length=1;
                }
                if ([self shouldChangeTextInRange:actRange replacementString:@""]) {
                    [self replaceCharactersInRange:actRange withString:@""];
                    [self didChangeText];
                }
            }
        }else{
            BOOL didIt=NO;
            NSArray *paragraphArray=[actPar componentsSeparatedByCharactersInSet:[NSCharacterSet newlineCharacterSet]];
            NSString *removerStr;
            NSUInteger xtraLength=0;
            NSInteger i=0;
            NSUInteger charCt=0;
            for (NSString *thisPar in paragraphArray) {
                if ([thisPar stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]].length>0) {
                    removerStr=removeString;
                    NSRange thisRange=actRange;
                    if (![thisPar hasPrefix:removerStr]) {
                        removerStr=@" ";
                    }
                    if ([thisPar hasPrefix:removerStr]) {                       
                        if ([thisPar hasPrefix:[removerStr stringByAppendingString:@" "]]) {
                            thisRange.length=2;
                        }else{
                            thisRange.length=1;
                        }
                        if (i>0) {                            
                            thisRange.location+=charCt;
                        }else{
                            selRange.location-=thisRange.length;
                            selRange.length+=thisRange.length;
                        }
                        
                        if ([self shouldChangeTextInRange:thisRange replacementString:@""]) {
                            didIt=YES;
                            [self replaceCharactersInRange:thisRange withString:@""];
                            xtraLength+=thisRange.length;                            
                            charCt-=thisRange.length;
                        }
                    }
                } 
                charCt++;
                charCt+=thisPar.length; 
                i++;
            }            
            if (didIt) {
                selRange.length-=xtraLength;
                [self setSelectedRange:selRange];
                [self didChangeText];
            } 
        }        
    }
}

#pragma mark Markdown writing

static BOOL NVWritingFence(NSString *core, NSString **outIndent, NSString **outMarker) {
	NSUInteger index = 0;
	NSUInteger length = [core length];
	while (index < length && ([core characterAtIndex:index] == ' ' || [core characterAtIndex:index] == '\t'))
		index++;
	if (length - index < 3)
		return NO;
	unichar fence = [core characterAtIndex:index];
	if (fence != '`' && fence != '~')
		return NO;
	NSUInteger start = index;
	while (index < length && [core characterAtIndex:index] == fence)
		index++;
	if (index - start < 3)
		return NO;
	NSUInteger markerEnd = index;
	while (index < length && ([core characterAtIndex:index] == ' ' || [core characterAtIndex:index] == '\t'))
		index++;
	while (index < length && [[NSCharacterSet alphanumericCharacterSet] characterIsMember:[core characterAtIndex:index]])
		index++;
	if (index != length)
		return NO;
	if (outIndent)
		*outIndent = [core substringToIndex:start];
	if (outMarker)
		*outMarker = [core substringWithRange:NSMakeRange(start, markerEnd - start)];
	return YES;
}

static BOOL NVBlockquoteParts(NSString *core, NSString **prefix, NSString **rest) {
	NSUInteger index = 0;
	NSUInteger length = [core length];
	BOOL sawQuote = NO;
	while (index < length) {
		unichar character = [core characterAtIndex:index];
		if (character != ' ' && character != '\t' && character != '>')
			break;
		if (character == '>')
			sawQuote = YES;
		index++;
	}
	if (!sawQuote)
		return NO;
	if (prefix)
		*prefix = [core substringToIndex:index];
	if (rest)
		*rest = [core substringFromIndex:index];
	return YES;
}

static NSString *NVCoreOfLine(NSString *line) {
	NSUInteger length = [line length];
	while (length && [[NSCharacterSet newlineCharacterSet] characterIsMember:[line characterAtIndex:length - 1]])
		length--;
	return [line substringToIndex:length];
}

- (void)setSelectedRange:(NSRange)charRange affinity:(NSSelectionAffinity)affinity stillSelecting:(BOOL)stillSelectingFlag {
	if (!markdownAdjustingSelection)
		markdownSelectionLevel = 0;
	[super setSelectedRange:charRange affinity:affinity stillSelecting:stillSelectingFlag];
}

- (void)highlightWritingGremlins {
	NSLayoutManager *layout = [self layoutManager];
	NSString *text = [self string];
	NSRange limit = [self activeMarkdownHighlightRange];
	if (!layout || ![text length] || !limit.length)
		return;
	NSColor *invisible = [NSColor colorWithCalibratedRed:0.82 green:0.22 blue:0.22 alpha:0.7];
	NSColor *space = [NSColor colorWithCalibratedRed:0.92 green:0.45 blue:0.28 alpha:0.35];
	NSUInteger index;
	for (index = limit.location; index < NSMaxRange(limit); index++) {
		unichar character = [text characterAtIndex:index];
		BOOL hidden = (character < 0x20 && character != '\t' && character != '\n' && character != '\r') || character == 0x7F || (character >= 0x80 && character <= 0x9F) || character == 0xAD || character == 0x200B || character == 0x200C || character == 0x200D || character == 0x2060 || character == 0xFEFF;
		BOOL unusualSpace = character == 0x00A0 || character == 0x202F || character == 0x205F || character == 0x3000 || (character >= 0x2000 && character <= 0x200A);
		if (!hidden && !unusualSpace)
			continue;
		[layout addTemporaryAttribute:NSBackgroundColorAttributeName value:(hidden ? invisible : space) forCharacterRange:NSMakeRange(index, 1)];
	}
}

- (BOOL)handleMarkdownCommandShortcut:(NSEvent *)event {
	NSUInteger flags = [event modifierFlags] & NSDeviceIndependentModifierFlagsMask;
	BOOL command = (flags & NSCommandKeyMask) != 0;
	BOOL option = (flags & NSAlternateKeyMask) != 0;
	BOOL control = (flags & NSControlKeyMask) != 0;
	BOOL shift = (flags & NSShiftKeyMask) != 0;
	NSString *chars = [event charactersIgnoringModifiers];
	unsigned short keyCode = [event keyCode];
	if (command && option && !control && !shift && ([chars isEqualToString:@"k"] || [chars isEqualToString:@"K"])) {
		[self insertMarkdownLink:nil];
		return YES;
	}
	if (command && control && !option && !shift && (keyCode == 123 || keyCode == 124)) {
		if (keyCode == 123)
			[self shiftLeftAction:nil];
		else
			[self shiftRightAction:nil];
		return YES;
	}
	if (command && control && !option && (keyCode == 126 || keyCode == 125)) {
		if (keyCode == 126)
			[self moveCurrentParagraphUp];
		else
			[self moveCurrentParagraphDown];
		return YES;
	}
	if (command && option && !control && (keyCode == 126 || keyCode == 125)) {
		if (keyCode == 126)
			[self expandMarkdownSelection];
		else
			[self shrinkMarkdownSelection];
		return YES;
	}
	return NO;
}

- (BOOL)handleMarkdownFunctionShortcut:(NSEvent *)event {
	NSUInteger flags = [event modifierFlags] & NSDeviceIndependentModifierFlagsMask;
	if ((flags & (NSCommandKeyMask | NSAlternateKeyMask | NSControlKeyMask)) != 0)
		return NO;
	unsigned short keyCode = [event keyCode];
	if (keyCode == 97)
		return [self insertOrJumpMagicReference];
	if (keyCode == 98)
		return [self insertOrJumpMagicFootnote];
	return NO;
}

- (IBAction)insertMarkdownLink:(id)sender {
	NSRange selection = [self selectedRange];
	NSString *text = [self string];
	if (selection.location > [text length])
		return;
	if (selection.length) {
		NSString *selected = [text substringWithRange:selection];
		NSString *replacement = [NSString stringWithFormat:@"[%@]()", selected];
		if (![self shouldChangeTextInRange:selection replacementString:replacement])
			return;
		[self replaceCharactersInRange:selection withString:replacement];
		[self setSelectedRange:NSMakeRange(selection.location + [replacement length] - 1, 0)];
	} else {
		if (![self shouldChangeTextInRange:NSMakeRange(selection.location, 0) replacementString:@"[]()"])
			return;
		[self replaceCharactersInRange:NSMakeRange(selection.location, 0) withString:@"[]()"];
		[self setSelectedRange:NSMakeRange(selection.location + 1, 0)];
	}
	[self didChangeText];
}

- (BOOL)insertMarkdownStructureNewline {
	NSString *text = [self string];
	/* Closing a fence counts every earlier fence. On a long note that walk is the return-key hang. */
	if (!NVMarkdownSyntaxHighlightEnabledForLength([text length]))
		return NO;
	NSRange selection = [self selectedRange];
	if (selection.location > [text length])
		return NO;
	NSRange lineRange = [text lineRangeForRange:NSMakeRange(selection.location, 0)];
	NSString *core = NVCoreOfLine([text substringWithRange:lineRange]);
	NSString *indent = nil;
	NSString *marker = nil;
	if (NVWritingFence(core, &indent, &marker)) {
		NSUInteger seen = 0;
		NSUInteger scan = 0;
		while (scan < lineRange.location && scan < [text length]) {
			NSRange previous = [text lineRangeForRange:NSMakeRange(scan, 0)];
			NSString *previousIndent = nil;
			NSString *previousMarker = nil;
			if (NVWritingFence(NVCoreOfLine([text substringWithRange:previous]), &previousIndent, &previousMarker) && [previousMarker isEqualToString:marker])
				seen++;
			if (NSMaxRange(previous) <= scan)
				break;
			scan = NSMaxRange(previous);
		}
		if (seen % 2 == 0) {
			NSString *insertion = [NSString stringWithFormat:@"\n%@\n%@%@", indent, indent, marker];
			if ([self shouldChangeTextInRange:selection replacementString:insertion]) {
				[self replaceCharactersInRange:selection withString:insertion];
				[self didChangeText];
				[self setSelectedRange:NSMakeRange(selection.location + 1 + [indent length], 0)];
				[self scrollRangeToVisible:[self selectedRange]];
			}
			return YES;
		}
	}
	NSString *prefix = nil;
	NSString *rest = nil;
	if (NVBlockquoteParts(core, &prefix, &rest)) {
		if (![[rest stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]] length]) {
			if ([self shouldChangeTextInRange:lineRange replacementString:@"\n"]) {
				[self replaceCharactersInRange:lineRange withString:@"\n"];
				[self didChangeText];
				[self setSelectedRange:NSMakeRange(lineRange.location, 0)];
			}
			return YES;
		}
		NSString *insertion = [@"\n" stringByAppendingString:prefix];
		if ([self shouldChangeTextInRange:selection replacementString:insertion]) {
			[self replaceCharactersInRange:selection withString:insertion];
			[self didChangeText];
			[self setSelectedRange:NSMakeRange(selection.location + [insertion length], 0)];
			[self scrollRangeToVisible:[self selectedRange]];
		}
		return YES;
	}
	return NO;
}

- (BOOL)adjustBlockquoteIndent:(BOOL)outdent {
	NSString *text = [self string];
	NSRange selection = [self selectedRange];
	if (![text length] || selection.location > [text length])
		return NO;
	NSUInteger endLocation = selection.length ? NSMaxRange(selection) - 1 : selection.location;
	if (endLocation >= [text length])
		endLocation = [text length] - 1;
	NSRange firstLine = [text lineRangeForRange:NSMakeRange(MIN(selection.location, [text length] - 1), 0)];
	NSRange lastLine = [text lineRangeForRange:NSMakeRange(endLocation, 0)];
	NSMutableArray *lines = [NSMutableArray array];
	NSUInteger scan = firstLine.location;
	BOOL found = NO;
	while (scan <= lastLine.location && scan < [text length]) {
		NSRange lineRange = [text lineRangeForRange:NSMakeRange(scan, 0)];
		if (NVBlockquoteParts(NVCoreOfLine([text substringWithRange:lineRange]), NULL, NULL))
			found = YES;
		[lines addObject:[NSValue valueWithRange:lineRange]];
		if (NSMaxRange(lineRange) <= scan)
			break;
		scan = NSMaxRange(lineRange);
	}
	if (!found)
		return NO;
	[[self undoManager] beginUndoGrouping];
	NSInteger lineIndex;
	for (lineIndex = (NSInteger)[lines count] - 1; lineIndex >= 0; lineIndex--) {
		NSRange lineRange = [[lines objectAtIndex:(NSUInteger)lineIndex] rangeValue];
		text = [self string];
		if (NSMaxRange(lineRange) > [text length])
			continue;
		NSString *prefix = nil;
		NSString *rest = nil;
		if (!NVBlockquoteParts(NVCoreOfLine([text substringWithRange:lineRange]), &prefix, &rest))
			continue;
		NSRange edited = NSMakeRange(NSNotFound, 0);
		NSString *change = nil;
		if (outdent) {
			NSRange quote = [prefix rangeOfString:@">" options:NSBackwardsSearch];
			if (quote.location == NSNotFound)
				continue;
			NSUInteger remove = 1;
			if (NSMaxRange(quote) < [prefix length] && [prefix characterAtIndex:NSMaxRange(quote)] == ' ')
				remove = 2;
			edited = NSMakeRange(lineRange.location + quote.location, remove);
			change = @"";
		} else {
			edited = NSMakeRange(lineRange.location + [prefix length], 0);
			change = @"> ";
		}
		if (![self shouldChangeTextInRange:edited replacementString:change])
			continue;
		[self replaceCharactersInRange:edited withString:change];
		NSInteger delta = (NSInteger)[change length] - (NSInteger)edited.length;
		if (NSMaxRange(edited) <= selection.location)
			selection.location += delta;
		else if (edited.location < NSMaxRange(selection)) {
			if (edited.location <= selection.location)
				selection.location = edited.location + [change length];
			NSInteger newLength = (NSInteger)selection.length + delta;
			selection.length = newLength > 0 ? (NSUInteger)newLength : 0;
		}
	}
	NSUInteger editedLength = [[self string] length];
	if (selection.location > editedLength)
		selection.location = editedLength;
	if (NSMaxRange(selection) > editedLength)
		selection.length = editedLength - selection.location;
	[self setSelectedRange:selection];
	[self didChangeText];
	[[self undoManager] setActionName:outdent ? NSLocalizedString(@"Outdent Quote", nil) : NSLocalizedString(@"Indent Quote", nil)];
	[[self undoManager] endUndoGrouping];
	return YES;
}

- (NSString *)singleURLOnPasteboard:(NSPasteboard *)pasteboard {
	NSString *string = [pasteboard stringForType:NSPasteboardTypeString];
	NSString *trimmed = [string stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
	if (![trimmed length] || [trimmed rangeOfCharacterFromSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]].location != NSNotFound)
		return nil;
	NSURL *url = [NSURL URLWithString:trimmed];
	if (!url || ![url scheme] || ![url host] && [[url scheme] caseInsensitiveCompare:@"mailto"] != NSOrderedSame)
		return nil;
	NSString *scheme = [[url scheme] lowercaseString];
	if ([scheme isEqualToString:@"http"] || [scheme isEqualToString:@"https"] || [scheme isEqualToString:@"mailto"] || [scheme isEqualToString:@"ftp"])
		return trimmed;
	return nil;
}

- (void)paste:(id)sender {
	if (![self markdownCompletionsEnabled]) {
		[super paste:sender];
		return;
	}
	NSString *urlString = [self singleURLOnPasteboard:[NSPasteboard generalPasteboard]];
	if (![urlString length]) {
		[super paste:sender];
		return;
	}
	NSString *text = [self string];
	NSRange selection = [self selectedRange];
	NSString *replacement = nil;
	if (selection.length && NSMaxRange(selection) <= [text length]) {
		replacement = [NSString stringWithFormat:@"[%@](%@)", [text substringWithRange:selection], urlString];
	} else {
		NSRange lineRange = [text lineRangeForRange:NSMakeRange(MIN(selection.location, [text length]), 0)];
		NSString *core = NVCoreOfLine([text substringWithRange:lineRange]);
		NSUInteger cursor = selection.location - lineRange.location;
		if (cursor > [core length])
			cursor = [core length];
		BOOL reference = NO;
		NSUInteger index = 0;
		while (index < [core length] && index < 3 && [core characterAtIndex:index] == ' ')
			index++;
		if (index < [core length] && [core characterAtIndex:index] == '[') {
			NSRange end = [core rangeOfString:@"]:" options:0 range:NSMakeRange(index, [core length] - index)];
			reference = end.location != NSNotFound;
		}
		BOOL insideParens = NO;
		if (cursor <= [core length]) {
			NSRange open = [core rangeOfString:@"(" options:NSBackwardsSearch range:NSMakeRange(0, cursor)];
			if (open.location != NSNotFound) {
				NSRange close = [core rangeOfString:@")" options:0 range:NSMakeRange(cursor, [core length] - cursor)];
				insideParens = close.location != NSNotFound;
			}
		}
		replacement = (reference || insideParens) ? urlString : [NSString stringWithFormat:@"<%@>", urlString];
	}
	if ([self shouldChangeTextInRange:selection replacementString:replacement]) {
		[self replaceCharactersInRange:selection withString:replacement];
		[self setSelectedRange:NSMakeRange(selection.location + [replacement length], 0)];
		[self didChangeText];
	}
}

- (BOOL)coreIsBlank:(NSString *)core {
	return [[core stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]] length] == 0;
}

- (NSUInteger)leadingIndentOfCore:(NSString *)core {
	return NVListIndentWidth(core, [core length], [self listTabWidth], NULL);
}

- (BOOL)coreIsListItem:(NSString *)core indent:(NSUInteger *)indent {
	NSUInteger markerIndex = 0;
	NSUInteger width = NVListIndentWidth(core, [core length], [self listTabWidth], &markerIndex);
	if (indent)
		*indent = width;
	return NVNumberedMarker(core, [core length], markerIndex, NULL, NULL) || NVBulletMarker(core, [core length], markerIndex);
}

- (NSRange)listBlockFromItemLine:(NSRange)itemLine inText:(NSString *)text indent:(NSUInteger)indent {
	NSUInteger end = NSMaxRange(itemLine);
	while (end < [text length]) {
		NSRange line = [text lineRangeForRange:NSMakeRange(end, 0)];
		NSString *core = NVCoreOfLine([text substringWithRange:line]);
		if ([self coreIsBlank:core]) {
			NSUInteger peek = NSMaxRange(line);
			BOOL keep = NO;
			while (peek < [text length]) {
				NSRange next = [text lineRangeForRange:NSMakeRange(peek, 0)];
				NSString *nextCore = NVCoreOfLine([text substringWithRange:next]);
				if ([self coreIsBlank:nextCore]) {
					peek = NSMaxRange(next);
					continue;
				}
				keep = [self leadingIndentOfCore:nextCore] > indent;
				break;
			}
			if (!keep)
				break;
		} else if ([self leadingIndentOfCore:core] <= indent) {
			break;
		}
		end = NSMaxRange(line);
	}
	return NSMakeRange(itemLine.location, end - itemLine.location);
}

- (NSRange)movableBlockAtLocation:(NSUInteger)location inText:(NSString *)text listItem:(BOOL *)isList indent:(NSUInteger *)outIndent {
	if (isList)
		*isList = NO;
	if (outIndent)
		*outIndent = 0;
	if (![text length])
		return NSMakeRange(0, 0);
	if (location > [text length])
		location = [text length];
	NSRange outer = NSMakeRange(NSNotFound, 0);
	if ([self markdownFenceAroundLocation:MIN(location, [text length] - 1) inner:NULL outer:&outer] && location >= outer.location && location <= NSMaxRange(outer))
		return outer;
	NSUInteger lineLocation = MIN(location, [text length] - 1);
	NSRange line = [text lineRangeForRange:NSMakeRange(lineLocation, 0)];
	NSString *core = NVCoreOfLine([text substringWithRange:line]);
	if ([self coreIsBlank:core])
		return line;
	NSUInteger indent = 0;
	if ([self coreIsListItem:core indent:&indent]) {
		if (isList)
			*isList = YES;
		if (outIndent)
			*outIndent = indent;
		return [self listBlockFromItemLine:line inText:text indent:indent];
	}
	NSUInteger myIndent = [self leadingIndentOfCore:core];
	NSRange scan = line;
	while (scan.location > 0) {
		NSRange previous = [text lineRangeForRange:NSMakeRange(scan.location - 1, 0)];
		NSString *previousCore = NVCoreOfLine([text substringWithRange:previous]);
		if ([self coreIsBlank:previousCore]) {
			scan = previous;
			continue;
		}
		NSUInteger previousIndent = 0;
		BOOL previousIsList = [self coreIsListItem:previousCore indent:&previousIndent];
		if (previousIsList && previousIndent < myIndent) {
			NSRange block = [self listBlockFromItemLine:previous inText:text indent:previousIndent];
			if (location >= block.location && location <= NSMaxRange(block)) {
				if (isList)
					*isList = YES;
				if (outIndent)
					*outIndent = previousIndent;
				return block;
			}
		}
		if (previousIndent < myIndent || previousIsList)
			break;
		scan = previous;
	}
	return line;
}

- (BOOL)blockAdjacentTo:(NSRange)block movingDown:(BOOL)movingDown isList:(BOOL)isList indent:(NSUInteger)indent inText:(NSString *)text block:(NSRange *)outBlock separator:(NSRange *)outSeparator {
	if (movingDown) {
		if (NSMaxRange(block) >= [text length])
			return NO;
		NSRange scan = [text lineRangeForRange:NSMakeRange(NSMaxRange(block), 0)];
		while ([self coreIsBlank:NVCoreOfLine([text substringWithRange:scan])]) {
			if (NSMaxRange(scan) >= [text length])
				return NO;
			scan = [text lineRangeForRange:NSMakeRange(NSMaxRange(scan), 0)];
		}
		NSRange fence = NSMakeRange(NSNotFound, 0);
		NSString *core = NVCoreOfLine([text substringWithRange:scan]);
		NSUInteger nextIndent = 0;
		BOOL nextIsList = [self coreIsListItem:core indent:&nextIndent];
		if (isList && nextIsList && nextIndent != indent)
			return NO;
		NSRange next = scan;
		if ([self markdownFenceAroundLocation:scan.location inner:NULL outer:&fence] && fence.location <= scan.location)
			next = fence;
		else if (nextIsList)
			next = [self listBlockFromItemLine:scan inText:text indent:nextIndent];
		if (next.location < NSMaxRange(block))
			return NO;
		*outBlock = next;
		*outSeparator = NSMakeRange(NSMaxRange(block), next.location - NSMaxRange(block));
		return YES;
	}
	if (block.location == 0)
		return NO;
	NSRange scan = [text lineRangeForRange:NSMakeRange(block.location - 1, 0)];
	while ([self coreIsBlank:NVCoreOfLine([text substringWithRange:scan])]) {
		if (scan.location == 0)
			return NO;
		scan = [text lineRangeForRange:NSMakeRange(scan.location - 1, 0)];
	}
	NSRange contentLine = scan;
	NSRange anchor = NSMakeRange(NSNotFound, 0);
	while (1) {
		NSString *core = NVCoreOfLine([text substringWithRange:scan]);
		if ([self coreIsBlank:core]) {
			if (scan.location == 0)
				return NO;
			scan = [text lineRangeForRange:NSMakeRange(scan.location - 1, 0)];
			continue;
		}
		NSUInteger lineIndent = 0;
		BOOL lineIsList = [self coreIsListItem:core indent:&lineIndent];
		NSUInteger boundary = isList ? indent : 0;
		if (isList && lineIsList && lineIndent == indent) {
			anchor = scan;
			break;
		}
		if (isList && lineIsList && lineIndent < indent)
			return NO;
		if (lineIndent > boundary) {
			if (scan.location == 0)
				return NO;
			scan = [text lineRangeForRange:NSMakeRange(scan.location - 1, 0)];
			continue;
		}
		anchor = scan;
		break;
	}
	if (anchor.location == NSNotFound)
		return NO;
	NSRange fence = NSMakeRange(NSNotFound, 0);
	NSString *anchorCore = NVCoreOfLine([text substringWithRange:anchor]);
	NSUInteger anchorIndent = 0;
	BOOL anchorIsList = [self coreIsListItem:anchorCore indent:&anchorIndent];
	NSRange previous = NSMakeRange(anchor.location, NSMaxRange(contentLine) - anchor.location);
	if ([self markdownFenceAroundLocation:anchor.location inner:NULL outer:&fence] && fence.location <= anchor.location && NSMaxRange(fence) <= NSMaxRange(contentLine))
		previous = fence;
	else if (anchorIsList)
		previous = [self listBlockFromItemLine:anchor inText:text indent:anchorIndent];
	if (NSMaxRange(previous) > block.location)
		return NO;
	*outBlock = previous;
	*outSeparator = NSMakeRange(NSMaxRange(previous), block.location - NSMaxRange(previous));
	return YES;
}

- (NSString *)stringByJoiningMovableParts:(NSArray *)parts {
	NSMutableString *result = [NSMutableString string];
	NSUInteger index;
	for (index = 0; index < [parts count]; index++) {
		NSString *part = [parts objectAtIndex:index];
		if (![part length])
			continue;
		if ([result length]) {
			unichar last = [result characterAtIndex:[result length] - 1];
			if (last != '\n' && last != '\r')
				[result appendString:@"\n"];
		}
		[result appendString:part];
	}
	return result;
}

- (void)moveCurrentBlockDown:(BOOL)movingDown {
	NSString *text = [self string];
	NSRange selection = [self selectedRange];
	if (![text length])
		return;
	NSUInteger location = MIN(selection.location, [text length]);
	BOOL isList = NO;
	NSUInteger indent = 0;
	NSRange block = [self movableBlockAtLocation:location inText:text listItem:&isList indent:&indent];
	NSRange other = NSMakeRange(NSNotFound, 0);
	NSRange separator = NSMakeRange(0, 0);
	if (![self blockAdjacentTo:block movingDown:movingDown isList:isList indent:indent inText:text block:&other separator:&separator])
		return;
	NSRange earlier = movingDown ? block : other;
	NSRange later = movingDown ? other : block;
	NSString *earlierText = [text substringWithRange:earlier];
	NSString *middleText = separator.length ? [text substringWithRange:separator] : @"";
	NSString *laterText = [text substringWithRange:later];
	NSString *currentText = [text substringWithRange:block];
	NSString *replacement = [self stringByJoiningMovableParts:[NSArray arrayWithObjects:laterText, middleText, earlierText, nil]];
	NSRange span = NSMakeRange(earlier.location, NSMaxRange(later) - earlier.location);
	NSUInteger destination = earlier.location;
	if (movingDown) {
		NSString *head = [self stringByJoiningMovableParts:[NSArray arrayWithObjects:laterText, middleText, nil]];
		destination += [head length];
		if ([head length] && [earlierText length]) {
			unichar last = [head characterAtIndex:[head length] - 1];
			if (last != '\n' && last != '\r')
				destination++;
		}
	}
	NSUInteger relative = 0;
	if (selection.location >= block.location)
		relative = selection.location - block.location;
	if (relative > [currentText length])
		relative = [currentText length];
	if (![self shouldChangeTextInRange:span replacementString:replacement])
		return;
	[[self undoManager] beginUndoGrouping];
	[self replaceCharactersInRange:span withString:replacement];
	NSUInteger caret = destination + relative;
	if (caret > [[self string] length])
		caret = [[self string] length];
	[self setSelectedRange:NSMakeRange(caret, 0)];
	[self scrollRangeToVisible:[self selectedRange]];
	[self didChangeText];
	NSString *name = nil;
	if (isList)
		name = movingDown ? NSLocalizedString(@"Move List Item Down", nil) : NSLocalizedString(@"Move List Item Up", nil);
	else
		name = movingDown ? NSLocalizedString(@"Move Paragraph Down", nil) : NSLocalizedString(@"Move Paragraph Up", nil);
	[[self undoManager] setActionName:name];
	[[self undoManager] endUndoGrouping];
}

- (void)moveCurrentParagraphUp {
	[self moveCurrentBlockDown:NO];
}

- (void)moveCurrentParagraphDown {
	[self moveCurrentBlockDown:YES];
}

- (NSRange)markdownWordAtLocation:(NSUInteger)location {
	NSString *text = [self string];
	if (![text length])
		return NSMakeRange(location, 0);
	if (location >= [text length])
		location = [text length] - 1;
	NSCharacterSet *breaks = [[NSCharacterSet alphanumericCharacterSet] invertedSet];
	NSUInteger start = location;
	while (start > 0 && ![breaks characterIsMember:[text characterAtIndex:start - 1]])
		start--;
	NSUInteger end = location;
	while (end < [text length] && ![breaks characterIsMember:[text characterAtIndex:end]])
		end++;
	return NSMakeRange(start, end - start);
}

- (NSRange)markdownSentenceAtLocation:(NSUInteger)location {
	NSString *text = [self string];
	if (![text length])
		return NSMakeRange(location, 0);
	if (location >= [text length])
		location = [text length] - 1;
	NSCharacterSet *stops = [NSCharacterSet characterSetWithCharactersInString:@".!?"];
	NSInteger start = (NSInteger)location;
	while (start > 0) {
		unichar character = [text characterAtIndex:(NSUInteger)start - 1];
		if (character == '\n')
			break;
		if ([stops characterIsMember:character]) {
			start++;
			while (start < (NSInteger)[text length]) {
				unichar next = [text characterAtIndex:(NSUInteger)start];
				if (next == ' ' || next == '\t' || next == '"' || next == '\'')
					start++;
				else
					break;
			}
			break;
		}
		start--;
	}
	NSUInteger end = location;
	while (end < [text length]) {
		unichar character = [text characterAtIndex:end];
		if (character == '\n')
			break;
		if ([stops characterIsMember:character]) {
			end++;
			while (end < [text length]) {
				unichar next = [text characterAtIndex:end];
				if (next == '"' || next == '\'' || next == ')')
					end++;
				else
					break;
			}
			break;
		}
		end++;
	}
	if (end <= (NSUInteger)start)
		return NSMakeRange(location, 0);
	return NSMakeRange((NSUInteger)start, end - (NSUInteger)start);
}

- (BOOL)markdownDelimiterAroundLocation:(NSUInteger)location inner:(NSRange *)inner outer:(NSRange *)outer {
	NSString *text = [self string];
	if (![text length] || location >= [text length])
		return NO;
	NSInteger index;
	for (index = (NSInteger)location; index >= 0; index--) {
		unichar character = [text characterAtIndex:(NSUInteger)index];
		unichar close = 0;
		if (character == '(') close = ')';
		else if (character == '[') close = ']';
		else if (character == '{') close = '}';
		else if (character == '"' || character == '\'') close = character;
		else continue;
		NSUInteger scan;
		for (scan = (NSUInteger)index + 1; scan < [text length]; scan++) {
			unichar next = [text characterAtIndex:scan];
			if (next == '\n')
				break;
			if (next == close) {
				if (location >= (NSUInteger)index && location <= scan) {
					if (inner)
						*inner = NSMakeRange((NSUInteger)index + 1, scan - (NSUInteger)index - 1);
					if (outer)
						*outer = NSMakeRange((NSUInteger)index, scan - (NSUInteger)index + 1);
					return YES;
				}
				break;
			}
		}
	}
	return NO;
}

- (BOOL)markdownFenceAroundLocation:(NSUInteger)location inner:(NSRange *)inner outer:(NSRange *)outer {
	NSString *text = [self string];
	if (![text length] || location > [text length])
		return NO;
	NSUInteger scan = 0;
	NSRange open = NSMakeRange(NSNotFound, 0);
	NSString *marker = nil;
	while (scan < [text length]) {
		NSRange line = [text lineRangeForRange:NSMakeRange(scan, 0)];
		NSString *found = nil;
		if (NVWritingFence(NVCoreOfLine([text substringWithRange:line]), NULL, &found)) {
			if (!marker) {
				marker = found;
				open = line;
			} else if ([found isEqualToString:marker]) {
				if (location >= open.location && location <= NSMaxRange(line)) {
					if (outer)
						*outer = NSMakeRange(open.location, NSMaxRange(line) - open.location);
					if (inner) {
						NSUInteger start = NSMaxRange(open);
						NSUInteger end = line.location;
						*inner = end > start ? NSMakeRange(start, end - start) : NSMakeRange(start, 0);
					}
					return YES;
				}
				marker = nil;
				open = NSMakeRange(NSNotFound, 0);
			}
		}
		if (NSMaxRange(line) <= scan)
			break;
		scan = NSMaxRange(line);
	}
	if (marker && location >= open.location) {
		if (outer)
			*outer = NSMakeRange(open.location, [text length] - open.location);
		if (inner) {
			NSUInteger start = NSMaxRange(open);
			*inner = start <= [text length] ? NSMakeRange(start, [text length] - start) : NSMakeRange(location, 0);
		}
		return YES;
	}
	return NO;
}

- (NSRange)markdownSelectionForLevel:(NSInteger)level atLocation:(NSUInteger)location {
	NSString *text = [self string];
	if (![text length])
		return NSMakeRange(location, 0);
	if (location > [text length])
		location = [text length];
	if (level == 1)
		return [self markdownWordAtLocation:location];
	if (level == 2 || level == 3) {
		NSRange inner = NSMakeRange(NSNotFound, 0);
		NSRange outer = NSMakeRange(NSNotFound, 0);
		if ([self markdownDelimiterAroundLocation:location inner:&inner outer:&outer])
			return level == 2 ? inner : outer;
		if ([self markdownFenceAroundLocation:location inner:&inner outer:&outer])
			return level == 2 ? inner : outer;
		return NSMakeRange(location, 0);
	}
	if (level == 4)
		return [self markdownSentenceAtLocation:location];
	if (level == 5)
		return [text paragraphRangeForRange:NSMakeRange(location, 0)];
	if (level == 6) {
		NSRange line = [text lineRangeForRange:NSMakeRange(location, 0)];
		NSCharacterSet *ink = [[NSCharacterSet whitespaceAndNewlineCharacterSet] invertedSet];
		if ([NVCoreOfLine([text substringWithRange:line]) rangeOfCharacterFromSet:ink].location == NSNotFound)
			return NSMakeRange(location, 0);
		NSUInteger start = line.location;
		NSUInteger end = NSMaxRange(line);
		while (start > 0) {
			NSRange previous = [text lineRangeForRange:NSMakeRange(start - 1, 0)];
			if ([NVCoreOfLine([text substringWithRange:previous]) rangeOfCharacterFromSet:ink].location == NSNotFound)
				break;
			start = previous.location;
		}
		while (end < [text length]) {
			NSRange next = [text lineRangeForRange:NSMakeRange(end, 0)];
			if ([NVCoreOfLine([text substringWithRange:next]) rangeOfCharacterFromSet:ink].location == NSNotFound)
				break;
			end = NSMaxRange(next);
		}
		return NSMakeRange(start, end - start);
	}
	if (level == 7) {
		NSRange inner = NSMakeRange(NSNotFound, 0);
		NSRange outer = NSMakeRange(NSNotFound, 0);
		if ([self markdownFenceAroundLocation:location inner:&inner outer:&outer] && outer.length)
			return outer;
		NSRange line = [text lineRangeForRange:NSMakeRange(location, 0)];
		NSString *core = NVCoreOfLine([text substringWithRange:line]);
		BOOL quote = NVBlockquoteParts(core, NULL, NULL);
		NSUInteger tabWidth = [self listTabWidth];
		NSUInteger markerIndex = 0;
		NVListIndentWidth(core, [core length], tabWidth, &markerIndex);
		BOOL list = NVNumberedMarker(core, [core length], markerIndex, NULL, NULL) || NVBulletMarker(core, [core length], markerIndex);
		if (!quote && !list)
			return [text paragraphRangeForRange:NSMakeRange(location, 0)];
		NSUInteger start = line.location;
		NSUInteger end = NSMaxRange(line);
		while (start > 0) {
			NSRange previous = [text lineRangeForRange:NSMakeRange(start - 1, 0)];
			NSString *previousCore = NVCoreOfLine([text substringWithRange:previous]);
			BOOL same = quote ? NVBlockquoteParts(previousCore, NULL, NULL) : NO;
			if (!quote) {
				NSUInteger previousMarker = 0;
				NVListIndentWidth(previousCore, [previousCore length], tabWidth, &previousMarker);
				same = NVNumberedMarker(previousCore, [previousCore length], previousMarker, NULL, NULL) || NVBulletMarker(previousCore, [previousCore length], previousMarker);
			}
			if (!same)
				break;
			start = previous.location;
		}
		while (end < [text length]) {
			NSRange next = [text lineRangeForRange:NSMakeRange(end, 0)];
			NSString *nextCore = NVCoreOfLine([text substringWithRange:next]);
			BOOL same = quote ? NVBlockquoteParts(nextCore, NULL, NULL) : NO;
			if (!quote) {
				NSUInteger nextMarker = 0;
				NVListIndentWidth(nextCore, [nextCore length], tabWidth, &nextMarker);
				same = NVNumberedMarker(nextCore, [nextCore length], nextMarker, NULL, NULL) || NVBulletMarker(nextCore, [nextCore length], nextMarker);
			}
			if (!same)
				break;
			end = NSMaxRange(next);
		}
		return NSMakeRange(start, end - start);
	}
	if (level >= 8)
		return NSMakeRange(0, [text length]);
	return NSMakeRange(location, 0);
}

- (void)expandMarkdownSelection {
	NSString *text = [self string];
	NSRange selection = [self selectedRange];
	if (markdownSelectionLevel <= 0 || selection.length == 0) {
		markdownSelectionAnchor = MIN(selection.location, [text length]);
		markdownSelectionLevel = 0;
	}
	NSRange current = selection;
	NSInteger level;
	for (level = markdownSelectionLevel + 1; level <= 8; level++) {
		NSRange range = [self markdownSelectionForLevel:level atLocation:markdownSelectionAnchor];
		if (!range.length || NSEqualRanges(range, current))
			continue;
		markdownSelectionLevel = level;
		markdownAdjustingSelection = YES;
		[self setSelectedRange:range];
		markdownAdjustingSelection = NO;
		[self scrollRangeToVisible:range];
		return;
	}
}

- (void)shrinkMarkdownSelection {
	if (markdownSelectionLevel <= 0)
		return;
	if (markdownSelectionLevel == 1) {
		markdownAdjustingSelection = YES;
		[self setSelectedRange:NSMakeRange(MIN(markdownSelectionAnchor, [[self string] length]), 0)];
		markdownAdjustingSelection = NO;
		markdownSelectionLevel = 0;
		return;
	}
	NSInteger level;
	for (level = markdownSelectionLevel - 1; level >= 1; level--) {
		NSRange range = [self markdownSelectionForLevel:level atLocation:markdownSelectionAnchor];
		if (!range.length)
			continue;
		markdownSelectionLevel = level;
		markdownAdjustingSelection = YES;
		[self setSelectedRange:range];
		markdownAdjustingSelection = NO;
		[self scrollRangeToVisible:range];
		return;
	}
}

- (BOOL)writingLocationInsideFence:(NSUInteger)location {
	NSRange inner = NSMakeRange(NSNotFound, 0);
	NSRange outer = NSMakeRange(NSNotFound, 0);
	if (![self markdownFenceAroundLocation:location inner:&inner outer:&outer])
		return NO;
	return location > outer.location && location < NSMaxRange(outer) && location >= inner.location && location <= NSMaxRange(inner);
}

static NSString *NVRegexLiteral(NSString *text) {
	NSCharacterSet *specials = [NSCharacterSet characterSetWithCharactersInString:@"\\.^$|?*+()[]{}"];
	NSMutableString *escaped = [NSMutableString stringWithCapacity:[text length]];
	NSUInteger index;
	for (index = 0; index < [text length]; index++) {
		unichar character = [text characterAtIndex:index];
		if ([specials characterIsMember:character])
			[escaped appendString:@"\\"];
		[escaped appendFormat:@"%C", character];
	}
	return escaped;
}

- (NSUInteger)highestMagicNumberForFootnote:(BOOL)footnote {
	NSString *text = [self string];
	NSRegularExpression *regex = [NSRegularExpression regularExpressionWithPattern:(footnote ? @"\\[\\^(\\d+)\\]" : @"(?<!\\^)\\[(\\d+)\\]") options:0 error:nil];
	__block NSUInteger highest = 0;
	[regex enumerateMatchesInString:text options:0 range:NSMakeRange(0, [text length]) usingBlock:^(NSTextCheckingResult *result, NSMatchingFlags flags, BOOL *stop) {
		NSRange numberRange = [result rangeAtIndex:1];
		if (numberRange.location == NSNotFound)
			return;
		NSUInteger value = (NSUInteger)[[text substringWithRange:numberRange] integerValue];
		if (value > highest)
			highest = value;
	}];
	return highest;
}

- (void)revealSelectedRange {
	NSString *text = [self string];
	NSRange selection = [self selectedRange];
	if (![text length] || selection.location > [text length]) {
		[self scrollRangeToVisible:selection];
		return;
	}
	NSUInteger index = selection.location == [text length] ? [text length] - 1 : selection.location;
	NSRange line = [text lineRangeForRange:NSMakeRange(index, 0)];
	NSLayoutManager *layout = [self layoutManager];
	NSTextContainer *container = [self textContainer];
	if (!layout || !container || !line.length) {
		[self scrollRangeToVisible:line.length ? line : selection];
		return;
	}
	[layout ensureLayoutForCharacterRange:line];
	NSRange glyphRange = [layout glyphRangeForCharacterRange:line actualCharacterRange:NULL];
	if (!glyphRange.length) {
		[self scrollRangeToVisible:line];
		return;
	}
	NSRect rect = [layout boundingRectForGlyphRange:glyphRange inTextContainer:container];
	NSPoint origin = [self textContainerOrigin];
	rect.origin.x += origin.x;
	rect.origin.y += origin.y;
	rect = NSInsetRect(rect, 0.0, -40.0);
	[self scrollRectToVisible:rect];
}

- (void)appendMagicDefinition:(NSString *)definition {
	NSString *text = [self string];
	NSString *gap = @"";
	if ([text length] && ![text hasSuffix:@"\n\n"])
		gap = [text hasSuffix:@"\n"] ? @"\n" : @"\n\n";
	NSString *insertion = [gap stringByAppendingString:definition];
	NSUInteger location = [text length];
	if ([self shouldChangeTextInRange:NSMakeRange(location, 0) replacementString:insertion]) {
		[self replaceCharactersInRange:NSMakeRange(location, 0) withString:insertion];
		[self didChangeText];
		[self setSelectedRange:NSMakeRange(location + [insertion length], 0)];
		[self revealSelectedRange];
	}
}

- (BOOL)insertOrJumpMagicReference {
	NSString *text = [self string];
	NSRange selection = [self selectedRange];
	NSUInteger probe = MIN(selection.location, [text length]);
	if ([self writingLocationInsideFence:probe])
		return YES;
	NSRange lineRange = [text lineRangeForRange:NSMakeRange(probe, 0)];
	NSString *line = [text substringWithRange:lineRange];
	NSRegularExpression *definition = [NSRegularExpression regularExpressionWithPattern:@"^\\s*\\[([^\\]]+)\\]:" options:0 error:nil];
	NSTextCheckingResult *definitionMatch = [definition firstMatchInString:line options:0 range:NSMakeRange(0, [line length])];
	NSRegularExpression *use = [NSRegularExpression regularExpressionWithPattern:@"\\[[^\\]\\n]*\\]\\[([^\\]\\n]+)\\]|\\[([^\\]\\n]+)\\](?![(\\[])" options:0 error:nil];
	if (definitionMatch) {
		NSString *identifier = NVRegexLiteral([line substringWithRange:[definitionMatch rangeAtIndex:1]]);
		NSString *pattern = [NSString stringWithFormat:@"\\[[^\\]\\n]*\\]\\[%@\\]|\\[%@\\](?![(\\[])", identifier, identifier];
		NSRegularExpression *finder = [NSRegularExpression regularExpressionWithPattern:pattern options:0 error:nil];
		__block NSRange found = NSMakeRange(NSNotFound, 0);
		[finder enumerateMatchesInString:text options:0 range:NSMakeRange(0, [text length]) usingBlock:^(NSTextCheckingResult *result, NSMatchingFlags flags, BOOL *stop) {
			(void)flags;
			if (NSIntersectionRange([text lineRangeForRange:result.range], lineRange).length)
				return;
			found = result.range;
			*stop = YES;
		}];
		if (found.location != NSNotFound) {
			[self setSelectedRange:NSMakeRange(NSMaxRange(found), 0)];
			[self revealSelectedRange];
		}
		return YES;
	}
	for (NSTextCheckingResult *match in [use matchesInString:line options:0 range:NSMakeRange(0, [line length])]) {
		NSRange token = NSMakeRange(lineRange.location + match.range.location, match.range.length);
		if (probe < token.location || probe > NSMaxRange(token))
			continue;
		NSRange idRange = [match rangeAtIndex:1].location != NSNotFound ? [match rangeAtIndex:1] : [match rangeAtIndex:2];
		NSString *rawIdentifier = [line substringWithRange:idRange];
		NSString *pattern = [NSString stringWithFormat:@"^\\s*\\[%@\\]:\\s*", NVRegexLiteral(rawIdentifier)];
		NSRegularExpression *finder = [NSRegularExpression regularExpressionWithPattern:pattern options:NSRegularExpressionAnchorsMatchLines error:nil];
		NSTextCheckingResult *found = [finder firstMatchInString:text options:0 range:NSMakeRange(0, [text length])];
		if (found)
			[self setSelectedRange:NSMakeRange(NSMaxRange(found.range), 0)];
		else
			[self appendMagicDefinition:[NSString stringWithFormat:@"[%@]: ", rawIdentifier]];
		[self revealSelectedRange];
		return YES;
	}
	if (!selection.length)
		return YES;
	NSUInteger number = [self highestMagicNumberForFootnote:NO] + 1;
	NSString *selected = [text substringWithRange:selection];
	NSString *marker = [NSString stringWithFormat:@"[%@][%lu]", selected, (unsigned long)number];
	if (![self shouldChangeTextInRange:selection replacementString:marker])
		return YES;
	[self replaceCharactersInRange:selection withString:marker];
	[self didChangeText];
	[self appendMagicDefinition:[NSString stringWithFormat:@"[%lu]: ", (unsigned long)number]];
	return YES;
}

- (BOOL)insertOrJumpMagicFootnote {
	NSString *text = [self string];
	NSRange selection = [self selectedRange];
	NSUInteger probe = MIN(selection.location, [text length]);
	if ([self writingLocationInsideFence:probe])
		return YES;
	NSRange lineRange = [text lineRangeForRange:NSMakeRange(probe, 0)];
	NSString *line = [text substringWithRange:lineRange];
	NSRegularExpression *definition = [NSRegularExpression regularExpressionWithPattern:@"^\\s*\\[\\^(\\d+)\\]:" options:0 error:nil];
	NSTextCheckingResult *definitionMatch = [definition firstMatchInString:line options:0 range:NSMakeRange(0, [line length])];
	if (definitionMatch) {
		NSString *number = [line substringWithRange:[definitionMatch rangeAtIndex:1]];
		NSRegularExpression *finder = [NSRegularExpression regularExpressionWithPattern:[NSString stringWithFormat:@"\\[\\^%@\\]", number] options:0 error:nil];
		NSRegularExpression *definitionLine = [NSRegularExpression regularExpressionWithPattern:[NSString stringWithFormat:@"^\\s*\\[\\^%@\\]:", number] options:0 error:nil];
		__block NSRange found = NSMakeRange(NSNotFound, 0);
		[finder enumerateMatchesInString:text options:0 range:NSMakeRange(0, [text length]) usingBlock:^(NSTextCheckingResult *result, NSMatchingFlags flags, BOOL *stop) {
			(void)flags;
			NSRange matchLine = [text lineRangeForRange:result.range];
			if ([definitionLine firstMatchInString:text options:0 range:matchLine])
				return;
			found = result.range;
			*stop = YES;
		}];
		if (found.location != NSNotFound) {
			[self setSelectedRange:NSMakeRange(NSMaxRange(found), 0)];
			[self revealSelectedRange];
		}
		return YES;
	}
	NSRegularExpression *inlineNote = [NSRegularExpression regularExpressionWithPattern:@"\\[\\^(\\d+)\\]" options:0 error:nil];
	for (NSTextCheckingResult *match in [inlineNote matchesInString:line options:0 range:NSMakeRange(0, [line length])]) {
		NSRange token = NSMakeRange(lineRange.location + match.range.location, match.range.length);
		if (probe < token.location || probe > NSMaxRange(token))
			continue;
		NSString *number = [line substringWithRange:[match rangeAtIndex:1]];
		NSRegularExpression *finder = [NSRegularExpression regularExpressionWithPattern:[NSString stringWithFormat:@"^\\s*\\[\\^%@\\]:\\s*", number] options:NSRegularExpressionAnchorsMatchLines error:nil];
		NSTextCheckingResult *found = [finder firstMatchInString:text options:0 range:NSMakeRange(0, [text length])];
		if (found)
			[self setSelectedRange:NSMakeRange(NSMaxRange(found.range), 0)];
		else
			[self appendMagicDefinition:[NSString stringWithFormat:@"[^%@]: ", number]];
		[self revealSelectedRange];
		return YES;
	}
	NSUInteger number = [self highestMagicNumberForFootnote:YES] + 1;
	NSString *body = selection.length ? [text substringWithRange:selection] : @"";
	NSString *marker = [NSString stringWithFormat:@"[^%lu]", (unsigned long)number];
	NSRange insert = selection.length ? selection : NSMakeRange(probe, 0);
	if (!selection.length) {
		NSRange word = [self markdownWordAtLocation:probe];
		if (word.length && probe > word.location && probe < NSMaxRange(word))
			insert.location = NSMaxRange(word);
	}
	if (![self shouldChangeTextInRange:insert replacementString:marker])
		return YES;
	[self replaceCharactersInRange:insert withString:marker];
	[self didChangeText];
	[self appendMagicDefinition:[NSString stringWithFormat:@"[^%lu]: %@", (unsigned long)number, body]];
	return YES;
}

@end
