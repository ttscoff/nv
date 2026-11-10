#import <Foundation/Foundation.h>

/* The opening of a note body as the notes list shows it: the first characters,
   with line breaks and other layout characters turned into spaces. */
@interface NSString (PreviewTruncation)

/* At most characterCount characters from the start of the receiver, cut so a
   composed character or surrogate pair stays whole. A count of zero or an
   empty receiver gives an empty string. */
- (NSString *)truncatedPreviewStringOfLength:(NSUInteger)characterCount;

@end
