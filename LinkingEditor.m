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
#import "GlobalPrefs.h"
#import "AppController.h"
#import "AppController_Importing.h"
#import "NotesTableView.h"
#import "LinkingEditor_Indentation.h"
#import "NSCollection_utils.h"
#import "AttributedPlainText.h"
#import "NSString_NV.h"
#import "NVPasswordGenerator.h"
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
	
    prefsController = [GlobalPrefs defaultPrefs];

    [prefsController registerWithTarget:self forChangesInSettings:
	 @selector(setNoteBodyFont:sender:),
	 @selector(setMakeURLsClickable:sender:),
	 @selector(setSearchTermHighlightColor:sender:),
	 @selector(setShouldHighlightSearchTerms:sender:), nil];
	
    self.managesTextWidth=[prefsController managesTextWidthInWindow];
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
		
		if (![prefsController highlightSearchTerms]) {
			[self removeHighlightedTerms];
		} else {
			NSString *typedString = [(AppController *)[NSApp delegate] typedString];
			if (typedString)
				[self highlightTermsTemporarilyReturningFirstRange:typedString avoidHighlight:NO];
		}
	}
}

- (BOOL)becomeFirstResponder {
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
	NSColor *fgColor = [(AppController *)[NSApp delegate] foregrndColor];
	NSColor *bgColor = [self backgroundColor];
    if (bgColor!=[(AppController *)[NSApp delegate]backgrndColor]) {
        bgColor=[(AppController *)[NSApp delegate]backgrndColor];
        [self setBackgroundColor:bgColor];
    }
	[[self enclosingScrollView] setBackgroundColor:bgColor];
    if (IsLionOrLater&&[self textFinderIsVisible]) {
        [[self window]invalidateCursorRectsForView:[[self enclosingScrollView]findBarView]];
    }
    
	//[self setBackgroundColor:bgColor];
	//[nvTextScroller setBackgroundColor:bgColor];
	//[[self enclosingScrollView] setNeedsDisplay:YES];
    
	[self setInsertionPointColor:[self _insertionPointColorForForegroundColor:fgColor backgroundColor:bgColor]];
	[self setLinkTextAttributes:[self preferredLinkAttributes]];
	[self setSelectedTextAttributes:[NSDictionary dictionaryWithObject:[self _selectionColorForForegroundColor:fgColor backgroundColor:bgColor] 
																forKey:NSBackgroundColorAttributeName]];
	[self setTypingAttributes:[prefsController noteBodyAttributes]];
    [[self enclosingScrollView]setNeedsDisplay:YES];
//    [[[self enclosingScrollView]contentView]setNeedsDisplay:YES];
    [self setNeedsDisplay:YES];
	[self scheduleMarkdownHighlight];
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
	
	return [NSDictionary dictionaryWithObjectsAndKeys:
			[NSCursor pointingHandCursor], NSCursorAttributeName,
			[NSNumber numberWithInt:NSUnderlineStyleSingle], NSUnderlineStyleAttributeName,
			[self _linkColorForForegroundColor:[(AppController *)[NSApp delegate] foregrndColor] backgroundColor:[(AppController *)[NSApp delegate] backgrndColor]],
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

- (void)layoutManager:(NSLayoutManager *)aLayoutManager didCompleteLayoutForTextContainer:(NSTextContainer *)aTextContainer atEnd:(BOOL)flag {
	didRenderFully = YES;
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
			NSLog(@"false alarm: no real styles");
#endif
			
		} else {
#if COPY_PASTE_DEBUG
			NSLog(@"homogeneous style");
#endif
		}
		
		if (0) {
copyRTFType:
			//we have more than a single styling segment within the selection--grudgingly allow regular RTF copying
#if COPY_PASTE_DEBUG
			NSLog(@"copying RTF due to multiple attributes");
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
        [self changeMarkdownAttribute:@"**"];
    }else{
        [self applyStyleOfTrait:NSBoldFontMask alternateAttributeName:NSStrokeWidthAttributeName 
        alternateAttributeValue:[NSNumber numberWithFloat:(float)STROKE_WIDTH_FOR_BOLD]];
        [[self undoManager] setActionName:NSLocalizedString(@"Bold",nil)];
	}
}

- (void)italic:(id)sender {
    if ([[NSUserDefaults standardUserDefaults]boolForKey:@"UsesMarkdownCompletions"]) {
        [self changeMarkdownAttribute:@"*"];
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


//use with rangesOfWordsInString:(NSString*)findString earliestRange:(NSRange*)aRange inRange:
- (void)highlightRangesTemporarily:(CFArrayRef)ranges {
	CFIndex rangeIndex;
	long bodyLength = (long)[[self string] length];
	NSDictionary *highlightDict = [prefsController searchTermHighlightAttributes];
	
	for (rangeIndex = 0; rangeIndex < CFArrayGetCount(ranges); rangeIndex++) {
		CFRange *range = (CFRange *)CFArrayGetValueAtIndex(ranges, rangeIndex);
		
		if (range && range->length > 0 && range->location + range->length <= bodyLength) {
			[[self layoutManager] addTemporaryAttributes:highlightDict forCharacterRange:*(NSRange*)range];
		} else {
			NSLog(@"highlightRangesTemporarily: Invalid range (%@)", range ? NSStringFromRange(*(NSRange*)range) : @"null");
		}
	}
}

- (NSRange)highlightTermsTemporarilyReturningFirstRange:(NSString*)typedString avoidHighlight:(BOOL)noHighlight {
	
	//if lengths of respective UTF8-string equivalents for contentString are the same, we should revert to cstring-based algorithm
	
	CFStringRef quoteStr = CFSTR("\"");
	NSRange firstRange = NSMakeRange(NSNotFound,0);
	CFRange quoteRange = CFStringFind((CFStringRef)typedString, quoteStr, 0);
	CFArrayRef terms = CFStringCreateArrayBySeparatingStrings(NULL, (CFStringRef)typedString, 
															  quoteRange.location == kCFNotFound ? CFSTR(" ") : quoteStr);
	if (terms) {
		CFIndex termIndex, rangeIndex;
		CFStringRef bodyString = (CFStringRef)[self string];
		NSDictionary *highlightDict = [prefsController searchTermHighlightAttributes];
		
		for (termIndex = 0; termIndex < CFArrayGetCount(terms); termIndex++) {
			CFStringRef term = CFArrayGetValueAtIndex(terms, termIndex);
			if (CFStringGetLength(term) > 0) {
				CFArrayRef ranges = CFStringCreateArrayWithFindResults(NULL, bodyString, term, CFRangeMake(0, CFStringGetLength(bodyString)),
																	   kCFCompareCaseInsensitive);
				if (!ranges)
					continue;
				for (rangeIndex = 0; rangeIndex < CFArrayGetCount(ranges); rangeIndex++) {
					CFRange *range = (CFRange *)CFArrayGetValueAtIndex(ranges, rangeIndex);
					
					if (range && range->length > 0 && range->location + range->length <= CFStringGetLength(bodyString)) {
						if (firstRange.location > (NSUInteger)range->location) {
							firstRange = *(NSRange*)range;
							if (noHighlight) {
								CFRelease(ranges);
								goto returnEarly;
							}
						}
						[[self layoutManager] addTemporaryAttributes:highlightDict forCharacterRange:*(NSRange*)range];
					} else {
						NSLog(@"highlightTermsTemporarily: Invalid range (%@)", range ? NSStringFromRange(*(NSRange*)range) : @"?");
					}
				}
				CFRelease(ranges);
			}
		}
	returnEarly:
		CFRelease(terms);
	}
	return (firstRange);
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

static NSRegularExpression *NVMarkdownRegex(NSString *pattern) {
	NSError *error = nil;
	NSRegularExpression *regex = [NSRegularExpression regularExpressionWithPattern:pattern options:NSRegularExpressionAnchorsMatchLines error:&error];
	if (!regex)
		NSLog(@"nvALT markdown highlight: %@", error);
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

- (void)noteTextStorageDidProcessEditing:(NSNotification *)note {
	if (![NSThread isMainThread]) {
		[self performSelectorOnMainThread:@selector(scheduleMarkdownHighlight) withObject:nil waitUntilDone:NO];
		return;
	}
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
	[self scheduleMarkdownHighlight];
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
	[regex enumerateMatchesInString:string options:0 range:NSMakeRange(0, [string length]) usingBlock:^(NSTextCheckingResult *result, NSMatchingFlags flags, BOOL *stop) {
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
	NSUInteger length = [string length];
	NSUInteger index = 0;
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

- (void)highlightMarkdownSyntax {
	NSLayoutManager *layout = [self layoutManager];
	NSString *string = [self string];
	NSRange fullRange = NSMakeRange(0, [string length]);
	if (layout && fullRange.length) {
		[layout removeTemporaryAttribute:NSForegroundColorAttributeName forCharacterRange:fullRange];
		[layout removeTemporaryAttribute:NSStrikethroughStyleAttributeName forCharacterRange:fullRange];
	}
	if (![self markdownCompletionsEnabled] || !layout || !fullRange.length || !prefsController) {
		[self highlightWritingGremlins];
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
	[self paintMarkdownMatches:listRegex string:string content:marker marker:marker contentGroup:0 delimiterGroup:0 hasCloser:NO strikethrough:NO];
	[self paintMarkdownMatches:ruleRegex string:string content:marker marker:marker contentGroup:0 delimiterGroup:0 hasCloser:NO strikethrough:NO];
	[self paintMarkdownMatches:codeRegex string:string content:code marker:marker contentGroup:2 delimiterGroup:1 hasCloser:YES strikethrough:NO];
	[self highlightFencedCodeInString:string code:code marker:marker];
	[self highlightWritingGremlins];
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

- (void)insertTab:(id)sender {
	//check prefs for tab behavior
    if ([[NSUserDefaults standardUserDefaults]boolForKey:@"UsesMarkdownCompletions"]) {
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
    if ([[NSUserDefaults standardUserDefaults]boolForKey:@"UsesMarkdownCompletions"]) {
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
        
      //  if ([[NSUserDefaults standardUserDefaults]boolForKey:@"UsesMarkdownCompletions"]) {  
           // [menuItem setHidden:NO];
            if ([self clipboardHasLink]) {                
                return YES;
            }
            
       // }
//        else{
//            
//            [menuItem setHidden:YES];
//        }
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

- (NSRange)rangeForUserCompletion {
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

- (void)insertCompletion:(NSString *)word forPartialWordRange:(NSRange)charRange movement:(NSInteger)movement isFinal:(BOOL)isFinal {
	NSString *str = [self string];
	BOOL finalizedCompletion = NO;
	
	isFinal = isFinal && movement != NSRightTextMovement;
	
	if (isFinal && [word length] && (movement == NSReturnTextMovement || movement == NSTabTextMovement)) {
		
		//automatically add a trailing double-bracket if one does not already exist
		NSRange endRange = NSMakeRange(charRange.location + [word length], 2);
		if ([str length] < NSMaxRange(endRange) || ![[str substringWithRange:endRange] isEqualToString:@"]]"]) {
			word = [word stringByAppendingString:@"]]"];
		}
		finalizedCompletion = YES;
	}
	
	//preserve capitalization by transferring charRange substring into word
	if (!finalizedCompletion && charRange.length <= [word length]) { 
		NSString *existingWord = [str substringWithRange:charRange];
		word = [existingWord stringByAppendingString:[word substringFromIndex:[existingWord length]]];
	}
	
	[super insertCompletion:word forPartialWordRange:charRange movement:movement isFinal:isFinal];
}

- (void)didChangeText {
	
	//if the text storage was somehow shortened since changedRange was set in -shouldChangeText, avoid unsigned underflow and stale offsets
	NSUInteger stringLength = [[self string] length];
	if (changedRange.location > stringLength)
		changedRange = NSMakeRange(stringLength, 0);
	else
		changedRange = NSMakeRange(changedRange.location, MIN(changedRange.length, stringLength - changedRange.location));


	//-removeAttribute:range: seems slow for some reason; try checking with -attributesAtIndex:effectiveRange: first
	if ([[self textStorage] attribute:NSLinkAttributeName existsInRange:changedRange])
		[[self textStorage] removeAttribute:NSLinkAttributeName range:changedRange];
	[[self textStorage] addLinkAttributesForRange:changedRange];
	
	[[self textStorage] addStrikethroughNearDoneTagsForRange:changedRange];
	
	if (!isAutocompleting && !wasDeleting && [prefsController linksAutoSuggested] && 
		![[self undoManager] isUndoing] && ![[self undoManager] isRedoing]) {
		isAutocompleting = YES;
		[self complete:self];
		isAutocompleting = NO;
	}
	
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
		NSLog(@"looking up");
		CFBundleRef csBundle = CFBundleCreate(NULL, CFURLCreateWithFileSystemPath(NULL, CFSTR("/System/Library/Frameworks/CoreServices.framework"), kCFURLPOSIXPathStyle, TRUE));
		if (csBundle) _GetScriptManagerVariablePointer = (long (*)(short))CFBundleGetDataPointerForName(csBundle, CFSTR("GetScriptManagerVariable"));
	}
	return _GetScriptManagerVariablePointer;
}
#endif

- (void)fixTypingAttributesForSubstitutedFonts {
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
	if (lineRange.length == 0)
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
	if ([line rangeOfCharacterFromSet:[whitespace invertedSet] options:0 range:NSMakeRange(markerEnd, contentLength - markerEnd)].location != NSNotFound)
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

	if ([self replaceEmptyListMarkerWithLineBreak])
		return;
	if ([self insertMarkdownStructureNewline])
		return;
	
	[super insertNewline:sender];
	
	if ([prefsController autoIndentsNewLines]) {
		// If we should indent automatically, check the previous line and scan all the whitespace at the beginning of the line into a string and insert that string into the new line
		NSString *previousLineWhitespaceString = nil;
		NSRange previousLineRange = [[self string] lineRangeForRange:NSMakeRange([self selectedRange].location - 1, 0)];
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
    NSLog(@"dealloc linkinged");
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
        NSString *oppositeAppend;
        NSInteger pairCode;
        NSString *insertString;
        if([string isKindOfClass:[NSAttributedString class]]){
            insertString=[(NSAttributedString *)string string];
        }else{
            insertString=(NSString *)string;
        }
        if((pairCode=[insertString isPairedCharacterWithMatchString:&oppositeAppend])>=0){
            NSString *appendString = insertString;
            NSRange selRange = [self selectedRange];
            NSString *postString = self.activeParagraphPastCursor;
            
            NSInteger autoPair=-1;
            if ((postString.length==0)||(selRange.length>0)||![[NSCharacterSet alphanumericCharacterSet]characterIsMember:[postString characterAtIndex:0]]) {
                autoPair=0;
            }
            NSString *preStr=self.activeParagraphBeforeCursor;
            if (pairCode==2&&autoPair==0) {
                NSUInteger closingIdx=[postString rangeOfString:oppositeAppend].location;
                if (closingIdx!=NSNotFound) {
                    NSUInteger openingIdx=[[postString substringToIndex:closingIdx] rangeOfString:appendString].location;
                    if (openingIdx==NSNotFound) {
                        openingIdx=[preStr rangeOfString:appendString options:NSBackwardsSearch].location;
                        if (openingIdx!=NSNotFound) {
                            NSUInteger opIdx=[preStr rangeOfString:oppositeAppend options:NSBackwardsSearch].location;
                            //                              NSLog(@"opIdx :>%lu<  op:%lu",opIdx,openingIdx);
                            if ((opIdx==NSNotFound)||(opIdx<openingIdx)) {
                                closingIdx=NSNotFound;
                            }
                        }
                    }else{
                        closingIdx=NSNotFound;
                    }
                }
                if (closingIdx==NSNotFound) {
                    autoPair=1;
                }
            }else{
                NSUInteger openingIdx=[postString rangeOfString:appendString].location;
                BOOL advance=NO;
                if (pairCode==1) {
                    advance=(openingIdx==0);
                    if (!advance&&autoPair==0) {
                        NSUInteger aftCt=([[postString componentsSeparatedByString:appendString] count]%2);
                        NSUInteger befCt=([[preStr componentsSeparatedByString:appendString] count]%2);
                        if (befCt==aftCt) {
                            autoPair=1;
                        }
                    }
                }else{
                    advance=(openingIdx==0);
                }
                if (advance&&(selRange.length==0)) {
                    openingIdx=[preStr rangeOfString:oppositeAppend options:NSBackwardsSearch].location;
                    NSUInteger clIdx=[preStr rangeOfString:appendString options:NSBackwardsSearch].location;
                    if ((openingIdx!=NSNotFound)&&((clIdx==NSNotFound)||(openingIdx>=clIdx))) {
                        selRange.location+=1;
                        [self selectRangeAndRegisterUndo:selRange];
                        return;
                    }
                }
            }
            if (autoPair==1) {
                if (selRange.length>0) {
                    [[[self undoManager] prepareWithInvocationTarget:self] setSelectedRange:selRange];
                    NSRange insRange=selRange;
                    insRange.length=0;
                    [super insertText:appendString replacementRange:insRange];
                    insRange.location+=(selRange.length+1); //add appendstr.length
                    [super insertText:oppositeAppend replacementRange:insRange];
                    insRange.location+=1;  //add oppappendstr.length
                    //                    selRange.length+=2;
                    [self setSelectedRange:insRange];
                    return;
                }else {
                    [super insertText:[appendString stringByAppendingString:oppositeAppend]];
                    [self setSelectedRange:NSMakeRange(selRange.location+appendString.length, 0)];
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
                NSLog(@"trying to add markdown syntax, but selection string contains an even# of the syntax. haven't dealt with this condition yet");
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
	if ([[self undoManager] isUndoing] || [[self undoManager] isRedoing])
		return;
	[NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(renumberNumberedLists) object:nil];
	[self performSelector:@selector(renumberNumberedLists) withObject:nil afterDelay:0.05];
}

- (void)renumberNumberedLists {
	if (renumberingNumberedLists || ![prefsController autoFormatsListBullets])
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

- (BOOL)cycleListMarkerOnCurrentLine {
	if (![prefsController autoFormatsListBullets])
		return NO;
	NSString *text = [self string];
	NSRange selection = [self selectedRange];
	if (selection.location > [text length])
		return NO;
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
		replacement = @"* ";
		replaceRange = NSMakeRange(lineRange.location + markerIndex, 0);
		cursorDelta = 2;
	} else {
		replacement = @"* ";
		replaceRange = NSMakeRange(lineRange.location + markerIndex, 0);
		cursorDelta = 2;
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


- (void)setFrame:(NSRect)frameRect{
    [self updateInsetForFrame:frameRect andForceLayout:NO];
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
        keyLoc=[@"([{\"'" rangeOfString:[NSString stringWithFormat:@"%C",[[self string] characterAtIndex:keyLoc-1]]].location;
        if ((keyLoc!=NSNotFound)&&(keyLoc==[@")]}\"'" rangeOfString:[NSString stringWithFormat:@"%C",[[self string] characterAtIndex:charRange.location]]].location)) {
            return YES;
        }
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
            NSLog(@"find action was invalid");
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
	if (!layout || ![text length])
		return;
	NSColor *invisible = [NSColor colorWithCalibratedRed:0.82 green:0.22 blue:0.22 alpha:0.7];
	NSColor *space = [NSColor colorWithCalibratedRed:0.92 green:0.45 blue:0.28 alpha:0.35];
	NSUInteger index;
	for (index = 0; index < [text length]; index++) {
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
		[self scrollRangeToVisible:[self selectedRange]];
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
		if (found.location != NSNotFound)
			[self setSelectedRange:NSMakeRange(NSMaxRange(found), 0)];
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
		[self scrollRangeToVisible:[self selectedRange]];
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
		if (found.location != NSNotFound)
			[self setSelectedRange:NSMakeRange(NSMaxRange(found), 0)];
		[self scrollRangeToVisible:[self selectedRange]];
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
		[self scrollRangeToVisible:[self selectedRange]];
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
