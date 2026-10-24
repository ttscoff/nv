/**
 * Kramdown Inline Attribute Lists (IAL) Extension for Apex
 *
 * Supports:
 * - Block IAL: {: #id .class key="value"} after blocks
 * - Span IAL: {:.class} after spans
 * - ALD (Attribute List Definitions): {:ref-name: #id .class}
 * - References: {: ref-name} to use defined attributes
 */

#ifndef APEX_IAL_H
#define APEX_IAL_H

#include <stdbool.h>
#include "cmark-gfm.h"

/* Forward declaration - actual definition in apex/apex.h */
#ifndef APEX_MODE_DEFINED
#define APEX_MODE_DEFINED
typedef enum {
    APEX_MODE_COMMONMARK = 0,
    APEX_MODE_GFM = 1,
    APEX_MODE_MULTIMARKDOWN = 2,
    APEX_MODE_KRAMDOWN = 3,
    APEX_MODE_UNIFIED = 4,
    APEX_MODE_QUARTO = 5
} apex_mode_t;
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Attribute structure
 */
typedef struct apex_attributes {
    char *id;                  /* Element ID */
    char **classes;            /* Array of class names */
    int class_count;
    char **keys;               /* Key-value pairs */
    char **values;
    int attr_count;
} apex_attributes;

/**
 * ALD (Attribute List Definition) entry
 */
typedef struct ald_entry {
    char *name;
    apex_attributes *attrs;
    struct ald_entry *next;
} ald_entry;

/**
 * Preprocess text to separate IAL markers from preceding content
 * This inserts blank lines before IAL markers so cmark parses them as separate paragraphs
 */
char *apex_preprocess_ial(const char *text);

/**
 * Extract ALDs from text (preprocessing)
 * Pattern: {:ref-name: #id .class key="value"}
 */
ald_entry *apex_extract_alds(char **text_ptr);

/**
 * Process IAL in AST (postprocessing)
 * Attaches attributes to nodes based on IAL markers
 */
void apex_process_ial_in_tree(cmark_node *document, ald_entry *alds);

/**
 * Free ALD list
 */
void apex_free_alds(ald_entry *alds);

/**
 * Free attributes structure
 */
void apex_free_attributes(apex_attributes *attrs);

/**
 * Parse IAL/ALD content
 * Format: #id .class .class2 key="value" key2='value2'
 * Returns a newly allocated attributes structure, or NULL on error
 * Caller must free the returned structure using apex_free_attributes
 */
apex_attributes *parse_ial_content(const char *content, int len);

/**
 * Convert attributes to HTML string
 * Returns a newly allocated string with HTML attributes (e.g., ' id="foo" class="bar"')
 * Caller must free the returned string
 */
char *attributes_to_html(apex_attributes *attrs);

/**
 * Image attribute entry (stored in document order for matching)
 */
typedef struct image_attr_entry {
    char *url;                  /* Encoded URL (for reference) */
    apex_attributes *attrs;     /* Attributes for this image */
    int index;                  /* Position in document (0-based for inline, -1 for reference-style) */
    char *ref_name;             /* Reference name (for reference-style definitions) */
    struct image_attr_entry *next;
} image_attr_entry;

/**
 * Preprocess markdown to extract image attributes and URL-encode all link URLs
 * Handles:
 * - Inline images: ![alt](url attributes)
 * - Reference images: ![][ref] with [ref]: url attributes
 * - URL encoding for all links (images and regular links)
 *
 * @param text Input markdown text
 * @param img_attrs Output: list of image attributes extracted
 * @param mode Processing mode to determine which features to enable
 * @return Preprocessed markdown text (must be freed by caller)
 */
char *apex_preprocess_image_attributes(const char *text, image_attr_entry **img_attrs, apex_mode_t mode);

/**
 * Free image attribute list
 */
void apex_free_image_attributes(image_attr_entry *img_attrs);

/**
 * Apply image attributes to image nodes in AST
 */
void apex_apply_image_attributes(cmark_node *document, image_attr_entry *img_attrs);

/**
 * Preprocess bracketed spans [text]{IAL}
 * Converts [text]{IAL} to <span markdown="span" ...>text</span> if [text] is not a reference link
 * @param text Input markdown text
 * @return Preprocessed markdown text (must be freed by caller), or NULL if unchanged
 */
char *apex_preprocess_bracketed_spans(const char *text);

#ifdef __cplusplus
}
#endif

#endif /* APEX_IAL_H */

