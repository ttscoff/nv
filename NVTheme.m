//
//  NVTheme.m
//  Notation
//

#import "NVTheme.h"
#import "NVThemeManager.h"

static NSColor *NVColorFromThemeValue(id value) {
	if ([value isKindOfClass:[NSColor class]])
		return value;
	if (![value isKindOfClass:[NSString class]])
		return nil;
	NSString *text = [value stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
	if ([text hasPrefix:@"#"])
		text = [text substringFromIndex:1];
	if ([text length] != 3 && [text length] != 6 && [text length] != 8)
		return nil;
	NSScanner *scanner = [NSScanner scannerWithString:text];
	unsigned long long hex = 0;
	if (![scanner scanHexLongLong:&hex] || ![scanner isAtEnd])
		return nil;
	unsigned int red, green, blue, alpha = 255;
	if ([text length] == 3) {
		red = (unsigned int)((hex >> 8) & 0xF) * 17;
		green = (unsigned int)((hex >> 4) & 0xF) * 17;
		blue = (unsigned int)(hex & 0xF) * 17;
	} else if ([text length] == 6) {
		red = (unsigned int)((hex >> 16) & 0xFF);
		green = (unsigned int)((hex >> 8) & 0xFF);
		blue = (unsigned int)(hex & 0xFF);
	} else {
		red = (unsigned int)((hex >> 24) & 0xFF);
		green = (unsigned int)((hex >> 16) & 0xFF);
		blue = (unsigned int)((hex >> 8) & 0xFF);
		alpha = (unsigned int)(hex & 0xFF);
	}
	return [NSColor colorWithSRGBRed:red / 255.0 green:green / 255.0 blue:blue / 255.0 alpha:alpha / 255.0];
}

static BOOL NVBoolFromThemeValue(id value) {
	if ([value isKindOfClass:[NSNumber class]] || [value isKindOfClass:[NSString class]])
		return [value boolValue];
	return NO;
}

static BOOL NVThemeValueIsPresent(id value) {
	return value && value != [NSNull null];
}

@implementation NVThemeStyle

+ (NVThemeStyle *)styleWithDictionary:(NSDictionary *)dict {
	if (![dict isKindOfClass:[NSDictionary class]])
		return nil;
	NVThemeStyle *style = [[NVThemeStyle alloc] init];
	NSColor *foreground = NVColorFromThemeValue([dict objectForKey:@"foregroundColor"]);
	if (foreground) {
		style->foregroundColor = [foreground retain];
		style->presentMask |= NVThemeStyleForeground;
	}
	NSColor *background = NVColorFromThemeValue([dict objectForKey:@"backgroundColor"]);
	if (background) {
		style->backgroundColor = [background retain];
		style->presentMask |= NVThemeStyleBackground;
	}
	NSColor *markup = NVColorFromThemeValue([dict objectForKey:@"markupForegroundColor"]);
	if (markup) {
		style->markupForegroundColor = [markup retain];
		style->presentMask |= NVThemeStyleMarkupForeground;
	}
	if (NVThemeValueIsPresent([dict objectForKey:@"bold"])) {
		style->bold = NVBoolFromThemeValue([dict objectForKey:@"bold"]);
		style->presentMask |= NVThemeStyleBold;
	}
	if (NVThemeValueIsPresent([dict objectForKey:@"italic"])) {
		style->italic = NVBoolFromThemeValue([dict objectForKey:@"italic"]);
		style->presentMask |= NVThemeStyleItalic;
	}
	if (NVThemeValueIsPresent([dict objectForKey:@"strikethrough"])) {
		style->strikethrough = NVBoolFromThemeValue([dict objectForKey:@"strikethrough"]);
		style->presentMask |= NVThemeStyleStrikethrough;
	}
	if (NVThemeValueIsPresent([dict objectForKey:@"mono"])) {
		style->mono = NVBoolFromThemeValue([dict objectForKey:@"mono"]);
		style->presentMask |= NVThemeStyleMono;
	}
	id fontName = [dict objectForKey:@"fontName"];
	if ([fontName isKindOfClass:[NSString class]] && [fontName length]) {
		style->fontName = [fontName retain];
		style->presentMask |= NVThemeStyleFontName;
	}
	id scale = [dict objectForKey:@"fontScale"];
	if ([scale isKindOfClass:[NSNumber class]]) {
		style->fontScale = [scale doubleValue];
		style->presentMask |= NVThemeStyleFontScale;
	}
	return [style autorelease];
}

+ (NVThemeStyle *)styleWithForegroundColor:(NSColor *)color {
	NVThemeStyle *style = [[NVThemeStyle alloc] init];
	if (color) {
		style->foregroundColor = [color retain];
		style->presentMask |= NVThemeStyleForeground;
	}
	return [style autorelease];
}

- (void)dealloc {
	[foregroundColor release];
	[backgroundColor release];
	[markupForegroundColor release];
	[fontName release];
	[super dealloc];
}

- (id)copyWithZone:(NSZone *)zone {
	NVThemeStyle *copy = [[NVThemeStyle allocWithZone:zone] init];
	copy->presentMask = presentMask;
	copy->foregroundColor = [foregroundColor retain];
	copy->backgroundColor = [backgroundColor retain];
	copy->markupForegroundColor = [markupForegroundColor retain];
	copy->fontName = [fontName retain];
	copy->fontScale = fontScale;
	copy->bold = bold;
	copy->italic = italic;
	copy->strikethrough = strikethrough;
	copy->mono = mono;
	return copy;
}

- (NVThemeStyle *)styleByOverlayingStyle:(NVThemeStyle *)overlay {
	if (!overlay)
		return self;
	NVThemeStyle *result = [[self copy] autorelease];
	if (overlay->presentMask & NVThemeStyleForeground) {
		[result->foregroundColor release];
		result->foregroundColor = [overlay->foregroundColor retain];
		result->presentMask |= NVThemeStyleForeground;
	}
	if (overlay->presentMask & NVThemeStyleBackground) {
		[result->backgroundColor release];
		result->backgroundColor = [overlay->backgroundColor retain];
		result->presentMask |= NVThemeStyleBackground;
	}
	if (overlay->presentMask & NVThemeStyleMarkupForeground) {
		[result->markupForegroundColor release];
		result->markupForegroundColor = [overlay->markupForegroundColor retain];
		result->presentMask |= NVThemeStyleMarkupForeground;
	}
	if (overlay->presentMask & NVThemeStyleBold) {
		result->bold = overlay->bold;
		result->presentMask |= NVThemeStyleBold;
	}
	if (overlay->presentMask & NVThemeStyleItalic) {
		result->italic = overlay->italic;
		result->presentMask |= NVThemeStyleItalic;
	}
	if (overlay->presentMask & NVThemeStyleStrikethrough) {
		result->strikethrough = overlay->strikethrough;
		result->presentMask |= NVThemeStyleStrikethrough;
	}
	if (overlay->presentMask & NVThemeStyleMono) {
		result->mono = overlay->mono;
		result->presentMask |= NVThemeStyleMono;
	}
	if (overlay->presentMask & NVThemeStyleFontName) {
		[result->fontName release];
		result->fontName = [overlay->fontName retain];
		result->presentMask |= NVThemeStyleFontName;
	}
	if (overlay->presentMask & NVThemeStyleFontScale) {
		result->fontScale = overlay->fontScale;
		result->presentMask |= NVThemeStyleFontScale;
	}
	return result;
}

- (NSColor *)foregroundColor {
	return (presentMask & NVThemeStyleForeground) ? foregroundColor : nil;
}

- (NSColor *)backgroundColor {
	return (presentMask & NVThemeStyleBackground) ? backgroundColor : nil;
}

- (NSColor *)markupForegroundColor {
	return (presentMask & NVThemeStyleMarkupForeground) ? markupForegroundColor : nil;
}

- (BOOL)isBold {
	return (presentMask & NVThemeStyleBold) && bold;
}

- (BOOL)isItalic {
	return (presentMask & NVThemeStyleItalic) && italic;
}

- (BOOL)isStrikethrough {
	return (presentMask & NVThemeStyleStrikethrough) && strikethrough;
}

- (NSFont *)fontBasedOnFont:(NSFont *)baseFont {
	if (!baseFont)
		baseFont = [NSFont userFixedPitchFontOfSize:14.0];
	BOOL wantsFont = (presentMask & (NVThemeStyleFontName | NVThemeStyleFontScale | NVThemeStyleMono | NVThemeStyleBold | NVThemeStyleItalic)) != 0;
	if (!wantsFont)
		return nil;
	CGFloat size = [baseFont pointSize];
	if ((presentMask & NVThemeStyleFontScale) && fontScale > 0.1)
		size = size * (CGFloat)fontScale;
	NSFont *font = baseFont;
	NSString *chosenName = nil;
	if ((presentMask & NVThemeStyleFontName) && [fontName length])
		chosenName = fontName;
	else if ((presentMask & NVThemeStyleMono) && mono)
		chosenName = @"Menlo";
	if (chosenName) {
		NSFont *named = [NSFont fontWithName:chosenName size:size];
		if (named)
			font = named;
	} else if (fabs(size - [baseFont pointSize]) > 0.1) {
		NSFont *resized = [NSFont fontWithName:[baseFont fontName] size:size];
		if (resized)
			font = resized;
	}
	NSFontTraitMask traits = 0;
	if ([self isBold])
		traits |= NSBoldFontMask;
	if ([self isItalic])
		traits |= NSItalicFontMask;
	if (traits)
		font = [[NSFontManager sharedFontManager] convertFont:font toHaveTrait:traits] ?: font;
	return font;
}

@end

@implementation NVEditorTheme

+ (NSDictionary *)dictionaryFromThemeData:(NSData *)data {
	if (![data length])
		return nil;
	const unsigned char *bytes = [data bytes];
	NSUInteger index = 0;
	while (index < [data length] && (bytes[index] == ' ' || bytes[index] == '\n' || bytes[index] == '\r' || bytes[index] == '\t'))
		index++;
	if (index < [data length] && (bytes[index] == '{' || bytes[index] == '[')) {
		id json = [NSJSONSerialization JSONObjectWithData:data options:0 error:NULL];
		if ([json isKindOfClass:[NSDictionary class]])
			return json;
	}
	id plist = [NSPropertyListSerialization propertyListWithData:data options:NSPropertyListImmutable format:NULL error:NULL];
	if ([plist isKindOfClass:[NSDictionary class]])
		return plist;
	id json = [NSJSONSerialization JSONObjectWithData:data options:0 error:NULL];
	if ([json isKindOfClass:[NSDictionary class]])
		return json;
	return nil;
}

+ (NVEditorTheme *)themeWithData:(NSData *)data filename:(NSString *)filename {
	NSDictionary *root = [self dictionaryFromThemeData:data];
	if (!root)
		return nil;
	NVEditorTheme *theme = [[NVEditorTheme alloc] init];
	theme->filename = [filename copy];
	theme->name = [[[filename stringByDeletingPathExtension] lastPathComponent] copy];
	NSDictionary *textView = [root objectForKey:@"textView"];
	if (![textView isKindOfClass:[NSDictionary class]])
		textView = nil;
	theme->windowBackgroundColor = [NVColorFromThemeValue([root objectForKey:@"backgroundColor"]) retain];
	theme->windowForegroundColor = [NVColorFromThemeValue([root objectForKey:@"foregroundColor"]) retain];
	theme->backgroundColor = [NVColorFromThemeValue([textView objectForKey:@"backgroundColor"]) retain];
	if (!theme->backgroundColor)
		theme->backgroundColor = [theme->windowBackgroundColor retain];
	theme->foregroundColor = [NVColorFromThemeValue([textView objectForKey:@"foregroundColor"]) retain];
	if (!theme->foregroundColor)
		theme->foregroundColor = [theme->windowForegroundColor retain];
	theme->caretColor = [NVColorFromThemeValue([textView objectForKey:@"caretColor"]) retain];
	if (!theme->caretColor)
		theme->caretColor = [NVColorFromThemeValue([root objectForKey:@"caretColor"]) retain];
	theme->selectionBackgroundColor = [NVColorFromThemeValue([root objectForKey:@"selectionBackgroundColor"]) retain];
	NSMutableDictionary *parsed = [NSMutableDictionary dictionary];
	for (NSString *key in root) {
		id value = [root objectForKey:key];
		if (![value isKindOfClass:[NSDictionary class]])
			continue;
		NVThemeStyle *style = [NVThemeStyle styleWithDictionary:value];
		if (style)
			[parsed setObject:style forKey:key];
	}
	theme->styles = [parsed copy];
	return [theme autorelease];
}

- (void)dealloc {
	[name release];
	[filename release];
	[backgroundColor release];
	[foregroundColor release];
	[windowBackgroundColor release];
	[windowForegroundColor release];
	[caretColor release];
	[selectionBackgroundColor release];
	[styles release];
	[super dealloc];
}

- (NSString *)name {
	return name;
}

- (NSString *)filename {
	return filename;
}

- (NVThemeStyle *)styleForKey:(NSString *)key {
	return [styles objectForKey:key];
}

- (NVThemeStyle *)styleForHeadingLevel:(NSUInteger)level {
	NVThemeStyle *base = [self styleForKey:@"heading"];
	if (level < 1)
		return base;
	NVThemeStyle *specific = [self styleForKey:[NSString stringWithFormat:@"heading%lu", (unsigned long)level]];
	if (!base)
		return specific;
	return [base styleByOverlayingStyle:specific];
}

- (NSColor *)windowBackgroundColor {
	return windowBackgroundColor;
}

- (NSColor *)windowForegroundColor {
	return windowForegroundColor;
}

- (NSColor *)editorBackgroundColor {
	return backgroundColor;
}

- (NSColor *)editorForegroundColor {
	return foregroundColor;
}

- (NSColor *)editorCaretColor {
	return caretColor;
}

- (NSColor *)editorSelectionColor {
	return selectionBackgroundColor;
}

BOOL NVThemeColorIsLight(NSColor *color) {
	NSColor *rgb = [color colorUsingColorSpace:[NSColorSpace sRGBColorSpace]];
	if (!rgb || [rgb numberOfComponents] < 2)
		return YES;
	CGFloat components[4] = {0, 0, 0, 0};
	[rgb getComponents:components];
	if ([rgb numberOfComponents] >= 3)
		return ((0.299 * components[0]) + (0.587 * components[1]) + (0.114 * components[2])) >= 0.55;
	return components[0] >= 0.55;
}

NSColor *NVColorResolvedInAppearance(NSColor *color, NSAppearance *appearance) {
	if (!color)
		return nil;
	if (!appearance)
		appearance = [NSApp effectiveAppearance];
	__block NSColor *resolved = color;
	[appearance performAsCurrentDrawingAppearance:^{
		NSColor *components = [color colorUsingType:NSColorTypeComponentBased];
		if (components)
			resolved = components;
	}];
	return resolved;
}

- (NSColor *)listBackgroundColor {
	return [[self styleForKey:@"tableView"] backgroundColor] ?: windowBackgroundColor ?: backgroundColor;
}

- (NSColor *)listForegroundColor {
	return [[self styleForKey:@"tableView"] foregroundColor] ?: windowForegroundColor ?: foregroundColor;
}

- (NSColor *)listSelectionColor {
	return [[self styleForKey:@"tableViewHighlighted"] backgroundColor] ?: selectionBackgroundColor;
}

- (NSColor *)listInactiveSelectionColor {
	return [[self styleForKey:@"tableViewInactive"] backgroundColor] ?: [self listSelectionColor];
}

- (NSColor *)listSelectionForegroundColor {
	NSColor *explicit = [[self styleForKey:@"tableViewHighlighted"] foregroundColor];
	if (explicit)
		return explicit;
	NSColor *selection = [self listSelectionColor];
	NSColor *foreground = [self listForegroundColor];
	if (selection && NVThemeColorIsLight(selection))
		return (foreground && !NVThemeColorIsLight(foreground)) ? foreground : [NSColor blackColor];
	return [NSColor whiteColor];
}

- (NSColor *)listInactiveSelectionForegroundColor {
	NSColor *explicit = [[self styleForKey:@"tableViewInactive"] foregroundColor];
	if (explicit)
		return explicit;
	return [self listForegroundColor] ?: [self listSelectionForegroundColor];
}

@end

NSColor *NVWindowChromeColor(void) {
	NSColor *color = [[[NVThemeManager sharedManager] currentEditorTheme] windowBackgroundColor];
	if (color)
		return color;
	return NVColorResolvedInAppearance([NSColor windowBackgroundColor], [NSApp effectiveAppearance]);
}

NSColor *NVWindowChromeKnobColor(void) {
	NSColor *track = NVWindowChromeColor();
	NSColor *foreground = [[[NVThemeManager sharedManager] currentEditorTheme] windowForegroundColor];
	if (foreground && track && (NVThemeColorIsLight(foreground) != NVThemeColorIsLight(track))) {
		if ([foreground alphaComponent] < 0.85)
			foreground = [foreground colorWithAlphaComponent:0.85];
		return foreground;
	}
	BOOL light = !track || NVThemeColorIsLight(track);
	return [NSColor colorWithCalibratedWhite:(light ? 0.22 : 0.92) alpha:1.0];
}
