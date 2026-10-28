#import <Foundation/Foundation.h>

BOOL NVSearchWordIsTagPrefix(NSString *word, NSString **prefixOut);
BOOL NVQueryContainsTagWord(NSString *query);
NSString *NVStringByRemovingTagWords(NSString *query);
NSString *NVHighlightStringByRemovingTagWords(NSString *query);
BOOL NVTagTitlesMatchPrefix(NSArray *tagTitles, NSString *prefix);
