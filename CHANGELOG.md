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

