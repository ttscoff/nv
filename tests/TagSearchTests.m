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

	expectTrue(NVQueryNewlyHasTagWord(@"#", @"#u") == YES, "hash to #u");
	expectTrue(NVQueryNewlyHasTagWord(@"#", @"#unix") == YES, "hash to #unix");
	expectTrue(NVQueryNewlyHasTagWord(nil, @"#unix") == YES, "nil to #unix");
	expectTrue(NVQueryNewlyHasTagWord(@"#uni", @"#unix") == NO, "grow tag prefix");
	expectTrue(NVQueryNewlyHasTagWord(@"#unix", @"#unix shell") == NO, "tag plus word");
	expectTrue(NVQueryNewlyHasTagWord(@"shell", @"shell") == NO, "plain stays plain");
	expectTrue(NVQueryNewlyHasTagWord(nil, nil) == NO, "nil to nil");
	expectTrue(NVQueryNewlyHasTagWord(@"", @"#") == NO, "empty to lone hash");
	expectTrue(NVQueryNewlyHasTagWord(@"#unix #", @"#unix #s") == YES, "second tag starts");
	expectTrue(NVQueryNewlyHasTagWord(@"#unix #s", @"#unix #sh") == NO, "second tag grows");
	expectTrue(NVQueryNewlyHasTagWord(@"shell #unix", @"shell #unix #s") == YES, "tag after text grows");

	expectEqual(NVStringByRemovingTagWords(@"#unix shell"), @"shell", "drop tag");
	expectEqual(NVStringByRemovingTagWords(@"shell #unix script"), @"shell script", "keep both sides");
	expectEqual(NVStringByRemovingTagWords(@"#unix \"shell script\""), @"\"shell script\"", "keep phrase");
	expectEqual(NVStringByRemovingTagWords(@"#unix:admin"), @"admin", "colon word");
	expectEqual(NVStringByRemovingTagWords(@"#unix"), @"", "only a tag");
	expectEqual(NVStringByRemovingTagWords(@"#"), @"#", "lone hash remains");
	expectEqual(NVStringByRemovingTagWords(@"# unix"), @"# unix", "split hash remains");
	expectEqual(NVStringByRemovingTagWords(@"\"#unix:admin\""), @"", "quoted tag dropped");

	expectEqual(NVHighlightStringByRemovingTagWords(@"hello:world"), @"hello:world", "plain highlight unchanged");
	expectEqual(NVHighlightStringByRemovingTagWords(@"#unix shell"), @"#unix shell", "highlight keeps hashtag");
	expectEqual(NVHighlightStringByRemovingTagWords(@"shell #unix script"), @"shell #unix script", "highlight keeps hashtag between words");
	expectEqual(NVHighlightStringByRemovingTagWords(@"#unix \"shell script\""), @"#unix \"shell script\"", "highlight keeps hashtag and phrase");
	expectEqual(NVHighlightStringByRemovingTagWords(@"#unix:admin"), @"#unix admin", "highlight keeps hashtag and admin");
	expectEqual(NVHighlightStringByRemovingTagWords(@"\"#unix:admin\""), @"\"#unix:admin\"", "highlight quoted hashtag");
	expectEqual(NVHighlightStringByRemovingTagWords(@"#"), @"#", "highlight lone hash");
	expectEqual(NVHighlightStringByRemovingTagWords(@"#unix"), @"#unix", "highlight a lone hashtag");
	expectEqual(NVHighlightStringByRemovingTagWords(nil), nil, "nil highlight");
	expectEqual(NVHighlightStringByRemovingTagWords(@""), nil, "empty highlight");
	expectEqual(NVStringByTrimmingHighlightTerm(@"#unix "), @"#unix", "trim hashtag term");
	expectEqual(NVStringByTrimmingHighlightTerm(@" shell script "), @"shell script", "trim phrase term");
	expectEqual(NVStringByTrimmingHighlightTerm(@"  "), @"", "trim blank term");
	expectEqual(NVStringByTrimmingHighlightTerm(nil), nil, "trim nil term");

	NSArray *unixTag = [NSArray arrayWithObject:@"unix"];
	expectTrue(NVNoteMatchesTagWord(unixTag, "", "no hash here", "", @"unix", "#unix") == YES, "tag still matches");
	expectTrue(NVNoteMatchesTagWord(nil, "", "see #unix today", "", @"unix", "#unix") == YES, "body hashtag");
	expectTrue(NVNoteMatchesTagWord(nil, "#unix notes", "", NULL, @"unix", "#unix") == YES, "title hashtag");
	expectTrue(NVNoteMatchesTagWord(nil, "", "plain", "#unix", @"unix", "#unix") == YES, "label hashtag");
	expectTrue(NVNoteMatchesTagWord(nil, "", "unix without a hash", "", @"unix", "#unix") == NO, "bare word is not a hashtag");
	expectTrue(NVNoteMatchesTagWord(nil, "", "see #shell today", "", @"unix", "#unix") == NO, "different hashtag");
	expectTrue(NVNoteMatchesTagWord(nil, "", "talk #unix-admin", "", @"uni", "#uni") == YES, "body hashtag prefix");
	expectTrue(NVNoteMatchesTagWord([NSArray arrayWithObject:@"unix-admin"], "", "", "", @"uni", "#uni") == YES, "tag prefix still matches");
	expectTrue(NVNoteMatchesTagWord([NSArray arrayWithObject:@"admin-unix"], "", "admin-unix", "", @"unix", "#unix") == NO, "suffix tag and no hash");
	expectTrue(NVNoteMatchesTagWord(nil, "", "##unix", "", @"#unix", "##unix") == YES, "double hash literal");
	expectTrue(NVNoteMatchesTagWord(nil, "", "#unix", "", @"#unix", "##unix") == NO, "single hash is not a double hash");
	expectTrue(NVNoteMatchesTagWord(nil, NULL, NULL, NULL, @"unix", "#unix") == NO, "empty note");
	expectTrue(NVNoteMatchesTagWord(unixTag, NULL, NULL, NULL, @"unix", NULL) == YES, "tag match ignores a missing literal");

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
