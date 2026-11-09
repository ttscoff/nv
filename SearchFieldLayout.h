#import <Foundation/Foundation.h>

/* Right edge of the search field in window coordinates, before the share
   button inset. titlebarMaxX and contentMaxX are where those views currently
   end. windowMaxX is the window frame's right edge in the same coordinates.

   Both views can keep the width they had when the window was created, while
   the window frame is already the new size. The frame is the edge to follow.
   When the frame has not been measured, use whichever view is wider. */
static inline CGFloat NVSearchFieldRightEdge(CGFloat titlebarMaxX, CGFloat contentMaxX, CGFloat windowMaxX) {
	if (windowMaxX >= 1.0)
		return windowMaxX;
	return MAX(titlebarMaxX, contentMaxX);
}

/* Width to give a view that starts at the left edge so it reaches targetMaxX.
   A view inset from the left is left alone, and a view that is already wider
   is not shrunk. */
static inline CGFloat NVFullBleedWidth(CGFloat minX, CGFloat width, CGFloat targetMaxX) {
	if (fabs(minX) < 1.0 && width + 0.5 < targetMaxX)
		return targetMaxX - minX;
	return width;
}
