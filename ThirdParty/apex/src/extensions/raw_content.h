/**
 * Pandoc/Quarto raw content ({=format}) preprocessing
 */

#ifndef APEX_RAW_CONTENT_H
#define APEX_RAW_CONTENT_H

#include <stdbool.h>

/**
 * Convert Pandoc/Quarto raw content blocks and inline spans.
 *
 * Block: ```{=html} ... ```  -> raw HTML (when unsafe) or HTML comment wrapper
 * Inline: `code`{=html}      -> raw HTML inline (when unsafe)
 *
 * Returns newly allocated text, or NULL when no changes were made.
 */
char *apex_preprocess_raw_content(const char *text, bool unsafe);

#endif /* APEX_RAW_CONTENT_H */
