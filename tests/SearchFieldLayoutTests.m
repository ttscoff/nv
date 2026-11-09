#import <Foundation/Foundation.h>
#import <stdio.h>
#import "SearchFieldLayout.h"

static int gFailures = 0;

static void expectEdge(CGFloat titlebarMaxX, CGFloat contentMaxX, CGFloat windowMaxX, CGFloat want, const char *what) {
	CGFloat got = NVSearchFieldRightEdge(titlebarMaxX, contentMaxX, windowMaxX);
	if (got == want)
		return;
	fprintf(stderr, "FAIL %s\n  got: %.1f\n  want: %.1f\n", what, got, want);
	gFailures++;
}

static void expectWidth(CGFloat minX, CGFloat width, CGFloat targetMaxX, CGFloat want, const char *what) {
	CGFloat got = NVFullBleedWidth(minX, width, targetMaxX);
	if (got == want)
		return;
	fprintf(stderr, "FAIL %s\n  got: %.1f\n  want: %.1f\n", what, got, want);
	gFailures++;
}

int main(void) {
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

	/* Growing a tile: the titlebar view is still the width from when the
	   window was tiled, and the content view is already the new width. */
	expectEdge(1026.0, 1206.0, 1206.0, 1206.0, "growing a tile follows the window");

	/* Shrinking a tile: the titlebar view is still the previous, wider size. */
	expectEdge(1206.0, 946.0, 946.0, 946.0, "shrinking a tile follows the window");

	expectEdge(900.0, 900.0, 900.0, 900.0, "a settled window uses the shared edge");

	/* Both views keep the width from when the window was created. The window
	   frame is already the new width, and the field has to follow that. */
	expectEdge(800.0, 800.0, 1100.0, 1100.0, "both views still at the creation width follow the window frame");

	/* The views are still the previous, wider size. Follow the window so the
	   field does not hang past the frame. */
	expectEdge(1200.0, 1200.0, 900.0, 900.0, "views still at the previous width follow a smaller window");

	expectEdge(800.0, 1000.0, 0.0, 1000.0, "without a window frame, use the wider view");

	expectWidth(0.0, 800.0, 1100.0, 1100.0, "a full-bleed bar shorter than the window stretches");
	expectWidth(0.0, 1100.0, 1100.0, 1100.0, "a full-bleed bar that already matches stays");
	expectWidth(40.0, 100.0, 1100.0, 100.0, "a view that is not full bleed stays put");
	expectWidth(0.0, 1200.0, 1100.0, 1200.0, "a bar wider than the target is not shrunk");

	[pool drain];
	if (!gFailures)
		fprintf(stderr, "test-ok\n");
	return gFailures ? 1 : 0;
}
