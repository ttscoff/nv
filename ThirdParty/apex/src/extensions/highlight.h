/**
 * Simple Highlight Extension
 * Handles ==text== syntax (not part of CommonMark, but widely supported)
 */

#ifndef APEX_HIGHLIGHT_H
#define APEX_HIGHLIGHT_H

/**
 * Process ==highlight== syntax in text
 * Converts ==text== to <mark>text</mark>
 */
char *apex_process_highlights(const char *text);

#endif

