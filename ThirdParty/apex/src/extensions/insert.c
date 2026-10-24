/**
 * Insert Extension
 * Converts ++text++ to <ins>text</ins>
 * Supports IAL attributes: ++text++{: .class} → <ins markdown="span" class="class">text</ins>
 */

#include "insert.h"
#include "ial.h"  /* For parse_ial_content and attributes_to_html */
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>

/**
 * Find IAL pattern after text
 * Returns pointer to IAL start, or NULL if not found
 * Sets ial_end to end of IAL pattern
 */
static const char *find_ial_after(const char *text, const char **ial_end) {
    const char *p = text;

    /* Skip whitespace */
    while (*p && (*p == ' ' || *p == '\t')) {
        p++;
    }

    /* Check for IAL pattern { ... } */
    if (*p == '{') {
        const char *start = p;
        p++;

        /* Find closing } */
        const char *close = strchr(p, '}');
        if (close) {
            *ial_end = close + 1;
            return start;
        }
    }

    return NULL;
}

/**
 * If text starts an HTML tag (<name or </name), return a pointer just past its
 * closing '>' (quote-aware), or NULL. Gives up at a blank line outside quotes
 * so a stray '<' in prose doesn't swallow the rest of the document.
 */
static const char *html_tag_end(const char *text) {
    if (text[0] != '<') return NULL;
    const char *p = text + 1;
    if (*p == '/') p++;
    if (!isalpha((unsigned char)*p)) return NULL;

    char quote = 0;
    for (; *p; p++) {
        if (quote) {
            if (*p == quote) quote = 0;
        } else if (*p == '"' || *p == '\'') {
            quote = *p;
        } else if (*p == '>') {
            return p + 1;
        } else if (*p == '\n' && (p[1] == '\n' || (p[1] == '\r' && p[2] == '\n'))) {
            return NULL;
        }
    }
    return NULL;
}

/**
 * Process ++insert++ syntax as preprocessing
 * Converts to <ins>text</ins> before parsing
 * If followed by IAL, converts to <ins markdown="span" ...>text</ins>
 */
char *apex_process_inserts(const char *text) {
    if (!text) return NULL;

    size_t len = strlen(text);
    size_t capacity = len * 2;  /* Room for <ins> tags */
    char *output = malloc(capacity);
    if (!output) return NULL;

    const char *read = text;
    char *write = output;
    size_t remaining = capacity;

    bool in_code_block = false;
    bool in_inline_code = false;

    while (*read) {
        /* Copy HTML tags verbatim so ++ inside attribute values (e.g. base64
         * data: URIs) is not converted */
        if (!in_code_block && !in_inline_code && *read == '<') {
            const char *tag_end = html_tag_end(read);
            if (tag_end) {
                size_t tag_len = (size_t)(tag_end - read);
                if (tag_len >= remaining) {
                    size_t written = write - output;
                    capacity = (written + tag_len + 1) * 2;
                    char *new_output = realloc(output, capacity);
                    if (!new_output) goto cleanup;
                    output = new_output;
                    write = output + written;
                    remaining = capacity - written;
                }
                memcpy(write, read, tag_len);
                write += tag_len;
                remaining -= tag_len;
                read = tag_end;
                continue;
            }
        }

        /* Track code blocks (skip processing inside them) */
        if (*read == '`') {
            if (read[1] == '`' && read[2] == '`') {
                in_code_block = !in_code_block;
            } else if (!in_code_block) {
                in_inline_code = !in_inline_code;
            }
        }

        /* Look for ++insert++ (not in code, not Critic Markup) */
        /* Skip if preceded by { (Critic Markup) */
        bool is_critic = (read > text && read[-1] == '{');

        /* Check opening ++ requirements:
         * - Exactly 2 + characters: read[0] == '+' && read[1] == '+'
         * - Not preceded by + or { (or beginning of line)
         * - Character after ++ is not + or }
         * - Character after ++ is not whitespace or newline
         */
        bool preceded_by_plus = (read > text && read[-1] == '+');
        bool preceded_by_brace = (read > text && read[-1] == '{');
        bool followed_by_plus = (read[2] == '+');
        bool followed_by_brace = (read[2] == '}');
        bool is_valid_insert_start = (read[0] == '+' && read[1] == '+' &&
                                     read[2] != '+' && !followed_by_brace &&
                                     read[2] != '\0' && read[2] != '\n' &&
                                     read[2] != '\r' && read[2] != ' ' && read[2] != '\t' &&
                                     !preceded_by_plus && !preceded_by_brace && !followed_by_plus);

        if (!in_code_block && !in_inline_code && !is_critic && is_valid_insert_start) {

            /* Find closing ++ */
            const char *close = read + 2;
            while (*close && *close != '\n' && *close != '\r') {
                if (close[0] == '+' && close[1] == '+') {
                    /* Check closing ++ requirements:
                     * - Character after closing ++ is not +
                     * - Character before closing ++ is not space
                     * - Character immediately before or after closing ++ is not +
                     */
                    bool closing_followed_by_plus = (close[2] == '+');
                    bool closing_preceded_by_space = (close > read + 2 && (close[-1] == ' ' || close[-1] == '\t'));
                    bool closing_preceded_by_plus = (close > read + 2 && close[-1] == '+');

                    if (!closing_followed_by_plus && !closing_preceded_by_space &&
                        !closing_preceded_by_plus) {
                        break;
                    }
                }
                close++;
            }

            if (*close && close[0] == '+' && close[1] == '+' && close[2] != '+') {
                /* Found complete ++insert++ */
                size_t content_len = close - (read + 2);

                /* Ensure there's actual content (not just ++ on a line by itself) */
                if (content_len > 0) {
                    const char *after_close = close + 2;
                    const char *ial_end = NULL;
                    const char *ial_start = find_ial_after(after_close, &ial_end);

                    if (ial_start) {
                        /* Has IAL - convert to <ins markdown="span" ...>text</ins> */
                        size_t ial_len = (ial_end - 1) - (ial_start + 1);  /* Content inside {} */

                        /* Parse IAL attributes */
                        apex_attributes *attrs = parse_ial_content(ial_start + 1, (int)ial_len);

                        if (attrs) {
                            /* Build ins tag with attributes */
                            char *attr_str = attributes_to_html(attrs);
                            if (attr_str) {
                                /* Calculate space needed */
                                size_t ins_open_len = 30 + strlen(attr_str) + content_len + 20;
                                if (remaining < ins_open_len) {
                                    size_t written = write - output;
                                    capacity = (written + ins_open_len + 1) * 2;
                                    char *new_output = realloc(output, capacity);
                                    if (!new_output) {
                                        free(attr_str);
                                        apex_free_attributes(attrs);
                                        goto cleanup;
                                    }
                                    output = new_output;
                                    write = output + written;
                                    remaining = capacity - written;
                                }

                                /* Write <ins markdown="span" ...> */
                                int written = snprintf(write, remaining, "<ins markdown=\"span\"%s>", attr_str);
                                if (written > 0) {
                                    if ((size_t)written >= remaining) {
                                        /* String was truncated - need more space */
                                        size_t current_written = write - output;
                                        capacity = (current_written + written + 1) * 2;
                                        char *new_output = realloc(output, capacity);
                                        if (!new_output) {
                                            free(attr_str);
                                            apex_free_attributes(attrs);
                                            goto cleanup;
                                        }
                                        output = new_output;
                                        write = output + current_written;
                                        remaining = capacity - current_written;
                                        /* Retry with more space */
                                        written = snprintf(write, remaining, "<ins markdown=\"span\"%s>", attr_str);
                                    }
                                    if (written > 0 && (size_t)written < remaining) {
                                        write += written;
                                        remaining -= written;
                                    }
                                }

                                /* Write the content */
                                if (content_len < remaining) {
                                    memcpy(write, read + 2, content_len);
                                    write += content_len;
                                    remaining -= content_len;
                                }

                                /* Write </ins> */
                                const char *close_tag = "</ins>";
                                size_t tag_len = strlen(close_tag);
                                if (tag_len < remaining) {
                                    memcpy(write, close_tag, tag_len);
                                    write += tag_len;
                                    remaining -= tag_len;
                                }

                                /* Skip past the closing ++ and IAL */
                                read = ial_end;
                                free(attr_str);
                                apex_free_attributes(attrs);
                                continue;
                            }
                            apex_free_attributes(attrs);
                        }
                    }

                    /* No IAL - write simple <ins>text</ins> */
                    const char *open_tag = "<ins>";
                    size_t tag_len = strlen(open_tag);
                    if (tag_len < remaining) {
                        memcpy(write, open_tag, tag_len);
                        write += tag_len;
                        remaining -= tag_len;
                    }

                    /* Copy inserted content */
                    if (content_len < remaining) {
                        memcpy(write, read + 2, content_len);
                        write += content_len;
                        remaining -= content_len;
                    }

                    /* Write </ins> */
                    const char *close_tag = "</ins>";
                    tag_len = strlen(close_tag);
                    if (tag_len < remaining) {
                        memcpy(write, close_tag, tag_len);
                        write += tag_len;
                        remaining -= tag_len;
                    }

                    /* Skip past the closing ++ */
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

cleanup:
    *write = '\0';
    return output;
}
