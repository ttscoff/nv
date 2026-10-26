//
//  LinkingEditor_Indentation.m
//  Notation
//

/*
 Modified code based on:
 
 Smultron version 3.1.2, 2007-07-16
 Written by Peter Borg, pgw3@mac.com
 Find the latest version at http://smultron.sourceforge.net
 
 Copyright 2004-2007 Peter Borg
 
 Licensed under the Apache License, Version 2.0 (the "License"); you may not
 use this file except in compliance with the License. You may obtain a copy
 of the License at
 
 http://www.apache.org/licenses/LICENSE-2.0
 
 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 License for the specific language governing permissions and limitations
 under the License.
 */



#import "LinkingEditor_Indentation.h"
#import "GlobalPrefs.h"

@implementation LinkingEditor (Indentation)

- (NSInteger)indentSpaceWidth {
	NSInteger width = [[GlobalPrefs defaultPrefs] numberOfSpacesInTab];
	return width > 0 ? width : 4;
}

- (NSString *)indentUnitString {
	if ([[GlobalPrefs defaultPrefs] softTabs])
		return [@"" stringByPaddingToLength:(NSUInteger)[self indentSpaceWidth] withString:@" " startingAtIndex:0];
	return @"\t";
}

- (NSUInteger)charactersToOutdentAtLineStart:(NSString *)line {
	if (![line length])
		return 0;
	if ([[GlobalPrefs defaultPrefs] softTabs]) {
		NSString *unit = [self indentUnitString];
		if ([line hasPrefix:unit])
			return [unit length];
		if ([line hasPrefix:@"\t"])
			return 1;
		NSUInteger spaces = 0;
		NSUInteger width = (NSUInteger)[self indentSpaceWidth];
		while (spaces < [line length] && spaces < width && [line characterAtIndex:spaces] == ' ')
			spaces++;
		return spaces;
	}
	unichar character = [line characterAtIndex:0];
	if (character == '\t' || character == ' ')
		return 1;
	return 0;
}

- (void)adjustSelection:(NSRange *)selection forEditAt:(NSRange)edited replacementLength:(NSUInteger)replacementLength {
	NSInteger delta = (NSInteger)replacementLength - (NSInteger)edited.length;
	if (NSMaxRange(edited) <= selection->location) {
		selection->location += delta;
	} else if (edited.location < NSMaxRange(*selection)) {
		if (edited.location <= selection->location)
			selection->location = edited.location + replacementLength;
		NSInteger newLength = (NSInteger)selection->length + delta;
		selection->length = newLength > 0 ? (NSUInteger)newLength : 0;
	}
}

- (IBAction)shiftLeftAction:(id)sender {
	NSString *text = [self string];
	if ([text length] < 1)
		return;
	NSArray *originalRanges = [[self selectedRanges] copy];
	[[self undoManager] beginUndoGrouping];
	NSMutableArray *updatedRanges = [NSMutableArray array];
	NSInteger removedSoFar = 0;
	BOOL removedAny = NO;
	NSUInteger rangeIndex;
	for (rangeIndex = 0; rangeIndex < [originalRanges count]; rangeIndex++) {
		NSRange selection = [[originalRanges objectAtIndex:rangeIndex] rangeValue];
		if ((NSInteger)selection.location >= removedSoFar)
			selection.location -= removedSoFar;
		else
			selection.location = 0;
		text = [self string];
		if (!text.length)
			break;
		if (selection.location > [text length])
			selection.location = [text length];
		NSUInteger endLocation = selection.length ? NSMaxRange(selection) - 1 : selection.location;
		if (endLocation >= [text length])
			endLocation = [text length] - 1;
		NSRange firstLine = [text lineRangeForRange:NSMakeRange(MIN(selection.location, [text length] - 1), 0)];
		NSRange lastLine = [text lineRangeForRange:NSMakeRange(endLocation, 0)];
		NSMutableArray *lines = [NSMutableArray array];
		NSUInteger scan = firstLine.location;
		while (scan <= lastLine.location && scan < [text length]) {
			NSRange lineRange = [text lineRangeForRange:NSMakeRange(scan, 0)];
			[lines addObject:[NSValue valueWithRange:lineRange]];
			if (NSMaxRange(lineRange) <= scan)
				break;
			scan = NSMaxRange(lineRange);
		}
		NSInteger removedInSelection = 0;
		NSInteger lineIndex;
		for (lineIndex = (NSInteger)[lines count] - 1; lineIndex >= 0; lineIndex--) {
			NSRange lineRange = [[lines objectAtIndex:(NSUInteger)lineIndex] rangeValue];
			text = [self string];
			if (lineRange.location >= [text length])
				continue;
			NSString *line = [text substringWithRange:lineRange];
			NSUInteger remove = [self charactersToOutdentAtLineStart:line];
			if (!remove)
				continue;
			NSRange edited = NSMakeRange(lineRange.location, remove);
			if (![self shouldChangeTextInRange:edited replacementString:@""])
				continue;
			[self replaceCharactersInRange:edited withString:@""];
			[self adjustSelection:&selection forEditAt:edited replacementLength:0];
			removedInSelection += (NSInteger)remove;
			removedAny = YES;
		}
		NSUInteger editedLength = [[self string] length];
		if (selection.location > editedLength)
			selection.location = editedLength;
		if (NSMaxRange(selection) > editedLength)
			selection.length = editedLength - selection.location;
		[updatedRanges addObject:[NSValue valueWithRange:selection]];
		removedSoFar += removedInSelection;
	}
	if (removedAny) {
		[self setSelectedRanges:updatedRanges];
		[self didChangeText];
		[[self undoManager] setActionName:NSLocalizedString(@"Outdent", nil)];
	} else {
		NSBeep();
	}
	[[self undoManager] endUndoGrouping];
	[originalRanges release];
}

- (IBAction)shiftRightAction:(id)sender {
	if ([[self string] length] < 1)
		return;
	NSString *unit = [self indentUnitString];
	NSArray *originalRanges = [[self selectedRanges] copy];
	[[self undoManager] beginUndoGrouping];
	NSMutableArray *updatedRanges = [NSMutableArray array];
	NSInteger insertedSoFar = 0;
	NSUInteger rangeIndex;
	for (rangeIndex = 0; rangeIndex < [originalRanges count]; rangeIndex++) {
		NSRange selection = [[originalRanges objectAtIndex:rangeIndex] rangeValue];
		selection.location += insertedSoFar;
		NSString *text = [self string];
		if (selection.location > [text length])
			selection.location = [text length];
		NSUInteger endLocation = selection.length ? NSMaxRange(selection) - 1 : selection.location;
		if (endLocation >= [text length])
			endLocation = [text length] ? [text length] - 1 : 0;
		NSRange firstLine = [text lineRangeForRange:NSMakeRange(MIN(selection.location, [text length] - 1), 0)];
		NSRange lastLine = [text lineRangeForRange:NSMakeRange(endLocation, 0)];
		NSMutableArray *lines = [NSMutableArray array];
		NSUInteger scan = firstLine.location;
		while (scan <= lastLine.location && scan < [text length]) {
			NSRange lineRange = [text lineRangeForRange:NSMakeRange(scan, 0)];
			[lines addObject:[NSValue valueWithRange:lineRange]];
			if (NSMaxRange(lineRange) <= scan)
				break;
			scan = NSMaxRange(lineRange);
		}
		NSInteger insertedInSelection = 0;
		NSInteger lineIndex;
		for (lineIndex = (NSInteger)[lines count] - 1; lineIndex >= 0; lineIndex--) {
			NSRange lineRange = [[lines objectAtIndex:(NSUInteger)lineIndex] rangeValue];
			NSRange edited = NSMakeRange(lineRange.location, 0);
			if (![self shouldChangeTextInRange:edited replacementString:unit])
				continue;
			[self replaceCharactersInRange:edited withString:unit];
			[self adjustSelection:&selection forEditAt:edited replacementLength:[unit length]];
			insertedInSelection += (NSInteger)[unit length];
		}
		NSUInteger editedLength = [[self string] length];
		if (selection.location > editedLength)
			selection.location = editedLength;
		if (NSMaxRange(selection) > editedLength)
			selection.length = editedLength - selection.location;
		[updatedRanges addObject:[NSValue valueWithRange:selection]];
		insertedSoFar += insertedInSelection;
	}
	[self setSelectedRanges:updatedRanges];
	[self didChangeText];
	[[self undoManager] setActionName:NSLocalizedString(@"Indent", nil)];
	[[self undoManager] endUndoGrouping];
	[originalRanges release];
}


@end
