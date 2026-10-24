/**
 * Abbreviations Extension for Apex
 * Implementation
 */

#include "abbreviations.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <stdbool.h>

/**
 * Free abbreviations list
 */
void apex_free_abbreviations(abbr_item *abbrs) {
    while (abbrs) {
        abbr_item *next = abbrs->next;
        free(abbrs->abbr);
        free(abbrs->expansion);
        free(abbrs);
        abbrs = next;
    }
}

/**
 * Trim whitespace
 */
static char *trim(char *str) {
    while (isspace((unsigned char)*str)) str++;
    char *end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) *end-- = '\0';
    return str;
}

/**
 * Process MMD 6 inline abbreviations in text (before parsing)
 * Pattern: [>(abbr) expansion] → abbr (with definition added)
 */
static char *process_inline_abbreviations(const char *text, abbr_item **abbrs_list) {
    if (!text) return NULL;

    size_t len = strlen(text);
    char *output = malloc(len * 2);  /* Room for expansion */
    if (!output) return strdup(text);

    const char *read = text;
    char *write = output;

    while (*read) {
        /* Check for [>(abbr) expansion] */
        if (strncmp(read, "[>(", 3) == 0) {
            const char *abbr_start = read + 3;
            const char *abbr_end = strchr(abbr_start, ')');

            if (abbr_end) {
                const char *exp_start = abbr_end + 1;
                while (*exp_start == ' ') exp_start++;
                const char *exp_end = strchr(exp_start, ']');

                if (exp_end) {
                    /* Extract abbreviation and expansion */
                    size_t abbr_len = (size_t)(abbr_end - abbr_start);
                    size_t exp_len = (size_t)(exp_end - exp_start);

                    if (abbr_len > 0 && abbr_len < 256 && exp_len > 0 && exp_len < 1024) {
                        char abbr_text[256], exp_text[1024];
                        memcpy(abbr_text, abbr_start, abbr_len);
                        abbr_text[abbr_len] = '\0';
                        memcpy(exp_text, exp_start, exp_len);
                        exp_text[exp_len] = '\0';

                        /* Add to abbreviations list */
                        abbr_item *item = malloc(sizeof(abbr_item));
                        if (item) {
                            item->abbr = strdup(trim(abbr_text));
                            item->expansion = strdup(trim(exp_text));
                            item->next = *abbrs_list;
                            *abbrs_list = item;
                        }

                        /* Write just the abbreviation text */
                        strcpy(write, abbr_text);
                        write += abbr_len;
                        read = exp_end + 1;
                        continue;
                    }
                }
            }
        }

        /* Regular character */
        *write++ = *read++;
    }

    *write = '\0';
    return output;
}

/**
 * Extract abbreviations from text
 * Pattern: *[abbr]: expansion or [>abbr]: expansion
 */
abbr_item *apex_extract_abbreviations(char **text_ptr) {
    if (!text_ptr || !*text_ptr) return NULL;

    char *text = *text_ptr;
    abbr_item *abbrs = NULL;
    abbr_item **tail = &abbrs;
    

    /* First, process inline abbreviations [>(abbr) expansion] */
    char *processed = process_inline_abbreviations(text, &abbrs);
    if (processed) {
        strcpy(text, processed);
        free(processed);
    }

    /* Tail must point to last node's next so reference-style defs append, not overwrite */
    while (*tail) tail = &((*tail)->next);

    char *line_start = text;
    char *line_end;
    char *output = malloc(strlen(text) + 1);
    char *output_write = output;

    if (!output) return NULL;

    while ((line_end = strchr(line_start, '\n')) != NULL || *line_start) {
        if (!line_end) line_end = line_start + strlen(line_start);

        size_t line_len = (size_t)(line_end - line_start);
        char *line = malloc(line_len + 1);
        if (!line) {
            apex_free_abbreviations(abbrs);
            free(output);
            return NULL;
        }
        memcpy(line, line_start, line_len);
        line[line_len] = '\0';

        /* Check for *[abbr]: expansion pattern (old MMD) */
        if (line[0] == '*' && line[1] == '[') {
            char *close_bracket = strchr(line + 2, ']');
            if (close_bracket && close_bracket[1] == ':') {
                /* Found abbreviation definition */
                *close_bracket = '\0';
                char *abbr = trim(line + 2);
                char *expansion = trim(close_bracket + 2);

                if (*abbr && *expansion) {
                    abbr_item *item = malloc(sizeof(abbr_item));
                    if (item) {
                        item->abbr = strdup(abbr);
                        item->expansion = strdup(expansion);
                        item->next = NULL;

                        *tail = item;
                        tail = &item->next;
                    }
                }

                /* Skip this line in output */
                free(line);
                line_start = *line_end ? line_end + 1 : line_end;
                continue;
            }
        }

        /* Check for [>abbr]: expansion pattern (MMD 6) */
        if (line[0] == '[' && line[1] == '>') {
            char *close_bracket = strchr(line + 2, ']');
            if (close_bracket && close_bracket[1] == ':') {
                /* Found MMD 6 abbreviation definition */
                *close_bracket = '\0';
                char *abbr = trim(line + 2);
                char *expansion = trim(close_bracket + 2);

                if (*abbr && *expansion) {
                    abbr_item *item = malloc(sizeof(abbr_item));
                    if (item) {
                        item->abbr = strdup(abbr);
                        item->expansion = strdup(expansion);
                        item->next = NULL;

                        *tail = item;
                        tail = &item->next;
                    }
                }

                /* Skip this line in output */
                free(line);
                line_start = *line_end ? line_end + 1 : line_end;
                continue;
            }
        }

        /* Not an abbreviation, copy line to output */
        memcpy(output_write, line_start, line_len);
        output_write += line_len;
        free(line);
        if (*line_end) {
            *output_write++ = '\n';
            line_start = line_end + 1;
        } else {
            break;
        }
    }

    *output_write = '\0';

    /* Update text pointer to cleaned text */
    strcpy(text, output);
    free(output);

    return abbrs;
}

/**
 * Replace abbreviations in HTML
 */
char *apex_replace_abbreviations(const char *html, abbr_item *abbrs) {
    if (!html || !abbrs) {
        return html ? strdup(html) : NULL;
    }

    size_t html_len = strlen(html);
    /* Calculate max possible expansion:
     * Each abbreviation could be replaced with <abbr title="expansion">abbr</abbr>
     * Worst case: very long expansions. Use html_len * 5 to be safe. */
    size_t capacity = html_len * 5;
    char *output = malloc(capacity);
    if (!output) return strdup(html);

    const char *read = html;
    char *write = output;
    size_t remaining = capacity;

    while (*read) {
        bool replaced = false;

        /* Check for MMD 6 inline abbreviation (HTML-escaped): [&gt;abbr] or [&gt;(abbr) expansion] */
        if (strncmp(read, "[&gt;", 5) == 0) {
            const char *start = read + 5;
            const char *end = start;

            /* Find closing ] */
            while (*end && *end != ']' && *end != '\n' && *end != '<') end++;

            if (*end == ']') {
                /* Check for inline expansion: [&gt;(abbr) expansion] */
                if (*start == '(') {
                    const char *abbr_end = strchr(start + 1, ')');
                    if (abbr_end && abbr_end < end) {
                        /* Extract abbreviation and expansion */
                        size_t abbr_len = (size_t)(abbr_end - (start + 1));
                        size_t exp_len = (size_t)(end - (abbr_end + 1));

                        if (abbr_len > 0 && exp_len > 0) {
                            char abbr_text[256];
                            char exp_text[1024];

                            memcpy(abbr_text, start + 1, abbr_len);
                            abbr_text[abbr_len] = '\0';

                            memcpy(exp_text, abbr_end + 1, exp_len);
                            exp_text[exp_len] = '\0';

                            /* Trim whitespace */
                            char *abbr_trimmed = trim(abbr_text);
                            char *exp_trimmed = trim(exp_text);

                            /* Generate abbr tag */
                            char abbr_tag[2048];
                            snprintf(abbr_tag, sizeof(abbr_tag),
                                    "<abbr title=\"%s\">%s</abbr>",
                                    exp_trimmed, abbr_trimmed);

                            size_t tag_len = strlen(abbr_tag);
                            if (tag_len < remaining) {
                                memcpy(write, abbr_tag, tag_len);
                                write += tag_len;
                                remaining -= tag_len;
                            }

                            read = end + 1;
                            replaced = true;
                        }
                    }
                } else {
                    /* Reference abbreviation: [&gt;MMD] */
                    size_t ref_len = (size_t)(end - start);
                    if (ref_len > 0 && ref_len < 256) {
                        char ref[256];
                        memcpy(ref, start, ref_len);
                        ref[ref_len] = '\0';
                        char *ref_trimmed = trim(ref);

                        /* Look up in abbreviation list */
                        for (abbr_item *item = abbrs; item; item = item->next) {
                            if (strcmp(item->abbr, ref_trimmed) == 0) {
                                /* Found match */
                                char abbr_tag[2048];
                                snprintf(abbr_tag, sizeof(abbr_tag),
                                        "<abbr title=\"%s\">%s</abbr>",
                                        item->expansion, item->abbr);

                                size_t tag_len = strlen(abbr_tag);
                                if (tag_len < remaining) {
                                    memcpy(write, abbr_tag, tag_len);
                                    write += tag_len;
                                    remaining -= tag_len;
                                }

                                read = end + 1;
                                replaced = true;
                                break;
                            }
                        }
                    }
                }

                if (replaced) continue;
            }
        }

        /* Try each abbreviation (automatic replacement) */
        for (abbr_item *item = abbrs; item; item = item->next) {
            size_t abbr_len = strlen(item->abbr);

            /* Check if we have a match */
            if (strncmp(read, item->abbr, abbr_len) == 0) {
                /* Check it's a whole word (not part of larger word) */
                bool word_boundary = true;
                if (read > html && isalnum((unsigned char)*(read - 1))) word_boundary = false;
                if (isalnum((unsigned char)read[abbr_len])) word_boundary = false;

                if (word_boundary) {
                    /* Replace with <abbr> tag */
                    char abbr_tag[2048];
                    snprintf(abbr_tag, sizeof(abbr_tag),
                            "<abbr title=\"%s\">%s</abbr>",
                            item->expansion, item->abbr);

                    size_t tag_len = strlen(abbr_tag);
                    if (tag_len < remaining) {
                        memcpy(write, abbr_tag, tag_len);
                        write += tag_len;
                        remaining -= tag_len;
                    }

                    read += abbr_len;
                    replaced = true;
                    break;
                }
            }
        }

        if (!replaced) {
            if (remaining > 0) {
                *write++ = *read++;
                remaining--;
            } else {
                read++;
            }
        }
    }

    *write = '\0';
    return output;
}

