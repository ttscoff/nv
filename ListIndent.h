/* The caret past a trailing newline sits on the blank line that newline
   opened. Stepping back one character reads the line above, and Tab indents
   that line. A caret at the end of a line with no trailing newline is still
   on that line. */
static inline NSRange NVLineRangeForCaret(NSString *text, NSUInteger location) {
	NSUInteger length = text ? [text length] : 0;
	if (location > length)
		location = length;
	if (!length)
		return NSMakeRange(0, 0);
	if (location == length && [[NSCharacterSet newlineCharacterSet] characterIsMember:[text characterAtIndex:length - 1]])
		return NSMakeRange(length, 0);
	if (location == length)
		location = length - 1;
	return [text lineRangeForRange:NSMakeRange(location, 0)];
}
