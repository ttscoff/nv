/**
 * Header ID Generation Extension
 * Generates IDs for headers following GFM or MMD6 rules
 */

#ifndef APEX_HEADER_IDS_H
#define APEX_HEADER_IDS_H

#include "cmark-gfm.h"
#include <stdbool.h>

/**
 * ID format options
 */
typedef enum {
    APEX_ID_FORMAT_GFM = 0,      /* GFM style: "emoji-support" (with dashes, collapsed spaces) */
    APEX_ID_FORMAT_MMD = 1,      /* MMD6 style: "emojisupport" (preserves dashes, removes spaces) */
    APEX_ID_FORMAT_KRAMDOWN = 2 /* Kramdown style: "header-one" (spaces→dashes, removes em/en dashes) */
} apex_id_format_t;

/**
 * Generate header ID from text
 * @param text Header text
 * @param format ID format (GFM or MMD)
 * @return Newly allocated ID string (must be freed)
 */
char *apex_generate_header_id(const char *text, apex_id_format_t format);

/**
 * Extract text content from a heading node
 * @param heading_node The heading AST node
 * @return Newly allocated text string (must be freed)
 */
char *apex_extract_heading_text(cmark_node *heading_node);

/**
 * Extract manual header ID from heading text
 * Supports:
 * - MultiMarkdown: "Heading [id]" -> returns "id", removes "[id]" from text
 * - Kramdown: "Heading {#id}" -> returns "id", removes "{#id}" from text
 * - IAL: "Heading {: #id}" -> handled separately by IAL processor
 *
 * @param heading_text Heading text (will be modified to remove ID syntax)
 * @param manual_id_out Output parameter for extracted ID (must be freed by caller)
 * @return true if manual ID was found and extracted
 */
bool apex_extract_manual_header_id(char **heading_text, char **manual_id_out);

/**
 * Process manual header IDs in a heading node
 * Extracts MMD [id] or Kramdown {#id} syntax and stores ID in user_data
 * Updates the heading text node to remove the manual ID syntax
 *
 * @param heading_node The heading AST node
 * @return true if manual ID was found and processed
 */
bool apex_process_manual_header_id(cmark_node *heading_node);

#endif

