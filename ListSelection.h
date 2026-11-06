#import <Foundation/Foundation.h>

typedef enum {
	NVListSelectionSystem = 0,
	NVListSelectionSourceList = 1,
	NVListSelectionTheme = 2
} NVListSelectionStyle;

/* One-line rows (vertical layout, or a stacked list with no preview) turn
   selection drawing off when a theme supplies a highlight, so that color is
   not covered by the system accent. With no theme color the selected title
   stays white and the row has nothing behind it. */
static inline NVListSelectionStyle NVListSelectionStyleForRow(BOOL oneRow, BOOL hasThemeSelectionColor) {
	if (!oneRow)
		return NVListSelectionSourceList;
	return hasThemeSelectionColor ? NVListSelectionTheme : NVListSelectionSystem;
}
