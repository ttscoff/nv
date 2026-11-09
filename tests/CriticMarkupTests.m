#import <AppKit/AppKit.h>
#import <stdio.h>
#import "CriticMarkup.h"
#import "NVTheme.h"

static int gFailures = 0;

static void expectTrue(BOOL cond, const char *what) {
	if (!cond) {
		fprintf(stderr, "FAIL %s\n", what);
		gFailures++;
	}
}

static NSRange rangeForKey(NSDictionary *span, NSString *key) {
	NSValue *value = [span objectForKey:key];
	if (!value)
		return NSMakeRange(NSNotFound, 0);
	return [value rangeValue];
}

static void expectCount(NSString *string, NSUInteger count, const char *what) {
	NSUInteger got = [NVCriticMarkupSpans(string) count];
	if (got != count) {
		fprintf(stderr, "FAIL %s\n  got %lu spans, want %lu\n", what, (unsigned long)got, (unsigned long)count);
		gFailures++;
	}
}

static void expectCritic(NSString *string, NSUInteger index, NSString *type, NSString *content, const char *what) {
	NSArray *spans = NVCriticMarkupSpans(string);
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
	NSString *gotType = [span objectForKey:@"type"];
	if (!got || ![got isEqualToString:content] || ![gotType isEqualToString:type] || opener.length != 3 || closer.length != 3 ||
		closer.location != NSMaxRange(body)) {
		fprintf(stderr, "FAIL %s\n  type: %s content: %s\n", what,
				gotType ? [gotType UTF8String] : "(nil)",
				got ? [got UTF8String] : "(nil)");
		gFailures++;
	}
}

static BOOL colorEquals(NSColor *color, CGFloat red, CGFloat green, CGFloat blue) {
	NSColor *rgb = [color colorUsingColorSpace:[NSColorSpace sRGBColorSpace]];
	CGFloat r = -1, g = -1, b = -1, a = -1;
	if (!rgb)
		return NO;
	[rgb getRed:&r green:&g blue:&b alpha:&a];
	return fabs(r - red) < 0.02 && fabs(g - green) < 0.02 && fabs(b - blue) < 0.02 && fabs(a - 1.0) < 0.01;
}

static void expectForeground(NVThemeStyle *style, CGFloat red, CGFloat green, CGFloat blue, const char *what) {
	if (!style || !colorEquals([style foregroundColor], red, green, blue)) {
		fprintf(stderr, "FAIL %s\n", what);
		gFailures++;
	}
}

int main(int argc, const char *argv[]) {
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

	expectCritic(@"{++added++}", 0, @"addition", @"added", "an insertion");
	expectCritic(@"{--gone--}", 0, @"deletion", @"gone", "a deletion");
	expectCritic(@"{==hi==}", 0, @"highlight", @"hi", "a critic highlight");
	expectCritic(@"{>>note<<}", 0, @"comment", @"note", "a comment");
	expectCritic(@"{~~old~>new~~}", 0, @"substitution", @"new", "the new half of a substitution");
	expectCount(@"==hi==", 0, "a plain highlight is not critic markup");
	expectCount(@"{++no", 0, "an unclosed mark is left alone");
	expectCount(@"`{++code++}`", 0, "inline code is left alone");
	expectCount(@"```\n{++code++}\n```\n{--yes--}", 1, "a fenced block is left alone");
	expectCritic(@"```\n{++code++}\n```\n{--yes--}", 0, @"deletion", @"yes", "a deletion after a fence");
	expectCount(@"    {++indent++}", 0, "indented code is left alone");
	expectCritic(@"    - {++item++}", 0, @"addition", @"item", "a mark in a list");
	expectCount(@"{++one++}{--two--}", 2, "two marks in a row");

	NSArray *subs = NVCriticMarkupSpans(@"{~~old~>new~~}");
	if ([subs count] == 1) {
		NSDictionary *span = [subs objectAtIndex:0];
		NSRange old = rangeForKey(span, @"old");
		NSRange separator = rangeForKey(span, @"separator");
		NSString *oldText = old.location == NSNotFound ? nil : [@"{~~old~>new~~}" substringWithRange:old];
		NSString *sepText = separator.location == NSNotFound ? nil : [@"{~~old~>new~~}" substringWithRange:separator];
		expectTrue([oldText isEqualToString:@"old"] && [sepText isEqualToString:@"~>"], "a substitution keeps the old text");
	} else {
		fprintf(stderr, "FAIL a substitution keeps the old text\n");
		gFailures++;
	}

	expectTrue(NVCriticKindStrikethrough(@"deletion") == YES, "deletions are crossed out");
	expectTrue(NVCriticKindStrikethrough(@"addition") == NO, "insertions are not crossed out");
	expectTrue(NVCriticKindUnderline(@"addition") == YES, "insertions are underlined");
	expectTrue(NVCriticKindUnderline(@"deletion") == NO, "deletions are not underlined");

	expectForeground([NVThemeStyle criticStyleForKind:@"addition" fromThemeStyle:nil], 0.18, 0.62, 0.27, "an insertion defaults to green");
	expectForeground([NVThemeStyle criticStyleForKind:@"deletion" fromThemeStyle:nil], 0.80, 0.20, 0.20, "a deletion defaults to red");
	expectForeground([NVThemeStyle criticStyleForKind:@"comment" fromThemeStyle:nil], 0.00, 0.64, 0.71, "a comment defaults to cyan");
	NVThemeStyle *highlight = [NVThemeStyle criticStyleForKind:@"highlight" fromThemeStyle:nil];
	expectForeground(highlight, 0, 0, 0, "a highlight defaults to black text");
	expectTrue(colorEquals([highlight backgroundColor], 1, 1, 0), "a highlight defaults to a yellow background");

	NVThemeStyle *themed = [NVThemeStyle styleWithDictionary:[NSDictionary dictionaryWithObject:@"#a2bf8a" forKey:@"foregroundColor"]];
	NVThemeStyle *used = [NVThemeStyle criticStyleForKind:@"addition" fromThemeStyle:themed];
	expectForeground(used, 0.635, 0.749, 0.541, "a theme addition color is used");
	expectTrue([used backgroundColor] == nil, "a foreground-only theme addition has no background");

	NVThemeStyle *gold = [NVThemeStyle styleWithDictionary:[NSDictionary dictionaryWithObject:@"#eccc87" forKey:@"foregroundColor"]];
	NVThemeStyle *themedHighlight = [NVThemeStyle criticStyleForKind:@"highlight" fromThemeStyle:gold];
	expectForeground(themedHighlight, 0.925, 0.800, 0.529, "a theme highlight color is used");
	expectTrue([themedHighlight backgroundColor] == nil, "a foreground-only theme highlight stays on the note background");
	expectTrue([NVThemeStyle criticStyleForKind:@"substitution" fromThemeStyle:nil] == nil, "a substitution has no single default color");

	[pool drain];
	if (gFailures) {
		fprintf(stderr, "%d failed\n", gFailures);
		return 1;
	}
	printf("test-ok\n");
	return 0;
}
