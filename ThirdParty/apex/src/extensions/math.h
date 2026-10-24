/**
 * Math Extension for Apex
 *
 * Detects and preserves LaTeX math for client-side rendering:
 * $inline math$
 * $$display math$$
 * \(inline\)
 * \[display\]
 */

#ifndef APEX_MATH_H
#define APEX_MATH_H

#include <stdbool.h>
#include "cmark-gfm.h"
#include "cmark-gfm-extension_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Create and return the math extension
 */
cmark_syntax_extension *create_math_extension(void);

#ifdef __cplusplus
}
#endif

#endif /* APEX_MATH_H */

