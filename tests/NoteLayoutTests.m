#import <Cocoa/Cocoa.h>
#import <stdio.h>
#import "NoteLayout.h"

static int gFailures = 0;

static void expectTrue(BOOL cond, const char *what) {
	if (!cond) {
		fprintf(stderr, "FAIL %s\n", what);
		gFailures++;
	}
}

/* Same resize policy as the note editor. */
@interface NVEditorTextView : NSTextView
@end

@implementation NVEditorTextView
- (void)setFrameSize:(NSSize)newSize {
	newSize.height = NVNoteHeightForProposedHeight(self, newSize.height);
	[super setFrameSize:newSize];
}
- (void)setFrame:(NSRect)frameRect {
	frameRect.size.height = NVNoteHeightForProposedHeight(self, frameRect.size.height);
	[super setFrame:frameRect];
}
- (void)setConstrainedFrameSize:(NSSize)size {
	size.height = NVNoteHeightForProposedHeight(self, size.height);
	[super setConstrainedFrameSize:size];
}
- (void)layoutManager:(NSLayoutManager *)layoutManager didCompleteLayoutForTextContainer:(NSTextContainer *)textContainer atEnd:(BOOL)flag {
}
@end

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

	NSMutableString *body = [NSMutableString string];
	NSUInteger line;
	for (line = 0; line < 300; line++)
		[body appendString:@"A long note needs a scrollbar once it has been laid out.\n"];
	[text setString:body];
	expectTrue([[scroll verticalScroller] isHidden], "replacing the note does not lay it out, so the bar stays hidden");
	expectTrue(NSHeight([text frame]) <= NSHeight([[scroll contentView] frame]) + 1.0, "the unloaded note is only as tall as the window");

	NVLayOutEntireNote(text);
	expectTrue(NSHeight([text frame]) > NSHeight([[scroll contentView] frame]) + 40.0, "a laid out note is taller than the window");
	expectTrue(![[scroll verticalScroller] isHidden], "a laid out note shows the scrollbar");

	[text setFrame:NSMakeRect(0, 0, NSWidth([[scroll contentView] bounds]) - 16.0, NSHeight([[scroll contentView] bounds]))];
	NVLayOutEntireNote(text);
	expectTrue(NSHeight([text frame]) > NSHeight([[scroll contentView] frame]) + 40.0, "a narrower pass keeps the full note height");
	expectTrue(![[scroll verticalScroller] isHidden], "the scrollbar stays after the width changes");

	[text setString:@"Short\n"];
	NVLayOutEntireNote(text);
	expectTrue([[scroll verticalScroller] isHidden], "a note that fits hides the scrollbar");

	NVLayOutEntireNote(nil);
	[text setString:@""];
	NVLayOutEntireNote(text);
	expectTrue([[scroll verticalScroller] isHidden], "an empty note hides the scrollbar");

	/* A later note can arrive with the container clamped to the window. Laying
	   out must open it again or the scrollbar never comes back. */
	[text setString:body];
	[[text textContainer] setHeightTracksTextView:NO];
	[[text textContainer] setContainerSize:NSMakeSize(NSWidth([text frame]), NSHeight([[scroll contentView] bounds]))];
	[text setFrameSize:NSMakeSize(NSWidth([text frame]), NSHeight([[scroll contentView] bounds]))];
	expectTrue([[scroll verticalScroller] isHidden], "a clamped container hides the scrollbar");
	NVLayOutEntireNote(text);
	expectTrue(NSHeight([text frame]) > NSHeight([[scroll contentView] frame]) + 40.0, "unclamping lays the whole note out");
	expectTrue(![[scroll verticalScroller] isHidden], "unclamping brings the scrollbar back");
	[[text textContainer] setContainerSize:NSMakeSize(NSWidth([text frame]), NSHeight([[scroll contentView] bounds]))];
	[text setFrameSize:NSMakeSize(NSWidth([text frame]), NSHeight([[scroll contentView] bounds]))];
	[text setString:body];
	NVLayOutEntireNote(text);
	expectTrue(![[scroll verticalScroller] isHidden], "switching back to the first long note shows the scrollbar");
	expectTrue(![[text layoutManager] backgroundLayoutEnabled], "background layout stays off so a partial pass cannot shrink the note");

	[text release];

	NVEditorTextView *editor = [[NVEditorTextView alloc] initWithFrame:[[scroll contentView] bounds]];
	NSMutableString *other = [NSMutableString string];
	CGFloat clipHeight = NSHeight([[scroll contentView] bounds]);
	[editor setVerticallyResizable:YES];
	[editor setHorizontallyResizable:NO];
	[editor setAutoresizingMask:NSViewWidthSizable];
	[editor setMinSize:NSMakeSize(0, NSHeight([scroll frame]))];
	[editor setMaxSize:NSMakeSize(100000, 10000000)];
	[[editor textContainer] setWidthTracksTextView:YES];
	[[editor textContainer] setHeightTracksTextView:NO];
	[[editor layoutManager] setDelegate:(id)editor];
	[scroll setDocumentView:editor];
	for (line = 0; line < 300; line++)
		[other appendString:@"Another long note also needs a scrollbar.\n"];
	[editor setString:body];
	NVLayOutEntireNote(editor);
	expectTrue(![[scroll verticalScroller] isHidden], "the first long note shows the scrollbar");
	[editor setString:other];
	[editor setFrameSize:NSMakeSize(NSWidth([editor frame]), clipHeight)];
	NVLayOutEntireNote(editor);
	expectTrue(![[scroll verticalScroller] isHidden], "another long note shows the scrollbar the first time");
	[editor setFrameSize:NSMakeSize(NSWidth([editor frame]), clipHeight)];
	expectTrue(![[scroll verticalScroller] isHidden], "a window-height resize does not hide that scrollbar");
	[editor setString:body];
	[editor setFrameSize:NSMakeSize(NSWidth([editor frame]), clipHeight)];
	NVLayOutEntireNote(editor);
	[editor setFrameSize:NSMakeSize(NSWidth([editor frame]), clipHeight)];
	expectTrue(![[scroll verticalScroller] isHidden], "returning to the first note keeps the scrollbar");
	[editor setString:@"Short\n"];
	NVLayOutEntireNote(editor);
	expectTrue([[scroll verticalScroller] isHidden], "a short note still hides the scrollbar");
	[editor setString:other];
	NVLayOutEntireNote(editor);
	expectTrue(![[scroll verticalScroller] isHidden], "leaving the short note shows the scrollbar again");
	/* Drawing lays out only the visible screen. That reports every glyph as
	   laid out even though the last line has no fragment, and a resize then
	   collapses the note to the window. */
	[[editor textContainer] setContainerSize:NSMakeSize(NSWidth([editor frame]), clipHeight)];
	[[editor layoutManager] invalidateLayoutForCharacterRange:NSMakeRange(0, [[editor string] length]) actualCharacterRange:NULL];
	[[editor layoutManager] ensureLayoutForBoundingRect:NSMakeRect(0, 0, NSWidth([editor frame]), clipHeight) inTextContainer:[editor textContainer]];
	[editor setFrameSize:NSMakeSize(NSWidth([editor frame]), clipHeight)];
	expectTrue(NSHeight([editor frame]) > clipHeight + 40.0, "a visible-only layout does not shrink a laid out note");
	expectTrue(![[scroll verticalScroller] isHidden], "the scrollbar stays after a visible-only layout");
	/* Invalidating the start of a note clears the used rect and leaves the last
	   line's old position in place. Laying out again must not treat the cleared
	   rect as a one-screen note. */
	NVLayOutEntireNote(editor);
	[[editor layoutManager] invalidateLayoutForCharacterRange:NSMakeRange(0, [[editor string] length] / 5) actualCharacterRange:NULL];
	NVLayOutEntireNote(editor);
	expectTrue(NSHeight([editor frame]) > clipHeight + 40.0, "a partial invalidate does not shrink a laid out note");
	expectTrue(![[scroll verticalScroller] isHidden], "the scrollbar stays after a partial invalidate");
	[editor setString:@"Short\n"];
	NVLayOutEntireNote(editor);
	expectTrue([[scroll verticalScroller] isHidden], "a short note still hides the scrollbar after that");

	[editor release];

	/* The view can stay as tall as an older note while the new text is at the
	   top. Background layout will not scroll back on its own. */
	NSTextView *parked = textViewInScrollView(scroll);
	[[parked layoutManager] setBackgroundLayoutEnabled:NO];
	[parked setVerticallyResizable:NO];
	[parked setString:@"Short note that must stay visible.\n"];
	NVLayOutEntireNote(parked);
	CGFloat tail = clipHeight * 8.0;
	[parked setFrameSize:NSMakeSize(NSWidth([parked frame]), tail)];
	[parked scrollPoint:NSMakePoint(0, tail - clipHeight)];
	expectTrue(NSHeight([parked frame]) > clipHeight * 2.0, "the text view is still taller than the window");
	expectTrue([parked visibleRect].origin.y > clipHeight, "the clip is in the empty tail");
	NVRevealNoteTextIfScrolledPastIt(parked);
	{
		NSRect used = [[parked layoutManager] usedRectForTextContainer:[parked textContainer]];
		used.origin.x += [parked textContainerOrigin].x;
		used.origin.y += [parked textContainerOrigin].y;
		expectTrue(NSIntersectsRect([parked visibleRect], used), "a note scrolled past its text is brought back");
	}
	[parked release];
	[scroll release];
	[window release];
	[pool drain];
	if (gFailures) {
		fprintf(stderr, "%d note layout tests failed\n", gFailures);
		return 1;
	}
	printf("note layout tests passed\n");
	return 0;
}
