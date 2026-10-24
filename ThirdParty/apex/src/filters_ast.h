/**
 * filters_ast.h - Run Pandoc-style JSON AST filters over a cmark document.
 *
 * This module wires together:
 *   - cmark-gfm AST <-> Pandoc JSON AST (via ast_json.c)
 *   - External filter processes (one per configured command)
 *
 * Each filter command receives the JSON AST on stdin and is expected to
 * write a (possibly transformed) JSON AST to stdout.
 */

#ifndef APEX_FILTERS_AST_H
#define APEX_FILTERS_AST_H

#include "cmark-gfm.h"
#include "apex/apex.h"

/**
 * Run all configured AST filters (if any) over the given cmark document.
 *
 * @param document       Root CMARK_NODE_DOCUMENT. Ownership remains with caller;
 *                       on success, a NEW document is returned and the caller
 *                       is responsible for freeing the old one if desired.
 * @param options        Apex options (must not be NULL).
 * @param target_format  Target writer format string (e.g. "html").
 *
 * @return New CMARK_NODE_DOCUMENT with transformations applied, or the original
 *         document pointer if no filters were run. On hard failure in strict
 *         mode, NULL is returned.
 */
cmark_node *apex_run_ast_filters(cmark_node *document,
                                 const apex_options *options,
                                 const char *target_format);

#endif /* APEX_FILTERS_AST_H */

