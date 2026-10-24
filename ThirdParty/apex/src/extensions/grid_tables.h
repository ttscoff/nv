/**
 * Grid Tables Extension for Apex
 *
 * Preprocessing extension that converts Pandoc grid table syntax to
 * pipe table format before the regular cmark parser runs.
 *
 * Grid tables are detected by lines starting with '+' followed by '-'
 * or '=' characters (e.g., '+---+', '+===+').
 */

#ifndef APEX_GRID_TABLES_H
#define APEX_GRID_TABLES_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Preprocess grid tables in markdown text
 * Converts grid table syntax to pipe table format
 *
 * @param text Input markdown text
 * @return Newly allocated text with grid tables converted (must be freed), or NULL on error
 */
char *apex_preprocess_grid_tables(const char *text);

#ifdef __cplusplus
}
#endif

#endif /* APEX_GRID_TABLES_H */
