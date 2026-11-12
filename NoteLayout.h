#import <Cocoa/Cocoa.h>

/* Lays out every character and sizes the text view to the result. Setting a
   note's text does not do this, so the view stays as tall as the window and
   the scrollbar stays hidden. */
void NVLayOutEntireNote(NSTextView *textView);

/* A resize that arrives while the note is on screen. Keeps a note as tall
   as the last measurement that included its last line. */
CGFloat NVNoteHeightForProposedHeight(NSTextView *textView, CGFloat proposed);
