#import <Foundation/Foundation.h>

// Reference-style Markdown links. A usage is [text][id] or ![alt][id] in the
// prose. A definition is a [id]: url line. Completion runs in the open bracket
// of either form: existing definition ids while writing a usage, and used ids
// that still have no definition while writing a new definition line.

typedef enum {
	NVLinkCompletionNone = 0,
	NVLinkCompletionUsage = 1,
	NVLinkCompletionDefinition = 2
} NVLinkCompletionKind;

typedef struct {
	NVLinkCompletionKind kind;
	NSRange range;
} NVLinkCompletionContext;

// range covers the partial id, from the character after '[' through location.
// A wiki link [[...]] is left as NVLinkCompletionNone so note-title completion
// can handle it. Ids inside fenced code are ignored.
NVLinkCompletionContext NVLinkCompletionContextAtLocation(NSString *document, NSUInteger location);

// Definitions for a usage, or unpaired usage ids for a definition. Matched by
// case-insensitive substring and sorted case-insensitively. Empty when none.
NSArray *NVLinkCompletionCandidates(NSString *document, NVLinkCompletionKind kind, NSString *query);

// The text to insert for a chosen id. A missing closer is included; an existing
// ']' is left alone so the caret can move past it.
NSString *NVLinkCompletionInsert(NSString *label, BOOL hasClosingBracket);

// YES when location sits just after the '[' that opens a usage or definition.
BOOL NVLinkReferenceJustOpened(NSString *document, NSUInteger location);

// Full ranges of [text][id], [text][], and ![alt][id] whose id matches a
// definition in the document. Matching ignores case and repeated whitespace.
// Usages and definitions inside fenced code are ignored. An id with no
// definition is left out.
NSArray *NVCompleteReferenceLinkRanges(NSString *document);
