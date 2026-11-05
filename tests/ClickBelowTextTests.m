#import <Cocoa/Cocoa.h>
#import <stdio.h>
#import "ClickBelowText.h"

static int gFailures = 0;

static void expectTrue(BOOL cond, const char *what) {
	if (!cond) {
		fprintf(stderr, "FAIL %s\n", what);
		gFailures++;
	}
}

static void expectPast(NSString *text, CGFloat containerY, BOOL past, const char *what) {
	NSTextView *view = [[NSTextView alloc] initUsingTextLayoutManager:NO];
	[view setFrame:NSMakeRect(0, 0, 400, 400)];
	[[view textContainer] setWidthTracksTextView:YES];
	[view setFont:[NSFont userFixedPitchFontOfSize:14]];
	[view setString:text];
	NSLayoutManager *layout = [view layoutManager];
	NSTextContainer *container = [view textContainer];
	[layout ensureLayoutForTextContainer:container];
	NSUInteger glyphs = [layout numberOfGlyphs];
	expectTrue(glyphs > 0, "layout produced glyphs");
	NSRect lastLine = [layout lineFragmentRectForGlyphAtIndex:glyphs - 1 effectiveRange:NULL];
	NSRect extra = [layout extraLineFragmentRect];
	BOOL hasExtra = !NSIsEmptyRect(extra);
	BOOL result = NVClickIsPastLastLine(containerY, NSMaxY(lastLine), hasExtra, extra.origin.y);
	expectTrue(result == past, what);
	[view release];
}

int main(void) {
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	[NSApplication sharedApplication];

	expectTrue(!NVClickIsPastLastLine(10, 32, NO, 0), "click on the last line stays there");
	expectTrue(NVClickIsPastLastLine(32, 32, NO, 0), "click at the bottom of the last line is past it");
	expectTrue(NVClickIsPastLastLine(80, 32, NO, 0), "click below a text line is past it");
	expectTrue(!NVClickIsPastLastLine(-4, 32, YES, 32), "click above the text is not past it");
	expectTrue(!NVClickIsPastLastLine(20, 32, YES, 32), "click on the line above a trailing newline stays there");
	expectTrue(NVClickIsPastLastLine(32, 32, YES, 32), "click on the blank line after a newline is past it");
	expectTrue(NVClickIsPastLastLine(90, 32, YES, 32), "click below the blank line is past it");

	NSTextView *newline = [[NSTextView alloc] initUsingTextLayoutManager:NO];
	[newline setFrame:NSMakeRect(0, 0, 400, 200)];
	[[newline textContainer] setWidthTracksTextView:YES];
	[newline setFont:[NSFont userFixedPitchFontOfSize:14]];
	[newline setString:@"note text\nlast line of text\n"];
	NSLayoutManager *layout = [newline layoutManager];
	[layout ensureLayoutForTextContainer:[newline textContainer]];
	NSRect extra = [layout extraLineFragmentRect];
	expectTrue(!NSIsEmptyRect(extra), "trailing newline produces a blank line");
	NSUInteger glyphs = [layout numberOfGlyphs];
	NSRect lastLine = [layout lineFragmentRectForGlyphAtIndex:glyphs - 1 effectiveRange:NULL];
	expectPast(@"note text\nlast line of text\n", NSMidY(lastLine), NO, "real layout: click on the text line");
	expectPast(@"note text\nlast line of text\n", NSMidY(extra), YES, "real layout: click on the blank line");
	expectPast(@"note text\nlast line of text\n", NSMaxY(extra) + 40, YES, "real layout: click below the blank line");

	NSTextView *spaces = [[NSTextView alloc] initUsingTextLayoutManager:NO];
	[spaces setFrame:NSMakeRect(0, 0, 400, 200)];
	[[spaces textContainer] setWidthTracksTextView:YES];
	[spaces setFont:[NSFont userFixedPitchFontOfSize:14]];
	[spaces setString:@"note text\nlast line of text\n    \n"];
	layout = [spaces layoutManager];
	[layout ensureLayoutForTextContainer:[spaces textContainer]];
	glyphs = [layout numberOfGlyphs];
	NSRect spacesLine = [layout lineFragmentRectForGlyphAtIndex:glyphs - 1 effectiveRange:NULL];
	extra = [layout extraLineFragmentRect];
	expectTrue(!NSIsEmptyRect(extra), "newline after spaces still produces a blank line");
	expectPast(@"note text\nlast line of text\n    \n", NSMidY(spacesLine), NO, "real layout: click on a spaces line");
	expectPast(@"note text\nlast line of text\n    \n", NSMidY(extra), YES, "real layout: click below a spaces line");

	NSTextView *text = [[NSTextView alloc] initUsingTextLayoutManager:NO];
	[text setFrame:NSMakeRect(0, 0, 400, 200)];
	[[text textContainer] setWidthTracksTextView:YES];
	[text setFont:[NSFont userFixedPitchFontOfSize:14]];
	[text setString:@"note text\nlast line of text"];
	layout = [text layoutManager];
	[text textContainer];
	[layout ensureLayoutForTextContainer:[text textContainer]];
	expectTrue(NSIsEmptyRect([layout extraLineFragmentRect]), "no trailing newline means no extra blank line");
	glyphs = [layout numberOfGlyphs];
	lastLine = [layout lineFragmentRectForGlyphAtIndex:glyphs - 1 effectiveRange:NULL];
	expectPast(@"note text\nlast line of text", NSMidY(lastLine), NO, "real layout: click on a final text line");
	expectPast(@"note text\nlast line of text", NSMaxY(lastLine) + 30, YES, "real layout: click below a final text line");

	[newline release];
	[spaces release];
	[text release];
	[pool drain];
	if (gFailures) {
		fprintf(stderr, "%d click-below-text checks failed\n", gFailures);
		return 1;
	}
	printf("click below text tests passed\n");
	return 0;
}
