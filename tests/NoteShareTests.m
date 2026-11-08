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

	expectEqual(NVPreviewShareFileName(nil), @"note.html", "a missing title uses note.html");
	expectEqual(NVPreviewShareFileName(@"  "), @"note.html", "a blank title uses note.html");
	expectEqual(NVPreviewShareFileName(@"a/b:c"), @"a b c.html", "separators and colons become spaces");
	expectEqual(NVPreviewShareFileName(@"note.html"), @"note.html", "an html title keeps its suffix");

	NSString *original = @"<html><head><title>Old</title></head><body><!--nv-content-start-->OLD<!--nv-content-end--></body></html>";
	expectEqual(NVPreviewHTMLByReplacingShareContent(original, @"<p>NEW</p>", @"A & B"),
				@"<html><head><title>A &amp; B</title></head><body><!--nv-content-start--><p>NEW</p><!--nv-content-end--></body></html>",
				"an in-place edit updates the stored preview");
	expectEqual(NVPreviewHTMLByReplacingShareContent(@"<html>plain</html>", @"NEW", @"T"),
				@"<html>plain</html>",
				"a document without markers stays as it was");

	NSURL *fileURL = [NSURL fileURLWithPath:@"/tmp/note.html"];
	NSAttributedString *emptyRich = [[[NSAttributedString alloc] initWithString:@""] autorelease];
	NSAttributedString *rich = [[[NSAttributedString alloc] initWithString:@"Hello"] autorelease];
	NSArray *fileOnly = NVPreviewShareObjects(fileURL, nil);
	expectTrue([fileOnly count] == 1 && [fileOnly objectAtIndex:0] == fileURL, "a missing rich-text item leaves the file");
	NSArray *dropped = NVPreviewShareObjects(fileURL, emptyRich);
	expectTrue([dropped count] == 1 && [[dropped objectAtIndex:0] isEqual:fileURL], "an empty rich-text item is omitted");
	NSArray *both = NVPreviewShareObjects(fileURL, rich);
	expectTrue([both count] == 2 && [[both objectAtIndex:0] isEqual:rich] && [[both objectAtIndex:1] isEqual:fileURL],
			   "rich text is kept ahead of the file");

	NSString *directory = [NSTemporaryDirectory() stringByAppendingPathComponent:@"nvalt-note-share-test"];
	[[NSFileManager defaultManager] removeItemAtPath:directory error:NULL];
	expectTrue(NVPreviewShareItems(@"", @"Hello", directory) == nil, "empty HTML produces no items");
	NSString *html = @"<html><body><p>Hello</p></body></html>";
	NSArray *items = NVPreviewShareItems(html, @"a/b:c", directory);
	expectTrue([items count] == 2, "a preview document shares rich text and a file");
	NSAttributedString *sharedRich = [items objectAtIndex:0];
	NSURL *sharedFile = [items objectAtIndex:1];
	expectTrue([[sharedRich string] rangeOfString:@"Hello"].location != NSNotFound, "the rich text contains the preview");
	NSString *written = [NSString stringWithContentsOfURL:sharedFile encoding:NSUTF8StringEncoding error:NULL];
	expectEqual(written, html, "the HTML file matches the rendered document");
	expectEqual([sharedFile lastPathComponent], @"a b c.html", "the HTML file uses the safe title");
	[[NSFileManager defaultManager] removeItemAtPath:directory error:NULL];

	if (gFailures) {
		fprintf(stderr, "%d failed\n", gFailures);
		return 1;
	}
	fprintf(stdout, "test-ok\n");
	[pool drain];
	return 0;
}
