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
	[fm createFileAtPath:[dir stringByAppendingPathComponent:@"mathjax/tex-svg.js"] contents:[NSData data] attributes:nil];
	[fm createDirectoryAtPath:[dir stringByAppendingPathComponent:@"mathjax/fonts"] withIntermediateDirectories:YES attributes:nil error:NULL];

	NSString *plain = @"<p>Hello</p>";
	NSString *plainOut = NVPreviewHTMLForDisplay(plain, dir);
	expectTrue([plainOut isEqualToString:plain], "plain html unchanged");

	NSString *math = @"<p><span class=\"math inline\">\\(e=mc^2\\)</span></p>";
	NSString *mathOut = NVPreviewHTMLForDisplay(math, dir);
	expectContains(mathOut, @"class=\"math inline\"", "keeps math span");
	expectContains(mathOut, @"tex-svg.js", "loads mathjax");
	expectContains(mathOut, @"file://", "mathjax is a file url");
	expectContains(mathOut, @"inlineMath", "configures inline math");
	expectContains(mathOut, @"loader:{paths:{fonts:", "points mathjax fonts at the bundle");
	expectContains(mathOut, @"mathjax/fonts", "font path stays inside preview-js");
	expectOmits(mathOut, @"mermaid.min.js", "math note skips mermaid");

	NSString *fence = @"<pre><code class=\"language-mermaid\">graph TD\nA-->B\n</code></pre>";
	NSString *fenceOut = NVPreviewHTMLForDisplay(fence, dir);
	expectContains(fenceOut, @"<pre class=\"mermaid\">graph TD\nA-->B\n</pre>", "rewrites mermaid fence");
	expectOmits(fenceOut, @"language-mermaid", "drops the code fence");
	expectContains(fenceOut, @"mermaid.min.js", "loads mermaid");
	expectContains(fenceOut, @"mermaid.initialize", "starts mermaid");
	expectOmits(fenceOut, @"tex-svg.js", "diagram skips mathjax");

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
	expectContains(NVPreviewHTMLForDisplay(ruby, dir), @"language-ruby", "other languages stay code");
	expectOmits(NVPreviewHTMLForDisplay(ruby, dir), @"mermaid.min.js", "ruby does not load mermaid");

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

	expectTrue(NVPreviewHTMLForDisplay(nil, dir) == nil, "nil html");

	if (gFailures) {
		fprintf(stderr, "%d preview render tests failed\n", gFailures);
		[pool drain];
		return 1;
	}
	printf("preview render tests passed\n");
	[pool drain];
	return 0;
}
