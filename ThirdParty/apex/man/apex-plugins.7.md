% APEX-PLUGINS(7)
% Brett Terpstra
% December 2025

# NAME

apex-plugins - Apex plugin system for extending Markdown processing

# DESCRIPTION

Apex supports a lightweight plugin system that lets you add new syntax and post-processing behavior without patching the core. Plugins can be small scripts (Ruby, Python, etc.) or simple declarative regex rules defined in YAML.

Plugins are disabled by default so Apex's performance and behavior are unchanged unless you explicitly opt in.

# ENABLING PLUGINS

## Command-line flags

- **Enable plugins**: `apex --plugins input.md`
- **Disable plugins**: `apex --no-plugins input.md`

## Metadata keys

In the document's front matter, any of these keys are recognized (case-insensitive):

- `plugins`
- `enable-plugins`
- `enable_plugins`

Example:

```yaml
---
title: Plugin demo
plugins: true
---
```

## Precedence

If metadata enables or disables plugins, you can still override it from the CLI:

- `--plugins` forces plugins **on**
- `--no-plugins` forces plugins **off**

CLI flags always win over metadata.

# PLUGIN LOCATIONS

Plugins are discovered from two locations, in this order:

1. **Project-local plugins**
   - Directory: `.apex/plugins/` in the same project as your documents
   - Structure: one subdirectory per plugin, for example:
     - `.apex/plugins/kbd/plugin.yml`
     - `.apex/plugins/kbd/kbd_plugin.rb`

2. **Global (user) plugins**
   - If `$XDG_CONFIG_HOME` is set: `$XDG_CONFIG_HOME/apex/plugins/`
   - Otherwise: `~/.config/apex/plugins/`
   - Same structure: one subdirectory per plugin

**Plugin IDs must be unique.** If a project plugin and a global plugin share the same `id`, the project plugin wins and the global one is ignored.

# PROCESSING PHASES

Apex exposes several phases in its pipeline. Plugins can hook into one or more phases:

- **`pre_parse`**
  - Runs on the raw Markdown text before it is parsed.
  - Good for: custom syntax (e.g. `{% kbd ... %}`), textual rewrites, adding/removing markup before Apex sees it

- **`post_render`**
  - Runs on the final HTML output after Apex finishes rendering.
  - Good for: wrapping elements in spans/divs, adding CSS classes, simple HTML post-processing (e.g. turning `:emoji:` into `<span>`)

Internally, plugins for each phase are run in a deterministic order:

1. Sorted by priority (lower numbers first; default is `100`).
2. Ties broken by plugin `id` (lexicographically).

# PLUGIN MANIFEST

Each plugin is defined by a manifest file:

- **File name**: `plugin.yml`
- **Location**: inside the plugin's directory

At minimum, a plugin needs:

```yaml
---
id: my-plugin
phase: pre_parse           # or post_render
priority: 100              # optional, lower runs earlier
---
```

From there, you choose one of two plugin types:

- **External handler plugin** - Runs an external command (Ruby, Python, shell, etc.). Declared with a `handler.command` field.
- **Declarative regex plugin** - No external code; in-process regex search/replace. Declared with `pattern` and `replacement` fields.

You can't mix both styles in a single plugin; if `handler.command` is present, the plugin is treated as external.

## Metadata fields

To support plugin directories, automatic installation, and future auto-update tools, Apex understands several optional metadata fields in `plugin.yml`:

- **`title`**: Short, human-friendly name for the plugin
- **`author`**: Free-form author string
- **`description`**: One-two sentence description of what the plugin does
- **`homepage`**: Informational URL where users can learn more about the plugin
- **`repo`**: Canonical Git URL for the plugin repository, used by Apex when installing plugins
- **`post_install`**: Optional command that Apex will run after cloning the plugin during `--install-plugin`

Only `id`, `phase`, and either `handler.command` (for external plugins) or `pattern`/`replacement` (for declarative plugins) are required for execution.

# EXTERNAL HANDLER PLUGINS

An external handler plugin defines a command to run, which receives JSON on stdin and writes the transformed text to stdout.

## Manifest example

```yaml
---
id: kbd
title: Keyboard Shortcuts
author: Brett Terpstra
description: Render {% kbd ... %} key combos to HTML
homepage: https://github.com/ApexMarkdown/apex-kbd-plugin
repo: https://github.com/ApexMarkdown/apex-kbd-plugin.git
phase: pre_parse
priority: 100
timeout_ms: 0        # optional
handler:
  command: "ruby kbd_plugin.rb"
---
```

## JSON protocol

For text phases (`pre_parse`, `post_render`), Apex sends your command a JSON object on stdin:

```json
{
  "version": 1,
  "plugin_id": "kbd",
  "phase": "pre_parse",
  "text": "raw or rendered text here"
}
```

Your plugin should:

1. Read all of stdin.
2. Parse the JSON.
3. Transform the `text` field.
4. Print the new text only to stdout (no extra JSON, headers, or logging).

If your plugin fails, times out, or prints nothing, Apex will treat it as a no-op and continue gracefully.

# DECLARATIVE REGEX PLUGINS

For many cases, you don't need a script at all. A declarative regex plugin uses `regex.h` inside Apex for fast in-process search/replace.

## Manifest example

```yaml
---
id: emoji-span
title: Emoji span wrapper
author: Brett Terpstra
description: Wrap :emoji: markers in a span for styling
homepage: https://github.com/ApexMarkdown/apex-emoji-plugin
repo: https://github.com/ApexMarkdown/apex-emoji-plugin.git
phase: post_render
pattern: "(:[a-zA-Z0-9_+-]+:)"
replacement: "<span class=\"emoji\">$1</span>"
flags: "i"          # optional: e.g. i, m, s
priority: 200
timeout_ms: 0
---
```

- **`pattern`**: POSIX regular expression (compiled via `regcomp`).
- **`replacement`**: Replacement string with capture groups like `$1`, `$2`, etc. Runs repeatedly across the text until no more matches.
- **`flags`** (optional): Currently supports `i` (case-insensitive), `m` (multi-line), `s` (dot matches newline).

This is ideal when you only need straightforward pattern substitution and performance matters.

# PLUGIN BUNDLES

Sometimes it is convenient for a single repository to provide multiple related plugins as a bundle. Apex supports a bundle syntax in `plugin.yml` when built with full YAML (libyaml) support.

## Bundle structure

A bundle manifest has:

- Top-level metadata that applies to the bundle as a whole.
- A `bundle:` key whose value is a YAML sequence (array) of per-plugin configs.

Example:

```yaml
---
id: documentation
title: Documentation helpers
author: Brett Terpstra
description: A bundle of documentation-related helpers
homepage: https://github.com/ApexMarkdown/apex-plugin-documentation
repo: https://github.com/ApexMarkdown/apex-plugin-documentation.git

bundle:
  - id: kbd
    title: Keyboard Shortcuts
    description: Render {% kbd ... %} key combos to HTML <kbd> elements
    phase: pre_parse
    priority: 100
    handler:
      command: "ruby kbd_plugin.rb"

  - id: menubar
    title: Menubar Paths
    description: Render {% menubar File, Open %} to a styled menu path
    phase: pre_parse
    handler:
      command: "ruby menubar_plugin.rb"
---
```

Apex will treat this as three distinct plugins: `kbd`, `menubar`, and `prefspane`, all sourced from the same repository and manifest.

# ENVIRONMENT VARIABLES

When Apex runs an external handler plugin, it sets:

- **`APEX_PLUGIN_DIR`**
  - Filesystem path to the plugin's directory (where `plugin.yml` lives).
  - Useful for loading sidecar files, templates, etc.

- **`APEX_SUPPORT_DIR`**
  - Base support directory: `$XDG_CONFIG_HOME/apex/support/` or `~/.config/apex/support/`
  - For each plugin, Apex creates: `APEX_SUPPORT_DIR/<plugin-id>/`
  - You can safely write caches, logs, or temporary files there.

- **`APEX_FILE_PATH`**
  - When Apex is invoked on a file, this is the original path that was passed on the command line.
  - When Apex reads from stdin, `APEX_FILE_PATH` is set to the current `base_directory` (if one was set) or an empty string.

All of these variables apply only during the external command's execution and are restored afterward.

# INSTALLING PLUGINS

Apex can install plugins directly from a central directory or from Git URLs.

## Listing available plugins

    apex --list-plugins

This command fetches the plugin directory and prints a listing of installed and available plugins.

## Installing a plugin

The `--install-plugin` command accepts three types of arguments:

1. **Plugin ID from the directory** (recommended for curated plugins):
   ```
   apex --install-plugin kbd
   ```

2. **Full Git URL** (for plugins not in the directory):
   ```
   apex --install-plugin https://github.com/ttscoff/apex-plugin-kbd.git
   ```

3. **GitHub shorthand** (`user/repo` format):
   ```
   apex --install-plugin ttscoff/apex-plugin-kbd
   ```

When installing from a direct Git URL or GitHub shorthand (i.e., anything outside the curated directory), Apex will prompt for confirmation since plugins execute unverified code.

## Uninstalling a plugin

    apex --uninstall-plugin kbd

The `--uninstall-plugin` command verifies that the plugin directory exists, prompts for confirmation, and removes the plugin's directory. Support data under `.../apex/support/<plugin-id>/` is left intact.

This command only works for plugins installed in the user plugin directory. Project-local plugins (in `.apex/plugins/`) must be removed manually.

# EXAMPLES

## Example: `kbd` liquid tag plugin

This example shows how to support a liquid-style `{% kbd ... %}` syntax, turning key combos into `<kbd>` markup.

### Directory layout

```
.apex/
  plugins/
    kbd/
      plugin.yml
      kbd_plugin.rb
```

### `plugin.yml`

```yaml
---
id: kbd
title: Keyboard Shortcuts
author: Brett Terpstra
description: Render {% kbd ... %} key combos to HTML <kbd> elements
homepage: https://github.com/ApexMarkdown/apex-kbd-plugin
repo: https://github.com/ApexMarkdown/apex-kbd-plugin.git
phase: pre_parse
priority: 100
timeout_ms: 0
handler:
  command: "ruby kbd_plugin.rb"
---
```

The Ruby script reads JSON from stdin, extracts `text`, replaces each `{% kbd ... %}` occurrence with properly formatted `<kbd>` HTML, and prints the full transformed text to stdout.

## Example: `:emoji:` span plugin (declarative)

This plugin turns `:emoji:` tokens in the final HTML into `<span class="emoji">:emoji:</span>`.

### `plugin.yml`

```yaml
---
id: emoji-span
title: Emoji span wrapper
author: Brett Terpstra
description: Wrap :emoji: markers in a span for styling
homepage: https://github.com/ApexMarkdown/apex-emoji-plugin
repo: https://github.com/ApexMarkdown/apex-emoji-plugin.git
phase: post_render
pattern: "(:[a-zA-Z0-9_+-]+:)"
replacement: "<span class=\"emoji\">$1</span>"
flags: "i"
priority: 200
timeout_ms: 0
---
```

Because this is a declarative plugin, no external command is run. Apex compiles the regex and runs the replacements internally.

# SEE ALSO

**apex**(1), **apex-config**(5)

For complete documentation, see the [Apex Wiki](https://github.com/ttscoff/apex/wiki).

# AUTHOR

Brett Terpstra

# COPYRIGHT

Copyright (c) 2025 Brett Terpstra. Licensed under MIT License.
