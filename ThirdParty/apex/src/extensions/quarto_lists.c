/**
 * Quarto/Pandoc list extensions: example lists (@), line blocks, roman markers
 */

#include "quarto_lists.h"
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char ROMAN_COMMENT_LOWER[] = "<!-- apex-alpha-list-lower-roman -->";
static const char ROMAN_COMMENT_UPPER[] = "<!-- apex-alpha-list-upper-roman -->";

static const char *line_end_ptr(const char *p) {
    const char *e = strchr(p, '\n');
    return e ? e : p + strlen(p);
}

static bool append_chunk(char **out, size_t *len, size_t *cap, const char *chunk, size_t chunk_len) {
    if (chunk_len == 0) {
        return true;
    }
    if (*len + chunk_len + 1 > *cap) {
        size_t new_cap = (*cap == 0 ? 256 : *cap * 2);
        while (*len + chunk_len + 1 > new_cap) {
            new_cap *= 2;
        }
        char *grown = realloc(*out, new_cap);
        if (!grown) {
            return false;
        }
        *out = grown;
        *cap = new_cap;
    }
    memcpy(*out + *len, chunk, chunk_len);
    *len += chunk_len;
    (*out)[*len] = '\0';
    return true;
}

static bool append_str(char **out, size_t *len, size_t *cap, const char *str) {
    if (!str) {
        return true;
    }
    return append_chunk(out, len, cap, str, strlen(str));
}

static const char *skip_indent(const char *line_start, const char *line_end, size_t *indent_out) {
    const char *p = line_start;
    while (p < line_end && (*p == ' ' || *p == '\t')) {
        p++;
    }
    if (indent_out) {
        *indent_out = (size_t)(p - line_start);
    }
    return p;
}

static bool line_is_blank(const char *line_start, const char *line_end) {
    const char *p = skip_indent(line_start, line_end, NULL);
    return p >= line_end;
}

static bool parse_example_list_marker(const char *line_start, const char *line_end,
                                      const char **body_start) {
    const char *p = skip_indent(line_start, line_end, NULL);
    if (p >= line_end || *p != '(') {
        return false;
    }
    p++;
    if (p >= line_end || *p != '@') {
        return false;
    }
    p++;
    while (p < line_end &&
           (isalnum((unsigned char)*p) || *p == '_' || *p == '-')) {
        p++;
    }
    if (p >= line_end || *p != ')') {
        return false;
    }
    p++;
    if (p < line_end && *p != ' ' && *p != '\t') {
        return false;
    }
    while (p < line_end && (*p == ' ' || *p == '\t')) {
        p++;
    }
    if (body_start) {
        *body_start = p;
    }
    return true;
}

char *apex_preprocess_example_lists(const char *text) {
    if (!text) {
        return NULL;
    }

    size_t cap = strlen(text) * 2 + 256;
    char *out = malloc(cap);
    if (!out) {
        return NULL;
    }
    size_t len = 0;
    out[0] = '\0';
    bool changed = false;
    int example_num = 1;

    const char *read = text;
    while (*read) {
        const char *line_start = read;
        const char *line_end = line_end_ptr(read);
        bool has_newline = (*line_end == '\n');

        const char *body_start = NULL;
        if (parse_example_list_marker(line_start, line_end, &body_start)) {
            const char *indent_end = skip_indent(line_start, line_end, NULL);
            char num_buf[32];
            int num_len = snprintf(num_buf, sizeof(num_buf), "%d. ", example_num++);
            if (num_len <= 0 || num_len >= (int)sizeof(num_buf)) {
                free(out);
                return NULL;
            }

            if (!append_chunk(&out, &len, &cap, line_start, (size_t)(indent_end - line_start)) ||
                !append_chunk(&out, &len, &cap, num_buf, (size_t)num_len)) {
                free(out);
                return NULL;
            }

            size_t body_len = (size_t)(line_end - body_start) + (has_newline ? 1 : 0);
            if (!append_chunk(&out, &len, &cap, body_start, body_len)) {
                free(out);
                return NULL;
            }

            changed = true;
            read = has_newline ? line_end + 1 : line_end;
            continue;
        }

        size_t line_len = (size_t)(line_end - line_start) + (has_newline ? 1 : 0);
        if (!append_chunk(&out, &len, &cap, line_start, line_len)) {
            free(out);
            return NULL;
        }
        read = has_newline ? line_end + 1 : line_end;
    }

    if (!changed) {
        free(out);
        return NULL;
    }
    return out;
}

static bool is_line_block_line(const char *line_start, const char *line_end) {
    const char *p = skip_indent(line_start, line_end, NULL);
    if (p >= line_end || *p != '|') {
        return false;
    }
    p++;
    for (const char *q = p; q < line_end; q++) {
        if (*q == '|') {
            return false;
        }
    }
    return true;
}

static bool extract_line_block_content(const char *line_start, const char *line_end,
                                       const char **content_start, size_t *content_len) {
    const char *p = skip_indent(line_start, line_end, NULL);
    if (p >= line_end || *p != '|') {
        return false;
    }
    p++;
    if (p < line_end && *p == ' ') {
        p++;
    }
    const char *end = line_end;
    while (end > p && (end[-1] == ' ' || end[-1] == '\t')) {
        end--;
    }
    *content_start = p;
    *content_len = (size_t)(end - p);
    return true;
}

static int roman_digit_value(char c) {
    switch (tolower((unsigned char)c)) {
        case 'i': return 1;
        case 'v': return 5;
        case 'x': return 10;
        case 'l': return 50;
        case 'c': return 100;
        case 'd': return 500;
        case 'm': return 1000;
        default: return 0;
    }
}

static bool roman_to_int(const char *start, const char *end, bool *is_upper, int *value_out) {
    if (!start || !end || start >= end || !value_out) {
        return false;
    }

    bool upper = true;
    bool lower = true;
    for (const char *p = start; p < end; p++) {
        if (*p >= 'a' && *p <= 'z') {
            upper = false;
        } else if (*p >= 'A' && *p <= 'Z') {
            lower = false;
        } else {
            return false;
        }
        if (roman_digit_value(*p) == 0) {
            return false;
        }
    }
    if (!upper && !lower) {
        return false;
    }
    if (is_upper) {
        *is_upper = upper;
    }

    int total = 0;
    int prev = 0;
    for (const char *p = end - 1; p >= start; p--) {
        int val = roman_digit_value(*p);
        if (val < prev) {
            total -= val;
        } else {
            total += val;
        }
        prev = val;
    }
    if (total <= 0) {
        return false;
    }
    *value_out = total;
    return true;
}

static bool parse_roman_list_marker(const char *line_start, const char *line_end,
                                    size_t *indent_out, int *value_out, bool *is_upper) {
    const char *p = skip_indent(line_start, line_end, indent_out);
    if (p >= line_end) {
        return false;
    }
    const char *digits_start = p;
    while (p < line_end && (tolower((unsigned char)*p) == 'i' ||
                            tolower((unsigned char)*p) == 'v' ||
                            tolower((unsigned char)*p) == 'x' ||
                            tolower((unsigned char)*p) == 'l' ||
                            tolower((unsigned char)*p) == 'c' ||
                            tolower((unsigned char)*p) == 'd' ||
                            tolower((unsigned char)*p) == 'm')) {
        p++;
    }
    if (p == digits_start || p >= line_end || *p != ')') {
        return false;
    }
    p++;
    if (p < line_end && *p != ' ' && *p != '\t') {
        return false;
    }
    return roman_to_int(digits_start, p - 1, is_upper, value_out);
}

char *apex_preprocess_line_blocks(const char *text, bool unsafe) {
    if (!text || !unsafe) {
        return NULL;
    }

    size_t cap = strlen(text) * 3 + 256;
    char *out = malloc(cap);
    if (!out) {
        return NULL;
    }
    size_t len = 0;
    out[0] = '\0';
    bool changed = false;
    bool in_fenced_code = false;

    const char *read = text;
    while (*read) {
        const char *line_start = read;
        const char *line_end = line_end_ptr(read);
        bool has_newline = (*line_end == '\n');
        bool at_line_start = (read == text || read[-1] == '\n');

        if (at_line_start) {
            const char *content = skip_indent(line_start, line_end, NULL);
            if ((size_t)(line_end - content) >= 3 && strncmp(content, "```", 3) == 0) {
                in_fenced_code = !in_fenced_code;
            }
        }

        if (!in_fenced_code && at_line_start && is_line_block_line(line_start, line_end)) {
            const char *block_start = line_start;
            const char *scan = has_newline ? line_end + 1 : line_end;
            while (*scan) {
                const char *next_start = scan;
                const char *next_end = line_end_ptr(scan);
                if (!is_line_block_line(next_start, next_end)) {
                    break;
                }
                bool next_has_newline = (*next_end == '\n');
                scan = next_has_newline ? next_end + 1 : next_end;
            }

            if (block_start > text && block_start[-1] != '\n') {
                if (!append_str(&out, &len, &cap, "\n")) {
                    free(out);
                    return NULL;
                }
            }
            if (!append_str(&out, &len, &cap, "<div class=\"line-block\">\n")) {
                free(out);
                return NULL;
            }

            const char *line = block_start;
            while (line < scan) {
                const char *le = line_end_ptr(line);
                const char *content = NULL;
                size_t content_len = 0;
                extract_line_block_content(line, le, &content, &content_len);
                if (!append_str(&out, &len, &cap, "<div class=\"line\">")) {
                    free(out);
                    return NULL;
                }
                if (content_len > 0 && !append_chunk(&out, &len, &cap, content, content_len)) {
                    free(out);
                    return NULL;
                }
                if (!append_str(&out, &len, &cap, "</div>\n")) {
                    free(out);
                    return NULL;
                }
                line = (*le == '\n') ? le + 1 : le;
            }

            if (!append_str(&out, &len, &cap, "</div>\n")) {
                free(out);
                return NULL;
            }

            changed = true;
            read = scan;
            continue;
        }

        size_t line_len = (size_t)(line_end - line_start) + (has_newline ? 1 : 0);
        if (!append_chunk(&out, &len, &cap, line_start, line_len)) {
            free(out);
            return NULL;
        }
        read = has_newline ? line_end + 1 : line_end;
    }

    if (!changed) {
        free(out);
        return NULL;
    }
    return out;
}

char *apex_preprocess_roman_lists(const char *text) {
    if (!text) {
        return NULL;
    }

    size_t cap = strlen(text) * 3 + 256;
    char *out = malloc(cap);
    if (!out) {
        return NULL;
    }
    size_t len = 0;
    out[0] = '\0';

    bool in_roman_list = false;
    size_t roman_list_indent = 0;
    bool is_upper = false;
    int item_number = 1;
    int expected_value = 1;
    int blank_lines_since_roman = 0;
    bool changed = false;

    const char *read = text;
    while (*read) {
        const char *line_start = read;
        const char *line_end = line_end_ptr(read);
        bool has_newline = (*line_end == '\n');

        size_t current_indent = 0;
        int roman_value = 0;
        bool roman_upper = false;
        bool is_roman_marker = parse_roman_list_marker(line_start, line_end, &current_indent,
                                                       &roman_value, &roman_upper);

        if (is_roman_marker) {
            bool continues_list = false;
            if (in_roman_list && roman_upper == is_upper && current_indent == roman_list_indent &&
                roman_value == expected_value) {
                continues_list = true;
            }

            if (!continues_list) {
                in_roman_list = true;
                is_upper = roman_upper;
                roman_list_indent = current_indent;
                item_number = 1;
                expected_value = roman_value;
                blank_lines_since_roman = 0;

                const char *indent_end = skip_indent(line_start, line_end, NULL);
                if (!append_chunk(&out, &len, &cap, line_start, (size_t)(indent_end - line_start))) {
                    free(out);
                    return NULL;
                }
                const char *comment = is_upper ? ROMAN_COMMENT_UPPER : ROMAN_COMMENT_LOWER;
                if (!append_str(&out, &len, &cap, comment) ||
                    !append_str(&out, &len, &cap, "\n\n")) {
                    free(out);
                    return NULL;
                }
            } else {
                blank_lines_since_roman = 0;
            }

            const char *indent_end = skip_indent(line_start, line_end, NULL);
            char num_buf[32];
            int num_len = snprintf(num_buf, sizeof(num_buf), "%d. ", item_number);
            if (num_len <= 0 || num_len >= (int)sizeof(num_buf)) {
                free(out);
                return NULL;
            }
            if (!append_chunk(&out, &len, &cap, line_start, (size_t)(indent_end - line_start)) ||
                !append_chunk(&out, &len, &cap, num_buf, (size_t)num_len)) {
                free(out);
                return NULL;
            }

            const char *marker_end = indent_end;
            while (marker_end < line_end && (tolower((unsigned char)*marker_end) == 'i' ||
                                            tolower((unsigned char)*marker_end) == 'v' ||
                                            tolower((unsigned char)*marker_end) == 'x' ||
                                            tolower((unsigned char)*marker_end) == 'l' ||
                                            tolower((unsigned char)*marker_end) == 'c' ||
                                            tolower((unsigned char)*marker_end) == 'd' ||
                                            tolower((unsigned char)*marker_end) == 'm')) {
                marker_end++;
            }
            if (marker_end < line_end && *marker_end == ')') {
                marker_end++;
            }
            while (marker_end < line_end && (*marker_end == ' ' || *marker_end == '\t')) {
                marker_end++;
            }

            size_t rest_len = (size_t)(line_end - marker_end) + (has_newline ? 1 : 0);
            if (!append_chunk(&out, &len, &cap, marker_end, rest_len)) {
                free(out);
                return NULL;
            }

            item_number++;
            expected_value = roman_value + 1;
            changed = true;
            read = has_newline ? line_end + 1 : line_end;
            continue;
        }

        if (in_roman_list) {
            if (line_is_blank(line_start, line_end)) {
                blank_lines_since_roman++;
                if (blank_lines_since_roman >= 2) {
                    in_roman_list = false;
                }
            } else if (current_indent > roman_list_indent) {
                blank_lines_since_roman = 0;
            } else {
                in_roman_list = false;
                blank_lines_since_roman = 0;
            }
        }

        size_t line_len = (size_t)(line_end - line_start) + (has_newline ? 1 : 0);
        if (!append_chunk(&out, &len, &cap, line_start, line_len)) {
            free(out);
            return NULL;
        }
        read = has_newline ? line_end + 1 : line_end;
    }

    if (!changed) {
        free(out);
        return NULL;
    }
    return out;
}

static char *apply_list_style_to_following_ol(const char *html, const char *comment,
                                              const char *style_attr) {
    if (!html || !comment || !style_attr) {
        return NULL;
    }
    if (!strstr(html, comment)) {
        return NULL;
    }

    size_t html_len = strlen(html);
    size_t cap = html_len + 1024;
    char *out = malloc(cap);
    if (!out) {
        return NULL;
    }
    size_t len = 0;
    out[0] = '\0';
    bool changed = false;

    const char *read = html;
    const char *read_start = read;
    const size_t comment_len = strlen(comment);
    const size_t style_len = strlen(style_attr);

    while (*read) {
        if (strncmp(read, comment, comment_len) == 0) {
            size_t copy_len = (size_t)(read - read_start);
            if (len + copy_len + style_len + 64 > cap) {
                cap = (len + copy_len + style_len + 64) * 2;
                char *grown = realloc(out, cap);
                if (!grown) {
                    free(out);
                    return NULL;
                }
                out = grown;
            }
            if (copy_len > 0) {
                memcpy(out + len, read_start, copy_len);
                len += copy_len;
            }
            read += comment_len;
            read_start = read;
            while (*read == ' ' || *read == '\t' || *read == '\n' || *read == '\r') {
                read++;
            }
            read_start = read;

            if (read[0] == '<' && read[1] == 'o' && read[2] == 'l' &&
                (read[3] == '>' || read[3] == ' ' || read[3] == '\t' || read[3] == '\n')) {
                const char *ol_start = read;
                const char *tag_end = strchr(ol_start, '>');
                if (tag_end) {
                    bool has_style = false;
                    for (const char *p = ol_start; p < tag_end; p++) {
                        if (strncmp(p, "style=", 6) == 0) {
                            has_style = true;
                            break;
                        }
                    }
                    size_t tag_len = (size_t)(tag_end - ol_start);
                    memcpy(out + len, ol_start, tag_len);
                    len += tag_len;
                    if (!has_style) {
                        memcpy(out + len, style_attr, style_len);
                        len += style_len;
                    } else {
                        out[len++] = '>';
                    }
                    read = tag_end + 1;
                    read_start = read;
                    changed = true;
                    continue;
                }
            }
            continue;
        }
        read++;
    }

    if (!changed) {
        free(out);
        return NULL;
    }

    size_t tail_len = strlen(read_start);
    if (len + tail_len + 1 > cap) {
        cap = len + tail_len + 2;
        char *grown = realloc(out, cap);
        if (!grown) {
            free(out);
            return NULL;
        }
        out = grown;
    }
    memcpy(out + len, read_start, tail_len);
    len += tail_len;
    out[len] = '\0';
    return out;
}

char *apex_postprocess_roman_lists_html(const char *html) {
    if (!html) {
        return NULL;
    }

    char *lower = apply_list_style_to_following_ol(html, ROMAN_COMMENT_LOWER,
                                                   " style=\"list-style-type: lower-roman\">");
    const char *src = lower ? lower : html;
    char *upper = apply_list_style_to_following_ol(src, ROMAN_COMMENT_UPPER,
                                                   " style=\"list-style-type: upper-roman\">");
    if (lower && upper) {
        free(lower);
        return upper;
    }
    return upper ? upper : lower;
}
