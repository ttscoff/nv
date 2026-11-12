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

static NSString * const NVPreviewTaskListCSS = @"li input[type=\"checkbox\"]{margin-right:0.4em;vertical-align:text-bottom;}li.nv-task-done{text-decoration:line-through;}li.nv-task-done input{text-decoration:none;}";

static void NVMarkCompletedPreviewTasks(NSMutableString *html) {
	if (!NVHTMLContains(html, @"type=\"checkbox\""))
		return;
	NSRegularExpression *expression = [NSRegularExpression regularExpressionWithPattern:@"<li\\b([^>]*)>(\\s*)<input\\b(?=[^>]*\\btype=\"checkbox\")(?=[^>]*\\bchecked=)[^>]*>" options:NSRegularExpressionCaseInsensitive error:NULL];
	if (!expression)
		return;
	NSArray *matches = [expression matchesInString:html options:0 range:NSMakeRange(0, [html length])];
	NSEnumerator *reverse = [matches reverseObjectEnumerator];
	NSTextCheckingResult *match;
	while ((match = [reverse nextObject])) {
		NSString *attributes = [html substringWithRange:[match rangeAtIndex:1]];
		NSString *space = [html substringWithRange:[match rangeAtIndex:2]];
		NSRange inputRange = NSMakeRange(NSMaxRange([match rangeAtIndex:2]), NSMaxRange([match range]) - NSMaxRange([match rangeAtIndex:2]));
		NSString *input = [html substringWithRange:inputRange];
		NSString *replacement = [NSString stringWithFormat:@"<li class=\"nv-task-done\"%@>%@%@", attributes, space, input];
		[html replaceCharactersInRange:[match range] withString:replacement];
	}
	[html insertString:[NSString stringWithFormat:@"<style>%@</style>", NVPreviewTaskListCSS] atIndex:0];
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
	NVMarkCompletedPreviewTasks(result);
	return result;
}

BOOL NVPreviewHTMLNeedsCodeHighlight(NSString *html) {
	if (![html length])
		return NO;
	NSRegularExpression *expression = [NSRegularExpression regularExpressionWithPattern:@"<pre\\b[^>]*>\\s*<code\\b" options:NSRegularExpressionCaseInsensitive error:NULL];
	if (!expression)
		return NO;
	return [expression numberOfMatchesInString:html options:0 range:NSMakeRange(0, [html length])] > 0;
}

/* Both sheets start disabled. The inline script enables one after it has read
   the computed background of the code block, walking up through transparent
   parents. 0.55 is NVThemeColorIsLight's cutoff. Token backgrounds, such as a
   diff line, stay; the code block's own background stays with the preview theme. */
static NSString * const NVPreviewHighlightBootstrap =
	@"<script>\n"
	"(function(){"
	"function opaque(node){var probe=node;while(probe&&probe.nodeType===1){"
	"var color=(window.getComputedStyle(probe).backgroundColor||'');"
	"var match=color.match(/rgba?\\(\\s*([0-9.]+)[,\\s]+([0-9.]+)[,\\s]+([0-9.]+)(?:[,\\s\\/]+([0-9.]+))?\\s*\\)/);"
	"if(match){var alpha=match[4]===undefined?1:parseFloat(match[4]);"
	"if(alpha>0.05)return [parseFloat(match[1]),parseFloat(match[2]),parseFloat(match[3])];}"
	"probe=probe.parentNode;}return [255,255,255];}"
	"function isLight(rgb){return (0.299*rgb[0]+0.587*rgb[1]+0.114*rgb[2])/255>=0.55;}"
	"function nvHighlightCode(){"
	"var blocks=document.querySelectorAll('pre code');if(!blocks.length)return;"
	"var pre=blocks[0];while(pre&&(!pre.tagName||pre.tagName.toLowerCase()!=='pre'))pre=pre.parentNode;"
	"var light=isLight(opaque(pre||blocks[0]));"
	"var day=document.getElementById('nv-hl-light');var night=document.getElementById('nv-hl-dark');"
	"if(day)day.media=light?'all':'not all';if(night)night.media=light?'not all':'all';"
	"if(window.hljs){if(hljs.configure)hljs.configure({ignoreUnescapedHTML:true});if(hljs.highlightAll)hljs.highlightAll();}}"
	"window.nvHighlightCode=nvHighlightCode;nvHighlightCode();"
	"})();\n"
	@"</script>";

static NSString *NVPreviewStylesheet(NSString *path) {
	NSString *css = [NSString stringWithContentsOfFile:path encoding:NSUTF8StringEncoding error:NULL];
	if (![css length])
		return nil;
	NSMutableString *safe = [NSMutableString stringWithString:css];
	[safe replaceOccurrencesOfString:@"</style" withString:@"<\\/style" options:NSCaseInsensitiveSearch range:NSMakeRange(0, [safe length])];
	return safe;
}

NSString *NVPreviewHighlightStyles(NSString *assetDirectory) {
	NSString *light = NVPreviewStylesheet([assetDirectory stringByAppendingPathComponent:@"highlight-light.css"]);
	NSString *dark = NVPreviewStylesheet([assetDirectory stringByAppendingPathComponent:@"highlight-dark.css"]);
	if (!light || !dark)
		return nil;
	return [NSString stringWithFormat:@"<style id=\"nv-hl-light\" media=\"not all\">%@</style><style id=\"nv-hl-dark\" media=\"not all\">%@</style>", light, dark];
}

NSString *NVPreviewHTMLInsertingHighlightStyles(NSString *html, NSString *styles) {
	if (![html length] || ![styles length])
		return html;
	NSMutableString *result = [NSMutableString stringWithString:html];
	NSRange style = [result rangeOfString:@"<style" options:NSCaseInsensitiveSearch];
	if (style.location != NSNotFound) {
		[result insertString:styles atIndex:style.location];
		return result;
	}
	NSRange head = [result rangeOfString:@"</head>" options:NSCaseInsensitiveSearch];
	if (head.location != NSNotFound) {
		[result insertString:styles atIndex:head.location];
		return result;
	}
	[result insertString:styles atIndex:0];
	return result;
}

NSString *NVPreviewRendererScripts(NSString *assetDirectory, BOOL needsMath, BOOL needsMermaid, BOOL needsWiki, BOOL needsHighlight) {
	if (!needsMath && !needsMermaid && !needsWiki && !needsHighlight)
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
	if (needsHighlight) {
		NSString *tag = NVScriptTagForFile([assetDirectory stringByAppendingPathComponent:@"highlight.min.js"]);
		if (tag) {
			[scripts appendString:tag];
			[scripts appendString:@"\n"];
			[scripts appendString:NVPreviewHighlightBootstrap];
			[scripts appendString:@"\n"];
		}
	}
	return [scripts length] ? scripts : nil;
}

static NSString *NVURLStringWithoutFragment(NSURL *url) {
	NSString *string = [url absoluteString];
	NSRange hash = [string rangeOfString:@"#"];
	return hash.location == NSNotFound ? string : [string substringToIndex:hash.location];
}

BOOL NVPreviewURLIsFragmentOfDocument(NSURL *url, NSURL *documentURL) {
	if (!url || !documentURL || ![url fragment])
		return NO;
	NSString *target = NVURLStringWithoutFragment(url);
	return [target length] > 0 && [target isEqualToString:NVURLStringWithoutFragment(documentURL)];
}

NSString *NVPreviewHTMLForDisplay(NSString *html, NSString *assetDirectory) {
	BOOL needsMath = NO, needsMermaid = NO, needsWiki = NO;
	NSString *prepared = NVPreviewHTMLPreparingContent(html, &needsMath, &needsMermaid, &needsWiki);
	if (!prepared)
		return nil;
	BOOL needsHighlight = NVPreviewHTMLNeedsCodeHighlight(prepared);
	NSString *scripts = NVPreviewRendererScripts(assetDirectory, needsMath, needsMermaid, needsWiki, needsHighlight);
	NSString *body = prepared;
	if (scripts) {
		NSMutableString *result = [NSMutableString stringWithString:prepared];
		NSRange close = [result rangeOfString:@"</body>" options:NSCaseInsensitiveSearch | NSBackwardsSearch];
		if (close.location != NSNotFound)
			[result insertString:scripts atIndex:close.location];
		else
			[result appendString:scripts];
		body = result;
	}
	return NVPreviewHTMLInsertingHighlightStyles(body, needsHighlight ? NVPreviewHighlightStyles(assetDirectory) : nil);
}
