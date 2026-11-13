#import <Foundation/Foundation.h>
#import <stdio.h>
#import "ListIndent.h"

static int gFailures = 0;

static void expectRange(NSRange range, NSUInteger location, NSUInteger length, const char *what) {
	if (range.location != location || range.length != length) {
		fprintf(stderr, "FAIL %s (got %lu,%lu)\n", what, (unsigned long)range.location, (unsigned long)range.length);
		gFailures++;
	}
}

int main(void) {
	NSString *ended = @"- x\n";
	expectRange(NVLineRangeForCaret(ended, 4), 4, 0, "the caret past the final newline is on its own blank line");
	expectRange(NVLineRangeForCaret(ended, 3), 0, 4, "the caret on the list line stays on that line");

	NSString *sameLine = @"- x";
	expectRange(NVLineRangeForCaret(sameLine, 3), 0, 3, "the caret at the end of a list item is still on that item");

	NSString *blank = @"- x\n\n";
	expectRange(NVLineRangeForCaret(blank, 5), 5, 0, "the caret past an extra blank line stays past it");
	expectRange(NVLineRangeForCaret(blank, 4), 4, 1, "the caret on the blank line between newlines stays there");

	expectRange(NVLineRangeForCaret(@"", 0), 0, 0, "an empty note has an empty line");
	expectRange(NVLineRangeForCaret(nil, 3), 0, 0, "a missing note has an empty line");

	if (gFailures) {
		fprintf(stderr, "%d list indent tests failed\n", gFailures);
		return 1;
	}
	printf("list indent tests passed\n");
	return 0;
}
