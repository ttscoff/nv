#import <Foundation/Foundation.h>

/* Rewrite Mermaid fences for the preview and report which renderers this HTML
   needs. No script tags are added, so the result can be swapped into a page that
   has already loaded them. Returns nil for nil input. */
NSString *NVPreviewHTMLPreparingContent(NSString *html, BOOL *needsMath, BOOL *needsMermaid);

/* Script tags that load the local MathJax and Mermaid renderers from
   assetDirectory (the preview-js folder). Returns nil when nothing is needed. */
NSString *NVPreviewRendererScripts(NSString *assetDirectory, BOOL needsMath, BOOL needsMermaid);

/* Both of the above: the prepared HTML with the script tags placed before
   </body>, or appended when there is none. */
NSString *NVPreviewHTMLForDisplay(NSString *html, NSString *assetDirectory);

/* YES when url names an anchor inside the page at documentURL: the same page,
   differing only in the fragment. Such a link scrolls the preview instead of
   opening elsewhere. */
BOOL NVPreviewURLIsFragmentOfDocument(NSURL *url, NSURL *documentURL);
