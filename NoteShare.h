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

static inline NSString *NVPreviewShareFileName(NSString *title) {
	NSString *trimmed = [title stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
	if (![trimmed length])
		return @"note.html";
	NSMutableString *name = [NSMutableString stringWithString:trimmed];
	[name replaceOccurrencesOfString:@"/" withString:@" " options:0 range:NSMakeRange(0, [name length])];
	[name replaceOccurrencesOfString:@":" withString:@" " options:0 range:NSMakeRange(0, [name length])];
	trimmed = [name stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
	if (![trimmed length])
		return @"note.html";
	if ([[trimmed lowercaseString] hasSuffix:@".html"])
		return trimmed;
	return [trimmed stringByAppendingString:@".html"];
}

/* Drops a missing or empty rich-text item. The file stays. */
static inline NSArray *NVPreviewShareObjects(NSURL *fileURL, NSAttributedString *richText) {
	NSMutableArray *items = [NSMutableArray array];
	if (richText && [[richText string] length] > 0)
		[items addObject:richText];
	if (fileURL)
		[items addObject:fileURL];
	return [items count] ? items : nil;
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

static inline NSArray *NVPreviewShareItems(NSString *html, NSString *title, NSString *directory) {
	if (![html length] || ![directory length])
		return nil;
	NSFileManager *manager = [NSFileManager defaultManager];
	if (![manager createDirectoryAtPath:directory withIntermediateDirectories:YES attributes:nil error:NULL])
		return nil;
	NSString *path = [directory stringByAppendingPathComponent:NVPreviewShareFileName(title)];
	if (![html writeToFile:path atomically:YES encoding:NSUTF8StringEncoding error:NULL])
		return nil;
	NSData *data = [html dataUsingEncoding:NSUTF8StringEncoding];
	NSAttributedString *rich = [[[NSAttributedString alloc] initWithHTML:data documentAttributes:NULL] autorelease];
	return NVPreviewShareObjects([NSURL fileURLWithPath:path], rich);
}
