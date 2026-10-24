//
//  AppController_Markdown.m
//  Notation
//
//  Converts a notes database to Markdown files in a new folder.

#import "AppController.h"
#import "GlobalPrefs.h"
#import "NotationController.h"
#import "NotationFileManager.h"
#import "NotationPrefs.h"
#import "NoteObject.h"
#import "NSData_transformations.h"
#import "NSFileManager_NV.h"
#import "NVRTFConverter.h"

static NSString * const NVOfferedMarkdownConversionKey = @"NVOfferedMarkdownConversion";
static BOOL nvMarkdownConversionInProgress = NO;

@interface AppController (MarkdownMigration)
- (void)beginMarkdownConversion;
- (BOOL)attributedStringHasRichText:(NSAttributedString *)string;
- (NSString *)markdownBodyForNote:(NoteObject *)note;
- (NSString *)markdownFilenameForTitle:(NSString *)title usedNames:(NSMutableSet *)usedNames;
- (void)showMarkdownAlertWithMessage:(NSString *)message info:(NSString *)info;
@end

@implementation AppController (MarkdownMigration)

- (void)offerMarkdownConversionIfNeeded {
	if (nvMarkdownConversionInProgress || !notationController)
		return;
	if ([[NSUserDefaults standardUserDefaults] boolForKey:NVOfferedMarkdownConversionKey])
		return;
	if ([[notationController notationPrefs] notesStorageFormat] != RTFTextFormat)
		return;
	if ([notationController totalNoteCount] == 0)
		return;

	[NSApp activateIgnoringOtherApps:YES];
	NSAlert *alert = [[NSAlert alloc] init];
	[alert setMessageText:NSLocalizedString(@"Convert Rich Text notes to Markdown?", @"RTF launch prompt title")];
	[alert setInformativeText:NSLocalizedString(@"nvALT 3.0 stores notes as plain text files so they can move to nvUltra. Your notes are stored as Rich Text. Convert them to Markdown in a new folder and use that folder from now on? The current folder is left unchanged.", @"RTF launch prompt message")];
	[alert addButtonWithTitle:NSLocalizedString(@"Convert to Markdown", @"RTF conversion confirm button")];
	[alert addButtonWithTitle:NSLocalizedString(@"Not Now", @"RTF conversion decline button")];
	NSModalResponse response = [alert runModal];
	[alert release];

	if (response != NSAlertFirstButtonReturn) {
		[[NSUserDefaults standardUserDefaults] setBool:YES forKey:NVOfferedMarkdownConversionKey];
		return;
	}
	[self beginMarkdownConversion];
}

- (IBAction)convertNotesToMarkdown:(id)sender {
	(void)sender;
	if (nvMarkdownConversionInProgress || !notationController)
		return;
	[self beginMarkdownConversion];
}

- (void)beginMarkdownConversion {
	if ([[notationController notationPrefs] doesEncryption]) {
		[self showMarkdownAlertWithMessage:NSLocalizedString(@"Turn off encryption first", @"Markdown conversion blocked title")
									   info:NSLocalizedString(@"Markdown notes are plain text files. Turn off encryption in Notes settings, then convert.", @"Markdown conversion blocked message")];
		return;
	}
	if ([notationController totalNoteCount] == 0) {
		[self showMarkdownAlertWithMessage:NSLocalizedString(@"There are no notes to convert", @"Markdown conversion empty title")
									   info:NSLocalizedString(@"Add a note first, or choose a notes folder that already has notes.", @"Markdown conversion empty message")];
		return;
	}

	NSOpenPanel *panel = [NSOpenPanel openPanel];
	[panel setCanChooseFiles:NO];
	[panel setCanChooseDirectories:YES];
	[panel setCanCreateDirectories:YES];
	[panel setAllowsMultipleSelection:NO];
	[panel setPrompt:NSLocalizedString(@"Convert", @"Markdown folder panel button")];
	[panel setMessage:NSLocalizedString(@"Create a new folder. nvALT will write Markdown copies of your notes there and then store notes in that folder. The current folder is left unchanged.", @"Markdown folder panel message")];
	if ([panel runModal] != NSModalResponseOK)
		return;

	NSString *folder = [[[panel URL] path] stringByStandardizingPath];
	NSString *currentFolder = [[notationController notesDirectoryPath] stringByStandardizingPath];
	if (currentFolder && [folder caseInsensitiveCompare:currentFolder] == NSOrderedSame) {
		[self showMarkdownAlertWithMessage:NSLocalizedString(@"Choose a different folder", @"Markdown conversion same-folder title")
									   info:NSLocalizedString(@"Pick a new folder so the current notes stay where they are.", @"Markdown conversion same-folder message")];
		return;
	}

	NSArray *contents = [[NSFileManager defaultManager] contentsOfDirectoryAtPath:folder error:NULL];
	NSMutableArray *existing = [NSMutableArray array];
	NSString *name;
	for (name in contents) {
		if ([name isEqualToString:@".DS_Store"] || [name isEqualToString:@".localized"])
			continue;
		[existing addObject:name];
	}
	if ([existing count] || [[NSFileManager defaultManager] fileExistsAtPath:[folder stringByAppendingPathComponent:NotesDatabaseFileName]]) {
		[self showMarkdownAlertWithMessage:NSLocalizedString(@"That folder is not empty", @"Markdown conversion occupied folder title")
									   info:NSLocalizedString(@"Create a new empty folder. A folder that already contains notes or a Notes & Settings database will not be changed.", @"Markdown conversion occupied folder message")];
		return;
	}

	nvMarkdownConversionInProgress = YES;
	[window setEnabled:NO];

	NSRect panelFrame = NSMakeRect(0, 0, 380, 84);
	NSWindow *progressWindow = [[NSWindow alloc] initWithContentRect:panelFrame
															styleMask:NSWindowStyleMaskTitled
															  backing:NSBackingStoreBuffered
																defer:NO];
	[progressWindow setTitle:NSLocalizedString(@"Converting Notes", @"Markdown conversion progress title")];
	[progressWindow setReleasedWhenClosed:NO];
	NSTextField *label = [[NSTextField alloc] initWithFrame:NSMakeRect(20, 48, 340, 20)];
	[label setStringValue:NSLocalizedString(@"Writing Markdown files…", @"Markdown conversion progress label")];
	[label setEditable:NO];
	[label setBezeled:NO];
	[label setDrawsBackground:NO];
	[[progressWindow contentView] addSubview:label];
	[label release];
	NSProgressIndicator *indicator = [[NSProgressIndicator alloc] initWithFrame:NSMakeRect(20, 22, 340, 16)];
	[indicator setIndeterminate:NO];
	[indicator setMinValue:0];
	NSArray *notes = [[notationController notes] retain];
	[indicator setMaxValue:MAX((double)[notes count], 1)];
	[[progressWindow contentView] addSubview:indicator];
	[progressWindow center];
	[progressWindow makeKeyAndOrderFront:nil];

	NSMutableSet *usedNames = [NSMutableSet set];
	NSFileManager *fileManager = [NSFileManager defaultManager];
	NSString *failedName = nil;
	NSError *writeError = nil;
	NSUInteger written = 0;
	NSUInteger index;
	for (index = 0; index < [notes count]; index++) {
		NoteObject *note = [notes objectAtIndex:index];
		NSString *filename = [self markdownFilenameForTitle:titleOfNote(note) usedNames:usedNames];
		NSString *path = [folder stringByAppendingPathComponent:filename];
		NSString *body = [self markdownBodyForNote:note];
		if (!body)
			body = @"";
		if (![body writeToFile:path atomically:YES encoding:NSUTF8StringEncoding error:&writeError]) {
			failedName = filename;
			break;
		}
		NSDate *created = [NSDate dateWithTimeIntervalSinceReferenceDate:createdDateOfNote(note)];
		NSDate *modified = [NSDate dateWithTimeIntervalSinceReferenceDate:modifiedDateOfNote(note)];
		[fileManager setAttributes:[NSDictionary dictionaryWithObjectsAndKeys:
									created, NSFileCreationDate,
									modified, NSFileModificationDate, nil]
					  ofItemAtPath:path
							 error:NULL];
		NSArray *tags = [note orderedLabelTitles];
		if ([tags count])
			[fileManager setTags:tags atFSPath:[path fileSystemRepresentation]];
		written++;
		[indicator setDoubleValue:(double)written];
		[indicator displayIfNeeded];
		[progressWindow displayIfNeeded];
	}

	[indicator release];
	[progressWindow orderOut:nil];
	[progressWindow release];
	[notes release];
	[window setEnabled:YES];
	nvMarkdownConversionInProgress = NO;

	if (failedName) {
		NSString *reason = [writeError localizedDescription] ?: NSLocalizedString(@"The file could not be written.", @"Markdown write failure fallback");
		[self showMarkdownAlertWithMessage:[NSString stringWithFormat:NSLocalizedString(@"Couldn't write %@", @"Markdown write failure title"), failedName]
									   info:[NSString stringWithFormat:NSLocalizedString(@"%@ The current notes folder was not changed. The new folder may contain a partial conversion.", @"Markdown write failure message"), reason]];
		return;
	}

	FSRef folderRef;
	if (FSPathMakeRef((const UInt8 *)[folder fileSystemRepresentation], &folderRef, NULL) != noErr) {
		[self showMarkdownAlertWithMessage:NSLocalizedString(@"Couldn't open the new folder", @"Markdown folder switch failure title")
									   info:NSLocalizedString(@"The Markdown files were written, but nvALT could not switch to that folder.", @"Markdown folder switch failure message")];
		return;
	}
	NSData *alias = [NSData aliasDataForFSRef:&folderRef];
	[[GlobalPrefs defaultPrefs] setAliasDataForDefaultDirectory:alias sender:self];

	NSString *now = [[notationController notesDirectoryPath] stringByStandardizingPath];
	if (!now || [now caseInsensitiveCompare:folder] != NSOrderedSame)
		return;

	[[NSUserDefaults standardUserDefaults] setBool:YES forKey:NVOfferedMarkdownConversionKey];
	[self showMarkdownAlertWithMessage:NSLocalizedString(@"Notes converted", @"Markdown conversion success title")
								   info:[NSString stringWithFormat:NSLocalizedString(@"Converted %lu notes to Markdown. nvALT is now storing notes in that folder.", @"Markdown conversion success message"), (unsigned long)written]];
}

- (BOOL)attributedStringHasRichText:(NSAttributedString *)string {
	if (!string || [string length] == 0)
		return NO;
	__block BOOL rich = NO;
	[string enumerateAttributesInRange:NSMakeRange(0, [string length])
								options:0
							 usingBlock:^(NSDictionary *attrs, NSRange range, BOOL *stop) {
		(void)range;
		if ([attrs objectForKey:NSLinkAttributeName] ||
			[attrs objectForKey:NSAttachmentAttributeName] ||
			[attrs objectForKey:NSUnderlineStyleAttributeName] ||
			[attrs objectForKey:NSStrikethroughStyleAttributeName] ||
			[attrs objectForKey:NSSuperscriptAttributeName] ||
			[attrs objectForKey:NSStrokeWidthAttributeName] ||
			[attrs objectForKey:NSObliquenessAttributeName]) {
			rich = YES;
			*stop = YES;
			return;
		}
		NSFont *font = [attrs objectForKey:NSFontAttributeName];
		if (font) {
			NSFontSymbolicTraits traits = [[NSFontManager sharedFontManager] traitsOfFont:font];
			if (traits & (NSFontBoldTrait | NSFontItalicTrait)) {
				rich = YES;
				*stop = YES;
				return;
			}
		}
		NSParagraphStyle *style = [attrs objectForKey:NSParagraphStyleAttributeName];
		if (style && [[style textLists] count] > 0) {
			rich = YES;
			*stop = YES;
		}
	}];
	return rich;
}

- (NSString *)markdownBodyForNote:(NoteObject *)note {
	NSAttributedString *content = [note contentString];
	NSString *plain = [content string] ?: @"";
	NSInteger format = storageFormatOfNote(note);
	BOOL convert = (format == RTFTextFormat || format == HTMLFormat || format == WordDocFormat || format == WordXMLFormat);
	if (format == SingleDatabaseFormat)
		convert = [self attributedStringHasRichText:content];
	if (!convert)
		return plain;

	NSString *markdown = [NVRTFConverter markdownFromAttributedString:content];
	if (!markdown || ([markdown length] == 0 && [plain length] > 0))
		return plain;
	return markdown;
}

- (NSString *)markdownFilenameForTitle:(NSString *)title usedNames:(NSMutableSet *)usedNames {
	NSMutableString *base = [[(title ?: @"") mutableCopy] autorelease];
	NSCharacterSet *illegal = [NSCharacterSet characterSetWithCharactersInString:@"/:\\?%*|\"<>"];
	NSUInteger i;
	for (i = 0; i < [base length]; i++) {
		unichar character = [base characterAtIndex:i];
		if (character < 32 || [illegal characterIsMember:character])
			[base replaceCharactersInRange:NSMakeRange(i, 1) withString:@"-"];
	}
	NSString *trimmed = [base stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
	if ([trimmed length] == 0)
		trimmed = NSLocalizedString(@"Untitled", @"Fallback Markdown filename");
	if ([trimmed characterAtIndex:0] == (unichar)'.')
		trimmed = [@"_" stringByAppendingString:trimmed];
	if ([trimmed length] > 80)
		trimmed = [trimmed substringToIndex:80];
	trimmed = [trimmed stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
	if ([trimmed length] == 0)
		trimmed = @"Untitled";

	NSString *filename = [trimmed stringByAppendingPathExtension:@"md"];
	NSUInteger suffix = 2;
	while ([usedNames containsObject:[filename lowercaseString]]) {
		filename = [[NSString stringWithFormat:@"%@ %lu", trimmed, (unsigned long)suffix] stringByAppendingPathExtension:@"md"];
		suffix++;
	}
	[usedNames addObject:[filename lowercaseString]];
	return filename;
}

- (void)showMarkdownAlertWithMessage:(NSString *)message info:(NSString *)info {
	[NSApp activateIgnoringOtherApps:YES];
	NSAlert *alert = [[NSAlert alloc] init];
	[alert setMessageText:message];
	[alert setInformativeText:info];
	[alert addButtonWithTitle:NSLocalizedString(@"OK", nil)];
	[alert runModal];
	[alert release];
}

@end
