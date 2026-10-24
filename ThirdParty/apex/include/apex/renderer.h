/**
 * @file renderer.h
 * @brief AST renderer interface
 */

#ifndef APEX_RENDERER_H
#define APEX_RENDERER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "parser.h"
#include "buffer.h"

/**
 * Render AST to HTML
 *
 * @param root Root node of AST
 * @param options Rendering options
 * @return HTML string (must be freed with apex_free)
 */
char *apex_render_html(apex_node *root, const apex_options *options);

/**
 * Render AST to XML
 *
 * @param root Root node of AST
 * @param options Rendering options
 * @return XML string (must be freed with apex_free)
 */
char *apex_render_xml(apex_node *root, const apex_options *options);

#ifdef __cplusplus
}
#endif

#endif /* APEX_RENDERER_H */

