#import <Foundation/Foundation.h>
#import <stdio.h>
#import "SearchHighlight.h"

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
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	NSString *title = @"nvalt task lists";
	NSRange none = NSMakeRange(0, 0);
	NSRange allOfTitle = NSMakeRange(0, [title length]);
	NSRange typedTail = NSMakeRange(10, [title length] - 10);

	expectEqual(NVSearchHighlightQuery(NO, YES, NO, YES, none, nil, @"nvalt task", title),
				@"nvalt task", "typing a search highlights those words");

	expectEqual(NVSearchHighlightQuery(NO, YES, NO, YES, typedTail, @"nvalt task", title, title),
				@"nvalt task", "autocomplete highlights the typed prefix");

	expectEqual(NVSearchHighlightQuery(NO, NO, YES, YES, typedTail, @"nvalt task", title, title),
				@"nvalt task", "the search field keeps the highlight after the keystroke");

	expectEqual(NVSearchHighlightQuery(YES, NO, YES, NO, none, @"nvalt task", title, title),
				nil, "the note body does not keep the search highlight");

	expectEqual(NVSearchHighlightQuery(NO, NO, YES, NO, allOfTitle, @"nvalt task", title, title),
				@"nvalt task", "opening a found note keeps the search highlighted");

	expectEqual(NVSearchHighlightQuery(NO, NO, YES, YES, allOfTitle, @"nvalt task", title, title),
				@"nvalt task", "the title in the field does not erase the search highlight");

	expectEqual(NVSearchHighlightQuery(NO, NO, NO, YES, allOfTitle, nil, title, title),
				nil, "a title with no search behind it is not highlighted");

	expectEqual(NVSearchHighlightQuery(NO, YES, YES, YES, none, @"nvalt task", @"", title),
				nil, "clearing the field leaves nothing to highlight");

	if (!NVSearchHighlightOverlayWanted(YES, @"nvalt task"))
		fprintf(stderr, "FAIL a search keeps the highlight overlay\n"), gFailures++;
	if (NVSearchHighlightOverlayWanted(YES, nil))
		fprintf(stderr, "FAIL the note body takes the highlight overlay down\n"), gFailures++;
	if (NVSearchHighlightOverlayWanted(YES, @""))
		fprintf(stderr, "FAIL an empty search takes the highlight overlay down\n"), gFailures++;
	if (NVSearchHighlightOverlayWanted(NO, @"nvalt task"))
		fprintf(stderr, "FAIL turning highlights off takes the overlay down\n"), gFailures++;

	NSRange avoided = NVSearchHighlightOverlayAvoidedSelection(NSMakeRange(12, 5));
	if (avoided.location != NSNotFound || avoided.length != 0)
		fprintf(stderr, "FAIL preview still paints the selected search match\n"), gFailures++;

	CGFloat red, green, blue;
	BOOL whiteInk = YES;
	NVSearchHighlightPaint(0.945, 0.702, 0.702, 1.0, 0.231, 0.259, 0.325, &red, &green, &blue, &whiteInk);
	if (red < 0.94 || green < 0.69 || blue < 0.69 || whiteInk)
		fprintf(stderr, "FAIL opaque highlight keeps its color\n"), gFailures++;

	whiteInk = YES;
	NVSearchHighlightPaint(0.945, 0.702, 0.702, 0.40, 0.231, 0.259, 0.325, &red, &green, &blue, &whiteInk);
	if (red < 0.90 || green > 0.80 || blue > 0.80 || whiteInk)
		fprintf(stderr, "FAIL translucent highlight stays colored on a dark note (%.3f %.3f %.3f)\n", red, green, blue), gFailures++;

	NVSearchHighlightPaint(0.871, 0.930, 0.110, 0.45, 0.231, 0.259, 0.325, &red, &green, &blue, &whiteInk);
	if (NVSearchHighlightSaturation(red, green, blue) < 0.40 || green < red || green < blue)
		fprintf(stderr, "FAIL yellow highlight stays yellow on a dark note (%.3f %.3f %.3f)\n", red, green, blue), gFailures++;

	NVSearchHighlightPaint(0.945, 0.702, 0.702, 0.40, 0.965, 0.948, 0.949, &red, &green, &blue, &whiteInk);
	if (NVSearchHighlightSaturation(red, green, blue) < 0.20 || whiteInk)
		fprintf(stderr, "FAIL a light note still gets a tint of the highlight (%.3f %.3f %.3f sat=%.3f)\n",
				red, green, blue, NVSearchHighlightSaturation(red, green, blue)), gFailures++;

	/* Translucent pink on window gray used to flatten to a gray bar with black
	   ink. Keep the highlighter color so the chip stays readable. */
	NVSearchHighlightPaint(0.945, 0.702, 0.702, 0.35, 0.93, 0.93, 0.93, &red, &green, &blue, &whiteInk);
	if (NVSearchHighlightSaturation(red, green, blue) < 0.20 || whiteInk)
		fprintf(stderr, "FAIL translucent highlight stays colored on a light gray note (%.3f %.3f %.3f sat=%.3f)\n",
				red, green, blue, NVSearchHighlightSaturation(red, green, blue)), gFailures++;

	NVSearchHighlightPaint(0.871, 0.930, 0.110, 0.45, 0.93, 0.93, 0.93, &red, &green, &blue, &whiteInk);
	if (NVSearchHighlightSaturation(red, green, blue) < 0.30 || green < red || whiteInk)
		fprintf(stderr, "FAIL yellow highlight stays yellow on a light gray note (%.3f %.3f %.3f)\n", red, green, blue), gFailures++;

	NVSearchHighlightPaint(0.15, 0.12, 0.25, 1.0, 1, 1, 1, &red, &green, &blue, &whiteInk);
	if (!whiteInk)
		fprintf(stderr, "FAIL a dark highlight uses light text\n"), gFailures++;

	[pool drain];
	if (gFailures) {
		fprintf(stderr, "%d failed\n", gFailures);
		return 1;
	}
	printf("test-ok\n");
	return 0;
}
