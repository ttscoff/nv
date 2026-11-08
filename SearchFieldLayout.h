#import <Foundation/Foundation.h>

/* Right edge of the search field in window coordinates, before the share
   button inset. titlebarMaxX is where the titlebar view currently ends.
   contentMaxX is where the content view currently ends.

   While a tiled window is resizing, the titlebar view keeps the width it
   had when the window was tiled. The content view already has the new
   width, and that is the window's right edge. */
static inline CGFloat NVSearchFieldRightEdge(CGFloat titlebarMaxX, CGFloat contentMaxX) {
	(void)titlebarMaxX;
	return contentMaxX;
}
