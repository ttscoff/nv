/**
 * NSString+Apex.h
 * Objective-C category for integrating Apex Markdown processor into Marked
 */

#import <Foundation/Foundation.h>
#import <apex/apex.h>

NS_ASSUME_NONNULL_BEGIN

/**
 * Apex mode constants for use with convertWithApex:mode:
 */
extern NSString * const ApexModeCommonmark;
extern NSString * const ApexModeGFM;
extern NSString * const ApexModeMultiMarkdown;
extern NSString * const ApexModeKramdown;
extern NSString * const ApexModeUnified;
extern NSString * const ApexModeQuarto;

@interface NSString (Apex)

/**
 * Return Apex default C options structure.
 * Exposed for Swift/plugin integrations that need low-level option flags.
 */
+ (apex_options)defaultApexOptions;

/**
 * Apex processor version string (e.g. "0.1.89")
 */
+ (NSString *)apexVersion;

/**
 * Convert Markdown to HTML using Apex processor in unified mode
 * @param inputString The markdown text to convert
 * @return HTML string
 */
+ (NSString *)convertWithApex:(NSString *)inputString;

/**
 * Convert Markdown to HTML using Apex with specific processor mode
 * @param inputString The markdown text to convert
 * @param mode Processor mode: Use ApexMode* constants (ApexModeCommonmark, ApexModeGFM, ApexModeMultiMarkdown, ApexModeKramdown, ApexModeUnified, or ApexModeQuarto) or string values: "commonmark", "gfm", "multimarkdown", "kramdown", "unified", or "quarto"
 * @return HTML string
 */
+ (NSString *)convertWithApex:(NSString *)inputString mode:(NSString *)mode;

/**
 * Convert Markdown to HTML using Apex with standalone document options
 * @param inputString The markdown text to convert
 * @param mode Processor mode: Use ApexMode* constants or string values
 * @param standalone If YES, generates a complete HTML5 document
 * @param stylesheetPath Optional path to CSS file to link in document head (nil for none)
 * @param title Optional document title (nil for default)
 * @return HTML string (complete document if standalone is YES)
 */
+ (NSString *)convertWithApex:(NSString *)inputString
                         mode:(NSString *)mode
                   standalone:(BOOL)standalone
                    stylesheet:(NSString * _Nullable)stylesheetPath
                         title:(NSString * _Nullable)title;

/**
 * Convert Markdown to HTML using Apex with pretty printing option
 * @param inputString The markdown text to convert
 * @param mode Processor mode: Use ApexMode* constants or string values
 * @param pretty If YES, pretty-prints HTML with indentation
 * @return HTML string
 */
+ (NSString *)convertWithApex:(NSString *)inputString
                         mode:(NSString *)mode
                        pretty:(BOOL)pretty;

/**
 * Convert Markdown to HTML using Apex with dictionary-based options
 * @param inputString The markdown text to convert
 * @param mode Processor mode: Use ApexMode* constants or string values
 * @param options Dictionary of option keys and values. Supported keys:
 *   - @"pretty": NSNumber (BOOL) - Pretty-print HTML
 *   - @"standalone": NSNumber (BOOL) - Generate complete HTML document
 *   - @"stylesheet": NSString - Path to CSS file
 *   - @"title": NSString - Document title
 *   - @"hardBreaks": NSNumber (BOOL) - Treat newlines as hard breaks
 *   - @"generateHeaderIDs": NSNumber (BOOL) - Generate IDs for headers
 *   - @"unsafe": NSNumber (BOOL) - Allow raw HTML
 *   - @"headerAnchors": NSNumber (BOOL) - Generate anchor tags instead of IDs
 *   - @"obfuscateEmails": NSNumber (BOOL) - Obfuscate email links
 *   - @"embedImages": NSNumber (BOOL) - Embed images as base64 data URLs
 *   - @"enablePlugins": NSNumber (BOOL) - Enable external plugin processing
 *   - @"enableAutolink": NSNumber (BOOL) - Autolink bare URLs and emails
 *   - @"outputFormat": NSString - @"html" (default) or @"rtf"
 * @return HTML or RTF string depending on outputFormat
 */
+ (NSString *)convertWithApex:(NSString *)inputString
                         mode:(NSString *)mode
                    options:(NSDictionary<NSString *, id> * _Nullable)options;

/**
 * Convert Markdown to HTML using Apex with common options combined
 * Swift-friendly method that combines frequently used options
 * @param inputString The markdown text to convert
 * @param mode Processor mode: Use ApexMode* constants or string values
 * @param generateHeaderIDs If YES, generates IDs for headers
 * @param hardBreaks If YES, treats newlines as hard breaks
 * @param pretty If YES, pretty-prints HTML with indentation
 * @return HTML string
 */
+ (NSString *)convertWithApex:(NSString *)inputString
                         mode:(NSString *)mode
            generateHeaderIDs:(BOOL)generateHeaderIDs
                   hardBreaks:(BOOL)hardBreaks
                       pretty:(BOOL)pretty;

/**
 * Convert Markdown to HTML using Apex with source file URL for includes.
 * Does not embed local images (callers may embed after security-scoped access).
 * @param inputString The markdown text to convert
 * @param mode Processor mode
 * @param sourceURL File URL used for include resolution and base directory
 * @return HTML string
 */
+ (NSString *)convertWithApex:(NSString *)inputString
                         mode:(NSString *)mode
                    sourceURL:(NSURL *)sourceURL;

/**
 * Same as convertWithApex:mode:sourceURL: with explicit autolink control.
 * Disabling autolink avoids a costly scan on large URL-heavy documents.
 */
+ (NSString *)convertWithApex:(NSString *)inputString
                         mode:(NSString *)mode
                    sourceURL:(NSURL *)sourceURL
               enableAutolink:(BOOL)enableAutolink;

/**
 * Convert this string (as Markdown) to HTML using Apex in unified mode
 * Instance method for convenient usage on NSString objects
 * @return HTML string
 */
- (NSString *)apexHTML;

/**
 * Convert this string (as Markdown) to HTML using Apex with specific mode
 * Instance method for convenient usage on NSString objects
 * @param mode Processor mode: Use ApexMode* constants or string values
 * @return HTML string
 */
- (NSString *)apexHTMLWithMode:(NSString *)mode;

/**
 * Convert this string (as Markdown) to RTF using Apex in unified mode.
 */
- (NSString *)apexRTF;

/**
 * Convert this string (as Markdown) to RTF using Apex with specific mode.
 */
- (NSString *)apexRTFWithMode:(NSString *)mode;

/**
 * Convert this string (as Markdown) to RTF with mode and options dictionary.
 * Sets outputFormat to rtf automatically.
 */
- (NSString *)apexRTFWithMode:(NSString *)mode
                      options:(NSDictionary<NSString *, id> * _Nullable)options;

#if __has_include(<AppKit/AppKit.h>) || __has_include(<UIKit/UIKit.h>)
/**
 * Build an NSAttributedString from Markdown via Apex RTF output (preferred).
 */
- (NSAttributedString * _Nullable)apexAttributedString;

/**
 * Build an NSAttributedString from Markdown using RTF or HTML document type.
 * @param useHTML If YES, use NSHTMLTextDocumentType; if NO, use NSRTFTextDocumentType.
 */
- (NSAttributedString * _Nullable)apexAttributedStringUsingHTML:(BOOL)useHTML;

/**
 * Build an NSAttributedString with mode/options, preferring RTF unless useHTML is YES.
 */
- (NSAttributedString * _Nullable)apexAttributedStringWithMode:(NSString *)mode
                                                      options:(NSDictionary<NSString *, id> * _Nullable)options
                                                     usingHTML:(BOOL)useHTML;
#endif

/**
 * Extract a flat table of contents for outline / table-view UIs.
 * Each dictionary has keys: @"level" (NSNumber 1-6), @"text", @"id".
 * Nesting for collapsible sections is derived from level (same rules as -t toc).
 */
+ (NSArray<NSDictionary<NSString *, id> *> *)tableOfContentsWithApex:(NSString *)inputString;

+ (NSArray<NSDictionary<NSString *, id> *> *)tableOfContentsWithApex:(NSString *)inputString
                                                                mode:(NSString *)mode
                                                             options:(NSDictionary<NSString *, id> * _Nullable)options;

- (NSArray<NSDictionary<NSString *, id> *> *)apexTableOfContents;

- (NSArray<NSDictionary<NSString *, id> *> *)apexTableOfContentsWithMode:(NSString *)mode
                                                                 options:(NSDictionary<NSString *, id> * _Nullable)options;

@end

NS_ASSUME_NONNULL_END

