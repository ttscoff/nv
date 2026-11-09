#import <Foundation/Foundation.h>
#import <stdio.h>
#import "NVAutoPair.h"

static int gFailures = 0;

static void expectTrue(BOOL cond, const char *what) {
	if (!cond) {
		fprintf(stderr, "FAIL %s\n", what);
		gFailures++;
	}
}

static void expectEqual(NSString *got, NSString *want, const char *what) {
	if ((got == nil && want == nil) || (got && want && [got isEqualToString:want]))
		return;
	fprintf(stderr, "FAIL %s\n  got: %s\n  want: %s\n", what,
			got ? [got UTF8String] : "(nil)",
			want ? [want UTF8String] : "(nil)");
	gFailures++;
}

static void typeInto(NSMutableString *text, NSUInteger *cursor, NSUInteger *selection, NSString *insert) {
	NSUInteger loc = *cursor;
	NSUInteger len = *selection;
	NSString *before = [text substringToIndex:loc];
	NSString *after = [text substringFromIndex:loc];
	NSString *opposite = nil;
	NSInteger code = -1;
	if ([insert length] == 1)
		code = NVPairedCharacterCode([insert characterAtIndex:0], &opposite);
	NVAutoPairAction action = NVAutoPairAsTyped;
	if (code >= 0)
		action = NVAutoPairActionForInsert(insert, opposite, code, before, after, len);
	if (action == NVAutoPairTypeOver) {
		*cursor = loc + 1;
		return;
	}
	if (action == NVAutoPairWrapOrInsert) {
		if (len > 0) {
			NSString *prefix = insert;
			NSString *suffix = opposite;
			NSString *affix = NVAutoPairSelectionAffix(insert);
			if (affix) {
				prefix = affix;
				suffix = affix;
			}
			[text insertString:prefix atIndex:loc];
			[text insertString:suffix atIndex:loc + len + [prefix length]];
			*cursor = loc + len + [prefix length] + [suffix length];
			*selection = 0;
			return;
		}
		[text insertString:[insert stringByAppendingString:opposite] atIndex:loc];
		*cursor = loc + [insert length];
		return;
	}
	[text replaceCharactersInRange:NSMakeRange(loc, len) withString:insert];
	*cursor = loc + [insert length];
	*selection = 0;
}

static void typeBackticks(NSMutableString *text, NSUInteger *cursor, NSUInteger count) {
	NSUInteger selection = 0;
	NSUInteger index;
	for (index = 0; index < count; index++)
		typeInto(text, cursor, &selection, @"`");
}

int main(int argc, const char *argv[]) {
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	NSString *match = nil;

	expectTrue(NVPairedCharacterCode('`', &match) == 1, "backtick is a symmetric pair");
	expectEqual(match, @"`", "backtick matches itself");
	expectTrue(NVPairedCharacterCode('"', &match) == 1, "quote stays symmetric");
	expectEqual(match, @"\"", "quote matches itself");
	expectTrue(NVPairedCharacterCode('(', &match) == 2, "paren is an opener");
	expectEqual(match, @")", "paren closer");
	expectTrue(NVPairedCharacterCode(')', &match) == 0, "close paren");
	expectEqual(match, @"(", "close paren opener");
	expectTrue(NVPairedCharacterCode('[', &match) == 2, "bracket opener");
	expectTrue(NVPairedCharacterCode(']', &match) == 0, "bracket closer");
	expectTrue(NVPairedCharacterCode('{', &match) == 2, "brace opener");
	expectTrue(NVPairedCharacterCode('}', &match) == 0, "brace closer");
	expectTrue(NVPairedCharacterCode('a', &match) == -1, "letter is not paired");

	expectTrue(NVCharactersFormEmptyPair('`', '`') == YES, "empty backtick pair");
	expectTrue(NVCharactersFormEmptyPair('(', ')') == YES, "empty paren pair");
	expectTrue(NVCharactersFormEmptyPair('[', ']') == YES, "empty bracket pair");
	expectTrue(NVCharactersFormEmptyPair('{', '}') == YES, "empty brace pair");
	expectTrue(NVCharactersFormEmptyPair('"', '"') == YES, "empty quote pair");
	expectTrue(NVCharactersFormEmptyPair('\'', '\'') == YES, "empty apostrophe pair");
	expectTrue(NVCharactersFormEmptyPair('`', 'a') == NO, "backtick does not close on a letter");
	expectTrue(NVCharactersFormEmptyPair('(', ']') == NO, "mismatched pair");

	NSMutableString *fence = [NSMutableString string];
	NSUInteger cursor = 0;
	typeBackticks(fence, &cursor, 1);
	expectEqual(fence, @"``", "first backtick inserts a pair");
	expectTrue(cursor == 1, "caret sits between the pair");
	typeBackticks(fence, &cursor, 1);
	expectEqual(fence, @"``", "second backtick types over the closer");
	expectTrue(cursor == 2, "caret moves past the closer");
	typeBackticks(fence, &cursor, 1);
	expectEqual(fence, @"```", "third backtick stays single so the fence is three");
	expectTrue(cursor == [fence length], "caret is ready for the fence newline");

	NSMutableString *longer = [NSMutableString string];
	cursor = 0;
	typeBackticks(longer, &cursor, 4);
	expectEqual(longer, @"````", "a fourth backtick extends the run");
	expectTrue(cursor == 4, "caret stays at the end of the run");

	NSMutableString *inlineCode = [NSMutableString string];
	cursor = 0;
	NSUInteger selection = 0;
	typeInto(inlineCode, &cursor, &selection, @"`");
	typeInto(inlineCode, &cursor, &selection, @"a");
	expectEqual(inlineCode, @"`a`", "a letter is typed inside the pair");
	expectTrue(cursor == 2, "caret stays before the closer");
	typeInto(inlineCode, &cursor, &selection, @"`");
	expectEqual(inlineCode, @"`a`", "closing backtick types over");
	expectTrue(cursor == 3, "caret leaves the inline code");

	NSMutableString *afterWord = [NSMutableString stringWithString:@"see "];
	cursor = [afterWord length];
	typeBackticks(afterWord, &cursor, 3);
	expectEqual(afterWord, @"see ```", "a fence run still works after text");
	expectTrue(cursor == [afterWord length], "caret ends the fence run");

	NSMutableString *spaced = [NSMutableString stringWithString:@"`` "];
	cursor = [spaced length];
	typeBackticks(spaced, &cursor, 1);
	expectEqual(spaced, @"`` ``", "a space breaks the run and starts a new pair");

	NSMutableString *selected = [NSMutableString stringWithString:@"hello"];
	cursor = 0;
	selection = 5;
	typeInto(selected, &cursor, &selection, @"`");
	expectEqual(selected, @"`hello`", "a selection is wrapped");
	expectTrue(cursor == [selected length] && selection == 0, "caret follows the wrapped selection");

	NSMutableString *word = [NSMutableString stringWithString:@"hello"];
	cursor = 0;
	selection = 0;
	typeInto(word, &cursor, &selection, @"`");
	expectEqual(word, @"`hello", "no pair when the next character is a letter");

	expectTrue(NVAutoPairActionForInsert(@"\"", @"\"", 1, @"", @"", 0) == NVAutoPairWrapOrInsert, "quote still pairs");
	expectTrue(NVAutoPairActionForInsert(@"\"", @"\"", 1, @"\"", @"\"", 0) == NVAutoPairTypeOver, "quote still types over");
	expectTrue(NVAutoPairActionForInsert(@"\"", @"\"", 1, @"\"hello", @"", 0) == NVAutoPairAsTyped, "odd quote does not pair");
	expectTrue(NVAutoPairActionForInsert(@"\"", @"\"", 1, @"\"hello\"", @"", 0) == NVAutoPairWrapOrInsert, "even quotes pair again");
	expectTrue(NVAutoPairActionForInsert(@"(", @")", 2, @"", @"", 0) == NVAutoPairWrapOrInsert, "paren still pairs");
	expectTrue(NVAutoPairActionForInsert(@")", @"(", 0, @"(", @")", 0) == NVAutoPairTypeOver, "close paren types over");
	expectTrue(NVAutoPairActionForInsert(@"(", @")", 2, @"", @")", 0) == NVAutoPairAsTyped, "paren does not pair into an unmatched closer");
	expectTrue(NVAutoPairActionForInsert(@"(", @")", 2, @"", @"hello", 0) == NVAutoPairAsTyped, "paren skips a following word");

	expectTrue(NVPairedCharacterCode('=', &match) == 3, "equals wraps a selection only");
	expectEqual(match, @"=", "equals matches itself");
	expectTrue(NVPairedCharacterCode('~', &match) == 3, "tilde wraps a selection only");
	expectEqual(match, @"~", "tilde matches itself");
	expectTrue(NVCharactersFormEmptyPair('=', '=') == NO, "equals is not an empty pair");
	expectTrue(NVCharactersFormEmptyPair('~', '~') == NO, "tilde is not an empty pair");
	expectTrue(NVAutoPairActionForInsert(@"=", @"=", 3, @"", @"", 5) == NVAutoPairWrapOrInsert, "equals wraps a selection");
	expectTrue(NVAutoPairActionForInsert(@"=", @"=", 3, @"", @"", 0) == NVAutoPairAsTyped, "equals does not pair without a selection");
	expectTrue(NVAutoPairActionForInsert(@"=", @"=", 3, @"", @"=", 0) == NVAutoPairAsTyped, "equals does not type over");
	expectTrue(NVAutoPairActionForInsert(@"~", @"~", 3, @"hello", @"", 0) == NVAutoPairAsTyped, "tilde does not pair without a selection");
	expectEqual(NVAutoPairSelectionAffix(@"="), @"==", "equals wraps with a highlight mark");
	expectEqual(NVAutoPairSelectionAffix(@"~"), @"~~", "tilde wraps with a strike mark");
	expectEqual(NVAutoPairSelectionAffix(@"`"), nil, "a backtick stays a single wrap");

	NSMutableString *highlighted = [NSMutableString stringWithString:@"hello"];
	cursor = 0;
	selection = 5;
	typeInto(highlighted, &cursor, &selection, @"=");
	expectEqual(highlighted, @"==hello==", "a selection is wrapped in highlight marks");
	expectTrue(cursor == [highlighted length] && selection == 0, "caret follows the highlight");

	NSMutableString *oneEquals = [NSMutableString stringWithString:@"hello"];
	cursor = 0;
	selection = 0;
	typeInto(oneEquals, &cursor, &selection, @"=");
	expectEqual(oneEquals, @"=hello", "equals before a word stays single");

	NSMutableString *struck = [NSMutableString stringWithString:@"say hello there"];
	cursor = 4;
	selection = 5;
	typeInto(struck, &cursor, &selection, @"~");
	expectEqual(struck, @"say ~~hello~~ there", "a selection is wrapped in strike marks");

	NSMutableString *oneTilde = [NSMutableString stringWithString:@"hello"];
	cursor = 2;
	selection = 0;
	typeInto(oneTilde, &cursor, &selection, @"~");
	expectEqual(oneTilde, @"he~llo", "a tilde inside a word stays single");

	if (gFailures) {
		fprintf(stderr, "%d auto-pair tests failed\n", gFailures);
		[pool drain];
		return 1;
	}
	printf("auto-pair tests passed\n");
	[pool drain];
	return 0;
}
