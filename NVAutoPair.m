#import "NVAutoPair.h"

NSInteger NVPairedCharacterCode(unichar character, NSString **matchString) {
	switch (character) {
		case '"':
			*matchString = @"\"";
			return 1;
		case '`':
			*matchString = @"`";
			return 1;
		case '(':
			*matchString = @")";
			return 2;
		case '[':
			*matchString = @"]";
			return 2;
		case '{':
			*matchString = @"}";
			return 2;
		case ')':
			*matchString = @"(";
			return 0;
		case ']':
			*matchString = @"[";
			return 0;
		case '}':
			*matchString = @"{";
			return 0;
		case '=':
			*matchString = @"=";
			return 3;
		case '~':
			*matchString = @"~";
			return 3;
		default:
			return -1;
	}
}

NSString *NVAutoPairSelectionAffix(NSString *insertString) {
	if ([insertString isEqualToString:@"="])
		return @"==";
	if ([insertString isEqualToString:@"~"])
		return @"~~";
	return nil;
}

BOOL NVCharactersFormEmptyPair(unichar left, unichar right) {
	static const unichar openers[] = { '(', '[', '{', '"', '\'', '`' };
	static const unichar closers[] = { ')', ']', '}', '"', '\'', '`' };
	NSUInteger index;
	for (index = 0; index < sizeof(openers) / sizeof(unichar); index++) {
		if (openers[index] == left)
			return closers[index] == right;
	}
	return NO;
}

NVAutoPairAction NVAutoPairActionForInsert(NSString *insertString, NSString *opposite, NSInteger pairCode, NSString *before, NSString *after, NSUInteger selectionLength) {
	if (!insertString || !opposite || pairCode < 0)
		return NVAutoPairAsTyped;
	if (pairCode == 3)
		return selectionLength > 0 ? NVAutoPairWrapOrInsert : NVAutoPairAsTyped;
	if (!before)
		before = @"";
	if (!after)
		after = @"";

	NSInteger autoPair = -1;
	if ([after length] == 0 || selectionLength > 0 || ![[NSCharacterSet alphanumericCharacterSet] characterIsMember:[after characterAtIndex:0]])
		autoPair = 0;

	if (pairCode == 2 && autoPair == 0) {
		NSUInteger closingIdx = [after rangeOfString:opposite].location;
		if (closingIdx != NSNotFound) {
			NSUInteger openingIdx = [[after substringToIndex:closingIdx] rangeOfString:insertString].location;
			if (openingIdx == NSNotFound) {
				openingIdx = [before rangeOfString:insertString options:NSBackwardsSearch].location;
				if (openingIdx != NSNotFound) {
					NSUInteger closerBehind = [before rangeOfString:opposite options:NSBackwardsSearch].location;
					if (closerBehind == NSNotFound || closerBehind < openingIdx)
						closingIdx = NSNotFound;
				}
			} else {
				closingIdx = NSNotFound;
			}
		}
		if (closingIdx == NSNotFound)
			autoPair = 1;
	} else {
		NSUInteger openingIdx = [after rangeOfString:insertString].location;
		BOOL advance = NO;
		if (pairCode == 1) {
			advance = (openingIdx == 0);
			if ([insertString isEqualToString:@"`"] && selectionLength > 0) {
				autoPair = 1;
			} else if (!advance && autoPair == 0) {
				// Typing ` after ` extends a run instead of wrapping another pair, so ``` is three characters.
				BOOL backtickRun = [insertString isEqualToString:@"`"] && [before hasSuffix:@"`"];
				if (!backtickRun) {
					NSUInteger afterCount = ([[after componentsSeparatedByString:insertString] count] % 2);
					NSUInteger beforeCount = ([[before componentsSeparatedByString:insertString] count] % 2);
					if (beforeCount == afterCount)
						autoPair = 1;
				}
			}
		} else {
			advance = (openingIdx == 0);
		}
		if (advance && selectionLength == 0) {
			openingIdx = [before rangeOfString:opposite options:NSBackwardsSearch].location;
			NSUInteger closerBehind = [before rangeOfString:insertString options:NSBackwardsSearch].location;
			if (openingIdx != NSNotFound && (closerBehind == NSNotFound || openingIdx >= closerBehind))
				return NVAutoPairTypeOver;
		}
	}
	if (autoPair == 1)
		return NVAutoPairWrapOrInsert;
	return NVAutoPairAsTyped;
}
