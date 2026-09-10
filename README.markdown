# nvALT

nvALT is a macOS notes app with editable syntax-highlighted source and read-only Markdown, Textile, HTML, and Org previews.
This fork of [ttscoff/nv](https://github.com/ttscoff/nv) adds multiple windows, fuzzy search, native macOS controls, and automatic backups.

![Full nvALT window with editable note source, hidden header rows, and its shadow against a neutral background](docs/screenshots/readme-source.png)

## What changes in this fork

| Area | Upstream nvALT | This fork |
| --- | --- | --- |
| Windows | One main notes window. | Multiple windows, one shared notes list. |
| Layout | The notes list is stacked or side-by-side panes. | The notes list is stacked. |
| Search | Exact match strings on note titles. | Fuzzy searches on note title and text. Exact remains available. |
| Appearance | Legacy window controls and color schemes. | Native macOS controls and a notes list that follows system light and dark modes. |
| Editor | Editable rich-text in Markdown, Textile, and HTML(?). | Editable source with syntax-highlighting and readonly rich-text previews  in MD, Textile, HTML, Org|

When migrating to this fork: Saved side-by-side layouts restore as stacked panes. The fork retains note links, tags, source import/export, and custom editor fonts.

### Multiple windows

Each window can show a different note or search. Edits to the same note appear in all windows that show that note. Undo and Redo share the history for that note, including committed title and tag edits.

The app restores open windows and their saved views after a restart. A change to the notes library applies to all windows.

![Two windows with independent searches and selections in one library](docs/screenshots/readme-windows.png)

### Search

Choose **Fuzzy** or **Exact** from the search-field menu.
Fuzzy matches characters in order, so `mtg` can match `meeting`. It searches titles, tags, and complete committed source text.
Double quotes require a contiguous phrase. Spaces and colons separate terms, and other punctuation remains literal.

Literal title matches appear first. The full fuzzy list follows in the order returned by [`dangduc/fzf-native`](https://github.com/dangduc/fzf-native).

![Fuzzy search results above the source editor](docs/screenshots/readme-search.png)

### Native controls and appearance

The title bar contains one search field beside the window controls.
Note commands remain available through menus and shortcuts.

The **View** menu can show or hide the notes list, title, tags, and Source/Preview controls.
These visibility preferences apply to all windows.
Hidden rows release space to the body. Showing the notes list restores each window's previous divider height.
**Note > Rename** and **Note > Tag** reveal hidden fields before editing.
Tags offer completion from the library.

The notes list follows system light and dark appearance.
The editor can follow the system appearance or use custom colors and fonts.
Choose **View > Color Schemes > Follow System Appearance** for system editor colors.
**User Scheme** uses separate custom light and dark colors and switches with the macOS appearance.
**Preferences > Fonts & Colors** contains both groups, each with search highlight, foreground text, and background colors.

![Dark appearance with a native search field and notes list](docs/screenshots/readme-dark.png)

All screenshots use disposable sample notes.
[Screenshot details](docs/screenshots/README.md) record the captured revision and environment.

## Use the app

| Action | Instruction |
| --- | --- |
| Open another window | Choose **Window > New Window**, or press **Command-Shift-N**. |
| Create a note | Press **Command-N**. If the search field has focus and contains text, that text becomes the title. |
| Find a note | Type in **Search or Create**. Use **Command-J** or **Command-K** to move through the results. |
| Edit a search result | Select the note. Then press **Return**. |
| Create from a search | If no note matches, press **Return** or click **Create**. |
| Edit the title | Choose **Note > Rename**. Press **Return** to commit, or **Escape** to cancel. |
| Edit tags | Choose **Note > Tag**. Press **Return** to commit, or **Escape** to cancel. |
| Resize the list | Drag the divider between the list and the editor. |
| Use system editor colors | Choose **View > Color Schemes > Follow System Appearance**. |
| Open a preview | Choose **Preview > Preview Format**, then select Markdown, Textile, HTML, or Org. |
| Return to editing | Choose **Preview > Show Source**. |
| Select source syntax | Choose **View > Syntax Type**, then select Plain Text, Markdown, Textile, HTML, JSON, or Org. |
| Show optional controls | Use the title, tag, and Source/Preview visibility commands in **View**. |
| Show or hide the notes list | Choose **View > Show Notes List** or **Hide Notes List**. |
| Manage backups | Open **Preferences > Backups**. |

## Automatic backups

Automatic backups run while nvALT is open, with a default 15-minute interval.
Unchanged libraries do not create duplicate automatic snapshots.
**Preferences > Backups** controls the destination, interval, and retention.
It also offers **Back Up Now**, **Show Backups in Finder**, and **Restore Backup…**.
A restore opens the backup in a new, empty folder and leaves the original library in place.
[Backup usage](docs/automatic-backups.md) describes snapshot contents, encryption, and recovery limits.

## Automated builds

The [macOS build workflow](https://github.com/dangduc/nv/actions/workflows/macos.yml) runs for pull requests to `master`, pushes to `master`, and manual runs.
It uses an Intel macOS 15 runner with Xcode 16.4.

1. Open a successful workflow run.
2. Download `nvALT-macos-x86_64-<run number>-<attempt>.zip` from **Artifacts**.
3. Extract `nvALT.app` from the ZIP file.

Downloads require a GitHub login. App archives expire after 30 days.
These are unsigned Development builds without notarization. Apple Silicon Macs require Rosetta.

Successful `master` builds create a `build-<run number>` tag at the built commit.
A rerun keeps the same tag. Pull requests and manual runs on other branches do not create tags.
Build tags do not change the app version or create GitHub Releases.

CI checks tag logic, native search behavior, and the app build. It also checks executable permissions in the archive.
The desktop integration suites remain separate. [Tests/CI/README.md](Tests/CI/README.md) describes the CI checks and tag rules.

## Dependency changes

This fork uses `NSJSONSerialization`, `NSDataDetector`, and an inline `WKWebView` preview.
Automatic updates and the Check for Updates menu items are disabled.

HTML files, web archives, and web-page downloads cannot be imported.
Pasted URLs remain plain text. Browser paste uses a plain-text representation when available.
The `nvalt://make` action accepts `txt`; its `html` and `url` import parameters are disabled.
You can type or paste HTML source and select the HTML viewer. Rendered HTML export remains available.

## Build and run

The build requires full Xcode. Command Line Tools alone are insufficient.
The bundled MultiMarkdown executable and OpenSSL archive require an Intel build. Apple Silicon Macs need Rosetta.

The command below passed on macOS 26.5.2 with Xcode 26.6 and the macOS 26.5 SDK. Other runtime versions need separate checks.

1. Clone this fork:

   ```sh
   git clone https://github.com/dangduc/nv.git
   cd nv
   ```

2. Build the Development app:

   ```sh
   xcodebuild -project Notation.xcodeproj -scheme 'Notation Develop' \
     -derivedDataPath build/DerivedData ARCHS=x86_64 \
     MACOSX_DEPLOYMENT_TARGET=10.13 CODE_SIGNING_ALLOWED=NO \
     GENERATE_PROFILING_CODE=NO OTHER_CFLAGS= WARNING_LDFLAGS= build
   ```

3. Quit any other nvALT build before the first run.
4. Run the app:

   ```sh
   open build/DerivedData/Build/Products/Development/nvALT.app
   ```

The build retains the upstream application identifier and can use existing nvALT settings and notes. The built-in updater is disabled. Updates to this fork require a new local build or CI artifact.

## Development checks

### Project layout

| Directory | Contents |
| --- | --- |
| `Sources/` | Application code, grouped by responsibility. Headers stay beside their implementations. |
| `Resources/` | Images, help, syntax queries, interfaces, and localized resources in `Localization/*.lproj/`. |
| `Config/` | Application plist, prefix header, and linker order files. |
| `ThirdParty/` | Bundled source dependencies, markup processors, and OpenSSL. |
| `Scripts/` | Development utilities. |
| `Tests/` | Cocoa integration suites, regression checks, CI checks, and review records. |
| `docs/` | Documentation assets and screenshots. |

`Notation.xcodeproj` stays at the repository root. Its navigator groups match the directories on disk.

### Run the suites

After a Development build, run these commands from an active desktop session:

```sh
python3 Tests/run-multiple-windows-tests.py
python3 Tests/run-regression-tests.py
```

The suites use temporary notes and a copy of the app. They cover shared edits, Undo/Redo, window restoration, search, metadata, preview ownership, and appearance.

[Tests/README.md](Tests/README.md) describes focused checks and full-screen checks. [The review record](Tests/NativeUIReview/VALIDATION.md) lists results and limits. External editor apps need separate manual checks.

## Source and preview

New notes start as editable Plain Text. The syntax menu enables Tree-sitter highlighting for Markdown, HTML, JSON, and Org.

Org source highlights headings, tags, properties, lists, checkboxes, timestamps, links, tables, blocks, default TODO/DONE keywords, and ordinary emphasis.
Importing an `.org` file selects Org syntax and preserves its source encoding.
Existing file libraries recognize `.org` files without changing their selected output extension.
Org web links open their URL. File, ID, and heading targets remain source text without an action.
Org syntax does not provide agenda views, folding, task commands, or embedded-language highlighting.
Textile source supports markup insertion commands with plain display.
Simplenote support is removed. Existing local notes remain available.
Syntax settings stay in the local library. They are independent of each window's preview format.

Each window can edit Source or show a read-only preview of the same note.
Switching modes retains the source, Undo, caret, and scroll position. Existing composition commits only in the editor being hidden.
Markdown preview uses MultiMarkdown. Org preview uses a bundled Orgize converter.
Preview and Save HTML use the same rendered result.

Org preview shows headings, emphasis, task labels, lists, tables, links, and literal code or example blocks.
Code stays text, and include directives remain visible without loading other files.
Choose **Preview > Preview Format > Org** to view any note as Org, independently of its source syntax.
The converter does not require an installed Rust toolchain, Pandoc, or Emacs.
Custom Emacs export settings, macros, footnotes, and agenda views are outside this viewer's scope.

![Read-only Markdown preview in the same window](docs/screenshots/readme-preview.png)

Preview offers selection, Copy, Find, and HTML export. Printing requires macOS 11 or later.

Notes can use a single database or separate plain-text files.
Plain-text paste and imports preserve source characters, whitespace, and line endings.
Text imports retain their original bytes, encoding, and byte-order mark where possible.
An edit that cannot use the original encoding offers UTF-8 conversion.

Rich-text notes, rich-text import/export, detached previews, sticky previews, sharing, and custom templates are no longer supported.
The viewer blocks active note scripts and remote resources. Passive local assets can load from the note's directory.
Quick Look previews are not included.

## Contribute and credits

[architecture.md](architecture.md) explains controller ownership, shared editing, storage, and window lifecycle.

[AGENTS.md](AGENTS.md) describes the source layout, coding conventions, and pull request requirements. Reports about this fork belong in [dangduc/nv issues](https://github.com/dangduc/nv/issues).

nvALT comes from Brett Terpstra and David Halter. It builds on Zachary Schneirov's [Notational Velocity](https://github.com/scrod/nv) and [DivineDominion's MultiMarkdown fork](https://github.com/DivineDominion/nv).

The repository includes the [GNU General Public License, version 3](COPYING.txt). Bundled components retain their own license notices.
