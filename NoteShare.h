#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>

/* The editor share item. A body selection wins. An empty result is not shared. */
static inline NSString *NVEditorShareText(NSString *body, NSRange selectedRange) {
	if (!body)
		body = @"";
	if (selectedRange.length > 0 && selectedRange.location != NSNotFound && NSMaxRange(selectedRange) <= [body length]) {
		NSString *selection = [body substringWithRange:selectedRange];
		return [selection length] ? selection : nil;
	}
	return [body length] ? body : nil;
}

static inline NSString *NVShareHTMLEscapedText(NSString *string) {
	if (!string)
		string = @"";
	NSMutableString *escaped = [NSMutableString stringWithString:string];
	[escaped replaceOccurrencesOfString:@"&" withString:@"&amp;" options:0 range:NSMakeRange(0, [escaped length])];
	[escaped replaceOccurrencesOfString:@"<" withString:@"&lt;" options:0 range:NSMakeRange(0, [escaped length])];
	[escaped replaceOccurrencesOfString:@">" withString:@"&gt;" options:0 range:NSMakeRange(0, [escaped length])];
	[escaped replaceOccurrencesOfString:@"\"" withString:@"&quot;" options:0 range:NSMakeRange(0, [escaped length])];
	return escaped;
}

/* Updates the stored preview document after an in-place render. No markers means leave it alone. */
static inline NSString *NVPreviewHTMLByReplacingShareContent(NSString *html, NSString *content, NSString *title) {
	if (![html length])
		return html;
	NSString *startMark = @"<!--nv-content-start-->";
	NSString *endMark = @"<!--nv-content-end-->";
	NSRange start = [html rangeOfString:startMark];
	NSRange end = [html rangeOfString:endMark];
	if (start.location == NSNotFound || end.location == NSNotFound || end.location < NSMaxRange(start))
		return html;
	NSMutableString *updated = [NSMutableString stringWithString:html];
	NSRange contentRange = NSMakeRange(NSMaxRange(start), end.location - NSMaxRange(start));
	[updated replaceCharactersInRange:contentRange withString:content ?: @""];
	NSRange titleOpen = [updated rangeOfString:@"<title>" options:NSCaseInsensitiveSearch];
	if (titleOpen.location != NSNotFound) {
		NSRange titleClose = [updated rangeOfString:@"</title>" options:NSCaseInsensitiveSearch range:NSMakeRange(NSMaxRange(titleOpen), [updated length] - NSMaxRange(titleOpen))];
		if (titleClose.location != NSNotFound && titleClose.location >= NSMaxRange(titleOpen)) {
			NSRange titleText = NSMakeRange(NSMaxRange(titleOpen), titleClose.location - NSMaxRange(titleOpen));
			[updated replaceCharactersInRange:titleText withString:NVShareHTMLEscapedText(title)];
		}
	}
	return updated;
}

/* The preview share item. Notes apps paste this into the note instead of attaching a file. */
static inline NSAttributedString *NVPreviewShareRichText(NSString *html) {
	if (![html length])
		return nil;
	NSData *data = [html dataUsingEncoding:NSUTF8StringEncoding];
	NSAttributedString *rich = [[[NSAttributedString alloc] initWithHTML:data documentAttributes:NULL] autorelease];
	if (!rich || ![[rich string] length])
		return nil;
	return rich;
}
