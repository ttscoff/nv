//
//  NVThemeManager.h
//  Notation
//
//  Editor themes are MultiMarkdown Composer .theme files (JSON or property list).
//  Preview themes are CSS files. Both live in Application Support/Themes.
//
//  URL scheme (nvalt:// or nv://):
//    nvalt://add-theme?url=<percent-encoded file, http, or https URL>
//    nvalt://add-theme?path=<filesystem path>

#import <Cocoa/Cocoa.h>
#import "NVTheme.h"

extern NSString * const NVThemesDidChangeNotification;

@interface NVThemeManager : NSObject {
	NVEditorTheme *editorTheme;
	NSString *previewCSS;
	BOOL editorResolved;
	BOOL previewResolved;
	dispatch_source_t editorWatchSource;
}

+ (NVThemeManager *)sharedManager;
- (NSString *)themesDirectory;
- (void)openThemesFolder;
- (NSArray *)editorThemeFilenames;
- (NSArray *)previewThemeFilenames;
- (NSString *)editorThemeFilename;
- (NSString *)previewThemeFilename;
- (void)setEditorThemeFilename:(NSString *)filename;
- (void)setPreviewThemeFilename:(NSString *)filename;
- (NVEditorTheme *)currentEditorTheme;
- (NSString *)currentPreviewCSS;
- (void)beginImportPanelForWindow:(NSWindow *)window;
- (void)importThemeFromAppURL:(NSURL *)url;

@end
