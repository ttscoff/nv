# Changelog

All notable changes to Apex will be documented in this file.

## [1.1.47] - 2026-10-20

### Improved

- Test coverage

### Fixed

- **LaTeX math** delimiters \(...\) and \[...\] render as inline and display math

## [1.1.46] - 2026-10-19

## [1.1.45] - 2026-10-19

## [1.1.44] - 2026-10-19

### Fixed

- **List item IALs** on the item's first line attach to that item, and an IAL on the following line still attaches to the list.

## [1.1.43] - 2026-10-19

### Fixed

- **Next-line IALs** attach to thematic breaks, definition lists, fenced divs, and raw HTML, and the marker is removed.
- **Blockquote IALs** on a quote line attach to the paragraph they follow, and an IAL on the next line without a quote marker attaches to the blockquote.
- **Multiline paragraph IALs** keep their class when the paragraph spans more than one line.
- **Image titles** can contain parentheses when the image also has attributes, and the title still becomes the caption.
- **Link titles** can contain parentheses without the destination ending at the parenthesis inside the title.

## [1.1.42] - 2026-10-19

### Fixed

- **Next-line IALs** attach to thematic breaks, definition lists, fenced divs, and raw HTML, and the marker is removed.
- **Blockquote IALs** on a quote line attach to the paragraph they follow, and an IAL on the next line without a quote marker attaches to the blockquote.
- **Multiline paragraph IALs** keep their class when the paragraph spans more than one line.
- **Image titles** can contain parentheses when the image also has attributes, and the title still becomes the caption.
- **Link titles** can contain parentheses without the destination ending at the parenthesis inside the title.

## [1.1.41] - 2026-10-16

### Fixed

- **CriticMarkup in code** stays literal in code spans, fenced blocks, and indented code blocks.
- **Short critic comments** and the text after a mark are no longer dropped when the HTML replacement outgrows the buffer.

## [1.1.40] - 2026-10-16

### Fixed

- **Headings after a table** no longer pick up header-cell text as attributes; a heading stays `<h3 id="tripod-issue">` instead of `<h3 Mechanism id="tripod-issue">`

## [1.1.39] - 2026-10-16

### Fixed

- **Image paths** that contain @, such as a @2x filename, stay intact when citations are on
- **Email addresses** and other mid-token @ uses are no longer rewritten as citations

## [1.1.38] - 2026-10-15

### Fixed

- **Long tables of contents** keep every entry and close properly instead of stopping at about 4 KB and swallowing the document into the TOC nav
- **TOC nesting** closes a nested item before its list when returning to a shallower heading
- **TOC entries that skip a heading level** (for example two #### under a ##) are listed as siblings instead of nested inside each other

## [1.1.37] - 2026-10-14

### Improved

- **ARIA pass performance** on large documents; attribute and class checks are limited to the tag instead of scanning to the end of the document

### Fixed

- **ARIA labels (--aria)** no longer hang on large documents; with image captions on, about 150 or more figures (or many captioned tables) filled a fixed 2KB slack and the conversion spun forever at full CPU
- **ARIA attributes** are no longer silently dropped once that slack ran out; every figure, table, figcaption, and TOC nav now gets its role, aria-describedby, id, or aria-label

## [1.1.36] - 2026-10-07

### Fixed

- **Header row colspans** (`| Wide | << | C |` or `| Wide || C |`) no longer delete the spanning cell and every header cell before it; the header now renders `<th colspan="2">Wide</th>`
- **Table cells containing `<<` or `^^` as text** (like `a << b`) are no longer dropped; only cells holding nothing but a span marker are removed

## [1.1.35] - 2026-10-02

### Fixed

- **Reference definition URL encoding** no longer rewrites inline code spans; text like `[id]: url` inside backticks renders as written instead of being percent-encoded to the end of the line
- **Fenced code blocks** are copied through the link and image preprocessor untouched, so definition-shaped lines and image syntax inside fences are no longer encoded
- **Mid-paragraph `[label]: text`** is left as plain text; only a label opening its line (up to three spaces of indent) is treated as a reference definition
- **Reference-style image expansion** no longer expands `![alt][ref]` written inside code spans
- **Reference definitions inside blockquotes** (`> [id]: url`) now resolve; the bare-URL autolinker no longer wraps the definition URL in `[url](url)`, which produced hrefs like `%5Bhttps://...%5D(...)`

## [1.1.34] - 2026-09-28

## [1.1.33] - 2026-09-28

## [1.1.32] - 2026-09-27

### Improved

- **Callout title escaping**: plain-text titles are now HTML-escaped and are no longer truncated at 1KB

### Fixed

- **Relaxed tables in blockquotes**: GFM pipe tables inside `>` blockquotes (including nested callouts) no longer get an unprefixed separator row injected, which split the table out of the blockquote and rendered it as pipe and dash text
- **Task lists in blockquotes**: `> - [ ] item` and `> - [x] item`, including task lists in callout bodies, now render checkboxes instead of literal `[ ]` text
- **Callout titles with inline markup**: titles like `> [!info]+ **Bold**`, emoji plus bold, code, links, and emphasis now render inside `<summary>` or `.callout-title` instead of falling back to the type name and leaving the markup in the body
- **Nested callouts**: `> > [!TYPE]` callouts inside another callout are now converted instead of rendering literal `[!TYPE]` text
- **Callout body spacing**: callout bodies no longer start with a stray line break, and a marker-only first line no longer leaves an empty paragraph
- **Config metadata in GFM/CommonMark output**: with a global or project config.yml (or --meta-file), `-m gfm` and `-m commonmark` no longer render the merged metadata as a horizontal rule and heading at the top of the document
- **Release builds missing parser fixes**: release binaries, Homebrew, SwiftPM, apex-node, and other builds from a clean checkout now include Apex's cmark-gfm patches (pipes in table code spans, escaped << in table cells, autolink fixes for @ in HTML tags and @2x image names, lists and definition lines interrupting paragraphs, and images not lazy-continuing blockquotes)

## [1.1.31] - 2026-09-26

### Changed

- **.yml and .yaml metadata files** (--meta-file, config files, plugin manifests) are now parsed as YAML even without --- markers, instead of as MultiMarkdown metadata, so quoted values like a: "x" are unquoted and a leading # comment no longer discards the whole file

### Improved

- **YAML files that libyaml rejects** (plugin manifests, config files, --meta-file) now print a one-line warning with the file path, libyaml's error, and the line and column before falling back to the simple key: value parser

### Fixed

- **--meta-file** metadata is no longer silently ignored when a global config exists but no project config does
- **--meta KEY=VALUE** now resolves [%key] in documents that have no front matter and no config files
- **Fallback YAML parser** (no libyaml, e.g. Apex.framework built with APEX_FRAMEWORK_NO_YAML, or input libyaml rejects) now turns '' into ' in single-quoted values, so title: 'Brett''s Notes' renders as Brett's Notes
- **Fallback YAML parser** now decodes double-quoted escapes (\t, \n, \", \, \uXXXX, and the rest of the YAML set), so plugin manifest replacements like "<span class=\"emoji\">" no longer keep literal backslashes
- **Fallback YAML parser** keeps leading and trailing spaces inside quoted values and accepts a # comment after a quoted value, matching libyaml
- **Fallback YAML parser** skips # comment lines instead of turning them into keys

## [1.1.30] - 2026-09-26

### Changed

- **.yml and .yaml metadata files** (--meta-file, config files, plugin manifests) are now parsed as YAML even without --- markers, instead of as MultiMarkdown metadata, so quoted values like a: "x" are unquoted and a leading # comment no longer discards the whole file

### Improved

- **YAML files that libyaml rejects** (plugin manifests, config files, --meta-file) now print a one-line warning with the file path, libyaml's error, and the line and column before falling back to the simple key: value parser

### Fixed

- **--meta-file** metadata is no longer silently ignored when a global config exists but no project config does
- **--meta KEY=VALUE** now resolves [%key] in documents that have no front matter and no config files
- **Fallback YAML parser** (no libyaml, e.g. Apex.framework built with APEX_FRAMEWORK_NO_YAML, or input libyaml rejects) now turns '' into ' in single-quoted values, so title: 'Brett''s Notes' renders as Brett's Notes
- **Fallback YAML parser** now decodes double-quoted escapes (\t, \n, \", \, \uXXXX, and the rest of the YAML set), so plugin manifest replacements like "<span class=\"emoji\">" no longer keep literal backslashes
- **Fallback YAML parser** keeps leading and trailing spaces inside quoted values and accepts a # comment after a quoted value, matching libyaml
- **Fallback YAML parser** skips # comment lines instead of turning them into keys

## [1.1.29] - 2026-09-26

### New

- **APEX_FRAMEWORK_NO_YAML CMake option** builds Apex.framework without libyaml for app embedding (YAML array support is unavailable in that build)

### Improved

- **Apex.framework** now links cmark-gfm statically so embedded frameworks do not depend on libcmark-gfm dylib paths from the build tree

### Fixed

- **++insert++ syntax** no longer converts ++ runs inside HTML tag attributes, so base64 data: URIs in inline images (e.g. from Bear) are no longer corrupted with <ins> tags
- **Relaxed tables** no longer treat pipes inside [[wiki|label]] links as cell separators, so consecutive list items or lines with labelled wiki links no longer become a table with an injected ---|---| separator row
- **markdown="1" blocks** now apply enabled GFM extensions (pipe tables, task lists, strikethrough, extended autolinks), including in nested blocks such as Obsidian callouts

## [1.1.28] - 2026-09-25

### Changed

- **emoji-span example plugin** pattern now uses the same non-alnum boundary rule so it does not wrap timecode segments in spans

### Fixed

- **Emoji shortcodes** now require non-alphanumeric boundaries on both sides, so digit-colon runs like 1:00:55:00 and letter-adjacent forms like foo:smile:bar are no longer treated as :name: candidates
- **Emoji autocorrect** no longer fuzzy-matches all-digit names (exact :100: / :1234: still work) and scales max Levenshtein distance by name length instead of always allowing distance 4

## [1.1.27] - 2026-09-25

## [1.1.26] - 2026-09-24

### Fixed

- **Relaxed table preprocessing** no longer inserts a second separator after the first body row of an already-separated GFM table, which previously corrupted the table and caused following tables (including `||` colspan rows) to leak as paragraphs

## [1.1.25] - 2026-09-24

### Changed

- **Emoji autocorrect docs** clarify that autocorrect is separate from `:name:` replacement

### New

- **`--[no-]emoji`** enables or disables GitHub-style `:name:` emoji replacement in HTML and terminal output (on by default in gfm and unified; off in commonmark, multimarkdown, and kramdown)
- **`emoji` config/metadata** boolean mirrors the CLI flag

## [1.1.24] - 2026-09-21

### Changed

- **Homebrew formula** updated to 1.1.23 with macOS and Linux release checksums

### Fixed

- **Author-in-text citations** no longer treat a trailing colon as part of the key when `@Name:` is followed by a space or digit (iA Writer Annotations dumps like `@Wayne: 2,7 10,8`)
- **Emoji replacement** skips HTML comments so `<!--CITE:key-->` placeholders are not corrupted when a key contains colon-delimited shortcodes (for example `:Wayne:` matching `:wave:`)

## [1.1.23] - 2026-09-21

## [1.1.22] - 2026-09-21

### Changed

- **Homebrew formula** bump to 1.1.21 and refresh the macOS universal checksum

### Improved

- **Header ID injection buffer** grows with realloc on demand instead of silently dropping input once capacity is exhausted

### Fixed

- **Homebrew formula** include Linux x86_64 and aarch64 tarball URLs so brew tap no longer fails Linux validation
- **Header permalink anchors** no longer truncate the rest of the HTML when many headings make injected `<a href="#..." id="...">` tags exceed the old fixed buffer budget

## [1.1.21] - 2026-09-07

### Changed

- **Apex version** Release this fix as 1.1.20.

### Improved

- **Block starters** (`>`, headings, lists, tables, HTML) are not buffered as potential definition terms

### Fixed

- **Metadata file options** Keep copied string and stylesheet values valid after metadata cleanup.
- **Fenced code after a definition list** parses as a real `<pre><code>` block instead of raw backticks (CommonMark no longer treats the fence as part of the `</dl>` HTML block)
- **Blank lines after a buffered non-term** are preserved so following callouts/blockquotes do not swallow later paragraphs

## [1.1.20] - 2026-09-05

### Fixed

- **Metadata file string ownership** keeps paths, titles, themes, and stylesheet values valid after `apex_options_apply_meta_file` returns

## [1.1.19] - 2026-09-05

### New

- **Metadata file API** lets embedding hosts load and apply CLI-compatible YAML, Pandoc, or MultiMarkdown metadata files to `apex_options`

## [1.1.18] - 2026-09-05

### New

- **Quoted heading overrides** (`{^"term"}` and `[visible]{^"override"}`) so standalone marks and custom index headings work without falling through to superscript
- **Underscore emphasis in TextIndex headings** renders as `<em>` in the index and sorts as plain text
- *** and ** wildcards** expand the preceding heading (stripping emphasis; `**` also lowercases)
- **Prefix wildcards** (`*^` / `*^-`) reuse an earlier matching heading path
- **See and see-also cross-references** (`|target`, `|+target`, hierarchical `parent>"child"`) with see-type marks omitting locators
- **Inbound cross-references** (`@` / `@+`) attach see/see-also refs onto the target entry
- **Aliases** (`#name` after a path, usable in headings and cross-refs) plus **unreferenced aliases** (`##name`) that define without a locator
- **Hierarchical TextIndex paths** (`"Parent">"Child"`) as nested index entries
- **`--concordance FILE`** loads TextIndex concordance TSV files and auto-inserts `[match]{^...}` marks before index processing (enables indices; repeatable)
- **Processing toggles** (`{^-}` / `{^+}`) to leave TextIndex marks literal in selected regions

### Improved

- **Duplicate headings merge into one entry** with multiple locators (TextIndex and mmark)
- **Homebrew installs man pages** (`apex.1`, `apex-config.5`, `apex-plugins.7`) from the release archive

### Fixed

- **`[phrase]{^}` strips square brackets** from visible text while indexing the phrase
- **Bare `word{^}` indexes only the adjacent word**, not a multi-word run of preceding text

## [1.1.17] - 2026-09-05

### New

- **Quoted heading overrides** (`{^"term"}` and `[visible]{^"override"}`) so standalone marks and custom index headings work without falling through to superscript
- **Underscore emphasis in TextIndex headings** renders as `<em>` in the index and sorts as plain text
- *** and ** wildcards** expand the preceding heading (stripping emphasis; `**` also lowercases)
- **Prefix wildcards** (`*^` / `*^-`) reuse an earlier matching heading path
- **See and see-also cross-references** (`|target`, `|+target`, hierarchical `parent>"child"`) with see-type marks omitting locators
- **Inbound cross-references** (`@` / `@+`) attach see/see-also refs onto the target entry
- **Aliases** (`#name` after a path, usable in headings and cross-refs) plus **unreferenced aliases** (`##name`) that define without a locator
- **Hierarchical TextIndex paths** (`"Parent">"Child"`) as nested index entries
- **`--concordance FILE`** loads TextIndex concordance TSV files and auto-inserts `[match]{^...}` marks before index processing (enables indices; repeatable)
- **Processing toggles** (`{^-}` / `{^+}`) to leave TextIndex marks literal in selected regions
- **Duplicate headings merge into one entry** with multiple locators (TextIndex and mmark)
- **Homebrew installs man pages** (`apex.1`, `apex-config.5`, `apex-plugins.7`) from the release archive
- **`[phrase]{^}` strips square brackets** from visible text while indexing the phrase
- **Bare `word{^}` indexes only the adjacent word**, not a multi-word run of preceding text

### Fixed

- **Spaced pipes (`| |`) stay blank cells** even when a later row uses consecutive pipes (`|||||`) for colspan
- **Colspan only from consecutive pipes** (`||` / `|||`) or bare `<<`, not from whitespace-only cells
- **Indent/blank columns keep alignment** when a full-width colspan row follows (Hazel-style tables)

## [1.1.16] - 2026-08-14

## [1.1.15] - 2026-08-07

### Fixed

- **Link IAL attributes** (e.g. {.js-lightbox}) are preserved when a raw HTML <a> appears earlier in the document.

## [1.1.14] - 2026-08-07

### Fixed

- **Link IAL attributes** (e.g. `{.js-lightbox}`) are preserved when a raw HTML `<a>` appears earlier in the document.
- **RTF remote images** show a hyperlinked alt label (or "image" when alt is empty) instead of dumping raw URLs.
- **-t rtf** writes Rich Text Format from the CLI (also `-o file.rtf`).
- **APEX_OUTPUT_RTF** / **apex_cmark_to_rtf()** return RTF from the C API.
- **RTF writer** covers headings, emphasis, links, lists (including tasks), code, quotes, tables, images, footnotes, definition lists, strike/sup/sub, and best-effort callouts/HTML.
- **ObjC/Swift apexRTF** and **apexAttributedString** (RTF preferred, HTML fallback) for AppKit/UIKit.
- Additional docset pages
- **RTF local images** embed as PNG/JPEG picts by default without needing --embed-images.
- **Link IAL attributes** (e.g. {.js-lightbox}) are preserved when a raw HTML <a> appears earlier in the document.

## [1.1.13] - 2026-08-03

### Changed

- **RTF remote images** show a hyperlinked alt label (or "image" when alt is empty) instead of dumping raw URLs.
- **-t rtf** writes Rich Text Format from the CLI (also `-o file.rtf`).
- **APEX_OUTPUT_RTF** / **apex_cmark_to_rtf()** return RTF from the C API.
- **RTF writer** covers headings, emphasis, links, lists (including tasks), code, quotes, tables, images, footnotes, definition lists, strike/sup/sub, and best-effort callouts/HTML.
- **ObjC/Swift apexRTF** and **apexAttributedString** (RTF preferred, HTML fallback) for AppKit/UIKit.
- Additional docset pages
- **RTF local images** embed as PNG/JPEG picts by default without needing --embed-images.

### New

- **Node npm binding design** documenting @apexmarkdown/apex native distribution, camelCase options, and v1 platform scope.
- **Node npm implementation plan** covering CMake binding, complete options mapping, and platform prebuilds.
- **Autolinking** scales linearly on large documents instead of slowing quadratically.

## [1.1.12] - 2026-07-19

### New

- **Bear image attributes** Apply safe JSON metadata while preserving its comment.
- **Reference metadata** Apply attributes to every matching image.
- **Per-image reference attributes** Override shared metadata on one image.

### Fixed

- **Bear image metadata parser** releases prior results when an output struct is reused, so early parse failures no longer leak or retain stale attributes
- **Bear image attributes** no longer apply in Kramdown, which only enters the image preprocessor for URL encoding.
- **Reference metadata** only applies when the Bear comment directly follows the definition URL, title, or attributes.
- **Definition comments** keep their original line ending so following blocks are not glued to the comment.
- **Bear titles** override quoted definition titles instead of being shadowed.

## [1.1.11] - 2026-07-14

### Improved

- Undelimited MMD metadata keys may only contain letters, digits, spaces, hyphens, and underscores; punctuation in the key rejects the line as ordinary text
- A single candidate metadata line longer than 100 characters is treated as prose rather than a lone MMD key/value pair

### Fixed

- Undelimited MultiMarkdown metadata is only recognized at the very start of a file; a leading blank line or any content before the first key/value pair (including headings) disables it
- Prose paragraphs with a colon after an H1 are no longer mistaken for MMD metadata and stripped from the output

## [1.1.9] - 2026-07-14

### Fixed

- Undelimited MultiMarkdown metadata is only recognized at the very start of a file; a leading blank line or any content before the first key/value pair (including headings) disables it
- Prose paragraphs with a colon after an H1 are no longer mistaken for MMD metadata and stripped from the output

### Improved

- Undelimited MMD metadata keys may only contain letters, digits, spaces, hyphens, and underscores; punctuation in the key rejects the line as ordinary text
- A single candidate metadata line longer than 100 characters is treated as prose rather than a lone MMD key/value pair

## [1.1.5] - 2026-07-06

### Fixed

- Buffered definition-list term text before a fenced code block is no longer discarded when the preprocessor closes the list for the fence
- Images and reference link definitions immediately after a blockquote line without a blank line are no longer swallowed into the blockquote by CommonMark lazy continuation
- Reference-style images like ![][id] following a blockquote render outside the blockquote with the reference resolved correctly

## [1.1.4] - 2026-06-25

### Changed

- Homebrew formula bumped to 1.1.3

### New

- --paginate-symbols renders images as chafa ANSI art compatible with less -R when paging terminal output
- --no-paginate disables terminal pagination, overriding -p/--paginate and paginate settings from config or metadata
- Paginate: symbols metadata enables pager-friendly terminal images (chafa -f symbols) alongside normal pagination
- APEX_DEBUG_TERMINAL environment variable logs terminal image viewer selection, exec commands, and syntax highlighter invocations to stderr

### Improved

- Terminal image tools use terminal-aware chafa formats (iterm, kitty, symbols) with 256- or 16-color levels matching terminal256 vs terminal
- --width wrapping preserves OSC, DCS, and Kitty graphics escape sequences instead of breaking inline image output

### Fixed

- Imgcat is only used in iTerm2 so inline images no longer appear as raw binary garbage in Cursor and other non-iTerm terminals
- Apex automatically skips the pager when inline terminal graphics are present, since less -R and most pagers only support ANSI color

## [1.1.3] - 2026-06-24

### Changed

- Quarto mode enables callouts and image captions, disables wiki links, marked extensions, py callouts, and index generation

### New

- Add APEX_MODE_QUARTO and --mode quarto / mode: quarto metadata for Quarto-compatible processing with unified-family defaults
- Add apex_mode_is_unified_family() and apex_mode_is_kramdown_or_unified_family() helpers to share unified/quarto code paths
- Add enable_quarto_extensions flag for future Pandoc/Quarto preprocessors
- Images with {fig-alt="..."} use markdown alt text as figure caption and fig-alt as accessible img alt attribute
- Add .smallcaps, .underline, and span.mark CSS in quarto standalone output
- Add quarto mode test suite and smoke fixtures (callouts, spans, fig-alt, fenced divs)
- Quarto mode preprocesses Pandoc/Quarto raw content: fenced ```{=html} blocks and inline `content`{=html} passthrough HTML when unsafe is enabled; non-html formats are wrapped in <!-- raw format=... --> comments
- Add raw content examples to quarto smoke fixture and quarto mode test suite
- Quarto mode supports Pandoc list continuation markers: standalone (@) lines are stripped and split ordered lists merge when only blank lines interrupt them; interrupted lists keep intervening content and continue numbering with start attribute
- Quarto mode supports roman list markers (i), ii), I), etc.) with lower-roman and upper-roman list-style-type in HTML output
- Quarto mode converts Pandoc line blocks (lines starting with |) to div.line-block with preserved spacing
- Add list extension tests and smoke fixture examples for (@), roman lists, and line blocks
- Quarto mode supports labeled example list markers such as (@good) with labels stripped from output
- Quarto mode preprocesses Pandoc/Quarto fenced code block attributes ({.python filename="run.py" linenos=true}): normalizes to plain language fences and preserves filename, linenos, and other keys as data-* attributes on the rendered pre element
- Per-block linenos=true on fenced code blocks enables line numbers during external syntax highlighting even when global --code-line-numbers is off
- Add code fence attribute tests and smoke fixture example
- Quarto mode preprocesses {mermaid}, {dot}, and {graphviz} fenced blocks into raw pre elements with matching diagram classes (requires unsafe)
- Standalone Quarto output auto-injects mermaid.js when mermaid diagrams are present and no mermaid script is already configured
- Add quarto-diagrams metadata key, diagram tests, and smoke fixture examples
- Quarto mode converts {{< pagebreak >}} shortcodes to page breaks in HTML output (raw HTML when unsafe, Leanpub marker otherwise)
- Quarto mode converts {{< kbd ... >}} and {{% kbd ... %}} shortcodes to {% kbd %} liquid tags for use with the kbd plugin
- Quarto mode converts {{< include path >}} shortcodes to <<[path]> Marked file include syntax
- Add enable_quarto_shortcodes option (enabled by default in --mode quarto) and quarto-shortcodes metadata key to toggle the shortcode preprocessor
- Unknown Quarto shortcodes are left unchanged; set APEX_VERBOSE=1 to log warnings for unrecognized shortcode names
- Wrap Quarto cross-ref tokens (@fig-id, @sec-id, @tbl-id, @eq-id) in span.quarto-xref in HTML output
- Add per-feature Quarto metadata toggles (quarto-raw, quarto-list-continuation, quarto-line-blocks, quarto-roman-lists, quarto-code-attrs, quarto-strict-lists, quarto-xrefs, quarto-extensions)
- Add optional quarto-strict-lists preprocessor for Pandoc strict blank-line-before-list behavior
- Add .hidden and .quarto-xref rules to standalone default CSS
- Split Quarto test fixtures by topic and add wiki Quarto-Mode documentation

### Improved

- CLI --mode quarto restores quarto extension flags after global/project config metadata merge

### Fixed

- Bracketed span IAL class attributes now emit valid HTML (class="..." instead of class="...")
- Quarto mode treats (@) and (@label) as Pandoc example_lists list markers (line-start markers with item text), not as standalone list-continuation lines between numbered items
- Example lists interrupted by paragraphs continue document-wide numbering with ol start on resumed lists
- When quarto-xrefs is enabled, bare @fig-/@sec-/@tbl-/@eq- tokens are no longer parsed as author-in-text citations (bracketed [@key] citations unchanged)

## [1.1.2] - 2026-06-23

### New

- Public C API (apex/plugins.h) to fetch the plugin directory, list installed plugins, and install or uninstall plugins programmatically.
- ApexPluginManager Swift API and ApexPluginCatalog Objective-C wrappers for building plugin picker UIs in Xcode apps.
- EnablePlugins option in ApexOptions and the NSString options dictionary to turn on external plugin processing during conversion.

## [1.1.1] - 2026-06-21

### Changed

- Version bumped to 1.1.0.

### New

- Pandoc grid table syntax via --grid-tables / --enable-grid-tables (opt-in; converts +---+ grids to tables before parsing).
- Enable_grid_tables API option (default off; requires enable_tables).
- --no-grid-tables explicitly disables Pandoc grid table preprocessing (grid tables remain off by default in unified mode).
- Grid-tables / grid_tables metadata key enables or disables Pandoc grid tables per document.

### Improved

- Grid tables support colspan rows, nested grids inside cells, multiline list content in cells, and HTML output for tables with partial in-cell separators.
- Embedded nested grids inside grid table cells convert to HTML tables so they display inside markdown="1" cells (pipe tables are not parsed there).
- Headerless single-row grid tables emit a valid pipe-table header row instead of a lone delimiter line.

### Fixed

- Reference-style links in markdown="1" HTML blocks (callouts, blockquotes, divs, etc.) now resolve definitions from the full document instead of rendering as literal text.
- Reference footnotes [^id] in markdown="1" blocks now resolve definitions from the full document and render footnote refs and a footnotes section inside the block.
- Markdown="1" blocks with footnotes no longer render as empty elements when parsed HTML exceeds the initial output buffer.
- Grid tables no longer treat every row with fewer pipe cells than columns as a full-row colspan (e.g. Property | Earth headers now render as separate cells with correct colspan, not one merged cell).
- Colspan rows with nested grids no longer leak raw pipe or grid syntax as paragraphs after the table; nested grids inside colspan cells render as HTML tables.
- Grid tables with in-row partial separators (+---+ inside a row) now parse Pandoc-style layouts with rowspan/colspan (e.g. Temperature / 1961-1990 spanning rows with min/mean/min data).
- Multiline grid table cells (lists, multiple lines) now render markdown correctly via HTML cells instead of literal text with <br> tags.
- Lines starting with "+" alone (e.g. list items) are no longer mistaken for grid tables; only "+---" / "+===" border rows start a grid block.
- Invalid or unconvertible grid blocks now preserve the original source lines instead of silently dropping content.

## [1.0.15] - 2026-06-11

### Fixed

- Reference-style links in markdown="1" HTML blocks (callouts, blockquotes, divs, etc.) now resolve link definitions from the full document instead of rendering as literal text.

## [1.0.14] - 2026-06-09

### Fixed

- Special markers (<!--BREAK-->, <!--PAUSE:N-->, {::pagebreak /}, {index}, and ^ end-of-block) are no longer converted inside fenced code blocks, indented code blocks, or inline code spans.
- Backslash-escaped MultiMarkdown TOC markers like \{\{TOC\}\} are no longer expanded into a table of contents after markdown strips the escape backslashes.
- Kramdown {:toc} markers inside fenced or indented code blocks are no longer converted to <!--TOC--> during IAL preprocessing.

## [1.0.13] - 2026-05-06

### New

- Add --py-callouts and --quarto-callouts CLI toggles to enable Python/markdown-callouts and Quarto callout parsing only when explicitly requested.
- Support Python callouts in both !!! type "Title" blocks and NOTE: label style, including collapsed >? NOTE: syntax when --py-callouts is enabled.
- Support Quarto callout fences (::: {.callout-*}) as callouts behind --quarto-callouts.

### Improved

- Fenced code examples with pipelines now render as code without unexpected table artifacts in highlighted HTML and markdown output.
- Preserve normal fenced div behavior for non-callout ::: blocks while Quarto callouts are enabled.
- Add dedicated fixtures and expanded tests for Obsidian, Python, and Quarto callout formats to prevent regressions.

### Fixed

- Relaxed table processing no longer injects ---|---|---| separator rows inside fenced code blocks that contain pipe-heavy shell commands.
- Remove leading empty paragraph artifacts from converted Quarto callout content blocks.

## [1.0.12] - 2026-04-20

### Improved

- Pre-parse plugin transforms now apply to the final selected include content (including section/address-filtered text) instead of rewriting source files before section matching.

### Fixed

- Section-targeted transclusions now extract the requested section before pre-parse plugins run, so ![[file#Section]] and other include syntaxes return the correct section content.

## [1.0.11] - 2026-04-20

### Changed

- Homebrew installs now point to Apex 1.0.10 with the updated macOS release artifact checksum.

### Improved

- Included file content now runs through pre-parse plugins before section slicing, address extraction, and nested include processing so plugin transformations apply consistently to transclusions.

### Fixed

- Section-targeted includes now fall back to including the full source document when the requested section heading does not exist.
- Obsidian ![[file#section]] and MultiMarkdown {{file#section}} now behave consistently when section names are missing or mistyped.

## [1.0.10] - 2026-04-20

### Changed

- Homebrew installs now point to Apex 1.0.9 with the updated macOS release artifact checksum.

### New

- Supports Obsidian embed transclusion syntax ![[file]] and ![[file#Section]] with section extraction.
- Section-targeted includes now work consistently across Marked, MultiMarkdown, and iA Writer include syntaxes.

### Fixed

- Obsidian embeds now respect --wikilink-extension for extensionless targets and fall back to .md when the configured extension file does not exist.
- Wiki link parsing no longer rewrites ![[...]] embeds into !<a ...> output.

## [1.0.9] - 2026-04-19

### Changed

- Homebrew installs now point to Apex 1.0.8 with the updated macOS release artifact checksum.

### Fixed

- Picture generation now preserves image attributes like loading, width, and height on fallback img tags when using avif/webp with retina variants.

## [1.0.8] - 2026-04-19

### Changed

- Homebrew users now install Apex v1.0.7 from the release tarball.

### Fixed

- Autolinking no longer rewrites image density filenames like text-basic@2x.jpg into mailto links.
- Autolinking now ignores content inside HTML tags and attributes such as href and srcset values.

## [1.0.7] - 2026-04-15

### New

- Tests for CommonMark !\[ (non-image), false ![ blog case, U+2033 vs ASCII quotes, real images, and glued ** after punctuation (see tests/test_escaping_repro.c).

### Fixed

- Prevented autolink preprocessing from rewriting text inside markdown link labels, which fixes broken rendering for links like [email](mailto:user@example.com?subject=...&body=...).

## [1.0.6] - 2026-04-02

### Changed

- Update Homebrew formula to Apex 1.0.5 and refresh the macOS release checksum.

### Fixed

- Keep synthetic nested ordered sublists tight in unified mode so first-level list items do not get unwanted paragraph wrappers.
- Preserve intentionally loose list rendering when users add explicit blank lines before nested ordered sublists.
- Prevent <<[file] includes from consuming following [ref]: and [^id]: lines as include address specs.
- Stop definition lines after included CSV tables from being misread as table captions.

## [1.0.5] - 2026-04-01

### Changed

- Update Homebrew formula to Apex 1.0.4 and refresh the macOS release checksum.

### Improved

- Strengthen public module exports so Swift and ObjC module consumers can resolve markdown and man serializer APIs consistently across build environments.
- Fix compiler warnings
- Tighten [Caption] detection so caption parsing only triggers when the paragraph contains standalone caption content.
- Restrict backward caption lookup to the nearest valid caption context so distant captions cannot be reused.
- Normalize TOC nesting relative to the selected minimum heading level so depth filtering produces stable structure.
- Normalize TOC labels by trimming and collapsing whitespace so generated links stay clean.

### Fixed

- Render indented numeric sublists as nested ordered lists in unified mode instead of flattening them into parent list item text.
- Correct alpha lists with nested sublists so continuation items render as proper list items instead of plain text.
- Remove leaked [apex-alpha-list:lower] markers from rendered HTML when alpha lists contain nested sublists.
- Prevent "$40" and similar currency amounts from being parsed as inline math delimiters.
- Restore Swift package and release-check builds that failed after merge due to missing markdown/man serializer type and function declarations.
- Render {{TOC}} markers in MultiMarkdown mode so table of contents placeholders are expanded instead of being left as literal text.
- Prevent footnote definitions like [^1]: ... from being treated as table captions.
- Prevent footnote and link definition lines like [ref]: ... from being treated as table captions.
- Stop inclusion-table captions from leaking onto other nearby tables that do not define their own caption.
- Make {{TOC:2}} render matching headings instead of an empty TOC.
- Keep same-level headings as siblings in range TOCs like {{TOC:2-6}}.
- Remove extra indentation whitespace that appeared inside TOC link text in pretty HTML output.

## [1.0.4] - 2026-03-31

### Fixed

- Prevent heading ID generation crashes on very large headings by making heading text extraction buffer growth handle required size safely.

## [1.0.3] - 2026-03-29

## [1.0.2] - 2026-03-29

## [1.0.1] - 2026-03-29

## [0.1.104] - 2026-03-29

### Fixed

- After merging global/project/document/`--meta` metadata, the CLI now re-applies every option that was set on the command line so config and YAML metadata cannot override explicit flags (including `--mode`, `-t`/`--to`, feature toggles, `--standalone`, `--style`/`--css`, bibliography/CSL, and the rest of the documented CLI surface).
- Track argv-set fields with apex_cli_option_mask, snapshot options after wiring bibliography/stylesheet, and re-apply after apex_apply_metadata_to_options so global/project/document metadata cannot override explicit flags.

## [0.1.103] - 2026-03-29

### New

- `i/--info` prints version, merged config (global, project, --meta-file, --meta),
- `--extract-meta` and `-e KEY` merge per-file document metadata in order (mode-aware)
- Add metadata_yaml_emit unit tests and man page entries.
- CLI `-s`/`--standalone` (and `--style`/`--css`, which imply standalone) now wins over `standalone: false` from document or config metadata, so explicit standalone output is not downgraded to an HTML fragment.

### Fixed

- Track explicit -s/--standalone and --style/--css and re-apply standalone after apex_apply_metadata_to_options so document or config YAML cannot force a fragment when the user asked for a full document.

## [0.1.102] - 2026-03-22

### Changed

- Use `--to xhtml`/`-t strict-xhtml` for semantic reasons, original `--[strict-]xhtml` flags left in place

## [0.1.100] - 2026-03-22

### Changed

- Homebrew formula bumped to 0.1.97.
- Bump Homebrew formula to v0.1.98 with updated release tarball SHA256.

### New

- Display inline images in terminal output using imgcat, chafa, viu, or catimg
- `--[no-]terminal-images`, `--terminal-image-width
- Remote images are downloaded to temp directory for terminal display (requires curl)
- Rough tests for image output

### Improved

- SwiftPM now exports the raw C API via the `ApexC` product.

### Fixed

- Avoid Swift module-name collisions so `apex_*` symbols are visible from Swift.
- Include new module map

## [0.1.98] - 2026-03-19

### Changed

- Homebrew formula bumped to 0.1.97.

### Improved

- SwiftPM now exports the raw C API via the `ApexC` product.

### Fixed

- Avoid Swift module-name collisions so `apex_*` symbols are visible from Swift.

## [0.1.97] - 2026-03-19

### Changed

- Metadata extraction is now mode-aware so Combined/Unified and Kramdown keep YAML-first interpretation of delimiter blocks, while MMD uses MMD-compatible precedence.

### New

- CSV and TSV includes now support custom separators via {delimiter=X} and single-character shorthand {X} while keeping comma/tab defaults when no override is provided.
- This release adds delimiter support as discussed in [#13](https://github.com/ApexMarkdown/apex/issues/13)

### Improved

- Metadata-to-meta conversion now works consistently in MultiMarkdown, Unified (combined), and Kramdown modes.
- In --mode mmd, delimiter-style metadata is parsed using MultiMarkdown rules while still falling back to YAML parsing for true YAML front matter.
- Include parsing now recognizes explicit delimiter overrides consistently across iA Writer, Marked, and MultiMarkdown include styles for CSV/TSV table conversion.

### Fixed

- Standalone HTML now emits generic metadata headers as <meta name="..."> tags instead of dropping them.
- Generated meta tags preserve declared metadata order and safely escape attribute content.
- MultiMarkdown mode now accepts delimited metadata blocks with dash or dot closers (for example "----" ... "......") without leaking the delimiter into HTML output.
- Marked include syntax now supports embedded delimiter overrides inside the include token (for example <<[data.csv{;}] and <<[data.csv{delimiter=;}] ) to avoid conflicts with bracket-plus-brace parsing patterns.
- MultiMarkdown transclusions now accept embedded delimiter overrides (for example {{data.csv{;}}} and {{data.csv{delimiter=;}}}) and no longer fail when braces appear inside the transclusion path.
- This release resolves [#17](https://github.com/ApexMarkdown/apex/issues/17)

## [0.1.96] - 2026-03-19

### Improved

- Swift plugin integrations can now fetch default low-level options through NSString.defaultApexOptions() and mutate apex_options fields directly.

### Fixed

- Swift-side module visibility for C interop is improved by exporting ApexC from the Apex Swift module.

## [0.1.95] - 2026-03-16

### Improved

- Added all_checks target that runs C tests and Swift package builds together for one-command verification before publishing
- Cleanup: quieter tests by fixing logical-op and unused-variable warnings in table test suite

### Fixed

- Swift package now compiles cleanly in debug and release as part of all_checks, catching Swift-side breakage before release
- Man-page renderer builds without enum or options-type warnings in both CMake CLI and Swift package builds

## [0.1.94] - 2026-03-07

### Fixed

- CMake/C99 build: add missing stdlib.h so malloc, free, realloc, getenv, and strtol are declared (fixes build on strict compilers)
- Bracketed spans like [-]{.taskmarker} no longer trigger list parsing; markdown="span" is only emitted when the span content contains inline markdown syntax (emphasis, links, code, etc.)

## [0.1.93] - 2026-03-05

### New

- Tests that one-line definition list syntax (term::definition) is not converted inside inline code spans, fenced code blocks, indented code blocks, and multi-line inline code
- Tests that emoji (:name:) patterns are not converted inside inline code spans, fenced code blocks, and indented code blocks

### Improved

- HTML emoji replacement skips content inside <code> and <pre> elements so code examples display :emoji: patterns as written

### Fixed

- Definition list one-line term::definition no longer converts inside inline code spans (backticks), preserving literal syntax in code examples
- Definition list processing now skips indented code blocks (4+ spaces or tab), not just fenced blocks
- Kramdown-style : definition lines inside multi-line inline code spans are no longer incorrectly parsed as definition list items
- Emoji replacement (:name:) is now skipped inside fenced code blocks, indented code blocks, and inline code spans so patterns remain literal
- Emoji autocorrect no longer modifies :emoji: patterns inside any code context

## [0.1.92] - 2026-03-05

### Changed

- Update apex_options cmark_init/cmark_done callback signatures to add a user_data parameter, requiring existing C/Obj-C callback implementations to add a fourth void* argument when upgrading

### Improved

- Ensure custom cmark-gfm init/done callbacks configured via apex_options work consistently so extension-based integrations remain stable across builds
- Better integration of Apex as library

### Fixed

- Fix cmark_done callback invocation to match the 3-argument apex_options callback signature so builds succeed again
- Update test cmark callback helper signatures and option fields to use the current cmark_init/cmark_done API and keep the test runner passing
- Fix cmark_done callback invocation to match the 4-argument apex_options callback signature including user_data so builds succeed again
- Add new tests and fix missing includes

## [0.1.91] - 2026-03-04

### New

- Add test fixtures for percent decoding

### Improved

- Apex_extract_heading_text now recurses into inline containers (EMPH, STRONG, LINK) and includes HTML_INLINE literal content so extracted text matches rendered HTML for reliable (level, text) matching during ID injection

### Fixed

- Headings with inline emphasis (e.g. "### *Processing* modes") now receive IDs correctly instead of being skipped
- Headings with ampersands (e.g. "## Documentation & resources") now receive IDs correctly by extracting text from HTML_INLINE nodes

## [0.1.90] - 2026-03-04

### New

- Apex_options.cmark_init callback: register custom cmark-gfm syntax extensions before parsing; call cmark_parser_attach_syntax_extension() in your callback (include cmark-gfm.h and cmark-gfm-extension_api.h when implementing) Resolves [#10](https://github.com/ApexMarkdown/apex/issues/10)
- Apex_version_string() exposed in ObjC/Swift via [NSString apexVersion] and Apex.version

### Improved

- CSV/TSV inline tables (```table fences, <!--TABLE-->, includes) now accept Markdown-style alignment specs in the second row. Cells containing only colons and dashes (e.g. :--, --:, :--:) are parsed by colon position: leading = left, trailing = right, both = center, neither = auto. Keywords (left, right, center, auto) continue to work unchanged. Resolves [#14](https://github.com/ApexMarkdown/apex/issues/14)

### Fixed

- Include paths now support percent encoding (e.g. <<[with%20space.txt], {{file%20name}}, /path%20to%2Ffile) so paths with spaces and special characters resolve correctly to files on disk. Resolves [#12](https://github.com/ApexMarkdown/apex/issues/12)

## [0.1.89] - 2026-03-04

### Improved

- CSV/TSV inline tables: alignment row may use Markdown-style syntax (`:--`, `--:`, `:--:`) in addition to keywords (left, right, center, auto). Cells containing only colons and dashes are parsed by colon position: leading colon = left, trailing = right, both = center, neither = auto.

### Changed

- Definition lists rewritten as preprocessing (no cmark extension): supports Kramdown "term" + ": definition" or ":: definition", plus one-line "term::definition" and "term :: definition" using last :: to avoid splitting URLs
- CLI flags --one-line-definitions and --no-one-line-definitions to enable or disable definition list processing
- Metadata keys one-line-definitions and one_line_definitions for front-matter control of definition lists
- Table caption ": Caption" no longer misparses Kramdown definition lines (e.g. "Term\n\n: definition 1") by requiring prev_line_was_table_row or in_table_section instead of prev_line_was_blank alone
- Table captions before tables: ": Caption" now recognized when the next non-blank line is a table row
- Table caption paragraphs removed from output so captions appear only in figcaption, not duplicated as standalone paragraphs

## [0.1.88] - 2026-03-02

### Changed

- Formula/apex.rb: version 0.1.87, update macOS universal tarball sha256

### Improved

- Definition lists: support indented continuation lines (4+ spaces) so multi-line definitions stay within a single dd element instead of splitting into separate paragraphs

### Fixed

- Disable smart typography for man and man-html output so option names like --to and --standalone render as literal double hyphen instead of en-dash
- Prevent metadata from overwriting -t man-html when document sets mode or other options in front matter
- Force disable smart typography for man and man-html output in apex_markdown_to_html so option names stay as literal --
- Man-html: Replace UTF-8 en-dash (U+2013) with "--" in rendered text and definition list HTML blocks so options like --standalone display correctly even when smart typography slips through

## [0.1.87] - 2026-03-02

### Changed

- Formula/apex.rb: version 0.1.86, update macOS universal tarball sha256

### Fixed

- Linux release build: add missing limits.h include for INT_MAX in apex_cli_terminal_width

## [0.1.86] - 2026-03-02

### Changed

- Syntax highlighter chooses HTML or ANSI output based on destination format (--format html vs --format ansi for Shiki when output is terminal).
- Apex_apply_syntax_highlighting() now takes a fifth parameter ansi_output (bool); pass false for HTML output, true for terminal/ANSI. All internal call sites updated; direct callers (e.g. Swift/SPM) must be updated.
- Pygments highlighting now respects code-highlight-theme for both HTML and terminal/terminal256 output (maps to style=THEME).
- Skylighting highlighting now respects code-highlight-theme for both HTML and ANSI terminal output (maps to --style THEME) and chooses --color-level=16/256 based on terminal vs terminal256.
- Shiki highlighting now uses --theme THEME for both HTML and ANSI output and chooses --format html/ansi based on destination format.

### New

- Support --code-highlight shiki (and abbreviation sh) on the command line; uses the shiki CLI (@shikijs/cli) when available.
- Support code-highlight: shiki and code-highlight: sh in metadata and config (front matter, meta-file, config.yml).
- Add --list-themes CLI command to print available Pygments and Skylighting themes in columns and point to bundled Shiki themes.
- Add -p/--paginate flag to page terminal/cli/terminal256 output through a user-configurable pager instead of writing directly to stdout.
- Add paginate: true config option (and terminal.paginate) to enable pagination by default for terminal-style output while still allowing per-run overrides.

### Improved

- When Shiki exits with an error (e.g. language not specified and cannot be auto-detected), the code block is left as plain text instead of failing.

### Fixed

- Prevent segfault when external code highlighting is enabled without an explicit theme by initializing the code highlight theme option safely in the default configuration.

## [0.1.85] - 2026-03-01

### Changed

- Makefile "make man" now generates man pages with the built apex binary (apex -t man) after running "make build"; pandoc and go-md2man are no longer required.
- When both a TOC marker in code and a TOC marker in normal flow exist, only the first marker that is not inside <code> or <pre> is replaced with the generated table of contents.

### New

- Definition list terms may use Kramdown-style double colon :: as well as single : before the definition (e.g. "term:: definition").
- Man page creation
- Man-html without -s/--standalone outputs content snippet only (no wrapper, no nav); with -s outputs full document with sidebar and headline.
- Man-html standalone: fixed left sidebar nav (TOC) for top-level sections only (NAME, SYNOPSIS, etc.), large headline from NAME section (command and description), document_title metadata used when present (e.g. APEX(1)).
- Man-html standalone: custom CSS via --css/--style emitted as <link rel="stylesheet"> after embedded style; optional syntax highlighting via --code-highlight (pygmentize/skylighting) for code blocks in both snippet and standalone output.

### Improved

- CMake man page generation uses only apex_cli -t man when pre-generated pages are missing; removed duplicate/broken pandoc branch from merge and fixed invalid add_custom_command structure.
- Man-html CSS: bold and headline color #a02172, links #2376b1, section headings #3f789b, sidebar background #f5f4f0 and border #e0ddd6.
- CLI help: --css/--style describes use with man-html and -s; -s/--standalone describes man-html behavior (with -s: nav sidebar and full page; without -s: snippet only).

### Fixed

- Man pages generated from Markdown no longer convert option hyphens to em dashes; pandoc is invoked with -f markdown-smart so -- stays as literal ASCII double hyphen in roff output.
- When both MMD inline abbreviations [>(abbr) expansion] and reference-style definitions [>abbr]: expansion appear in the same document, all abbreviations are now wrapped in <abbr> tags instead of only the reference-style ones (inline entries were previously overwritten in the list).
- Definition list terms and definitions now render with correct content when using "**term**" followed by ":: definition" (previously produced empty <dt></dt><dd></dd>).
- TOC markers ({{TOC}}, <!--TOC--> and variants) inside inline code (backticks) or inside code blocks (fenced or indented) are no longer expanded; they are left as literal text in the output.

## [0.1.84] - 2026-02-27

### Changed

- Homebrew formula bumped to 0.1.83 with updated release checksum

### New

- --width flag wraps terminal and terminal256 output to a fixed column width for better integration with terminal file managers and previews
- Terminal.width metadata and config option lets you set a default wrap width per document instead of relying only on CLI flags
- Terminal.theme metadata and config option lets documents select a default terminal theme when using -t terminal or -t terminal256
- Terminal theme list_marker style controls the color and emphasis of bullet and numbered list markers, defaulting to bold bright red when unset

### Improved

- Terminal themes can now mark headings, links, code spans, code blocks, blockquotes, and tables as bold with a bold: true flag in theme YAML instead of encoding bold in the color string
- Span_classes mappings in terminal themes style inline span classes and attribute list classes consistently in terminal and terminal256 output

## [0.1.83] - 2026-02-26

### New

- Span_classes theme mapping lets you define styles for inline span classes in terminal and terminal256 output

### Improved

- Terminal output now respects classes from inline attribute lists and bracketed spans (e.g. *emphasis*{.tag} and [text]{.tag}) including spans generated by plugins
- Terminal theme YAML parsing is more robust so existing themes continue to work even when libyaml is unavailable

## [0.1.82] - 2026-02-25

### New

- Add -t/--to output formats for html, json, json-filtered/ast-json/ast, markdown/md, mmd, commonmark/cmark, kramdown, gfm, terminal/cli, and terminal256.
- Add terminal and terminal256 ANSI renderers with theme support and compact list and blockquote formatting for comfortable reading in a TTY.
- Add --theme option and support for user theme files in ~/.config/apex/terminal/themes/NAME.theme with automatic default.theme selection when no explicit theme is given.
- Add JSON and AST JSON output points before and after AST filters so external tools can consume either the raw or fully-processed document structure.

### Improved

- Improve MultiMarkdown output so TOC markers and escaping work correctly in -t mmd output without breaking MMD-specific syntax.
- Improve terminal table rendering with Unicode box drawing, column alignment, captions, footer rules, and advanced colspan/rowspan handling that more closely matches advanced_tables behavior.
- Terminal output now replaces :emoji: patterns with Unicode emoji when in GFM or unified mode, matching HTML behavior.

### Fixed

- Replace APEXLTLT placeholders with literal << in terminal table cells so escaped \<< renders correctly in CLI output.
- Integrate external code highlighters (Pygments/Skylighting) into terminal output when --code-highlight is enabled, using readable pastel styles appropriate for 8-color and 256-color modes.
- Compact list item spacing and blockquote rendering in terminal output so lists, quotes, and callouts read cleanly without stray blank lines.

## [0.1.81] - 2026-02-23

### New

- Tests for extended syntax in indented and fenced code blocks, inline code, nested list lines with 4-space indent, and real indented code without list markers

### Improved

- Lines that start with a list marker after 4+ spaces or tab (nested or continuation list lines) are no longer treated as code blocks, so sup/sub and highlight are still applied there

### Fixed

- Superscript (^), subscript (~), underline (~text~), strikethrough (~~), and highlight (==) are no longer processed inside indented code blocks (4+ spaces or tab)
- Extended syntax remains skipped inside fenced code blocks and inline code as before

## [0.1.80] - 2026-02-18

## [0.1.79] - 2026-02-18

### New

- IAL attributes for picture formats: webp, avif (emit <picture> with srcset), and video formats: webm, ogg, mp4, mov, m4v (emit <video> with <source> elements)
- IAL attribute "auto" discovers format variants (2x, 3x, webp, avif, video) from filesystem and expands img to picture/video when files exist
- Video URLs (mp4, webm, ogg, mov, m4v, ogv) automatically render as <video> elements instead of <img>
- --[no-]image-captions and --[no-]title-captions-only CLI options to control figure/figcaption wrapping (title-captions-only: only add captions for images with title, alt-only images get no caption)
- Image URL ending in .* (e.g. ![](image.*)) auto-discovers format variants from filesystem, same as auto attribute

### Improved

- Image attribute matching uses URL + alt to disambiguate same-src images when injecting IAL attributes
- Picture elements with title or alt now get figure/figcaption wrapping when image captions are enabled

### Fixed

- TOC HTML structure now produces valid ul > li > ul nesting instead of invalid ul > ul (nested lists inside list items, never ul directly in ul)
- Image captions from title: ![alt](url "Title caption") now correctly uses the title for figcaption instead of alt text (quoted titles were being stripped by preprocessor before cmark could parse them)
- Wildcard image syntax (![](image.*)) now expands correctly when document contains image examples in code blocks (e.g. `![](image.*)` in documentation)
- Truncated </figure> tag in picture output (memcpy used wrong length for "</figcaption></figure>")
- Invalid HTML5: strip <p> wrapper around figure, video, and picture elements (p may only contain phrasing content)
- Auto media expansion when replacement exceeds buffer (grow buffer instead of falling back to original img tag)

## [0.1.78] - 2026-02-13

### New

- Test added to verify autolinking does not run inside indented code blocks.

### Fixed

- Autolink preprocessor now skips indented code blocks (4+ spaces or tab at line start) so URLs inside them are not converted to links.

## [0.1.77] - 2026-02-09

### Fixed

- Package.swift missing filters
- Package.swift missing ast_json

## [0.1.76] - 2026-02-07

### Changed

- Formula/apex.rb: version 0.1.75 and release sha256.

### New

- Install filter by path: directory JSON can specify "path" (e.g. "contrib/code-includes.lua"); apex clones repo to temp, copies that file to filters/<basename>, then removes temp.
- Code blocks with info string "inc" emit Pandoc keyvals [["inc","yes"]] in AST JSON so code-includes-style filters can replace block content with file contents.

### Improved

- Uninstall: when filter is not a directory, try filters/NAME.lua, NAME.py, NAME.rb and remove that file.
- Filter resolution: --filter NAME now resolves to filters/NAME, filters/NAME.lua, filters/NAME.py, or filters/NAME.rb (regular files) before trying NAME as a directory.

### Fixed

- .lua filters are run via `lua "path"` so scripts without a shebang work.

## [0.1.75] - 2026-02-06

## [0.1.74] - 2026-02-06

### New

- AST filters (Pandoc-style JSON): run filters via --filter, --filters, and --lua-filter; apex_options gains ast_filter_commands, ast_filter_count, and ast_filter_strict
- --install-filter to install AST filters from the apex-filters directory or from a Git URL
- --no-strict-filters to skip failing filters and invalid filter JSON instead of aborting
- Div blocks in Pandoc JSON are now parsed (attributes skipped, inner blocks appended to the document)
- --list-filters lists installed filters (from the user filters directory) and available filters from the central apex-filters directory, with titles, authors, descriptions, and homepages for available entries
- --uninstall-filter ID removes a filter by id (file or directory) after a confirmation prompt; cannot be combined with --install-filter

### Improved

- Silence C compiler narrowing warnings in the abbreviations extension so embedding Apex and building language bindings can run with cleaner, warning-free builds
- Images already inside a <figure> (e.g. from ::: >figure) are no longer wrapped again in figure/figcaption by the image-caption logic
- Redundant <p> around a single <img> inside <figure> is stripped so ::: >figure with "< ![Image](...)" yields <figure><img...></figure> without an inner paragraph
- Pandoc JSON parser accepts block and inline objects with keys in any order (e.g. "c" before "t" as emitted by dkjson/Lua) so filter output from Lua and other generators parses correctly
- Filter directory JSON parsing is shared for both "plugins" and "filters" arrays; apex_remote_fetch_filters_directory fetches and parses apex-filters.json for list/install
- Apex_remote_print_plugins_filtered accepts an optional noun so the empty-list message says "filters" when listing filters instead of "plugins"

### Fixed

- Pandoc JSON parser now consumes the closing "c" array bracket after Header inlines so multi-block filter output (e.g. unwrap filter with heading plus figure) parses correctly and renders full output
- Multi-block Pandoc JSON (e.g. Header + RawBlock) now parses to all blocks instead of only the first; parse_blocks_array returns the container and callers adopt its children then free it so the block chain is preserved

## [0.1.73] - 2026-02-04

### New

- Multi-image test coverage ensures that a reference-style image with attributes between two @2x images keeps its attributes without gaining an unwanted 2x srcset, and that the surrounding images still emit correct srcset values
- @3x image attribute adds support for 3x retina assets and automatically emits srcset entries for 1x, 2x, and 3x variants of the same path for both inline and reference-style images

### Fixed

- @2x image attribute no longer mangles domains or query-string-only URLs; srcset 2x URLs are only generated when the path has a real file extension and the domain portion is never altered
- Reference-style image attributes (width, height, style, classes, id) are correctly applied in Unified, MultiMarkdown, and GFM modes, even when mixed with inline images and fenced div/figure blocks
- Reference-style image attributes no longer leak across images; inline images with @2x keep their own srcset and reference-style images only gain 2x srcset when explicitly marked

## [0.1.72] - 2026-02-02

### Changed

- --indices now enables mmark, TextIndex, and Leanpub syntax; Leanpub is on by default in unified mode.
- Index anchor IDs use idxref-N format (was idxref:N) to avoid emoji processing corrupting placeholders.

### New

- Add Leanpub index syntax: {i: term}, {i: "term"}, and {i: "Main!sub"} for hierarchical entries under headings.
- {index} marker: write {index} in the document to place the generated index at that position (replaced with <!--INDEX--> internally).

### Fixed

- TextIndex [term]{^} no longer includes the closing bracket in the index entry (e.g. "fresh" instead of "fresh]").
- Leanpub {i: term} is no longer consumed by MMD metadata or parsed as IAL; content is preserved in output.

## [0.1.71] - 2026-01-30

### Changed

- Strikethrough is now controlled by the enable_strikethrough option instead of mode; can be enabled in modes that lack it by default (commonmark, mmd, kramdown) and disabled in modes that include it (gfm, unified).

### New

- Add --strikethrough and --no-strikethrough CLI flags to enable or disable GFM-style ~~strikethrough~~ processing independent of mode.
- Strikethrough is configurable via metadata using the strikethrough or strike-through keys (true/false).

## [0.1.70] - 2026-01-30

### Changed

- Strikethrough is now controlled by the enable_strikethrough option instead of mode; can be enabled in modes that lack it by default (commonmark, mmd, kramdown) and disabled in modes that include it (gfm, unified).

### New

- Add --strikethrough and --no-strikethrough CLI flags to enable or disable GFM-style ~~strikethrough~~ processing independent of mode.
- Strikethrough is configurable via metadata using the strikethrough or strike-through keys (true/false).

## [0.1.69] - 2026-01-30

### Improved

- URLs with a protocol (http, https, etc.) are no longer URL-encoded so query parameters in image URLs are preserved.
- Image reference definitions with key= attributes or bare @2x after the URL are now split and applied (width, height, srcset) in all modes, not only when image attributes are enabled.

### Fixed

- Paragraph IAL on the same line (no blank line), e.g. "Text\n{: .lead }", now applies the class to the paragraph.
- Block IAL (e.g. {: .lead }) inside fenced divs and markdown="1" blocks now applies to the previous paragraph and the IAL line is removed from output.
- Block IAL is now recognized when an HTML block (e.g. </div>) sits between the content paragraph and the IAL paragraph.
- Reference-style images (e.g. ![alt][ref] with [ref]: url width=250 height=83) no longer produce blank output; attributes are applied and refs are expanded correctly.
- First line of document that looks like a Markdown link or image (e.g. [![...](#){: .class }]) is no longer consumed as MMD metadata, fixing blank output for such content.
- Image width, height, and other attributes from reference definitions are now applied to images inside fenced divs (::: ... :::) and other HTML elements with markdown="1"; they were previously dropped when inner content was parsed and rendered separately.

## [0.1.68] - 2026-01-30

### Changed

- Run image caption conversion whenever HTML is produced so caption="" is always honored; pass enable_image_captions and title_captions_only into the converter.

### New

- Allow caption="TEXT" attribute on images (IAL); always wraps in figure/figcaption even when --image-captions is disabled, and strips the caption attribute from the emitted img tag.
- Add --title-captions-only and --no-title-captions-only CLI flags.
- Add title-captions-only / title_captions_only metadata support.

### Improved

- When --title-captions-only is set, automatically enable image captions so both flags are not required.

## [0.1.67] - 2026-01-30

### New

- Image attribute @2x in inline, reference-style, or IAL (e.g. ![alt](url @2x) or [ref]: url @2x) emits srcset="url 1x, url@2x 2x"; the 2x asset is assumed to have the same path with @2x before the extension (e.g. icon.png -> icon@2x.png).

### Fixed

- SPM builds no longer fail with "config.h" file not found or "Could not build module 'CcmarkGFM'" when adding Apex as an integrated Xcode package or as a Swift Package dependency.

## [0.1.66] - 2026-01-30

### New

- Image attribute @2x in inline, reference-style, or IAL (e.g. ![alt](url @2x) or [ref]: url @2x) emits srcset="url 1x, url@2x 2x"; the 2x asset is assumed to have the same path with @2x before the extension (e.g. icon.png -> icon@2x.png).

### Fixed

- SPM builds no longer fail with "config.h" file not found or "Could not build module 'CcmarkGFM'" when adding Apex as an integrated Xcode package or as a Swift Package dependency.

## [0.1.65] - 2026-01-29

### Fixed

- SPM builds no longer fail with missing config.h or cmark-gfm_export.h when adding Apex via Swift Package Manager.
- SPM builds no longer fail at link time with undefined symbol _apex_apply_syntax_highlighting when adding Apex via Swift Package Manager.

## [0.1.64] - 2026-01-29

### Fixed

- Require Marked-style include syntax (`<<[file]`, `<<(file)`, `<<{file}`) to appear at the very beginning of a line, preventing indented code blocks from being processed as includes.
- Normalize even-numbered fenced code block delimiters to odd backticks so include syntax inside fenced code blocks (e.g. ````...````) is never processed; only fence delimiters at line start are normalized, not backticks inside code block content.
- Pandoc fenced divs using custom element names (e.g. ::: >custom-element customclass) now output correct HTML: the custom element wraps its paragraphs instead of the opening tag being incorrectly wrapped in a <p> (fixes GitHub issue #5).
- Escaped `\<<` in table cells now renders as literal `<<` in HTML (placeholder APEXLTLT is replaced with &lt;&lt; in table HTML postprocess).
- Rowspan no longer puts cell content in the opening tag (e.g. <td Engineering rowspan="2">); rowspan is now applied correctly so the first cell shows <td rowspan="2">A</td>.
- When injecting rowspan/colspan into table HTML, only the tag name (<td or <th) is copied then attributes are injected; content between tag name and ">" is skipped so malformed or erroneously placed content is never written into the tag.

## [0.1.63] - 2026-01-29

### Fixed

- Pandoc fenced divs using custom element names (e.g. ::: >custom-element customclass) now output correct HTML: the custom element wraps its paragraphs instead of the opening tag being incorrectly wrapped in a <p> (fixes GitHub issue #5).
- Escaped `\<<` in table cells now renders as literal `<<` in HTML (placeholder APEXLTLT is replaced with &lt;&lt; in table HTML postprocess).

## [0.1.62] - 2026-01-29

### Fixed

- Require Marked-style include syntax (`<<[file]`, `<<(file)`, `<<{file}`) to appear at the very beginning of a line, preventing indented code blocks from being processed as includes.
- Normalize even-numbered fenced code block delimiters to odd backticks so include syntax inside fenced code blocks (e.g. ````...````) is never processed; only fence delimiters at line start are normalized, not backticks inside code block content.

## [0.1.61] - 2026-01-28

### Fixed

- Preserve literal include syntax inside backticked code spans so examples like `<<[path/file]` render correctly in HTML without being processed.

## [0.1.60] - 2026-01-28

### Changed

- Convert images with alt or title text to <figure> and <figcaption> in supported modes while leaving plain images unchanged.

### New

- Add --[no-]image-captions CLI flag to control automatic figure and figcaption wrapping of images.
- Enable automatic image captions by default in MultiMarkdown and Unified modes, configurable per document via metadata.

### Improved

- Add regression tests covering image caption behavior across modes and configuration combinations.

### Fixed

- Prevent image titles and other inline image attributes from bleeding onto different images when parsing and rendering image attributes.

## [0.1.59] - 2026-01-27

### Changed

- Show plugin metadata (id, title, author, description, homepage) in a stable order grouped under a single Installed Plugins section when using --list-plugins

### New

- --wikilink-sanitize  to sanitize the wiki links
- Add comprehensive --wikilink-sanitize documentation to Command-Line-Options.md wiki page
- Support ++insert++ syntax that renders to <ins> and supports IAL attributes for ids and classes on inserted spans
- Discover project plugins from .apex directories in the current directory, base directory, and git repository root in addition to global config
- List installed plugins from project and global .apex/plugins roots in the CLI, following the same precedence rules as runtime plugin loading so users can see exactly which plugins are active for a given project

### Improved

- Highlight detection logic now properly validates both opening and closing == markers with stricter requirements
- Allow project plugins to shadow global plugins with the same id so projects can locally override or disable user-global plugins
- Add APEX_SUPPRESS_HIGHLIGHT_WARNINGS environment variable to suppress noisy missing-syntax-highlighter warnings when running in controlled environments
- Read configuration from project .apex/config.yml (with global config as a fallback) so per-project defaults override user-wide settings while keeping existing metadata and CLI precedence intact

### Fixed

- Highlight conversion now correctly rejects === (three equals) and only matches == (exactly two equals) for mark tags
- Highlight conversion no longer matches when preceded by = or + characters
- Highlight conversion no longer matches when followed by + character
- Highlight conversion no longer matches when closing == is preceded by space or followed by = or +
- Honor MultiMarkdown-style image attributes on reference-style images in unified and MultiMarkdown modes so definitions like [id]: path \"Title\" class=center width=300 height=200 style=\"...\" correctly apply class, width, height, and style
- Honor MultiMarkdown-style image attributes on inline and reference-style images in unified and MultiMarkdown modes so syntax like ![alt](/img.jpg \"Title\" class=center width=300 height=200 style=\"...\") or [id]: /img.jpg \"Title\" class=center width=300 height=200 style=\"...\" correctly applies class, width, height, and style to the rendered img tag
- Preserve MMD6-style image titles in parentheses like ![Image](image.png (Parentheses title)) so they render a proper title attribute instead of leaving a stray closing parenthesis in the output

## [0.1.58] - 2026-01-15

### Fixed

- Missing test files

## [0.1.57] - 2026-01-15

### New

- Definition lists now support blank lines between the term and first definition, and between multiple definitions. Terms are preserved across blank lines and definition lists remain open across blank lines instead of closing prematurely.

### Improved

- Expanded test coverage

### Fixed

- Definition lists with blank lines between definitions no longer create separate <dl> blocks - all definitions for a term are now grouped in a single definition list.
- Terms with blank lines before their definitions are now correctly converted to <dt> tags instead of being rendered as paragraphs.

## [0.1.56] - 2026-01-14

### New

- Add test_marked_integration.c test file for Marked integration features

## [0.1.55] - 2026-01-14

### New

- Add test_marked_integration.c test file for Marked integration features

## [0.1.54] - 2026-01-14

### New

- --highlight-language-only flag to only highlight code blocks that have a language specified (via ```language or IAL), leaving plain code blocks unhighlighted
- Config file support for code-highlight option (accepts pygments, skylighting, or abbreviations p/pyg, s/sky)
- Config file support for code-line-numbers option (boolean)
- Config file support for highlight-language-only option (boolean)
- Display 256-color ANSI art logo alongside version info when running `apex --version` in a wide terminal (>=70 columns)

### Improved

- Version output detects terminal capabilities and falls back gracefully (narrow terminal, piped output, NO_COLOR set)
- Logo uses transparent background, blending with user's terminal colors instead of forcing black rectangle

### Fixed

- Blank line above ASCII art in --version output

## [0.1.53] - 2026-01-13

### New

- Display ASCII art logo alongside version info when running `apex --version` in a wide enough terminal (>=80 columns)
- --code-highlight TOOL flag for syntax highlighting via external tools (pygments, skylighting, or abbreviations p, s)
- --code-line-numbers flag to include line numbers in syntax-highlighted code blocks (requires --code-highlight)
- --css flag now accepts multiple stylesheets via repeated flags or comma-separated list (e.g., --css style.css --css syntax.css or --css style.css,syntax.css)
- Automatic GitHub-style syntax highlighting CSS embedded when --code-highlight is used (covers both Pygments and Skylighting class names)
- Add test runner `--badge` mode that outputs pass/fail count (e.g., "981/981") for badge generation

### Improved

- Version output automatically detects terminal capabilities and falls back to text-only when logo cannot display (narrow terminal, piped output, NO_COLOR set)
- --embed-css now embeds all specified stylesheets as inline <style> blocks
- Test output in errors-only mode (`-e`) now only prints suite titles for suites with failures

## [0.1.52] - 2026-01-11

### Changed

- Per-cell alignment is now enabled by default only in unified mode, disabled in all other modes (CommonMark, GFM, MultiMarkdown, Kramdown)

### New

- Table preprocessing converts consecutive pipes without whitespace (|||) to << markers for colspan detection, distinguishing between whitespace-separated empty cells (|  |  |) which remain separate and consecutive pipes (|||) which create colspans.
- Table cells can now specify alignment by adding colons at the start and/or end of cell content: leading colon (:) for left-align, trailing colon (:) for right-align, or both (:content:) for center-align. The colons are stripped from the output and replaced with style="text-align: ..." attributes.
- Added --per-cell-alignment and --no-per-cell-alignment CLI flags to enable or disable per-cell alignment markers in tables
- Added tests to verify colspan behavior with consecutive pipes vs empty cells with whitespace

### Improved

- Colspan attribute injection in HTML post-processing now includes fallback matching that checks nearby rows when position-based matching fails, ensuring colspan attributes are applied correctly even when row indices shift due to removed cells.
- HTML post-processing now removes cells containing &lt;&lt; markers (entity-encoded <<) that were missed by AST-level removal.
- Cell text extraction for attribute matching now recursively checks nested text nodes (paragraphs, etc.) to properly match cells with complex content structures.
- Colspan merge logic now preserves cell alignment styles when cells are merged together, ensuring alignment attributes are maintained correctly when cells span multiple columns.
- Per-cell alignment processing is now conditional and only runs when the feature is enabled, improving performance when disabled
- Row-header detection in tables now correctly identifies empty first header cells

### Fixed

- Colspan detection now only triggers on cells with << markers (from consecutive pipes), not plain empty cells, so whitespace-separated pipes create separate empty cells as intended.
- Cells with << markers now always merge with previous cell to create colspan, even when followed by additional content in subsequent cells.
- Colspan now only merges consecutive empty cells (|||), not empty cells with whitespace between pipes (|    |)
- Email autolinking no longer converts @ symbols in URLs to email addresses (e.g., Mastodon profile links like https://hachyderm.io/@ttscoff)
- Email autolinking now requires that the @ symbol is preceded by an alphanumeric character (not space or punctuation)
- Email autolinking now requires a TLD (top-level domain) to match, so only [user]@[domain].[tld] format is autolinked
- Email autolinking is now skipped inside markdown link URLs [text](url) to prevent incorrect conversions
- Row-header tables (tables with empty first header cell) now correctly convert first-column body cells to `<th scope="row">` even when `relaxed_tables` is disabled

## [0.1.51] - 2026-01-09

### New

- Support for : Caption syntax before tables, with or without IAL attributes
- Caption format now works before tables (previously only worked after tables)
- Added tests for : Caption before tables (basic, with IAL, without blank line)
- Add regression test to ensure table parsing works correctly when files don't end with a newline, preventing future regressions
- Add regression test to ensure table parsing works correctly when files use CR line endings, preventing future regressions with Table: Caption syntax

### Improved

- Definition list processor now skips : Caption lines that are followed by tables to avoid conflicts
- Table caption detection now handles blank lines between captions and tables
- Paragraph removal logic now recognizes and removes : Caption format paragraphs from output
- Refactored test suite to use centralized test_result() and test_resultf() helper functions instead of scattered printf statements with manual errors_only_output checks

### Fixed

- Buffer overflow in stdin reading that caused segfaults when piping from pbpaste
- Prevent memory leak in is_table_caption by only storing full_text in user_data when caption is confirmed, not before validation
- Fix potential crash when processing multiple tables by ensuring full_text is properly freed when caption validation fails
- Fix double-free in add_table_caption by checking for existing caption before freeing user_data, preventing use-after-free errors
- Fix : Caption lines before tables being incorrectly parsed as definition lists by always treating them as captions when followed by a table, regardless of IAL presence or blank lines
- Fix table parsing issue where last row of first table is not parsed when file doesn't end with a newline by normalizing input to always end with a line ending character before preprocessing and parsing
- Normalization breaking file includes
- Fix table parsing issue where last row of first table is not parsed when file doesn't end with a newline by normalizing input to always end with a line ending character before table preprocessing and final parsing
- Fix Table: Caption syntax not being processed when file uses CR line endings by updating table caption preprocessing to handle CR, LF, and CRLF line endings correctly
- --errors-only flag now correctly suppresses all passing test output, including negative tests that pass

## [0.1.50] - 2026-01-04

### Changed

- Removed rouge_css function from documentation generators (generate_docset.rb, generate_single_html.rb, generate_app_docs.rb) in favor of using the shared stylesheet approach.

### New

- Added generate_docset.rb script to generate Dash docsets from Apex documentation
- Added support for single-page CLI options docset using mmd2cheatset
- Added support for multi-page docset from wiki files with full navigation
- Added shared_styles.css with common styling for all documentation generators
- Added shared_scripts.js with hamburger menu functionality for mobile navigation
- Added hamburger menu button for mobile navigation that slides sidebar in from left
- Added mobile menu overlay that closes sidebar when clicked

### Improved

- Documentation generators now use shared CSS and JavaScript files for consistency
- Sidebar width increased to 180-250px on larger screens to prevent menu item wrapping
- Hamburger menu fades to 0.2 opacity when not hovered, full opacity on hover
- Hamburger menu repositions to right of sidebar when menu is open
- Hash handling on page load now supports both page IDs and section IDs
- Added hashchange event listener to handle URL hash changes after page load
- Moved Rouge syntax highlighting CSS to shared stylesheet (shared_styles.css) for better maintainability. All documentation generators now use the centralized GitHub theme CSS instead of generating it dynamically.
- Documentation generators now prioritize build/apex over build-release/apex when searching for the Apex binary, ensuring the most recently built version is used for documentation generation.

### Fixed

- TOC link navigation now uses getAbsoluteTop function for reliable scroll positioning
- Hash navigation in single-page HTML files now correctly shows page and scrolls to section
- TOC links in C API and other pages now scroll correctly instead of only moving 5px
- Definition lists are no longer incorrectly processed inside fenced code blocks. The definition list preprocessor now detects code block boundaries and skips processing when inside a code block, preserving the literal markdown syntax in code examples.
- Definition lists are no longer incorrectly processed inside fenced code blocks. The definition list preprocessor now detects code block boundaries and distinguishes between closing fences (```) and opening fences (```markdown). When inside a code block, only closing fences exit the block, while opening fences with language identifiers are treated as content, preventing definition list syntax from being rendered as HTML in code examples.
- Fixed double-free memory error in definition list preprocessor by setting ref_definitions to NULL after freeing and adding NULL checks in error paths to prevent attempting to free already-freed memory.

## [0.1.49] - 2026-01-03

### New

- Added standalone HTML document generation method with stylesheet and title support
- Added pretty-printing option method for formatted HTML output
- Added dictionary-based options method for flexible configuration
- Added Swift-friendly convenience method combining common options (generateHeaderIDs, hardBreaks, pretty)
- Added instance methods (apexHTML and apexHTMLWithMode) for fluent NSString usage
- Added Swift API wrapper (Apex.swift) with type-safe ApexMode enum and ApexOptions struct
- Added String extensions providing idiomatic Swift API for Markdown conversion
- Added static Apex converter struct for functional-style Swift usage
- Swift Package Manager (SPM) support - Apex can now be added as a package dependency in Xcode projects via SPM
- IOS support - Apex now supports iOS 11+ in addition to macOS 10.13+ through bundled libyaml dependency
- Module map for Swift C API imports - Added module.modulemap to enable direct C API access from Swift
- Add Package.swift for Swift Package Manager integration
- Add module.modulemap for Swift/Objective-C interop
- Add SPM test script and test project

### Improved

- Framework build now includes module map for proper Swift module support
- Add default initializer to ApexOptions struct for better Swift ergonomics

### Fixed

- Swift/Objective-C bridging issues in Apex.swift to work correctly with SPM module structure
- Fix node type declarations to use enum values instead of extern variables for better module compatibility

## [0.1.48] - 2026-01-03

### Fixed

- Linux build error: add missing stdint.h include to emoji.c for SIZE_MAX definition

## [0.1.47] - 2026-01-03

### Changed

- Updated Objective-C API to use mode constants instead of string literals in default implementation

### New

- Added emoji autocorrect option to enable automatic conversion of emoji names like :rocket: to Unicode emoji characters
- Added progress indicator option to show processing progress on stderr for operations longer than 1 second

### Improved

- Added mode constants (ApexModeCommonmark, ApexModeGFM, etc.) to Objective-C API for better type safety and code clarity

## [0.1.46] - 2026-01-03

## [0.1.45] - 2026-01-03

### Improved

- Table caption preprocessing now handles blank lines between tables and captions by buffering and discarding them appropriately
- Table matching in HTML renderer now uses sequential matching instead of index-based matching to correctly handle tables with attributes
- Caption detection now concatenates all text nodes in a paragraph to ensure IAL attributes are found even when split across nodes

### Fixed

- Table captions now work correctly when separated from tables by blank lines
- IAL attributes (id, class) are now correctly extracted and applied to tables with captions
- Table: caption format now works when appearing immediately after table rows without blank lines
- Table: caption format now supports case-insensitive "table:" in addition to "Table:"

## [0.1.44] - 2026-01-02

### Changed

- Updated emoji_entry structure to support both unicode and image-based emojis with separate unicode and image_url fields
- Table alignment test now accepts both align="center" and style="text-align: center" attributes to be more flexible with cmark-gfm's output format

### New

- Fenced divs now support specifying different HTML block elements using >blocktype syntax (e.g., ::: >aside {.sidebar} creates <aside> instead of <div>)
- Support for common HTML5 block elements: aside, article, section, details, summary, header, footer, nav, and custom elements
- Block type syntax works with all attribute types including IDs, classes, and custom attributes
- Added comprehensive test coverage for block type feature including nesting, multiple attributes, and edge cases
- In GFM format, emojis in headers are converted to their textual names in generated IDs (e.g., #  Support -> id="smile-support"), matching Pandoc's GFM behavior
- Added support for all 861 GitHub emojis (expanded from ~200), including 14 image-based emojis like :bowtie:, :octocat:, and :feelsgood:
- Added emoji name autocorrect feature using fuzzy matching with Levenshtein distance algorithm to correct typos and formatting errors in emoji names
- Added --emoji-autocorrect and --no-emoji-autocorrect command-line flags to control emoji autocorrection
- Emoji autocorrect is enabled by default in unified mode and can be enabled in GFM mode
- Image-based emojis in headers now use em units (height: 1em) for proper scaling instead of fixed pixel sizes

### Improved

- Fenced div block types can be nested and mixed with regular divs
- Emoji processing now validates that only complete :emoji_name: patterns are processed (requires at least one character and no spaces between colons)
- Emoji names are normalized (lowercase, hyphens to underscores) before matching to handle case variations and formatting differences
- Table processing now completes in under 30ms for large tables (previously timing out after 5+ seconds) by avoiding expensive string comparisons when no changes are made
- Added early exit in table attribute injection to skip processing for simple tables without special attributes, avoiding expensive AST traversal for tables that don't need it
- Table post-processing performance significantly improved for large tables (2600+ cells) by implementing lazy cell content extraction, only extracting content when needed for attribute matching or alignment processing
- Added timeout protection (10 seconds) to table post-processing loop to prevent hangs on extremely large tables, gracefully exiting and returning processed HTML
- Per-cell alignment processing now disabled automatically for tables with more than 1000 cells to avoid timeouts, while column alignment from delimiter rows continues to work (handled by cmark-gfm)
- Optimized alignment colon detection to only check first and last non-whitespace characters instead of scanning entire cell content, reducing character comparisons by ~50x
- Added early exit for tables with only simple attributes (no rowspan/colspan/data-remove) to skip expensive HTML processing when no complex features are needed
- Content-based cell matching now skipped for tables with more than 500 attributes to avoid performance degradation
- Added CMARK_OPT_LIBERAL_HTML_TAG option when unsafe mode is enabled to allow cmark-gfm to properly recognize inline HTML tags instead of encoding them
- MacOS binaries now use @rpath for libyaml instead of hardcoded Homebrew paths, allowing the binary to work when copied to /usr/local/bin as long as libyaml is installed in /usr/local/lib or /opt/homebrew/lib
- Emoji name resolution now prefers longer, more descriptive names (e.g., "thumbsup" over "+1")
- Header ID generation now normalizes common Latin diacritics (e, a, c, etc.)
- Table rowspan matching now extracts cell content for more accurate matching

### Fixed

- Fixed issue where partial emoji patterns or empty patterns could cause incorrect matches
- Emoji replacement now correctly ignores table alignment patterns like :---: and :|: to prevent incorrect emoji processing in table delimiter rows
- Emoji patterns (like :bowtie:) inside HTML tag attributes are now correctly ignored and not processed, preventing mangled HTML output when emojis appear in attributes like title=":emoji:"
- Autolink processing now skips URLs inside HTML tag attributes, preventing URLs in attributes like src="https://..." from being converted to markdown links
- Inline HTML tags like <img> are no longer HTML-encoded when --unsafe is enabled, preserving raw HTML in paragraphs, blockquotes, and definition lists
- Header IDs with emojis now correctly replace cmark-gfm's auto-generated IDs instead of being skipped, ensuring custom ID formats (like emoji-to-name conversion) are applied
- Emoji processing now correctly skips index placeholders (<!--IDX:...-->)
- Table processing now correctly detects and processes rowspan markers (^^)
- Table attribute processing optimization no longer incorrectly skips rowspan/colspan attributes
- Table formatting in fixture
- Remove unused function

## [0.1.43] - 2025-12-31

### Changed

- Reference image attribute expansion now converts IAL

  attributes (ID, classes) to key=value format for
  compatibility with inline image parsing

- Test suite refactored: split test_runner.c into multiple

  files (test_basic.c, test_extensions.c, test_helpers.c,
  test_ial.c, test_links.c, test_metadata.c, test_output.c,
  test_tables.c) for better organization

- Test fixtures reorganized: moved all .md test files from

  tests/ to tests/fixtures/ with subdirectories (basic/,
  demos/, extensions/, ial/, images/, output/, tables/)

### New

Support for Pandoc-style table captions using `: Caption` syntax (in addition to existing `[Caption]` and `Table: Caption` formats)

IAL attributes in table captions are now extracted and applied to the table element (e.g., `: Caption {#id .class}` applies `id` and `class` to the table)

Support for Pandoc-style IAL syntax without colon prefix (`{#id .class}` in addition to Kramdown `{:#id .class}` format)

- Support Pandoc-style IAL syntax ({#id .class}) in addition

  to Kramdown-style ({: #id .class}) for all IAL contexts
  including block-level, inline, and paragraph IALs

- Extract_ial_from_text function now recognizes both {: and

  {# or {. formats when extracting IALs from text

- Extract_ial_from_paragraph function now accepts

  Pandoc-style IALs for pure IAL paragraphs

- Process_span_ial_in_container function now processes

  Pandoc-style IALs for inline elements like links, images,
  and emphasis

- Is_ial_line function now detects Pandoc-style IAL-only

  lines in addition to Kramdown format

- Support Pandoc-style IAL syntax ({#id .class}) in addition

  to Kramdown-style ({: #id .class}) for all IAL contexts
  including block-level elements, paragraphs, inline
  elements, and headings

- Add support for Pandoc fenced divs syntax (::::: {#id

  .class} ... :::::) in unified mode, enabled by default

- Add --divs and --no-divs command-line flags to control

  fenced divs processing

- Add comprehensive test suite for Pandoc fenced divs

  covering basic divs, nested divs, attributes, and edge
  cases

- Add bracketed spans feature that converts [text]{IAL}

  syntax to HTML span elements with attributes, enabled by
  default in unified mode

- Add --spans and --no-spans command-line flags to

  enable/disable bracketed spans in other modes

- Bracketed spans support all IAL attribute types (IDs,

  classes, key-value pairs) and process markdown inside
  spans

- Reference link definitions take precedence over bracketed

  spans - if [text] matches a reference link, it remains a
  link

- Add comprehensive test suite for bracketed spans including

  reference link precedence, nested brackets, and markdown
  processing

- Add bracketed spans examples and documentation
- Add automatic width/height attribute conversion:

  percentages and non-integer/non-px values convert to style
  attributes, Xpx values convert to integer width/height
  attributes (strips px suffix), bare integers remain as
  width/height attributes

- Add support for Pandoc/Kramdown IAL syntax on images:

  inline images support IAL after closing paren like
  ![alt](url){#id .class width=50%}

- Add support for IAL syntax after titles in reference image

  definitions: [ref]: url "title" {#id .class width=50%}

- Add support for Pandoc-style IAL with space prefix: {

  width=50% } syntax works for images

- Add comprehensive test suite for width/height conversion

  covering percentages, pixels, integers, mixed cases, and
  edge cases like decimals and viewport units

- Reference image definitions now preserve and include title

  attributes from definitions like [ref]: url "title" {#id}

### Improved

- Table caption preprocessing now converts `: Caption`

  format to `[Caption]` format before definition list
  processing to avoid conflicts

- HTML renderer now extracts and injects IAL attributes from

  table captions while excluding internal attributes like
  `data-caption`

- IAL parsing automatically detects format and adjusts

  content offset accordingly (2 chars for {: format, 1 char
  for {# or {. format)

- HTML markdown extension now uses CMARK_OPT_UNSAFE to allow

  raw HTML including nested divs in processed content

- Width/height conversion properly merges with existing

  style attributes when both are present

- IAL detection now properly handles whitespace before IAL

  syntax for both inline and reference images

- Reference image expansion now correctly skips IAL even

  when closing paren is not found

### Fixed

- Table caption test assertions now correctly match table

  tags with attributes by using <table instead of <table>

- Extract_ial_from_paragraph now allows newline character

  after closing brace in IAL syntax

- List items with key:value format (e.g., "- Foo: Bar") are

  no longer incorrectly parsed as MMD metadata in unified
  mode

- Fenced divs now add markdown="1" attribute so content

  inside divs is properly parsed as markdown

- HTML markdown extension now preserves all attributes (id,

  class, custom attributes) when processing divs with
  markdown="1"

- HTML markdown extension now recursively processes nested

  divs with markdown="1" attributes

- HTML markdown extension now adds newline after closing div

  tags to ensure following markdown headers and content are
  parsed correctly

- Bracketed spans now correctly handle nested brackets by

  matching outer brackets instead of first closing bracket

- Remove test assertions checking for markdown attribute on

  bracketed spans which is correctly removed by
  html_markdown extension

- IAL syntax with spaces (e.g., { width=50% }) now correctly

  detected and processed for images

- IAL syntax is now properly stripped from output even when

  parsing fails, preventing raw IAL from appearing in HTML

- Reference image definitions with IAL after URL (no title)

  now correctly detected and processed

- Test string length issues: replaced hardcoded lengths with

  strlen() calls to ensure full IAL syntax is processed in
  width/height conversion tests

- Removed unused style_attr_index variable to eliminate

  compiler warning

## [0.1.42] - 2025-12-30

### New

- ALD references can now be combined with additional

  attributes in the same IAL (e.g., {:id .class3} where id
  is an ALD reference and .class3 is an additional class)

### Improved

- When merging ALD attributes with additional attributes,

  duplicate key-value pairs are now replaced instead of
  duplicated (e.g., if ALD defines rel="x" and IAL includes
  rel="y", the result is rel="y")

- Classes from additional attributes are appended to ALD

  classes, and IDs in IALs override ALD IDs when specified

- Enhanced merge_attributes function to properly handle

  attribute key conflicts by replacing existing values
  rather than creating duplicates

## [0.1.41] - 2025-12-30

### New

- Inline Attribute Lists (IALs) can now appear immediately

  after inline elements within paragraphs, not just at the
  end of paragraphs

- IALs can be applied to strong (bold), emphasis (italic),

  and code elements in addition to links and images

- IALs now work with nested inline elements, allowing

  attributes to be applied to italic text inside bold text
  and similar nested structures

- Added comprehensive test suite for inline IAL

  functionality covering links, strong, emphasis, code,
  multiple IALs, and edge cases

- Added IAL demo markdown file (tests/ial_demo.md)

  demonstrating all supported IAL features

- Added script (tests/generate_ial_demo.sh) to automatically

  generate an HTML file with interactive attribute
  inspection tooltips

### Fixed

- IALs (Inline Attribute Lists) are now correctly applied to

  the intended link element when multiple links in a
  document share the same URL

- IALs are now correctly applied to the intended element

  when multiple elements share the same URL or content,
  using separate element counters for each inline element
  type

- Block-level HTML elements now correctly include a space

  between the tag name and attributes when IAL attributes
  are injected

## [0.1.40] - 2025-12-23

### Changed

- Table captions now default to below the table instead of

  above

- Tfoot row detection in AST now separates the logic for

  marking the === row itself versus rows that come after it,
  improving accuracy of tfoot section identification.

- Disallow using --combine and --mmd-merge together to avoid

  ambiguous multi-file behavior

- Update CSV include and inline table handling so both share

  the same CSV-to-table conversion and alignment behavior

### New

- Added --captions option to control caption position (above

  or below)

- Added default CSS styling for figcaption elements in

  standalone output (centered, bold, 0.8em)

- Added CSS styling for table figures to align captions with

  tables (fit-content width)

- Support for tables that start with alignment rows

  (separator rows) without header rows. Column alignment
  specified in the separator row is automatically applied to
  all data columns.

- Support for individual cell alignment in tables using

  colons, similar to Jekyll Spaceship. Cells can be aligned
  independently with :Text (left), Text: (right), or :Text:
  (center). Colons are removed from output and alignment is
  applied via CSS text-align styles.

- Support per-cell alignment markers inside table cells
- Support multiline table cells using trailing backslash

  markers

- Support header and footer colspans based on empty cells
- Add --combine CLI mode to concatenate Markdown files with

  include expansion and GitBook-style SUMMARY.md index
  support.

- Add --mmd-merge CLI mode to merge MultiMarkdown index

  files into a single Markdown stream

- Support indentation-based header level shifting when

  merging mmd_merge index entries

- Support inline CSV/TSV tables using ```table fenced blocks

  with automatic CSV/TSV delimiter detection

- Support <!--TABLE--> markers that convert following

  CSV/TSV lines into Markdown tables until a blank line

- Add --aria command-line option to enable ARIA labels and

  accessibility attributes in HTML output

- Add aria-label="Table of contents" to TOC navigation

  elements when --aria is enabled

Add role="figure" to figure elements when --aria is enabled

- Add role="table" to table elements when --aria is enabled
- Generate id attributes for figcaption elements in table

  figures when --aria is enabled

- Add aria-describedby attributes linking tables to their

  captions when --aria is enabled

### Improved

- Removed unused variables to eliminate compiler warnings
- Empty thead sections from headerless tables are now

  removed from HTML output instead of rendering empty header
  cells.

- Table row mapping now better handles the relationship

  between HTML row indices and AST row indices, accounting
  for separator rows that are removed from HTML output.

- Added safeguards to prevent rows that should be in tbody

  from being skipped, including protection for the first few
  rows (header and first two data rows) when a === separator
  is present.

- Make table captions positionable above or below tables
- Center and style figcaptions in standalone HTML output
- Support optional alignment keyword rows (left, right,

  center, auto) and headless tables for both included CSV
  files and inline CSV/TSV data

- Preserve ```table fences without commas or tabs by leaving

  them as code blocks so users can show literal CSV/TSV
  without conversion

### Fixed

- Removed unused variable 'row_idx' from advanced_tables.c

  to eliminate compiler warnings.

- Rows before the === separator are now correctly placed in

  tbody instead of being incorrectly placed in tfoot or
  skipped entirely. The fix includes HTML position
  verification to ensure rows that appear before the === row
  in the rendered HTML are always in tbody, regardless of
  AST marking.

- Prevent === separator rows from appearing as table content

Ensure footer rows render in tfoot without losing body rows

- Preserve legitimate empty cells such as missing Q4 values
- Apply ^^ rowspans correctly for all table sections
- Apply ^^ rowspans correctly across table sections without

  leaking into unrelated rows

- Support footer colspans so footer cells can span multiple

  columns like headers and body rows

- Preserve legitimate empty table cells that are not part of

  colspans or rowspans

- KaTeX auto-render now properly configures delimiters and

  manually renders math spans to prevent plain text from
  appearing after rendered equations

- Relaxed table header conversion now only runs when

  relaxed_tables option is enabled

- HTML document wrapping now strips existing </body></html>

  tags to prevent duplicates when content already contains
  them

- KaTeX auto-render now properly configures delimiters and

  manually renders math spans to prevent plain text from
  appearing after rendered equations

- Relaxed table header conversion now only runs when

  relaxed_tables option is enabled

- HTML document wrapping now strips existing </body></html>

  tags to prevent duplicates when content already contains
  them

- Table captions appearing after their tables now correctly

  link via aria-describedby attributes

## [0.1.39] - 2025-12-19

### Changed

- Table post-processing now tracks all cells (including

  removed ones) to correctly map HTML column positions to
  original AST column indices, ensuring attributes are
  applied to the correct cells.

- Added content-based detection for ^^ marker cells during

  HTML post-processing to ensure they are properly removed
  even when attribute matching fails.

### New

- Added content verification to prevent false matches when

  cells are covered by rowspans

- Added tracking of previous cell's colspan to detect and

  remove empty cells after colspan

- Added detection and removal of === marker rows in tfoot

  sections

### Improved

- Rowspan cell tracking now uses a per-column active cell

  approach (inspired by Jekyll Spaceship) for more reliable
  rowspan calculation across complex table structures.

- Cell matching now uses position-based fallback to previous

  row when current row match fails

- Row mapping accounts for rowspan coverage to correctly

  identify visible columns

- Tfoot row detection now uses AST row indices instead of

  HTML row indices for accuracy

### Fixed

- Table rowspan rendering now correctly handles rows where

  most cells use ^^ markers. Previously, cells like "Beta"
  and "Gamma" in rows with multiple ^^ cells would be
  missing or appear in the wrong position. The fix includes
  proper mapping between HTML row indices and AST row
  indices, accounting for separator rows that are removed
  from HTML output.

- Missing cells after colspan (e.g., "92.00" cell was

  missing when "Absent" had colspan="2")

- Rowspan not applying correctly when HTML row mapping was

  off by one

- Footer alignment rows (=== markers) appearing in output

  instead of being removed

- Empty cells after colspan not being removed from rendered

  HTML

## [0.1.38] - 2025-12-18

### Changed

- In standalone mode, insert script tags just before </body>
- In snippet mode, append script tags at the end of the HTML

  fragment

- When --embed-css is used with --css, replace the

  stylesheet <link> tag with an inline <style> block
  containing the CSS file contents

### New

- Support Pandoc-style "Table: Caption" syntax and
- Add --script CLI flag to inject scripts into HTML output
- Support shorthands for common JS libraries (mermaid,

  mathjax, katex, highlightjs, prism, htmx, alpine)

- Add --embed-css option to inline CSS files into the

  standalone document head

### Improved

- Compress extraneous newlines between HTML elements
- Remove unused apex_remote_trim helper to eliminate

  compiler warnings

### Fixed

- Prevent caption paragraphs from being reused across
- Skip URL encoding for footnote definitions ([^id]: ...) so

  footnote

## [0.1.37] - 2025-12-17

### Changed

- Image attributes are mode-dependent: work in Unified and

  MultiMarkdown modes only

- URL encoding is mode-dependent: works in Unified,

  MultiMarkdown, and Kramdown modes

- Improved caption detection to check all table rows for

  caption markers, not just the last row, to handle cases
  where captions come after tfoot rows.

### New

- Support for MultiMarkdown-style image attributes in

  unified and MultiMarkdown modes

Inline image attributes: ![alt](url width=300 style="float:left" "title")

- Reference-style image attributes: ![][ref] with [ref]: url

  width=300

- Automatic URL encoding for links with spaces in unified,

  MultiMarkdown, and Kramdown modes

- URLs with spaces are automatically percent-encoded (e.g.,

  "path with spaces.png" becomes "path%20with%20spaces.png")

- Added support for MultiMarkdown-style image attributes in

  reference-style images. Reference definitions can now
  include attributes: [img1]: image.png width=300
  style="float:left"

Added support for inline image attributes: ![alt](url width=300 style="...")

- Added automatic URL encoding for all link URLs (images and

  regular links). URLs with spaces are automatically
  percent-encoded (e.g., "path to/image.png" becomes
  "path%20to/image.png")

- Added detection and removal of table alignment separator

  rows that were incorrectly being rendered as table rows.

- Added test cases for table captions appearing before and

  after tables.

Added support for tfoot sections in tables using `===` row markers. Rows containing `===` markers are now placed in `<tfoot>` sections, and all subsequent rows after the first `===` row are also placed in tfoot.

- Added comprehensive table feature tests that validate

  rowspan,

### Improved

- Improved attribute injection in HTML renderer to correctly

  place attributes before closing > or /> in img and link
  tags

- Enhanced URL parsing to distinguish between spaces within

  URLs vs spaces before attributes using forward-scanning
  pattern detection

- Self-closing img tags now consistently use " />" (space

  before slash) when attributes are injected, matching the
  format used by cmark-gfm for img tags without injected
  attributes

- Rowspan and colspan attribute handling now properly

  appends to existing attributes instead of replacing them,
  allowing multiple attributes to coexist on table cells.

- Alignment rows (rows containing only '' characters) are

  now detected and marked for removal, preventing them from
  appearing in HTML output.

### Fixed

- Fixed bug where image prefix "![" was incorrectly removed

  during preprocessing of expanded reference-style images

- URL encoding now only encodes unsafe characters (space,

  control chars, non-ASCII). Valid URL characters like /, :,
  ?, #, ~, etc. are preserved and no longer incorrectly
  encoded.

- Titles in links and images are now correctly detected and

  excluded from URL encoding. Supports quoted titles
  ("title", 'title') and parentheses titles ((title)). URLs
  with parentheses (like Wikipedia links) are correctly
  distinguished from titles based on whether a space
  precedes the opening parenthesis.

- Reference-style images with attributes now render

  correctly. Reference definitions with image attributes are
  removed from output, while those without attributes are
  preserved (with URL encoding) so cmark can resolve the
  references.

- Spacing between attributes in HTML output. Attributes

  injected into img and link tags now have proper spacing,
  preventing malformed HTML like alt="text"width="100".

- Table attributes now render correctly with proper spacing.

  Fixed missing space in table tag when id attribute
  immediately follows (e.g., <tableid="..." now renders as
  <table id="...").

- Rowspan and colspan injection now works correctly in all

  cases. Fixed bug where table tracking variables weren't
  set when fixing missing space in table tag (e.g.,
  <tableid="..."), causing row and cell processing to be
  skipped. Table tracking is now properly initialized even
  when correcting tag spacing.

- Captions after tables were not being detected when tables

  had IAL attributes, as IAL processing replaced the caption
  data stored in user_data. Added fallback logic to check
  for caption paragraphs directly in the AST when user_data
  lookup fails.

- Rows containing only `===` markers are now properly

  skipped entirely rather than rendering as empty cells in
  tfoot sections.

- Caption paragraphs before tables are now properly removed,

## [0.1.36] - 2025-12-16

### Fixed

- Resolve CMake error when building framework where

  file(GLOB) returns multiple dylib files, causing
  semicolon-concatenated paths in file(COPY) command. Now
  extracts first file from glob result before copying.

- Homebrew installation now correctly links to system

  libyaml instead of hardcoded CI path

## [0.1.35] - 2025-12-16

### Changed

- Update Homebrew formula to install from a precompiled

  macOS universal binary instead of building from source
  with cmake.

- Allow --install-plugin to accept a Git URL or GitHub

  shorthand (user/repo) in addition to directory IDs when
  installing plugins.

### Improved

- Simplify Homebrew installation so users no longer need

  cmake or Xcode build tools to install apex.

- Add an interactive security confirmation when installing

  plugins from a direct Git URL or GitHub repo name,
  reminding users that plugins execute unverified code.

## [0.1.34] - 2025-12-16

## [0.1.33] - 2025-12-16

## [0.1.32] - 2025-12-16

## [0.1.31] - 2025-12-16

## [0.1.30] - 2025-12-16

### Changed

- Make --list-plugins show installed plugins before remote

  ones.

- Prevent remote plugins that are already installed from

  being listed under Available Plugins.

- Build system now detects libyaml via multiple methods

  (yaml-0.1, yaml, libyaml) for better cross-platform
  support.

- Homebrew formula now includes libyaml as a dependency to

  ensure full YAML support.

- Suppressed unused-parameter warnings from vendored

  cmark-gfm extensions to reduce build noise.

### New

Add --uninstall-plugin CLI flag to remove installed plugins.

- Run optional post_install command from plugin.yml after

  cloning a plugin.

- Full YAML parsing support using libyaml for arrays and

  nested structures in metadata and plugin manifests.

- Plugin bundle support allowing multiple plugins to be

  defined in a single plugin.yml manifest.

- Expose APEX_FILE_PATH to external plugins so scripts can

  see the original input path or base directory when
  processing.

### Improved

- Split() metadata transform now accepts regular expressions

  as delimiters (for example split(,\s*)).

- YAML arrays are automatically normalized to

  comma-separated strings for backward compatibility with
  existing metadata transforms.

- External plugin environment now includes the source file

  path (when available) alongside APEX_PLUGIN_DIR and
  APEX_SUPPORT_DIR.

### Fixed

- Tighten mutual-exclusion checks between install and

  uninstall plugin flags.

- Ensure CMake policy version is compatible with vendored

  cmark-gfm on newer CMake releases.

- Install the Apex framework with its public apex.h header

  correctly embedded in Apex.framework/Headers for Xcode
  use.

- Bundle libcmark-gfm and libcmark-gfm-extensions dylibs

  into Apex.framework so dependent apps no longer hit
  missing library errors at runtime.

## [0.1.29] - 2025-12-15

### Changed

- Make --list-plugins show installed plugins before remote

  ones.

- Prevent remote plugins that are already installed from

  being listed under Available Plugins.

### New

- Initial planning for a remote plugin directory and install

  features

Add --uninstall-plugin CLI flag to remove installed plugins.

### Fixed

- Superscript/subscript no longer process content inside

  Liquid {% %} tags.

- Autolink detection skips Liquid {% %} tags so emails and

  URLs are not rewritten there.

- Fix directory url for `--list-plugins`

## [0.1.28] - 2025-12-15

### Changed

- Default wikilink URLs now replace spaces with dashes (e.g.

  [[Home Page]] -> href="Home-Page").

### New

- Add --wikilink-space and --wikilink-extension flags to

  control how [[WikiLink]] hrefs are generated.

- Allow wikilink space and extension configuration via

  metadata keys wikilink-space and wikilink-extension.

- Support Kramdown-style {:toc ...} markers mapped to Apex

  TOC generation.

- Add tests for `{:toc}` syntaxes
- MMD includes support full glob patterns like {{*.md}} and

  {{c?de.py}}.

- Add plugin discovery from .apex/plugins and

  ~/.config/apex/plugins.

- Allow external handler plugins in any language via JSON

  stdin/stdout.

- Support declarative regex plugins for pre_parse and

  post_render phases.

- Add `--no-plugins` CLI flag to disable all plugins for a

  run.

- Support `plugins: true/false` metadata to enable or

  disable plugins.

- Initial planning for a remote plugin directory and install

  features

### Improved

- Exclude headings with .no_toc class from generated tables

  of contents for finer-grained TOC control.

- MMD-style {{file.*}} now resolves preferred extensions

  before globbing.

- Transclusion respects brace-style patterns such as

  {{{intro,part1}.md}} where supported.

- Provide `APEX_PLUGIN_DIR` and `APEX_SUPPORT_DIR` for

  plugin code and data.

- Add profiling (APEX_PROFILE_PLUGINS=1) for plugins

## [0.1.27] - 2025-12-15

### Changed

- Default wikilink URLs now replace spaces with dashes (e.g.

  [[Home Page]] -> href="Home-Page").

### New

- Add --wikilink-space and --wikilink-extension flags to

  control how [[WikiLink]] hrefs are generated.

- Allow wikilink space and extension configuration via

  metadata keys wikilink-space and wikilink-extension.

- Support Kramdown-style {:toc ...} markers mapped to Apex

  TOC generation.

- Add tests for `{:toc}` syntaxes
- MMD includes support full glob patterns like {{*.md}} and

  {{c?de.py}}.

- Add plugin discovery from .apex/plugins and

  ~/.config/apex/plugins.

- Allow external handler plugins in any language via JSON

  stdin/stdout.

- Support declarative regex plugins for pre_parse and

  post_render phases.

- Add `--no-plugins` CLI flag to disable all plugins for a

  run.

- Support `plugins: true/false` metadata to enable or

  disable plugins.

- Initial planning for a remote plugin directory and install

  features

### Improved

- Exclude headings with .no_toc class from generated tables

  of contents for finer-grained TOC control.

- MMD-style {{file.*}} now resolves preferred extensions

  before globbing.

- Transclusion respects brace-style patterns such as

  {{{intro,part1}.md}} where supported.

- Provide `APEX_PLUGIN_DIR` and `APEX_SUPPORT_DIR` for

  plugin code and data.

- Add profiling (APEX_PROFILE_PLUGINS=1) for plugins

## [0.1.26] - 2025-12-14

### Changed

- Change `--enable-includes` to `--[no-]includes`, allowing

  `--no-includes` to disable includes in unified mode and
  shortening the flag

- Integrate metadata-to-options application into CLI after

  metadata merging

- Preserve bibliography files array when metadata mode

  resets options structure

### New

- Add apex_apply_metadata_to_options() function to apply

  metadata values to apex_options structure

- Support controlling boolean flags via metadata (indices,

  wikilinks, includes, relaxed-tables, alpha-lists,
  mixed-lists, sup-sub, autolink, transforms, unsafe,
  tables, footnotes, smart, math, ids, header-anchors,
  embed-images, link-citations, show-tooltips,
  suppress-bibliography, suppress-index,
  group-index-by-letter, obfuscate-emails, pretty,
  standalone, hardbreaks)

- Support controlling string options via metadata

  (bibliography, csl, title, style/css, id-format, base-dir,
  mode)

- Boolean metadata values accept true/false, yes/no, or 1/0

  (case-insensitive, downcased)

- String metadata values used directly for options that take

  arguments

- Metadata mode option resets options to mode defaults

  before applying other metadata

- Comprehensive tests for metadata control of command line

  options

## [0.1.25] - 2025-12-13

### New

- Add citation processing with support for Pandoc,

  MultiMarkdown, and mmark syntaxes

- Add bibliography loading from BibTeX, CSL JSON, and CSL

  YAML formats

- Add --bibliography CLI option to specify bibliography

  files (can be used multiple times)

- Add --csl CLI option to specify citation style file
- Add --no-bibliography CLI option to suppress bibliography

  output

- Add --link-citations CLI option to link citations to

  bibliography entries

- Add --show-tooltips CLI option for citation tooltips
- Add bibliography generation and insertion at <!--

  REFERENCES --> marker

Add support for bibliography specified in document metadata

- Added missing docs and man page for citation support
- Add support for transclude base metadata to control file

  transclusion paths

- Add Base Header Level and HTML Header Level metadata to

  adjust heading levels

- Add CSS metadata to link external stylesheets in

  standalone HTML documents

- Add HTML Header and HTML Footer metadata to inject custom

  HTML

- Add Language metadata to set HTML lang attribute in

  standalone documents

- Add Quotes Language metadata to control smart quote styles

  (French, German, Spanish, etc.)

- Add --css CLI flag as alias for --style with metadata

  override precedence

- Add metadata key normalization: case-insensitive matching

  with spaces removed (e.g., "HTML Header Level" matches
  "htmlheaderlevel")

- Add index extension supporting mmark syntax (!item),

  (!item, subitem), and (!!item, subitem) for primary
  entries

- Add TextIndex syntax support with {^}, [term]{^}, and

  {^params} patterns

- Add automatic index generation at end of document or at

  <!--INDEX--> marker

- Add alphabetical sorting and optional grouping by first

  letter for index entries

- Add hierarchical sub-item support in generated index
- Add --indices CLI flag to enable index processing
- Add --no-indices CLI flag to disable index processing
- Add --no-index CLI flag to suppress index generation while

  keeping markers

- Add comprehensive test suite with 40 index tests covering

  both syntaxes

### Improved

- Only process citations when bibliography is actually

  provided for better performance

- Add comprehensive tests for MultiMarkdown metadata keys
- Add comprehensive performance profiling system

  (APEX_PROFILE=1) to measure processing time for all
  extensions and CLI operations

- Add early exit checks for IAL processing when no {:

  markers are present

- Add early exit checks for index processing when no index

  patterns are found

- Add early exit checks for citation processing when no

  citation patterns are found

- Add early exit checks for definition list processing when

  no : patterns are found

- Optimize alpha lists postprocessing with single-pass

  algorithm replacing O(n*m) strstr() loops

- Add early exit check for alpha lists postprocessing when

  no markers are present

- Optimize file I/O by using fwrite() with known length

  instead of fputs()

- Add markdown syntax detection in definition lists to skip

  parser creation for plain text

- Optimize definition lists by selectively extracting only

  needed reference definitions instead of prepending all

- Add profiling instrumentation for all preprocessing,

  parsing, rendering, and post-processing steps

- Add profiling instrumentation for CLI operations (file

  I/O, metadata processing)

### Fixed

- Prevent autolinking of @ symbols in citation syntax (e.g.,

  [@key])

- Handle HTML comments in autolinker to preserve citation

  placeholders

- Fix quote language adjustment to handle Unicode curly

  quotes in addition to HTML entities

Fix bibliography_files assignment to remove unnecessary cast

- Fix heap-buffer-overflow in html_renderer.c when writing

  null terminator (allocate capacity+1)

- Fix use-after-free in ial.c by deferring node unlinking

  until after iteration completes

- Fix buffer overflow in definition_list.c HTML entity

  escaping (correct length calculation for &amp; and &quot;)

## [0.1.24] - 2025-12-13

### New

- Add citation processing with support for Pandoc,

  MultiMarkdown, and mmark syntaxes

- Add bibliography loading from BibTeX, CSL JSON, and CSL

  YAML formats

- Add --bibliography CLI option to specify bibliography

  files (can be used multiple times)

- Add --csl CLI option to specify citation style file
- Add --no-bibliography CLI option to suppress bibliography

  output

- Add --link-citations CLI option to link citations to

  bibliography entries

- Add --show-tooltips CLI option for citation tooltips
- Add bibliography generation and insertion at <!--

  REFERENCES --> marker

Add support for bibliography specified in document metadata

### Improved

- Only process citations when bibliography is actually

  provided for better performance

### Fixed

- Raw HTML tags and comments are now preserved in definition

  lists by default in unified mode. Previously, HTML content
  in definition list definitions was being replaced with
  "raw HTML omitted" even when using --unsafe or in unified
  mode.

- Unified mode now explicitly sets unsafe=true by default to

  ensure raw HTML is allowed.

- Prevent autolinking of @ symbols in citation syntax (e.g.,

  [@key])

- Handle HTML comments in autolinker to preserve citation

  placeholders

## [0.1.23] - 2025-12-12

### Changed

- Remove remote image embedding support (curl dependency

  removed)

### New

- Add metadata variable transforms with [%key:transform]

  syntax

- Add --transforms and --no-transforms flags to

  enable/disable transforms

- Add 19 text transforms: upper, lower, title, capitalize,

  trim, slug, replace (with regex support), substring,
  truncate, default, html_escape, basename, urlencode,
  urldecode, prefix, suffix, remove, repeat, reverse,
  format, length, pad, contains

- Add array transforms: split, join, first, last, slice
- Add date/time transform: strftime with date parsing
- Add transform chaining support (multiple transforms

  separated by colons)

- Add --meta-file flag to load metadata from external files

  (YAML, MMD, or Pandoc format, auto-detected)

- Add --meta KEY=VALUE flag to set metadata from command

  line (supports multiple flags and comma-separated pairs)

- Add metadata merging with proper precedence: command-line
  > document > file
- Add --embed-images flag to embed local images as base64

  data URLs in HTML output

- Add --base-dir flag to set base directory for resolving

  relative paths (images, includes, wiki links)

- Add automatic base directory detection from input file

  directory when reading from file

- Add base64 encoding utility for image data
- Add MIME type detection from file extensions (supports

  jpg, png, gif, webp, svg, bmp, ico)

- Add image embedding function that processes HTML and

  replaces local image src attributes with data URLs

- Add test suite for image embedding functionality

### Improved

- Wiki link scanner now processes all links in a text node

  in a single pass instead of recursively processing one at
  a time, significantly improving performance for documents
  with multiple wiki links per text node.

- Added early-exit optimization to skip wiki link AST

  traversal entirely when no wiki link markers are present
  in the document.

- Improve error handling in transform execution to return

  original value instead of NULL on failure

- Add comprehensive test coverage for all transforms

  including edge cases

- Relative path resolution for images now uses

  base_directory option

- Base directory is automatically set from input file

  location when not specified

### Fixed

- Fix bracket handling in regex patterns - properly match

  closing brackets in [%...] syntax when patterns contain
  brackets

- Fix YAML metadata parsing to strip quotes from quoted

  string values

- Raw HTML tags and comments are now preserved in definition

  lists by default in unified mode. Previously, HTML content
  in definition list definitions was being replaced with
  "raw HTML omitted" even when using --unsafe or in unified
  mode.

- Unified mode now explicitly sets unsafe=true by default to

  ensure raw HTML is allowed.

## [0.1.20] - 2025-12-11

#### NEW

- Added man page generation and installation support. Man

  pages can be generated from Markdown source using pandoc
  or go-md2man, with pre-generated man pages included in the
  repository as fallback. CMake build system now handles man
  page installation, and Homebrew formula installs the man
  page.

- Added comprehensive test suite for MMD 6 features

  including multi-line setext headers and link/image titles
  with different quote styles (single quotes, double quotes,
  parentheses). Tests verify these features work in both
  MultiMarkdown and unified modes.

- Added build-test man_page_copy target for man page

  installation.

- Added --obfuscate-emails flag to hex-encode mailto links.

#### IMPROVED

- Superscript processing now stops at sentence terminators

  (. , ; : ! ?) instead of including them in the superscript
  content. This prevents punctuation from being incorrectly
  included in superscripts.

- Enhanced subscript and underline detection logic. The

  processor now correctly differentiates between subscript
  (tildes within a word, e.g., H~2~O) and underline (tildes
  at word boundaries, e.g., ~text~) by checking if tildes
  are within alphanumeric words or at word boundaries.

- Expanded test coverage for superscript, subscript,

  underline, strikethrough, and highlight features with
  additional edge case tests.

- Email autolink detection trims trailing punctuation.

#### FIXED

- Autolink now only wraps real URLs/emails instead of every

  word.

Email autolinks now use mailto: hrefs instead of bare text.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.1.19] - 2025-12-09

#### CHANGED

- HTML comments now replaced with "raw HTML omitted" in

  CommonMark and GFM modes by default

- Added enable_sup_sub flag to apex_options struct
- Updated mode configurations to enable sup/sub in

  appropriate modes

- Added sup_sub.c to CMakeLists.txt build configuration
- Removed unused variables to resolve compiler warnings
- Tag filter (GFM security feature) now only applies in GFM

  mode, not Unified mode, allowing raw HTML and autolinks in
  Unified mode as intended.

- Autolink extension registration now respects the

  enable_autolink option flag.

#### NEW

- Added MultiMarkdown-style superscript (^text^) and

  subscript (~text~) syntax support

- Added --[no-]sup-sub command-line option to enable/disable

  superscript/subscript

- Superscript/subscript enabled by default in unified and

  MultiMarkdown modes

- Created sup_sub extension (sup_sub.c and sup_sub.h) for

  processing ^ and ~ syntax

- Added --[no-]unsafe command-line option to control raw

  HTML handling

- Added test_sup_sub() function with 13 tests covering

  superscript and

- Added test_mixed_lists() function with 10 tests covering

  mixed list

- Added test_unsafe_mode() function with 8 tests covering

  raw HTML

- Added preprocessing for angle-bracket autolinks

  (<http://...>) to convert them to explicit markdown links,
  ensuring they work correctly with custom rendering paths.

- Added --[no-]autolink CLI option to control automatic

  linking of URLs and email addresses. Autolinking is
  enabled by default in GFM, MultiMarkdown, Kramdown, and
  unified modes, and disabled in CommonMark mode.

- Added enable_autolink field to apex_options structure to

  control autolink behavior programmatically.

- Added underline syntax support: ~text~ now renders as

  <u>text</u> when there's a closing ~ with no space before
  it.

#### IMPROVED

- Test suite now includes 36 additional tests, increasing

  total test

- Autolink preprocessing now skips processing inside code

  spans (`...`) and code blocks (```...```), preventing URLs
  from being converted to links when they appear in code
  examples.

- Metadata replacement retains HTML edge-case handling and

  properly cleans up intermediate buffers.

#### FIXED

- Unified mode now correctly enables mixed list markers and

  alpha lists by default when no --mode is specified

- ^ marker now properly separates lists by creating a

  paragraph break instead of just blank lines

- Empty paragraphs created by ^ marker are now removed from

  final HTML output

- Superscript and subscript processing now skips ^ and ~

  characters

- Superscript processing now skips ^ when part of footnote

  reference

- Subscript processing now skips ~ when part of critic

  markup patterns

- Setext headers are no longer broken when followed by

  highlight syntax (==text==). Highlight processing now
  stops at line breaks to prevent interference with header
  parsing.

- Metadata parser no longer incorrectly treats URLs and

  angle-bracket autolinks as metadata. Lines containing < or
  URLs (http://, https://, mailto:) are now skipped during
  metadata extraction.

- Superscript/subscript processor now correctly

  differentiates between ~text~ (underline), ~word
  (subscript), and ~~text~~ (strikethrough). Double-tilde
  sequences are skipped so strikethrough extension can
  handle them.

- Subscript processing now stops at sentence terminators (.

  , ; : ! ?) instead of including them in the subscript
  content.

- Metadata variable replacement now runs before autolinking

  so [%key] values containing URLs are turned into links
  when autolinking is enabled.

- MMD metadata parsing no longer incorrectly rejects entries

  with URL values; only URL-like keys or '<' characters in
  keys are rejected, allowing "URL: https://example.com" as
  valid metadata.

Headers starting with `#` are now correctly recognized instead of being treated as autolinks. The autolink preprocessor now skips `#` at the start of a line when followed by whitespace.

Math processor now validates that `\(...\)` sequences contain actual math content (letters, numbers, or operators) before processing them. This prevents false positives like `\(%\)` from being treated as math when they only contain special characters.

## [0.1.18] - 2025-12-06

### Fixed
- GitHub Actions workflow now properly builds separate Linux

  x86_64 and ARM64 binaries

## [0.1.17] - 2025-12-06

### Fixed
- Relaxed tables now disabled by default for CommonMark,

  GFM, and MultiMarkdown modes (only enabled for Kramdown
  and Unified modes)

- Header ID extraction no longer incorrectly parses metadata

  variables like `[%title]` as MMD-style header IDs

- Tables with alignment/separator rows now correctly

  generate `<thead>` even when relaxed table mode is enabled

- Relaxed tables preprocessor preserves input newline

  behavior in output

- Memory management bug in IAL preprocessing removed

  unnecessary free call

## [0.1.16] - 2025-12-06

### Fixed
- IAL (Inline Attribute List) markers appearing immediately

  after content without a blank line are now correctly
  parsed

Added `apex_preprocess_ial()` function to ensure Kramdown-style IAL syntax works correctly with cmark-gfm parser

## [0.1.15] - 2025-12-06

### Fixed
- Homebrew formula updated with correct version and commit

  hash

## [0.1.10] - 2025-12-06

### Changed
- License changed to MIT

### Added
- Homebrew formula update scripts

## [0.1.9] - 2025-12-06

### Fixed
- Shell syntax in Linux checksum step for GitHub Actions

## [0.1.8] - 2025-12-06

### Fixed
- Link order for Linux static builds

## [0.1.7] - 2025-12-06

### Fixed
- Added write permissions for GitHub releases

## [0.1.6] - 2025-12-06

### Fixed
`.gitignore` pattern fixed to properly include apex headers (was incorrectly matching `include/apex/`)

## [0.1.5] - 2025-12-06

### Changed
- Added verbose build output for CI debugging

## [0.1.4] - 2025-12-06

### Fixed
- CMake build rules updated

## [0.1.3] - 2025-12-06

### Fixed
- CMake policy version for cmark-gfm compatibility

## [0.1.2] - 2025-12-06

### Fixed
- GitHub Actions workflow fixes

## [0.1.1] - 2025-12-04

### Added
- CMake setup documentation

## [0.1.0] - 2025-12-04

### Added

**Core Features:**

- Initial release of Apex unified Markdown processor
- Based on cmark-gfm for CommonMark + GFM support
- Support for 5 processor modes: CommonMark, GFM,

  MultiMarkdown, Kramdown, Unified

**Metadata:**

- YAML front matter parsing
- MultiMarkdown metadata format
- Pandoc title block format
- Metadata variable replacement with `[%key]` syntax

**Extended Syntax:**

- Wiki-style links: `[[Page]]`, `[[Page|Display]]`,

  `[[Page#Section]]`

- Math support: `$inline$` and `$$display$$` with LaTeX
- Critic Markup: All 5 types ({++add++}, {--del--},

  {~~sub~~}, {==mark==}, {>>comment<<})

- GFM tables, strikethrough, task lists, autolinks
- Reference-style footnotes
- Smart typography (smart quotes, dashes, ellipsis)

**Build System:**

- CMake build system for cross-platform support
- Builds shared library, static library, CLI binary, and

  macOS framework

- Clean compilation on macOS with Apple Clang

**CLI Tool:**

- `apex` command-line binary
- Support for all processor modes via `--mode` flag
- Stdin/stdout support for Unix pipes
- Comprehensive help and version information

**Integration:**

Objective-C wrapper (`NSString+Apex`) for Marked integration

- macOS framework with proper exports
- Detailed integration documentation and examples

**Testing:**

- Automated test suite with 31 tests
- 90% pass rate across all feature areas
- Manual testing validated

**Documentation:**

- Comprehensive user guide
- Complete API reference
- Architecture documentation
- Integration guides
- Code examples

### Known Issues

- Critic Markup substitutions have edge cases with certain

  inputs

- Definition lists not yet implemented
- Kramdown attributes not yet implemented
- Inline footnotes not yet implemented

### Performance

- Small documents (< 10KB): < 10ms
- Medium documents (< 100KB): < 100ms
- Large documents (< 1MB): < 1s

### Credits

Based on [cmark-gfm](https://github.com/github/cmark-gfm) by GitHub

Developed for [Marked](https://marked2app.com) by Brett Terpstra

[1.1.47]: https://github.com/ApexMarkdown/apex/releases/tag/v1.1.47
[1.1.46]: https://github.com/ApexMarkdown/apex/releases/tag/v1.1.46
[1.1.45]: https://github.com/ApexMarkdown/apex/releases/tag/v1.1.45
[1.1.44]: https://github.com/ApexMarkdown/apex/releases/tag/v1.1.44
[1.1.43]: https://github.com/ApexMarkdown/apex/releases/tag/v1.1.43
[1.1.42]: https://github.com/ApexMarkdown/apex/releases/tag/v1.1.42
[1.1.41]: https://github.com/ApexMarkdown/apex/releases/tag/v1.1.41
[1.1.40]: https://github.com/ApexMarkdown/apex/releases/tag/v1.1.40
[1.1.39]: https://github.com/ApexMarkdown/apex/releases/tag/v1.1.39
[1.1.38]: https://github.com/ApexMarkdown/apex/releases/tag/v1.1.38
[1.1.37]: https://github.com/ApexMarkdown/apex/releases/tag/v1.1.37
[1.1.36]: https://github.com/ApexMarkdown/apex/releases/tag/v1.1.36
[1.1.35]: https://github.com/ApexMarkdown/apex/releases/tag/v1.1.35
[1.1.34]: https://github.com/ApexMarkdown/apex/releases/tag/v1.1.34
[1.1.33]: https://github.com/ApexMarkdown/apex/releases/tag/v1.1.33
[1.1.32]: https://github.com/ApexMarkdown/apex/releases/tag/v1.1.32
[1.1.31]: https://github.com/ApexMarkdown/apex/releases/tag/v1.1.31
[1.1.30]: https://github.com/ApexMarkdown/apex/releases/tag/v1.1.30
[1.1.29]: https://github.com/ApexMarkdown/apex/releases/tag/v1.1.29
[1.1.28]: https://github.com/ApexMarkdown/apex/releases/tag/v1.1.28
[1.1.27]: https://github.com/ApexMarkdown/apex/releases/tag/v1.1.27
[1.1.26]: https://github.com/ApexMarkdown/apex/releases/tag/v1.1.26
[1.1.25]: https://github.com/ApexMarkdown/apex/releases/tag/v1.1.25
[1.1.24]: https://github.com/ApexMarkdown/apex/releases/tag/v1.1.24
[1.1.23]: https://github.com/ApexMarkdown/apex/releases/tag/v1.1.23
[1.1.22]: https://github.com/ApexMarkdown/apex/releases/tag/v1.1.22
[1.1.21]: https://github.com/ApexMarkdown/apex/releases/tag/v1.1.21
[1.1.20]: https://github.com/ApexMarkdown/apex/releases/tag/v1.1.20
[1.1.19]: https://github.com/ApexMarkdown/apex/releases/tag/v1.1.19
[1.1.18]: https://github.com/ApexMarkdown/apex/releases/tag/v1.1.18
[1.1.17]: https://github.com/ApexMarkdown/apex/releases/tag/v1.1.17
[1.1.16]: https://github.com/ApexMarkdown/apex/releases/tag/v1.1.16
[1.1.15]: https://github.com/ApexMarkdown/apex/releases/tag/v1.1.15
[1.1.14]: https://github.com/ApexMarkdown/apex/releases/tag/v1.1.14
[1.1.13]: https://github.com/ApexMarkdown/apex/releases/tag/v1.1.13
[1.1.12]: https://github.com/ApexMarkdown/apex/releases/tag/v1.1.12
[1.1.11]: https://github.com/ApexMarkdown/apex/releases/tag/v1.1.11
[1.1.10]: https://github.com/ApexMarkdown/apex/releases/tag/v1.1.10
[1.1.9]: https://github.com/ApexMarkdown/apex/releases/tag/v1.1.9
[1.1.8]: https://github.com/ApexMarkdown/apex/releases/tag/v1.1.8
[1.1.7]: https://github.com/ApexMarkdown/apex/releases/tag/v1.1.7
[1.1.6]: https://github.com/ApexMarkdown/apex/releases/tag/v1.1.6
[1.1.5]: https://github.com/ApexMarkdown/apex/releases/tag/v1.1.5
[1.1.4]: https://github.com/ApexMarkdown/apex/releases/tag/v1.1.4
[1.1.3]: https://github.com/ApexMarkdown/apex/releases/tag/v1.1.3
[1.1.2]: https://github.com/ApexMarkdown/apex/releases/tag/v1.1.2
[1.1.1]: https://github.com/ApexMarkdown/apex/releases/tag/v1.1.1
[1.0.15]: https://github.com/ApexMarkdown/apex/releases/tag/v1.0.15
[1.0.14]: https://github.com/ApexMarkdown/apex/releases/tag/v1.0.14
[1.0.13]: https://github.com/ApexMarkdown/apex/releases/tag/v1.0.13
[1.0.12]: https://github.com/ApexMarkdown/apex/releases/tag/v1.0.12
[1.0.11]: https://github.com/ApexMarkdown/apex/releases/tag/v1.0.11
[1.0.10]: https://github.com/ApexMarkdown/apex/releases/tag/v1.0.10
[1.0.9]: https://github.com/ApexMarkdown/apex/releases/tag/v1.0.9
[1.0.8]: https://github.com/ApexMarkdown/apex/releases/tag/v1.0.8
[1.0.7]: https://github.com/ApexMarkdown/apex/releases/tag/v1.0.7
[1.0.6]: https://github.com/ApexMarkdown/apex/releases/tag/v1.0.6
[1.0.5]: https://github.com/ApexMarkdown/apex/releases/tag/v1.0.5
[1.0.4]: https://github.com/ApexMarkdown/apex/releases/tag/v1.0.4
[1.0.3]: https://github.com/ApexMarkdown/apex/releases/tag/v1.0.3
[1.0.2]: https://github.com/ApexMarkdown/apex/releases/tag/v1.0.2
[1.0.1]: https://github.com/ApexMarkdown/apex/releases/tag/v1.0.1
[0.1.104]: https://github.com/ApexMarkdown/apex/releases/tag/v0.1.104
[0.1.103]: https://github.com/ApexMarkdown/apex/releases/tag/v0.1.103
[0.1.102]: https://github.com/ApexMarkdown/apex/releases/tag/v0.1.102
[0.1.100]: https://github.com/ApexMarkdown/apex/releases/tag/v0.1.100
[0.1.98]: https://github.com/ApexMarkdown/apex/releases/tag/v0.1.98
[0.1.97]: https://github.com/ApexMarkdown/apex/releases/tag/v0.1.97
[0.1.96]: https://github.com/ApexMarkdown/apex/releases/tag/v0.1.96
[0.1.95]: https://github.com/ApexMarkdown/apex/releases/tag/v0.1.95
[0.1.94]: https://github.com/ApexMarkdown/apex/releases/tag/v0.1.94
[0.1.93]: https://github.com/ApexMarkdown/apex/releases/tag/v0.1.93
[0.1.92]: https://github.com/ApexMarkdown/apex/releases/tag/v0.1.92
[0.1.91]: https://github.com/ApexMarkdown/apex/releases/tag/v0.1.91
[0.1.90]: https://github.com/ApexMarkdown/apex/releases/tag/v0.1.90
[0.1.89]: https://github.com/ApexMarkdown/apex/releases/tag/v0.1.89
[0.1.88]: https://github.com/ApexMarkdown/apex/releases/tag/v0.1.88
[0.1.87]: https://github.com/ApexMarkdown/apex/releases/tag/v0.1.87
[0.1.86]: https://github.com/ApexMarkdown/apex/releases/tag/v0.1.86
[0.1.85]: https://github.com/ApexMarkdown/apex/releases/tag/v0.1.85
[0.1.84]: https://github.com/ApexMarkdown/apex/releases/tag/v0.1.84
[0.1.83]: https://github.com/ApexMarkdown/apex/releases/tag/v0.1.83
[0.1.82]: https://github.com/ApexMarkdown/apex/releases/tag/v0.1.82
[0.1.81]: https://github.com/ApexMarkdown/apex/releases/tag/v0.1.81
[0.1.80]: https://github.com/ApexMarkdown/apex/releases/tag/v0.1.80
[0.1.79]: https://github.com/ApexMarkdown/apex/releases/tag/v0.1.79
[0.1.78]: https://github.com/ApexMarkdown/apex/releases/tag/v0.1.78
[0.1.77]: https://github.com/ApexMarkdown/apex/releases/tag/v0.1.77
[0.1.76]: https://github.com/ApexMarkdown/apex/releases/tag/v0.1.76
[0.1.75]: https://github.com/ApexMarkdown/apex/releases/tag/v0.1.75
[0.1.74]: https://github.com/ApexMarkdown/apex/releases/tag/v0.1.74
[0.1.73]: https://github.com/ApexMarkdown/apex/releases/tag/v0.1.73
[0.1.72]: https://github.com/ApexMarkdown/apex/releases/tag/v0.1.72
[0.1.71]: https://github.com/ApexMarkdown/apex/releases/tag/v0.1.71
[0.1.70]: https://github.com/ApexMarkdown/apex/releases/tag/v0.1.70
[0.1.69]: https://github.com/ApexMarkdown/apex/releases/tag/v0.1.69
[0.1.68]: https://github.com/ApexMarkdown/apex/releases/tag/v0.1.68
[0.1.67]: https://github.com/ApexMarkdown/apex/releases/tag/v0.1.67
[0.1.66]: https://github.com/ApexMarkdown/apex/releases/tag/v0.1.66
[0.1.65]: https://github.com/ApexMarkdown/apex/releases/tag/v0.1.65
[0.1.64]: https://github.com/ApexMarkdown/apex/releases/tag/v0.1.64
[0.1.63]: https://github.com/ApexMarkdown/apex/releases/tag/v0.1.63
[0.1.62]: https://github.com/ApexMarkdown/apex/releases/tag/v0.1.62
[0.1.61]: https://github.com/ApexMarkdown/apex/releases/tag/v0.1.61
[0.1.60]: https://github.com/ApexMarkdown/apex/releases/tag/v0.1.60
[0.1.59]: https://github.com/ApexMarkdown/apex/releases/tag/v0.1.59
[0.1.58]: https://github.com/ApexMarkdown/apex/releases/tag/v0.1.58
[0.1.57]: https://github.com/ApexMarkdown/apex/releases/tag/v0.1.57
[0.1.56]: https://github.com/ApexMarkdown/apex/releases/tag/v0.1.56
[0.1.55]: https://github.com/ApexMarkdown/apex/releases/tag/v0.1.55
[0.1.54]: https://github.com/ApexMarkdown/apex/releases/tag/v0.1.54
[0.1.53]: https://github.com/ApexMarkdown/apex/releases/tag/v0.1.53
[0.1.52]: https://github.com/ttscoff/apex/releases/tag/v0.1.52
[0.1.51]: https://github.com/ttscoff/apex/releases/tag/v0.1.51
[0.1.50]: https://github.com/ttscoff/apex/releases/tag/v0.1.50
[0.1.49]: https://github.com/ttscoff/apex/releases/tag/v0.1.49
[0.1.48]: https://github.com/ttscoff/apex/releases/tag/v0.1.48
[0.1.47]: https://github.com/ttscoff/apex/releases/tag/v0.1.47
[0.1.46]: https://github.com/ttscoff/apex/releases/tag/v0.1.46
[0.1.45]: https://github.com/ttscoff/apex/releases/tag/v0.1.45
[0.1.44]: https://github.com/ttscoff/apex/releases/tag/v0.1.44
[0.1.43]:
https://github.com/ttscoff/apex/releases/tag/v0.1.43
[0.1.42]:
https://github.com/ttscoff/apex/releases/tag/v0.1.42
[0.1.41]:
https://github.com/ttscoff/apex/releases/tag/v0.1.41
[0.1.40]:
https://github.com/ttscoff/apex/releases/tag/v0.1.40
[0.1.39]:
https://github.com/ttscoff/apex/releases/tag/v0.1.39
[0.1.38]:
https://github.com/ttscoff/apex/releases/tag/v0.1.38
[0.1.37]:
https://github.com/ttscoff/apex/releases/tag/v0.1.37
[0.1.36]:
https://github.com/ttscoff/apex/releases/tag/v0.1.36
[0.1.35]:
https://github.com/ttscoff/apex/releases/tag/v0.1.35
[0.1.34]:
https://github.com/ttscoff/apex/releases/tag/v0.1.34
[0.1.33]:
https://github.com/ttscoff/apex/releases/tag/v0.1.33
[0.1.32]:
https://github.com/ttscoff/apex/releases/tag/v0.1.32
[0.1.31]:
https://github.com/ttscoff/apex/releases/tag/v0.1.31
[0.1.30]:
https://github.com/ttscoff/apex/releases/tag/v0.1.30
[0.1.29]:
https://github.com/ttscoff/apex/releases/tag/v0.1.29
[0.1.28]:
https://github.com/ttscoff/apex/releases/tag/v0.1.28
[0.1.27]:
https://github.com/ttscoff/apex/releases/tag/v0.1.27
[0.1.26]:
https://github.com/ttscoff/apex/releases/tag/v0.1.26
[0.1.25]:
https://github.com/ttscoff/apex/releases/tag/v0.1.25
[0.1.24]:
https://github.com/ttscoff/apex/releases/tag/v0.1.24
[0.1.23]:
https://github.com/ttscoff/apex/releases/tag/v0.1.23
[0.1.20]:
https://github.com/ttscoff/apex/releases/tag/v0.1.20
[0.1.19]:
https://github.com/ttscoff/apex/releases/tag/v0.1.19
[0.1.18]:
https://github.com/ttscoff/apex/releases/tag/v0.1.18
[0.1.17]:
https://github.com/ttscoff/apex/releases/tag/v0.1.17
[0.1.16]:
https://github.com/ttscoff/apex/releases/tag/v0.1.16
[0.1.15]:
https://github.com/ttscoff/apex/releases/tag/v0.1.15
[0.1.10]:
https://github.com/ttscoff/apex/releases/tag/v0.1.10
[0.1.9]: https://github.com/ttscoff/apex/releases/tag/v0.1.9
[0.1.8]: https://github.com/ttscoff/apex/releases/tag/v0.1.8
[0.1.7]: https://github.com/ttscoff/apex/releases/tag/v0.1.7
[0.1.6]: https://github.com/ttscoff/apex/releases/tag/v0.1.6
[0.1.5]: https://github.com/ttscoff/apex/releases/tag/v0.1.5
[0.1.4]: https://github.com/ttscoff/apex/releases/tag/v0.1.4
[0.1.3]: https://github.com/ttscoff/apex/releases/tag/v0.1.3
[0.1.2]: https://github.com/ttscoff/apex/releases/tag/v0.1.2
[0.1.1]: https://github.com/ttscoff/apex/releases/tag/v0.1.1
[0.1.0]: https://github.com/ttscoff/apex/releases/tag/v0.1.0
