/**
 * Callouts Extension for Apex
 *
 * Supports Bear/Obsidian callout syntax:
 * > [!NOTE] Title
 * > Content
 */

#ifndef APEX_CALLOUTS_H
#define APEX_CALLOUTS_H

#include <stdbool.h>
#include "cmark-gfm.h"
#include "cmark-gfm-extension_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Callout types for Bear/Obsidian style
 */
typedef enum {
    CALLOUT_NONE = 0,
    CALLOUT_NOTE,
    CALLOUT_ABSTRACT,      /* Also: SUMMARY, TLDR */
    CALLOUT_INFO,
    CALLOUT_TODO,
    CALLOUT_TIP,           /* Also: HINT, IMPORTANT */
    CALLOUT_SUCCESS,       /* Also: CHECK, DONE */
    CALLOUT_QUESTION,      /* Also: HELP, FAQ */
    CALLOUT_WARNING,       /* Also: CAUTION, ATTENTION */
    CALLOUT_FAILURE,       /* Also: FAIL, MISSING */
    CALLOUT_DANGER,        /* Also: ERROR */
    CALLOUT_BUG,
    CALLOUT_EXAMPLE,
    CALLOUT_QUOTE          /* Also: CITE */
} callout_type_t;

/**
 * Process callouts in AST (postprocessing). render_options are the cmark
 * options used to render inline markup in callout titles.
 */
void apex_process_callouts_in_tree(cmark_node *document, bool enable_py_callouts, int render_options);

/**
 * Preprocess Python-Markdown callouts into Obsidian-style blockquotes.
 * Pattern: !!! type "Optional title" followed by indented content.
 * Returns newly allocated string or NULL on allocation failure.
 */
char *apex_preprocess_py_callouts(const char *text);

/**
 * Preprocess Quarto ::: {.callout-*} callouts into Obsidian-style blockquotes.
 * Returns newly allocated string or NULL on allocation failure.
 */
char *apex_preprocess_quarto_callouts(const char *text);

#ifdef __cplusplus
}
#endif

#endif /* APEX_CALLOUTS_H */

