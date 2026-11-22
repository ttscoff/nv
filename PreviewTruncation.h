#import <Foundation/Foundation.h>

/* Front matter at the very start of a note. YAML is a block wrapped in ---
   lines. MultiMarkdown is key: value lines through the blank line that ends
   them. The range includes that closing line. NSNotFound when the note has none. */
NSRange NVFrontMatterRange(NSString *text);

/* How far from the start an edit in the opening should repaint, so a front
   matter block that grows or disappears does not leave stale coloring. */
NSUInteger NVFrontMatterHighlightPrefix(NSString *text);

/* The opening of a note body as the notes list shows it: the first characters,
   with line breaks and other layout characters turned into spaces. */
@interface NSString (PreviewTruncation)

/* At most characterCount characters from the start of the receiver, cut so a
   composed character or surrogate pair stays whole. A count of zero or an
   empty receiver gives an empty string. */
- (NSString *)truncatedPreviewStringOfLength:(NSUInteger)characterCount;

@end
