#import <Foundation/Foundation.h>
#import <stdio.h>
#import "ListEnter.h"

static int gFailures = 0;

static void expectTrue(BOOL cond, const char *what) {
	if (!cond) {
		fprintf(stderr, "FAIL %s\n", what);
		gFailures++;
	}
}

int main(void) {
	expectTrue(NVListEnterStartsRename(YES, YES), "Enter renames when the preference is on and a title column is visible");
	expectTrue(!NVListEnterStartsRename(NO, YES), "Enter edits the note when the preference is off");
	expectTrue(!NVListEnterStartsRename(YES, NO), "Enter edits the note when there is no title column");
	expectTrue(!NVListEnterStartsRename(NO, NO), "Enter edits the note when renaming is off and there is no title column");
	return gFailures ? 1 : 0;
}
