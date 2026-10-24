/**
 * Pandoc/Quarto raw content ({=format}) preprocessing
 */

#include "raw_content.h"
#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

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

static bool parse_raw_format(const char *start, const char *end,
                             char *format_buf, size_t format_buf_size) {
    if (!start || !end || end <= start || !format_buf || format_buf_size == 0) {
        return false;
    }

    const char *p = start;
    while (p < end && isspace((unsigned char)*p)) {
        p++;
    }
    if (p >= end) {
        return false;
    }

    const char *fmt_start = NULL;
    const char *fmt_end = NULL;

    if (*p == '{') {
        p++;
        if (p >= end || *p != '=') {
            return false;
        }
        p++;
        fmt_start = p;
        while (p < end && (isalnum((unsigned char)*p) || *p == '_' || *p == '-')) {
            p++;
        }
        fmt_end = p;
        if (fmt_end == fmt_start || p >= end || *p != '}') {
            return false;
        }
        p++;
        while (p < end && isspace((unsigned char)*p)) {
            p++;
        }
        if (p != end) {
            return false;
        }
    } else if (*p == '=') {
        p++;
        fmt_start = p;
        while (p < end && (isalnum((unsigned char)*p) || *p == '_' || *p == '-')) {
            p++;
        }
        fmt_end = p;
        if (fmt_end == fmt_start) {
            return false;
        }
        while (p < end && isspace((unsigned char)*p)) {
            p++;
        }
        if (p != end) {
            return false;
        }
    } else {
        return false;
    }

    size_t fmt_len = (size_t)(fmt_end - fmt_start);
    if (fmt_len == 0 || fmt_len >= format_buf_size) {
        return false;
    }
    for (size_t i = 0; i < fmt_len; i++) {
        format_buf[i] = (char)tolower((unsigned char)fmt_start[i]);
    }
    format_buf[fmt_len] = '\0';
    return true;
}

static bool is_html_format(const char *format) {
    return format && strcasecmp(format, "html") == 0;
}

static bool emit_raw_block(char **out, size_t *len, size_t *cap,
                           const char *body, size_t body_len,
                           const char *format, bool unsafe) {
    if (is_html_format(format) && unsafe) {
        if (body_len > 0 && !append_chunk(out, len, cap, body, body_len)) {
            return false;
        }
        if (body_len == 0 || body[body_len - 1] != '\n') {
            if (!append_chunk(out, len, cap, "\n", 1)) {
                return false;
            }
        }
        return true;
    }

    if (!append_str(out, len, cap, "<!-- raw format=")) {
        return false;
    }
    if (!append_str(out, len, cap, format)) {
        return false;
    }
    if (!append_str(out, len, cap, " -->\n")) {
        return false;
    }
    if (body_len > 0 && !append_chunk(out, len, cap, body, body_len)) {
        return false;
    }
    if (body_len == 0 || body[body_len - 1] != '\n') {
        if (!append_chunk(out, len, cap, "\n", 1)) {
            return false;
        }
    }
    return append_str(out, len, cap, "<!-- /raw -->\n");
}

static bool emit_raw_inline(char **out, size_t *len, size_t *cap,
                            const char *content, size_t content_len,
                            const char *format, bool unsafe) {
    if (is_html_format(format) && unsafe) {
        return append_chunk(out, len, cap, content, content_len);
    }

    if (!append_str(out, len, cap, "<!-- raw format=")) {
        return false;
    }
    if (!append_str(out, len, cap, format)) {
        return false;
    }
    if (!append_str(out, len, cap, " -->")) {
        return false;
    }
    if (!append_chunk(out, len, cap, content, content_len)) {
        return false;
    }
    return append_str(out, len, cap, "<!-- /raw -->");
}

static const char *line_end_ptr(const char *p) {
    const char *e = strchr(p, '\n');
    return e ? e : p + strlen(p);
}

static bool try_raw_block_fence(const char *line_start, const char *line_end,
                                const char **read_out, char **out, size_t *len, size_t *cap,
                                bool unsafe, bool *changed) {
    const char *content = line_start;
    while (content < line_end && (*content == ' ' || *content == '\t')) {
        content++;
    }
    if ((size_t)(line_end - content) < 3 || strncmp(content, "```", 3) != 0) {
        return false;
    }

    int tick_count = 0;
    const char *ticks = content;
    while (ticks < line_end && *ticks == '`') {
        tick_count++;
        ticks++;
    }
    if (tick_count < 3) {
        return false;
    }
    while (ticks < line_end && (*ticks == ' ' || *ticks == '\t')) {
        ticks++;
    }

    char format[64];
    if (!parse_raw_format(ticks, line_end, format, sizeof(format))) {
        return false;
    }

    const char *body_start = (line_end < line_start + strlen(line_start) && *line_end == '\n')
        ? line_end + 1 : line_end;
    const char *scan = body_start;
    while (*scan) {
        const char *cl_start = scan;
        const char *cl_end = line_end_ptr(scan);
        const char *cl_content = cl_start;
        while (cl_content < cl_end && (*cl_content == ' ' || *cl_content == '\t')) {
            cl_content++;
        }
        int close_ticks = 0;
        while (cl_content + close_ticks < cl_end && cl_content[close_ticks] == '`') {
            close_ticks++;
        }
        if (close_ticks >= tick_count) {
            const char *after_ticks = cl_content + close_ticks;
            while (after_ticks < cl_end && (*after_ticks == ' ' || *after_ticks == '\t')) {
                after_ticks++;
            }
            if (after_ticks == cl_end) {
                size_t body_len = (size_t)(cl_start - body_start);
                if (!emit_raw_block(out, len, cap, body_start, body_len, format, unsafe)) {
                    return false;
                }
                *changed = true;
                *read_out = (*cl_end == '\n') ? cl_end + 1 : cl_end;
                return true;
            }
        }
        scan = (*cl_end == '\n') ? cl_end + 1 : cl_end;
    }

    return false;
}

char *apex_preprocess_raw_content(const char *text, bool unsafe) {
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
    bool in_fenced_code = false;
    const char *read = text;

    while (*read) {
        const char *line_start = read;
        const char *line_end = line_end_ptr(read);
        bool at_line_start = (read == text || read[-1] == '\n');

        if (!in_fenced_code && at_line_start) {
            const char *after_block = read;
            if (try_raw_block_fence(line_start, line_end, &after_block, &out, &len, &cap, unsafe, &changed)) {
                read = after_block;
                continue;
            }
        }

        if (at_line_start) {
            const char *content = line_start;
            while (content < line_end && (*content == ' ' || *content == '\t')) {
                content++;
            }
            if ((size_t)(line_end - content) >= 3 && strncmp(content, "```", 3) == 0) {
                in_fenced_code = !in_fenced_code;
            }
        }

        if (!in_fenced_code && *read == '`') {
            int tick_count = 0;
            const char *tick_start = read;
            while (read[tick_count] == '`') {
                tick_count++;
            }
            if (tick_count > 0) {
                const char *content_start = tick_start + tick_count;
                const char *content_end = content_start;
                while (*content_end) {
                    if (*content_end == '`') {
                        int close_ticks = 0;
                        while (content_end[close_ticks] == '`') {
                            close_ticks++;
                        }
                        if (close_ticks == tick_count) {
                            break;
                        }
                    }
                    content_end++;
                }

                if (*content_end == '`') {
                    const char *after_ticks = content_end + tick_count;
                    const char *fmt_scan = after_ticks;
                    while (*fmt_scan == ' ' || *fmt_scan == '\t') {
                        fmt_scan++;
                    }

                    const char *fmt_end = fmt_scan;
                    if (*fmt_scan == '{') {
                        fmt_end = strchr(fmt_scan, '}');
                        if (fmt_end) {
                            fmt_end++;
                        } else {
                            fmt_end = fmt_scan;
                        }
                    } else if (*fmt_scan == '=') {
                        fmt_end = fmt_scan + 1;
                        while (*fmt_end && (isalnum((unsigned char)*fmt_end) || *fmt_end == '_' || *fmt_end == '-')) {
                            fmt_end++;
                        }
                    }

                    char format[64];
                    if (parse_raw_format(fmt_scan, fmt_end, format, sizeof(format))) {
                        size_t content_len = (size_t)(content_end - content_start);
                        if (!emit_raw_inline(&out, &len, &cap, content_start, content_len, format, unsafe)) {
                            free(out);
                            return NULL;
                        }
                        changed = true;
                        read = fmt_end;
                        continue;
                    }
                }
            }
        }

        if (!append_chunk(&out, &len, &cap, read, 1)) {
            free(out);
            return NULL;
        }
        read++;
    }

    if (!changed) {
        free(out);
        return NULL;
    }
    return out;
}
