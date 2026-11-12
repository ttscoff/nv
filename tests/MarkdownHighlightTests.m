#import <AppKit/AppKit.h>
#import <stdio.h>
#import "MarkdownHighlight.h"
#import "NVTheme.h"

static int gFailures = 0;

static NSRange rangeForKey(NSDictionary *span, NSString *key) {
	return [[span objectForKey:key] rangeValue];
}

static void expectSpan(NSString *string, NSUInteger index, NSString *content, const char *what) {
	NSArray *spans = NVMarkdownHighlightSpans(string);
	if (index >= [spans count]) {
		fprintf(stderr, "FAIL %s\n  got %lu spans, wanted one at %lu\n", what, (unsigned long)[spans count], (unsigned long)index);
		gFailures++;
		return;
	}
	NSDictionary *span = [spans objectAtIndex:index];
	NSRange body = rangeForKey(span, @"content");
	NSRange opener = rangeForKey(span, @"opener");
	NSRange closer = rangeForKey(span, @"closer");
	NSString *got = (body.location == NSNotFound || NSMaxRange(body) > [string length]) ? nil : [string substringWithRange:body];
	if (!got || ![got isEqualToString:content] || opener.length != 2 || closer.length != 2 ||
		NSMaxRange(opener) != body.location || closer.location != NSMaxRange(body)) {
		fprintf(stderr, "FAIL %s\n  content: %s\n", what, got ? [got UTF8String] : "(nil)");
		gFailures++;
	}
}

static void expectCount(NSString *string, NSUInteger count, const char *what) {
	NSUInteger got = [NVMarkdownHighlightSpans(string) count];
	if (got != count) {
		fprintf(stderr, "FAIL %s\n  got %lu spans, want %lu\n", what, (unsigned long)got, (unsigned long)count);
		gFailures++;
	}
}

static BOOL colorEquals(NSColor *color, CGFloat red, CGFloat green, CGFloat blue) {
	NSColor *rgb = [color colorUsingColorSpace:[NSColorSpace sRGBColorSpace]];
	CGFloat r = -1, g = -1, b = -1, a = -1;
	if (!rgb)
		return NO;
	[rgb getRed:&r green:&g blue:&b alpha:&a];
	return fabs(r - red) < 0.01 && fabs(g - green) < 0.01 && fabs(b - blue) < 0.01 && fabs(a - 1.0) < 0.01;
}

static void expectColors(NVThemeStyle *style, CGFloat fr, CGFloat fg, CGFloat fb, CGFloat br, CGFloat bg, CGFloat bb, const char *what) {
	if (!style || !colorEquals([style foregroundColor], fr, fg, fb) || !colorEquals([style backgroundColor], br, bg, bb)) {
		fprintf(stderr, "FAIL %s\n", what);
		gFailures++;
	}
}

int main(int argc, const char *argv[]) {
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

	expectSpan(@"See ==highlight== here", 0, @"highlight", "a marked word");
	expectCount(@"==one== and ==two==", 2, "two marks on one line");
	expectSpan(@"==one== and ==two==", 1, @"two", "the second mark");
	expectCount(@"== ==", 0, "spaces are not a mark");
	expectCount(@"==a ==", 0, "a space before the close is not a mark");
	expectCount(@"{==critic==}", 0, "critic markup is left alone");
	expectCount(@"`==code==`", 0, "inline code is left alone");
	expectCount(@"```\n==code==\n```\n==yes==", 1, "a fenced block is left alone");
	expectSpan(@"```\n==code==\n```\n==yes==", 0, @"yes", "a mark after a fence");
	expectCount(@"    ==indent==", 0, "indented code is left alone");
	expectSpan(@"    - ==item==", 0, @"item", "a mark in a list");
	expectCount(@"==\nno==", 0, "a mark stays on one line");
	expectCount(@"====", 0, "empty marks are left alone");
	expectCount(@"===no===", 0, "three equals are not a mark");
	expectCount(@"+==no==", 0, "a plus before the mark is left alone");
	expectCount(@"==no==+", 0, "a plus after the mark is left alone");
	expectCount(@"plain", 0, "plain text has no mark");

	if (NVMarkdownSyntaxHighlightEnabledForLength(0) != YES ||
		NVMarkdownSyntaxHighlightEnabledForLength(NVMarkdownSyntaxHighlightLimit) != YES ||
		NVMarkdownSyntaxHighlightEnabledForLength(NVMarkdownSyntaxHighlightLimit + 1) != NO) {
		fprintf(stderr, "FAIL syntax coloring stays on through 300KB and turns off past it\n");
		gFailures++;
	}

	{
		NSString *note = @"alpha\nbeta\ngamma";
		NSRange beta = NVMarkdownHighlightRangeForEdit(note, NSMakeRange(6, 1));
		NSRange gamma = NVMarkdownHighlightRangeForEdit(note, NSMakeRange(11, 0));
		if (beta.location != 6 || beta.length != 5 || gamma.location != 11 || gamma.length != 5) {
			fprintf(stderr, "FAIL an edit stays on its own line\n");
			gFailures++;
		}
		NSString *fenced = @"before\n```\ncode\n```\nafter";
		NSRange inside = NVMarkdownHighlightRangeForEdit(fenced, NSMakeRange(11, 1));
		NSRange before = NVMarkdownHighlightRangeForEdit(fenced, NSMakeRange(0, 1));
		NSRange after = NVMarkdownHighlightRangeForEdit(fenced, NSMakeRange(20, 1));
		if (inside.location != 7 || inside.length != 13 || before.location != 0 || before.length != 7 || after.location != 20 || after.length != 5) {
			fprintf(stderr, "FAIL a fenced block is colored with the edit, and the lines outside it are not\n  inside %lu,%lu before %lu,%lu after %lu,%lu\n",
				(unsigned long)inside.location, (unsigned long)inside.length,
				(unsigned long)before.location, (unsigned long)before.length,
				(unsigned long)after.location, (unsigned long)after.length);
			gFailures++;
		}
		NSString *openFence = @"```\ncode\nmore";
		NSRange unclosed = NVMarkdownHighlightRangeForEdit(openFence, NSMakeRange(9, 1));
		if (unclosed.location != 0 || unclosed.length != [openFence length]) {
			fprintf(stderr, "FAIL an unclosed fence runs to the end of the note\n");
			gFailures++;
		}
		NSString *critic = @"keep\n{++\nadded\n++}\nend";
		NSRange marked = NVMarkdownHighlightRangeForEdit(critic, NSMakeRange(9, 1));
		if (marked.location != 5 || marked.length != 14) {
			fprintf(stderr, "FAIL a critic span is colored with the edit\n  got %lu,%lu\n", (unsigned long)marked.location, (unsigned long)marked.length);
			gFailures++;
		}
		NSString *defined = @"hello\n[id]: http://x\nthere";
		NSRange definition = NVMarkdownHighlightRangeForEdit(defined, NSMakeRange(6, 1));
		NSRange plain = NVMarkdownHighlightRangeForEdit(defined, NSMakeRange(0, 1));
		if (definition.location != 0 || definition.length != [defined length] || plain.location != 0 || plain.length != 6) {
			fprintf(stderr, "FAIL a link definition recolors the note, and a plain line does not\n");
			gFailures++;
		}
		NSRange typed = NVMarkdownHighlightUnionEdit(NSMakeRange(NSNotFound, 0), NSMakeRange(10, 1), 1, 100);
		NSRange shifted = NVMarkdownHighlightUnionEdit(NSMakeRange(100, 5), NSMakeRange(50, 1), 1, 200);
		NSRange insideEdit = NVMarkdownHighlightUnionEdit(NSMakeRange(100, 5), NSMakeRange(102, 1), 1, 200);
		NSRange adjacent = NVMarkdownHighlightUnionEdit(NSMakeRange(100, 1), NSMakeRange(101, 1), 1, 200);
		if (typed.location != 10 || typed.length != 1 || shifted.location != 50 || shifted.length != 56 ||
			insideEdit.location != 100 || insideEdit.length != 6 || adjacent.location != 100 || adjacent.length != 2) {
			fprintf(stderr, "FAIL edits combine into one coloring range\n  typed %lu,%lu shifted %lu,%lu inside %lu,%lu adjacent %lu,%lu\n",
				(unsigned long)typed.location, (unsigned long)typed.length,
				(unsigned long)shifted.location, (unsigned long)shifted.length,
				(unsigned long)insideEdit.location, (unsigned long)insideEdit.length,
				(unsigned long)adjacent.location, (unsigned long)adjacent.length);
			gFailures++;
		}
		NSArray *onlySecond = NVMarkdownHighlightSpansInRange(@"==no==\n==yes==", NSMakeRange(7, 7));
		NSString *second = [onlySecond count] ? [@"==no==\n==yes==" substringWithRange:[[[onlySecond objectAtIndex:0] objectForKey:@"content"] rangeValue]] : nil;
		if ([onlySecond count] != 1 || ![second isEqualToString:@"yes"]) {
			fprintf(stderr, "FAIL a ranged mark scan stays inside the range\n");
			gFailures++;
		}
	}

	expectColors([NVThemeStyle highlightStyleFromThemeStyle:nil], 0, 0, 0, 1, 1, 0, "a theme with no highlight uses black on yellow");

	NVThemeStyle *backgroundOnly = [NVThemeStyle styleWithDictionary:[NSDictionary dictionaryWithObject:@"#00ff00" forKey:@"backgroundColor"]];
	expectColors([NVThemeStyle highlightStyleFromThemeStyle:backgroundOnly], 0, 0, 0, 0, 1, 0, "a background-only highlight keeps black text");

	NVThemeStyle *foregroundOnly = [NVThemeStyle styleWithDictionary:[NSDictionary dictionaryWithObject:@"#ffffff" forKey:@"foregroundColor"]];
	expectColors([NVThemeStyle highlightStyleFromThemeStyle:foregroundOnly], 1, 1, 1, 1, 1, 0, "a foreground-only highlight keeps the yellow background");

	NVThemeStyle *both = [NVThemeStyle styleWithDictionary:[NSDictionary dictionaryWithObjectsAndKeys:@"#ff0000", @"foregroundColor", @"#0000ff", @"backgroundColor", nil]];
	expectColors([NVThemeStyle highlightStyleFromThemeStyle:both], 1, 0, 0, 0, 0, 1, "a theme highlight replaces both colors");

	[pool drain];
	if (gFailures) {
		fprintf(stderr, "%d failed\n", gFailures);
		return 1;
	}
	printf("test-ok\n");
	return 0;
}
