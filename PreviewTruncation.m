#import "PreviewTruncation.h"

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
	NSUInteger count = MIN(characterCount, [self length]);
	if (!count)
		return @"";

	NSRange range = [self rangeOfComposedCharacterSequencesForRange:NSMakeRange(0, count)];
	NSMutableString *preview = [[self substringWithRange:range] mutableCopy];

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
