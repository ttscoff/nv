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

		expectEqual([@"Title: Hello\nAuthor: Me\n\nThe note" truncatedPreviewStringOfLength:100], @"The note", "MultiMarkdown front matter is left out of the summary");
		expectEqual([@"Title: Hello\nThe note" truncatedPreviewStringOfLength:100], @"Title: Hello The note", "a key line without a blank line stays in the summary");
		expectEqual([@"https://example.com\n\nBody" truncatedPreviewStringOfLength:100], @"https://example.com  Body", "a URL at the top is not front matter");
		expectEqual([@"---\ntitle: Hello\nauthor: Me\n---\n\nThe note" truncatedPreviewStringOfLength:100], @"The note", "YAML front matter is left out of the summary");
		expectEqual([@"---\ntags:\n  - one\n  - two\n---\n\nThe note" truncatedPreviewStringOfLength:100], @"The note", "YAML lists inside the borders are left out of the summary");
		expectEqual([@"---\ntitle: Hello\n---\nThe note" truncatedPreviewStringOfLength:100], @"The note", "YAML body starts on the line after the closing border");
		expectEqual([@"---\ntitle: Hello\n...\n\nThe note" truncatedPreviewStringOfLength:100], @"The note", "a YAML block can close with dots");
		expectEqual([@"---\nThe note" truncatedPreviewStringOfLength:100], @"--- The note", "a horizontal rule without a closing border stays in the summary");
		expectEqual([@"\n---\ntitle: Hello\n---\n\nThe note" truncatedPreviewStringOfLength:100], @" --- title: Hello ---  The note", "YAML has to start on the first line");

		NSRange yaml = NVFrontMatterRange(@"---\ntitle: Hello\n---\n\nBody");
		NSRange want = NSMakeRange(0, [@"---\ntitle: Hello\n---\n\n" length]);
		if (!NSEqualRanges(yaml, want)) {
			fprintf(stderr, "FAIL YAML range\n  got: %lu,%lu\n  want: %lu,%lu\n",
				(unsigned long)yaml.location, (unsigned long)yaml.length,
				(unsigned long)want.location, (unsigned long)want.length);
			gFailures++;
		}
		NSRange mmd = NVFrontMatterRange(@"Title: Hello\n\nBody");
		NSRange mmdWant = NSMakeRange(0, [@"Title: Hello\n\n" length]);
		if (!NSEqualRanges(mmd, mmdWant)) {
			fprintf(stderr, "FAIL MultiMarkdown range\n  got: %lu,%lu\n  want: %lu,%lu\n",
				(unsigned long)mmd.location, (unsigned long)mmd.length,
				(unsigned long)mmdWant.location, (unsigned long)mmdWant.length);
			gFailures++;
		}
		NSRange none = NVFrontMatterRange(@"Hello world");
		if (none.location != NSNotFound) {
			fprintf(stderr, "FAIL plain note reported front matter\n");
			gFailures++;
		}

		if (gFailures) {
			fprintf(stderr, "%d preview truncation test(s) failed\n", gFailures);
			return 1;
		}
		printf("preview truncation tests passed\n");
		return 0;
	}
}
