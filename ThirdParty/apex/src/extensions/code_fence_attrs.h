/**
 * Pandoc/Quarto fenced code block attributes ({.python filename="..."})
 */

#ifndef APEX_CODE_FENCE_ATTRS_H
#define APEX_CODE_FENCE_ATTRS_H

#include <stdbool.h>

/**
 * Normalize ```{.class key="val"} fences to plain language fences and emit
 * HTML comment markers for preserved attributes. Returns NULL if unchanged.
 */
char *apex_preprocess_code_fence_attrs(const char *text);

/**
 * Apply preserved fence attributes to the following <pre> tag and strip markers.
 * Returns NULL if unchanged.
 */
char *apex_postprocess_code_fence_attrs_html(const char *html);

#endif /* APEX_CODE_FENCE_ATTRS_H */
