# List Pane Empty Band Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Clamp restored/resized list pane height so the note body keeps ≥120pt, and keep selection when clicking empty list area.

**Architecture:** Pure helpers in `ListSplit.h`; wire clamp into `AppController` after split restore and on window resize; wire empty-click guard into `NotesTableView -mouseDown:`.

**Tech Stack:** Objective-C, Foundation/Cocoa, existing clang-compiled `tests/*.m` helpers.

## Global Constraints

- List min 80, list max 600 (existing `notesSubview` limits)
- Body minimum 120pt for stacked (list-above) layout
- Do not change intentional `deselectAll:` / search clear paths
- TDD: failing tests before production code

---

### Task 1: Clamp helper + tests

**Files:**
- Modify: `ListSplit.h`
- Modify: `tests/ListSplitTests.m`

**Interfaces:**
- Produces: `CGFloat NVClampedListDimension(CGFloat savedDimension, CGFloat splitSize, CGFloat bodyMin);`
- Produces: `static const CGFloat NVListBodyMinimumDimension = 120.0;`
- Produces: `static const CGFloat NVListMinimumDimension = 80.0;`
- Produces: `static const CGFloat NVListMaximumDimension = 600.0;`

- [ ] **Step 1: Write failing clamp tests** in `tests/ListSplitTests.m`

```objc
expectTrue(fabs(NVClampedListDimension(600.0, 716.0, 120.0) - 596.0) < 0.01,
	"a maxed list in a 716pt split leaves 120 for the body");
expectTrue(fabs(NVClampedListDimension(150.0, 800.0, 120.0) - 150.0) < 0.01,
	"a safe saved height is left alone");
expectTrue(fabs(NVClampedListDimension(600.0, 500.0, 120.0) - 380.0) < 0.01,
	"a short split clamps the list to split minus body min");
expectTrue(NVClampedListDimension(50.0, 800.0, 120.0) >= 80.0,
	"below the list minimum rises to 80");
expectTrue(NVClampedListDimension(700.0, 2000.0, 120.0) <= 600.0,
	"above the list maximum falls to 600");
```

- [ ] **Step 2: Run tests — expect fail** (undefined `NVClampedListDimension`)

```bash
clang -framework Foundation -I. -o /tmp/ListSplitTests tests/ListSplitTests.m && /tmp/ListSplitTests
```

- [ ] **Step 3: Implement helpers in `ListSplit.h`**

```objc
static const CGFloat NVListMinimumDimension = 80.0;
static const CGFloat NVListMaximumDimension = 600.0;
static const CGFloat NVListBodyMinimumDimension = 120.0;

static inline CGFloat NVClampedListDimension(CGFloat savedDimension, CGFloat splitSize, CGFloat bodyMin) {
	CGFloat limit = splitSize - bodyMin;
	if (limit < NVListMinimumDimension)
		limit = NVListMinimumDimension;
	CGFloat result = savedDimension;
	if (result > limit)
		result = limit;
	if (result > NVListMaximumDimension)
		result = NVListMaximumDimension;
	if (result < NVListMinimumDimension)
		result = NVListMinimumDimension;
	return result;
}
```

- [ ] **Step 4: Run tests — expect pass**

- [ ] **Step 5: Commit** clamp helper + tests

---

### Task 2: Empty-click predicate + tests

**Files:**
- Modify: `ListSplit.h`
- Modify: `tests/ListSplitTests.m` (same binary; keep one target)

**Interfaces:**
- Produces: `BOOL NVShouldKeepSelectionForEmptyListClick(NSInteger row, BOOL hasSelection);`

- [ ] **Step 1: Write failing predicate tests**

```objc
expectTrue(NVShouldKeepSelectionForEmptyListClick(-1, YES),
	"empty area with a selection keeps it");
expectTrue(!NVShouldKeepSelectionForEmptyListClick(0, YES),
	"a real row is not intercepted");
expectTrue(!NVShouldKeepSelectionForEmptyListClick(-1, NO),
	"empty area with no selection is not intercepted");
```

- [ ] **Step 2: Run — expect fail**

- [ ] **Step 3: Implement predicate in `ListSplit.h`**

```objc
static inline BOOL NVShouldKeepSelectionForEmptyListClick(NSInteger row, BOOL hasSelection) {
	return row < 0 && hasSelection;
}
```

- [ ] **Step 4: Run — expect pass**

- [ ] **Step 5: Commit**

---

### Task 3: Wire clamp into AppController

**Files:**
- Modify: `AppController.m` (`setupViewsAfterAppAwakened`, `splitView:wasResizedFrom:to:`)

**Interfaces:**
- Consumes: `NVClampedListDimension`, `NVListBodyMinimumDimension`

- [ ] **Step 1: After default-dimension block and before `[splitView adjustSubviews]` in `setupViewsAfterAppAwakened`, clamp:**

```objc
{
	CGFloat splitSize = [splitView isVertical] ? NSWidth([splitView frame]) : NSHeight([splitView frame]);
	CGFloat clamped = NVClampedListDimension([notesSubview dimension], splitSize, NVListBodyMinimumDimension);
	if (fabs(clamped - [notesSubview dimension]) > 0.5)
		[notesSubview setDimension:clamped];
}
```

- [ ] **Step 2: In `splitView:wasResizedFrom:to:`, after `adjustSubviewsExcepting:notesSubview`, apply the same clamp and `adjustSubviews` if changed**

- [ ] **Step 3: Build sanity — re-run ListSplitTests**

- [ ] **Step 4: Commit**

---

### Task 4: Wire empty-click into NotesTableView

**Files:**
- Modify: `NotesTableView.m` (`mouseDown:`)

**Interfaces:**
- Consumes: `NVShouldKeepSelectionForEmptyListClick`

- [ ] **Step 1: `#import "ListSplit.h"`**

- [ ] **Step 2: Before `[super mouseDown:event]`, guard empty clicks:**

```objc
NSPoint clickPoint = [self convertPoint:[event locationInWindow] fromView:nil];
NSInteger clickedRow = [self rowAtPoint:clickPoint];
if (NVShouldKeepSelectionForEmptyListClick(clickedRow, [self numberOfSelectedRows] > 0))
	return;
[super mouseDown:event];
```

- [ ] **Step 3: Re-run ListSplitTests**

- [ ] **Step 4: Commit**
