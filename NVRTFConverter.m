#import "NVRTFConverter.h"
#import "HTMLToMarkdownConverter.h"
#import <AppKit/AppKit.h>

@implementation NVRTFConverter

+ (NSString *)markdownFromRTFPath:(NSString *)rtfPath error:(NSError **)error {
    if (![[NSFileManager defaultManager] fileExistsAtPath:rtfPath]) {
        if (error) {
            *error = [NSError errorWithDomain:@"NVRTFConverter"
                                         code:1
                                     userInfo:@{
                                         NSLocalizedDescriptionKey : @"RTF file does not exist."
                                     }];
        }
        return @"";
    }

    NSString *ext = [[rtfPath pathExtension] lowercaseString];
    BOOL isRTFD = [ext isEqualToString:@"rtfd"];
    NSAttributedString *preloaded = nil;
    NSMutableString *mutableRTF = nil;
    NSData *rtfData = nil;

    if (isRTFD) {
        NSURL *url = [NSURL fileURLWithPath:rtfPath];
        preloaded = [[NSAttributedString alloc] initWithURL:url
                                                      options:@{
                                                          NSDocumentTypeDocumentAttribute :
                                                              NSRTFDTextDocumentType
                                                      }
                                           documentAttributes:NULL
                                                        error:error];
        if (!preloaded || preloaded.length == 0) {
            return @"";
        }
    } else {
        NSError *readError = nil;
        NSString *rtfString =
            [NSString stringWithContentsOfFile:rtfPath encoding:NSUTF8StringEncoding error:&readError];
        if (!rtfString) {
            NSData *fileData = [NSData dataWithContentsOfFile:rtfPath];
            if (fileData) {
                const NSStringEncoding encodings[] = {
                    NSUTF8StringEncoding, NSUnicodeStringEncoding, NSUTF16BigEndianStringEncoding,
                    NSUTF16LittleEndianStringEncoding, NSMacOSRomanStringEncoding
                };
                for (size_t i = 0; i < sizeof(encodings) / sizeof(encodings[0]); i++) {
                    rtfString = [[NSString alloc] initWithData:fileData encoding:encodings[i]];
                    if (rtfString) {
                        break;
                    }
                }
            }
        }
        if (rtfString) {
            mutableRTF = [rtfString mutableCopy];
            rtfData = [rtfString dataUsingEncoding:NSUTF8StringEncoding];
        } else {
            rtfData = [NSData dataWithContentsOfFile:rtfPath];
            if (!rtfData) {
                return @"";
            }
        }
    }

    if (mutableRTF.length > 0) {
        mutableRTF = [[self preprocessRTF:mutableRTF] mutableCopy];
        rtfData = [mutableRTF dataUsingEncoding:NSUTF8StringEncoding];
    }

    NSString *html = [self htmlFromRTFPath:rtfPath
                                mutableRTF:mutableRTF
                                   rtfData:rtfData
                          preloadedAttrStr:preloaded
                                     error:error];
    if (!html.length) {
        return @"";
    }

    HTMLToMarkdownConverter *converter = [[HTMLToMarkdownConverter alloc] init];
    NSString *markdown = [converter markdownFromHTML:html];
    return [self postprocessMarkdown:markdown ?: @""];
}

+ (NSString *)markdownFromAttributedString:(NSAttributedString *)attrStr {
    if (!attrStr || attrStr.length == 0) {
        return @"";
    }

    NSError *htmlError = nil;
    NSData *htmlData = [attrStr dataFromRange:NSMakeRange(0, attrStr.length)
                         documentAttributes:@{
                             NSDocumentTypeDocumentAttribute : NSHTMLTextDocumentType
                         }
                                      error:&htmlError];
    NSString *html = nil;
    if (htmlData) {
        html = [[NSString alloc] initWithData:htmlData encoding:NSUTF8StringEncoding];
    }
    if (!html.length) {
        return attrStr.string ?: @"";
    }

    html = [self extractBodyContentFromHTML:html];

    HTMLToMarkdownConverter *converter = [[HTMLToMarkdownConverter alloc] init];
    NSString *markdown = [converter markdownFromHTML:html];
    return [self postprocessMarkdown:markdown ?: @""];
}

#pragma mark - HTML pipeline

+ (NSString *)htmlFromRTFPath:(NSString *)rtfPath
                   mutableRTF:(NSMutableString *)mutableRTF
                      rtfData:(NSData *)rtfData
             preloadedAttrStr:(NSAttributedString *)preloadedAttrStr
                        error:(NSError **)error {
    NSAttributedString *attrStr = preloadedAttrStr;
    NSString *html = nil;

    if (!attrStr && rtfData.length > 0) {
        attrStr = [[NSAttributedString alloc] initWithData:rtfData
                                                     options:@{
                                                         NSDocumentTypeDocumentAttribute :
                                                             NSRTFTextDocumentType
                                                     }
                                          documentAttributes:NULL
                                                       error:error];
        (void)preloadedAttrStr;
    }

    if (attrStr.length > 0) {
        NSError *htmlError = nil;
        NSData *htmlData = [attrStr dataFromRange:NSMakeRange(0, attrStr.length)
                             documentAttributes:@{
                                 NSDocumentTypeDocumentAttribute : NSHTMLTextDocumentType
                             }
                                          error:&htmlError];
        if (htmlData) {
            html = [[NSString alloc] initWithData:htmlData encoding:NSUTF8StringEncoding];
        }
    }

    if (!html.length) {
        NSTask *task = [[NSTask alloc] init];
        task.launchPath = @"/usr/bin/textutil";
        task.arguments = @[ @"-convert", @"html", @"-stdout", rtfPath ];
        NSPipe *pipe = [NSPipe pipe];
        task.standardOutput = pipe;
        @try {
            [task launch];
            NSData *data = [[pipe fileHandleForReading] readDataToEndOfFile];
            [task waitUntilExit];
            if (data.length > 0) {
                html = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
            }
        } @catch (NSException *exception) {
            NSLog(@"textutil RTF conversion failed: %@", exception);
        }
    }

    if (!html.length) {
        return nil;
    }

    return [self extractBodyContentFromHTML:html];
}

+ (NSString *)extractBodyContentFromHTML:(NSString *)html {
    if (!html) {
        return @"";
    }
    NSRange bodyStart = [html rangeOfString:@"<body" options:NSCaseInsensitiveSearch];
    if (bodyStart.location == NSNotFound) {
        return html;
    }
    NSRange bodyTagEnd = [html rangeOfString:@">"
                                     options:0
                                       range:NSMakeRange(bodyStart.location,
                                                         html.length - bodyStart.location)];
    if (bodyTagEnd.location == NSNotFound) {
        return html;
    }
    NSUInteger contentStart = bodyTagEnd.location + 1;
    NSRange bodyEnd = [html rangeOfString:@"</body>" options:NSCaseInsensitiveSearch];
    if (bodyEnd.location == NSNotFound || bodyEnd.location <= contentStart) {
        return html;
    }
    return [html substringWithRange:NSMakeRange(contentStart, bodyEnd.location - contentStart)];
}

#pragma mark - RTF preprocessing

+ (NSString *)preprocessRTF:(NSString *)rtf {
    if (!rtf) {
        return @"";
    }
    NSMutableString *input = [rtf mutableCopy];
    NSDictionary *replacements = @{
        @"\u201c" : @"\"",
        @"\u201d" : @"\"",
        @"\u2018" : @"'",
        @"\u2019" : @"'",
        @"\u2013" : @"--",
        @"\u2014" : @"---",
        @"\u2026" : @"..."
    };
    for (NSString *key in replacements) {
        [input replaceOccurrencesOfString:key
                                 withString:replacements[key]
                                    options:0
                                      range:NSMakeRange(0, input.length)];
    }
    return input;
}

+ (NSString *)postprocessMarkdown:(NSString *)markdown {
    if (!markdown.length) {
        return @"";
    }
    NSMutableString *md = [markdown mutableCopy];

    NSRegularExpression *hyperlinkRe = [NSRegularExpression
        regularExpressionWithPattern:
            @"\\[(.*?)\\]\\(\\{\\field\\{\\*\\fldinst\\{HYPERLINK "
            @"\"(.*?)\"\\}\\}\\{\\fldrslt.*?\\}\\}\\)"
                             options:0
                               error:nil];
    md = [[hyperlinkRe stringByReplacingMatchesInString:md
                                              options:0
                                                range:NSMakeRange(0, md.length)
                                         withTemplate:@"[$1]($2)"] mutableCopy];

    NSRegularExpression *blankLinesRe =
        [NSRegularExpression regularExpressionWithPattern:@"\n{3,}" options:0 error:nil];
    md = [[blankLinesRe stringByReplacingMatchesInString:md
                                                 options:0
                                                   range:NSMakeRange(0, md.length)
                                            withTemplate:@"\n\n"] mutableCopy];

    NSRegularExpression *breakRE =
        [NSRegularExpression regularExpressionWithPattern:@"<br */?>" options:0 error:nil];
    md = [[breakRE stringByReplacingMatchesInString:md
                                             options:0
                                               range:NSMakeRange(0, md.length)
                                        withTemplate:@"  \n"] mutableCopy];
    return md;
}

@end
