#import <Foundation/Foundation.h>
#import <stdio.h>
#import "ListPivot.h"

static int gFailures = 0;

static void expectTrue(BOOL cond, const char *what) {
	if (!cond) {
		fprintf(stderr, "FAIL %s\n", what);
		gFailures++;
	}
}

int main(void) {
	expectTrue(!NVShouldRecordListPivot(0),
			   "no selection means no list pivot");
	expectTrue(NVShouldRecordListPivot(1),
			   "a selected note records a pivot");
	expectTrue(NVShouldRecordListPivot(3),
			   "multiple selected notes still record a pivot");
	if (gFailures) {
		fprintf(stderr, "%d list pivot tests failed\n", gFailures);
		return 1;
	}
	printf("list pivot tests passed\n");
	return 0;
}
