# Note Body Line Height Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Let themes set `textView.lineHeight` and let Fonts & Colors choose Follow Theme / Small / Regular / Large / Custom for note body leading.

**Architecture:** Pure resolve helpers in `NoteLineHeight.h`; prefs store a Follow Theme sentinel (`0`); `GlobalPrefs` applies `lineHeightMultiple` on the body paragraph style for all fonts; `NVEditorTheme` parses theme line height; Prefs UI mirrors the List Font controls pattern.

**Tech Stack:** Objective-C, Cocoa, existing clang `tests/*.m` helpers, `.theme` JSON.

## Global Constraints

- Pref key `NoteBodyLineHeight`; sentinel `0` = Follow Theme
- Explicit multiples clamped to 0.5…3.0
- Presets: Small 0.9, Regular 1.0, Large 1.4
- Default effective height 1.0 when following theme with no key
- Apply via `NSParagraphStyle.lineHeightMultiple` for all body fonts

## Files

- Create: `NoteLineHeight.h`, `tests/NoteLineHeightTests.m`
- Modify: `NVTheme.h` / `NVTheme.m`, `GlobalPrefs.h` / `GlobalPrefs.m`, `PrefsWindowController.m` / `.h`, `AppController.m` (callback for restyle), optionally `Paper.theme` (leave unset)

---

### Task 1: Resolve helpers + tests

**Files:** Create `NoteLineHeight.h`, `tests/NoteLineHeightTests.m`

**Interfaces:**
- `NVNoteLineHeightFollowTheme` = `0.0`
- `NVNoteLineHeightSmall` = `0.9`, `Regular` = `1.0`, `Large` = `1.4`
- `CGFloat NVClampNoteLineHeight(CGFloat value);` — clamp to 0.5…3.0
- `CGFloat NVResolvedNoteLineHeight(CGFloat prefOrSentinel, CGFloat themeOrZero);` — sentinel → theme if in range else 1.0; else clamped pref
- `BOOL NVNoteLineHeightIsFollowTheme(CGFloat pref);`

- [ ] Write failing tests, run (compile fail)
- [ ] Implement header, run pass
- [ ] Commit

---

### Task 2: Theme `textView.lineHeight`

**Files:** `NVTheme.h`, `NVTheme.m`

- Parse `lineHeight` from `textView` dict into ivar (0 if absent/invalid)
- Expose `- (CGFloat)editorLineHeight;` — raw theme value, 0 if unset
- Commit

---

### Task 3: GlobalPrefs storage + paragraph style

**Files:** `GlobalPrefs.h`, `GlobalPrefs.m`, `AppController.m`

- `noteBodyLineHeight` / `setNoteBodyLineHeight:sender:` (stores sentinel or clamped)
- `effectiveNoteBodyLineHeight` using current theme from `NVThemeManager`
- Invalidate paragraph style + attributes on set; include `lineHeightMultiple` for all fonts in `noteBodyAttributes`
- Register callback selector like body font; AppController restyles notes + updates typing attributes
- Theme change while following theme also restyles (hook existing theme apply path)
- Commit

---

### Task 4: Fonts & Colors UI

**Files:** `PrefsWindowController.m` / `.h`

- Add line-height popup + float field below body font (shift layout like List Font)
- Tags e.g. 7320 popup, 7321 field
- Sync UI from prefs; Follow Theme shows effective value; edit → Custom
- Commit

---

### Task 5: Smoke verification

- Run `NoteLineHeightTests`
- Manual: Follow Theme, Large, Custom 1.8, switch theme
