/**
 * Abbreviations Extension for Apex
 *
 * Supports Kramdown/MMD abbreviation syntax:
 * *[HTML]: HyperText Markup Language
 * *[CSS]: Cascading Style Sheets
 *
 * Then HTML and CSS in the text are wrapped in <abbr> tags
 */

#ifndef APEX_ABBREVIATIONS_H
#define APEX_ABBREVIATIONS_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct abbr_item {
    char *abbr;
    char *expansion;
    struct abbr_item *next;
} abbr_item;

/**
 * Extract abbreviation definitions from text
 * Modifies text_ptr to skip abbreviation definitions
 */
abbr_item *apex_extract_abbreviations(char **text_ptr);

/**
 * Replace abbreviations in HTML with <abbr> tags
 */
char *apex_replace_abbreviations(const char *html, abbr_item *abbrs);

/**
 * Free abbreviation list
 */
void apex_free_abbreviations(abbr_item *abbrs);

#ifdef __cplusplus
}
#endif

#endif /* APEX_ABBREVIATIONS_H */

