#import <Foundation/Foundation.h>
#import <string.h>

/* A GitHub task line is indent, then -, *, +, or a number, then an optional
   [ ], [x], or [X]. The shortcut, the click, and the strikethrough all use
   this parse so they agree on which character is the checkbox. */

typedef struct {
	NSRange range;
	NSString *replacement;
} NVTaskEdit;

typedef struct {
	NSUInteger contentLength;
	NSUInteger indentEnd;
	NSUInteger markerEnd;
	NSUInteger contentStart;
	NSUInteger markIndex;
	BOOL hasMarker;
	BOOL hasCheckbox;
	BOOL checked;
} NVTaskParts;

static inline NVTaskEdit NVTaskNoEdit(void) {
	NVTaskEdit edit;
	edit.range = NSMakeRange(NSNotFound, 0);
	edit.replacement = nil;
	return edit;
}

static inline NVTaskEdit NVTaskMakeEdit(NSUInteger location, NSUInteger length, NSString *replacement) {
	NVTaskEdit edit;
	edit.range = NSMakeRange(location, length);
	edit.replacement = replacement;
	return edit;
}

static inline BOOL NVTaskIsIndent(unichar character) {
	return character == ' ' || character == '\t';
}

static inline NSUInteger NVTaskContentLength(NSString *line) {
	NSUInteger length = line ? [line length] : 0;
	NSCharacterSet *newlines = [NSCharacterSet newlineCharacterSet];
	while (length && [newlines characterIsMember:[line characterAtIndex:length - 1]])
		length--;
	return length;
}

static inline NVTaskParts NVTaskParseLine(NSString *line) {
	NVTaskParts parts;
	memset(&parts, 0, sizeof(parts));
	parts.markerEnd = NSNotFound;
	parts.markIndex = NSNotFound;
	parts.contentLength = NVTaskContentLength(line);
	NSUInteger index = 0;
	while (index < parts.contentLength && NVTaskIsIndent([line characterAtIndex:index]))
		index++;
	parts.indentEnd = index;
	if (index >= parts.contentLength)
		return parts;

	unichar marker = [line characterAtIndex:index];
	NSUInteger markerEnd = NSNotFound;
	if ((marker == '-' || marker == '*' || marker == '+') && index + 1 < parts.contentLength &&
		[[NSCharacterSet whitespaceCharacterSet] characterIsMember:[line characterAtIndex:index + 1]]) {
		markerEnd = index + 1;
	} else if ([[NSCharacterSet decimalDigitCharacterSet] characterIsMember:marker]) {
		NSUInteger end = index;
		while (end < parts.contentLength && [[NSCharacterSet decimalDigitCharacterSet] characterIsMember:[line characterAtIndex:end]])
			end++;
		if (end < parts.contentLength && ([line characterAtIndex:end] == '.' || [line characterAtIndex:end] == ')') &&
			end + 1 < parts.contentLength && [[NSCharacterSet whitespaceCharacterSet] characterIsMember:[line characterAtIndex:end + 1]])
			markerEnd = end + 1;
	}
	if (markerEnd == NSNotFound)
		return parts;

	parts.hasMarker = YES;
	parts.markerEnd = markerEnd;
	NSUInteger cursor = markerEnd;
	while (cursor < parts.contentLength && [[NSCharacterSet whitespaceCharacterSet] characterIsMember:[line characterAtIndex:cursor]])
		cursor++;
	if (cursor + 2 < parts.contentLength && [line characterAtIndex:cursor] == '[') {
		unichar mark = [line characterAtIndex:cursor + 1];
		if ((mark == ' ' || mark == 'x' || mark == 'X') && [line characterAtIndex:cursor + 2] == ']') {
			parts.hasCheckbox = YES;
			parts.checked = mark != ' ';
			parts.markIndex = cursor + 1;
			cursor += 3;
			while (cursor < parts.contentLength && [[NSCharacterSet whitespaceCharacterSet] characterIsMember:[line characterAtIndex:cursor]])
				cursor++;
		}
	}
	parts.contentStart = cursor;
	return parts;
}

static inline NVTaskEdit NVTaskShortcutEdit(NSString *line) {
	NVTaskParts parts = NVTaskParseLine(line);
	if (!parts.hasMarker)
		return NVTaskMakeEdit(parts.indentEnd, 0, @"- [ ] ");
	if (parts.hasCheckbox) {
		NSUInteger end = parts.markIndex + 2;
		while (end < parts.contentLength && [[NSCharacterSet whitespaceCharacterSet] characterIsMember:[line characterAtIndex:end]])
			end++;
		return NVTaskMakeEdit(parts.markerEnd, end - parts.markerEnd, @" ");
	}
	return NVTaskMakeEdit(parts.markerEnd, parts.contentStart - parts.markerEnd, @" [ ] ");
}

static inline NVTaskEdit NVTaskToggleEdit(NSString *line, NSUInteger indexInLine) {
	NVTaskParts parts = NVTaskParseLine(line);
	if (!parts.hasCheckbox || indexInLine != parts.markIndex)
		return NVTaskNoEdit();
	return NVTaskMakeEdit(parts.markIndex, 1, parts.checked ? @" " : @"x");
}

static inline NSRange NVTaskDoneStrikethroughRange(NSString *line) {
	NVTaskParts parts = NVTaskParseLine(line);
	if (!parts.hasCheckbox || !parts.checked || parts.contentStart >= parts.contentLength)
		return NSMakeRange(NSNotFound, 0);
	NSUInteger end = parts.contentLength;
	NSRange search = NSMakeRange(parts.contentStart, parts.contentLength - parts.contentStart);
	NSRange doneTag = [[line substringWithRange:search] rangeOfString:@" @done"];
	if (doneTag.location != NSNotFound)
		end = parts.contentStart + doneTag.location;
	if (end <= parts.contentStart)
		return NSMakeRange(NSNotFound, 0);
	return NSMakeRange(parts.contentStart, end - parts.contentStart);
}

/* Appended after the marker a continued list already copies. A completed
   task starts the next line open. An empty checkbox does not continue. */
static inline NSString *NVTaskContinuationSuffix(NSString *line) {
	NVTaskParts parts = NVTaskParseLine(line);
	if (!parts.hasCheckbox || parts.contentStart >= parts.contentLength)
		return nil;
	return @"[ ] ";
}

static inline BOOL NVTaskLineIsEmptyItem(NSString *line) {
	NVTaskParts parts = NVTaskParseLine(line);
	return parts.hasCheckbox && parts.contentStart >= parts.contentLength;
}

static inline NSRange NVTaskCheckedPrefixRange(NSString *line) {
	NVTaskParts parts = NVTaskParseLine(line);
	if (!parts.hasCheckbox || !parts.checked || parts.contentStart == 0)
		return NSMakeRange(NSNotFound, 0);
	return NSMakeRange(0, parts.contentStart);
}

static inline BOOL NVTaskFenceInfo(NSString *line, NSUInteger contentLength, unichar *outChar, NSUInteger *outLength) {
	NSUInteger index = 0;
	while (index < contentLength && NVTaskIsIndent([line characterAtIndex:index]))
		index++;
	if (contentLength < index + 3)
		return NO;
	unichar fence = [line characterAtIndex:index];
	if (fence != '`' && fence != '~')
		return NO;
	NSUInteger start = index;
	while (index < contentLength && [line characterAtIndex:index] == fence)
		index++;
	if (index - start < 3)
		return NO;
	if (outChar)
		*outChar = fence;
	if (outLength)
		*outLength = index - start;
	while (index < contentLength && NVTaskIsIndent([line characterAtIndex:index]))
		index++;
	while (index < contentLength && [[NSCharacterSet alphanumericCharacterSet] characterIsMember:[line characterAtIndex:index]])
		index++;
	return index == contentLength;
}

static inline BOOL NVTaskIndexIsInsideFence(NSString *text, NSUInteger index) {
	if (!text)
		return NO;
	NSUInteger length = [text length];
	if (!length || index > length)
		return NO;
	BOOL open = NO;
	unichar openChar = 0;
	NSUInteger openLength = 0;
	NSUInteger scan = 0;
	while (scan <= length) {
		NSRange lineRange = [text lineRangeForRange:NSMakeRange(MIN(scan, length), 0)];
		NSString *line = [text substringWithRange:lineRange];
		NSUInteger contentLength = NVTaskContentLength(line);
		unichar fenceChar = 0;
		NSUInteger fenceLength = 0;
		BOOL isFence = NVTaskFenceInfo(line, contentLength, &fenceChar, &fenceLength);
		BOOL inside = open;
		if (isFence) {
			inside = YES;
			if (!open) {
				open = YES;
				openChar = fenceChar;
				openLength = fenceLength;
			} else if (fenceChar == openChar && fenceLength >= openLength) {
				open = NO;
				openChar = 0;
				openLength = 0;
			}
		}
		BOOL onLine = index >= lineRange.location && (index < NSMaxRange(lineRange) || (index == length && NSMaxRange(lineRange) == length));
		if (onLine)
			return inside;
		if (NSMaxRange(lineRange) <= scan)
			break;
		scan = NSMaxRange(lineRange);
		if (scan >= length)
			break;
	}
	return NO;
}

/* The checkbox's ] sits where Markdown link completion looks for a link.
   Tab and Backtab should indent the task instead of inserting [] or (http://). */
static inline BOOL NVTaskCursorBelongsToCheckbox(NSString *line, NSUInteger cursorInLine) {
	NVTaskParts parts = NVTaskParseLine(line);
	if (!parts.hasCheckbox || parts.markIndex == NSNotFound)
		return NO;
	NSUInteger closer = parts.markIndex + 1;
	if (cursorInLine < parts.markIndex || cursorInLine > parts.contentLength)
		return NO;
	if (cursorInLine <= closer)
		return YES;
	NSUInteger index = closer + 1;
	while (index < cursorInLine) {
		unichar character = [line characterAtIndex:index];
		if (character != ' ' && character != '\t')
			return NO;
		index++;
	}
	return YES;
}
