/**
 * Special Markers Extension for Apex
 *
 * Handles Marked's special HTML comment markers:
 * <!--BREAK-->        - Page break for print/PDF
 * <!--PAUSE:X-->      - Autoscroll pause for X seconds
 * {::pagebreak /}     - Leanpub page break
 * {index}             - Leanpub index placement (replaced with <!--INDEX-->)
 */

#ifndef APEX_SPECIAL_MARKERS_H
#define APEX_SPECIAL_MARKERS_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Process special markers in text (preprocessing)
 * Replaces markers with appropriate HTML
 */
char *apex_process_special_markers(const char *text);

#ifdef __cplusplus
}
#endif

#endif /* APEX_SPECIAL_MARKERS_H */

