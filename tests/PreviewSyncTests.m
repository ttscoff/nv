#import <Foundation/Foundation.h>
#import "PreviewSync.h"
#include <stdio.h>

int main(void) {
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	NSString *stripped;
	if (NVSourceLineForIndex(nil, 0) != 0) return 1;
	if (NVSourceLineForIndex(@"", 0) != 1) return 1;
	if (NVSourceLineForIndex(@"hello", 5) != 1) return 1;
	if (NVSourceLineForIndex(@"a\nb", 0) != 1) return 1;
	if (NVSourceLineForIndex(@"a\nb", 1) != 1) return 1;
	if (NVSourceLineForIndex(@"a\nb", 2) != 2) return 1;
	if (NVSourceLineForIndex(@"a\nb\n", 4) != 3) return 1;
	stripped = NVHTMLWithoutSourcePositions(@"<p data-sourcepos=\"1:1-1:5\">Hi</p>");
	if (![stripped isEqualToString:@"<p>Hi</p>"]) {
		fprintf(stderr, "stripped: %s\n", [stripped UTF8String]);
		return 1;
	}
	if (![NVHTMLWithoutSourcePositions(@"<p>Hi</p>") isEqualToString:@"<p>Hi</p>"]) return 1;
	if (NVHTMLWithoutSourcePositions(nil) != nil) return 1;
	printf("test-ok\n");
	[pool drain];
	return 0;
}
