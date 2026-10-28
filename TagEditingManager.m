//
//  TagEditingManager.m
//  Notation
//

#import "TagEditingManager.h"
#import "NoteObject.h"
#import "NSString_NV.h"
#import "NVTheme.h"
#import "NVThemeManager.h"
#import "LabelsListController.h"
#import <objc/runtime.h>

@implementation NVTagToken

+ (NSArray *)tokensWithTitles:(NSArray *)titles partial:(BOOL)flag {
	NSMutableArray *tokens = [NSMutableArray arrayWithCapacity:[titles count]];
	for (NSString *title in titles) {
		if (![title length])
			continue;
		NVTagToken *token = [[NVTagToken alloc] init];
		[token setTitle:title];
		[token setPartial:flag];
		[tokens addObject:token];
		[token release];
	}
	return tokens;
}

- (void)dealloc {
	[title release];
	[super dealloc];
}

- (NSString *)title {
	return title;
}

- (void)setTitle:(NSString *)aTitle {
	if (title != aTitle) {
		[title release];
		title = [aTitle copy];
	}
}

- (BOOL)isPartial {
	return partial;
}

- (void)setPartial:(BOOL)flag {
	partial = flag;
}

- (id)copyWithZone:(NSZone *)zone {
	NVTagToken *copy = [[NVTagToken allocWithZone:zone] init];
	[copy setTitle:title];
	[copy setPartial:partial];
	return copy;
}

- (BOOL)isEqual:(id)other {
	if (other == self)
		return YES;
	if (![other isKindOfClass:[NVTagToken class]])
		return NO;
	return [[title lowercaseString] isEqualToString:[[other title] lowercaseString]];
}

- (NSUInteger)hash {
	return [[title lowercaseString] hash];
}

@end

static void NVPartialTokenDraw(id self, SEL _cmd, NSRect frame, NSView *view) {
	Class base = class_getSuperclass(object_getClass(self));
	void (*orig)(id, SEL, NSRect, NSView *) = (void (*)(id, SEL, NSRect, NSView *))class_getMethodImplementation(base, _cmd);
	id represented = [self respondsToSelector:@selector(representedObject)] ? [self representedObject] : nil;
	BOOL dim = [represented isKindOfClass:[NVTagToken class]] && [(NVTagToken *)represented isPartial];
	if (!dim) {
		orig(self, _cmd, frame, view);
		return;
	}
	[NSGraphicsContext saveGraphicsState];
	CGContextSetAlpha([[NSGraphicsContext currentContext] CGContext], 0.45);
	orig(self, _cmd, frame, view);
	[NSGraphicsContext restoreGraphicsState];
}

static Class NVPartialTokenCellClass(Class base) {
	static Class dimClass = Nil;
	if (dimClass)
		return dimClass;
	Method method = class_getInstanceMethod(base, @selector(drawWithFrame:inView:));
	if (!method)
		return base;
	dimClass = objc_allocateClassPair(base, "NVPartialTokenAttachmentCell", 0);
	if (!dimClass)
		return base;
	class_addMethod(dimClass, @selector(drawWithFrame:inView:), (IMP)NVPartialTokenDraw, method_getTypeEncoding(method));
	objc_registerClassPair(dimClass);
	return dimClass;
}

static void NVDimPartialTagTokens(NSTokenField *field) {
	NSTextView *editor = (NSTextView *)[field currentEditor];
	NSAttributedString *text = editor ? [editor textStorage] : [field attributedStringValue];
	[text enumerateAttribute:NSAttachmentAttributeName inRange:NSMakeRange(0, [text length]) options:0 usingBlock:^(id value, NSRange range, BOOL *stop) {
		NSTextAttachment *attachment = value;
		if (!attachment)
			return;
		NSTextAttachmentCell *cell = (NSTextAttachmentCell *)[attachment attachmentCell];
		if (!cell || ![cell respondsToSelector:@selector(representedObject)])
			return;
		if (![[cell representedObject] isKindOfClass:[NVTagToken class]] || ![(NVTagToken *)[cell representedObject] isPartial])
			return;
		Class dim = NVPartialTokenCellClass([cell class]);
		if (object_getClass(cell) != dim)
			object_setClass(cell, dim);
	}];
	[field setNeedsDisplay:YES];
}

static void NVContrastTokenEditor(NSTokenField *field) {
	NSTextView *editor = (NSTextView *)[field currentEditor];
	NSColor *textColor = [field textColor];
	if (!textColor || !NVThemeColorIsLight(textColor) || ![editor isKindOfClass:[NSTextView class]])
		return;
	[editor setTextColor:textColor];
	[editor setInsertionPointColor:textColor];
	NSMutableDictionary *typing = [[editor typingAttributes] mutableCopy];
	if (!typing)
		typing = [[NSMutableDictionary alloc] init];
	[typing setObject:textColor forKey:NSForegroundColorAttributeName];
	[editor setTypingAttributes:typing];
	[typing release];
	NSMutableDictionary *selected = [[editor selectedTextAttributes] mutableCopy];
	if (!selected)
		selected = [[NSMutableDictionary alloc] init];
	[selected setObject:textColor forKey:NSForegroundColorAttributeName];
	[selected setObject:[NSColor colorWithCalibratedWhite:1.0 alpha:0.22] forKey:NSBackgroundColorAttributeName];
	[editor setSelectedTextAttributes:selected];
	[selected release];
}

static void NVApplyTagFieldChrome(NSTokenField *field, NSWindow *window) {
	NVEditorTheme *theme = [[NVThemeManager sharedManager] currentEditorTheme];
	NSColor *windowBG = [theme windowBackgroundColor];
	NSAppearance *appearance = [window appearance] ?: [window effectiveAppearance];
	NSAppearanceName match = [appearance bestMatchFromAppearancesWithNames:@[NSAppearanceNameAqua, NSAppearanceNameDarkAqua]];
	BOOL dark = [match isEqualToString:NSAppearanceNameDarkAqua];
	if (windowBG && !NVThemeColorIsLight(windowBG))
		dark = YES;
	[field setDrawsBackground:YES];
	[field setBezeled:YES];
	if (dark) {
		NSColor *fg = [theme windowForegroundColor];
		[field setTextColor:(fg && NVThemeColorIsLight(fg)) ? fg : [NSColor whiteColor]];
		NSColor *bg = NVColorResolvedInAppearance([NSColor textBackgroundColor], appearance);
		[field setBackgroundColor:bg ?: [NSColor colorWithCalibratedWhite:0.16 alpha:1.0]];
	}
}

static NSString *NVKeyForTag(NSString *title) {
	return [title lowercaseString];
}

static NSString *NVSeparatorForLabelString(NSString *labels) {
	return ([labels rangeOfString:@","].location != NSNotFound) ? @"," : @" ";
}

static void NVRemoveTagKey(NSString *key, NoteObject *note) {
	NSString *labels = labelsOfNote(note);
	NSArray *words = [labels labelCompatibleWords];
	NSMutableArray *kept = [NSMutableArray array];
	for (NSString *word in words) {
		if (![NVKeyForTag(word) isEqualToString:key])
			[kept addObject:word];
	}
	[note setLabelString:[kept componentsJoinedByString:NVSeparatorForLabelString(labels)]];
}

static void NVTagGroupsForNotes(NSArray *notes, NSArray **outCommon, NSArray **outPartial) {
	NSMutableDictionary *counts = [NSMutableDictionary dictionary];
	NSMutableDictionary *display = [NSMutableDictionary dictionary];
	NSUInteger noteCount = [notes count];
	for (NoteObject *note in notes) {
		NSMutableSet *seen = [NSMutableSet set];
		for (NSString *tag in [labelsOfNote(note) labelCompatibleWords]) {
			if (![tag length])
				continue;
			NSString *key = [tag lowercaseString];
			if ([seen containsObject:key])
				continue;
			[seen addObject:key];
			if (![display objectForKey:key])
				[display setObject:tag forKey:key];
			NSUInteger count = [[counts objectForKey:key] unsignedIntegerValue] + 1;
			[counts setObject:[NSNumber numberWithUnsignedInteger:count] forKey:key];
		}
	}
	NSMutableArray *common = [NSMutableArray array];
	NSMutableArray *partial = [NSMutableArray array];
	for (NSString *key in display) {
		NSUInteger count = [[counts objectForKey:key] unsignedIntegerValue];
		NSString *title = [display objectForKey:key];
		if (noteCount > 0 && count == noteCount)
			[common addObject:title];
		else if (count > 0)
			[partial addObject:title];
	}
	NSSortDescriptor *sort = [NSSortDescriptor sortDescriptorWithKey:@"self" ascending:YES selector:@selector(caseInsensitiveCompare:)];
	*outCommon = [common sortedArrayUsingDescriptors:[NSArray arrayWithObject:sort]];
	*outPartial = [partial sortedArrayUsingDescriptors:[NSArray arrayWithObject:sort]];
}

static void NVAddTagTitle(NSString *title, NoteObject *note) {
	NSString *labels = labelsOfNote(note);
	NSArray *words = [labels labelCompatibleWords];
	NSString *key = NVKeyForTag(title);
	for (NSString *word in words) {
		if ([NVKeyForTag(word) isEqualToString:key])
			return;
	}
	NSString *separator = NVSeparatorForLabelString(labels);
	NSString *updated = [labels length] ? [labels stringByAppendingFormat:@"%@%@", separator, title] : title;
	[note setLabelString:updated];
}

@implementation NVTagTokenController

- (void)dealloc {
	[targetNotes release];
	[originalLabels release];
	[syncedKeys release];
	[partialKeys release];
	[super dealloc];
}

- (void)setDelegate:(id)aDelegate {
	delegate = aDelegate;
}

- (void)setLabelsSource:(id)aSource {
	labelsSource = aSource;
}

- (void)setTargetNotes:(NSArray *)notes {
	if (targetNotes != notes) {
		[targetNotes release];
		targetNotes = [notes copy];
	}
	[originalLabels release];
	originalLabels = [[NSMutableArray alloc] initWithCapacity:[targetNotes count]];
	for (NoteObject *note in targetNotes) {
		NSString *labels = labelsOfNote(note);
		[originalLabels addObject:labels ? [[labels copy] autorelease] : @""];
	}
}

- (void)attachToField:(NSTokenField *)aField {
	field = aField;
	[field setDelegate:self];
	[field setTokenizingCharacterSet:[NSCharacterSet labelSeparatorCharacterSet]];
	[field setCompletionDelay:0.15];
	[field setTokenStyle:NSTokenStyleRounded];
	NVApplyTagFieldChrome(field, [field window] ?: [NSApp mainWindow]);
}

- (void)showTitles:(NSArray *)titles partialTitles:(NSArray *)partialTitles {
	suppressSync = YES;
	NSMutableArray *tokens = [NSMutableArray array];
	[tokens addObjectsFromArray:[NVTagToken tokensWithTitles:titles partial:NO]];
	[tokens addObjectsFromArray:[NVTagToken tokensWithTitles:partialTitles partial:YES]];
	[field setObjectValue:tokens];
	[syncedKeys release];
	syncedKeys = [[NSMutableSet alloc] init];
	[partialKeys release];
	partialKeys = [[NSMutableSet alloc] init];
	for (NVTagToken *token in tokens)
		[syncedKeys addObject:NVKeyForTag([token title])];
	for (NSString *title in partialTitles)
		[partialKeys addObject:NVKeyForTag(title)];
	suppressSync = NO;
	[field setNeedsDisplay:YES];
	[self scheduleTokenAppearanceUpdate];
}

- (NSArray *)titles {
	NSMutableArray *titles = [NSMutableArray array];
	for (id object in [field objectValue]) {
		if ([object isKindOfClass:[NVTagToken class]] && [[object title] length])
			[titles addObject:[object title]];
		else if ([object isKindOfClass:[NSString class]] && [object length])
			[titles addObject:object];
	}
	return titles;
}

- (NSString *)titleFromTagObject:(id)object {
	if ([object isKindOfClass:[NVTagToken class]])
		return [object title];
	if ([object isKindOfClass:[NSString class]])
		return object;
	return nil;
}

- (void)addTokenKey:(NSString *)key title:(NSString *)title toKeys:(NSMutableSet *)keys titles:(NSMutableArray *)titles {
	if (![title length] || [keys containsObject:key])
		return;
	[keys addObject:key];
	if (titles)
		[titles addObject:title];
}

- (void)collectCommittedTokensIntoKeys:(NSMutableSet *)keys titles:(NSMutableArray *)titles {
	for (id object in [field objectValue]) {
		NSString *title = [self titleFromTagObject:object];
		[self addTokenKey:NVKeyForTag(title) title:title toKeys:keys titles:titles];
	}
}

- (NSSet *)keysVisibleAsAttachments {
	NSMutableSet *keys = [NSMutableSet set];
	NSTextView *editor = (NSTextView *)[field currentEditor];
	if (![editor isKindOfClass:[NSTextView class]])
		return keys;
	NSString *string = [editor string];
	NSTextStorage *storage = [editor textStorage];
	NSUInteger i, length = [string length];
	for (i = 0; i < length; i++) {
		if ([string characterAtIndex:i] != NSAttachmentCharacter)
			continue;
		id value = [storage attribute:NSAttachmentAttributeName atIndex:i effectiveRange:NULL];
		NSTextAttachmentCell *cell = (NSTextAttachmentCell *)[(NSTextAttachment *)value attachmentCell];
		id represented = [cell respondsToSelector:@selector(representedObject)] ? [cell representedObject] : nil;
		NSString *title = [self titleFromTagObject:represented];
		if ([title length])
			[keys addObject:NVKeyForTag(title)];
	}
	return keys;
}

- (NSString *)editingTagKey {
	NSTextView *editor = (NSTextView *)[field currentEditor];
	if (![editor isKindOfClass:[NSTextView class]])
		return nil;
	NSMutableString *plain = [NSMutableString string];
	NSString *string = [editor string];
	NSUInteger i, length = [string length];
	for (i = 0; i < length; i++) {
		unichar ch = [string characterAtIndex:i];
		if (ch != NSAttachmentCharacter)
			[plain appendFormat:@"%C", ch];
	}
	NSString *trimmed = [plain stringByTrimmingCharactersInSet:[NSCharacterSet labelSeparatorCharacterSet]];
	return [trimmed length] ? NVKeyForTag(trimmed) : nil;
}

- (void)syncToNotes {
	if (suppressSync || ![targetNotes count])
		return;
	NSMutableSet *now = [NSMutableSet set];
	NSMutableArray *ordered = [NSMutableArray array];
	[self collectCommittedTokensIntoKeys:now titles:ordered];
	/* Selecting or editing one token can drop the others out of objectValue for a moment. Those tags are still on screen and were not changed. */
	NSSet *visible = [self keysVisibleAsAttachments];
	NSString *editingKey = [self editingTagKey];
	suppressSync = YES;
	for (NSString *key in [[syncedKeys copy] autorelease]) {
		if ([now containsObject:key] || [visible containsObject:key] || [key isEqualToString:editingKey])
			continue;
		for (NoteObject *note in targetNotes)
			NVRemoveTagKey(key, note);
		[syncedKeys removeObject:key];
		[partialKeys removeObject:key];
	}
	for (NSString *title in ordered) {
		NSString *key = NVKeyForTag(title);
		if ([syncedKeys containsObject:key])
			continue;
		for (NoteObject *note in targetNotes)
			NVAddTagTitle(title, note);
		[syncedKeys addObject:key];
		[partialKeys removeObject:key];
		for (id object in [field objectValue]) {
			if ([object isKindOfClass:[NVTagToken class]] && [NVKeyForTag([object title]) isEqualToString:key])
				[(NVTagToken *)object setPartial:NO];
		}
	}
	suppressSync = NO;
	if (!didEnd)
		[self scheduleTokenAppearanceUpdate];
}

- (NSArray *)selectedPartialTokens {
	NSTextView *editor = (NSTextView *)[field currentEditor];
	if (![editor isKindOfClass:[NSTextView class]])
		return nil;
	NSRange selection = [editor selectedRange];
	if (!selection.length || NSMaxRange(selection) > [[editor string] length])
		return nil;
	NSMutableArray *tokens = [NSMutableArray array];
	NSString *string = [editor string];
	NSTextStorage *storage = [editor textStorage];
	NSUInteger i;
	for (i = selection.location; i < NSMaxRange(selection); i++) {
		unichar ch = [string characterAtIndex:i];
		if (ch == NSAttachmentCharacter) {
			id value = [storage attribute:NSAttachmentAttributeName atIndex:i effectiveRange:NULL];
			NSTextAttachmentCell *cell = (NSTextAttachmentCell *)[(NSTextAttachment *)value attachmentCell];
			id represented = [cell respondsToSelector:@selector(representedObject)] ? [cell representedObject] : nil;
			if ([represented isKindOfClass:[NVTagToken class]] && [(NVTagToken *)represented isPartial])
				[tokens addObject:represented];
			continue;
		}
		if (![[NSCharacterSet whitespaceAndNewlineCharacterSet] characterIsMember:ch])
			return nil;
	}
	return [tokens count] ? tokens : nil;
}

- (void)promoteTokens:(NSArray *)tokens {
	suppressSync = YES;
	for (NVTagToken *token in tokens) {
		NSString *key = NVKeyForTag([token title]);
		for (NoteObject *note in targetNotes)
			NVAddTagTitle([token title], note);
		[token setPartial:NO];
		[partialKeys removeObject:key];
		[syncedKeys addObject:key];
		for (id object in [field objectValue]) {
			if ([object isKindOfClass:[NVTagToken class]] && [NVKeyForTag([(NVTagToken *)object title]) isEqualToString:key])
				[(NVTagToken *)object setPartial:NO];
		}
	}
	suppressSync = NO;
	NVDimPartialTagTokens(field);
	[field setNeedsDisplay:YES];
}

- (void)restoreNotes {
	NSUInteger i, count = [targetNotes count];
	suppressSync = YES;
	for (i = 0; i < count && i < [originalLabels count]; i++)
		[(NoteObject *)[targetNotes objectAtIndex:i] setLabelString:[originalLabels objectAtIndex:i]];
	suppressSync = NO;
}

- (void)scheduleTokenAppearanceUpdate {
	if (didEnd || !field)
		return;
	dispatch_async(dispatch_get_main_queue(), ^{
		/* Commit frees the field. The ivar is cleared first so this block cannot message it. */
		if (didEnd || !field)
			return;
		NVDimPartialTagTokens(field);
		NVContrastTokenEditor(field);
	});
}

- (void)finishEditing {
	if (didEnd)
		return;
	didEnd = YES;
	if (cancelled)
		[self restoreNotes];
	else
		[self syncToNotes];
	NSArray *titles = [self titles];
	field = nil;
	if ([delegate respondsToSelector:@selector(tagTokenController:didEndWithTitles:cancelled:)])
		[delegate tagTokenController:self didEndWithTitles:titles cancelled:cancelled];
}

- (void)endEditing {
	[self finishEditing];
}

- (void)cancelEditing {
	cancelled = YES;
	[self finishEditing];
}

- (void)controlTextDidBeginEditing:(NSNotification *)notification {
	NVContrastTokenEditor(field);
	NVDimPartialTagTokens(field);
}

- (void)controlTextDidChange:(NSNotification *)notification {
	NVContrastTokenEditor(field);
	if ([targetNotes count])
		[self syncToNotes];
	[self scheduleTokenAppearanceUpdate];
}

- (BOOL)isAwaitingEndEditing {
	return awaitingEnd;
}

- (void)controlTextDidEndEditing:(NSNotification *)notification {
	if (didEnd)
		return;
	/* Becoming first responder sends this while the field editor is still in place. A real dismissal leaves currentEditor nil once the call stack unwinds. */
	awaitingEnd = YES;
	NSTokenField *endingField = field;
	dispatch_async(dispatch_get_main_queue(), ^{
		awaitingEnd = NO;
		if (didEnd || [endingField currentEditor])
			return;
		[self finishEditing];
	});
}

- (BOOL)control:(NSControl *)control textView:(NSTextView *)textView doCommandBySelector:(SEL)command {
	if (command == @selector(cancelOperation:)) {
		cancelled = YES;
		[[control window] makeFirstResponder:[[control window] contentView]];
		return YES;
	}
	if (command == @selector(insertNewline:)) {
		NSArray *selected = [self selectedPartialTokens];
		if ([selected count]) {
			[self promoteTokens:selected];
			return YES;
		}
		[control validateEditing];
		[[control window] makeFirstResponder:[[control window] contentView]];
		return YES;
	}
	if ([delegate respondsToSelector:@selector(tagTokenController:textView:doCommandBySelector:)])
		return [delegate tagTokenController:self textView:textView doCommandBySelector:command];
	return NO;
}

- (NSString *)tokenField:(NSTokenField *)tokenField displayStringForRepresentedObject:(id)representedObject {
	if ([representedObject isKindOfClass:[NVTagToken class]])
		return [representedObject title];
	if ([representedObject isKindOfClass:[NSString class]])
		return representedObject;
	return nil;
}

- (NSString *)tokenField:(NSTokenField *)tokenField editingStringForRepresentedObject:(id)representedObject {
	return [self tokenField:tokenField displayStringForRepresentedObject:representedObject];
}

- (id)tokenField:(NSTokenField *)tokenField representedObjectForEditingString:(NSString *)editingString {
	NSString *trimmed = [editingString stringByTrimmingCharactersInSet:[NSCharacterSet labelSeparatorCharacterSet]];
	if (![trimmed length])
		return nil;
	NVTagToken *token = [[NVTagToken alloc] init];
	[token setTitle:trimmed];
	[token setPartial:[partialKeys containsObject:NVKeyForTag(trimmed)]];
	return [token autorelease];
}

- (NSArray *)tokenField:(NSTokenField *)tokenField shouldAddObjects:(NSArray *)tokens atIndex:(NSUInteger)index {
	NSMutableArray *accepted = [NSMutableArray array];
	NSMutableArray *existing = [NSMutableArray arrayWithArray:[tokenField objectValue] ?: [NSArray array]];
	/* The field puts the token being committed into objectValue before asking. That pending token is not a duplicate of itself. */
	if (index <= [existing count] && [tokens count] && NSMaxRange(NSMakeRange(index, [tokens count])) <= [existing count]) {
		BOOL pending = YES;
		NSUInteger i;
		for (i = 0; i < [tokens count]; i++) {
			id have = [existing objectAtIndex:index + i];
			id want = [tokens objectAtIndex:i];
			NSString *haveTitle = [have isKindOfClass:[NVTagToken class]] ? [(NVTagToken *)have title] : ([have isKindOfClass:[NSString class]] ? have : nil);
			NSString *wantTitle = [want isKindOfClass:[NVTagToken class]] ? [(NVTagToken *)want title] : ([want isKindOfClass:[NSString class]] ? want : nil);
			if (!haveTitle || !wantTitle || ![NVKeyForTag(haveTitle) isEqualToString:NVKeyForTag(wantTitle)]) {
				pending = NO;
				break;
			}
		}
		if (pending)
			[existing removeObjectsInRange:NSMakeRange(index, [tokens count])];
	}
	NSMutableSet *seen = [NSMutableSet set];
	for (id object in existing) {
		NSString *title = [object isKindOfClass:[NVTagToken class]] ? [(NVTagToken *)object title] : ([object isKindOfClass:[NSString class]] ? object : nil);
		if ([title length])
			[seen addObject:NVKeyForTag(title)];
	}
	for (id object in tokens) {
		NSString *title = [object isKindOfClass:[NVTagToken class]] ? [object title] : ([object isKindOfClass:[NSString class]] ? object : nil);
		title = [title stringByTrimmingCharactersInSet:[NSCharacterSet labelSeparatorCharacterSet]];
		if (![title length])
			continue;
		NSString *key = NVKeyForTag(title);
		if ([seen containsObject:key])
			continue;
		[seen addObject:key];
		if ([object isKindOfClass:[NVTagToken class]])
			[accepted addObject:object];
		else
			[accepted addObject:[self tokenField:tokenField representedObjectForEditingString:title]];
	}
	return accepted;
}

- (NSArray *)tokenField:(NSTokenField *)tokenField completionsForSubstring:(NSString *)substring indexOfToken:(NSInteger)tokenIndex indexOfSelectedItem:(NSInteger *)selectedIndex {
	if (![labelsSource respondsToSelector:@selector(labelTitlesPrefixedByString:indexOfSelectedItem:minusWordSet:)])
		return [NSArray array];
	NSMutableSet *existing = [NSMutableSet set];
	for (id object in [tokenField objectValue]) {
		if ([object isKindOfClass:[NVTagToken class]])
			[existing addObject:[object title]];
	}
	return [labelsSource labelTitlesPrefixedByString:substring indexOfSelectedItem:selectedIndex minusWordSet:existing];
}

- (NSTokenStyle)tokenField:(NSTokenField *)tokenField styleForRepresentedObject:(id)representedObject {
	return NSTokenStyleRounded;
}

@end

@implementation TagEditingManager

- (id)initWithDelegate:(id)del notes:(NSArray *)notes labelsSource:(id)labelsSource atPoint:(NSPoint)centerpoint {
	if ((self = [super init])) {
		if (![NSBundle loadNibNamed:@"TagEditingManager" owner:self]) {
			NSLog(@"Failed to load TagEditer.nib");
		} else {
			isHappening = YES;
			targetNotes = [notes copy];
			NSWindow *themeWindow = [NSApp mainWindow];
			if ([themeWindow appearance])
				[tagPanel setAppearance:[themeWindow appearance]];
			NSView *nibField = tagField;
			NSTokenField *tokens = [[NSTokenField alloc] initWithFrame:[nibField frame]];
			[tokens setAutoresizingMask:[nibField autoresizingMask]];
			[tokens setFont:[nibField font]];
			/* The nib field is already in the autorelease pool. replaceSubview releases the superview's retain; releasing it here frees it before that pool drains. */
			[[nibField superview] replaceSubview:nibField with:tokens];
			tagField = tokens;
			tokenController = [[NVTagTokenController alloc] init];
			[tokenController setDelegate:self];
			[tokenController setLabelsSource:labelsSource];
			[tokenController setTargetNotes:targetNotes];
			[tokenController attachToField:tagField];
			NSArray *common = nil, *partial = nil;
			NVTagGroupsForNotes(targetNotes, &common, &partial);
			[tokenController showTitles:common partialTitles:partial];
			NSRect zRect = [tagPanel frame];
			centerpoint.x -= (zRect.size.width / 2.0);
			centerpoint.y -= (zRect.size.height + 70.0);
			[tagPanel setDelegate:self];
			[tagPanel setInitialFirstResponder:tagField];
			[tagField setTarget:self];
			[tagField setAction:@selector(commitTokens:)];
			[tagPanel setFrameOrigin:centerpoint];
			[tagPanel makeKeyAndOrderFront:del];
			[tagPanel makeFirstResponder:tagField];
		}
	}
	return self;
}

- (void)dealloc {
	[NSObject cancelPreviousPerformRequestsWithTarget:self];
	[tagField setDelegate:nil];
	[tokenController release];
	tokenController = nil;
	[tagField removeFromSuperview];
	[tagField release];
	tagField = nil;
	[targetNotes release];
	[tagPanel setDelegate:nil];
	[tagPanel release];
	[super dealloc];
}

- (void)commitTokens:(id)sender {
	if (!isHappening)
		return;
	[NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(commitIfStillInactive) object:nil];
	[tagField validateEditing];
	[[tagField window] makeFirstResponder:[[tagField window] contentView]];
}

- (void)commitIfStillInactive {
	if (!isHappening || [tagPanel isKeyWindow])
		return;
	[self commitTokens:nil];
}

- (void)tagTokenController:(NVTagTokenController *)controller didEndWithTitles:(NSArray *)titles cancelled:(BOOL)wasCancelled {
	(void)titles;
	(void)wasCancelled;
	if (!isHappening)
		return;
	isHappening = NO;
	[tagPanel orderOut:nil];
	[[NSNotificationCenter defaultCenter] postNotificationName:@"TagEditorShouldRelease" object:nil];
}

- (void)cancelOperation:(id)sender {
	[tagPanel makeFirstResponder:[tagPanel contentView]];
}

- (void)windowDidBecomeKey:(NSNotification *)notification {
	[NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(commitIfStillInactive) object:nil];
}

- (void)windowDidResignKey:(NSNotification *)notification {
	if (!isHappening || ![tagPanel isVisible])
		return;
	/* The completion popup can take key focus for a moment. Wait, and commit only if it does not come back. */
	[self performSelector:@selector(commitIfStillInactive) withObject:nil afterDelay:0.15];
}

- (NSTokenField *)tagField {
	return tagField;
}

- (BOOL)isMultitagging {
	return isHappening;
}

@end
