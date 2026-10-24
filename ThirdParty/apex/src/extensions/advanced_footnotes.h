/**
 * Advanced Footnotes Extension for Apex
 *
 * Extends cmark-gfm footnotes to support block-level Markdown content
 * in footnote definitions.
 *
 * Standard footnote:
 * [^1]: Simple inline text
 *
 * Advanced footnote:
 * [^2]: Footnote with multiple paragraphs
 *
 *     Second paragraph in the footnote
 *
 *     ```
 *     code block
 *     ```
 *
 *     - List items
 *     - Also supported
 */

#ifndef APEX_ADVANCED_FOOTNOTES_H
#define APEX_ADVANCED_FOOTNOTES_H

#include "cmark-gfm.h"
#include "cmark-gfm-extension_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Post-process footnote definitions to allow block-level content
 * This walks the AST and re-parses footnote definition content
 */
cmark_node *apex_process_advanced_footnotes(cmark_node *root, cmark_parser *parser);

/**
 * Create advanced footnotes extension
 * This extends the base cmark-gfm footnote support
 */
cmark_syntax_extension *create_advanced_footnotes_extension(void);

#ifdef __cplusplus
}
#endif

#endif /* APEX_ADVANCED_FOOTNOTES_H */

