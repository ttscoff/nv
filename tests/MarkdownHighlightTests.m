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
