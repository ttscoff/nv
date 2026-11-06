#import <Cocoa/Cocoa.h>
#import <stdio.h>
#import "MarkdownButtonLayout.h"

static int gFailures = 0;

static void expectTrue(BOOL cond, const char *what) {
	if (!cond) {
		fprintf(stderr, "FAIL %s\n", what);
		gFailures++;
	}
}

static void expectClearsCheckbox(NSRect checkbox, CGFloat height, CGFloat slack, const char *what) {
	CGFloat shift = -1.0;
	NSRect button = NVMarkdownConversionButtonFrame(checkbox, 180.0, height, slack, &shift);
	NSRect moved = checkbox;
	moved.origin.y += shift;
	expectTrue(shift >= 0.0 && shift <= slack + 0.01, what);
	expectTrue(NSMinY(button) >= 8.0 - 0.01, what);
	expectTrue(NSMaxY(button) <= NSMinY(moved) - 8.0 + 0.01, what);
	expectTrue(!NSIntersectsRect(button, moved), what);
	expectTrue(NSMinX(button) == NSMinX(checkbox), what);
}

int main(void) {
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

	NSRect english = NSMakeRect(43, 51, 312, 18);
	NSRect french = NSMakeRect(57, 44, 351, 18);
	NSRect portuguese = NSMakeRect(43, 42, 312, 18);
	NSRect german = NSMakeRect(82, 35, 301, 18);
	NSRect chinese = NSMakeRect(43, 33, 312, 18);
	CGFloat heights[] = {24.0, 28.0, 32.0, 40.0, 48.0};
	NSRect checkboxes[] = {english, french, portuguese, german, chinese};
	int i, h;
	for (i = 0; i < 5; i++) {
		for (h = 0; h < 5; h++)
			expectClearsCheckbox(checkboxes[i], heights[h], 20.0, "button stays under the deletion checkbox");
	}

	/* English has almost no room above the storage label. A tall macOS 15
	   button still has to clear the checkbox without leaving the view. */
	expectClearsCheckbox(english, 48.0, 2.0, "tall button in the English storage pane");
	expectClearsCheckbox(chinese, 32.0, 20.0, "Chinese checkbox sits lower");

	CGFloat shift = 0.0;
	NSRect fitted = NVMarkdownConversionButtonFrame(english, 180.0, 24.0, 2.0, &shift);
	expectTrue(shift == 0.0, "English single-line button does not move the checkbox");
	expectTrue(NSMinY(fitted) > 14.0, "English button is not stuck at the old hardcoded origin");

	NSRect oldButton = NSMakeRect(42, 14, 180, 32);
	expectTrue(NSIntersectsRect(oldButton, chinese), "the old frame covers the Chinese checkbox");
	expectTrue(NSIntersectsRect(oldButton, german), "the old frame covers the German checkbox");

	[pool drain];
	return gFailures ? 1 : 0;
}
