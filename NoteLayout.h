#import <Cocoa/Cocoa.h>

/* Lays out every character and sizes the text view to the result. Setting a
   note's text does not do this, so the view stays as tall as the window and
   the scrollbar stays hidden. */
void NVLayOutEntireNote(NSTextView *textView);

/* A resize that arrives while the note is on screen. Keeps a note as tall
   as the last measurement that included its last line. */
CGFloat NVNoteHeightForProposedHeight(NSTextView *textView, CGFloat proposed);

/* The character that starts the first line showing in the view, or NSNotFound
   when the note is empty, not laid out, or not on screen. A line partly
   scrolled off the top still counts as showing. */
NSUInteger NVFirstVisibleCharIndex(NSTextView *textView);

/* Scrolls so the line holding charIndex sits at the top of the view. The note
   should already be laid out in full, or the scroll is clamped to the window.
   An index outside the note leaves the view alone and returns NO. */
BOOL NVScrollCharIndexToTop(NSTextView *textView, NSUInteger charIndex);

/* Scrolls the least distance that puts the line holding the start of range on
   screen, and nothing at all when it already is. Unlike the text view's own
   scrollRangeToVisible: it moves right away rather than after a later layout
   pass, so the position can be read back at once. */
void NVScrollRangeIntoView(NSTextView *textView, NSRange range);

/* When none of the ranges (NSValues) has its line on screen, scrolls the least
   distance that shows the one nearest the view. Nothing moves when one is
   already showing or the array is empty. */
void NVScrollToShowNearestRange(NSTextView *textView, NSArray *ranges);
