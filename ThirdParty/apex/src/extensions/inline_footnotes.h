/**
 * Inline Footnotes Extension for Apex
 *
 * Supports two inline footnote syntaxes:
 * 1. Kramdown: ^[footnote text]
 * 2. MultiMarkdown: [^footnote text with spaces]
 *
 * Both are converted to standard footnote references + definitions
 * before the main parsing phase.
 */

#ifndef APEX_INLINE_FOOTNOTES_H
#define APEX_INLINE_FOOTNOTES_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Process inline footnotes by converting them to reference style
 *
 * Kramdown: text^[inline note] → text[^fn1]...[^fn1]: inline note
 * MMD: text[^inline note] → text[^fn1]...[^fn1]: inline note
 *
 * Returns newly allocated string with footnotes converted
 */
char *apex_process_inline_footnotes(const char *text);

#ifdef __cplusplus
}
#endif

#endif /* APEX_INLINE_FOOTNOTES_H */

