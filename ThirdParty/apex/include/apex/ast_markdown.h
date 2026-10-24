/**
 * ast_markdown.h - Convert cmark-gfm AST to Markdown
 *
 * Supports multiple Markdown dialects: unified, mmd, commonmark, kramdown, gfm
 */

#ifndef APEX_AST_MARKDOWN_H
#define APEX_AST_MARKDOWN_H

#include "cmark-gfm.h"

/* Forward declaration of options struct; full typedef lives in apex.h */
struct apex_options;

/**
 * Markdown dialect types
 */
#ifndef APEX_MARKDOWN_DIALECT_DEFINED
#define APEX_MARKDOWN_DIALECT_DEFINED
typedef enum {
    APEX_MD_DIALECT_UNIFIED = 0,      /* Unified-mode compatible */
    APEX_MD_DIALECT_MMD = 1,          /* MultiMarkdown-compatible */
    APEX_MD_DIALECT_COMMONMARK = 2,   /* CommonMark-compatible */
    APEX_MD_DIALECT_KRAMDOWN = 3,     /* Kramdown-compatible */
    APEX_MD_DIALECT_GFM = 4           /* GitHub Flavored Markdown */
} apex_markdown_dialect_t;
#endif

/**
 * Serialize a cmark-gfm document node to Markdown
 *
 * @param document  Root cmark document node (CMARK_NODE_DOCUMENT)
 * @param options   Apex options used for this render (may be NULL)
 * @param dialect   Markdown dialect to emit
 * @return Newly allocated Markdown string or NULL on error.
 *         Caller must free with free().
 */
char *apex_cmark_to_markdown(cmark_node *document,
                             const struct apex_options *options,
                             apex_markdown_dialect_t dialect);

#endif /* APEX_AST_MARKDOWN_H */
