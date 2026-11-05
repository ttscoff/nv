#import <Foundation/Foundation.h>

/* Rewrite Mermaid fences for the preview and append local MathJax, Mermaid,
   and wiki-link script tags when this HTML needs them. assetDirectory is the
   preview-js folder. */
NSString *NVPreviewHTMLForDisplay(NSString *html, NSString *assetDirectory);
