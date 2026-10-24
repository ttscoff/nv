#ifndef APEX_AST_MAN_H
#define APEX_AST_MAN_H

#include "cmark-gfm.h"

/* Forward declaration of options struct; full typedef lives in apex.h */
struct apex_options;

#ifdef __cplusplus
extern "C" {
#endif

char *apex_cmark_to_man_roff(cmark_node *document, const struct apex_options *options);
char *apex_cmark_to_man_html(cmark_node *document, const struct apex_options *options);

#ifdef __cplusplus
}
#endif

#endif
