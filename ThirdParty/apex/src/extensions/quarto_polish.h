/**
 * Quarto mode polish: strict lists, cross-ref markers
 */

#ifndef APEX_QUARTO_POLISH_H
#define APEX_QUARTO_POLISH_H

#include <stdbool.h>

/**
 * Insert blank lines before list blocks when a non-blank line immediately
 * precedes a list marker (Pandoc strict blank-line-before-list behavior).
 * Returns NULL if unchanged.
 */
char *apex_preprocess_quarto_strict_lists(const char *text);

/**
 * Wrap Quarto cross-ref tokens (@fig-id, @sec-id, @tbl-id, @eq-id) in
 * span.quarto-xref in HTML output. Skips content inside tags. Returns NULL if unchanged.
 */
char *apex_postprocess_quarto_xrefs_html(const char *html);

#endif /* APEX_QUARTO_POLISH_H */
