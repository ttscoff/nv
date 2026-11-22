#import "PreviewTruncation.h"

/* A header longer than this is not a front matter block. */
static const NSUInteger NVFrontMatterScanCap = 8192;
static const NSUInteger NVFrontMatterSingleLineMax = 100;

static BOOL NVFrontMatterFence(NSString *text, NSUInteger loc, NSUInteger len, unichar marker) {
	NSUInteger start = 0;
	NSUInteger end = len;
	while (start < end) {
		unichar c = [text characterAtIndex:loc + start];
		if (c != ' ' && c != '\t')
			break;
		start++;
	}
	while (end > start) {
		unichar c = [text characterAtIndex:loc + end - 1];
		if (c != ' ' && c != '\t')
			break;
		end--;
	}
	if (end - start < 3)
		return NO;
	NSUInteger i;
	for (i = start; i < end; i++) {
		if ([text characterAtIndex:loc + i] != marker)
			return NO;
	}
	return YES;
}

static BOOL NVFrontMatterKeyLine(NSString *text, NSUInteger loc, NSUInteger len) {
	NSUInteger colon = NSNotFound;
	NSUInteger i;
	if (!len)
		return NO;
	if ([text characterAtIndex:loc] == ' ' || [text characterAtIndex:loc] == '\t')
		return NO;
	for (i = 0; i < len; i++) {
		if ([text characterAtIndex:loc + i] == ':') {
			colon = i;
			break;
		}
	}
	if (colon == NSNotFound || colon == 0 || colon + 1 >= len)
		return NO;
	unichar after = [text characterAtIndex:loc + colon + 1];
	if (after != ' ' && after != '\t')
		return NO;

	NSUInteger keyEnd = colon;
	while (keyEnd > 0) {
		unichar c = [text characterAtIndex:loc + keyEnd - 1];
		if (c != ' ' && c != '\t')
			break;
		keyEnd--;
	}
	if (!keyEnd)
		return NO;
	for (i = 0; i < keyEnd; i++) {
		unichar c = [text characterAtIndex:loc + i];
		if (c >= 128)
			continue;
		BOOL ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == ' ' || c == '-' || c == '_';
		if (!ok)
			return NO;
	}
	for (i = colon + 1; i < len; i++) {
		unichar c = [text characterAtIndex:loc + i];
		if (c != ' ' && c != '\t')
			return YES;
	}
	return NO;
}

static NSUInteger NVFrontMatterLineContentEnd(NSString *text, NSRange line, BOOL *blank) {
	NSUInteger contentEnd = NSMaxRange(line);
	while (contentEnd > line.location) {
		unichar c = [text characterAtIndex:contentEnd - 1];
		if (c != '\n' && c != '\r')
			break;
		contentEnd--;
	}
	NSUInteger i;
	BOOL onlySpace = YES;
	for (i = line.location; i < contentEnd; i++) {
		unichar c = [text characterAtIndex:i];
		if (c != ' ' && c != '\t') {
			onlySpace = NO;
			break;
		}
	}
	if (blank)
		*blank = onlySpace;
	return contentEnd;
}

static NSRange NVYAMLFrontMatterRange(NSString *text, NSUInteger index, NSUInteger length) {
	NSRange none = NSMakeRange(NSNotFound, 0);
	NSUInteger cap = MIN(length, NVFrontMatterScanCap);
	if (index >= length)
		return none;

	NSRange open = [text lineRangeForRange:NSMakeRange(index, 0)];
	BOOL blank = NO;
	NSUInteger contentEnd = NVFrontMatterLineContentEnd(text, open, &blank);
	if (blank || !NVFrontMatterFence(text, open.location, contentEnd - open.location, '-'))
		return none;

	NSUInteger scan = NSMaxRange(open);
	while (scan < length && scan < cap) {
		NSRange line = [text lineRangeForRange:NSMakeRange(scan, 0)];
		BOOL lineBlank = NO;
		NSUInteger lineContentEnd = NVFrontMatterLineContentEnd(text, line, &lineBlank);
		NSUInteger lineEnd = NSMaxRange(line);
		BOOL closes = !lineBlank && (NVFrontMatterFence(text, line.location, lineContentEnd - line.location, '-') ||
			NVFrontMatterFence(text, line.location, lineContentEnd - line.location, '.'));
		if (closes) {
			NSUInteger end = lineEnd;
			if (end < length) {
				NSRange next = [text lineRangeForRange:NSMakeRange(end, 0)];
				BOOL nextBlank = NO;
				NVFrontMatterLineContentEnd(text, next, &nextBlank);
				if (nextBlank)
					end = NSMaxRange(next);
			}
			return NSMakeRange(0, end);
		}
		if (lineEnd <= scan)
			break;
		scan = lineEnd;
	}
	return none;
}

static NSRange NVMultiMarkdownFrontMatterRange(NSString *text, NSUInteger index, NSUInteger length) {
	NSRange none = NSMakeRange(NSNotFound, 0);
	BOOL found = NO;
	NSUInteger keyLines = 0;
	NSUInteger soleLength = 0;
	NSUInteger cap = MIN(length, NVFrontMatterScanCap);

	while (index < length) {
		if (index >= cap)
			return none;
		NSRange line = [text lineRangeForRange:NSMakeRange(index, 0)];
		BOOL blank = NO;
		NSUInteger contentEnd = NVFrontMatterLineContentEnd(text, line, &blank);
		NSUInteger lineEnd = NSMaxRange(line);
		if (blank) {
			if (!found || (keyLines == 1 && soleLength > NVFrontMatterSingleLineMax))
				return none;
			return NSMakeRange(0, lineEnd);
		}
		if (!NVFrontMatterKeyLine(text, line.location, contentEnd - line.location))
			return none;
		if (!found)
			soleLength = contentEnd - line.location;
		found = YES;
		keyLines++;
		if (lineEnd <= index)
			break;
		index = lineEnd;
	}
	if (!found || (keyLines == 1 && soleLength > NVFrontMatterSingleLineMax))
		return none;
	return NSMakeRange(0, index);
}

NSRange NVFrontMatterRange(NSString *text) {
	NSUInteger length = [text length];
	NSRange none = NSMakeRange(NSNotFound, 0);
	if (!length)
		return none;

	NSUInteger index = 0;
	if ([text characterAtIndex:0] == 0xFEFF)
		index = 1;

	NSRange yaml = NVYAMLFrontMatterRange(text, index, length);
	if (yaml.location == 0 && yaml.length)
		return yaml;
	return NVMultiMarkdownFrontMatterRange(text, index, length);
}

NSUInteger NVFrontMatterHighlightPrefix(NSString *text) {
	NSUInteger length = [text length];
	if (!length)
		return 0;
	NSRange front = NVFrontMatterRange(text);
	if (front.location == 0 && front.length)
		return MIN(NSMaxRange(front), length);

	NSUInteger index = ([text characterAtIndex:0] == 0xFEFF) ? 1 : 0;
	if (index >= length)
		return 0;
	NSRange first = [text lineRangeForRange:NSMakeRange(index, 0)];
	BOOL blank = NO;
	NSUInteger contentEnd = NVFrontMatterLineContentEnd(text, first, &blank);
	NSUInteger contentLen = contentEnd - first.location;
	BOOL fence = !blank && NVFrontMatterFence(text, first.location, contentLen, '-');
	BOOL key = !blank && NVFrontMatterKeyLine(text, first.location, contentLen);
	if (!fence && !key)
		return 0;
	if (fence)
		return MIN(length, NVFrontMatterScanCap);

	NSUInteger cap = MIN(length, NVFrontMatterScanCap);
	NSUInteger scan = NSMaxRange(first);
	while (scan < cap) {
		NSRange line = [text lineRangeForRange:NSMakeRange(scan, 0)];
		BOOL lineBlank = NO;
		NVFrontMatterLineContentEnd(text, line, &lineBlank);
		NSUInteger lineEnd = NSMaxRange(line);
		if (lineBlank)
			return MIN(lineEnd, length);
		if (lineEnd <= scan)
			break;
		scan = lineEnd;
	}
	return cap;
}

/* What the list has always flattened: tab, newline, carriage return, form feed,
   end-of-text (U+0003), and the Unicode line and paragraph separators. */
static NSCharacterSet *NVPreviewBreakCharacters(void) {
	static NSCharacterSet *breaks = nil;
	if (!breaks)
		breaks = [[NSCharacterSet characterSetWithCharactersInString:@"\t\n\r\f\x03  "] retain];
	return breaks;
}

@implementation NSString (PreviewTruncation)

- (NSString *)truncatedPreviewStringOfLength:(NSUInteger)characterCount {
	NSString *body = self;
	NSRange front = NVFrontMatterRange(self);
	if (front.location == 0 && front.length && NSMaxRange(front) <= [self length])
		body = [self substringFromIndex:NSMaxRange(front)];

	NSUInteger count = MIN(characterCount, [body length]);
	if (!count)
		return @"";

	NSRange range = [body rangeOfComposedCharacterSequencesForRange:NSMakeRange(0, count)];
	NSMutableString *preview = [[body substringWithRange:range] mutableCopy];

	NSCharacterSet *breaks = NVPreviewBreakCharacters();
	NSRange found = [preview rangeOfCharacterFromSet:breaks];
	while (found.location != NSNotFound) {
		[preview replaceCharactersInRange:found withString:@" "];
		NSUInteger next = found.location + 1;
		if (next >= [preview length])
			break;
		found = [preview rangeOfCharacterFromSet:breaks options:0 range:NSMakeRange(next, [preview length] - next)];
	}
	return [preview autorelease];
}

@end
