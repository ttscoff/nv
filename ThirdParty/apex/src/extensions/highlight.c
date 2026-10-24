/**
 * Simple Highlight Extension
 * Converts ==text== to <mark>text</mark>
 */

#include "highlight.h"
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>

/** True if content at p looks like a list marker (- , * , + , or digit+. ) */
static bool looks_like_list_marker(const char *p) {
    if (*p == '-' || *p == '*' || *p == '+')
        return (p[1] == ' ' || p[1] == '\t');
    if (isdigit((unsigned char)*p)) {
        while (isdigit((unsigned char)*p)) p++;
        return (*p == '.' && (p[1] == ' ' || p[1] == '\t'));
    }
    return false;
}

/** True if we're at the start of a line that is an indented code block (4+ spaces or tab)
 * and not a list line. List lines (nested or continuation) should still get highlight. */
static bool line_is_indented_code_block(const char *read) {
    if (*read == '\t') {
        return !looks_like_list_marker(read + 1);
    }
    if (read[0] != ' ' || read[1] != ' ' || read[2] != ' ' || read[3] != ' ')
        return false;
    const char *content = read + 4;
    while (*content == ' ')
        content++;
    return !looks_like_list_marker(content);
}

/**
 * Process ==highlight== syntax as preprocessing
 * Converts to <mark>text</mark> before parsing
 */
char *apex_process_highlights(const char *text) {
    if (!text) return NULL;

    size_t len = strlen(text);
    size_t capacity = len * 2;  /* Room for <mark> tags */
    char *output = malloc(capacity);
    if (!output) return NULL;

    const char *read = text;
    char *write = output;
    size_t remaining = capacity;

    bool in_code_block = false;
    bool in_inline_code = false;
    bool in_indented_code_block = false;

    while (*read) {
        /* At line start: indented code block only if 4+ spaces/tab and not a list line */
        if (read == text || read[-1] == '\n') {
            in_indented_code_block = line_is_indented_code_block(read);
        }

        /* Track fenced and inline code (skip highlighting inside them) */
        if (*read == '`') {
            if (read[1] == '`' && read[2] == '`') {
                in_code_block = !in_code_block;
            } else if (!in_code_block) {
                in_inline_code = !in_inline_code;
            }
        }

        /* Look for ==highlight== (not in code, not Critic Markup) */
        /* Skip if preceded by { (Critic Markup) */
        bool is_critic = (read > text && read[-1] == '{');

        /* Check opening == requirements:
         * - Exactly 2 = characters: read[0] == '=' && read[1] == '='
         * - Not preceded by = or + (or beginning of line): (?<![=+])
         * - Character after == is not =: read[2] != '='
         * - Character immediately before or after == is not +
         * - Character after == is not whitespace
         */
        bool preceded_by_equals = (read > text && read[-1] == '=');
        bool preceded_by_plus = (read > text && read[-1] == '+');
        bool followed_by_plus = (read[2] == '+');
        bool is_valid_highlight_start = (read[0] == '=' && read[1] == '=' &&
                                         read[2] != '=' && read[2] != '}' &&
                                         read[2] != '\0' && read[2] != '\n' &&
                                         read[2] != '\r' && read[2] != ' ' && read[2] != '\t' &&
                                         !preceded_by_equals && !preceded_by_plus && !followed_by_plus);

        if (!in_code_block && !in_inline_code && !in_indented_code_block && !is_critic && is_valid_highlight_start) {

            /* Find closing == */
            const char *close = read + 2;
            while (*close && *close != '\n' && *close != '\r') {
                if (close[0] == '=' && close[1] == '=') {
                    /* Check closing == requirements:
                     * - Character after closing == is not =
                     * - Character before closing == is not space
                     * - Character immediately before or after closing == is not +
                     */
                    bool closing_followed_by_equals = (close[2] == '=');
                    bool closing_preceded_by_space = (close > read + 2 && (close[-1] == ' ' || close[-1] == '\t'));
                    bool closing_preceded_by_plus = (close > read + 2 && close[-1] == '+');
                    bool closing_followed_by_plus = (close[2] == '+');

                    if (!closing_followed_by_equals && !closing_preceded_by_space &&
                        !closing_preceded_by_plus && !closing_followed_by_plus) {
                        break;
                    }
                }
                close++;
            }

            if (*close && close[0] == '=' && close[1] == '=' && close[2] != '=') {
                /* Found complete ==highlight== */
                size_t content_len = close - (read + 2);

                /* Ensure there's actual content (not just == on a line by itself) */
                if (content_len > 0) {
                    /* Write <mark> */
                    const char *open_tag = "<mark>";
                    size_t tag_len = strlen(open_tag);
                    if (tag_len < remaining) {
                        memcpy(write, open_tag, tag_len);
                        write += tag_len;
                        remaining -= tag_len;
                    }

                    /* Copy highlighted content */
                    if (content_len < remaining) {
                        memcpy(write, read + 2, content_len);
                        write += content_len;
                        remaining -= content_len;
                    }

                    /* Write </mark> */
                    const char *close_tag = "</mark>";
                    tag_len = strlen(close_tag);
                    if (tag_len < remaining) {
                        memcpy(write, close_tag, tag_len);
                        write += tag_len;
                        remaining -= tag_len;
                    }

                    /* Skip past the closing == */
                    read = close + 2;
                    continue;
                }
            }
        }

        /* Copy character */
        if (remaining > 0) {
            *write++ = *read++;
            remaining--;
        } else {
            read++;
        }
    }

    *write = '\0';
    return output;
}


