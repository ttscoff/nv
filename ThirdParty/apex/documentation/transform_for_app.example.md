# Documentation Transformation Instructions for App Usage

This file contains instructions for transforming Apex documentation from command-line focused to app-focused (e.g., for Marked.app). Copy this file to `transform_for_app.md` and edit it as needed for your specific app.

## General Transformation Rules

### 1. Replace CLI Flags with Settings References

Replace all command-line flags with app settings paths using the format: `Settings->Category->Setting Name`

**Examples:**
- `--plugins` → `Settings->Processor->Include Plugins`
- `--mode unified` → Select `unified` in **Settings->Processor->Mode**
- `--meta-file FILE` → Select a metadata file in **Settings->Processor->Metadata File**, or specify a file in document metadata
- `--bibliography FILE` → Add bibliography files to the list in **Settings->Processor->Citations**, or specify in document metadata

**Patterns to replace:**
- `--[no-]flag` → `Settings->Category->Setting Name` (with enable/disable context)
- `--flag VALUE` → Set the value in **Settings->Category->Setting Name**
- `apex document.md --flag` → Use **Settings->Category->Setting Name** in the app

### 2. Remove Command-Line Examples

Remove all code blocks that contain only command-line examples. These are not relevant in an app context where the app handles execution.

**What to remove:**
- Code blocks showing `apex` commands
- Code blocks showing command-line usage examples
- Examples like:
  ```bash
  apex document.md --mode unified
  apex --plugins document.md
  ```

**What to keep:**
- Code blocks showing Markdown syntax examples
- Code blocks showing configuration file examples (YAML, JSON, etc.)
- Code blocks showing actual code (Python, Ruby, etc.)

### 3. Remove References to Command Line Options

- Remove all links to "Command Line Options" page
- Remove phrases like "See [Command Line Options](Command-Line-Options) for details"
- Replace with app-specific instructions or remove entirely

### 4. Rewrite Usage Instructions

Transform instructions from "how to run commands" to "how to use the app":

**Before:**
> Use the `--mode` flag to select the processor mode.

**After:**
> Select the processor mode from **Settings->Processor->Mode**.

**Before:**
> Run `apex document.md --plugins` to enable plugins.

**After:**
> Enable plugins by checking **Settings->Processor->Include Plugins**.

## Page-Specific Transformations

### Multi-File-Documents

**Key changes:**
1. Indicate that Marked will automatically detect the file format for merge documents
2. Specify that mmd-merge documents must start with a `#merge` line to be automatically detected
3. Replace CLI examples with app usage instructions

**Example transformation:**
- Add: "Marked will automatically detect the file format for merge documents."
- Add: "MMD merge documents must start with a `#merge` line to be automatically detected by Marked."
- Replace: "Use `apex --combine file1.md file2.md`" → "Select multiple files and enable **Settings->Processor->Combine Files**"

### Plugins

**Key changes:**
1. Indicate that Marked has an "Install Plugins" option that allows selecting a plugin directory and automatically installing to `~/.config/apex/plugins`
2. Mention that Marked lists available plugins in a Plugins panel within Processor settings
3. Remove all references and links to "Command Line Options"
4. Replace CLI installation examples with app-based installation instructions

**Example transformation:**
- Add: "Marked provides an **Install Plugins** option that allows you to select a plugin directory on your disk and have it automatically installed to `~/.config/apex/plugins`."
- Add: "Marked will list available plugins in a **Plugins panel** within the Processor settings."
- Remove: "Install plugins using `apex --install-plugin ID-or-URL`"

### Modes

**Key changes:**
1. Rewrite to indicate that modes (unified, multimarkdown, commonmark, kramdown, gfm) can be selected from the Settings->Processor panel
2. Keep the descriptions of the modes the same
3. Remove CLI command examples
4. Remove links to "Command Line Options"

**Example transformation:**
- Replace: "Use `--mode unified` to enable unified mode" → "Select `unified` from **Settings->Processor->Mode**"
- Keep: All mode descriptions and feature lists
- Remove: All `apex --mode` examples

### Header IDs

**Key changes:**
1. Reference a Settings->Processor->Header IDs option that will be a dropdown for selecting the header ID mode
2. Remove CLI command examples
3. Remove links to "Command Line Options"

**Example transformation:**
- Replace: "Use `--id-format gfm`" → "Select `gfm` from **Settings->Processor->Header ID Format**"
- Replace: "Disable with `--no-ids`" → "Disable in **Settings->Processor->Generate Header IDs**"

### Citations

**Key changes:**
1. Mention that bibliography files can be added to a list in Settings->Processor->Citations, or specified in document metadata
2. Remove CLI command examples
3. Remove links to "Command Line Options"

**Example transformation:**
- Replace: "Use `--bibliography refs.bib`" → "Add bibliography files to the list in **Settings->Processor->Citations**, or specify in document metadata"
- Keep: All citation syntax examples

### Syntax

**Key changes:**
1. Replace all CLI flag references with Settings references
2. Remove CLI command examples
3. Remove links to "Command Line Options"
4. Keep all syntax examples and feature descriptions

**Example transformation:**
- Replace: "Use `--sup-sub` to enable" → "Enable in **Settings->Processor->Superscript/Subscript**"
- Replace: "Use `--autolink` to enable" → "Enable **Settings->Processor->Autolinks**"
- Remove: All `apex --flag` command examples

### Inline-Attribute-Lists

**Key changes:**
1. Remove CLI references
2. Remove links to "Command Line Options"
3. Keep all syntax examples

### Metadata-Transforms

**Key changes:**
1. Remove CLI references
2. Remove links to "Command Line Options"
3. Keep all transform syntax examples and descriptions

### Credits

**Key changes:**
- No specific app-focused transformations needed (primarily lists external projects)
- Keep content as-is

## Settings Path Structure

Use this structure for Settings references:

```
Settings->Category->Setting Name
```

**Categories:**
- **General**: General app settings (e.g., Show Progress)
- **Processor**: Processing options (e.g., Mode, Plugins, Header IDs)
- **Output**: Output formatting options (e.g., Standalone HTML, CSS File)

**Examples:**
- `Settings->Processor->Mode`
- `Settings->Processor->Include Plugins`
- `Settings->Processor->Header ID Format`
- `Settings->Processor->Citations`
- `Settings->Output->Standalone HTML`
- `Settings->Output->CSS File`

## Formatting Guidelines

### Settings References

- Use **bold** for Settings paths: **Settings->Processor->Mode**
- Use **bold** for UI elements: **Plugins panel**, **Install Plugins** option
- Use regular text for descriptive phrases: "select from", "enable by checking"

### Code Blocks

- Keep Markdown syntax examples in code blocks
- Keep configuration file examples (YAML, JSON) in code blocks
- Remove command-line execution examples
- When showing app usage, use descriptive text instead of code blocks

### Links

- Remove links to "Command Line Options"
- Keep internal links to other documentation pages (Syntax, Modes, etc.)
- Update links to reference app features where appropriate

## Quality Checklist

After transformation, verify:

- [ ] All CLI flags replaced with Settings references
- [ ] All command-line examples removed
- [ ] All links to "Command Line Options" removed
- [ ] App-specific instructions added where appropriate
- [ ] Markdown syntax examples preserved
- [ ] Configuration examples preserved
- [ ] Page-specific transformations applied
- [ ] Settings paths use consistent formatting
- [ ] Text flows naturally without CLI references
- [ ] No broken internal links

## Example: Before and After

### Before (CLI-focused)

```markdown
## Enabling Plugins

Plugins are disabled by default. To enable them, use the `--plugins` flag:

```bash
apex document.md --plugins
```

You can also install plugins using:

```bash
apex --install-plugin kbd
```

See [Command Line Options](Command-Line-Options) for all plugin-related flags.
```

### After (App-focused)

```markdown
## Enabling Plugins

Plugins are disabled by default. Enable plugins by checking **Settings->Processor->Include Plugins**.

You can also enable plugins in document metadata (in the document's front matter):

```yaml
---
title: Plugin demo
plugins: true
---
```

Marked provides an **Install Plugins** option that allows you to select a plugin directory on your disk and have it automatically installed to `~/.config/apex/plugins`. Marked will list available plugins in a **Plugins panel** within the Processor settings.
```

## Notes

- This transformation is designed for apps like Marked.app that provide a GUI for Apex
- The goal is to make documentation user-friendly for app users who don't use the command line
- Settings paths are suggestions and should be adjusted to match your app's actual settings structure
- Some pages may need additional app-specific context beyond these general rules
