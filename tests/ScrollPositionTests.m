#import <Cocoa/Cocoa.h>
#import <stdio.h>
#import "NoteLayout.h"

/* Build and run from the project root:
   clang -framework Cocoa -I. tests/ScrollPositionTests.m NoteLayout.m -o /tmp/ScrollPositionTests && /tmp/ScrollPositionTests */

static int gFailures = 0;

static void expectTrue(BOOL cond, const char *what) {
	if (!cond) {
		fprintf(stderr, "FAIL %s\n", what);
		gFailures++;
	}
}

static NSTextView *textViewInScrollView(NSScrollView *scroll) {
	NSTextView *text = [[NSTextView alloc] initWithFrame:[[scroll contentView] bounds]];
	[text setVerticallyResizable:YES];
	[text setHorizontallyResizable:NO];
	[text setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
	[text setMinSize:NSMakeSize(0, NSHeight([scroll frame]))];
	[text setMaxSize:NSMakeSize(100000, 10000000)];
	[[text textContainer] setWidthTracksTextView:YES];
	[[text textContainer] setHeightTracksTextView:NO];
	[scroll setDocumentView:text];
	return text;
}

int main(void) {
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	[NSApplication sharedApplication];
	NSRect frame = NSMakeRect(0, 0, 420, 280);
	NSWindow *window = [[NSWindow alloc] initWithContentRect:frame styleMask:NSWindowStyleMaskTitled backing:NSBackingStoreBuffered defer:NO];
	NSScrollView *scroll = [[NSScrollView alloc] initWithFrame:frame];
	[scroll setHasVerticalScroller:YES];
	[scroll setAutohidesScrollers:YES];
	[scroll setScrollerStyle:NSScrollerStyleLegacy];
	[[window contentView] addSubview:scroll];
	NSTextView *text = textViewInScrollView(scroll);
	NSClipView *clip = [scroll contentView];

	NSString *line = @"Each line of this note is a paragraph of its own for the test.\n";
	NSUInteger lineLength = [line length];
	NSMutableString *body = [NSMutableString string];
	NSUInteger i;
	for (i = 0; i < 300; i++)
		[body appendString:line];
	[text setString:body];
	NVLayOutEntireNote(text);

	expectTrue(NVFirstVisibleCharIndex(text) == 0, "a freshly laid out note shows its first character at the top");

	expectTrue(NVScrollCharIndexToTop(text, 50 * lineLength), "a line inside the note can be scrolled to the top");
	CGFloat wideOffset = NSMinY([clip bounds]);
	expectTrue(wideOffset > 0, "the view moved down to reach it");
	expectTrue(NVFirstVisibleCharIndex(text) == 50 * lineLength, "that line is then the first one showing");
	NSRect line50 = [[text layoutManager] lineFragmentRectForGlyphAtIndex:[[text layoutManager] glyphIndexForCharacterAtIndex:50 * lineLength] effectiveRange:NULL];
	expectTrue(fabs(wideOffset - (NSMinY(line50) + [text textContainerOrigin].y)) < 0.5, "the line sits exactly at the top edge");

	expectTrue(NVScrollCharIndexToTop(text, 0), "the first line can be asked for");
	expectTrue(NSMinY([clip bounds]) == 0, "the first line keeps the margin above it");
	expectTrue(NVFirstVisibleCharIndex(text) == 0, "and reads back as the first character");

	expectTrue(NVScrollCharIndexToTop(text, 120 * lineLength + 7), "a character in the middle of a line is accepted");
	expectTrue(NVFirstVisibleCharIndex(text) == 120 * lineLength, "the first showing character is the start of its line");

	CGFloat before = NSMinY([clip bounds]);
	expectTrue(!NVScrollCharIndexToTop(text, [body length]), "an index past the end is refused");
	expectTrue(!NVScrollCharIndexToTop(text, NSNotFound), "a missing index is refused");
	expectTrue(NSMinY([clip bounds]) == before, "a refused index leaves the view where it was");

	expectTrue(NVScrollCharIndexToTop(text, 299 * lineLength), "the last line is accepted");
	NSUInteger top = NVFirstVisibleCharIndex(text);
	expectTrue(top > 120 * lineLength && top < 299 * lineLength, "the last line cannot reach the top, so an earlier line shows there");
	expectTrue(top % lineLength == 0, "the first showing character still starts a line");

	NVScrollCharIndexToTop(text, 10 * lineLength);
	NSPoint nudged = [clip bounds].origin;
	nudged.y += 5.0;
	[clip scrollToPoint:nudged];
	[scroll reflectScrolledClipView:clip];
	expectTrue(NVFirstVisibleCharIndex(text) == 10 * lineLength, "a line partly scrolled off the top still counts as showing");

	/* A narrower window wraps each paragraph onto two lines. The saved character
	   must still find the same paragraph. */
	[scroll setFrame:NSMakeRect(0, 0, 200, 280)];
	NVLayOutEntireNote(text);
	expectTrue(NSWidth([text frame]) < 220, "the note view followed the narrower window");
	expectTrue(NVScrollCharIndexToTop(text, 50 * lineLength), "the saved index still works after re-wrapping");
	expectTrue(NVFirstVisibleCharIndex(text) == 50 * lineLength, "the same paragraph is at the top after re-wrapping");
	expectTrue(NSMinY([clip bounds]) > wideOffset, "the wrapped text needed a longer scroll to get there");

	/* Bringing a range on screen moves only as far as needed. */
	[scroll setFrame:frame];
	NVLayOutEntireNote(text);
	NVScrollCharIndexToTop(text, 50 * lineLength);
	CGFloat at50 = NSMinY([clip bounds]);
	NVScrollRangeIntoView(text, NSMakeRange(55 * lineLength + 3, 4));
	expectTrue(NSMinY([clip bounds]) == at50, "a range already on screen does not move the view");
	NVScrollRangeIntoView(text, NSMakeRange(49 * lineLength, 0));
	expectTrue(NVFirstVisibleCharIndex(text) == 49 * lineLength, "a range just above comes in at the top");
	NVScrollRangeIntoView(text, NSMakeRange(200 * lineLength, 0));
	NSRect line200 = [[text layoutManager] lineFragmentRectForGlyphAtIndex:[[text layoutManager] glyphIndexForCharacterAtIndex:200 * lineLength] effectiveRange:NULL];
	expectTrue(fabs(NSMaxY([clip bounds]) - (NSMaxY(line200) + [text textContainerOrigin].y)) < 0.5, "a range below comes in at the bottom");
	expectTrue(NVFirstVisibleCharIndex(text) < 200 * lineLength, "the lines above it fill the rest of the view");
	NVScrollRangeIntoView(text, NSMakeRange(0, 0));
	expectTrue(NSMinY([clip bounds]) == 0, "the start of the note brings back the margin");
	NVScrollRangeIntoView(text, NSMakeRange([body length], 0));
	NSRect extraLine = [[text layoutManager] extraLineFragmentRect];
	expectTrue(!NSIsEmptyRect(extraLine), "a note ending in a line break has an empty line after it");
	expectTrue(NSMaxY([clip bounds]) + 0.5 >= NSMaxY(extraLine) + [text textContainerOrigin].y, "a caret at the very end shows that empty line");
	CGFloat atEnd = NSMinY([clip bounds]);
	NVScrollRangeIntoView(text, NSMakeRange(NSNotFound, 0));
	NVScrollRangeIntoView(nil, NSMakeRange(0, 0));
	expectTrue(NSMinY([clip bounds]) == atEnd, "a missing range or view changes nothing");

	/* Showing the nearest of several ranges. */
	[text setString:body];
	NVLayOutEntireNote(text);
	NVScrollCharIndexToTop(text, 100 * lineLength);
	CGFloat at100 = NSMinY([clip bounds]);
	NSArray *farBelow = [NSArray arrayWithObjects:[NSValue valueWithRange:NSMakeRange(250 * lineLength, 4)], [NSValue valueWithRange:NSMakeRange(200 * lineLength, 4)], nil];
	NVScrollToShowNearestRange(text, farBelow);
	expectTrue(NVFirstVisibleCharIndex(text) < 200 * lineLength && NSMaxY([clip bounds]) > at100 + 1.0, "with every match below, the view moves down to the nearest one");
	line200 = [[text layoutManager] lineFragmentRectForGlyphAtIndex:[[text layoutManager] glyphIndexForCharacterAtIndex:200 * lineLength] effectiveRange:NULL];
	expectTrue(fabs(NSMaxY([clip bounds]) - (NSMaxY(line200) + [text textContainerOrigin].y)) < 0.5, "and stops as soon as that match is on screen");
	NVScrollCharIndexToTop(text, 100 * lineLength);
	NSArray *aboveAndBelow = [NSArray arrayWithObjects:[NSValue valueWithRange:NSMakeRange(5 * lineLength, 4)], [NSValue valueWithRange:NSMakeRange(95 * lineLength, 4)], [NSValue valueWithRange:NSMakeRange(180 * lineLength, 4)], nil];
	NVScrollToShowNearestRange(text, aboveAndBelow);
	expectTrue(NVFirstVisibleCharIndex(text) == 95 * lineLength, "the nearest match wins, here the one just above");
	NVScrollCharIndexToTop(text, 100 * lineLength);
	NSArray *oneVisible = [NSArray arrayWithObjects:[NSValue valueWithRange:NSMakeRange(5 * lineLength, 4)], [NSValue valueWithRange:NSMakeRange(104 * lineLength, 4)], nil];
	NVScrollToShowNearestRange(text, oneVisible);
	expectTrue(NSMinY([clip bounds]) == at100, "a match already on screen leaves the view alone");
	NVScrollToShowNearestRange(text, [NSArray array]);
	NVScrollToShowNearestRange(text, nil);
	NVScrollToShowNearestRange(nil, oneVisible);
	expectTrue(NSMinY([clip bounds]) == at100, "no matches or no view changes nothing");
	NVScrollCharIndexToTop(text, 0);
	NVScrollToShowNearestRange(text, farBelow);
	expectTrue(NVFirstVisibleCharIndex(text) < 200 * lineLength && NSMinY([clip bounds]) > 0, "from the top, the first match in the note is the nearest");

	[text setString:@""];
	NVLayOutEntireNote(text);
	expectTrue(NVFirstVisibleCharIndex(text) == NSNotFound, "an empty note has no first character");
	expectTrue(!NVScrollCharIndexToTop(text, 0), "an empty note cannot be scrolled");
	expectTrue(NVFirstVisibleCharIndex(nil) == NSNotFound, "no view gives no character");
	expectTrue(!NVScrollCharIndexToTop(nil, 0), "no view cannot be scrolled");

	if (gFailures)
		fprintf(stderr, "%d failure(s)\n", gFailures);
	else
		printf("ScrollPositionTests: all passed\n");
	[window release];
	[pool drain];
	return gFailures ? 1 : 0;
}
