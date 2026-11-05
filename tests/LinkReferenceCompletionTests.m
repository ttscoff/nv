#import <Foundation/Foundation.h>
#import <stdio.h>
#import "LinkReferenceCompletion.h"

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

static void expectLabels(NSArray *got, NSArray *want, const char *what) {
	if (got && want && [got isEqualToArray:want])
		return;
	fprintf(stderr, "FAIL %s\n  got: %s\n  want: %s\n", what,
			got ? [[got description] UTF8String] : "(nil)",
			want ? [[want description] UTF8String] : "(nil)");
	gFailures++;
}

static NSString *sampleDocument(void) {
	return @"See [the spec][spec] and ![diagram][diagram].\n"
		@"Also [CommonMark][].\n"
		@"\n"
		@"```\n"
		@"[hidden][secret]\n"
		@"[secret]: https://example.com/secret\n"
		@"```\n"
		@"\n"
		@"[spec]: https://example.com/spec\n"
		@"[diagram]: https://example.com/diagram.png";
}

static NVLinkCompletionContext contextAtEnd(NSString *document) {
	return NVLinkCompletionContextAtLocation(document, [document length]);
}

static NSArray *labelsAtEnd(NSString *document) {
	NVLinkCompletionContext context = contextAtEnd(document);
	NSString *query = @"";
	if (context.range.location != NSNotFound && NSMaxRange(context.range) <= [document length])
		query = [document substringWithRange:context.range];
	return NVLinkCompletionCandidates(document, context.kind, query);
}

int main(int argc, const char *argv[]) {
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	NSString *doc = sampleDocument();
	NSArray *defined = [NSArray arrayWithObjects:@"diagram", @"spec", nil];

	NSString *open = [doc stringByAppendingString:@"\nRead [more]["];
	NVLinkCompletionContext openContext = contextAtEnd(open);
	expectTrue(openContext.kind == NVLinkCompletionUsage, "open usage kind");
	expectTrue(openContext.range.location == [open length] && openContext.range.length == 0, "open usage range");
	expectLabels(labelsAtEnd(open), defined, "open usage offers defined ids");

	NSString *filtered = [doc stringByAppendingString:@"\nRead [more][sp"];
	NVLinkCompletionContext filteredContext = contextAtEnd(filtered);
	expectTrue(filteredContext.kind == NVLinkCompletionUsage, "filtered usage kind");
	expectTrue(filteredContext.range.length == 2, "filtered usage query length");
	expectTrue(NSMaxRange(filteredContext.range) == [filtered length], "filtered usage ends at caret");
	expectLabels(labelsAtEnd(filtered), [NSArray arrayWithObject:@"spec"], "filtered usage");

	NSString *middle = [doc stringByAppendingString:@"\nRead [more][pec"];
	expectLabels(labelsAtEnd(middle), [NSArray arrayWithObject:@"spec"], "substring usage");

	NSString *image = [doc stringByAppendingString:@"\n![plot]["];
	expectTrue(contextAtEnd(image).kind == NVLinkCompletionUsage, "image usage kind");
	expectTrue(contextAtEnd(image).range.location == [image length], "image usage range");
	expectLabels(labelsAtEnd(image), defined, "image usage offers defined ids");

	NSString *definition = [doc stringByAppendingString:@"\n["];
	expectTrue(contextAtEnd(definition).kind == NVLinkCompletionDefinition, "definition kind");
	expectTrue(contextAtEnd(definition).range.location == [definition length], "definition range");
	expectLabels(labelsAtEnd(definition), [NSArray arrayWithObject:@"CommonMark"], "definition offers unused id");

	NSString *definitionQuery = [doc stringByAppendingString:@"\n[com"];
	expectLabels(labelsAtEnd(definitionQuery), [NSArray arrayWithObject:@"CommonMark"], "definition query");

	NSString *indented = [doc stringByAppendingString:@"\n    ["];
	expectTrue(contextAtEnd(indented).kind == NVLinkCompletionNone, "four spaces is not a definition");

	NSString *spaced = [doc stringByAppendingString:@"\n  ["];
	expectTrue(contextAtEnd(spaced).kind == NVLinkCompletionDefinition, "two spaces still opens a definition");

	NSString *closed = [doc stringByAppendingString:@"\n[]"];
	NSUInteger between = [closed length] - 1;
	NVLinkCompletionContext closedContext = NVLinkCompletionContextAtLocation(closed, between);
	expectTrue(closedContext.kind == NVLinkCompletionDefinition, "cursor between definition brackets");
	expectEqual(NVLinkCompletionInsert(@"CommonMark", YES), @"CommonMark", "keep an existing closer");
	expectEqual(NVLinkCompletionInsert(@"CommonMark", NO), @"CommonMark]", "insert a missing closer");

	expectTrue(NVLinkReferenceJustOpened(@"See [more][", [@"See [more][" length]), "just opened a usage");
	expectTrue(NVLinkReferenceJustOpened(@"[\n  [", [@"[\n  [" length]), "just opened an indented definition");
	expectTrue(NVLinkReferenceJustOpened(@"    [", 5) == NO, "four spaces does not open");
	expectTrue(NVLinkReferenceJustOpened(@"See [more]", [@"See [more]" length]) == NO, "a closed link does not open");

	expectTrue(NVLinkCompletionContextAtLocation(@"See the spec", 12).kind == NVLinkCompletionNone, "ordinary text");
	expectTrue(contextAtEnd(@"See [").kind == NVLinkCompletionNone, "a bracket mid-line is not a definition");
	expectTrue(contextAtEnd([doc stringByAppendingString:@"\nRead [more][spec]"]).kind == NVLinkCompletionNone, "a closed usage does not complete");
	expectTrue(NVLinkCompletionContextAtLocation(@"[[Chicago", 9).kind == NVLinkCompletionNone, "wiki link is not a reference");

	NSString *folded = @"[Foo   Bar][]\n[foo bar]: https://example.com";
	expectLabels(labelsAtEnd([folded stringByAppendingString:@"\n["]), [NSArray array], "normalized id is already defined");

	NSString *firstWins = @"[Spec]: https://example.com/a\n[spec]: https://example.com/b\nRead [more][";
	expectLabels(labelsAtEnd(firstWins), [NSArray arrayWithObject:@"Spec"], "first definition wins");

	if (gFailures) {
		fprintf(stderr, "%d link reference tests failed\n", gFailures);
		[pool drain];
		return 1;
	}
	printf("link reference tests passed\n");
	[pool drain];
	return 0;
}
