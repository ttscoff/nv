#import <Foundation/Foundation.h>

/* Words to mark while a search is underway.
   The mark follows the search that found the notes. It stays while moving
   between those notes, including after the field shows a note title.
   It ends when the note body takes over, and when the search is cleared. */
static inline NSString *NVSearchHighlightQuery(BOOL bodyIsFirstResponder,
											   BOOL isFilteringFromTyping,
											   BOOL typedStringIsCached,
											   BOOL searchFieldIsEditing,
											   NSRange searchFieldSelection,
											   NSString *typedString,
											   NSString *fieldString,
											   NSString *noteTitle) {
	if (bodyIsFirstResponder)
		return nil;

	if (isFilteringFromTyping) {
		if (![fieldString length])
			return nil;
		if ([noteTitle length] && [fieldString isEqualToString:noteTitle] &&
			searchFieldSelection.length > 0 && searchFieldSelection.location > 0 &&
			NSMaxRange(searchFieldSelection) == [noteTitle length])
			return [noteTitle substringToIndex:searchFieldSelection.location];
		return fieldString;
	}

	/* Selecting a note puts its title in the field. The words that found it
	   stay marked until the note body takes focus. */
	BOOL wholeTitleSelected = [noteTitle length] && [fieldString isEqualToString:noteTitle] &&
		searchFieldSelection.location == 0 && searchFieldSelection.length == [fieldString length];
	if (typedStringIsCached && [typedString length] && (!searchFieldIsEditing || wholeTitleSelected))
		return typedString;

	if (!searchFieldIsEditing)
		return nil;

	if (wholeTitleSelected)
		return nil;
	if (typedStringIsCached && [typedString length])
		return typedString;
	return [fieldString length] ? fieldString : nil;
}

static inline CGFloat NVSearchHighlightLuminance(CGFloat red, CGFloat green, CGFloat blue) {
	return (0.2126 * red) + (0.7152 * green) + (0.0722 * blue);
}

static inline CGFloat NVSearchHighlightSaturation(CGFloat red, CGFloat green, CGFloat blue) {
	CGFloat max = red > green ? red : green;
	if (blue > max)
		max = blue;
	CGFloat min = red < green ? red : green;
	if (blue < min)
		min = blue;
	if (max <= 0.0001)
		return 0;
	return (max - min) / max;
}

/* The mark drawn for a search hit.
   Temporary attributes drop an alpha component, so a translucent highlighter has to be
   flattened first. Flattening it onto the light color stored in Settings turns it into
   a gray bar on a dark note. Mix it with the background that is actually behind the
   text, and keep the chosen color when that mix would go gray. */
static inline void NVSearchHighlightPaint(CGFloat highlightRed, CGFloat highlightGreen, CGFloat highlightBlue, CGFloat highlightAlpha,
										  CGFloat backgroundRed, CGFloat backgroundGreen, CGFloat backgroundBlue,
										  CGFloat *paintedRed, CGFloat *paintedGreen, CGFloat *paintedBlue, BOOL *whiteInk) {
	if (highlightAlpha < 0)
		highlightAlpha = 0;
	if (highlightAlpha > 1)
		highlightAlpha = 1;
	CGFloat red = (highlightRed * highlightAlpha) + (backgroundRed * (1.0 - highlightAlpha));
	CGFloat green = (highlightGreen * highlightAlpha) + (backgroundGreen * (1.0 - highlightAlpha));
	CGFloat blue = (highlightBlue * highlightAlpha) + (backgroundBlue * (1.0 - highlightAlpha));
	CGFloat backdrop = NVSearchHighlightLuminance(backgroundRed, backgroundGreen, backgroundBlue);
	if (highlightAlpha < 0.999 && backdrop < 0.45 &&
		NVSearchHighlightSaturation(highlightRed, highlightGreen, highlightBlue) > 0.15 &&
		NVSearchHighlightSaturation(red, green, blue) < 0.18) {
		red = highlightRed;
		green = highlightGreen;
		blue = highlightBlue;
	}
	if (paintedRed)
		*paintedRed = red;
	if (paintedGreen)
		*paintedGreen = green;
	if (paintedBlue)
		*paintedBlue = blue;
	if (whiteInk)
		*whiteInk = NVSearchHighlightLuminance(red, green, blue) <= 0.55;
}
