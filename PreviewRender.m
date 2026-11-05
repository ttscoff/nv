#import "PreviewRender.h"

static void NVReplaceMermaidFences(NSMutableString *html, NSString *pattern) {
	NSRegularExpression *expression = [NSRegularExpression regularExpressionWithPattern:pattern
																				options:NSRegularExpressionCaseInsensitive
																				  error:NULL];
	if (!expression)
		return;
	NSArray *matches = [expression matchesInString:html options:0 range:NSMakeRange(0, [html length])];
	NSEnumerator *reverse = [matches reverseObjectEnumerator];
	NSTextCheckingResult *match;
	while ((match = [reverse nextObject])) {
		NSString *source = [html substringWithRange:[match rangeAtIndex:1]];
		NSString *block = [NSString stringWithFormat:@"<pre class=\"mermaid\">%@</pre>", source];
		[html replaceCharactersInRange:[match range] withString:block];
	}
}

static NSString *NVFileURLString(NSString *path) {
	if (![path length] || ![[NSFileManager defaultManager] fileExistsAtPath:path])
		return nil;
	return [[NSURL fileURLWithPath:path] absoluteString];
}

static NSString *NVScriptTagForFile(NSString *path) {
	NSString *url = NVFileURLString(path);
	if (!url)
		return nil;
	return [NSString stringWithFormat:@"<script src=\"%@\"></script>", url];
}

/* Inline a small script. The preview document is loaded with a nil base URL,
   which can refuse file-URL subresources, so the wiki linker has to travel
   inside the HTML. */
static NSString *NVInlineScriptForFile(NSString *path) {
	NSString *source = [NSString stringWithContentsOfFile:path encoding:NSUTF8StringEncoding error:NULL];
	if (![source length])
		return nil;
	NSMutableString *safe = [NSMutableString stringWithString:source];
	[safe replaceOccurrencesOfString:@"</" withString:@"<\\/" options:NSCaseInsensitiveSearch range:NSMakeRange(0, [safe length])];
	return [NSString stringWithFormat:@"<script>\n%@\n</script>", safe];
}

static BOOL NVHTMLContains(NSString *html, NSString *needle) {
	return [html rangeOfString:needle options:NSCaseInsensitiveSearch].location != NSNotFound;
}

NSString *NVPreviewHTMLPreparingContent(NSString *html, BOOL *needsMath, BOOL *needsMermaid, BOOL *needsWiki) {
	if (needsMath)
		*needsMath = NO;
	if (needsMermaid)
		*needsMermaid = NO;
	if (needsWiki)
		*needsWiki = NO;
	if (!html)
		return nil;

	NSMutableString *result = [NSMutableString stringWithString:html];
	NVReplaceMermaidFences(result, @"<pre\\b[^>]*>\\s*<code\\b[^>]*\\bclass=\"language-\\{?mermaid\\}?\"[^>]*>([\\s\\S]*?)</code></pre>");
	NVReplaceMermaidFences(result, @"<pre\\b[^>]*\\blang=\"\\{?mermaid\\}?\"[^>]*>\\s*<code\\b[^>]*>([\\s\\S]*?)</code></pre>");

	if (needsMath)
		*needsMath = NVHTMLContains(result, @"class=\"math ") || NVHTMLContains(result, @"class=\"math\"");
	if (needsMermaid)
		*needsMermaid = NVHTMLContains(result, @"class=\"mermaid\"");
	if (needsWiki)
		*needsWiki = [result rangeOfString:@"[[" options:NSLiteralSearch].location != NSNotFound;
	return result;
}

NSString *NVPreviewRendererScripts(NSString *assetDirectory, BOOL needsMath, BOOL needsMermaid, BOOL needsWiki) {
	if (!needsMath && !needsMermaid && !needsWiki)
		return nil;

	NSMutableString *scripts = [NSMutableString string];
	if (needsMath) {
		NSString *tag = NVScriptTagForFile([assetDirectory stringByAppendingPathComponent:@"mathjax/tex-svg.js"]);
		if (tag) {
			NSMutableString *config = [NSMutableString stringWithString:@"<script>window.MathJax={"];
			NSString *fontsURL = NVFileURLString([assetDirectory stringByAppendingPathComponent:@"mathjax/fonts"]);
			if (fontsURL)
				[config appendFormat:@"loader:{paths:{fonts:\"%@\"}},", fontsURL];
			[config appendString:@"tex:{inlineMath:[[\"\\\\(\",\"\\\\)\"]],displayMath:[[\"\\\\[\",\"\\\\]\"]]}};</script>\n"];
			[scripts appendString:config];
			[scripts appendString:tag];
			[scripts appendString:@"\n"];
		}
	}
	if (needsMermaid) {
		NSString *tag = NVScriptTagForFile([assetDirectory stringByAppendingPathComponent:@"mermaid.min.js"]);
		if (tag) {
			[scripts appendString:tag];
			[scripts appendString:@"\n<script>mermaid.initialize({startOnLoad:true,securityLevel:\"strict\"});</script>\n"];
		}
	}
	if (needsWiki) {
		NSString *tag = NVInlineScriptForFile([assetDirectory stringByAppendingPathComponent:@"wikilinks.js"]);
		if (tag) {
			[scripts appendString:tag];
			[scripts appendString:@"\n"];
		}
	}
	return [scripts length] ? scripts : nil;
}

NSString *NVPreviewHTMLForDisplay(NSString *html, NSString *assetDirectory) {
	BOOL needsMath = NO, needsMermaid = NO, needsWiki = NO;
	NSString *prepared = NVPreviewHTMLPreparingContent(html, &needsMath, &needsMermaid, &needsWiki);
	if (!prepared)
		return nil;
	NSString *scripts = NVPreviewRendererScripts(assetDirectory, needsMath, needsMermaid, needsWiki);
	if (!scripts)
		return prepared;

	NSMutableString *result = [NSMutableString stringWithString:prepared];
	NSRange body = [result rangeOfString:@"</body>" options:NSCaseInsensitiveSearch | NSBackwardsSearch];
	if (body.location != NSNotFound)
		[result insertString:scripts atIndex:body.location];
	else
		[result appendString:scripts];
	return result;
}
