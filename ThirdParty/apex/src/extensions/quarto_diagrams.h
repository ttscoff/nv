/**
 * Quarto/Pandoc diagram fences (mermaid, graphviz/dot)
 */

#ifndef APEX_QUARTO_DIAGRAMS_H
#define APEX_QUARTO_DIAGRAMS_H

#include <stdbool.h>

/**
 * Convert ```{mermaid}, ```{dot}, ```{graphviz} (and bare language names) to
 * raw HTML pre.diagram blocks. Requires unsafe HTML mode. Returns NULL if unchanged.
 */
char *apex_preprocess_quarto_diagrams(const char *text, bool unsafe);

/** True when html output contains a mermaid diagram block. */
bool apex_html_has_mermaid_diagram(const char *html);

/** Mermaid CDN script tag (same source as CLI --script mermaid). */
const char *apex_mermaid_script_tag(void);

#endif /* APEX_QUARTO_DIAGRAMS_H */
