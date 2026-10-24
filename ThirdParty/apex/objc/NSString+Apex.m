/**
 * NSString+Apex.m
 * Implementation of Apex Markdown processor integration
 */

#import "NSString+Apex.h"
#import <apex/apex.h>

#if __has_include(<AppKit/AppKit.h>)
#import <AppKit/AppKit.h>
#elif __has_include(<UIKit/UIKit.h>)
#import <UIKit/UIKit.h>
#endif

/**
 * Apex mode constants
 */
NSString *const ApexModeCommonmark = @"commonmark";
NSString *const ApexModeGFM = @"gfm";
NSString *const ApexModeMultiMarkdown = @"multimarkdown";
NSString *const ApexModeKramdown = @"kramdown";
NSString *const ApexModeUnified = @"unified";
NSString *const ApexModeQuarto = @"quarto";

@implementation NSString (Apex)

+ (apex_options)defaultApexOptions {
  return apex_options_default();
}

+ (NSString *)apexVersion {
  const char *ver = apex_version_string();
  return ver ? [NSString stringWithUTF8String:ver] : @"";
}

/**
 * Convert mode string to apex_mode_t enum
 */
+ (apex_mode_t)apexModeFromString:(NSString *)modeString {
  NSString *mode = [modeString lowercaseString];

  if ([mode isEqualToString:@"commonmark"]) {
    return APEX_MODE_COMMONMARK;
  } else if ([mode isEqualToString:@"gfm"]) {
    return APEX_MODE_GFM;
  } else if ([mode isEqualToString:@"multimarkdown"] ||
             [mode isEqualToString:@"mmd"]) {
    return APEX_MODE_MULTIMARKDOWN;
  } else if ([mode isEqualToString:@"kramdown"]) {
    return APEX_MODE_KRAMDOWN;
  } else if ([mode isEqualToString:@"quarto"]) {
    return APEX_MODE_QUARTO;
  } else {
    return APEX_MODE_UNIFIED; /* Default to unified */
  }
}

/**
 * Convert Markdown to HTML using Apex (unified mode)
 */
+ (NSString *)convertWithApex:(NSString *)inputString {
  return [self convertWithApex:inputString mode:ApexModeUnified];
}

/**
 * Convert Markdown to HTML using Apex with specific mode
 */
+ (NSString *)convertWithApex:(NSString *)inputString
                         mode:(NSString *)modeString {
  if (!inputString || [inputString length] == 0) {
    return @"";
  }

  /* Convert to C string */
  const char *markdown = [inputString UTF8String];
  if (!markdown) {
    return @"";
  }

  /* Get options for the specified mode */
  apex_mode_t mode = [self apexModeFromString:modeString];
  apex_options options = apex_options_for_mode(mode);

  /* Convert to HTML */
  char *html_c = apex_markdown_to_html(markdown, strlen(markdown), &options);

  if (!html_c) {
    return @"";
  }

  /* Convert back to NSString */
  NSString *html = [NSString stringWithUTF8String:html_c];
  apex_free_string(html_c);

  return html ? html : @"";
}

/**
 * Convert Markdown to HTML using Apex with standalone document options
 */
+ (NSString *)convertWithApex:(NSString *)inputString
                         mode:(NSString *)modeString
                   standalone:(BOOL)standalone
                   stylesheet:(NSString *_Nullable)stylesheetPath
                        title:(NSString *_Nullable)title {
  if (!inputString || [inputString length] == 0) {
    return @"";
  }

  /* Convert to C string */
  const char *markdown = [inputString UTF8String];
  if (!markdown) {
    return @"";
  }

  /* Get options for the specified mode */
  apex_mode_t mode = [self apexModeFromString:modeString];
  apex_options options = apex_options_for_mode(mode);

  /* Set standalone document options */
  options.standalone = standalone;
  if (stylesheetPath && [stylesheetPath length] > 0) {
    const char *paths[2];
    paths[0] = [stylesheetPath UTF8String];
    paths[1] = NULL;
    options.stylesheet_paths = paths;
    options.stylesheet_count = 1;
  }
  if (title && [title length] > 0) {
    options.document_title = [title UTF8String];
  }

  /* Convert to HTML */
  char *html_c = apex_markdown_to_html(markdown, strlen(markdown), &options);

  if (!html_c) {
    return @"";
  }

  /* Convert back to NSString */
  NSString *html = [NSString stringWithUTF8String:html_c];
  apex_free_string(html_c);

  return html ? html : @"";
}

/**
 * Convert Markdown to HTML using Apex with pretty printing option
 */
+ (NSString *)convertWithApex:(NSString *)inputString
                         mode:(NSString *)modeString
                       pretty:(BOOL)pretty {
  if (!inputString || [inputString length] == 0) {
    return @"";
  }

  /* Convert to C string */
  const char *markdown = [inputString UTF8String];
  if (!markdown) {
    return @"";
  }

  /* Get options for the specified mode */
  apex_mode_t mode = [self apexModeFromString:modeString];
  apex_options options = apex_options_for_mode(mode);

  /* Set pretty printing option */
  options.pretty = pretty;

  /* Convert to HTML */
  char *html_c = apex_markdown_to_html(markdown, strlen(markdown), &options);

  if (!html_c) {
    return @"";
  }

  /* Convert back to NSString */
  NSString *html = [NSString stringWithUTF8String:html_c];
  apex_free_string(html_c);

  return html ? html : @"";
}

/**
 * Convert Markdown to HTML using Apex with dictionary-based options
 */
+ (NSString *)convertWithApex:(NSString *)inputString
                         mode:(NSString *)modeString
                      options:
                          (NSDictionary<NSString *, id> *_Nullable)optionsDict {
  if (!inputString || [inputString length] == 0) {
    return @"";
  }

  /* Convert to C string */
  const char *markdown = [inputString UTF8String];
  if (!markdown) {
    return @"";
  }

  /* Get options for the specified mode */
  apex_mode_t mode = [self apexModeFromString:modeString];
  apex_options options = apex_options_for_mode(mode);

  /* Apply dictionary options if provided */
  if (optionsDict) {
    /* Pretty printing */
    id prettyValue = optionsDict[@"pretty"];
    if (prettyValue && [prettyValue isKindOfClass:[NSNumber class]]) {
      options.pretty = [prettyValue boolValue];
    }

    /* Standalone document */
    id standaloneValue = optionsDict[@"standalone"];
    if (standaloneValue && [standaloneValue isKindOfClass:[NSNumber class]]) {
      options.standalone = [standaloneValue boolValue];
    }

    /* Stylesheet path */
    id stylesheetValue = optionsDict[@"stylesheet"];
    if (stylesheetValue && [stylesheetValue isKindOfClass:[NSString class]]) {
      NSString *stylesheet = (NSString *)stylesheetValue;
      if ([stylesheet length] > 0) {
        const char *paths[2];
        paths[0] = [stylesheet UTF8String];
        paths[1] = NULL;
        options.stylesheet_paths = paths;
        options.stylesheet_count = 1;
      }
    }

    /* Document title */
    id titleValue = optionsDict[@"title"];
    if (titleValue && [titleValue isKindOfClass:[NSString class]]) {
      NSString *title = (NSString *)titleValue;
      if ([title length] > 0) {
        options.document_title = [title UTF8String];
      }
    }

    /* Hard breaks */
    id hardBreaksValue = optionsDict[@"hardBreaks"];
    if (hardBreaksValue && [hardBreaksValue isKindOfClass:[NSNumber class]]) {
      options.hardbreaks = [hardBreaksValue boolValue];
    }

    /* Generate header IDs */
    id generateHeaderIDsValue = optionsDict[@"generateHeaderIDs"];
    if (generateHeaderIDsValue &&
        [generateHeaderIDsValue isKindOfClass:[NSNumber class]]) {
      options.generate_header_ids = [generateHeaderIDsValue boolValue];
    }

    /* Unsafe HTML */
    id unsafeValue = optionsDict[@"unsafe"];
    if (unsafeValue && [unsafeValue isKindOfClass:[NSNumber class]]) {
      options.unsafe = [unsafeValue boolValue];
    }

    /* Header anchors */
    id headerAnchorsValue = optionsDict[@"headerAnchors"];
    if (headerAnchorsValue &&
        [headerAnchorsValue isKindOfClass:[NSNumber class]]) {
      options.header_anchors = [headerAnchorsValue boolValue];
    }

    /* Obfuscate emails */
    id obfuscateEmailsValue = optionsDict[@"obfuscateEmails"];
    if (obfuscateEmailsValue &&
        [obfuscateEmailsValue isKindOfClass:[NSNumber class]]) {
      options.obfuscate_emails = [obfuscateEmailsValue boolValue];
    }

    /* Embed images */
    id embedImagesValue = optionsDict[@"embedImages"];
    if (embedImagesValue && [embedImagesValue isKindOfClass:[NSNumber class]]) {
      options.embed_images = [embedImagesValue boolValue];
    }

    /* Plugins */
    id enablePluginsValue = optionsDict[@"enablePlugins"];
    if (enablePluginsValue && [enablePluginsValue isKindOfClass:[NSNumber class]]) {
      options.enable_plugins = [enablePluginsValue boolValue];
    }

    /* Autolink bare URLs / emails */
    id enableAutolinkValue = optionsDict[@"enableAutolink"];
    if (enableAutolinkValue && [enableAutolinkValue isKindOfClass:[NSNumber class]]) {
      options.enable_autolink = [enableAutolinkValue boolValue];
    }

    /* Output format: html (default) or rtf */
    id outputFormatValue = optionsDict[@"outputFormat"];
    if (outputFormatValue && [outputFormatValue isKindOfClass:[NSString class]]) {
      NSString *fmt = [(NSString *)outputFormatValue lowercaseString];
      if ([fmt isEqualToString:@"rtf"]) {
        options.output_format = APEX_OUTPUT_RTF;
      } else {
        options.output_format = APEX_OUTPUT_HTML;
      }
    }
  }

  /* Convert to HTML or RTF */
  char *html_c = apex_markdown_to_html(markdown, strlen(markdown), &options);

  if (!html_c) {
    return @"";
  }

  /* Convert back to NSString */
  NSString *html = [NSString stringWithUTF8String:html_c];
  apex_free_string(html_c);

  return html ? html : @"";
}

/**
 * Convert Markdown to HTML using Apex with common options combined
 */
+ (NSString *)convertWithApex:(NSString *)inputString
                         mode:(NSString *)modeString
            generateHeaderIDs:(BOOL)generateHeaderIDs
                   hardBreaks:(BOOL)hardBreaks
                       pretty:(BOOL)pretty {
  if (!inputString || [inputString length] == 0) {
    return @"";
  }

  /* Convert to C string */
  const char *markdown = [inputString UTF8String];
  if (!markdown) {
    return @"";
  }

  /* Get options for the specified mode */
  apex_mode_t mode = [self apexModeFromString:modeString];
  apex_options options = apex_options_for_mode(mode);

  /* Set common options */
  options.generate_header_ids = generateHeaderIDs;
  options.hardbreaks = hardBreaks;
  options.pretty = pretty;

  /* Convert to HTML */
  char *html_c = apex_markdown_to_html(markdown, strlen(markdown), &options);

  if (!html_c) {
    return @"";
  }

  /* Convert back to NSString */
  NSString *html = [NSString stringWithUTF8String:html_c];
  apex_free_string(html_c);

  return html ? html : @"";
}

/**
 * Convert Markdown to HTML using Apex with source file URL for includes.
 * Leaves local images as paths; sandboxed callers (e.g. Marked Quick Look)
 * embed after security-scoped folder grants.
 */
+ (NSString *)convertWithApex:(NSString *)inputString
                         mode:(NSString *)modeString
                    sourceURL:(NSURL *)sourceURL {
  return [self convertWithApex:inputString
                          mode:modeString
                     sourceURL:sourceURL
                enableAutolink:YES];
}

+ (NSString *)convertWithApex:(NSString *)inputString
                         mode:(NSString *)modeString
                    sourceURL:(NSURL *)sourceURL
               enableAutolink:(BOOL)enableAutolink {
  if (!inputString || [inputString length] == 0) {
    return @"";
  }

  const char *markdown = [inputString UTF8String];
  if (!markdown) {
    return @"";
  }

  apex_mode_t mode = [self apexModeFromString:modeString];
  apex_options options = apex_options_for_mode(mode);

  options.unsafe = true;
  options.generate_header_ids = true;
  options.enable_critic_markup = true;
  options.critic_mode = 2; /* CRITIC_MARKUP: show markup with classes */
  /* Do not embed images here: fopen/stat under Dropbox/CloudStorage can stall
   * sandboxed Quick Look. Callers (Marked Quick Look) embed after security-scoped
   * folder grants with a non-blocking Swift pass. */
  options.embed_images = false;
  options.enable_autolink = enableAutolink;

  if (sourceURL && sourceURL.isFileURL) {
    NSString *path = sourceURL.path;
    if (path.length > 0) {
      options.input_file_path = [path UTF8String];
      NSString *baseDir = [path stringByDeletingLastPathComponent];
      if (baseDir.length > 0) {
        options.base_directory = [baseDir UTF8String];
      }
    }
  }

  char *html_c = apex_markdown_to_html(markdown, strlen(markdown), &options);

  if (!html_c) {
    return @"";
  }

  NSString *html = [NSString stringWithUTF8String:html_c];
  apex_free_string(html_c);

  return html ? html : @"";
}

/**
 * Convert this string (as Markdown) to HTML using Apex in unified mode
 */
- (NSString *)apexHTML {
  return [NSString convertWithApex:self];
}

/**
 * Convert this string (as Markdown) to HTML using Apex with specific mode
 */
- (NSString *)apexHTMLWithMode:(NSString *)mode {
  return [NSString convertWithApex:self mode:mode];
}

- (NSString *)apexRTF {
  return [self apexRTFWithMode:ApexModeUnified options:nil];
}

- (NSString *)apexRTFWithMode:(NSString *)mode {
  return [self apexRTFWithMode:mode options:nil];
}

- (NSString *)apexRTFWithMode:(NSString *)mode
                      options:(NSDictionary<NSString *, id> *_Nullable)options {
  NSMutableDictionary *dict =
      options ? [options mutableCopy] : [NSMutableDictionary dictionary];
  dict[@"outputFormat"] = @"rtf";
  return [NSString convertWithApex:self mode:mode options:dict];
}

#if __has_include(<AppKit/AppKit.h>) || __has_include(<UIKit/UIKit.h>)
- (NSAttributedString *)apexAttributedString {
  return [self apexAttributedStringWithMode:ApexModeUnified options:nil usingHTML:NO];
}

- (NSAttributedString *)apexAttributedStringUsingHTML:(BOOL)useHTML {
  return [self apexAttributedStringWithMode:ApexModeUnified options:nil usingHTML:useHTML];
}

- (NSAttributedString *)apexAttributedStringWithMode:(NSString *)mode
                                             options:(NSDictionary<NSString *, id> *_Nullable)options
                                            usingHTML:(BOOL)useHTML {
  NSString *source = nil;
  NSAttributedStringDocumentType docType;
  if (useHTML) {
    source = [NSString convertWithApex:self mode:mode options:options];
    docType = NSHTMLTextDocumentType;
  } else {
    source = [self apexRTFWithMode:mode options:options];
    docType = NSRTFTextDocumentType;
  }
  if (!source || [source length] == 0) {
    return [[NSAttributedString alloc] initWithString:@""];
  }
  NSData *data = [source dataUsingEncoding:NSUTF8StringEncoding];
  if (!data) {
    return nil;
  }
  NSDictionary *attrs = @{
    NSDocumentTypeDocumentAttribute : docType,
    NSCharacterEncodingDocumentAttribute : @(NSUTF8StringEncoding)
  };
  NSError *error = nil;
  NSAttributedString *attr =
      [[NSAttributedString alloc] initWithData:data options:attrs documentAttributes:nil error:&error];
  (void)error;
  return attr;
}
#endif

/**
 * Extract a flat table of contents as dictionaries suitable for Swift OutlineGroup.
 * Each entry: @{ @"level": NSNumber, @"text": NSString, @"id": NSString }
 * Optional options keys: @"idFormat" (@"gfm"/@"mmd"/@"kramdown"),
 * @"tocMinMax" (@"2,4" or @[@2, @4]).
 */
+ (NSArray<NSDictionary<NSString *, id> *> *)tableOfContentsWithApex:(NSString *)inputString
                                                                mode:(NSString *)modeString
                                                             options:(NSDictionary<NSString *, id> *_Nullable)optionsDict {
  if (!inputString || [inputString length] == 0) {
    return @[];
  }

  const char *markdown = [inputString UTF8String];
  if (!markdown) {
    return @[];
  }

  apex_mode_t mode = [self apexModeFromString:modeString ?: ApexModeUnified];
  apex_options options = apex_options_for_mode(mode);

  if (optionsDict) {
    id idFormatVal = optionsDict[@"idFormat"];
    if ([idFormatVal isKindOfClass:[NSString class]]) {
      NSString *fmt = [(NSString *)idFormatVal lowercaseString];
      if ([fmt isEqualToString:@"mmd"]) {
        options.id_format = 1;
      } else if ([fmt isEqualToString:@"kramdown"]) {
        options.id_format = 2;
      } else {
        options.id_format = 0;
      }
    }
    id tocMinMaxVal = optionsDict[@"tocMinMax"];
    if ([tocMinMaxVal isKindOfClass:[NSString class]]) {
      int min = 0, max = 0;
      if (sscanf([(NSString *)tocMinMaxVal UTF8String], "%d,%d", &min, &max) == 2 &&
          min >= 1 && max <= 6 && min <= max) {
        options.toc_min = min;
        options.toc_max = max;
      }
    } else if ([tocMinMaxVal isKindOfClass:[NSArray class]] &&
               [(NSArray *)tocMinMaxVal count] >= 2) {
      int min = [[(NSArray *)tocMinMaxVal objectAtIndex:0] intValue];
      int max = [[(NSArray *)tocMinMaxVal objectAtIndex:1] intValue];
      if (min >= 1 && max <= 6 && min <= max) {
        options.toc_min = min;
        options.toc_max = max;
      }
    }
  }

  size_t count = 0;
  apex_toc_entry *entries =
      apex_markdown_to_toc_entries(markdown, strlen(markdown), &options, &count);
  if (!entries || count == 0) {
    apex_toc_entries_free(entries, count);
    return @[];
  }

  NSMutableArray<NSDictionary<NSString *, id> *> *result =
      [NSMutableArray arrayWithCapacity:count];
  for (size_t i = 0; i < count; i++) {
    NSString *text = entries[i].text
                         ? [NSString stringWithUTF8String:entries[i].text]
                         : @"";
    NSString *ident = entries[i].id
                          ? [NSString stringWithUTF8String:entries[i].id]
                          : @"";
    [result addObject:@{
      @"level" : @(entries[i].level),
      @"text" : text ?: @"",
      @"id" : ident ?: @""
    }];
  }
  apex_toc_entries_free(entries, count);
  return result;
}

+ (NSArray<NSDictionary<NSString *, id> *> *)tableOfContentsWithApex:(NSString *)inputString {
  return [self tableOfContentsWithApex:inputString mode:ApexModeUnified options:nil];
}

- (NSArray<NSDictionary<NSString *, id> *> *)apexTableOfContents {
  return [NSString tableOfContentsWithApex:self];
}

- (NSArray<NSDictionary<NSString *, id> *> *)apexTableOfContentsWithMode:(NSString *)mode
                                                                 options:(NSDictionary<NSString *, id> *_Nullable)options {
  return [NSString tableOfContentsWithApex:self mode:mode options:options];
}

@end
