#import "HTMLToMarkdownConverter.h"
@interface HTMLToMarkdownConverter () <NSXMLParserDelegate>
@property(nonatomic, strong) NSMutableString *markdown;
@property(nonatomic, strong)
    NSMutableArray<NSMutableArray<NSString *> *> *currentTable;
@property(nonatomic, strong) NSMutableArray<NSString *> *currentRow;
@property(nonatomic, strong) NSMutableArray<NSNumber *> *currentTableRowHasTH;
@property(nonatomic, strong) NSMutableArray<NSNumber *> *currentTableRowHasTD;
@property(nonatomic, assign) BOOL currentRowHasTH;
@property(nonatomic, assign) BOOL currentRowHasTD;
@property(nonatomic, assign) BOOL inTableHeader;
@property(nonatomic, strong) NSMutableString *currentCell;
@property(nonatomic, strong)
    NSMutableArray<NSString *> *listStack; // "ul" or "ol"
@property(nonatomic, assign) NSUInteger listItemIndex;
@property(nonatomic, assign) NSUInteger blockquoteLevel;
@property(nonatomic, assign) BOOL inCodeBlock;
@property(nonatomic, assign) BOOL inCodeSpan;
@property(nonatomic, strong) NSMutableString *currentLinkHref;
@property(nonatomic, strong) NSMutableString *currentImageSrc;
@property(nonatomic, strong) NSMutableString *currentImageAlt;
@property(nonatomic, strong) NSMutableArray<NSString *> *footnotes;
@property(nonatomic, assign) NSUInteger footnoteCounter;
@property(nonatomic, strong) NSMutableString *currentFootnoteText;
@property(nonatomic, assign) BOOL inFootnote;
@property(nonatomic, assign) NSUInteger listItemDepth;
@property(nonatomic, assign) NSUInteger listItemParagraphCount;
/** Cocoa HTML Writer: map paragraph class (e.g. "p1") to heading level 1-5 from style block font sizes; nil if not applicable. */
@property(nonatomic, strong) NSDictionary<NSString *, NSNumber *> *cocoaParagraphClassToHeadingLevel;
/** When > 0, current <p> is being emitted as a heading of this level. */
@property(nonatomic, assign) NSUInteger currentParagraphHeadingLevel;
/** When > 0, we are inside an h1–h6 so foundCharacters should be normalized to one line. */
@property(nonatomic, assign) NSUInteger currentHeadingLevel;
/** Accumulates raw character content inside the current h1–h6; flushed with collapse on didEndElement. */
@property(nonatomic, strong) NSMutableString *currentHeadingBuffer;
/** Accumulates raw link text inside <a>; flushed with collapse when </a>. */
@property(nonatomic, strong) NSMutableString *currentLinkTextBuffer;
@end

@implementation HTMLToMarkdownConverter

- (NSString *)markdownFromHTML:(NSString *)html {
  self.markdown = [NSMutableString string];
  self.listStack = [NSMutableArray array];
  self.blockquoteLevel = 0;
  self.inCodeBlock = NO;
  self.inCodeSpan = NO;
  self.footnotes = [NSMutableArray array];
  self.footnoteCounter = 0;
  self.currentFootnoteText = [NSMutableString string];
  self.inFootnote = NO;
  self.currentParagraphHeadingLevel = 0;
  self.currentHeadingLevel = 0;
  NSError *error = nil;

  // Move whitespace outside of emphasis tags
  html = [self moveWhitespaceOutsideEmphasisTags:html];

  html = [self normalizeHTMLForConversion:html];

  // Strip DOCTYPE (NSXMLParser does not support it; Cocoa HTML Writer and others emit <!DOCTYPE ...>)
  NSRegularExpression *doctypeRe = [NSRegularExpression
      regularExpressionWithPattern:@"<!DOCTYPE[^>]*>"
                           options:NSRegularExpressionCaseInsensitive
                             error:nil];
  html = [doctypeRe stringByReplacingMatchesInString:html
                                             options:0
                                               range:NSMakeRange(0, html.length)
                                        withTemplate:@""];

  // Extract Cocoa HTML Writer style block to map p.pN font sizes to heading levels (48+ = h1, 36-48 = h2, 24-36 = h3, 18-24 = h4, 14-18 = h5), then strip the block
  NSRegularExpression *styleBlockRe = [NSRegularExpression
      regularExpressionWithPattern:@"<style[^>]*>(.*?)</style>"
                           options:NSRegularExpressionCaseInsensitive |
                               NSRegularExpressionDotMatchesLineSeparators
                             error:nil];
  NSTextCheckingResult *styleMatch =
      [styleBlockRe firstMatchInString:html
                               options:0
                                 range:NSMakeRange(0, html.length)];
  if (styleMatch.numberOfRanges >= 2) {
    NSString *styleContent = [html substringWithRange:[styleMatch rangeAtIndex:1]];
    self.cocoaParagraphClassToHeadingLevel =
        [self headingLevelMapFromCocoaStyleContent:styleContent];
  } else {
    self.cocoaParagraphClassToHeadingLevel = nil;
  }
  NSRegularExpression *styleRe = [NSRegularExpression
      regularExpressionWithPattern:@"<style[^>]*>.*?</style>"
                           options:NSRegularExpressionCaseInsensitive |
                               NSRegularExpressionDotMatchesLineSeparators
                             error:nil];
  html = [styleRe stringByReplacingMatchesInString:html
                                            options:0
                                              range:NSMakeRange(0, html.length)
                                       withTemplate:@""];

  // Strip empty tags (e.g. <b></b> from Cocoa HTML Writer) so they don't become extra ** or _ in Markdown
  NSRegularExpression *emptyTagRe = [NSRegularExpression
      regularExpressionWithPattern:@"<([a-z][a-z0-9]*)(?:\\s[^>]*)?>\\s*</\\1>"
                           options:NSRegularExpressionCaseInsensitive
                             error:nil];
  for (;;) {
    NSString *replaced = [emptyTagRe stringByReplacingMatchesInString:html
                                                             options:0
                                                               range:NSMakeRange(0, html.length)
                                                        withTemplate:@""];
    if ([replaced isEqualToString:html]) {
      break;
    }
    html = replaced;
  }

  // remove empty paragraphs
  NSRegularExpression *emptyParaRe = [NSRegularExpression
      regularExpressionWithPattern:@"<p[^>]*>(<br\\s*/?>)*</p>"
                           options:NSRegularExpressionCaseInsensitive
                             error:&error];
  html =
      [emptyParaRe stringByReplacingMatchesInString:html
                                            options:0
                                              range:NSMakeRange(0, html.length)
                                       withTemplate:@"\n\n"];
  // Fix <br> tags to be self-closing for XML
  NSRegularExpression *hrRe = [NSRegularExpression
      regularExpressionWithPattern:@"<hr\\s*[^>]*/?>"
                           options:NSRegularExpressionCaseInsensitive
                             error:nil];
  html = [hrRe stringByReplacingMatchesInString:html
                                        options:0
                                          range:NSMakeRange(0, html.length)
                                   withTemplate:@"<hr />"];
  // Fix <br> tags to be self-closing for XML
  NSRegularExpression *brRe = [NSRegularExpression
      regularExpressionWithPattern:@"<br\\s*[^>]*/?>"
                           options:NSRegularExpressionCaseInsensitive
                             error:nil];
  html = [brRe stringByReplacingMatchesInString:html
                                        options:0
                                          range:NSMakeRange(0, html.length)
                                   withTemplate:@"<br />"];
  // Fix <img> tags to be self-closing for XML
  NSRegularExpression *imgRe = [NSRegularExpression
      regularExpressionWithPattern:@"<img([^>]*)>"
                           options:NSRegularExpressionCaseInsensitive
                             error:nil];
  html = [imgRe stringByReplacingMatchesInString:html
                                         options:0
                                           range:NSMakeRange(0, html.length)
                                    withTemplate:@"<img$1 />"];
  // Fix <meta> and <link> (void elements in HTML) to be self-closing for XML
  NSRegularExpression *metaRe = [NSRegularExpression
      regularExpressionWithPattern:@"<meta([^>]*)>"
                           options:NSRegularExpressionCaseInsensitive
                             error:nil];
  html = [metaRe stringByReplacingMatchesInString:html
                                           options:0
                                             range:NSMakeRange(0, html.length)
                                      withTemplate:@"<meta$1 />"];
  NSRegularExpression *linkRe = [NSRegularExpression
      regularExpressionWithPattern:@"<link([^>]*)>"
                           options:NSRegularExpressionCaseInsensitive
                             error:nil];
  html = [linkRe stringByReplacingMatchesInString:html
                                          options:0
                                            range:NSMakeRange(0, html.length)
                                     withTemplate:@"<link$1 />"];
  // Wrap in a root element to ensure all content is parsed
  NSString *wrappedHTML = [NSString stringWithFormat:@"<root>%@</root>", html];
  NSData *data = [wrappedHTML dataUsingEncoding:NSUTF8StringEncoding];
  NSXMLParser *parser = [[NSXMLParser alloc] initWithData:data];
  parser.delegate = self;
  [parser parse];

  // Append footnotes at the end
  if (self.footnotes.count > 0) {
    [self.markdown appendString:@"\n\n"];
    [self.markdown
        appendString:[self.footnotes componentsJoinedByString:@"\n\n"]];
    [self.markdown appendString:@"\n\n"];
  }

  NSString *result = [self stripEmptyEmphasisFromMarkdown:self.markdown];
  result = [self stripEmphasisMarkersFromMarkdownHeadingLines:result];
  return result;
}

- (BOOL)isInsideHeading {
  return self.currentHeadingLevel > 0 || self.currentParagraphHeadingLevel > 0;
}

- (NSString *)imageBasenameFromSrc:(NSString *)src {
  if (!src.length) {
    return @"";
  }
  NSString *path = src;
  if ([src hasPrefix:@"file://"]) {
    NSString *fromURL = [[NSURL URLWithString:src] path];
    if (fromURL.length > 0) {
      path = fromURL;
    }
  }
  return path.lastPathComponent ?: @"";
}

/** Cocoa/Pages often set alt to the filename; that is not a real caption. */
- (BOOL)isPlaceholderImageLabel:(NSString *)label src:(NSString *)src {
  NSString *trimmed = [label
      stringByTrimmingCharactersInSet:[NSCharacterSet
                                          whitespaceAndNewlineCharacterSet]];
  if (trimmed.length == 0) {
    return YES;
  }
  NSString *basename = [self imageBasenameFromSrc:src];
  if (basename.length == 0) {
    return NO;
  }
  if ([trimmed caseInsensitiveCompare:basename] == NSOrderedSame) {
    return YES;
  }
  NSString *basenameNoExt = [basename stringByDeletingPathExtension];
  NSString *labelNoExt = [trimmed stringByDeletingPathExtension];
  if (labelNoExt.length > 0 && basenameNoExt.length > 0 &&
      [labelNoExt caseInsensitiveCompare:basenameNoExt] == NSOrderedSame) {
    return YES;
  }
  return NO;
}

/** Alt like pastedGraphic.png after src is rewritten to image-1.png in cache. */
- (BOOL)isFilenameLikeImageAltLabel:(NSString *)label {
  NSString *trimmed = [label
      stringByTrimmingCharactersInSet:[NSCharacterSet
                                          whitespaceAndNewlineCharacterSet]];
  if (trimmed.length == 0) {
    return NO;
  }
  if ([trimmed rangeOfString:@" "].location != NSNotFound) {
    return NO;
  }
  static NSRegularExpression *imageExtRe;
  static dispatch_once_t once;
  dispatch_once(&once, ^{
    imageExtRe = [NSRegularExpression
        regularExpressionWithPattern:
            @"\\.(png|jpe?g|gif|webp|tiff?|bmp|heic|pdf|svg)$"
                             options:NSRegularExpressionCaseInsensitive
                               error:NULL];
  });
  if (!imageExtRe) {
    return NO;
  }
  return [imageExtRe firstMatchInString:trimmed
                                options:0
                                  range:NSMakeRange(0, trimmed.length)] != nil;
}

- (BOOL)isNonCaptionImageLabel:(NSString *)label src:(NSString *)src {
  return [self isPlaceholderImageLabel:label src:src] ||
         [self isFilenameLikeImageAltLabel:label];
}

- (void)appendMarkdownForImageSrc:(NSString *)src
                              alt:(NSString *)alt
                            title:(NSString *)title {
  NSString *trimmedAlt = [alt
      stringByTrimmingCharactersInSet:[NSCharacterSet
                                          whitespaceAndNewlineCharacterSet]];
  NSString *trimmedTitle = [title
      stringByTrimmingCharactersInSet:[NSCharacterSet
                                          whitespaceAndNewlineCharacterSet]];

  if (trimmedAlt.length == 0 && trimmedTitle.length > 0 &&
      ![self isNonCaptionImageLabel:trimmedTitle src:src]) {
    trimmedAlt = trimmedTitle;
  }

  NSString *markdownAlt =
      [self isNonCaptionImageLabel:trimmedAlt src:src] ? @"" : trimmedAlt;

  NSString *titleSuffix = @"";
  if (trimmedTitle.length > 0 &&
      ![self isNonCaptionImageLabel:trimmedTitle src:src] &&
      ![trimmedTitle isEqualToString:markdownAlt]) {
    titleSuffix = [NSString stringWithFormat:@" \"%@\"", trimmedTitle];
  }

  [self.markdown appendFormat:@"![%@](%@%@)", markdownAlt, src, titleSuffix];
}

/** Removes empty **, *, __, _, and mixed emphasis pairs left after stripping inline tags. */
- (NSString *)stripEmptyEmphasisFromMarkdown:(NSString *)markdown {
  if (!markdown.length) {
    return markdown ?: @"";
  }
  NSString *result = markdown;
  NSArray<NSString *> *patterns = @[
    @"\\*\\*\\*\\s*\\*\\*\\*",
    @"\\*\\*\\s*\\*\\*",
    @"\\*\\s*\\*(?!\\*)",
    @"___\\s*___",
    @"__\\s*__",
    @"(?<![_A-Za-z0-9])_(?![_])\\s*_(?![_A-Za-z0-9])",
  ];
  for (NSString *pattern in patterns) {
    NSRegularExpression *re = [NSRegularExpression
        regularExpressionWithPattern:pattern
                             options:0
                               error:nil];
    if (!re) {
      continue;
    }
    for (;;) {
      NSString *replaced = [re stringByReplacingMatchesInString:result
                                                        options:0
                                                          range:NSMakeRange(0, result.length)
                                                   withTemplate:@""];
      if ([replaced isEqualToString:result]) {
        break;
      }
      result = replaced;
    }
  }
  return result;
}

/** Headings should not include bold/italic markers; unwrap and strip leftover markers. */
- (NSString *)stripEmphasisMarkersFromMarkdownHeadingLines:(NSString *)markdown {
  if (!markdown.length) {
    return markdown ?: @"";
  }
  NSRegularExpression *headingLineRe = [NSRegularExpression
      regularExpressionWithPattern:@"^(#{1,6}\\s+)(.+)$"
                           options:NSRegularExpressionAnchorsMatchLines
                             error:nil];
  if (!headingLineRe) {
    return markdown;
  }
  NSMutableString *result = [markdown mutableCopy];
  NSArray<NSTextCheckingResult *> *matches =
      [headingLineRe matchesInString:markdown
                             options:0
                               range:NSMakeRange(0, markdown.length)];
  for (NSTextCheckingResult *match in [matches reverseObjectEnumerator]) {
    if (match.numberOfRanges < 3) {
      continue;
    }
    NSString *prefix = [markdown substringWithRange:[match rangeAtIndex:1]];
    NSString *content = [markdown substringWithRange:[match rangeAtIndex:2]];
    NSString *cleaned = [self stringByRemovingMarkdownEmphasisFromText:content];
    [result replaceCharactersInRange:match.range
                          withString:[NSString stringWithFormat:@"%@%@", prefix,
                                                                cleaned]];
  }
  return [result copy];
}

- (NSString *)stringByRemovingMarkdownEmphasisFromText:(NSString *)text {
  if (!text.length) {
    return text ?: @"";
  }
  NSString *result = text;
  NSArray<NSString *> *unwrapPatterns = @[
    @"\\*\\*\\*([^*]+?)\\*\\*\\*",
    @"\\*\\*([^*]+?)\\*\\*",
    @"\\*([^*]+?)\\*",
    @"___([^_]+?)___",
    @"__([^_]+?)__",
    @"(?<![_A-Za-z0-9])_([^_]+?)_(?![_A-Za-z0-9])",
  ];
  for (NSString *pattern in unwrapPatterns) {
    NSRegularExpression *re = [NSRegularExpression
        regularExpressionWithPattern:pattern
                             options:0
                               error:nil];
    if (!re) {
      continue;
    }
    for (;;) {
      NSString *replaced = [re stringByReplacingMatchesInString:result
                                                        options:0
                                                          range:NSMakeRange(0, result.length)
                                                   withTemplate:@"$1"];
      if ([replaced isEqualToString:result]) {
        break;
      }
      result = replaced;
    }
  }
  NSRegularExpression *strayMarkers = [NSRegularExpression
      regularExpressionWithPattern:@"\\*\\*|\\*|__|_|___"
                           options:0
                             error:nil];
  if (strayMarkers) {
    result = [strayMarkers stringByReplacingMatchesInString:result
                                                  options:0
                                                    range:NSMakeRange(0, result.length)
                                             withTemplate:@""];
  }
  return [result
      stringByTrimmingCharactersInSet:[NSCharacterSet
                                          whitespaceCharacterSet]];
}

- (void)parser:(NSXMLParser *)parser parseErrorOccurred:(NSError *)parseError {
  NSLog(@"HTMLToMarkdownConverter parse error: %@", parseError);
}

#pragma mark - NSXMLParserDelegate

- (void)parser:(NSXMLParser *)parser
    didStartElement:(NSString *)elementName
       namespaceURI:(NSString *)namespaceURI
      qualifiedName:(NSString *)qName
         attributes:(NSDictionary<NSString *, NSString *> *)attributeDict {
  if ([elementName isEqualToString:@"hr"]) {
    [self.markdown appendString:@"\n\n* * *\n\n"];
  }
  // Headings (h1–h6; match case-insensitively for XML parsers that preserve tag case)
  else if (elementName.length == 2 &&
           [[elementName lowercaseString] hasPrefix:@"h"] &&
           isdigit([elementName characterAtIndex:1])) {
    NSUInteger level = [[elementName substringFromIndex:1] integerValue];
    if (level >= 1 && level <= 6) {
      self.currentHeadingLevel = level;
      self.currentHeadingBuffer = [NSMutableString string];
      [self.markdown appendFormat:@"\n%@ ", [@"" stringByPaddingToLength:level
                                                              withString:@"#"
                                                         startingAtIndex:0]];
    }
  }
  // Paragraph (or Cocoa p.pN as heading when font size maps to a level)
  else if ([elementName isEqualToString:@"p"]) {
    if (self.listItemDepth > 0) {
      if (self.listItemParagraphCount > 0) {
        [self.markdown appendString:@"\n"];
        for (NSUInteger i = 0; i < self.listStack.count; i++) {
          [self.markdown appendString:@"  "];
        }
        [self.markdown appendString:@"    "];
      }
      self.listItemParagraphCount++;
      return;
    }
    self.currentParagraphHeadingLevel = 0;
    if (self.cocoaParagraphClassToHeadingLevel.count > 0) {
      NSString *classAttr = attributeDict[@"class"];
      if (classAttr.length > 0) {
        NSArray<NSString *> *tokens = [classAttr
            componentsSeparatedByCharactersInSet:[NSCharacterSet
                                                    whitespaceCharacterSet]];
        for (NSString *token in tokens) {
          NSString *trimmed = [token
              stringByTrimmingCharactersInSet:[NSCharacterSet
                                                  whitespaceCharacterSet]];
          NSNumber *levelNum = self.cocoaParagraphClassToHeadingLevel[trimmed];
          if (levelNum != nil && levelNum.unsignedIntegerValue >= 1 &&
              levelNum.unsignedIntegerValue <= 5) {
            self.currentParagraphHeadingLevel = levelNum.unsignedIntegerValue;
            break;
          }
        }
      }
    }
    if (self.currentParagraphHeadingLevel > 0) {
      [self.markdown appendString:@"\n\n"];
      for (NSUInteger i = 0; i < self.currentParagraphHeadingLevel; i++) {
        [self.markdown appendString:@"#"];
      }
      [self.markdown appendString:@" "];
    } else {
      [self.markdown appendString:@"\n\n"];
    }
    return;
  }
  // Line break
  else if ([elementName isEqualToString:@"br"]) {
    [self.markdown appendString:@"  \n"];
  }
  // Emphasis (skip inside headings; # already implies emphasis)
  else if ([elementName isEqualToString:@"em"] ||
           [elementName isEqualToString:@"i"]) {
    if (![self isInsideHeading]) {
      [self.markdown appendString:@"*"];
    }
  }
  // Strong
  else if ([elementName isEqualToString:@"strong"] ||
           [elementName isEqualToString:@"b"]) {
    if (![self isInsideHeading]) {
      [self.markdown appendString:@"**"];
    }
  }
  // Unordered List
  else if ([elementName isEqualToString:@"ul"]) {
    [self.listStack addObject:@"ul"];
    [self.markdown appendString:@"\n\n"];
  }
  // Ordered List
  else if ([elementName isEqualToString:@"ol"]) {
    [self.listStack addObject:@"ol"];
    [self.markdown appendString:@"\n\n"];
    self.listItemIndex = 1;
  }
  // List Item
  else if ([elementName isEqualToString:@"li"]) {
    self.listItemDepth++;
    self.listItemParagraphCount = 0;
    [self.markdown appendString:@"\n"];
    for (NSUInteger i = 0; i < self.listStack.count; i++) {
      [self.markdown appendString:@"  "];
    }
    NSString *listType = self.listStack.lastObject;
    if ([listType isEqualToString:@"ul"]) {
      [self.markdown appendString:@"- "];
    } else if ([listType isEqualToString:@"ol"]) {
      [self.markdown
          appendFormat:@"%lu. ", (unsigned long)self.listItemIndex++];
    }
  }
  // Link
  else if ([elementName isEqualToString:@"a"]) {
    self.currentLinkHref =
        [NSMutableString stringWithString:attributeDict[@"href"] ?: @""];
    self.currentLinkTextBuffer = [NSMutableString string];
    [self.markdown appendString:@"["];
  }
  // Image
  else if ([elementName isEqualToString:@"img"]) {
    [self appendMarkdownForImageSrc:attributeDict[@"src"] ?: @""
                                alt:attributeDict[@"alt"] ?: @""
                              title:attributeDict[@"title"] ?: @""];
  }
  // Blockquote
  else if ([elementName isEqualToString:@"blockquote"]) {
    self.blockquoteLevel++;
    [self.markdown appendString:@"\n"];
    for (NSUInteger i = 0; i < self.blockquoteLevel; i++) {
      [self.markdown appendString:@"> "];
    }
  }
  // Aside (treat as blockquote)
  else if ([elementName isEqualToString:@"aside"]) {
    self.blockquoteLevel++;
    [self.markdown appendString:@"\n"];
    for (NSUInteger i = 0; i < self.blockquoteLevel; i++) {
      [self.markdown appendString:@"> "];
    }
  }
  // Code block
  else if ([elementName isEqualToString:@"pre"]) {
    self.inCodeBlock = YES;
    [self.markdown appendString:@"\n\n```\n"];
  }
  // Code span
  else if ([elementName isEqualToString:@"code"]) {
    if (self.inCodeBlock) {
      // Already in code block, do nothing
    } else {
      self.inCodeSpan = YES;
      [self.markdown appendString:@"`"];
    }
  }
  // Table
  else if ([elementName isEqualToString:@"table"]) {
    self.currentTable = [NSMutableArray array];
    self.currentTableRowHasTH = [NSMutableArray array];
    self.currentTableRowHasTD = [NSMutableArray array];
  }
  // Table row
  else if ([elementName isEqualToString:@"tr"]) {
    self.currentRow = [NSMutableArray array];
    self.currentRowHasTH = NO;
    self.currentRowHasTD = NO;
  }
  // Table header cell
  else if ([elementName isEqualToString:@"th"]) {
    self.inTableHeader = YES;
    self.currentRowHasTH = YES;
    self.currentCell = [NSMutableString string];
  }
  // Table data cell
  else if ([elementName isEqualToString:@"td"]) {
    self.inTableHeader = NO;
    self.currentRowHasTD = YES;
    self.currentCell = [NSMutableString string];
  }
  // Page break
  else if ([elementName isEqualToString:@"div"] &&
           [attributeDict[@"class"] isEqualToString:@"page-break"]) {
    [self.markdown appendString:@"\n\n<!--BREAK-->\n\n"];
  }
  // Footnote
  else if ([elementName isEqualToString:@"a"] &&
           [attributeDict[@"class"] isEqualToString:@"footnote"]) {
    self.footnoteCounter++;
    [self.markdown appendFormat:@"[^%lu]", (unsigned long)self.footnoteCounter];
  }
  // Footnote content
  else if ([elementName isEqualToString:@"div"] &&
           [attributeDict[@"class"] isEqualToString:@"footnote"]) {
    self.inFootnote = YES;
    self.currentFootnoteText = [NSMutableString string];
  }
}

/** Collapses newlines and multiple spaces to a single space and trims. Used for heading text and link text so they stay on one line. */
- (NSString *)stringByCollapsingWhitespaceForInline:(NSString *)string {
  if (!string || string.length == 0)
    return @"";
  NSString *trimmed = [string
      stringByTrimmingCharactersInSet:[NSCharacterSet
                                          whitespaceAndNewlineCharacterSet]];
  if (trimmed.length == 0)
    return @"";
  static NSRegularExpression *wsRe;
  static dispatch_once_t once;
  dispatch_once(&once, ^{
    wsRe = [NSRegularExpression
        regularExpressionWithPattern:@"[ \\t\\n\\r]+"
                             options:0
                               error:NULL];
  });
  return [wsRe stringByReplacingMatchesInString:trimmed
                                        options:0
                                          range:NSMakeRange(0, trimmed.length)
                                   withTemplate:@" "];
}

- (void)parser:(NSXMLParser *)parser foundCharacters:(NSString *)string {
  if (self.currentCell) {
    [self.currentCell appendString:string];
  } else if (self.currentLinkHref) {
    [self.currentLinkTextBuffer appendString:string];
  } else if (self.currentHeadingLevel > 0) {
    [self.currentHeadingBuffer appendString:string];
  } else if (self.inCodeBlock) {
    [self.markdown appendString:string];
  } else if (self.inCodeSpan) {
    [self.markdown appendString:string];
  } else if (self.inFootnote) {
    [self.currentFootnoteText appendString:string];
  } else {
    [self.markdown appendString:string];
  }
}

- (void)parser:(NSXMLParser *)parser
    didEndElement:(NSString *)elementName
     namespaceURI:(NSString *)namespaceURI
    qualifiedName:(NSString *)qName {
  // Headings (h1–h6; match case-insensitively)
  if (elementName.length == 2 &&
      [[elementName lowercaseString] hasPrefix:@"h"] &&
      isdigit([elementName characterAtIndex:1])) {
    [self.markdown appendString:[self stringByCollapsingWhitespaceForInline:self.currentHeadingBuffer ?: @""]];
    self.currentHeadingBuffer = nil;
    self.currentHeadingLevel = 0;
    [self.markdown appendString:@"\n"];
  }
  // Paragraph (or Cocoa heading)
  else if ([elementName isEqualToString:@"p"]) {
    if (self.listItemDepth > 0) {
      return;
    }
    [self.markdown appendString:@"\n"];
    if (self.currentParagraphHeadingLevel == 0) {
      // Trim any extra whitespace for normal paragraphs
      NSString *trimmed = [self.markdown
          stringByTrimmingCharactersInSet:[NSCharacterSet
                                              whitespaceAndNewlineCharacterSet]];
      if ([trimmed hasSuffix:@"\n\n"]) {
        [self.markdown setString:[trimmed stringByAppendingString:@"\n"]];
      }
    }
    self.currentParagraphHeadingLevel = 0;
  }
  // Emphasis
  else if ([elementName isEqualToString:@"em"] ||
           [elementName isEqualToString:@"i"]) {
    if (![self isInsideHeading]) {
      [self.markdown appendString:@"*"];
    }
  }
  // Strong
  else if ([elementName isEqualToString:@"strong"] ||
           [elementName isEqualToString:@"b"]) {
    if (![self isInsideHeading]) {
      [self.markdown appendString:@"**"];
    }
  }
  // Unordered List
  else if ([elementName isEqualToString:@"ul"]) {
    [self.listStack removeLastObject];
    [self.markdown appendString:@"\n"];
  }
  // Ordered List
  else if ([elementName isEqualToString:@"ol"]) {
    [self.listStack removeLastObject];
    [self.markdown appendString:@"\n"];
  }
  // List Item
  else if ([elementName isEqualToString:@"li"]) {
    if (self.listItemDepth > 0) {
      self.listItemDepth--;
    }
  }
  // Link
  else if ([elementName isEqualToString:@"a"]) {
    [self.markdown appendString:[self stringByCollapsingWhitespaceForInline:self.currentLinkTextBuffer ?: @""]];
    [self.markdown appendFormat:@"](%@)", self.currentLinkHref];
    self.currentLinkHref = nil;
    self.currentLinkTextBuffer = nil;
  }
  // Blockquote
  else if ([elementName isEqualToString:@"blockquote"]) {
    self.blockquoteLevel--;
    [self.markdown appendString:@"\n"];
  }
  // Aside (treat as blockquote)
  else if ([elementName isEqualToString:@"aside"]) {
    self.blockquoteLevel--;
    [self.markdown appendString:@"\n"];
  }
  // Code block
  else if ([elementName isEqualToString:@"pre"]) {
    self.inCodeBlock = NO;
    [self.markdown appendString:@"\n```\n"];
  }
  // Code span
  else if ([elementName isEqualToString:@"code"]) {
    if (self.inCodeBlock) {
      // Already in code block, do nothing
    } else {
      self.inCodeSpan = NO;
      [self.markdown appendString:@"`"];
    }
  }
  // Table
  else if ([elementName isEqualToString:@"table"]) {
    [self appendMarkdownTable:self.currentTable
                 rowHasTHInfo:self.currentTableRowHasTH
                 rowHasTDInfo:self.currentTableRowHasTD];
    self.currentTable = nil;
    self.currentTableRowHasTH = nil;
    self.currentTableRowHasTD = nil;
    [self.markdown appendString:@"\n"];
  }
  // Table row
  else if ([elementName isEqualToString:@"tr"]) {
    if (self.currentRow) {
      [self.currentTable addObject:self.currentRow];
      [self.currentTableRowHasTH addObject:@(self.currentRowHasTH)];
      [self.currentTableRowHasTD addObject:@(self.currentRowHasTD)];
      self.currentRow = nil;
    }
  }
  // Table header cell
  else if ([elementName isEqualToString:@"th"]) {
    if (self.currentCell && self.currentRow) {
      [self.currentRow
          addObject:[self.currentCell
                        stringByTrimmingCharactersInSet:
                            [NSCharacterSet whitespaceAndNewlineCharacterSet]]];
      self.currentCell = nil;
    }
  }
  // Table data cell
  else if ([elementName isEqualToString:@"td"]) {
    if (self.currentCell && self.currentRow) {
      [self.currentRow
          addObject:[self.currentCell
                        stringByTrimmingCharactersInSet:
                            [NSCharacterSet whitespaceAndNewlineCharacterSet]]];
      self.currentCell = nil;
    }
  }
  // Footnote content end
  else if ([elementName isEqualToString:@"div"] && self.inFootnote) {
    self.inFootnote = NO;
    NSString *footnoteText = [self.currentFootnoteText
        stringByTrimmingCharactersInSet:[NSCharacterSet
                                            whitespaceAndNewlineCharacterSet]];
    if (footnoteText.length > 0) {
      [self.footnotes
          addObject:[NSString
                        stringWithFormat:@"[^%lu]: %@",
                                         (unsigned long)self.footnoteCounter,
                                         footnoteText]];
    }
    self.currentFootnoteText = [NSMutableString string];
  }
}

- (void)appendMarkdownRow:(NSArray<NSString *> *)row
              toMarkdown:(NSMutableString *)tableMD {
  [tableMD appendString:@"|"];
  for (NSString *cell in row) {
    if (cell.length == 0) {
      [tableMD appendString:@"|"];
    } else {
      [tableMD appendFormat:@" %@ |", cell];
    }
  }
  [tableMD appendString:@"\n"];
}

- (void)appendMarkdownTable:(NSArray<NSArray<NSString *> *> *)tableRows
               rowHasTHInfo:(NSArray<NSNumber *> *)rowHasTHInfo
               rowHasTDInfo:(NSArray<NSNumber *> *)rowHasTDInfo {
  if (tableRows.count == 0)
    return;
  if (tableRows.count == 1 && tableRows[0].count == 1) {
    NSString *contents = tableRows[0][0];
    contents =
        [contents stringByReplacingOccurrencesOfString:@"|" withString:@""];
    contents = [contents
        stringByTrimmingCharactersInSet:[NSCharacterSet
                                            whitespaceAndNewlineCharacterSet]];
    NSArray *lines = [contents componentsSeparatedByString:@"\n"];
    for (NSString *line in lines) {
      [self.markdown
          appendFormat:@"> %@\n",
                       [line stringByTrimmingCharactersInSet:
                                 [NSCharacterSet
                                     whitespaceAndNewlineCharacterSet]]];
    }
    return;
  }

  NSUInteger maxColumnCount = 0;
  for (NSArray<NSString *> *row in tableRows) {
    if (row.count > maxColumnCount) {
      maxColumnCount = row.count;
    }
  }
  if (maxColumnCount == 0)
    return;

  BOOL firstRowIsAllTH =
      (rowHasTHInfo.count > 0 && rowHasTDInfo.count > 0 &&
       rowHasTHInfo[0].boolValue && !rowHasTDInfo[0].boolValue);
  BOOL subsequentRowsContainTH = NO;
  for (NSUInteger i = 1; i < rowHasTHInfo.count; i++) {
    if (rowHasTHInfo[i].boolValue) {
      subsequentRowsContainTH = YES;
      break;
    }
  }
  BOOL hasExplicitHeaderRow = firstRowIsAllTH && !subsequentRowsContainTH;

  NSMutableString *tableMD = [NSMutableString string];

  NSMutableArray<NSString *> *header = [NSMutableArray array];
  if (hasExplicitHeaderRow) {
    [header addObjectsFromArray:tableRows[0]];
  }
  while (header.count < maxColumnCount) {
    [header addObject:@""];
  }

  [tableMD appendString:@"\n"];
  [self appendMarkdownRow:header toMarkdown:tableMD];

  [tableMD appendString:@"|"];
  for (NSUInteger i = 0; i < maxColumnCount; i++) {
    [tableMD appendString:@" --- |"];
  }
  [tableMD appendString:@"\n"];

  NSUInteger startRowIndex = hasExplicitHeaderRow ? 1 : 0;
  for (NSUInteger i = startRowIndex; i < tableRows.count; i++) {
    NSMutableArray<NSString *> *row = [tableRows[i] mutableCopy];
    while (row.count < maxColumnCount) {
      [row addObject:@""];
    }
    [self appendMarkdownRow:row toMarkdown:tableMD];
  }
  [self.markdown appendString:tableMD];
}

/** Absolute fallback when body size cannot be inferred (legacy Scrivener exports). */
static NSUInteger headingLevelForFontSizeAbsolute(double fontSize) {
  if (fontSize >= 48)
    return 1;
  if (fontSize >= 36)
    return 2;
  if (fontSize >= 24)
    return 3;
  if (fontSize >= 18)
    return 4;
  return 0;
}

/** Maps font size relative to document body size (Pages uses 16px body; old absolute map treated 16px as h5). */
static NSUInteger headingLevelForFontSize(double fontSize, double bodyFontSize) {
  if (bodyFontSize <= 0) {
    return headingLevelForFontSizeAbsolute(fontSize);
  }
  if (fontSize <= bodyFontSize + 1.0) {
    return 0;
  }
  double ratio = fontSize / bodyFontSize;
  if (ratio >= 1.75) {
    return 3;
  }
  if (ratio >= 1.12) {
    return 4;
  }
  if (ratio >= 1.05) {
    return 5;
  }
  return 0;
}

/** Parses Cocoa HTML Writer style content for p.pN { font: XXpx } or font-size: XXpx and returns a map of class name (e.g. "p1") to heading level 1-5. */
- (NSDictionary<NSString *, NSNumber *> *)headingLevelMapFromCocoaStyleContent:
    (NSString *)styleContent {
  if (!styleContent.length)
    return nil;
  NSMutableDictionary<NSString *, NSNumber *> *classToSize =
      [NSMutableDictionary dictionary];
  NSMutableDictionary<NSNumber *, NSNumber *> *sizeFrequency =
      [NSMutableDictionary dictionary];
  NSArray *patterns = @[
    @"p\\.(p\\d+)\\s*\\{[^}]*?font:\\s*([\\d.]+)\\s*px",
    @"p\\.(p\\d+)\\s*\\{[^}]*?font-size:\\s*([\\d.]+)\\s*px"
  ];
  for (NSString *pattern in patterns) {
    NSRegularExpression *ruleRe = [NSRegularExpression
        regularExpressionWithPattern:pattern
                             options:NSRegularExpressionCaseInsensitive
                               error:nil];
    [ruleRe enumerateMatchesInString:styleContent
                             options:0
                               range:NSMakeRange(0, styleContent.length)
                          usingBlock:^(NSTextCheckingResult *result,
                                       NSMatchingFlags __unused flags,
                                       BOOL *__unused stop) {
                            if (result.numberOfRanges < 3) {
                              return;
                            }
                            NSString *className = [styleContent
                                substringWithRange:[result rangeAtIndex:1]];
                            double fontSize = [[styleContent
                                substringWithRange:[result rangeAtIndex:2]]
                                doubleValue];
                            if (fontSize < 9.0) {
                              return;
                            }
                            classToSize[className] = @(fontSize);
                            NSNumber *sizeKey = @(fontSize);
                            sizeFrequency[sizeKey] = @([sizeFrequency[sizeKey]
                                                              unsignedIntegerValue] +
                                                         1);
                          }];
  }
  if (classToSize.count == 0) {
    return nil;
  }

  double bodyFontSize = 0;
  NSUInteger bestCount = 0;
  for (NSNumber *sizeKey in sizeFrequency) {
    NSUInteger count = [sizeFrequency[sizeKey] unsignedIntegerValue];
    if (count > bestCount) {
      bestCount = count;
      bodyFontSize = [sizeKey doubleValue];
    }
  }

  NSMutableDictionary<NSString *, NSNumber *> *map = [NSMutableDictionary dictionary];
  [classToSize
      enumerateKeysAndObjectsUsingBlock:^(NSString *className, NSNumber *sizeNum,
                                         BOOL *__unused stop) {
        NSUInteger level =
            headingLevelForFontSize(sizeNum.doubleValue, bodyFontSize);
        if (level > 0) {
          map[className] = @(level);
        }
      }];
  return map.count > 0 ? [map copy] : nil;
}

// Move whitespace outside of emphasis tags for better Markdown formatting
- (NSString *)moveWhitespaceOutsideEmphasisTags:(NSString *)html {
  if (!html)
    return @"";

  NSMutableString *result = [html mutableCopy];
  NSArray *emphasisTags = @[ @"i", @"b", @"em", @"strong" ];

  for (NSString *tag in emphasisTags) {
    // Pattern 1: Move leading whitespace outside opening tag
    // <tag> whitespace... -> whitespace<tag>...
    NSString *leadingPattern = [NSString stringWithFormat:@"<%@>\\s+", tag];
    NSRegularExpression *leadingRe = [NSRegularExpression
        regularExpressionWithPattern:leadingPattern
                             options:NSRegularExpressionCaseInsensitive
                               error:nil];

    NSArray<NSTextCheckingResult *> *leadingMatches =
        [leadingRe matchesInString:result
                           options:0
                             range:NSMakeRange(0, result.length)];
    // Process in reverse order to avoid index shifting
    for (NSTextCheckingResult *match in
         [leadingMatches reverseObjectEnumerator]) {
      NSRange tagRange =
          [result rangeOfString:[NSString stringWithFormat:@"<%@>", tag]
                        options:NSCaseInsensitiveSearch
                          range:match.range];
      if (tagRange.location != NSNotFound) {
        NSRange whitespaceRange =
            NSMakeRange(tagRange.location + tagRange.length,
                        match.range.location + match.range.length -
                            (tagRange.location + tagRange.length));
        NSString *whitespace = [result substringWithRange:whitespaceRange];
        NSString *replacement =
            [NSString stringWithFormat:@"%@<%@>", whitespace, tag];
        [result replaceCharactersInRange:match.range withString:replacement];
      }
    }

    // Pattern 2: Move trailing whitespace outside closing tag
    // ...whitespace</tag> -> ...</tag>whitespace
    NSString *trailingPattern = [NSString stringWithFormat:@"\\s+</%@>", tag];
    NSRegularExpression *trailingRe = [NSRegularExpression
        regularExpressionWithPattern:trailingPattern
                             options:NSRegularExpressionCaseInsensitive
                               error:nil];

    NSArray<NSTextCheckingResult *> *trailingMatches =
        [trailingRe matchesInString:result
                            options:0
                              range:NSMakeRange(0, result.length)];
    // Process in reverse order to avoid index shifting
    for (NSTextCheckingResult *match in
         [trailingMatches reverseObjectEnumerator]) {
      NSRange closingTagRange =
          [result rangeOfString:[NSString stringWithFormat:@"</%@>", tag]
                        options:NSCaseInsensitiveSearch
                          range:match.range];
      if (closingTagRange.location != NSNotFound) {
        NSRange whitespaceRange =
            NSMakeRange(match.range.location,
                        closingTagRange.location - match.range.location);
        NSString *whitespace = [result substringWithRange:whitespaceRange];
        NSString *replacement =
            [NSString stringWithFormat:@"</%@>%@", tag, whitespace];
        [result replaceCharactersInRange:match.range withString:replacement];
      }
    }
  }

  return [result copy];
}

/** Prepares Cocoa/Pages HTML for NSXMLParser (strip spans, fix invalid list nesting). */
- (NSString *)normalizeHTMLForConversion:(NSString *)html {
  if (!html || html.length == 0) {
    return html ?: @"";
  }
  NSString *result = html;
  NSError *error = nil;

  NSRegularExpression *spanTagRe = [NSRegularExpression
      regularExpressionWithPattern:@"</?span[^>]*>"
                           options:NSRegularExpressionCaseInsensitive
                             error:&error];
  if (spanTagRe) {
    result = [spanTagRe stringByReplacingMatchesInString:result
                                                 options:0
                                                   range:NSMakeRange(0, result.length)
                                            withTemplate:@""];
  }

  NSRegularExpression *nestedOpenRe = [NSRegularExpression
      regularExpressionWithPattern:@"<ul([^>]*)>\\s*<ul[^>]*>"
                           options:NSRegularExpressionCaseInsensitive
                             error:&error];
  NSRegularExpression *nestedCloseRe = [NSRegularExpression
      regularExpressionWithPattern:@"</ul>\\s*</ul>"
                           options:NSRegularExpressionCaseInsensitive
                             error:&error];
  // Pages wraps lists in redundant <ul><ul>; unwrap instead of inserting an empty <li>.
  if (nestedOpenRe && nestedCloseRe) {
    for (;;) {
      NSString *replaced = [nestedOpenRe
          stringByReplacingMatchesInString:result
                                   options:0
                                     range:NSMakeRange(0, result.length)
                              withTemplate:@"<ul$1>"];
      if ([replaced isEqualToString:result]) {
        break;
      }
      result = replaced;
    }
    for (;;) {
      NSString *replaced = [nestedCloseRe
          stringByReplacingMatchesInString:result
                                   options:0
                                     range:NSMakeRange(0, result.length)
                              withTemplate:@"</ul>"];
      if ([replaced isEqualToString:result]) {
        break;
      }
      result = replaced;
    }
  }

  return result;
}

@end
