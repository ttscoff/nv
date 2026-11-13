nvALT 3.0.39 (144)
-------------------------

#### CHANGED

- The **search field** keeps the text you typed when you select a note

nvALT 3.0.38 (144)
-------------------------

#### CHANGED

- **Syntax highlighting** turns off past about 300KB so a long note stays responsive while you type
- Notes past 300KB skip **list renumbering** and code-fence closing, and the word count stays blank

#### NEW

- **Code blocks** in the preview are syntax colored with a light or dark GitHub palette chosen from the background

#### IMPROVED

- **Syntax highlighting** below that size updates the lines you edit instead of the whole note

#### FIXED

- A long note keeps the **theme text color** when highlighting is off, including characters you type
- Pressing **return** at the end of a long note no longer hangs
- A long note's **scrollbar** stays available instead of being hidden or painted over

nvALT 3.0.37 (144)
-------------------------

#### NEW

- A **Debug** pane writes errors and warnings to Console, and can include every diagnostic message or go quiet

#### FIXED

- Deleting a note no longer leaves the **notes list** empty or showing a random handful of notes
- A one-character note shows its **preview** in the notes list
- **Note previews** keep Japanese text, emoji, and accented characters whole

nvALT 3.0.36 (144)
-------------------------

#### NEW

- **Highlight syntax** (==text==) is colored in the editor from the theme, or as black text on yellow
- = wraps a **selection** as ==selection== and ~ wraps it as ~~selection~~, and neither key pairs when nothing is selected
- **CriticMarkup** is colored from the theme, or in red, green, yellow, and cyan when a theme leaves it unset
- **Deletions** are crossed out, **insertions** are underlined, and a substitution shows the old text crossed out and the new text underlined

nvALT 3.0.35 (144)
-------------------------

#### CHANGED

- The notes list, the editor, and the preview use the system **scroll bars**
- **Use nvALT Scrollbars** is gone from Settings
- **Search highlights** stay while switching notes and clear when the editor takes focus

#### FIXED

- **Scroll bars** keep a single knob instead of leaving copies behind while scrolling
- **Search field** follows the window when the title bar is still the width from when the window was created
- **Share button** stays on the right edge as the window grows
- Moving the **selection** no longer leaves a hairline on a fractional line
- **Search highlights** on a dark note keep their color instead of turning into a gray bar
- Matched text sits on the **highlight color** instead of black letters on the note background
- Letters return to the **theme color** when they stop matching the search

nvALT 3.0.34 (144)
-------------------------

#### CHANGED

- **View Source** is now a `</>` icon, and the sync toggle sits to its left
- Removed **Open in Marked** from the preview toolbar. Share to Marked from the share menu instead

#### NEW

- **Share** button in the title bar sends the selected text, or the whole note when nothing is selected
- **Share Preview** sends the rendered note as rich text so other notes apps can paste it into the note
- **Preview sync** keeps the preview on the block you are editing, and only scrolls when that block leaves the screen

#### FIXED

- **Search field** right edge follows the window when a tiled window is resized

nvALT 3.0.33 (144)
-------------------------

#### CHANGED

- **Search field** starts just after the window title in side-by-side layout, the same way it does in vertical layout

#### FIXED

- **Search field** stays inside the window when the window is tiled or made very narrow

nvALT 3.0.32 (144)
-------------------------

#### FIXED

- **Search highlights** clear when the field shows the note title, and clearing the search removes them
- **Search highlights** use dark or light text so the marked words stay readable
- **Tab** and **Backtab** on a task checkbox indent the item instead of inserting link brackets

nvALT 3.0.31 (144)
-------------------------

#### NEW

- **Option-Command-T** turns the current line into a task. A list item gains a checkbox, and a line that already has one loses it
- Click the space inside a task's brackets to mark it **done** or open again
- A completed task is **crossed out** in the editor, and the preview shows it as a checked item
- **Return** on a task continues the list with a new open task, and Return on an empty checkbox clears that line

nvALT 3.0.30 (144)
-------------------------

#### NEW

- **Option-Command-T** turns the current line into a task. A list item gains a checkbox, and a line that already has one loses it
- Click the space inside a task's brackets to mark it **done** or open again
- A completed task is **crossed out** in the editor, and the preview shows it as a checked item
- **Return** on a task continues the list with a new open task, and Return on an empty checkbox clears that line

nvALT 3.0.29 (144)
-------------------------

#### FIXED

- Selected notes in **Vertical Layout** with the **Default** theme use the system selection bar again instead of white text on the list background

nvALT 3.0.28 (144)
-------------------------

#### NEW

- **Enter in list renames note** in Editing settings stays on by default. Turn it off and Enter in the notes list jumps to the editor

#### FIXED

- Collapsing the **notes list** in Vertical Layout no longer leaves it visible behind the search field
- **Convert to Markdown** in Notes settings stays below the deletion checkbox instead of covering its title

nvALT 3.0.27 (144)
-------------------------

#### FIXED

- **Vertical Layout** notes list uses the theme background, selection, and text color
- **Search field** in Vertical Layout stays in place and leaves the window title visible
- **Vertical Layout** selected rows use the theme highlight instead of the system blue bar
- Selected **tags** in Vertical Layout still use the selected style

nvALT 3.0.26 (144)
-------------------------

#### FIXED

- **Vertical Layout** notes list uses the theme background, selection, and text color
- **Search field** in Vertical Layout stays in place and leaves the window title visible

nvALT 3.0.25 (144)
-------------------------

#### FIXED

- Editing a note no longer jumps the **preview** back to the top or flashes on each keystroke
- Links to **anchors** in the same note scroll the preview

nvALT 3.0.24 (144)
-------------------------

#### FIXED

- Opening **Settings** on **General** shows its controls when that pane was open at quit

nvALT 3.0.23 (144)
-------------------------

#### FIXED

- Clicking **below the last line** places the caret at the end of the note, including a trailing newline, a blank line that contains spaces, and a last line of text

nvALT 3.0.22 (144)
-------------------------

#### NEW

- **Editing** settings choose the marker for **bold** (`**` or `__`), **italic** (`*` or `_`), and **unordered lists** (`-`, `*`, or `+`)
- **Link ids** complete inside `[text][id]` from definitions already in the note, and a new `[id]:` line offers ids that are still unused
- **Backticks** wrap a selection or the caret, the next backtick types over the closer, and a third backtick stays single so ``` is a fence
- **Wiki links** in the preview open the named note. The brackets stay visible, and the same text in code, math, or a diagram stays plain
- Defined **reference links** pick up the link color, including collapsed references and images. Undefined ids and fenced code stay plain

#### IMPROVED

- **Command-Option-L** on a multiline selection adds that list marker, turns a solid bullet list into numbers, clears a numbered list, and strips markers when only some lines have one. Indents stay put

#### FIXED

- **Autocomplete** text, including the letters already typed and the brackets inside a link, stays the theme foreground instead of turning black
- **Word count** in the toolbar stays readable on dark themes instead of drawing as a light cyan token
- Opening a shorter note, including from a **note link**, no longer leaves the previous note's tail on screen

nvALT 3.0.21 (144)
-------------------------

#### NEW

- **Link ids** complete inside `[text][id]` from definitions already in the note, and a new `[id]:` line offers ids that are still unused

#### FIXED

- **Convert to Markdown** writes the notes instead of stopping before the first file
- **Convert to Markdown** switches to the new folder and reports when it finishes

nvALT 3.0.20 (144)
-------------------------

#### NEW

- **Preview** typesets math with MathJax, including `$...$`, `$$...$$`, `\(...\)`, and `\[...\]`
- **Preview** draws a fenced Mermaid diagram

#### IMPROVED

- **Math and Mermaid** are bundled in the app and load only when a note uses them

#### FIXED

- **Convert to Markdown** writes the notes instead of stopping before the first file. Thanks to Don Patterson (djp3).
- **Convert to Markdown** switches to the new folder and reports when it finishes. Thanks to Don Patterson (djp3).

nvALT 3.0.19 (144)
-------------------------

#### FIXED

- **#tag search** matches a tag prefix or the same #tag written in the title, body, or labels
- **Search highlights** mark a #tag in the note again

nvALT 3.0.18 (144)
-------------------------

#### NEW

- **Import URL** opens with Shift-Command-U while nvALT is focused.

nvALT 3.0.17 (144)
-------------------------

#### NEW

- **Import URL** opens with Shift-Command-U while nvALT is focused.

nvALT 3.0.16 (144)
-------------------------

#### NEW

- **Safari extension** is included with nvALT. Turn it on in Safari Settings.
- Add a built-in Markdownifier, not reliant on Python scripts

#### FIXED

- **Safari extension** includes its toolbar icons and uses the app version.

nvALT 3.0.15
-------------------------

#### CHANGED

- **URL import** converts HTML inside the app with HTML2Text and Readability, following the Convert imported URLs and Process with Readability preferences.

#### NEW

- **Import URL** on the Note menu asks for an address and creates a note. A copied web address fills the field, and holding Option turns on Readability for that import.
- **markdownify** URL scheme (`nvalt://` and `nv://`) turns a page URL or raw HTML into a Markdown note, with an optional title and Readability override. It converts even when Convert imported URLs to Markdown is off.

#### FIXED

- **Imported URL** is selected right away. The search field is set to the new note's title, so the list no longer looks empty until the search is cleared.
- **Option-drag** onto the notes list forces Readability for that import.

nvALT 3.0.14
-------------------------

#### CHANGED

- **Search highlighting** skips #tag words and still marks the other words in the query.
- **Search field** corners are rounded, and the field lines up with the note editor and grows from the right when the window resizes.
- **Selected note** stays highlighted while its tags are being edited.
- **Svelte** theme uses dark text on the highlighted and inactive note rows.

#### NEW

- **Tag search** in the search field. #unix lists notes whose tags start with unix, and mixes with normal search words.
- **Tag tokens** in the notes list and the multi-note panel. Tags on every selected note are solid chips, and tags on only some notes stay dimmed until you apply them to the rest.

#### FIXED

- **Tag search** after a lone #. Typing on to #unix lists tagged notes again.
- **Tag search** while typing a second tag. #unix #shell keeps notes that have both tags.
- **Tag editing** text stays readable on a dark editor theme.
- **Notes list** does not jump to another note while you are editing that row's tags.

nvALT 3.0.13
-------------------------

#### CHANGED

- **Search highlighting** skips #tag words and still marks the other words in the query.
- **Search field** corners are rounded, and the field lines up with the note editor and grows from the right when the window resizes.
- **Selected note** stays highlighted while its tags are being edited.
- **Svelte** theme uses dark text on the highlighted and inactive note rows.

#### NEW

- **Tag search** in the search field. #unix lists notes whose tags start with unix, and mixes with normal search words.
- **Tag tokens** in the notes list and the multi-note panel. Tags on every selected note are solid chips, and tags on only some notes stay dimmed until you apply them to the rest.

#### FIXED

- **Tag search** after a lone #. Typing on to #unix lists tagged notes again.
- **Tag search** while typing a second tag. #unix #shell keeps notes that have both tags.
- **Tag editing** text stays readable on a dark editor theme.
- **Notes list** does not jump to another note while you are editing that row's tags.

nvALT 3.0.13
-------------------------

#### CHANGED

- Pasted **URLs** stay plain text unless **Enable Markdown Completion** is on.
- **Paste Markdown Link** is hidden unless **Enable Markdown Completion** is on.

nvALT 3.0.12
-------------------------

nvALT 3.0.11
-------------------------

#### FIXED

- A leftover **search** no longer highlights part of what you typed after the field shows the note title.
- **Escape** in the editor clears that search.

nvALT 3.0.10
-------------------------

#### FIXED

- **Fonts & Colors** keeps the top controls on screen and places the theme options under the note width slider.

nvALT 3.0.9
-------------------------

#### FIXED

- **Theme links** import a .theme file when the server sends it as plain text.
- A failed **theme download** reports the server error.

nvALT 3.0.8
-------------------------

#### NEW

- Choose an **editor theme** and a **preview theme** in Settings, Fonts and Colors, with Import and Open Themes Folder.
- Import **MultiMarkdown Composer** themes and **Marked** preview stylesheets.
- Install a theme from a link with **nvalt://add-theme** or **nv://add-theme**.
- The **notes list** uses the editor theme's table colors, and the note body keeps the text view colors.
- A preview theme wraps the note in **#wrapper** so Marked CSS applies directly.
- A sample **Paper** editor theme and preview stylesheet are copied into the themes folder.
- An editor theme's **window background** colors the title bar, the divider, and the scrollbars.
- The **scrollbar knob** uses the window foreground color when it stays visible against the track.

#### FIXED

- With no editor theme, that chrome follows system **light and dark** mode.
- Margins around a **readable note width** use the same window color.

nvALT 3.0.7
-------------------------

#### CHANGED

- Open **Keyboard Shortcuts** in its own window.

#### NEW

- Insert a **Markdown link** with Option-Command-K.
- Paste a **URL** as a Markdown link when text is selected, or as an autolink when it is not.
- Cycle the **list marker** with Option-Command-L when automatic list formatting is on. Lock Note to Preview no longer uses that shortcut.
- Indent or outdent a **list item** or blockquote with Tab and Shift-Tab, and indent lines with Control-Command-Left and Control-Command-Right.
- Continue a **blockquote** or code fence when you press Return, and leave the quote when the line is empty.
- Renumber a **numbered list** as you edit it, when automatic list formatting is on.
- Move a **paragraph** or list item with Control-Command-Up and Control-Command-Down.
- Expand or shrink the **selection** by Markdown structure with Option-Command-Up and Option-Command-Down.
- Insert or jump to a **reference link** with F6, and a footnote with F7.
- Highlight **invisible characters** and unusual spaces in a note.

#### IMPROVED

- Color **nested list** markers while Markdown highlighting is on.
- Indent using the **tab width** from settings.

#### FIXED

- Scroll to show the **cursor** when F6 or F7 jumps to a reference or footnote.

nvALT 3.0.6
-------------------------

#### CHANGED

- The default **note font** is SF Pro at 16 pt. A font you already chose stays as it is.
- Tell you to paste the **login code** and press Return.

#### NEW

- Set the **note list** font from Fonts & Colors. It starts as SF Pro at 14 pt.

#### IMPROVED

- Leave more room above the first line of a **note**.

#### FIXED

- Open the **notes folder** picker when the saved folder is gone.
- Keep **alternating rows** lined up with the grid.
- Keep the **selected note** readable on a light list in Dark Mode when the editor has focus.
- Submit a **Simplenote login code** once, when you press Return.
- Say when a **login code** has expired so you can email yourself a new one.

nvALT 3.0.5
-------------------------

#### CHANGED

- Sign in to **Simplenote** with a code emailed to you instead of a password.
- The default **note font** is SF Pro at 16 pt. A font you already chose stays as it is.

#### NEW

- Set the **note list** font from Fonts & Colors. It starts as SF Pro at 14 pt.

#### IMPROVED

- Leave more room above the first line of a **note**.

#### FIXED

- Put **Simplenote** sync back, including the Synchronization tab in Notes settings.
- Warn again when **Simplenote** sync is on and notes are stored in Dropbox or SimpleText.
- Open the **notes folder** picker when the saved folder is gone.
- Keep **alternating rows** lined up with the grid.

nvALT 3.0.4
-------------------------

#### CHANGED

- Sign in to **Simplenote** with a code emailed to you instead of a password.

#### FIXED

- Put **Simplenote** sync back, including the Synchronization tab in Notes settings.
- Warn again when **Simplenote** sync is on and notes are stored in Dropbox or SimpleText.

nvALT 3.0.3
-------------------------

#### CHANGED

- List **Settings** panes in a sidebar and show the selected pane beside them.
- Rename the **Preferences** menu item to Settings.

#### IMPROVED

- Size the **Settings** window so each pane's options fit without collapsing into a chevron.

nvALT 3.0.2
-------------------------

#### CHANGED

- List **Settings** panes in a sidebar and show the selected pane beside them.
- Rename the **Preferences** menu item to Settings.

#### IMPROVED

- Size the **Settings** window so each pane's options fit without collapsing into a chevron.

nvALT 3.0.1
-------------------------

#### CHANGED

- NvALT now requires **macOS 12** or later.
- **Check for Updates** works again on current macOS, using Sparkle 2 and the existing designheresy feed.
- **MultiMarkdown preview** is rendered by Apex instead of the bundled multimarkdown tool.
- Preview **Share** publishes the note to Marked Share instead of Peggd and copies an unlisted link.
- **Note text and the note list** use the regular system font size. Existing Helvetica 12 notes and the old 11 or 12 point list size are updated once.
- **Note list headers** are flat, with a hairline separator and a chevron for the sorted column.
- The **search field** no longer draws its old horizontal border lines.
- The version is **3.0.0** (build 129).
- Remove **Simplenote** sync and the Synchronization tab from Notes settings.
- Replace the preview **Save HTML** button with **Open in Marked** for plain text notes stored on disk.
- Render the preview with Apex **Unified** Markdown.
- Turn on **Markdown** shortcuts by default, so Command-B and Command-I insert syntax instead of rich text.
- Use the new **app icon**, including the Icon Composer icon on current macOS and a generated icon for older systems.
- Store new note databases as **plain text** files instead of a single database. Existing database-only libraries stay as they are.
- Use **md** as the default extension for plain text notes, including libraries that were using txt, text, or utf8. A custom extension is left as the default.
- Store note **tags** as Finder tags. Existing OpenMeta tags on note files are copied over on launch, and later edits stay in Finder.
- Revise the **app icon** fill and the placement of the pages and pen.

#### NEW

- **Open Marked Streaming Preview** sends the current note to Marked and opens x-marked://stream/. It is in the Preview menu and the status menu.
- Highlight **Markdown** syntax in the editor while Markdown completion is on.
- Ask once, on launch, whether to convert a **Rich Text** library to Markdown.
- Add **Convert to Markdown** in Notes settings. Rich Text notes are converted, plain text notes are copied as they are, and the files are written into a new folder that then becomes the notes folder. The current folder is left unchanged.

#### IMPROVED

- NvALT runs natively on **Apple silicon** as well as Intel.
- **Web links** in notes are found by the system link detector. Double-bracket wiki links still work.
- **Encrypted notes** still open. Crypto now uses the system libraries instead of the bundled OpenSSL.

#### FIXED

- **Typing in a note** no longer crashes if the text gets shorter during an edit.
- The **password generator** no longer stalls when the length is longer than the character set.
- Make the **Marked Share** confirmation follow light and dark mode and show its buttons.
- Pressing **Return** on an empty continued list item removes the marker and leaves a blank line.
- Show the **menu bar icon** when that setting is on, using the rocket image instead of the app icon.

# Changelog

