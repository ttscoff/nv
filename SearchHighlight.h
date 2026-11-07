#import <Foundation/Foundation.h>

/* Words to mark while a search is underway.
   The mark follows the search field. It stays while that field is where the
   user is typing, including when autocomplete has filled in the note title.
   It ends when the note body takes over, when the search is cleared, and when
   a note is opened with the field only displaying its title. */
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

	if (!searchFieldIsEditing)
		return nil;

	/* The whole title being selected means the field is labeling the note.
	   Autocomplete leaves only the untyped tail selected, and that is still a search. */
	BOOL wholeTitleSelected = [noteTitle length] && [fieldString isEqualToString:noteTitle] &&
		searchFieldSelection.location == 0 && searchFieldSelection.length == [fieldString length];
	if (wholeTitleSelected)
		return nil;
	if (typedStringIsCached && [typedString length])
		return typedString;
	return [fieldString length] ? fieldString : nil;
}
