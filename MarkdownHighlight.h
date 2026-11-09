#import <Foundation/Foundation.h>

/* ==highlight== spans, matching the preview renderer.
   The mark is skipped inside code and inside critic markup {== ==}.
   Each span is a dictionary of NSValue ranges: opener, content, closer. */

static inline unichar NVMarkdownHighlightChar(NSString *string, NSUInteger index, NSUInteger length) {
	return index < length ? [string characterAtIndex:index] : 0;
}

static inline BOOL NVMarkdownHighlightIsListMarker(NSString *string, NSUInteger index, NSUInteger length) {
	unichar character = NVMarkdownHighlightChar(string, index, length);
	if (character == '-' || character == '*' || character == '+') {
		unichar next = NVMarkdownHighlightChar(string, index + 1, length);
		return next == ' ' || next == '\t';
	}
	if (character < '0' || character > '9')
		return NO;
	while (index < length) {
		unichar digit = [string characterAtIndex:index];
		if (digit < '0' || digit > '9')
			break;
		index++;
	}
	return NVMarkdownHighlightChar(string, index, length) == '.' &&
		(NVMarkdownHighlightChar(string, index + 1, length) == ' ' ||
		 NVMarkdownHighlightChar(string, index + 1, length) == '\t');
}

static inline BOOL NVMarkdownHighlightLineIsIndentedCode(NSString *string, NSUInteger lineStart, NSUInteger length) {
	unichar first = NVMarkdownHighlightChar(string, lineStart, length);
	if (first == '\t')
		return !NVMarkdownHighlightIsListMarker(string, lineStart + 1, length);
	if (NVMarkdownHighlightChar(string, lineStart, length) != ' ' ||
		NVMarkdownHighlightChar(string, lineStart + 1, length) != ' ' ||
		NVMarkdownHighlightChar(string, lineStart + 2, length) != ' ' ||
		NVMarkdownHighlightChar(string, lineStart + 3, length) != ' ')
		return NO;
	NSUInteger content = lineStart + 4;
	while (NVMarkdownHighlightChar(string, content, length) == ' ')
		content++;
	return !NVMarkdownHighlightIsListMarker(string, content, length);
}

static inline NSArray *NVMarkdownHighlightSpans(NSString *string) {
	NSUInteger length = [string length];
	if (!length)
		return [NSArray array];
	NSMutableArray *spans = [NSMutableArray array];
	BOOL inCodeBlock = NO;
	BOOL inInlineCode = NO;
	BOOL inIndentedCode = NO;
	NSUInteger index = 0;
	while (index < length) {
		BOOL lineStart = (index == 0 || [string characterAtIndex:index - 1] == '\n' || [string characterAtIndex:index - 1] == '\r');
		if (lineStart)
			inIndentedCode = NVMarkdownHighlightLineIsIndentedCode(string, index, length);
		unichar character = [string characterAtIndex:index];
		if (character == '`') {
			BOOL fence = NVMarkdownHighlightChar(string, index + 1, length) == '`' &&
				NVMarkdownHighlightChar(string, index + 2, length) == '`';
			if (fence)
				inCodeBlock = !inCodeBlock;
			else if (!inCodeBlock)
				inInlineCode = !inInlineCode;
		}
		BOOL critic = index > 0 && [string characterAtIndex:index - 1] == '{';
		BOOL precededByEquals = index > 0 && [string characterAtIndex:index - 1] == '=';
		BOOL precededByPlus = index > 0 && [string characterAtIndex:index - 1] == '+';
		unichar after = NVMarkdownHighlightChar(string, index + 2, length);
		BOOL opening = index + 1 < length && character == '=' && [string characterAtIndex:index + 1] == '=' &&
			after != 0 && after != '=' && after != '}' && after != '\n' && after != '\r' &&
			after != ' ' && after != '\t' && after != '+' &&
			!precededByEquals && !precededByPlus && !critic;
		if (!inCodeBlock && !inInlineCode && !inIndentedCode && opening) {
			NSUInteger close = index + 2;
			while (close < length) {
				unichar mark = [string characterAtIndex:close];
				if (mark == '\n' || mark == '\r')
					break;
				if (mark == '=' && NVMarkdownHighlightChar(string, close + 1, length) == '=') {
					unichar following = NVMarkdownHighlightChar(string, close + 2, length);
					BOOL followedByEquals = following == '=';
					BOOL followedByPlus = following == '+';
					unichar before = [string characterAtIndex:close - 1];
					BOOL precededBySpace = before == ' ' || before == '\t';
					BOOL closingPrecededByPlus = before == '+';
					if (!followedByEquals && !followedByPlus && !precededBySpace && !closingPrecededByPlus)
						break;
				}
				close++;
			}
			if (close + 1 < length && [string characterAtIndex:close] == '=' && [string characterAtIndex:close + 1] == '=' &&
				NVMarkdownHighlightChar(string, close + 2, length) != '=') {
				NSUInteger contentLength = close - (index + 2);
				if (contentLength > 0) {
					[spans addObject:[NSDictionary dictionaryWithObjectsAndKeys:
						[NSValue valueWithRange:NSMakeRange(index, 2)], @"opener",
						[NSValue valueWithRange:NSMakeRange(index + 2, contentLength)], @"content",
						[NSValue valueWithRange:NSMakeRange(close, 2)], @"closer",
						nil]];
					index = close + 2;
					continue;
				}
			}
		}
		index++;
	}
	return spans;
}
