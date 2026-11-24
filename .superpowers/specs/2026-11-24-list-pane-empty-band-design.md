# List pane empty band and empty-click deselection

Date: 2026-11-24

## Problem

With the notes list above the note body, a remembered list height can sit at the
maximum (600pt) while the body is crushed (~116pt). When a search leaves only a
few rows, most of the list pane is empty white space. A click in that space
deselects the note ("No Note Selected") and focuses Search. Scrolling cannot
fill the band because there is no more list content.

Confirmed by customer defaults:

- `"RBSplitView H centralSplitView" = "2 600 116"` (list-above layout)
- Empty click → deselection + search focus
- Scaled / large-text display makes the empty band more likely

## Goal

1. Clamp a restored or resized list so the note body always keeps a usable
   minimum height.
2. Clicks in the empty area below the last row must not clear the current
   selection.

## Non-goals

- Fixing "opens / clears search onto an old note"
- Fixing fractional cut-off of the first body line
- Making the split divider more visible
- Changing side-by-side list width except where the same clamp helper is
  naturally reused

## Approach

Pure helpers in `ListSplit.h` (same style as `NVShouldApplyDefaultListDimension`
and `ClickBelowText.h`), with call sites in `AppController` and
`NotesTableView`.

### Clamp

Add `NVClampedListDimension(savedDimension, splitSize, bodyMin)` that returns:

- `savedDimension` unchanged when it already leaves at least `bodyMin` for the
  other pane and stays within the list min/max (80…600)
- otherwise `min(saved, max(listMin, splitSize - bodyMin))` capped at list max

Use **bodyMin = 120** for the list-above (stacked) layout so a restored
`"2 600 116"` becomes a shorter list and a body of at least 120pt.

Call sites:

1. After `restoreState:` in `setupViewsAfterAppAwakened` (alongside the existing
   default-dimension logic)
2. When the window / split is resized in a way that would otherwise leave the
   body below `bodyMin` while the list is excepted from shrink
   (`splitView:wasResizedFrom:to:` / adjust path)

Side-by-side layout may reuse the same helper with width as `splitSize` if a
one-line call is obvious; stacked height is the required fix.

### Empty-list click

Add a small predicate, e.g. `NVShouldKeepSelectionForEmptyListClick(row, hasSelection)`,
true when `row < 0` and `hasSelection` is YES.

In `NotesTableView -mouseDown:`, after converting the event to a row (and after
the existing option-drag handling), if that predicate is true, return without
calling `[super mouseDown:event]` so AppKit does not clear the selection.

Unchanged:

- Clicks on a real row
- Option-drag export path
- Explicit `deselectAll:` and search / filtering paths that clear selection in
  `AppController`

### Tests

- Extend `tests/ListSplitTests.m` for clamp cases: maxed list in a short split,
  already-safe heights, min/max bounds, bodyMin enforcement
- Add a tiny test file (or extend ListSplit tests) for the empty-click
  predicate: no row + selection → keep; row present → do not intercept; no
  selection → do not intercept

## Success criteria

- Restoring `"2 600 116"` into a window whose split is shorter than 720pt yields
  a list height ≤ `splitHeight - 120` and a body ≥ 120pt
- Clicking below the last list row with a note selected leaves that note
  selected and does not focus Search solely because of that click
- Existing `ListSplitTests` for default-dimension application still pass
