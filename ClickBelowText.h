#import <Foundation/Foundation.h>

/* A click below the last typeset line, or on the blank line AppKit draws after
   a trailing newline, belongs at the end of the text. Nearest-glyph hit testing
   puts that click on the line above, and the column is arbitrary when the last
   line has text. A line that only contains spaces is still a real line: a click
   on it stays on it, and a click past its bottom goes to the end. */
static inline BOOL NVClickIsPastLastLine(CGFloat containerY, CGFloat lastLineMaxY, BOOL hasExtraLine, CGFloat extraLineMinY) {
	if (hasExtraLine && containerY >= extraLineMinY)
		return YES;
	return containerY >= lastLineMaxY;
}
