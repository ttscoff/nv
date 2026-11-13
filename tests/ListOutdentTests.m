#import <Foundation/Foundation.h>
#import <stdio.h>
#import "ListOutdent.h"

static int gFailures = 0;

static NSRange lineContaining(NSString *text, NSString *fragment) {
	NSRange found = [text rangeOfString:fragment];
	if (found.location == NSNotFound)
		return NSMakeRange(NSNotFound, 0);
	return [text lineRangeForRange:found];
}

static void expectMarker(NSString *text, NSString *fragment, NSUInteger tabWidth, NSString *want, NSUInteger wantLocation, const char *what) {
	NSRange line = lineContaining(text, fragment);
	NSRange marker = NSMakeRange(NSNotFound, 0);
	NSString *got = NVListMarkerAfterOutdent(text, line, tabWidth, &marker);
	BOOL same = (got == want) || (got && want && [got isEqualToString:want]);
	if (!same || (want && marker.location != wantLocation)) {
		fprintf(stderr, "FAIL %s (got \"%s\" at %lu, want \"%s\" at %lu)\n", what,
			got ? [got UTF8String] : "(nil)", (unsigned long)marker.location,
			want ? [want UTF8String] : "(nil)", (unsigned long)wantLocation);
		gFailures++;
	}
}

int main(void) {
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	NSString *note =
		@"1. Hello\n"
		@"2. Hey\n"
		@"    - yo\n"
		@"    - butter butt\n"
		@"- \n";

	expectMarker(note, @"- \n", 4, @"3. ", 0,
		"an outdented bullet under a numbered list continues the sequence");
	expectMarker(@"1. Hello\n2. Hey\n    - yo\n- butter butt\n", @"- butter butt", 4, @"3. ", 0,
		"outdenting the nested bullet itself continues from the number above");
	expectMarker(note, @"- yo", 4, nil, 0,
		"a bullet that is still nested under the numbers keeps its marker");

	expectMarker(@"1. Hello\n- next\n", @"- next", 4, @"2. ", 0,
		"the first outdented bullet takes the next number");
	expectMarker(@"1) Hello\n- next\n", @"- next", 4, @"2) ", 0,
		"the continued number keeps the parent delimiter");
	expectMarker(@"9. Hello\n- next\n", @"- next", 4, @"10. ", 0,
		"the continued number grows an extra digit");
	expectMarker(@"1. Hello\n\n- next\n", @"- next", 4, @"2. ", 0,
		"a blank line still belongs to the numbered list");
	expectMarker(@"1. Hello\nwords\n- next\n", @"- next", 4, nil, 0,
		"prose between the lists ends the numbered sequence");
	expectMarker(@"- apples\n- butter\n", @"- butter", 4, nil, 0,
		"a bullet landing on a bullet list stays a bullet");
	expectMarker(@"* apples\n- butter\n", @"- butter", 4, @"* ", 0,
		"a bullet landing on a different bullet adopts that marker");
	expectMarker(@"- apples\n1. butter\n", @"1. butter", 4, @"- ", 0,
		"a number landing on a bullet list becomes a bullet");
	expectMarker(@"1. Hello\n3. Hey\n", @"3. Hey", 4, nil, 0,
		"a number landing on the same number style is left for renumbering");
	expectMarker(@"1) Hello\n2. Hey\n", @"2. Hey", 4, @"2) ", 0,
		"a different number delimiter adopts the parent delimiter");
	expectMarker(@"1. Hello\n    - still in\n", @"- still in", 4, nil, 0,
		"one indent level short of the numbers stays a bullet");
	expectMarker(@"    1. Hello\n    - [ ] task\n", @"- [ ] task", 4, @"2. ", 4,
		"the checkbox stays put when the marker becomes a number");
	expectMarker(@"1. Hello\n  - two spaces\n", @"- two spaces", 4, nil, 0,
		"a shallower indent than the tab width is not the numbered level");
	expectMarker(@"1. Hello\n  1. nested\n  - two spaces\n", @"- two spaces", 2, @"2. ", 2,
		"a matching indent width continues that numbered level");

	[pool drain];
	if (!gFailures)
		printf("test-ok\n");
	return gFailures ? 1 : 0;
}
