/**
 * Pandoc Fenced Divs Extension for Apex
 *
 * Support for Pandoc's fenced_divs extension:
 *
 * ::::: {#special .sidebar}
 * Here is a paragraph.
 *
 * And another.
 * :::::
 *
 * Fenced divs can be nested. Opening fences must have attributes.
 * Closing fences need at least 3 colons (no attributes needed).
 */

#ifndef APEX_FENCED_DIVS_H
#define APEX_FENCED_DIVS_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Process fenced divs (preprocessing)
 * Converts Pandoc fenced div syntax to HTML div tags
 * Returns newly allocated string, or NULL on error
 */
char *apex_process_fenced_divs(const char *text);

/**
 * Post-process HTML to restore custom element tags from wrapper divs.
 * Replaces <div data-apex-fenced-element="tagname">...</div> with <tagname>...</tagname>.
 * Call after rendering when fenced divs with custom elements were used.
 * Returns newly allocated string, or NULL on error
 */
char *apex_postprocess_fenced_divs_html(const char *html);

#ifdef __cplusplus
}
#endif

#endif /* APEX_FENCED_DIVS_H */

