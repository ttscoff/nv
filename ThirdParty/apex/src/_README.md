<!--README-->
[![Version: <!--VER-->1.1.46<!--END VER-->](https://img.shields.io/badge/Version-<!--VER-->1.1.46<!--END VER-->-528c9e)](https://github.com/ApexMarkdown/apex/releases/latest) ![](https://img.shields.io/badge/CMake-064F8C?style=for-the-badge&logo=cmake&logoColor=white) [![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT) <!--TESTS_BADGE-->![Tests passing 0/2372](https://img.shields.io/badge/Tests-0/2372-f97373)<!--END TESTS_BADGE-->

<!--GITHUB-->
# Apex
<!--END GITHUB-->

Apex is a unified Markdown processor that combines the best
features from CommonMark, GitHub Flavored Markdown (GFM),
MultiMarkdown, Kramdown, and Marked. One processor to rule
them all.

<!--GITHUB-->
![](apex-header-2-rb@2x.webp)
<!--END GITHUB-->

<!--JEKYLL {% img alignright /uploads/2025/12/apexicon.png 300 300 "Apex Icon" %}-->There are so many variations of
Markdown, extending its features in all kinds of ways. But
picking one flavor means giving up the features of another
flavor. So I'm building Apex with the goal of making all of
the most popular features of various processors available in
one tool.

## Table of Contents

- [Features](#features)
  - [Compatibility Modes](#compatibility-modes)
  - [Markdown Extensions](#markdown-extensions)
  - [Document Features](#document-features)
  - [Citations and Bibliography](#citations-and-bibliography)
  - [Indices](#indices)
  - [Critic Markup](#critic-markup)
  - [Output Options](#output-options)
  - [Advanced Features](#advanced-features)
  - [Extensibility and Plugins](#extensibility-and-plugins)
- [Installation](#installation)
- [Basic Usage](#basic-usage)
- [Documentation](#documentation)
- [Contributing](#contributing)
- [License](#license)

## Features

### Compatibility Modes

- **Multiple compatibility modes**: [CommonMark, GFM, MultiMarkdown, Kramdown, and Unified](https://github.com/ApexMarkdown/apex/wiki/Modes) (all features)
- **Mode-specific features**: Each mode enables appropriate extensions for maximum compatibility ([more info](https://github.com/ApexMarkdown/apex/wiki/Modes))

### Markdown Extensions

- **Tables**: [GitHub Flavored Markdown tables](https://github.com/ApexMarkdown/apex/wiki/Tables) with advanced features (rowspan via `^^`, colspan via empty cells/`<<`, captions before/after tables including Pandoc-style `Table: Caption` and `: Caption` syntax, and individual cell alignment using colons `:Left`, `Right:`, `:Center:`)
- **Grid tables**: Pandoc-style `+---+` grid tables (opt-in with `--grid-tables`; [more info](https://github.com/ApexMarkdown/apex/wiki/Tables#grid-tables-pandoc-style))
- **Table caption positioning**: Control caption placement with `--captions above` or `--captions below` (default: below)
- **Table caption IAL**: IAL attributes in table captions (e.g., `: Caption {#id .class}`) are extracted and applied to the table element
- **Relaxed tables**: Support for tables without separator rows (Kramdown-style)
- **Headerless tables**: Support for tables that start with alignment rows (separator rows) without header rows; column alignment is automatically applied
- **Footnotes**: Three syntaxes supported (reference-style, Kramdown inline, MultiMarkdown inline) ([more info](https://github.com/ApexMarkdown/apex/wiki/Syntax))
- **Definition lists**: Kramdown-style definition lists with Markdown content support ([more info](https://github.com/ApexMarkdown/apex/wiki/Syntax))
- **Task lists**: GitHub-style checkboxes (`- [ ]` and `- [x]`)
- **Strikethrough**: `~~text~~` syntax from GFM
- **Smart typography**: Automatic conversion of quotes, dashes, ellipses, and more
- **Math support**: LaTeX math expressions with `$...$` (inline) and `$$...$$` (display)

- **Syntax highlighting**: External syntax highlighting for fenced code blocks via Pygments, Skylighting, or Shiki with `--code-highlight` flag.

  Supports language-aware highlighting, auto-detection, and line numbers with `--code-line-numbers` ([more info](https://github.com/ApexMarkdown/apex/wiki/Command-Line-Options))

- **Wiki links**: `[[Page Name]]`, `[[Page Name|Display Text]]`, and `[[Page Name#Section]]` syntax with configurable link targets via `--wikilink-space` and `--wikilink-extension` ([more info](https://github.com/ApexMarkdown/apex/wiki/Syntax))
- **Abbreviations**: Three syntaxes (classic MMD, MMD 6 reference, MMD 6 inline)
- **Callouts**: Bear/Obsidian-style callouts with collapsible support (`> [!NOTE]`, `> [!WARNING]`, etc.), plus optional Python-Markdown (`!!!`) and Quarto (`:::`) callout parsing behind explicit flags ([more info](https://github.com/ApexMarkdown/apex/wiki/Callouts))
- **GitHub emoji**: 350+ emoji support (`:rocket:`, `:heart:`, etc.)

### Document Features

<!--JEKYLL {% raw %}-->

- **Metadata blocks**: YAML front matter, MultiMarkdown metadata, and Pandoc title blocks ([more info](https://github.com/ApexMarkdown/apex/wiki/Syntax))
- **Metadata variables**: Insert metadata values with `[%key]` syntax ([more info](https://github.com/ApexMarkdown/apex/wiki/Configuration))
- **Metadata transforms**: Transform metadata values with `[%key:transform]` syntax

  Supports case conversion, string manipulation, regex replacement, date formatting, and more.

  See [Metadata Transforms](https://github.com/ApexMarkdown/apex/wiki/Metadata-Transforms) for complete documentation
- **Metadata control of options**: [Control command-line options via metadata](https://github.com/ApexMarkdown/apex/wiki/Configuration)
  - set boolean flags (`indices: false`, `wikilinks: true`) and string options (`bibliography: refs.bib`, `title: My Document`, `wikilink-space: dash`, `wikilink-extension: html`) directly in document metadata for per-document configuration
- **Table of Contents**: Automatic TOC generation with depth control using HTML (`<!--TOC-->`), MMD (`{{TOC}}` / `{{TOC:2-4}}`), and Kramdown `{:toc}` markers. Headings marked with `{:.no_toc}` are excluded from the generated TOC. ([more info](https://github.com/ApexMarkdown/apex/wiki/Syntax))
- **File includes**: Three syntaxes (Marked `<<[file]`, MultiMarkdown `{{file}}`, iA Writer `/file`), with support for address ranges and wildcard/glob patterns such as `{{file.*}}`, `{{*.md}}`, and `{{c?de.py}}`. ([more info](https://github.com/ApexMarkdown/apex/wiki/Multi-File-Documents))
- **Markdown combiner (`--combine`)**: Concatenate one or more Markdown files into a single Markdown stream, expanding all include syntaxes.

  When a `SUMMARY.md` file is provided, Apex treats it as a GitBook-style index and combines the linked files in order, perfect for building books, multi-file indices, and shared tables of contents that can then be piped back into Apex for final rendering. ([more info](https://github.com/ApexMarkdown/apex/wiki/Multi-File-Documents))
- **MultiMarkdown merge (`--mmd-merge`)**: Read one or more mmd_merge-style index files and stitch their referenced documents into a single Markdown stream.

  Each non-empty, non-comment line specifies a file to include; indentation with tabs or four-space groups shifts all headings in that file down by one level per indent, mirroring the original `mmd_merge.pl` behavior.

  Output is raw Markdown that can be piped into Apex (e.g., `apex --mmd-merge index.txt | apex --mode mmd`). ([more info](https://github.com/ApexMarkdown/apex/wiki/Multi-File-Documents))

- **CSV/TSV support**: Automatic table conversion from CSV and TSV files ([more info](https://github.com/ApexMarkdown/apex/wiki/Tables))
- **Inline Attribute Lists (IAL)**: [Kramdown-style attributes](https://github.com/ApexMarkdown/apex/wiki/Inline-Attribute-Lists) `{: #id .class}` and Pandoc-style attributes `{#id .class}`

  Both formats work in all contexts (block-level, inline, paragraphs, headings, table captions)
- **Bracketed spans**: Convert `[text]{IAL}` syntax to HTML span elements with attributes, enabled by default in unified mode ([more info](https://github.com/ApexMarkdown/apex/wiki/Syntax))
- **Fenced divs**: Pandoc-style fenced divs `::::: {#id .class} ... :::::` for creating custom block containers, enabled by default in unified mode.

  Supports block type syntax `>blocktype` to create different HTML elements (e.g., `::: >aside {.sidebar}` creates `<aside>` instead of `<div>`). Common block types include `aside`, `article`, `section`, `details`, `summary`, `header`, `footer`, `nav`, and custom elements ([more info](https://github.com/ApexMarkdown/apex/wiki/Syntax))
- **Image IAL support**: Inline and reference-style images support IAL syntax with automatic width/height conversion (percentages and non-integer/non-px values convert to style attributes, Xpx values convert to integer width/height attributes, bare integers remain as width/height attributes) ([more info](https://github.com/ApexMarkdown/apex/wiki/Multi-Format-Images))
- **Special markers**: Page breaks (`<!--BREAK-->`), autoscroll pauses (`<!--PAUSE:N-->`), end-of-block markers
<!--JEKYLL {% endraw %}-->

### Citations and Bibliography

See the [Citations and Bibliography](https://github.com/ApexMarkdown/apex/wiki/Citations) wiki page for a complete guide.

- **Multiple citation syntaxes**: Pandoc (`[@key]`), MultiMarkdown (`[#key]`), and mmark (`[@RFC1234]`) styles
- **Bibliography formats**: Support for BibTeX (`.bib`), CSL JSON (`.json`), and CSL YAML (`.yml`, `.yaml`) formats
- **Automatic bibliography generation**: Bibliography automatically generated from cited entries
- **Citation linking**: Option to link citations to bibliography entries
- **Metadata support**: Bibliography can be specified in document metadata or via command-line flags
- **Multiple bibliography files**: Support for loading and merging multiple bibliography files
- **CSL style support**: Citation Style Language (CSL) files for custom citation formatting
- **Mode support**: Citations enabled in MultiMarkdown and unified modes

### Indices

See the [Indices](https://github.com/ApexMarkdown/apex/wiki/Indices) wiki page for syntax and examples.

- **mmark syntax**: `(!item)`, `(!item, subitem)`, `(!!item, subitem)` for primary entries
- **TextIndex syntax**: `{^}`, `[term]{^}`, `{^params}` for flexible indexing
- **Automatic index generation**: Index automatically generated at end of document or at `<!--INDEX-->` marker
- **Alphabetical sorting**: Entries sorted alphabetically with optional grouping by first letter
- **Hierarchical sub-items**: Support for nested index entries
- **Mode support**: Indices enabled by default in MultiMarkdown and unified modes

### Critic Markup

- **Change tracking**: Additions (`{++text++}`), deletions (`{--text--}`), substitutions (`{~~old~>new~~}`) ([more info](https://github.com/ApexMarkdown/apex/wiki/Syntax))
- **Annotations**: Highlights (`{==text==}`) and comments (`{>>text<<}`)
- **Accept mode**: `--accept` flag to apply all changes for final output
- **Reject mode**: `--reject` flag to revert all changes to original

### Output Options

- **Flexible output**: Compact HTML fragments, pretty-printed HTML, or complete standalone documents
- **Standalone documents**: Generate complete HTML5 documents with `<html>`, `<head>`, `<body>` tags
- **Custom styling**: Link multiple external CSS files in standalone mode (use `--css` multiple times or comma-separated list)
- **Syntax highlighting**: External syntax highlighting via Pygments, Skylighting, or Shiki with `--code-highlight` flag, includes automatic GitHub-style CSS in standalone mode
- **Pretty-print**: Formatted HTML with proper indentation for readability
- **XHTML output**: `--xhtml` writes void/empty elements in XML form (`<br />`, `<meta ... />`). `--strict-xhtml` adds polyglot XHTML document scaffolding when used with `--standalone` (XML declaration, XHTML namespace, `Content-Type` meta). You can also select the same behavior with `-t xhtml` or `-t strict-xhtml` (aliases for HTML output with those flags). In **fragment** mode, strict mode does not validate or repair all markup as XML; raw HTML can still be ill-formed.
- **Header ID generation**: [Automatic or manual header IDs](https://github.com/ApexMarkdown/apex/wiki/Header-IDs) with multiple format options (GFM, MMD, Kramdown)
- **Emoji-to-name conversion**: In GFM mode, emojis in headers are converted to their textual names in IDs (e.g., `# 😄 Support` → `id="smile-support"`), matching Pandoc's GFM behavior
- **Header anchors**: Option to generate `<a>` anchor tags instead of header IDs
- **ARIA accessibility**: Add ARIA labels and accessibility attributes (`--aria`) for better screen reader support, including aria-label on TOC navigation, role attributes on figures and tables, and aria-describedby linking tables to their captions
- **Terminal output**: Render Markdown in the terminal with `-t terminal` / `-t terminal256`, themes, pagination, and optional inline images ([more info](https://github.com/ApexMarkdown/apex/wiki/Rendering-Markdown-In-Terminal))
- **RTF output**: Native Rich Text Format via `-t rtf` (files and `NSAttributedString`); see clipboard notes in [Command Line Options](https://github.com/ApexMarkdown/apex/wiki/Command-Line-Options)
- **Terminal inline images**: With `-t terminal` / `-t terminal256`, when stdout is a TTY and a viewer is available on `PATH`, Markdown images are rendered as inline terminal graphics (viewer order: `imgcat`, `chafa`, `viu`, `catimg`). Width is controlled with `--terminal-image-width` (default 50 character cells). HTTP(S) URLs are downloaded with `curl` (60s timeout, 10 MiB max) to a temp file under `TMPDIR` or `/tmp`. Use `--no-terminal-images` to always show images as styled link text plus URL instead. Metadata: `terminal.inline_images` / `terminal_inline_images`, `terminal.image_width` / `terminal_image_width`.

### Advanced Features

- **Hard breaks**: Option to treat newlines as hard line breaks
- **Feature toggles**: Granular control to enable/disable specific features (tables, footnotes, math, smart typography, etc.)
- **Unsafe HTML**: Option to allow or block raw HTML in documents
- **Autolinks**: Automatic URL detection and linking
- **Superscript/Subscript**: Support for `^superscript^` and `~subscript~` syntax

### Extensibility and Plugins

Apex supports a flexible plugin system that lets you add new syntax and post-processing features in any language while keeping the core parser stable and fast. Plugins are disabled by default so there is no performance impact unless you opt in. Enable them per run with `--plugins`, or per document with a `plugins: true` (or `enable-plugins: true`) key in your metadata.

You can manage plugins from the CLI:

- Install plugins with `--install-plugin`:

From the central directory using an ID: `--install-plugin kbd`

Directly from a Git URL or GitHub shorthand: `--install-plugin https://github.com/user/repo.git` or `--install-plugin user/repo`

- Uninstall a local plugin with `--uninstall-plugin ID`.
- See installed and available plugins with `--list-plugins`.

When installing from a direct Git URL or GitHub repo name,
Apex will prompt with a security warning before cloning,
since plugins execute unverified code.

For a complete guide to writing, installing, and publishing plugins, see the [Plugins](https://github.com/ApexMarkdown/apex/wiki/Plugins) page in the Apex Wiki. App developers can also use the [plugin catalog API](https://github.com/ApexMarkdown/apex/wiki/Xcode-Integration#plugin-catalog-and-installation) from Swift or Objective-C ([C API](https://github.com/ApexMarkdown/apex/wiki/C-API#plugin-catalog-api)).

**AST filters** (Pandoc-style JSON filters) are also supported via `--filter`, `--filters`, and `--lua-filter`. See [Filters](https://github.com/ApexMarkdown/apex/wiki/Filters).

## Installation

See the [Installation](https://github.com/ApexMarkdown/apex/wiki/Installation) wiki page for additional build options and platform notes.

### Homebrew (macOS/Linux)

```bash
brew trust ttscoff/thelab
brew tap ttscoff/thelab
brew install ttscoff/thelab/apex

```

### Building from Source

```bash
git clone https://github.com/ApexMarkdown/apex.git
cd apex
git submodule update --init --recursive
make

```

The `apex` binary will be in the `build/` directory.

To install the built binary and libraries system-wide:

```bash
make install

```

**Note:** The default `make` command runs both `cmake -S . -B build` (to configure the project) and `cmake --build build` (to compile). If you prefer to run cmake commands directly, you can use those instead.

### Pre-built Binaries

Download pre-built binaries from the [latest release](https://github.com/ApexMarkdown/apex/releases/latest). Binaries are available for:

- macOS (Universal binary for arm64 and x86_64)
- Linux (x86_64 and arm64)

## Basic Usage

See [Usage](https://github.com/ApexMarkdown/apex/wiki/Usage) and [Getting Started](https://github.com/ApexMarkdown/apex/wiki/Getting-Started) for more examples.

### Command Line

```bash
# Process a markdown file
apex input.md

# Output to a file
apex input.md -o output.html

# Generate standalone HTML document
apex input.md --standalone --title "My Document"

# Pretty-print HTML output
apex input.md --pretty

```

### Processing Modes

Apex supports multiple [compatibility modes](https://github.com/ApexMarkdown/apex/wiki/Modes):

- `--mode commonmark` - Pure CommonMark specification
- `--mode gfm` - GitHub Flavored Markdown

`--mode mmd` or `--mode multimarkdown` - MultiMarkdown compatibility

- `--mode kramdown` - Kramdown compatibility
- `--mode unified` - All features enabled (default)

```bash
# Use GFM mode
apex input.md --mode gfm

# Use Kramdown mode with relaxed tables
apex input.md --mode kramdown

```

### Common Options

See [Command Line Options](https://github.com/ApexMarkdown/apex/wiki/Command-Line-Options) for the full reference.

- `--pretty` - Pretty-print HTML with indentation

`--standalone` - Generate complete HTML document with `<html>`, `<head>`, `<body>`

`--style FILE` / `--css FILE` - Link to CSS file(s) in document head (requires `--standalone`). Can be used multiple times or with comma-separated list (e.g., `--css style.css --css syntax.css` or `--css style.css,syntax.css`)

`--embed-css` - Embed CSS file contents as inline `<style>` tags instead of `<link>` tags (works with multiple stylesheets)

- `--title TITLE` - Document title (requires `--standalone`)
- `--relaxed-tables` - Enable relaxed table parsing (default

  in unified/kramdown modes)

- `--no-relaxed-tables` - Disable relaxed table parsing

`--captions POSITION` - Table caption position: `above` or `below` (default: `below`)

`--id-format FORMAT` - Header ID format: `gfm`, `mmd`, or `kramdown`

- `--no-ids` - Disable automatic header ID generation
- `--header-anchors` - Generate `<a>` anchor tags instead of

  header IDs

- `--aria` - Add ARIA labels and accessibility attributes to

  HTML output

- `--bibliography FILE` - Bibliography file (BibTeX, CSL

  JSON, or CSL YAML) - can be used multiple times

- `--csl FILE` - Citation style file (CSL format)

`--link-citations` - Link citations to bibliography entries

- `--indices` - Enable index processing (mmark and TextIndex

  syntax)

- `--no-indices` - Disable index processing
- `--no-index` - Suppress index generation (markers still

  created)

`--wikilinks` - Enable wiki link syntax `[[Page]]`, `[[Page|Display]]`, and `[[Page#Section]]`

`--wikilink-space MODE` - Control how spaces in wiki link page names are converted (`dash`, `none`, `underscore`, `space`; default: `dash`)

`--wikilink-extension EXT` - File extension to append to wiki link URLs (e.g. `html`, `md`)

- `--divs` / `--no-divs` - Enable/disable Pandoc fenced divs

  syntax (enabled by default in unified mode)

- `--py-callouts` / `--no-py-callouts` - Enable/disable Python-Markdown callout syntax (`!!!`) plus markdown-callouts label syntax (`NOTE: ...`, `>? NOTE: ...`) (disabled by default)

- `--quarto-callouts` / `--no-quarto-callouts` - Enable/disable Quarto `:::` callout syntax for `.callout-*` blocks (disabled by default; recognized callouts bypass generic div processing)

`--spans` / `--no-spans` - Enable/disable bracketed spans `[text]{IAL}` syntax (enabled by default in unified mode)

`--code-highlight TOOL` - Use external tool for syntax highlighting (supports `pygments`/`p`/`pyg`, `skylighting`/`s`/`sky`, or `shiki`/`sh`). Uses HTML or ANSI output based on destination format. Automatically includes GitHub-style CSS in standalone mode

`--code-line-numbers` - Include line numbers in syntax-highlighted code blocks (requires `--code-highlight`)

### All Options

```
@cli(build/apex -h 2>&1)

```

### Per-Document Configuration via Metadata

Most command-line options can be controlled via document
metadata, allowing different files to be processed with
different settings when processing batches. Boolean options
accept `true`/`false`, `yes`/`no`, or `1`/`0`
(case-insensitive). String options use the value directly.

See [Configuration](https://github.com/ApexMarkdown/apex/wiki/Configuration) for the complete list of metadata-controlled options.

**Example:**

```yaml
---
indices: false
wikilinks: true
bibliography: references.bib
title: My Research Paper
pretty: true
standalone: true
---

```

This allows you to process multiple files with `apex *.md` and have each file use its own configuration. You can also use `--meta-file` to specify a shared configuration file that applies to all processed files.

## Documentation

For complete documentation, see the [Apex Wiki](https://github.com/ApexMarkdown/apex/wiki).

Key documentation pages:

- [Getting Started](https://github.com/ApexMarkdown/apex/wiki/Getting-Started) - Your first steps with Apex
- [Installation](https://github.com/ApexMarkdown/apex/wiki/Installation) - Build and install options
- [Usage](https://github.com/ApexMarkdown/apex/wiki/Usage) - Basic usage examples
- [Syntax](https://github.com/ApexMarkdown/apex/wiki/Syntax) - Complete syntax reference
- [Modes](https://github.com/ApexMarkdown/apex/wiki/Modes) - Processor compatibility modes
- [Tables](https://github.com/ApexMarkdown/apex/wiki/Tables) - Table syntax, captions, rowspan/colspan
- [Callouts](https://github.com/ApexMarkdown/apex/wiki/Callouts) - Callout formats and flags
- [Command Line Options](https://github.com/ApexMarkdown/apex/wiki/Command-Line-Options) - All CLI flags explained
- [Configuration](https://github.com/ApexMarkdown/apex/wiki/Configuration) - Metadata-controlled options
- [Multi-File Documents](https://github.com/ApexMarkdown/apex/wiki/Multi-File-Documents) - `--combine`, `--mmd-merge`, includes
- [Citations](https://github.com/ApexMarkdown/apex/wiki/Citations) - Citations and bibliographies
- [Indices](https://github.com/ApexMarkdown/apex/wiki/Indices) - Index generation
- [Plugins](https://github.com/ApexMarkdown/apex/wiki/Plugins) - Plugin system and recipes
- [Filters](https://github.com/ApexMarkdown/apex/wiki/Filters) - AST filters
- [Xcode Integration](https://github.com/ApexMarkdown/apex/wiki/Xcode-Integration) - Swift Package Manager and app integration
- [C API](https://github.com/ApexMarkdown/apex/wiki/C-API) - Programmatic API
- [Pandoc Integration](https://github.com/ApexMarkdown/apex/wiki/Pandoc-Integration) - Use Apex with Pandoc
- [Troubleshooting](https://github.com/ApexMarkdown/apex/wiki/Troubleshooting) - Common issues

## Contributing

Contributions are welcome! Please feel free to submit a Pull
Request.

Please note the [tests requirement for new features](https://github.com/ApexMarkdown/apex/wiki/Writing-Tests).

1. Fork the [repository](https://github.com/ApexMarkdown/apex)
2. Create your feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add some amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

## License

This project is licensed under the MIT License - see the [LICENSE](https://github.com/ApexMarkdown/apex/blob/main/LICENSE) file for details.
<!--END README-->
