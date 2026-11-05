#import "LinkReferenceCompletion.h"

static NSRegularExpression *NVLinkRegex(NSString *pattern) {
	NSError *error = nil;
	NSRegularExpression *regex = [NSRegularExpression regularExpressionWithPattern:pattern options:0 error:&error];
	if (!regex)
		NSLog(@"link reference completion: %@", error);
	return regex;
}

static NSRegularExpression *NVLinkUsageRegex(void) {
	static NSRegularExpression *regex = nil;
	if (!regex)
		regex = [NVLinkRegex(@"!?\\[([^\\]\\n]*)\\]\\[([^\\]\\n]*)\\]") retain];
	return regex;
}

static NSRegularExpression *NVLinkDefinitionRegex(void) {
	static NSRegularExpression *regex = nil;
	if (!regex)
		regex = [NVLinkRegex(@"^[ ]{0,3}\\[([^\\]\\n]+)\\]:") retain];
	return regex;
}

static NSRegularExpression *NVLinkOpenUsageRegex(void) {
	static NSRegularExpression *regex = nil;
	if (!regex)
		regex = [NVLinkRegex(@"!?\\[[^\\]\\n]*\\]\\[([^\\]\\n]*)$") retain];
	return regex;
}

static NSRegularExpression *NVLinkOpenDefinitionRegex(void) {
	static NSRegularExpression *regex = nil;
	if (!regex)
		regex = [NVLinkRegex(@"^[ ]{0,3}\\[([^\\]\\n]*)$") retain];
	return regex;
}

static NSRegularExpression *NVLinkFenceRegex(void) {
	static NSRegularExpression *regex = nil;
	if (!regex)
		regex = [NVLinkRegex(@"^[ ]{0,3}(`{3,}|~{3,})") retain];
	return regex;
}

static NSRegularExpression *NVLinkWikiOpenRegex(void) {
	static NSRegularExpression *regex = nil;
	if (!regex)
		regex = [NVLinkRegex(@"\\[\\[[^\\[\\]|\\n#]*$") retain];
	return regex;
}

static NSString *NVNormalizeLinkLabel(NSString *value) {
	if (![value length])
		return @"";
	NSArray *parts = [value componentsSeparatedByCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
	NSMutableArray *kept = [NSMutableArray array];
	NSUInteger i;
	for (i = 0; i < [parts count]; i++) {
		NSString *part = [parts objectAtIndex:i];
		if ([part length])
			[kept addObject:part];
	}
	return [kept componentsJoinedByString:@" "];
}

static NSString *NVLinkLabelKey(NSString *label) {
	return [label lowercaseString] ?: @"";
}

static NSString *NVStringByStrippingTrailingNewlines(NSString *line) {
	NSUInteger length = [line length];
	while (length) {
		unichar character = [line characterAtIndex:length - 1];
		if (character != '\n' && character != '\r')
			break;
		length--;
	}
	if (length == [line length])
		return line;
	return [line substringToIndex:length];
}

static unichar NVFenceMarker(NSString *line) {
	NSRegularExpression *regex = NVLinkFenceRegex();
	if (!regex || !line)
		return 0;
	NSRange range = NSMakeRange(0, [line length]);
	NSTextCheckingResult *match = [regex firstMatchInString:line options:0 range:range];
	if (!match || [match numberOfRanges] < 2)
		return 0;
	NSRange marker = [match rangeAtIndex:1];
	if (!marker.length || NSMaxRange(marker) > [line length])
		return 0;
	return [line characterAtIndex:marker.location];
}

static void NVCollectLinkReferences(NSString *document, NSMutableArray *definitions, NSMutableArray *used) {
	if (!document)
		return;
	NSMutableSet *defined = [NSMutableSet set];
	NSMutableSet *seenUsed = [NSMutableSet set];
	unichar fence = 0;
	NSUInteger length = [document length];
	NSUInteger index = 0;
	while (index < length) {
		NSRange lineRange = [document lineRangeForRange:NSMakeRange(index, 0)];
		NSString *line = NVStringByStrippingTrailingNewlines([document substringWithRange:lineRange]);
		index = NSMaxRange(lineRange);

		unichar marker = NVFenceMarker(line);
		if (marker) {
			fence = !fence ? marker : (fence == marker ? 0 : fence);
			continue;
		}
		if (fence)
			continue;

		NSTextCheckingResult *definition = [NVLinkDefinitionRegex() firstMatchInString:line options:0 range:NSMakeRange(0, [line length])];
		if (definition && [definition numberOfRanges] > 1) {
			NSString *label = NVNormalizeLinkLabel([line substringWithRange:[definition rangeAtIndex:1]]);
			NSString *key = NVLinkLabelKey(label);
			if ([label length] && ![defined containsObject:key]) {
				[defined addObject:key];
				[definitions addObject:label];
			}
			continue;
		}

		NSRegularExpression *usage = NVLinkUsageRegex();
		if (!usage)
			continue;
		[usage enumerateMatchesInString:line options:0 range:NSMakeRange(0, [line length]) usingBlock:^(NSTextCheckingResult *match, NSMatchingFlags flags, BOOL *stop) {
			if ([match numberOfRanges] < 3)
				return;
			NSString *explicit = NVNormalizeLinkLabel([line substringWithRange:[match rangeAtIndex:2]]);
			NSString *text = NVNormalizeLinkLabel([line substringWithRange:[match rangeAtIndex:1]]);
			NSString *label = [explicit length] ? explicit : text;
			NSString *key = NVLinkLabelKey(label);
			if (![label length] || [seenUsed containsObject:key])
				return;
			[seenUsed addObject:key];
			[used addObject:label];
		}];
	}
}

static NSRange NVLinePrefixRange(NSString *document, NSUInteger location) {
	if (location > [document length])
		location = [document length];
	NSUInteger start = 0;
	if (location > 0) {
		NSRange newline = [document rangeOfCharacterFromSet:[NSCharacterSet newlineCharacterSet] options:NSBackwardsSearch range:NSMakeRange(0, location)];
		if (newline.location != NSNotFound)
			start = NSMaxRange(newline);
	}
	return NSMakeRange(start, location - start);
}

static BOOL NVRangeIsInside(NSRange range, NSUInteger length) {
	return range.location != NSNotFound && NSMaxRange(range) <= length;
}

NVLinkCompletionContext NVLinkCompletionContextAtLocation(NSString *document, NSUInteger location) {
	NVLinkCompletionContext context;
	context.kind = NVLinkCompletionNone;
	context.range = NSMakeRange(NSNotFound, 0);
	if (!document || location > [document length])
		return context;

	NSRange lineRange = NVLinePrefixRange(document, location);
	NSString *line = [document substringWithRange:lineRange];
	NSRange full = NSMakeRange(0, [line length]);
	if ([NVLinkWikiOpenRegex() firstMatchInString:line options:0 range:full])
		return context;

	NSTextCheckingResult *usage = [NVLinkOpenUsageRegex() firstMatchInString:line options:0 range:full];
	if (usage && [usage numberOfRanges] > 1) {
		NSRange query = [usage rangeAtIndex:1];
		query.location += lineRange.location;
		if (NVRangeIsInside(query, [document length])) {
			context.kind = NVLinkCompletionUsage;
			context.range = query;
			return context;
		}
	}

	NSTextCheckingResult *definition = [NVLinkOpenDefinitionRegex() firstMatchInString:line options:0 range:full];
	if (definition && [definition numberOfRanges] > 1) {
		NSRange query = [definition rangeAtIndex:1];
		query.location += lineRange.location;
		if (NVRangeIsInside(query, [document length])) {
			context.kind = NVLinkCompletionDefinition;
			context.range = query;
		}
	}
	return context;
}

NSArray *NVLinkCompletionCandidates(NSString *document, NVLinkCompletionKind kind, NSString *query) {
	if (kind != NVLinkCompletionUsage && kind != NVLinkCompletionDefinition)
		return [NSArray array];
	NSMutableArray *definitions = [NSMutableArray array];
	NSMutableArray *used = [NSMutableArray array];
	NVCollectLinkReferences(document, definitions, used);

	NSArray *source = definitions;
	if (kind == NVLinkCompletionDefinition) {
		NSMutableSet *defined = [NSMutableSet set];
		NSMutableArray *missing = [NSMutableArray array];
		NSUInteger i;
		for (i = 0; i < [definitions count]; i++)
			[defined addObject:NVLinkLabelKey([definitions objectAtIndex:i])];
		for (i = 0; i < [used count]; i++) {
			NSString *label = [used objectAtIndex:i];
			if (![defined containsObject:NVLinkLabelKey(label)])
				[missing addObject:label];
		}
		source = missing;
	}

	if (!query)
		query = @"";
	NSMutableArray *matches = [NSMutableArray array];
	NSUInteger i;
	for (i = 0; i < [source count]; i++) {
		NSString *label = [source objectAtIndex:i];
		if (![query length] || [label rangeOfString:query options:NSCaseInsensitiveSearch].location != NSNotFound)
			[matches addObject:label];
	}
	return [matches sortedArrayUsingSelector:@selector(localizedCaseInsensitiveCompare:)];
}

NSString *NVLinkCompletionInsert(NSString *label, BOOL hasClosingBracket) {
	if (!label)
		label = @"";
	if (hasClosingBracket)
		return label;
	return [label stringByAppendingString:@"]"];
}

BOOL NVLinkReferenceJustOpened(NSString *document, NSUInteger location) {
	if (!document || location > [document length])
		return NO;
	if (location >= 2 && [[document substringWithRange:NSMakeRange(location - 2, 2)] isEqualToString:@"]["])
		return YES;
	NSRange lineRange = NVLinePrefixRange(document, location);
	NSString *line = [document substringWithRange:lineRange];
	return [line rangeOfString:@"^[ ]{0,3}\\[$" options:NSRegularExpressionSearch].location != NSNotFound;
}
