/**
 * One-Line Definition List Extension for Apex
 *
 * Supports: Term :: Definition text  or  Term::Definition text
 * Multiple consecutive lines create one <dl> with multiple <dt>/<dd> pairs.
 */

#ifndef APEX_DEFINITION_LIST_H
#define APEX_DEFINITION_LIST_H

#include <stdbool.h>
#include "cmark-gfm.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Process one-line definition lists via preprocessing.
 * Converts "Term :: Definition" lines to <dl><dt>Term</dt><dd>Definition</dd></dl>
 * @param text The markdown text to process
 * @param unsafe If true, allow raw HTML in output
 */
char *apex_process_definition_lists(const char *text, bool unsafe);

/**
 * Debug touch - no-op for one-line format
 */
void apex_deflist_debug_touch(int enable_definition_lists);

#ifdef __cplusplus
}
#endif

#endif /* APEX_DEFINITION_LIST_H */
