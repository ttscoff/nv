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

#import "AppController.h"
#import "PrefsWindowController.h"
#import "PTKeyComboPanel.h"
#import "PTKeyCombo.h"
#import "NotationPrefsViewController.h"
#import "ExternalEditorListController.h"
#import "NSData_transformations.h"
#import "NSString_NV.h"
#import "NSFileManager_NV.h"
#import "NSBezierPath_NV.h"
#import "NotationPrefs.h"
#import "GlobalPrefs.h"
#import "BufferUtils.h"

@interface PrefsWindowController () <NSTableViewDataSource, NSTableViewDelegate>
{
	NSTableView *sidebarTable;
	NSView *detailView;
	NSArray *settingsPanes;
}
@end

@implementation PrefsWindowController

- (id)init {
    if (self=[super init]) {
		prefsController = [GlobalPrefs defaultPrefs];
		fontPanelWasOpen = NO;
      // remove opacity slider from color pickers -bt
    [[NSColorPanel sharedColorPanel] setShowsAlpha:NO];
		[prefsController registerWithTarget:self forChangesInSettings:
		 @selector(resolveNoteBodyFontFromNotationPrefsFromSender:), 
//		 @selector(setCheckSpellingAsYouType:sender:), 
		 @selector(setConfirmNoteDeletion:sender:), nil];
    }
    return self;
}

- (void)showWindow:(id)sender {
	if (!window) {
		if (![NSBundle loadNibNamed:@"Preferences" owner:self])  {
			NSLog(@"Failed to load Preferences.nib");
			return;
		}
	}
	[checkSpellingButton setState:[prefsController checkSpellingAsYouType]];
	if (![window isVisible])
		[window center];
	
	[window makeKeyAndOrderFront:self];
    if (!NSApp.isActive) {
        [NSApp activateIgnoringOtherApps:YES];
    }
}

- (void)windowWillClose:(NSNotification *)aNotification {
	[prefsController performSelector:@selector(synchronize) withObject:nil afterDelay:0.0];
	
	[[NSFontPanel sharedFontPanel] close];
}
- (void)windowDidResignMain:(NSNotification *)aNotification {
	//hide the font panel--don't want to confuse people into thinking it will affect some other part of the program
	fontPanelWasOpen = [[NSFontPanel sharedFontPanel] isVisible];
	[[NSFontPanel sharedFontPanel] orderOut:nil];
}
- (void)windowDidBecomeMain:(NSNotification *)aNotification {
	if (fontPanelWasOpen) {
		[self changeBodyFont:self];
	}
}

- (void)menuNeedsUpdate:(NSMenu *)menu {
	NSLog(@"I need an update: %@", [menu description]);
}

- (IBAction)setAppShortcut:(id)sender {
	[[PTKeyComboPanel sharedPanel] showSheetForHotkey:[prefsController appActivationHotKey] forWindow:window modalDelegate:self];
}

- (void)keyComboPanelEnded:(PTKeyComboPanel*)panel {
	PTKeyCombo *oldKeyCombo = [[prefsController appActivationKeyCombo] retain];
	[prefsController setAppActivationKeyCombo:[panel keyCombo] sender:self];
	
	[appShortcutField setStringValue:[[prefsController appActivationKeyCombo] description]];
		
	if (![prefsController registerAppActivationKeystrokeWithTarget:[NSApp delegate] selector:@selector(toggleNVActivation:)]) {
		[prefsController setAppActivationKeyCombo:oldKeyCombo sender:self];
		NSLog(@"reverting to old (hopefully working key combo");
	}
	
	[oldKeyCombo release];
}

- (IBAction)changeBodyFont:(id)sender {
	[[NSFontManager sharedFontManager] setSelectedFont:[prefsController noteBodyFont] isMultiple:NO];
    [[NSFontManager sharedFontManager] orderFrontFontPanel:self];
}

- (void)changeFont:(id)sender {
	NSFontManager *fontMan = [NSFontManager sharedFontManager];
	NSFont *panelFont = [fontMan convertFont:[fontMan selectedFont]];
	
	if (/*![fontMan fontNamed:[panelFont fontName] hasTraits:NSUnboldFontMask | NSUnitalicFontMask]*/
	([fontMan traitsOfFont:panelFont] & NSItalicFontMask) == NSItalicFontMask ||
	([fontMan traitsOfFont:panelFont] & NSBoldFontMask) == NSBoldFontMask) {
		//revert the font--using a bold or italic variant as the default could cause some notes to lose styles
	//	NSLog(@"traits: %u", [fontMan traitsOfFont:panelFont]); 
		
		[self performSelector:@selector(changeBodyFont:) withObject:sender afterDelay:0.0];
		NSBeep();
	} else {
		[prefsController setNoteBodyFont:panelFont sender:self];
	
		[self previewNoteBodyFont];
	}
}

- (NSUInteger)validModesForFontPanel:(NSFontPanel *)fontPanel {
	
	return NSFontPanelSizeModeMask | NSFontPanelCollectionModeMask;
}

- (void)previewNoteBodyFont {

	if (!centerStyle) {
		centerStyle = [[NSMutableParagraphStyle alloc] init];
		[centerStyle setAlignment:NSCenterTextAlignment];
	}

	NSFont *font = [prefsController noteBodyFont];
    CGFloat lh=[font pointSize];
    if (lh<27.0) {
        lh=floorf(27.0-((27.0-lh)/2));
    }
    [centerStyle setMaximumLineHeight:lh];
	NSDictionary *attributes = [NSDictionary dictionaryWithObjectsAndKeys:font ? font : [NSFont systemFontOfSize:12.0],
		NSFontAttributeName, [NSColor blackColor], NSForegroundColorAttributeName, centerStyle, NSParagraphStyleAttributeName, nil];

	NSString *fontNameAndSize = font ? [NSString stringWithFormat:@"%@ %g", [font fontName], [font pointSize]] : @"Unknown";
	NSAttributedString *attributedString = [[NSAttributedString alloc] initWithString:fontNameAndSize attributes:attributes];
	
	[[bodyTextFontField cell] setAttributedStringValue:attributedString];
    [bodyTextFontField updateCell:[bodyTextFontField cell]];
	
	[attributedString autorelease];
	
}

- (IBAction)changedUseETScrollbarsOnLion:(id)sender{
    [prefsController setUseETScrollbarsOnLion:[useETScrollbarsOnLionButton state] sender:self];
}

- (IBAction)changedBackgroundTextColorWell:(id)sender {
	[prefsController setBackgroundTextColor:[backgroundColorWell color] sender:self];
}
- (IBAction)changedForegroundTextColorWell:(id)sender {
	[prefsController setForegroundTextColor:[foregroundColorWell color] sender:self];
}
- (IBAction)changedSearchHighlightColorWell:(id)sender {
	[prefsController setSearchTermHighlightColor:[searchHighlightColorWell color] sender:self];
}
- (IBAction)changedHighlightSearchTerms:(id)sender {
	[prefsController setShouldHighlightSearchTerms:[highlightSearchTermsButton state] sender:self];
}
- (IBAction)changedStyledTextBehavior:(id)sender {
    [prefsController setPastePreservesStyle:[styledTextButton state] sender:self];
}
- (IBAction)changedAutoSuggestLinks:(id)sender {
    [prefsController setLinksAutoSuggested:[autoSuggestLinksButton state] sender:self];
}

- (IBAction)changedMakeURLsClickable:(id)sender {
	[prefsController setMakeURLsClickable:[makeURLsClickable state] sender:self];
}

- (IBAction)changedNoteDeletion:(id)sender {
	[prefsController setConfirmNoteDeletion:[confirmDeletionButton state] sender:self];
}

- (IBAction)changedNotesFolderLocation:(id)sender {
    NSLog(@"Changed notes folder menu");
}

- (IBAction)changedQuitBehavior:(id)sender {
    [prefsController setQuitWhenClosingWindow:[quitWhenClosingButton state] sender:self];
}

- (IBAction)changedSpellChecking:(id)sender {
    [prefsController setCheckSpellingAsYouType:[checkSpellingButton state] sender:self];
}


- (IBAction)changedTabBehavior:(id)sender {
    if (sender != self)
	[self performSelector:@selector(changedTabBehavior:) withObject:self afterDelay:0.0];
    else
	[prefsController setTabIndenting:[[tabKeyRadioMatrix cellAtRow:0 column:0] state] sender:self];
}

- (IBAction)changedExternalEditorsMenu:(id)sender {
  //not currently called as an action in practice
  [self _selectDefaultExternalEditor];
}

- (void)_selectDefaultExternalEditor {
  ExternalEditor *ed = [[ExternalEditorListController sharedInstance] defaultExternalEditor];
  NSInteger idx = ed ? [externalEditorMenuButton indexOfItemWithRepresentedObject:ed] : 0;
  if (idx > -1) {
    [externalEditorMenuButton selectItemAtIndex:idx];
  }
}

- (IBAction)changedTableText:(id)sender {
	if (sender == tableTextMenuButton) {
		if ([tableTextSizeField selectedTag] != 3) [tableTextSizeField setFloatValue:[prefsController tableFontSize]];
		[self performSelector:@selector(changedTableText:) withObject:nil afterDelay:0.0];
	} else {
		[window makeFirstResponder:window];
		float newFontSize = 0.0;
		switch ([tableTextMenuButton selectedTag]) {
			case 1:
				newFontSize = [NSFont smallSystemFontSize];
				break;
			case 2:
				newFontSize = [NSFont systemFontSize];
				break;
			case 3:
				newFontSize = [tableTextSizeField floatValue];
		}
		[tableTextSizeField setHidden:([tableTextMenuButton selectedTag] != 3)];
		if (![tableTextSizeField isHidden])
			[tableTextSizeField selectText:sender];
		
		[prefsController setTableFontSize:newFontSize sender:self];
	}	
}

- (IBAction)changedTitleCompletion:(id)sender {
    [prefsController setAutoCompleteSearches:[completeNoteTitlesButton state] sender:self];
}

- (IBAction)changedSoftTabs:(id)sender {
	[prefsController setSoftTabs:[softTabsButton state] sender:self];
}

- (void)settingChangedForSelectorString:(NSString*)selectorString {
    if ([selectorString isEqualToString:SEL_STR(resolveNoteBodyFontFromNotationPrefsFromSender:)]) {
		[self previewNoteBodyFont];
//	} else if ([selectorString isEqualToString:SEL_STR(setCheckSpellingAsYouType:sender:)]) {
//		[checkSpellingButton setState:[prefsController checkSpellingAsYouType]];
	} else if ([selectorString isEqualToString:SEL_STR(setConfirmNoteDeletion:sender:)]) {
		[confirmDeletionButton setState:[prefsController confirmNoteDeletion]];
	}
}

- (NSMenu*)directorySelectionMenu {
    NSMenu *theMenu = [[[NSMenu alloc] initWithTitle:@"Note Directory Menu"] autorelease];
    
    FSRef targetRef = {{0}};
    NSString *name = [prefsController displayNameForDefaultDirectoryWithFSRef:&targetRef];
    if (!name)
		name = NSLocalizedString(@"<Directory unknown>", nil);
	
	NSImage *iconImage = nil;
	if (!IsZeros(&targetRef, sizeof(FSRef)) || [[prefsController aliasDataForDefaultDirectory] fsRefAsAlias:&targetRef])
	    iconImage = [NSImage smallIconForFSRef:&targetRef];
	
    NSMenuItem *theMenuItem = [[[NSMenuItem alloc] initWithTitle:name action:nil keyEquivalent:@""] autorelease];
    
    if (iconImage)
		[theMenuItem setImage:iconImage];
    
    [theMenu addItem:theMenuItem];
    
    [theMenu addItem:[NSMenuItem separatorItem]];
    
    theMenuItem = [[[NSMenuItem alloc] initWithTitle:NSLocalizedString(@"Other...", @"title of menu item for selecting a different notes folder")
											  action:@selector(changeDefaultDirectory) keyEquivalent:@""] autorelease];
    [theMenuItem setTarget:self];
    [theMenu addItem:theMenuItem];
    
    return theMenu;
}

- (void)changeDefaultDirectory {
	FSRef notesDirectoryRef;
	NSData *aliasData = nil;
	NSString *directoryPath = nil;

	if ([self getNewNotesRefFromOpenPanel:&notesDirectoryRef returnedPath:&directoryPath]) {
		
		//make sure we're not choosing the same folder as what we started with, because:
		//-[NotationController initWithAliasData:] might attempt to initialize journaling, which will already be in use
		FSRef currentNotesDirectoryRef;
		[[prefsController aliasDataForDefaultDirectory] fsRefAsAlias:&currentNotesDirectoryRef];
		if (FSCompareFSRefs(&notesDirectoryRef, &currentNotesDirectoryRef) != noErr) {
			
			if ((aliasData = [NSData aliasDataForFSRef:&notesDirectoryRef])) {
				[prefsController setAliasDataForDefaultDirectory:aliasData sender:self];
			}
		} else {
			NSLog(@"This folder is already chosen!");
		}
		
	}

	[folderLocationsMenuButton setMenu:[self directorySelectionMenu]];

	if ([folderLocationsMenuButton numberOfItems] > 0)
		[folderLocationsMenuButton selectItemAtIndex:0];
}

- (IBAction)changedRTL:(id)sender {
	[prefsController setRTL:[rtlButton state] sender:self];
	[[NSApp delegate] updateRTL];
}

- (BOOL)getNewNotesRefFromOpenPanel:(FSRef*)notesDirectoryRef returnedPath:(NSString**)path {
    NSString *startingDirectory = nil;
	
    if (!notesDirectoryRef) {
		NSLog(@"notesDirectoryRef is NULL!");
		return NO;
    }
    
    FSRef currentNotesDirectoryRef;
    //resolve alias to fsref; get path from fsref
    if ([[prefsController aliasDataForDefaultDirectory] fsRefAsAlias:&currentNotesDirectoryRef]) {
		NSString *resolvedPath = [[NSFileManager defaultManager] pathWithFSRef:&currentNotesDirectoryRef];
		if (resolvedPath) startingDirectory = resolvedPath;
    }
    
    NSOpenPanel *openPanel = [NSOpenPanel openPanel];
    [openPanel setCanCreateDirectories:YES];
    [openPanel setCanChooseFiles:NO];
    [openPanel setCanChooseDirectories:YES];
    [openPanel setResolvesAliases:YES];
    [openPanel setAllowsMultipleSelection:NO];
    [openPanel setTreatsFilePackagesAsDirectories:NO];
    [openPanel setTitle:NSLocalizedString(@"Select a folder",@"title of open panel for selecting a notes folder")];
    [openPanel setPrompt:NSLocalizedString(@"Select", @"title of open panel button to select a folder")];
    [openPanel setMessage:NSLocalizedString(@"Select the folder that Notational Velocity should use for reading and storing notes.",nil)];
    [openPanel setDirectoryURL:[NSURL fileURLWithPath:startingDirectory]];
    [openPanel setAllowedFileTypes:nil];
    if ([openPanel runModal]==NSFileHandlingPanelOKButton) {
        
		CFStringRef filename = (CFStringRef)[[openPanel URL]path];
		if (!filename)
			return NO;
		
		if (path)
			*path = [[[[openPanel URL]path] copy] autorelease];
		
		//yes, I know that navigation services uses uses FSRefs, but NSSavePanel saves us much more work
		CFURLRef url = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, filename, kCFURLPOSIXPathStyle, true);
		[(id)url autorelease];
		if (!url || !CFURLGetFSRef(url, notesDirectoryRef))
			return NO;
		
		return YES;
    }
    
    return NO;
}

- (NotationPrefsViewController*)notationPrefsViewController {
	if (!notationPrefsViewController) {
		notationPrefsViewController = [[NotationPrefsViewController alloc] init];
	}
	return notationPrefsViewController;
}

- (NSView*)databaseView {
    if (![notationPrefsView subviews] || ![[notationPrefsView subviews] count])
		[notationPrefsView addSubview:[[self notationPrefsViewController] view]];
	
    return databaseView;
}

- (void)addToolbarItemWithName:(NSString*)name {
    NSToolbarItem *item = [[NSToolbarItem alloc] initWithItemIdentifier:name];
	
	NSString *localizedTitle = [[NSBundle mainBundle] localizedStringForKey:name value:@"" table:nil];
    [item setPaletteLabel:localizedTitle];
    [item setLabel:localizedTitle];
    //[item setToolTip:@"General settings: appearance and behavior"];
    [item setImage:[[[NSImage alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:name ofType:@"tiff"]] autorelease]];
    [item setTarget:self];
    [item setAction:@selector(switchViews:)];
    [items setObject:item forKey:name];
    [item release];
}

- (void)awakeFromNib {
	
	[window setDelegate:self];
	
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(changedTableText:)
												 name:NSControlTextDidEndEditingNotification object:tableTextSizeField];
    
    [tabKeyRadioMatrix setState:[prefsController tabKeyIndents] atRow:0 column:0];
    [tabKeyRadioMatrix setState:![prefsController tabKeyIndents] atRow:1 column:0];
    
    float fontSize = [prefsController tableFontSize];
    int fontButtonIndex = 3;
    if (fontSize == [NSFont smallSystemFontSize]) fontButtonIndex = 0;
    else if (fabsf(fontSize - (float)[NSFont systemFontSize]) < 0.01f) fontButtonIndex = 1;
    [tableTextMenuButton selectItemAtIndex:fontButtonIndex];
    [tableTextSizeField setFloatValue:fontSize];
    [tableTextSizeField setHidden:(fontButtonIndex != 3)];
    
    [externalEditorMenuButton setMenu:[[ExternalEditorListController sharedInstance] addEditorPrefsMenu]];
    [self _selectDefaultExternalEditor];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(changedExternalEditorsMenu:) 
                           name:ExternalEditorsChangedNotification object:nil];
    
    [completeNoteTitlesButton setState:[prefsController autoCompleteSearches]];
//    [checkSpellingButton setState:[prefsController checkSpellingAsYouType]];
    [confirmDeletionButton setState:[prefsController confirmNoteDeletion]];
    [quitWhenClosingButton setState:[prefsController quitWhenClosingWindow]];
    [styledTextButton setState:[prefsController pastePreservesStyle]];
    [autoSuggestLinksButton setState:[prefsController linksAutoSuggested]];
	[softTabsButton setState:[prefsController softTabs]];
	[makeURLsClickable setState:[prefsController URLsAreClickable]];
    [rtlButton setState:[prefsController rtl]];
    [self previewNoteBodyFont];
	[appShortcutField setStringValue:[[prefsController appActivationKeyCombo] description]];
	[searchHighlightColorWell setColor:[prefsController searchTermHighlightColorRaw:YES]];
	[highlightSearchTermsButton setState:[prefsController highlightSearchTerms]];
	[foregroundColorWell setColor:[prefsController foregroundTextColor]];
	[backgroundColorWell setColor:[prefsController backgroundTextColor]];
    [maxWidthSlider setDoubleValue:[[NSUserDefaults standardUserDefaults] doubleForKey:@"NoteBodyMaxWidth"]];
	//for elasticthreads' hide dock icon option, check if OS compatible
	if (IsSnowLeopardOrLater) {
		[togDockButton setEnabled:YES];
		
		if ([[NSUserDefaults standardUserDefaults] boolForKey:@"ShowDockIcon"]) {
            [togDockButton setTitle:@"Hide Dock Icon"];
//			[togDockLabel setStringValue:@"This will immediately restart NV"];		
		}else {
            [togDockButton setTitle:@"Show Dock Icon"];
//			[togDockLabel setStringValue:@""];
		}

	}else {	
		[togDockButton setEnabled:NO];
		[togDockButton setHidden:YES];
//		[togDockLabel setHidden:YES];
	}
    //for Brett's Markdownify/Readability import
	[useMarkdownImportButton setState:[prefsController useMarkdownImport]];
	[useReadabilityButton setState:[prefsController useReadability]];
	[useReadabilityButton setEnabled:[useMarkdownImportButton state]];
	
    [altRowsButton setState:[prefsController alternatingRows]];
    [showGridButton setState:[prefsController showGrid]];
    [autoPairButton setState:[prefsController useAutoPairing]];
    items = [[NSMutableDictionary alloc] init];
    [useETScrollbarsOnLionButton setState:[prefsController useETScrollbarsOnLion]];
    [useETScrollbarsOnLionButton setHidden:!IsLionOrLater];
    [self installSettingsSplitView];
}


- (NSToolbarItem *)toolbar:(NSToolbar *)toolbar itemForItemIdentifier:(NSString *)itemIdentifier willBeInsertedIntoToolbar:(BOOL)flag {
    return [items objectForKey:itemIdentifier];
}

- (NSArray *)toolbarAllowedItemIdentifiers:(NSToolbar*)theToolbar {
    return [self toolbarDefaultItemIdentifiers:theToolbar];
}

- (NSArray *)toolbarDefaultItemIdentifiers:(NSToolbar*)theToolbar {
    return [NSArray arrayWithObjects:@"General", @"Notes", @"Editing", @"Fonts & Colors", nil];
}

- (NSArray *)toolbarSelectableItemIdentifiers: (NSToolbar *)toolbar {
    //make all of them selectable. This puts that little grey outline thing around an item when you select it.
    return [items allKeys];
}

- (NSArray *)settingsPaneNames {
	return [NSArray arrayWithObjects:@"General", @"Notes", @"Editing", @"Fonts & Colors", nil];
}

- (NSArray *)settingsPaneSymbols {
	return [NSArray arrayWithObjects:@"gearshape", @"note.text", @"square.and.pencil", @"textformat", nil];
}

- (NSString *)localizedSettingsTitleForPane:(NSString *)name {
	return [[NSBundle mainBundle] localizedStringForKey:name value:name table:nil];
}

- (NSView *)settingsViewForPane:(NSString *)name {
	if ([name isEqualToString:@"General"])
		return generalView;
	if ([name isEqualToString:@"Notes"])
		return [self databaseView];
	if ([name isEqualToString:@"Editing"])
		return editingView;
	if ([name isEqualToString:@"Fonts & Colors"])
		return fontsColorsView;
	return nil;
}

- (void)installSettingsSplitView {
	CGFloat sidebarWidth = 196;
	CGFloat paneWidth = 0;
	CGFloat paneHeight = 0;
	NSView *notesPane = [self databaseView];
	NSArray *panes = [NSArray arrayWithObjects:generalView, notesPane, editingView, fontsColorsView, nil];
	for (NSView *pane in panes) {
		paneWidth = MAX(paneWidth, NSWidth([pane frame]));
		paneHeight = MAX(paneHeight, NSHeight([pane frame]));
	}
	settingsPanes = [panes retain];
	CGFloat inset = 16;
	NSSize contentSize = NSMakeSize(sidebarWidth + paneWidth + inset * 2, paneHeight + inset * 2);

	detailView = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, paneWidth + inset * 2, contentSize.height)];

	NSVisualEffectView *sidebarBackground = [[NSVisualEffectView alloc] initWithFrame:NSMakeRect(0, 0, sidebarWidth, contentSize.height)];
	[sidebarBackground setMaterial:NSVisualEffectMaterialSidebar];
	[sidebarBackground setBlendingMode:NSVisualEffectBlendingModeBehindWindow];
	[sidebarBackground setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];

	NSScrollView *sidebarScroll = [[NSScrollView alloc] initWithFrame:[sidebarBackground bounds]];
	[sidebarScroll setDrawsBackground:NO];
	[sidebarScroll setBorderType:NSNoBorder];
	[sidebarScroll setHasVerticalScroller:NO];
	[sidebarScroll setAutohidesScrollers:YES];
	[sidebarScroll setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];

	sidebarTable = [[NSTableView alloc] initWithFrame:[sidebarScroll bounds]];
	NSTableColumn *column = [[NSTableColumn alloc] initWithIdentifier:@"pane"];
	[column setWidth:sidebarWidth - 8];
	[column setResizingMask:NSTableColumnAutoresizingMask];
	[sidebarTable addTableColumn:column];
	[column release];
	[sidebarTable setHeaderView:nil];
	[sidebarTable setRowHeight:32];
	[sidebarTable setIntercellSpacing:NSMakeSize(0, 4)];
	[sidebarTable setStyle:NSTableViewStyleSourceList];
	[sidebarTable setSelectionHighlightStyle:NSTableViewSelectionHighlightStyleSourceList];
	[sidebarTable setBackgroundColor:[NSColor clearColor]];
	[sidebarTable setAllowsEmptySelection:NO];
	[sidebarTable setFocusRingType:NSFocusRingTypeNone];
	[sidebarTable setDataSource:self];
	[sidebarTable setDelegate:self];
	[sidebarScroll setDocumentView:sidebarTable];
	[sidebarBackground addSubview:sidebarScroll];
	[sidebarScroll release];

	NSViewController *sidebarController = [[NSViewController alloc] init];
	[sidebarController setView:sidebarBackground];
	[sidebarBackground release];
	NSViewController *detailController = [[NSViewController alloc] init];
	[detailController setView:detailView];

	NSSplitViewController *split = [[NSSplitViewController alloc] init];
	NSSplitViewItem *sidebarItem = [NSSplitViewItem sidebarWithViewController:sidebarController];
	[sidebarItem setMinimumThickness:sidebarWidth];
	[sidebarItem setMaximumThickness:sidebarWidth];
	[sidebarItem setCanCollapse:NO];
	NSSplitViewItem *detailItem = [NSSplitViewItem splitViewItemWithViewController:detailController];
	[split addSplitViewItem:sidebarItem];
	[split addSplitViewItem:detailItem];
	[sidebarController release];
	[detailController release];

	[window setTitle:NSLocalizedString(@"Settings", @"settings window title")];
	[window setContentMinSize:contentSize];
	[window setContentMaxSize:contentSize];
	[window setContentViewController:split];
	[window setContentSize:contentSize];
	[split release];

	NSString *selected = [prefsController lastSelectedPreferencesPane];
	NSUInteger index = [[self settingsPaneNames] indexOfObject:selected];
	if (index == NSNotFound)
		index = 0;
	[sidebarTable selectRowIndexes:[NSIndexSet indexSetWithIndex:index] byExtendingSelection:NO];
}

- (void)showSettingsPaneNamed:(NSString *)name {
	NSView *prefsView = [self settingsViewForPane:name];
	if (!prefsView)
		return;
	if (prefsView == databaseView)
		[folderLocationsMenuButton setMenu:[self directorySelectionMenu]];

	[[NSFontPanel sharedFontPanel] close];

	NSArray *existing = [[detailView subviews] copy];
	for (NSView *subview in existing)
		[subview removeFromSuperview];
	[existing release];

	CGFloat inset = 16;
	NSRect paneFrame = [prefsView frame];
	CGFloat hostHeight = NSHeight([detailView bounds]);
	paneFrame.origin.x = inset;
	paneFrame.origin.y = hostHeight - NSHeight(paneFrame) - inset;
	[prefsView setFrame:paneFrame];
	[prefsView setAutoresizingMask:NSViewMinYMargin | NSViewMaxXMargin];
	[detailView addSubview:prefsView];
}

- (NSInteger)numberOfRowsInTableView:(NSTableView *)tableView {
	return (NSInteger)[[self settingsPaneNames] count];
}

- (NSView *)tableView:(NSTableView *)tableView viewForTableColumn:(NSTableColumn *)tableColumn row:(NSInteger)row {
	NSTableCellView *cell = [tableView makeViewWithIdentifier:@"SettingsPane" owner:self];
	if (!cell) {
		cell = [[[NSTableCellView alloc] initWithFrame:NSMakeRect(0, 0, 180, 32)] autorelease];
		[cell setIdentifier:@"SettingsPane"];
		NSImageView *imageView = [[NSImageView alloc] initWithFrame:NSMakeRect(10, 6, 20, 20)];
		[imageView setImageScaling:NSImageScaleProportionallyUpOrDown];
		NSTextField *label = [[NSTextField alloc] initWithFrame:NSMakeRect(36, 6, 140, 18)];
		[label setBezeled:NO];
		[label setDrawsBackground:NO];
		[label setEditable:NO];
		[label setSelectable:NO];
		[label setFont:[NSFont systemFontOfSize:13]];
		[label setTextColor:[NSColor labelColor]];
		[label setAutoresizingMask:NSViewWidthSizable];
		[cell setImageView:imageView];
		[cell setTextField:label];
		[cell addSubview:imageView];
		[cell addSubview:label];
		[imageView release];
		[label release];
	}
	NSString *name = [[self settingsPaneNames] objectAtIndex:(NSUInteger)row];
	NSString *title = [self localizedSettingsTitleForPane:name];
	[[cell textField] setStringValue:title];
	NSImage *symbol = [NSImage imageWithSystemSymbolName:[[self settingsPaneSymbols] objectAtIndex:(NSUInteger)row] accessibilityDescription:title];
	[symbol setTemplate:YES];
	[[cell imageView] setImage:symbol];
	return cell;
}

- (void)tableViewSelectionDidChange:(NSNotification *)notification {
	NSInteger row = [sidebarTable selectedRow];
	if (row < 0)
		return;
	NSString *name = [[self settingsPaneNames] objectAtIndex:(NSUInteger)row];
	[prefsController setLastSelectedPreferencesPane:name sender:self];
	[self showSettingsPaneNamed:name];
}

- (void)switchViews:(NSToolbarItem *)item {
	NSString *sender = item ? [item itemIdentifier] : [prefsController lastSelectedPreferencesPane];
	NSUInteger index = [[self settingsPaneNames] indexOfObject:sender];
	if (index == NSNotFound)
		index = 0;
	[sidebarTable selectRowIndexes:[NSIndexSet indexSetWithIndex:index] byExtendingSelection:NO];
}

//elasticwork

- (IBAction)toggleHideDockIcon:(id)sender{
    NSUserDefaults *stdDefaults=[NSUserDefaults standardUserDefaults];
    BOOL showIt=![stdDefaults boolForKey:@"ShowDockIcon"];
    if (showIt) {
        [stdDefaults setBool:YES forKey:@"ShowDockIcon"];
        [togDockButton setTitle:@"Hide Dock Icon"];        
//        [togDockLabel setStringValue:@"This will immediately restart NV"];	
    }else{
        [stdDefaults setBool:YES forKey:@"StatusBarItem"];
        [stdDefaults setBool:NO forKey:@"ShowDockIcon"];
        [togDockButton setTitle:@"Show Dock Icon"];
    }
    [stdDefaults synchronize];
	[[NSNotificationCenter defaultCenter]postNotificationName:@"AppShouldToggleDockIcon" object:[NSNumber numberWithBool:showIt]];
}

- (IBAction)toggleStatusItem:(id)sender{
    BOOL show = ([sender state] == NSOnState);
    [[NSUserDefaults standardUserDefaults] setBool:show forKey:@"StatusBarItem"];
	[[NSNotificationCenter defaultCenter] postNotificationName:@"AppShouldToggleStatusItem" object:[NSNumber numberWithBool:show]];
}


- (IBAction)toggleKeepsTextWidthInWindow:(id)sender{
   
    [prefsController setManagesTextWidthInWindow:[sender state] sender:self];
//		[[NSApp delegate] setMaxNoteBodyWidth];
}

- (IBAction)setMaxWidth:(id)sender{
	CGFloat dbWidth = [maxWidthSlider floatValue];	
	dbWidth = dbWidth - fmod(dbWidth,2.0);
	[prefsController setMaxNoteBodyWidth:dbWidth sender:self];
//	[[NSApp delegate] setMaxNoteBodyWidth];
}

- (IBAction)changedUseMarkdownImport:(id)sender {
	[prefsController setUseMarkdownImport:[useMarkdownImportButton state] sender:self];
	[useReadabilityButton setEnabled:[useMarkdownImportButton state]];
}

- (IBAction)changedUseReadability:(id)sender {
	[prefsController setUseReadability:[useReadabilityButton state] sender:self];
}

- (IBAction)changedAltRows:(id)sender {
	[prefsController setAlternatingRows:[altRowsButton state] sender:self];
    [[NSApp delegate] refreshNotesList];
}

- (IBAction)changedAutoPairing:(id)sender{
	[prefsController setUseAutoPairing:[autoPairButton state]];
    //  [[NSApp delegate] refreshNotesList];
}

- (IBAction)changedShowGrid:(id)sender {
	[prefsController setShowGrid:[showGridButton state] sender:self];
    [[NSApp delegate] refreshNotesList];
}

@end
