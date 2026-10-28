#import "TagSearch.h"

static NSString *NVCanonicalLowercase(NSString *string) {
	if (![string length])
		return @"";
	CFMutableStringRef copy = CFStringCreateMutableCopy(NULL, 0, (CFStringRef)string);
	CFStringLowercase(copy, NULL);
	NSString *lower = [(NSString *)copy copy];
	CFRelease(copy);
	return [lower autorelease];
}

static NSCharacterSet *NVSearchSeparatorSet(NSString *query) {
	NSString *chars = @" :\t\r\n";
	if (query && [query rangeOfString:@"\""].location != NSNotFound)
		chars = @"\"";
	return [NSCharacterSet characterSetWithCharactersInString:chars];
}

static NSArray *NVSearchWords(NSString *query) {
	if (![query length])
		return [NSArray array];
	return [query componentsSeparatedByCharactersInSet:NVSearchSeparatorSet(query)];
}

BOOL NVSearchWordIsTagPrefix(NSString *word, NSString **prefixOut) {
	if (prefixOut)
		*prefixOut = nil;
	if (![word length])
		return NO;
	NSString *trimmed = [word stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
	if ([trimmed length] < 2)
		return NO;
	if ([trimmed characterAtIndex:0] != '#')
		return NO;
	if (prefixOut)
		*prefixOut = [trimmed substringFromIndex:1];
	return YES;
}

BOOL NVQueryContainsTagWord(NSString *query) {
	NSArray *words = NVSearchWords(query);
	NSUInteger i;
	for (i = 0; i < [words count]; i++) {
		if (NVSearchWordIsTagPrefix([words objectAtIndex:i], NULL))
			return YES;
	}
	return NO;
}

BOOL NVQueryNewlyHasTagWord(NSString *previousQuery, NSString *query) {
	return NVQueryContainsTagWord(query) && !NVQueryContainsTagWord(previousQuery);
}

NSString *NVStringByRemovingTagWords(NSString *query) {
	BOOL quoted = (query && [query rangeOfString:@"\""].location != NSNotFound);
	NSArray *words = NVSearchWords(query);
	NSMutableArray *kept = [NSMutableArray array];
	NSUInteger i;
	for (i = 0; i < [words count]; i++) {
		NSString *word = [words objectAtIndex:i];
		if (NVSearchWordIsTagPrefix(word, NULL))
			continue;
		NSString *trimmed = [word stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
		if (![trimmed length])
			continue;
		if (quoted && (i % 2 == 1))
			trimmed = [NSString stringWithFormat:@"\"%@\"", trimmed];
		[kept addObject:trimmed];
	}
	return [kept componentsJoinedByString:@" "];
}

NSString *NVHighlightStringByRemovingTagWords(NSString *query) {
	if (![query length])
		return nil;
	if (!NVQueryContainsTagWord(query))
		return query;
	NSString *stripped = NVStringByRemovingTagWords(query);
	return [stripped length] ? stripped : nil;
}

BOOL NVTagTitlesMatchPrefix(NSArray *tagTitles, NSString *prefix) {
	if (![prefix length] || ![tagTitles count])
		return NO;
	NSString *foldedPrefix = NVCanonicalLowercase(prefix);
	NSUInteger i;
	for (i = 0; i < [tagTitles count]; i++) {
		NSString *tag = [tagTitles objectAtIndex:i];
		if (![tag isKindOfClass:[NSString class]] || ![tag length])
			continue;
		if ([NVCanonicalLowercase(tag) hasPrefix:foldedPrefix])
			return YES;
	}
	return NO;
}
