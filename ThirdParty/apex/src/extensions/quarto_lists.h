/**
 * Quarto/Pandoc list extensions: example lists (@), line blocks, roman markers
 */

#ifndef APEX_QUARTO_LISTS_H
#define APEX_QUARTO_LISTS_H

#include <stdbool.h>

/**
 * Convert Pandoc example list markers (@) to sequentially numbered ordered lists.
 * Supports (@) and (@label) markers per the example_lists extension.
 * Returns newly allocated text, or NULL if unchanged.
 */
char *apex_preprocess_example_lists(const char *text);

/**
 * Convert Pandoc line blocks (lines starting with |) to HTML div.line-block.
 * Requires unsafe HTML mode for passthrough. Returns NULL if unchanged.
 */
char *apex_preprocess_line_blocks(const char *text, bool unsafe);

/**
 * Convert roman list markers (i), ii), I), etc.) to numbered lists with style hints.
 * Returns NULL if unchanged.
 */
char *apex_preprocess_roman_lists(const char *text);

/**
 * Add list-style-type for roman list HTML comment markers.
 * Returns NULL if unchanged.
 */
char *apex_postprocess_roman_lists_html(const char *html);

#endif /* APEX_QUARTO_LISTS_H */
