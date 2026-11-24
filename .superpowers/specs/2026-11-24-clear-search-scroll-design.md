# Clear search scrolls to oldest notes

Date: 2026-11-24

## Problem

Clearing search (clear button / Escape → `cancelOperation:`) can leave the notes
list scrolled to the oldest end. Default sort is Date Modified newest-first, so
that looks like “jumped to an old note.”

## Cause

1. `cancelOperation:` deselects, then `filterNotesFromString:@""`.
2. `notationListMightChange:` records `viewingLocation` with **no selection**.
3. With no selection, `viewingLocation` falls back to the last row of the short
   filtered list and sets `pivotRowWasEdge = YES`.
4. After the full list reloads, `setViewingLocation:` edge-snaps that pivot to
   the nearer end of the full list — often `lastRow` (oldest).

Sort/layout paths already clear `pivotRowWasEdge`. Clear does not.

## Fix

1. **`viewingLocation`:** when nothing is selected, do not record a pivot
   (`nonRetainedPivotObject = nil`, `pivotRowWasEdge = NO`) so
   `setViewingLocation:` is a no-op.
2. **`cancelOperation:`:** after refiltering, `[notesTableView scrollRowToVisible:0]`
   so the full list lands at the top (newest under the default sort).

## Non-goals

- Changing sort order or launch restore
- Changing the typing-clear path (`controlTextDidChange` with empty string),
  which already skips viewing-location restore via `isFilteringFromTyping`

## Tests

Pure helper: `NVShouldRecordListPivot(selectedCount)` — record only when
`selectedCount > 0`. Extend or add a small test file next to `ListSplitTests`.
