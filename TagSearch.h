#import <Foundation/Foundation.h>

BOOL NVSearchWordIsTagPrefix(NSString *word, NSString **prefixOut);
BOOL NVQueryContainsTagWord(NSString *query);
BOOL NVQueryNewlyHasTagWord(NSString *previousQuery, NSString *query);
NSString *NVStringByRemovingTagWords(NSString *query);
NSString *NVHighlightStringByRemovingTagWords(NSString *query);
NSString *NVStringByTrimmingHighlightTerm(NSString *term);
BOOL NVTagTitlesMatchPrefix(NSArray *tagTitles, NSString *prefix);
BOOL NVNoteMatchesTagWord(NSArray *tagTitles, const char *title, const char *body, const char *labels, NSString *prefix, const char *literal);
