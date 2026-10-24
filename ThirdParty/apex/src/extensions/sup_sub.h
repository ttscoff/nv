/**
 * Superscript and Subscript Extension
 * Handles MultiMarkdown-style ^text^ and ~text~ syntax
 */

#ifndef APEX_SUP_SUB_H
#define APEX_SUP_SUB_H

/**
 * Process superscript and subscript syntax in text
 * Converts ^text^ to <sup>text</sup> and ~text~ to <sub>text</sub>
 * Also supports ^(text)^ and ~(text)~ for complex expressions
 */
char *apex_process_sup_sub(const char *text);

#endif
