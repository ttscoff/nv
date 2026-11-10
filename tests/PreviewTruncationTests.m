#import <Foundation/Foundation.h>
#import <stdio.h>
#import "PreviewTruncation.h"

static int gFailures = 0;

static void expectEqual(NSString *got, NSString *want, const char *what) {
	if ((got == nil && want == nil) || (got && want && [got isEqualToString:want]))
		return;
	fprintf(stderr, "FAIL %s\n  got: %s\n  want: %s\n", what,
			got ? [got UTF8String] : "(nil)",
			want ? [want UTF8String] : "(nil)");
	gFailures++;
}

int main(int argc, const char *argv[]) {
	@autoreleasepool {
		// A note body comes from the text storage, which keeps its characters as UTF-16. The old code
		// never copied a one-character body and then tried to read its empty buffer as UTF-16.
		NSMutableString *storage = [NSMutableString stringWithString:@"h"];
		expectEqual([storage truncatedPreviewStringOfLength:1], @"h", "one character from a mutable body");
		expectEqual([@"h" truncatedPreviewStringOfLength:1], @"h", "one character from a literal");
		expectEqual([@"h" truncatedPreviewStringOfLength:40], @"h", "count past the end gives the whole body");

		expectEqual([@"" truncatedPreviewStringOfLength:10], @"", "empty body");
		expectEqual([@"hello" truncatedPreviewStringOfLength:0], @"", "count of zero");

		expectEqual([@"hello world" truncatedPreviewStringOfLength:5], @"hello", "plain cut");
		expectEqual([@"one\ntwo\tthree\r\nfour" truncatedPreviewStringOfLength:100], @"one two three  four", "breaks and tabs become spaces");
		expectEqual([@"a\u2028b\u2029c\x03" @"d\fe" truncatedPreviewStringOfLength:100], @"a b c d e", "separators, end-of-text and form feed become spaces");
		expectEqual([@"\n\n" truncatedPreviewStringOfLength:2], @"  ", "a body of only breaks");

		// Characters that need several UTF-8 bytes used to overflow a byte-sized buffer and fail outright.
		expectEqual([@"日本語のメモ" truncatedPreviewStringOfLength:1], @"日", "one multibyte character");
		expectEqual([@"日本語のメモ" truncatedPreviewStringOfLength:3], @"日本語", "count is in characters, not bytes");
		expectEqual([@"café au lait" truncatedPreviewStringOfLength:4], @"café", "accented character kept");

		// An emoji is a surrogate pair in UTF-16; the cut keeps it whole rather than leaving half of it.
		expectEqual([@"😀 smile" truncatedPreviewStringOfLength:1], @"😀", "surrogate pair stays whole");
		expectEqual([@"a😀b" truncatedPreviewStringOfLength:2], @"a😀", "cut inside a pair extends to its end");
		expectEqual([@"éx" truncatedPreviewStringOfLength:1], @"é", "combining mark stays with its base");

		if (gFailures) {
			fprintf(stderr, "%d preview truncation test(s) failed\n", gFailures);
			return 1;
		}
		printf("preview truncation tests passed\n");
		return 0;
	}
}
