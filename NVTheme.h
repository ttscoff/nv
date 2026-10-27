//
//  NVTheme.h
//  Notation
//
//  MultiMarkdown Composer editor themes (.theme JSON or property list).

#import <Cocoa/Cocoa.h>

enum {
	NVThemeStyleForeground = 1 << 0,
	NVThemeStyleBackground = 1 << 1,
	NVThemeStyleMarkupForeground = 1 << 2,
	NVThemeStyleBold = 1 << 3,
	NVThemeStyleItalic = 1 << 4,
	NVThemeStyleStrikethrough = 1 << 5,
	NVThemeStyleMono = 1 << 6,
	NVThemeStyleFontName = 1 << 7,
	NVThemeStyleFontScale = 1 << 8
};

@interface NVThemeStyle : NSObject <NSCopying> {
	NSUInteger presentMask;
	NSColor *foregroundColor;
	NSColor *backgroundColor;
	NSColor *markupForegroundColor;
	NSString *fontName;
	CGFloat fontScale;
	BOOL bold;
	BOOL italic;
	BOOL strikethrough;
	BOOL mono;
}

+ (NVThemeStyle *)styleWithDictionary:(NSDictionary *)dict;
+ (NVThemeStyle *)styleWithForegroundColor:(NSColor *)color;
- (NVThemeStyle *)styleByOverlayingStyle:(NVThemeStyle *)overlay;
- (NSColor *)foregroundColor;
- (NSColor *)backgroundColor;
- (NSColor *)markupForegroundColor;
- (BOOL)isBold;
- (BOOL)isItalic;
- (BOOL)isStrikethrough;
- (NSFont *)fontBasedOnFont:(NSFont *)baseFont;

@end

@interface NVEditorTheme : NSObject {
	NSString *name;
	NSString *filename;
	NSColor *backgroundColor;
	NSColor *foregroundColor;
	NSColor *windowBackgroundColor;
	NSColor *windowForegroundColor;
	NSColor *caretColor;
	NSColor *selectionBackgroundColor;
	NSDictionary *styles;
}

+ (NVEditorTheme *)themeWithData:(NSData *)data filename:(NSString *)filename;
- (NSString *)name;
- (NSString *)filename;
- (NVThemeStyle *)styleForKey:(NSString *)key;
- (NVThemeStyle *)styleForHeadingLevel:(NSUInteger)level;
- (NSColor *)windowBackgroundColor;
- (NSColor *)windowForegroundColor;
- (NSColor *)editorBackgroundColor;
- (NSColor *)editorForegroundColor;
- (NSColor *)editorCaretColor;
- (NSColor *)editorSelectionColor;
- (NSColor *)listBackgroundColor;
- (NSColor *)listForegroundColor;
- (NSColor *)listSelectionColor;
- (NSColor *)listInactiveSelectionColor;
- (NSColor *)listSelectionForegroundColor;
- (NSColor *)listInactiveSelectionForegroundColor;

@end

BOOL NVThemeColorIsLight(NSColor *color);
NSColor *NVColorResolvedInAppearance(NSColor *color, NSAppearance *appearance);
NSColor *NVWindowChromeColor(void);
NSColor *NVWindowChromeKnobColor(void);
