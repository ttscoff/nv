#import <Foundation/Foundation.h>

/* 1-based source line of a caret index, matching cmark's data-sourcepos lines. */
static inline NSUInteger NVSourceLineForIndex(NSString *text, NSUInteger index) {
	NSUInteger length = [text length];
	NSUInteger line = 1;
	NSRange search;
	NSRange found;
	if (!text)
		return 0;
	if (index > length)
		index = length;
	search = NSMakeRange(0, index);
	while (search.length > 0) {
		found = [text rangeOfString:@"\n" options:0 range:search];
		if (found.location == NSNotFound)
			break;
		line++;
		search.location = NSMaxRange(found);
		search.length = index - search.location;
	}
	return line;
}

/* Source positions are for scrolling the preview. They should not travel with shared text. */
static inline NSString *NVHTMLWithoutSourcePositions(NSString *html) {
	NSRegularExpression *expression;
	if (![html length] || [html rangeOfString:@"data-sourcepos"].location == NSNotFound)
		return html;
	expression = [NSRegularExpression regularExpressionWithPattern:@"\\sdata-sourcepos=\"[^\"]*\"" options:0 error:NULL];
	if (!expression)
		return html;
	return [expression stringByReplacingMatchesInString:html options:0 range:NSMakeRange(0, [html length]) withTemplate:@""];
}
