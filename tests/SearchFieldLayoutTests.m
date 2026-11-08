#import <Foundation/Foundation.h>
#import <stdio.h>
#import "SearchFieldLayout.h"

static int gFailures = 0;

static void expectEdge(CGFloat titlebarMaxX, CGFloat contentMaxX, CGFloat want, const char *what) {
	CGFloat got = NVSearchFieldRightEdge(titlebarMaxX, contentMaxX);
	if (got == want)
		return;
	fprintf(stderr, "FAIL %s\n  got: %.1f\n  want: %.1f\n", what, got, want);
	gFailures++;
}

int main(void) {
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

	/* Growing a tile: the titlebar view is still the width from when the
	   window was tiled, and the content view is already the new width. */
	expectEdge(1026.0, 1206.0, 1206.0, "growing a tile follows the content edge");

	/* Shrinking a tile: the titlebar view is still the previous, wider size. */
	expectEdge(1206.0, 946.0, 946.0, "shrinking a tile follows the content edge");

	expectEdge(900.0, 900.0, 900.0, "a settled window uses the shared edge");

	[pool drain];
	return gFailures ? 1 : 0;
}
