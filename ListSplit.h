/* A note list the user has dragged stays at that size. The built-in height
   applies only on a first launch, when nothing was saved and the placeholder
   pane is still under 200 points. */
static inline BOOL NVShouldApplyDefaultListDimension(BOOL restored, CGFloat dimension) {
	return !restored && dimension < 200.0;
}
