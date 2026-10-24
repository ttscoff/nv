/**
 * Pretty HTML Formatter
 * Adds proper indentation and whitespace to HTML output
 */

#include "apex/apex.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

/* Block-level tags that should be indented */
static const char *block_tags[] = {
    "html", "head", "body", "div", "section", "article", "nav", "header", "footer",
    "main", "aside", "h1", "h2", "h3", "h4", "h5", "h6",
    "p", "blockquote", "pre", "ul", "ol", "li", "dl", "dt", "dd",
    "table", "thead", "tbody", "tfoot", "tr", "th", "td",
    "figure", "figcaption", "details", "summary",
    NULL
};

/* Tags that should keep content on same line (no newline before closing) */
static const char *inline_tags[] = {
    "a", "strong", "em", "code", "span", "abbr", "mark", "del", "ins",
    "sup", "sub", "small", "i", "b", "u",
    NULL
};

/* Self-closing tags */
static const char *void_tags[] = {
    "meta", "link", "br", "hr", "img", "input",
    NULL
};

/**
 * Check if tag is in list
 */
static bool is_tag_in_list(const char *tag, const char **list) {
    for (int i = 0; list[i]; i++) {
        if (strcmp(tag, list[i]) == 0) return true;
    }
    return false;
}

/**
 * Extract tag name from opening or closing tag
 */
static char *extract_tag_name(const char *tag_start, bool *is_closing, bool *is_self_closing) {
    const char *p = tag_start;
    if (*p != '<') return NULL;
    p++;

    *is_closing = (*p == '/');
    if (*is_closing) p++;

    const char *name_start = p;
    while (*p && !isspace((unsigned char)*p) && *p != '>' && *p != '/') p++;

    size_t len = (size_t)(p - name_start);
    if (len == 0) return NULL;

    char *name = malloc(len + 1);
    if (name) {
        memcpy(name, name_start, len);
        name[len] = '\0';
    }

    /* Check for self-closing */
    while (*p && isspace((unsigned char)*p)) p++;
    if (*p == '/') *is_self_closing = true;

    return name;
}

/**
 * Pretty-print HTML
 */
char *apex_pretty_print_html(const char *html) {
    if (!html) return NULL;

    size_t input_len = strlen(html);
    size_t capacity = input_len * 2;  /* Room for indentation */
    char *output = malloc(capacity);
    if (!output) return strdup(html);

    const char *read = html;
    char *write = output;
    size_t remaining = capacity;
    int indent_level = 0;
    bool at_line_start = true;
    bool in_pre = false;
    bool in_inline = false;
    bool last_was_table_row = false;

    #define WRITE_STR(str) do { \
        size_t len = strlen(str); \
        if (len < remaining) { \
            memcpy(write, str, len); \
            write += len; \
            remaining -= len; \
        } \
    } while(0)

    #define WRITE_CHAR(c) do { \
        if (remaining > 0) { \
            *write++ = c; \
            remaining--; \
        } \
    } while(0)

    #define WRITE_INDENT() do { \
        if (at_line_start && !in_pre) { \
            for (int i = 0; i < indent_level; i++) { \
                WRITE_STR("  "); \
            } \
            at_line_start = false; \
        } \
    } while(0)

    while (*read) {
        /* Expand buffer if needed */
        if (remaining < 100) {
            size_t written = write - output;
            capacity *= 2;
            char *new_output = realloc(output, capacity);
            if (!new_output) break;
            output = new_output;
            write = output + written;
            remaining = capacity - written;
        }

        /* Check for HTML tags */
        if (*read == '<') {
            bool is_closing = false;
            bool is_self_closing = false;
            char *tag_name = extract_tag_name(read, &is_closing, &is_self_closing);

            if (tag_name) {
                bool is_block = is_tag_in_list(tag_name, block_tags);
                bool is_inline = is_tag_in_list(tag_name, inline_tags);
                bool is_void = is_tag_in_list(tag_name, void_tags);
                bool is_pre_tag = strcmp(tag_name, "pre") == 0 || strcmp(tag_name, "code") == 0;

                /* Track pre/code context */
                if (is_pre_tag && !is_closing) {
                    in_pre = true;
                } else if (is_pre_tag && is_closing) {
                    in_pre = false;
                }

                /* Handle block tags */
                if (is_block && !in_pre) {
                    bool is_table_row = (strcmp(tag_name, "tr") == 0);

                    if (is_closing) {
                        /* Closing tag: decrease indent first */
                        indent_level--;
                        if (indent_level < 0) indent_level = 0;

                        /* Add newline before closing tag if not at line start */
                        /* But don't add blank lines for table rows */
                        if (!at_line_start && !in_inline) {
                            if (!is_table_row) {
                                WRITE_CHAR('\n');
                                at_line_start = true;
                            }
                        }

                        WRITE_INDENT();

                        /* Track if this was a table row closing */
                        if (is_table_row) {
                            last_was_table_row = true;
                        }
                    } else {
                        /* Opening tag */
                        /* Don't add blank line before table row if last tag was also a table row */
                        if (!at_line_start) {
                            if (is_table_row && last_was_table_row) {
                                /* Skip newline - no blank line between table rows */
                                /* We're already at line start from the previous </tr> (which didn't add newline) */
                            } else if (!is_table_row) {
                                WRITE_CHAR('\n');
                                at_line_start = true;
                            }
                        }
                        /* If we're at line start and this is a table row after another table row,
                         * we don't need to do anything - the previous </tr> already positioned us correctly */
                        WRITE_INDENT();

                        /* Reset flag when opening a new row */
                        if (is_table_row) {
                            last_was_table_row = false;
                        }
                    }

                    /* Copy the tag */
                    const char *tag_end = read;
                    while (*tag_end && *tag_end != '>') tag_end++;
                    if (*tag_end == '>') tag_end++;

                    /* For closing table rows, check if next tag is also a table row or tbody closing BEFORE copying */
                    bool next_is_table_row = false;
                    bool next_is_tbody_close = false;
                    const char *whitespace_end = tag_end;
                    if (is_closing && is_table_row) {
                        const char *next = tag_end;
                        while (*next && (*next == ' ' || *next == '\t' || *next == '\n' || *next == '\r')) {
                            next++;
                        }
                        if (strncmp(next, "<tr", 3) == 0) {
                            next_is_table_row = true;
                            /* Skip whitespace between </tr> and <tr> */
                            whitespace_end = next;
                        } else if (strncmp(next, "</tbody>", 8) == 0) {
                            next_is_tbody_close = true;
                            /* Skip whitespace between </tr> and </tbody> */
                            whitespace_end = next;
                        }
                    }

                    size_t tag_len = tag_end - read;
                    if (tag_len < remaining) {
                        memcpy(write, read, tag_len);
                        write += tag_len;
                        remaining -= tag_len;
                    }
                    /* Skip whitespace if next tag is a table row or tbody closing */
                    read = (next_is_table_row || next_is_tbody_close) ? whitespace_end : tag_end;

                    /* After opening block tag, increase indent */
                    if (!is_closing && !is_self_closing && !is_void) {
                        indent_level++;
                        /* Always add newline after opening tag for proper indentation */
                        WRITE_CHAR('\n');
                        at_line_start = true;
                    } else if (is_closing || is_self_closing || is_void) {
                        /* For table rows, check if next tag is also a table row or tbody closing */
                        if (is_table_row && (next_is_table_row || next_is_tbody_close)) {
                            /* Next tag is <tr> or </tbody> - add single newline (no blank line) */
                            WRITE_CHAR('\n');
                            at_line_start = true;
                            if (next_is_table_row) {
                                last_was_table_row = true;
                            }
                        } else if (is_table_row) {
                            /* Table row closing but next is not <tr> or </tbody> - add newline normally */
                            WRITE_CHAR('\n');
                            at_line_start = true;
                            last_was_table_row = true;
                        } else {
                            WRITE_CHAR('\n');
                            at_line_start = true;
                        }
                    }

                    free(tag_name);
                    continue;
                }

                /* Track inline context */
                if (is_inline) {
                    /* If an inline tag starts at a new line (e.g. after pretty-printing
                     * a block tag like <li>), align the tag itself with current indent.
                     */
                    WRITE_INDENT();

                    if (!is_closing) {
                        in_inline = true;
                    } else {
                        in_inline = false;
                    }

                    /* Copy inline tag as-is */
                    const char *tag_end = read;
                    while (*tag_end && *tag_end != '>') tag_end++;
                    if (*tag_end == '>') tag_end++;

                    size_t tag_len = tag_end - read;
                    if (tag_len < remaining) {
                        memcpy(write, read, tag_len);
                        write += tag_len;
                        remaining -= tag_len;
                    }
                    read = tag_end;

                    free(tag_name);
                    continue;
                }

                /* Unknown tag - copy as-is */
                const char *tag_end = read;
                while (*tag_end && *tag_end != '>') tag_end++;
                if (*tag_end == '>') tag_end++;

                size_t tag_len = tag_end - read;
                if (tag_len < remaining) {
                    memcpy(write, read, tag_len);
                    write += tag_len;
                    remaining -= tag_len;
                }
                read = tag_end;

                free(tag_name);
                continue;
            }
        }

        /* Regular content */
        if ((!at_line_start || in_pre || *read != '\n') && !in_inline) {
            WRITE_INDENT();
        }

        WRITE_CHAR(*read);

        if (*read == '\n') {
            at_line_start = true;
        } else if (!isspace((unsigned char)*read)) {
            at_line_start = false;
        }

        read++;
    }

    *write = '\0';

    /* Collapse sequences of more than two consecutive newlines down to
     * exactly two, so we never produce more than a single blank line
     * between blocks in pretty-printed output.
     */
    size_t out_len = strlen(output);
    char *collapsed = malloc(out_len + 1);
    if (collapsed) {
        const char *r = output;
        char *w = collapsed;
        int newline_run = 0;

        while (*r) {
            if (*r == '\n') {
                newline_run++;
                if (newline_run <= 2) {
                    *w++ = *r++;
                } else {
                    /* Skip extra newlines beyond two */
                    r++;
                }
            } else {
                newline_run = 0;
                *w++ = *r++;
            }
        }
        *w = '\0';
        free(output);
        output = collapsed;
    }

    #undef WRITE_STR
    #undef WRITE_CHAR
    #undef WRITE_INDENT

    return output;
}

