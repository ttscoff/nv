# Note body line height

Date: 2026-11-24

## Problem

Editor themes have no line-height key, and the note body uses the default
paragraph line height. Users (and themes) should be able to loosen or tighten
body leading. Preview CSS already has `line-height`; this is for the editor.

## Goals

1. Optional `lineHeight` on a theme's `textView` object (float, 0.5…3.0).
2. Fonts & Colors control: Follow Theme, Small, Regular, Large, and Custom
   (editable float 0.5…3.0).
3. A prefs sentinel for Follow Theme so an explicit user value can override the
   theme, and clearing that override follows the theme again.

## Non-goals

- Per-style line height (headings, code blocks, etc.)
- Changing preview CSS `line-height`
- List view row height

## Preference

- Key: `NoteBodyLineHeight` (float in `NSUserDefaults`)
- **Sentinel:** `0` means Follow Theme (also the default when the key is absent)
- Explicit values are clamped to **0.5…3.0**
- Presets written to prefs:
  - Small: `0.9`
  - Regular: `1.0`
  - Large: `1.4`
  - Follow Theme: `0`
  - Custom: whatever the user enters (clamped)

## Resolution

Effective line height multiple:

1. If pref is Follow Theme (`0` / absent): use `textView.lineHeight` from the
   current editor theme when present and in 0.5…3.0; otherwise `1.0`
2. Else use the clamped pref value

Theme changes while on Follow Theme re-resolve and restyle. An explicit pref
does not change when the theme changes.

## Theme format

Under `textView` in `.theme` JSON:

```json
"textView": {
  "backgroundColor": "#fbfaf7",
  "foregroundColor": "#2c2825",
  "lineHeight": 1.35
}
```

`NVEditorTheme` exposes the resolved editor line height (or 0 if unset). Style
dictionaries do not gain a per-style lineHeight in this work.

## Application

- Apply as `NSParagraphStyle.lineHeightMultiple` on the shared note-body
  paragraph style used for note attributes / typing attributes.
- Apply for **all** body fonts, not only monospace (today paragraph style is
  mainly attached for mono tab stops; line height must still apply for
  proportional fonts).
- Changing the pref or switching themes triggers the same restyle / note
  redisplay path used when the body font changes.

## Fonts & Colors UI

Add controls near the body font row (same programmatic style as List Font):

- Popup or segmented control: **Follow Theme · Small · Regular · Large · Custom**
- Float text field for the multiple (0.5…3.0)
  - Choosing a preset updates the field
  - Choosing Follow Theme shows the *effective* theme/default value in the field
    (read-only or still editable; editing switches to Custom and writes a pref)
  - Editing the field selects Custom and stores the clamped value

## Tests

- Resolve helper: Follow Theme + no theme key → 1.0; Follow Theme + theme 1.35
  → 1.35; explicit 1.2 wins over theme; out-of-range clamped; sentinel 0
- Preset constants match Small / Regular / Large

## Success criteria

- Default install looks unchanged (effective 1.0)
- Follow Theme picks up `textView.lineHeight` when a theme sets it
- Explicit Small / Regular / Large / Custom override the theme until the user
  chooses Follow Theme again
- Changing line height updates the open note without relaunch
