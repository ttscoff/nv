#import <Foundation/Foundation.h>

/* A viewing-location pivot is only meaningful when a note is selected.
   With an empty selection, restoring from a short filtered list can
   edge-snap into the oldest end of the full list after clearing search. */
static inline BOOL NVShouldRecordListPivot(NSUInteger selectedCount) {
	return selectedCount > 0;
}
