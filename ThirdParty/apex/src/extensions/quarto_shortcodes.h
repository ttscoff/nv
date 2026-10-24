/**
 * Quarto/Pandoc shortcode shim ({{< ... >}}, {{% ... %}})
 */

#ifndef APEX_QUARTO_SHORTCODES_H
#define APEX_QUARTO_SHORTCODES_H

#include <stdbool.h>

/**
 * Convert known Quarto shortcodes to Apex/plugin syntax:
 *   pagebreak -> {::pagebreak /}
 *   kbd       -> {% kbd ... %}
 *   include   -> <<[path]>
 * Unknown shortcodes are left unchanged; when warn_unknown is true, a message
 * is written to stderr. Pagebreak shortcodes emit raw HTML when unsafe is true.
 * Returns NULL if unchanged.
 */
char *apex_preprocess_quarto_shortcodes(const char *text, bool warn_unknown, bool unsafe);

#endif /* APEX_QUARTO_SHORTCODES_H */
