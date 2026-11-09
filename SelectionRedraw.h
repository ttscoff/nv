#import <Foundation/Foundation.h>

/* Monaco's line box is 13.835pt, so its edges fall between device pixels. The
   selection fill covers the partial pixel above the box and the dirty rect
   does not. Changing the selection leaves that pixel as a hairline. Grow the
   rect by a point so the partial pixel is painted again. */
static inline NSRect NVTextRedrawRect(NSRect dirtyRect) {
	if (NSIsEmptyRect(dirtyRect))
		return dirtyRect;
	return NSInsetRect(dirtyRect, 0.0, -1.0);
}
