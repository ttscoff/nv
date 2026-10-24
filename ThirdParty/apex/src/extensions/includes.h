/**
 * File Includes Extension for Apex
 *
 * Supports Marked's include syntax:
 * <<[file.md]   - include and process as Markdown
 * <<(file.ext)  - include as code block
 * <<{file.html} - include as raw HTML (after processing)
 *
 * Supports MultiMarkdown transclusion:
 * {{file.txt}}  - include file (MMD style)
 * {{file.*}}    - wildcard extension (chooses .html, .tex, etc based on output)
 * transclude base: path  - metadata to set base directory
 */

#ifndef APEX_INCLUDES_H
#define APEX_INCLUDES_H

#include <stdbool.h>
#include "metadata.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_INCLUDE_DEPTH 10

/**
 * Process file includes in text (preprocessing)
 * Returns newly allocated string with includes expanded
 * base_dir: base directory for relative paths (NULL for current dir)
 * metadata: metadata for transclude base support (can be NULL)
 * depth: recursion depth (for preventing infinite loops)
 */
char *apex_process_includes(const char *text,
                            const char *base_dir,
                            apex_metadata_item *metadata,
                            int depth,
                            const char *default_extension,
                            apex_plugin_manager *plugin_manager,
                            const apex_options *options);

/**
 * Check if a file exists
 */
bool apex_file_exists(const char *filepath);

char *apex_csv_to_table(const char *csv_content, bool is_tsv);
char *apex_csv_to_table_with_delimiter(const char *csv_content, bool is_tsv, char delimiter_override);

/**
 * Resolve wildcard path (e.g., file.* -> file.html)
 * Tries common extensions in order: .html, .md, .txt
 */
char *apex_resolve_wildcard(const char *filepath, const char *base_dir);

#ifdef __cplusplus
}
#endif

#endif /* APEX_INCLUDES_H */

