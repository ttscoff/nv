/**
 * Table HTML Postprocessing
 *
 * This injects rowspan/colspan attributes into already-rendered HTML
 * by matching AST nodes with user_data to HTML output
 */

#ifndef APEX_TABLE_HTML_POSTPROCESS_H
#define APEX_TABLE_HTML_POSTPROCESS_H

#include "cmark-gfm.h"

/**
 * Inject table attributes (rowspan, colspan) into HTML
 * Also removes cells marked for removal
 * @param html Input HTML string
 * @param document AST document node
 * @param caption_position 0=above, 1=below
 */
char *apex_inject_table_attributes(const char *html, cmark_node *document, int caption_position);

#endif /* APEX_TABLE_HTML_POSTPROCESS_H */

