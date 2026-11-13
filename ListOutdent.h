#import <Foundation/Foundation.h>
#import <string.h>

/* After outdent removes one indent level, the line joins whatever list is
   already running at the indent it landed on. A bullet that lands on
   "1. 2." becomes the next number. A number that lands on a bullet becomes
   that bullet. The caller passes the note as it reads after the indent is
   gone, and replaces the returned range with the returned marker. */

static inline BOOL NVOutdentIsBullet(unichar character) {
	return character == '-' || character == '+' || character == '*' || character == '!' ||
		character == 0x2022 || character == 0x2014 || character == 0x2013;
}

typedef struct {
	BOOL blank;
	BOOL present;
	BOOL numbered;
	unichar kind;
	NSInteger number;
	NSUInteger width;
	NSRange marker;
} NVOutdentLine;

static inline NVOutdentLine NVOutdentParseLine(NSString *line, NSUInteger tabWidth) {
	NVOutdentLine parsed;
	memset(&parsed, 0, sizeof(parsed));
	parsed.marker = NSMakeRange(NSNotFound, 0);
	NSUInteger contentLength = line ? [line length] : 0;
	NSCharacterSet *newlines = [NSCharacterSet newlineCharacterSet];
	while (contentLength && [newlines characterIsMember:[line characterAtIndex:contentLength - 1]])
		contentLength--;
	if (!contentLength || !tabWidth) {
		parsed.blank = YES;
		return parsed;
	}

	NSUInteger index = 0;
	NSUInteger width = 0;
	while (index < contentLength) {
		unichar character = [line characterAtIndex:index];
		if (character == '\t')
			width += tabWidth;
		else if (character == ' ')
			width += 1;
		else
			break;
		index++;
	}
	parsed.width = width;
	if (index >= contentLength) {
		parsed.blank = YES;
		return parsed;
	}

	NSCharacterSet *whitespace = [NSCharacterSet whitespaceCharacterSet];
	NSCharacterSet *digits = [NSCharacterSet decimalDigitCharacterSet];
	unichar marker = [line characterAtIndex:index];
	NSUInteger markerEnd = NSNotFound;
	if (NVOutdentIsBullet(marker) && index + 1 < contentLength && [line characterAtIndex:index + 1] != marker &&
		[whitespace characterIsMember:[line characterAtIndex:index + 1]]) {
		parsed.kind = marker;
		markerEnd = index + 1;
	} else if ([digits characterIsMember:marker]) {
		NSUInteger end = index;
		while (end < contentLength && [digits characterIsMember:[line characterAtIndex:end]])
			end++;
		if (end < contentLength && ([line characterAtIndex:end] == '.' || [line characterAtIndex:end] == ')') &&
			end + 1 < contentLength && [whitespace characterIsMember:[line characterAtIndex:end + 1]]) {
			parsed.numbered = YES;
			parsed.kind = [line characterAtIndex:end];
			parsed.number = [[line substringWithRange:NSMakeRange(index, end - index)] integerValue];
			markerEnd = end + 1;
		}
	}
	if (markerEnd == NSNotFound)
		return parsed;
	while (markerEnd < contentLength && [whitespace characterIsMember:[line characterAtIndex:markerEnd]])
		markerEnd++;
	parsed.present = YES;
	parsed.marker = NSMakeRange(index, markerEnd - index);
	return parsed;
}

static inline NSString *NVListMarkerAfterOutdent(NSString *text, NSRange lineRange, NSUInteger tabWidth, NSRange *markerRange) {
	if (markerRange)
		*markerRange = NSMakeRange(NSNotFound, 0);
	if (!text || !tabWidth || lineRange.location > [text length] || NSMaxRange(lineRange) > [text length])
		return nil;
	NVOutdentLine current = NVOutdentParseLine([text substringWithRange:lineRange], tabWidth);
	if (!current.present)
		return nil;

	NSUInteger index = lineRange.location;
	while (index > 0) {
		NSRange previousRange = [text lineRangeForRange:NSMakeRange(index - 1, 0)];
		if (previousRange.location >= index)
			break;
		NVOutdentLine previous = NVOutdentParseLine([text substringWithRange:previousRange], tabWidth);
		index = previousRange.location;
		if (previous.blank || (previous.present && previous.width > current.width))
			continue;
		if (!previous.present || previous.width < current.width)
			return nil;
		if (previous.numbered == current.numbered && previous.kind == current.kind)
			return nil;
		if (markerRange)
			*markerRange = current.marker;
		if (previous.numbered) {
			NSInteger next = previous.number + 1;
			if (next < 1)
				next = 1;
			return [NSString stringWithFormat:@"%ld%C ", (long)next, previous.kind];
		}
		return [NSString stringWithFormat:@"%C ", previous.kind];
	}
	return nil;
}
