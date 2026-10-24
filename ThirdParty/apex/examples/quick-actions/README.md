# Markdown to RTF Quick Action

macOS Quick Action (Service) that converts **selected Markdown** to rich text via Apex and pastes it in place (Mail, Pages, TextEdit, etc.).

## Why not “replace selected text”?

Automator shell stdout is plain text. Printing Apex RTF would paste literal `{\rtf1...}` source. This action instead:

1. Runs `apex -t rtf` on the selection
2. Puts the result on the pasteboard with `pbcopy` (auto-detects the RTF header)
3. Simulates **Cmd+V** via System Events

## Install

1. Install Apex: `brew tap ttscoff/thelab && brew install apex`
2. Double-click `Markdown-to-RTF.workflow.zip` (or copy `Markdown to RTF.workflow` into `~/Library/Services/`)
3. Open **System Settings → Privacy & Security → Accessibility** and allow **Automator** / **Script Editor** / your runner as prompted when you first use it
4. Optional: **System Settings → Keyboard → Keyboard Shortcuts → Services** and assign a hotkey to **Markdown to RTF**

## Use

1. Select Markdown in a compose field (e.g. Mail)
2. **Services → Markdown to RTF** (or your hotkey)
3. Selection is replaced with rich paste

## Bundle layout

```
Markdown to RTF.workflow/
  Contents/
    Info.plist
    document.wflow
```
