/* A note list the user has dragged stays at that size. The built-in height
   applies only on a first launch, when nothing was saved and the placeholder
   pane is still under 200 points. */
static inline BOOL NVShouldApplyDefaultListDimension(BOOL restored, CGFloat dimension) {
	return !restored && dimension < 200.0;
}

static const CGFloat NVListMinimumDimension = 80.0;
static const CGFloat NVListMaximumDimension = 600.0;
/* Stacked layout: keep at least this much for the note body when restoring or
   resizing the list pane. */
static const CGFloat NVListBodyMinimumDimension = 120.0;

/* Cap a remembered list size so the other pane keeps bodyMin points. Always
   stays within NVListMinimumDimension…NVListMaximumDimension. */
static inline CGFloat NVClampedListDimension(CGFloat savedDimension, CGFloat splitSize, CGFloat bodyMin) {
	CGFloat limit = splitSize - bodyMin;
	CGFloat result = savedDimension;
	if (limit < NVListMinimumDimension)
		limit = NVListMinimumDimension;
	if (result > limit)
		result = limit;
	if (result > NVListMaximumDimension)
		result = NVListMaximumDimension;
	if (result < NVListMinimumDimension)
		result = NVListMinimumDimension;
	return result;
}

/* A click below the last row would clear the selection. Keep the current note
   when that empty area is hit and something is already selected. */
static inline BOOL NVShouldKeepSelectionForEmptyListClick(NSInteger row, BOOL hasSelection) {
	return row < 0 && hasSelection;
}
