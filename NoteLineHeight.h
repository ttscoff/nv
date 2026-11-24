#import <Foundation/Foundation.h>

/* Pref sentinel: follow textView.lineHeight from the current theme, or 1.0. */
static const CGFloat NVNoteLineHeightFollowTheme = 0.0;
static const CGFloat NVNoteLineHeightSmall = 0.9;
static const CGFloat NVNoteLineHeightRegular = 1.0;
static const CGFloat NVNoteLineHeightLarge = 1.4;
static const CGFloat NVNoteLineHeightMinimum = 0.5;
static const CGFloat NVNoteLineHeightMaximum = 3.0;

static inline BOOL NVNoteLineHeightIsFollowTheme(CGFloat pref) {
	return pref <= 0.0;
}

static inline CGFloat NVClampNoteLineHeight(CGFloat value) {
	if (value < NVNoteLineHeightMinimum)
		return NVNoteLineHeightMinimum;
	if (value > NVNoteLineHeightMaximum)
		return NVNoteLineHeightMaximum;
	return value;
}

/* prefOrSentinel is the stored pref (0 = follow theme). themeOrZero is the
   theme's textView.lineHeight, or 0 when unset / out of range upstream. */
static inline CGFloat NVResolvedNoteLineHeight(CGFloat prefOrSentinel, CGFloat themeOrZero) {
	if (NVNoteLineHeightIsFollowTheme(prefOrSentinel)) {
		if (themeOrZero >= NVNoteLineHeightMinimum && themeOrZero <= NVNoteLineHeightMaximum)
			return themeOrZero;
		return NVNoteLineHeightRegular;
	}
	return NVClampNoteLineHeight(prefOrSentinel);
}
