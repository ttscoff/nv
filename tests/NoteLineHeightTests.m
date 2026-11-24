#import <Foundation/Foundation.h>
#import <stdio.h>
#import "NoteLineHeight.h"

static int gFailures = 0;

static void expectTrue(BOOL cond, const char *what) {
	if (!cond) {
		fprintf(stderr, "FAIL %s\n", what);
		gFailures++;
	}
}

static void expectNear(CGFloat got, CGFloat want, const char *what) {
	if (fabs(got - want) > 0.001) {
		fprintf(stderr, "FAIL %s\n  got: %g\n  want: %g\n", what, (double)got, (double)want);
		gFailures++;
	}
}

int main(void) {
	expectTrue(NVNoteLineHeightIsFollowTheme(0.0), "zero is follow theme");
	expectTrue(NVNoteLineHeightIsFollowTheme(NVNoteLineHeightFollowTheme),
			   "sentinel constant is follow theme");
	expectTrue(!NVNoteLineHeightIsFollowTheme(1.0), "1.0 is not follow theme");

	expectNear(NVNoteLineHeightSmall, 0.9, "small preset");
	expectNear(NVNoteLineHeightRegular, 1.0, "regular preset");
	expectNear(NVNoteLineHeightLarge, 1.4, "large preset");

	expectNear(NVClampNoteLineHeight(0.2), 0.5, "below minimum rises to 0.5");
	expectNear(NVClampNoteLineHeight(4.0), 3.0, "above maximum falls to 3.0");
	expectNear(NVClampNoteLineHeight(1.25), 1.25, "in-range value stays");

	expectNear(NVResolvedNoteLineHeight(0.0, 0.0), 1.0,
			   "follow theme with no theme key is 1.0");
	expectNear(NVResolvedNoteLineHeight(0.0, 1.35), 1.35,
			   "follow theme uses theme line height");
	expectNear(NVResolvedNoteLineHeight(0.0, 0.2), 1.0,
			   "follow theme ignores out-of-range theme value");
	expectNear(NVResolvedNoteLineHeight(1.2, 1.35), 1.2,
			   "explicit pref wins over theme");
	expectNear(NVResolvedNoteLineHeight(5.0, 1.35), 3.0,
			   "explicit pref is clamped");

	if (gFailures) {
		fprintf(stderr, "%d note line height tests failed\n", gFailures);
		return 1;
	}
	printf("note line height tests passed\n");
	return 0;
}
