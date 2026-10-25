//
//  NotationPrefsViewController.m
//  Notation
//
//  Created by Zachary Schneirov on 4/1/06.

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


#import "GlobalPrefs.h"
#import "NotationPrefsViewController.h"
#import "InvocationRecorder.h"
#import "NotationPrefs.h"
#import "NSString_NV.h"
#import "NSCollection_utils.h"
#import "SyncResponseFetcher.h"
#import "SimplenoteSession.h"
#import "NSDictionary+BSJSONAdditions.h"
#import "PassphrasePicker.h"
#import "PassphraseChanger.h"
#import "NSFileManager_NV.h"
#import "AppController.h"

@implementation FileKindListView 

- (BOOL)acceptsFirstResponder {
    
    if (storageFormatPopupButton)
		return ([storageFormatPopupButton selectedTag] != SingleDatabaseFormat);
	
    return YES;
}
@end

enum {VERIFY_NOT_ATTEMPTED, VERIFY_FAILED, VERIFY_IN_PROGRESS, VERIFY_SUCCESS};

@implementation NotationPrefsViewController

- (NSView*)view {
    if (!view) {
		if (![NSBundle loadNibNamed:@"NotationPrefsView" owner:self])  {
			NSLog(@"Failed to load NotationPrefsView.nib");
			return nil;
		}
    }
    
    return view;
}

- (id)init {
    if (self=[super init]) {
        
		didAwakeFromNib = NO;
		notationPrefs = [[[GlobalPrefs defaultPrefs] notationPrefs] retain];
		
		disableEncryptionString = NSLocalizedString(@"Turn Off Note Encryption...",nil);
		enableEncryptionString = NSLocalizedString(@"Turn On Note Encryption...",nil);
	
		[[GlobalPrefs defaultPrefs] registerForSettingChange:@selector(setNotationPrefs:sender:) withTarget:self];
    
        return self;
    }
	return nil;
}
- (void)dealloc {
	[passphrasePicker release];
	[changer release];
	[notationPrefs release];
	[postStorageFormatInvocation release];
	
	[[NSNotificationCenter defaultCenter] removeObserver:self];
	
	[super dealloc];
}

- (void)awakeFromNib {
    didAwakeFromNib = YES;
    [allowedExtensionsTable setDataSource:self];
    [allowedTypesTable setDataSource:self];
    [allowedExtensionsTable setDelegate:self];
    [allowedTypesTable setDelegate:self];
	
	
	//this additional management for sync prefs, plus the need for per-service settings and externally triggering updates really demands its own class
	NSNotificationCenter *center = [NSNotificationCenter defaultCenter];
	if (syncAccountField) [center addObserver:self selector:@selector(syncCredentialsDidChange:) name:NSControlTextDidChangeNotification object:syncAccountField];
	if (syncPasswordField) {
		[center addObserver:self selector:@selector(syncCredentialsDidChange:) name:NSControlTextDidChangeNotification object:syncPasswordField];
		[center addObserver:self selector:@selector(syncEditingDidEnd:) name:NSControlTextDidEndEditingNotification object:syncPasswordField];
	}
	[center addObserver:self selector:@selector(initializeControls) name:NotationPrefsDidChangeNotification object:nil];

    [self configureSimplenoteLoginControls];
    [self initializeControls];
	[self addMarkdownConversionButton];
}

- (void)configureSimplenoteLoginControls {
	if (!syncAccountField || !syncPasswordField)
		return;
	[self usePlainSimplenoteCodeField];
	NSView *syncView = [syncAccountField superview];
	for (NSView *subview in [syncView subviews]) {
		if (![subview isKindOfClass:[NSTextField class]] || [subview isKindOfClass:[NSSecureTextField class]])
			continue;
		NSTextField *label = (NSTextField *)subview;
		if ([[label cell] isEditable] || [[label cell] isSelectable])
			continue;
		if (fabs(NSMaxY([label frame]) - NSMaxY([syncPasswordField frame])) > 12)
			continue;
		if (NSMaxX([label frame]) > NSMinX([syncPasswordField frame]) + 4)
			continue;
		[label setStringValue:NSLocalizedString(@"Code:", @"Simplenote login code label")];
		break;
	}
	if ([[syncPasswordField cell] respondsToSelector:@selector(setEchosBullets:)])
		[(NSSecureTextFieldCell *)[syncPasswordField cell] setEchosBullets:NO];
	[[syncPasswordField cell] setPlaceholderString:NSLocalizedString(@"Code from email", @"Simplenote login code placeholder")];
	[[syncAccountField cell] setLineBreakMode:NSLineBreakByTruncatingTail];

	if ([syncView viewWithTag:7302])
		return;
	NSButton *button = [[NSButton alloc] initWithFrame:NSZeroRect];
	[button setBezelStyle:NSBezelStyleRounded];
	[button setButtonType:NSButtonTypeMomentaryPushIn];
	[button setTitle:NSLocalizedString(@"Email Code", @"Simplenote login button")];
	[button setTag:7302];
	[button setTarget:self];
	[button setAction:@selector(emailSimplenoteLoginCode:)];
	[button sizeToFit];
	NSRect codeFrame = [syncPasswordField frame];
	NSRect buttonFrame = [button frame];
	CGFloat buttonWidth = buttonFrame.size.width + 14;
	CGFloat gap = 8;
	if (codeFrame.size.width > buttonWidth + gap + 70) {
		codeFrame.size.width -= buttonWidth + gap;
		[syncPasswordField setFrame:codeFrame];
	}
	buttonFrame.size.width = buttonWidth;
	buttonFrame.size.height = MAX(buttonFrame.size.height, codeFrame.size.height);
	buttonFrame.origin.x = NSMaxX([syncPasswordField frame]) + gap;
	buttonFrame.origin.y = NSMidY(codeFrame) - (buttonFrame.size.height / 2.0);
	[button setFrame:NSIntegralRect(buttonFrame)];
	[button setAutoresizingMask:NSViewMinXMargin | NSViewMinYMargin];
	[syncView addSubview:button];
	[button release];
}

- (void)usePlainSimplenoteCodeField {
	if (![syncPasswordField isKindOfClass:[NSSecureTextField class]])
		return;
	NSTextField *codeField = [[NSTextField alloc] initWithFrame:[syncPasswordField frame]];
	[codeField setAutoresizingMask:[syncPasswordField autoresizingMask]];
	[codeField setEditable:YES];
	[codeField setSelectable:YES];
	[codeField setBezeled:YES];
	[codeField setDrawsBackground:YES];
	[codeField setFont:[syncPasswordField font]];
	[[codeField cell] setScrollable:YES];
	[[codeField cell] setSendsActionOnEndEditing:YES];
	NSNotificationCenter *center = [NSNotificationCenter defaultCenter];
	[center removeObserver:self name:NSControlTextDidChangeNotification object:syncPasswordField];
	[center removeObserver:self name:NSControlTextDidEndEditingNotification object:syncPasswordField];
	[[syncPasswordField superview] replaceSubview:syncPasswordField with:codeField];
	syncPasswordField = codeField;
	[center addObserver:self selector:@selector(syncCredentialsDidChange:) name:NSControlTextDidChangeNotification object:syncPasswordField];
	[center addObserver:self selector:@selector(syncEditingDidEnd:) name:NSControlTextDidEndEditingNotification object:syncPasswordField];
	[codeField release];
}

- (NSData *)simplenoteJSONData:(NSDictionary *)body {
	if (![NSJSONSerialization isValidJSONObject:body])
		return nil;
	return [NSJSONSerialization dataWithJSONObject:body options:0 error:nil];
}

- (NSString *)simplenoteLoginCodeFromEntry:(NSString *)entry {
	NSString *trimmed = [entry stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
	NSString *code = trimmed;
	if ([trimmed rangeOfString:@"auth_code="].location != NSNotFound)
		code = [self simplenoteQueryValueNamed:@"auth_code" inEntry:trimmed];
	NSCharacterSet *letters = [NSCharacterSet alphanumericCharacterSet];
	NSMutableString *onlyLetters = [NSMutableString string];
	NSUInteger i;
	for (i = 0; i < [code length]; i++) {
		unichar character = [code characterAtIndex:i];
		if ([letters characterIsMember:character])
			[onlyLetters appendFormat:@"%C", character];
	}
	if ([onlyLetters length] < 4 || [onlyLetters length] > 8)
		return nil;
	return [onlyLetters uppercaseString];
}

- (NSDictionary *)simplenoteAccountHeaders {
	return [NSDictionary dictionaryWithObjectsAndKeys:
			@"Simplenote/2.22 (Macintosh)", @"User-Agent",
			@"application/json", @"Accept", nil];
}

- (NSString *)simplenoteQueryValueNamed:(NSString *)name inEntry:(NSString *)entry {
	NSRange found = [entry rangeOfString:[name stringByAppendingString:@"="]];
	if (found.location == NSNotFound)
		return nil;
	NSString *rest = [entry substringFromIndex:NSMaxRange(found)];
	NSRange end = [rest rangeOfCharacterFromSet:[NSCharacterSet characterSetWithCharactersInString:@"&# \t\r\n\"'"]];
	if (end.location != NSNotFound)
		rest = [rest substringToIndex:end.location];
	NSString *decoded = [rest stringByRemovingPercentEncoding];
	return [decoded length] ? decoded : rest;
}

- (void)acceptSimplenoteSyncToken:(NSString *)token {
	NSString *stored = [SimplenoteSession storedSecretForSyncToken:token];
	if (![stored length]) {
		[self setVerificationStatus:VERIFY_FAILED withString:NSLocalizedString(@"Simplenote did not return a login token.", @"Simplenote login status")];
		return;
	}
	[syncPasswordField setStringValue:@""];
	[notationPrefs setSyncPassword:stored forService:SimplenoteServiceName];
	[self setVerificationStatus:VERIFY_SUCCESS withString:NSLocalizedString(@"Signed in", @"Simplenote login status")];
}

- (IBAction)emailSimplenoteLoginCode:(id)sender {
	NSString *email = [[syncAccountField stringValue] stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
	if (![email length]) {
		[self setVerificationStatus:VERIFY_FAILED withString:NSLocalizedString(@"Enter the email address for your Simplenote account.", @"Simplenote login status")];
		return;
	}
	[self cancelLoginVerifier];
	NSDictionary *body = [NSDictionary dictionaryWithObjectsAndKeys:[email lowercaseString], @"username", @"macOS", @"request_source", nil];
	NSData *postData = [self simplenoteJSONData:body];
	if (!postData)
		return;
	loginVerifier = [[SyncResponseFetcher alloc] initWithURL:[SimplenoteSession simplenoteLoginRequestURL] POSTData:postData headers:[self simplenoteAccountHeaders] contentType:@"application/json" delegate:self];
	[loginVerifier setRepresentedObject:@"request-login"];
	[loginVerifier start];
	[self setVerificationStatus:VERIFY_IN_PROGRESS withString:NSLocalizedString(@"Requesting a login code…", @"Simplenote login status")];
}

- (void)addMarkdownConversionButton {
	NSView *storageView = [storageFormatPopupButton superview];
	if (!storageView || [storageView viewWithTag:7301])
		return;
	NSButton *button = [[NSButton alloc] initWithFrame:NSZeroRect];
	[button setBezelStyle:NSBezelStyleRounded];
	[button setButtonType:NSButtonTypeMomentaryPushIn];
	[button setTitle:NSLocalizedString(@"Convert to Markdown", @"Notes storage button")];
	[button setTag:7301];
	[button setTarget:self];
	[button setAction:@selector(convertNotesToMarkdown:)];
	[button sizeToFit];
	NSRect frame = [button frame];
	frame.origin = NSMakePoint(42, 14);
	frame.size.width = MAX(frame.size.width + 16, 180);
	[button setFrame:frame];
	[button setAutoresizingMask:NSViewMaxXMargin | NSViewMaxYMargin];
	[storageView addSubview:button];
	[button release];
}

- (IBAction)convertNotesToMarkdown:(id)sender {
	[(AppController *)[NSApp delegate] convertNotesToMarkdown:sender];
}

- (BOOL)tableView:(NSTableView *)aTableView shouldEditTableColumn:(NSTableColumn *)aTableColumn row:(NSInteger)rowIndex {
	return (notationPrefs && [notationPrefs notesStorageFormat]);
}
- (BOOL)tableView:(NSTableView *)aTableView shouldSelectRow:(NSInteger)rowIndex {
    return (notationPrefs && [notationPrefs notesStorageFormat]);
}
- (void)tableViewSelectionDidChange:(NSNotification *)aNotification {
	NSTableView *tv = [aNotification object];
	BOOL isRowSelected = (([tv selectedRow] > -1)&&([tv selectedRow]!=NSNotFound));
	
	if (tv == allowedExtensionsTable) {
		[removeExtensionButton setEnabled:isRowSelected];
		[makeDefaultExtensionButton setEnabled:isRowSelected];
	} else if (tv == allowedTypesTable) {
		[removeTypeButton setEnabled:isRowSelected];
	}
}

- (void)settingChangedForSelectorString:(NSString*)selectorString {
	
	
	if ([selectorString isEqualToString:SEL_STR(setNotationPrefs:sender:)]) {
		
		//force these objects to re-init with the new notationprefs
		[changer release]; changer = nil;
		[passphrasePicker release]; passphrasePicker = nil;
		
		[notationPrefs release];
		notationPrefs = [[[GlobalPrefs defaultPrefs] notationPrefs] retain];
		
		if (didAwakeFromNib)
			[self initializeControls];
	}
}

- (void)initializeControls {
    //set up outlets to reflect new settings
    if (notationPrefs) {
		
		[keyLengthField setIntValue:[notationPrefs keyLengthInBits]];
		[keyLengthStepper setIntValue:[notationPrefs keyLengthInBits]];
		[self setEncryptionControlsState:[notationPrefs doesEncryption]];
		[self setSeparateFileControlsState:[notationPrefs notesStorageFormat]];
		[self updateRemoveKeychainItemStatus];
		[confirmFileDeletionButton setState:[notationPrefs confirmFileDeletion]];
		
		[enabledSyncButton setState:[notationPrefs syncServiceIsEnabled:SimplenoteServiceName]];
		NSString *username = [[notationPrefs syncAccountForServiceName:SimplenoteServiceName] objectForKey:@"username"];
		[syncAccountField setStringValue:username ? username : @""];
		
		[syncingFrequency selectItemWithTag:[notationPrefs syncFrequencyInMinutesForServiceName:SimplenoteServiceName]];
		
		[self setSyncControlsState:[notationPrefs syncServiceIsEnabled:SimplenoteServiceName]];
		
		[secureTextEntryButton setState:[notationPrefs secureTextEntry]];
		
		[allowedTypesTable reloadData];
		[allowedExtensionsTable reloadData];
        if (!IsMavericksOrLater||([notationPrefs notesStorageFormat]==SingleDatabaseFormat)) {
            [useFinderTaggingButton setHidden:YES];
        }
    }
}

- (void)setSyncControlsState:(BOOL)syncState {
	
	if (!syncState) {
		[self cancelLoginVerifier];
	}
	[self setVerificationStatus:VERIFY_NOT_ATTEMPTED withString:@""];
	[syncingFrequency setEnabled:syncState];
	[syncAccountField setEnabled:syncState];
	[syncPasswordField setEnabled:syncState];
	[(NSButton *)[[syncAccountField superview] viewWithTag:7302] setEnabled:syncState];
	[syncEncAlertView setHidden:!syncState || ![notationPrefs doesEncryption]];
	[syncEncAlertField setHidden:!syncState || ![notationPrefs doesEncryption]];
	if (syncState) {
		NSString *stored = [notationPrefs syncPasswordForServiceName:SimplenoteServiceName];
		if ([SimplenoteSession isStoredSyncToken:stored]) {
			[self setVerificationStatus:VERIFY_SUCCESS withString:NSLocalizedString(@"Signed in", @"Simplenote login status")];
		} else {
			[self setVerificationStatus:VERIFY_NOT_ATTEMPTED withString:NSLocalizedString(@"Email yourself a login code, then paste it here.", @"Simplenote login status")];
		}
	}
}

- (void)setEncryptionControlsState:(BOOL)encryptionState {
    [enableEncryptionButton setTitle:(encryptionState ? disableEncryptionString : enableEncryptionString)];
    [changePasswordButton setEnabled:encryptionState];
	[passwordSettingsMatrix setEnabled:encryptionState];
	
	[passwordSettingsMatrix setState:[notationPrefs storesPasswordInKeychain] atRow:0 column:0];
	[passwordSettingsMatrix setState:![notationPrefs storesPasswordInKeychain] atRow:1 column:0];
	
    [keyLengthField setEnabled:encryptionState];
    [keyLengthStepper setEnabled:encryptionState];
	
	BOOL syncState = [notationPrefs syncServiceIsEnabled:SimplenoteServiceName];
	[syncEncAlertView setHidden:!syncState || !encryptionState];
	[syncEncAlertField setHidden:!syncState || !encryptionState];
}

- (void)setSeparateFileControlsState:(BOOL)separateFileControlsState {
	[newExtensionButton setEnabled:separateFileControlsState];
	[removeExtensionButton setEnabled:separateFileControlsState && [allowedExtensionsTable selectedRow] > -1];
	[makeDefaultExtensionButton setEnabled:separateFileControlsState && [allowedExtensionsTable selectedRow] > -1];
	[newTypeButton setEnabled:separateFileControlsState];
	[removeTypeButton setEnabled:separateFileControlsState && [allowedTypesTable selectedRow] > -1];
	
	[allowedTypesTable setEnabled:separateFileControlsState];
	[allowedExtensionsTable setEnabled:separateFileControlsState];
	
	[confirmFileDeletionButton setEnabled:separateFileControlsState];
	
	[storageFormatPopupButton selectItemWithTag:[notationPrefs notesStorageFormat]];
	
	[fileAttributesHelpText setTextColor: separateFileControlsState ? [NSColor controlTextColor] : [NSColor grayColor]];	
}

- (void)updateRemoveKeychainItemStatus {
	
	if (![removeFromKeychainButton isHidden]) {
		SecKeychainItemRef itemRef = [notationPrefs currentKeychainItem];
		
		[removeFromKeychainButton setEnabled:(itemRef != NULL)];
		
		if (itemRef)
			CFRelease(itemRef);
	}
}

- (void)tableView:(NSTableView *)aTableView setObjectValue:(id)anObject 
   forTableColumn:(NSTableColumn *)aTableColumn row:(NSInteger)rowIndex {

	if (aTableView == allowedExtensionsTable) {
		if (![notationPrefs setExtension:anObject atIndex:(unsigned int)rowIndex])
			[self removedExtension:self];
	} else if (aTableView == allowedTypesTable) {
		if (![notationPrefs setType:anObject atIndex:(unsigned int)rowIndex])
			[self removedType:self];
	}
}


- (id)tableView:(NSTableView *)aTableView objectValueForTableColumn:(NSTableColumn *)aTableColumn row:(NSInteger)rowIndex {

	if (aTableView == allowedExtensionsTable) {
		NSString *extension = [notationPrefs pathExtensionAtIndex:rowIndex];
		
		if ([notationPrefs indexOfChosenPathExtension] == (unsigned int)rowIndex) {
			return [[[NSAttributedString alloc] initWithString:extension attributes:
					[NSDictionary dictionaryWithObjectsAndKeys:
					 [NSFont boldSystemFontOfSize:[NSFont smallSystemFontSize]], NSFontAttributeName, nil]] autorelease];
		}
		return extension;
			
	} else if (aTableView == allowedTypesTable) {
		
		return [notationPrefs typeStringAtIndex:rowIndex];
	}
	return 0;
}

- (NSInteger)numberOfRowsInTableView:(NSTableView *)aTableView {
	if (aTableView == allowedExtensionsTable)
		return [notationPrefs pathExtensionsCount];
	else if (aTableView == allowedTypesTable)
		return [notationPrefs typeStringsCount];
	
	return 0;
}

- (IBAction)addedExtension:(id)sender {
    [notationPrefs addAllowedPathExtension:@""];
	[allowedExtensionsTable reloadData];
	
	[allowedExtensionsTable selectRowIndexes:[NSIndexSet indexSetWithIndex:[notationPrefs pathExtensionsCount]-1] byExtendingSelection:NO];
	[allowedExtensionsTable editColumn:0 row:[notationPrefs pathExtensionsCount]-1 withEvent:nil select:YES];
}

- (IBAction)addedType:(id)sender {
    [notationPrefs addAllowedType:@""];
	[allowedTypesTable reloadData];
	
	[allowedTypesTable selectRowIndexes:[NSIndexSet indexSetWithIndex:[notationPrefs typeStringsCount]-1] byExtendingSelection:NO];
	[allowedTypesTable editColumn:0 row:[notationPrefs typeStringsCount]-1 withEvent:nil select:YES];

}

- (IBAction)changedKeyLength:(id)sender {
    
    int bits = [keyLengthStepper intValue];
    [keyLengthField setIntValue:bits];
    [notationPrefs setKeyLengthInBits:bits];
}

- (IBAction)changedKeychainSettings:(id)sender {
	//matrix does not change until the next runloop iteration, apparently
	if (sender != self)
		[self performSelector:@selector(changedKeychainSettings:) withObject:self afterDelay:0.0];
	else
		[notationPrefs setStoresPasswordInKeychain:[[passwordSettingsMatrix cellAtRow:0 column:0] state]];
		
}

- (IBAction)changedFileDeletionWarningSettings:(id)sender {
    [notationPrefs setConfirmsFileDeletion:[confirmFileDeletionButton state]];
}

- (IBAction)removeFromKeychain:(id)sender {
	[notationPrefs removeKeychainData];

	[self updateRemoveKeychainItemStatus];
}

- (NSInteger)notesStorageFormatInProgress {
	return notesStorageFormatInProgress;
}

- (void)runQueuedStorageFormatChangeInvocation {
	[postStorageFormatInvocation performSelector:@selector(invoke) withObject:nil afterDelay:0.0];
	[postStorageFormatInvocation release];
	postStorageFormatInvocation = nil;
}

- (void)notesStorageFormatDidChange {
	notesStorageFormatInProgress = [notationPrefs notesStorageFormat];
	[self setSeparateFileControlsState:notesStorageFormatInProgress];
	
    [allowedExtensionsTable reloadData];
    [allowedTypesTable reloadData];
}

- (IBAction)changedFileStorageFormat:(id)sender {
    NSInteger storageTag = [storageFormatPopupButton selectedTag];
	if (storageTag != SingleDatabaseFormat && [notationPrefs doesEncryption]) {
		if (NSRunAlertPanel(NSLocalizedString(@"Encryption is currently on, but storing notes individually requires it to be off. Disable encryption?",nil),
							NSLocalizedString(@"Warning: Your notes will be written to disk in clear text.",nil), NSLocalizedString(@"Disable Encryption",nil), 
							NSLocalizedString(@"Cancel",nil), NULL) == NSAlertDefaultReturn) {
			
			//disable encryption
			[self disableEncryptionWithWarning:NO];
		} else {
			//cancelled
			[self notesStorageFormatDidChange];
			return;
		}
	}
	
	notesStorageFormatInProgress = storageTag;
	
	//if we're changing to a database format from a non-database-format, ask to trash existing files
    if ([notationPrefs shouldDisplaySheetForProposedFormat:notesStorageFormatInProgress]) {
		
		NSAlert *alert = [NSAlert alertWithMessageText:NSLocalizedString(@"Individual files remain in the notes directory. Leave them alone or move them to the Trash?",nil) 
										 defaultButton:NSLocalizedString(@"Keep Files", @"button title for not discarding note files") 
									   alternateButton:NSLocalizedString(@"Cancel",nil) otherButton:NSLocalizedString(@"Move to Trash", @"button title for trashing notes")
							 informativeTextWithFormat:NSLocalizedString(@"When notes are stored in a single database individual files become redundant.",nil)];
		
		[alert beginSheetModalForWindow:[view window] modalDelegate:notationPrefs 
						 didEndSelector:@selector(noteFilesCleanupSheetDidEnd:returnCode:contextInfo:) contextInfo:self];
		//will ultimately call -notesStorageFormatDidChange
	} else {
		//just call setNotesStorageFormat straight-out
		[notationPrefs setNotesStorageFormat:notesStorageFormatInProgress];
		[self notesStorageFormatDidChange];
		
		//sheet ending will not do this for us--there is no sheet
		[self runQueuedStorageFormatChangeInvocation];
	}
	
	
	if ([[storageFormatPopupButton objectValue] intValue]==0) {
		[[NSUserDefaults standardUserDefaults] setObject:nil forKey:@"TextEditor"];
		[[NSUserDefaults standardUserDefaults] synchronize];
	}else {
		[[NSUserDefaults standardUserDefaults] setObject:@"Default" forKey:@"TextEditor"];
		[[NSUserDefaults standardUserDefaults] synchronize];
//		if ( [[NSApp delegate] respondsToSelector: @selector(updateTextApp:)] ) {
//			[[NSApp delegate] updateTextApp:self];
//		}
	}

	
}

- (IBAction)toggledSyncing:(id)sender {
	[notationPrefs setSyncEnabled:[enabledSyncButton state] forService:SimplenoteServiceName];
	[self setSyncControlsState:[enabledSyncButton state]];
}

- (IBAction)syncFrequencyChange:(id)sender {
	if (sender) {
		[self performSelector:_cmd withObject:nil afterDelay:0.0];
	} else {
		[notationPrefs setSyncFrequency:[syncingFrequency selectedTag] forService:SimplenoteServiceName];
	}
}

- (void)syncEditingDidEnd:(NSNotification *)aNotification {
	if (!verificationAttempted) {
		[self cancelLoginVerifier];
		[self startLoginVerifier];
	}
}

- (void)syncCredentialsDidChange:(NSNotification *)aNotification {
	
	if ([aNotification object] == syncAccountField) {
		[notationPrefs removeSyncPasswordForService:SimplenoteServiceName];
		[notationPrefs setSyncUsername:[syncAccountField stringValue] forService:SimplenoteServiceName];
		
		[self startVerifyingAfterDelay];
	} else if ([aNotification object] == syncPasswordField) {
		[self startVerifyingAfterDelay];
	}
}


- (void)setVerificationStatus:(int)status withString:(NSString*)aString {
	
	switch (status) {
		case VERIFY_NOT_ATTEMPTED:
			verificationAttempted = NO;
			[verifyStatusImageView setImage:nil];
			break;
		case VERIFY_FAILED:
			verificationAttempted = YES;
			[verifyStatusImageView setImage:[NSImage imageNamed:@"statusError"]];
			break;
		case VERIFY_IN_PROGRESS:
			[verifyStatusImageView setImage:[NSImage imageNamed:@"statusInProgress"]];
			break;
		case VERIFY_SUCCESS:
			verificationAttempted = YES;
			[verifyStatusImageView setImage:[NSImage imageNamed:@"statusValidated"]];
			break;
	}
	[verifyStatusImageView setHidden: VERIFY_NOT_ATTEMPTED == status];
	[verifyStatusField setStringValue: aString ? aString : @""];
}

- (void)startVerifyingAfterDelay {
	[self cancelLoginVerifier];
	
	[self performSelector:@selector(startLoginVerifier) withObject:nil afterDelay:1.5];
}

- (void)cancelLoginVerifier {
	[loginVerifier cancel];
	[loginVerifier autorelease];
	loginVerifier = nil;
	[self setVerificationStatus:VERIFY_NOT_ATTEMPTED withString:@""];
	
	[NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(startLoginVerifier) object:nil];
}

- (void)startLoginVerifier {
	if (loginVerifier)
		return;
	NSString *email = [[syncAccountField stringValue] stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
	NSString *entry = [[syncPasswordField stringValue] stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
	if (![email length] || ![entry length])
		return;

	if ([entry rangeOfString:@"://"].location != NSNotFound) {
		NSString *directToken = [self simplenoteQueryValueNamed:@"token" inEntry:entry];
		if ([directToken length] && [[self simplenoteQueryValueNamed:@"auth_code" inEntry:entry] length] == 0) {
			[self acceptSimplenoteSyncToken:directToken];
			return;
		}
	}
	NSString *code = [self simplenoteLoginCodeFromEntry:entry];
	if (![code length])
		return;
	NSDictionary *body = [NSDictionary dictionaryWithObjectsAndKeys:[email lowercaseString], @"username", code, @"auth_code", nil];
	NSData *postData = [self simplenoteJSONData:body];
	if (!postData)
		return;
	loginVerifier = [[SyncResponseFetcher alloc] initWithURL:[SimplenoteSession simplenoteLoginCompletionURL] POSTData:postData headers:[self simplenoteAccountHeaders] contentType:@"application/json" delegate:self];
	[loginVerifier setRepresentedObject:@"complete-login"];
	[loginVerifier start];
	[self setVerificationStatus:VERIFY_IN_PROGRESS withString:@""];
}

- (void)syncResponseFetcher:(SyncResponseFetcher*)fetcher receivedData:(NSData*)data returningError:(NSString*)errString {
	BOOL httpFailed = errString || [fetcher statusCode] >= 400;
	BOOL requestedCode = [[fetcher representedObject] isEqualToString:@"request-login"];
	if (requestedCode) {
		if ([fetcher statusCode] == 429) {
			[self setVerificationStatus:VERIFY_FAILED withString:NSLocalizedString(@"Simplenote asked me to wait before sending another code.", @"Simplenote login status")];
		} else if (httpFailed) {
			[self setVerificationStatus:VERIFY_FAILED withString:NSLocalizedString(@"Simplenote did not send a login code.", @"Simplenote login status")];
		} else {
			[self setVerificationStatus:VERIFY_NOT_ATTEMPTED withString:NSLocalizedString(@"Check your email and paste the login code.", @"Simplenote login status")];
		}
		return;
	}

	if ([fetcher statusCode] == 429) {
		[self setVerificationStatus:VERIFY_FAILED withString:NSLocalizedString(@"Simplenote asked me to wait before trying that code again.", @"Simplenote login status")];
		return;
	}
	if (httpFailed) {
		[self setVerificationStatus:VERIFY_FAILED withString:NSLocalizedString(@"That login code was not accepted.", @"Simplenote login status")];
		return;
	}

	NSString *bodyString = [[[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding] autorelease];
	NSDictionary *response = nil;
	@try {
		response = [NSDictionary dictionaryWithJSONString:bodyString];
	} @catch (NSException *exception) {
		response = nil;
	}
	NSString *token = [response objectForKey:@"sync_token"];
	if (![token isKindOfClass:[NSString class]] || ![token length])
		token = [response objectForKey:@"access_token"];
	if (![token isKindOfClass:[NSString class]] || ![token length]) {
		[self setVerificationStatus:VERIFY_FAILED withString:NSLocalizedString(@"Simplenote did not return a login token.", @"Simplenote login status")];
		return;
	}
	[self acceptSimplenoteSyncToken:token];
}

- (IBAction)changedSecureTextEntry:(id)sender {
	[notationPrefs setSecureTextEntry:[secureTextEntryButton state]];
}

- (IBAction)changePassphrase:(id)sender {
	
	NSAssert([notationPrefs doesEncryption], @"Encryption must be on before the password can be changed.");
	
	if (!changer) changer = [[PassphraseChanger alloc] initWithNotationPrefs:notationPrefs];
	[changer showAroundWindow:[view window]];
}

- (IBAction)visitSimplenoteSite:(id)sender {
    [[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:@"http://simplenote.com/"]];
}

- (IBAction)makeDefaultExtension:(id)sender {
	[[allowedExtensionsTable window] makeFirstResponder:allowedExtensionsTable];
	
	NSUInteger selectedRow = (NSUInteger)[allowedExtensionsTable selectedRow];
	if (selectedRow != NSNotFound)
		[notationPrefs setChosenPathExtensionAtIndex:selectedRow];
	
	[allowedExtensionsTable reloadData];	
}

- (IBAction)removedExtension:(id)sender {
	[allowedExtensionsTable abortEditing];
	
	NSUInteger selectedRow = (NSUInteger)[allowedExtensionsTable selectedRow];
	if (selectedRow != NSNotFound)
		if (![notationPrefs removeAllowedPathExtensionAtIndex:selectedRow]) NSBeep();
	
	[allowedExtensionsTable reloadData];
}

- (IBAction)removedType:(id)sender {
	[allowedTypesTable abortEditing];
	
	NSUInteger selectedRow =(NSUInteger)[allowedTypesTable selectedRow];
	if (selectedRow !=NSNotFound)
		[notationPrefs removeAllowedTypeAtIndex:selectedRow];
	
	[allowedTypesTable reloadData];
}

- (void)passphrasePicker:(PassphrasePicker*)aPicker choseAPassphrase:(BOOL)success {
	
	[self setEncryptionControlsState:success];
	[notationPrefs setDoesEncryption:success];
	[self updateRemoveKeychainItemStatus];
}

- (void)encryptionFormatMismatchSheetDidEnd:(NSWindow *)sheet returnCode:(int)returnCode contextInfo:(void *)contextInfo {
	if (returnCode == NSAlertDefaultReturn) {
		//switching to single DB
		[storageFormatPopupButton selectItemWithTag:SingleDatabaseFormat];
		
		[self performSelector:@selector(changedFileStorageFormat:) withObject:storageFormatPopupButton afterDelay:0.0];
		
		//need to show PW picker dialog after this ->
		
		//[picker showAroundWindow:[view window] resultDelegate:self];
		
		[postStorageFormatInvocation release];
		
		//so queue it up:
		InvocationRecorder *invRecorder = [InvocationRecorder invocationRecorder];
		[[invRecorder prepareWithInvocationTarget:passphrasePicker] showAroundWindow:[view window] resultDelegate:self];
		postStorageFormatInvocation = [[invRecorder invocation] retain];
	}
}

- (void)enableEncryption {
	if (!passphrasePicker) passphrasePicker = [[PassphrasePicker alloc] initWithNotationPrefs:notationPrefs];
	
	NSInteger format = [notationPrefs notesStorageFormat];
	if (format == SingleDatabaseFormat) {
		
		[passphrasePicker showAroundWindow:[view window] resultDelegate:self];
	} else {
		NSString *formatStrings[] = { NSLocalizedString(@"(WHAT??)",@"user shouldn't see this"), 
			NSLocalizedString(@"plain text",nil), NSLocalizedString(@"rich text",nil), NSLocalizedString(@"HTML",nil) };
		NSAlert *alert = [NSAlert alertWithMessageText:[NSString stringWithFormat:NSLocalizedString(@"Your notes are currently stored as %@ files on disk, but encryption requires a single database. Switch to a database format?",nil), formatStrings[format]]
										 defaultButton:NSLocalizedString(@"Use a single database file",nil) alternateButton:NSLocalizedString(@"Cancel",nil) otherButton:nil
							 informativeTextWithFormat:NSLocalizedString(@"Notational Velocity supports encryption only for notes stored in a database file.",nil)];
		
		[alert beginSheetModalForWindow:[view window] modalDelegate:self 
						 didEndSelector:@selector(encryptionFormatMismatchSheetDidEnd:returnCode:contextInfo:) contextInfo:NULL];
	}
}

- (void)disableEncryptionWarningSheetDidEnd:(NSWindow *)sheet returnCode:(int)returnCode contextInfo:(void *)contextInfo {
	if (returnCode == NSAlertDefaultReturn) {
		[self _disableEncryption];
	}
}

- (void)_disableEncryption {
	[self setEncryptionControlsState:NO];
	[notationPrefs setDoesEncryption:NO];
	[self updateRemoveKeychainItemStatus];
	
	[passphrasePicker release]; passphrasePicker = nil;
}

- (void)disableEncryptionWithWarning:(BOOL)warning {
	if ([notationPrefs doesEncryption]) {
		if (warning) {
			NSAlert *alert = [NSAlert alertWithMessageText:NSLocalizedString(@"Disable note encryption now?",nil)
											 defaultButton:NSLocalizedString(@"Disable Encryption",@"button title for disabling note encryption") 
										   alternateButton:NSLocalizedString(@"Cancel",nil) otherButton:nil
								 informativeTextWithFormat:NSLocalizedString(@"Warning: Your notes will be written to disk in clear text.",nil)];
			
			[alert beginSheetModalForWindow:[view window] modalDelegate:self 
							 didEndSelector:@selector(disableEncryptionWarningSheetDidEnd:returnCode:contextInfo:) contextInfo:NULL];
			
		} else {
			[self _disableEncryption];
		}
		
	} else {
		NSLog(@"Not disabling encryption because it is already off.");
	}
}

- (IBAction)toggledEncryption:(id)sender {
	BOOL encryptionOn = ![notationPrefs doesEncryption];
	
	if (encryptionOn) {
		[self enableEncryption];
	} else {
		[self disableEncryptionWithWarning:YES];
	}
}


#pragma mark nvALT Finder tagging

- (IBAction)switchToFinderTags:(id)sender{
    if (IsMavericksOrLater) {
        NSAlert *tagWarning=[NSAlert new];
        [tagWarning setMessageText:NSLocalizedString(@"This will permanently convert all your nvALT tags to Finder tags, and use Finder tags from here on out.", @"Finder tag warning message text")];
        [tagWarning setInformativeText:NSLocalizedString(@"This cannot be undone.", @"Finder tag warning informative text")];
        [tagWarning addButtonWithTitle:NSLocalizedString(@"Do It", @"name of delete button")];
        [tagWarning addButtonWithTitle:NSLocalizedString(@"Cancel", @"name of cancel button")];
        [tagWarning beginSheetModalForWindow:[view window] completionHandler:^(NSModalResponse returnCode) {
            if (returnCode == NSAlertFirstButtonReturn) {
                [[GlobalPrefs defaultPrefs]setUseFinderTags:sender];
            }
        }];
        [tagWarning release];
    }
}

@end
