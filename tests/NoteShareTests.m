#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>
#import <stdio.h>
#import "NoteShare.h"

static int gFailures = 0;

static void expectEqual(NSString *got, NSString *want, const char *what) {
	if ((got == nil && want == nil) || (got && want && [got isEqualToString:want]))
		return;
	fprintf(stderr, "FAIL %s\n  got: %s\n  want: %s\n", what,
			got ? [got UTF8String] : "(nil)",
			want ? [want UTF8String] : "(nil)");
	gFailures++;
}

static void expectTrue(BOOL got, const char *what) {
	if (got)
		return;
	fprintf(stderr, "FAIL %s\n", what);
	gFailures++;
}

int main(int argc, const char *argv[]) {
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	NSString *body = @"nvalt task lists";
	NSRange none = NSMakeRange(0, 0);
	NSRange task = NSMakeRange(6, 4);

	expectEqual(NVEditorShareText(body, task), @"task", "a body selection is what gets shared");
	expectEqual(NVEditorShareText(body, none), body, "no selection shares the whole note");
	expectEqual(NVEditorShareText(@"", none), nil, "an empty note is not shared");
	expectEqual(NVEditorShareText(body, NSMakeRange(100, 4)), body, "a selection past the end shares the whole note");

	NSString *original = @"<html><head><title>Old</title></head><body><!--nv-content-start-->OLD<!--nv-content-end--></body></html>";
	expectEqual(NVPreviewHTMLByReplacingShareContent(original, @"<p>NEW</p>", @"A & B"),
				@"<html><head><title>A &amp; B</title></head><body><!--nv-content-start--><p>NEW</p><!--nv-content-end--></body></html>",
				"an in-place edit updates the stored preview");
	expectEqual(NVPreviewHTMLByReplacingShareContent(@"<html>plain</html>", @"NEW", @"T"),
				@"<html>plain</html>",
				"a document without markers stays as it was");

	expectTrue(NVPreviewShareRichText(@"") == nil, "empty HTML produces no rich text");
	expectTrue(NVPreviewShareRichText(@"<html><body></body></html>") == nil, "HTML with no text produces no rich text");
	NSAttributedString *sharedRich = NVPreviewShareRichText(@"<html><body><p>Hello</p></body></html>");
	expectTrue(sharedRich && [[sharedRich string] rangeOfString:@"Hello"].location != NSNotFound, "the rich text contains the preview");

	if (gFailures) {
		fprintf(stderr, "%d failed\n", gFailures);
		return 1;
	}
	fprintf(stdout, "test-ok\n");
	[pool drain];
	return 0;
}
