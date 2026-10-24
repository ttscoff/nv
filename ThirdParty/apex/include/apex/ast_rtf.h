#ifndef APEX_AST_RTF_H
#define APEX_AST_RTF_H

#include "cmark-gfm.h"

/* Forward declaration of options struct; full typedef lives in apex.h */
struct apex_options;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Serialize a cmark document to Rich Text Format (RTF).
 * Returns a newly allocated string (caller frees with free/apex_free_string).
 */
char *apex_cmark_to_rtf(cmark_node *document, const struct apex_options *options);

#ifdef __cplusplus
}
#endif

#endif
