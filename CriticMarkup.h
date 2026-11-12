#import <Foundation/Foundation.h>
#import "MarkdownHighlight.h"

/* CriticMarkup spans: {++addition++} {--deletion--} {~~old~>new~~} {==highlight==} {>>comment<<}.
   Each span is a dictionary with type, opener, content, and closer.
   Substitutions also include old and separator. Marks inside code are skipped. */

typedef struct {
	BOOL inCodeBlock;
	BOOL inInlineCode;
	BOOL inIndentedCode;
} NVCriticCodeState;

static inline void NVCriticUpdateCodeState(NSString *string, NSUInteger index, NSUInteger length, NVCriticCodeState *state) {
	BOOL lineStart = index == 0 || [string characterAtIndex:index - 1] == '\n' || [string characterAtIndex:index - 1] == '\r';
	if (lineStart)
		state->inIndentedCode = NVMarkdownHighlightLineIsIndentedCode(string, index, length);
	if ([string characterAtIndex:index] != '`')
		return;
	BOOL fence = NVMarkdownHighlightChar(string, index + 1, length) == '`' &&
		NVMarkdownHighlightChar(string, index + 2, length) == '`';
	if (fence)
		state->inCodeBlock = !state->inCodeBlock;
	else if (!state->inCodeBlock)
		state->inInlineCode = !state->inInlineCode;
}

static inline NSString *NVCriticOpenKind(NSString *string, NSUInteger index, NSUInteger length, NSString **closerToken) {
	if (index + 2 >= length || [string characterAtIndex:index] != '{')
		return nil;
	unichar first = [string characterAtIndex:index + 1];
	unichar second = [string characterAtIndex:index + 2];
	if (first == '+' && second == '+') {
		*closerToken = @"++}";
		return @"addition";
	}
	if (first == '-' && second == '-') {
		*closerToken = @"--}";
		return @"deletion";
	}
	if (first == '~' && second == '~') {
		*closerToken = @"~~}";
		return @"substitution";
	}
	if (first == '=' && second == '=') {
		*closerToken = @"==}";
		return @"highlight";
	}
	if (first == '>' && second == '>') {
		*closerToken = @"<<}";
		return @"comment";
	}
	return nil;
}

static inline NSArray *NVCriticMarkupSpansInRange(NSString *string, NSRange range) {
	NSUInteger length = [string length];
	if (range.location > length)
		return [NSArray array];
	if (NSMaxRange(range) > length)
		range.length = length - range.location;
	if (!range.length)
		return [NSArray array];
	NSMutableArray *spans = [NSMutableArray array];
	NVCriticCodeState state = {NO, NO, NO};
	NSUInteger index = range.location;
	NSUInteger end = NSMaxRange(range);
	while (index < end) {
		NVCriticUpdateCodeState(string, index, length, &state);
		BOOL inCode = state.inCodeBlock || state.inInlineCode || state.inIndentedCode;
		NSString *closerToken = nil;
		NSString *kind = inCode ? nil : NVCriticOpenKind(string, index, length, &closerToken);
		if (kind && index + 3 < end) {
			NSRange search = NSMakeRange(index + 3, end - (index + 3));
			NSRange found = [string rangeOfString:closerToken options:0 range:search];
			if (found.location != NSNotFound && found.length == 3) {
				NSRange content = NSMakeRange(index + 3, found.location - (index + 3));
				NSRange oldRange = NSMakeRange(NSNotFound, 0);
				NSRange separator = NSMakeRange(NSNotFound, 0);
				if ([kind isEqualToString:@"substitution"]) {
					NSRange split = [string rangeOfString:@"~>" options:0 range:content];
					if (split.location != NSNotFound) {
						oldRange = NSMakeRange(content.location, split.location - content.location);
						separator = split;
						content = NSMakeRange(NSMaxRange(split), found.location - NSMaxRange(split));
					}
				}
				NSMutableDictionary *span = [NSMutableDictionary dictionary];
				[span setObject:kind forKey:@"type"];
				[span setObject:[NSValue valueWithRange:NSMakeRange(index, 3)] forKey:@"opener"];
				[span setObject:[NSValue valueWithRange:content] forKey:@"content"];
				[span setObject:[NSValue valueWithRange:found] forKey:@"closer"];
				if (oldRange.location != NSNotFound)
					[span setObject:[NSValue valueWithRange:oldRange] forKey:@"old"];
				if (separator.location != NSNotFound)
					[span setObject:[NSValue valueWithRange:separator] forKey:@"separator"];
				[spans addObject:span];
				NSUInteger spanEnd = NSMaxRange(found);
				NSUInteger cursor = index + 1;
				index = spanEnd;
				while (cursor < spanEnd) {
					NVCriticUpdateCodeState(string, cursor, length, &state);
					cursor++;
				}
				continue;
			}
		}
		index++;
	}
	return spans;
}

static inline NSArray *NVCriticMarkupSpans(NSString *string) {
	return NVCriticMarkupSpansInRange(string, NSMakeRange(0, [string length]));
}

static inline BOOL NVCriticKindStrikethrough(NSString *type) {
	return [type isEqualToString:@"deletion"];
}

static inline BOOL NVCriticKindUnderline(NSString *type) {
	return [type isEqualToString:@"addition"];
}
