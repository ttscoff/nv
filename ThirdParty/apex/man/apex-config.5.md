% APEX-CONFIG(5)
% Brett Terpstra
% December 2025

# NAME

apex-config - Apex configuration file format

# DESCRIPTION

Apex supports flexible configuration through metadata files that allow you to define defaults once and reuse them across many documents, while still allowing per-document and per-invocation overrides.

Configuration can come from three main sources:

1. **Global configuration file** - `$XDG_CONFIG_HOME/apex/config.yml` or `~/.config/apex/config.yml`
2. **External metadata files** - Any file passed via `--meta-file`
3. **Per-document metadata** - YAML, MultiMarkdown, or Pandoc-style metadata in the document

Command-line flags always override configuration file settings.

# GLOBAL CONFIG FILE

When you run **apex**(1) without an explicit `--meta-file`, Apex automatically checks for a global configuration file:

- If `$XDG_CONFIG_HOME` is set: `$XDG_CONFIG_HOME/apex/config.yml`
- Otherwise: `~/.config/apex/config.yml`

If the file exists, Apex loads it as if you had passed it via `--meta-file`, and merges its metadata with the document and any `--meta` command-line metadata.

This makes `config.yml` ideal for defining project-agnostic defaults such as:

- Default `mode` (e.g. `unified`, `gfm`, `mmd`)
- Default `language` and `quoteslanguage`
- Common feature flags (`pretty`, `standalone`, `indices`, `wikilinks`, etc.)
- Default bibliography style

## Example `config.yml`

```yaml
---
# Default processor mode and output style
mode: unified
standalone: true
pretty: true

# Locale and quotation style
language: en
quoteslanguage: english

# Common feature flags
autolink: true
includes: true
relaxed-tables: true
sup-sub: true

# Index and bibliography defaults
indices: true
group-index-by-letter: true
bibliography: references.bib
csl: apa.csl

# Syntax highlighting (optional)
code-highlight: pygments
code-line-numbers: true
highlight-language-only: true
---
```

# EXTERNAL METADATA FILES

For project-specific or task-specific configurations, use `--meta-file` to point to a reusable metadata file:

    apex document.md --meta-file project-defaults.yml

These files use the same metadata keys as `config.yml` and document front matter.

## Example project config

```yaml
---
mode: unified
standalone: true
pretty: true

# Project-specific bibliography
bibliography: project-refs.bib
csl: chicago-note-bibliography.csl

# Enable wiki links and relaxed tables for this project
wikilinks: true
relaxed-tables: true
---
```

# PER-DOCUMENT METADATA

Each document can define its own metadata in YAML, MultiMarkdown, or Pandoc formats. Apex extracts this metadata before processing, merges it with any file/config metadata, and then applies it to options.

## YAML front matter example

```markdown
---
title: My Research Paper
author: Jane Doe
mode: unified
standalone: true
pretty: true
indices: false
wikilinks: true
bibliography: references.bib
language: en
quoteslanguage: english
---

# Introduction
...
```

## MultiMarkdown metadata example

```markdown
Title: My Research Paper
Author: Jane Doe
Mode: unified
Standalone: true
Pretty: true
Indices: false
WikiLinks: true
Bibliography: references.bib
Language: en
Quotes Language: english

# Introduction
...
```

Apex normalizes keys (case-insensitive, ignores spaces and dashes), so `Quotes Language`, `quoteslanguage`, and `quotes-language` are treated equivalently.

# PRECEDENCE AND MERGING

When Apex builds the final configuration for a run, it merges all sources in this order:

1. **File metadata** (lowest precedence)
   - Global `config.yml` if present and no explicit `--meta-file` was given
   - Any metadata file provided via `--meta-file`
2. **Document metadata**
   - YAML/MultiMarkdown/Pandoc metadata inside the document
3. **Command-line metadata**
   - Values passed via `--meta KEY=VALUE`
4. **Command-line flags**
   - Flags like `--mode`, `--pretty`, `--no-tables`, etc.

Later sources override earlier ones. In practice:

- A document can override defaults from `config.yml` or `--meta-file`
- `--meta` values can override both document and file metadata
- Explicit CLI flags still behave as final overrides where applicable

**Note:** If `mode` is specified in metadata (file or document), Apex resets options to that mode's defaults before applying other metadata keys, so the mode behaves as if you had passed it on the command line first.

# CONFIGURATION KEYS

Configuration keys correspond closely to Apex command-line options. Most options can be controlled via metadata in `config.yml`, `--meta-file` files, or per-document metadata.

## Boolean options

Boolean keys accept any of the following values (case-insensitive):

- `true` / `false`
- `yes` / `no`
- `1` / `0`

**Supported boolean keys include:**

- `indices` - Enable index processing
- `wikilinks` - Enable wiki link syntax (`[[Page]]`)
- `wikilink-sanitize` - Sanitize wiki link URLs (lowercase, remove apostrophes, clean non-alphanumeric)
- `includes` - Enable file inclusion
- `relaxed-tables` - Enable relaxed table parsing
- `alpha-lists` - Enable alphabetic list markers
- `mixed-lists` - Allow mixing list marker types at the same level
- `sup-sub` - Enable MultiMarkdown-style superscript/subscript
- `autolink` - Enable automatic URL/email linking
- `transforms` - Enable metadata transforms (`[%key:transform]`)
- `unsafe` - Allow raw HTML in output
- `tables` - Enable/disable table support
- `footnotes` - Enable/disable footnote support
- `smart` - Enable/disable smart typography
- `math` - Enable/disable math support
- `ids` - Enable/disable automatic header IDs
- `header-anchors` - Use `<a>` anchors instead of plain `id` attributes
- `embed-images` - Enable base64 image embedding
- `link-citations` - Link citations to bibliography entries
- `show-tooltips` - Add tooltips to citations
- `suppress-bibliography` - Suppress bibliography output
- `suppress-index` - Suppress index output while still creating markers
- `group-index-by-letter` - Group index entries alphabetically
- `obfuscate-emails` - Hex-encode `mailto:` links and visible email addresses
- `pretty` - Pretty-print HTML
- `standalone` - Generate a full HTML document (`<html>`, `<head>`, `<body>`)
- `hardbreaks` - Treat newlines as `<br>`
- `plugins` - Enable plugin processing (see **apex-plugins**(7))
- `code-line-numbers` - Include line numbers in syntax-highlighted code blocks
- `highlight-language-only` - Only highlight code blocks that have a language specified

## String options

String keys take a free-form string value (sometimes with a constrained set of options):

- `mode`
  - Values: `commonmark`, `gfm`, `mmd`/`multimarkdown`, `kramdown`, `unified`
- `id-format`
  - Values: `gfm`, `mmd`, `kramdown`
- `title`
  - Document title (used especially in standalone mode)
- `style` / `css`
  - Stylesheet path for standalone documents
- `base-dir`
  - Base directory used for resolving relative paths (images, includes, etc.)
- `bibliography`
  - Path to a bibliography file (`.bib`, `.json`, `.yml`, `.yaml`)
- `csl`
  - Path to a CSL style file
- `language`
  - BCP 47 or similar language tag (e.g. `en`, `en-US`, `fr`); sets the HTML `lang` attribute
- `quoteslanguage` / `Quotes Language`
  - Human-readable language name used for quote styling (e.g. `english`, `french`, `german`)
- `code-highlight`
  - External syntax highlighting tool: `pygments` (or `p`/`pyg`), `skylighting` (or `s`/`sky`), `shiki` (or `sh`), or `false`/`none` to disable
- `code-highlight-theme` / `code_highlight_theme`
  - Preferred syntax highlighting theme/style name. Maps to Pygments styles, Skylighting styles, or Shiki themes in both HTML and terminal/ANSI output. Use `apex --list-themes` for an overview of built-in themes.

You can also use arbitrary keys for your own templates and transforms; Apex simply passes them through to the metadata system so they can be referenced via `[%key]` and `[%key:transform]`.

# EXAMPLES

A typical setup might look like this:

1. **Global defaults** in `config.yml` for your overall writing style and locale.
2. **Project config** via `--meta-file project.yml` for bibliography and project-specific settings.
3. **Per-document metadata** for title, author, and any exceptions to the defaults.
4. Occasional **CLI overrides** for ad-hoc one-off changes (e.g. `--mode gfm` for a specific run).

Because all three configuration layers use the same metadata keys, you can gradually refine behavior without repeating yourself:

- Put long-lived, cross-project settings in `config.yml`
- Put project-scoped settings in `--meta-file` configs
- Put document-specific settings in front matter

# SEE ALSO

**apex**(1), **apex-plugins**(7)

For complete documentation, see the [Apex Wiki](https://github.com/ttscoff/apex/wiki).

# AUTHOR

Brett Terpstra

# COPYRIGHT

Copyright (c) 2025 Brett Terpstra. Licensed under MIT License.
