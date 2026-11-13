#import "NoteLayout.h"
#import <objc/runtime.h>

static BOOL layingOut = NO;
static BOOL readingHeight = NO;
static const void *NVCommittedHeightKey = &NVCommittedHeightKey;

static CGFloat NVCommittedHeight(NSTextView *textView) {
	NSNumber *number = objc_getAssociatedObject(textView, NVCommittedHeightKey);
	return number ? [number doubleValue] : 0;
}

static void NVSetCommittedHeight(NSTextView *textView, CGFloat height) {
	NSNumber *number = [[NSNumber alloc] initWithDouble:height];
	objc_setAssociatedObject(textView, NVCommittedHeightKey, number, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
	[number release];
}

/* A container clipped to the window still reports every glyph as laid out.
   The last glyph has no line fragment in that case, so the used rect is only
   one screen tall and cannot be used to size the note. */
static BOOL NVNoteLayoutIsReal(NSTextView *textView) {
	NSLayoutManager *layout = [textView layoutManager];
	NSUInteger glyphs;
	NSRect last;
	if (![[textView string] length])
		return YES;
	if (!layout)
		return NO;
	glyphs = [layout numberOfGlyphs];
	if (!glyphs)
		return NO;
	last = [layout lineFragmentRectForGlyphAtIndex:glyphs - 1 effectiveRange:NULL];
	return !NSIsEmptyRect(last);
}

static CGFloat NVUsedNoteHeight(NSTextView *textView) {
	NSLayoutManager *layout = [textView layoutManager];
	NSTextContainer *container = [textView textContainer];
	NSRect used, extra;
	CGFloat height;
	if (!layout || !container)
		return 0;
	used = [layout usedRectForTextContainer:container];
	extra = [layout extraLineFragmentRect];
	height = NSMaxY(used);
	if (!NSIsEmptyRect(extra))
		height = MAX(height, NSMaxY(extra));
	/* The used rect is cleared as soon as any line is invalidated, while the
	   last line can still be sitting at the bottom of the note. An empty used
	   rect is not a one-screen note. */
	if ([layout numberOfGlyphs]) {
		NSRect last = [layout lineFragmentRectForGlyphAtIndex:[layout numberOfGlyphs] - 1 effectiveRange:NULL];
		if (!NSIsEmptyRect(last))
			height = MAX(height, NSMaxY(last));
	}
	height = ceil(height + [textView textContainerInset].height * 2.0);
	if (height < [textView minSize].height)
		height = [textView minSize].height;
	if ([textView maxSize].height > 1.0 && height > [textView maxSize].height)
		height = [textView maxSize].height;
	return height;
}

CGFloat NVNoteHeightForProposedHeight(NSTextView *textView, CGFloat proposed) {
	NSLayoutManager *layout;
	NSUInteger length, unlaid;
	CGFloat current, used;
	if (!textView)
		return proposed;
	/* A note that has already been measured stays that tall. Measuring again
	   here is what lets a cleared used rect shrink it back to the window. */
	if (NVCommittedHeight(textView) > proposed)
		return NVCommittedHeight(textView);
	current = NSHeight([textView frame]);
	/* Measuring the note can resize it. A nested resize must not take the
	   short height that triggered the measurement. */
	if (readingHeight)
		return proposed > current ? proposed : current;
	layout = [textView layoutManager];
	length = [[textView string] length];
	if (!layout || !length)
		return proposed;
	readingHeight = YES;
	unlaid = [layout firstUnlaidCharacterIndex];
	if ((unlaid == NSNotFound || unlaid >= length) && NVNoteLayoutIsReal(textView)) {
		used = NVUsedNoteHeight(textView);
		if (used > proposed)
			proposed = used;
	} else if (current > proposed) {
		proposed = current;
	}
	readingHeight = NO;
	return proposed;
}

void NVLayOutEntireNote(NSTextView *textView) {
	NSLayoutManager *layout;
	NSTextContainer *container;
	NSUInteger length;
	NSSize containerSize;
	CGFloat height;
	NSRect frame;
	NSScrollView *scroll;
	if (!textView || layingOut)
		return;
	layout = [textView layoutManager];
	container = [textView textContainer];
	if (!layout || !container)
		return;
	/* Idle layout measures the visible screen and resizes the view to match,
	   which hides the scrollbar. */
	[layout setBackgroundLayoutEnabled:NO];
	length = [[textView string] length];
	layingOut = YES;
	[container setHeightTracksTextView:NO];
	if (length) {
		containerSize = [container containerSize];
		if (containerSize.height < CGFLOAT_MAX / 4.0) {
			if (containerSize.width < 1.0)
				containerSize.width = NSWidth([textView bounds]);
			[container setContainerSize:NSMakeSize(containerSize.width, CGFLOAT_MAX)];
		}
		[layout ensureLayoutForCharacterRange:NSMakeRange(0, length)];
	}
	height = length ? NVUsedNoteHeight(textView) : [textView minSize].height;
	frame = [textView frame];
	/* The height captured above can be the window: the used rect was empty, so
	   it was raised to the minimum, and only the check below forces the last
	   line to lay out. Commit that line, not the earlier minimum. */
	if (!length) {
		NVSetCommittedHeight(textView, height);
	} else if ([container containerSize].height > CGFLOAT_MAX / 4.0 && [layout numberOfGlyphs]) {
		NSRect used = [layout usedRectForTextContainer:container];
		NSRect last = [layout lineFragmentRectForGlyphAtIndex:[layout numberOfGlyphs] - 1 effectiveRange:NULL];
		NSUInteger lastChar = [layout characterIndexForGlyphAtIndex:[layout numberOfGlyphs] - 1];
		if (!NSIsEmptyRect(last) && lastChar + 1 >= length && NSHeight(used) > 1.0 && NSMaxY(used) + 1.0 >= NSMaxY(last)) {
			CGFloat verified = ceil(MAX(NSMaxY(used), NSMaxY(last)) + [textView textContainerInset].height * 2.0);
			if (verified < [textView minSize].height)
				verified = [textView minSize].height;
			if ([textView maxSize].height > 1.0 && verified > [textView maxSize].height)
				verified = [textView maxSize].height;
			height = verified;
			NVSetCommittedHeight(textView, height);
		} else if (!NSIsEmptyRect(last)) {
			CGFloat lastHeight = ceil(NSMaxY(last) + [textView textContainerInset].height * 2.0);
			if (lastHeight > height)
				height = lastHeight;
		}
	}
	if (NVCommittedHeight(textView) > height)
		height = NVCommittedHeight(textView);
	/* A one-screen measurement must not replace a note that is already taller. */
	if (length && !NVNoteLayoutIsReal(textView) && NSHeight(frame) > height)
		height = NSHeight(frame);
	if (height > 0.0 && fabs(NSHeight(frame) - height) > 1.0) {
		frame.size.height = height;
		[textView setFrame:frame];
	}
	layingOut = NO;
	scroll = [textView enclosingScrollView];
	if (!scroll)
		return;
	[scroll tile];
	[scroll reflectScrolledClipView:[scroll contentView]];
	NVRevealNoteTextIfScrolledPastIt(textView);
}

void NVRevealNoteTextIfScrolledPastIt(NSTextView *textView) {
	NSScrollView *scroll;
	NSLayoutManager *layout;
	NSTextContainer *container;
	NSRect used, visible;
	NSPoint origin;
	static BOOL revealing = NO;
	if (!textView || revealing || layingOut)
		return;
	scroll = [textView enclosingScrollView];
	layout = [textView layoutManager];
	container = [textView textContainer];
	if (!scroll || !layout || !container)
		return;
	if (![[textView string] length] || ![layout numberOfGlyphs])
		return;
	used = [layout usedRectForTextContainer:container];
	if (NSHeight(used) < 1.0)
		return;
	origin = [textView textContainerOrigin];
	used.origin.x += origin.x;
	used.origin.y += origin.y;
	visible = [textView visibleRect];
	if (NSIntersectsRect(visible, used))
		return;
	/* The clip is in the empty tail left by a taller note. Nothing else will
	   lay that area out or scroll back, because background layout is off. */
	revealing = YES;
	[textView scrollRectToVisible:NSMakeRect(used.origin.x, used.origin.y, 1.0, MIN(NSHeight(used), NSHeight(visible)))];
	revealing = NO;
}

/* Layout works in container coordinates, which start below the top inset.
   The first line is the only one placed with its inset showing above it; any
   other line goes right at the top edge, which is also where the text view's
   own minimal scroll leaves a line. */

static CGFloat NVScrollOffsetForLineTop(NSTextView *textView, CGFloat lineTop) {
	return lineTop <= 0.0 ? 0.0 : lineTop + [textView textContainerOrigin].y;
}

NSUInteger NVFirstVisibleCharIndex(NSTextView *textView) {
	NSLayoutManager *layout = [textView layoutManager];
	NSTextContainer *container = [textView textContainer];
	NSRect visible;
	NSPoint point;
	NSUInteger glyph;
	NSRange lineGlyphs;
	if (!textView || !layout || !container || ![[textView string] length])
		return NSNotFound;
	visible = [textView visibleRect];
	if (NSIsEmptyRect(visible))
		return NSNotFound;
	/* One point below the edge, so a line starting exactly there is the one found. */
	point = NSMakePoint(0, NSMinY(visible) - [textView textContainerOrigin].y + 1.0);
	if (point.y < 0)
		point.y = 0;
	[layout ensureLayoutForBoundingRect:NSMakeRect(0, point.y, NSWidth(visible), 1.0) inTextContainer:container];
	glyph = [layout glyphIndexForPoint:point inTextContainer:container];
	if (glyph >= [layout numberOfGlyphs])
		return NSNotFound;
	if (NSIsEmptyRect([layout lineFragmentRectForGlyphAtIndex:glyph effectiveRange:&lineGlyphs]))
		return NSNotFound;
	return [layout characterIndexForGlyphAtIndex:lineGlyphs.location];
}

BOOL NVScrollCharIndexToTop(NSTextView *textView, NSUInteger charIndex) {
	NSLayoutManager *layout = [textView layoutManager];
	NSTextContainer *container = [textView textContainer];
	NSScrollView *scroll = [textView enclosingScrollView];
	NSClipView *clip = [scroll contentView];
	NSUInteger glyph;
	NSRect line, bounds;
	if (!textView || !layout || !container || !clip)
		return NO;
	if (charIndex == NSNotFound || charIndex >= [[textView string] length])
		return NO;
	[layout ensureLayoutForCharacterRange:NSMakeRange(0, charIndex + 1)];
	glyph = [layout glyphIndexForCharacterAtIndex:charIndex];
	if (glyph >= [layout numberOfGlyphs])
		return NO;
	line = [layout lineFragmentRectForGlyphAtIndex:glyph effectiveRange:NULL];
	if (NSIsEmptyRect(line))
		return NO;
	bounds = [clip bounds];
	bounds.origin.y = NVScrollOffsetForLineTop(textView, NSMinY(line));
	[clip scrollToPoint:[clip constrainBoundsRect:bounds].origin];
	[scroll reflectScrolledClipView:clip];
	return YES;
}

void NVScrollRangeIntoView(NSTextView *textView, NSRange range) {
	NSLayoutManager *layout = [textView layoutManager];
	NSTextContainer *container = [textView textContainer];
	NSScrollView *scroll = [textView enclosingScrollView];
	NSClipView *clip = [scroll contentView];
	NSUInteger length = [[textView string] length];
	NSUInteger charIndex, glyph;
	NSRect line, bounds;
	if (!textView || !layout || !container || !clip || !length || range.location == NSNotFound)
		return;
	charIndex = MIN(range.location, length - 1);
	[layout ensureLayoutForCharacterRange:NSMakeRange(0, charIndex + 1)];
	glyph = [layout glyphIndexForCharacterAtIndex:charIndex];
	if (glyph >= [layout numberOfGlyphs])
		return;
	line = [layout lineFragmentRectForGlyphAtIndex:glyph effectiveRange:NULL];
	/* A caret after a final line break sits on the empty line below it. */
	if (range.location >= length && !NSIsEmptyRect([layout extraLineFragmentRect]))
		line = [layout extraLineFragmentRect];
	if (NSIsEmptyRect(line))
		return;
	bounds = [clip bounds];
	if (NVScrollOffsetForLineTop(textView, NSMinY(line)) < NSMinY(bounds))
		bounds.origin.y = NVScrollOffsetForLineTop(textView, NSMinY(line));
	else if (NSMaxY(line) + [textView textContainerOrigin].y > NSMaxY(bounds))
		bounds.origin.y = NSMaxY(line) + [textView textContainerOrigin].y - NSHeight(bounds);
	else
		return;
	[clip scrollToPoint:[clip constrainBoundsRect:bounds].origin];
	[scroll reflectScrolledClipView:clip];
}

NSRange NVScrollToShowNearestRange(NSTextView *textView, NSArray *ranges) {
	NSLayoutManager *layout = [textView layoutManager];
	NSClipView *clip = [[textView enclosingScrollView] contentView];
	NSUInteger length = [[textView string] length];
	NSRect visible;
	CGFloat originY, best = CGFLOAT_MAX;
	NSRange nearest = NSMakeRange(NSNotFound, 0);
	NSValue *value;
	if (!textView || !layout || !clip || !length || ![ranges count])
		return nearest;
	visible = [clip bounds];
	originY = [textView textContainerOrigin].y;
	for (value in ranges) {
		NSRange range = [value rangeValue];
		NSUInteger charIndex, glyph;
		NSRect line;
		CGFloat distance;
		if (range.location == NSNotFound)
			continue;
		charIndex = MIN(range.location, length - 1);
		[layout ensureLayoutForCharacterRange:NSMakeRange(0, charIndex + 1)];
		glyph = [layout glyphIndexForCharacterAtIndex:charIndex];
		if (glyph >= [layout numberOfGlyphs])
			continue;
		line = [layout lineFragmentRectForGlyphAtIndex:glyph effectiveRange:NULL];
		if (NSIsEmptyRect(line))
			continue;
		line.origin.y += originY;
		if (NSMinY(line) >= NSMinY(visible) && NSMaxY(line) <= NSMaxY(visible))
			return range;
		if (NSMaxY(line) <= NSMinY(visible))
			distance = NSMinY(visible) - NSMaxY(line);
		else if (NSMinY(line) >= NSMaxY(visible))
			distance = NSMinY(line) - NSMaxY(visible);
		else
			distance = 0.0;
		if (distance < best) {
			best = distance;
			nearest = range;
		}
	}
	if (nearest.location != NSNotFound)
		NVScrollRangeIntoView(textView, nearest);
	return nearest;
}
