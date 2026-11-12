#import <Foundation/Foundation.h>

/* Rewrite Mermaid fences for the preview and report which renderers this HTML
   needs. No script tags are added, so the result can be swapped into a page that
   has already loaded them. Returns nil for nil input. Wiki links are detected
   here, before MathJax's own [[ brackets are added to the page. */
NSString *NVPreviewHTMLPreparingContent(NSString *html, BOOL *needsMath, BOOL *needsMermaid, BOOL *needsWiki);

/* Script tags that load the local MathJax, Mermaid, wiki-link, and highlight.js
   scripts from assetDirectory (the preview-js folder). Returns nil when nothing
   is needed. The wiki linker is inlined because the preview is loaded with a
   nil base URL. Highlight.js is a file URL, followed by a short inline script
   that picks the GitHub light or dark token colors. */
NSString *NVPreviewRendererScripts(NSString *assetDirectory, BOOL needsMath, BOOL needsMermaid, BOOL needsWiki, BOOL needsHighlight);

/* YES when html has a code block (a pre whose first element is code). Mermaid
   diagrams and inline code do not count. */
BOOL NVPreviewHTMLNeedsCodeHighlight(NSString *html);

/* The GitHub light and dark token styles, each in a style tag that stays
   inactive (media="not all") until the highlight script chooses one. The block
   background is left to the preview theme. Returns nil when either sheet is
   missing. A theme stylesheet placed after these tags overrides any class it
   defines itself. */
NSString *NVPreviewHighlightStyles(NSString *assetDirectory);

/* Inserts styles before the first stylesheet so that sheet can override them.
   Falls back to just before </head>, then to the start of the document. Returns
   html unchanged when styles is empty. */
NSString *NVPreviewHTMLInsertingHighlightStyles(NSString *html, NSString *styles);

/* Prepared HTML, highlight styles, then the script tags placed before </body>,
   or appended when there is none. */
NSString *NVPreviewHTMLForDisplay(NSString *html, NSString *assetDirectory);

/* YES when url names an anchor inside the page at documentURL: the same page,
   differing only in the fragment. Such a link scrolls the preview instead of
   opening elsewhere. */
BOOL NVPreviewURLIsFragmentOfDocument(NSURL *url, NSURL *documentURL);
