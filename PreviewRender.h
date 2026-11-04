#import <Foundation/Foundation.h>

/* Rewrite Mermaid fences for the preview and append local MathJax and Mermaid
   script tags when this HTML needs them. assetDirectory is the preview-js folder. */
NSString *NVPreviewHTMLForDisplay(NSString *html, NSString *assetDirectory);
