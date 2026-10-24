/**
 * Inline Footnotes Extension for Apex
 * Implementation
 */

#include "inline_footnotes.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>

/**
 * Check if a string contains spaces (indicates inline footnote vs reference)
 */
static bool has_spaces(const char *text, int len) {
    for (int i = 0; i < len; i++) {
        if (isspace((unsigned char)text[i])) return true;
    }
    return false;
}

/**
 * Process inline footnotes
 */
char *apex_process_inline_footnotes(const char *text) {
    if (!text) return NULL;

    size_t len = strlen(text);
    /* Allocate generous buffer (inline footnotes become references + definitions) */
    size_t capacity = len * 3;
    char *output = malloc(capacity);
    if (!output) return strdup(text);

    const char *read = text;
    char *write = output;
    size_t remaining = capacity;

    /* Track footnotes to add at end */
    typedef struct footnote_def {
        int number;
        char *content;
        struct footnote_def *next;
    } footnote_def;

    footnote_def *footnotes = NULL;
    footnote_def **footnote_tail = &footnotes;
    int footnote_count = 0;

    bool in_code_block = false;
    bool in_code_span = false;

    #define WRITE_STR(str) do { \
        size_t slen = strlen(str); \
        if (slen < remaining) { \
            memcpy(write, str, slen); \
            write += slen; \
            remaining -= slen; \
        } \
    } while(0)

    #define WRITE_CHAR(c) do { \
        if (remaining > 0) { \
            *write++ = c; \
            remaining--; \
        } \
    } while(0)

    while (*read) {
        /* Track code blocks (don't process footnotes inside) */
        if (strncmp(read, "```", 3) == 0 || strncmp(read, "~~~", 3) == 0) {
            in_code_block = !in_code_block;
            WRITE_CHAR(*read);
            read++;
            continue;
        }

        /* Track inline code spans */
        if (*read == '`' && !in_code_block) {
            in_code_span = !in_code_span;
            WRITE_CHAR(*read);
            read++;
            continue;
        }

        if (in_code_block || in_code_span) {
            WRITE_CHAR(*read);
            read++;
            continue;
        }

        /* Check for Kramdown inline footnote: ^[text] */
        if (*read == '^' && read[1] == '[') {
            const char *start = read + 2;
            const char *end = start;
            int bracket_depth = 1;

            /* Find matching ] */
            while (*end && bracket_depth > 0) {
                if (*end == '[') bracket_depth++;
                else if (*end == ']') bracket_depth--;
                if (bracket_depth > 0) end++;
            }

            if (*end == ']') {
                /* Found complete inline footnote */
                int content_len = (int)(end - start);

                /* Create footnote definition */
                footnote_def *fn = malloc(sizeof(footnote_def));
                if (fn) {
                    fn->number = ++footnote_count;
                    fn->content = malloc(content_len + 1);
                    if (fn->content) {
                        memcpy(fn->content, start, content_len);
                        fn->content[content_len] = '\0';
                    }
                    fn->next = NULL;
                    *footnote_tail = fn;
                    footnote_tail = &fn->next;

                    /* Write reference */
                    char ref[32];
                    snprintf(ref, sizeof(ref), "[^fn%d]", fn->number);
                    WRITE_STR(ref);

                    read = end + 1;
                    continue;
                }
            }
        }

        /* Check for MMD inline footnote: [^text with spaces] */
        if (*read == '[' && read[1] == '^') {
            const char *start = read + 2;
            const char *end = start;

            /* Find closing ] */
            while (*end && *end != ']' && *end != '\n') end++;

            if (*end == ']') {
                int content_len = (int)(end - start);

                /* Check if it has spaces (MMD inline) vs no spaces (reference) */
                if (has_spaces(start, content_len)) {
                    /* MMD inline footnote */
                    footnote_def *fn = malloc(sizeof(footnote_def));
                    if (fn) {
                        fn->number = ++footnote_count;
                        fn->content = malloc(content_len + 1);
                        if (fn->content) {
                            memcpy(fn->content, start, content_len);
                            fn->content[content_len] = '\0';
                        }
                        fn->next = NULL;
                        *footnote_tail = fn;
                        footnote_tail = &fn->next;

                        /* Write reference */
                        char ref[32];
                        snprintf(ref, sizeof(ref), "[^fn%d]", fn->number);
                        WRITE_STR(ref);

                        read = end + 1;
                        continue;
                    }
                }
                /* else: it's a regular footnote reference, fall through */
            }
        }

        /* Regular character */
        WRITE_CHAR(*read);
        read++;
    }

    /* Add footnote definitions at the end */
    if (footnotes) {
        WRITE_STR("\n\n");

        for (footnote_def *fn = footnotes; fn; fn = fn->next) {
            char def[64];
            snprintf(def, sizeof(def), "[^fn%d]: ", fn->number);
            WRITE_STR(def);
            WRITE_STR(fn->content);
            WRITE_CHAR('\n');
        }
    }

    *write = '\0';

    /* Clean up footnote list */
    while (footnotes) {
        footnote_def *next = footnotes->next;
        free(footnotes->content);
        free(footnotes);
        footnotes = next;
    }

    #undef WRITE_STR
    #undef WRITE_CHAR

    return output;
}

