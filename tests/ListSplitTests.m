#import <Foundation/Foundation.h>
#import <stdio.h>
#import "ListSplit.h"

static int gFailures = 0;

static void expectTrue(BOOL cond, const char *what) {
	if (!cond) {
		fprintf(stderr, "FAIL %s\n", what);
		gFailures++;
	}
}

int main(void) {
	expectTrue(!NVShouldApplyDefaultListDimension(YES, 100.0), "a saved 100 point list stays");
	expectTrue(!NVShouldApplyDefaultListDimension(YES, 125.0), "a saved 125 point list stays");
	expectTrue(!NVShouldApplyDefaultListDimension(YES, 150.0), "a saved 150 point list stays");
	expectTrue(!NVShouldApplyDefaultListDimension(YES, 199.0), "a saved list just under 200 stays");
	expectTrue(!NVShouldApplyDefaultListDimension(YES, 225.0), "a saved 225 point list stays");
	expectTrue(!NVShouldApplyDefaultListDimension(YES, 80.0), "a saved list at the minimum stays");
	expectTrue(NVShouldApplyDefaultListDimension(NO, 100.0), "a first launch under 200 gets the default");
	expectTrue(NVShouldApplyDefaultListDimension(NO, 160.0), "a placeholder pane under 200 gets the default");
	expectTrue(!NVShouldApplyDefaultListDimension(NO, 200.0), "a first launch already at 200 is left alone");
	expectTrue(!NVShouldApplyDefaultListDimension(NO, 225.0), "a first launch over 200 is left alone");
	expectTrue(fabs(NVClampedListDimension(600.0, 716.0, 120.0) - 596.0) < 0.01,
			   "a maxed list in a 716pt split leaves 120 for the body");
	expectTrue(fabs(NVClampedListDimension(150.0, 800.0, 120.0) - 150.0) < 0.01,
			   "a safe saved height is left alone");
	expectTrue(fabs(NVClampedListDimension(600.0, 500.0, 120.0) - 380.0) < 0.01,
			   "a short split clamps the list to split minus body min");
	expectTrue(NVClampedListDimension(50.0, 800.0, 120.0) >= 80.0,
			   "below the list minimum rises to 80");
	expectTrue(NVClampedListDimension(700.0, 2000.0, 120.0) <= 600.0,
			   "above the list maximum falls to 600");
	expectTrue(NVShouldKeepSelectionForEmptyListClick(-1, YES),
			   "empty area with a selection keeps it");
	expectTrue(!NVShouldKeepSelectionForEmptyListClick(0, YES),
			   "a real row is not intercepted");
	expectTrue(!NVShouldKeepSelectionForEmptyListClick(-1, NO),
			   "empty area with no selection is not intercepted");
	if (gFailures) {
		fprintf(stderr, "%d list split tests failed\n", gFailures);
		return 1;
	}
	printf("list split tests passed\n");
	return 0;
}
