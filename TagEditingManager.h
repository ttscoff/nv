//
//  TagEditingManager.h
//  Notation
//

#import <Cocoa/Cocoa.h>

@class NoteObject;

@interface NVTagToken : NSObject <NSCopying> {
	NSString *title;
	BOOL partial;
}

+ (NSArray *)tokensWithTitles:(NSArray *)titles partial:(BOOL)partial;
- (NSString *)title;
- (void)setTitle:(NSString *)aTitle;
- (BOOL)isPartial;
- (void)setPartial:(BOOL)flag;

@end

@interface NVTagTokenController : NSObject <NSTokenFieldDelegate> {
	NSTokenField *field;
	id delegate;
	id labelsSource;
	NSArray *targetNotes;
	NSMutableArray *originalLabels;
	NSMutableSet *syncedKeys;
	NSMutableSet *partialKeys;
	BOOL suppressSync;
	BOOL cancelled;
	BOOL didEnd;
	BOOL awaitingEnd;
}

- (void)attachToField:(NSTokenField *)aField;
- (void)showTitles:(NSArray *)titles partialTitles:(NSArray *)partialTitles;
- (void)endEditing;
- (void)cancelEditing;
- (BOOL)isAwaitingEndEditing;
- (NSArray *)titles;
- (void)setDelegate:(id)aDelegate;
- (void)setLabelsSource:(id)aSource;
- (void)setTargetNotes:(NSArray *)notes;

@end

@interface TagEditingManager : NSObject <NSWindowDelegate> {
	IBOutlet NSPanel *tagPanel;
	IBOutlet NSTokenField *tagField;
	NVTagTokenController *tokenController;
	BOOL isHappening;
	NSArray *targetNotes;
}

- (id)initWithDelegate:(id)del notes:(NSArray *)notes labelsSource:(id)labelsSource atPoint:(NSPoint)centerpoint;
- (void)commitTokens:(id)sender;
- (NSTokenField *)tagField;
- (BOOL)isMultitagging;

@end
