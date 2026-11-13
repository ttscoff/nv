#import <Foundation/Foundation.h>
#import <stdio.h>
#import "PreviewSlice.h"

static int gFailures = 0;

static void expectSlice(NVPreviewSlice got, NSUInteger location, NSUInteger length, BOOL before, BOOL after, const char *what) {
	if (got.range.location != location || got.range.length != length || got.omittedBefore != before || got.omittedAfter != after) {
		fprintf(stderr, "FAIL %s (got %lu+%lu before %d after %d, want %lu+%lu before %d after %d)\n", what,
			(unsigned long)got.range.location, (unsigned long)got.range.length, got.omittedBefore, got.omittedAfter,
			(unsigned long)location, (unsigned long)length, before, after);
		gFailures++;
	}
}

int main(void) {
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	NVPreviewSlice slice;
	NSString *note;
	NSString *marked;

	slice = NVPreviewSliceForCaret(nil, 0, 10, 10, 10);
	expectSlice(slice, 0, 0, NO, NO, "nil text previews nothing");

	slice = NVPreviewSliceForCaret(@"short note", 4, 100, 4, 4);
	expectSlice(slice, 0, 10, NO, NO, "a note under the limit is previewed whole");

	note = @"0123456789abcdef";
	slice = NVPreviewSliceForCaret(note, 8, 4, 8, 4);
	expectSlice(slice, 4, 8, YES, YES, "a long note previews the characters around the caret");

	slice = NVPreviewSliceForCaret(note, 0, 4, 8, 4);
	expectSlice(slice, 0, 8, NO, YES, "the start of a long note keeps the opening");

	slice = NVPreviewSliceForCaret(note, 16, 4, 8, 4);
	expectSlice(slice, 8, 8, YES, NO, "the end of a long note keeps the closing");

	slice = NVPreviewSliceForCaret(note, NSNotFound, 4, 8, 4);
	expectSlice(slice, 0, 8, NO, YES, "a missing caret previews the opening");

	note = @"ab\ncd\nef\n";
	slice = NVPreviewSliceForCaret(note, 4, 0, 4, 0);
	expectSlice(slice, 0, 6, NO, YES, "the window grows to the whole lines it lands on");

	note = @"AAAA\n```\nBBBB\n```\nCCCC\n";
	slice = NVPreviewSliceForCaret(note, 11, 0, 4, 20);
	expectSlice(slice, 5, 8, YES, YES, "a slice that starts inside a code fence includes the opener");

	slice = NVPreviewSliceForCaret(note, 11, 0, 4, 2);
	expectSlice(slice, 9, 4, YES, YES, "a fence outside the lookback stays out of the slice");

	marked = NVPreviewHTMLMarkingOmittedEdges(@"<p>Hi</p>", YES, YES);
	if ([marked rangeOfString:@"Earlier text"].location == NSNotFound || [marked rangeOfString:@"Later text"].location == NSNotFound || [marked rangeOfString:@"<p>Hi</p>"].location == NSNotFound) {
		fprintf(stderr, "FAIL omitted edges wrap the section\n");
		gFailures++;
	}
	if (![NVPreviewHTMLMarkingOmittedEdges(@"<p>Hi</p>", NO, NO) isEqualToString:@"<p>Hi</p>"]) {
		fprintf(stderr, "FAIL a whole note is not wrapped\n");
		gFailures++;
	}

	if (NVPreviewUpdateDelayForLength(100) != NVPreviewLiveUpdateDelay) {
		fprintf(stderr, "FAIL a short note updates immediately\n");
		gFailures++;
	}
	if (NVPreviewUpdateDelayForLength(NVMarkdownSyntaxHighlightLimit) != NVPreviewLiveUpdateDelay) {
		fprintf(stderr, "FAIL a note at the limit still updates immediately\n");
		gFailures++;
	}
	if (NVPreviewCaretLine(@"ab\ncd\n", 4, NSMakeRange(0, 6)) != 2) {
		fprintf(stderr, "FAIL the caret line is counted inside the slice\n");
		gFailures++;
	}
	if (NVPreviewUpdateDelayForLength(NVMarkdownSyntaxHighlightLimit + 1) != NVPreviewLargeNoteUpdateDelay) {
		fprintf(stderr, "FAIL a note past the limit waits for a pause\n");
		gFailures++;
	}

	[pool drain];
	if (!gFailures)
		printf("test-ok\n");
	return gFailures ? 1 : 0;
}
