#import <Foundation/Foundation.h>

// Symmetric characters (quotes, backticks) return 1 and set match to themselves.
// Openers return 2 and closers return 0. Anything else returns -1.
NSInteger NVPairedCharacterCode(unichar character, NSString **matchString);

typedef enum {
	NVAutoPairAsTyped = 0,
	NVAutoPairWrapOrInsert = 1,
	NVAutoPairTypeOver = 2
} NVAutoPairAction;

// before/after are the current line on either side of the caret. after includes a selection.
// A backtick inserts a pair, and the next backtick types over that closer. A backtick typed
// immediately after another backtick is left as a single character, so ``` stays a fence.
NVAutoPairAction NVAutoPairActionForInsert(NSString *insertString, NSString *opposite, NSInteger pairCode, NSString *before, NSString *after, NSUInteger selectionLength);

BOOL NVCharactersFormEmptyPair(unichar left, unichar right);
