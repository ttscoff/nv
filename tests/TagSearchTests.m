#import <Foundation/Foundation.h>
#import <stdio.h>
#import "TagSearch.h"

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

static void testWord(NSString *word, BOOL isTag, NSString *prefix) {
	NSString *got = nil;
	BOOL result = NVSearchWordIsTagPrefix(word, &got);
	const char *label = word ? [word UTF8String] : "(nil)";
	expectTrue(result == isTag, label);
	if (isTag)
		expectEqual(got, prefix, label);
}

int main(int argc, const char *argv[]) {
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

	testWord(@"#unix", YES, @"unix");
	testWord(@"#", NO, nil);
	testWord(@" #unix ", YES, @"unix");
	testWord(@"##unix", YES, @"#unix");
	testWord(@"unix", NO, nil);
	testWord(@"", NO, nil);
	testWord(nil, NO, nil);
	expectTrue(NVSearchWordIsTagPrefix(@"#unix", NULL) == YES, "null prefix out");

	expectTrue(NVQueryContainsTagWord(@"#unix") == YES, "contains #unix");
	expectTrue(NVQueryContainsTagWord(@"#") == NO, "lone hash");
	expectTrue(NVQueryContainsTagWord(@"# unix") == NO, "split hash");
	expectTrue(NVQueryContainsTagWord(@"shell") == NO, "plain word");
	expectTrue(NVQueryContainsTagWord(@"hello:world") == NO, "colon is not a tag");
	expectTrue(NVQueryContainsTagWord(@"#unix shell") == YES, "mixed");
	expectTrue(NVQueryContainsTagWord(@"shell #unix") == YES, "tag after text");
	expectTrue(NVQueryContainsTagWord(@"\"#unix:admin\"") == YES, "quoted tag");
	expectTrue(NVQueryContainsTagWord(nil) == NO, "nil query");

	expectEqual(NVStringByRemovingTagWords(@"#unix shell"), @"shell", "drop tag");
	expectEqual(NVStringByRemovingTagWords(@"shell #unix script"), @"shell script", "keep both sides");
	expectEqual(NVStringByRemovingTagWords(@"#unix \"shell script\""), @"\"shell script\"", "keep phrase");
	expectEqual(NVStringByRemovingTagWords(@"#unix:admin"), @"admin", "colon word");
	expectEqual(NVStringByRemovingTagWords(@"#unix"), @"", "only a tag");
	expectEqual(NVStringByRemovingTagWords(@"#"), @"#", "lone hash remains");
	expectEqual(NVStringByRemovingTagWords(@"# unix"), @"# unix", "split hash remains");
	expectEqual(NVStringByRemovingTagWords(@"\"#unix:admin\""), @"", "quoted tag dropped");

	expectEqual(NVHighlightStringByRemovingTagWords(@"hello:world"), @"hello:world", "plain highlight unchanged");
	expectEqual(NVHighlightStringByRemovingTagWords(@"#unix shell"), @"shell", "highlight drops tag");
	expectEqual(NVHighlightStringByRemovingTagWords(@"#unix \"shell script\""), @"\"shell script\"", "highlight phrase");
	expectEqual(NVHighlightStringByRemovingTagWords(@"#unix:admin"), @"admin", "highlight admin");
	expectEqual(NVHighlightStringByRemovingTagWords(@"#"), @"#", "highlight lone hash");
	expectEqual(NVHighlightStringByRemovingTagWords(@"#unix"), nil, "nothing to highlight");
	expectEqual(NVHighlightStringByRemovingTagWords(nil), nil, "nil highlight");
	expectEqual(NVHighlightStringByRemovingTagWords(@""), nil, "empty highlight");

	NSArray *tags = [NSArray arrayWithObjects:@"unix", @"unix-admin", @"admin-unix", @"Unix:admin", nil];
	expectTrue(NVTagTitlesMatchPrefix(tags, @"unix") == YES, "exact unix");
	expectTrue(NVTagTitlesMatchPrefix(tags, @"uni") == YES, "prefix uni");
	expectTrue(NVTagTitlesMatchPrefix(tags, @"UNIX") == YES, "case");
	expectTrue(NVTagTitlesMatchPrefix(tags, @"admin-unix") == YES, "exact admin-unix");
	expectTrue(NVTagTitlesMatchPrefix(tags, @"admin-uni") == YES, "prefix of admin-unix");
	expectTrue(NVTagTitlesMatchPrefix([NSArray arrayWithObject:@"admin-unix"], @"unix") == NO, "not a suffix");
	expectTrue(NVTagTitlesMatchPrefix([NSArray array], @"unix") == NO, "empty list");
	expectTrue(NVTagTitlesMatchPrefix(nil, @"unix") == NO, "nil list");
	expectTrue(NVTagTitlesMatchPrefix(tags, @"") == NO, "empty prefix");
	expectTrue(NVTagTitlesMatchPrefix(tags, nil) == NO, "nil prefix");
	expectTrue(NVTagTitlesMatchPrefix([NSArray arrayWithObject:@"Unix"], @"unix") == YES, "folded tag");

	if (gFailures) {
		fprintf(stderr, "%d tag search tests failed\n", gFailures);
		[pool drain];
		return 1;
	}
	printf("tag search tests passed\n");
	[pool drain];
	return 0;
}
