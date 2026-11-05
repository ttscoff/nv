#import <Foundation/Foundation.h>

/* Rewrite Mermaid fences for the preview and report which scripts this HTML
   needs: MathJax, Mermaid, and the wiki-link script for [[note title]] text. No
   script tags are added, so the result can be swapped into a page that has
   already loaded them. Returns nil for nil input. */
NSString *NVPreviewHTMLPreparingContent(NSString *html, BOOL *needsMath, BOOL *needsMermaid, BOOL *needsWiki);

/* Script tags that load the local MathJax and Mermaid renderers and the inline
   wiki linker from assetDirectory (the preview-js folder). Returns nil when
   nothing is needed. */
NSString *NVPreviewRendererScripts(NSString *assetDirectory, BOOL needsMath, BOOL needsMermaid, BOOL needsWiki);

/* Both of the above: the prepared HTML with the script tags placed before
   </body>, or appended when there is none. */
NSString *NVPreviewHTMLForDisplay(NSString *html, NSString *assetDirectory);

/* YES when url names an anchor inside the page at documentURL: the same page,
   differing only in the fragment. Such a link scrolls the preview instead of
   opening elsewhere. */
BOOL NVPreviewURLIsFragmentOfDocument(NSURL *url, NSURL *documentURL);
