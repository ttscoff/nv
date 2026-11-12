#import <Foundation/Foundation.h>

/* Syntax coloring scans the whole note on each edit. Past this length the
   editor stays plain text. The count is UTF-16 characters, about 300KB of
   typical notes. */
enum { NVMarkdownSyntaxHighlightLimit = 300 * 1024 };

static inline BOOL NVMarkdownSyntaxHighlightEnabledForLength(NSUInteger length) {
	return length <= NVMarkdownSyntaxHighlightLimit;
}

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

static inline NSArray *NVMarkdownHighlightSpansInRange(NSString *string, NSRange range) {
	NSUInteger length = [string length];
	if (range.location > length)
		return [NSArray array];
	if (NSMaxRange(range) > length)
		range.length = length - range.location;
	if (!range.length)
		return [NSArray array];
	NSMutableArray *spans = [NSMutableArray array];
	BOOL inCodeBlock = NO;
	BOOL inInlineCode = NO;
	BOOL inIndentedCode = NO;
	NSUInteger index = range.location;
	NSUInteger end = NSMaxRange(range);
	while (index < end) {
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
		BOOL opening = index + 1 < end && character == '=' && [string characterAtIndex:index + 1] == '=' &&
			after != 0 && after != '=' && after != '}' && after != '\n' && after != '\r' &&
			after != ' ' && after != '\t' && after != '+' &&
			!precededByEquals && !precededByPlus && !critic;
		if (!inCodeBlock && !inInlineCode && !inIndentedCode && opening) {
			NSUInteger close = index + 2;
			while (close < end) {
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
			if (close + 1 < end && [string characterAtIndex:close] == '=' && [string characterAtIndex:close + 1] == '=' &&
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

static inline NSArray *NVMarkdownHighlightSpans(NSString *string) {
	return NVMarkdownHighlightSpansInRange(string, NSMakeRange(0, [string length]));
}

static inline BOOL NVMarkdownLineIsFence(NSString *string, NSRange line, unichar *marker) {
	NSUInteger i = line.location;
	NSUInteger end = NSMaxRange(line);
	NSUInteger length = [string length];
	if (end > length)
		end = length;
	while (i < end) {
		unichar character = [string characterAtIndex:i];
		if (character == '\n' || character == '\r')
			return NO;
		if (character != ' ' && character != '\t')
			break;
		i++;
	}
	if (i + 2 >= end)
		return NO;
	unichar fence = [string characterAtIndex:i];
	if ((fence != '`' && fence != '~') || [string characterAtIndex:i + 1] != fence || [string characterAtIndex:i + 2] != fence)
		return NO;
	if (marker)
		*marker = fence;
	return YES;
}

static inline BOOL NVMarkdownRangesTouch(NSRange a, NSRange b) {
	if (!a.length && !b.length)
		return a.location == b.location;
	if (!a.length)
		return a.location >= b.location && a.location <= NSMaxRange(b);
	if (!b.length)
		return b.location >= a.location && b.location <= NSMaxRange(a);
	return NSIntersectionRange(a, b).length > 0;
}

/* A link definition lives on its own line. Changing one can recolor a reference anywhere in the note. */
static inline BOOL NVMarkdownRangeHasLinkDefinition(NSString *string, NSRange lines) {
	NSUInteger length = [string length];
	NSUInteger index = lines.location;
	NSUInteger end = NSMaxRange(lines);
	if (index > length)
		return NO;
	if (end > length)
		end = length;
	while (index < end) {
		NSRange line = [string lineRangeForRange:NSMakeRange(index, 0)];
		NSUInteger i = line.location;
		NSUInteger lineEnd = MIN(NSMaxRange(line), length);
		NSUInteger spaces = 0;
		while (i < lineEnd && spaces < 4) {
			unichar character = [string characterAtIndex:i];
			if (character != ' ' && character != '\t')
				break;
			spaces++;
			i++;
		}
		if (i < lineEnd && [string characterAtIndex:i] == '[') {
			NSRange rest = NSMakeRange(i, lineEnd - i);
			if ([string rangeOfString:@"]:" options:0 range:rest].location != NSNotFound)
				return YES;
		}
		NSUInteger next = NSMaxRange(line);
		if (next <= index)
			break;
		index = next;
	}
	return NO;
}

static inline NSRange NVMarkdownUnionCriticSpan(NSString *string, NSRange lines) {
	NSString *openers[5] = {@"{++", @"{--", @"{~~", @"{==", @"{>>"};
	NSString *closers[5] = {@"++}", @"--}", @"~~}", @"==}", @"<<}"};
	NSUInteger length = [string length];
	NSUInteger searchEnd = MIN(NSMaxRange(lines), length);
	NSRange search = NSMakeRange(0, searchEnd);
	NSUInteger bestLoc = NSNotFound;
	NSUInteger bestEnd = length;
	NSUInteger kind;
	for (kind = 0; kind < 5; kind++) {
		NSRange look = search;
		NSUInteger steps = 0;
		while (look.length && steps < 32) {
			NSRange found = [string rangeOfString:openers[kind] options:NSBackwardsSearch range:look];
			if (found.location == NSNotFound)
				break;
			steps++;
			NSUInteger closerFrom = NSMaxRange(found);
			NSRange closerSearch = NSMakeRange(closerFrom, length - closerFrom);
			NSRange closer = [string rangeOfString:closers[kind] options:0 range:closerSearch];
			NSUInteger spanEnd = closer.location == NSNotFound ? length : NSMaxRange(closer);
			if (spanEnd > lines.location && spanEnd <= found.location + (16 * 1024)) {
				if (bestLoc == NSNotFound || found.location > bestLoc) {
					bestLoc = found.location;
					bestEnd = spanEnd;
				}
				break;
			}
			if (!found.location)
				break;
			look.length = found.location;
		}
	}
	if (bestLoc == NSNotFound)
		return lines;
	NSRange span = NSMakeRange(bestLoc, bestEnd - bestLoc);
	if (!NVMarkdownRangesTouch(span, lines))
		return lines;
	NSRange spanLines = [string lineRangeForRange:span.length ? span : NSMakeRange(span.location, 0)];
	return NSUnionRange(lines, spanLines);
}

/* Lines touched by an edit, widened to a fenced block or a short critic span that crosses them. */
static inline NSRange NVMarkdownHighlightRangeForEdit(NSString *string, NSRange edit) {
	NSUInteger length = [string length];
	if (!length || edit.location == NSNotFound)
		return NSMakeRange(0, length);
	if (edit.location > length)
		edit = NSMakeRange(length, 0);
	else if (NSMaxRange(edit) > length)
		edit.length = length - edit.location;

	NSRange lines = [string lineRangeForRange:NSMakeRange(edit.location, edit.length)];
	BOOL hasFence = [string rangeOfString:@"```" options:0 range:NSMakeRange(0, length)].location != NSNotFound ||
		[string rangeOfString:@"~~~" options:0 range:NSMakeRange(0, length)].location != NSNotFound;
	if (hasFence) {
		unichar marker = 0;
		NSUInteger fenceStart = NSNotFound;
		NSUInteger index = 0;
		while (index < length) {
			if (!marker && index >= NSMaxRange(lines))
				break;
			NSRange line = [string lineRangeForRange:NSMakeRange(index, 0)];
			unichar lineMarker = 0;
			if (NVMarkdownLineIsFence(string, line, &lineMarker)) {
				if (!marker) {
					marker = lineMarker;
					fenceStart = line.location;
				} else if (lineMarker == marker) {
					NSRange block = NSMakeRange(fenceStart, NSMaxRange(line) - fenceStart);
					if (NVMarkdownRangesTouch(block, lines))
						lines = NSUnionRange(lines, block);
					marker = 0;
					fenceStart = NSNotFound;
				}
			}
			NSUInteger next = NSMaxRange(line);
			if (next <= index)
				break;
			index = next;
		}
		if (marker && fenceStart != NSNotFound) {
			NSRange block = NSMakeRange(fenceStart, length - fenceStart);
			if (NVMarkdownRangesTouch(block, lines))
				lines = NSUnionRange(lines, block);
		}
	}
	if ([string rangeOfString:@"{" options:0 range:NSMakeRange(0, MIN(NSMaxRange(lines), length))].location != NSNotFound)
		lines = NVMarkdownUnionCriticSpan(string, lines);
	if (NVMarkdownRangeHasLinkDefinition(string, lines))
		return NSMakeRange(0, length);
	return lines;
}

/* Fold a new text change into the range waiting to be colored. `change` is the storage's changeInLength. */
static inline NSRange NVMarkdownHighlightUnionEdit(NSRange dirty, NSRange edited, NSInteger change, NSUInteger length) {
	if (edited.location == NSNotFound || edited.location > length)
		return dirty.location == NSNotFound ? NSMakeRange(0, 0) : dirty;
	if (NSMaxRange(edited) > length)
		edited.length = length - edited.location;
	if (dirty.location == NSNotFound || dirty.location > length)
		return edited;
	if (NSMaxRange(dirty) > length)
		dirty.length = length - dirty.location;

	NSRange shifted = dirty;
	if (edited.location <= dirty.location) {
		NSInteger loc = (NSInteger)dirty.location + change;
		NSInteger floor = (NSInteger)NSMaxRange(edited);
		if (loc < floor)
			loc = floor;
		if (loc < 0)
			loc = 0;
		if (loc > (NSInteger)length)
			loc = (NSInteger)length;
		shifted.location = (NSUInteger)loc;
		if (shifted.location > length)
			shifted.location = length;
		if (shifted.location + shifted.length > length)
			shifted.length = length - shifted.location;
	} else if (edited.location < NSMaxRange(dirty)) {
		NSInteger len = (NSInteger)dirty.length + change;
		if (len < 0)
			len = 0;
		shifted.length = (NSUInteger)len;
		if (shifted.location + shifted.length > length)
			shifted.length = length - shifted.location;
	}
	NSRange united = NSUnionRange(shifted, edited);
	if (united.location > length)
		return NSMakeRange(length, 0);
	if (NSMaxRange(united) > length)
		united.length = length - united.location;
	return united;
}
