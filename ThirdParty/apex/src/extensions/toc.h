/**
 * Table of Contents (TOC) Extension for Apex
 *
 * Supports multiple TOC marker formats:
 * <!--TOC-->
 * <!--TOC max3 min1-->
 * {{TOC}}
 * {{TOC:2-5}}
 */

#ifndef APEX_TOC_H
#define APEX_TOC_H

#include "cmark-gfm.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Process TOC markers and generate table of contents
 * Returns new HTML with TOC inserted at markers
 * @param html The HTML output
 * @param document The AST document
 * @param id_format 0=GFM (with dashes), 1=MMD (no dashes)
 * @param default_min Default minimum heading level for bare TOC markers
 * @param default_max Default maximum heading level for bare TOC markers
 */
char *apex_process_toc(const char *html, cmark_node *document, int id_format,
                       int default_min, int default_max);

/**
 * Generate a Markdown table of contents from document headings.
 * Returns a newly allocated string containing "- [text](#id)" entries.
 * @param document The AST document
 * @param id_format 0=GFM (with dashes), 1=MMD (no dashes)
 * @param min_level Inclusive minimum heading level
 * @param max_level Inclusive maximum heading level
 */
char *apex_generate_toc_markdown(cmark_node *document, int id_format,
                                 int min_level, int max_level);

/**
 * Replace backslash-escaped {{TOC...}} markers with placeholders before parsing.
 * Call before markdown parse when TOC processing will run later.
 */
char *apex_protect_escaped_toc_markers(const char *text);

/**
 * Restore placeholders to literal {{TOC...}} text in HTML after TOC processing.
 */
char *apex_restore_escaped_toc_markers(const char *html);

#ifdef __cplusplus
}
#endif

#endif /* APEX_TOC_H */

