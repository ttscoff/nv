#import <Foundation/Foundation.h>
#import <stdio.h>
#import "ListSelection.h"

static int gFailures = 0;

static void expectStyle(NVListSelectionStyle got, NVListSelectionStyle want, const char *what) {
	if (got != want) {
		fprintf(stderr, "FAIL %s (got %d, want %d)\n", what, (int)got, (int)want);
		gFailures++;
	}
}

int main(void) {
	expectStyle(NVListSelectionStyleForRow(YES, NO), NVListSelectionSystem,
		"a one-line list with the default theme uses the system selection bar");
	expectStyle(NVListSelectionStyleForRow(YES, YES), NVListSelectionTheme,
		"a one-line list with a theme selection color paints that color");
	expectStyle(NVListSelectionStyleForRow(NO, NO), NVListSelectionSourceList,
		"a stacked list with the default theme keeps the source-list selection");
	expectStyle(NVListSelectionStyleForRow(NO, YES), NVListSelectionSourceList,
		"a stacked list with a theme keeps the source-list selection");
	if (!gFailures)
		printf("test-ok\n");
	return gFailures ? 1 : 0;
}
