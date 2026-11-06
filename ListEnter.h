#import <Foundation/Foundation.h>

/* Enter in the notes list renames the selected title only when that preference
   is on and a title column is visible. Otherwise it moves to the editor, the
   same as Tab. */
static inline BOOL NVListEnterStartsRename(BOOL preferenceOn, BOOL titleColumnVisible) {
	return preferenceOn && titleColumnVisible;
}
