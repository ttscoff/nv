# Apex Documentation Generators

This directory contains scripts to generate documentation from the Apex wiki.

## Scripts

### `generate_docset.rb`

Generates Dash docsets for Apex documentation.

**Usage:**
```bash
./generate_docset.rb [single|multi]
```

- `single` - Generate single-page CLI options docset (ApexCLI.docset)
- `multi` - Generate multi-page docset from wiki files (Apex.docset, default)

**Output:**
- Docsets are generated in `docsets/` directory
- The script automatically clones the wiki from GitHub and cleans up after running

### `generate_single_html.rb`

Generates a single HTML file with all wiki pages, using JavaScript to show/hide pages.

**Usage:**
```bash
./generate_single_html.rb
```

**Output:**
- HTML file is generated in `html/apex-docs.html`
- The script automatically clones the wiki from GitHub and cleans up after running

### `generate_app_docs.rb`

Generates app-focused documentation from pre-transformed Markdown files. This script reads from `app-transformed/` and generates a single HTML file suitable for embedding in apps like Marked.app.

**Usage:**
```bash
./generate_app_docs.rb
```

**Output:**
- HTML file is generated in `html/apex-app-docs.html`
- Settings table is generated in `app-settings-table.md`
- Uses pre-transformed files from `app-transformed/` directory

**Note:** This script requires pre-transformed Markdown files. See "AI Transformation Workflow" below for how to create them.

### `generate_app_docs_ai.rb`

Uses AI (via cursor-agent or similar) to transform CLI-focused documentation into app-focused documentation. This script can automatically transform wiki pages by replacing CLI references with app Settings references.

**Usage:**
```bash
./generate_app_docs_ai.rb
```

**Requirements:**
- `cursor-agent` or similar AI agent tool
- `transform_for_app.md` file (copy from `transform_for_app.example.md`)

**Output:**
- Transformed Markdown files in `app-transformed/` directory
- HTML file is generated in `html/apex-app-docs.html`
- Settings table is generated in `app-settings-table.md`

**Note:** This script requires manual setup. See "AI Transformation Workflow" below.

## Requirements

- Ruby
- Apex binary (in PATH or in `../build-release/apex`)
- `sqlite3` gem (for multi-page docset): `gem install sqlite3`
- `mmd2cheatset` (for single-page docset): Should be at `~/Desktop/Code/mmd2cheatset/mmd2cheatset.rb`
- Git (for cloning the wiki)

## Directory Structure

```
documentation/
├── generate_docset.rb           # Dash docset generator
├── generate_single_html.rb      # Single HTML file generator
├── generate_app_docs.rb         # App docs generator (uses pre-transformed files)
├── generate_app_docs_ai.rb      # AI-powered app docs transformer
├── transform_for_app.example.md # Example transformation instructions
├── docsets/                     # Generated docsets (gitignored)
│   ├── Apex.docset
│   └── ApexCLI.docset
├── html/                        # Generated HTML files (gitignored)
│   ├── apex-docs.html
│   └── apex-app-docs.html
├── app-transformed/             # Transformed Markdown files (gitignored)
│   ├── Syntax.md
│   ├── Modes.md
│   ├── Plugins.md
│   └── ...
└── app-settings-table.md        # Generated settings reference table
```

## How It Works

### Standard Documentation Generation

The standard scripts (`generate_docset.rb` and `generate_single_html.rb`):
1. Clone the wiki from `https://github.com/ApexMarkdown/apex.wiki.git` into `apex.wiki/`
2. Process the wiki files
3. Generate the output
4. Clean up by removing the cloned wiki directory

This ensures the latest version of the wiki is always used and no local wiki clone is required.

### AI Transformation Workflow

The app-focused documentation uses a hybrid approach:

1. **Manual Transformation (Recommended)**
   - Copy `transform_for_app.example.md` to `transform_for_app.md`
   - Edit `transform_for_app.md` to match your app's settings structure
   - Manually transform wiki pages using the instructions in `transform_for_app.md`
   - Save transformed files to `app-transformed/` directory
   - Run `generate_app_docs.rb` to generate the HTML

2. **AI-Powered Transformation (Experimental)**
   - Copy `transform_for_app.example.md` to `transform_for_app.md`
   - Edit `transform_for_app.md` if needed for your app
   - Ensure `cursor-agent` or similar AI tool is available
   - Run `generate_app_docs_ai.rb` to automatically transform files
   - Review and edit the generated files in `app-transformed/` as needed
   - Run `generate_app_docs.rb` to generate the HTML

**Transformation Instructions:**

The `transform_for_app.example.md` file contains comprehensive instructions for transforming CLI-focused documentation to app-focused documentation. Key transformations include:

- Replace CLI flags (`--flag`) with Settings references (`Settings->Category->Setting Name`)
- Remove command-line examples (code blocks showing `apex` commands)
- Remove links to "Command Line Options" page
- Add app-specific instructions (e.g., "Marked will automatically detect...")
- Page-specific transformations for Multi-File-Documents, Plugins, Modes, etc.

**Example:**

**Before (CLI-focused):**
> Use the `--plugins` flag to enable plugins:
>
> ```bash
> apex document.md --plugins
> ```

**After (App-focused):**
> Enable plugins by checking **Settings->Processor->Include Plugins**.

**Important Notes:**

- The `app-transformed/` directory and `transform_for_app.md` are gitignored
- This allows you to maintain app-specific documentation without committing it to the public repo
- The example file (`transform_for_app.example.md`) is tracked in git as a template
