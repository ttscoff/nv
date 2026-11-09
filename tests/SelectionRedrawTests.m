#import <Foundation/Foundation.h>
#import <stdio.h>
#import "SelectionRedraw.h"

static int gFailures = 0;

static void expectRect(NSRect got, NSRect want, const char *what) {
	if (fabs(got.origin.x - want.origin.x) > 0.001 || fabs(got.origin.y - want.origin.y) > 0.001 ||
		fabs(got.size.width - want.size.width) > 0.001 || fabs(got.size.height - want.size.height) > 0.001) {
		fprintf(stderr, "FAIL %s (got {{%.3f, %.3f}, {%.3f, %.3f}})\n", what,
				got.origin.x, got.origin.y, got.size.width, got.size.height);
		gFailures++;
	}
}

int main(void) {
	NSRect line = NSMakeRect(8, 43.669921875, 420, 13.8349609375);
	NSRect redraw = NVTextRedrawRect(line);
	expectRect(redraw, NSMakeRect(8, 42.669921875, 420, 15.8349609375),
			   "a fractional line box grows by a point above and below");
	expectRect(NVTextRedrawRect(NSZeroRect), NSZeroRect, "an empty rect stays empty");
	if (!gFailures)
		printf("test-ok\n");
	return gFailures ? 1 : 0;
}
