#import <Foundation/Foundation.h>

/* The Convert to Markdown button sits in the open space under the
   "confirm deletion" checkbox. That checkbox is not at the same height in
   every localization, and a rounded button's fitted height is larger on
   macOS 15 than the gap under the English checkbox. Shift controls that
   sit at or above the checkbox up by *shiftOut, limited by upperSlack
   (the room between those controls and the top of the storage view), and
   return the button frame in the view's coordinates after that shift.
   The button stays below the checkbox. */
static inline NSRect NVMarkdownConversionButtonFrame(NSRect checkboxFrame, CGFloat buttonWidth, CGFloat buttonHeight, CGFloat upperSlack, CGFloat *shiftOut) {
	const CGFloat gap = 8.0;
	const CGFloat margin = 8.0;
	CGFloat shift = (margin + buttonHeight + gap) - NSMinY(checkboxFrame);
	if (shift < 0.0)
		shift = 0.0;
	if (upperSlack < 0.0)
		upperSlack = 0.0;
	if (shift > upperSlack)
		shift = upperSlack;

	NSRect checkbox = checkboxFrame;
	checkbox.origin.y += shift;
	CGFloat top = NSMinY(checkbox) - gap;
	NSRect frame = NSMakeRect(NSMinX(checkbox), top - buttonHeight, buttonWidth, buttonHeight);
	if (frame.origin.y < margin) {
		frame.origin.y = margin;
		frame.size.height = top - margin;
	}
	if (frame.size.height < 0.0)
		frame.size.height = 0.0;
	if (shiftOut)
		*shiftOut = shift;
	return frame;
}
