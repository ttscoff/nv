/**
 * Metadata Extension for Apex
 *
 * Supports three metadata formats:
 * - YAML front matter (--- delimited blocks)
 * - MultiMarkdown metadata (key: value pairs)
 * - Pandoc title blocks (% lines)
 */

#ifndef APEX_METADATA_H
#define APEX_METADATA_H

#include <stdio.h>
#include "cmark-gfm.h"
#include "cmark-gfm-extension_api.h"
#include "../../include/apex/apex.h"

#ifdef APEX_HAVE_LIBYAML
#include <yaml.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Custom node type for metadata blocks */
/* Note: APEX_NODE_METADATA is defined as an enum value in parser.h, not as a variable */

/**
 * Metadata key-value pair structure
 */
typedef struct apex_metadata_item {
    char *key;
    char *value;
    struct apex_metadata_item *next;
} apex_metadata_item;

/**
 * Create and return the metadata extension (stub for now)
 * Metadata is handled via preprocessing rather than as a block extension
 */
cmark_syntax_extension *create_metadata_extension(void);

/**
 * Extract metadata from the beginning of text (preprocessing approach)
 * Modifies *text_ptr to point past the metadata section
 * Returns the extracted metadata list
 */
apex_metadata_item *apex_extract_metadata(char **text_ptr);

/**
 * Extract metadata from the beginning of text using mode-aware precedence.
 * In MultiMarkdown mode, MMD metadata parsing takes precedence over YAML-style
 * block parsing when an MMD delimiter block is detected.
 */
apex_metadata_item *apex_extract_metadata_for_mode(char **text_ptr, apex_mode_t mode);

/**
 * Get metadata from a document node
 * Returns a linked list of key-value pairs
 */
apex_metadata_item *apex_get_metadata(cmark_node *document);

/**
 * Free metadata list
 */
void apex_free_metadata(apex_metadata_item *metadata);

/**
 * Get a specific metadata value by key (case-insensitive)
 * Returns NULL if not found
 */
const char *apex_metadata_get(apex_metadata_item *metadata, const char *key);

/**
 * Replace [%key] patterns in text with metadata values
 * If options->enable_metadata_transforms is true, supports [%key:transform:transform2] syntax
 */
char *apex_metadata_replace_variables(const char *text, apex_metadata_item *metadata, const apex_options *options);

/**
 * Load metadata from a file
 * Auto-detects format: YAML (---), MMD (key: value), or Pandoc (% lines)
 * Returns a metadata list, or NULL on error
 */
apex_metadata_item *apex_load_metadata_from_file(const char *filepath);

/**
 * Parse command-line metadata from KEY=VALUE string
 * Handles quoted values and comma-separated pairs
 * Returns a metadata list, or NULL on error
 */
apex_metadata_item *apex_parse_command_metadata(const char *arg);

/**
 * Print metadata as YAML key: value lines (no document delimiters).
 * Values are quoted when they contain :, newline, ", or \\ (same rules as CLI front matter).
 */
void apex_metadata_fprint_yaml_mapping(FILE *fp, const apex_metadata_item *metadata);

/**
 * Print a YAML document with leading/trailing --- markers and a mapping body.
 */
void apex_metadata_fprint_yaml_document(FILE *fp, const apex_metadata_item *metadata);

/**
 * Merge multiple metadata lists with precedence
 * Later lists take precedence over earlier ones
 * Returns a new merged list (caller must free with apex_free_metadata)
 * The argument list is NULL-terminated, so an empty (NULL) list after the
 * first ends the merge early; use apex_merge_metadata_lists when any list
 * may be NULL.
 */
apex_metadata_item *apex_merge_metadata(apex_metadata_item *first, ...);

/**
 * Merge count metadata lists in order (later lists win). NULL entries are
 * skipped. Returns a new list (caller must free with apex_free_metadata).
 */
apex_metadata_item *apex_merge_metadata_lists(apex_metadata_item *const *lists, size_t count);

/**
 * Apply metadata values to apex_options structure
 * Maps metadata keys to command-line options, allowing per-document control
 * Boolean values: accepts "true", "false", "yes", "no", "1", "0" (case-insensitive)
 * String values: used directly for options that take arguments
 * Modifies the options structure in-place
 */
void apex_apply_metadata_to_options(apex_metadata_item *metadata, apex_options *options);

#ifdef APEX_HAVE_LIBYAML
/**
 * Load YAML document from file and return structured representation
 * Returns a yaml_document_t pointer (caller must delete with yaml_document_delete)
 * Returns NULL on error
 */
yaml_document_t *apex_load_yaml_document(const char *filepath);

/**
 * Extract bundle array from plugin manifest YAML
 * Returns array of metadata item lists, one per bundle entry
 * Caller must free each list with apex_free_metadata, then free the array itself
 * Returns NULL if no bundle key found or on error
 */
apex_metadata_item **apex_extract_plugin_bundle(const char *filepath, size_t *count);
#endif

#ifdef __cplusplus
}
#endif

#endif /* APEX_METADATA_H */

