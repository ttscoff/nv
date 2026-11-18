#import <Foundation/Foundation.h>

// Symmetric characters (quotes, backticks, asterisks, underscores) return 1 and set match to themselves.
// Openers return 2 and closers return 0.
// Selection marks (= and ~) return 3: they wrap a selection and otherwise insert as typed.
// Anything else returns -1.
NSInteger NVPairedCharacterCode(unichar character, NSString **matchString);

typedef enum {
	NVAutoPairAsTyped = 0,
	NVAutoPairWrapOrInsert = 1,
	NVAutoPairTypeOver = 2
} NVAutoPairAction;

// before/after are the current line on either side of the caret. after includes a selection.
// A symmetric mark inserts a pair, and the next matching mark types over that closer.
// A backtick, asterisk, or underscore typed immediately after the same mark stays a single
// character, so ``` stays a fence and *** stays a run.
NVAutoPairAction NVAutoPairActionForInsert(NSString *insertString, NSString *opposite, NSInteger pairCode, NSString *before, NSString *after, NSUInteger selectionLength);

// == and ~~ when a selection is wrapped. Nil for an ordinary one-character pair.
NSString *NVAutoPairSelectionAffix(NSString *insertString);

BOOL NVCharactersFormEmptyPair(unichar left, unichar right);
