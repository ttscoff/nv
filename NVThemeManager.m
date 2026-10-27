//
//  NVThemeManager.m
//  Notation
//

#import "NVThemeManager.h"
#import "NSFileManager+DirectoryLocations.h"
#import <fcntl.h>
#import <unistd.h>

NSString * const NVThemesDidChangeNotification = @"NVThemesDidChangeNotification";

static NSString * const NVEditorThemeFilenameKey = @"NVEditorThemeFilename";
static NSString * const NVPreviewThemeFilenameKey = @"NVPreviewThemeFilename";
static const NSUInteger NVThemeMaxDownloadBytes = 2 * 1024 * 1024;

@implementation NVThemeManager

+ (NVThemeManager *)sharedManager {
	static NVThemeManager *manager = nil;
	if (!manager) {
		manager = [[NVThemeManager alloc] init];
		[manager installBundledThemesIfNeeded];
		[manager startWatchingThemeDirectories];
	}
	return manager;
}

- (NSString *)themesDirectory {
	NSString *support = [[NSFileManager defaultManager] applicationSupportDirectory];
	return [support stringByAppendingPathComponent:@"Themes"];
}

- (NSString *)editorThemesDirectory {
	return [self themesDirectory];
}

- (NSString *)previewThemesDirectory {
	return [self themesDirectory];
}

- (void)ensureThemeDirectories {
	NSFileManager *manager = [NSFileManager defaultManager];
	NSString *path = [self themesDirectory];
	if (![manager fileExistsAtPath:path])
		[manager createDirectoryAtPath:path withIntermediateDirectories:YES attributes:nil error:NULL];
}

- (BOOL)isEditorExtension:(NSString *)extension {
	NSString *lower = [extension lowercaseString];
	return [lower isEqualToString:@"theme"] || [lower isEqualToString:@"plist"] || [lower isEqualToString:@"json"];
}

- (BOOL)isPreviewExtension:(NSString *)extension {
	return [[extension lowercaseString] isEqualToString:@"css"];
}

- (NSString *)safeThemeFilename:(NSString *)name {
	name = [name lastPathComponent];
	if (![name length] || [name hasPrefix:@"."] || [name isEqualToString:@".."])
		return nil;
	NSString *extension = [name pathExtension];
	if (![self isEditorExtension:extension] && ![self isPreviewExtension:extension])
		return nil;
	return name;
}

- (void)installBundledThemesIfNeeded {
	[self ensureThemeDirectories];
	NSFileManager *manager = [NSFileManager defaultManager];
	NSString *editorSource = [[NSBundle mainBundle] pathForResource:@"Paper" ofType:@"theme"];
	NSString *editorDest = [[self editorThemesDirectory] stringByAppendingPathComponent:@"Paper.theme"];
	if (editorSource && ![manager fileExistsAtPath:editorDest])
		[manager copyItemAtPath:editorSource toPath:editorDest error:NULL];
	NSString *previewSource = [[NSBundle mainBundle] pathForResource:@"Paper" ofType:@"css"];
	NSString *previewDest = [[self previewThemesDirectory] stringByAppendingPathComponent:@"Paper.css"];
	if (previewSource && ![manager fileExistsAtPath:previewDest])
		[manager copyItemAtPath:previewSource toPath:previewDest error:NULL];
}

- (void)invalidateThemeCache {
	editorResolved = NO;
	previewResolved = NO;
	[editorTheme release];
	editorTheme = nil;
	[previewCSS release];
	previewCSS = nil;
}

- (void)deliverThemesChanged {
	[self invalidateThemeCache];
	[[NSNotificationCenter defaultCenter] postNotificationName:NVThemesDidChangeNotification object:self];
}

- (void)themesDirectoryChanged {
	[NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(deliverThemesChanged) object:nil];
	[self performSelector:@selector(deliverThemesChanged) withObject:nil afterDelay:0.2];
}

- (void)watchDirectory:(NSString *)path source:(dispatch_source_t *)sourceSlot {
	if (*sourceSlot)
		return;
	int fd = open([path fileSystemRepresentation], O_EVTONLY);
	if (fd < 0)
		return;
	dispatch_source_t source = dispatch_source_create(DISPATCH_SOURCE_TYPE_VNODE, (uintptr_t)fd,
		DISPATCH_VNODE_WRITE | DISPATCH_VNODE_DELETE | DISPATCH_VNODE_RENAME | DISPATCH_VNODE_EXTEND | DISPATCH_VNODE_ATTRIB,
		dispatch_get_main_queue());
	if (!source) {
		close(fd);
		return;
	}
	NVThemeManager *manager = self;
	dispatch_source_set_event_handler(source, ^{
		[manager themesDirectoryChanged];
	});
	dispatch_source_set_cancel_handler(source, ^{
		close(fd);
	});
	dispatch_resume(source);
	*sourceSlot = source;
}

- (void)startWatchingThemeDirectories {
	[self ensureThemeDirectories];
	[self watchDirectory:[self themesDirectory] source:&editorWatchSource];
}

- (NSArray *)filenamesInDirectory:(NSString *)directory extensions:(NSArray *)extensions {
	[self ensureThemeDirectories];
	NSArray *contents = [[NSFileManager defaultManager] contentsOfDirectoryAtPath:directory error:NULL];
	NSMutableArray *names = [NSMutableArray array];
	for (NSString *name in contents) {
		if ([name hasPrefix:@"."])
			continue;
		NSString *extension = [[name pathExtension] lowercaseString];
		if ([extensions containsObject:extension])
			[names addObject:name];
	}
	[names sortUsingSelector:@selector(localizedCaseInsensitiveCompare:)];
	return names;
}

- (NSArray *)editorThemeFilenames {
	return [self filenamesInDirectory:[self editorThemesDirectory] extensions:[NSArray arrayWithObjects:@"theme", @"plist", @"json", nil]];
}

- (NSArray *)previewThemeFilenames {
	return [self filenamesInDirectory:[self previewThemesDirectory] extensions:[NSArray arrayWithObjects:@"css", nil]];
}

- (NSString *)editorThemeFilename {
	return [[NSUserDefaults standardUserDefaults] stringForKey:NVEditorThemeFilenameKey] ?: @"";
}

- (NSString *)previewThemeFilename {
	return [[NSUserDefaults standardUserDefaults] stringForKey:NVPreviewThemeFilenameKey] ?: @"";
}

- (void)setEditorThemeFilename:(NSString *)filename {
	if (!filename)
		filename = @"";
	[[NSUserDefaults standardUserDefaults] setObject:filename forKey:NVEditorThemeFilenameKey];
	[self deliverThemesChanged];
}

- (void)setPreviewThemeFilename:(NSString *)filename {
	if (!filename)
		filename = @"";
	[[NSUserDefaults standardUserDefaults] setObject:filename forKey:NVPreviewThemeFilenameKey];
	[self deliverThemesChanged];
}

- (NVEditorTheme *)currentEditorTheme {
	if (editorResolved)
		return editorTheme;
	editorResolved = YES;
	[editorTheme release];
	editorTheme = nil;
	NSString *filename = [self editorThemeFilename];
	NSString *safe = [self safeThemeFilename:filename];
	if (!safe)
		return nil;
	NSString *path = [[self editorThemesDirectory] stringByAppendingPathComponent:safe];
	NSData *data = [NSData dataWithContentsOfFile:path];
	NVEditorTheme *theme = [NVEditorTheme themeWithData:data filename:filename];
	if (!theme)
		return nil;
	editorTheme = [theme retain];
	return editorTheme;
}

- (NSString *)currentPreviewCSS {
	if (previewResolved)
		return previewCSS;
	previewResolved = YES;
	[previewCSS release];
	previewCSS = nil;
	NSString *filename = [self previewThemeFilename];
	if (![filename length])
		return nil;
	NSString *safe = [self safeThemeFilename:filename];
	if (!safe)
		return nil;
	NSString *path = [[self previewThemesDirectory] stringByAppendingPathComponent:safe];
	NSString *css = [NSString stringWithContentsOfFile:path encoding:NSUTF8StringEncoding error:NULL];
	if (![css length])
		return nil;
	previewCSS = [css retain];
	return previewCSS;
}

- (void)openThemesFolder {
	[self ensureThemeDirectories];
	[[NSWorkspace sharedWorkspace] openURL:[NSURL fileURLWithPath:[self themesDirectory]]];
}

- (void)presentThemeMessage:(NSString *)message detail:(NSString *)detail {
	NSAlert *alert = [[NSAlert alloc] init];
	[alert setMessageText:message];
	if ([detail length])
		[alert setInformativeText:detail];
	[alert addButtonWithTitle:NSLocalizedString(@"OK", nil)];
	[alert runModal];
	[alert release];
}

- (BOOL)importThemeData:(NSData *)data filename:(NSString *)filename select:(BOOL)select errorDescription:(NSString **)errorDescription {
	NSString *safe = [self safeThemeFilename:filename];
	if (!safe || ![data length]) {
		if (errorDescription)
			*errorDescription = NSLocalizedString(@"Choose a Composer .theme, .plist, .json, or a preview .css file.", @"theme import type error");
		return NO;
	}
	if ([data length] > NVThemeMaxDownloadBytes) {
		if (errorDescription)
			*errorDescription = NSLocalizedString(@"That theme file is too large.", @"theme import size error");
		return NO;
	}
	[self ensureThemeDirectories];
	NSString *extension = [[safe pathExtension] lowercaseString];
	NSString *destinationDirectory = nil;
	BOOL editorFile = [self isEditorExtension:extension];
	if (editorFile) {
		if (![NVEditorTheme themeWithData:data filename:safe]) {
			if (errorDescription)
				*errorDescription = NSLocalizedString(@"That file is not a Composer editor theme.", @"theme parse error");
			return NO;
		}
		destinationDirectory = [self editorThemesDirectory];
	} else {
		NSString *css = [[[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding] autorelease];
		if (![css length]) {
			if (errorDescription)
				*errorDescription = NSLocalizedString(@"That CSS file could not be read as text.", @"css import error");
			return NO;
		}
		destinationDirectory = [self previewThemesDirectory];
	}
	NSString *destination = [destinationDirectory stringByAppendingPathComponent:safe];
	if (![data writeToFile:destination atomically:YES]) {
		if (errorDescription)
			*errorDescription = NSLocalizedString(@"The theme could not be saved into the themes folder.", @"theme save error");
		return NO;
	}
	if (select) {
		if (editorFile)
			[self setEditorThemeFilename:safe];
		else
			[self setPreviewThemeFilename:safe];
	}
	return YES;
}

- (BOOL)importThemeAtPath:(NSString *)path select:(BOOL)select errorDescription:(NSString **)errorDescription {
	NSString *filename = [self safeThemeFilename:path];
	if (!filename) {
		if (errorDescription)
			*errorDescription = [NSString stringWithFormat:NSLocalizedString(@"%@ is not a theme file.", @"bad theme filename"), [path lastPathComponent]];
		return NO;
	}
	NSData *data = [NSData dataWithContentsOfFile:path];
	if (!data) {
		if (errorDescription)
			*errorDescription = NSLocalizedString(@"The theme file could not be read.", @"theme read error");
		return NO;
	}
	return [self importThemeData:data filename:filename select:select errorDescription:errorDescription];
}

- (void)importThemeFromSourceURL:(NSURL *)sourceURL {
	if (!sourceURL) {
		[self presentThemeMessage:NSLocalizedString(@"Could not import theme", @"theme import title") detail:NSLocalizedString(@"The theme link did not include a file.", @"missing theme url")];
		return;
	}
	NSString *scheme = [[sourceURL scheme] lowercaseString];
	if (![scheme length] || [scheme isEqualToString:@"file"]) {
		NSString *error = nil;
		if (![self importThemeAtPath:[sourceURL path] select:YES errorDescription:&error])
			[self presentThemeMessage:NSLocalizedString(@"Could not import theme", @"theme import title") detail:error];
		else {
			[NSApp activateIgnoringOtherApps:YES];
			[self presentThemeMessage:NSLocalizedString(@"Theme imported", @"theme import success") detail:[[sourceURL path] lastPathComponent]];
		}
		return;
	}
	if (![scheme isEqualToString:@"http"] && ![scheme isEqualToString:@"https"]) {
		[self presentThemeMessage:NSLocalizedString(@"Could not import theme", @"theme import title") detail:NSLocalizedString(@"Theme links can use a file, http, or https URL.", @"theme scheme error")];
		return;
	}
	if (![self safeThemeFilename:[sourceURL lastPathComponent]]) {
		[self presentThemeMessage:NSLocalizedString(@"Could not import theme", @"theme import title") detail:NSLocalizedString(@"The link does not point at a .theme, .plist, .json, or .css file.", @"theme url extension error")];
		return;
	}
	NSURLSessionDataTask *task = [[NSURLSession sharedSession] dataTaskWithURL:sourceURL completionHandler:^(NSData *data, NSURLResponse *response, NSError *error) {
		dispatch_async(dispatch_get_main_queue(), ^{
			NSString *importError = nil;
			// Prefer the name in the link. A text/plain response renames Svelte.theme to Svelte.theme.txt.
			NSString *filename = [self safeThemeFilename:[sourceURL lastPathComponent]];
			if (![filename length]) {
				NSString *suggested = [response suggestedFilename];
				if ([[suggested pathExtension] caseInsensitiveCompare:@"txt"] == NSOrderedSame) {
					NSString *stripped = [self safeThemeFilename:[suggested stringByDeletingPathExtension]];
					if (stripped)
						suggested = stripped;
				}
				filename = [self safeThemeFilename:suggested];
				if (![filename length])
					filename = [suggested length] ? suggested : [sourceURL lastPathComponent];
			}
			NSInteger status = 0;
			if ([response isKindOfClass:[NSHTTPURLResponse class]])
				status = [(NSHTTPURLResponse *)response statusCode];
			BOOL httpFailed = status >= 400;
			BOOL ok = !error && !httpFailed && [self importThemeData:data filename:filename select:YES errorDescription:&importError];
			if (!ok) {
				if (!importError) {
					if (error)
						importError = [error localizedDescription];
					else if (httpFailed)
						importError = [NSString stringWithFormat:NSLocalizedString(@"The download failed (%ld).", @"theme http error"), (long)status];
					else
						importError = NSLocalizedString(@"The download failed.", @"theme download error");
				}
				[self presentThemeMessage:NSLocalizedString(@"Could not import theme", @"theme import title") detail:importError];
			} else {
				[NSApp activateIgnoringOtherApps:YES];
				[self presentThemeMessage:NSLocalizedString(@"Theme imported", @"theme import success") detail:[filename lastPathComponent]];
			}
		});
	}];
	[task resume];
}

- (NSURL *)sourceURLFromThemeParameter:(NSString *)source {
	if (![source length])
		return nil;
	if ([source hasPrefix:@"/"] || [source hasPrefix:@"~"])
		return [NSURL fileURLWithPath:[source stringByExpandingTildeInPath]];
	NSURL *parsed = [NSURL URLWithString:source];
	if (parsed && [[parsed scheme] length])
		return parsed;
	NSString *encoded = [source stringByAddingPercentEncodingWithAllowedCharacters:[NSCharacterSet URLQueryAllowedCharacterSet]];
	parsed = [NSURL URLWithString:encoded];
	if (parsed && [[parsed scheme] length])
		return parsed;
	return nil;
}

- (void)importThemeFromAppURL:(NSURL *)url {
	[NSApp activateIgnoringOtherApps:YES];
	NSURLComponents *components = [NSURLComponents componentsWithURL:url resolvingAgainstBaseURL:NO];
	NSString *source = nil;
	for (NSURLQueryItem *item in [components queryItems]) {
		if ([item.name isEqualToString:@"url"] || [item.name isEqualToString:@"path"]) {
			source = item.value;
			break;
		}
	}
	if (![source length]) {
		[self presentThemeMessage:NSLocalizedString(@"Could not import theme", @"theme import title") detail:NSLocalizedString(@"Use nvalt://add-theme?url= followed by a file or web address.", @"theme url usage")];
		return;
	}
	NSURL *sourceURL = [self sourceURLFromThemeParameter:source];
	if (!sourceURL) {
		[self presentThemeMessage:NSLocalizedString(@"Could not import theme", @"theme import title") detail:NSLocalizedString(@"The theme link did not include a file.", @"missing theme url")];
		return;
	}
	[self importThemeFromSourceURL:sourceURL];
}

- (void)beginImportPanelForWindow:(NSWindow *)window {
	NSOpenPanel *panel = [NSOpenPanel openPanel];
	[panel setCanChooseFiles:YES];
	[panel setCanChooseDirectories:NO];
	[panel setAllowsMultipleSelection:YES];
	[panel setAllowedFileTypes:[NSArray arrayWithObjects:@"theme", @"css", @"plist", @"json", nil]];
	[panel setTitle:NSLocalizedString(@"Import Themes", @"theme import panel title")];
	[panel setPrompt:NSLocalizedString(@"Import", @"theme import button")];
	void (^finish)(NSInteger) = ^(NSInteger result) {
		if (result != NSModalResponseOK)
			return;
		NSString *error = nil;
		NSUInteger imported = 0;
		for (NSURL *fileURL in [panel URLs]) {
			NSString *oneError = nil;
			if ([self importThemeAtPath:[fileURL path] select:YES errorDescription:&oneError])
				imported++;
			else if (!error)
				error = oneError;
		}
		if (error && imported == 0)
			[self presentThemeMessage:NSLocalizedString(@"Could not import theme", @"theme import title") detail:error];
		else if (error)
			[self presentThemeMessage:NSLocalizedString(@"Some themes were not imported", @"partial theme import title") detail:error];
	};
	if (window)
		[panel beginSheetModalForWindow:window completionHandler:finish];
	else
		finish([panel runModal]);
}

@end
