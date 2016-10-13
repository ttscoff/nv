/* AppController */

/*Copyright (c) 2010, Zachary Schneirov. All rights reserved.
  Redistribution and use in source and binary forms, with or without modification, are permitted 
  provided that the following conditions are met:
   - Redistributions of source code must retain the above copyright notice, this list of conditions 
     and the following disclaimer.
   - Redistributions in binary form must reproduce the above copyright notice, this list of 
	 conditions and the following disclaimer in the documentation and/or other materials provided with
     the distribution.
   - Neither the name of Notational Velocity nor the names of its contributors may be used to endorse 
     or promote products derived from this software without specific prior written permission. */


#import <Cocoa/Cocoa.h>

#import "NotationController.h"
#import "NotesTableView.h"
//#import "Spaces.h"

@class LinkingEditor;
@class EmptyView;
@class NotesTableView;
@class GlobalPrefs;
@class PrefsWindowController;
@class DualField;
@class RBSplitView;
@class RBSplitSubview;
@class TitlebarButton;
@class LinearDividerShader;
@class TagEditingManager;
@class DFView;
@class PreviewController;
@class WordCountToken;
//@class AugmentedScrollView;
@class ETContentView;
@class ETScrollView;
@class ETNoteScrollView;

#ifndef MarkdownPreview
#define MarkdownPreview 13371
#endif

#ifndef MultiMarkdownPreview
#define MultiMarkdownPreview 13372
#endif

#ifndef TextilePreview
#define TextilePreview 13373
#endif

@interface AppController : NSObject 
#if MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_6
<NSToolbarDelegate, NSTableViewDelegate, NSWindowDelegate, NSTextFieldDelegate, NSTextViewDelegate>
#endif
{
	IBOutlet NSMenuItem *fsMenuItem;
	BOOL wasVert;
    BOOL wasDFVisible;
    BOOL fieldWasFirstResponder;
    BOOL isAutocompleting;
    BOOL wasDeleting;
    IBOutlet ETContentView *mainView;
	DFView *dualFieldView;
    NSStatusItem *statusItem;
	IBOutlet NSMenu *statBarMenu;
	TagEditingManager *tagEditor;
	NSColor *backgrndColor;
	NSColor *foregrndColor;
	NSInteger userScheme;
	NSString *noteFormat;
	NSTextView *theFieldEditor;
    NSDictionary *fieldAttributes;
	NSTimer *modifierTimer;
	IBOutlet WordCountToken *wordCounter;
    IBOutlet DualField *field;
	RBSplitSubview *splitSubview;
	RBSplitSubview *notesSubview;
	RBSplitView *splitView;
    IBOutlet ETScrollView *notesScrollView;
    IBOutlet ETNoteScrollView *textScrollView;
    IBOutlet NotesTableView *notesTableView;
    IBOutlet LinkingEditor *textView;
	IBOutlet EmptyView *editorStatusView;
	IBOutlet NSMenuItem *sparkleUpdateItem;
    IBOutlet NSWindow *window;
	IBOutlet NSPanel *syncWaitPanel;
	IBOutlet NSProgressIndicator *syncWaitSpinner;
	NSToolbar *toolbar;
	NSToolbarItem *dualFieldItem;
	TitlebarButton *titleBarButton;
	
	BOOL waitedForUncommittedChanges;
	
    //	NSImage *verticalDividerImg;
	LinearDividerShader *dividerShader;
	
	NSString *URLToInterpretOnLaunch;
	NSMutableArray *pathsToOpenOnLaunch;
	
    NSUndoManager *windowUndoManager;
    PrefsWindowController *prefsWindowController;
    GlobalPrefs *prefsController;
    NotationController *notationController;
	
//	SpaceSwitchingContext spaceSwitchCtx;
	ViewLocationContext listUpdateViewCtx;
	BOOL isFilteringFromTyping, typedStringIsCached;
	BOOL isCreatingANote;
	NSString *typedString;
    BOOL isEditing;
	
	NoteObject *currentNote;
	NSArray *savedSelectedNotes;
	BOOL hasLaunched;
    PreviewController *previewController;
    // IBOutlet NSMenuItem *markdownPreview;
    IBOutlet NSMenuItem *multiMarkdownPreview;
    IBOutlet NSMenuItem *textilePreview;
    IBOutlet NSMenuItem *previewToggler;
    IBOutlet NSMenuItem *lockNoteItem;
    IBOutlet NSMenuItem *printPreviewItem;
    IBOutlet NSMenuItem *savePreviewItem;
    NSInteger currentPreviewMode;
    BOOL splitViewIsChangingLayout;
}

@property(readwrite)BOOL isEditing;

void outletObjectAwoke(id sender);

- (void)setNotationController:(NotationController*)newNotation;
- (void)handleGetURLEvent:(NSAppleEventDescriptor *)event withReplyEvent:(NSAppleEventDescriptor *)replyEvent;

- (void)setupViewsAfterAppAwakened;
- (void)runDelayedUIActionsAfterLaunch;
- (void)updateNoteMenus;

- (IBAction)makeActiveAndShowWindow:(id)sender;
- (IBAction)renameNote:(id)sender;
- (IBAction)deleteNote:(id)sender;
- (IBAction)copyNoteLink:(id)sender;
- (IBAction)exportNote:(id)sender;
- (IBAction)revealNote:(id)sender;
- (IBAction)editNoteExternally:(id)sender;
- (IBAction)printNote:(id)sender;
- (IBAction)tagNote:(id)sender;
- (IBAction)importNotes:(id)sender;
- (IBAction)switchViewLayout:(id)sender;

- (IBAction)fieldAction:(id)sender;
- (NoteObject*)createNoteIfNecessary;
- (void)searchForString:(NSString*)string;
- (NSUInteger)revealNote:(NoteObject*)note options:(NSUInteger)opts;
- (BOOL)displayContentsForNoteAtIndex:(NSUInteger)noteIndex;
- (void)processChangedSelectionForTable:(NSTableView*)table;
- (void)setEmptyViewState:(BOOL)state;
- (void)cancelOperation:(id)sender;
- (void)_setCurrentNote:(NoteObject*)aNote;
//- (void)_expandToolbar;
//- (void)_collapseToolbar;
- (void)_forceRegeneratePreviewsForTitleColumn;
- (void)_configureDividerForCurrentLayout;
- (NoteObject*)selectedNoteObject;

- (void)restoreListStateUsingPreferences;

- (void)_finishSyncWait;
- (IBAction)syncWaitQuit:(id)sender;

- (void)setTableAllowsMultipleSelection;

- (NSString*)fieldSearchString;
- (void)cacheTypedStringIfNecessary:(NSString*)aString;
- (NSString*)typedString;

- (IBAction)showHelpDocument:(id)sender;
- (IBAction)showPreferencesWindow:(id)sender;
- (IBAction)toggleNVActivation:(id)sender;
- (IBAction)bringFocusToControlField:(id)sender;
- (NSWindow*)window;

//elasticwork
//- (void)setIsEditing:(BOOL)inBool inCell:(NSCell *)theCell;
//- (void)focusOnCtrlFld:(id)sender;
- (NSMenu *)statBarMenu;
- (NSArray *)commonLabelsForNotesAtIndexes:(NSIndexSet *)selDexes;
- (IBAction)multiTag:(id)sender;
- (void)releaseTagEditor:(NSNotification *)note;
- (void)setDualFieldInToolbar;
- (void)setDualFieldInView;
- (void)setDualFieldIsVisible:(BOOL)isVis;
//- (void)hideDualFieldView;
//- (void)showDualFieldView;
- (BOOL)dualFieldIsVisible;
- (IBAction)toggleCollapse:(id)sender;
- (IBAction)switchFullScreen:(id)sender;
- (BOOL)isInFullScreen;
//- (IBAction)openFileInEditor:(id)sender;
//- (NSArray *)getTxtAppList;
//- (void)updateTextApp:(id)sender;
- (IBAction)setBWColorScheme:(id)sender;
- (IBAction)setLCColorScheme:(id)sender;
- (IBAction)setUserColorScheme:(id)sender;
- (void)updateFieldAttributes;
- (void)updateColorScheme;
- (void)setBackgrndColor:(NSColor *)inColor;
- (void)setForegrndColor:(NSColor *)inColor;
- (NSColor *)backgrndColor;
- (NSColor *)foregrndColor;
- (void)updateWordCount:(BOOL)doIt;
- (void)ensurePreviewIsVisible;
- (void)resetModTimers:(NSNotification *)notification;
- (IBAction)toggleWordCount:(id)sender;
- (void)popWordCount:(BOOL)showIt;
- (void)popPreview:(BOOL)showIt;
- (IBAction)previewNoteWithMarked:(id)sender;
- (IBAction)togglePreview:(id)sender;
- (IBAction)toggleSourceView:(id)sender;
- (IBAction)savePreview:(id)sender;
- (IBAction)sharePreview:(id)sender;
- (IBAction)lockPreview:(id)sender;
- (IBAction)printPreview:(id)sender;
- (void)postTextUpdate;
- (IBAction)selectPreviewMode:(id)sender;
- (BOOL)setNoteIfNecessary;
- (void)updateRTL;
- (void)refreshNotesList;
- (void)focusControlField:(id)sender activate:(BOOL)shouldActivate;
- (void)updateModifier:(NSTimer*)theTimer;
#pragma mark toggling dock icon
- (void)togDockIcon:(NSNotification *)notification;
- (void)hideDockIconAfterDelay;
- (void)hideDockIcon;
- (void)showDockIcon;
- (void)reActivate:(id)sender;
- (void)toggleStatusItem:(NSNotification *)notification;
- (void)setUpStatusBarItem;
- (NSArray *)referenceLinksInString:(NSString *)contentString;
//- (IBAction)testThing:(id)sender;
#if MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_7
- (void)postToggleToolbar:(NSNumber *)boolNum;
#endif

@end
