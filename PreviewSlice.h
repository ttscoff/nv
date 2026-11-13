#import <Foundation/Foundation.h>
#import <string.h>
#import "MarkdownHighlight.h"
#import "PreviewSync.h"

/* A note past the highlighting limit is previewed as the lines around the
   caret, so Apex and the web view never see the whole file. Shorter notes
   stay whole. The window grows to a nearby open code fence so a slice does
   not start in the middle of one. */

enum {
	NVPreviewAroundCaretCharacters = 64 * 1024,
	NVPreviewFenceLookbackCharacters = 16 * 1024
};

static const NSTimeInterval NVPreviewLiveUpdateDelay = 0.05;
static const NSTimeInterval NVPreviewLargeNoteUpdateDelay = 0.6;

typedef struct {
	NSRange range;
	BOOL omittedBefore;
	BOOL omittedAfter;
} NVPreviewSlice;

static inline NSTimeInterval NVPreviewUpdateDelayForLength(NSUInteger length) {
	return NVMarkdownSyntaxHighlightEnabledForLength(length) ? NVPreviewLiveUpdateDelay : NVPreviewLargeNoteUpdateDelay;
}

static inline BOOL NVPreviewRangeIsFence(NSString *text, NSRange lineRange) {
	NSUInteger end = NSMaxRange(lineRange);
	NSUInteger index = lineRange.location;
	unichar fence;
	NSUInteger start;
	if (end > [text length])
		end = [text length];
	while (end > index && [[NSCharacterSet newlineCharacterSet] characterIsMember:[text characterAtIndex:end - 1]])
		end--;
	while (index < end && ([text characterAtIndex:index] == ' ' || [text characterAtIndex:index] == '\t'))
		index++;
	if (end - index < 3)
		return NO;
	fence = [text characterAtIndex:index];
	if (fence != '`' && fence != '~')
		return NO;
	start = index;
	while (index < end && [text characterAtIndex:index] == fence)
		index++;
	if (index - start < 3)
		return NO;
	while (index < end && ([text characterAtIndex:index] == ' ' || [text characterAtIndex:index] == '\t'))
		index++;
	while (index < end && [[NSCharacterSet alphanumericCharacterSet] characterIsMember:[text characterAtIndex:index]])
		index++;
	return index == end;
}

static inline NSUInteger NVPreviewFenceOpenerBefore(NSString *text, NSUInteger start, NSUInteger lookback) {
	NSUInteger length = [text length];
	NSUInteger origin;
	NSUInteger index;
	BOOL inside = NO;
	NSUInteger opener = start;
	if (!start || !lookback)
		return start;
	origin = start > lookback ? start - lookback : 0;
	index = origin;
	/* A line that began before the lookback is not evidence of an open fence. */
	if (index > 0 && index < start) {
		NSRange partial = [text lineRangeForRange:NSMakeRange(index, 0)];
		if (partial.location < index)
			index = NSMaxRange(partial);
	}
	while (index < start && index < length) {
		NSRange line = [text lineRangeForRange:NSMakeRange(index, 0)];
		if (NVPreviewRangeIsFence(text, line)) {
			inside = !inside;
			if (inside)
				opener = line.location;
		}
		if (NSMaxRange(line) <= index)
			break;
		index = NSMaxRange(line);
	}
	return inside ? opener : start;
}

static inline NVPreviewSlice NVPreviewSliceForCaret(NSString *text, NSUInteger caret, NSUInteger fullPreviewLimit, NSUInteger maxCharacters, NSUInteger fenceLookback) {
	NVPreviewSlice slice;
	NSUInteger length = [text length];
	NSUInteger start = 0;
	NSUInteger end = length;
	memset(&slice, 0, sizeof(slice));
	if (!text || !length || !maxCharacters || length <= fullPreviewLimit) {
		slice.range = NSMakeRange(0, length);
		return slice;
	}
	if (caret == NSNotFound || caret > length)
		caret = caret == NSNotFound ? 0 : length;
	if (length <= maxCharacters) {
		slice.range = NSMakeRange(0, length);
		return slice;
	}
	if (caret > maxCharacters / 2)
		start = caret - maxCharacters / 2;
	end = start + maxCharacters;
	if (end > length) {
		end = length;
		start = length - maxCharacters;
	}
	if (start > 0 && start < length) {
		NSRange line = [text lineRangeForRange:NSMakeRange(start, 0)];
		if (line.location < start && line.length <= maxCharacters)
			start = line.location;
	}
	if (end > 0 && end < length) {
		NSRange line = [text lineRangeForRange:NSMakeRange(end, 0)];
		if (line.location < end && line.length <= maxCharacters)
			end = NSMaxRange(line);
	}
	if (end < start)
		end = start;
	if (end > length)
		end = length;
	start = NVPreviewFenceOpenerBefore(text, start, fenceLookback);
	slice.range = NSMakeRange(start, end - start);
	slice.omittedBefore = start > 0;
	slice.omittedAfter = end < length;
	return slice;
}

/* 1-based line of the caret inside a slice, matching that slice's source positions. */
static inline NSUInteger NVPreviewCaretLine(NSString *sliceText, NSUInteger caret, NSRange sliceRange) {
	NSUInteger local = 0;
	NSUInteger length = [sliceText length];
	if (caret != NSNotFound && caret >= sliceRange.location) {
		local = caret - sliceRange.location;
		if (local > length)
			local = length;
	}
	return NVSourceLineForIndex(sliceText, local);
}

static inline NSString *NVPreviewHTMLMarkingOmittedEdges(NSString *html, BOOL omittedBefore, BOOL omittedAfter) {
	NSMutableString *marked;
	if (!omittedBefore && !omittedAfter)
		return html;
	marked = [NSMutableString string];
	if (omittedBefore)
		[marked appendString:@"<p class=\"nv-preview-omitted\">Showing the section around the cursor. Earlier text is not in the preview.</p>\n"];
	if ([html length])
		[marked appendString:html];
	if (omittedAfter)
		[marked appendString:@"\n<p class=\"nv-preview-omitted\">Later text is not in the preview.</p>"];
	return marked;
}
