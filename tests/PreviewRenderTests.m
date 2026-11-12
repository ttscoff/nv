#import <Foundation/Foundation.h>
#import <stdio.h>
#import "PreviewRender.h"

static int gFailures = 0;

static void expectTrue(BOOL cond, const char *what) {
	if (!cond) {
		fprintf(stderr, "FAIL %s\n", what);
		gFailures++;
	}
}

static void expectContains(NSString *html, NSString *needle, const char *what) {
	expectTrue(html && [html rangeOfString:needle].location != NSNotFound, what);
}

static void expectOmits(NSString *html, NSString *needle, const char *what) {
	expectTrue(!html || [html rangeOfString:needle].location == NSNotFound, what);
}

int main(int argc, const char *argv[]) {
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	NSFileManager *fm = [NSFileManager defaultManager];
	NSString *dir = [NSTemporaryDirectory() stringByAppendingPathComponent:@"nv-preview-js-test"];
	[fm removeItemAtPath:dir error:NULL];
	[fm createDirectoryAtPath:[dir stringByAppendingPathComponent:@"mathjax/output/svg/fonts"] withIntermediateDirectories:YES attributes:nil error:NULL];
	[fm createFileAtPath:[dir stringByAppendingPathComponent:@"mermaid.min.js"] contents:[NSData data] attributes:nil];
	[fm createFileAtPath:[dir stringByAppendingPathComponent:@"highlight.min.js"] contents:[NSData data] attributes:nil];
	[fm createFileAtPath:[dir stringByAppendingPathComponent:@"highlight-light.css"] contents:[@".hljs{color:#24292e}.hljs-keyword{color:#d73a49}" dataUsingEncoding:NSUTF8StringEncoding] attributes:nil];
	[fm createFileAtPath:[dir stringByAppendingPathComponent:@"highlight-dark.css"] contents:[@".hljs{color:#c9d1d9}.hljs-keyword{color:#ff7b72}" dataUsingEncoding:NSUTF8StringEncoding] attributes:nil];
	[fm createFileAtPath:[dir stringByAppendingPathComponent:@"wikilinks.js"] contents:[@"/* wiki-linker */" dataUsingEncoding:NSUTF8StringEncoding] attributes:nil];
	[fm createFileAtPath:[dir stringByAppendingPathComponent:@"mathjax/tex-svg.js"] contents:[NSData data] attributes:nil];
	[fm createDirectoryAtPath:[dir stringByAppendingPathComponent:@"mathjax/fonts"] withIntermediateDirectories:YES attributes:nil error:NULL];

	NSString *plain = @"<p>Hello</p>";
	NSString *plainOut = NVPreviewHTMLForDisplay(plain, dir);
	expectTrue([plainOut isEqualToString:plain], "plain html unchanged");
	expectOmits(plainOut, @"wiki-linker", "plain html skips wiki links");
	expectOmits(plainOut, @"highlight.min.js", "plain html skips highlight");

	NSString *math = @"<p><span class=\"math inline\">\\(e=mc^2\\)</span></p>";
	NSString *mathOut = NVPreviewHTMLForDisplay(math, dir);
	expectContains(mathOut, @"class=\"math inline\"", "keeps math span");
	expectContains(mathOut, @"tex-svg.js", "loads mathjax");
	expectContains(mathOut, @"file://", "mathjax is a file url");
	expectContains(mathOut, @"inlineMath", "configures inline math");
	expectContains(mathOut, @"loader:{paths:{fonts:", "points mathjax fonts at the bundle");
	expectContains(mathOut, @"mathjax/fonts", "font path stays inside preview-js");
	expectOmits(mathOut, @"mermaid.min.js", "math note skips mermaid");
	expectOmits(mathOut, @"wiki-linker", "math config brackets are not wiki links");

	NSString *fence = @"<pre><code class=\"language-mermaid\">graph TD\nA-->B\n</code></pre>";
	NSString *fenceOut = NVPreviewHTMLForDisplay(fence, dir);
	expectContains(fenceOut, @"<pre class=\"mermaid\">graph TD\nA-->B\n</pre>", "rewrites mermaid fence");
	expectOmits(fenceOut, @"language-mermaid", "drops the code fence");
	expectContains(fenceOut, @"mermaid.min.js", "loads mermaid");
	expectContains(fenceOut, @"mermaid.initialize", "starts mermaid");
	expectOmits(fenceOut, @"tex-svg.js", "diagram skips mathjax");
	expectOmits(fenceOut, @"highlight.min.js", "diagram skips highlight");

	NSString *named = @"<pre><code class=\"language-Mermaid\">flowchart LR\n</code></pre>";
	expectContains(NVPreviewHTMLForDisplay(named, dir), @"<pre class=\"mermaid\">flowchart LR\n</pre>", "mermaid language is case insensitive");

	NSString *braced = @"<pre><code class=\"language-{mermaid}\">sequenceDiagram\n</code></pre>";
	expectContains(NVPreviewHTMLForDisplay(braced, dir), @"<pre class=\"mermaid\">sequenceDiagram\n</pre>", "braced mermaid fence");

	NSString *lang = @"<pre lang=\"mermaid\"><code>flowchart LR\nA--&gt;B\n</code></pre>";
	NSString *langOut = NVPreviewHTMLForDisplay(lang, dir);
	expectContains(langOut, @"<pre class=\"mermaid\">flowchart LR\nA--&gt;B\n</pre>", "github lang mermaid fence");
	expectContains(langOut, @"mermaid.min.js", "github lang fence loads mermaid");

	NSString *langCase = @"<pre class=\"source\" lang=\"Mermaid\"><code>graph TD\n</code></pre>";
	expectContains(NVPreviewHTMLForDisplay(langCase, dir), @"<pre class=\"mermaid\">graph TD\n</pre>", "lang attribute with other attributes");

	NSString *ruby = @"<pre><code class=\"language-ruby\">puts 1</code></pre>";
	NSString *rubyOut = NVPreviewHTMLForDisplay(ruby, dir);
	expectContains(rubyOut, @"language-ruby", "other languages stay code");
	expectOmits(rubyOut, @"mermaid.min.js", "ruby does not load mermaid");
	expectContains(rubyOut, @"highlight.min.js", "code block loads highlight.js");
	expectContains(rubyOut, @"nvHighlightCode", "code block chooses a highlight palette");
	expectContains(rubyOut, @"0.55", "palette uses the same lightness cutoff as the app");
	expectContains(rubyOut, @"id=\"nv-hl-light\"", "ships the GitHub light tokens");
	expectContains(rubyOut, @"id=\"nv-hl-dark\"", "ships the GitHub dark tokens");
	expectContains(rubyOut, @"media=\"not all\"", "both palettes wait until the background is known");
	expectContains(rubyOut, @"#d73a49", "light palette colors keywords");
	expectContains(rubyOut, @"#ff7b72", "dark palette colors keywords");
	NSRange lightAt = [rubyOut rangeOfString:@"nv-hl-light"];
	NSRange codeAt = [rubyOut rangeOfString:@"language-ruby"];
	NSRange scriptAt = [rubyOut rangeOfString:@"highlight.min.js"];
	expectTrue(lightAt.location < codeAt.location && codeAt.location < scriptAt.location, "highlight styles come before the code and the script follows it");

	NSString *dollar = @"<pre><code class=\"language-mermaid\">A-->B[$]\n</code></pre>";
	expectContains(NVPreviewHTMLForDisplay(dollar, dir), @"<pre class=\"mermaid\">A-->B[$]\n</pre>", "keeps dollars in diagrams");

	NSString *doc = @"<html><body><p><span class=\"math inline\">\\(x\\)</span></p></body></html>";
	NSString *docOut = NVPreviewHTMLForDisplay(doc, dir);
	NSRange script = [docOut rangeOfString:@"tex-svg.js"];
	NSRange close = [docOut rangeOfString:@"</body>" options:NSBackwardsSearch];
	expectTrue(script.location != NSNotFound && close.location != NSNotFound && script.location < close.location, "script before closing body");

	NSString *ready = @"<pre class=\"mermaid\">graph TD</pre>";
	NSString *readyOut = NVPreviewHTMLForDisplay(ready, dir);
	expectContains(readyOut, @"<pre class=\"mermaid\">graph TD</pre>", "existing mermaid block stays");
	expectContains(readyOut, @"mermaid.min.js", "existing mermaid block loads the script");

	NSString *both = [fence stringByAppendingString:math];
	NSString *bothOut = NVPreviewHTMLForDisplay(both, dir);
	expectContains(bothOut, @"tex-svg.js", "combined loads mathjax");
	expectContains(bothOut, @"mermaid.min.js", "combined loads mermaid");

	NSString *noMermaidDir = [NSTemporaryDirectory() stringByAppendingPathComponent:@"nv-preview-js-math-only"];
	[fm removeItemAtPath:noMermaidDir error:NULL];
	[fm createDirectoryAtPath:[noMermaidDir stringByAppendingPathComponent:@"mathjax"] withIntermediateDirectories:YES attributes:nil error:NULL];
	[fm createFileAtPath:[noMermaidDir stringByAppendingPathComponent:@"mathjax/tex-svg.js"] contents:[NSData data] attributes:nil];
	NSString *missing = NVPreviewHTMLForDisplay(fence, noMermaidDir);
	expectContains(missing, @"<pre class=\"mermaid\">", "rewrite even when the script is missing");
	expectOmits(missing, @"mermaid.min.js", "missing mermaid file adds no script");

	NSString *wiki = @"<p>See [[Hello]] and [[My Note]].</p>";
	NSString *wikiOut = NVPreviewHTMLForDisplay(wiki, dir);
	expectContains(wikiOut, @"wiki-linker", "wiki markup inlines the linker");
	expectOmits(wikiOut, @"wikilinks.js", "wiki linker is not a file url");
	expectContains(wikiOut, @"[[Hello]]", "leaves brackets for the script");
	expectContains(wikiOut, @"[[My Note]]", "leaves spaced titles for the script");

	NSString *wikiDoc = @"<html><body><p>[[Hello]]</p></body></html>";
	NSString *wikiDocOut = NVPreviewHTMLForDisplay(wikiDoc, dir);
	NSRange wikiScript = [wikiDocOut rangeOfString:@"wiki-linker"];
	NSRange wikiClose = [wikiDocOut rangeOfString:@"</body>" options:NSBackwardsSearch];
	expectTrue(wikiScript.location != NSNotFound && wikiClose.location != NSNotFound && wikiScript.location < wikiClose.location, "wiki script before closing body");

	NSString *wikiCode = @"<p>[[Hello]]</p><pre><code>[[Not a note]]</code></pre>";
	NSString *wikiCodeOut = NVPreviewHTMLForDisplay(wikiCode, dir);
	expectContains(wikiCodeOut, @"wiki-linker", "wiki script still loads beside code");
	expectContains(wikiCodeOut, @"<code>[[Not a note]]</code>", "code brackets stay in the html");
	expectContains(wikiCodeOut, @"highlight.min.js", "a code block without a language is still highlighted");

	NSString *missingWiki = NVPreviewHTMLForDisplay(wiki, noMermaidDir);
	expectContains(missingWiki, @"[[Hello]]", "brackets stay when the script is missing");
	expectOmits(missingWiki, @"wiki-linker", "missing wiki script adds no tag");

	expectTrue(NVPreviewHTMLForDisplay(nil, dir) == nil, "nil html");

	BOOL needsMath = YES, needsMermaid = YES, needsWiki = YES;
	NSString *prepared = NVPreviewHTMLPreparingContent(plain, &needsMath, &needsMermaid, &needsWiki);
	expectTrue([prepared isEqualToString:plain] && !needsMath && !needsMermaid && !needsWiki, "prepare: plain html unchanged and needs nothing");
	prepared = NVPreviewHTMLPreparingContent(both, &needsMath, &needsMermaid, &needsWiki);
	expectContains(prepared, @"<pre class=\"mermaid\">", "prepare: rewrites mermaid fences");
	expectOmits(prepared, @"<script", "prepare: adds no script tags");
	expectTrue(needsMath && needsMermaid && !needsWiki, "prepare: reports math and mermaid");
	prepared = NVPreviewHTMLPreparingContent(@"<p>[[Hello]]</p>", &needsMath, &needsMermaid, &needsWiki);
	expectTrue(needsWiki && !needsMath && !needsMermaid, "prepare: wiki markup needs the linker");
	expectOmits(prepared, @"<script", "prepare: wiki markup adds no script");
	expectTrue(NVPreviewHTMLPreparingContent(nil, &needsMath, &needsMermaid, &needsWiki) == nil && !needsMath && !needsMermaid && !needsWiki, "prepare: nil html clears the flags");
	expectTrue(NVPreviewHTMLPreparingContent(plain, NULL, NULL, NULL) != nil, "prepare: flags are optional");
	expectTrue(NVPreviewHTMLNeedsCodeHighlight(ruby), "a fenced code block needs highlight");
	expectTrue(NVPreviewHTMLNeedsCodeHighlight(@"<pre><code>plain</code></pre>"), "a code block without a language needs highlight");
	expectTrue(NVPreviewHTMLNeedsCodeHighlight(@"<PRE><CODE>plain</CODE></PRE>"), "code block tags are case insensitive");
	expectTrue(!NVPreviewHTMLNeedsCodeHighlight(plain), "plain html needs no highlight");
	expectTrue(!NVPreviewHTMLNeedsCodeHighlight(@"<p><code>inline</code></p>"), "inline code needs no highlight");
	expectTrue(!NVPreviewHTMLNeedsCodeHighlight(NVPreviewHTMLPreparingContent(fence, NULL, NULL, NULL)), "a mermaid diagram needs no highlight");
	expectTrue(!NVPreviewHTMLNeedsCodeHighlight(nil) && !NVPreviewHTMLNeedsCodeHighlight(@""), "empty html needs no highlight");

	expectTrue(NVPreviewRendererScripts(dir, NO, NO, NO, NO) == nil, "scripts: none when nothing is needed");
	NSString *mathOnly = NVPreviewRendererScripts(dir, YES, NO, NO, NO);
	expectContains(mathOnly, @"tex-svg.js", "scripts: math loads mathjax");
	expectOmits(mathOnly, @"mermaid.min.js", "scripts: math alone does not load mermaid");
	expectOmits(mathOnly, @"wiki-linker", "scripts: math alone does not load the wiki linker");
	expectOmits(mathOnly, @"highlight.min.js", "scripts: math alone does not load highlight");
	expectTrue(NVPreviewRendererScripts(noMermaidDir, NO, YES, NO, NO) == nil, "scripts: missing mermaid file gives none");
	NSString *wikiScripts = NVPreviewRendererScripts(dir, NO, NO, YES, NO);
	expectContains(wikiScripts, @"wiki-linker", "scripts: wiki inlines the linker");
	expectOmits(wikiScripts, @"wikilinks.js", "scripts: wiki linker is not a file url");
	NSString *highlightScripts = NVPreviewRendererScripts(dir, NO, NO, NO, YES);
	expectContains(highlightScripts, @"highlight.min.js", "scripts: highlight loads highlight.js");
	expectContains(highlightScripts, @"nvHighlightCode", "scripts: highlight inlines the palette picker");
	expectOmits(highlightScripts, @"mermaid.min.js", "scripts: highlight alone does not load mermaid");
	expectTrue(NVPreviewRendererScripts(noMermaidDir, NO, NO, NO, YES) == nil, "scripts: missing highlight file gives none");
	NSString *styles = NVPreviewHighlightStyles(dir);
	expectContains(styles, @"id=\"nv-hl-light\"", "styles: light sheet is named");
	expectContains(styles, @"id=\"nv-hl-dark\"", "styles: dark sheet is named");
	expectContains(styles, @"#d73a49", "styles: light sheet is included");
	expectContains(styles, @"#ff7b72", "styles: dark sheet is included");
	expectTrue([styles rangeOfString:@"nv-hl-light"].location < [styles rangeOfString:@"nv-hl-dark"].location, "styles: light sheet comes first");
	expectTrue(NVPreviewHighlightStyles(noMermaidDir) == nil, "styles: missing sheets give none");
	NSString *wrapped = NVPreviewHTMLInsertingHighlightStyles(@"<head><style type=\"text/css\">body{color:#123456}</style></head>", styles);
	expectTrue([wrapped rangeOfString:@"nv-hl-light"].location < [wrapped rangeOfString:@"body{color:#123456}"].location, "styles land before the theme sheet");
	expectTrue([NVPreviewHTMLInsertingHighlightStyles(@"<p>Hi</p>", nil) isEqualToString:@"<p>Hi</p>"], "inserting no styles leaves the html");
	expectTrue([NVPreviewHTMLInsertingHighlightStyles(@"<p>No style tag</p>", styles) hasPrefix:styles], "styles lead when the page has no sheet");
	NSString *assembled = [NVPreviewHTMLPreparingContent(both, NULL, NULL, NULL) stringByAppendingString:NVPreviewRendererScripts(dir, YES, YES, NO, NO)];
	expectTrue([NVPreviewHTMLForDisplay(both, dir) isEqualToString:assembled], "display output is prepared content plus scripts");

	NSString *tasks = @"<ul>\n<li><input type=\"checkbox\" checked=\"\" disabled=\"\" /> done</li>\n<li><input type=\"checkbox\" disabled=\"\" /> open</li>\n</ul>";
	NSString *taskOut = NVPreviewHTMLPreparingContent(tasks, NULL, NULL, NULL);
	expectContains(taskOut, @"<li class=\"nv-task-done\"><input type=\"checkbox\" checked=\"\" disabled=\"\" />", "prepare: a completed task is marked done");
	expectContains(taskOut, @"<li><input type=\"checkbox\" disabled=\"\" />", "prepare: an open task stays an open checkbox");
	expectContains(taskOut, @"text-decoration:line-through", "prepare: completed tasks are struck in the preview");

	NSURL *page = [NSURL URLWithString:@"applewebdata://ABC-123"];
	expectTrue(NVPreviewURLIsFragmentOfDocument([NSURL URLWithString:@"applewebdata://ABC-123#section-2"], page), "fragment: anchor in the same page");
	expectTrue(NVPreviewURLIsFragmentOfDocument([NSURL URLWithString:@"applewebdata://ABC-123#a"], [NSURL URLWithString:@"applewebdata://ABC-123#b"]), "fragment: page already at another anchor");
	expectTrue(!NVPreviewURLIsFragmentOfDocument([NSURL URLWithString:@"applewebdata://OTHER#section"], page), "fragment: anchor in another page");
	expectTrue(!NVPreviewURLIsFragmentOfDocument([NSURL URLWithString:@"https://example.com/#top"], page), "fragment: external link with an anchor");
	expectTrue(!NVPreviewURLIsFragmentOfDocument([NSURL URLWithString:@"applewebdata://ABC-123"], page), "fragment: same page without an anchor is not a fragment jump");
	expectTrue(!NVPreviewURLIsFragmentOfDocument(nil, page) && !NVPreviewURLIsFragmentOfDocument([NSURL URLWithString:@"applewebdata://ABC-123#x"], nil), "fragment: missing urls");

	if (gFailures) {
		fprintf(stderr, "%d preview render tests failed\n", gFailures);
		[pool drain];
		return 1;
	}
	printf("preview render tests passed\n");
	[pool drain];
	return 0;
}
